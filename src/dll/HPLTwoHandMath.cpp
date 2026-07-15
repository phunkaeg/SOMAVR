#include "HPLTwoHandMath.h"

#include <algorithm>
#include <cmath>

namespace somavr::two_hand_math {
namespace {

bool IsFinite(const camera_math::Vector3& value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

float Dot(const camera_math::Vector3& left, const camera_math::Vector3& right)
{
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

camera_math::Vector3 Cross(
    const camera_math::Vector3& left,
    const camera_math::Vector3& right)
{
    return {
        left.y * right.z - left.z * right.y,
        left.z * right.x - left.x * right.z,
        left.x * right.y - left.y * right.x,
    };
}

bool Normalize(camera_math::Vector3& value)
{
    const float lengthSquared = Dot(value, value);
    if (!std::isfinite(lengthSquared) || lengthSquared < 1.0e-8f) return false;
    const float inverseLength = 1.0f / std::sqrt(lengthSquared);
    value.x *= inverseLength;
    value.y *= inverseLength;
    value.z *= inverseLength;
    return true;
}

camera_math::Vector3 ProjectPerpendicular(
    const camera_math::Vector3& value,
    const camera_math::Vector3& normal)
{
    const float projection = Dot(value, normal);
    return {
        value.x - normal.x * projection,
        value.y - normal.y * projection,
        value.z - normal.z * projection,
    };
}

} // namespace

bool BuildTwoHandBasis(
    const camera_math::Vector3& dominantPosition,
    const camera_math::Vector3& dominantForward,
    const camera_math::Vector3& dominantUp,
    const camera_math::Vector3& supportPosition,
    float directionBlend,
    float minimumSeparation,
    float maximumSeparation,
    TwoHandBasis& basis)
{
    basis = {};
    if (!IsFinite(dominantPosition)
        || !IsFinite(dominantForward)
        || !IsFinite(dominantUp)
        || !IsFinite(supportPosition)
        || !std::isfinite(directionBlend)
        || !std::isfinite(minimumSeparation)
        || !std::isfinite(maximumSeparation)
        || minimumSeparation <= 0.0f
        || maximumSeparation <= minimumSeparation) {
        return false;
    }

    camera_math::Vector3 nativeForward = dominantForward;
    camera_math::Vector3 supportDirection{
        supportPosition.x - dominantPosition.x,
        supportPosition.y - dominantPosition.y,
        supportPosition.z - dominantPosition.z,
    };
    const float separationSquared = Dot(supportDirection, supportDirection);
    if (!std::isfinite(separationSquared)) return false;
    basis.separation = std::sqrt(std::max(separationSquared, 0.0f));
    if (basis.separation < minimumSeparation || basis.separation > maximumSeparation
        || !Normalize(nativeForward) || !Normalize(supportDirection)) {
        return false;
    }

    const float blend = std::clamp(directionBlend, 0.0f, 1.0f);
    basis.forward = {
        nativeForward.x * (1.0f - blend) + supportDirection.x * blend,
        nativeForward.y * (1.0f - blend) + supportDirection.y * blend,
        nativeForward.z * (1.0f - blend) + supportDirection.z * blend,
    };
    if (!Normalize(basis.forward)) return false;

    basis.up = ProjectPerpendicular(dominantUp, basis.forward);
    if (Normalize(basis.up)) return true;
    basis.up = ProjectPerpendicular({0.0f, 1.0f, 0.0f}, basis.forward);
    if (Normalize(basis.up)) return true;
    basis.up = ProjectPerpendicular({1.0f, 0.0f, 0.0f}, basis.forward);
    return Normalize(basis.up);
}

camera_math::Vector3 ResolveDirectionAngularTargetVelocity(
    const camera_math::Vector3& anchorDirection,
    const camera_math::Vector3& currentDirection,
    float gain,
    float sign,
    float maxAngularSpeed)
{
    if (!IsFinite(anchorDirection) || !IsFinite(currentDirection)
        || !std::isfinite(gain) || !std::isfinite(sign) || !std::isfinite(maxAngularSpeed)
        || gain <= 0.0f || std::fabs(sign) < 0.001f || maxAngularSpeed <= 0.0f) {
        return {};
    }

    camera_math::Vector3 anchor = anchorDirection;
    camera_math::Vector3 current = currentDirection;
    if (!Normalize(anchor) || !Normalize(current)) return {};

    const float cosine = std::clamp(Dot(anchor, current), -1.0f, 1.0f);
    camera_math::Vector3 axis = Cross(anchor, current);
    if (!Normalize(axis)) {
        if (cosine > 0.99999f) return {};
        axis = Cross(anchor, {0.0f, 1.0f, 0.0f});
        if (!Normalize(axis)) {
            axis = Cross(anchor, {1.0f, 0.0f, 0.0f});
        }
        if (!Normalize(axis)) return {};
    }

    const float angle = std::acos(cosine);
    const float speed = std::min(angle * gain, maxAngularSpeed) * sign;
    const camera_math::Vector3 result{axis.x * speed, axis.y * speed, axis.z * speed};
    return IsFinite(result) ? result : camera_math::Vector3{};
}

} // namespace somavr::two_hand_math
