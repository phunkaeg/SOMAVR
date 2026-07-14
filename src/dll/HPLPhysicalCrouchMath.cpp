#include "HPLPhysicalCrouchMath.h"

#include <algorithm>
#include <cmath>

namespace somavr::crouch_math {

PhysicalCrouchUpdate UpdatePhysicalCrouch(
    PhysicalCrouchState& state,
    float headHeight,
    bool fullyTracked,
    uint64_t calibrationGeneration,
    float enterDropMeters,
    float exitDropMeters)
{
    if (!fullyTracked
        || !std::isfinite(headHeight)
        || !std::isfinite(enterDropMeters)
        || !std::isfinite(exitDropMeters)
        || enterDropMeters <= 0.0f
        || exitDropMeters <= 0.0f
        || exitDropMeters >= enterDropMeters) {
        return PhysicalCrouchUpdate::Invalid;
    }

    const bool generationChanged = state.calibrationGeneration != calibrationGeneration;
    state.calibrationGeneration = calibrationGeneration;
    if (!state.calibrated || (generationChanged && !state.crouched)) {
        state.calibrated = true;
        state.standingHeight = headHeight;
        return PhysicalCrouchUpdate::Calibrated;
    }

    if (!state.crouched) {
        state.standingHeight = std::max(state.standingHeight, headHeight);
    }
    const float drop = state.standingHeight - headHeight;
    if (!state.crouched && drop >= enterDropMeters) {
        state.crouched = true;
        return PhysicalCrouchUpdate::Enter;
    }
    if (state.crouched && drop <= exitDropMeters) {
        state.crouched = false;
        return PhysicalCrouchUpdate::Exit;
    }
    return PhysicalCrouchUpdate::None;
}

} // namespace somavr::crouch_math
