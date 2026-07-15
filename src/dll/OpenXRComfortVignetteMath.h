#pragma once

#include <cstdint>
#include <vector>

namespace somavr::comfort_vignette_math {

float ComputeMotionIntensity(
    float moveX,
    float moveY,
    float turnX,
    bool includeSmoothTurn,
    float moveDeadzone,
    float turnDeadzone);

float AdvanceEnvelope(
    float currentLevel,
    float targetLevel,
    float deltaSeconds,
    int fadeMilliseconds);

bool Rasterize(
    int sizePixels,
    float level,
    float strength,
    float innerRadius,
    std::vector<uint8_t>& rgbaPixels);

} // namespace somavr::comfort_vignette_math
