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

struct OpenXRApiLayerRegistration {
    std::wstring scope;
    std::filesystem::path manifestPath;
    bool implicit = false;
    bool registryEnabled = false;
};

bool ClassifyCompatibilityName(
    std::wstring_view name,
    bool loadedModule,
    CompatibilityFinding& finding);
bool MatchSomaInteractionSignature(uintptr_t rva, const uint8_t* bytes, size_t size);
bool ValidateSomaInteractionSignatures(
    const std::filesystem::path& executable,
    std::wstring& failureReason);

std::vector<CompatibilityFinding> ScanCompatibility(DWORD processId);
std::vector<CompatibilityFinding> ScanCompatibilityDirectory(
    const std::filesystem::path& directory);
std::vector<OpenXRApiLayerRegistration> EnumerateRegisteredOpenXRApiLayers();
bool PrintCompatibilityFindings(const std::vector<CompatibilityFinding>& findings);

} // namespace somavr::injector
