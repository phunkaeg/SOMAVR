#include "HPLTerminalMath.h"

#include <algorithm>
#include <cmath>

namespace somavr::terminal_math {

bool ProjectAimToHudSurface(
    const camera_math::Quaternion& headOrientation,
    const camera_math::Quaternion& aimOrientation,
    bool cylinder,
    float cylinderAngleDegrees,
    float distanceMeters,
    float widthMeters,
    float textureAspect,
    HudPointerPosition& position)
{
    position = {};
    if (!std::isfinite(cylinderAngleDegrees)
        || !std::isfinite(distanceMeters)
        || !std::isfinite(widthMeters)
        || !std::isfinite(textureAspect)
        || distanceMeters <= 0.0f || widthMeters <= 0.0f
        || textureAspect <= 0.0f) {
        return false;
    }
    const camera_math::Quaternion relative = camera_math::Normalize(
        camera_math::Multiply(
            camera_math::Conjugate(camera_math::Normalize(headOrientation)),
            camera_math::Normalize(aimOrientation)));
    const camera_math::Vector3 direction = camera_math::RotateVector(
        relative, {0.0f, 0.0f, -1.0f});
    if (!std::isfinite(direction.x) || !std::isfinite(direction.y)
        || !std::isfinite(direction.z) || direction.z >= -0.05f) {
        return false;
    }

    const float heightMeters = widthMeters / textureAspect;
    float hitX = 0.0f;
    float hitY = 0.0f;
    float normalizedX = 0.5f;
    if (!cylinder) {
        const float rayDistance = -distanceMeters / direction.z;
        hitX = direction.x * rayDistance;
        hitY = direction.y * rayDistance;
        normalizedX = 0.5f + hitX / widthMeters;
    } else {
        constexpr float kDegreesToRadians = 0.01745329251994329577f;
        const float angle = cylinderAngleDegrees * kDegreesToRadians;
        if (angle <= 0.01f || angle >= 6.27f) return false;
        const float radius = widthMeters / angle;
        const float axisZ = -distanceMeters + radius;
        const float a = direction.x * direction.x + direction.z * direction.z;
        const float b = -2.0f * direction.z * axisZ;
        const float c = axisZ * axisZ - radius * radius;
        const float discriminant = b * b - 4.0f * a * c;
        if (a <= 1.0e-6f || discriminant < 0.0f) return false;
        const float root = std::sqrt(discriminant);
        const float first = (-b - root) / (2.0f * a);
        const float second = (-b + root) / (2.0f * a);
        const float rayDistance = std::max(first, second);
        if (!std::isfinite(rayDistance) || rayDistance <= 0.0f) return false;
        hitX = direction.x * rayDistance;
        hitY = direction.y * rayDistance;
        const float hitZ = direction.z * rayDistance;
        const float theta = std::atan2(hitX, axisZ - hitZ);
        normalizedX = 0.5f + theta / angle;
    }
    position.x = std::clamp(normalizedX, 0.0f, 1.0f);
    position.y = std::clamp(0.5f - hitY / heightMeters, 0.0f, 1.0f);
    return std::isfinite(position.x) && std::isfinite(position.y);
}

ClearPolicyResult ResolveRetainedSurfaceClear(
    uint32_t requestedMask,
    uint32_t colorBufferBit,
    bool suppressionActive,
    bool ownerThread,
    bool targetFramebuffer)
{
    ClearPolicyResult result{requestedMask, false};
    if (!suppressionActive
        || !ownerThread
        || !targetFramebuffer
        || (requestedMask & colorBufferBit) == 0) {
        return result;
    }

    result.forwardedMask = requestedMask & ~colorBufferBit;
    result.colorSuppressed = true;
    return result;
}

bool ShouldFallbackToLiveTerminalFrames(
    uint32_t completedRetainedSamplesWithoutClear,
    uint32_t sampleThreshold,
    bool colorClearObserved,
    bool scissorRepairObserved)
{
    return !colorClearObserved
        && !scissorRepairObserved
        && sampleThreshold > 0
        && completedRetainedSamplesWithoutClear >= sampleThreshold;
}

} // namespace somavr::terminal_math
