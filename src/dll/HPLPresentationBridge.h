#pragma once

#include <cstdint>

namespace somavr {

struct Config;
class OpenXRRuntime;

bool InstallHPLPresentationBridge(const Config& config, OpenXRRuntime* openxr);
void RemoveHPLPresentationBridge();
void UpdateHPLPresentationBridge(uint64_t frameIndex);
bool IsHPLLoadingScreenActive();
uint64_t GetHPLLoadingGeneration();
void PublishHPLWakeSetAsleep(bool asleep);
void PublishHPLWakeStart(float durationSeconds);
bool IsHPLWakePresentationActive();
bool IsHPLWakeAsleep();
void PublishHPLInventoryOpen();
bool IsHPLInventoryPresentationActive();
void LogHPLPresentationBridgeSummary();

} // namespace somavr
