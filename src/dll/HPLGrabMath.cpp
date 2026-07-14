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

} // namespace somavr::grab_math
