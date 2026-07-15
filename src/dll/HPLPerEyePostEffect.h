#pragma once

#include <cstdint>

namespace somavr {

struct Config;

using HPLImageTrailResourceFn = void (*)(void* effect);

struct HPLPerEyePostEffectStatus {
    bool configured = false;
    bool available = false;
    bool faulted = false;
    uint64_t effects = 0;
    uint64_t allocations = 0;
    uint64_t restores = 0;
    uint64_t captures = 0;
    uint64_t resets = 0;
    uint64_t releases = 0;
    uint64_t failures = 0;
};

void InitializeHPLPerEyePostEffect(
    const Config& config,
    HPLImageTrailResourceFn createResources,
    HPLImageTrailResourceFn destroyResources);
bool IsHPLPerEyeImageTrailAvailable();
void BeginHPLPerEyePostEffect(
    void* effect,
    bool imageTrail,
    bool stereoEligible,
    int eyeIndex,
    uint64_t poseFrame,
    uint64_t calibrationGeneration);
void EndHPLPerEyePostEffect(void* effect);
void DestroyHPLPerEyeImageTrail(void* effect);
HPLPerEyePostEffectStatus GetHPLPerEyePostEffectStatus();
void RemoveHPLPerEyePostEffect();

} // namespace somavr
