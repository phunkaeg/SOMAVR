#pragma once

#include "Config.h"
#include "OpenXRRuntime.h"

namespace somavr {

bool InstallHPLInteractionBridge(const Config& config, OpenXRRuntime* openxr);
void RemoveHPLInteractionBridge();
void LogHPLInteractionBridgeSummary();

} // namespace somavr
