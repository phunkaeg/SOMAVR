#include "HPLCameraBridge.h"

#include "HPLCameraMath.h"
#include "HPLPlayerState.h"
#include "Logger.h"
#include "NativeMemoryAccess.h"

#include <Windows.h>

#include <MinHook.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <intrin.h>
#include <mutex>

namespace somavr {
namespace {

using camera_math::BuildOpenXRProjection;
using camera_math::BuildRoomscaleSafetySampleOffsets;
using camera_math::CenterProjectionFov;
using camera_math::Conjugate;
using camera_math::ComputeRoomscaleSafetyFactor;
using camera_math::MatrixMultiply;
using camera_math::Multiply;
using camera_math::Normalize;
using camera_math::PoseStabilityState;
using camera_math::PoseStabilityUpdate;
using camera_math::Quaternion;
using camera_math::ReplaceTrackedHeadTranslation;
using camera_math::RotateVector;
using camera_math::RotationMatrix;
using camera_math::ResolveStereoPairBaseAction;
using camera_math::ResolveTrackedEyeOffset;
using camera_math::StereoPairBaseAction;
using camera_math::TranslationMatrix;
using camera_math::UpdatePoseStability;
using camera_math::ValidateStereoProjectionMath;
using camera_math::Vector3;

// Consecutive failed stereo applies before stereo ownership is actually given
// up. Matches the existing stereoCaptureFailures_ suspend threshold in
// OpenXRRuntime so the two stereo lanes fail over on the same budget.
constexpr uint32_t kStereoApplyFailureLimit = 8;
constexpr uint64_t kStereoPairBaseMaxAgeMilliseconds = 100;

constexpr uintptr_t kCameraGetFrustumRva = 0x271b80;
constexpr uintptr_t kSetupPerspectiveFrustumRva = 0x270230;
constexpr uintptr_t kCheckLineOfSightRva = 0x0cd710;
constexpr uintptr_t kRenderViewportGetFrustumReturnRva = 0x298697;
constexpr uint32_t kActivationStablePoseFrames = 8;
constexpr float kActivationMaxPositionStepMeters = 0.25f;
constexpr float kActivationMaxOrientationStepRadians = 0.7853981633974483f;
constexpr float kRadiansToDegrees = 57.29577951308232f;

constexpr size_t kFrustumFarOffset = 0x18;
constexpr size_t kFrustumNearOffset = 0x1c;
constexpr size_t kFrustumAspectOffset = 0x20;
constexpr size_t kFrustumFovOffset = 0x24;
constexpr size_t kFrustumInfiniteFarOffset = 0x30;
constexpr size_t kFrustumProjectionTypeOffset = 0x34;
constexpr size_t kFrustumOriginOffset = 0x38;
constexpr size_t kFrustumProjectionMatrixOffset = 0xd8;
constexpr size_t kFrustumViewMatrixOffset = 0x158;

constexpr size_t kCameraSecondaryRotationXOffset = 0x60;
constexpr size_t kCameraSecondaryRotationYOffset = 0x64;
constexpr size_t kCameraSecondaryRotationZOffset = 0x68;
constexpr size_t kCameraBaseRollOffset = 0x4c;
constexpr size_t kCameraBasePitchOffset = 0x44;
constexpr size_t kCameraViewDirtyOffset = 0x709;
constexpr size_t kCameraProjectionDirtyOffset = 0x70b;
constexpr size_t kCameraBaseFrustumDirtyOffset = 0x70c;
constexpr size_t kCameraSecondaryFrustumDirtyOffset = 0x70d;

using CameraGetFrustumFn = void* (*)(void* camera, bool projectionFlag);
using CheckLineOfSightFn = bool (*)(
    const float* start,
    const float* end,
    bool checkOnlyShadowCasters,
    bool checkOnlyStatic);
using SetupPerspectiveFrustumFn = void (*)(
    void* frustum,
    const float* projection,
    const float* view,
    float farPlane,
    float nearPlane,
    float fov,
    float aspect,
    const float* origin,
    bool infiniteFar,
    const float* customFarProjection,
    bool obliqueNearPlane);

struct FrustumParameters {
    float farPlane = 0.0f;
    float nearPlane = 0.0f;
    float aspect = 0.0f;
    float fov = 0.0f;
    std::array<float, 3> origin{};
    bool infiniteFar = false;
    int projectionType = -1;
};

struct RoomscaleSafetyResult {
    Vector3 translation{};
    float factor = 1.0f;
    uint32_t probeCount = 0;
    uint32_t validProbeCount = 0;
    uint32_t blockedProbeCount = 0;
    bool queried = false;
    bool clamped = false;
};

struct RoomscaleProbeResult {
    float clearFraction = 1.0f;
    bool valid = false;
    bool blocked = false;
};

struct BridgeState {
    bool f2Down = false;
    bool f10Down = false;
    bool f11Down = false;
    bool activationPending = false;
    bool recenterPending = false;
    bool trackingEnabled = false;
    bool stereoEnabled = false;
    bool trackingFallbackActive = false;
    bool baseMatricesValid = false;
    int currentEyeIndex = -1;
    uint64_t currentEyePoseFrame = 0;
    uint32_t stereoApplyFailures = 0;
    bool pairRotationValid = false;
    Quaternion pairRotation{};
    bool pairBaseValid = false;
    uint64_t pairBaseCapturedAtMilliseconds = 0;
    void* pairBaseFrustum = nullptr;
    std::array<float, 16> pairBaseProjection{};
    std::array<float, 16> pairBaseView{};
    FrustumParameters pairBaseParameters{};
    bool pairViewsValid = false;
    OpenXRStereoViewSnapshot pairViews{};
    uint32_t activationTrackingWaitLogs = 0;
    uint32_t recenterTrackingWaitLogs = 0;
    void* activeCamera = nullptr;
    void* activeFrustum = nullptr;
    PoseStabilityState activationPoseStability{};
    PoseStabilityState recenterPoseStability{};
    Quaternion neutralOrientation{};
    Vector3 neutralPosition{};
    uint64_t calibrationGeneration = 0;
    std::array<float, 16> baseProjection{};
    std::array<float, 16> baseView{};
    FrustumParameters parameters{};
    bool roomscaleSafetyCacheValid = false;
    bool roomscaleSafetyWasClamped = false;
    uint64_t roomscaleSafetyFrame = 0;
    Vector3 roomscaleSafetyInput{};
    std::array<float, 3> roomscaleSafetyOrigin{};
    RoomscaleSafetyResult roomscaleSafetyResult{};
};

Config g_config;
OpenXRRuntime* g_openxr = nullptr;
CameraGetFrustumFn g_originalCameraGetFrustum = nullptr;
SetupPerspectiveFrustumFn g_setupPerspectiveFrustum = nullptr;
CheckLineOfSightFn g_checkLineOfSight = nullptr;
void* g_cameraGetFrustumTarget = nullptr;
uintptr_t g_executableBase = 0;
std::mutex g_stateMutex;
BridgeState g_state;
std::atomic<uint64_t> g_getFrustumCalls = 0;
std::atomic<uint64_t> g_candidateCalls = 0;
std::atomic<uint64_t> g_secondaryCameraCandidates = 0;
std::atomic<uint64_t> g_secondaryCameraControlSkips = 0;
std::atomic<uint64_t> g_authoredCameraOwnershipChanges = 0;
std::atomic<uint64_t> g_appliedCalls = 0;
std::atomic<uint64_t> g_baseRefreshes = 0;
std::atomic<uint64_t> g_poseMisses = 0;
std::atomic<uint64_t> g_stereoAppliedCalls = 0;
std::atomic<uint64_t> g_stereoEyeCalls[2] = {};
std::atomic<uint32_t> g_nextEyeIndex = 0;
std::atomic<int> g_committableEye = -1;
std::atomic<uint64_t> g_committableEyePoseFrame = 0;
std::atomic<uint64_t> g_stereoFillCommits = 0;
std::atomic<uint64_t> g_stereoFillRejects = 0;
std::atomic<uint64_t> g_pairRotationLatches = 0;
std::atomic<uint64_t> g_pairRotationReuses = 0;
std::atomic<uint64_t> g_pairBaseLatches = 0;
std::atomic<uint64_t> g_pairBaseReplays = 0;
std::atomic<uint64_t> g_pairBaseMissingRejects = 0;
std::atomic<uint64_t> g_pairBaseStaleRejects = 0;
std::atomic<uint64_t> g_pairBaseFrustumRejects = 0;
std::atomic<uint64_t> g_pairViewLatches = 0;
std::atomic<uint64_t> g_pairViewReplays = 0;
std::atomic<uint64_t> g_pairViewRejects = 0;
std::atomic<uint64_t> g_trackingFallbackFrames = 0;
std::atomic<uint64_t> g_trackingRecoveryEvents = 0;
std::atomic<uint64_t> g_nativeRollObservedCalls = 0;
std::atomic<uint64_t> g_nativeRollSuppressedCalls = 0;
std::atomic<uint64_t> g_nativePitchObservedCalls = 0;
std::atomic<uint64_t> g_nativePitchSuppressedCalls = 0;
std::atomic<uint64_t> g_nativeMemoryReadFailures = 0;
std::atomic<uint64_t> g_nativeMemoryWriteFailures = 0;
std::atomic<uint32_t> g_nativeMemoryWarningLogs = 0;
std::atomic<uint64_t> g_roomscaleSafetySamples = 0;
std::atomic<uint64_t> g_roomscaleSafetyQueries = 0;
std::atomic<uint64_t> g_roomscaleSafetyProbes = 0;
std::atomic<uint64_t> g_roomscaleSafetySkippedProbes = 0;
std::atomic<uint64_t> g_roomscaleSafetyBlocked = 0;
std::atomic<uint64_t> g_roomscaleSafetyClamped = 0;
std::atomic<uint64_t> g_roomscaleSafetyFallbacks = 0;
std::atomic<uint64_t> g_roomscaleBodyShiftProbes = 0;
std::atomic<uint64_t> g_roomscaleBodyShiftBlocks = 0;
std::atomic<uint64_t> g_roomscaleBodyShiftCommits = 0;
std::atomic<bool> g_projectionCenterF5Down = false;
std::atomic<bool> g_projectionCentered = false;
std::atomic<bool> g_roomscaleF4Down = false;
std::atomic<bool> g_roomscaleEnabled = true;

void ResetStereoFillPhase()
{
    g_nextEyeIndex.store(0, std::memory_order_release);
    g_committableEye.store(-1, std::memory_order_release);
    g_committableEyePoseFrame.store(0, std::memory_order_release);
    g_state.pairRotationValid = false;
    g_state.pairBaseValid = false;
    g_state.pairViewsValid = false;
}

Vector3 TransformLocalDirectionToWorld(const Vector3& local, const std::array<float, 16>& baseView)
{
    Vector3 world{
        baseView[0] * local.x + baseView[4] * local.y + baseView[8] * local.z,
        baseView[1] * local.x + baseView[5] * local.y + baseView[9] * local.z,
        baseView[2] * local.x + baseView[6] * local.y + baseView[10] * local.z,
    };
    const float lengthSquared = world.x * world.x + world.y * world.y + world.z * world.z;
    if (!std::isfinite(lengthSquared) || lengthSquared < 1.0e-8f) {
        return {};
    }
    const float inverseLength = 1.0f / std::sqrt(lengthSquared);
    world.x *= inverseLength;
    world.y *= inverseLength;
    world.z *= inverseLength;
    return world;
}

Vector3 TransformLocalOffsetToWorld(const Vector3& local, const std::array<float, 16>& baseView)
{
    return {
        baseView[0] * local.x + baseView[4] * local.y + baseView[8] * local.z,
        baseView[1] * local.x + baseView[5] * local.y + baseView[9] * local.z,
        baseView[2] * local.x + baseView[6] * local.y + baseView[10] * local.z,
    };
}

Vector3 TransformWorldOffsetToLocal(const Vector3& world, const std::array<float, 16>& baseView)
{
    return {
        baseView[0] * world.x + baseView[1] * world.y + baseView[2] * world.z,
        baseView[4] * world.x + baseView[5] * world.y + baseView[6] * world.z,
        baseView[8] * world.x + baseView[9] * world.y + baseView[10] * world.z,
    };
}

bool Near(float left, float right, float epsilon = 1.0e-5f)
{
    return std::isfinite(left) && std::isfinite(right) && std::fabs(left - right) <= epsilon;
}

bool IsFinite(const Vector3& value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

void InvalidateRoomscaleSafetyCache()
{
    g_state.roomscaleSafetyCacheValid = false;
    g_state.roomscaleSafetyWasClamped = false;
    g_state.roomscaleSafetyResult = RoomscaleSafetyResult{};
}

RoomscaleProbeResult QueryRoomscaleProbe(
    const std::array<float, 3>& start,
    const Vector3& worldTranslation)
{
    RoomscaleProbeResult result;
    const std::array<float, 3> end = {
        start[0] + worldTranslation.x,
        start[1] + worldTranslation.y,
        start[2] + worldTranslation.z,
    };
    g_roomscaleSafetyProbes.fetch_add(1, std::memory_order_relaxed);
    g_roomscaleSafetyQueries.fetch_add(1, std::memory_order_relaxed);
    const bool staticOnly = !g_config.hplRoomscaleSafetyDynamic;
    if (g_checkLineOfSight(start.data(), end.data(), false, staticOnly)) {
        result.valid = true;
        return result;
    }

    result.blocked = true;
    g_roomscaleSafetyQueries.fetch_add(1, std::memory_order_relaxed);
    if (!g_checkLineOfSight(start.data(), start.data(), false, staticOnly)) {
        g_roomscaleSafetySkippedProbes.fetch_add(1, std::memory_order_relaxed);
        return result;
    }

    result.valid = true;
    float clearFraction = 0.0f;
    float blockedFraction = 1.0f;
    for (int index = 0; index < g_config.hplRoomscaleSafetyIterations; ++index) {
        const float candidateFraction = (clearFraction + blockedFraction) * 0.5f;
        const std::array<float, 3> candidate = {
            start[0] + worldTranslation.x * candidateFraction,
            start[1] + worldTranslation.y * candidateFraction,
            start[2] + worldTranslation.z * candidateFraction,
        };
        g_roomscaleSafetyQueries.fetch_add(1, std::memory_order_relaxed);
        if (g_checkLineOfSight(start.data(), candidate.data(), false, staticOnly)) {
            clearFraction = candidateFraction;
        } else {
            blockedFraction = candidateFraction;
        }
    }
    result.clearFraction = clearFraction;
    return result;
}

RoomscaleSafetyResult ClampRoomscaleHeadTranslation(
    const Vector3& translation,
    uint64_t poseFrame)
{
    RoomscaleSafetyResult result;
    result.translation = translation;
    if (!g_config.hplRoomscaleSafety
        || !g_roomscaleEnabled.load(std::memory_order_relaxed)
        || g_checkLineOfSight == nullptr
        || !g_state.baseMatricesValid
        || !IsFinite(translation)) {
        return result;
    }

    const float distance = std::sqrt(
        translation.x * translation.x
        + translation.y * translation.y
        + translation.z * translation.z);
    if (!std::isfinite(distance) || distance <= 0.001f) {
        return result;
    }

    const bool cacheMatch = g_state.roomscaleSafetyCacheValid
        && g_state.roomscaleSafetyFrame == poseFrame
        && Near(g_state.roomscaleSafetyInput.x, translation.x)
        && Near(g_state.roomscaleSafetyInput.y, translation.y)
        && Near(g_state.roomscaleSafetyInput.z, translation.z)
        && Near(g_state.roomscaleSafetyOrigin[0], g_state.parameters.origin[0])
        && Near(g_state.roomscaleSafetyOrigin[1], g_state.parameters.origin[1])
        && Near(g_state.roomscaleSafetyOrigin[2], g_state.parameters.origin[2]);
    if (cacheMatch) {
        return g_state.roomscaleSafetyResult;
    }

    g_roomscaleSafetySamples.fetch_add(1, std::memory_order_relaxed);
    const Vector3 worldTranslation = TransformLocalOffsetToWorld(translation, g_state.baseView);
    const std::array<float, 3> origin = g_state.parameters.origin;
    if (!IsFinite(worldTranslation)
        || !std::isfinite(origin[0]) || !std::isfinite(origin[1]) || !std::isfinite(origin[2])) {
        g_roomscaleSafetyFallbacks.fetch_add(1, std::memory_order_relaxed);
    } else {
        std::array<Vector3, camera_math::kMaxRoomscaleSafetySamples> offsets{};
        const float worldScale = std::max(g_config.hplWorldScale, 0.001f);
        const size_t offsetCount = BuildRoomscaleSafetySampleOffsets(
            g_config.hplRoomscaleSafetyRadiusMeters * worldScale,
            g_config.hplRoomscaleSafetyVerticalRadiusMeters * worldScale,
            g_config.hplRoomscaleSafetyRadialSamples,
            offsets);
        result.probeCount = static_cast<uint32_t>(offsetCount);
        float clearFraction = 1.0f;
        for (size_t index = 0; index < offsetCount; ++index) {
            const Vector3 worldOffset = TransformLocalOffsetToWorld(offsets[index], g_state.baseView);
            const std::array<float, 3> start = {
                origin[0] + worldOffset.x,
                origin[1] + worldOffset.y,
                origin[2] + worldOffset.z,
            };
            const RoomscaleProbeResult probe = QueryRoomscaleProbe(start, worldTranslation);
            if (!probe.valid) {
                continue;
            }
            ++result.validProbeCount;
            if (probe.blocked) {
                ++result.blockedProbeCount;
                clearFraction = std::min(clearFraction, probe.clearFraction);
            }
        }

        result.queried = result.validProbeCount != 0;
        if (!result.queried) {
            g_roomscaleSafetyFallbacks.fetch_add(1, std::memory_order_relaxed);
        } else if (result.blockedProbeCount != 0) {
            g_roomscaleSafetyBlocked.fetch_add(1, std::memory_order_relaxed);
            const float clearance = std::max(g_config.hplRoomscaleSafetyClearanceMeters, 0.0f)
                * worldScale;
            result.factor = ComputeRoomscaleSafetyFactor(clearFraction, distance, clearance);
            result.translation = {
                translation.x * result.factor,
                translation.y * result.factor,
                translation.z * result.factor,
            };
            result.clamped = result.factor < 0.999f;
            if (result.clamped) {
                g_roomscaleSafetyClamped.fetch_add(1, std::memory_order_relaxed);
            }
        }
    }

    const bool transition = result.clamped != g_state.roomscaleSafetyWasClamped;
    const uint64_t sample = g_roomscaleSafetySamples.load(std::memory_order_relaxed);
    const uint64_t interval = static_cast<uint64_t>(std::max(g_config.hplCameraLogInterval, 1));
    if (transition || sample <= 4 || sample % interval == 0) {
        Logger::Instance().Write(
            result.clamped ? LogLevel::Warn : LogLevel::Info,
            "hpl_roomscale_safety sample=%llu poseFrame=%llu queried=%d clamped=%d transition=%d factor=%.5f probes=%u validProbes=%u blockedProbes=%u raw=%.5f,%.5f,%.5f safe=%.5f,%.5f,%.5f distance=%.5f clearanceMeters=%.3f radiusMeters=%.3f verticalRadiusMeters=%.3f radialSamples=%d iterations=%d staticOnly=%d dynamic=%d",
            static_cast<unsigned long long>(sample),
            static_cast<unsigned long long>(poseFrame),
            result.queried ? 1 : 0,
            result.clamped ? 1 : 0,
            transition ? 1 : 0,
            result.factor,
            result.probeCount,
            result.validProbeCount,
            result.blockedProbeCount,
            translation.x, translation.y, translation.z,
            result.translation.x, result.translation.y, result.translation.z,
            distance,
            g_config.hplRoomscaleSafetyClearanceMeters,
            g_config.hplRoomscaleSafetyRadiusMeters,
            g_config.hplRoomscaleSafetyVerticalRadiusMeters,
            g_config.hplRoomscaleSafetyRadialSamples,
            g_config.hplRoomscaleSafetyIterations,
            g_config.hplRoomscaleSafetyDynamic ? 0 : 1,
            g_config.hplRoomscaleSafetyDynamic ? 1 : 0);
    }

    g_state.roomscaleSafetyCacheValid = true;
    g_state.roomscaleSafetyWasClamped = result.clamped;
    g_state.roomscaleSafetyFrame = poseFrame;
    g_state.roomscaleSafetyInput = translation;
    g_state.roomscaleSafetyOrigin = g_state.parameters.origin;
    g_state.roomscaleSafetyResult = result;
    return result;
}

Vector3 ResolveSafeTrackedOffset(
    const Vector3& trackedPosition,
    const Vector3& headCenter,
    uint64_t poseFrame,
    float eyeHeightOffsetMeters)
{
    const bool roomscaleEnabled = g_roomscaleEnabled.load(std::memory_order_relaxed);
    const Vector3 rawOffset = ResolveTrackedEyeOffset(
        trackedPosition,
        headCenter,
        g_state.neutralPosition,
        g_state.neutralOrientation,
        roomscaleEnabled,
        g_config.hplRoomscaleVertical,
        g_config.hplWorldScale,
        eyeHeightOffsetMeters);
    if (!roomscaleEnabled || !g_config.hplRoomscaleSafety) {
        return rawOffset;
    }

    const Vector3 rawHeadTranslation = ResolveTrackedEyeOffset(
        headCenter,
        headCenter,
        g_state.neutralPosition,
        g_state.neutralOrientation,
        true,
        g_config.hplRoomscaleVertical,
        g_config.hplWorldScale,
        0.0f);
    const RoomscaleSafetyResult safety = ClampRoomscaleHeadTranslation(
        rawHeadTranslation,
        poseFrame);
    return ReplaceTrackedHeadTranslation(rawOffset, rawHeadTranslation, safety.translation);
}

template <typename T>
bool ReadField(const void* object, size_t offset, T& value)
{
    if (native_memory::TryReadField(object, offset, value)) {
        return true;
    }
    value = T{};
    g_nativeMemoryReadFailures.fetch_add(1, std::memory_order_relaxed);
    return false;
}

template <typename T>
bool WriteField(void* object, size_t offset, const T& value)
{
    if (native_memory::TryWriteField(object, offset, value)) {
        return true;
    }
    g_nativeMemoryWriteFailures.fetch_add(1, std::memory_order_relaxed);
    return false;
}

bool ReadBytes(const void* source, void* destination, size_t size)
{
    if (native_memory::TryReadBytes(source, destination, size)) {
        return true;
    }
    g_nativeMemoryReadFailures.fetch_add(1, std::memory_order_relaxed);
    return false;
}

bool ReadFieldBytes(
    const void* object, size_t offset, void* destination, size_t size)
{
    if (native_memory::TryReadFieldBytes(object, offset, destination, size)) {
        return true;
    }
    g_nativeMemoryReadFailures.fetch_add(1, std::memory_order_relaxed);
    return false;
}

bool MarkCameraRotationDirty(void* camera)
{
    constexpr uint8_t dirty = 1;
    bool success = WriteField(camera, kCameraViewDirtyOffset, dirty);
    success = WriteField(camera, kCameraProjectionDirtyOffset, dirty) && success;
    success = WriteField(camera, kCameraBaseFrustumDirtyOffset, dirty) && success;
    success = WriteField(camera, kCameraSecondaryFrustumDirtyOffset, dirty) && success;
    return success;
}

bool CanWriteCameraRotation(void* camera, bool writePitch, bool writeRoll)
{
    if (camera == nullptr) {
        return false;
    }
    const bool pitchWritable = !writePitch
        || (native_memory::IsWritableFieldRange(
                camera, kCameraBasePitchOffset, sizeof(float))
            && native_memory::IsWritableFieldRange(
                camera, kCameraSecondaryRotationXOffset, sizeof(float)));
    const bool rollWritable = !writeRoll
        || (native_memory::IsWritableFieldRange(
                camera, kCameraBaseRollOffset, sizeof(float))
            && native_memory::IsWritableFieldRange(
                camera, kCameraSecondaryRotationZOffset, sizeof(float)));
    return pitchWritable
        && rollWritable
        && native_memory::IsWritableFieldRange(
            camera,
            kCameraViewDirtyOffset,
            kCameraSecondaryFrustumDirtyOffset - kCameraViewDirtyOffset + 1);
}

bool WriteCameraRotation(
    void* camera,
    bool writePitch,
    float basePitch,
    float secondaryPitch,
    bool writeRoll,
    float baseRoll,
    float secondaryRoll)
{
    bool success = true;
    if (writePitch) {
        success = WriteField(camera, kCameraBasePitchOffset, basePitch) && success;
        success = WriteField(camera, kCameraSecondaryRotationXOffset, secondaryPitch) && success;
    }
    if (writeRoll) {
        success = WriteField(camera, kCameraBaseRollOffset, baseRoll) && success;
        success = WriteField(camera, kCameraSecondaryRotationZOffset, secondaryRoll) && success;
    }
    success = MarkCameraRotationDirty(camera) && success;
    return success;
}

void LogNativeMemoryWarning(const char* operation, void* object)
{
    const uint32_t sample = g_nativeMemoryWarningLogs.fetch_add(1, std::memory_order_relaxed) + 1;
    if (sample <= 4) {
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_camera native_memory_failed operation=%s object=%p sample=%u action=skip_vr_mutation",
            operation,
            object,
            sample);
    }
}

bool MatchBytes(const void* address, const uint8_t* expected, size_t size)
{
    return address != nullptr && std::memcmp(address, expected, size) == 0;
}

bool IsInsideImage(HMODULE module, uintptr_t rva, size_t bytes)
{
    if (module == nullptr) {
        return false;
    }

    const auto* base = reinterpret_cast<const std::byte*>(module);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        return false;
    }

    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        return false;
    }

    const size_t imageSize = nt->OptionalHeader.SizeOfImage;
    return rva < imageSize && bytes <= imageSize - rva;
}

bool ReadFrustumParameters(const void* frustum, FrustumParameters& parameters)
{
    FrustumParameters candidate;
    uint8_t infiniteFar = 0;
    const bool success = ReadField(frustum, kFrustumFarOffset, candidate.farPlane)
        && ReadField(frustum, kFrustumNearOffset, candidate.nearPlane)
        && ReadField(frustum, kFrustumAspectOffset, candidate.aspect)
        && ReadField(frustum, kFrustumFovOffset, candidate.fov)
        && ReadField(frustum, kFrustumInfiniteFarOffset, infiniteFar)
        && ReadField(frustum, kFrustumProjectionTypeOffset, candidate.projectionType)
        && ReadFieldBytes(
            frustum,
            kFrustumOriginOffset,
            candidate.origin.data(),
            sizeof(candidate.origin));
    if (!success) {
        return false;
    }
    candidate.infiniteFar = infiniteFar != 0;
    parameters = candidate;
    return true;
}

bool IsCameraPerspective(const FrustumParameters& parameters)
{
    return parameters.projectionType == 0
        && std::isfinite(parameters.farPlane)
        && std::isfinite(parameters.nearPlane)
        && std::isfinite(parameters.fov)
        && std::isfinite(parameters.aspect)
        && parameters.farPlane > 10.0f
        && parameters.nearPlane > 0.0f
        && parameters.nearPlane < 0.5f
        && parameters.fov > 0.4f
        && parameters.fov < 2.6f
        && parameters.aspect > 0.5f
        && parameters.aspect < 4.0f;
}

bool WasFrustumDirty(const void* camera, bool& wasDirty)
{
    float secondaryX = 0.0f;
    float secondaryY = 0.0f;
    float secondaryZ = 0.0f;
    if (!ReadField(camera, kCameraSecondaryRotationXOffset, secondaryX)
        || !ReadField(camera, kCameraSecondaryRotationYOffset, secondaryY)
        || !ReadField(camera, kCameraSecondaryRotationZOffset, secondaryZ)) {
        wasDirty = false;
        return false;
    }
    const bool usesSecondaryFrustum = secondaryX != 0.0f || secondaryY != 0.0f || secondaryZ != 0.0f;
    uint8_t dirty = 0;
    if (!ReadField(
        camera,
        usesSecondaryFrustum ? kCameraSecondaryFrustumDirtyOffset : kCameraBaseFrustumDirtyOffset,
        dirty)) {
        wasDirty = false;
        return false;
    }
    wasDirty = dirty != 0;
    return true;
}

bool ReadHeadPose(
    Quaternion& orientation,
    Vector3& position,
    uint64_t& gameFrame,
    bool* fullyTracked = nullptr)
{
    if (fullyTracked != nullptr) {
        *fullyTracked = false;
    }
    if (g_openxr == nullptr) {
        return false;
    }

    OpenXRHeadPose pose;
    if (!g_openxr->GetLatestHeadPose(pose)) {
        return false;
    }

    orientation = Normalize({
        pose.orientationX,
        pose.orientationY,
        pose.orientationZ,
        pose.orientationW,
    });
    position = {pose.positionX, pose.positionY, pose.positionZ};
    gameFrame = pose.gameFrame;
    if (fullyTracked != nullptr) {
        *fullyTracked = pose.orientationTracked && pose.positionTracked;
    }
    return true;
}

bool ReadStereoViews(OpenXRStereoViewSnapshot& views)
{
    return g_openxr != nullptr && g_openxr->GetLatestStereoViews(views);
}

bool RefreshBaseMatrices(void* frustum, const FrustumParameters& parameters)
{
    std::array<float, 16> projection{};
    std::array<float, 16> view{};
    const bool success = ReadFieldBytes(
        frustum,
        kFrustumProjectionMatrixOffset,
        projection.data(),
        sizeof(projection))
        && ReadFieldBytes(
        frustum,
        kFrustumViewMatrixOffset,
        view.data(),
        sizeof(view));
    if (!success) {
        g_state.baseMatricesValid = false;
        return false;
    }
    g_state.baseProjection = projection;
    g_state.baseView = view;
    g_state.parameters = parameters;
    g_state.activeFrustum = frustum;
    g_state.baseMatricesValid = true;
    g_baseRefreshes.fetch_add(1, std::memory_order_relaxed);
    return true;
}

const char* StereoPairBaseActionName(StereoPairBaseAction action)
{
    switch (action) {
    case StereoPairBaseAction::CaptureFresh: return "capture_fresh";
    case StereoPairBaseAction::ReplayCached: return "replay_cached";
    case StereoPairBaseAction::RejectMissing: return "reject_missing";
    case StereoPairBaseAction::RejectStale: return "reject_stale";
    case StereoPairBaseAction::RejectFrustumMismatch: return "reject_frustum_mismatch";
    default: return "unknown";
    }
}

bool PrepareStereoPairBase(
    void* frustum,
    const FrustumParameters& parameters,
    uint32_t eyeIndex,
    bool wasDirty)
{
    const uint64_t nowMilliseconds = GetTickCount64();
    const uint64_t ageMilliseconds = g_state.pairBaseValid
        && nowMilliseconds >= g_state.pairBaseCapturedAtMilliseconds
            ? nowMilliseconds - g_state.pairBaseCapturedAtMilliseconds
            : 0;
    const StereoPairBaseAction action = ResolveStereoPairBaseAction(
        eyeIndex,
        g_state.pairBaseValid,
        g_state.pairBaseFrustum == frustum,
        ageMilliseconds,
        kStereoPairBaseMaxAgeMilliseconds);

    if (action == StereoPairBaseAction::CaptureFresh) {
        if (wasDirty || !g_state.baseMatricesValid || g_state.activeFrustum != frustum) {
            if (!RefreshBaseMatrices(frustum, parameters)) {
                LogNativeMemoryWarning("refresh_pair_base_matrices", frustum);
                return false;
            }
        }
        g_state.pairBaseProjection = g_state.baseProjection;
        g_state.pairBaseView = g_state.baseView;
        g_state.pairBaseParameters = g_state.parameters;
        g_state.pairBaseFrustum = frustum;
        g_state.pairBaseCapturedAtMilliseconds = nowMilliseconds;
        g_state.pairBaseValid = true;
        g_pairBaseLatches.fetch_add(1, std::memory_order_relaxed);
        return true;
    }

    if (action == StereoPairBaseAction::ReplayCached) {
        g_state.baseProjection = g_state.pairBaseProjection;
        g_state.baseView = g_state.pairBaseView;
        g_state.parameters = g_state.pairBaseParameters;
        g_state.activeFrustum = g_state.pairBaseFrustum;
        g_state.baseMatricesValid = true;
        g_pairBaseReplays.fetch_add(1, std::memory_order_relaxed);
        return true;
    }

    if (action == StereoPairBaseAction::RejectMissing) {
        g_pairBaseMissingRejects.fetch_add(1, std::memory_order_relaxed);
    } else if (action == StereoPairBaseAction::RejectStale) {
        g_pairBaseStaleRejects.fetch_add(1, std::memory_order_relaxed);
    } else if (action == StereoPairBaseAction::RejectFrustumMismatch) {
        g_pairBaseFrustumRejects.fetch_add(1, std::memory_order_relaxed);
    }
    Logger::Instance().Write(
        LogLevel::Warn,
        "hpl_stereo pair_base_rejected eye=%u action=%s ageMs=%llu maxAgeMs=%llu frustum=%p cachedFrustum=%p policy=abandon_pair_no_mixed_base",
        eyeIndex,
        StereoPairBaseActionName(action),
        static_cast<unsigned long long>(ageMilliseconds),
        static_cast<unsigned long long>(kStereoPairBaseMaxAgeMilliseconds),
        frustum,
        g_state.pairBaseFrustum);
    return false;
}

bool ResolveStereoPairViews(uint32_t eyeIndex, OpenXRStereoViewSnapshot& views)
{
    views = {};
    if (eyeIndex == 0) {
        if (!ReadStereoViews(views) || !views.valid || views.gameFrame == 0) {
            g_pairViewRejects.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
        g_state.pairViews = views;
        g_state.pairViewsValid = true;
        g_pairViewLatches.fetch_add(1, std::memory_order_relaxed);
        return true;
    }

    if (!g_state.pairViewsValid
        || !g_state.pairViews.valid
        || g_state.pairViews.gameFrame == 0) {
        const uint64_t rejects = g_pairViewRejects.fetch_add(1, std::memory_order_relaxed) + 1;
        if (rejects <= 8 || rejects % 120 == 0) {
            Logger::Instance().Write(
                LogLevel::Warn,
                "hpl_stereo pair_views_rejected eye=%u rejects=%llu policy=abandon_pair_no_cross_tick_pose",
                eyeIndex,
                static_cast<unsigned long long>(rejects));
        }
        return false;
    }

    views = g_state.pairViews;
    g_pairViewReplays.fetch_add(1, std::memory_order_relaxed);
    return true;
}

void SetupFrustum(
    void* frustum,
    const std::array<float, 16>& projection,
    const std::array<float, 16>& view,
    float fov,
    float aspect,
    const std::array<float, 3>& origin)
{
    const FrustumParameters& parameters = g_state.parameters;
    g_setupPerspectiveFrustum(
        frustum,
        projection.data(),
        view.data(),
        parameters.farPlane,
        parameters.nearPlane,
        fov,
        aspect,
        origin.data(),
        parameters.infiniteFar,
        nullptr,
        false);
}

void SetupWithView(void* frustum, const std::array<float, 16>& view)
{
    SetupFrustum(
        frustum,
        g_state.baseProjection,
        view,
        g_state.parameters.fov,
        g_state.parameters.aspect,
        g_state.parameters.origin);
}

void RestoreBaseView(void* frustum)
{
    if (g_state.baseMatricesValid && g_setupPerspectiveFrustum != nullptr && frustum != nullptr) {
        SetupWithView(frustum, g_state.baseView);
    }
}

bool ApplyStereoEye(
    void* frustum,
    const OpenXRStereoViewSnapshot& views,
    uint32_t eyeIndex,
    bool wasDirty)
{
    if (eyeIndex >= 2 || !views.valid || !views.eyes[eyeIndex].valid) {
        return false;
    }

    const OpenXREyeView& eye = views.eyes[eyeIndex];
    const bool projectionCentered = g_projectionCentered.load(std::memory_order_relaxed);
    OpenXREyeView renderEye = projectionCentered ? CenterProjectionFov(eye) : eye;
    Quaternion eyeOrientation = Normalize({
        eye.orientationX,
        eye.orientationY,
        eye.orientationZ,
        eye.orientationW,
    });
    if (eyeIndex == 0) {
        g_state.pairRotation = eyeOrientation;
        g_state.pairRotationValid = true;
        g_pairRotationLatches.fetch_add(1, std::memory_order_relaxed);
    } else if (g_state.pairRotationValid) {
        eyeOrientation = g_state.pairRotation;
        g_pairRotationReuses.fetch_add(1, std::memory_order_relaxed);
    }
    renderEye.orientationX = eyeOrientation.x;
    renderEye.orientationY = eyeOrientation.y;
    renderEye.orientationZ = eyeOrientation.z;
    renderEye.orientationW = eyeOrientation.w;
    const Quaternion eyeViewRotation = Normalize(Multiply(
        Conjugate(eyeOrientation),
        g_state.neutralOrientation));

    const Vector3 currentHeadCenter{
        (views.eyes[0].positionX + views.eyes[1].positionX) * 0.5f,
        (views.eyes[0].positionY + views.eyes[1].positionY) * 0.5f,
        (views.eyes[0].positionZ + views.eyes[1].positionZ) * 0.5f,
    };
    const bool roomscaleEnabled = g_roomscaleEnabled.load(std::memory_order_relaxed);
    const Vector3 relativeEyePosition = ResolveSafeTrackedOffset(
        {eye.positionX, eye.positionY, eye.positionZ},
        currentHeadCenter,
        eye.gameFrame,
        g_config.hplEyeHeightOffsetMeters);

    const std::array<float, 16> inverseEyeTranslation = TranslationMatrix({
        -relativeEyePosition.x,
        -relativeEyePosition.y,
        -relativeEyePosition.z,
    });
    const std::array<float, 16> eyeDeltaView = MatrixMultiply(
        RotationMatrix(eyeViewRotation),
        inverseEyeTranslation);
    const std::array<float, 16> modifiedView = MatrixMultiply(
        eyeDeltaView,
        g_state.baseView);

    std::array<float, 16> eyeProjection{};
    float eyeFov = 0.0f;
    float eyeAspect = 0.0f;
    if (!BuildOpenXRProjection(
            renderEye,
            g_state.parameters.nearPlane,
            g_state.parameters.farPlane,
            eyeProjection,
            eyeFov,
            eyeAspect)) {
        return false;
    }

    const std::array<float, 3> eyeOrigin = {
        g_state.parameters.origin[0]
            + g_state.baseView[0] * relativeEyePosition.x
            + g_state.baseView[4] * relativeEyePosition.y
            + g_state.baseView[8] * relativeEyePosition.z,
        g_state.parameters.origin[1]
            + g_state.baseView[1] * relativeEyePosition.x
            + g_state.baseView[5] * relativeEyePosition.y
            + g_state.baseView[9] * relativeEyePosition.z,
        g_state.parameters.origin[2]
            + g_state.baseView[2] * relativeEyePosition.x
            + g_state.baseView[6] * relativeEyePosition.y
            + g_state.baseView[10] * relativeEyePosition.z,
    };

    SetupFrustum(
        frustum,
        eyeProjection,
        modifiedView,
        eyeFov,
        eyeAspect,
        eyeOrigin);
    if (g_openxr == nullptr || !g_openxr->MarkRenderedStereoEye(eyeIndex, renderEye)) {
        return false;
    }
    g_state.currentEyeIndex = static_cast<int>(eyeIndex);
    g_state.currentEyePoseFrame = eye.gameFrame;
    g_committableEyePoseFrame.store(renderEye.gameFrame, std::memory_order_release);
    g_committableEye.store(static_cast<int>(eyeIndex), std::memory_order_release);

    const uint64_t stereoApplied = g_stereoAppliedCalls.fetch_add(1, std::memory_order_relaxed) + 1;
    g_stereoEyeCalls[eyeIndex].fetch_add(1, std::memory_order_relaxed);
    g_appliedCalls.fetch_add(1, std::memory_order_relaxed);
    const uint64_t logInterval = static_cast<uint64_t>(std::max(g_config.hplCameraLogInterval, 1));
    if (stereoApplied <= 2 || stereoApplied % logInterval == 0) {
        Logger::Instance().Write(
            LogLevel::Info,
            "hpl_stereo applied=%llu eye=%u poseFrame=%llu poseAgeFrames=%llu dirtyBefore=%d worldScale=%.4f eyeHeightOffsetMeters=%.4f verticalRoomscale=%d eyeOffset=%.5f,%.5f,%.5f fovDegrees=%.3f aspect=%.5f projectionCentered=%d roomscale=%d projectionOffset=%.6f,%.6f",
            static_cast<unsigned long long>(stereoApplied),
            eyeIndex,
            static_cast<unsigned long long>(eye.gameFrame),
            static_cast<unsigned long long>(views.head.sampleAgeFrames),
            wasDirty ? 1 : 0,
            g_config.hplWorldScale,
            g_config.hplEyeHeightOffsetMeters,
            g_config.hplRoomscaleVertical ? 1 : 0,
            relativeEyePosition.x,
            relativeEyePosition.y,
            relativeEyePosition.z,
            eyeFov * kRadiansToDegrees,
            eyeAspect,
            projectionCentered ? 1 : 0,
            roomscaleEnabled ? 1 : 0,
            eyeProjection[2],
            eyeProjection[6]);
    }
    return true;
}

void* HookCameraGetFrustum(void* camera, bool projectionFlag)
{
    const uintptr_t returnAddress = reinterpret_cast<uintptr_t>(_ReturnAddress());
    const uintptr_t callerRva = returnAddress >= g_executableBase
        ? returnAddress - g_executableBase
        : UINTPTR_MAX;
    g_getFrustumCalls.fetch_add(1, std::memory_order_relaxed);
    if (g_originalCameraGetFrustum == nullptr) {
        return nullptr;
    }

    bool wasDirty = false;
    float nativeBasePitch = 0.0f;
    float nativeSecondaryPitch = 0.0f;
    float nativeBaseRoll = 0.0f;
    float nativeSecondaryRoll = 0.0f;
    bool cameraFieldsReadable = camera != nullptr;
    if (camera != nullptr) {
        cameraFieldsReadable = WasFrustumDirty(camera, wasDirty) && cameraFieldsReadable;
        cameraFieldsReadable = ReadField(camera, kCameraBasePitchOffset, nativeBasePitch)
            && cameraFieldsReadable;
        cameraFieldsReadable = ReadField(camera, kCameraSecondaryRotationXOffset, nativeSecondaryPitch)
            && cameraFieldsReadable;
        cameraFieldsReadable = ReadField(camera, kCameraBaseRollOffset, nativeBaseRoll)
            && cameraFieldsReadable;
        cameraFieldsReadable = ReadField(camera, kCameraSecondaryRotationZOffset, nativeSecondaryRoll)
            && cameraFieldsReadable;
    }
    const bool nativeRollActive = cameraFieldsReadable
        && std::isfinite(nativeBaseRoll) && std::isfinite(nativeSecondaryRoll)
        && (nativeBaseRoll != 0.0f || nativeSecondaryRoll != 0.0f);
    const bool nativePitchActive = cameraFieldsReadable
        && std::isfinite(nativeBasePitch) && std::isfinite(nativeSecondaryPitch)
        && (nativeBasePitch != 0.0f || nativeSecondaryPitch != 0.0f);
    bool trackingOwnsCamera = false;
    if (camera != nullptr && (nativeRollActive || nativePitchActive)) {
        std::lock_guard lock(g_stateMutex);
        trackingOwnsCamera = g_state.trackingEnabled && g_state.activeCamera == camera;
    }
    const bool suppressNativeRollRequested = trackingOwnsCamera && nativeRollActive
        && g_config.hplNativeCameraRollSuppression;
    const bool suppressNativePitchRequested = trackingOwnsCamera && nativePitchActive
        && g_config.hplNativeCameraPitchSuppression;
    bool suppressNativePitch = false;
    bool suppressNativeRoll = false;
    if (suppressNativePitchRequested || suppressNativeRollRequested) {
        constexpr float zero = 0.0f;
        if (!CanWriteCameraRotation(camera, suppressNativePitchRequested, suppressNativeRollRequested)) {
            g_nativeMemoryWriteFailures.fetch_add(1, std::memory_order_relaxed);
            LogNativeMemoryWarning("preflight_camera_suppression", camera);
        } else if (WriteCameraRotation(
                       camera,
                       suppressNativePitchRequested,
                       zero,
                       zero,
                       suppressNativeRollRequested,
                       zero,
                       zero)) {
            suppressNativePitch = suppressNativePitchRequested;
            suppressNativeRoll = suppressNativeRollRequested;
        } else {
            WriteCameraRotation(
                camera,
                suppressNativePitchRequested,
                nativeBasePitch,
                nativeSecondaryPitch,
                suppressNativeRollRequested,
                nativeBaseRoll,
                nativeSecondaryRoll);
            LogNativeMemoryWarning("apply_camera_suppression", camera);
        }
    }
    void* frustum = g_originalCameraGetFrustum(camera, projectionFlag);
    if (suppressNativePitch || suppressNativeRoll) {
        if (WriteCameraRotation(
                camera,
                suppressNativePitch,
                nativeBasePitch,
                nativeSecondaryPitch,
                suppressNativeRoll,
                nativeBaseRoll,
                nativeSecondaryRoll)) {
            if (suppressNativePitch) {
                g_nativePitchSuppressedCalls.fetch_add(1, std::memory_order_relaxed);
            }
            if (suppressNativeRoll) {
                g_nativeRollSuppressedCalls.fetch_add(1, std::memory_order_relaxed);
            }
        } else {
            LogNativeMemoryWarning("restore_camera_rotation", camera);
        }
    }
    if (camera == nullptr || frustum == nullptr || g_setupPerspectiveFrustum == nullptr) {
        return frustum;
    }
    if (callerRva != kRenderViewportGetFrustumReturnRva) {
        return frustum;
    }

    FrustumParameters parameters;
    if (!ReadFrustumParameters(frustum, parameters)) {
        LogNativeMemoryWarning("read_frustum_parameters", frustum);
        return frustum;
    }
    if (!IsCameraPerspective(parameters)) {
        return frustum;
    }

    const uint64_t candidate = g_candidateCalls.fetch_add(1, std::memory_order_relaxed) + 1;
    if (nativeRollActive) {
        g_nativeRollObservedCalls.fetch_add(1, std::memory_order_relaxed);
    }
    if (nativePitchActive) {
        g_nativePitchObservedCalls.fetch_add(1, std::memory_order_relaxed);
    }
    const uint64_t candidateLogInterval = static_cast<uint64_t>(std::max(g_config.hplCameraLogInterval, 1));
    if (candidate <= 8
        || ((nativeRollActive || nativePitchActive) && candidate % candidateLogInterval == 0)) {
        Logger::Instance().Write(
            LogLevel::Info,
            "hpl_camera candidate=%llu callerRva=0x%llx camera=%p frustum=%p dirtyBefore=%d projectionFlag=%d far=%.5f near=%.5f fov=%.5f aspect=%.5f nativeBasePitch=%.6f nativeExtendedPitch=%.6f pitchSuppressed=%d nativeBaseRoll=%.6f nativeExtendedRoll=%.6f rollSuppressed=%d",
            static_cast<unsigned long long>(candidate),
            static_cast<unsigned long long>(callerRva),
            camera,
            frustum,
            wasDirty ? 1 : 0,
            projectionFlag ? 1 : 0,
            parameters.farPlane,
            parameters.nearPlane,
            parameters.fov,
            parameters.aspect,
            nativeBasePitch,
            nativeSecondaryPitch,
            suppressNativePitch ? 1 : 0,
            nativeBaseRoll,
            nativeSecondaryRoll,
            suppressNativeRoll ? 1 : 0);
    }

    HPLPlayerStateSnapshot playerState{};
    const bool playerCameraKnown = GetHPLPlayerStateSnapshot(playerState)
        && playerState.playerValid
        && playerState.camera != nullptr;

    std::lock_guard lock(g_stateMutex);
    void* controlCamera = nullptr;
    const char* controlOwner = "candidate_fallback";
    if ((g_state.trackingEnabled || g_state.activationPending)
        && g_state.activeCamera != nullptr) {
        controlCamera = g_state.activeCamera;
        controlOwner = "active_vr_camera";
    } else if (playerCameraKnown) {
        controlCamera = playerState.camera;
        controlOwner = "player_camera";
    }
    if (controlCamera != nullptr && controlCamera != camera) {
        const uint64_t secondary = g_secondaryCameraCandidates.fetch_add(
            1, std::memory_order_relaxed) + 1;
        g_secondaryCameraControlSkips.fetch_add(1, std::memory_order_relaxed);
        if (secondary <= 12 || secondary % candidateLogInterval == 0) {
            Logger::Instance().Write(
                LogLevel::Info,
                "hpl_camera secondary_candidate=%llu camera=%p frustum=%p controlCamera=%p controlOwner=%s playerCamera=%p activeCamera=%p tracking=%d activationPending=%d policy=native_frustum_no_vr_controls",
                static_cast<unsigned long long>(secondary),
                camera,
                frustum,
                controlCamera,
                controlOwner,
                playerCameraKnown ? playerState.camera : nullptr,
                g_state.activeCamera,
                g_state.trackingEnabled ? 1 : 0,
                g_state.activationPending ? 1 : 0);
        }
        return frustum;
    }
    if (g_config.hplRoomscaleControl) {
        const bool f4Down = (GetAsyncKeyState(VK_F4) & 0x8000) != 0;
        const bool wasF4Down = g_roomscaleF4Down.exchange(f4Down, std::memory_order_relaxed);
        if (f4Down && !wasF4Down) {
            const bool enabled = !g_roomscaleEnabled.load(std::memory_order_relaxed);
            g_roomscaleEnabled.store(enabled, std::memory_order_relaxed);
            InvalidateRoomscaleSafetyCache();
            Logger::Instance().Write(
                LogLevel::Warn,
                "hpl_roomscale enabled=%d key=F4 policy=retain_ipd_and_orientation stereo=%d",
                enabled ? 1 : 0,
                g_state.stereoEnabled ? 1 : 0);
        }
    }
    if (g_config.hplProjectionCenterControl) {
        const bool f5Down = (GetAsyncKeyState(VK_F5) & 0x8000) != 0;
        const bool wasF5Down = g_projectionCenterF5Down.exchange(f5Down, std::memory_order_relaxed);
        if (f5Down && !wasF5Down) {
            const bool centered = !g_projectionCentered.load(std::memory_order_relaxed);
            g_projectionCentered.store(centered, std::memory_order_relaxed);
            Logger::Instance().Write(
                LogLevel::Warn,
                "hpl_projection_center enabled=%d key=F5 policy=fully_symmetric_fov stereo=%d",
                centered ? 1 : 0,
                g_state.stereoEnabled ? 1 : 0);
        }
    }
    const bool f2Down = (GetAsyncKeyState(VK_F2) & 0x8000) != 0;
    const bool f2Pressed = f2Down && !g_state.f2Down;
    g_state.f2Down = f2Down;
    const bool f10Down = (GetAsyncKeyState(VK_CONTROL) & 0x8000) == 0
        && (GetAsyncKeyState(VK_F10) & 0x8000) != 0;
    const bool f10Pressed = f10Down && !g_state.f10Down;
    g_state.f10Down = f10Down;
    const bool f11Down = (GetAsyncKeyState(VK_F11) & 0x8000) != 0;
    const bool f11Pressed = f11Down && !g_state.f11Down;
    g_state.f11Down = f11Down;

    if (f10Pressed && (g_state.trackingEnabled || g_state.activationPending)) {
        if (g_state.activationPending && !g_state.trackingEnabled) {
            const bool runtimeSuspended = g_openxr != nullptr
                && g_openxr->Suspend("f10_activation_cancelled");
            g_state = BridgeState{};
            g_state.f2Down = f2Down;
            g_state.f10Down = true;
            g_state.f11Down = f11Down;
            Logger::Instance().Write(
                LogLevel::Warn,
                "hpl_vr_mode cancelled key=F10 reason=user_request runtimeSuspended=%d",
                runtimeSuspended ? 1 : 0);
            return frustum;
        }
        bool runtimeSuspended = false;
        if (g_openxr != nullptr) {
            g_openxr->SetStereoSubmissionEnabled(false);
            runtimeSuspended = g_openxr->Suspend("f10_vr_mode_disabled");
        }
        const bool restored = g_state.activeCamera == camera;
        if (restored) {
            RestoreBaseView(frustum);
        }
        Logger::Instance().Write(
            LogLevel::Warn,
            "hpl_vr_mode disabled key=F10 camera=%p frustum=%p restored=%d recenterPending=%d applied=%llu runtimeSuspended=%d",
            camera,
            frustum,
            restored ? 1 : 0,
            g_state.recenterPending ? 1 : 0,
            static_cast<unsigned long long>(g_appliedCalls.load(std::memory_order_relaxed)),
            runtimeSuspended ? 1 : 0);
        g_state = BridgeState{};
        g_state.f2Down = f2Down;
        g_state.f10Down = true;
        g_state.f11Down = f11Down;
        return frustum;
    }

    if ((g_state.trackingEnabled || g_state.activationPending) && g_state.activeCamera != camera) {
        return frustum;
    }

    if (f10Pressed && !g_state.trackingEnabled) {
        const bool runtimeRequested = g_openxr != nullptr && g_openxr->RequestManualStart();
        if (!runtimeRequested) {
            Logger::Instance().Write(
                LogLevel::Warn,
                "hpl_vr_mode request_failed key=F10 reason=openxr_unavailable camera=%p",
                camera);
            return frustum;
        }
        g_projectionCentered.store(true, std::memory_order_relaxed);
        g_roomscaleEnabled.store(g_config.hplRoomscaleEnabledDefault, std::memory_order_relaxed);
        g_state.activationPending = true;
        g_state.activeCamera = camera;
        g_state.activeFrustum = frustum;
        g_state.baseMatricesValid = false;
        InvalidateRoomscaleSafetyCache();
        Logger::Instance().Write(
            LogLevel::Warn,
            "hpl_vr_mode requested key=F10 runtimeRequested=%d camera=%p frustum=%p policy=openxr_tracking_stereo_fullcenter",
            runtimeRequested ? 1 : 0,
            camera,
            frustum);
    }

    if (g_state.activationPending && g_state.activeCamera == camera) {
        Quaternion orientation;
        Vector3 position;
        uint64_t poseFrame = 0;
        bool fullyTracked = false;
        OpenXRStereoViewSnapshot views;
        const bool poseReady = ReadHeadPose(orientation, position, poseFrame, &fullyTracked);
        const bool stereoReady = !g_config.hplStereoAfr || ReadStereoViews(views);
        if (!poseReady || !stereoReady) {
            return frustum;
        }
        if (!fullyTracked) {
            if (g_state.activationTrackingWaitLogs < 4) {
                ++g_state.activationTrackingWaitLogs;
                Logger::Instance().Write(
                    LogLevel::Info,
                    "hpl_vr_mode calibration_wait reason=tracking_not_settled poseFrame=%llu sample=%u",
                    static_cast<unsigned long long>(poseFrame),
                    g_state.activationTrackingWaitLogs);
            }
            return frustum;
        }
        if (g_config.hplStereoAfr && views.gameFrame != poseFrame) {
            return frustum;
        }

        float positionStep = 0.0f;
        float orientationStepRadians = 0.0f;
        const PoseStabilityUpdate stability = UpdatePoseStability(
            g_state.activationPoseStability,
            poseFrame,
            orientation,
            position,
            kActivationStablePoseFrames,
            kActivationMaxPositionStepMeters,
            kActivationMaxOrientationStepRadians,
            positionStep,
            orientationStepRadians);
        if (stability == PoseStabilityUpdate::Invalid) {
            Logger::Instance().Write(
                LogLevel::Warn,
                "hpl_vr_mode calibration_wait reason=invalid_pose poseFrame=%llu",
                static_cast<unsigned long long>(poseFrame));
            return frustum;
        }
        if (stability == PoseStabilityUpdate::Started) {
            Logger::Instance().Write(
                LogLevel::Info,
                "hpl_vr_mode calibration_wait reason=initial_tracked_pose poseFrame=%llu stable=%u/%u position=%.6f,%.6f,%.6f",
                static_cast<unsigned long long>(poseFrame),
                g_state.activationPoseStability.consecutiveFrames,
                kActivationStablePoseFrames,
                position.x,
                position.y,
                position.z);
            return frustum;
        }
        if (stability == PoseStabilityUpdate::Reset) {
            Logger::Instance().Write(
                LogLevel::Warn,
                "hpl_vr_mode calibration_reset reason=reference_space_jump poseFrame=%llu positionStep=%.5f orientationStepDeg=%.3f stable=%u/%u position=%.6f,%.6f,%.6f",
                static_cast<unsigned long long>(poseFrame),
                positionStep,
                orientationStepRadians * kRadiansToDegrees,
                g_state.activationPoseStability.consecutiveFrames,
                kActivationStablePoseFrames,
                position.x,
                position.y,
                position.z);
            return frustum;
        }
        if (stability != PoseStabilityUpdate::Ready) {
            return frustum;
        }

        g_state.activationPending = false;
        g_state.trackingEnabled = true;
        g_state.neutralOrientation = YawOnly(orientation);
        g_state.neutralPosition = position;
        ++g_state.calibrationGeneration;
        g_state.baseMatricesValid = false;
        InvalidateRoomscaleSafetyCache();
        if (g_config.hplStereoAfr) {
            g_state.stereoEnabled = true;
            g_state.stereoApplyFailures = 0;
            ResetStereoFillPhase();
            g_state.currentEyeIndex = -1;
            g_state.currentEyePoseFrame = 0;
            if (g_openxr != nullptr) {
                g_openxr->SetStereoSubmissionEnabled(true);
            }
        }
        Logger::Instance().Write(
            LogLevel::Warn,
            "hpl_vr_mode activated key=F10 camera=%p frustum=%p poseFrame=%llu tracking=1 fullyTracked=1 stablePoseFrames=%u stereo=%d projectionCentered=%d roomscale=%d neutralPosition=%.6f,%.6f,%.6f neutralQuaternion=%.6f,%.6f,%.6f,%.6f",
            camera,
            frustum,
            static_cast<unsigned long long>(poseFrame),
            g_state.activationPoseStability.consecutiveFrames,
            g_state.stereoEnabled ? 1 : 0,
            g_projectionCentered.load(std::memory_order_relaxed) ? 1 : 0,
            g_roomscaleEnabled.load(std::memory_order_relaxed) ? 1 : 0,
            position.x,
            position.y,
            position.z,
            g_state.neutralOrientation.x,
            g_state.neutralOrientation.y,
            g_state.neutralOrientation.z,
            g_state.neutralOrientation.w);
    }

    if (!g_state.trackingEnabled || g_state.activeCamera != camera) {
        if (f2Pressed && g_config.hplRecenterControl) {
            Logger::Instance().Write(
                LogLevel::Warn,
                "hpl_recenter ignored key=F2 reason=head_tracking_disabled");
        }
        if (f11Pressed) {
            Logger::Instance().Write(
                LogLevel::Warn,
                "hpl_stereo enable_ignored key=F11 reason=head_tracking_disabled");
        }
        return frustum;
    }

    if (f2Pressed) {
        if (!g_config.hplRecenterControl) {
            Logger::Instance().Write(
                LogLevel::Warn,
                "hpl_recenter ignored key=F2 reason=config_disabled");
        } else {
            g_state.recenterPending = true;
            g_state.recenterTrackingWaitLogs = 0;
            g_state.recenterPoseStability = PoseStabilityState{};
            InvalidateRoomscaleSafetyCache();
            Logger::Instance().Write(
                LogLevel::Warn,
                "hpl_recenter requested key=F2 camera=%p frustum=%p stereo=%d roomscale=%d",
                camera,
                frustum,
                g_state.stereoEnabled ? 1 : 0,
                g_roomscaleEnabled.load(std::memory_order_relaxed) ? 1 : 0);
        }
    }

    if (f11Pressed) {
        if (g_state.stereoEnabled) {
            g_state.stereoEnabled = false;
            g_state.currentEyeIndex = -1;
            g_state.currentEyePoseFrame = 0;
            ResetStereoFillPhase();
            if (g_openxr != nullptr) {
                g_openxr->SetStereoSubmissionEnabled(false);
            }
            Logger::Instance().Write(
                LogLevel::Warn,
                "hpl_stereo disabled key=F11 fallback=mono_orientation");
        } else if (!g_config.hplStereoAfr) {
            Logger::Instance().Write(
                LogLevel::Warn,
                "hpl_stereo enable_ignored key=F11 reason=config_disabled");
        } else {
            OpenXRStereoViewSnapshot views;
            if (!ReadStereoViews(views)) {
                Logger::Instance().Write(
                    LogLevel::Warn,
                    "hpl_stereo enable_ignored key=F11 reason=stereo_views_unavailable");
            } else {
                g_state.stereoEnabled = true;
                g_state.stereoApplyFailures = 0;
                ResetStereoFillPhase();
                g_state.currentEyeIndex = -1;
                g_state.currentEyePoseFrame = 0;
                if (g_openxr != nullptr) {
                    g_openxr->SetStereoSubmissionEnabled(true);
                }
                Logger::Instance().Write(
                    LogLevel::Warn,
                    "hpl_stereo enabled key=F11 mode=alternating_eye worldScale=%.4f poseFrame=%llu ipdMeters=%.5f",
                    g_config.hplWorldScale,
                    static_cast<unsigned long long>(views.gameFrame),
                    std::sqrt(
                        (views.eyes[1].positionX - views.eyes[0].positionX)
                            * (views.eyes[1].positionX - views.eyes[0].positionX)
                        + (views.eyes[1].positionY - views.eyes[0].positionY)
                            * (views.eyes[1].positionY - views.eyes[0].positionY)
                        + (views.eyes[1].positionZ - views.eyes[0].positionZ)
                            * (views.eyes[1].positionZ - views.eyes[0].positionZ)));
            }
        }
    }

    if (g_state.recenterPending) {
        do {
            Quaternion orientation;
            Vector3 position;
            uint64_t poseFrame = 0;
            bool fullyTracked = false;
            OpenXRStereoViewSnapshot views;
            const bool poseReady = ReadHeadPose(orientation, position, poseFrame, &fullyTracked);
            const bool stereoReady = !g_state.stereoEnabled || ReadStereoViews(views);
            if (!poseReady || !stereoReady) {
                break;
            }
            if (!fullyTracked) {
                if (g_state.recenterTrackingWaitLogs < 4) {
                    ++g_state.recenterTrackingWaitLogs;
                    Logger::Instance().Write(
                        LogLevel::Info,
                        "hpl_recenter calibration_wait reason=tracking_not_settled poseFrame=%llu sample=%u",
                        static_cast<unsigned long long>(poseFrame),
                        g_state.recenterTrackingWaitLogs);
                }
                break;
            }
            if (g_state.stereoEnabled && views.gameFrame != poseFrame) {
                break;
            }

            float positionStep = 0.0f;
            float orientationStepRadians = 0.0f;
            const PoseStabilityUpdate stability = UpdatePoseStability(
                g_state.recenterPoseStability,
                poseFrame,
                orientation,
                position,
                kActivationStablePoseFrames,
                kActivationMaxPositionStepMeters,
                kActivationMaxOrientationStepRadians,
                positionStep,
                orientationStepRadians);
            if (stability == PoseStabilityUpdate::Invalid) {
                Logger::Instance().Write(
                    LogLevel::Warn,
                    "hpl_recenter calibration_wait reason=invalid_pose poseFrame=%llu",
                    static_cast<unsigned long long>(poseFrame));
                break;
            }
            if (stability == PoseStabilityUpdate::Started) {
                Logger::Instance().Write(
                    LogLevel::Info,
                    "hpl_recenter calibration_wait reason=initial_tracked_pose poseFrame=%llu stable=%u/%u position=%.6f,%.6f,%.6f",
                    static_cast<unsigned long long>(poseFrame),
                    g_state.recenterPoseStability.consecutiveFrames,
                    kActivationStablePoseFrames,
                    position.x,
                    position.y,
                    position.z);
                break;
            }
            if (stability == PoseStabilityUpdate::Reset) {
                Logger::Instance().Write(
                    LogLevel::Warn,
                    "hpl_recenter calibration_reset reason=reference_space_jump poseFrame=%llu positionStep=%.5f orientationStepDeg=%.3f stable=%u/%u position=%.6f,%.6f,%.6f",
                    static_cast<unsigned long long>(poseFrame),
                    positionStep,
                    orientationStepRadians * kRadiansToDegrees,
                    g_state.recenterPoseStability.consecutiveFrames,
                    kActivationStablePoseFrames,
                    position.x,
                    position.y,
                    position.z);
                break;
            }
            if (stability != PoseStabilityUpdate::Ready) {
                break;
            }

            g_state.recenterPending = false;
            g_state.neutralOrientation = YawOnly(orientation);
            g_state.neutralPosition = position;
            ++g_state.calibrationGeneration;
            g_state.currentEyeIndex = -1;
            g_state.currentEyePoseFrame = 0;
            g_state.baseMatricesValid = false;
            ResetStereoFillPhase();
            InvalidateRoomscaleSafetyCache();
            if (g_openxr != nullptr && g_config.hplControllerComfortBlackoutFrames > 0) {
                g_openxr->RequestComfortBlackout(
                    static_cast<uint32_t>(g_config.hplControllerComfortBlackoutFrames),
                    "recenter");
            }
            Logger::Instance().Write(
                LogLevel::Warn,
                "hpl_recenter applied key=F2 camera=%p frustum=%p poseFrame=%llu stablePoseFrames=%u stereo=%d roomscale=%d neutralPosition=%.6f,%.6f,%.6f neutralQuaternion=%.6f,%.6f,%.6f,%.6f",
                camera,
                frustum,
                static_cast<unsigned long long>(poseFrame),
                g_state.recenterPoseStability.consecutiveFrames,
                g_state.stereoEnabled ? 1 : 0,
                g_roomscaleEnabled.load(std::memory_order_relaxed) ? 1 : 0,
                position.x,
                position.y,
                position.z,
                g_state.neutralOrientation.x,
                g_state.neutralOrientation.y,
                g_state.neutralOrientation.z,
                g_state.neutralOrientation.w);
        } while (false);
    }

    if (g_state.stereoEnabled) {
        OpenXRStereoViewSnapshot views;
        const uint32_t eyeIndex = g_nextEyeIndex.load(std::memory_order_acquire);
        if (!PrepareStereoPairBase(frustum, parameters, eyeIndex, wasDirty)) {
            RestoreBaseView(frustum);
            g_state.currentEyeIndex = -1;
            g_state.currentEyePoseFrame = 0;
            ResetStereoFillPhase();
            if (g_openxr != nullptr) {
                g_openxr->InvalidateStereoCaches("camera_pair_base_rejected");
            }
            return frustum;
        }
        if (!ResolveStereoPairViews(eyeIndex, views)) {
            g_trackingFallbackFrames.fetch_add(1, std::memory_order_relaxed);
            if (!g_state.trackingFallbackActive) {
                g_state.trackingFallbackActive = true;
                Logger::Instance().Write(
                    LogLevel::Warn,
                    "hpl_stereo tracking_fallback active=1 eye=%u action=restore_base_view stereoIntent=preserved",
                    eyeIndex);
            }
            if (eyeIndex == 1) {
                ResetStereoFillPhase();
                if (g_openxr != nullptr) {
                    g_openxr->InvalidateStereoCaches("camera_pair_views_rejected");
                }
            }
            RestoreBaseView(frustum);
            return frustum;
        }
        if (g_state.trackingFallbackActive) {
            g_state.trackingFallbackActive = false;
            g_trackingRecoveryEvents.fetch_add(1, std::memory_order_relaxed);
            Logger::Instance().Write(
                LogLevel::Info,
                "hpl_stereo tracking_fallback active=0 action=resume_stereo poseFrame=%llu",
                static_cast<unsigned long long>(views.gameFrame));
        }
        if (ApplyStereoEye(frustum, views, eyeIndex, wasDirty)) {
            if (g_state.stereoApplyFailures != 0) {
                Logger::Instance().Write(
                    LogLevel::Info,
                    "hpl_stereo apply_recovered afterConsecutiveFailures=%u",
                    g_state.stereoApplyFailures);
                g_state.stereoApplyFailures = 0;
            }
            return frustum;
        }

        // A failed apply is a frame-level event, not a mode change. Clearing
        // stereoEnabled here would let one bad frame drop the player to mono
        // permanently, with the F11 toggle as the only way back - and it would
        // do it through the same four assignments as an explicit F11 disable,
        // so nothing downstream could tell a transient fault from a user
        // decision. Only an explicit transition owns the mode; a fault falls
        // back to mono for this frame and stereo is retried on the next one.
        // Corroborated by FEAR-VR, whose bridge clears its persistent
        // STEREO_ACTIVE bit on any non-stereo present: the derived FarCry2-VR
        // mod had to NOP that instruction to stop layer-type flapping.
        g_state.currentEyeIndex = -1;
        g_state.currentEyePoseFrame = 0;
        ResetStereoFillPhase();
        ++g_state.stereoApplyFailures;
        if (g_state.stereoApplyFailures < kStereoApplyFailureLimit) {
            if (g_state.stereoApplyFailures <= 4) {
                Logger::Instance().Write(
                    LogLevel::Warn,
                    "hpl_stereo apply_failed consecutive=%u limit=%u fallback=mono_orientation_this_frame",
                    g_state.stereoApplyFailures,
                    kStereoApplyFailureLimit);
            }
        } else {
            g_state.stereoEnabled = false;
            if (g_openxr != nullptr) {
                g_openxr->SetStereoSubmissionEnabled(false);
            }
            Logger::Instance().Write(
                LogLevel::Warn,
                "hpl_stereo suspended reason=projection_apply_failed consecutive=%u fallback=mono_orientation",
                g_state.stereoApplyFailures);
        }
    }

    if (wasDirty || !g_state.baseMatricesValid || g_state.activeFrustum != frustum) {
        if (!RefreshBaseMatrices(frustum, parameters)) {
            LogNativeMemoryWarning("refresh_base_matrices", frustum);
            return frustum;
        }
    }

    Quaternion currentOrientation;
    Vector3 currentPosition;
    uint64_t poseFrame = 0;
    if (!ReadHeadPose(currentOrientation, currentPosition, poseFrame)) {
        const uint64_t misses = g_poseMisses.fetch_add(1, std::memory_order_relaxed) + 1;
        if (misses <= 4) {
            Logger::Instance().Write(
                LogLevel::Warn,
                "hpl_head_tracking pose_unavailable miss=%llu camera=%p",
                static_cast<unsigned long long>(misses),
                camera);
        }
        RestoreBaseView(frustum);
        return frustum;
    }

    const Quaternion headViewRotation = Normalize(Multiply(
        Conjugate(currentOrientation),
        g_state.neutralOrientation));
    const std::array<float, 16> modifiedView = MatrixMultiply(
        RotationMatrix(headViewRotation),
        g_state.baseView);
    SetupWithView(frustum, modifiedView);

    const uint64_t applied = g_appliedCalls.fetch_add(1, std::memory_order_relaxed) + 1;
    const int logInterval = std::max(g_config.hplCameraLogInterval, 1);
    if (applied == 1 || applied % static_cast<uint64_t>(logInterval) == 0) {
        const float angleDegrees = 2.0f * std::acos(std::clamp(std::abs(headViewRotation.w), 0.0f, 1.0f))
            * kRadiansToDegrees;
        Logger::Instance().Write(
            LogLevel::Info,
            "hpl_head_tracking applied=%llu poseFrame=%llu camera=%p frustum=%p dirtyBefore=%d deltaAngleDeg=%.4f viewQuaternion=%.6f,%.6f,%.6f,%.6f viewRow0=%.6f,%.6f,%.6f,%.6f",
            static_cast<unsigned long long>(applied),
            static_cast<unsigned long long>(poseFrame),
            camera,
            frustum,
            wasDirty ? 1 : 0,
            angleDegrees,
            headViewRotation.x,
            headViewRotation.y,
            headViewRotation.z,
            headViewRotation.w,
            modifiedView[0],
            modifiedView[1],
            modifiedView[2],
            modifiedView[3]);
    }

    return frustum;
}

} // namespace

bool InstallHPLCameraBridge(const Config& config, OpenXRRuntime* openxr)
{
    if (!config.hplCameraBridge) {
        Logger::Instance().Write(LogLevel::Info, "hpl_camera_bridge install_skipped enabled=0");
        return true;
    }

    HMODULE executable = GetModuleHandleW(nullptr);
    if (!IsInsideImage(executable, kCameraGetFrustumRva, 32)
        || !IsInsideImage(executable, kSetupPerspectiveFrustumRva, 32)) {
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_camera_bridge install_failed reason=rva_outside_image cameraRva=0x%llx setupRva=0x%llx",
            static_cast<unsigned long long>(kCameraGetFrustumRva),
            static_cast<unsigned long long>(kSetupPerspectiveFrustumRva));
        return false;
    }

    auto* base = reinterpret_cast<std::byte*>(executable);
    void* cameraTarget = base + kCameraGetFrustumRva;
    void* setupTarget = base + kSetupPerspectiveFrustumRva;
    void* lineOfSightTarget = IsInsideImage(executable, kCheckLineOfSightRva, 32)
        ? base + kCheckLineOfSightRva
        : nullptr;
    static constexpr uint8_t kCameraSignature[] = {
        0x40, 0x55, 0x56, 0x41, 0x54, 0x48, 0x8d, 0x6c, 0x24,
        0x90, 0x48, 0x81, 0xec, 0x70, 0x01, 0x00, 0x00,
    };
    static constexpr uint8_t kSetupSignature[] = {
        0x48, 0x83, 0xec, 0x48, 0xf3, 0x0f, 0x10, 0x44, 0x24,
        0x78, 0x0f, 0xb6, 0x84, 0x24, 0xa0, 0x00, 0x00, 0x00,
    };
    static constexpr uint8_t kCheckLineOfSightSignature[] = {
        0x48, 0x83, 0xec, 0x38,
        0x48, 0xc7, 0x44, 0x24, 0x28, 0x00, 0x00, 0x00, 0x00,
        0x44, 0x88, 0x4c, 0x24, 0x20,
        0x45, 0x0f, 0xb6, 0xc8,
        0x4c, 0x8b, 0xc2,
        0x48, 0x8b, 0xd1,
    };
    if (!MatchBytes(cameraTarget, kCameraSignature, sizeof(kCameraSignature))
        || !MatchBytes(setupTarget, kSetupSignature, sizeof(kSetupSignature))) {
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_camera_bridge install_failed reason=signature_mismatch exe=%s cameraRva=0x%llx setupRva=0x%llx",
            ModulePath(executable).c_str(),
            static_cast<unsigned long long>(kCameraGetFrustumRva),
            static_cast<unsigned long long>(kSetupPerspectiveFrustumRva));
        return false;
    }

    g_config = config;
    const bool lineOfSightAvailable = MatchBytes(
        lineOfSightTarget,
        kCheckLineOfSightSignature,
        sizeof(kCheckLineOfSightSignature));
    if (g_config.hplRoomscaleSafety && !lineOfSightAvailable) {
        g_config.hplRoomscaleSafety = false;
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_roomscale_safety disabled reason=line_of_sight_signature_mismatch rva=0x%llx",
            static_cast<unsigned long long>(kCheckLineOfSightRva));
    }
    g_projectionCentered.store(config.hplProjectionCenteredDefault, std::memory_order_relaxed);
    g_roomscaleEnabled.store(config.hplRoomscaleEnabledDefault, std::memory_order_relaxed);
    if (g_config.hplStereoAfr && !ValidateStereoProjectionMath()) {
        g_config.hplStereoAfr = false;
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_stereo disabled reason=projection_self_test_failed");
    }
    g_openxr = openxr;
    g_setupPerspectiveFrustum = reinterpret_cast<SetupPerspectiveFrustumFn>(setupTarget);
    g_checkLineOfSight = lineOfSightAvailable
        ? reinterpret_cast<CheckLineOfSightFn>(lineOfSightTarget)
        : nullptr;
    g_cameraGetFrustumTarget = cameraTarget;
    g_executableBase = reinterpret_cast<uintptr_t>(executable);
    {
        std::lock_guard lock(g_stateMutex);
        g_state = BridgeState{};
        ResetStereoFillPhase();
    }
    g_getFrustumCalls.store(0, std::memory_order_relaxed);
    g_candidateCalls.store(0, std::memory_order_relaxed);
    g_appliedCalls.store(0, std::memory_order_relaxed);
    g_baseRefreshes.store(0, std::memory_order_relaxed);
    g_poseMisses.store(0, std::memory_order_relaxed);
    g_stereoAppliedCalls.store(0, std::memory_order_relaxed);
    g_stereoEyeCalls[0].store(0, std::memory_order_relaxed);
    g_stereoEyeCalls[1].store(0, std::memory_order_relaxed);
    g_stereoFillCommits.store(0, std::memory_order_relaxed);
    g_stereoFillRejects.store(0, std::memory_order_relaxed);
    g_pairRotationLatches.store(0, std::memory_order_relaxed);
    g_pairRotationReuses.store(0, std::memory_order_relaxed);
    g_pairBaseLatches.store(0, std::memory_order_relaxed);
    g_pairBaseReplays.store(0, std::memory_order_relaxed);
    g_pairBaseMissingRejects.store(0, std::memory_order_relaxed);
    g_pairBaseStaleRejects.store(0, std::memory_order_relaxed);
    g_pairBaseFrustumRejects.store(0, std::memory_order_relaxed);
    g_pairViewLatches.store(0, std::memory_order_relaxed);
    g_pairViewReplays.store(0, std::memory_order_relaxed);
    g_pairViewRejects.store(0, std::memory_order_relaxed);
    g_nativeRollObservedCalls.store(0, std::memory_order_relaxed);
    g_nativeRollSuppressedCalls.store(0, std::memory_order_relaxed);
    g_nativePitchObservedCalls.store(0, std::memory_order_relaxed);
    g_nativePitchSuppressedCalls.store(0, std::memory_order_relaxed);
    g_nativeMemoryReadFailures.store(0, std::memory_order_relaxed);
    g_nativeMemoryWriteFailures.store(0, std::memory_order_relaxed);
    g_nativeMemoryWarningLogs.store(0, std::memory_order_relaxed);
    g_roomscaleSafetySamples.store(0, std::memory_order_relaxed);
    g_roomscaleSafetyQueries.store(0, std::memory_order_relaxed);
    g_roomscaleSafetyProbes.store(0, std::memory_order_relaxed);
    g_roomscaleSafetySkippedProbes.store(0, std::memory_order_relaxed);
    g_roomscaleSafetyBlocked.store(0, std::memory_order_relaxed);
    g_roomscaleSafetyClamped.store(0, std::memory_order_relaxed);
    g_roomscaleSafetyFallbacks.store(0, std::memory_order_relaxed);

    MH_STATUS status = MH_CreateHook(
        cameraTarget,
        reinterpret_cast<void*>(&HookCameraGetFrustum),
        reinterpret_cast<void**>(&g_originalCameraGetFrustum));
    if (status != MH_OK) {
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_camera_bridge install_failed reason=create_hook status=%s",
            MH_StatusToString(status));
        return false;
    }

    status = MH_EnableHook(cameraTarget);
    if (status != MH_OK && status != MH_ERROR_ENABLED) {
        MH_RemoveHook(cameraTarget);
        g_originalCameraGetFrustum = nullptr;
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_camera_bridge install_failed reason=enable_hook status=%s",
            MH_StatusToString(status));
        return false;
    }

    Logger::Instance().Write(
        LogLevel::Warn,
        "hpl_camera_bridge install_ok exe=%s base=%p cameraGetFrustumRva=0x%llx setupPerspectiveRva=0x%llx lineOfSightRva=0x%llx renderViewportReturnRva=0x%llx headKey=F10 stereoKey=F11 recenterKey=F2 projectionKey=F5 roomscaleKey=F4 recenterControl=%d stereoAfr=%d projectionCenteredDefault=%d roomscaleDefault=%d verticalRoomscale=%d roomscaleSafety=%d roomscaleSafetyDynamic=%d roomscaleBodyReconciliation=%d roomscaleClearanceMeters=%.3f roomscaleRadiusMeters=%.3f roomscaleVerticalRadiusMeters=%.3f roomscaleRadialSamples=%d roomscaleIterations=%d roomscaleStaticOnly=%d eyeHeightOffsetMeters=%.4f nativePitchSuppression=%d nativePitchOffsets=0x%zx,0x%zx nativeRollSuppression=%d nativeRollOffsets=0x%zx,0x%zx worldScale=%.4f activationStableFrames=%u activationMaxPositionStep=%.3f activationMaxOrientationStepDeg=%.1f logInterval=%d",
        ModulePath(executable).c_str(),
        executable,
        static_cast<unsigned long long>(kCameraGetFrustumRva),
        static_cast<unsigned long long>(kSetupPerspectiveFrustumRva),
        static_cast<unsigned long long>(kCheckLineOfSightRva),
        static_cast<unsigned long long>(kRenderViewportGetFrustumReturnRva),
        g_config.hplRecenterControl ? 1 : 0,
        g_config.hplStereoAfr ? 1 : 0,
        g_projectionCentered.load(std::memory_order_relaxed) ? 1 : 0,
        g_roomscaleEnabled.load(std::memory_order_relaxed) ? 1 : 0,
        g_config.hplRoomscaleVertical ? 1 : 0,
        g_config.hplRoomscaleSafety ? 1 : 0,
        g_config.hplRoomscaleSafetyDynamic ? 1 : 0,
        g_config.hplRoomscaleBodyReconciliation ? 1 : 0,
        g_config.hplRoomscaleSafetyClearanceMeters,
        g_config.hplRoomscaleSafetyRadiusMeters,
        g_config.hplRoomscaleSafetyVerticalRadiusMeters,
        g_config.hplRoomscaleSafetyRadialSamples,
        g_config.hplRoomscaleSafetyIterations,
        g_config.hplRoomscaleSafetyDynamic ? 0 : 1,
        g_config.hplEyeHeightOffsetMeters,
        g_config.hplNativeCameraPitchSuppression ? 1 : 0,
        kCameraBasePitchOffset,
        kCameraSecondaryRotationXOffset,
        g_config.hplNativeCameraRollSuppression ? 1 : 0,
        kCameraBaseRollOffset,
        kCameraSecondaryRotationZOffset,
        g_config.hplWorldScale,
        kActivationStablePoseFrames,
        kActivationMaxPositionStepMeters,
        kActivationMaxOrientationStepRadians * kRadiansToDegrees,
        g_config.hplCameraLogInterval);
    return true;
}

void LogHPLCameraBridgeSummary()
{
    std::lock_guard lock(g_stateMutex);
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_camera_bridge summary getFrustumCalls=%llu candidateCalls=%llu secondaryCameraCandidates=%llu secondaryCameraControlSkips=%llu authoredCameraOwnershipChanges=%llu activationPending=%d recenterPending=%d trackingEnabled=%d stereoEnabled=%d trackingFallbackActive=%d activeCamera=%p activeFrustum=%p appliedCalls=%llu stereoApplied=%llu leftApplied=%llu rightApplied=%llu fillCommits=%llu fillRejects=%llu pairRotationLatches=%llu pairRotationReuses=%llu pairBaseLatches=%llu pairBaseReplays=%llu pairBaseMissingRejects=%llu pairBaseStaleRejects=%llu pairBaseFrustumRejects=%llu pairBaseMaxAgeMs=%llu pairViewLatches=%llu pairViewReplays=%llu pairViewRejects=%llu baseRefreshes=%llu poseMisses=%llu trackingFallbackFrames=%llu trackingRecoveryEvents=%llu nativePitchObserved=%llu nativePitchSuppressed=%llu nativeRollObserved=%llu nativeRollSuppressed=%llu nativeMemoryReadFailures=%llu nativeMemoryWriteFailures=%llu roomscaleSafetySamples=%llu roomscaleSafetyQueries=%llu roomscaleSafetyProbes=%llu roomscaleSafetySkippedProbes=%llu roomscaleSafetyBlocked=%llu roomscaleSafetyClamped=%llu roomscaleSafetyFallbacks=%llu roomscaleBodyShiftProbes=%llu roomscaleBodyShiftBlocks=%llu roomscaleBodyShiftCommits=%llu",
        static_cast<unsigned long long>(g_getFrustumCalls.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_candidateCalls.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_secondaryCameraCandidates.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_secondaryCameraControlSkips.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_authoredCameraOwnershipChanges.load(std::memory_order_relaxed)),
        g_state.activationPending ? 1 : 0,
        g_state.recenterPending ? 1 : 0,
        g_state.trackingEnabled ? 1 : 0,
        g_state.stereoEnabled ? 1 : 0,
        g_state.trackingFallbackActive ? 1 : 0,
        g_state.activeCamera,
        g_state.activeFrustum,
        static_cast<unsigned long long>(g_appliedCalls.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_stereoAppliedCalls.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_stereoEyeCalls[0].load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_stereoEyeCalls[1].load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_stereoFillCommits.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_stereoFillRejects.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_pairRotationLatches.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_pairRotationReuses.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_pairBaseLatches.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_pairBaseReplays.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_pairBaseMissingRejects.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_pairBaseStaleRejects.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_pairBaseFrustumRejects.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(kStereoPairBaseMaxAgeMilliseconds),
        static_cast<unsigned long long>(g_pairViewLatches.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_pairViewReplays.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_pairViewRejects.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_baseRefreshes.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_poseMisses.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_trackingFallbackFrames.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_trackingRecoveryEvents.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_nativePitchObservedCalls.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_nativePitchSuppressedCalls.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_nativeRollObservedCalls.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_nativeRollSuppressedCalls.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_nativeMemoryReadFailures.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_nativeMemoryWriteFailures.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_roomscaleSafetySamples.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_roomscaleSafetyQueries.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_roomscaleSafetyProbes.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_roomscaleSafetySkippedProbes.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_roomscaleSafetyBlocked.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_roomscaleSafetyClamped.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_roomscaleSafetyFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_roomscaleBodyShiftProbes.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_roomscaleBodyShiftBlocks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_roomscaleBodyShiftCommits.load(std::memory_order_relaxed)));
}

HPLCameraBridgeStatus GetHPLCameraBridgeStatus()
{
    std::lock_guard lock(g_stateMutex);
    HPLCameraBridgeStatus status;
    status.installed = g_cameraGetFrustumTarget != nullptr;
    status.trackingEnabled = g_state.trackingEnabled;
    status.stereoEnabled = g_state.stereoEnabled;
    status.projectionCentered = g_projectionCentered.load(std::memory_order_relaxed);
    status.roomscaleEnabled = g_roomscaleEnabled.load(std::memory_order_relaxed);
    status.stereoRenderEye = g_state.currentEyeIndex;
    status.stereoRenderPoseFrame = g_state.currentEyePoseFrame;
    status.activeCamera = g_state.activeCamera;
    status.activeFrustum = g_state.activeFrustum;
    status.calibrationGeneration = g_state.calibrationGeneration;
    status.worldUnitsPerMeter = std::max(g_config.hplWorldScale, 0.001f);
    if (g_state.baseMatricesValid
        && std::isfinite(g_state.parameters.nearPlane)
        && std::isfinite(g_state.parameters.farPlane)) {
        status.projectionParametersValid = true;
        status.nearPlane = g_state.parameters.nearPlane;
        status.farPlane = g_state.parameters.farPlane;
        status.projectionType = g_state.parameters.projectionType;
    }
    if (g_state.baseMatricesValid
        && std::isfinite(g_state.parameters.origin[0])
        && std::isfinite(g_state.parameters.origin[1])
        && std::isfinite(g_state.parameters.origin[2])) {
        status.cameraWorldPositionValid = true;
        status.cameraWorldPositionX = g_state.parameters.origin[0];
        status.cameraWorldPositionY = g_state.parameters.origin[1];
        status.cameraWorldPositionZ = g_state.parameters.origin[2];
        const Vector3 nativeForward = TransformLocalDirectionToWorld(
            {0.0f, 0.0f, -1.0f}, g_state.baseView);
        const Vector3 nativeUp = TransformLocalDirectionToWorld(
            {0.0f, 1.0f, 0.0f}, g_state.baseView);
        if (IsFinite(nativeForward) && IsFinite(nativeUp)) {
            status.nativeCameraBasisValid = true;
            status.nativeCameraForwardX = nativeForward.x;
            status.nativeCameraForwardY = nativeForward.y;
            status.nativeCameraForwardZ = nativeForward.z;
            status.nativeCameraUpX = nativeUp.x;
            status.nativeCameraUpY = nativeUp.y;
            status.nativeCameraUpZ = nativeUp.z;
        }
    }
    if (g_state.trackingEnabled) {
        Quaternion currentOrientation;
        Vector3 currentPosition;
        uint64_t poseFrame = 0;
        if (ReadHeadPose(currentOrientation, currentPosition, poseFrame)) {
            const Quaternion headViewRotation = Normalize(Multiply(
                Conjugate(currentOrientation),
                g_state.neutralOrientation));
            const Quaternion headWorldRotation = Conjugate(headViewRotation);
            status.headWorldRotationValid = true;
            status.headPoseFrame = poseFrame;
            status.headWorldRotationX = headWorldRotation.x;
            status.headWorldRotationY = headWorldRotation.y;
            status.headWorldRotationZ = headWorldRotation.z;
            status.headWorldRotationW = headWorldRotation.w;

            if (g_state.baseMatricesValid) {
                const Vector3 localOffset = ResolveSafeTrackedOffset(
                    currentPosition,
                    currentPosition,
                    poseFrame,
                    g_config.hplEyeHeightOffsetMeters);
                const Vector3 worldOffset = TransformLocalOffsetToWorld(localOffset, g_state.baseView);
                if (std::isfinite(worldOffset.x)
                    && std::isfinite(worldOffset.y)
                    && std::isfinite(worldOffset.z)) {
                    status.headWorldPositionValid = true;
                    status.headWorldOffsetX = worldOffset.x;
                    status.headWorldOffsetY = worldOffset.y;
                    status.headWorldOffsetZ = worldOffset.z;
                    status.headWorldPositionX = g_state.parameters.origin[0] + worldOffset.x;
                    status.headWorldPositionY = g_state.parameters.origin[1] + worldOffset.y;
                    status.headWorldPositionZ = g_state.parameters.origin[2] + worldOffset.z;
                }
            }
        }
    }
    status.roomscaleSafetyQueried = g_state.roomscaleSafetyResult.queried;
    status.roomscaleSafetyClamped = g_state.roomscaleSafetyResult.clamped;
    return status;
}

bool GetHPLPendingStereoRenderTarget(HPLPendingStereoRenderTarget& target)
{
    std::lock_guard lock(g_stateMutex);
    target = {};
    const uint32_t nextEyeIndex = g_nextEyeIndex.load(std::memory_order_acquire);
    if (!g_state.trackingEnabled || !g_state.stereoEnabled || nextEyeIndex > 1) {
        return false;
    }

    OpenXRStereoViewSnapshot views;
    bool viewsReady = false;
    if (nextEyeIndex == 1 && g_state.pairViewsValid) {
        views = g_state.pairViews;
        viewsReady = views.valid && views.gameFrame != 0;
    } else {
        viewsReady = ReadStereoViews(views) && views.gameFrame != 0;
    }
    if (!viewsReady) {
        return false;
    }

    target.eyeIndex = static_cast<int>(nextEyeIndex);
    target.poseFrame = views.gameFrame;
    target.calibrationGeneration = g_state.calibrationGeneration;
    return true;
}

bool CommitHPLStereoEyeFill(uint32_t eyeIndex, uint64_t poseFrame, const char* source)
{
    const int committableEye = g_committableEye.load(std::memory_order_acquire);
    const uint64_t committablePoseFrame =
        g_committableEyePoseFrame.load(std::memory_order_acquire);
    uint32_t expectedEye = eyeIndex;
    const bool committed = eyeIndex < 2
        && committableEye == static_cast<int>(eyeIndex)
        && committablePoseFrame == poseFrame
        && g_nextEyeIndex.compare_exchange_strong(
            expectedEye, eyeIndex ^ 1u, std::memory_order_acq_rel);
    if (committed) {
        g_committableEye.store(-1, std::memory_order_release);
        const uint64_t count = g_stereoFillCommits.fetch_add(1, std::memory_order_relaxed) + 1;
        if (count <= 4 || count % 120 == 0) {
            Logger::Instance().Write(
                LogLevel::Info,
                "hpl_stereo fill_committed count=%llu eye=%u nextEye=%u poseFrame=%llu source=%s policy=advance_only_after_cache_fill",
                static_cast<unsigned long long>(count),
                eyeIndex,
                eyeIndex ^ 1u,
                static_cast<unsigned long long>(poseFrame),
                source != nullptr ? source : "unspecified");
        }
        return true;
    }

    const uint64_t rejects = g_stereoFillRejects.fetch_add(1, std::memory_order_relaxed) + 1;
    if (rejects <= 8 || rejects % 120 == 0) {
        Logger::Instance().Write(
            LogLevel::Warn,
            "hpl_stereo fill_rejected count=%llu eye=%u expectedEye=%u committableEye=%d poseFrame=%llu committablePoseFrame=%llu source=%s action=retain_eye_phase",
            static_cast<unsigned long long>(rejects),
            eyeIndex,
            expectedEye,
            committableEye,
            static_cast<unsigned long long>(poseFrame),
            static_cast<unsigned long long>(committablePoseFrame),
            source != nullptr ? source : "unspecified");
    }
    return false;
}

bool ResolveHPLTrackedPoseWorld(
    const OpenXRControllerPose& pose,
    uint64_t gameFrame,
    HPLTrackedPoseWorld& worldPose)
{
    std::lock_guard lock(g_stateMutex);
    worldPose = {};
    if (!g_state.trackingEnabled
        || !g_state.baseMatricesValid
        || g_state.activeCamera == nullptr
        || !pose.valid) {
        return false;
    }

    Quaternion headOrientation;
    Vector3 headPosition;
    uint64_t headFrame = 0;
    if (!ReadHeadPose(headOrientation, headPosition, headFrame)) {
        return false;
    }

    const Vector3 localOffset = ResolveSafeTrackedOffset(
        {pose.positionX, pose.positionY, pose.positionZ},
        headPosition,
        gameFrame != 0 ? gameFrame : headFrame,
        g_config.hplEyeHeightOffsetMeters);
    const Vector3 worldOffset = TransformLocalOffsetToWorld(localOffset, g_state.baseView);

    const Quaternion controllerOrientation = Normalize({
        pose.orientationX,
        pose.orientationY,
        pose.orientationZ,
        pose.orientationW,
    });
    const Quaternion controllerRelative = Normalize(Multiply(
        Conjugate(g_state.neutralOrientation),
        controllerOrientation));
    const Vector3 worldForward = TransformLocalDirectionToWorld(
        RotateVector(controllerRelative, {0.0f, 0.0f, -1.0f}),
        g_state.baseView);
    const Vector3 worldUp = TransformLocalDirectionToWorld(
        RotateVector(controllerRelative, {0.0f, 1.0f, 0.0f}),
        g_state.baseView);
    const bool finite = std::isfinite(worldOffset.x)
        && std::isfinite(worldOffset.y)
        && std::isfinite(worldOffset.z)
        && std::isfinite(worldForward.x)
        && std::isfinite(worldForward.y)
        && std::isfinite(worldForward.z)
        && std::isfinite(worldUp.x)
        && std::isfinite(worldUp.y)
        && std::isfinite(worldUp.z);
    if (!finite) {
        return false;
    }

    worldPose.valid = true;
    worldPose.orientationTracked = pose.orientationTracked;
    worldPose.positionTracked = pose.positionTracked;
    worldPose.gameFrame = gameFrame != 0 ? gameFrame : headFrame;
    worldPose.positionX = g_state.parameters.origin[0] + worldOffset.x;
    worldPose.positionY = g_state.parameters.origin[1] + worldOffset.y;
    worldPose.positionZ = g_state.parameters.origin[2] + worldOffset.z;
    worldPose.forwardX = worldForward.x;
    worldPose.forwardY = worldForward.y;
    worldPose.forwardZ = worldForward.z;
    worldPose.upX = worldUp.x;
    worldPose.upY = worldUp.y;
    worldPose.upZ = worldUp.z;
    return true;
}

bool ResolveHPLReferenceVectorWorld(
    float x,
    float y,
    float z,
    bool applyWorldScale,
    float& worldX,
    float& worldY,
    float& worldZ)
{
    std::lock_guard lock(g_stateMutex);
    if (!g_state.trackingEnabled || !g_state.baseMatricesValid
        || !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) {
        return false;
    }

    Vector3 local = RotateVector(Conjugate(g_state.neutralOrientation), {x, y, z});
    if (applyWorldScale) {
        const float scale = std::max(g_config.hplWorldScale, 0.001f);
        local.x *= scale;
        local.y *= scale;
        local.z *= scale;
    }
    const Vector3 world = TransformLocalOffsetToWorld(local, g_state.baseView);
    if (!std::isfinite(world.x) || !std::isfinite(world.y) || !std::isfinite(world.z)) {
        return false;
    }
    worldX = world.x;
    worldY = world.y;
    worldZ = world.z;
    return true;
}

bool ValidateHPLRoomscaleBodyShift(
    float feetX,
    float feetY,
    float feetZ,
    float sizeX,
    float sizeY,
    float sizeZ,
    float shiftX,
    float shiftZ,
    uint32_t& probeCount)
{
    std::lock_guard lock(g_stateMutex);
    probeCount = 0;
    if (!g_config.hplRoomscaleBodyReconciliation
        || !g_config.hplRoomscaleSafety
        || !g_state.trackingEnabled
        || !g_state.baseMatricesValid
        || !g_roomscaleEnabled.load(std::memory_order_relaxed)
        || g_checkLineOfSight == nullptr) {
        return false;
    }

    const std::array<float, 8> values = {
        feetX, feetY, feetZ, sizeX, sizeY, sizeZ, shiftX, shiftZ,
    };
    if (!std::all_of(values.begin(), values.end(), [](float value) { return std::isfinite(value); })
        || sizeX <= 0.0f || sizeY <= 0.0f || sizeZ <= 0.0f
        || std::fabs(shiftX) + std::fabs(shiftZ) <= 1.0e-6f) {
        return false;
    }

    const float worldScale = std::max(g_config.hplWorldScale, 0.001f);
    const float radius = std::max(sizeX, sizeZ) * 0.45f;
    const float verticalInset = std::min(
        std::max(radius, 0.05f * worldScale),
        sizeY * 0.25f);
    const std::array<float, 3> heights = {
        feetY + verticalInset,
        feetY + sizeY * 0.5f,
        feetY + sizeY - verticalInset,
    };
    const int radialSamples = std::clamp(g_config.hplRoomscaleSafetyRadialSamples, 4, 12);
    const bool staticOnly = !g_config.hplRoomscaleSafetyDynamic;
    constexpr float kTwoPi = 6.283185307179586f;
    const std::array<float, 3> worldShift = {shiftX, 0.0f, shiftZ};

    for (float height : heights) {
        for (int sample = -1; sample < radialSamples; ++sample) {
            float radialX = 0.0f;
            float radialZ = 0.0f;
            if (sample >= 0) {
                const float angle = kTwoPi * static_cast<float>(sample)
                    / static_cast<float>(radialSamples);
                radialX = std::cos(angle) * radius;
                radialZ = std::sin(angle) * radius;
            }
            const std::array<float, 3> start = {
                feetX + radialX,
                height,
                feetZ + radialZ,
            };
            const std::array<float, 3> end = {
                start[0] + worldShift[0],
                start[1],
                start[2] + worldShift[2],
            };
            ++probeCount;
            g_roomscaleBodyShiftProbes.fetch_add(1, std::memory_order_relaxed);
            if (!g_checkLineOfSight(start.data(), end.data(), false, staticOnly)) {
                g_roomscaleBodyShiftBlocks.fetch_add(1, std::memory_order_relaxed);
                return false;
            }
        }
    }
    return true;
}

bool CommitHPLRoomscaleBodyShift(float shiftX, float shiftZ)
{
    std::lock_guard lock(g_stateMutex);
    if (!g_config.hplRoomscaleBodyReconciliation
        || !g_state.trackingEnabled
        || !g_state.baseMatricesValid
        || !g_roomscaleEnabled.load(std::memory_order_relaxed)
        || !std::isfinite(shiftX) || !std::isfinite(shiftZ)) {
        return false;
    }

    const float worldScale = std::max(g_config.hplWorldScale, 0.001f);
    const Vector3 localShift = TransformWorldOffsetToLocal(
        {shiftX, 0.0f, shiftZ}, g_state.baseView);
    const Vector3 trackingShift = RotateVector(
        g_state.neutralOrientation,
        {localShift.x / worldScale, localShift.y / worldScale, localShift.z / worldScale});
    if (!IsFinite(trackingShift)) {
        return false;
    }

    g_state.neutralPosition.x += trackingShift.x;
    g_state.neutralPosition.y += trackingShift.y;
    g_state.neutralPosition.z += trackingShift.z;
    InvalidateRoomscaleSafetyCache();
    g_roomscaleBodyShiftCommits.fetch_add(1, std::memory_order_relaxed);
    return true;
}

bool RequestHPLRecenter(const char* source)
{
    std::lock_guard lock(g_stateMutex);
    if (!g_config.hplRecenterControl || !g_state.trackingEnabled) {
        return false;
    }
    g_state.recenterPending = true;
    g_state.recenterTrackingWaitLogs = 0;
    g_state.recenterPoseStability = PoseStabilityState{};
    InvalidateRoomscaleSafetyCache();
    Logger::Instance().Write(
        LogLevel::Warn,
        "hpl_recenter requested source=%s camera=%p frustum=%p stereo=%d roomscale=%d",
        source != nullptr ? source : "api",
        g_state.activeCamera,
        g_state.activeFrustum,
        g_state.stereoEnabled ? 1 : 0,
        g_roomscaleEnabled.load(std::memory_order_relaxed) ? 1 : 0);
    return true;
}

bool SetHPLRoomscaleEnabled(bool enabled, const char* source)
{
    std::lock_guard lock(g_stateMutex);
    if (!g_config.hplRoomscaleControl || g_cameraGetFrustumTarget == nullptr) return false;
    g_roomscaleEnabled.store(enabled, std::memory_order_relaxed);
    InvalidateRoomscaleSafetyCache();
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_roomscale enabled=%d source=%s policy=retain_ipd_and_orientation stereo=%d",
        enabled ? 1 : 0,
        source != nullptr ? source : "api",
        g_state.stereoEnabled ? 1 : 0);
    return true;
}

bool SetHPLProjectionCentered(bool enabled, const char* source)
{
    std::lock_guard lock(g_stateMutex);
    if (!g_config.hplProjectionCenterControl || g_cameraGetFrustumTarget == nullptr) return false;
    g_projectionCentered.store(enabled, std::memory_order_relaxed);
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_projection_center enabled=%d source=%s policy=fully_symmetric_fov stereo=%d",
        enabled ? 1 : 0,
        source != nullptr ? source : "api",
        g_state.stereoEnabled ? 1 : 0);
    return true;
}

void NotifyHPLPlayerCameraChanged(void* previousCamera, void* currentCamera)
{
    std::lock_guard lock(g_stateMutex);
    if (currentCamera == nullptr
        || (!g_state.trackingEnabled && !g_state.activationPending)
        || g_state.activeCamera == currentCamera) {
        return;
    }

    const bool wasStereo = g_state.stereoEnabled;
    if (g_openxr != nullptr) {
        g_openxr->SetStereoSubmissionEnabled(false);
        g_openxr->InvalidateStereoCaches("player_camera_changed");
    }
    g_state.activationPending = true;
    g_state.recenterPending = false;
    g_state.trackingEnabled = false;
    g_state.stereoEnabled = false;
    g_state.stereoApplyFailures = 0;
    g_state.baseMatricesValid = false;
    g_state.activeCamera = currentCamera;
    g_state.activeFrustum = nullptr;
    g_state.activationTrackingWaitLogs = 0;
    g_state.recenterTrackingWaitLogs = 0;
    g_state.activationPoseStability = PoseStabilityState{};
    g_state.recenterPoseStability = PoseStabilityState{};
    ResetStereoFillPhase();
    g_state.currentEyeIndex = -1;
    g_state.currentEyePoseFrame = 0;
    InvalidateRoomscaleSafetyCache();
    Logger::Instance().Write(
        LogLevel::Warn,
        "hpl_vr_mode camera_replaced previous=%p current=%p stereoWas=%d policy=cache_invalidate_and_rearm",
        previousCamera,
        currentCamera,
        wasStereo ? 1 : 0);
}

void NotifyHPLAuthoredCameraOwnershipChanged(void* camera, bool authoredCameraActive)
{
    std::lock_guard lock(g_stateMutex);
    if (camera == nullptr
        || camera != g_state.activeCamera
        || (!g_state.trackingEnabled && !g_state.activationPending)) {
        return;
    }

    if (g_openxr != nullptr) {
        g_openxr->InvalidateStereoCaches("authored_camera_ownership_changed");
    }
    g_state.baseMatricesValid = false;
    g_state.activeFrustum = nullptr;
    ResetStereoFillPhase();
    g_state.currentEyeIndex = -1;
    g_state.currentEyePoseFrame = 0;
    ++g_state.calibrationGeneration;
    InvalidateRoomscaleSafetyCache();
    const uint64_t change = g_authoredCameraOwnershipChanges.fetch_add(
        1, std::memory_order_relaxed) + 1;
    Logger::Instance().Write(
        LogLevel::Warn,
        "hpl_vr_mode authored_camera_ownership_changed change=%llu camera=%p authoredCamera=%d tracking=%d stereo=%d calibrationGeneration=%llu policy=preserve_vr_refresh_native_base_reset_temporal_histories",
        static_cast<unsigned long long>(change),
        camera,
        authoredCameraActive ? 1 : 0,
        g_state.trackingEnabled ? 1 : 0,
        g_state.stereoEnabled ? 1 : 0,
        static_cast<unsigned long long>(g_state.calibrationGeneration));
}

void RemoveHPLCameraBridge()
{
    std::lock_guard lock(g_stateMutex);
    if (g_openxr != nullptr) {
        g_openxr->SetStereoSubmissionEnabled(false);
    }
    if (g_cameraGetFrustumTarget != nullptr) {
        MH_DisableHook(g_cameraGetFrustumTarget);
        MH_RemoveHook(g_cameraGetFrustumTarget);
    }
    g_cameraGetFrustumTarget = nullptr;
    g_executableBase = 0;
    g_originalCameraGetFrustum = nullptr;
    g_setupPerspectiveFrustum = nullptr;
    g_checkLineOfSight = nullptr;
    g_openxr = nullptr;
    g_state = BridgeState{};
    ResetStereoFillPhase();
    g_projectionCentered.store(false, std::memory_order_relaxed);
    g_projectionCenterF5Down.store(false, std::memory_order_relaxed);
    g_roomscaleEnabled.store(true, std::memory_order_relaxed);
    g_roomscaleF4Down.store(false, std::memory_order_relaxed);
    Logger::Instance().Write(LogLevel::Info, "hpl_camera_bridge removed");
}

} // namespace somavr
