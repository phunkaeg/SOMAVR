#pragma once

#include <cstdint>

namespace somavr {

struct Config;

struct HPLSSAOTemporalHistoryStatus {
    bool configured = false;
    bool available = false;
    bool faulted = false;
    uint64_t trackedRenderers = 0;
    uint64_t allocations = 0;
    uint64_t restores = 0;
    uint64_t commits = 0;
    uint64_t seeds = 0;
    uint64_t resets = 0;
    uint64_t failures = 0;
};

void InitializeHPLSSAOTemporalHistory(const Config& config);
void BeginHPLSSAOTemporalPass(
    void* renderer,
    void* nativeHistoryTexture,
    bool stereoEligible,
    int eyeIndex,
    uint64_t poseFrame,
    uint64_t calibrationGeneration);
void EndHPLSSAOTemporalPass(void* renderer);
void BeginHPLSSAOTemporalTextureBind(void* nativeTexture);
void EndHPLSSAOTemporalTextureBind();
void ObserveHPLSSAOTemporalGLBind(uint32_t target, uint32_t texture);
HPLSSAOTemporalHistoryStatus GetHPLSSAOTemporalHistoryStatus();
void RemoveHPLSSAOTemporalHistory();

} // namespace somavr
