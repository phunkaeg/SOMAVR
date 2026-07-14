#pragma once

#include <Windows.h>

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace somavr::injector {

enum class CompatibilitySeverity {
    Warning,
    Blocking,
};

struct CompatibilityFinding {
    CompatibilitySeverity severity = CompatibilitySeverity::Warning;
    std::wstring source;
    std::filesystem::path path;
    std::wstring reason;
};

bool ClassifyCompatibilityName(
    std::wstring_view name,
    bool loadedModule,
    CompatibilityFinding& finding);

std::vector<CompatibilityFinding> ScanCompatibility(DWORD processId);
bool PrintCompatibilityFindings(const std::vector<CompatibilityFinding>& findings);

} // namespace somavr::injector
