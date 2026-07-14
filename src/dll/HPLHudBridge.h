#pragma once

#include "Config.h"

namespace somavr {

class OpenXRRuntime;

bool InstallHPLHudBridge(const Config& config, OpenXRRuntime* openxr);
void LogHPLHudBridgeSummary();
void RemoveHPLHudBridge();

} // namespace somavr
