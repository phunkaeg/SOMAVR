#include "HPLGameplayHapticsBridge.h"

#include "HPLGameplayHapticsMath.h"
#include "Logger.h"

#include <Windows.h>

#include <MinHook.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>

namespace somavr {
namespace {

constexpr uintptr_t kSetRumbleRva = 0x109b30;
constexpr uint8_t kSetRumbleSignature[] = {
    0x48, 0x89, 0x5c, 0x24, 0x08, 0x57, 0x48, 0x83,
    0xec, 0x40, 0x80, 0xb9, 0xb3, 0x01, 0x00, 0x00,
    0x00, 0x0f, 0x29, 0x74, 0x24, 0x30, 0x0f, 0x29,
    0x7c, 0x24, 0x20, 0x8b, 0xfa, 0x48, 0x8b, 0xd9,
};

using SetRumbleFn = void (*)(void* inputHandler, int device, float strength, float durationSeconds);

Config g_config;
OpenXRRuntime* g_openxr = nullptr;
SetRumbleFn g_originalSetRumble = nullptr;
void* g_setRumbleTarget = nullptr;
std::mutex g_installMutex;
std::mutex g_stateMutex;
gameplay_haptics_math::State g_state;
std::atomic<uint64_t> g_calls = 0;
std::atomic<uint64_t> g_nonzeroCalls = 0;
std::atomic<uint64_t> g_pulses = 0;
std::atomic<uint64_t> g_appliedHands = 0;
std::atomic<uint64_t> g_stops = 0;
std::atomic<uint64_t> g_stoppedHands = 0;
std::atomic<uint64_t> g_throttled = 0;

bool IsInsideImage(HMODULE module, uintptr_t rva, size_t bytes)
{
    if (module == nullptr) return false;
    const auto* base = reinterpret_cast<const std::byte*>(module);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    return nt->Signature == IMAGE_NT_SIGNATURE
        && rva <= nt->OptionalHeader.SizeOfImage
        && bytes <= nt->OptionalHeader.SizeOfImage - rva;
}

void HookSetRumble(void* inputHandler, int device, float strength, float durationSeconds)
{
    g_calls.fetch_add(1, std::memory_order_relaxed);
    g_originalSetRumble(inputHandler, device, strength, durationSeconds);

    if (!g_config.hplControllerInput
        || !g_config.hplControllerHaptics
        || !g_config.hplControllerGameplayHaptics
        || g_openxr == nullptr) {
        return;
    }
    if (strength > 0.0f && durationSeconds > 0.0f) {
        g_nonzeroCalls.fetch_add(1, std::memory_order_relaxed);
    }

    gameplay_haptics_math::Settings settings;
    settings.amplitudeScale = g_config.hplControllerGameplayHapticAmplitudeScale;
    settings.minAmplitude = g_config.hplControllerGameplayHapticMinAmplitude;
    settings.retriggerDelta = g_config.hplControllerGameplayHapticRetriggerDelta;
    settings.refreshMs = static_cast<uint64_t>(g_config.hplControllerGameplayHapticRefreshMs);
    settings.segmentMs = g_config.hplControllerGameplayHapticSegmentMs;
    gameplay_haptics_math::Decision decision;
    {
        std::lock_guard lock(g_stateMutex);
        decision = gameplay_haptics_math::Update(
            g_state, strength, durationSeconds, GetTickCount64(), settings);
    }

    if (decision.action == gameplay_haptics_math::Action::None) {
        if (strength > 0.0f) g_throttled.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    if (decision.action == gameplay_haptics_math::Action::Stop) {
        g_stops.fetch_add(1, std::memory_order_relaxed);
        for (uint32_t hand = 0; hand < 2; ++hand) {
            if (g_openxr->StopHaptic(hand, "native_gameplay_rumble")) {
                g_stoppedHands.fetch_add(1, std::memory_order_relaxed);
            }
        }
        return;
    }

    const uint64_t pulse = g_pulses.fetch_add(1, std::memory_order_relaxed) + 1;
    uint32_t applied = 0;
    for (uint32_t hand = 0; hand < 2; ++hand) {
        if (g_openxr->RequestHapticPulse(
                hand, decision.amplitude, decision.durationMs, "native_gameplay_rumble")) {
            ++applied;
            g_appliedHands.fetch_add(1, std::memory_order_relaxed);
        }
    }
    if (pulse <= 8 || pulse % static_cast<uint64_t>(g_config.hplControllerLogInterval) == 0) {
        Logger::Instance().Write(
            LogLevel::Info,
            "hpl_gameplay_haptics pulse=%llu device=%d nativeStrength=%.3f nativeDuration=%.3f amplitude=%.3f segmentMs=%d appliedHands=%u",
            static_cast<unsigned long long>(pulse), device, strength, durationSeconds,
            decision.amplitude, decision.durationMs, applied);
    }
}

} // namespace

bool InstallHPLGameplayHapticsBridge(const Config& config, OpenXRRuntime* openxr)
{
    std::lock_guard lock(g_installMutex);
    g_config = config;
    g_openxr = openxr;
    if (!config.hplControllerInput
        || !config.hplControllerGameplayHaptics
        || !config.hplControllerHaptics) {
        Logger::Instance().Write(LogLevel::Info, "hpl_gameplay_haptics disabled config=0");
        return true;
    }
    if (g_setRumbleTarget != nullptr) return true;

    HMODULE executable = GetModuleHandleW(nullptr);
    if (!IsInsideImage(executable, kSetRumbleRva, sizeof(kSetRumbleSignature))) {
        Logger::Instance().Write(LogLevel::Error, "hpl_gameplay_haptics install_failed reason=invalid_image_range");
        return false;
    }
    auto* target = reinterpret_cast<std::byte*>(executable) + kSetRumbleRva;
    if (std::memcmp(target, kSetRumbleSignature, sizeof(kSetRumbleSignature)) != 0) {
        Logger::Instance().Write(LogLevel::Error,
            "hpl_gameplay_haptics install_failed reason=signature_mismatch rva=0x%llx",
            static_cast<unsigned long long>(kSetRumbleRva));
        return false;
    }

    MH_STATUS status = MH_CreateHook(target, reinterpret_cast<void*>(&HookSetRumble),
        reinterpret_cast<void**>(&g_originalSetRumble));
    if (status != MH_OK && status != MH_ERROR_ALREADY_CREATED) {
        Logger::Instance().Write(LogLevel::Error,
            "hpl_gameplay_haptics install_failed reason=create_hook status=%s",
            MH_StatusToString(status));
        return false;
    }
    status = MH_EnableHook(target);
    if (status != MH_OK && status != MH_ERROR_ENABLED) {
        MH_RemoveHook(target);
        g_originalSetRumble = nullptr;
        Logger::Instance().Write(LogLevel::Error,
            "hpl_gameplay_haptics install_failed reason=enable_hook status=%s",
            MH_StatusToString(status));
        return false;
    }

    g_setRumbleTarget = target;
    Logger::Instance().Write(LogLevel::Info,
        "hpl_gameplay_haptics install_ok function=SetRumble rva=0x%llx target=%p policy=authored_global_bilateral scale=%.3f minAmplitude=%.3f retriggerDelta=%.3f refreshMs=%d segmentMs=%d",
        static_cast<unsigned long long>(kSetRumbleRva), target,
        config.hplControllerGameplayHapticAmplitudeScale,
        config.hplControllerGameplayHapticMinAmplitude,
        config.hplControllerGameplayHapticRetriggerDelta,
        config.hplControllerGameplayHapticRefreshMs,
        config.hplControllerGameplayHapticSegmentMs);
    return true;
}

void RemoveHPLGameplayHapticsBridge()
{
    std::lock_guard lock(g_installMutex);
    if (g_setRumbleTarget != nullptr) {
        MH_DisableHook(g_setRumbleTarget);
        MH_RemoveHook(g_setRumbleTarget);
    }
    g_setRumbleTarget = nullptr;
    g_originalSetRumble = nullptr;
    {
        std::lock_guard stateLock(g_stateMutex);
        g_state = {};
    }
    g_openxr = nullptr;
    Logger::Instance().Write(LogLevel::Info, "hpl_gameplay_haptics removed");
}

void LogHPLGameplayHapticsBridgeSummary()
{
    Logger::Instance().Write(LogLevel::Info,
        "hpl_gameplay_haptics_summary installed=%d calls=%llu nonzeroCalls=%llu pulses=%llu appliedHands=%llu stops=%llu stoppedHands=%llu throttled=%llu",
        g_setRumbleTarget != nullptr ? 1 : 0,
        static_cast<unsigned long long>(g_calls.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_nonzeroCalls.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_pulses.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_appliedHands.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_stops.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_stoppedHands.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_throttled.load(std::memory_order_relaxed)));
}

} // namespace somavr
