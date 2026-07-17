#pragma once

#include "HPLCameraMath.h"

#include <array>

namespace somavr::read_math {

bool BuildReadPresentationMatrix(
    const std::array<float, 16>& nativeMatrix,
    const camera_math::Vector3& cameraPosition,
    float targetDistance,
    float objectScale,
    std::array<float, 16>& output);

} // namespace somavr::read_math
