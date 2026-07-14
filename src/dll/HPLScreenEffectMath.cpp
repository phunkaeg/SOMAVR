#include "HPLScreenEffectMath.h"

#include <cmath>

namespace somavr::screen_effect_math {
namespace {

bool IsFinite(const camera_math::Vector3& value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

} // namespace

bool ScaleCameraRelativePosition(
    const camera_math::Vector3& cameraPosition,
    const camera_math::Vector3& nativePosition,
    float distanceScale,
    camera_math::Vector3& scaledPosition)
{
    if (!IsFinite(cameraPosition) || !IsFinite(nativePosition)
        || !std::isfinite(distanceScale) || distanceScale <= 0.0f) {
        return false;
    }

    scaledPosition = {
        cameraPosition.x + (nativePosition.x - cameraPosition.x) * distanceScale,
        cameraPosition.y + (nativePosition.y - cameraPosition.y) * distanceScale,
        cameraPosition.z + (nativePosition.z - cameraPosition.z) * distanceScale,
    };
    return IsFinite(scaledPosition);
}

bool ScaleBillboardSize(const Size2& nativeSize, float distanceScale, Size2& scaledSize)
{
    if (!std::isfinite(nativeSize.width) || !std::isfinite(nativeSize.height)
        || nativeSize.width <= 0.0f || nativeSize.height <= 0.0f
        || !std::isfinite(distanceScale) || distanceScale <= 0.0f) {
        return false;
    }

    scaledSize = {nativeSize.width * distanceScale, nativeSize.height * distanceScale};
    return std::isfinite(scaledSize.width) && std::isfinite(scaledSize.height);
}

} // namespace somavr::screen_effect_math
