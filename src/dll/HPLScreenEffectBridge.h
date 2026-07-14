#pragma once

namespace somavr {

struct Config;

bool InstallHPLScreenEffectBridge(const Config& config);
void RemoveHPLScreenEffectBridge();
void LogHPLScreenEffectBridgeSummary();

} // namespace somavr
