#pragma once

#include "Config.h"

namespace somavr {

bool InstallHPLComfortBridge(const Config& config);
void RemoveHPLComfortBridge();
void LogHPLComfortBridgeSummary();

} // namespace somavr
