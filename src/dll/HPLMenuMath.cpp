#include "HPLMenuMath.h"

#include <algorithm>
#include <cmath>

namespace somavr::menu_math {
namespace {

bool ValidQuaternion(const camera_math::Quaternion& value)
{
    const float lengthSquared = value.x * value.x + value.y * value.y
        + value.z * value.z + value.w * value.w;
    return std::isfinite(lengthSquared) && lengthSquared >= 1.0e-8f;
}

} // namespace

bool ProjectAimToMenu(
    const camera_math::Quaternion& headOrientation,
    const camera_math::Quaternion& aimOrientation,
    float horizontalFovDegrees,
    float verticalFovDegrees,
    MenuPointerPosition& position)
{
    position = {};
    if (!ValidQuaternion(headOrientation)
        || !ValidQuaternion(aimOrientation)
        || !std::isfinite(horizontalFovDegrees)
        || !std::isfinite(verticalFovDegrees)
        || horizontalFovDegrees <= 1.0f
        || horizontalFovDegrees >= 179.0f
        || verticalFovDegrees <= 1.0f
        || verticalFovDegrees >= 179.0f) {
        return false;
    }

    const camera_math::Quaternion relative = camera_math::Normalize(camera_math::Multiply(
        camera_math::Conjugate(camera_math::Normalize(headOrientation)),
        camera_math::Normalize(aimOrientation)));
    const camera_math::Vector3 forward = camera_math::RotateVector(relative, {0.0f, 0.0f, -1.0f});
    if (!std::isfinite(forward.x)
        || !std::isfinite(forward.y)
        || !std::isfinite(forward.z)
        || forward.z >= -0.05f) {
        return false;
    }

    constexpr float radians = 3.14159265358979323846f / 180.0f;
    const float halfWidth = std::tan(horizontalFovDegrees * 0.5f * radians);
    const float halfHeight = std::tan(verticalFovDegrees * 0.5f * radians);
    if (!std::isfinite(halfWidth)
        || !std::isfinite(halfHeight)
        || halfWidth <= 0.0f
        || halfHeight <= 0.0f) {
        return false;
    }

    const float planeX = forward.x / -forward.z;
    const float planeY = forward.y / -forward.z;
    position.x = std::clamp(0.5f + 0.5f * planeX / halfWidth, 0.0f, 1.0f);
    position.y = std::clamp(0.5f - 0.5f * planeY / halfHeight, 0.0f, 1.0f);
    return true;
}

} // namespace somavr::menu_math
