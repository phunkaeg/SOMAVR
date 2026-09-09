#include "HPLReadMath.h"

#include <cstddef>
#include <cmath>

namespace somavr::read_math {

bool BuildReadPresentationMatrix(
    const std::array<float, 16>& nativeMatrix,
    float scaleMultiplier,
    const camera_math::Quaternion* orientationOverride,
    std::array<float, 16>& output)
{
    if (!std::isfinite(scaleMultiplier) || scaleMultiplier <= 0.05f) {
        return false;
    }

    output = nativeMatrix;
    std::array<float, 3> nativeScales{};
    for (std::size_t column = 0; column < 3; ++column) {
        const float lengthSquared = nativeMatrix[column] * nativeMatrix[column]
            + nativeMatrix[column + 4] * nativeMatrix[column + 4]
            + nativeMatrix[column + 8] * nativeMatrix[column + 8];
        if (!std::isfinite(lengthSquared) || lengthSquared < 1.0e-6f) return false;
        nativeScales[column] = std::sqrt(lengthSquared);
    }
    if (orientationOverride != nullptr) {
        const std::array<float, 16> rotation =
            camera_math::RotationMatrix(*orientationOverride);
        for (std::size_t row = 0; row < 3; ++row) {
            for (std::size_t column = 0; column < 3; ++column) {
                output[row * 4 + column] =
                    rotation[row * 4 + column]
                    * nativeScales[column] * scaleMultiplier;
            }
        }
    } else {
        for (std::size_t column = 0; column < 3; ++column) {
            output[column] = nativeMatrix[column] * scaleMultiplier;
            output[column + 4] = nativeMatrix[column + 4] * scaleMultiplier;
            output[column + 8] = nativeMatrix[column + 8] * scaleMultiplier;
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

bool BuildStablePresentationOffset(
    const camera_math::Vector3& viewForward,
    float sourceDistance,
    float distanceScale,
    camera_math::Vector3& output)
{
    output = {};
    if (!std::isfinite(viewForward.x)
        || !std::isfinite(viewForward.y)
        || !std::isfinite(viewForward.z)
        || !std::isfinite(sourceDistance)
        || !std::isfinite(distanceScale)
        || sourceDistance <= 0.05f
        || distanceScale < 0.5f
        || distanceScale > 4.0f) {
        return false;
    }
    const float forwardLength = std::sqrt(
        viewForward.x * viewForward.x
        + viewForward.y * viewForward.y
        + viewForward.z * viewForward.z);
    if (!std::isfinite(forwardLength) || forwardLength <= 1.0e-5f) return false;
    const float distance = sourceDistance * distanceScale;
    output = {
        viewForward.x / forwardLength * distance,
        viewForward.y / forwardLength * distance,
        viewForward.z / forwardLength * distance,
    };
    return std::isfinite(output.x)
        && std::isfinite(output.y)
        && std::isfinite(output.z);
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

bool ResolveLatchedCameraRelativePosition(
    const camera_math::Vector3& sourceCameraPosition,
    const camera_math::Vector3& sourceObjectPosition,
    const camera_math::Vector3& currentCameraPosition,
    float distanceScale,
    camera_math::Vector3& output)
{
    const camera_math::Vector3 translatedNativePosition{
        currentCameraPosition.x + sourceObjectPosition.x - sourceCameraPosition.x,
        currentCameraPosition.y + sourceObjectPosition.y - sourceCameraPosition.y,
        currentCameraPosition.z + sourceObjectPosition.z - sourceCameraPosition.z,
    };
    return ScaleCameraRelativePosition(
        currentCameraPosition,
        translatedNativePosition,
        distanceScale,
        output);
}

} // namespace somavr::read_math
