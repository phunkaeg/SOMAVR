#pragma once

#include "HPLCameraMath.h"

namespace somavr::two_hand_math {

struct TwoHandBasis {
    camera_math::Vector3 forward{};
    camera_math::Vector3 up{};
    float separation = 0.0f;
};

bool BuildTwoHandBasis(
    const camera_math::Vector3& dominantPosition,
    const camera_math::Vector3& dominantForward,
    const camera_math::Vector3& dominantUp,
    const camera_math::Vector3& supportPosition,
    float directionBlend,
    float minimumSeparation,
    float maximumSeparation,
    TwoHandBasis& basis);

camera_math::Vector3 ResolveDirectionAngularTargetVelocity(
    const camera_math::Vector3& anchorDirection,
    const camera_math::Vector3& currentDirection,
    float gain,
    float sign,
    float maxAngularSpeed);

} // namespace somavr::two_hand_math
