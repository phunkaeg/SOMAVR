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

} // namespace somavr::flashlight_math
