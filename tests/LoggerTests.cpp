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

} // namespace

int main()
{
    const std::filesystem::path root = std::filesystem::temp_directory_path()
        / (L"somavr-logger-test-" + std::to_wstring(GetCurrentProcessId()));
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    std::filesystem::create_directories(root, ec);
    if (ec) {
        std::cerr << "FAILED: create test directory\n";
        return 1;
    }

    SetEnvironmentVariableW(L"SOMAVR_ROOT", root.c_str());
    somavr::InitializeWorkRoot(nullptr);
    const bool workRootOk = somavr::WorkRoot() == root.lexically_normal()
        && somavr::WorkRootSource() == "environment:SOMAVR_ROOT";

    const std::filesystem::path current = root / L"somavr.log";
    const std::filesystem::path previous = root / L"somavr.previous.log";
    {
        std::ofstream seed(current, std::ios::binary);
        seed << "old-session-marker\n";
    }

    somavr::Logger::Instance().Initialize(current, somavr::LogLevel::Info);
    somavr::Logger::Instance().Write(somavr::LogLevel::Warn, "new-session-marker");
    somavr::Logger::Instance().Shutdown();

    const bool previousOk = ReadAll(previous).find("old-session-marker") != std::string::npos;
    const std::string currentText = ReadAll(current);
    const bool currentOk = currentText.find("new-session-marker") != std::string::npos
        && currentText.find("previous log preserved") != std::string::npos;
    std::filesystem::remove_all(root, ec);
    if (!workRootOk || !previousOk || !currentOk) {
        std::cerr << "FAILED: workRoot=" << workRootOk
                  << " previous=" << previousOk << " current=" << currentOk << '\n';
        return 1;
    }
    std::cout << "Logger tests passed\n";
    return 0;
}
