#pragma once

#include "Config.h"
#include "OpenXRRuntime.h"

namespace somavr {

bool InstallHPLContactHapticsBridge(const Config& config, OpenXRRuntime* openxr);
void RemoveHPLContactHapticsBridge();
void LogHPLContactHapticsBridgeSummary();

} // namespace somavr
