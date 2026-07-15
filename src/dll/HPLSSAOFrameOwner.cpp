#include "HPLSSAOFrameOwner.h"

#include "Config.h"
#include "HPLToneMappingFrameMath.h"
#include "Logger.h"

#include <Windows.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <mutex>
#include <unordered_map>

namespace somavr {
namespace {

using tone_mapping_frame_math::PassRole;

struct RendererState {
    tone_mapping_frame_math::State schedule;
    float baseline = 0.0f;
    float committed = 0.0f;
    bool baselineValid = false;
    bool committedValid = false;
};

struct ActivePass {
    void* renderer = nullptr;
    PassRole role = PassRole::None;
    uint64_t poseFrame = 0;
};

bool g_configured = false;
bool g_faulted = false;
float* g_temporalPhase = nullptr;
std::mutex g_mutex;
std::unordered_map<void*, RendererState> g_renderers;
thread_local ActivePass g_activePass;
std::atomic<uint64_t> g_firstPasses = 0;
std::atomic<uint64_t> g_replayPasses = 0;
std::atomic<uint64_t> g_committedRestores = 0;
std::atomic<uint64_t> g_mismatches = 0;
std::atomic<uint64_t> g_failures = 0;

bool IsAccessible(const void* address, bool writable)
{
    if (address == nullptr) return false;
    MEMORY_BASIC_INFORMATION info{};
    if (VirtualQuery(address, &info, sizeof(info)) == 0 || info.State != MEM_COMMIT
        || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) {
        return false;
    }
    const DWORD protection = info.Protect & 0xff;
    const bool readable = protection == PAGE_READONLY || protection == PAGE_READWRITE
        || protection == PAGE_WRITECOPY || protection == PAGE_EXECUTE_READ
        || protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY;
    const bool writeable = protection == PAGE_READWRITE || protection == PAGE_WRITECOPY
        || protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY;
    const uintptr_t start = reinterpret_cast<uintptr_t>(address);
    const uintptr_t end = reinterpret_cast<uintptr_t>(info.BaseAddress) + info.RegionSize;
    return readable && (!writable || writeable) && start <= end && sizeof(float) <= end - start;
}

bool ReadPhase(float& value)
{
    if (!IsAccessible(g_temporalPhase, false)) return false;
    std::memcpy(&value, g_temporalPhase, sizeof(value));
    return std::isfinite(value);
}

bool WritePhase(float value)
{
    if (!std::isfinite(value) || !IsAccessible(g_temporalPhase, true)) return false;
    std::memcpy(g_temporalPhase, &value, sizeof(value));
    return true;
}

bool Near(float left, float right)
{
    if (!std::isfinite(left) || !std::isfinite(right)) return left == right;
    return std::abs(left - right) <= 1.0e-5f * std::max(1.0f, std::max(std::abs(left), std::abs(right)));
}

void Fault(const char* reason, void* renderer)
{
    g_faulted = true;
    g_activePass = {};
    const uint64_t failures = g_failures.fetch_add(1, std::memory_order_relaxed) + 1;
    Logger::Instance().Write(
        LogLevel::Error,
        "hpl_ssao_frame_owner fault=1 reason=%s renderer=%p failures=%llu fallback=native_per_eye_phase_advance",
        reason,
        renderer,
        static_cast<unsigned long long>(failures));
}

} // namespace

void InitializeHPLSSAOFrameOwner(const Config& config, float* temporalPhase)
{
    std::lock_guard lock(g_mutex);
    g_configured = config.hplSSAOFrameOwnerControl;
    g_faulted = false;
    g_temporalPhase = temporalPhase;
    g_renderers.clear();
    g_activePass = {};
    g_firstPasses.store(0, std::memory_order_relaxed);
    g_replayPasses.store(0, std::memory_order_relaxed);
    g_committedRestores.store(0, std::memory_order_relaxed);
    g_mismatches.store(0, std::memory_order_relaxed);
    g_failures.store(0, std::memory_order_relaxed);
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_ssao_frame_owner initialized configured=%d temporalPhase=%p policy=single_same_pose_advance",
        g_configured ? 1 : 0,
        g_temporalPhase);
}

void BeginHPLSSAOFrameOwner(
    void* renderer,
    bool stereoEligible,
    int eyeIndex,
    uint64_t poseFrame,
    uint64_t calibrationGeneration)
{
    g_activePass = {};
    if (!g_configured || g_faulted) return;

    std::lock_guard lock(g_mutex);
    RendererState& state = g_renderers[renderer];
    const PassRole role = tone_mapping_frame_math::Begin(
        state.schedule,
        reinterpret_cast<uintptr_t>(renderer),
        stereoEligible,
        eyeIndex,
        poseFrame,
        calibrationGeneration);
    if (role == PassRole::None) return;

    if (role == PassRole::FirstEye) {
        state.baselineValid = ReadPhase(state.baseline);
        state.committedValid = false;
        if (!state.baselineValid) {
            Fault("baseline_read_failed", renderer);
            return;
        }
        g_firstPasses.fetch_add(1, std::memory_order_relaxed);
    } else {
        if (!state.baselineValid || !state.committedValid || !WritePhase(state.baseline)) {
            Fault("replay_baseline_restore_failed", renderer);
            return;
        }
        g_replayPasses.fetch_add(1, std::memory_order_relaxed);
    }
    g_activePass = {renderer, role, poseFrame};
}

void EndHPLSSAOFrameOwner(void* renderer)
{
    const ActivePass pass = g_activePass;
    g_activePass = {};
    if (pass.role == PassRole::None) return;

    std::lock_guard lock(g_mutex);
    const auto found = g_renderers.find(renderer);
    if (renderer != pass.renderer || found == g_renderers.end()) {
        Fault("tracked_renderer_missing", renderer);
        return;
    }
    RendererState& state = found->second;
    float output = 0.0f;
    if (!ReadPhase(output)) {
        Fault("phase_read_failed", renderer);
        return;
    }

    if (pass.role == PassRole::FirstEye) {
        state.committed = output;
        state.committedValid = true;
    } else {
        if (!Near(output, state.committed)) {
            const uint64_t mismatches = g_mismatches.fetch_add(1, std::memory_order_relaxed) + 1;
            if (mismatches <= 8) {
                Logger::Instance().Write(
                    LogLevel::Warn,
                    "hpl_ssao_frame_owner mismatch=%llu renderer=%p poseFrame=%llu firstPhase=%.6f replayPhase=%.6f action=restore_first_commit",
                    static_cast<unsigned long long>(mismatches),
                    renderer,
                    static_cast<unsigned long long>(pass.poseFrame),
                    state.committed,
                    output);
            }
        }
        if (!state.committedValid || !WritePhase(state.committed)) {
            Fault("committed_phase_restore_failed", renderer);
            return;
        }
        g_committedRestores.fetch_add(1, std::memory_order_relaxed);
    }
    if (!tone_mapping_frame_math::Commit(state.schedule, pass.role)) {
        Fault("schedule_commit_failed", renderer);
    }
}

HPLSSAOFrameOwnerStatus GetHPLSSAOFrameOwnerStatus()
{
    std::lock_guard lock(g_mutex);
    return {
        g_configured,
        g_configured && !g_faulted && g_temporalPhase != nullptr,
        g_faulted,
        static_cast<uint64_t>(g_renderers.size()),
        g_firstPasses.load(std::memory_order_relaxed),
        g_replayPasses.load(std::memory_order_relaxed),
        g_committedRestores.load(std::memory_order_relaxed),
        g_mismatches.load(std::memory_order_relaxed),
        g_failures.load(std::memory_order_relaxed),
    };
}

void RemoveHPLSSAOFrameOwner()
{
    const HPLSSAOFrameOwnerStatus status = GetHPLSSAOFrameOwnerStatus();
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_ssao_frame_owner removed trackedRenderers=%llu firstPasses=%llu replayPasses=%llu committedRestores=%llu mismatches=%llu failures=%llu",
        static_cast<unsigned long long>(status.trackedRenderers),
        static_cast<unsigned long long>(status.firstPasses),
        static_cast<unsigned long long>(status.replayPasses),
        static_cast<unsigned long long>(status.committedRestores),
        static_cast<unsigned long long>(status.mismatches),
        static_cast<unsigned long long>(status.failures));
    std::lock_guard lock(g_mutex);
    g_renderers.clear();
    g_activePass = {};
    g_temporalPhase = nullptr;
    g_configured = false;
    g_faulted = false;
}

} // namespace somavr
