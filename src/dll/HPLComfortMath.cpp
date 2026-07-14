#include "HPLComfortMath.h"

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

} // namespace somavr::comfort_math
