#pragma once

#include "Config.h"

#include <cstdint>

namespace somavr {

class OpenXRRuntime;

bool InstallHPLHudBridge(const Config& config, OpenXRRuntime* openxr);
bool IsHPLCurrentImGuiSurfaceActive(uint64_t frameIndex, uint64_t maximumAgeFrames);
void ResetHPLTerminalHudRetention(const char* reason);
void LogHPLHudBridgeSummary();
void RemoveHPLHudBridge();

} // namespace somavr
