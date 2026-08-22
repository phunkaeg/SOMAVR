#include "Logger.h"

#include <Windows.h>

#include <Shlwapi.h>

#include <chrono>
#include <cstdio>
#include <iomanip>
#include <mutex>
#include <share.h>
#include <sstream>

namespace somavr {
namespace {

std::mutex g_workRootMutex;
std::filesystem::path g_workRoot;
std::string g_workRootSource = "uninitialized";

std::string NowString()
{
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::tm local = {};
    localtime_s(&local, &time);

    std::ostringstream out;
    out << std::put_time(&local, "%Y-%m-%d %H:%M:%S");
    return out.str();
}

std::string Normalize(std::string value)
{
    for (char& ch : value) {
        if (ch >= 'A' && ch <= 'Z') {
            ch = static_cast<char>(ch - 'A' + 'a');
        }
    }
    return value;
}

} // namespace

Logger& Logger::Instance()
{
    static Logger logger;
    return logger;
}

void Logger::Initialize(const std::filesystem::path& logPath, LogLevel level)
{
    std::lock_guard lock(mutex_);

    logPath_ = logPath;
    level_ = level;
    fastLevel_.store(static_cast<int>(level), std::memory_order_relaxed);

    std::error_code ec;
    std::filesystem::create_directories(logPath_.parent_path(), ec);

    logStream_.close();
    bool previousLogPreserved = false;
    std::filesystem::path previousLogPath;
    std::error_code rotationError;
    if (!initialized_
        && std::filesystem::exists(logPath_, rotationError)
        && !rotationError
        && std::filesystem::file_size(logPath_, rotationError) > 0
        && !rotationError) {
        previousLogPath = logPath_.parent_path()
            / (logPath_.stem().wstring() + L".previous" + logPath_.extension().wstring());
        std::error_code removeError;
        std::filesystem::remove(previousLogPath, removeError);
        if (!removeError) {
            std::filesystem::rename(logPath_, previousLogPath, rotationError);
            previousLogPreserved = !rotationError;
        } else {
            rotationError = removeError;
        }
    }
    logStream_.open(logPath_, std::ios::out | std::ios::trunc, _SH_DENYNO);
    pendingBufferedLines_ = 0;
    if (logStream_.is_open()) {
        logStream_ << "SOMAVR log started " << NowString() << "\n";
        if (previousLogPreserved) {
            logStream_ << "SOMAVR previous log preserved "
                       << previousLogPath.string() << "\n";
        } else if (rotationError) {
            logStream_ << "SOMAVR previous log preservation failed error="
                       << rotationError.value() << " message=\""
                       << rotationError.message() << "\"\n";
        }
        logStream_.flush();
    }
    initialized_ = true;
}

void Logger::Shutdown()
{
    std::lock_guard lock(mutex_);
    if (logStream_.is_open()) {
        logStream_.flush();
        logStream_.close();
    }
    pendingBufferedLines_ = 0;
    initialized_ = false;
}

void Logger::SetLevel(LogLevel level)
{
    std::lock_guard lock(mutex_);
    level_ = level;
    fastLevel_.store(static_cast<int>(level), std::memory_order_relaxed);
}

LogLevel Logger::GetLevel() const
{
    std::lock_guard lock(mutex_);
    return level_;
}

void Logger::Write(LogLevel level, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    WriteV(level, fmt, args);
    va_end(args);
}

void Logger::WriteV(LogLevel level, const char* fmt, va_list args)
{
    if (static_cast<int>(level) < fastLevel_.load(std::memory_order_relaxed)) {
        return;
    }

    char message[4096] = {};
    // _TRUNCATE returns -1 when the line did not fit. Dropping the tail in
    // silence is the worst outcome for a config or identity dump, where the
    // truncated part is exactly the evidence the line is being read for.
    const int formatted = vsnprintf_s(message, sizeof(message), _TRUNCATE, fmt, args);
    const bool truncated = formatted < 0;
    if (truncated) {
        truncatedLines_.fetch_add(1, std::memory_order_relaxed);
    }

    std::ostringstream line;
    line << NowString() << " [" << LevelName(level) << "] " << message;
    if (truncated) {
        line << " [log_truncated bufferBytes=" << sizeof(message)
             << " totalTruncated="
             << truncatedLines_.load(std::memory_order_relaxed) << "]";
    }
    line << "\n";
    const std::string text = line.str();

    {
        std::lock_guard lock(mutex_);
        if (!initialized_ || level_ == LogLevel::Off || static_cast<int>(level) < static_cast<int>(level_)) {
            return;
        }

        if (!logStream_.is_open() && !logPath_.empty()) {
            logStream_.open(logPath_, std::ios::out | std::ios::app, _SH_DENYNO);
        }
        if (logStream_.is_open()) {
            logStream_ << text;
            ++pendingBufferedLines_;
            if (static_cast<int>(level) >= static_cast<int>(LogLevel::Warn)
                || pendingBufferedLines_ >= 64) {
                logStream_.flush();
                pendingBufferedLines_ = 0;
            }
        }
    }

    if (static_cast<int>(level) >= static_cast<int>(LogLevel::Warn)) {
        OutputDebugStringA(text.c_str());
    }
}

std::filesystem::path Logger::Path() const
{
    std::lock_guard lock(mutex_);
    return logPath_;
}

uint64_t Logger::TruncatedLineCount() const
{
    return truncatedLines_.load(std::memory_order_relaxed);
}

LogLevel Logger::ParseLevel(const std::string& value, LogLevel fallback)
{
    const std::string normalized = Normalize(value);
    if (normalized == "trace") return LogLevel::Trace;
    if (normalized == "debug") return LogLevel::Debug;
    if (normalized == "info") return LogLevel::Info;
    if (normalized == "warn" || normalized == "warning") return LogLevel::Warn;
    if (normalized == "error") return LogLevel::Error;
    if (normalized == "off" || normalized == "none") return LogLevel::Off;
    return fallback;
}

const char* Logger::LevelName(LogLevel level)
{
    switch (level) {
    case LogLevel::Trace: return "trace";
    case LogLevel::Debug: return "debug";
    case LogLevel::Info: return "info";
    case LogLevel::Warn: return "warn";
    case LogLevel::Error: return "error";
    case LogLevel::Off: return "off";
    }
    return "unknown";
}

void InitializeWorkRoot(HMODULE module)
{
    std::lock_guard lock(g_workRootMutex);
    if (!g_workRoot.empty()) return;

    std::wstring overridePath(32768, L'\0');
    const DWORD overrideLength = GetEnvironmentVariableW(
        L"SOMAVR_ROOT", overridePath.data(), static_cast<DWORD>(overridePath.size()));
    if (overrideLength > 0 && overrideLength < overridePath.size()) {
        overridePath.resize(overrideLength);
        g_workRoot = std::filesystem::path(overridePath).lexically_normal();
        g_workRootSource = "environment:SOMAVR_ROOT";
        return;
    }

    const std::filesystem::path modulePath = ModulePath(module);
    if (!modulePath.empty() && modulePath.has_parent_path()) {
        g_workRoot = modulePath.parent_path().lexically_normal();
        g_workRootSource = module != nullptr ? "module" : "executable";
        return;
    }

    std::wstring localAppData(32768, L'\0');
    const DWORD localAppDataLength = GetEnvironmentVariableW(
        L"LOCALAPPDATA", localAppData.data(), static_cast<DWORD>(localAppData.size()));
    if (localAppDataLength > 0 && localAppDataLength < localAppData.size()) {
        localAppData.resize(localAppDataLength);
        g_workRoot = (std::filesystem::path(localAppData) / L"SOMAVR").lexically_normal();
        g_workRootSource = "localappdata";
        return;
    }

#ifdef SOMAVR_DEV_WORK_ROOT
    g_workRoot = std::filesystem::path(SOMAVR_DEV_WORK_ROOT).lexically_normal();
    g_workRootSource = "compiled-dev-override";
#else
    g_workRoot = std::filesystem::current_path().lexically_normal();
    g_workRootSource = "current-directory-fallback";
#endif
}

std::filesystem::path WorkRoot()
{
    InitializeWorkRoot(nullptr);
    std::lock_guard lock(g_workRootMutex);
    return g_workRoot;
}

std::string WorkRootSource()
{
    InitializeWorkRoot(nullptr);
    std::lock_guard lock(g_workRootMutex);
    return g_workRootSource;
}

std::filesystem::path LogPath()
{
    return WorkRoot() / "logs" / "somavr.log";
}

std::filesystem::path ConfigPath()
{
    return WorkRoot() / "somavr.ini";
}

std::wstring ToWide(const std::string& value)
{
    if (value.empty()) {
        return {};
    }

    const int length = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
    if (length <= 1) {
        return {};
    }

    std::wstring result(static_cast<size_t>(length - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, result.data(), length);
    return result;
}

std::string ToUtf8(const std::wstring& value)
{
    if (value.empty()) {
        return {};
    }

    const int length = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (length <= 1) {
        return {};
    }

    std::string result(static_cast<size_t>(length - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, result.data(), length, nullptr, nullptr);
    return result;
}

std::string ModulePath(HMODULE module)
{
    wchar_t path[MAX_PATH] = {};
    const DWORD size = GetModuleFileNameW(module, path, MAX_PATH);
    if (size == 0 || size == MAX_PATH) {
        return {};
    }
    return ToUtf8(std::wstring(path, size));
}

} // namespace somavr
