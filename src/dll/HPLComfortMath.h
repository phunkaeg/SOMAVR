#pragma once

namespace somavr::comfort_math {

enum class CameraAddType : int {
    Crouch = 0,
    Bob = 1,
    Shake = 2,
    Climb = 3,
    Terminal = 4,
    Script = 5,
    Dead = 6,
    Lean = 7,
    Crawl = 8,
    Sway = 9,
    Conversation = 10,
};

enum class CameraRollType : int {
    Script = 0,
    Lean = 1,
    Move = 2,
    Climb = 3,
};

bool ShouldSuppressCameraAdd(
    int type,
    bool suppressHeadBob,
    bool suppressCameraShake,
    bool suppressSway);

const char* CameraAddTypeName(int type);

bool ShouldSuppressCameraRoll(
    int type,
    bool suppressScript,
    bool suppressLean,
    bool suppressMove,
    bool suppressClimb);

const char* CameraRollTypeName(int type);

bool ShouldBlackoutPlayerStateTransition(int previousState, int currentState);

} // namespace somavr::comfort_math
