#include "HPLTerminalMath.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace somavr::terminal_math {

bool ProjectAimToHudSurface(
    const camera_math::Vector3& headPosition,
    const camera_math::Quaternion& headOrientation,
    const camera_math::Vector3& aimPosition,
    const camera_math::Quaternion& aimOrientation,
    bool cylinder,
    float cylinderAngleDegrees,
    float distanceMeters,
    float verticalOffsetMeters,
    float widthMeters,
    float textureAspect,
    HudPointerPosition& position)
{
    position = {};
    if (!std::isfinite(headPosition.x)
        || !std::isfinite(headPosition.y)
        || !std::isfinite(headPosition.z)
        || !std::isfinite(aimPosition.x)
        || !std::isfinite(aimPosition.y)
        || !std::isfinite(aimPosition.z)
        || !std::isfinite(cylinderAngleDegrees)
        || !std::isfinite(distanceMeters)
        || !std::isfinite(verticalOffsetMeters)
        || !std::isfinite(widthMeters)
        || !std::isfinite(textureAspect)
        || distanceMeters <= 0.0f || widthMeters <= 0.0f
        || textureAspect <= 0.0f) {
        return false;
    }
    const camera_math::Quaternion inverseHead = camera_math::Conjugate(
        camera_math::Normalize(headOrientation));
    const camera_math::Quaternion relative = camera_math::Normalize(
        camera_math::Multiply(
            inverseHead,
            camera_math::Normalize(aimOrientation)));
    const camera_math::Vector3 origin = camera_math::RotateVector(
        inverseHead,
        {
            aimPosition.x - headPosition.x,
            aimPosition.y - headPosition.y,
            aimPosition.z - headPosition.z,
        });
    const camera_math::Vector3 direction = camera_math::RotateVector(
        relative, {0.0f, 0.0f, -1.0f});
    if (!std::isfinite(origin.x) || !std::isfinite(origin.y)
        || !std::isfinite(origin.z)
        || !std::isfinite(direction.x) || !std::isfinite(direction.y)
        || !std::isfinite(direction.z)) {
        return false;
    }

    const float heightMeters = widthMeters / textureAspect;
    float hitX = 0.0f;
    float hitY = 0.0f;
    float normalizedX = 0.5f;
    if (!cylinder) {
        if (direction.z >= -0.05f) return false;
        const float rayDistance = (-distanceMeters - origin.z) / direction.z;
        if (!std::isfinite(rayDistance) || rayDistance <= 0.0f) return false;
        hitX = origin.x + direction.x * rayDistance;
        hitY = origin.y + direction.y * rayDistance;
        normalizedX = 0.5f + hitX / widthMeters;
    } else {
        constexpr float kDegreesToRadians = 0.01745329251994329577f;
        const float angle = cylinderAngleDegrees * kDegreesToRadians;
        if (angle <= 0.01f || angle >= 6.27f) return false;
        const float radius = widthMeters / angle;
        const float axisZ = -distanceMeters + radius;
        const float a = direction.x * direction.x + direction.z * direction.z;
        const float relativeOriginZ = origin.z - axisZ;
        const float b = 2.0f * (
            origin.x * direction.x + relativeOriginZ * direction.z);
        const float c = origin.x * origin.x
            + relativeOriginZ * relativeOriginZ - radius * radius;
        const float discriminant = b * b - 4.0f * a * c;
        if (a <= 1.0e-6f || discriminant < 0.0f) return false;
        const float root = std::sqrt(discriminant);
        const float first = (-b - root) / (2.0f * a);
        const float second = (-b + root) / (2.0f * a);
        const float rayDistance = first > 0.0f && second > 0.0f
            ? std::min(first, second) : std::max(first, second);
        if (!std::isfinite(rayDistance) || rayDistance <= 0.0f) return false;
        hitX = origin.x + direction.x * rayDistance;
        hitY = origin.y + direction.y * rayDistance;
        const float hitZ = origin.z + direction.z * rayDistance;
        const float theta = std::atan2(hitX, axisZ - hitZ);
        normalizedX = 0.5f + theta / angle;
    }
    position.x = std::clamp(normalizedX, 0.0f, 1.0f);
    position.y = std::clamp(
        0.5f - (hitY - verticalOffsetMeters) / heightMeters,
        0.0f,
        1.0f);
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

bool RemapScissorToCaptureViewport(
    const ScissorRect& sourceScissor,
    const ScissorRect& sourceViewport,
    const ScissorRect& captureViewport,
    ScissorRect& captureScissor)
{
    captureScissor = sourceScissor;
    if (sourceViewport.width <= 0 || sourceViewport.height <= 0
        || captureViewport.width <= 0 || captureViewport.height <= 0
        || sourceScissor.width < 0 || sourceScissor.height < 0) {
        return false;
    }

    const double scaleX = static_cast<double>(captureViewport.width)
        / static_cast<double>(sourceViewport.width);
    const double scaleY = static_cast<double>(captureViewport.height)
        / static_cast<double>(sourceViewport.height);
    const double left = static_cast<double>(captureViewport.x)
        + (static_cast<double>(sourceScissor.x) - sourceViewport.x) * scaleX;
    const double bottom = static_cast<double>(captureViewport.y)
        + (static_cast<double>(sourceScissor.y) - sourceViewport.y) * scaleY;
    const double right = static_cast<double>(captureViewport.x)
        + (static_cast<double>(sourceScissor.x)
            + static_cast<double>(sourceScissor.width)
            - static_cast<double>(sourceViewport.x)) * scaleX;
    const double top = static_cast<double>(captureViewport.y)
        + (static_cast<double>(sourceScissor.y)
            + static_cast<double>(sourceScissor.height)
            - static_cast<double>(sourceViewport.y)) * scaleY;
    if (!std::isfinite(left) || !std::isfinite(bottom)
        || !std::isfinite(right) || !std::isfinite(top)
        || left < static_cast<double>(std::numeric_limits<int>::min())
        || bottom < static_cast<double>(std::numeric_limits<int>::min())
        || right > static_cast<double>(std::numeric_limits<int>::max())
        || top > static_cast<double>(std::numeric_limits<int>::max())
        || left > static_cast<double>(std::numeric_limits<int>::max())
        || bottom > static_cast<double>(std::numeric_limits<int>::max())
        || right < static_cast<double>(std::numeric_limits<int>::min())
        || top < static_cast<double>(std::numeric_limits<int>::min())
        || std::ceil(right) - std::floor(left) > std::numeric_limits<int>::max()
        || std::ceil(top) - std::floor(bottom) > std::numeric_limits<int>::max()) {
        return false;
    }

    captureScissor.x = static_cast<int>(std::floor(left));
    captureScissor.y = static_cast<int>(std::floor(bottom));
    const int mappedRight = static_cast<int>(std::ceil(right));
    const int mappedTop = static_cast<int>(std::ceil(top));
    captureScissor.width = sourceScissor.width == 0 ? 0 : mappedRight - captureScissor.x;
    captureScissor.height = sourceScissor.height == 0 ? 0 : mappedTop - captureScissor.y;
    return (captureScissor.x != sourceScissor.x
            || captureScissor.y != sourceScissor.y
            || captureScissor.width != sourceScissor.width
            || captureScissor.height != sourceScissor.height);
}

} // namespace somavr::terminal_math
