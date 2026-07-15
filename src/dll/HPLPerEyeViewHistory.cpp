#include "HPLPerEyeViewHistory.h"

#include "Config.h"
#include "HPLPerEyeViewHistoryMath.h"
#include "Logger.h"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstring>

namespace somavr {
namespace {

using per_eye_view_history_math::Bank;
using per_eye_view_history_math::PrepareResult;
using per_eye_view_history_math::ViewHistoryPacket;

constexpr size_t kRendererHistoryStateOffset = 0x438;
constexpr size_t kPreviousViewMatrixOffset = 0x80;

struct ActivePass {
    bool active = false;
    void* renderer = nullptr;
    void* historyAddress = nullptr;
    int eyeIndex = -1;
    uint64_t poseFrame = 0;
};

bool g_configured = false;
bool g_faulted = false;
void* g_observedRenderer = nullptr;
Bank g_bank;
ActivePass g_pass;
std::atomic<uint64_t> g_activations = 0;
std::atomic<uint64_t> g_resets = 0;
std::atomic<uint64_t> g_seeds = 0;
std::atomic<uint64_t> g_restores = 0;
std::atomic<uint64_t> g_captures = 0;
std::atomic<uint64_t> g_failures = 0;

bool IsReadableProtection(DWORD protection)
{
    if ((protection & (PAGE_GUARD | PAGE_NOACCESS)) != 0) return false;
    const DWORD base = protection & 0xff;
    return base == PAGE_READONLY
        || base == PAGE_READWRITE
        || base == PAGE_WRITECOPY
        || base == PAGE_EXECUTE_READ
        || base == PAGE_EXECUTE_READWRITE
        || base == PAGE_EXECUTE_WRITECOPY;
}

bool IsWritableProtection(DWORD protection)
{
    if ((protection & (PAGE_GUARD | PAGE_NOACCESS)) != 0) return false;
    const DWORD base = protection & 0xff;
    return base == PAGE_READWRITE
        || base == PAGE_WRITECOPY
        || base == PAGE_EXECUTE_READWRITE
        || base == PAGE_EXECUTE_WRITECOPY;
}

bool IsAccessibleSpan(const void* address, size_t size, bool writable)
{
    if (address == nullptr || size == 0) return false;
    const auto* cursor = static_cast<const uint8_t*>(address);
    size_t checked = 0;
    while (checked < size) {
        MEMORY_BASIC_INFORMATION info{};
        if (VirtualQuery(cursor + checked, &info, sizeof(info)) == 0
            || info.State != MEM_COMMIT
            || !(writable ? IsWritableProtection(info.Protect) : IsReadableProtection(info.Protect))) {
            return false;
        }
        const uintptr_t regionEnd = reinterpret_cast<uintptr_t>(info.BaseAddress) + info.RegionSize;
        const uintptr_t current = reinterpret_cast<uintptr_t>(cursor + checked);
        if (regionEnd <= current) return false;
        const size_t available = static_cast<size_t>(regionEnd - current);
        checked += (std::min)(available, size - checked);
    }
    return true;
}

bool ReadPacket(const void* address, ViewHistoryPacket& packet)
{
    if (!IsAccessibleSpan(address, packet.size(), false)) return false;
    std::memcpy(packet.data(), address, packet.size());
    return true;
}

bool WritePacket(void* address, const ViewHistoryPacket& packet)
{
    if (!IsAccessibleSpan(address, packet.size(), true)) return false;
    std::memcpy(address, packet.data(), packet.size());
    return true;
}

bool ReadPointer(const void* object, size_t offset, void*& value)
{
    value = nullptr;
    if (object == nullptr) return false;
    const void* address = static_cast<const uint8_t*>(object) + offset;
    if (!IsAccessibleSpan(address, sizeof(value), false)) return false;
    std::memcpy(&value, address, sizeof(value));
    return value != nullptr;
}

bool ShouldLog(uint64_t sequence)
{
    return sequence <= 8 || sequence % 300 == 0;
}

void Fault(const char* reason, void* renderer, void* historyAddress, int eyeIndex, uint64_t poseFrame)
{
    const uint64_t failure = g_failures.fetch_add(1, std::memory_order_relaxed) + 1;
    g_faulted = true;
    g_pass = {};
    per_eye_view_history_math::SetActive(g_bank, false);
    Logger::Instance().Write(
        LogLevel::Error,
        "hpl_per_eye_view_history fault=1 reason=%s renderer=%p history=%p eye=%d poseFrame=%llu failures=%llu fallback=shared_native_history_until_toggle",
        reason,
        renderer,
        historyAddress,
        eyeIndex,
        static_cast<unsigned long long>(poseFrame),
        static_cast<unsigned long long>(failure));
}

} // namespace

void InitializeHPLPerEyeViewHistory(const Config& config)
{
    g_configured = config.hplPerEyeViewHistoryControl;
    g_faulted = false;
    g_observedRenderer = nullptr;
    g_bank = {};
    g_pass = {};
    g_activations.store(0, std::memory_order_relaxed);
    g_resets.store(0, std::memory_order_relaxed);
    g_seeds.store(0, std::memory_order_relaxed);
    g_restores.store(0, std::memory_order_relaxed);
    g_captures.store(0, std::memory_order_relaxed);
    g_failures.store(0, std::memory_order_relaxed);
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_per_eye_view_history initialized configured=%d packetBytes=0x%llx policy=continuous_exact_player_only_fail_closed",
        g_configured ? 1 : 0,
        static_cast<unsigned long long>(per_eye_view_history_math::kViewHistoryPacketSize));
}

void SetHPLPerEyeViewHistoryActive(bool active, const char* source)
{
    const bool requested = g_configured && active;
    if (!requested) {
        if (g_bank.active || g_faulted) {
            per_eye_view_history_math::SetActive(g_bank, false);
            g_pass = {};
            g_faulted = false;
            g_resets.fetch_add(1, std::memory_order_relaxed);
            Logger::Instance().Write(
                LogLevel::Info,
                "hpl_per_eye_view_history active=0 source=%s faultCleared=1",
                source != nullptr ? source : "unknown");
        }
        return;
    }
    if (g_faulted || g_bank.active) return;

    per_eye_view_history_math::SetActive(g_bank, true);
    const uint64_t activation = g_activations.fetch_add(1, std::memory_order_relaxed) + 1;
    Logger::Instance().Write(
        LogLevel::Warn,
        "hpl_per_eye_view_history active=1 source=%s activation=%llu packet=previous_view_matrix_0x40",
        source != nullptr ? source : "unknown",
        static_cast<unsigned long long>(activation));
}

void ObserveHPLPerEyeViewHistoryRenderer(void* renderer)
{
    if (renderer != nullptr) g_observedRenderer = renderer;
}

void BeginHPLPerEyeViewHistoryPass(int eyeIndex, uint64_t poseFrame)
{
    g_pass = {};
    void* renderer = g_observedRenderer;
    if (!g_configured || !g_bank.active || g_faulted
        || renderer == nullptr || (eyeIndex != 0 && eyeIndex != 1) || poseFrame == 0) {
        return;
    }

    void* historyState = nullptr;
    if (!ReadPointer(renderer, kRendererHistoryStateOffset, historyState)) {
        Fault("history_state_unreadable", renderer, nullptr, eyeIndex, poseFrame);
        return;
    }
    void* historyAddress = static_cast<uint8_t*>(historyState) + kPreviousViewMatrixOffset;
    ViewHistoryPacket livePacket{};
    if (!ReadPacket(historyAddress, livePacket)) {
        Fault("previous_view_unreadable", renderer, historyAddress, eyeIndex, poseFrame);
        return;
    }

    const PrepareResult prepared = per_eye_view_history_math::Prepare(
        g_bank,
        reinterpret_cast<uintptr_t>(renderer),
        reinterpret_cast<uintptr_t>(historyAddress),
        eyeIndex,
        poseFrame,
        livePacket);
    if (!prepared.valid || !WritePacket(historyAddress, prepared.restorePacket)) {
        Fault("previous_view_restore_failed", renderer, historyAddress, eyeIndex, poseFrame);
        return;
    }

    if (prepared.reset) g_resets.fetch_add(1, std::memory_order_relaxed);
    if (prepared.seeded) g_seeds.fetch_add(1, std::memory_order_relaxed);
    const uint64_t restore = g_restores.fetch_add(1, std::memory_order_relaxed) + 1;
    if (prepared.reset || prepared.seeded || ShouldLog(restore)) {
        Logger::Instance().Write(
            LogLevel::Warn,
            "hpl_per_eye_view_history restore=%llu eye=%d poseFrame=%llu renderer=%p history=%p reset=%d seeded=%d",
            static_cast<unsigned long long>(restore),
            eyeIndex,
            static_cast<unsigned long long>(poseFrame),
            renderer,
            historyAddress,
            prepared.reset ? 1 : 0,
            prepared.seeded ? 1 : 0);
    }

    g_pass.active = true;
    g_pass.renderer = renderer;
    g_pass.historyAddress = historyAddress;
    g_pass.eyeIndex = eyeIndex;
    g_pass.poseFrame = poseFrame;
}

void EndHPLPerEyeViewHistoryPass(int actualEyeIndex, uint64_t actualPoseFrame)
{
    const ActivePass pass = g_pass;
    g_pass = {};
    if (!pass.active) return;

    if (actualEyeIndex != pass.eyeIndex || actualPoseFrame != pass.poseFrame) {
        Fault("render_eye_sequence_mismatch", pass.renderer, pass.historyAddress,
            actualEyeIndex, actualPoseFrame);
        return;
    }

    ViewHistoryPacket packet{};
    if (!ReadPacket(pass.historyAddress, packet)
        || !per_eye_view_history_math::Commit(
            g_bank,
            reinterpret_cast<uintptr_t>(pass.renderer),
            reinterpret_cast<uintptr_t>(pass.historyAddress),
            pass.eyeIndex,
            pass.poseFrame,
            packet)) {
        Fault("previous_view_capture_failed", pass.renderer, pass.historyAddress,
            pass.eyeIndex, pass.poseFrame);
        return;
    }

    const uint64_t capture = g_captures.fetch_add(1, std::memory_order_relaxed) + 1;
    if (ShouldLog(capture)) {
        Logger::Instance().Write(
            LogLevel::Info,
            "hpl_per_eye_view_history capture=%llu eye=%d poseFrame=%llu renderer=%p history=%p",
            static_cast<unsigned long long>(capture),
            pass.eyeIndex,
            static_cast<unsigned long long>(pass.poseFrame),
            pass.renderer,
            pass.historyAddress);
    }
}

HPLPerEyeViewHistoryStatus GetHPLPerEyeViewHistoryStatus()
{
    return {
        g_configured,
        g_bank.active,
        g_faulted,
        g_activations.load(std::memory_order_relaxed),
        g_resets.load(std::memory_order_relaxed),
        g_seeds.load(std::memory_order_relaxed),
        g_restores.load(std::memory_order_relaxed),
        g_captures.load(std::memory_order_relaxed),
        g_failures.load(std::memory_order_relaxed),
    };
}

void RemoveHPLPerEyeViewHistory()
{
    g_configured = false;
    g_faulted = false;
    g_observedRenderer = nullptr;
    g_bank = {};
    g_pass = {};
}

} // namespace somavr
