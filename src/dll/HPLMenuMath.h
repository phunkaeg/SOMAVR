#pragma once

#include "HPLCameraMath.h"

namespace somavr::menu_math {

struct MenuPointerPosition {
    float x = 0.5f;
    float y = 0.5f;
};

bool ProjectAimToMenu(
    const camera_math::Quaternion& headOrientation,
    const camera_math::Quaternion& aimOrientation,
    float horizontalFovDegrees,
    float verticalFovDegrees,
    MenuPointerPosition& position);

} // namespace somavr::menu_math
