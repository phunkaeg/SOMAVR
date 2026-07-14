#include "HPLHandsMath.h"

#include <cmath>

namespace somavr::hands_math {
namespace {

constexpr float kPi = 3.14159265358979323846f;

bool IsFinite(const camera_math::Vector3& value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

camera_math::Vector3 Cross(
    const camera_math::Vector3& left,
    const camera_math::Vector3& right)
{
    return {
        left.y * right.z - left.z * right.y,
        left.z * right.x - left.x * right.z,
        left.x * right.y - left.y * right.x,
    };
}

bool Normalize(camera_math::Vector3& value)
{
    const float lengthSquared = value.x * value.x + value.y * value.y + value.z * value.z;
    if (!std::isfinite(lengthSquared) || lengthSquared < 1.0e-8f) return false;
    const float inverseLength = 1.0f / std::sqrt(lengthSquared);
    value.x *= inverseLength;
    value.y *= inverseLength;
    value.z *= inverseLength;
    return true;
}

std::array<float, 16> RotationX(float angle)
{
    const float cosine = std::cos(angle);
    const float sine = std::sin(angle);
    return {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, cosine, -sine, 0.0f,
        0.0f, sine, cosine, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
}

std::array<float, 16> RotationY(float angle)
{
    const float cosine = std::cos(angle);
    const float sine = std::sin(angle);
    return {
        cosine, 0.0f, sine, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        -sine, 0.0f, cosine, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
}

std::array<float, 16> RotationZ(float angle)
{
    const float cosine = std::cos(angle);
    const float sine = std::sin(angle);
    return {
        cosine, -sine, 0.0f, 0.0f,
        sine, cosine, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
}

} // namespace

bool BuildControllerHandMatrix(
    const camera_math::Vector3& gripPosition,
    const camera_math::Vector3& gripForward,
    const camera_math::Vector3& gripUp,
    float scale,
    const HandRootCalibration& calibration,
    std::array<float, 16>& matrix)
{
    matrix = {};
    if (!IsFinite(gripPosition)
        || !IsFinite(gripForward)
        || !IsFinite(gripUp)
        || !IsFinite(calibration.positionOffset)
        || !IsFinite(calibration.rotationDegrees)
        || !std::isfinite(scale)
        || scale <= 0.0f) {
        return false;
    }

    camera_math::Vector3 forward = gripForward;
    camera_math::Vector3 upHint = gripUp;
    if (!Normalize(forward) || !Normalize(upHint)) return false;

    camera_math::Vector3 controllerRight = Cross(forward, upHint);
    if (!Normalize(controllerRight)) return false;
    camera_math::Vector3 up = Cross(controllerRight, forward);
    if (!Normalize(up)) return false;

    const camera_math::Vector3 handRight{
        -controllerRight.x,
        -controllerRight.y,
        -controllerRight.z,
    };
    std::array<float, 16> root = {
        handRight.x, up.x, forward.x, 0.0f,
        handRight.y, up.y, forward.y, 0.0f,
        handRight.z, up.z, forward.z, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };

    const float radians = kPi / 180.0f;
    const std::array<float, 16> correction = camera_math::MatrixMultiply(
        RotationZ(calibration.rotationDegrees.z * radians),
        camera_math::MatrixMultiply(
            RotationY(calibration.rotationDegrees.y * radians),
            RotationX(calibration.rotationDegrees.x * radians)));
    const std::array<float, 16> scaleMatrix = {
        scale, 0.0f, 0.0f, 0.0f,
        0.0f, scale, 0.0f, 0.0f,
        0.0f, 0.0f, scale, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
    matrix = camera_math::MatrixMultiply(
        root,
        camera_math::MatrixMultiply(correction, scaleMatrix));

    matrix[3] = gripPosition.x
        + controllerRight.x * calibration.positionOffset.x
        + up.x * calibration.positionOffset.y
        + forward.x * calibration.positionOffset.z;
    matrix[7] = gripPosition.y
        + controllerRight.y * calibration.positionOffset.x
        + up.y * calibration.positionOffset.y
        + forward.y * calibration.positionOffset.z;
    matrix[11] = gripPosition.z
        + controllerRight.z * calibration.positionOffset.x
        + up.z * calibration.positionOffset.y
        + forward.z * calibration.positionOffset.z;
    return true;
}

} // namespace somavr::hands_math
