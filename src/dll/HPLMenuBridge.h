#pragma once

#include "Config.h"
#include "OpenXRRuntime.h"

namespace somavr {

bool InstallHPLMenuBridge(const Config& config);
bool UpdateHPLMenuPointer(
    const OpenXRHeadPose& headPose,
    const OpenXRControllerPose& aimPose);
bool IsHPLNativeMenuCursorVisible();
void DeactivateHPLMenuPointer();
void LogHPLMenuBridgeSummary();
void RemoveHPLMenuBridge();

} // namespace somavr
