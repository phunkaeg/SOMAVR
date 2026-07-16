#pragma once

#include "Config.h"

#include <cstdint>

namespace somavr
{

struct HPLPlayerStateSnapshot
{
    bool installed = false;
    bool playerValid = false;
    bool cameraControlValid = false;
    bool cameraUpdateActive = false;
    bool authoredCameraActive = false;
    uint64_t frame = 0;
    void* player = nullptr;
    void* camera = nullptr;
    void* characterBody = nullptr;
    int playerStateId = -1;
    int moveStateId = -1;
    int cameraRotateMode = -1;
};

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
