#include "HPLReadMath.h"

#include <cstddef>
#include <cmath>

namespace somavr::read_math {

bool BuildReadPresentationMatrix(
    const std::array<float, 16>& nativeMatrix,
    const camera_math::Vector3& cameraPosition,
    float targetDistance,
    float objectScale,
    std::array<float, 16>& output)
{
    if (!std::isfinite(targetDistance) || targetDistance <= 0.05f
        || !std::isfinite(objectScale) || objectScale <= 0.05f) {
        return false;
    }

    const camera_math::Vector3 offset{
        nativeMatrix[3] - cameraPosition.x,
        nativeMatrix[7] - cameraPosition.y,
        nativeMatrix[11] - cameraPosition.z,
    };
    const float distanceSquared = offset.x * offset.x
        + offset.y * offset.y + offset.z * offset.z;
    if (!std::isfinite(distanceSquared) || distanceSquared < 1.0e-6f) return false;
    const float inverseDistance = 1.0f / std::sqrt(distanceSquared);

    output = nativeMatrix;
    for (std::size_t column = 0; column < 3; ++column) {
        const float lengthSquared = nativeMatrix[column] * nativeMatrix[column]
            + nativeMatrix[column + 4] * nativeMatrix[column + 4]
            + nativeMatrix[column + 8] * nativeMatrix[column + 8];
        if (!std::isfinite(lengthSquared) || lengthSquared < 1.0e-6f) return false;
        const float scale = objectScale / std::sqrt(lengthSquared);
        output[column] = nativeMatrix[column] * scale;
        output[column + 4] = nativeMatrix[column + 4] * scale;
        output[column + 8] = nativeMatrix[column + 8] * scale;
    }
    output[3] = cameraPosition.x + offset.x * inverseDistance * targetDistance;
    output[7] = cameraPosition.y + offset.y * inverseDistance * targetDistance;
    output[11] = cameraPosition.z + offset.z * inverseDistance * targetDistance;
    return std::isfinite(output[3]) && std::isfinite(output[7]) && std::isfinite(output[11]);
}

} // namespace somavr::read_math
