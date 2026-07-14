#pragma once

#include "Config.h"
#include "OpenXRRuntime.h"

namespace somavr {

bool InstallHPLHandsBridge(const Config& config, OpenXRRuntime* openxr);
void RemoveHPLHandsBridge();
void LogHPLHandsBridgeSummary();

} // namespace somavr
