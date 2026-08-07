#pragma once

#include <Windows.h>

#include <atomic>
#include <cstdarg>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>

namespace somavr {

enum class LogLevel {
    Trace = 0,
    Debug = 1,
    Info = 2,
    Warn = 3,
    Error = 4,
    Off = 5,
};

class Logger {
public:
    static Logger& Instance();

    void Initialize(const std::filesystem::path& logPath, LogLevel level);
    void Shutdown();
    void SetLevel(LogLevel level);
    LogLevel GetLevel() const;

    void Write(LogLevel level, const char* fmt, ...);
    void WriteV(LogLevel level, const char* fmt, va_list args);

    std::filesystem::path Path() const;

    static LogLevel ParseLevel(const std::string& value, LogLevel fallback);
    static const char* LevelName(LogLevel level);

private:
    mutable std::mutex mutex_;
    std::filesystem::path logPath_;
    std::ofstream logStream_;
    LogLevel level_ = LogLevel::Info;
    std::atomic<int> fastLevel_ = static_cast<int>(LogLevel::Info);
    uint32_t pendingBufferedLines_ = 0;
    bool initialized_ = false;
};

std::filesystem::path WorkRoot();
void InitializeWorkRoot(HMODULE module);
std::string WorkRootSource();
std::filesystem::path LogPath();
std::filesystem::path ConfigPath();

std::wstring ToWide(const std::string& value);
std::string ToUtf8(const std::wstring& value);
std::string ModulePath(HMODULE module);

} // namespace somavr
