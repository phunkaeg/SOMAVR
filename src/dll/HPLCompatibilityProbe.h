#pragma once

#include "Config.h"
#include "OpenXRRuntime.h"

namespace somavr {

bool InstallHPLCompatibilityProbe(const Config& config, OpenXRRuntime* openxr);
void LogHPLCompatibilityProbeSummary();
void RemoveHPLCompatibilityProbe();

} // namespace somavr
