#include "HPLHudMath.h"

#include <algorithm>
#include <cmath>

namespace somavr::hud_math {

bool BuildHeadLockedQuadPose(
    const camera_math::Vector3& headPosition,
    const camera_math::Quaternion& headOrientation,
    float distanceMeters,
    float verticalOffsetMeters,
    float widthMeters,
    float textureAspect,
    HudQuadPose& pose)
{
    if (!std::isfinite(distanceMeters)
        || !std::isfinite(verticalOffsetMeters)
        || !std::isfinite(widthMeters)
        || !std::isfinite(textureAspect)
        || distanceMeters <= 0.0f
        || widthMeters <= 0.0f
        || textureAspect <= 0.0f) {
        return false;
    }

    const camera_math::Quaternion orientation = camera_math::Normalize(headOrientation);
    const camera_math::Vector3 localOffset{0.0f, verticalOffsetMeters, -distanceMeters};
    const camera_math::Vector3 worldOffset = camera_math::RotateVector(orientation, localOffset);
    pose.position = {
        headPosition.x + worldOffset.x,
        headPosition.y + worldOffset.y,
        headPosition.z + worldOffset.z,
    };
    pose.orientation = orientation;
    pose.widthMeters = widthMeters;
    pose.heightMeters = widthMeters / textureAspect;
    return std::isfinite(pose.heightMeters) && pose.heightMeters > 0.0f;
}

bool ComputeAngularQuadSize(
    float distanceMeters,
    float angularSizeDegrees,
    float minSizeMeters,
    float maxSizeMeters,
    float& sizeMeters)
{
    constexpr float kPi = 3.14159265358979323846f;
    if (!std::isfinite(distanceMeters)
        || !std::isfinite(angularSizeDegrees)
        || !std::isfinite(minSizeMeters)
        || !std::isfinite(maxSizeMeters)
        || distanceMeters <= 0.0f
        || angularSizeDegrees <= 0.0f
        || minSizeMeters <= 0.0f
        || maxSizeMeters < minSizeMeters) {
        return false;
    }
    const float angularRadians = angularSizeDegrees * kPi / 180.0f;
    sizeMeters = std::clamp(
        2.0f * std::tan(angularRadians * 0.5f) * distanceMeters,
        minSizeMeters,
        maxSizeMeters);
    return std::isfinite(sizeMeters) && sizeMeters > 0.0f;
}

} // namespace somavr::hud_math
