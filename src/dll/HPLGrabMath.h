#pragma once

#include "HPLCameraMath.h"

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

} // namespace somavr::grab_math
