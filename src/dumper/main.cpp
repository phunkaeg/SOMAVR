#include "Logger.h"

#include <Windows.h>
#include <DbgHelp.h>
#include <TlHelp32.h>

#include <filesystem>
#include <iostream>
#include <string>

namespace {

DWORD FindProcessIdByName(const std::wstring& processName)
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return 0;
    }

    PROCESSENTRY32W entry = {};
    entry.dwSize = sizeof(entry);
    DWORD processId = 0;
    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, processName.c_str()) == 0) {
                processId = entry.th32ProcessID;
                break;
            }
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return processId;
}

DWORD ResolveProcessId(const wchar_t* value)
{
    if (value == nullptr || *value == L'\0') {
        return 0;
    }
    wchar_t* end = nullptr;
    const unsigned long parsed = std::wcstoul(value, &end, 10);
    if (end != value && *end == L'\0') {
        return static_cast<DWORD>(parsed);
    }
    return FindProcessIdByName(value);
}

} // namespace

int wmain(int argc, wchar_t** argv)
{
    if (argc < 2) {
        std::wcerr
            << L"usage: somavr_dumper <pid|Soma_NoSteam.exe> [output.dmp] [--full]\n";
        return 2;
    }

    const DWORD processId = ResolveProcessId(argv[1]);
    if (processId == 0) {
        std::wcerr << L"process not found: " << argv[1] << L"\n";
        return 3;
    }

    bool fullDump = false;
    std::filesystem::path outputPath;
    for (int index = 2; index < argc; ++index) {
        if (_wcsicmp(argv[index], L"--full") == 0) {
            fullDump = true;
        } else if (outputPath.empty()) {
            outputPath = argv[index];
        }
    }
    if (outputPath.empty()) {
        outputPath = somavr::WorkRoot()
            / L"logs"
            / L"dumps"
            / (L"Soma_NoSteam-" + std::to_wstring(processId) + L".dmp");
    }

    std::error_code ec;
    std::filesystem::create_directories(outputPath.parent_path(), ec);
    HANDLE process = OpenProcess(
        PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_DUP_HANDLE,
        FALSE,
        processId);
    if (process == nullptr) {
        std::wcerr << L"OpenProcess failed error=" << GetLastError() << L"\n";
        return 4;
    }

    HANDLE output = CreateFileW(
        outputPath.c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (output == INVALID_HANDLE_VALUE) {
        std::wcerr << L"CreateFile failed error=" << GetLastError()
                   << L" path=" << outputPath.wstring() << L"\n";
        CloseHandle(process);
        return 5;
    }

    MINIDUMP_TYPE type = static_cast<MINIDUMP_TYPE>(
        MiniDumpNormal
        | MiniDumpWithThreadInfo
        | MiniDumpWithUnloadedModules
        | MiniDumpWithIndirectlyReferencedMemory);
    if (fullDump) {
        type = static_cast<MINIDUMP_TYPE>(type | MiniDumpWithFullMemory);
    }

    const BOOL written = MiniDumpWriteDump(
        process,
        processId,
        output,
        type,
        nullptr,
        nullptr,
        nullptr);
    const DWORD error = written ? ERROR_SUCCESS : GetLastError();
    CloseHandle(output);
    CloseHandle(process);

    if (!written) {
        std::wcerr << L"MiniDumpWriteDump failed error=" << error << L"\n";
        return 6;
    }

    std::wcout << L"dump written pid=" << processId
               << L" full=" << (fullDump ? 1 : 0)
               << L" path=" << std::filesystem::absolute(outputPath).wstring()
               << L"\n";
    return 0;
}
