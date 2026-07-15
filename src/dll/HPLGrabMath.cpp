#include "HPLGrabMath.h"

#include <algorithm>
#include <cmath>

namespace somavr::grab_math {

camera_math::Vector3 ResolveAngularTargetVelocity(
    const camera_math::Quaternion& anchor,
    const camera_math::Quaternion& current,
    float gain,
    float sign,
    float maxAngularSpeed)
{
    using camera_math::Conjugate;
    using camera_math::Multiply;
    using camera_math::Normalize;
    using camera_math::Quaternion;
    using camera_math::Vector3;

    if (!std::isfinite(gain) || !std::isfinite(sign) || !std::isfinite(maxAngularSpeed)
        || gain <= 0.0f || std::fabs(sign) < 0.001f || maxAngularSpeed <= 0.0f) {
        return {};
    }

    Quaternion delta = Normalize(Multiply(Normalize(current), Conjugate(Normalize(anchor))));
    if (delta.w < 0.0f) {
        delta.x = -delta.x;
        delta.y = -delta.y;
        delta.z = -delta.z;
        delta.w = -delta.w;
    }

    const float halfAngleSin = std::sqrt(std::max(
        0.0f,
        delta.x * delta.x + delta.y * delta.y + delta.z * delta.z));
    if (!std::isfinite(halfAngleSin) || halfAngleSin < 1.0e-6f) {
        return {};
    }

    const float angle = 2.0f * std::atan2(halfAngleSin, std::clamp(delta.w, -1.0f, 1.0f));
    const float speed = std::min(angle * gain, maxAngularSpeed) * sign;
    const float axisScale = speed / halfAngleSin;
    const Vector3 result{delta.x * axisScale, delta.y * axisScale, delta.z * axisScale};
    if (!std::isfinite(result.x) || !std::isfinite(result.y) || !std::isfinite(result.z)) {
        return {};
    }
    return result;
}

float ResolveHingeAngularVelocity(
    const camera_math::Vector3& pivot,
    const camera_math::Vector3& point,
    const camera_math::Vector3& pointVelocity,
    const camera_math::Vector3& pin,
    float gain,
    float maxAngularSpeed)
{
    if (!std::isfinite(gain) || gain <= 0.0f
        || !std::isfinite(maxAngularSpeed) || maxAngularSpeed <= 0.0f) {
        return 0.0f;
    }
    const float pinLengthSquared = pin.x * pin.x + pin.y * pin.y + pin.z * pin.z;
    if (!std::isfinite(pinLengthSquared) || pinLengthSquared < 0.25f) {
        return 0.0f;
    }
    const float inversePinLength = 1.0f / std::sqrt(pinLengthSquared);
    const camera_math::Vector3 axis{
        pin.x * inversePinLength,
        pin.y * inversePinLength,
        pin.z * inversePinLength,
    };
    const camera_math::Vector3 pivotToPoint{
        point.x - pivot.x,
        point.y - pivot.y,
        point.z - pivot.z,
    };
    const float axial = pivotToPoint.x * axis.x
        + pivotToPoint.y * axis.y
        + pivotToPoint.z * axis.z;
    const camera_math::Vector3 radius{
        pivotToPoint.x - axis.x * axial,
        pivotToPoint.y - axis.y * axial,
        pivotToPoint.z - axis.z * axial,
    };
    const float radiusSquared = radius.x * radius.x
        + radius.y * radius.y
        + radius.z * radius.z;
    if (!std::isfinite(radiusSquared) || radiusSquared < 0.0025f) {
        return 0.0f;
    }
    const camera_math::Vector3 cross{
        radius.y * pointVelocity.z - radius.z * pointVelocity.y,
        radius.z * pointVelocity.x - radius.x * pointVelocity.z,
        radius.x * pointVelocity.y - radius.y * pointVelocity.x,
    };
    const float angularVelocity = (cross.x * axis.x + cross.y * axis.y + cross.z * axis.z)
        / radiusSquared * gain;
    if (!std::isfinite(angularVelocity)) return 0.0f;
    return std::clamp(angularVelocity, -maxAngularSpeed, maxAngularSpeed);
}

} // namespace somavr::grab_math
