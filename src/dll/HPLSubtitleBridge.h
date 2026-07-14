#pragma once

#include "Config.h"

namespace somavr {

bool InstallHPLSubtitleBridge(const Config& config);
void LogHPLSubtitleBridgeSummary();
void RemoveHPLSubtitleBridge();

} // namespace somavr
