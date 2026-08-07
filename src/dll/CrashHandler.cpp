#include "CrashHandler.h"

#include "BuildInfo.h"
#include "CrashCapturePolicy.h"
#include "Logger.h"

#include <DbgHelp.h>

#include <algorithm>
#include <atomic>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>

namespace somavr {
namespace {

constexpr DWORD kStatusHeapCorruption = 0xC0000374u;
constexpr DWORD kStatusStackBufferOverrun = 0xC0000409u;
constexpr uint64_t kFilterMaintenanceIntervalMs = 5000;

HMODULE g_module = nullptr;
PVOID g_vectoredHandle = nullptr;
std::wstring g_dumpDirectory;
std::wstring g_crashLogPath;
std::wstring g_processStem = L"Soma_NoSteam";
std::string g_buildIdentity;
std::atomic<bool> g_initialized{false};
std::atomic<LPTOP_LEVEL_EXCEPTION_FILTER> g_displacedFilter{nullptr};
std::atomic<uint64_t> g_nextMaintenanceMs{0};
std::atomic<uint64_t> g_maintenanceChecks{0};
std::atomic<uint64_t> g_filterRearms{0};
std::atomic<uint64_t> g_dumpWrites{0};
std::atomic<uint64_t> g_dumpFailures{0};
std::atomic<uint64_t> g_busySuppressions{0};
std::atomic<uint64_t> g_duplicateSuppressions{0};
std::atomic<uint64_t> g_limitSuppressions{0};
crash_capture::Policy g_capturePolicy{3};

LONG WINAPI TopLevelExceptionFilter(EXCEPTION_POINTERS* exceptionPointers);

void DirectLog(const char* format, ...)
{
    char message[3584]{};
    va_list args;
    va_start(args, format);
    vsnprintf_s(message, sizeof(message), _TRUNCATE, format, args);
    va_end(args);

    SYSTEMTIME time{};
    GetSystemTime(&time);
    char line[4096]{};
    const int length = snprintf(
        line,
        sizeof(line),
        "%04u-%02u-%02uT%02u:%02u:%02u.%03uZ %s\r\n",
        time.wYear,
        time.wMonth,
        time.wDay,
        time.wHour,
        time.wMinute,
        time.wSecond,
        time.wMilliseconds,
        message);
    if (length <= 0) {
        return;
    }

    OutputDebugStringA(line);
    if (g_crashLogPath.empty()) {
        return;
    }
    HANDLE file = CreateFileW(
        g_crashLogPath.c_str(),
        FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }
    DWORD written = 0;
    WriteFile(file, line, static_cast<DWORD>(std::min<int>(length, sizeof(line) - 1)), &written, nullptr);
    FlushFileBuffers(file);
    CloseHandle(file);
}

bool ResolveAddressModule(
    uintptr_t address,
    HMODULE& module,
    wchar_t* path,
    size_t pathCount,
    uintptr_t& relativeAddress)
{
    module = nullptr;
    relativeAddress = 0;
    if (address == 0 || path == nullptr || pathCount == 0) {
        return false;
    }
    if (!GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(address),
            &module)) {
        return false;
    }
    const DWORD length = GetModuleFileNameW(module, path, static_cast<DWORD>(pathCount));
    if (length == 0 || length >= pathCount) {
        path[0] = L'\0';
    }
    relativeAddress = address - reinterpret_cast<uintptr_t>(module);
    return true;
}

void LogExceptionAddress(uintptr_t address)
{
    HMODULE module = nullptr;
    wchar_t path[1024]{};
    uintptr_t rva = 0;
    if (ResolveAddressModule(address, module, path, std::size(path), rva)) {
        DirectLog(
            "fault module=%ls base=%p rva=0x%llx address=%p",
            path[0] != L'\0' ? path : L"unknown",
            module,
            static_cast<unsigned long long>(rva),
            reinterpret_cast<void*>(address));
    } else {
        DirectLog("fault module=unresolved address=%p", reinterpret_cast<void*>(address));
    }
}

void LogAccessViolation(const EXCEPTION_RECORD& record)
{
    if ((record.ExceptionCode != EXCEPTION_ACCESS_VIOLATION
            && record.ExceptionCode != EXCEPTION_IN_PAGE_ERROR)
        || record.NumberParameters < 2) {
        return;
    }
    const ULONG_PTR operation = record.ExceptionInformation[0];
    const char* operationName = operation == 0 ? "read"
        : operation == 1 ? "write"
        : operation == 8 ? "execute"
        : "unknown";
    DirectLog(
        "access operation=%s operationCode=%llu target=%p",
        operationName,
        static_cast<unsigned long long>(operation),
        reinterpret_cast<void*>(record.ExceptionInformation[1]));
}

void LogContext(const CONTEXT* context)
{
    if (context == nullptr) {
        return;
    }
#if defined(_M_X64)
    DirectLog(
        "registers rip=%p rsp=%p rbp=%p rax=%p rbx=%p rcx=%p rdx=%p rsi=%p rdi=%p r8=%p r9=%p r10=%p r11=%p r12=%p r13=%p r14=%p r15=%p",
        reinterpret_cast<void*>(context->Rip),
        reinterpret_cast<void*>(context->Rsp),
        reinterpret_cast<void*>(context->Rbp),
        reinterpret_cast<void*>(context->Rax),
        reinterpret_cast<void*>(context->Rbx),
        reinterpret_cast<void*>(context->Rcx),
        reinterpret_cast<void*>(context->Rdx),
        reinterpret_cast<void*>(context->Rsi),
        reinterpret_cast<void*>(context->Rdi),
        reinterpret_cast<void*>(context->R8),
        reinterpret_cast<void*>(context->R9),
        reinterpret_cast<void*>(context->R10),
        reinterpret_cast<void*>(context->R11),
        reinterpret_cast<void*>(context->R12),
        reinterpret_cast<void*>(context->R13),
        reinterpret_cast<void*>(context->R14),
        reinterpret_cast<void*>(context->R15));

    MEMORY_BASIC_INFORMATION memory{};
    if (VirtualQuery(reinterpret_cast<void*>(context->Rsp), &memory, sizeof(memory)) == 0
        || memory.State != MEM_COMMIT
        || (memory.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) {
        DirectLog("stack_candidates unavailable rsp=%p", reinterpret_cast<void*>(context->Rsp));
        return;
    }
    const uintptr_t regionEnd = reinterpret_cast<uintptr_t>(memory.BaseAddress) + memory.RegionSize;
    const uintptr_t stackStart = context->Rsp;
    const size_t availableEntries = regionEnd > stackStart
        ? static_cast<size_t>((regionEnd - stackStart) / sizeof(uintptr_t)) : 0;
    const size_t entries = std::min<size_t>(availableEntries, 96);
    size_t logged = 0;
    for (size_t index = 0; index < entries && logged < 24; ++index) {
        uintptr_t candidate = 0;
        __try {
            candidate = reinterpret_cast<const uintptr_t*>(stackStart)[index];
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            break;
        }
        HMODULE module = nullptr;
        wchar_t path[1024]{};
        uintptr_t rva = 0;
        if (!ResolveAddressModule(candidate, module, path, std::size(path), rva)) {
            continue;
        }
        DirectLog(
            "stack_candidate index=%zu stack=%p value=%p module=%ls rva=0x%llx",
            index,
            reinterpret_cast<void*>(stackStart + index * sizeof(uintptr_t)),
            reinterpret_cast<void*>(candidate),
            path[0] != L'\0' ? path : L"unknown",
            static_cast<unsigned long long>(rva));
        ++logged;
    }
#else
    DirectLog("registers unsupported_architecture");
#endif
}

bool FullDumpRequested()
{
    wchar_t value[16]{};
    const DWORD length = GetEnvironmentVariableW(L"SOMAVR_FULLDUMP", value, std::size(value));
    return length > 0
        && (_wcsicmp(value, L"1") == 0
            || _wcsicmp(value, L"true") == 0
            || _wcsicmp(value, L"yes") == 0);
}

bool WriteDump(EXCEPTION_POINTERS* exceptionPointers, uint32_t sequence)
{
    SYSTEMTIME time{};
    GetSystemTime(&time);
    wchar_t outputPath[2048]{};
    swprintf_s(
        outputPath,
        L"%ls\\%ls-crash-%lu-%04u%02u%02uT%02u%02u%02u-%u.dmp",
        g_dumpDirectory.c_str(),
        g_processStem.c_str(),
        static_cast<unsigned long>(GetCurrentProcessId()),
        time.wYear,
        time.wMonth,
        time.wDay,
        time.wHour,
        time.wMinute,
        time.wSecond,
        sequence);

    HANDLE output = CreateFileW(
        outputPath,
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (output == INVALID_HANDLE_VALUE) {
        DirectLog("dump create_failed path=%ls error=%lu", outputPath, GetLastError());
        return false;
    }

    MINIDUMP_TYPE type = static_cast<MINIDUMP_TYPE>(
        MiniDumpWithDataSegs
        | MiniDumpWithHandleData
        | MiniDumpWithUnloadedModules
        | MiniDumpWithIndirectlyReferencedMemory
        | MiniDumpWithProcessThreadData
        | MiniDumpWithThreadInfo
        | MiniDumpWithFullMemoryInfo
        | MiniDumpWithTokenInformation);
    const bool fullDump = FullDumpRequested();
    if (fullDump) {
        type = static_cast<MINIDUMP_TYPE>(
            type | MiniDumpWithFullMemory | MiniDumpIgnoreInaccessibleMemory);
    }

    MINIDUMP_EXCEPTION_INFORMATION exceptionInfo{};
    exceptionInfo.ThreadId = GetCurrentThreadId();
    exceptionInfo.ExceptionPointers = exceptionPointers;
    exceptionInfo.ClientPointers = FALSE;
    const BOOL written = MiniDumpWriteDump(
        GetCurrentProcess(),
        GetCurrentProcessId(),
        output,
        type,
        exceptionPointers != nullptr ? &exceptionInfo : nullptr,
        nullptr,
        nullptr);
    const DWORD error = written ? ERROR_SUCCESS : GetLastError();
    FlushFileBuffers(output);
    CloseHandle(output);
    DirectLog(
        "dump result=%s full=%d path=%ls error=%lu",
        written ? "written" : "failed",
        fullDump ? 1 : 0,
        outputPath,
        static_cast<unsigned long>(error));
    return written != FALSE;
}

void CaptureException(EXCEPTION_POINTERS* exceptionPointers, const char* source)
{
    if (!g_initialized.load(std::memory_order_acquire)
        || exceptionPointers == nullptr
        || exceptionPointers->ExceptionRecord == nullptr) {
        return;
    }

    const EXCEPTION_RECORD& record = *exceptionPointers->ExceptionRecord;
    const uintptr_t address = reinterpret_cast<uintptr_t>(record.ExceptionAddress);
    const crash_capture::Decision decision = g_capturePolicy.Begin(record.ExceptionCode, address);
    if (decision != crash_capture::Decision::Capture) {
        if (decision == crash_capture::Decision::Busy) {
            ++g_busySuppressions;
        } else if (decision == crash_capture::Decision::Duplicate) {
            ++g_duplicateSuppressions;
        } else {
            ++g_limitSuppressions;
        }
        DirectLog(
            "capture suppressed source=%s decision=%d code=0x%08lx address=%p attempts=%u",
            source,
            static_cast<int>(decision),
            static_cast<unsigned long>(record.ExceptionCode),
            record.ExceptionAddress,
            g_capturePolicy.Attempts());
        return;
    }

    const uint32_t sequence = g_capturePolicy.Attempts();
    DirectLog(
        "capture begin source=%s sequence=%u code=0x%08lx flags=0x%08lx address=%p thread=%lu build=%s",
        source,
        sequence,
        static_cast<unsigned long>(record.ExceptionCode),
        static_cast<unsigned long>(record.ExceptionFlags),
        record.ExceptionAddress,
        static_cast<unsigned long>(GetCurrentThreadId()),
        g_buildIdentity.c_str());
    LogExceptionAddress(address);
    LogAccessViolation(record);
    LogContext(exceptionPointers->ContextRecord);
    if (WriteDump(exceptionPointers, sequence)) {
        ++g_dumpWrites;
    } else {
        ++g_dumpFailures;
    }
    DirectLog("capture end source=%s sequence=%u", source, sequence);
    g_capturePolicy.End();
}

bool IsAlwaysFatal(DWORD code)
{
    return code == kStatusHeapCorruption
        || code == kStatusStackBufferOverrun
        || code == EXCEPTION_STACK_OVERFLOW
        || code == EXCEPTION_ILLEGAL_INSTRUCTION
        || code == EXCEPTION_PRIV_INSTRUCTION
        || code == EXCEPTION_IN_PAGE_ERROR
        || code == EXCEPTION_NONCONTINUABLE_EXCEPTION;
}

LONG CALLBACK VectoredExceptionHandler(EXCEPTION_POINTERS* exceptionPointers)
{
    if (exceptionPointers != nullptr
        && exceptionPointers->ExceptionRecord != nullptr
        && IsAlwaysFatal(exceptionPointers->ExceptionRecord->ExceptionCode)) {
        CaptureException(exceptionPointers, "vectored_fatal");
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

LONG WINAPI TopLevelExceptionFilter(EXCEPTION_POINTERS* exceptionPointers)
{
    CaptureException(exceptionPointers, "unhandled");
    const auto displaced = g_displacedFilter.load(std::memory_order_acquire);
    if (displaced != nullptr && displaced != &TopLevelExceptionFilter) {
        __try {
            return displaced(exceptionPointers);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            DirectLog("displaced_filter raised exception filter=%p", displaced);
        }
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

} // namespace

void InitializeCrashHandler(HMODULE module, const std::filesystem::path& dumpDirectory)
{
    if (g_initialized.exchange(true, std::memory_order_acq_rel)) {
        return;
    }
    g_module = module;
    g_dumpDirectory = std::filesystem::absolute(dumpDirectory).wstring();
    std::error_code ec;
    std::filesystem::create_directories(g_dumpDirectory, ec);
    g_crashLogPath = (std::filesystem::path(g_dumpDirectory) / L"somavr-crash.log").wstring();
    wchar_t processPath[1024]{};
    const DWORD processPathLength = GetModuleFileNameW(nullptr, processPath, std::size(processPath));
    if (processPathLength > 0 && processPathLength < std::size(processPath)) {
        g_processStem = std::filesystem::path(processPath).stem().wstring();
    }
    g_buildIdentity = BuildIdentityString();
    g_vectoredHandle = AddVectoredExceptionHandler(1, &VectoredExceptionHandler);
    g_displacedFilter.store(
        SetUnhandledExceptionFilter(&TopLevelExceptionFilter),
        std::memory_order_release);
    g_nextMaintenanceMs.store(
        GetTickCount64() + kFilterMaintenanceIntervalMs,
        std::memory_order_relaxed);
    Logger::Instance().Write(
        LogLevel::Info,
        "crash_handler initialized vectored=%d topLevel=1 displaced=%p dumpDirectory=%s fullDumpEnv=SOMAVR_FULLDUMP maxAttempts=3 build=%s",
        g_vectoredHandle != nullptr ? 1 : 0,
        g_displacedFilter.load(std::memory_order_relaxed),
        ToUtf8(g_dumpDirectory).c_str(),
        g_buildIdentity.c_str());
}

void MaintainCrashHandler()
{
    if (!g_initialized.load(std::memory_order_acquire)) {
        return;
    }
    const uint64_t now = GetTickCount64();
    const uint64_t next = g_nextMaintenanceMs.load(std::memory_order_relaxed);
    if (now < next) {
        return;
    }
    g_nextMaintenanceMs.store(now + kFilterMaintenanceIntervalMs, std::memory_order_relaxed);
    ++g_maintenanceChecks;
    const auto previous = SetUnhandledExceptionFilter(&TopLevelExceptionFilter);
    if (previous != &TopLevelExceptionFilter) {
        g_displacedFilter.store(previous, std::memory_order_release);
        ++g_filterRearms;
        Logger::Instance().Write(
            LogLevel::Warn,
            "crash_handler top_level_filter_rearmed displaced=%p rearms=%llu",
            previous,
            static_cast<unsigned long long>(g_filterRearms.load(std::memory_order_relaxed)));
    }
}

void LogCrashHandlerSummary()
{
    Logger::Instance().Write(
        LogLevel::Info,
        "crash_handler_summary initialized=%d attempts=%u dumps=%llu failures=%llu busySuppressed=%llu duplicateSuppressed=%llu limitSuppressed=%llu maintenanceChecks=%llu filterRearms=%llu",
        g_initialized.load(std::memory_order_relaxed) ? 1 : 0,
        g_capturePolicy.Attempts(),
        static_cast<unsigned long long>(g_dumpWrites.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_dumpFailures.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_busySuppressions.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_duplicateSuppressions.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_limitSuppressions.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_maintenanceChecks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_filterRearms.load(std::memory_order_relaxed)));
}

void ShutdownCrashHandler()
{
    if (!g_initialized.exchange(false, std::memory_order_acq_rel)) {
        return;
    }
    if (g_vectoredHandle != nullptr) {
        RemoveVectoredExceptionHandler(g_vectoredHandle);
        g_vectoredHandle = nullptr;
    }
    const auto displaced = g_displacedFilter.load(std::memory_order_acquire);
    const auto current = SetUnhandledExceptionFilter(displaced);
    if (current != &TopLevelExceptionFilter) {
        SetUnhandledExceptionFilter(current);
    }
    g_module = nullptr;
}

} // namespace somavr
