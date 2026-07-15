#pragma once

namespace somavr {

struct Config;

bool InstallHPLUserModuleBridge(const Config& config);
void RemoveHPLUserModuleBridge();
void LogHPLUserModuleBridgeSummary();

} // namespace somavr
