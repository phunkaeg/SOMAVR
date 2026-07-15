#include "HPLToneMappingFrame.h"

#include "Config.h"
#include "HPLToneMappingFrameMath.h"
#include "Logger.h"

#include <Windows.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <mutex>
#include <unordered_map>

namespace somavr {
namespace {

using tone_mapping_frame_math::PassRole;

struct ToneMappingPacket {
    float exposure = 0.0f;
    float whiteCut = 0.0f;
    void* gradingTexture = nullptr;
    void* targetGradingTexture = nullptr;
    void* queuedGradingTexture = nullptr;
    float gradingTransitionSpeed = 0.0f;
    float queuedGradingTransitionSpeed = 0.0f;
    float gradingTransitionWeight = 0.0f;
    uint8_t gradingTransition = 0;
    float exposureSrc = 0.0f;
    float exposureDst = 0.0f;
    float whiteCutSrc = 0.0f;
    float whiteCutDst = 0.0f;
    float windowExposure = 0.0f;
    float windowWhiteCut = 0.0f;
    float windowExposureTarget = 0.0f;
    float windowWhiteCutTarget = 0.0f;
    float transition = 0.0f;
    float transitionTime = 0.0f;
    float transitionSpeed = 0.0f;
};

struct EffectState {
    tone_mapping_frame_math::State schedule;
    ToneMappingPacket baseline;
    ToneMappingPacket committed;
    bool baselineValid = false;
    bool committedValid = false;
};

struct ActivePass {
    void* effect = nullptr;
    PassRole role = PassRole::None;
    uint64_t poseFrame = 0;
};

bool g_configured = false;
bool g_faulted = false;
std::mutex g_mutex;
std::unordered_map<void*, EffectState> g_effects;
thread_local ActivePass g_activePass;
std::atomic<uint64_t> g_firstPasses = 0;
std::atomic<uint64_t> g_replayPasses = 0;
std::atomic<uint64_t> g_committedRestores = 0;
std::atomic<uint64_t> g_mismatches = 0;
std::atomic<uint64_t> g_failures = 0;

bool IsAccessible(const void* address, size_t size, bool writable)
{
    if (address == nullptr || size == 0) return false;
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
    return readable && (!writable || writeable) && start <= end && size <= end - start;
}

template <typename T>
bool ReadField(const void* object, size_t offset, T& value)
{
    const void* address = static_cast<const uint8_t*>(object) + offset;
    if (!IsAccessible(address, sizeof(T), false)) return false;
    std::memcpy(&value, address, sizeof(T));
    return true;
}

template <typename T>
bool WriteField(void* object, size_t offset, const T& value)
{
    void* address = static_cast<uint8_t*>(object) + offset;
    if (!IsAccessible(address, sizeof(T), true)) return false;
    std::memcpy(address, &value, sizeof(T));
    return true;
}

bool ReadPacket(void* effect, ToneMappingPacket& packet)
{
    return ReadField(effect, 0x8c, packet.exposure)
        && ReadField(effect, 0x94, packet.whiteCut)
        && ReadField(effect, 0xa0, packet.gradingTexture)
        && ReadField(effect, 0xd8, packet.targetGradingTexture)
        && ReadField(effect, 0xe0, packet.queuedGradingTexture)
        && ReadField(effect, 0xe8, packet.gradingTransitionSpeed)
        && ReadField(effect, 0xec, packet.queuedGradingTransitionSpeed)
        && ReadField(effect, 0xf0, packet.gradingTransitionWeight)
        && ReadField(effect, 0xf4, packet.gradingTransition)
        && ReadField(effect, 0xf8, packet.exposureSrc)
        && ReadField(effect, 0xfc, packet.exposureDst)
        && ReadField(effect, 0x100, packet.whiteCutSrc)
        && ReadField(effect, 0x104, packet.whiteCutDst)
        && ReadField(effect, 0x108, packet.windowExposure)
        && ReadField(effect, 0x10c, packet.windowWhiteCut)
        && ReadField(effect, 0x110, packet.windowExposureTarget)
        && ReadField(effect, 0x114, packet.windowWhiteCutTarget)
        && ReadField(effect, 0x118, packet.transition)
        && ReadField(effect, 0x11c, packet.transitionTime)
        && ReadField(effect, 0x120, packet.transitionSpeed);
}

bool WritePacket(void* effect, const ToneMappingPacket& packet)
{
    return WriteField(effect, 0x8c, packet.exposure)
        && WriteField(effect, 0x94, packet.whiteCut)
        && WriteField(effect, 0xa0, packet.gradingTexture)
        && WriteField(effect, 0xd8, packet.targetGradingTexture)
        && WriteField(effect, 0xe0, packet.queuedGradingTexture)
        && WriteField(effect, 0xe8, packet.gradingTransitionSpeed)
        && WriteField(effect, 0xec, packet.queuedGradingTransitionSpeed)
        && WriteField(effect, 0xf0, packet.gradingTransitionWeight)
        && WriteField(effect, 0xf4, packet.gradingTransition)
        && WriteField(effect, 0xf8, packet.exposureSrc)
        && WriteField(effect, 0xfc, packet.exposureDst)
        && WriteField(effect, 0x100, packet.whiteCutSrc)
        && WriteField(effect, 0x104, packet.whiteCutDst)
        && WriteField(effect, 0x108, packet.windowExposure)
        && WriteField(effect, 0x10c, packet.windowWhiteCut)
        && WriteField(effect, 0x110, packet.windowExposureTarget)
        && WriteField(effect, 0x114, packet.windowWhiteCutTarget)
        && WriteField(effect, 0x118, packet.transition)
        && WriteField(effect, 0x11c, packet.transitionTime)
        && WriteField(effect, 0x120, packet.transitionSpeed);
}

bool Near(float left, float right)
{
    if (!std::isfinite(left) || !std::isfinite(right)) return left == right;
    return std::abs(left - right) <= 1.0e-5f * std::max(1.0f, std::max(std::abs(left), std::abs(right)));
}

bool Equivalent(const ToneMappingPacket& left, const ToneMappingPacket& right)
{
    return left.gradingTexture == right.gradingTexture
        && left.targetGradingTexture == right.targetGradingTexture
        && left.queuedGradingTexture == right.queuedGradingTexture
        && left.gradingTransition == right.gradingTransition
        && Near(left.exposure, right.exposure)
        && Near(left.whiteCut, right.whiteCut)
        && Near(left.gradingTransitionSpeed, right.gradingTransitionSpeed)
        && Near(left.queuedGradingTransitionSpeed, right.queuedGradingTransitionSpeed)
        && Near(left.gradingTransitionWeight, right.gradingTransitionWeight)
        && Near(left.exposureSrc, right.exposureSrc)
        && Near(left.exposureDst, right.exposureDst)
        && Near(left.whiteCutSrc, right.whiteCutSrc)
        && Near(left.whiteCutDst, right.whiteCutDst)
        && Near(left.windowExposure, right.windowExposure)
        && Near(left.windowWhiteCut, right.windowWhiteCut)
        && Near(left.windowExposureTarget, right.windowExposureTarget)
        && Near(left.windowWhiteCutTarget, right.windowWhiteCutTarget)
        && Near(left.transition, right.transition)
        && Near(left.transitionTime, right.transitionTime)
        && Near(left.transitionSpeed, right.transitionSpeed);
}

void Fault(const char* reason, void* effect)
{
    g_faulted = true;
    g_activePass = {};
    const uint64_t failures = g_failures.fetch_add(1, std::memory_order_relaxed) + 1;
    Logger::Instance().Write(
        LogLevel::Error,
        "hpl_tone_mapping_frame fault=1 reason=%s effect=%p failures=%llu fallback=native_per_eye_update",
        reason,
        effect,
        static_cast<unsigned long long>(failures));
}

} // namespace

void InitializeHPLToneMappingFrame(const Config& config)
{
    std::lock_guard lock(g_mutex);
    g_configured = config.hplToneMappingFrameControl;
    g_faulted = false;
    g_effects.clear();
    g_activePass = {};
    g_firstPasses.store(0, std::memory_order_relaxed);
    g_replayPasses.store(0, std::memory_order_relaxed);
    g_committedRestores.store(0, std::memory_order_relaxed);
    g_mismatches.store(0, std::memory_order_relaxed);
    g_failures.store(0, std::memory_order_relaxed);
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_tone_mapping_frame initialized configured=%d policy=shared_once_per_pose_frame fields=exposure_whitecut_grading_transition",
        g_configured ? 1 : 0);
}

void BeginHPLToneMappingFrame(
    void* effect,
    bool toneMapping,
    bool stereoEligible,
    int eyeIndex,
    uint64_t poseFrame,
    uint64_t calibrationGeneration)
{
    g_activePass = {};
    if (!toneMapping || effect == nullptr) return;

    std::lock_guard lock(g_mutex);
    if (!g_configured || g_faulted) return;

    EffectState& state = g_effects[effect];
    const PassRole role = tone_mapping_frame_math::Begin(
        state.schedule,
        reinterpret_cast<uintptr_t>(effect),
        stereoEligible,
        eyeIndex,
        poseFrame,
        calibrationGeneration);
    if (role == PassRole::None) return;

    if (role == PassRole::FirstEye) {
        state.baselineValid = ReadPacket(effect, state.baseline);
        state.committedValid = false;
        if (!state.baselineValid) {
            Fault("baseline_read_failed", effect);
            return;
        }
        g_firstPasses.fetch_add(1, std::memory_order_relaxed);
    } else {
        if (!state.baselineValid || !state.committedValid || !WritePacket(effect, state.baseline)) {
            Fault("replay_baseline_restore_failed", effect);
            return;
        }
        const uint64_t replay = g_replayPasses.fetch_add(1, std::memory_order_relaxed) + 1;
        if (replay <= 8 || replay % 300 == 0) {
            Logger::Instance().Write(
                LogLevel::Info,
                "hpl_tone_mapping_frame replay=%llu effect=%p eye=%d poseFrame=%llu exposure=%.6f whiteCut=%.6f transition=%.6f",
                static_cast<unsigned long long>(replay),
                effect,
                eyeIndex,
                static_cast<unsigned long long>(poseFrame),
                state.baseline.exposure,
                state.baseline.whiteCut,
                state.baseline.transition);
        }
    }
    g_activePass = {effect, role, poseFrame};
}

void EndHPLToneMappingFrame(void* effect)
{
    const ActivePass pass = g_activePass;
    g_activePass = {};
    if (pass.role == PassRole::None) return;

    ToneMappingPacket output;
    if (effect != pass.effect || !ReadPacket(effect, output)) {
        std::lock_guard lock(g_mutex);
        Fault("output_read_failed", effect);
        return;
    }

    std::lock_guard lock(g_mutex);
    const auto found = g_effects.find(effect);
    if (found == g_effects.end()) {
        Fault("tracked_effect_missing", effect);
        return;
    }
    EffectState& state = found->second;
    if (pass.role == PassRole::FirstEye) {
        state.committed = output;
        state.committedValid = true;
    } else {
        if (!state.committedValid) {
            Fault("committed_packet_missing", effect);
            return;
        }
        if (!Equivalent(output, state.committed)) {
            const uint64_t mismatch = g_mismatches.fetch_add(1, std::memory_order_relaxed) + 1;
            Logger::Instance().Write(
                LogLevel::Warn,
                "hpl_tone_mapping_frame mismatch=%llu effect=%p poseFrame=%llu firstExposure=%.6f replayExposure=%.6f firstWhiteCut=%.6f replayWhiteCut=%.6f action=restore_first_commit",
                static_cast<unsigned long long>(mismatch),
                effect,
                static_cast<unsigned long long>(pass.poseFrame),
                state.committed.exposure,
                output.exposure,
                state.committed.whiteCut,
                output.whiteCut);
        }
        if (!WritePacket(effect, state.committed)) {
            Fault("committed_packet_restore_failed", effect);
            return;
        }
        g_committedRestores.fetch_add(1, std::memory_order_relaxed);
    }
    if (!tone_mapping_frame_math::Commit(state.schedule, pass.role)) {
        Fault("schedule_commit_failed", effect);
    }
}

HPLToneMappingFrameStatus GetHPLToneMappingFrameStatus()
{
    std::lock_guard lock(g_mutex);
    return {
        g_configured,
        g_configured && !g_faulted,
        g_faulted,
        static_cast<uint64_t>(g_effects.size()),
        g_firstPasses.load(std::memory_order_relaxed),
        g_replayPasses.load(std::memory_order_relaxed),
        g_committedRestores.load(std::memory_order_relaxed),
        g_mismatches.load(std::memory_order_relaxed),
        g_failures.load(std::memory_order_relaxed),
    };
}

void RemoveHPLToneMappingFrame()
{
    const HPLToneMappingFrameStatus status = GetHPLToneMappingFrameStatus();
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_tone_mapping_frame removed trackedEffects=%llu firstPasses=%llu replayPasses=%llu committedRestores=%llu mismatches=%llu failures=%llu",
        static_cast<unsigned long long>(status.trackedEffects),
        static_cast<unsigned long long>(status.firstPasses),
        static_cast<unsigned long long>(status.replayPasses),
        static_cast<unsigned long long>(status.committedRestores),
        static_cast<unsigned long long>(status.mismatches),
        static_cast<unsigned long long>(status.failures));
    std::lock_guard lock(g_mutex);
    g_effects.clear();
    g_activePass = {};
    g_configured = false;
    g_faulted = false;
}

} // namespace somavr
