#include "HPLContactHapticsBridge.h"

#include "HPLCameraBridge.h"
#include "HPLContactHapticsMath.h"
#include "HPLInteractionBridge.h"
#include "HPLPlayerState.h"
#include "Logger.h"

#include <Windows.h>

#include <MinHook.h>

#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>

namespace somavr {
namespace {

constexpr uintptr_t kSurfaceImpactRva = 0x32f0e0;
constexpr int kGrabPlayerState = 1;
constexpr uint8_t kSurfaceImpactSignature[] = {
    0x40, 0x55, 0x56, 0x41, 0x56, 0x48, 0x8d, 0x6c,
    0x24, 0xc1, 0x48, 0x81, 0xec, 0x90, 0x00, 0x00,
    0x00, 0x48, 0x8b, 0xf1, 0x48, 0x8b, 0x49, 0x18,
    0x0f, 0x29, 0x74, 0x24, 0x70, 0x4d, 0x8b, 0xf0,
};

using SurfaceImpactFn = void (*)(
    void* surfaceData,
    float speed,
    const float* position,
    int contacts,
    void* body);

Config g_config;
OpenXRRuntime* g_openxr = nullptr;
SurfaceImpactFn g_originalSurfaceImpact = nullptr;
void* g_surfaceImpactTarget = nullptr;
std::mutex g_installMutex;
std::mutex g_stateMutex;
contact_haptics_math::State g_state;
std::atomic<uint64_t> g_calls = 0;
std::atomic<uint64_t> g_grabStateCandidates = 0;
std::atomic<uint64_t> g_poseRejects = 0;
std::atomic<uint64_t> g_staleRejects = 0;
std::atomic<uint64_t> g_invalidRejects = 0;
std::atomic<uint64_t> g_speedRejects = 0;
std::atomic<uint64_t> g_distanceRejects = 0;
std::atomic<uint64_t> g_cooldownRejects = 0;
std::atomic<uint64_t> g_pulses = 0;
std::atomic<uint64_t> g_applied = 0;

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

void CountReject(contact_haptics_math::RejectReason reason)
{
    switch (reason) {
    case contact_haptics_math::RejectReason::Ineligible:
    case contact_haptics_math::RejectReason::NonFinite:
        g_invalidRejects.fetch_add(1, std::memory_order_relaxed);
        break;
    case contact_haptics_math::RejectReason::BelowSpeed:
        g_speedRejects.fetch_add(1, std::memory_order_relaxed);
        break;
    case contact_haptics_math::RejectReason::TooFar:
        g_distanceRejects.fetch_add(1, std::memory_order_relaxed);
        break;
    case contact_haptics_math::RejectReason::Cooldown:
        g_cooldownRejects.fetch_add(1, std::memory_order_relaxed);
        break;
    default:
        break;
    }
}

void HookSurfaceImpact(
    void* surfaceData,
    float speed,
    const float* position,
    int contacts,
    void* body)
{
    g_originalSurfaceImpact(surfaceData, speed, position, contacts, body);
    g_calls.fetch_add(1, std::memory_order_relaxed);

    if (!g_config.hplControllerInput
        || !g_config.hplControllerHaptics
        || !g_config.hplControllerContactHaptics
        || g_openxr == nullptr
        || position == nullptr) {
        return;
    }

    HPLPlayerStateSnapshot player;
    if (!GetHPLPlayerStateSnapshot(player)
        || !player.playerValid
        || player.playerStateId != kGrabPlayerState
        || player.authoredCameraActive) {
        return;
    }
    g_grabStateCandidates.fetch_add(1, std::memory_order_relaxed);

    OpenXRInputSnapshot input;
    if (!g_openxr->GetLatestInput(input) || !input.active || input.gameFrame == 0) {
        g_poseRejects.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    if (player.frame > input.gameFrame
        && player.frame - input.gameFrame
            > static_cast<uint64_t>(g_config.hplControllerMaxInputAgeFrames)) {
        g_staleRejects.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    uint32_t handIndex = g_config.hplControllerDominantHand == "left" ? 0u : 1u;
    GetHPLInteractionOwnerHand(input.gameFrame, 120, handIndex);
    const OpenXRHandInput& hand = handIndex == 0 ? input.left : input.right;
    HPLTrackedPoseWorld grip;
    if (!hand.active
        || !hand.gripPose.valid
        || !hand.gripPose.positionTracked
        || !ResolveHPLTrackedPoseWorld(hand.gripPose, input.gameFrame, grip)
        || !grip.valid
        || !grip.positionTracked) {
        g_poseRejects.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    const float dx = position[0] - grip.positionX;
    const float dy = position[1] - grip.positionY;
    const float dz = position[2] - grip.positionZ;
    const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
    contact_haptics_math::Settings settings;
    settings.minSpeed = g_config.hplControllerContactHapticMinSpeed * g_config.hplWorldScale;
    settings.maxSpeed = g_config.hplControllerContactHapticMaxSpeed * g_config.hplWorldScale;
    settings.maxDistance = g_config.hplControllerContactHapticMaxDistanceMeters * g_config.hplWorldScale;
    settings.minAmplitude = g_config.hplControllerContactHapticMinAmplitude;
    settings.maxAmplitude = g_config.hplControllerContactHapticMaxAmplitude;
    settings.durationMs = g_config.hplControllerContactHapticDurationMs;
    settings.cooldownMs = static_cast<uint64_t>(g_config.hplControllerContactHapticCooldownMs);

    contact_haptics_math::Decision decision;
    {
        std::lock_guard lock(g_stateMutex);
        decision = contact_haptics_math::Evaluate(
            g_state, true, speed, distance, GetTickCount64(), settings);
    }
    if (!decision.pulse) {
        CountReject(decision.reject);
        return;
    }

    const uint64_t pulse = g_pulses.fetch_add(1, std::memory_order_relaxed) + 1;
    const bool applied = g_openxr->RequestHapticPulse(
        handIndex, decision.amplitude, decision.durationMs, "native_grab_contact");
    if (applied) g_applied.fetch_add(1, std::memory_order_relaxed);
    if (pulse <= 12
        || pulse % static_cast<uint64_t>(g_config.hplControllerLogInterval) == 0) {
        Logger::Instance().Write(
            LogLevel::Info,
            "hpl_contact_haptics pulse=%llu hand=%s speed=%.3f contacts=%d distanceWorld=%.3f amplitude=%.3f durationMs=%d body=%p applied=%d policy=grab_state_near_interaction_owner_grip",
            static_cast<unsigned long long>(pulse),
            handIndex == 0 ? "left" : "right",
            speed,
            contacts,
            distance,
            decision.amplitude,
            decision.durationMs,
            body,
            applied ? 1 : 0);
    }
}

} // namespace

bool InstallHPLContactHapticsBridge(const Config& config, OpenXRRuntime* openxr)
{
    std::lock_guard lock(g_installMutex);
    g_config = config;
    g_openxr = openxr;
    if (!config.hplControllerInput
        || !config.hplControllerHaptics
        || !config.hplControllerContactHaptics) {
        Logger::Instance().Write(LogLevel::Info, "hpl_contact_haptics disabled config=0");
        return true;
    }
    if (g_surfaceImpactTarget != nullptr) return true;

    HMODULE executable = GetModuleHandleW(nullptr);
    if (!IsInsideImage(executable, kSurfaceImpactRva, sizeof(kSurfaceImpactSignature))) {
        Logger::Instance().Write(LogLevel::Error,
            "hpl_contact_haptics install_failed reason=invalid_image_range");
        return false;
    }
    auto* target = reinterpret_cast<std::byte*>(executable) + kSurfaceImpactRva;
    if (std::memcmp(target, kSurfaceImpactSignature, sizeof(kSurfaceImpactSignature)) != 0) {
        Logger::Instance().Write(LogLevel::Error,
            "hpl_contact_haptics install_failed reason=signature_mismatch rva=0x%llx",
            static_cast<unsigned long long>(kSurfaceImpactRva));
        return false;
    }

    MH_STATUS status = MH_CreateHook(target, reinterpret_cast<void*>(&HookSurfaceImpact),
        reinterpret_cast<void**>(&g_originalSurfaceImpact));
    if (status != MH_OK && status != MH_ERROR_ALREADY_CREATED) {
        Logger::Instance().Write(LogLevel::Error,
            "hpl_contact_haptics install_failed reason=create_hook status=%s",
            MH_StatusToString(status));
        return false;
    }
    status = MH_EnableHook(target);
    if (status != MH_OK && status != MH_ERROR_ENABLED) {
        MH_RemoveHook(target);
        g_originalSurfaceImpact = nullptr;
        Logger::Instance().Write(LogLevel::Error,
            "hpl_contact_haptics install_failed reason=enable_hook status=%s",
            MH_StatusToString(status));
        return false;
    }

    g_surfaceImpactTarget = target;
    Logger::Instance().Write(LogLevel::Info,
        "hpl_contact_haptics install_ok function=cSurfaceData::OnImpact rva=0x%llx target=%p fallbackHand=%s speedMeters=%.3f..%.3f distanceMeters=%.3f amplitude=%.3f..%.3f durationMs=%d cooldownMs=%d policy=grab_state_near_interaction_owner_grip",
        static_cast<unsigned long long>(kSurfaceImpactRva),
        target,
        config.hplControllerDominantHand.c_str(),
        config.hplControllerContactHapticMinSpeed,
        config.hplControllerContactHapticMaxSpeed,
        config.hplControllerContactHapticMaxDistanceMeters,
        config.hplControllerContactHapticMinAmplitude,
        config.hplControllerContactHapticMaxAmplitude,
        config.hplControllerContactHapticDurationMs,
        config.hplControllerContactHapticCooldownMs);
    return true;
}

void RemoveHPLContactHapticsBridge()
{
    std::lock_guard lock(g_installMutex);
    if (g_surfaceImpactTarget != nullptr) {
        MH_DisableHook(g_surfaceImpactTarget);
        MH_RemoveHook(g_surfaceImpactTarget);
    }
    g_surfaceImpactTarget = nullptr;
    g_originalSurfaceImpact = nullptr;
    {
        std::lock_guard stateLock(g_stateMutex);
        g_state = {};
    }
    g_openxr = nullptr;
    Logger::Instance().Write(LogLevel::Info, "hpl_contact_haptics removed");
}

void LogHPLContactHapticsBridgeSummary()
{
    Logger::Instance().Write(LogLevel::Info,
        "hpl_contact_haptics_summary installed=%d calls=%llu grabStateCandidates=%llu pulses=%llu applied=%llu rejects={pose=%llu stale=%llu invalid=%llu speed=%llu distance=%llu cooldown=%llu}",
        g_surfaceImpactTarget != nullptr ? 1 : 0,
        static_cast<unsigned long long>(g_calls.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_grabStateCandidates.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_pulses.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_applied.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_poseRejects.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_staleRejects.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_invalidRejects.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_speedRejects.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_distanceRejects.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_cooldownRejects.load(std::memory_order_relaxed)));
}

} // namespace somavr
