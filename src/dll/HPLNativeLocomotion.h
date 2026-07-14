#pragma once

#include "Config.h"
#include "HPLPlayerState.h"

namespace somavr
{

bool InstallHPLNativeLocomotion(const Config& config);
bool CanApplyHPLNativeMovement(const HPLPlayerStateSnapshot& player);
bool CanApplyHPLNativeTurn(const HPLPlayerStateSnapshot& player);
bool ApplyHPLNativeMovement(const HPLPlayerStateSnapshot& player, float right, float forward);
bool ApplyHPLNativeTurn(const HPLPlayerStateSnapshot& player, float radians);
bool GetHPLGamePausedState(bool& paused);
void LogHPLNativeLocomotionSummary();
void RemoveHPLNativeLocomotion();

} // namespace somavr
