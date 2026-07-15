#pragma once

#include <cstdint>

namespace somavr {

struct Config;

struct HPLPerEyeViewHistoryStatus {
    bool configured = false;
    bool active = false;
    bool faulted = false;
    uint64_t activations = 0;
    uint64_t resets = 0;
    uint64_t seeds = 0;
    uint64_t restores = 0;
    uint64_t captures = 0;
    uint64_t failures = 0;
};

void InitializeHPLPerEyeViewHistory(const Config& config);
void SetHPLPerEyeViewHistoryActive(bool active, const char* source);
void ObserveHPLPerEyeViewHistoryRenderer(void* renderer);
void BeginHPLPerEyeViewHistoryPass(
    int eyeIndex, uint64_t poseFrame, uint64_t calibrationGeneration);
void EndHPLPerEyeViewHistoryPass(int actualEyeIndex, uint64_t actualPoseFrame);
HPLPerEyeViewHistoryStatus GetHPLPerEyeViewHistoryStatus();
void RemoveHPLPerEyeViewHistory();

} // namespace somavr
