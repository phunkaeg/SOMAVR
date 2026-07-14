#include "HPLHudMath.h"

#include <algorithm>
#include <cmath>

namespace somavr::hud_math {

bool BuildHeadLockedQuadPose(
    const camera_math::Vector3& headPosition,
    const camera_math::Quaternion& headOrientation,
    float distanceMeters,
    float verticalOffsetMeters,
    float widthMeters,
    float textureAspect,
    HudQuadPose& pose)
{
    if (!std::isfinite(distanceMeters)
        || !std::isfinite(verticalOffsetMeters)
        || !std::isfinite(widthMeters)
        || !std::isfinite(textureAspect)
        || distanceMeters <= 0.0f
        || widthMeters <= 0.0f
        || textureAspect <= 0.0f) {
        return false;
    }

    const camera_math::Quaternion orientation = camera_math::Normalize(headOrientation);
    const camera_math::Vector3 localOffset{0.0f, verticalOffsetMeters, -distanceMeters};
    const camera_math::Vector3 worldOffset = camera_math::RotateVector(orientation, localOffset);
    pose.position = {
        headPosition.x + worldOffset.x,
        headPosition.y + worldOffset.y,
        headPosition.z + worldOffset.z,
    };
    pose.orientation = orientation;
    pose.widthMeters = widthMeters;
    pose.heightMeters = widthMeters / textureAspect;
    return std::isfinite(pose.heightMeters) && pose.heightMeters > 0.0f;
}

bool ComputeAngularQuadSize(
    float distanceMeters,
    float angularSizeDegrees,
    float minSizeMeters,
    float maxSizeMeters,
    float& sizeMeters)
{
    constexpr float kPi = 3.14159265358979323846f;
    if (!std::isfinite(distanceMeters)
        || !std::isfinite(angularSizeDegrees)
        || !std::isfinite(minSizeMeters)
        || !std::isfinite(maxSizeMeters)
        || distanceMeters <= 0.0f
        || angularSizeDegrees <= 0.0f
        || minSizeMeters <= 0.0f
        || maxSizeMeters < minSizeMeters) {
        return false;
    }
    const float angularRadians = angularSizeDegrees * kPi / 180.0f;
    sizeMeters = std::clamp(
        2.0f * std::tan(angularRadians * 0.5f) * distanceMeters,
        minSizeMeters,
        maxSizeMeters);
    return std::isfinite(sizeMeters) && sizeMeters > 0.0f;
}

bool ComputeInteractionReticleColor(int crosshairState, InteractionReticleColor& color)
{
    if (crosshairState <= 0 || crosshairState >= 35) {
        return false;
    }

    color = {};
    switch (crosshairState) {
    case 2:  // CarryOneHanded
    case 3:  // CarryTwoHanded
    case 14: // PickUp
        color = {0.25f, 1.0f, 0.55f, 0.95f};
        break;
    case 4:  // Push
    case 5:  // PullLever
    case 6:  // PullLeverSmall
    case 7:  // PullSideways
    case 8:  // PullOut
    case 9:  // PullDoor
    case 10: // PullDoorHatch
    case 11: // Rotate
    case 12: // RotateOneHanded
    case 13: // PushButton
    case 32: // PullVertical
        color = {1.0f, 0.72f, 0.20f, 0.95f};
        break;
    case 15: // UseToolInsert
    case 16: // UseToolAction
    case 17: // Terminal
    case 18: // Datamine
    case 22: // Read
    case 28: // Examine
    case 29: // Recharge
    case 33: // FireGun
        color = {0.25f, 0.72f, 1.0f, 0.95f};
        break;
    case 19: // ClimbLadder
    case 23: // ExitLevel
    case 24: // ClimbLedge
    case 25: // SitDown
        color = {0.95f, 0.95f, 0.95f, 0.95f};
        break;
    case 20: // Talk
    case 21: // Eat
        color = {0.85f, 0.45f, 1.0f, 0.95f};
        break;
    case 30: // RechargeBad
    case 31: // TalkBusy
        color = {1.0f, 0.30f, 0.25f, 0.95f};
        break;
    default:
        break;
    }
    return true;
}

bool ComputeInteractionHapticProfile(
    int crosshairState,
    float& amplitudeScale,
    float& durationScale)
{
    if (crosshairState <= 1 || crosshairState >= 35) {
        return false;
    }

    amplitudeScale = 1.0f;
    durationScale = 1.0f;
    switch (crosshairState) {
    case 2:
    case 3:
    case 14:
        amplitudeScale = 0.75f;
        durationScale = 0.80f;
        break;
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
    case 13:
    case 32:
        amplitudeScale = 1.15f;
        durationScale = 1.25f;
        break;
    case 19:
    case 23:
    case 24:
    case 25:
        amplitudeScale = 1.10f;
        durationScale = 1.25f;
        break;
    case 20:
    case 21:
    case 31:
        amplitudeScale = 0.65f;
        durationScale = 1.10f;
        break;
    case 30:
        amplitudeScale = 0.45f;
        durationScale = 0.65f;
        break;
    case 34:
        amplitudeScale = 0.60f;
        durationScale = 0.75f;
        break;
    default:
        break;
    }
    return true;
}

} // namespace somavr::hud_math
