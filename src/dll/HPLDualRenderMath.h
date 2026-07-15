#pragma once

#include <cstdint>

namespace somavr::dual_render_math {

constexpr uint64_t kWorldRenderMask = 1ull;
constexpr uint64_t kScreenGuiRenderMask = 2ull;

uint64_t BuildReplayMask(uint64_t originalMask);
bool IsReplayEligible(
    bool enabled,
    bool playerViewport,
    bool trackingEnabled,
    bool stereoEnabled,
    bool runtimeAvailable,
    int firstEye,
    uint64_t firstPoseFrame,
    uint64_t originalMask);
bool IsSamePoseOppositeEye(
    int firstEye,
    uint64_t firstPoseFrame,
    int secondEye,
    uint64_t secondPoseFrame);

} // namespace somavr::dual_render_math
