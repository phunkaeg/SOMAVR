#pragma once

#include <Windows.h>

#include <filesystem>

namespace somavr {

void InitializeCrashHandler(HMODULE module, const std::filesystem::path& dumpDirectory);
void MaintainCrashHandler();
void LogCrashHandlerSummary();
void ShutdownCrashHandler();

} // namespace somavr
