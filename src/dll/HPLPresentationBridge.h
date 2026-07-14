#pragma once

#include <cstdint>

namespace somavr {

struct Config;
class OpenXRRuntime;

bool InstallHPLPresentationBridge(const Config& config, OpenXRRuntime* openxr);
void RemoveHPLPresentationBridge();
void UpdateHPLPresentationBridge(uint64_t frameIndex);
bool IsHPLLoadingScreenActive();
void LogHPLPresentationBridgeSummary();

} // namespace somavr
