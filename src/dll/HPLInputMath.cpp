#include "HPLInputMath.h"

#include <algorithm>
#include <cmath>

namespace somavr::input_math
{

Axis2 ApplyRadialDeadzone(float x, float y, float deadzone)
{
    const float clampedDeadzone = std::clamp(deadzone, 0.0f, 0.999f);
    const float length = std::sqrt(x * x + y * y);
    if (!std::isfinite(length) || length <= clampedDeadzone || length <= 0.000001f)
        return {};
    const float normalizedLength = std::min((length - clampedDeadzone) / (1.0f - clampedDeadzone), 1.0f);
    const float scale = normalizedLength / length;
    return {x * scale, y * scale};
}

Axis2 ApplyHeadRelativeMovement(
    float right,
    float forward,
    const camera_math::Quaternion& headOrientation)
{
    if (!std::isfinite(right) || !std::isfinite(forward)) return {};
    const camera_math::Vector3 headForward = camera_math::RotateVector(
        headOrientation, {0.0f, 0.0f, -1.0f});
    const float horizontalLength = std::sqrt(
        headForward.x * headForward.x + headForward.z * headForward.z);
    if (!std::isfinite(horizontalLength) || horizontalLength < 0.0001f) return {right, forward};
    const float forwardRight = headForward.x / horizontalLength;
    const float forwardForward = -headForward.z / horizontalLength;
    const Axis2 result{
        forwardForward * right + forwardRight * forward,
        -forwardRight * right + forwardForward * forward,
    };
    if (!std::isfinite(result.x) || !std::isfinite(result.y)) return {};
    return result;
}

float DegreesToRadians(float degrees)
{
    constexpr float kDegreesToRadians = 0.01745329251994329577f;
    return degrees * kDegreesToRadians;
}

} // namespace somavr::input_math
