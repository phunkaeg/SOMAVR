#pragma once

#include "Config.h"
#include "OpenXRRuntime.h"

namespace somavr {

bool InstallHPLGameplayHapticsBridge(const Config& config, OpenXRRuntime* openxr);
void RemoveHPLGameplayHapticsBridge();
void LogHPLGameplayHapticsBridgeSummary();

} // namespace somavr
