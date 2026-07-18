#include "HPLReadMath.h"

#include <cstddef>
#include <cmath>

namespace somavr::read_math {

bool BuildReadPresentationMatrix(
    const std::array<float, 16>& nativeMatrix,
    float objectScale,
    const camera_math::Quaternion* orientationOverride,
    std::array<float, 16>& output)
{
    if (!std::isfinite(objectScale) || objectScale <= 0.05f) {
        return false;
    }

    output = nativeMatrix;
    if (orientationOverride != nullptr) {
        const std::array<float, 16> rotation =
            camera_math::RotationMatrix(*orientationOverride);
        for (std::size_t row = 0; row < 3; ++row) {
            for (std::size_t column = 0; column < 3; ++column) {
                output[row * 4 + column] =
                    rotation[row * 4 + column] * objectScale;
            }
        }
    } else {
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
    }
    return std::isfinite(output[3])
        && std::isfinite(output[7])
        && std::isfinite(output[11]);
}

camera_math::Quaternion ResolveRelativeOrientation(
    const camera_math::Quaternion& anchorController,
    const camera_math::Quaternion& currentController,
    const camera_math::Quaternion& anchorObject)
{
    return camera_math::Normalize(camera_math::Multiply(
        camera_math::Multiply(
            camera_math::Normalize(currentController),
            camera_math::Conjugate(camera_math::Normalize(anchorController))),
        camera_math::Normalize(anchorObject)));
}

bool ScaleCameraRelativePosition(
    const camera_math::Vector3& cameraPosition,
    const camera_math::Vector3& nativePosition,
    float distanceScale,
    camera_math::Vector3& output)
{
    if (!std::isfinite(cameraPosition.x)
        || !std::isfinite(cameraPosition.y)
        || !std::isfinite(cameraPosition.z)
        || !std::isfinite(nativePosition.x)
        || !std::isfinite(nativePosition.y)
        || !std::isfinite(nativePosition.z)
        || !std::isfinite(distanceScale)
        || distanceScale < 0.5f
        || distanceScale > 4.0f) {
        return false;
    }
    output = {
        cameraPosition.x + (nativePosition.x - cameraPosition.x) * distanceScale,
        cameraPosition.y + (nativePosition.y - cameraPosition.y) * distanceScale,
        cameraPosition.z + (nativePosition.z - cameraPosition.z) * distanceScale,
    };
    return std::isfinite(output.x)
        && std::isfinite(output.y)
        && std::isfinite(output.z);
}

} // namespace somavr::read_math
