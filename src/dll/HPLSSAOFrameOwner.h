#pragma once

#include <cstdint>

namespace somavr {

struct Config;

struct HPLSSAOFrameOwnerStatus {
    bool configured = false;
    bool available = false;
    bool faulted = false;
    uint64_t trackedRenderers = 0;
    uint64_t firstPasses = 0;
    uint64_t replayPasses = 0;
    uint64_t committedRestores = 0;
    uint64_t mismatches = 0;
    uint64_t failures = 0;
};

void InitializeHPLSSAOFrameOwner(const Config& config, float* temporalPhase);
void BeginHPLSSAOFrameOwner(
    void* renderer,
    bool stereoEligible,
    int eyeIndex,
    uint64_t poseFrame,
    uint64_t calibrationGeneration);
void EndHPLSSAOFrameOwner(void* renderer);
HPLSSAOFrameOwnerStatus GetHPLSSAOFrameOwnerStatus();
void RemoveHPLSSAOFrameOwner();

} // namespace somavr
