#pragma once

#include "HPLCameraMath.h"

#include <array>

namespace somavr::arm_ik_math {

struct TwoBoneSolution {
    camera_math::Vector3 elbow{};
    camera_math::Vector3 wrist{};
    float requestedDistance = 0.0f;
    float solvedDistance = 0.0f;
    bool reachClamped = false;
};

struct ShoulderReachSolution {
    camera_math::Vector3 target{};
    camera_math::Vector3 offsetWorld{};
    camera_math::Vector3 offsetLocal{};
    float reachRatio = 0.0f;
    float blend = 0.0f;
    bool applied = false;
};

struct ElbowPoleSolution {
    camera_math::Vector3 pole{};
    camera_math::Vector3 directionWorld{};
    camera_math::Vector3 directionLocal{};
    float crossMagnitude = 0.0f;
    float singularityBlend = 0.0f;
    bool historyUsed = false;
    bool crossFallbackUsed = false;
    bool nativeFallbackUsed = false;
    bool swivelLimited = false;
};

bool ComputeReachShoulderTarget(
    const camera_math::Vector3& shoulder,
    const camera_math::Vector3& wristTarget,
    const camera_math::Vector3& bodyForward,
    const camera_math::Vector3& worldUp,
    bool leftHand,
    float totalArmLength,
    float reachStartFraction,
    float reachFullFraction,
    float maximumOffset,
    float smoothing,
    const camera_math::Vector3* previousOffsetLocal,
    ShoulderReachSolution& solution);

bool ComputeErgonomicElbowPole(
    const camera_math::Vector3& shoulder,
    const camera_math::Vector3& wristTarget,
    const camera_math::Vector3& nativeElbow,
    const camera_math::Vector3& bodyForward,
    const camera_math::Vector3& worldUp,
    bool leftHand,
    float downwardWeight,
    float poleDistance,
    float maximumSwivelDegrees,
    const camera_math::Vector3* previousDirectionLocal,
    ElbowPoleSolution& solution);

bool SolveTwoBone(
    const camera_math::Vector3& shoulder,
    const camera_math::Vector3& target,
    const camera_math::Vector3& pole,
    float upperLength,
    float lowerLength,
    float maxReachFraction,
    TwoBoneSolution& solution);

bool RotateWorldMatrixToward(
    const std::array<float, 16>& currentWorld,
    const camera_math::Vector3& currentEndpoint,
    const camera_math::Vector3& desiredEndpoint,
    float blend,
    std::array<float, 16>& desiredWorld);

} // namespace somavr::arm_ik_math
