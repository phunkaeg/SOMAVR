#pragma once

#include "Config.h"
#include "OpenXRRuntime.h"

namespace somavr {

bool InstallHPLTerminalBridge(const Config& config);
bool UpdateHPLTerminalPointer(
    const OpenXRHeadPose& headPose,
    const OpenXRControllerPose& aimPose);
void DeactivateHPLTerminalPointer();
void LogHPLTerminalBridgeSummary();
void RemoveHPLTerminalBridge();

} // namespace somavr
