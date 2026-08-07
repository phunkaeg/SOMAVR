#pragma once

#include "HPLCameraMath.h"

#include <cstdint>

namespace somavr::terminal_math {

struct ClearPolicyResult {
    uint32_t forwardedMask = 0;
    bool colorSuppressed = false;
};

struct HudPointerPosition {
    float x = 0.5f;
    float y = 0.5f;
};

bool ProjectAimToHudSurface(
    const camera_math::Quaternion& headOrientation,
    const camera_math::Quaternion& aimOrientation,
    bool cylinder,
    float cylinderAngleDegrees,
    float distanceMeters,
    float widthMeters,
    float textureAspect,
    HudPointerPosition& position);

ClearPolicyResult ResolveRetainedSurfaceClear(
    uint32_t requestedMask,
    uint32_t colorBufferBit,
    bool suppressionActive,
    bool ownerThread,
    bool targetFramebuffer);

bool ShouldFallbackToLiveTerminalFrames(
    uint32_t completedRetainedSamplesWithoutClear,
    uint32_t sampleThreshold,
    bool colorClearObserved,
    bool scissorRepairObserved = false);

} // namespace somavr::terminal_math
