#include "HPLRoomscaleReconciliationMath.h"

#include <algorithm>
#include <cmath>

namespace somavr::roomscale_reconciliation_math {

bool ComputeBodyCatchupStep(
    float offsetX,
    float offsetZ,
    float threshold,
    float target,
    float maximumStep,
    bool active,
    float& stepX,
    float& stepZ)
{
    stepX = 0.0f;
    stepZ = 0.0f;
    if (!std::isfinite(offsetX) || !std::isfinite(offsetZ)
        || !std::isfinite(threshold) || !std::isfinite(target)
        || !std::isfinite(maximumStep) || threshold <= 0.0f
        || target < 0.0f || target >= threshold || maximumStep <= 0.0f) {
        return false;
    }

    const float distance = std::sqrt(offsetX * offsetX + offsetZ * offsetZ);
    const float activationDistance = active ? target : threshold;
    if (!std::isfinite(distance) || distance <= activationDistance) {
        return false;
    }

    const float reduction = std::min(distance - target, maximumStep);
    if (reduction <= 0.0f) {
        return false;
    }
    const float scale = reduction / distance;
    stepX = offsetX * scale;
    stepZ = offsetZ * scale;
    return std::isfinite(stepX) && std::isfinite(stepZ);
}

} // namespace somavr::roomscale_reconciliation_math
