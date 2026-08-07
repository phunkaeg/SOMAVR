#pragma once

#include "Config.h"
#include "OpenXRRuntime.h"

namespace somavr {

bool InstallHPLTerminalBridge(const Config& config, OpenXRRuntime* openxr);
bool UpdateHPLTerminalPointer(
    const OpenXRHeadPose& headPose,
    const OpenXRControllerPose& aimPose,
    uint64_t gameFrame);
void DeactivateHPLTerminalPointer();
void LogHPLTerminalBridgeSummary();
void RemoveHPLTerminalBridge();

} // namespace somavr
