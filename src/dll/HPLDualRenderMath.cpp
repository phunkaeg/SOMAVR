#include "HPLDualRenderMath.h"

namespace somavr::dual_render_math {

uint64_t BuildReplayMask(uint64_t originalMask)
{
    return originalMask & ~kScreenGuiRenderMask;
}

bool IsReplayEligible(
    bool enabled,
    bool playerViewport,
    bool trackingEnabled,
    bool stereoEnabled,
    bool runtimeAvailable,
    int firstEye,
    uint64_t firstPoseFrame,
    uint64_t originalMask)
{
    return enabled
        && playerViewport
        && trackingEnabled
        && stereoEnabled
        && runtimeAvailable
        && (firstEye == 0 || firstEye == 1)
        && firstPoseFrame != 0
        && (originalMask & kWorldRenderMask) != 0;
}

bool IsSamePoseOppositeEye(
    int firstEye,
    uint64_t firstPoseFrame,
    int secondEye,
    uint64_t secondPoseFrame)
{
    return (firstEye == 0 || firstEye == 1)
        && secondEye == (firstEye ^ 1)
        && firstPoseFrame != 0
        && firstPoseFrame == secondPoseFrame;
}

ReplayOutcome ResolveReplayOutcome(
    int firstEye,
    uint64_t firstPoseFrame,
    int secondEye,
    uint64_t secondPoseFrame,
    bool pairBaseRejected)
{
    if (pairBaseRejected) return ReplayOutcome::ExpectedPairAbort;
    if (IsSamePoseOppositeEye(
            firstEye, firstPoseFrame, secondEye, secondPoseFrame)) {
        return ReplayOutcome::SamePoseOppositeEye;
    }
    return ReplayOutcome::EyeSequenceMismatch;
}

} // namespace somavr::dual_render_math
