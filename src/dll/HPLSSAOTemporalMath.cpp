#include "HPLSSAOTemporalMath.h"

namespace somavr::ssao_temporal_math {

BeginResult Begin(
    State& state,
    bool stereoEligible,
    int eyeIndex,
    uint64_t poseFrame,
    uint64_t calibrationGeneration,
    bool gpuHistoryReady)
{
    if (!stereoEligible || (eyeIndex != 0 && eyeIndex != 1) || poseFrame == 0) {
        const bool reset = state.seeded[0] || state.seeded[1];
        Reset(state);
        return {BeginAction::None, reset};
    }

    bool reset = false;
    if (state.calibrationGeneration != calibrationGeneration) {
        Reset(state);
        state.calibrationGeneration = calibrationGeneration;
        reset = true;
    }
    return {
        gpuHistoryReady && state.seeded[eyeIndex]
            ? BeginAction::Restore
            : BeginAction::Observe,
        reset,
    };
}

bool Commit(State& state, int eyeIndex, uint64_t poseFrame)
{
    if ((eyeIndex != 0 && eyeIndex != 1) || poseFrame == 0) return false;
    state.seeded[eyeIndex] = true;
    state.lastPoseFrame[eyeIndex] = poseFrame;
    return true;
}

void SeedBoth(State& state, uint64_t poseFrame)
{
    state.seeded[0] = true;
    state.seeded[1] = true;
    state.lastPoseFrame[0] = poseFrame;
    state.lastPoseFrame[1] = poseFrame;
}

void Reset(State& state)
{
    const uint64_t generation = state.calibrationGeneration;
    state = {};
    state.calibrationGeneration = generation;
}

} // namespace somavr::ssao_temporal_math
