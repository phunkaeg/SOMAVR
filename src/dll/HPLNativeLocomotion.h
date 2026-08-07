#pragma once

#include "Config.h"
#include "HPLCameraBridge.h"
#include "HPLPlayerState.h"

namespace somavr
{

bool InstallHPLNativeLocomotion(const Config& config);
bool CanApplyHPLNativeMovement(const HPLPlayerStateSnapshot& player);
bool CanApplyHPLInteractionMovement(const HPLPlayerStateSnapshot& player);
bool CanApplyHPLNativeTurn(const HPLPlayerStateSnapshot& player);
bool ApplyHPLNativeMovement(const HPLPlayerStateSnapshot& player, float right, float forward);
bool ApplyHPLInteractionMovement(const HPLPlayerStateSnapshot& player, float right, float forward);
bool ApplyHPLNativeTurn(const HPLPlayerStateSnapshot& player, float radians);
bool ApplyHPLRoomscaleBodyReconciliation(
    const HPLPlayerStateSnapshot& player,
    const HPLCameraBridgeStatus& camera);
bool GetHPLGamePausedState(bool& paused);
void LogHPLNativeLocomotionSummary();
void RemoveHPLNativeLocomotion();

} // namespace somavr
