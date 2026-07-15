#include "HPLInteractionBridge.h"

#include "HPLCameraBridge.h"
#include "HPLHudMath.h"
#include "HPLPlayerState.h"
#include "Logger.h"

#include <Windows.h>

#include <MinHook.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>

namespace somavr {
namespace {

constexpr uintptr_t kGetClosestEntityRva = 0x0cd750;
constexpr uint8_t kGetClosestEntitySignature[] = {
    0x48, 0x89, 0x5c, 0x24, 0x08,
    0x57,
    0x48, 0x83, 0xec, 0x50,
    0x48, 0x8b, 0xbc, 0x24, 0x88, 0x00, 0x00, 0x00,
};

using GetClosestEntityFn = bool (*)(
    const float* start,
    const float* direction,
    float rayLength,
    int interactType,
    bool checkLineOfSight,
    void* output);

Config g_config;
OpenXRRuntime* g_openxr = nullptr;
GetClosestEntityFn g_originalGetClosestEntity = nullptr;
void* g_getClosestEntityTarget = nullptr;
std::mutex g_installMutex;
std::atomic<uint64_t> g_calls = 0;
std::atomic<uint64_t> g_substitutions = 0;
std::atomic<uint64_t> g_substitutionHits = 0;
std::atomic<uint64_t> g_fallbackDisabled = 0;
std::atomic<uint64_t> g_fallbackQueryType = 0;
std::atomic<uint64_t> g_fallbackAuthoredCamera = 0;
std::atomic<uint64_t> g_fallbackCamera = 0;
std::atomic<uint64_t> g_fallbackInput = 0;
std::atomic<uint64_t> g_fallbackTracking = 0;
std::atomic<uint64_t> g_fallbackOrigin = 0;
std::atomic<uint64_t> g_hitSnapshots = 0;
std::atomic<uint64_t> g_hitPayloadRejects = 0;
std::atomic<uint64_t> g_hitPayloadDistanceRejects = 0;
std::atomic<uint64_t> g_hitPayloadNullTargets = 0;
std::atomic<uint64_t> g_reticleUpdates = 0;
std::atomic<uint64_t> g_semanticStates = 0;
std::atomic<uint64_t> g_semanticAccepted = 0;
std::atomic<uint64_t> g_focusHapticRequests = 0;
std::atomic<uint64_t> g_focusHapticApplied = 0;
std::atomic<uintptr_t> g_lastFocusTarget = 0;
std::atomic<uint64_t> g_lastFocusHapticFrame = 0;
std::atomic<int> g_lastSubstitutionHitState = -1;
std::atomic<int> g_lastSemanticState = -1;
std::mutex g_hitMutex;
HPLInteractionHitSnapshot g_latestHit;

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

bool IsFiniteVector(const float* value)
{
    return value != nullptr
        && std::isfinite(value[0])
        && std::isfinite(value[1])
        && std::isfinite(value[2]);
}

void ClearHitSnapshot()
{
    {
        std::lock_guard lock(g_hitMutex);
        g_latestHit.valid = false;
    }
    g_lastFocusTarget.store(0, std::memory_order_relaxed);
    if (g_openxr != nullptr) {
        g_openxr->ClearInteractionReticle();
    }
}

bool PublishHitSnapshot(
    void* output,
    uint64_t gameFrame,
    uint32_t handIndex,
    const OpenXRControllerPose& aimPose,
    const float* start,
    const float* direction,
    float rayLength)
{
    if (output == nullptr || !IsFiniteVector(start) || !IsFiniteVector(direction)) {
        g_hitPayloadRejects.fetch_add(1, std::memory_order_relaxed);
        ClearHitSnapshot();
        return false;
    }

    // Inner raycast writes distance +0x18, body +0x20, entity +0x28.
    const auto* bytes = static_cast<const std::byte*>(output);
    void* entity = nullptr;
    void* body = nullptr;
    float distance = 0.0f;
    std::memcpy(&distance, bytes + 0x18, sizeof(distance));
    std::memcpy(&body, bytes + 0x20, sizeof(body));
    std::memcpy(&entity, bytes + 0x28, sizeof(entity));
    if (entity == nullptr && body == nullptr) {
        g_hitPayloadNullTargets.fetch_add(1, std::memory_order_relaxed);
    }

    const float directionLength = std::sqrt(
        direction[0] * direction[0]
        + direction[1] * direction[1]
        + direction[2] * direction[2]);
    if (!std::isfinite(distance)
        || distance < 0.0f
        || distance > std::max(rayLength, 0.0f) + 0.01f
        || !std::isfinite(directionLength)
        || directionLength < 1.0e-5f) {
        const uint64_t rejected = g_hitPayloadRejects.fetch_add(
            1, std::memory_order_relaxed) + 1;
        g_hitPayloadDistanceRejects.fetch_add(1, std::memory_order_relaxed);
        if (rejected <= 12
            || rejected % static_cast<uint64_t>(
                std::max(g_config.hplControllerLogInterval, 1)) == 0) {
            Logger::Instance().Write(
                LogLevel::Warn,
                "hpl_interaction_payload rejected=%llu reason=distance_or_direction entity=%p body=%p distance=%.6f rayLength=%.6f directionLength=%.6f frame=%llu hand=%s",
                static_cast<unsigned long long>(rejected),
                entity,
                body,
                distance,
                rayLength,
                directionLength,
                static_cast<unsigned long long>(gameFrame),
                handIndex == 0 ? "left" : "right");
        }
        ClearHitSnapshot();
        return false;
    }

    const float inverseDirectionLength = 1.0f / directionLength;
    HPLInteractionHitSnapshot snapshot;
    snapshot.valid = true;
    snapshot.sequence = g_hitSnapshots.fetch_add(1, std::memory_order_relaxed) + 1;
    snapshot.gameFrame = gameFrame;
    snapshot.handIndex = handIndex;
    snapshot.distance = distance;
    snapshot.worldX = start[0] + direction[0] * inverseDirectionLength * distance;
    snapshot.worldY = start[1] + direction[1] * inverseDirectionLength * distance;
    snapshot.worldZ = start[2] + direction[2] * inverseDirectionLength * distance;
    snapshot.entity = entity;
    snapshot.body = body;
    {
        std::lock_guard lock(g_hitMutex);
        g_latestHit = snapshot;
    }

    if (g_openxr != nullptr) {
        OpenXRInteractionReticleState reticle;
        reticle.valid = true;
        reticle.gameFrame = gameFrame;
        reticle.handIndex = handIndex;
        reticle.distanceMeters = distance / std::max(g_config.hplWorldScale, 0.001f);
        reticle.aimPose = aimPose;
        g_openxr->SetInteractionReticle(reticle);
        g_reticleUpdates.fetch_add(1, std::memory_order_relaxed);
    }

    return true;
}

const OpenXRHandInput* SelectDominantHand(const OpenXRInputSnapshot& input, uint32_t& handIndex)
{
    handIndex = g_config.hplControllerDominantHand == "left" ? 0u : 1u;
    const OpenXRHandInput* preferred = handIndex == 0 ? &input.left : &input.right;
    if (preferred->active) return preferred;
    if (!g_config.hplControllerOneHandFallback) return nullptr;

    handIndex ^= 1u;
    const OpenXRHandInput* fallback = handIndex == 0 ? &input.left : &input.right;
    return fallback->active ? fallback : nullptr;
}

bool HookGetClosestEntity(
    const float* start,
    const float* direction,
    float rayLength,
    int interactType,
    bool checkLineOfSight,
    void* output)
{
    const uint64_t call = g_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    if (!g_config.hplControllerInteractionRay || !g_config.hplControllerInput) {
        g_fallbackDisabled.fetch_add(1, std::memory_order_relaxed);
        ClearHitSnapshot();
        return g_originalGetClosestEntity(start, direction, rayLength, interactType, checkLineOfSight, output);
    }
    if (interactType != 0) {
        g_fallbackQueryType.fetch_add(1, std::memory_order_relaxed);
        return g_originalGetClosestEntity(start, direction, rayLength, interactType, checkLineOfSight, output);
    }
    if (!IsFiniteVector(start) || !IsFiniteVector(direction)) {
        g_fallbackQueryType.fetch_add(1, std::memory_order_relaxed);
        ClearHitSnapshot();
        return g_originalGetClosestEntity(start, direction, rayLength, interactType, checkLineOfSight, output);
    }

    HPLPlayerStateSnapshot player;
    if (GetHPLPlayerStateSnapshot(player)
        && g_config.hplControllerSuppressDuringAuthoredCamera
        && player.authoredCameraActive) {
        g_fallbackAuthoredCamera.fetch_add(1, std::memory_order_relaxed);
        ClearHitSnapshot();
        return g_originalGetClosestEntity(start, direction, rayLength, interactType, checkLineOfSight, output);
    }

    const HPLCameraBridgeStatus camera = GetHPLCameraBridgeStatus();
    if (!camera.trackingEnabled
        || !camera.cameraWorldPositionValid
        || !camera.headWorldRotationValid) {
        g_fallbackCamera.fetch_add(1, std::memory_order_relaxed);
        ClearHitSnapshot();
        return g_originalGetClosestEntity(start, direction, rayLength, interactType, checkLineOfSight, output);
    }

    OpenXRInputSnapshot input;
    uint32_t handIndex = 1;
    const OpenXRHandInput* hand = nullptr;
    if (g_openxr == nullptr
        || !g_openxr->GetLatestInput(input)
        || !input.active
        || (camera.headPoseFrame >= input.gameFrame
            && camera.headPoseFrame - input.gameFrame
                > static_cast<uint64_t>(g_config.hplControllerMaxInputAgeFrames))
        || (hand = SelectDominantHand(input, handIndex)) == nullptr) {
        g_fallbackInput.fetch_add(1, std::memory_order_relaxed);
        ClearHitSnapshot();
        return g_originalGetClosestEntity(start, direction, rayLength, interactType, checkLineOfSight, output);
    }

    HPLTrackedPoseWorld worldAim;
    if (!hand->aimPose.valid
        || !hand->aimPose.orientationTracked
        || !hand->aimPose.positionTracked
        || !ResolveHPLTrackedPoseWorld(hand->aimPose, input.gameFrame, worldAim)
        || !worldAim.orientationTracked
        || !worldAim.positionTracked) {
        g_fallbackTracking.fetch_add(1, std::memory_order_relaxed);
        ClearHitSnapshot();
        return g_originalGetClosestEntity(start, direction, rayLength, interactType, checkLineOfSight, output);
    }

    const float dx = start[0] - camera.cameraWorldPositionX;
    const float dy = start[1] - camera.cameraWorldPositionY;
    const float dz = start[2] - camera.cameraWorldPositionZ;
    const float originDelta = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (!std::isfinite(originDelta)
        || originDelta > g_config.hplControllerInteractionRayOriginTolerance) {
        const uint64_t fallback = g_fallbackOrigin.fetch_add(1, std::memory_order_relaxed) + 1;
        if (fallback <= 4 || fallback % static_cast<uint64_t>(std::max(g_config.hplControllerLogInterval, 1)) == 0) {
            Logger::Instance().Write(
                LogLevel::Info,
                "hpl_interaction_ray call=%llu applied=0 reason=origin_mismatch nativeStart=%.4f,%.4f,%.4f cameraOrigin=%.4f,%.4f,%.4f originDelta=%.4f tolerance=%.4f type=%d",
                static_cast<unsigned long long>(call),
                start[0], start[1], start[2],
                camera.cameraWorldPositionX, camera.cameraWorldPositionY, camera.cameraWorldPositionZ,
                originDelta,
                g_config.hplControllerInteractionRayOriginTolerance,
                interactType);
        }
        ClearHitSnapshot();
        return g_originalGetClosestEntity(start, direction, rayLength, interactType, checkLineOfSight, output);
    }

    const float controllerStart[3] = {worldAim.positionX, worldAim.positionY, worldAim.positionZ};
    const float controllerDirection[3] = {worldAim.forwardX, worldAim.forwardY, worldAim.forwardZ};
    const bool hit = g_originalGetClosestEntity(
        controllerStart,
        controllerDirection,
        rayLength,
        interactType,
        checkLineOfSight,
        output);
    const uint64_t substitution = g_substitutions.fetch_add(1, std::memory_order_relaxed) + 1;
    if (hit) g_substitutionHits.fetch_add(1, std::memory_order_relaxed);
    const bool hitSnapshot = hit
        ? PublishHitSnapshot(
            output,
            input.gameFrame,
            handIndex,
            hand->aimPose,
            controllerStart,
            controllerDirection,
            rayLength)
        : (ClearHitSnapshot(), false);

    const int hitState = hit ? (hitSnapshot ? 2 : 1) : 0;
    const int previousHitState = g_lastSubstitutionHitState.exchange(
        hitState, std::memory_order_relaxed);
    const bool hitStateChanged = previousHitState != hitState;

    HPLInteractionHitSnapshot snapshot;
    if (hitSnapshot) {
        GetHPLInteractionHitSnapshot(snapshot);
    }

    if (hitStateChanged
        || substitution <= 8
        || substitution % static_cast<uint64_t>(std::max(g_config.hplControllerLogInterval, 1)) == 0) {
        Logger::Instance().Write(
            LogLevel::Info,
            "hpl_interaction_ray call=%llu inputFrame=%llu applied=1 hit=%d hitSnapshot=%d transition=%d previousState=%d currentState=%d hand=%s nativeStart=%.4f,%.4f,%.4f controllerStart=%.4f,%.4f,%.4f controllerDir=%.5f,%.5f,%.5f hitDistance=%.4f hitWorld=%.4f,%.4f,%.4f entity=%p body=%p originDelta=%.4f rayLength=%.4f type=%d los=%d",
            static_cast<unsigned long long>(call),
            static_cast<unsigned long long>(input.gameFrame),
            hit ? 1 : 0,
            hitSnapshot ? 1 : 0,
            hitStateChanged ? 1 : 0,
            previousHitState,
            hitState,
            handIndex == 0 ? "left" : "right",
            start[0], start[1], start[2],
            controllerStart[0], controllerStart[1], controllerStart[2],
            controllerDirection[0], controllerDirection[1], controllerDirection[2],
            snapshot.distance,
            snapshot.worldX, snapshot.worldY, snapshot.worldZ,
            snapshot.entity,
            snapshot.body,
            originDelta,
            rayLength,
            interactType,
            checkLineOfSight ? 1 : 0);
    }
    return hit;
}

} // namespace

bool InstallHPLInteractionBridge(const Config& config, OpenXRRuntime* openxr)
{
    std::lock_guard lock(g_installMutex);
    g_config = config;
    g_openxr = openxr;
    if (!config.hplControllerInteractionRay) {
        Logger::Instance().Write(LogLevel::Info, "hpl_interaction_bridge disabled config=0");
        return true;
    }
    if (g_getClosestEntityTarget != nullptr) return true;

    HMODULE executable = GetModuleHandleW(nullptr);
    if (!IsInsideImage(executable, kGetClosestEntityRva, sizeof(kGetClosestEntitySignature))) {
        Logger::Instance().Write(LogLevel::Error, "hpl_interaction_bridge install_failed reason=invalid_image_range");
        return false;
    }
    auto* target = reinterpret_cast<std::byte*>(executable) + kGetClosestEntityRva;
    if (std::memcmp(target, kGetClosestEntitySignature, sizeof(kGetClosestEntitySignature)) != 0) {
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_interaction_bridge install_failed reason=signature_mismatch rva=0x%llx",
            static_cast<unsigned long long>(kGetClosestEntityRva));
        return false;
    }

    MH_STATUS status = MH_CreateHook(
        target,
        reinterpret_cast<void*>(&HookGetClosestEntity),
        reinterpret_cast<void**>(&g_originalGetClosestEntity));
    if (status != MH_OK && status != MH_ERROR_ALREADY_CREATED) {
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_interaction_bridge install_failed reason=create_hook status=%s",
            MH_StatusToString(status));
        return false;
    }
    status = MH_EnableHook(target);
    if (status != MH_OK && status != MH_ERROR_ENABLED) {
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_interaction_bridge install_failed reason=enable_hook status=%s",
            MH_StatusToString(status));
        return false;
    }

    g_getClosestEntityTarget = target;
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_interaction_bridge install_ok function=GetClosestEntity rva=0x%llx target=%p dominantHand=%s oneHandFallback=%d originTolerance=%.3f policy=replace_start_direction_only",
        static_cast<unsigned long long>(kGetClosestEntityRva),
        target,
        config.hplControllerDominantHand.c_str(),
        config.hplControllerOneHandFallback ? 1 : 0,
        config.hplControllerInteractionRayOriginTolerance);
    return true;
}

void RemoveHPLInteractionBridge()
{
    std::lock_guard lock(g_installMutex);
    if (g_getClosestEntityTarget != nullptr) {
        MH_DisableHook(g_getClosestEntityTarget);
        MH_RemoveHook(g_getClosestEntityTarget);
    }
    g_getClosestEntityTarget = nullptr;
    g_originalGetClosestEntity = nullptr;
    ClearHitSnapshot();
    g_openxr = nullptr;
    Logger::Instance().Write(LogLevel::Info, "hpl_interaction_bridge removed");
}

void LogHPLInteractionBridgeSummary()
{
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_interaction_bridge_summary installed=%d calls=%llu substitutions=%llu hits=%llu hitSnapshots=%llu hitPayloadRejects=%llu hitPayloadDistanceRejects=%llu hitPayloadNullTargets=%llu reticleUpdates=%llu semanticStates=%llu semanticAccepted=%llu focusHapticRequests=%llu focusHapticApplied=%llu fallbackDisabled=%llu fallbackQueryType=%llu fallbackAuthoredCamera=%llu fallbackCamera=%llu fallbackInput=%llu fallbackTracking=%llu fallbackOrigin=%llu lastHitState=%d lastSemanticState=%d",
        g_getClosestEntityTarget != nullptr ? 1 : 0,
        static_cast<unsigned long long>(g_calls.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_substitutions.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_substitutionHits.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_hitSnapshots.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_hitPayloadRejects.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_hitPayloadDistanceRejects.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_hitPayloadNullTargets.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_reticleUpdates.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_semanticStates.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_semanticAccepted.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_focusHapticRequests.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_focusHapticApplied.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_fallbackDisabled.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_fallbackQueryType.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_fallbackAuthoredCamera.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_fallbackCamera.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_fallbackInput.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_fallbackTracking.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_fallbackOrigin.load(std::memory_order_relaxed)),
        g_lastSubstitutionHitState.load(std::memory_order_relaxed),
        g_lastSemanticState.load(std::memory_order_relaxed));
}

bool GetHPLInteractionHitSnapshot(HPLInteractionHitSnapshot& snapshot)
{
    std::lock_guard lock(g_hitMutex);
    snapshot = g_latestHit;
    return snapshot.valid;
}

void PublishHPLInteractionCrosshairState(int crosshairState)
{
    g_semanticStates.fetch_add(1, std::memory_order_relaxed);
    const int previousSemanticState = g_lastSemanticState.exchange(
        crosshairState, std::memory_order_relaxed);
    const bool semanticChanged = previousSemanticState != crosshairState;
    if (g_openxr != nullptr) {
        g_openxr->SetInteractionReticleSemantic(crosshairState);
    }
    if (crosshairState <= 1 || crosshairState >= 35) {
        g_lastFocusTarget.store(0, std::memory_order_relaxed);
        if (semanticChanged) {
            Logger::Instance().Write(
                LogLevel::Info,
                "hpl_interaction_semantic transition=1 previous=%d current=%d accepted=0 reason=inactive_or_out_of_range hitValid=0",
                previousSemanticState,
                crosshairState);
        }
        return;
    }

    HPLInteractionHitSnapshot snapshot;
    if (!GetHPLInteractionHitSnapshot(snapshot)) {
        if (semanticChanged) {
            Logger::Instance().Write(
                LogLevel::Warn,
                "hpl_interaction_semantic transition=1 previous=%d current=%d accepted=0 reason=no_hit_snapshot hitValid=0",
                previousSemanticState,
                crosshairState);
        }
        return;
    }
    g_semanticAccepted.fetch_add(1, std::memory_order_relaxed);

    void* focusTarget = snapshot.entity != nullptr ? snapshot.entity : snapshot.body;
    const uintptr_t targetValue = reinterpret_cast<uintptr_t>(focusTarget);
    const uintptr_t previousTarget = g_lastFocusTarget.exchange(targetValue, std::memory_order_relaxed);
    const uint64_t lastHapticFrame = g_lastFocusHapticFrame.load(std::memory_order_relaxed);
    const uint64_t cooldown = static_cast<uint64_t>(g_config.hplControllerFocusHapticCooldownFrames);
    float hapticAmplitudeScale = 1.0f;
    float hapticDurationScale = 1.0f;
    const bool hapticProfileValid = hud_math::ComputeInteractionHapticProfile(
        crosshairState,
        hapticAmplitudeScale,
        hapticDurationScale);
    if (g_config.hplControllerHaptics
        && g_config.hplControllerFocusHaptics
        && hapticProfileValid
        && g_openxr != nullptr
        && targetValue != 0
        && targetValue != previousTarget
        && (lastHapticFrame == 0 || snapshot.gameFrame >= lastHapticFrame + cooldown)) {
        g_focusHapticRequests.fetch_add(1, std::memory_order_relaxed);
        g_lastFocusHapticFrame.store(snapshot.gameFrame, std::memory_order_relaxed);
        if (g_openxr->RequestHapticPulse(
                snapshot.handIndex,
                std::clamp(
                    g_config.hplControllerFocusHapticAmplitude * hapticAmplitudeScale,
                    0.0f,
                    1.0f),
                std::max(
                    1,
                    static_cast<int>(std::lround(
                        g_config.hplControllerFocusHapticDurationMs * hapticDurationScale))),
                "interaction_semantic_focus_profile")) {
            g_focusHapticApplied.fetch_add(1, std::memory_order_relaxed);
        }
    }
    if (semanticChanged) {
        Logger::Instance().Write(
            LogLevel::Info,
            "hpl_interaction_semantic transition=1 previous=%d current=%d accepted=1 hitValid=1 frame=%llu hand=%s distance=%.4f world=%.4f,%.4f,%.4f entity=%p body=%p",
            previousSemanticState,
            crosshairState,
            static_cast<unsigned long long>(snapshot.gameFrame),
            snapshot.handIndex == 0 ? "left" : "right",
            snapshot.distance,
            snapshot.worldX,
            snapshot.worldY,
            snapshot.worldZ,
            snapshot.entity,
            snapshot.body);
    }
}

} // namespace somavr
