#pragma once

#include "HPLCameraMath.h"

#include <array>

namespace somavr::hands_math {

struct HandRootCalibration {
    camera_math::Vector3 positionOffset{};
    camera_math::Vector3 rotationDegrees{};
};

bool BuildControllerHandMatrix(
    const camera_math::Vector3& gripPosition,
    const camera_math::Vector3& gripForward,
    const camera_math::Vector3& gripUp,
    float scale,
    const HandRootCalibration& calibration,
    std::array<float, 16>& matrix);

} // namespace somavr::hands_math
