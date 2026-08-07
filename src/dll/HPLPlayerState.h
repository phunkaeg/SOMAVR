#pragma once

#include "Config.h"

#include <cstdint>

namespace somavr
{

enum class HPLPlayerStateKind : int
{
    Normal = 0,
    Grab = 1,
    Push = 2,
    Wheel = 3,
    Slide = 4,
    SwingDoor = 5,
    Lever = 6,
    Tear = 7,
    Terminal = 8,
    HandheldTerminal = 9,
    Read = 10,
    Ladder = 11,
    ClimbLedge = 12,
    MovingButton = 13,
    InteractiveCameraAnimation = 14,
    Sit = 15,
    Conversation = 16,
    Dead = 17,
    ZoomArea = 18,
    CustomControls = 19,
    NullState = 20,
    Unknown = -1,
};

struct HPLPlayerStateSnapshot
{
    bool installed = false;
    bool playerValid = false;
    bool cameraControlValid = false;
    bool cameraUpdateActive = false;
    bool authoredCameraActive = false;
    bool semanticAuthoredState = false;
    bool characterBodyCameraValid = false;
    bool characterBodyCameraDetached = false;
    uint64_t frame = 0;
    void* player = nullptr;
    void* camera = nullptr;
    void* characterBody = nullptr;
    void* characterBodyCamera = nullptr;
    int playerStateId = -1;
    int moveStateId = -1;
    int cameraRotateMode = -1;
};

const char* HPLPlayerStateName(int playerStateId);
bool IsHPLSemanticAuthoredState(int playerStateId);
bool InstallHPLPlayerState(const Config& config);
void UpdateHPLPlayerState(uint64_t frameIndex);
bool GetHPLPlayerStateSnapshot(HPLPlayerStateSnapshot& snapshot);
bool IsHPLPlayerStateActiveNow(
    int playerStateId,
    void* expectedPlayer = nullptr,
    void* expectedCharacterBody = nullptr);
void LogHPLPlayerStateSummary();
void RemoveHPLPlayerState();

} // namespace somavr
