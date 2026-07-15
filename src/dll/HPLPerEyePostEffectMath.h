#pragma once

#include <array>
#include <cstdint>

namespace somavr::per_eye_post_effect_math {

constexpr uint64_t kMaxPoseFrameGap = 8;

struct ResourcePair {
    uintptr_t framebuffer = 0;
    uintptr_t texture = 0;
};

struct EyeState {
    ResourcePair resources{};
    bool clear = true;
    uint64_t lastPoseFrame = 0;
};

struct Bank {
    bool initialized = false;
    uintptr_t effect = 0;
    uint64_t calibrationGeneration = 0;
    std::array<EyeState, 2> eyes{};
    int boundEye = -1;
    int pendingEye = -1;
    uint64_t pendingPoseFrame = 0;
};

struct PrepareResult {
    bool valid = false;
    bool reset = false;
    ResourcePair resources{};
    bool clear = true;
};

bool IsValid(const ResourcePair& resources);
bool Initialize(
    Bank& bank,
    uintptr_t effect,
    const ResourcePair& primary,
    bool primaryClear,
    const ResourcePair& secondary,
    uint64_t calibrationGeneration);
PrepareResult Prepare(
    Bank& bank,
    uintptr_t effect,
    int eyeIndex,
    uint64_t poseFrame,
    uint64_t calibrationGeneration,
    const ResourcePair& liveResources);
bool Commit(
    Bank& bank,
    uintptr_t effect,
    int eyeIndex,
    uint64_t poseFrame,
    const ResourcePair& liveResources,
    bool clear);

} // namespace somavr::per_eye_post_effect_math
