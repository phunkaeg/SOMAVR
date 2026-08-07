#pragma once

#include "HPLCameraMath.h"

namespace somavr::input_math
{

struct Axis2
{
    float x = 0.0f;
    float y = 0.0f;
};

struct ManipulationMotionState
{
    double remainderX = 0.0;
    double remainderY = 0.0;
};

struct ManipulationMouseDelta
{
    int x = 0;
    int y = 0;
    float rightMeters = 0.0f;
    float upMeters = 0.0f;
};

struct ManipulationRotationDelta
{
    int x = 0;
    int y = 0;
    float yawRadians = 0.0f;
    float pitchRadians = 0.0f;
};

Axis2 ApplyRadialDeadzone(float x, float y, float deadzone);
Axis2 ApplyHeadRelativeMovement(
    float right,
    float forward,
    const camera_math::Quaternion& headOrientation);
float DegreesToRadians(float degrees);
float WrapRadians(float radians);
bool ResolveHorizontalYaw(
    const camera_math::Quaternion& orientation,
    float& yawRadians);
float ComputeBodyFollowStepRadians(
    float yawErrorRadians,
    float releaseDegrees,
    float degreesPerSecond,
    uint64_t elapsedMilliseconds);
float QuaternionAngularDistanceDegrees(
    const camera_math::Quaternion& from,
    const camera_math::Quaternion& to);
ManipulationMouseDelta ComputeManipulationMouseDelta(
    const camera_math::Vector3& previousHandRelativePosition,
    const camera_math::Vector3& currentHandRelativePosition,
    const camera_math::Quaternion& headOrientation,
    float pixelsPerMeter,
    float deadzoneMeters,
    int maxPixelsPerFrame,
    float horizontalSign,
    float verticalSign,
    ManipulationMotionState& state);
ManipulationRotationDelta ComputeManipulationRotationDelta(
    const camera_math::Quaternion& previousLocalOrientation,
    const camera_math::Quaternion& currentLocalOrientation,
    float pixelsPerRadian,
    int maxPixelsPerFrame,
    float horizontalSign,
    float verticalSign,
    ManipulationMotionState& state);

} // namespace somavr::input_math
