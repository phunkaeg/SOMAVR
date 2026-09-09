#pragma once

#include "Config.h"
#include "OpenXRRuntime.h"

namespace somavr {

bool InstallHPLGrabBridge(const Config& config, OpenXRRuntime* openxr);
bool ArmHPLControllerThrow(const OpenXRControllerPose& gripPose, uint64_t gameFrame);
void CancelHPLControllerThrow();
void RemoveHPLGrabBridge();
void LogHPLGrabBridgeSummary();

} // namespace somavr
