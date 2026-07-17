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
