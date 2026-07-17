#include "CompatibilityScan.h"

#include "SomaBuildSignatures.h"

#include <TlHelp32.h>

#include <algorithm>
#include <cstring>
#include <cwctype>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <sstream>
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

template <typename T>
bool ReadFileValue(const std::vector<uint8_t>& file, size_t offset, T& value)
{
    if (offset > file.size() || sizeof(T) > file.size() - offset) return false;
    std::memcpy(&value, file.data() + offset, sizeof(T));
    return true;
}

const uint8_t* ResolvePeRva(
    const std::vector<uint8_t>& file,
    const IMAGE_FILE_HEADER& fileHeader,
    size_t sectionTableOffset,
    uintptr_t rva,
    size_t requiredBytes)
{
    for (uint16_t sectionIndex = 0;
         sectionIndex < fileHeader.NumberOfSections;
         ++sectionIndex) {
        IMAGE_SECTION_HEADER section{};
        const size_t sectionOffset = sectionTableOffset
            + static_cast<size_t>(sectionIndex) * sizeof(section);
        if (!ReadFileValue(file, sectionOffset, section)) return nullptr;
        const uint64_t sectionSize = std::max<uint32_t>(
            section.Misc.VirtualSize,
            section.SizeOfRawData);
        const uint64_t sectionStart = section.VirtualAddress;
        const uint64_t sectionEnd = sectionStart + sectionSize;
        if (rva < sectionStart || rva >= sectionEnd) continue;
        const uint64_t rawOffset = static_cast<uint64_t>(section.PointerToRawData)
            + (rva - sectionStart);
        if (rawOffset > file.size() || requiredBytes > file.size() - rawOffset) {
            return nullptr;
        }
        return file.data() + rawOffset;
    }
    return nullptr;
}

std::wstring SignatureMismatchReason(uintptr_t rva, const uint8_t* bytes, size_t size)
{
    std::wostringstream stream;
    stream << L"SOMA interaction hook signature mismatch at RVA 0x"
           << std::hex << rva << L" actual=";
    const size_t prefixBytes = std::min<size_t>(size, 6);
    for (size_t index = 0; index < prefixBytes; ++index) {
        if (index != 0) stream << L',';
        stream << std::setw(2) << std::setfill(L'0')
               << static_cast<unsigned int>(bytes[index]);
    }
    return stream.str();
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

bool MatchSomaInteractionSignature(uintptr_t rva, const uint8_t* bytes, size_t size)
{
    if (bytes == nullptr) return false;
    if (rva == soma_signatures::kGetClosestEntityRva) {
        return size >= sizeof(soma_signatures::kGetClosestEntity)
            && std::memcmp(
                bytes,
                soma_signatures::kGetClosestEntity,
                sizeof(soma_signatures::kGetClosestEntity)) == 0;
    }
    if (rva == soma_signatures::kGetClosestEntityRaycastRva) {
        return size >= sizeof(soma_signatures::kGetClosestEntityRaycast)
            && std::memcmp(
                bytes,
                soma_signatures::kGetClosestEntityRaycast,
                sizeof(soma_signatures::kGetClosestEntityRaycast)) == 0;
    }
    return false;
}

bool ValidateSomaInteractionSignatures(
    const std::filesystem::path& executable,
    std::wstring& failureReason)
{
    failureReason.clear();
    std::ifstream input(executable, std::ios::binary | std::ios::ate);
    if (!input) {
        failureReason = L"could not open SOMA executable for hook signature validation";
        return false;
    }
    const std::streamoff length = input.tellg();
    if (length <= 0) {
        failureReason = L"SOMA executable is empty or unreadable";
        return false;
    }
    std::vector<uint8_t> file(static_cast<size_t>(length));
    input.seekg(0, std::ios::beg);
    if (!input.read(reinterpret_cast<char*>(file.data()), length)) {
        failureReason = L"could not read SOMA executable for hook signature validation";
        return false;
    }

    IMAGE_DOS_HEADER dos{};
    if (!ReadFileValue(file, 0, dos) || dos.e_magic != IMAGE_DOS_SIGNATURE
        || dos.e_lfanew < 0) {
        failureReason = L"SOMA executable has an invalid DOS header";
        return false;
    }
    const size_t ntOffset = static_cast<size_t>(dos.e_lfanew);
    DWORD ntSignature = 0;
    IMAGE_FILE_HEADER fileHeader{};
    WORD optionalMagic = 0;
    const size_t fileHeaderOffset = ntOffset + sizeof(DWORD);
    const size_t optionalHeaderOffset = fileHeaderOffset + sizeof(fileHeader);
    if (!ReadFileValue(file, ntOffset, ntSignature)
        || ntSignature != IMAGE_NT_SIGNATURE
        || !ReadFileValue(file, fileHeaderOffset, fileHeader)
        || !ReadFileValue(file, optionalHeaderOffset, optionalMagic)
        || fileHeader.Machine != IMAGE_FILE_MACHINE_AMD64
        || optionalMagic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        failureReason = L"SOMA executable has an unsupported PE header";
        return false;
    }
    const size_t sectionTableOffset = optionalHeaderOffset
        + fileHeader.SizeOfOptionalHeader;
    const uintptr_t rvas[] = {
        soma_signatures::kGetClosestEntityRva,
        soma_signatures::kGetClosestEntityRaycastRva,
    };
    const size_t sizes[] = {
        sizeof(soma_signatures::kGetClosestEntity),
        sizeof(soma_signatures::kGetClosestEntityRaycast),
    };
    for (size_t index = 0; index < std::size(rvas); ++index) {
        const uint8_t* bytes = ResolvePeRva(
            file, fileHeader, sectionTableOffset, rvas[index], sizes[index]);
        if (bytes == nullptr) {
            failureReason = L"could not map SOMA interaction hook RVA 0x";
            std::wostringstream rvaStream;
            rvaStream << std::hex << rvas[index];
            failureReason += rvaStream.str();
            return false;
        }
        if (!MatchSomaInteractionSignature(rvas[index], bytes, sizes[index])) {
            failureReason = SignatureMismatchReason(rvas[index], bytes, sizes[index]);
            return false;
        }
    }
    return true;
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
