#pragma once

namespace somavr::roomscale_reconciliation_math {

bool ComputeBodyCatchupStep(
    float offsetX,
    float offsetZ,
    float threshold,
    float target,
    float maximumStep,
    bool active,
    float& stepX,
    float& stepZ);

} // namespace somavr::roomscale_reconciliation_math
