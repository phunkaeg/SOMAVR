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

ManipulationMouseDelta ComputeManipulationMouseDelta(
    const camera_math::Vector3& previousHandRelativePosition,
    const camera_math::Vector3& currentHandRelativePosition,
    const camera_math::Quaternion& headOrientation,
    float pixelsPerMeter,
    float deadzoneMeters,
    int maxPixelsPerFrame,
    float horizontalSign,
    float verticalSign,
    ManipulationMotionState& state)
{
    ManipulationMouseDelta result;
    const camera_math::Vector3 displacement{
        currentHandRelativePosition.x - previousHandRelativePosition.x,
        currentHandRelativePosition.y - previousHandRelativePosition.y,
        currentHandRelativePosition.z - previousHandRelativePosition.z,
    };
    const camera_math::Vector3 headRight = camera_math::RotateVector(
        headOrientation, {1.0f, 0.0f, 0.0f});
    const camera_math::Vector3 headUp = camera_math::RotateVector(
        headOrientation, {0.0f, 1.0f, 0.0f});
    result.rightMeters = displacement.x * headRight.x
        + displacement.y * headRight.y + displacement.z * headRight.z;
    result.upMeters = displacement.x * headUp.x
        + displacement.y * headUp.y + displacement.z * headUp.z;
    const float magnitude = std::sqrt(
        result.rightMeters * result.rightMeters + result.upMeters * result.upMeters);
    if (!std::isfinite(magnitude) || !std::isfinite(pixelsPerMeter)
        || !std::isfinite(horizontalSign) || !std::isfinite(verticalSign)
        || magnitude <= std::max(deadzoneMeters, 0.0f) || pixelsPerMeter <= 0.0f
        || maxPixelsPerFrame <= 0) {
        return result;
    }

    const double cap = static_cast<double>(maxPixelsPerFrame);
    const double outputX = std::clamp(
        state.remainderX + static_cast<double>(result.rightMeters)
            * static_cast<double>(pixelsPerMeter) * static_cast<double>(horizontalSign),
        -cap,
        cap);
    const double outputY = std::clamp(
        state.remainderY + static_cast<double>(result.upMeters)
            * static_cast<double>(pixelsPerMeter) * static_cast<double>(verticalSign),
        -cap,
        cap);
    result.x = static_cast<int>(std::trunc(outputX));
    result.y = static_cast<int>(std::trunc(outputY));
    state.remainderX = outputX - static_cast<double>(result.x);
    state.remainderY = outputY - static_cast<double>(result.y);
    return result;
}

} // namespace somavr::input_math
