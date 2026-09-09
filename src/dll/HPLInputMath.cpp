#include "HPLInputMath.h"

#include <algorithm>
#include <cmath>

namespace somavr::input_math
{

camera_math::Quaternion OrientationFromHorizontalYaw(float headingRadians)
{
    // ResolveHorizontalYaw is clockwise from -Z; quaternion +Y is counterclockwise.
    const float halfYaw = headingRadians * 0.5f;
    return {0.0f, -std::sin(halfYaw), 0.0f, std::cos(halfYaw)};
}

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

float WrapRadians(float radians)
{
    if (!std::isfinite(radians)) return 0.0f;
    constexpr float kPi = 3.14159265358979323846f;
    constexpr float kTwoPi = 2.0f * kPi;
    radians = std::fmod(radians + kPi, kTwoPi);
    if (radians < 0.0f) radians += kTwoPi;
    return radians - kPi;
}

bool ResolveHorizontalYaw(
    const camera_math::Quaternion& orientation,
    float& yawRadians)
{
    const camera_math::Vector3 forward = camera_math::RotateVector(
        orientation, {0.0f, 0.0f, -1.0f});
    const float horizontalLengthSquared = forward.x * forward.x + forward.z * forward.z;
    if (!std::isfinite(horizontalLengthSquared) || horizontalLengthSquared < 1.0e-6f) {
        yawRadians = 0.0f;
        return false;
    }
    yawRadians = std::atan2(forward.x, -forward.z);
    return std::isfinite(yawRadians);
}

float ComposeBodyFollowWorldYaw(
    float relativeHeadYawRadians,
    float nativeBodyYawRadians,
    bool nativeBodyYawValid)
{
    if (!std::isfinite(relativeHeadYawRadians)) relativeHeadYawRadians = 0.0f;
    if (!nativeBodyYawValid || !std::isfinite(nativeBodyYawRadians)) {
        return WrapRadians(relativeHeadYawRadians);
    }
    return WrapRadians(nativeBodyYawRadians + relativeHeadYawRadians);
}

float ComputeBodyFollowStepRadians(
    float yawErrorRadians,
    float releaseDegrees,
    float degreesPerSecond,
    uint64_t elapsedMilliseconds)
{
    if (!std::isfinite(yawErrorRadians)
        || !std::isfinite(releaseDegrees)
        || !std::isfinite(degreesPerSecond)
        || degreesPerSecond <= 0.0f
        || elapsedMilliseconds == 0) {
        return 0.0f;
    }
    const float releaseRadians = DegreesToRadians(std::max(releaseDegrees, 0.0f));
    const float remaining = std::max(std::fabs(yawErrorRadians) - releaseRadians, 0.0f);
    const float maximumStep = DegreesToRadians(degreesPerSecond)
        * (static_cast<float>(elapsedMilliseconds) / 1000.0f);
    return std::copysign(std::min(remaining, maximumStep), yawErrorRadians);
}

float QuaternionAngularDistanceDegrees(
    const camera_math::Quaternion& from,
    const camera_math::Quaternion& to)
{
    const camera_math::Quaternion normalizedFrom = camera_math::Normalize(from);
    const camera_math::Quaternion normalizedTo = camera_math::Normalize(to);
    const float dot = std::abs(
        normalizedFrom.x * normalizedTo.x
        + normalizedFrom.y * normalizedTo.y
        + normalizedFrom.z * normalizedTo.z
        + normalizedFrom.w * normalizedTo.w);
    constexpr float kRadiansToDegrees = 57.295779513082320876f;
    return 2.0f * std::acos(std::clamp(dot, 0.0f, 1.0f)) * kRadiansToDegrees;
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

ManipulationRotationDelta ComputeManipulationRotationDelta(
    const camera_math::Quaternion& previousLocalOrientation,
    const camera_math::Quaternion& currentLocalOrientation,
    float pixelsPerRadian,
    int maxPixelsPerFrame,
    float horizontalSign,
    float verticalSign,
    ManipulationMotionState& state)
{
    ManipulationRotationDelta result;
    camera_math::Quaternion delta = camera_math::Normalize(camera_math::Multiply(
        camera_math::Normalize(currentLocalOrientation),
        camera_math::Conjugate(camera_math::Normalize(previousLocalOrientation))));
    if (delta.w < 0.0f) {
        delta.x = -delta.x;
        delta.y = -delta.y;
        delta.z = -delta.z;
        delta.w = -delta.w;
    }

    const float halfAngleSin = std::sqrt(std::max(
        0.0f,
        delta.x * delta.x + delta.y * delta.y + delta.z * delta.z));
    if (!std::isfinite(halfAngleSin) || halfAngleSin < 1.0e-7f
        || !std::isfinite(pixelsPerRadian) || pixelsPerRadian <= 0.0f
        || !std::isfinite(horizontalSign) || !std::isfinite(verticalSign)
        || maxPixelsPerFrame <= 0) {
        return result;
    }

    const float angle = 2.0f * std::atan2(
        halfAngleSin,
        std::clamp(delta.w, -1.0f, 1.0f));
    const float axisScale = angle / halfAngleSin;
    result.pitchRadians = delta.x * axisScale;
    result.yawRadians = delta.y * axisScale;
    const double cap = static_cast<double>(maxPixelsPerFrame);
    const double outputX = std::clamp(
        state.remainderX + static_cast<double>(result.yawRadians)
            * static_cast<double>(pixelsPerRadian) * static_cast<double>(horizontalSign),
        -cap,
        cap);
    const double outputY = std::clamp(
        state.remainderY + static_cast<double>(result.pitchRadians)
            * static_cast<double>(pixelsPerRadian) * static_cast<double>(verticalSign),
        -cap,
        cap);
    result.x = static_cast<int>(std::trunc(outputX));
    result.y = static_cast<int>(std::trunc(outputY));
    state.remainderX = outputX - static_cast<double>(result.x);
    state.remainderY = outputY - static_cast<double>(result.y);
    return result;
}

} // namespace somavr::input_math
