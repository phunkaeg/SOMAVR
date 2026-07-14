#include "OpenXRDepthMath.h"

#include <cmath>

namespace somavr::depth_math {

bool BuildStandardDepthRange(
    float nearWorld,
    float farWorld,
    float worldUnitsPerMeter,
    CompositionDepthRange& range)
{
    range = {};
    if (!std::isfinite(nearWorld)
        || !std::isfinite(farWorld)
        || !std::isfinite(worldUnitsPerMeter)
        || nearWorld <= 0.0f
        || farWorld <= nearWorld
        || worldUnitsPerMeter <= 0.0f) {
        return false;
    }

    range.minDepth = 0.0f;
    range.maxDepth = 1.0f;
    range.nearMeters = nearWorld / worldUnitsPerMeter;
    range.farMeters = farWorld / worldUnitsPerMeter;
    return std::isfinite(range.nearMeters)
        && std::isfinite(range.farMeters)
        && range.nearMeters > 0.0f
        && range.farMeters > range.nearMeters;
}

} // namespace somavr::depth_math
