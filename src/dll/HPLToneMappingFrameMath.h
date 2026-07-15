#pragma once

#include <cstdint>

namespace somavr::tone_mapping_frame_math {

enum class PassRole : uint8_t {
    None,
    FirstEye,
    ReplayEye,
};

struct State {
    uintptr_t effect = 0;
    uint64_t poseFrame = 0;
    uint64_t calibrationGeneration = 0;
    int firstEye = -1;
    bool firstCommitted = false;
    bool replayCommitted = false;
};

PassRole Begin(
    State& state,
    uintptr_t effect,
    bool stereoEligible,
    int eyeIndex,
    uint64_t poseFrame,
    uint64_t calibrationGeneration);
bool Commit(State& state, PassRole role);
void Reset(State& state);

} // namespace somavr::tone_mapping_frame_math
