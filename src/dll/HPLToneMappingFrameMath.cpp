#include "HPLToneMappingFrameMath.h"

namespace somavr::tone_mapping_frame_math {

PassRole Begin(
    State& state,
    uintptr_t effect,
    bool stereoEligible,
    int eyeIndex,
    uint64_t poseFrame,
    uint64_t calibrationGeneration)
{
    if (!stereoEligible || effect == 0 || (eyeIndex != 0 && eyeIndex != 1) || poseFrame == 0) {
        Reset(state);
        return PassRole::None;
    }

    if (state.effect != effect
        || state.poseFrame != poseFrame
        || state.calibrationGeneration != calibrationGeneration) {
        state = {effect, poseFrame, calibrationGeneration, eyeIndex, false, false};
        return PassRole::FirstEye;
    }

    if (eyeIndex != state.firstEye && state.firstCommitted && !state.replayCommitted) {
        return PassRole::ReplayEye;
    }
    return PassRole::None;
}

bool Commit(State& state, PassRole role)
{
    if (role == PassRole::FirstEye && !state.firstCommitted) {
        state.firstCommitted = true;
        return true;
    }
    if (role == PassRole::ReplayEye && state.firstCommitted && !state.replayCommitted) {
        state.replayCommitted = true;
        return true;
    }
    return false;
}

void Reset(State& state)
{
    state = {};
}

} // namespace somavr::tone_mapping_frame_math
