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

Vector3 RotateVector(const Quaternion& input, const Vector3& value)
{
    const Quaternion q = Normalize(input);
    const Quaternion pure{value.x, value.y, value.z, 0.0f};
    const Quaternion rotated = Multiply(Multiply(q, pure), Conjugate(q));
    return {rotated.x, rotated.y, rotated.z};
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
