#pragma once

#include "Config.h"
#include "OpenXRRuntime.h"

namespace somavr {

bool InstallHPLGrabBridge(const Config& config, OpenXRRuntime* openxr);
void RemoveHPLGrabBridge();
void LogHPLGrabBridgeSummary();

} // namespace somavr
