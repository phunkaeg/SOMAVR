#pragma once

#include "HPLCameraMath.h"

namespace somavr::hud_math {

struct HudQuadPose {
    camera_math::Vector3 position{};
    camera_math::Quaternion orientation{};
    float widthMeters = 0.0f;
    float heightMeters = 0.0f;
};

bool BuildHeadLockedQuadPose(
    const camera_math::Vector3& headPosition,
    const camera_math::Quaternion& headOrientation,
    float distanceMeters,
    float verticalOffsetMeters,
    float widthMeters,
    float textureAspect,
    HudQuadPose& pose);

} // namespace somavr::hud_math
