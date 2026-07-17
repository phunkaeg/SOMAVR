#pragma once

#include "OpenXRRuntime.h"

#include <array>
#include <cstdint>

namespace somavr::camera_math {

struct Quaternion {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 1.0f;
};

struct Vector3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

constexpr size_t kMaxRoomscaleSafetySamples = 19;

struct PoseStabilityState {
    bool valid = false;
    uint64_t gameFrame = 0;
    uint32_t consecutiveFrames = 0;
    Quaternion orientation{};
    Vector3 position{};
};

enum class PoseStabilityUpdate {
    Invalid,
    DuplicateFrame,
    Started,
    Accumulating,
    Reset,
    Ready,
};

Quaternion Normalize(Quaternion value);
Quaternion Conjugate(const Quaternion& value);
Quaternion Multiply(const Quaternion& left, const Quaternion& right);
Vector3 RotateVector(const Quaternion& input, const Vector3& value);
bool QuaternionFromRotationMatrix(
    const std::array<float, 16>& matrix,
    Quaternion& output);
bool QuaternionFromForwardUp(
    const Vector3& forward,
    const Vector3& up,
    Quaternion& output);

std::array<float, 16> RotationMatrix(const Quaternion& input);
std::array<float, 16> MatrixMultiply(
    const std::array<float, 16>& left,
    const std::array<float, 16>& right);
std::array<float, 16> TranslationMatrix(const Vector3& translation);
Vector3 ResolveTrackedEyeOffset(
    const Vector3& eyePosition,
    const Vector3& headCenter,
    const Vector3& neutralPosition,
    const Quaternion& neutralOrientation,
    bool roomscaleEnabled,
    bool verticalRoomscale,
    float worldScale,
    float eyeHeightOffsetMeters);

float ComputeRoomscaleSafetyFactor(
    float unobstructedFraction,
    float translationDistance,
    float clearanceDistance);

Vector3 ReplaceTrackedHeadTranslation(
    const Vector3& rawTrackedOffset,
    const Vector3& rawHeadTranslation,
    const Vector3& safeHeadTranslation);

size_t BuildRoomscaleSafetySampleOffsets(
    float horizontalRadius,
    float verticalRadius,
    int radialSamples,
    std::array<Vector3, kMaxRoomscaleSafetySamples>& offsets);

PoseStabilityUpdate UpdatePoseStability(
    PoseStabilityState& state,
    uint64_t gameFrame,
    const Quaternion& orientation,
    const Vector3& position,
    uint32_t requiredConsecutiveFrames,
    float maxPositionStep,
    float maxOrientationStepRadians,
    float& positionStep,
    float& orientationStepRadians);

OpenXREyeView CenterProjectionFov(const OpenXREyeView& eye);
bool BuildOpenXRProjection(
    const OpenXREyeView& eye,
    float nearPlane,
    float farPlane,
    std::array<float, 16>& projection,
    float& verticalFov,
    float& aspect);
bool ValidateStereoProjectionMath();

} // namespace somavr::camera_math
