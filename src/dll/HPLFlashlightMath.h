#pragma once

#include "HPLCameraMath.h"

#include <array>

namespace somavr::flashlight_math {

struct FlashlightCalibration {
    camera_math::Vector3 positionOffset{};
    camera_math::Vector3 rotationDegrees{};
};

bool BuildControllerFlashlightMatrix(
    const camera_math::Vector3& aimPosition,
    const camera_math::Vector3& aimForward,
    const camera_math::Vector3& aimUp,
    const FlashlightCalibration& calibration,
    std::array<float, 16>& matrix);

bool RedirectConeDirection(
    const camera_math::Vector3& nativeDirection,
    const camera_math::Vector3& nativeForward,
    const camera_math::Vector3& nativeUp,
    const camera_math::Vector3& targetForward,
    const camera_math::Vector3& targetUp,
    camera_math::Vector3& redirectedDirection);

} // namespace somavr::flashlight_math
