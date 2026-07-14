#pragma once

#include "Config.h"
#include "OpenXRRuntime.h"

#include <cstdint>

namespace somavr {

bool InstallHPLInputBridge(const Config& config, OpenXRRuntime* openxr);
void UpdateHPLInputBridge(uint64_t frameIndex);
void RemoveHPLInputBridge();
void LogHPLInputBridgeSummary();

} // namespace somavr
