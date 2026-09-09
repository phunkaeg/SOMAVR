#pragma once

#include "HPLCameraMath.h"
#include <cstdint>

namespace somavr::grab_math {

camera_math::Vector3 ResolveAngularTargetVelocity(
    const camera_math::Quaternion& anchor,
    const camera_math::Quaternion& current,
    float gain,
    float sign,
    float maxAngularSpeed);

camera_math::Quaternion ResolveRelativeOrientationTarget(
    const camera_math::Quaternion& anchorController,
    const camera_math::Quaternion& currentController,
    const camera_math::Quaternion& anchorObject);

camera_math::Vector3 ClampVectorMagnitude(
    const camera_math::Vector3& value,
    float maximumMagnitude);

camera_math::Vector3 ResolveGrabPositionCorrection(
    const camera_math::Vector3& initialHandCorrection,
    const camera_math::Vector3& controllerMovement,
    float movementScale,
    float maximumMovement);

float ResolveSlideTargetSpeed(
    float controllerVelocityAlongPin,
    float controllerDisplacementAlongPin,
    float bodyDisplacementAlongPin,
    float velocityScale,
    float positionGain,
    float maximumSpeed);

camera_math::Vector3 RebaseRigidPoint(
    const camera_math::Vector3& point,
    const camera_math::Vector3& anchorPosition,
    const camera_math::Quaternion& anchorOrientation,
    const camera_math::Vector3& currentPosition,
    const camera_math::Quaternion& currentOrientation);

bool IsPhysicalThrowRelease(
    bool velocityValid,
    const camera_math::Vector3& velocity,
    float velocityThreshold);

struct NativeThrowHandoff {
    bool pending = false;
    uint64_t deadlineMs = 0;
};

struct NativeThrowDecision {
    bool begin = false;
    bool holdButtons = false;
    bool timedOut = false;
};

NativeThrowDecision AdvanceNativeThrowHandoff(
    NativeThrowHandoff& state, uint64_t nowMs, bool throwState, bool request);

float ResolveHingeAngularVelocity(
    const camera_math::Vector3& pivot,
    const camera_math::Vector3& point,
    const camera_math::Vector3& pointVelocity,
    const camera_math::Vector3& pin,
    float gain,
    float maxAngularSpeed);

float CombineHingeAngularVelocity(
    float pointAngularVelocity,
    const camera_math::Vector3& controllerAngularVelocity,
    const camera_math::Vector3& pin,
    float controllerScale,
    float maxAngularSpeed);

float ResolveThrowVelocityScale(
    float controllerSpeed,
    float velocityThreshold,
    float velocityReference,
    bool enabled);

camera_math::Vector3 ResolveSafeThrowDirection(
    const camera_math::Vector3& requestedDirection,
    const camera_math::Vector3& cameraForward,
    float minimumForwardDot);

} // namespace somavr::grab_math
