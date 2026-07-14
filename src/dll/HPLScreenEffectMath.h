#pragma once

#include "HPLCameraMath.h"

namespace somavr::screen_effect_math {

struct Size2 {
    float width = 0.0f;
    float height = 0.0f;
};

bool ScaleCameraRelativePosition(
    const camera_math::Vector3& cameraPosition,
    const camera_math::Vector3& nativePosition,
    float distanceScale,
    camera_math::Vector3& scaledPosition);
bool ScaleBillboardSize(
    const Size2& nativeSize,
    float distanceScale,
    Size2& scaledSize);

} // namespace somavr::screen_effect_math
