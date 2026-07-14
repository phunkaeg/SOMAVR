#include "HPLHandsBridge.h"

#include "HPLCameraBridge.h"
#include "HPLFlashlightMath.h"
#include "HPLHandsMath.h"
#include "HPLPlayerState.h"
#include "Logger.h"

#include <Windows.h>

#include <MinHook.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>

namespace somavr {
namespace {

constexpr uintptr_t kLuxEntitySetMatrixRva = 0x0bcd90;
constexpr uint8_t kLuxEntitySetMatrixSignature[] = {
    0x40, 0x53,
    0x48, 0x81, 0xec, 0xa0, 0x00, 0x00, 0x00,
    0x48, 0x8b, 0x01,
    0x48, 0x8b, 0xd9,
    0xc6, 0x81, 0x62, 0x04, 0x00, 0x00, 0x01,
};
constexpr uintptr_t kLuxEntityGetNameRva = 0x00fb60;
constexpr uint8_t kLuxEntityGetNameSignature[] = {
    0x48, 0x8d, 0x81, 0x20, 0x01, 0x00, 0x00,
    0xc3,
};
constexpr uintptr_t kGetClosestBodyRva = 0x0cd7d0;
constexpr uint8_t kGetClosestBodySignature[] = {
    0x48, 0x83, 0xec, 0x38,
    0x48, 0x8b, 0x44, 0x24, 0x60,
    0x4c, 0x8b, 0xc2,
    0x48, 0x8b, 0xd1,
};
constexpr size_t kNativeStringInlineCapacity = 15;
constexpr size_t kMaxNativeNameLength = 127;
constexpr size_t kMaxIdentityCache = 4096;
constexpr size_t kMaxIdentityLogs = 16;
constexpr int kNormalPlayerState = 0;
constexpr int kNormalMoveState = 0;
constexpr float kQuarterScale = 0.25f;
constexpr float kQuarterScaleTolerance = 0.04f;
constexpr float kUniformScaleTolerance = 0.04f;

using LuxEntitySetMatrixFn = void (*)(void* entity, const float* matrix);
using LuxEntityGetNameFn = const void* (*)(void* entity);
using GetClosestBodyFn = void* (*)(
    const float* start,
    const float* direction,
    float rayLength,
    float* outDistance,
    float* outSurfaceNormal);

struct NativeStringLayout {
    std::array<std::byte, 16> storage{};
    uint64_t size = 0;
    uint64_t capacity = 0;
};

struct EntityIdentity {
    bool playerHands = false;
    bool flashlight = false;
    std::string name;
};

struct FlashlightPoseCache {
    bool valid = false;
    uint64_t frame = 0;
    std::array<float, 16> matrix{};
};

Config g_config;
OpenXRRuntime* g_openxr = nullptr;
LuxEntitySetMatrixFn g_originalSetMatrix = nullptr;
LuxEntityGetNameFn g_getEntityName = nullptr;
GetClosestBodyFn g_originalGetClosestBody = nullptr;
void* g_setMatrixTarget = nullptr;
void* g_getClosestBodyTarget = nullptr;
std::mutex g_installMutex;
std::mutex g_identityMutex;
std::unordered_map<void*, EntityIdentity> g_identityCache;
std::atomic<uint64_t> g_calls = 0;
std::atomic<uint64_t> g_identityReads = 0;
std::atomic<uint64_t> g_identityReadFailures = 0;
std::atomic<uint64_t> g_identityLogs = 0;
std::atomic<uint64_t> g_playerHandsIdentities = 0;
std::atomic<uint64_t> g_playerHandsCalls = 0;
std::atomic<uint64_t> g_flashlightIdentities = 0;
std::atomic<uint64_t> g_flashlightCalls = 0;
std::atomic<uint64_t> g_flashlightOverrideAttempts = 0;
std::atomic<uint64_t> g_flashlightOverrides = 0;
std::atomic<uint64_t> g_flashlightStateFallbacks = 0;
std::atomic<uint64_t> g_flashlightAuthoredFallbacks = 0;
std::atomic<uint64_t> g_flashlightPoseFallbacks = 0;
std::atomic<uint64_t> g_flashlightStaleFallbacks = 0;
std::atomic<uint64_t> g_flashlightMathFallbacks = 0;
std::mutex g_flashlightPoseMutex;
FlashlightPoseCache g_flashlightPoseCache;
std::atomic<uint64_t> g_flashlightGameplayRayCalls = 0;
std::atomic<uint64_t> g_flashlightGameplayRayCandidates = 0;
std::atomic<uint64_t> g_flashlightGameplayRayRedirects = 0;
std::atomic<uint64_t> g_flashlightGameplayRayHits = 0;
std::atomic<uint64_t> g_flashlightGameplayRayOriginFallbacks = 0;
std::atomic<uint64_t> g_flashlightGameplayRayPoseFallbacks = 0;
std::atomic<uint64_t> g_flashlightGameplayRayStaleFallbacks = 0;
std::atomic<uint64_t> g_flashlightGameplayRayMathFallbacks = 0;
std::atomic<uint64_t> g_matrixReadFailures = 0;
std::atomic<uint64_t> g_quarterScaleSamples = 0;
std::atomic<uint64_t> g_fullScaleSamples = 0;
std::atomic<uint64_t> g_otherScaleSamples = 0;
std::atomic<uint64_t> g_trackedGripSamples = 0;
std::atomic<uint64_t> g_authoredCameraSamples = 0;
std::atomic<uint64_t> g_rootOverrideAttempts = 0;
std::atomic<uint64_t> g_rootOverrides = 0;
std::atomic<uint64_t> g_rootScaleFallbacks = 0;
std::atomic<uint64_t> g_rootStateFallbacks = 0;
std::atomic<uint64_t> g_rootAuthoredFallbacks = 0;
std::atomic<uint64_t> g_rootPoseFallbacks = 0;
std::atomic<uint64_t> g_rootStaleFallbacks = 0;
std::atomic<uint64_t> g_rootMathFallbacks = 0;

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

bool ReadMemory(const void* source, void* destination, size_t bytes)
{
    if (source == nullptr || destination == nullptr || bytes == 0) return false;
    SIZE_T bytesRead = 0;
    return ReadProcessMemory(
        GetCurrentProcess(),
        source,
        destination,
        bytes,
        &bytesRead) != FALSE
        && bytesRead == bytes;
}

bool ReadNativeString(const void* nativeString, std::string& value)
{
    value.clear();
    NativeStringLayout layout;
    if (!ReadMemory(nativeString, &layout, sizeof(layout))
        || layout.size > kMaxNativeNameLength
        || layout.capacity < layout.size
        || layout.capacity > 1024 * 1024) {
        return false;
    }

    const char* source = nullptr;
    if (layout.capacity <= kNativeStringInlineCapacity) {
        source = reinterpret_cast<const char*>(layout.storage.data());
    } else {
        std::memcpy(&source, layout.storage.data(), sizeof(source));
    }
    if (source == nullptr && layout.size != 0) return false;

    value.resize(static_cast<size_t>(layout.size));
    return layout.size == 0 || ReadMemory(source, value.data(), value.size());
}

bool StartsWithPlayerHands(const std::string& name)
{
    constexpr char prefix[] = "PlayerHands_";
    return name.size() >= sizeof(prefix) - 1
        && std::memcmp(name.data(), prefix, sizeof(prefix) - 1) == 0;
}

EntityIdentity ResolveIdentity(void* entity)
{
    {
        std::lock_guard lock(g_identityMutex);
        const auto found = g_identityCache.find(entity);
        if (found != g_identityCache.end()) return found->second;
    }

    EntityIdentity identity;
    const void* nativeName = g_getEntityName != nullptr ? g_getEntityName(entity) : nullptr;
    g_identityReads.fetch_add(1, std::memory_order_relaxed);
    if (!ReadNativeString(nativeName, identity.name)) {
        g_identityReadFailures.fetch_add(1, std::memory_order_relaxed);
    } else {
        identity.playerHands = StartsWithPlayerHands(identity.name);
        identity.flashlight = identity.name == "Flashlight";
        if (identity.playerHands) {
            g_playerHandsIdentities.fetch_add(1, std::memory_order_relaxed);
        }
        if (identity.flashlight) {
            g_flashlightIdentities.fetch_add(1, std::memory_order_relaxed);
        }
        const uint64_t logIndex = g_identityLogs.fetch_add(1, std::memory_order_relaxed);
        if (logIndex < kMaxIdentityLogs || identity.playerHands || identity.flashlight) {
            Logger::Instance().Write(
                identity.playerHands || identity.flashlight ? LogLevel::Warn : LogLevel::Info,
                "hpl_entity_identity entity=%p name=%s playerHands=%d flashlight=%d nameObject=%p",
                entity,
                identity.name.c_str(),
                identity.playerHands ? 1 : 0,
                identity.flashlight ? 1 : 0,
                nativeName);
        }
    }

    std::lock_guard lock(g_identityMutex);
    if (g_identityCache.size() < kMaxIdentityCache || identity.playerHands || identity.flashlight) {
        g_identityCache[entity] = identity;
    }
    return identity;
}

float ColumnLength(const std::array<float, 16>& matrix, size_t column)
{
    return std::sqrt(
        matrix[column] * matrix[column]
        + matrix[column + 4] * matrix[column + 4]
        + matrix[column + 8] * matrix[column + 8]);
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

void CacheFlashlightPose(const std::array<float, 16>& matrix, uint64_t frame)
{
    std::lock_guard lock(g_flashlightPoseMutex);
    g_flashlightPoseCache.valid = true;
    g_flashlightPoseCache.frame = frame;
    g_flashlightPoseCache.matrix = matrix;
}

void InvalidateFlashlightPose()
{
    std::lock_guard lock(g_flashlightPoseMutex);
    g_flashlightPoseCache = {};
}

bool ReadFlashlightPose(FlashlightPoseCache& cache)
{
    std::lock_guard lock(g_flashlightPoseMutex);
    cache = g_flashlightPoseCache;
    return cache.valid;
}

bool IsFiniteVector(const float* value)
{
    return value != nullptr
        && std::isfinite(value[0])
        && std::isfinite(value[1])
        && std::isfinite(value[2]);
}

void* HookGetClosestBody(
    const float* start,
    const float* direction,
    float rayLength,
    float* outDistance,
    float* outSurfaceNormal)
{
    const uint64_t call = g_flashlightGameplayRayCalls.fetch_add(1, std::memory_order_relaxed) + 1;
    if (!g_config.hplControllerFlashlightGameplayRay
        || !g_config.hplControllerFlashlightAim
        || !IsFiniteVector(start)
        || !IsFiniteVector(direction)
        || !std::isfinite(rayLength)
        || rayLength < 5.0f
        || rayLength > 20.0f) {
        return g_originalGetClosestBody(start, direction, rayLength, outDistance, outSurfaceNormal);
    }
    const uint64_t candidate = g_flashlightGameplayRayCandidates.fetch_add(1, std::memory_order_relaxed) + 1;
    const HPLCameraBridgeStatus camera = GetHPLCameraBridgeStatus();
    const float dx = start[0] - camera.cameraWorldPositionX;
    const float dy = start[1] - camera.cameraWorldPositionY;
    const float dz = start[2] - camera.cameraWorldPositionZ;
    const float originDelta = std::sqrt(dx * dx + dy * dy + dz * dz);
    const float originTolerance = std::max(0.35f * g_config.hplWorldScale, 0.05f);
    if (!camera.trackingEnabled || !camera.cameraWorldPositionValid
        || !camera.nativeCameraBasisValid || !std::isfinite(originDelta)
        || originDelta > originTolerance) {
        g_flashlightGameplayRayOriginFallbacks.fetch_add(1, std::memory_order_relaxed);
        return g_originalGetClosestBody(start, direction, rayLength, outDistance, outSurfaceNormal);
    }

    HPLPlayerStateSnapshot player;
    FlashlightPoseCache cache;
    if (!GetHPLPlayerStateSnapshot(player) || !player.playerValid || !ReadFlashlightPose(cache)) {
        g_flashlightGameplayRayPoseFallbacks.fetch_add(1, std::memory_order_relaxed);
        return g_originalGetClosestBody(start, direction, rayLength, outDistance, outSurfaceNormal);
    }
    const uint64_t poseAge = player.frame >= cache.frame ? player.frame - cache.frame : 0;
    if (cache.frame == 0
        || poseAge > static_cast<uint64_t>(g_config.hplControllerMaxInputAgeFrames)) {
        g_flashlightGameplayRayStaleFallbacks.fetch_add(1, std::memory_order_relaxed);
        return g_originalGetClosestBody(start, direction, rayLength, outDistance, outSurfaceNormal);
    }

    const camera_math::Vector3 nativeForward{
        camera.nativeCameraForwardX, camera.nativeCameraForwardY, camera.nativeCameraForwardZ};
    const camera_math::Vector3 nativeUp{
        camera.nativeCameraUpX, camera.nativeCameraUpY, camera.nativeCameraUpZ};
    const camera_math::Vector3 flashlightForward{
        -cache.matrix[2], -cache.matrix[6], -cache.matrix[10]};
    const camera_math::Vector3 flashlightUp{
        cache.matrix[1], cache.matrix[5], cache.matrix[9]};
    camera_math::Vector3 redirectedDirection;
    if (!flashlight_math::RedirectConeDirection(
            {direction[0], direction[1], direction[2]},
            nativeForward,
            nativeUp,
            flashlightForward,
            flashlightUp,
            redirectedDirection)) {
        g_flashlightGameplayRayMathFallbacks.fetch_add(1, std::memory_order_relaxed);
        return g_originalGetClosestBody(start, direction, rayLength, outDistance, outSurfaceNormal);
    }
    const float redirectedStart[3] = {cache.matrix[3], cache.matrix[7], cache.matrix[11]};
    const float redirected[3] = {
        redirectedDirection.x, redirectedDirection.y, redirectedDirection.z};
    void* body = g_originalGetClosestBody(
        redirectedStart, redirected, rayLength, outDistance, outSurfaceNormal);
    const uint64_t redirect = g_flashlightGameplayRayRedirects.fetch_add(1, std::memory_order_relaxed) + 1;
    if (body != nullptr) {
        g_flashlightGameplayRayHits.fetch_add(1, std::memory_order_relaxed);
    }
    const uint64_t interval = static_cast<uint64_t>(std::max(g_config.hplControllerLogInterval, 1));
    if (redirect <= 12 || redirect % interval == 0) {
        Logger::Instance().Write(
            LogLevel::Info,
            "hpl_flashlight_gameplay_ray call=%llu candidate=%llu redirected=%llu frame=%llu poseAge=%llu hit=%d rayLength=%.3f originDelta=%.4f nativeStart=%.4f,%.4f,%.4f redirectedStart=%.4f,%.4f,%.4f nativeDirection=%.5f,%.5f,%.5f redirectedDirection=%.5f,%.5f,%.5f policy=preserve_random_cone",
            static_cast<unsigned long long>(call),
            static_cast<unsigned long long>(candidate),
            static_cast<unsigned long long>(redirect),
            static_cast<unsigned long long>(player.frame),
            static_cast<unsigned long long>(poseAge),
            body != nullptr ? 1 : 0,
            rayLength,
            originDelta,
            start[0], start[1], start[2],
            redirectedStart[0], redirectedStart[1], redirectedStart[2],
            direction[0], direction[1], direction[2],
            redirected[0], redirected[1], redirected[2]);
    }
    return body;
}

void HookLuxEntitySetMatrix(void* entity, const float* matrixPointer)
{
    const uint64_t call = g_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    const EntityIdentity identity = ResolveIdentity(entity);
    const float* submittedMatrix = matrixPointer;
    std::array<float, 16> controllerMatrix{};
    if (identity.playerHands) {
        const uint64_t handCall = g_playerHandsCalls.fetch_add(1, std::memory_order_relaxed) + 1;
        std::array<float, 16> matrix{};
        if (!ReadMemory(matrixPointer, matrix.data(), sizeof(matrix))) {
            g_matrixReadFailures.fetch_add(1, std::memory_order_relaxed);
        } else {
            const float scaleX = ColumnLength(matrix, 0);
            const float scaleY = ColumnLength(matrix, 1);
            const float scaleZ = ColumnLength(matrix, 2);
            const float averageScale = (scaleX + scaleY + scaleZ) / 3.0f;
            const char* scaleMode = "other";
            if (std::fabs(averageScale - kQuarterScale) <= kQuarterScaleTolerance) {
                scaleMode = "quarter";
                g_quarterScaleSamples.fetch_add(1, std::memory_order_relaxed);
            } else if (std::fabs(averageScale - 1.0f) <= 0.1f) {
                scaleMode = "full";
                g_fullScaleSamples.fetch_add(1, std::memory_order_relaxed);
            } else {
                g_otherScaleSamples.fetch_add(1, std::memory_order_relaxed);
            }

            HPLPlayerStateSnapshot player;
            const bool playerSnapshotValid = GetHPLPlayerStateSnapshot(player);
            if (player.authoredCameraActive) {
                g_authoredCameraSamples.fetch_add(1, std::memory_order_relaxed);
            }
            const HPLCameraBridgeStatus camera = GetHPLCameraBridgeStatus();

            OpenXRInputSnapshot input;
            HPLTrackedPoseWorld worldGrip;
            uint32_t handIndex = 1;
            bool gripValid = false;
            uint64_t inputAge = UINT64_MAX;
            if (g_openxr != nullptr
                && g_openxr->GetLatestInput(input)
                && input.active) {
                const OpenXRHandInput* hand = SelectDominantHand(input, handIndex);
                gripValid = hand != nullptr
                    && hand->gripPose.valid
                    && ResolveHPLTrackedPoseWorld(hand->gripPose, input.gameFrame, worldGrip)
                    && worldGrip.orientationTracked
                    && worldGrip.positionTracked;
                if (player.frame != 0 && input.gameFrame != 0) {
                    inputAge = player.frame >= input.gameFrame
                        ? player.frame - input.gameFrame
                        : 0;
                }
            }
            if (gripValid) g_trackedGripSamples.fetch_add(1, std::memory_order_relaxed);

            const float positionX = matrix[3];
            const float positionY = matrix[7];
            const float positionZ = matrix[11];
            const float cameraDx = camera.cameraWorldPositionValid
                ? positionX - camera.cameraWorldPositionX : 0.0f;
            const float cameraDy = camera.cameraWorldPositionValid
                ? positionY - camera.cameraWorldPositionY : 0.0f;
            const float cameraDz = camera.cameraWorldPositionValid
                ? positionZ - camera.cameraWorldPositionZ : 0.0f;
            const float cameraDistance = camera.cameraWorldPositionValid
                ? std::sqrt(cameraDx * cameraDx + cameraDy * cameraDy + cameraDz * cameraDz) : -1.0f;
            const float gripDx = gripValid ? positionX - worldGrip.positionX : 0.0f;
            const float gripDy = gripValid ? positionY - worldGrip.positionY : 0.0f;
            const float gripDz = gripValid ? positionZ - worldGrip.positionZ : 0.0f;
            const float gripDistance = gripValid
                ? std::sqrt(gripDx * gripDx + gripDy * gripDy + gripDz * gripDz) : -1.0f;

            bool rootOverridden = false;
            const bool uniformScale = std::fabs(scaleX - averageScale) <= kUniformScaleTolerance
                && std::fabs(scaleY - averageScale) <= kUniformScaleTolerance
                && std::fabs(scaleZ - averageScale) <= kUniformScaleTolerance;
            if (g_config.hplHandControllerRoot) {
                g_rootOverrideAttempts.fetch_add(1, std::memory_order_relaxed);
                if (!uniformScale
                    || std::fabs(averageScale - kQuarterScale) > kQuarterScaleTolerance) {
                    g_rootScaleFallbacks.fetch_add(1, std::memory_order_relaxed);
                } else if (player.authoredCameraActive) {
                    g_rootAuthoredFallbacks.fetch_add(1, std::memory_order_relaxed);
                } else if (!playerSnapshotValid
                    || !player.playerValid
                    || player.playerStateId != kNormalPlayerState
                    || player.moveStateId != kNormalMoveState) {
                    g_rootStateFallbacks.fetch_add(1, std::memory_order_relaxed);
                } else if (!gripValid || !camera.trackingEnabled) {
                    g_rootPoseFallbacks.fetch_add(1, std::memory_order_relaxed);
                } else if (inputAge > static_cast<uint64_t>(g_config.hplControllerMaxInputAgeFrames)) {
                    g_rootStaleFallbacks.fetch_add(1, std::memory_order_relaxed);
                } else {
                    const hands_math::HandRootCalibration calibration{
                        {
                            g_config.hplHandRootOffsetX,
                            g_config.hplHandRootOffsetY,
                            g_config.hplHandRootOffsetZ,
                        },
                        {
                            g_config.hplHandRootPitchDegrees,
                            g_config.hplHandRootYawDegrees,
                            g_config.hplHandRootRollDegrees,
                        },
                    };
                    rootOverridden = hands_math::BuildControllerHandMatrix(
                        {worldGrip.positionX, worldGrip.positionY, worldGrip.positionZ},
                        {worldGrip.forwardX, worldGrip.forwardY, worldGrip.forwardZ},
                        {worldGrip.upX, worldGrip.upY, worldGrip.upZ},
                        averageScale,
                        calibration,
                        controllerMatrix);
                    if (rootOverridden) {
                        submittedMatrix = controllerMatrix.data();
                        g_rootOverrides.fetch_add(1, std::memory_order_relaxed);
                    } else {
                        g_rootMathFallbacks.fetch_add(1, std::memory_order_relaxed);
                    }
                }
            }

            const uint64_t interval = static_cast<uint64_t>(std::max(g_config.hplControllerLogInterval, 1));
            if (handCall <= 12 || handCall % interval == 0) {
                Logger::Instance().Write(
                    LogLevel::Info,
                    "hpl_hands_pose call=%llu totalCall=%llu entity=%p name=%s matrix=%p pos=%.4f,%.4f,%.4f scale=%.4f,%.4f,%.4f scaleMode=%s right=%.5f,%.5f,%.5f up=%.5f,%.5f,%.5f forward=%.5f,%.5f,%.5f cameraValid=%d cameraDistance=%.4f gripValid=%d gripHand=%s gripPos=%.4f,%.4f,%.4f gripForward=%.5f,%.5f,%.5f gripDistance=%.4f inputAge=%llu rootRequested=%d rootOverridden=%d rootPos=%.4f,%.4f,%.4f authoredCamera=%d playerState=%d moveState=%d",
                    static_cast<unsigned long long>(handCall),
                    static_cast<unsigned long long>(call),
                    entity,
                    identity.name.c_str(),
                    matrixPointer,
                    positionX, positionY, positionZ,
                    scaleX, scaleY, scaleZ,
                    scaleMode,
                    matrix[0], matrix[4], matrix[8],
                    matrix[1], matrix[5], matrix[9],
                    matrix[2], matrix[6], matrix[10],
                    camera.cameraWorldPositionValid ? 1 : 0,
                    cameraDistance,
                    gripValid ? 1 : 0,
                    handIndex == 0 ? "left" : "right",
                    worldGrip.positionX, worldGrip.positionY, worldGrip.positionZ,
                    worldGrip.forwardX, worldGrip.forwardY, worldGrip.forwardZ,
                    gripDistance,
                    static_cast<unsigned long long>(inputAge),
                    g_config.hplHandControllerRoot ? 1 : 0,
                    rootOverridden ? 1 : 0,
                    rootOverridden ? controllerMatrix[3] : positionX,
                    rootOverridden ? controllerMatrix[7] : positionY,
                    rootOverridden ? controllerMatrix[11] : positionZ,
                    player.authoredCameraActive ? 1 : 0,
                    player.playerStateId,
                    player.moveStateId);
            }
        }
    } else if (identity.flashlight) {
        const uint64_t flashlightCall = g_flashlightCalls.fetch_add(1, std::memory_order_relaxed) + 1;
        HPLPlayerStateSnapshot player{};
        const bool playerSnapshotValid = GetHPLPlayerStateSnapshot(player);
        const HPLCameraBridgeStatus camera = GetHPLCameraBridgeStatus();

        OpenXRInputSnapshot input;
        HPLTrackedPoseWorld worldAim;
        uint32_t handIndex = 1;
        bool aimValid = false;
        uint64_t inputAge = UINT64_MAX;
        if (g_openxr != nullptr
            && g_openxr->GetLatestInput(input)
            && input.active) {
            const OpenXRHandInput* hand = SelectDominantHand(input, handIndex);
            aimValid = hand != nullptr
                && hand->aimPose.valid
                && ResolveHPLTrackedPoseWorld(hand->aimPose, input.gameFrame, worldAim)
                && worldAim.orientationTracked
                && worldAim.positionTracked;
            if (player.frame != 0 && input.gameFrame != 0) {
                inputAge = player.frame >= input.gameFrame
                    ? player.frame - input.gameFrame
                    : 0;
            }
        }

        bool flashlightOverridden = false;
        if (g_config.hplControllerFlashlightAim) {
            g_flashlightOverrideAttempts.fetch_add(1, std::memory_order_relaxed);
            if (!playerSnapshotValid || !player.playerValid) {
                g_flashlightStateFallbacks.fetch_add(1, std::memory_order_relaxed);
            } else if (player.authoredCameraActive
                && g_config.hplControllerSuppressDuringAuthoredCamera) {
                g_flashlightAuthoredFallbacks.fetch_add(1, std::memory_order_relaxed);
            } else if (!aimValid || !camera.trackingEnabled) {
                g_flashlightPoseFallbacks.fetch_add(1, std::memory_order_relaxed);
            } else if (inputAge > static_cast<uint64_t>(g_config.hplControllerMaxInputAgeFrames)) {
                g_flashlightStaleFallbacks.fetch_add(1, std::memory_order_relaxed);
            } else {
                const flashlight_math::FlashlightCalibration calibration{
                    {
                        g_config.hplFlashlightOffsetX,
                        g_config.hplFlashlightOffsetY,
                        g_config.hplFlashlightOffsetZ,
                    },
                    {
                        g_config.hplFlashlightPitchDegrees,
                        g_config.hplFlashlightYawDegrees,
                        g_config.hplFlashlightRollDegrees,
                    },
                };
                flashlightOverridden = flashlight_math::BuildControllerFlashlightMatrix(
                    {worldAim.positionX, worldAim.positionY, worldAim.positionZ},
                    {worldAim.forwardX, worldAim.forwardY, worldAim.forwardZ},
                    {worldAim.upX, worldAim.upY, worldAim.upZ},
                    calibration,
                    controllerMatrix);
                if (flashlightOverridden) {
                    submittedMatrix = controllerMatrix.data();
                    g_flashlightOverrides.fetch_add(1, std::memory_order_relaxed);
                } else {
                    g_flashlightMathFallbacks.fetch_add(1, std::memory_order_relaxed);
                }
            }
        }
        if (flashlightOverridden) {
            CacheFlashlightPose(controllerMatrix, input.gameFrame);
        } else {
            InvalidateFlashlightPose();
        }

        const uint64_t interval = static_cast<uint64_t>(std::max(g_config.hplControllerLogInterval, 1));
        if (flashlightCall <= 12 || flashlightCall % interval == 0) {
            std::array<float, 16> nativeMatrix{};
            const bool matrixValid = ReadMemory(matrixPointer, nativeMatrix.data(), sizeof(nativeMatrix));
            Logger::Instance().Write(
                LogLevel::Info,
                "hpl_flashlight_pose call=%llu entity=%p matrix=%p nativeMatrixValid=%d nativePos=%.4f,%.4f,%.4f aimValid=%d aimHand=%s aimPos=%.4f,%.4f,%.4f aimForward=%.5f,%.5f,%.5f inputAge=%llu requested=%d overridden=%d finalPos=%.4f,%.4f,%.4f authoredCamera=%d playerState=%d moveState=%d",
                static_cast<unsigned long long>(flashlightCall),
                entity,
                matrixPointer,
                matrixValid ? 1 : 0,
                matrixValid ? nativeMatrix[3] : 0.0f,
                matrixValid ? nativeMatrix[7] : 0.0f,
                matrixValid ? nativeMatrix[11] : 0.0f,
                aimValid ? 1 : 0,
                handIndex == 0 ? "left" : "right",
                worldAim.positionX, worldAim.positionY, worldAim.positionZ,
                worldAim.forwardX, worldAim.forwardY, worldAim.forwardZ,
                static_cast<unsigned long long>(inputAge),
                g_config.hplControllerFlashlightAim ? 1 : 0,
                flashlightOverridden ? 1 : 0,
                flashlightOverridden ? controllerMatrix[3] : (matrixValid ? nativeMatrix[3] : 0.0f),
                flashlightOverridden ? controllerMatrix[7] : (matrixValid ? nativeMatrix[7] : 0.0f),
                flashlightOverridden ? controllerMatrix[11] : (matrixValid ? nativeMatrix[11] : 0.0f),
                player.authoredCameraActive ? 1 : 0,
                player.playerStateId,
                player.moveStateId);
        }
    }

    g_originalSetMatrix(entity, submittedMatrix);
}

} // namespace

bool InstallHPLHandsBridge(const Config& config, OpenXRRuntime* openxr)
{
    std::lock_guard lock(g_installMutex);
    g_config = config;
    g_openxr = openxr;
    if (!config.hplHandTrackingProbe
        && !config.hplHandControllerRoot
        && !config.hplControllerFlashlightAim
        && !config.hplControllerFlashlightGameplayRay) {
        Logger::Instance().Write(LogLevel::Info, "hpl_hands_bridge disabled config=0");
        return true;
    }
    if (g_setMatrixTarget != nullptr) return true;

    HMODULE executable = GetModuleHandleW(nullptr);
    if (!IsInsideImage(executable, kLuxEntitySetMatrixRva, sizeof(kLuxEntitySetMatrixSignature))
        || !IsInsideImage(executable, kLuxEntityGetNameRva, sizeof(kLuxEntityGetNameSignature))
        || (config.hplControllerFlashlightGameplayRay
            && !IsInsideImage(executable, kGetClosestBodyRva, sizeof(kGetClosestBodySignature)))) {
        Logger::Instance().Write(LogLevel::Error, "hpl_hands_bridge install_failed reason=invalid_image_range");
        return false;
    }

    auto* setMatrixTarget = reinterpret_cast<std::byte*>(executable) + kLuxEntitySetMatrixRva;
    auto* getNameTarget = reinterpret_cast<std::byte*>(executable) + kLuxEntityGetNameRva;
    auto* getClosestBodyTarget = reinterpret_cast<std::byte*>(executable) + kGetClosestBodyRva;
    if (std::memcmp(setMatrixTarget, kLuxEntitySetMatrixSignature, sizeof(kLuxEntitySetMatrixSignature)) != 0
        || std::memcmp(getNameTarget, kLuxEntityGetNameSignature, sizeof(kLuxEntityGetNameSignature)) != 0
        || (config.hplControllerFlashlightGameplayRay
            && std::memcmp(getClosestBodyTarget, kGetClosestBodySignature,
                sizeof(kGetClosestBodySignature)) != 0)) {
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_hands_bridge install_failed reason=signature_mismatch setMatrixRva=0x%llx getNameRva=0x%llx getClosestBodyRva=0x%llx gameplayRay=%d",
            static_cast<unsigned long long>(kLuxEntitySetMatrixRva),
            static_cast<unsigned long long>(kLuxEntityGetNameRva),
            static_cast<unsigned long long>(kGetClosestBodyRva),
            config.hplControllerFlashlightGameplayRay ? 1 : 0);
        return false;
    }
    g_getEntityName = reinterpret_cast<LuxEntityGetNameFn>(getNameTarget);

    MH_STATUS status = MH_CreateHook(
        setMatrixTarget,
        reinterpret_cast<void*>(&HookLuxEntitySetMatrix),
        reinterpret_cast<void**>(&g_originalSetMatrix));
    if (status != MH_OK && status != MH_ERROR_ALREADY_CREATED) {
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_hands_bridge install_failed reason=create_hook status=%s",
            MH_StatusToString(status));
        return false;
    }
    status = MH_EnableHook(setMatrixTarget);
    if (status != MH_OK && status != MH_ERROR_ENABLED) {
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_hands_bridge install_failed reason=enable_hook status=%s",
            MH_StatusToString(status));
        return false;
    }

    g_setMatrixTarget = setMatrixTarget;
    if (config.hplControllerFlashlightGameplayRay) {
        status = MH_CreateHook(
            getClosestBodyTarget,
            reinterpret_cast<void*>(&HookGetClosestBody),
            reinterpret_cast<void**>(&g_originalGetClosestBody));
        if (status != MH_OK && status != MH_ERROR_ALREADY_CREATED) {
            MH_DisableHook(setMatrixTarget);
            MH_RemoveHook(setMatrixTarget);
            g_setMatrixTarget = nullptr;
            g_originalSetMatrix = nullptr;
            g_getEntityName = nullptr;
            Logger::Instance().Write(
                LogLevel::Error,
                "hpl_hands_bridge install_failed reason=create_gameplay_ray_hook status=%s",
                MH_StatusToString(status));
            return false;
        }
        status = MH_EnableHook(getClosestBodyTarget);
        if (status != MH_OK && status != MH_ERROR_ENABLED) {
            MH_RemoveHook(getClosestBodyTarget);
            g_originalGetClosestBody = nullptr;
            MH_DisableHook(setMatrixTarget);
            MH_RemoveHook(setMatrixTarget);
            g_setMatrixTarget = nullptr;
            g_originalSetMatrix = nullptr;
            g_getEntityName = nullptr;
            Logger::Instance().Write(
                LogLevel::Error,
                "hpl_hands_bridge install_failed reason=enable_gameplay_ray_hook status=%s",
                MH_StatusToString(status));
            return false;
        }
        g_getClosestBodyTarget = getClosestBodyTarget;
    }
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_hands_bridge install_ok setMatrixRva=0x%llx getNameRva=0x%llx getClosestBodyRva=0x%llx probe=%d controllerRoot=%d flashlightAim=%d flashlightGameplayRay=%d handOffset=%.4f,%.4f,%.4f handRotationDegrees=%.2f,%.2f,%.2f flashlightOffset=%.4f,%.4f,%.4f flashlightRotationDegrees=%.2f,%.2f,%.2f policy=exact_identity_guarded_tracked_pose gameplayRayPolicy=ray_length_camera_origin_preserve_cone cacheLimit=%llu identityLogLimit=%llu",
        static_cast<unsigned long long>(kLuxEntitySetMatrixRva),
        static_cast<unsigned long long>(kLuxEntityGetNameRva),
        static_cast<unsigned long long>(kGetClosestBodyRva),
        config.hplHandTrackingProbe ? 1 : 0,
        config.hplHandControllerRoot ? 1 : 0,
        config.hplControllerFlashlightAim ? 1 : 0,
        config.hplControllerFlashlightGameplayRay ? 1 : 0,
        config.hplHandRootOffsetX,
        config.hplHandRootOffsetY,
        config.hplHandRootOffsetZ,
        config.hplHandRootPitchDegrees,
        config.hplHandRootYawDegrees,
        config.hplHandRootRollDegrees,
        config.hplFlashlightOffsetX,
        config.hplFlashlightOffsetY,
        config.hplFlashlightOffsetZ,
        config.hplFlashlightPitchDegrees,
        config.hplFlashlightYawDegrees,
        config.hplFlashlightRollDegrees,
        static_cast<unsigned long long>(kMaxIdentityCache),
        static_cast<unsigned long long>(kMaxIdentityLogs));
    return true;
}

void RemoveHPLHandsBridge()
{
    std::lock_guard lock(g_installMutex);
    if (g_getClosestBodyTarget != nullptr) {
        MH_DisableHook(g_getClosestBodyTarget);
        MH_RemoveHook(g_getClosestBodyTarget);
    }
    if (g_setMatrixTarget != nullptr) {
        MH_DisableHook(g_setMatrixTarget);
        MH_RemoveHook(g_setMatrixTarget);
    }
    g_setMatrixTarget = nullptr;
    g_getClosestBodyTarget = nullptr;
    g_originalSetMatrix = nullptr;
    g_originalGetClosestBody = nullptr;
    g_getEntityName = nullptr;
    g_openxr = nullptr;
    InvalidateFlashlightPose();
    {
        std::lock_guard identityLock(g_identityMutex);
        g_identityCache.clear();
    }
    Logger::Instance().Write(LogLevel::Info, "hpl_hands_bridge removed");
}

void LogHPLHandsBridgeSummary()
{
    size_t cachedIdentities = 0;
    {
        std::lock_guard lock(g_identityMutex);
        cachedIdentities = g_identityCache.size();
    }
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_hands_bridge_summary installed=%d gameplayRayInstalled=%d calls=%llu cachedIdentities=%llu identityReads=%llu identityReadFailures=%llu playerHandsIdentities=%llu playerHandsCalls=%llu flashlightIdentities=%llu flashlightCalls=%llu matrixReadFailures=%llu quarterScaleSamples=%llu fullScaleSamples=%llu otherScaleSamples=%llu trackedGripSamples=%llu authoredCameraSamples=%llu rootOverrideAttempts=%llu rootOverrides=%llu rootScaleFallbacks=%llu rootStateFallbacks=%llu rootAuthoredFallbacks=%llu rootPoseFallbacks=%llu rootStaleFallbacks=%llu rootMathFallbacks=%llu flashlightOverrideAttempts=%llu flashlightOverrides=%llu flashlightStateFallbacks=%llu flashlightAuthoredFallbacks=%llu flashlightPoseFallbacks=%llu flashlightStaleFallbacks=%llu flashlightMathFallbacks=%llu gameplayRayCalls=%llu gameplayRayCandidates=%llu gameplayRayRedirects=%llu gameplayRayHits=%llu gameplayRayOriginFallbacks=%llu gameplayRayPoseFallbacks=%llu gameplayRayStaleFallbacks=%llu gameplayRayMathFallbacks=%llu",
        g_setMatrixTarget != nullptr ? 1 : 0,
        g_getClosestBodyTarget != nullptr ? 1 : 0,
        static_cast<unsigned long long>(g_calls.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(cachedIdentities),
        static_cast<unsigned long long>(g_identityReads.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_identityReadFailures.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_playerHandsIdentities.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_playerHandsCalls.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_flashlightIdentities.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_flashlightCalls.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_matrixReadFailures.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_quarterScaleSamples.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_fullScaleSamples.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_otherScaleSamples.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_trackedGripSamples.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_authoredCameraSamples.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_rootOverrideAttempts.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_rootOverrides.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_rootScaleFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_rootStateFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_rootAuthoredFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_rootPoseFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_rootStaleFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_rootMathFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_flashlightOverrideAttempts.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_flashlightOverrides.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_flashlightStateFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_flashlightAuthoredFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_flashlightPoseFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_flashlightStaleFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_flashlightMathFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_flashlightGameplayRayCalls.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_flashlightGameplayRayCandidates.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_flashlightGameplayRayRedirects.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_flashlightGameplayRayHits.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_flashlightGameplayRayOriginFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_flashlightGameplayRayPoseFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_flashlightGameplayRayStaleFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_flashlightGameplayRayMathFallbacks.load(std::memory_order_relaxed)));
}

} // namespace somavr
