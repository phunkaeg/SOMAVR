#include "HPLComfortMath.h"

#include <cmath>

namespace somavr::comfort_math {

bool ShouldSuppressCameraAdd(
    int type,
    bool suppressHeadBob,
    bool suppressCameraShake,
    bool suppressSway)
{
    switch (static_cast<CameraAddType>(type)) {
    case CameraAddType::Bob:
        return suppressHeadBob;
    case CameraAddType::Shake:
        return suppressCameraShake;
    case CameraAddType::Sway:
        return suppressSway;
    default:
        return false;
    }
}

const char* CameraAddTypeName(int type)
{
    switch (static_cast<CameraAddType>(type)) {
    case CameraAddType::Crouch: return "crouch";
    case CameraAddType::Bob: return "bob";
    case CameraAddType::Shake: return "shake";
    case CameraAddType::Climb: return "climb";
    case CameraAddType::Terminal: return "terminal";
    case CameraAddType::Script: return "script";
    case CameraAddType::Dead: return "dead";
    case CameraAddType::Lean: return "lean";
    case CameraAddType::Crawl: return "crawl";
    case CameraAddType::Sway: return "sway";
    case CameraAddType::Conversation: return "conversation";
    default: return "unknown";
    }
}

bool ShouldSuppressCameraRoll(
    int type,
    bool suppressScript,
    bool suppressLean,
    bool suppressMove,
    bool suppressClimb)
{
    switch (static_cast<CameraRollType>(type)) {
    case CameraRollType::Script: return suppressScript;
    case CameraRollType::Lean: return suppressLean;
    case CameraRollType::Move: return suppressMove;
    case CameraRollType::Climb: return suppressClimb;
    default: return false;
    }
}

const char* CameraRollTypeName(int type)
{
    switch (static_cast<CameraRollType>(type)) {
    case CameraRollType::Script: return "script";
    case CameraRollType::Lean: return "lean";
    case CameraRollType::Move: return "move";
    case CameraRollType::Climb: return "climb";
    default: return "unknown";
    }
}

bool ShouldBlackoutPlayerStateTransition(int previousState, int currentState)
{
    const auto isHighMotionState = [](int state) {
        switch (state) {
        case 11: // Ladder
        case 12: // ClimbLedge
        case 14: // InteractiveCameraAnimation
        case 15: // Sit
        case 17: // Dead
            return true;
        default:
            return false;
        }
    };
    return previousState != currentState
        && previousState >= 0
        && currentState >= 0
        && (isHighMotionState(previousState) || isHighMotionState(currentState));
}

float ResolveComfortOpticsTarget(
    OpticsChannel channel, float requestedTarget, float defaultFov)
{
    switch (channel) {
    case OpticsChannel::Fov:
        return std::isfinite(defaultFov) && defaultFov > 0.0f
            ? defaultFov
            : requestedTarget;
    case OpticsChannel::FovMultiplier:
    case OpticsChannel::AspectMultiplier:
        return 1.0f;
    default:
        return requestedTarget;
    }
}

} // namespace somavr::comfort_math
