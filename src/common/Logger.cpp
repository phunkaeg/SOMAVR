#include "Logger.h"

#include <Windows.h>

#include <Shlwapi.h>

#include <chrono>
#include <cstdio>
#include <iomanip>
#include <share.h>
#include <sstream>

namespace somavr {
namespace {

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
    logStream_.open(logPath_, std::ios::out | std::ios::trunc, _SH_DENYNO);
    pendingBufferedLines_ = 0;
    if (logStream_.is_open()) {
        logStream_ << "SOMAVR log started " << NowString() << "\n";
        logStream_.flush();
    }
    initialized_ = true;
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
    vsnprintf_s(message, sizeof(message), _TRUNCATE, fmt, args);

    std::ostringstream line;
    line << NowString() << " [" << LevelName(level) << "] " << message << "\n";
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

const std::filesystem::path& Logger::Path() const
{
    return logPath_;
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

std::filesystem::path WorkRoot()
{
#ifdef SOMAVR_WORK_ROOT
    return std::filesystem::path(SOMAVR_WORK_ROOT);
#else
    return std::filesystem::current_path();
#endif
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
