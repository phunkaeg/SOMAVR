#include "HPLPerEyePostEffectMath.h"

#include <cstddef>

namespace somavr::per_eye_post_effect_math {
namespace {

bool SameResources(const ResourcePair& left, const ResourcePair& right)
{
    return left.framebuffer == right.framebuffer && left.texture == right.texture;
}

} // namespace

bool IsValid(const ResourcePair& resources)
{
    return resources.framebuffer != 0 && resources.texture != 0;
}

bool Initialize(
    Bank& bank,
    uintptr_t effect,
    const ResourcePair& primary,
    bool primaryClear,
    const ResourcePair& secondary,
    uint64_t calibrationGeneration)
{
    bank = {};
    if (effect == 0 || !IsValid(primary) || !IsValid(secondary)
        || SameResources(primary, secondary)) {
        return false;
    }
    bank.initialized = true;
    bank.effect = effect;
    bank.calibrationGeneration = calibrationGeneration;
    bank.eyes[0] = {primary, primaryClear, 0};
    bank.eyes[1] = {secondary, true, 0};
    bank.boundEye = 0;
    return true;
}

PrepareResult Prepare(
    Bank& bank,
    uintptr_t effect,
    int eyeIndex,
    uint64_t poseFrame,
    uint64_t calibrationGeneration,
    const ResourcePair& liveResources)
{
    PrepareResult result;
    if (!bank.initialized || bank.effect != effect || eyeIndex < 0 || eyeIndex > 1
        || poseFrame == 0 || bank.pendingEye != -1 || bank.boundEye < 0 || bank.boundEye > 1
        || !SameResources(liveResources, bank.eyes[static_cast<size_t>(bank.boundEye)].resources)) {
        return result;
    }

    bool reset = calibrationGeneration != bank.calibrationGeneration;
    const uint64_t lastPoseFrame = bank.eyes[static_cast<size_t>(eyeIndex)].lastPoseFrame;
    if (lastPoseFrame != 0
        && (poseFrame <= lastPoseFrame || poseFrame - lastPoseFrame > kMaxPoseFrameGap)) {
        reset = true;
    }
    if (reset) {
        bank.calibrationGeneration = calibrationGeneration;
        for (EyeState& eye : bank.eyes) {
            eye.clear = true;
            eye.lastPoseFrame = 0;
        }
    }

    bank.pendingEye = eyeIndex;
    bank.pendingPoseFrame = poseFrame;
    result.valid = true;
    result.reset = reset;
    result.resources = bank.eyes[static_cast<size_t>(eyeIndex)].resources;
    result.clear = bank.eyes[static_cast<size_t>(eyeIndex)].clear;
    return result;
}

bool Commit(
    Bank& bank,
    uintptr_t effect,
    int eyeIndex,
    uint64_t poseFrame,
    const ResourcePair& liveResources,
    bool clear)
{
    if (!bank.initialized || bank.effect != effect || eyeIndex < 0 || eyeIndex > 1
        || bank.pendingEye != eyeIndex || bank.pendingPoseFrame != poseFrame
        || !SameResources(liveResources, bank.eyes[static_cast<size_t>(eyeIndex)].resources)) {
        bank.pendingEye = -1;
        bank.pendingPoseFrame = 0;
        return false;
    }
    EyeState& eye = bank.eyes[static_cast<size_t>(eyeIndex)];
    eye.clear = clear;
    eye.lastPoseFrame = poseFrame;
    bank.boundEye = eyeIndex;
    bank.pendingEye = -1;
    bank.pendingPoseFrame = 0;
    return true;
}

} // namespace somavr::per_eye_post_effect_math
