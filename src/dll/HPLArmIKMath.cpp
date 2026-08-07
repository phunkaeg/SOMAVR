#include "HPLArmIKMath.h"

#include <algorithm>
#include <cmath>

namespace somavr::arm_ik_math {
namespace {

constexpr float kEpsilon = 1.0e-6f;

camera_math::Vector3 Add(const camera_math::Vector3& a, const camera_math::Vector3& b)
{
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

camera_math::Vector3 Subtract(const camera_math::Vector3& a, const camera_math::Vector3& b)
{
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

camera_math::Vector3 Scale(const camera_math::Vector3& value, float scale)
{
    return {value.x * scale, value.y * scale, value.z * scale};
}

float Dot(const camera_math::Vector3& a, const camera_math::Vector3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

camera_math::Vector3 Cross(const camera_math::Vector3& a, const camera_math::Vector3& b)
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

float Length(const camera_math::Vector3& value)
{
    return std::sqrt(std::max(Dot(value, value), 0.0f));
}

bool Normalize(camera_math::Vector3& value)
{
    const float length = Length(value);
    if (!std::isfinite(length) || length <= kEpsilon) return false;
    value = Scale(value, 1.0f / length);
    return true;
}

bool IsFinite(const camera_math::Vector3& value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

camera_math::Vector3 Lerp(
    const camera_math::Vector3& a,
    const camera_math::Vector3& b,
    float t)
{
    return Add(a, Scale(Subtract(b, a), t));
}

float Smoothstep(float edge0, float edge1, float value)
{
    if (!(edge1 > edge0)) return value >= edge1 ? 1.0f : 0.0f;
    const float t = std::clamp((value - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

struct TorsoBasis {
    camera_math::Vector3 forward{};
    camera_math::Vector3 right{};
    camera_math::Vector3 up{};
};

bool BuildTorsoBasis(
    const camera_math::Vector3& bodyForward,
    const camera_math::Vector3& worldUp,
    TorsoBasis& basis)
{
    basis = {};
    basis.up = worldUp;
    if (!Normalize(basis.up)) return false;
    basis.forward = Subtract(
        bodyForward, Scale(basis.up, Dot(bodyForward, basis.up)));
    if (!Normalize(basis.forward)) return false;
    basis.right = Cross(basis.forward, basis.up);
    return Normalize(basis.right);
}

camera_math::Vector3 ToLocal(
    const TorsoBasis& basis,
    const camera_math::Vector3& world)
{
    return {
        Dot(world, basis.right),
        Dot(world, basis.up),
        Dot(world, basis.forward),
    };
}

camera_math::Vector3 ToWorld(
    const TorsoBasis& basis,
    const camera_math::Vector3& local)
{
    return Add(
        Add(Scale(basis.right, local.x), Scale(basis.up, local.y)),
        Scale(basis.forward, local.z));
}

bool ProjectDirection(
    const camera_math::Vector3& direction,
    const camera_math::Vector3& axis,
    camera_math::Vector3& projected)
{
    projected = Subtract(direction, Scale(axis, Dot(direction, axis)));
    return Normalize(projected);
}

} // namespace

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
    ShoulderReachSolution& solution)
{
    solution = {};
    solution.target = shoulder;
    if (!IsFinite(shoulder) || !IsFinite(wristTarget)
        || !IsFinite(bodyForward) || !IsFinite(worldUp)
        || !std::isfinite(totalArmLength) || totalArmLength <= kEpsilon
        || !std::isfinite(reachStartFraction)
        || !std::isfinite(reachFullFraction)
        || reachStartFraction < 0.0f || reachStartFraction >= reachFullFraction
        || !std::isfinite(maximumOffset) || maximumOffset < 0.0f
        || !std::isfinite(smoothing) || smoothing < 0.0f || smoothing > 1.0f
        || (previousOffsetLocal != nullptr && !IsFinite(*previousOffsetLocal))) {
        return false;
    }

    TorsoBasis basis;
    if (!BuildTorsoBasis(bodyForward, worldUp, basis)) return false;
    camera_math::Vector3 toWrist = Subtract(wristTarget, shoulder);
    const float distance = Length(toWrist);
    if (!Normalize(toWrist)) return false;

    solution.reachRatio = distance / totalArmLength;
    solution.blend = Smoothstep(
        reachStartFraction, reachFullFraction, solution.reachRatio);
    const camera_math::Vector3 outward = leftHand
        ? Scale(basis.right, -1.0f) : basis.right;
    const float forwardAmount = std::max(Dot(toWrist, basis.forward), 0.0f);
    const float upwardAmount = std::max(Dot(toWrist, basis.up), 0.0f);
    const float outwardAmount = std::max(Dot(toWrist, outward), 0.0f);
    camera_math::Vector3 reachDirection = Add(
        Add(Scale(basis.forward, forwardAmount), Scale(basis.up, upwardAmount)),
        Scale(outward, outwardAmount * 0.35f));

    camera_math::Vector3 desiredOffset{};
    if (maximumOffset > kEpsilon && solution.blend > 0.0f
        && Normalize(reachDirection)) {
        desiredOffset = Scale(
            reachDirection, maximumOffset * solution.blend);
    }
    camera_math::Vector3 desiredLocal = ToLocal(basis, desiredOffset);
    if (previousOffsetLocal != nullptr) {
        desiredLocal = Lerp(*previousOffsetLocal, desiredLocal, smoothing);
    }
    solution.offsetLocal = desiredLocal;
    solution.offsetWorld = ToWorld(basis, desiredLocal);
    solution.target = Add(shoulder, solution.offsetWorld);
    solution.applied = Length(solution.offsetWorld) > kEpsilon;
    return IsFinite(solution.target) && IsFinite(solution.offsetLocal);
}

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
    ElbowPoleSolution& solution)
{
    solution = {};
    if (!IsFinite(shoulder) || !IsFinite(wristTarget) || !IsFinite(nativeElbow)
        || !IsFinite(bodyForward) || !IsFinite(worldUp)
        || !std::isfinite(downwardWeight) || downwardWeight < 0.0f
        || !std::isfinite(poleDistance) || poleDistance <= kEpsilon
        || !std::isfinite(maximumSwivelDegrees)
        || maximumSwivelDegrees <= 0.0f || maximumSwivelDegrees > 180.0f
        || (previousDirectionLocal != nullptr
            && !IsFinite(*previousDirectionLocal))) {
        return false;
    }

    TorsoBasis basis;
    if (!BuildTorsoBasis(bodyForward, worldUp, basis)) return false;
    camera_math::Vector3 axis = Subtract(wristTarget, shoulder);
    if (!Normalize(axis)) return false;
    const camera_math::Vector3 outward = leftHand
        ? Scale(basis.right, -1.0f) : basis.right;
    const float vertical = Dot(axis, basis.up);
    const float forward = Dot(axis, basis.forward);
    const float side = Dot(axis, outward);
    const float outwardWeight = std::clamp(
        0.30f + side * 0.25f + std::max(vertical, 0.0f) * 0.35f,
        0.10f, 0.80f);
    const float backwardWeight = std::clamp(
        0.22f + std::max(forward, 0.0f) * 0.30f,
        0.15f, 0.55f);
    camera_math::Vector3 preferred = Add(
        Add(Scale(basis.up, -downwardWeight), Scale(outward, outwardWeight)),
        Scale(basis.forward, -backwardWeight));

    camera_math::Vector3 desiredDirection{};
    if (!ProjectDirection(preferred, axis, desiredDirection)) {
        if (!ProjectDirection(
                Subtract(nativeElbow, shoulder), axis, desiredDirection)) {
            return false;
        }
        solution.nativeFallbackUsed = true;
    }

    solution.singularityBlend = Smoothstep(
        0.82f, 0.98f, std::fabs(Dot(axis, basis.up)));
    camera_math::Vector3 previousDirection{};
    if (previousDirectionLocal != nullptr
        && ProjectDirection(
            ToWorld(basis, *previousDirectionLocal), axis, previousDirection)) {
        solution.historyUsed = true;
        const float historyWeight = 0.35f + 0.60f * solution.singularityBlend;
        camera_math::Vector3 blended = Lerp(
            desiredDirection, previousDirection, historyWeight);
        if (Normalize(blended)) desiredDirection = blended;

        const float cosine = std::clamp(
            Dot(previousDirection, desiredDirection), -1.0f, 1.0f);
        const float angle = std::acos(cosine);
        const float maximumAngle = maximumSwivelDegrees * 0.017453292519943295f;
        if (angle > maximumAngle && angle > kEpsilon) {
            camera_math::Vector3 limited = Lerp(
                previousDirection, desiredDirection, maximumAngle / angle);
            if (ProjectDirection(limited, axis, desiredDirection)) {
                solution.swivelLimited = true;
            }
        }
    }

    solution.directionWorld = desiredDirection;
    solution.directionLocal = ToLocal(basis, desiredDirection);
    solution.pole = Add(shoulder, Scale(desiredDirection, poleDistance));
    return IsFinite(solution.pole) && IsFinite(solution.directionLocal);
}

bool SolveTwoBone(
    const camera_math::Vector3& shoulder,
    const camera_math::Vector3& target,
    const camera_math::Vector3& pole,
    float upperLength,
    float lowerLength,
    float maxReachFraction,
    TwoBoneSolution& solution)
{
    solution = {};
    if (!IsFinite(shoulder) || !IsFinite(target) || !IsFinite(pole)
        || !std::isfinite(upperLength) || !std::isfinite(lowerLength)
        || !std::isfinite(maxReachFraction)
        || upperLength <= kEpsilon || lowerLength <= kEpsilon
        || maxReachFraction <= 0.0f || maxReachFraction > 1.0f) {
        return false;
    }

    camera_math::Vector3 targetDirection = Subtract(target, shoulder);
    const float requestedDistance = Length(targetDirection);
    if (!Normalize(targetDirection)) return false;

    const float minimumDistance = std::fabs(upperLength - lowerLength) + kEpsilon;
    const float maximumDistance = (upperLength + lowerLength) * maxReachFraction;
    const float solvedDistance = std::clamp(requestedDistance, minimumDistance, maximumDistance);
    const camera_math::Vector3 wrist = Add(shoulder, Scale(targetDirection, solvedDistance));

    camera_math::Vector3 poleDirection = Subtract(pole, shoulder);
    poleDirection = Subtract(poleDirection, Scale(targetDirection, Dot(poleDirection, targetDirection)));
    if (!Normalize(poleDirection)) {
        const camera_math::Vector3 fallback = std::fabs(targetDirection.y) < 0.9f
            ? camera_math::Vector3{0.0f, 1.0f, 0.0f}
            : camera_math::Vector3{1.0f, 0.0f, 0.0f};
        poleDirection = Cross(targetDirection, fallback);
        if (!Normalize(poleDirection)) return false;
    }

    const float along = (
        upperLength * upperLength
        - lowerLength * lowerLength
        + solvedDistance * solvedDistance) / (2.0f * solvedDistance);
    const float perpendicularSquared = std::max(
        upperLength * upperLength - along * along, 0.0f);
    const float perpendicular = std::sqrt(perpendicularSquared);

    solution.elbow = Add(
        Add(shoulder, Scale(targetDirection, along)),
        Scale(poleDirection, perpendicular));
    solution.wrist = wrist;
    solution.requestedDistance = requestedDistance;
    solution.solvedDistance = solvedDistance;
    solution.reachClamped = std::fabs(requestedDistance - solvedDistance) > 1.0e-5f;
    return IsFinite(solution.elbow) && IsFinite(solution.wrist);
}

bool RotateWorldMatrixToward(
    const std::array<float, 16>& currentWorld,
    const camera_math::Vector3& currentEndpoint,
    const camera_math::Vector3& desiredEndpoint,
    float blend,
    std::array<float, 16>& desiredWorld)
{
    desiredWorld = {};
    if (!std::isfinite(blend) || blend < 0.0f || blend > 1.0f) return false;
    for (float value : currentWorld) {
        if (!std::isfinite(value)) return false;
    }

    const camera_math::Vector3 origin{currentWorld[3], currentWorld[7], currentWorld[11]};
    camera_math::Vector3 from = Subtract(currentEndpoint, origin);
    camera_math::Vector3 to = Subtract(desiredEndpoint, origin);
    if (!Normalize(from) || !Normalize(to)) return false;

    camera_math::Vector3 axis = Cross(from, to);
    float sine = Length(axis);
    float cosine = std::clamp(Dot(from, to), -1.0f, 1.0f);
    if (sine <= kEpsilon) {
        if (cosine > 0.0f || blend == 0.0f) {
            desiredWorld = currentWorld;
            return true;
        }
        const camera_math::Vector3 fallback = std::fabs(from.y) < 0.9f
            ? camera_math::Vector3{0.0f, 1.0f, 0.0f}
            : camera_math::Vector3{1.0f, 0.0f, 0.0f};
        axis = Cross(from, fallback);
        if (!Normalize(axis)) return false;
    } else {
        axis = Scale(axis, 1.0f / sine);
    }

    const float angle = std::atan2(sine, cosine) * blend;
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    const float t = 1.0f - c;
    const float x = axis.x;
    const float y = axis.y;
    const float z = axis.z;
    const std::array<float, 16> delta = {
        t*x*x + c,     t*x*y - s*z,   t*x*z + s*y,   0.0f,
        t*x*y + s*z,   t*y*y + c,     t*y*z - s*x,   0.0f,
        t*x*z - s*y,   t*y*z + s*x,   t*z*z + c,     0.0f,
        0.0f,          0.0f,          0.0f,          1.0f,
    };
    desiredWorld = camera_math::MatrixMultiply(delta, currentWorld);
    desiredWorld[3] = origin.x;
    desiredWorld[7] = origin.y;
    desiredWorld[11] = origin.z;
    desiredWorld[12] = 0.0f;
    desiredWorld[13] = 0.0f;
    desiredWorld[14] = 0.0f;
    desiredWorld[15] = 1.0f;
    return true;
}

} // namespace somavr::arm_ik_math
