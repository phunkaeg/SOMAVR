#pragma once

#include "HPLCameraMath.h"

#include <array>

namespace somavr::read_math {

bool BuildReadPresentationMatrix(
    const std::array<float, 16>& nativeMatrix,
    float scaleMultiplier,
    const camera_math::Quaternion* orientationOverride,
    std::array<float, 16>& output);

camera_math::Quaternion ResolveRelativeOrientation(
    const camera_math::Quaternion& anchorController,
    const camera_math::Quaternion& currentController,
    const camera_math::Quaternion& anchorObject);

bool ScaleCameraRelativePosition(
    const camera_math::Vector3& cameraPosition,
    const camera_math::Vector3& nativePosition,
    float distanceScale,
    camera_math::Vector3& output);

bool ResolveLatchedCameraRelativePosition(
    const camera_math::Vector3& sourceCameraPosition,
    const camera_math::Vector3& sourceObjectPosition,
    const camera_math::Vector3& currentCameraPosition,
    float distanceScale,
    camera_math::Vector3& output);

} // namespace somavr::read_math
