#pragma once

#include <cstdint>

namespace somavr {

struct Config;

struct HPLToneMappingFrameStatus {
    bool configured = false;
    bool active = false;
    bool faulted = false;
    uint64_t trackedEffects = 0;
    uint64_t firstPasses = 0;
    uint64_t replayPasses = 0;
    uint64_t committedRestores = 0;
    uint64_t mismatches = 0;
    uint64_t failures = 0;
};

void InitializeHPLToneMappingFrame(const Config& config);
void BeginHPLToneMappingFrame(
    void* effect,
    bool toneMapping,
    bool stereoEligible,
    int eyeIndex,
    uint64_t poseFrame,
    uint64_t calibrationGeneration);
void EndHPLToneMappingFrame(void* effect);
HPLToneMappingFrameStatus GetHPLToneMappingFrameStatus();
void RemoveHPLToneMappingFrame();

} // namespace somavr
