#pragma once

#include "Config.h"
#include "OpenXRRuntime.h"

namespace somavr {

bool InstallHPLLifecycle(const Config& config, OpenXRRuntime* openxr);
void RemoveHPLLifecycle();
void LogHPLLifecycleSummary();

} // namespace somavr
