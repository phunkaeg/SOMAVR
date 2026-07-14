#include "HPLHudMath.h"

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

} // namespace somavr::hud_math
