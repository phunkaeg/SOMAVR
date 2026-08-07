#pragma once

#include "HPLCameraMath.h"

#include <array>

namespace somavr::hands_math {

struct HandRootCalibration {
    camera_math::Vector3 positionOffset{};
    camera_math::Vector3 rotationDegrees{};
};

struct HudObjectCalibration {
    camera_math::Vector3 positionOffset;
    camera_math::Vector3 rotationDegrees;
};

bool BuildControllerHandMatrix(
    const camera_math::Vector3& gripPosition,
    const camera_math::Vector3& gripForward,
    const camera_math::Vector3& gripUp,
    float scale,
    const HandRootCalibration& calibration,
    std::array<float, 16>& matrix);

bool BuildControllerHudObjectMatrix(
    const camera_math::Vector3& gripPosition,
    const camera_math::Vector3& gripForward,
    const camera_math::Vector3& gripUp,
    float scale,
    const HudObjectCalibration& calibration,
    std::array<float, 16>& matrix);

bool InvertAffineMatrix(
    const std::array<float, 16>& matrix,
    std::array<float, 16>& inverse);

bool BuildPostTransformForWorldTarget(
    const std::array<float, 16>& parentWorld,
    const std::array<float, 16>& animatedLocal,
    const std::array<float, 16>& desiredWorld,
    std::array<float, 16>& postTransform);

bool NormalizeUniformScale(
    const std::array<float, 16>& source,
    float expectedScale,
    float targetScale,
    float tolerance,
    std::array<float, 16>& normalized);

bool ApplyRootBasisScale(
    const std::array<float, 16>& scaleSource,
    const std::array<float, 16>& poseSource,
    std::array<float, 16>& reconciled);

bool BuildTrackedWristWorldMatrix(
    const camera_math::Vector3& trackedPosition,
    const camera_math::Vector3& trackedForward,
    const camera_math::Vector3& trackedUp,
    const camera_math::Quaternion& anchorControllerOrientation,
    const camera_math::Quaternion& anchorWristOrientation,
    const std::array<float, 16>& currentWristWorld,
    std::array<float, 16>& desiredWristWorld);

bool BuildPalmToWristOrientation(
    const std::array<float, 16>& wristWorld,
    const std::array<float, 16>& indexRootWorld,
    const std::array<float, 16>& middleRootWorld,
    const std::array<float, 16>& ringRootWorld,
    const std::array<float, 16>& pinkyRootWorld,
    camera_math::Quaternion& palmToWrist);

camera_math::Quaternion ApplyControllerForwardRoll(
    const camera_math::Quaternion& controllerToWrist,
    float rollDegrees);

camera_math::Quaternion ApplyControllerLocalPitch(
    const camera_math::Quaternion& controllerToWrist,
    float pitchDegrees);

bool BuildTrackedWristWorldMatrixFromOffset(
    const camera_math::Vector3& trackedPosition,
    const camera_math::Vector3& trackedForward,
    const camera_math::Vector3& trackedUp,
    const camera_math::Quaternion& controllerToWrist,
    const std::array<float, 16>& currentWristWorld,
    std::array<float, 16>& desiredWristWorld);

bool BuildBodyAnchoredRootMatrix(
    const std::array<float, 16>& sourceRoot,
    const camera_math::Vector3& sourceCameraPosition,
    const camera_math::Quaternion& sourceBodyYaw,
    const camera_math::Vector3& targetCameraPosition,
    const camera_math::Quaternion& targetBodyYaw,
    std::array<float, 16>& anchoredRoot);

} // namespace somavr::hands_math
