#pragma once

#include <cstdint>

namespace somavr::crouch_math {

struct PhysicalCrouchState {
    bool calibrated = false;
    bool crouched = false;
    float standingHeight = 0.0f;
    uint64_t calibrationGeneration = 0;
};

enum class PhysicalCrouchUpdate {
    Invalid,
    Calibrated,
    None,
    Enter,
    Exit,
};

PhysicalCrouchUpdate UpdatePhysicalCrouch(
    PhysicalCrouchState& state,
    float headHeight,
    bool fullyTracked,
    uint64_t calibrationGeneration,
    float enterDropMeters,
    float exitDropMeters);

} // namespace somavr::crouch_math
