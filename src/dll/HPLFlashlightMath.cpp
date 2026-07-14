#include "HPLFlashlightMath.h"

#include <cmath>

namespace somavr::flashlight_math {
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

bool BuildControllerFlashlightMatrix(
    const camera_math::Vector3& aimPosition,
    const camera_math::Vector3& aimForward,
    const camera_math::Vector3& aimUp,
    const FlashlightCalibration& calibration,
    std::array<float, 16>& matrix)
{
    matrix = {};
    if (!IsFinite(aimPosition)
        || !IsFinite(aimForward)
        || !IsFinite(aimUp)
        || !IsFinite(calibration.positionOffset)
        || !IsFinite(calibration.rotationDegrees)) {
        return false;
    }

    camera_math::Vector3 forward = aimForward;
    camera_math::Vector3 upHint = aimUp;
    if (!Normalize(forward) || !Normalize(upHint)) return false;

    camera_math::Vector3 right = Cross(forward, upHint);
    if (!Normalize(right)) return false;
    camera_math::Vector3 up = Cross(right, forward);
    if (!Normalize(up)) return false;
    const camera_math::Vector3 backward{-forward.x, -forward.y, -forward.z};

    const std::array<float, 16> aimBasis = {
        right.x, up.x, backward.x, 0.0f,
        right.y, up.y, backward.y, 0.0f,
        right.z, up.z, backward.z, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };

    const float radians = kPi / 180.0f;
    const std::array<float, 16> correction = camera_math::MatrixMultiply(
        RotationZ(calibration.rotationDegrees.z * radians),
        camera_math::MatrixMultiply(
            RotationY(calibration.rotationDegrees.y * radians),
            RotationX(calibration.rotationDegrees.x * radians)));
    matrix = camera_math::MatrixMultiply(aimBasis, correction);

    matrix[3] = aimPosition.x
        + right.x * calibration.positionOffset.x
        + up.x * calibration.positionOffset.y
        + forward.x * calibration.positionOffset.z;
    matrix[7] = aimPosition.y
        + right.y * calibration.positionOffset.x
        + up.y * calibration.positionOffset.y
        + forward.y * calibration.positionOffset.z;
    matrix[11] = aimPosition.z
        + right.z * calibration.positionOffset.x
        + up.z * calibration.positionOffset.y
        + forward.z * calibration.positionOffset.z;
    return true;
}

bool RedirectConeDirection(
    const camera_math::Vector3& nativeDirection,
    const camera_math::Vector3& nativeForward,
    const camera_math::Vector3& nativeUp,
    const camera_math::Vector3& targetForward,
    const camera_math::Vector3& targetUp,
    camera_math::Vector3& redirectedDirection)
{
    redirectedDirection = {};
    if (!IsFinite(nativeDirection) || !IsFinite(nativeForward) || !IsFinite(nativeUp)
        || !IsFinite(targetForward) || !IsFinite(targetUp)) {
        return false;
    }
    camera_math::Vector3 direction = nativeDirection;
    camera_math::Vector3 sourceForward = nativeForward;
    camera_math::Vector3 sourceUpHint = nativeUp;
    camera_math::Vector3 destinationForward = targetForward;
    camera_math::Vector3 destinationUpHint = targetUp;
    if (!Normalize(direction) || !Normalize(sourceForward) || !Normalize(sourceUpHint)
        || !Normalize(destinationForward) || !Normalize(destinationUpHint)) {
        return false;
    }
    camera_math::Vector3 sourceRight = Cross(sourceForward, sourceUpHint);
    camera_math::Vector3 destinationRight = Cross(destinationForward, destinationUpHint);
    if (!Normalize(sourceRight) || !Normalize(destinationRight)) return false;
    camera_math::Vector3 sourceUp = Cross(sourceRight, sourceForward);
    camera_math::Vector3 destinationUp = Cross(destinationRight, destinationForward);
    if (!Normalize(sourceUp) || !Normalize(destinationUp)) return false;

    const float localRight = direction.x * sourceRight.x
        + direction.y * sourceRight.y + direction.z * sourceRight.z;
    const float localUp = direction.x * sourceUp.x
        + direction.y * sourceUp.y + direction.z * sourceUp.z;
    const float localForward = direction.x * sourceForward.x
        + direction.y * sourceForward.y + direction.z * sourceForward.z;
    redirectedDirection = {
        destinationRight.x * localRight + destinationUp.x * localUp + destinationForward.x * localForward,
        destinationRight.y * localRight + destinationUp.y * localUp + destinationForward.y * localForward,
        destinationRight.z * localRight + destinationUp.z * localUp + destinationForward.z * localForward,
    };
    return Normalize(redirectedDirection);
}

} // namespace somavr::flashlight_math
