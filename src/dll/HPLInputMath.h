#pragma once

#include "HPLCameraMath.h"

namespace somavr::input_math
{

struct Axis2
{
    float x = 0.0f;
    float y = 0.0f;
};

Axis2 ApplyRadialDeadzone(float x, float y, float deadzone);
Axis2 ApplyHeadRelativeMovement(
    float right,
    float forward,
    const camera_math::Quaternion& headOrientation);
float DegreesToRadians(float degrees);

} // namespace somavr::input_math
