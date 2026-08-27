#include "HPLCameraMath.h"

#include <algorithm>
#include <cmath>

namespace somavr::camera_math {

Quaternion Normalize(Quaternion value)
{
    const float lengthSquared = value.x * value.x + value.y * value.y
        + value.z * value.z + value.w * value.w;
    if (!std::isfinite(lengthSquared) || lengthSquared < 1.0e-8f) {
        return {};
    }

    const float inverseLength = 1.0f / std::sqrt(lengthSquared);
    value.x *= inverseLength;
    value.y *= inverseLength;
    value.z *= inverseLength;
    value.w *= inverseLength;
    return value;
}

Quaternion Conjugate(const Quaternion& value)
{
    return {-value.x, -value.y, -value.z, value.w};
}

Quaternion Multiply(const Quaternion& left, const Quaternion& right)
{
    return {
        left.w * right.x + left.x * right.w + left.y * right.z - left.z * right.y,
        left.w * right.y - left.x * right.z + left.y * right.w + left.z * right.x,
        left.w * right.z + left.x * right.y - left.y * right.x + left.z * right.w,
        left.w * right.w - left.x * right.x - left.y * right.y - left.z * right.z,
    };
}

Quaternion YawOnly(const Quaternion& value)
{
    const Quaternion normalized = Normalize(value);
    const Vector3 forward = RotateVector(normalized, {0.0f, 0.0f, -1.0f});
    const float horizontalLengthSquared = forward.x * forward.x + forward.z * forward.z;
    if (!std::isfinite(horizontalLengthSquared) || horizontalLengthSquared < 1.0e-8f) {
        return {};
    }

    const float yaw = std::atan2(-forward.x, -forward.z);
    const float halfYaw = yaw * 0.5f;
    return {0.0f, std::sin(halfYaw), 0.0f, std::cos(halfYaw)};
}

Vector3 RotateVector(const Quaternion& input, const Vector3& value)
{
    const Quaternion q = Normalize(input);
    const Quaternion pure{value.x, value.y, value.z, 0.0f};
    const Quaternion rotated = Multiply(Multiply(q, pure), Conjugate(q));
    return {rotated.x, rotated.y, rotated.z};
}

bool QuaternionFromRotationMatrix(
    const std::array<float, 16>& matrix,
    Quaternion& output)
{
    std::array<float, 16> rotation = matrix;
    for (size_t column = 0; column < 3; ++column) {
        const float lengthSquared = matrix[column] * matrix[column]
            + matrix[column + 4] * matrix[column + 4]
            + matrix[column + 8] * matrix[column + 8];
        if (!std::isfinite(lengthSquared) || lengthSquared < 1.0e-8f) return false;
        const float inverseLength = 1.0f / std::sqrt(lengthSquared);
        rotation[column] *= inverseLength;
        rotation[column + 4] *= inverseLength;
        rotation[column + 8] *= inverseLength;
    }

    const Vector3 right{rotation[0], rotation[4], rotation[8]};
    const Vector3 up{rotation[1], rotation[5], rotation[9]};
    const Vector3 backward{rotation[2], rotation[6], rotation[10]};
    const auto dot = [](const Vector3& left, const Vector3& rightValue) {
        return left.x * rightValue.x + left.y * rightValue.y + left.z * rightValue.z;
    };
    const Vector3 rightCrossUp{
        right.y * up.z - right.z * up.y,
        right.z * up.x - right.x * up.z,
        right.x * up.y - right.y * up.x,
    };
    constexpr float kBasisTolerance = 0.02f;
    const float determinant = dot(rightCrossUp, backward);
    if (!std::isfinite(determinant)
        || std::fabs(dot(right, up)) > kBasisTolerance
        || std::fabs(dot(right, backward)) > kBasisTolerance
        || std::fabs(dot(up, backward)) > kBasisTolerance
        || std::fabs(determinant - 1.0f) > kBasisTolerance) {
        return false;
    }

    Quaternion result;
    const float trace = rotation[0] + rotation[5] + rotation[10];
    if (trace > 0.0f) {
        const float scale = std::sqrt(trace + 1.0f) * 2.0f;
        if (!std::isfinite(scale) || scale < 1.0e-6f) return false;
        result.w = 0.25f * scale;
        result.x = (rotation[9] - rotation[6]) / scale;
        result.y = (rotation[2] - rotation[8]) / scale;
        result.z = (rotation[4] - rotation[1]) / scale;
    } else if (rotation[0] > rotation[5] && rotation[0] > rotation[10]) {
        const float scale = std::sqrt(1.0f + rotation[0] - rotation[5] - rotation[10]) * 2.0f;
        if (!std::isfinite(scale) || scale < 1.0e-6f) return false;
        result.w = (rotation[9] - rotation[6]) / scale;
        result.x = 0.25f * scale;
        result.y = (rotation[1] + rotation[4]) / scale;
        result.z = (rotation[2] + rotation[8]) / scale;
    } else if (rotation[5] > rotation[10]) {
        const float scale = std::sqrt(1.0f + rotation[5] - rotation[0] - rotation[10]) * 2.0f;
        if (!std::isfinite(scale) || scale < 1.0e-6f) return false;
        result.w = (rotation[2] - rotation[8]) / scale;
        result.x = (rotation[1] + rotation[4]) / scale;
        result.y = 0.25f * scale;
        result.z = (rotation[6] + rotation[9]) / scale;
    } else {
        const float scale = std::sqrt(1.0f + rotation[10] - rotation[0] - rotation[5]) * 2.0f;
        if (!std::isfinite(scale) || scale < 1.0e-6f) return false;
        result.w = (rotation[4] - rotation[1]) / scale;
        result.x = (rotation[2] + rotation[8]) / scale;
        result.y = (rotation[6] + rotation[9]) / scale;
        result.z = 0.25f * scale;
    }
    output = Normalize(result);
    return std::isfinite(output.x) && std::isfinite(output.y)
        && std::isfinite(output.z) && std::isfinite(output.w);
}

bool QuaternionFromForwardUp(
    const Vector3& forwardInput,
    const Vector3& upInput,
    Quaternion& output)
{
    auto normalize = [](Vector3& value) {
        const float lengthSquared = value.x * value.x
            + value.y * value.y + value.z * value.z;
        if (!std::isfinite(lengthSquared) || lengthSquared < 1.0e-8f) return false;
        const float inverseLength = 1.0f / std::sqrt(lengthSquared);
        value.x *= inverseLength;
        value.y *= inverseLength;
        value.z *= inverseLength;
        return true;
    };
    auto cross = [](const Vector3& left, const Vector3& right) {
        return Vector3{
            left.y * right.z - left.z * right.y,
            left.z * right.x - left.x * right.z,
            left.x * right.y - left.y * right.x,
        };
    };

    Vector3 forward = forwardInput;
    Vector3 upHint = upInput;
    if (!normalize(forward) || !normalize(upHint)) return false;
    Vector3 right = cross(forward, upHint);
    if (!normalize(right)) return false;
    Vector3 up = cross(right, forward);
    if (!normalize(up)) return false;
    const Vector3 backward{-forward.x, -forward.y, -forward.z};
    const std::array<float, 16> matrix{
        right.x, up.x, backward.x, 0.0f,
        right.y, up.y, backward.y, 0.0f,
        right.z, up.z, backward.z, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
    return QuaternionFromRotationMatrix(matrix, output);
}

std::array<float, 16> RotationMatrix(const Quaternion& input)
{
    const Quaternion q = Normalize(input);
    const float tx = 2.0f * q.x;
    const float ty = 2.0f * q.y;
    const float tz = 2.0f * q.z;
    const float twx = tx * q.w;
    const float twy = ty * q.w;
    const float twz = tz * q.w;
    const float txx = tx * q.x;
    const float txy = ty * q.x;
    const float txz = tz * q.x;
    const float tyy = ty * q.y;
    const float tyz = tz * q.y;
    const float tzz = tz * q.z;

    return {
        1.0f - (tyy + tzz), txy - twz, txz + twy, 0.0f,
        txy + twz, 1.0f - (txx + tzz), tyz - twx, 0.0f,
        txz - twy, tyz + twx, 1.0f - (txx + tyy), 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
}

std::array<float, 16> MatrixMultiply(
    const std::array<float, 16>& left,
    const std::array<float, 16>& right)
{
    std::array<float, 16> result{};
    for (size_t row = 0; row < 4; ++row) {
        for (size_t column = 0; column < 4; ++column) {
            float value = 0.0f;
            for (size_t index = 0; index < 4; ++index) {
                value += left[row * 4 + index] * right[index * 4 + column];
            }
            result[row * 4 + column] = value;
        }
    }
    return result;
}

std::array<float, 16> TranslationMatrix(const Vector3& translation)
{
    return {
        1.0f, 0.0f, 0.0f, translation.x,
        0.0f, 1.0f, 0.0f, translation.y,
        0.0f, 0.0f, 1.0f, translation.z,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
}

Vector3 ResolveTrackedEyeOffset(
    const Vector3& eyePosition,
    const Vector3& headCenter,
    const Vector3& neutralPosition,
    const Quaternion& neutralOrientation,
    bool roomscaleEnabled,
    bool verticalRoomscale,
    float worldScale,
    float eyeHeightOffsetMeters)
{
    Vector3 referenceOffset = roomscaleEnabled
        ? Vector3{
            eyePosition.x - neutralPosition.x,
            eyePosition.y - neutralPosition.y,
            eyePosition.z - neutralPosition.z,
        }
        : Vector3{
            eyePosition.x - headCenter.x,
            eyePosition.y - headCenter.y,
            eyePosition.z - headCenter.z,
        };
    if (!verticalRoomscale) {
        referenceOffset.y = eyePosition.y - headCenter.y;
    }

    Vector3 result = RotateVector(Conjugate(neutralOrientation), referenceOffset);
    result.x *= worldScale;
    result.y = result.y * worldScale + eyeHeightOffsetMeters * worldScale;
    result.z *= worldScale;
    return result;
}

float ComputeRoomscaleSafetyFactor(
    float unobstructedFraction,
    float translationDistance,
    float clearanceDistance)
{
    if (!std::isfinite(unobstructedFraction)
        || !std::isfinite(translationDistance)
        || !std::isfinite(clearanceDistance)
        || translationDistance <= 1.0e-6f) {
        return 1.0f;
    }

    const float fraction = std::clamp(unobstructedFraction, 0.0f, 1.0f);
    const float clearance = std::max(clearanceDistance, 0.0f);
    return std::clamp(
        fraction - clearance / translationDistance,
        0.0f,
        1.0f);
}

Vector3 ReplaceTrackedHeadTranslation(
    const Vector3& rawTrackedOffset,
    const Vector3& rawHeadTranslation,
    const Vector3& safeHeadTranslation)
{
    return {
        rawTrackedOffset.x - rawHeadTranslation.x + safeHeadTranslation.x,
        rawTrackedOffset.y - rawHeadTranslation.y + safeHeadTranslation.y,
        rawTrackedOffset.z - rawHeadTranslation.z + safeHeadTranslation.z,
    };
}

size_t BuildRoomscaleSafetySampleOffsets(
    float horizontalRadius,
    float verticalRadius,
    int radialSamples,
    std::array<Vector3, kMaxRoomscaleSafetySamples>& offsets)
{
    offsets = {};
    size_t count = 1;
    const float radius = std::isfinite(horizontalRadius)
        ? std::max(horizontalRadius, 0.0f) : 0.0f;
    const int samples = std::clamp(radialSamples, 0, 16);
    if (radius > 0.0f && samples > 0) {
        constexpr float kTwoPi = 6.2831853071795864769f;
        for (int index = 0; index < samples; ++index) {
            const float angle = kTwoPi * static_cast<float>(index)
                / static_cast<float>(samples);
            offsets[count++] = {
                std::cos(angle) * radius,
                0.0f,
                std::sin(angle) * radius,
            };
        }
    }

    const float height = std::isfinite(verticalRadius)
        ? std::max(verticalRadius, 0.0f) : 0.0f;
    if (height > 0.0f) {
        offsets[count++] = {0.0f, height, 0.0f};
        offsets[count++] = {0.0f, -height, 0.0f};
    }
    return count;
}

PoseStabilityUpdate UpdatePoseStability(
    PoseStabilityState& state,
    uint64_t gameFrame,
    const Quaternion& orientation,
    const Vector3& position,
    uint32_t requiredConsecutiveFrames,
    float maxPositionStep,
    float maxOrientationStepRadians,
    float& positionStep,
    float& orientationStepRadians)
{
    positionStep = 0.0f;
    orientationStepRadians = 0.0f;

    const float orientationLengthSquared = orientation.x * orientation.x
        + orientation.y * orientation.y + orientation.z * orientation.z
        + orientation.w * orientation.w;
    const bool finite = gameFrame != 0
        && std::isfinite(orientationLengthSquared)
        && orientationLengthSquared >= 1.0e-8f
        && std::isfinite(position.x)
        && std::isfinite(position.y)
        && std::isfinite(position.z)
        && requiredConsecutiveFrames != 0
        && std::isfinite(maxPositionStep)
        && maxPositionStep > 0.0f
        && std::isfinite(maxOrientationStepRadians)
        && maxOrientationStepRadians > 0.0f;
    if (!finite) {
        state = {};
        return PoseStabilityUpdate::Invalid;
    }

    const Quaternion normalizedOrientation = Normalize(orientation);
    const auto storeSample = [&] {
        state.valid = true;
        state.gameFrame = gameFrame;
        state.orientation = normalizedOrientation;
        state.position = position;
    };

    if (!state.valid) {
        storeSample();
        state.consecutiveFrames = 1;
        return requiredConsecutiveFrames == 1
            ? PoseStabilityUpdate::Ready
            : PoseStabilityUpdate::Started;
    }
    if (gameFrame == state.gameFrame) {
        return PoseStabilityUpdate::DuplicateFrame;
    }

    const float dx = position.x - state.position.x;
    const float dy = position.y - state.position.y;
    const float dz = position.z - state.position.z;
    positionStep = std::sqrt(dx * dx + dy * dy + dz * dz);

    const float orientationDot = std::abs(
        normalizedOrientation.x * state.orientation.x
        + normalizedOrientation.y * state.orientation.y
        + normalizedOrientation.z * state.orientation.z
        + normalizedOrientation.w * state.orientation.w);
    orientationStepRadians = 2.0f * std::acos(std::clamp(orientationDot, 0.0f, 1.0f));

    const bool reset = gameFrame < state.gameFrame
        || !std::isfinite(positionStep)
        || !std::isfinite(orientationStepRadians)
        || positionStep > maxPositionStep
        || orientationStepRadians > maxOrientationStepRadians;
    storeSample();
    if (reset) {
        state.consecutiveFrames = 1;
        return PoseStabilityUpdate::Reset;
    }

    if (state.consecutiveFrames < requiredConsecutiveFrames) {
        ++state.consecutiveFrames;
    }
    return state.consecutiveFrames >= requiredConsecutiveFrames
        ? PoseStabilityUpdate::Ready
        : PoseStabilityUpdate::Accumulating;
}

OpenXREyeView CenterProjectionFov(const OpenXREyeView& eye)
{
    OpenXREyeView centered = eye;

    const float horizontalWidth = std::tan(eye.angleRight) - std::tan(eye.angleLeft);
    const float symmetricHalfHorizontalAngle = std::atan(horizontalWidth * 0.5f);
    centered.angleLeft = -symmetricHalfHorizontalAngle;
    centered.angleRight = symmetricHalfHorizontalAngle;

    const float verticalHeight = std::tan(eye.angleUp) - std::tan(eye.angleDown);
    const float symmetricHalfVerticalAngle = std::atan(verticalHeight * 0.5f);
    centered.angleDown = -symmetricHalfVerticalAngle;
    centered.angleUp = symmetricHalfVerticalAngle;

    return centered;
}

bool BuildOpenXRProjection(
    const OpenXREyeView& eye,
    float nearPlane,
    float farPlane,
    std::array<float, 16>& projection,
    float& verticalFov,
    float& aspect)
{
    if (!eye.valid
        || !std::isfinite(eye.angleLeft)
        || !std::isfinite(eye.angleRight)
        || !std::isfinite(eye.angleUp)
        || !std::isfinite(eye.angleDown)
        || nearPlane <= 0.0f
        || farPlane <= nearPlane) {
        return false;
    }

    const float tanLeft = std::tan(eye.angleLeft);
    const float tanRight = std::tan(eye.angleRight);
    const float tanUp = std::tan(eye.angleUp);
    const float tanDown = std::tan(eye.angleDown);
    const float width = tanRight - tanLeft;
    const float height = tanUp - tanDown;
    if (!std::isfinite(width) || !std::isfinite(height) || width <= 0.01f || height <= 0.01f) {
        return false;
    }

    projection = {
        2.0f / width, 0.0f, (tanRight + tanLeft) / width, 0.0f,
        0.0f, 2.0f / height, (tanUp + tanDown) / height, 0.0f,
        0.0f, 0.0f, -(farPlane + nearPlane) / (farPlane - nearPlane),
        -(2.0f * farPlane * nearPlane) / (farPlane - nearPlane),
        0.0f, 0.0f, -1.0f, 0.0f,
    };
    verticalFov = eye.angleUp - eye.angleDown;
    aspect = width / height;
    return std::isfinite(verticalFov)
        && std::isfinite(aspect)
        && verticalFov > 0.4f
        && verticalFov < 3.0f
        && aspect > 0.4f
        && aspect < 4.0f;
}

bool ValidateStereoProjectionMath()
{
    constexpr float kPi = 3.14159265358979323846f;
    const float halfVerticalFov = 35.0f * kPi / 180.0f;
    const float halfHorizontalFov = std::atan(std::tan(halfVerticalFov) * (16.0f / 9.0f));
    OpenXREyeView testEye;
    testEye.valid = true;
    testEye.angleLeft = -halfHorizontalFov;
    testEye.angleRight = halfHorizontalFov;
    testEye.angleUp = halfVerticalFov;
    testEye.angleDown = -halfVerticalFov;

    std::array<float, 16> projection{};
    float fov = 0.0f;
    float aspect = 0.0f;
    return BuildOpenXRProjection(testEye, 0.03f, 1000.0f, projection, fov, aspect)
        && std::abs(projection[0] - 0.803333f) < 0.0001f
        && std::abs(projection[5] - 1.428148f) < 0.0001f
        && std::abs(projection[10] + 1.000060f) < 0.0001f
        && std::abs(projection[11] + 0.060002f) < 0.0001f
        && std::abs(projection[14] + 1.0f) < 0.0001f
        && std::abs(aspect - (16.0f / 9.0f)) < 0.0001f;
}

} // namespace somavr::camera_math
