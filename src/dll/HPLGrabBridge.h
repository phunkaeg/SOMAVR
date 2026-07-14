#pragma once

#include "Config.h"
#include "OpenXRRuntime.h"

namespace somavr {

bool InstallHPLGrabBridge(const Config& config, OpenXRRuntime* openxr);
void ArmHPLControllerThrow(const OpenXRControllerPose& gripPose, uint64_t gameFrame);
void RemoveHPLGrabBridge();
void LogHPLGrabBridgeSummary();

} // namespace somavr
