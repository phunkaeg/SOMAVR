#pragma once

#include <cstdint>

namespace somavr::dual_render_math {

enum class ReplayOutcome {
    SamePoseOppositeEye,
    ExpectedPairAbort,
    EyeSequenceMismatch,
};

constexpr uint64_t kWorldRenderMask = 1ull;
constexpr uint64_t kScreenGuiRenderMask = 2ull;
constexpr bool IsReplaySceneReady(bool worldPresent, bool loading)
{
    return worldPresent && !loading;
}

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
ReplayOutcome ResolveReplayOutcome(
    int firstEye,
    uint64_t firstPoseFrame,
    int secondEye,
    uint64_t secondPoseFrame,
    bool pairBaseRejected);

} // namespace somavr::dual_render_math
