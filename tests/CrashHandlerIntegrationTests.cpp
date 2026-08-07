#include "BuildInfo.h"
#include "CrashHandler.h"
#include "Logger.h"

#include <Windows.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {

std::string ReadAll(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    std::ostringstream output;
    output << input.rdbuf();
    return output.str();
}

int RunChild(const std::filesystem::path& root)
{
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    somavr::Logger::Instance().Initialize(root / L"somavr.log", somavr::LogLevel::Info);
    somavr::LogBuildIdentity(GetModuleHandleW(nullptr));
    somavr::InitializeCrashHandler(GetModuleHandleW(nullptr), root / L"dumps");

    ULONG_PTR arguments[2] = {1, 0};
    RaiseException(EXCEPTION_ACCESS_VIOLATION, EXCEPTION_NONCONTINUABLE, 2, arguments);
    return 99;
}

} // namespace

int wmain(int argc, wchar_t** argv)
{
    if (argc == 3 && _wcsicmp(argv[1], L"--child") == 0) {
        return RunChild(argv[2]);
    }

    const std::filesystem::path root = std::filesystem::temp_directory_path()
        / (L"somavr-crash-test-" + std::to_wstring(GetCurrentProcessId()));
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    std::filesystem::create_directories(root, ec);
    if (ec) {
        std::cerr << "FAILED: create crash test directory\n";
        return 1;
    }

    wchar_t executable[2048]{};
    const DWORD executableLength = GetModuleFileNameW(nullptr, executable, std::size(executable));
    if (executableLength == 0 || executableLength >= std::size(executable)) {
        std::cerr << "FAILED: resolve test executable\n";
        return 1;
    }
    std::wstring command = L"\"" + std::wstring(executable) + L"\" --child \""
        + root.wstring() + L"\"";
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(
            executable,
            command.data(),
            nullptr,
            nullptr,
            FALSE,
            CREATE_NO_WINDOW,
            nullptr,
            nullptr,
            &startup,
            &process)) {
        std::cerr << "FAILED: launch child error=" << GetLastError() << '\n';
        return 1;
    }
    CloseHandle(process.hThread);
    const DWORD waitResult = WaitForSingleObject(process.hProcess, 30000);
    DWORD exitCode = STILL_ACTIVE;
    GetExitCodeProcess(process.hProcess, &exitCode);
    if (waitResult == WAIT_TIMEOUT) {
        TerminateProcess(process.hProcess, 98);
        WaitForSingleObject(process.hProcess, 5000);
    }
    CloseHandle(process.hProcess);

    size_t dumpCount = 0;
    const std::filesystem::path dumpDirectory = root / L"dumps";
    if (std::filesystem::exists(dumpDirectory)) {
        for (const auto& entry : std::filesystem::directory_iterator(dumpDirectory)) {
            if (entry.is_regular_file() && entry.path().extension() == L".dmp"
                && entry.file_size() > 0) {
                ++dumpCount;
            }
        }
    }
    const std::string crashLog = ReadAll(dumpDirectory / L"somavr-crash.log");
    const bool logOk = crashLog.find("capture begin source=unhandled") != std::string::npos
        && crashLog.find("dump result=written") != std::string::npos;
    const bool passed = waitResult == WAIT_OBJECT_0
        && exitCode != 0
        && exitCode != STILL_ACTIVE
        && dumpCount == 1
        && logOk;
    if (!passed) {
        std::cerr << "FAILED: wait=" << waitResult
                  << " exit=0x" << std::hex << exitCode << std::dec
                  << " dumps=" << dumpCount
                  << " log=" << logOk << '\n';
        std::cerr << crashLog;
    }
    std::filesystem::remove_all(root, ec);
    if (!passed) {
        return 1;
    }
    std::cout << "Crash handler integration test passed\n";
    return 0;
}
