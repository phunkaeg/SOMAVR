#include "CompatibilityScan.h"

#include <TlHelp32.h>

#include <algorithm>
#include <cwctype>
#include <iostream>
#include <system_error>
#include <utility>

namespace somavr::injector {
namespace {

std::wstring Lower(std::wstring_view value)
{
    std::wstring result(value);
    std::transform(result.begin(), result.end(), result.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(std::towlower(ch));
    });
    return result;
}

bool Contains(std::wstring_view value, std::wstring_view needle)
{
    return value.find(needle) != std::wstring_view::npos;
}

std::filesystem::path QueryProcessPath(DWORD processId)
{
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, processId);
    if (process == nullptr) return {};

    std::wstring buffer(32768, L'\0');
    DWORD size = static_cast<DWORD>(buffer.size());
    const bool ok = QueryFullProcessImageNameW(process, 0, buffer.data(), &size) != FALSE;
    CloseHandle(process);
    if (!ok) return {};
    buffer.resize(size);
    return std::filesystem::path(buffer);
}

void AddDirectoryFindings(
    const std::filesystem::path& directory,
    std::vector<CompatibilityFinding>& findings)
{
    std::error_code error;
    for (std::filesystem::directory_iterator iterator(directory, error), end;
         !error && iterator != end;
         iterator.increment(error)) {
        if (!iterator->is_regular_file(error) || error) continue;
        CompatibilityFinding finding;
        if (ClassifyCompatibilityName(iterator->path().filename().wstring(), false, finding)) {
            finding.source = L"game_directory";
            finding.path = iterator->path();
            findings.push_back(std::move(finding));
        }
    }
}

void AddModuleFindings(DWORD processId, std::vector<CompatibilityFinding>& findings)
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, processId);
    if (snapshot == INVALID_HANDLE_VALUE) return;

    MODULEENTRY32W module = {};
    module.dwSize = sizeof(module);
    if (Module32FirstW(snapshot, &module)) {
        do {
            CompatibilityFinding finding;
            if (ClassifyCompatibilityName(module.szModule, true, finding)) {
                finding.source = L"loaded_module";
                finding.path = module.szExePath;
                findings.push_back(std::move(finding));
            }
        } while (Module32NextW(snapshot, &module));
    }
    CloseHandle(snapshot);
}

} // namespace

bool ClassifyCompatibilityName(
    std::wstring_view name,
    bool loadedModule,
    CompatibilityFinding& finding)
{
    finding = {};
    const std::wstring lower = Lower(name);
    if (loadedModule && lower == L"somavr.dll") {
        finding.severity = CompatibilitySeverity::Blocking;
        finding.reason = L"SOMAVR is already loaded; a second injection would duplicate hooks and DLL references";
        return true;
    }

    const bool reshade = Contains(lower, L"reshade")
        || (!loadedModule && (lower == L"opengl32.dll" || lower == L"dxgi.dll"
            || lower == L"d3d9.dll" || lower == L"d3d11.dll"));
    if (reshade) {
        finding.reason = L"graphics proxy or ReShade component may change OpenGL hook order and framebuffer ownership";
        return true;
    }
    if (Contains(lower, L"specialk")) {
        finding.reason = L"Special K may alter presentation, timing, or graphics API hooks";
        return true;
    }
    if (Contains(lower, L"rtsshooks")) {
        finding.reason = L"RTSS injection may alter swap/presentation hook order";
        return true;
    }
    if (Contains(lower, L"openxr_toolkit") || Contains(lower, L"openxr-api-layer")) {
        finding.reason = L"OpenXR API layer may alter swapchains, views, or frame submission";
        return true;
    }
    if (Contains(lower, L"vrperfkit") || lower == L"vrperfkit.yml") {
        finding.reason = L"VR performance wrapper may alter OpenXR or render-target behavior";
        return true;
    }
    if (lower == L"openvr_api.dll") {
        finding.reason = L"unexpected OpenVR loader may indicate another VR mod or injection path";
        return true;
    }
    if (!loadedModule && lower == L"dinput8.dll") {
        finding.reason = L"local dinput8 proxy may load another mod framework before SOMAVR";
        return true;
    }
    return false;
}

std::vector<CompatibilityFinding> ScanCompatibility(DWORD processId)
{
    std::vector<CompatibilityFinding> findings;
    const std::filesystem::path processPath = QueryProcessPath(processId);
    if (!processPath.empty()) {
        AddDirectoryFindings(processPath.parent_path(), findings);
    }
    AddModuleFindings(processId, findings);

    std::sort(findings.begin(), findings.end(), [](const auto& left, const auto& right) {
        if (left.severity != right.severity) {
            return left.severity == CompatibilitySeverity::Blocking;
        }
        return left.path.wstring() < right.path.wstring();
    });
    findings.erase(std::unique(findings.begin(), findings.end(), [](const auto& left, const auto& right) {
        return left.severity == right.severity
            && Lower(left.path.wstring()) == Lower(right.path.wstring());
    }), findings.end());
    return findings;
}

std::vector<CompatibilityFinding> ScanCompatibilityDirectory(
    const std::filesystem::path& directory)
{
    std::vector<CompatibilityFinding> findings;
    AddDirectoryFindings(directory, findings);
    std::sort(findings.begin(), findings.end(), [](const auto& left, const auto& right) {
        return left.path.wstring() < right.path.wstring();
    });
    return findings;
}

bool PrintCompatibilityFindings(const std::vector<CompatibilityFinding>& findings)
{
    bool blocked = false;
    if (findings.empty()) {
        std::wcout << L"Compatibility scan: no known injection or graphics proxy conflicts detected\n";
        return false;
    }

    std::wcerr << L"Compatibility scan: " << findings.size() << L" potential conflict(s)\n";
    for (const CompatibilityFinding& finding : findings) {
        const bool blocking = finding.severity == CompatibilitySeverity::Blocking;
        blocked = blocked || blocking;
        std::wcerr
            << (blocking ? L"  BLOCK: " : L"  WARN: ")
            << finding.path.wstring()
            << L" [" << finding.source << L"]\n"
            << L"        " << finding.reason << L"\n";
    }
    return blocked;
}

} // namespace somavr::injector
