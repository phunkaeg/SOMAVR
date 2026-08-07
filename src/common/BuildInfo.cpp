#include "BuildInfo.h"

#include "Logger.h"
#include "SomaVRBuildInfo.generated.h"

#include <winternl.h>

#include <cstdint>
#include <filesystem>
#include <sstream>

namespace somavr {
namespace {

struct PeIdentity {
    bool valid = false;
    uint32_t timestamp = 0;
    uint32_t imageSize = 0;
};

PeIdentity QueryPeIdentity(HMODULE module)
{
    PeIdentity identity;
    if (module == nullptr) {
        return identity;
    }

    const auto* base = reinterpret_cast<const uint8_t*>(module);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0) {
        return identity;
    }
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE
        || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        return identity;
    }

    identity.valid = true;
    identity.timestamp = nt->FileHeader.TimeDateStamp;
    identity.imageSize = nt->OptionalHeader.SizeOfImage;
    return identity;
}

std::string QueryWindowsVersion()
{
    using RtlGetVersionFn = LONG(WINAPI*)(PRTL_OSVERSIONINFOW);
    const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    const auto rtlGetVersion = ntdll != nullptr
        ? reinterpret_cast<RtlGetVersionFn>(GetProcAddress(ntdll, "RtlGetVersion"))
        : nullptr;
    RTL_OSVERSIONINFOW version{};
    version.dwOSVersionInfoSize = sizeof(version);
    if (rtlGetVersion == nullptr || rtlGetVersion(&version) != 0) {
        return "unavailable";
    }

    std::ostringstream out;
    out << version.dwMajorVersion << '.' << version.dwMinorVersion
        << '.' << version.dwBuildNumber;
    return out.str();
}

} // namespace

std::string BuildIdentityString()
{
    std::ostringstream out;
    out << SOMAVR_GENERATED_VERSION
        << '+' << SOMAVR_GENERATED_GIT_DESCRIBE
        << "/" << SOMAVR_GENERATED_FLAVOR
        << "/" << SOMAVR_GENERATED_CONFIGURATION;
    return out.str();
}

void LogBuildIdentity(HMODULE module)
{
    const PeIdentity pe = QueryPeIdentity(module);
    const std::string modulePath = ModulePath(module);
    std::error_code sizeError;
    const uint64_t fileSize = modulePath.empty()
        ? 0
        : std::filesystem::file_size(std::filesystem::path(ToWide(modulePath)), sizeError);

    Logger::Instance().Write(
        LogLevel::Warn,
        "build_identity identity=%s version=%s flavor=%s configuration=%s openxr=%d gitDescribe=%s gitCommit=%s dirty=%d generatedUtc=%s compiler=msvc-%d",
        BuildIdentityString().c_str(),
        SOMAVR_GENERATED_VERSION,
        SOMAVR_GENERATED_FLAVOR,
        SOMAVR_GENERATED_CONFIGURATION,
        SOMAVR_GENERATED_OPENXR,
        SOMAVR_GENERATED_GIT_DESCRIBE,
        SOMAVR_GENERATED_GIT_COMMIT,
        SOMAVR_GENERATED_GIT_DIRTY,
        SOMAVR_GENERATED_BUILD_UTC,
#if defined(_MSC_FULL_VER)
        _MSC_FULL_VER
#else
        0
#endif
    );
    Logger::Instance().Write(
        LogLevel::Info,
        "module_identity path=%s peValid=%d peTimestamp=0x%08x imageSize=%u fileSize=%llu",
        modulePath.c_str(),
        pe.valid ? 1 : 0,
        pe.timestamp,
        pe.imageSize,
        static_cast<unsigned long long>(sizeError ? 0 : fileSize));

    SYSTEM_INFO systemInfo{};
    GetNativeSystemInfo(&systemInfo);
    MEMORYSTATUSEX memory{};
    memory.dwLength = sizeof(memory);
    const bool memoryValid = GlobalMemoryStatusEx(&memory) != FALSE;
    Logger::Instance().Write(
        LogLevel::Info,
        "environment windows=%s architecture=%u logicalProcessors=%lu pageSize=%lu ramMiB=%llu",
        QueryWindowsVersion().c_str(),
        static_cast<unsigned>(systemInfo.wProcessorArchitecture),
        static_cast<unsigned long>(systemInfo.dwNumberOfProcessors),
        static_cast<unsigned long>(systemInfo.dwPageSize),
        static_cast<unsigned long long>(memoryValid
            ? memory.ullTotalPhys / (1024ull * 1024ull) : 0));
}

} // namespace somavr
