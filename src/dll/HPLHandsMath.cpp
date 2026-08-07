#include "HPLHandsMath.h"

#include <cmath>

namespace somavr::hands_math {
namespace {

constexpr float kPi = 3.14159265358979323846f;

bool IsFinite(const camera_math::Vector3& value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool IsFinite(const std::array<float, 16>& matrix)
{
    for (float value : matrix) {
        if (!std::isfinite(value)) return false;
    }
    return true;
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
    const float lengthSquared = value.x * value.x + value.y * value.y + value.z * value.z;
    if (!std::isfinite(lengthSquared) || lengthSquared < 1.0e-8f) return false;
    const float inverseLength = 1.0f / std::sqrt(lengthSquared);
    value.x *= inverseLength;
    value.y *= inverseLength;
    value.z *= inverseLength;
    return true;
}

std::array<float, 16> RotationX(float angle)
{
    const float cosine = std::cos(angle);
    const float sine = std::sin(angle);
    return {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, cosine, -sine, 0.0f,
        0.0f, sine, cosine, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
}

std::array<float, 16> RotationY(float angle)
{
    const float cosine = std::cos(angle);
    const float sine = std::sin(angle);
    return {
        cosine, 0.0f, sine, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        -sine, 0.0f, cosine, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
}

std::array<float, 16> RotationZ(float angle)
{
    const float cosine = std::cos(angle);
    const float sine = std::sin(angle);
    return {
        cosine, -sine, 0.0f, 0.0f,
        sine, cosine, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
}

} // namespace

bool BuildControllerHandMatrix(
    const camera_math::Vector3& gripPosition,
    const camera_math::Vector3& gripForward,
    const camera_math::Vector3& gripUp,
    float scale,
    const HandRootCalibration& calibration,
    std::array<float, 16>& matrix)
{
    matrix = {};
    if (!IsFinite(gripPosition)
        || !IsFinite(gripForward)
        || !IsFinite(gripUp)
        || !IsFinite(calibration.positionOffset)
        || !IsFinite(calibration.rotationDegrees)
        || !std::isfinite(scale)
        || scale <= 0.0f) {
        return false;
    }

    camera_math::Vector3 forward = gripForward;
    camera_math::Vector3 upHint = gripUp;
    if (!Normalize(forward) || !Normalize(upHint)) return false;

    camera_math::Vector3 controllerRight = Cross(forward, upHint);
    if (!Normalize(controllerRight)) return false;
    camera_math::Vector3 up = Cross(controllerRight, forward);
    if (!Normalize(up)) return false;

    const camera_math::Vector3 handRight{
        -controllerRight.x,
        -controllerRight.y,
        -controllerRight.z,
    };
    std::array<float, 16> root = {
        handRight.x, up.x, forward.x, 0.0f,
        handRight.y, up.y, forward.y, 0.0f,
        handRight.z, up.z, forward.z, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };

    const float radians = kPi / 180.0f;
    const std::array<float, 16> correction = camera_math::MatrixMultiply(
        RotationZ(calibration.rotationDegrees.z * radians),
        camera_math::MatrixMultiply(
            RotationY(calibration.rotationDegrees.y * radians),
            RotationX(calibration.rotationDegrees.x * radians)));
    const std::array<float, 16> scaleMatrix = {
        scale, 0.0f, 0.0f, 0.0f,
        0.0f, scale, 0.0f, 0.0f,
        0.0f, 0.0f, scale, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
    matrix = camera_math::MatrixMultiply(
        root,
        camera_math::MatrixMultiply(correction, scaleMatrix));

    matrix[3] = gripPosition.x
        + controllerRight.x * calibration.positionOffset.x
        + up.x * calibration.positionOffset.y
        + forward.x * calibration.positionOffset.z;
    matrix[7] = gripPosition.y
        + controllerRight.y * calibration.positionOffset.x
        + up.y * calibration.positionOffset.y
        + forward.y * calibration.positionOffset.z;
    matrix[11] = gripPosition.z
        + controllerRight.z * calibration.positionOffset.x
        + up.z * calibration.positionOffset.y
        + forward.z * calibration.positionOffset.z;
    return true;
}

bool BuildControllerHudObjectMatrix(
    const camera_math::Vector3& gripPosition,
    const camera_math::Vector3& gripForward,
    const camera_math::Vector3& gripUp,
    float scale,
    const HudObjectCalibration& calibration,
    std::array<float, 16>& matrix)
{
    matrix = {};
    if (!IsFinite(gripPosition)
        || !IsFinite(gripForward)
        || !IsFinite(gripUp)
        || !IsFinite(calibration.positionOffset)
        || !IsFinite(calibration.rotationDegrees)
        || !std::isfinite(scale)
        || scale <= 0.0f) {
        return false;
    }

    camera_math::Vector3 forward = gripForward;
    camera_math::Vector3 upHint = gripUp;
    if (!Normalize(forward) || !Normalize(upHint)) return false;

    camera_math::Vector3 right = Cross(forward, upHint);
    if (!Normalize(right)) return false;
    camera_math::Vector3 up = Cross(right, forward);
    if (!Normalize(up)) return false;
    const camera_math::Vector3 backward{-forward.x, -forward.y, -forward.z};

    const std::array<float, 16> root = {
        right.x, up.x, backward.x, 0.0f,
        right.y, up.y, backward.y, 0.0f,
        right.z, up.z, backward.z, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
    const float radians = kPi / 180.0f;
    const std::array<float, 16> correction = camera_math::MatrixMultiply(
        RotationZ(calibration.rotationDegrees.z * radians),
        camera_math::MatrixMultiply(
            RotationY(calibration.rotationDegrees.y * radians),
            RotationX(calibration.rotationDegrees.x * radians)));
    const std::array<float, 16> scaleMatrix = {
        scale, 0.0f, 0.0f, 0.0f,
        0.0f, scale, 0.0f, 0.0f,
        0.0f, 0.0f, scale, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
    matrix = camera_math::MatrixMultiply(
        root,
        camera_math::MatrixMultiply(correction, scaleMatrix));
    matrix[3] = gripPosition.x
        + right.x * calibration.positionOffset.x
        + up.x * calibration.positionOffset.y
        + forward.x * calibration.positionOffset.z;
    matrix[7] = gripPosition.y
        + right.y * calibration.positionOffset.x
        + up.y * calibration.positionOffset.y
        + forward.y * calibration.positionOffset.z;
    matrix[11] = gripPosition.z
        + right.z * calibration.positionOffset.x
        + up.z * calibration.positionOffset.y
        + forward.z * calibration.positionOffset.z;
    return true;
}

bool InvertAffineMatrix(
    const std::array<float, 16>& matrix,
    std::array<float, 16>& inverse)
{
    inverse = {};
    if (!IsFinite(matrix)
        || std::fabs(matrix[12]) > 1.0e-4f
        || std::fabs(matrix[13]) > 1.0e-4f
        || std::fabs(matrix[14]) > 1.0e-4f
        || std::fabs(matrix[15] - 1.0f) > 1.0e-4f) {
        return false;
    }

    const float a00 = matrix[0];
    const float a01 = matrix[1];
    const float a02 = matrix[2];
    const float a10 = matrix[4];
    const float a11 = matrix[5];
    const float a12 = matrix[6];
    const float a20 = matrix[8];
    const float a21 = matrix[9];
    const float a22 = matrix[10];
    const float determinant =
        a00 * (a11 * a22 - a12 * a21)
        - a01 * (a10 * a22 - a12 * a20)
        + a02 * (a10 * a21 - a11 * a20);
    if (!std::isfinite(determinant) || std::fabs(determinant) < 1.0e-8f) {
        return false;
    }

    const float inverseDeterminant = 1.0f / determinant;
    inverse[0] = (a11 * a22 - a12 * a21) * inverseDeterminant;
    inverse[1] = (a02 * a21 - a01 * a22) * inverseDeterminant;
    inverse[2] = (a01 * a12 - a02 * a11) * inverseDeterminant;
    inverse[4] = (a12 * a20 - a10 * a22) * inverseDeterminant;
    inverse[5] = (a00 * a22 - a02 * a20) * inverseDeterminant;
    inverse[6] = (a02 * a10 - a00 * a12) * inverseDeterminant;
    inverse[8] = (a10 * a21 - a11 * a20) * inverseDeterminant;
    inverse[9] = (a01 * a20 - a00 * a21) * inverseDeterminant;
    inverse[10] = (a00 * a11 - a01 * a10) * inverseDeterminant;

    const float tx = matrix[3];
    const float ty = matrix[7];
    const float tz = matrix[11];
    inverse[3] = -(inverse[0] * tx + inverse[1] * ty + inverse[2] * tz);
    inverse[7] = -(inverse[4] * tx + inverse[5] * ty + inverse[6] * tz);
    inverse[11] = -(inverse[8] * tx + inverse[9] * ty + inverse[10] * tz);
    inverse[15] = 1.0f;
    return IsFinite(inverse);
}

bool BuildPostTransformForWorldTarget(
    const std::array<float, 16>& parentWorld,
    const std::array<float, 16>& animatedLocal,
    const std::array<float, 16>& desiredWorld,
    std::array<float, 16>& postTransform)
{
    postTransform = {};
    std::array<float, 16> inverseParent{};
    std::array<float, 16> inverseLocal{};
    if (!IsFinite(desiredWorld)
        || !InvertAffineMatrix(parentWorld, inverseParent)
        || !InvertAffineMatrix(animatedLocal, inverseLocal)) {
        return false;
    }

    postTransform = camera_math::MatrixMultiply(
        camera_math::MatrixMultiply(inverseParent, desiredWorld),
        inverseLocal);
    return IsFinite(postTransform);
}

bool NormalizeUniformScale(
    const std::array<float, 16>& source,
    float expectedScale,
    float targetScale,
    float tolerance,
    std::array<float, 16>& normalized)
{
    normalized = {};
    if (!IsFinite(source)
        || !std::isfinite(expectedScale)
        || !std::isfinite(targetScale)
        || !std::isfinite(tolerance)
        || expectedScale <= 0.0f
        || targetScale <= 0.0f
        || tolerance < 0.0f
        || std::fabs(source[12]) > 1.0e-4f
        || std::fabs(source[13]) > 1.0e-4f
        || std::fabs(source[14]) > 1.0e-4f
        || std::fabs(source[15] - 1.0f) > 1.0e-4f) {
        return false;
    }

    const auto columnLength = [&source](size_t column) {
        return std::sqrt(
            source[column] * source[column]
            + source[column + 4] * source[column + 4]
            + source[column + 8] * source[column + 8]);
    };
    const float scales[] = {columnLength(0), columnLength(1), columnLength(2)};
    for (float scale : scales) {
        if (!std::isfinite(scale)
            || scale <= 1.0e-6f
            || std::fabs(scale - expectedScale) > tolerance) {
            return false;
        }
    }

    normalized = source;
    for (size_t column = 0; column < 3; ++column) {
        const float multiplier = targetScale / scales[column];
        normalized[column] *= multiplier;
        normalized[column + 4] *= multiplier;
        normalized[column + 8] *= multiplier;
    }
    return IsFinite(normalized);
}

bool ApplyRootBasisScale(
    const std::array<float, 16>& scaleSource,
    const std::array<float, 16>& poseSource,
    std::array<float, 16>& reconciled)
{
    reconciled = {};
    if (!IsFinite(scaleSource) || !IsFinite(poseSource)) return false;

    reconciled = poseSource;
    for (size_t column = 0; column < 3; ++column) {
        const float targetX = scaleSource[column];
        const float targetY = scaleSource[column + 4];
        const float targetZ = scaleSource[column + 8];
        const float targetScale = std::sqrt(
            targetX * targetX + targetY * targetY + targetZ * targetZ);
        const float poseX = poseSource[column];
        const float poseY = poseSource[column + 4];
        const float poseZ = poseSource[column + 8];
        const float poseScale = std::sqrt(
            poseX * poseX + poseY * poseY + poseZ * poseZ);
        if (!std::isfinite(targetScale) || !std::isfinite(poseScale)
            || targetScale <= 1.0e-6f || poseScale <= 1.0e-6f) {
            reconciled = {};
            return false;
        }
        const float ratio = targetScale / poseScale;
        reconciled[column] *= ratio;
        reconciled[column + 4] *= ratio;
        reconciled[column + 8] *= ratio;
    }
    return IsFinite(reconciled);
}

bool BuildTrackedWristWorldMatrix(
    const camera_math::Vector3& trackedPosition,
    const camera_math::Vector3& trackedForward,
    const camera_math::Vector3& trackedUp,
    const camera_math::Quaternion& anchorControllerOrientation,
    const camera_math::Quaternion& anchorWristOrientation,
    const std::array<float, 16>& currentWristWorld,
    std::array<float, 16>& desiredWristWorld)
{
    const camera_math::Quaternion controllerToWrist = camera_math::Normalize(
        camera_math::Multiply(
            camera_math::Conjugate(anchorControllerOrientation),
            anchorWristOrientation));
    return BuildTrackedWristWorldMatrixFromOffset(
        trackedPosition,
        trackedForward,
        trackedUp,
        controllerToWrist,
        currentWristWorld,
        desiredWristWorld);
}

bool BuildPalmToWristOrientation(
    const std::array<float, 16>& wristWorld,
    const std::array<float, 16>& indexRootWorld,
    const std::array<float, 16>& middleRootWorld,
    const std::array<float, 16>& ringRootWorld,
    const std::array<float, 16>& pinkyRootWorld,
    camera_math::Quaternion& palmToWrist)
{
    if (!IsFinite(wristWorld) || !IsFinite(indexRootWorld)
        || !IsFinite(middleRootWorld) || !IsFinite(ringRootWorld)
        || !IsFinite(pinkyRootWorld)) {
        return false;
    }
    const camera_math::Vector3 wrist{
        wristWorld[3], wristWorld[7], wristWorld[11]};
    const camera_math::Vector3 fingerCenter{
        (indexRootWorld[3] + middleRootWorld[3] + ringRootWorld[3]
            + pinkyRootWorld[3]) * 0.25f,
        (indexRootWorld[7] + middleRootWorld[7] + ringRootWorld[7]
            + pinkyRootWorld[7]) * 0.25f,
        (indexRootWorld[11] + middleRootWorld[11] + ringRootWorld[11]
            + pinkyRootWorld[11]) * 0.25f,
    };
    camera_math::Vector3 palmForward{
        fingerCenter.x - wrist.x,
        fingerCenter.y - wrist.y,
        fingerCenter.z - wrist.z,
    };
    camera_math::Vector3 indexToPinky{
        indexRootWorld[3] - pinkyRootWorld[3],
        indexRootWorld[7] - pinkyRootWorld[7],
        indexRootWorld[11] - pinkyRootWorld[11],
    };
    if (!Normalize(palmForward) || !Normalize(indexToPinky)) return false;
    camera_math::Vector3 palmUp = Cross(indexToPinky, palmForward);
    if (!Normalize(palmUp)) return false;

    camera_math::Quaternion palmOrientation{};
    camera_math::Quaternion wristOrientation{};
    if (!camera_math::QuaternionFromForwardUp(
            palmForward, palmUp, palmOrientation)
        || !camera_math::QuaternionFromRotationMatrix(
            wristWorld, wristOrientation)) {
        return false;
    }
    palmToWrist = camera_math::Normalize(camera_math::Multiply(
        camera_math::Conjugate(palmOrientation), wristOrientation));
    return std::isfinite(palmToWrist.x) && std::isfinite(palmToWrist.y)
        && std::isfinite(palmToWrist.z) && std::isfinite(palmToWrist.w);
}

camera_math::Quaternion ApplyControllerForwardRoll(
    const camera_math::Quaternion& controllerToWrist,
    float rollDegrees)
{
    if (!std::isfinite(rollDegrees)) return camera_math::Normalize(controllerToWrist);
    constexpr float kDegreesToRadians = 0.01745329251994329577f;
    const float halfAngle = rollDegrees * kDegreesToRadians * 0.5f;
    const camera_math::Quaternion rollAroundForward{
        0.0f, 0.0f, -std::sin(halfAngle), std::cos(halfAngle)};
    return camera_math::Normalize(camera_math::Multiply(
        rollAroundForward,
        camera_math::Normalize(controllerToWrist)));
}

camera_math::Quaternion ApplyControllerLocalPitch(
    const camera_math::Quaternion& controllerToWrist,
    float pitchDegrees)
{
    if (!std::isfinite(pitchDegrees)) return camera_math::Normalize(controllerToWrist);
    constexpr float kDegreesToRadians = 0.01745329251994329577f;
    const float halfAngle = pitchDegrees * kDegreesToRadians * 0.5f;
    const camera_math::Quaternion pitchAroundRight{
        std::sin(halfAngle), 0.0f, 0.0f, std::cos(halfAngle)};
    return camera_math::Normalize(camera_math::Multiply(
        pitchAroundRight,
        camera_math::Normalize(controllerToWrist)));
}

bool BuildTrackedWristWorldMatrixFromOffset(
    const camera_math::Vector3& trackedPosition,
    const camera_math::Vector3& trackedForward,
    const camera_math::Vector3& trackedUp,
    const camera_math::Quaternion& controllerToWrist,
    const std::array<float, 16>& currentWristWorld,
    std::array<float, 16>& desiredWristWorld)
{
    desiredWristWorld = {};
    if (!IsFinite(trackedPosition) || !IsFinite(trackedForward)
        || !IsFinite(trackedUp) || !IsFinite(currentWristWorld)) {
        return false;
    }
    camera_math::Quaternion controllerOrientation{};
    if (!camera_math::QuaternionFromForwardUp(
            trackedForward, trackedUp, controllerOrientation)) {
        return false;
    }
    const camera_math::Quaternion desiredOrientation = camera_math::Normalize(
        camera_math::Multiply(controllerOrientation, controllerToWrist));
    desiredWristWorld = camera_math::RotationMatrix(desiredOrientation);

    for (size_t column = 0; column < 3; ++column) {
        const float x = currentWristWorld[column];
        const float y = currentWristWorld[column + 4];
        const float z = currentWristWorld[column + 8];
        const float scale = std::sqrt(x*x + y*y + z*z);
        if (!std::isfinite(scale) || scale <= 1.0e-6f) return false;
        desiredWristWorld[column] *= scale;
        desiredWristWorld[column + 4] *= scale;
        desiredWristWorld[column + 8] *= scale;
    }
    desiredWristWorld[3] = trackedPosition.x;
    desiredWristWorld[7] = trackedPosition.y;
    desiredWristWorld[11] = trackedPosition.z;
    return IsFinite(desiredWristWorld);
}

bool BuildBodyAnchoredRootMatrix(
    const std::array<float, 16>& sourceRoot,
    const camera_math::Vector3& sourceCameraPosition,
    const camera_math::Quaternion& sourceBodyYaw,
    const camera_math::Vector3& targetCameraPosition,
    const camera_math::Quaternion& targetBodyYaw,
    std::array<float, 16>& anchoredRoot)
{
    anchoredRoot = {};
    if (!IsFinite(sourceRoot)
        || !IsFinite(sourceCameraPosition)
        || !IsFinite(targetCameraPosition)) {
        return false;
    }
    const camera_math::Quaternion deltaYaw = camera_math::Multiply(
        targetBodyYaw, camera_math::Conjugate(sourceBodyYaw));
    anchoredRoot = camera_math::MatrixMultiply(
        camera_math::RotationMatrix(deltaYaw), sourceRoot);
    const camera_math::Vector3 relativeRoot{
        sourceRoot[3] - sourceCameraPosition.x,
        sourceRoot[7] - sourceCameraPosition.y,
        sourceRoot[11] - sourceCameraPosition.z,
    };
    const camera_math::Vector3 rotatedRoot = camera_math::RotateVector(
        deltaYaw, relativeRoot);
    anchoredRoot[3] = targetCameraPosition.x + rotatedRoot.x;
    anchoredRoot[7] = targetCameraPosition.y + rotatedRoot.y;
    anchoredRoot[11] = targetCameraPosition.z + rotatedRoot.z;
    return IsFinite(anchoredRoot);
}

} // namespace somavr::hands_math
