#pragma once

#include "Config.h"

namespace somavr {

bool InstallHPLCrosshairBridge(const Config& config);
void RemoveHPLCrosshairBridge();
void LogHPLCrosshairBridgeSummary();
const char* HPLCrosshairStateName(int state);

} // namespace somavr
