#pragma once

#include <cstdint>

namespace somavr::ssao_temporal_math {

enum class BeginAction : uint8_t {
    None,
    Observe,
    Restore,
};

struct State {
    uint64_t calibrationGeneration = 0;
    uint64_t lastPoseFrame[2] = {};
    bool seeded[2] = {};
};

struct BeginResult {
    BeginAction action = BeginAction::None;
    bool reset = false;
};

BeginResult Begin(
    State& state,
    bool stereoEligible,
    int eyeIndex,
    uint64_t poseFrame,
    uint64_t calibrationGeneration,
    bool gpuHistoryReady);
bool Commit(State& state, int eyeIndex, uint64_t poseFrame);
void SeedBoth(State& state, uint64_t poseFrame);
void Reset(State& state);

} // namespace somavr::ssao_temporal_math
