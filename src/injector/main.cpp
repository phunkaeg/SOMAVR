#include "Config.h"
#include "CompatibilityScan.h"

#include <Windows.h>
#include <TlHelp32.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

namespace {

std::wstring Widen(const char* text)
{
    if (text == nullptr) {
        return {};
    }

    const int needed = MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
    if (needed <= 0) {
        return {};
    }

    std::wstring result(static_cast<size_t>(needed), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text, -1, result.data(), needed);
    if (!result.empty() && result.back() == L'\0') {
        result.pop_back();
    }
    return result;
}

std::wstring ErrorMessage(DWORD error)
{
    wchar_t* buffer = nullptr;
    const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    const DWORD size = FormatMessageW(flags, nullptr, error, 0, reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
    if (size == 0 || buffer == nullptr) {
        return L"unknown error";
    }

    std::wstring message(buffer, size);
    LocalFree(buffer);
    while (!message.empty() && (message.back() == L'\r' || message.back() == L'\n')) {
        message.pop_back();
    }
    return message;
}

bool IsDecimal(std::string_view text)
{
    if (text.empty()) {
        return false;
    }

    for (const char ch : text) {
        if (ch < '0' || ch > '9') {
            return false;
        }
    }
    return true;
}

bool IsOption(std::string_view text, std::string_view option)
{
    return text == option;
}

std::string Trim(std::string value)
{
    const auto isSpace = [](unsigned char ch) { return std::isspace(ch) != 0; };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), [&](char ch) {
        return !isSpace(static_cast<unsigned char>(ch));
    }));
    value.erase(std::find_if(value.rbegin(), value.rend(), [&](char ch) {
        return !isSpace(static_cast<unsigned char>(ch));
    }).base(), value.end());
    return value;
}

std::string Lower(std::string value)
{
    for (char& ch : value) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return value;
}

std::wstring QuoteCommandLineArgument(std::wstring_view argument)
{
    if (argument.empty()) {
        return L"\"\"";
    }

    bool needsQuotes = false;
    for (const wchar_t ch : argument) {
        if (ch == L' ' || ch == L'\t' || ch == L'"') {
            needsQuotes = true;
            break;
        }
    }

    if (!needsQuotes) {
        return std::wstring(argument);
    }

    std::wstring quoted;
    quoted.push_back(L'"');

    size_t backslashes = 0;
    for (const wchar_t ch : argument) {
        if (ch == L'\\') {
            ++backslashes;
            continue;
        }

        if (ch == L'"') {
            quoted.append(backslashes * 2 + 1, L'\\');
            quoted.push_back(ch);
            backslashes = 0;
            continue;
        }

        quoted.append(backslashes, L'\\');
        backslashes = 0;
        quoted.push_back(ch);
    }

    quoted.append(backslashes * 2, L'\\');
    quoted.push_back(L'"');
    return quoted;
}

std::wstring BuildLaunchCommandLine(
    const std::filesystem::path& exePath,
    int argc,
    char** argv,
    int firstGameArg)
{
    std::wstring commandLine = QuoteCommandLineArgument(exePath.wstring());
    for (int i = firstGameArg; i < argc; ++i) {
        commandLine.push_back(L' ');
        commandLine += QuoteCommandLineArgument(Widen(argv[i]));
    }
    return commandLine;
}

DWORD FindProcessIdByName(const std::wstring& processName)
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return 0;
    }

    PROCESSENTRY32W entry = {};
    entry.dwSize = sizeof(entry);
    DWORD pid = 0;

    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, processName.c_str()) == 0) {
                pid = entry.th32ProcessID;
                break;
            }
        } while (Process32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return pid;
}

std::filesystem::path DefaultDllPath(const char* argv0)
{
    wchar_t exePath[MAX_PATH] = {};
    if (GetModuleFileNameW(nullptr, exePath, MAX_PATH) != 0) {
        return std::filesystem::path(exePath).parent_path() / L"somavr.dll";
    }

    return std::filesystem::path(Widen(argv0)).parent_path() / L"somavr.dll";
}

struct DllBuildFlavor {
    bool known = false;
    bool openxr = false;
    std::string version;
    std::string flavor;
    std::filesystem::path sourcePath;
};

struct DoctorSummary {
    int passed = 0;
    int warnings = 0;
    int failures = 0;
};

void DoctorResult(DoctorSummary& summary, const char* status, const std::wstring& message)
{
    std::wcout << Widen(status) << L": " << message << L"\n";
    if (std::string_view(status) == "PASS") ++summary.passed;
    else if (std::string_view(status) == "WARN") ++summary.warnings;
    else ++summary.failures;
}

std::filesystem::path QueryActiveOpenXRRuntime()
{
    wchar_t value[32768] = {};
    DWORD bytes = sizeof(value);
    const LSTATUS result = RegGetValueW(
        HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Khronos\\OpenXR\\1",
        L"ActiveRuntime",
        RRF_RT_REG_SZ,
        nullptr,
        value,
        &bytes);
    return result == ERROR_SUCCESS ? std::filesystem::path(value) : std::filesystem::path{};
}

bool IsX64PortableExecutable(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    IMAGE_DOS_HEADER dos{};
    if (!in.read(reinterpret_cast<char*>(&dos), sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE) {
        return false;
    }
    in.seekg(dos.e_lfanew, std::ios::beg);
    DWORD signature = 0;
    IMAGE_FILE_HEADER header{};
    return in.read(reinterpret_cast<char*>(&signature), sizeof(signature))
        && signature == IMAGE_NT_SIGNATURE
        && in.read(reinterpret_cast<char*>(&header), sizeof(header))
        && header.Machine == IMAGE_FILE_MACHINE_AMD64;
}

DllBuildFlavor ReadDllBuildFlavor(const std::filesystem::path& dllPath)
{
    DllBuildFlavor result = {};
    const std::filesystem::path fullPath = std::filesystem::absolute(dllPath);
    const std::filesystem::path flavorPath = fullPath.parent_path() / L"somavr_build_flavor.txt";
    result.sourcePath = flavorPath;

    std::ifstream in(flavorPath);
    if (in) {
        std::string line;
        while (std::getline(in, line)) {
            const size_t equals = line.find('=');
            if (equals == std::string::npos) {
                continue;
            }

            const std::string key = Lower(Trim(line.substr(0, equals)));
            const std::string value = Trim(line.substr(equals + 1));
            if (key == "openxr") {
                const std::string lowered = Lower(value);
                result.openxr = lowered == "1" || lowered == "true" || lowered == "yes" || lowered == "on";
                result.known = true;
            } else if (key == "version") {
                result.version = value;
            } else if (key == "flavor") {
                result.flavor = value;
            }
        }
    }

    if (!result.known) {
        std::string lowerPath = Lower(fullPath.string());
        if (lowerPath.find("build-openxr") != std::string::npos) {
            result.known = true;
            result.openxr = true;
            result.flavor = "openxr-path";
        } else if (lowerPath.find("\\build\\") != std::string::npos || lowerPath.find("/build/") != std::string::npos) {
            result.known = true;
            result.openxr = false;
            result.flavor = "opengl-path";
        }
    }

    return result;
}

int RunDoctor(
    const std::filesystem::path& dllPath,
    const std::filesystem::path& gamePath)
{
    DoctorSummary summary;
    const std::filesystem::path absoluteDll = std::filesystem::absolute(dllPath);
    const std::filesystem::path packageDirectory = absoluteDll.parent_path();
    std::wcout << L"SOMAVR readiness diagnostic\n"
               << L"Package: " << packageDirectory.wstring() << L"\n";

    if (!std::filesystem::exists(absoluteDll)) {
        DoctorResult(summary, "FAIL", L"SOMAVR DLL missing: " + absoluteDll.wstring());
    } else if (!IsX64PortableExecutable(absoluteDll)) {
        DoctorResult(summary, "FAIL", L"SOMAVR DLL is not a readable x64 PE: " + absoluteDll.wstring());
    } else {
        DoctorResult(summary, "PASS", L"x64 SOMAVR DLL found: " + absoluteDll.wstring());
    }

    const DllBuildFlavor flavor = ReadDllBuildFlavor(absoluteDll);
    if (!flavor.known || !flavor.openxr) {
        DoctorResult(summary, "FAIL", L"selected DLL is not identified as an OpenXR build");
    } else {
        DoctorResult(summary, "PASS",
            L"build flavor=" + Widen(flavor.flavor.c_str())
            + L" version=" + Widen(flavor.version.c_str()));
    }

    const std::filesystem::path loaderPath = packageDirectory / L"openxr_loader.dll";
    DoctorResult(summary,
        std::filesystem::exists(loaderPath) ? "PASS" : "FAIL",
        std::filesystem::exists(loaderPath)
            ? L"packaged OpenXR loader found"
            : L"openxr_loader.dll is missing beside somavr.dll");

    std::filesystem::path configPath = packageDirectory / L"somavr.ini";
    if (!std::filesystem::exists(configPath)
        && std::filesystem::exists(somavr::ConfigPath())) {
        configPath = somavr::ConfigPath();
        DoctorResult(summary, "WARN",
            L"using developer work-root config because somavr.ini is not beside the DLL: "
            + configPath.wstring());
    }
    if (!std::filesystem::exists(configPath)) {
        DoctorResult(summary, "FAIL", L"somavr.ini is missing beside somavr.dll and from the developer work root");
    } else {
        somavr::ConfigManager configManager;
        configManager.InitializeAtPath(configPath);
        const somavr::Config& config = configManager.Get();
        const bool ready = config.openxrProbe && config.openxrSessionProbe
            && config.openxrFrameSubmit;
        DoctorResult(summary, ready ? "PASS" : "FAIL",
            L"config OpenXR probe/session/frameSubmit="
            + std::to_wstring(config.openxrProbe ? 1 : 0) + L"/"
            + std::to_wstring(config.openxrSessionProbe ? 1 : 0) + L"/"
            + std::to_wstring(config.openxrFrameSubmit ? 1 : 0)
            + L" comfortPreset=" + Widen(config.comfortPreset.c_str()));
    }

    const std::filesystem::path runtimePath = QueryActiveOpenXRRuntime();
    if (runtimePath.empty()) {
        DoctorResult(summary, "FAIL", L"no 64-bit OpenXR ActiveRuntime is registered");
    } else if (!std::filesystem::exists(runtimePath)) {
        DoctorResult(summary, "FAIL", L"registered OpenXR runtime JSON is missing: " + runtimePath.wstring());
    } else {
        DoctorResult(summary, "PASS", L"active OpenXR runtime: " + runtimePath.wstring());
    }

    if (gamePath.empty()) {
        DoctorResult(summary, "WARN", L"game path not supplied; SOMA executable and proxy scan skipped");
    } else {
        const std::filesystem::path absoluteGame = std::filesystem::absolute(gamePath);
        if (!std::filesystem::exists(absoluteGame) || !IsX64PortableExecutable(absoluteGame)) {
            DoctorResult(summary, "FAIL", L"SOMA executable is missing or not x64: " + absoluteGame.wstring());
        } else {
            DoctorResult(summary, "PASS", L"x64 SOMA executable found: " + absoluteGame.wstring());
            std::wstring signatureFailure;
            if (somavr::injector::ValidateSomaInteractionSignatures(
                    absoluteGame, signatureFailure)) {
                DoctorResult(summary, "PASS",
                    L"SOMA interaction and wrist hook signatures match the supported build");
            } else {
                DoctorResult(summary, "FAIL", signatureFailure);
            }
            const auto findings = somavr::injector::ScanCompatibilityDirectory(absoluteGame.parent_path());
            if (findings.empty()) {
                DoctorResult(summary, "PASS", L"no known game-directory graphics or injection proxies found");
            } else {
                somavr::injector::PrintCompatibilityFindings(findings);
                DoctorResult(summary, "WARN",
                    std::to_wstring(findings.size()) + L" game-directory compatibility warning(s) found");
            }
        }
    }

    std::wcout << L"SUMMARY: pass=" << summary.passed
               << L" warn=" << summary.warnings
               << L" fail=" << summary.failures << L"\n";
    return summary.failures == 0 ? 0 : 1;
}

void WarnIfConfigDllMismatch(const std::filesystem::path& dllPath)
{
    somavr::ConfigManager configManager;
    configManager.Initialize();
    const somavr::Config& config = configManager.Get();
    const DllBuildFlavor flavor = ReadDllBuildFlavor(dllPath);

    if (!config.openxrProbe) {
        return;
    }

    if (!flavor.known) {
        std::wcerr
            << L"WARNING: [OpenXR] Probe=1, but injector could not determine DLL build flavor for "
            << std::filesystem::absolute(dllPath).wstring() << L"\n";
        return;
    }

    if (!flavor.openxr) {
        std::wcerr
            << L"WARNING: [OpenXR] Probe=1, but selected DLL is not OpenXR-enabled.\n"
            << L"         DLL: " << std::filesystem::absolute(dllPath).wstring() << L"\n"
            << L"         Flavor: " << Widen(flavor.flavor.c_str()) << L" version=" << Widen(flavor.version.c_str()) << L"\n"
            << L"         Use D:\\Dev Debug\\SOMAVR\\build-openxr\\Release\\somavr_injector.exe with its default somavr.dll,\n"
            << L"         or pass D:\\Dev Debug\\SOMAVR\\build-openxr\\Release\\somavr.dll explicitly.\n";
    }
}

bool InjectDll(DWORD pid, const std::filesystem::path& dllPath)
{
    const std::wstring fullPath = std::filesystem::absolute(dllPath).wstring();
    if (!std::filesystem::exists(fullPath)) {
        std::wcerr << L"DLL does not exist: " << fullPath << L"\n";
        return false;
    }
    WarnIfConfigDllMismatch(fullPath);
    const std::vector<somavr::injector::CompatibilityFinding> findings =
        somavr::injector::ScanCompatibility(pid);
    if (somavr::injector::PrintCompatibilityFindings(findings)) {
        std::wcerr << L"Injection refused because SOMAVR is already loaded in the target process\n";
        return false;
    }

    HANDLE process = OpenProcess(
        PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
        FALSE,
        pid);
    if (process == nullptr) {
        std::wcerr << L"OpenProcess failed: " << ErrorMessage(GetLastError()) << L"\n";
        return false;
    }

    const SIZE_T bytes = (fullPath.size() + 1) * sizeof(wchar_t);
    void* remotePath = VirtualAllocEx(process, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (remotePath == nullptr) {
        std::wcerr << L"VirtualAllocEx failed: " << ErrorMessage(GetLastError()) << L"\n";
        CloseHandle(process);
        return false;
    }

    SIZE_T written = 0;
    if (!WriteProcessMemory(process, remotePath, fullPath.c_str(), bytes, &written) || written != bytes) {
        std::wcerr << L"WriteProcessMemory failed: " << ErrorMessage(GetLastError()) << L"\n";
        VirtualFreeEx(process, remotePath, 0, MEM_RELEASE);
        CloseHandle(process);
        return false;
    }

    HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
    auto* loadLibrary = reinterpret_cast<LPTHREAD_START_ROUTINE>(GetProcAddress(kernel32, "LoadLibraryW"));
    if (loadLibrary == nullptr) {
        std::wcerr << L"GetProcAddress(LoadLibraryW) failed\n";
        VirtualFreeEx(process, remotePath, 0, MEM_RELEASE);
        CloseHandle(process);
        return false;
    }

    HANDLE thread = CreateRemoteThread(process, nullptr, 0, loadLibrary, remotePath, 0, nullptr);
    if (thread == nullptr) {
        std::wcerr << L"CreateRemoteThread failed: " << ErrorMessage(GetLastError()) << L"\n";
        VirtualFreeEx(process, remotePath, 0, MEM_RELEASE);
        CloseHandle(process);
        return false;
    }

    const DWORD waitResult = WaitForSingleObject(thread, 10000);
    if (waitResult != WAIT_OBJECT_0) {
        if (waitResult == WAIT_TIMEOUT) {
            std::wcerr
                << L"Remote LoadLibraryW did not complete within 10 seconds; "
                   L"the remote path page is intentionally retained\n";
        } else {
            std::wcerr << L"Waiting for remote LoadLibraryW failed: "
                       << ErrorMessage(GetLastError())
                       << L"; the remote path page is intentionally retained\n";
        }
        CloseHandle(thread);
        CloseHandle(process);
        return false;
    }

    DWORD remoteModule = 0;
    const bool exitCodeRead = GetExitCodeThread(thread, &remoteModule) != FALSE;
    CloseHandle(thread);
    VirtualFreeEx(process, remotePath, 0, MEM_RELEASE);
    CloseHandle(process);

    if (!exitCodeRead) {
        std::wcerr << L"GetExitCodeThread failed after remote LoadLibraryW completed\n";
        return false;
    }

    const auto loadedFindings = somavr::injector::ScanCompatibility(pid);
    const bool modulePresent = std::any_of(
        loadedFindings.begin(), loadedFindings.end(),
        [](const somavr::injector::CompatibilityFinding& finding) {
            return finding.source == L"loaded_module"
                && _wcsicmp(finding.path.filename().c_str(), L"somavr.dll") == 0;
        });
    if (!modulePresent) {
        std::wcerr << L"Remote LoadLibraryW completed but somavr.dll was not found in the target module list"
                   << L" (threadExitLow32=0x" << std::hex << remoteModule << std::dec << L")\n";
        return false;
    }

    std::wcout << L"Injected and verified " << fullPath << L" in pid " << pid << L"\n";
    return true;
}

bool LaunchSuspendedAndInject(
    const std::filesystem::path& requestedExePath,
    const std::filesystem::path& dllPath,
    int argc,
    char** argv,
    int firstGameArg)
{
    const std::filesystem::path exePath = std::filesystem::absolute(requestedExePath);
    if (!std::filesystem::exists(exePath)) {
        std::wcerr << L"Game executable does not exist: " << exePath.wstring() << L"\n";
        return false;
    }

    const std::filesystem::path workDir = exePath.parent_path();
    std::wstring commandLine = BuildLaunchCommandLine(exePath, argc, argv, firstGameArg);

    STARTUPINFOW startupInfo = {};
    startupInfo.cb = sizeof(startupInfo);

    PROCESS_INFORMATION processInfo = {};
    if (!CreateProcessW(
            exePath.c_str(),
            commandLine.data(),
            nullptr,
            nullptr,
            FALSE,
            CREATE_SUSPENDED,
            nullptr,
            workDir.empty() ? nullptr : workDir.c_str(),
            &startupInfo,
            &processInfo)) {
        std::wcerr << L"CreateProcessW failed: " << ErrorMessage(GetLastError()) << L"\n";
        return false;
    }

    std::wcout << L"Launched suspended pid " << processInfo.dwProcessId << L": " << exePath.wstring() << L"\n";

    const bool injected = InjectDll(processInfo.dwProcessId, dllPath);
    if (!injected) {
        std::wcerr << L"Injection failed; terminating suspended process " << processInfo.dwProcessId << L"\n";
        TerminateProcess(processInfo.hProcess, 1);
        CloseHandle(processInfo.hThread);
        CloseHandle(processInfo.hProcess);
        return false;
    }

    if (ResumeThread(processInfo.hThread) == static_cast<DWORD>(-1)) {
        std::wcerr << L"ResumeThread failed: " << ErrorMessage(GetLastError()) << L"\n";
        TerminateProcess(processInfo.hProcess, 1);
        CloseHandle(processInfo.hThread);
        CloseHandle(processInfo.hProcess);
        return false;
    }

    std::wcout << L"Resumed injected process " << processInfo.dwProcessId << L"\n";

    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);
    return true;
}

void PrintUsage()
{
    std::cerr
        << "usage:\n"
        << "  somavr_injector <pid|Soma_NoSteam.exe|SOMA.exe> [path-to-somavr.dll]\n"
        << "  somavr_injector --launch <path-to-Soma_NoSteam.exe> [path-to-somavr.dll] [-- game-args...]\n"
        << "  somavr_injector --doctor [path-to-Soma_NoSteam.exe] [path-to-somavr.dll]\n";
}

} // namespace

int main(int argc, char** argv)
{
    if (argc >= 2 && (IsOption(argv[1], "--help") || IsOption(argv[1], "-h") || IsOption(argv[1], "/?"))) {
        PrintUsage();
        return 0;
    }

    if (argc >= 2 && IsOption(argv[1], "--doctor")) {
        if (argc > 4) {
            PrintUsage();
            return 2;
        }
        const std::filesystem::path gamePath = argc >= 3
            ? std::filesystem::path(Widen(argv[2])) : std::filesystem::path{};
        const std::filesystem::path dllPath = argc >= 4
            ? std::filesystem::path(Widen(argv[3])) : DefaultDllPath(argv[0]);
        return RunDoctor(dllPath, gamePath);
    }

    if (argc >= 3 && IsOption(argv[1], "--launch")) {
        int nextArg = 3;
        std::filesystem::path dllPath = DefaultDllPath(argv[0]);
        if (nextArg < argc && !IsOption(argv[nextArg], "--")) {
            dllPath = std::filesystem::path(Widen(argv[nextArg]));
            ++nextArg;
        }

        if (nextArg < argc && IsOption(argv[nextArg], "--")) {
            ++nextArg;
        }

        return LaunchSuspendedAndInject(std::filesystem::path(Widen(argv[2])), dllPath, argc, argv, nextArg) ? 0 : 1;
    }

    if (argc < 2 || argc > 3) {
        PrintUsage();
        return 2;
    }

    DWORD pid = 0;
    if (IsDecimal(argv[1])) {
        pid = static_cast<DWORD>(std::stoul(argv[1]));
    } else {
        pid = FindProcessIdByName(Widen(argv[1]));
    }

    if (pid == 0) {
        std::cerr << "target process not found\n";
        return 3;
    }

    const auto dllPath = argc == 3 ? std::filesystem::path(Widen(argv[2])) : DefaultDllPath(argv[0]);
    return InjectDll(pid, dllPath) ? 0 : 1;
}
