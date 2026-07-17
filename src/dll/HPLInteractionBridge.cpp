#include "HPLInteractionBridge.h"

#include "HPLCameraBridge.h"
#include "HPLHudMath.h"
#include "HPLInteractionMath.h"
#include "HPLPlayerState.h"
#include "Logger.h"
#include "SomaBuildSignatures.h"

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

constexpr uintptr_t kGetClosestEntityRva = soma_signatures::kGetClosestEntityRva;
constexpr uintptr_t kGetClosestEntityRaycastRva =
    soma_signatures::kGetClosestEntityRaycastRva;
constexpr auto& kGetClosestEntitySignature = soma_signatures::kGetClosestEntity;
constexpr auto& kGetClosestEntityRaycastSignature =
    soma_signatures::kGetClosestEntityRaycast;

using GetClosestEntityFn = bool (*)(
    const float* start,
    const float* direction,
    float rayLength,
    int interactType,
    bool checkLineOfSight,
    void* output);
using GetClosestEntityRaycastFn = bool (*)(
    void* rayOwner,
    const float* start,
    const float* direction,
    float rayLength,
    int interactType,
    bool checkLineOfSight,
    float* outDistance,
    void** outBody,
    void** outEntity);
using FinalizeClosestEntityOutputFn = void (*)(void* output);

Config g_config;
OpenXRRuntime* g_openxr = nullptr;
GetClosestEntityFn g_originalGetClosestEntity = nullptr;
GetClosestEntityRaycastFn g_getClosestEntityRaycast = nullptr;
void** g_gameContextSlot = nullptr;
void* g_getClosestEntityTarget = nullptr;
std::mutex g_installMutex;
std::atomic<uint64_t> g_calls = 0;
std::atomic<uint64_t> g_substitutions = 0;
std::atomic<uint64_t> g_substitutionHits = 0;
std::atomic<uint64_t> g_handProbes[2] = {};
std::atomic<uint64_t> g_handHits[2] = {};
std::atomic<uint64_t> g_handSelections[2] = {};
std::atomic<uint64_t> g_handSwitches = 0;
std::atomic<int> g_activeInteractionHand = -1;
std::atomic<uint64_t> g_activeInteractionFrame = 0;
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

bool ReadMemory(const void* source, void* destination, size_t bytes)
{
    if (source == nullptr || destination == nullptr || bytes == 0) return false;
    SIZE_T bytesRead = 0;
    return ReadProcessMemory(
        GetCurrentProcess(), source, destination, bytes, &bytesRead) != FALSE
        && bytesRead == bytes;
}

template <typename T>
bool ReadField(const void* base, size_t offset, T& value)
{
    return ReadMemory(static_cast<const std::byte*>(base) + offset, &value, sizeof(value));
}

struct NativeHandRay {
    bool tracked = false;
    bool hit = false;
    uint32_t handIndex = 1;
    float distance = 10000000.0f;
    void* body = nullptr;
    void* entity = nullptr;
    const OpenXRHandInput* hand = nullptr;
    HPLTrackedPoseWorld worldAim{};
};

bool ProbeHandRay(
    void* rayOwner,
    const OpenXRInputSnapshot& input,
    uint32_t handIndex,
    float rayLength,
    int interactType,
    bool checkLineOfSight,
    NativeHandRay& ray)
{
    ray = {};
    ray.handIndex = handIndex;
    ray.distance = 10000000.0f;
    ray.hand = handIndex == 0 ? &input.left : &input.right;
    if (rayOwner == nullptr || g_getClosestEntityRaycast == nullptr
        || !ray.hand->active
        || !ray.hand->aimPose.valid
        || !ray.hand->aimPose.orientationTracked
        || !ray.hand->aimPose.positionTracked
        || !ResolveHPLTrackedPoseWorld(ray.hand->aimPose, input.gameFrame, ray.worldAim)
        || !ray.worldAim.orientationTracked
        || !ray.worldAim.positionTracked) {
        return false;
    }

    ray.tracked = true;
    const float controllerStart[3] = {
        ray.worldAim.positionX, ray.worldAim.positionY, ray.worldAim.positionZ,
    };
    const float controllerDirection[3] = {
        ray.worldAim.forwardX, ray.worldAim.forwardY, ray.worldAim.forwardZ,
    };
    g_handProbes[handIndex].fetch_add(1, std::memory_order_relaxed);
    ray.hit = g_getClosestEntityRaycast(
        rayOwner,
        controllerStart,
        controllerDirection,
        rayLength,
        interactType,
        checkLineOfSight,
        &ray.distance,
        &ray.body,
        &ray.entity);
    if (ray.hit) g_handHits[handIndex].fetch_add(1, std::memory_order_relaxed);
    return true;
}

bool FinalizeClosestEntityOutput(void* output, const NativeHandRay& ray)
{
    if (output == nullptr) return false;
    void* vtable = nullptr;
    FinalizeClosestEntityOutputFn finalize = nullptr;
    if (!ReadMemory(output, &vtable, sizeof(vtable))
        || !ReadField(vtable, 0x40, finalize)
        || finalize == nullptr) {
        return false;
    }

    auto* bytes = static_cast<std::byte*>(output);
    std::memcpy(bytes + 0x18, &ray.distance, sizeof(ray.distance));
    std::memcpy(bytes + 0x20, &ray.body, sizeof(ray.body));
    std::memcpy(bytes + 0x28, &ray.entity, sizeof(ray.entity));
    finalize(output);
    return true;
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
    if (g_openxr == nullptr
        || !g_openxr->GetLatestInput(input)
        || !input.active
        || (camera.headPoseFrame >= input.gameFrame
            && camera.headPoseFrame - input.gameFrame
                > static_cast<uint64_t>(g_config.hplControllerMaxInputAgeFrames))) {
        g_fallbackInput.fetch_add(1, std::memory_order_relaxed);
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

    void* gameContext = nullptr;
    void* rayOwner = nullptr;
    if (g_gameContextSlot == nullptr
        || !ReadMemory(g_gameContextSlot, &gameContext, sizeof(gameContext))
        || gameContext == nullptr
        || !ReadField(gameContext, 0xc0, rayOwner)
        || rayOwner == nullptr) {
        g_fallbackTracking.fetch_add(1, std::memory_order_relaxed);
        ClearHitSnapshot();
        return g_originalGetClosestEntity(
            start, direction, rayLength, interactType, checkLineOfSight, output);
    }

    NativeHandRay rays[2];
    const uint32_t preferredHand = g_config.hplControllerDominantHand == "left" ? 0u : 1u;
    if (g_config.hplControllerInteractionBothHands) {
        ProbeHandRay(
            rayOwner, input, 0, rayLength, interactType, checkLineOfSight, rays[0]);
        ProbeHandRay(
            rayOwner, input, 1, rayLength, interactType, checkLineOfSight, rays[1]);
    } else {
        const bool preferredTracked = ProbeHandRay(
            rayOwner,
            input,
            preferredHand,
            rayLength,
            interactType,
            checkLineOfSight,
            rays[preferredHand]);
        if (!preferredTracked && g_config.hplControllerOneHandFallback) {
            ProbeHandRay(
                rayOwner,
                input,
                preferredHand ^ 1u,
                rayLength,
                interactType,
                checkLineOfSight,
                rays[preferredHand ^ 1u]);
        }
    }
    if (!rays[0].tracked && !rays[1].tracked) {
        g_fallbackTracking.fetch_add(1, std::memory_order_relaxed);
        ClearHitSnapshot();
        return g_originalGetClosestEntity(
            start, direction, rayLength, interactType, checkLineOfSight, output);
    }

    const bool leftPressed = rays[0].tracked
        && (input.left.select || input.left.trigger >= 0.75f);
    const bool rightPressed = rays[1].tracked
        && (input.right.select || input.right.trigger >= 0.75f);
    const int previousHand = g_activeInteractionHand.load(std::memory_order_relaxed);
    const uint32_t handIndex = interaction_math::SelectInteractionHand(
        {rays[0].tracked, rays[0].hit, leftPressed, rays[0].distance},
        {rays[1].tracked, rays[1].hit, rightPressed, rays[1].distance},
        preferredHand,
        previousHand);
    NativeHandRay& selected = rays[handIndex];
    if (!selected.tracked || selected.hand == nullptr
        || !FinalizeClosestEntityOutput(output, selected)) {
        g_fallbackTracking.fetch_add(1, std::memory_order_relaxed);
        ClearHitSnapshot();
        return g_originalGetClosestEntity(
            start, direction, rayLength, interactType, checkLineOfSight, output);
    }

    if (previousHand >= 0 && previousHand != static_cast<int>(handIndex)) {
        g_handSwitches.fetch_add(1, std::memory_order_relaxed);
    }
    g_activeInteractionHand.store(static_cast<int>(handIndex), std::memory_order_relaxed);
    g_activeInteractionFrame.store(input.gameFrame, std::memory_order_relaxed);
    g_handSelections[handIndex].fetch_add(1, std::memory_order_relaxed);
    const bool hit = selected.hit;
    const float controllerStart[3] = {
        selected.worldAim.positionX,
        selected.worldAim.positionY,
        selected.worldAim.positionZ,
    };
    const float controllerDirection[3] = {
        selected.worldAim.forwardX,
        selected.worldAim.forwardY,
        selected.worldAim.forwardZ,
    };
    const uint64_t substitution = g_substitutions.fetch_add(1, std::memory_order_relaxed) + 1;
    if (hit) g_substitutionHits.fetch_add(1, std::memory_order_relaxed);
    const bool hitSnapshot = hit
        ? PublishHitSnapshot(
            output,
            input.gameFrame,
            handIndex,
            selected.hand->aimPose,
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
            "hpl_interaction_ray call=%llu inputFrame=%llu applied=1 dual=%d hit=%d hitSnapshot=%d transition=%d previousState=%d currentState=%d hand=%s previousHand=%d candidates={leftTracked=%d leftHit=%d leftPressed=%d leftDistance=%.4f rightTracked=%d rightHit=%d rightPressed=%d rightDistance=%.4f} nativeStart=%.4f,%.4f,%.4f controllerStart=%.4f,%.4f,%.4f controllerDir=%.5f,%.5f,%.5f hitDistance=%.4f hitWorld=%.4f,%.4f,%.4f entity=%p body=%p originDelta=%.4f rayLength=%.4f type=%d los=%d",
            static_cast<unsigned long long>(call),
            static_cast<unsigned long long>(input.gameFrame),
            g_config.hplControllerInteractionBothHands ? 1 : 0,
            hit ? 1 : 0,
            hitSnapshot ? 1 : 0,
            hitStateChanged ? 1 : 0,
            previousHitState,
            hitState,
            handIndex == 0 ? "left" : "right",
            previousHand,
            rays[0].tracked ? 1 : 0,
            rays[0].hit ? 1 : 0,
            leftPressed ? 1 : 0,
            rays[0].distance,
            rays[1].tracked ? 1 : 0,
            rays[1].hit ? 1 : 0,
            rightPressed ? 1 : 0,
            rays[1].distance,
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
    if (!IsInsideImage(executable, kGetClosestEntityRva, sizeof(kGetClosestEntitySignature))
        || !IsInsideImage(
            executable,
            kGetClosestEntityRaycastRva,
            sizeof(kGetClosestEntityRaycastSignature))) {
        Logger::Instance().Write(LogLevel::Error, "hpl_interaction_bridge install_failed reason=invalid_image_range");
        return false;
    }
    auto* base = reinterpret_cast<std::byte*>(executable);
    auto* target = base + kGetClosestEntityRva;
    auto* raycast = base + kGetClosestEntityRaycastRva;
    if (std::memcmp(target, kGetClosestEntitySignature, sizeof(kGetClosestEntitySignature)) != 0) {
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_interaction_bridge install_failed reason=outer_signature_mismatch rva=0x%llx actual=%02x,%02x,%02x,%02x expected=%02x,%02x,%02x,%02x",
            static_cast<unsigned long long>(kGetClosestEntityRva),
            static_cast<unsigned int>(target[0]),
            static_cast<unsigned int>(target[1]),
            static_cast<unsigned int>(target[2]),
            static_cast<unsigned int>(target[3]),
            kGetClosestEntitySignature[0], kGetClosestEntitySignature[1],
            kGetClosestEntitySignature[2], kGetClosestEntitySignature[3]);
        return false;
    }
    if (std::memcmp(
            raycast,
            kGetClosestEntityRaycastSignature,
            sizeof(kGetClosestEntityRaycastSignature)) != 0) {
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_interaction_bridge install_failed reason=inner_signature_mismatch rva=0x%llx actual=%02x,%02x,%02x,%02x expected=%02x,%02x,%02x,%02x",
            static_cast<unsigned long long>(kGetClosestEntityRaycastRva),
            static_cast<unsigned int>(raycast[0]),
            static_cast<unsigned int>(raycast[1]),
            static_cast<unsigned int>(raycast[2]),
            static_cast<unsigned int>(raycast[3]),
            kGetClosestEntityRaycastSignature[0], kGetClosestEntityRaycastSignature[1],
            kGetClosestEntityRaycastSignature[2], kGetClosestEntityRaycastSignature[3]);
        return false;
    }

    int32_t gameContextDisplacement = 0;
    std::memcpy(
        &gameContextDisplacement,
        raycast + soma_signatures::kRaycastGameContextDisplacementOffset,
        sizeof(gameContextDisplacement));
    g_gameContextSlot = reinterpret_cast<void**>(
        raycast + soma_signatures::kRaycastGameContextNextInstructionOffset
            + gameContextDisplacement);
    void* gameContextProbe = nullptr;
    if (!ReadMemory(g_gameContextSlot, &gameContextProbe, sizeof(gameContextProbe))) {
        g_gameContextSlot = nullptr;
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_interaction_bridge install_failed reason=invalid_game_context_slot");
        return false;
    }
    g_getClosestEntityRaycast = reinterpret_cast<GetClosestEntityRaycastFn>(raycast);

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
        "hpl_interaction_bridge install_ok function=GetClosestEntity rva=0x%llx raycastRva=0x%llx target=%p bothHands=%d dominantHand=%s oneHandFallback=%d originTolerance=%.3f policy=probe_both_finalize_selected_once",
        static_cast<unsigned long long>(kGetClosestEntityRva),
        static_cast<unsigned long long>(kGetClosestEntityRaycastRva),
        target,
        config.hplControllerInteractionBothHands ? 1 : 0,
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
    g_getClosestEntityRaycast = nullptr;
    g_gameContextSlot = nullptr;
    g_activeInteractionHand.store(-1, std::memory_order_relaxed);
    g_activeInteractionFrame.store(0, std::memory_order_relaxed);
    ClearHitSnapshot();
    g_openxr = nullptr;
    Logger::Instance().Write(LogLevel::Info, "hpl_interaction_bridge removed");
}

void LogHPLInteractionBridgeSummary()
{
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_interaction_bridge_summary installed=%d bothHands=%d calls=%llu substitutions=%llu hits=%llu handProbes=%llu,%llu handHits=%llu,%llu handSelections=%llu,%llu handSwitches=%llu activeHand=%d hitSnapshots=%llu hitPayloadRejects=%llu hitPayloadDistanceRejects=%llu hitPayloadNullTargets=%llu reticleUpdates=%llu semanticStates=%llu semanticAccepted=%llu focusHapticRequests=%llu focusHapticApplied=%llu fallbackDisabled=%llu fallbackQueryType=%llu fallbackAuthoredCamera=%llu fallbackCamera=%llu fallbackInput=%llu fallbackTracking=%llu fallbackOrigin=%llu lastHitState=%d lastSemanticState=%d",
        g_getClosestEntityTarget != nullptr ? 1 : 0,
        g_config.hplControllerInteractionBothHands ? 1 : 0,
        static_cast<unsigned long long>(g_calls.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_substitutions.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_substitutionHits.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_handProbes[0].load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_handProbes[1].load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_handHits[0].load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_handHits[1].load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_handSelections[0].load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_handSelections[1].load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_handSwitches.load(std::memory_order_relaxed)),
        g_activeInteractionHand.load(std::memory_order_relaxed),
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

bool GetHPLInteractionOwnerHand(
    uint64_t gameFrame,
    uint64_t maximumAgeFrames,
    uint32_t& handIndex)
{
    const int activeHand = g_activeInteractionHand.load(std::memory_order_relaxed);
    const uint64_t activeFrame = g_activeInteractionFrame.load(std::memory_order_relaxed);
    if (activeHand < 0 || activeHand >= 2 || activeFrame == 0
        || gameFrame < activeFrame || gameFrame - activeFrame > maximumAgeFrames) {
        return false;
    }
    handIndex = static_cast<uint32_t>(activeHand);
    return true;
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
