#include "HPLLifecycle.h"

#include "Logger.h"

#include <Windows.h>

#include <MinHook.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>

namespace somavr {
namespace {

constexpr uintptr_t kSDLEngineSetupDestructorRva = 0x3b16e0;
constexpr uint8_t kSDLEngineSetupDestructorSignature[] = {
    0x40, 0x53,                   // push rbx
    0x48, 0x83, 0xec, 0x20,     // sub rsp,20h
    0x48, 0x8d, 0x05,            // lea rax,[vftable]
};

using SDLEngineSetupDestructorFn = void (*)(void* self);

Config g_config;
OpenXRRuntime* g_openxr = nullptr;
SDLEngineSetupDestructorFn g_originalDestructor = nullptr;
void* g_destructorTarget = nullptr;
std::mutex g_installMutex;
std::atomic<uint64_t> g_destructorCalls = 0;
std::atomic<uint64_t> g_preGraphicsShutdowns = 0;

bool IsInsideImage(HMODULE module, uintptr_t rva, size_t bytes)
{
    if (module == nullptr) {
        return false;
    }
    const auto* base = reinterpret_cast<const std::byte*>(module);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        return false;
    }
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    return nt->Signature == IMAGE_NT_SIGNATURE
        && rva <= nt->OptionalHeader.SizeOfImage
        && bytes <= nt->OptionalHeader.SizeOfImage - rva;
}

void HookSDLEngineSetupDestructor(void* self)
{
    g_destructorCalls.fetch_add(1, std::memory_order_relaxed);
    if (g_preGraphicsShutdowns.fetch_add(1, std::memory_order_relaxed) == 0) {
        Logger::Instance().Write(
            LogLevel::Warn,
            "hpl_lifecycle pre_graphics_shutdown begin anchor=cSDLEngineSetup_destructor rva=0x%llx self=%p",
            static_cast<unsigned long long>(kSDLEngineSetupDestructorRva),
            self);
        if (g_openxr != nullptr) {
            g_openxr->SetStereoSubmissionEnabled(false);
            g_openxr->Shutdown();
        }
        Logger::Instance().Write(
            LogLevel::Warn,
            "hpl_lifecycle pre_graphics_shutdown complete");
    }

    if (g_originalDestructor != nullptr) {
        g_originalDestructor(self);
    }
}

} // namespace

bool InstallHPLLifecycle(const Config& config, OpenXRRuntime* openxr)
{
    std::lock_guard lock(g_installMutex);
    if (!config.hplLifecycleShutdown) {
        Logger::Instance().Write(LogLevel::Info, "hpl_lifecycle disabled config=0");
        return true;
    }
    if (g_destructorTarget != nullptr) {
        return true;
    }

    g_config = config;
    g_openxr = openxr;
    HMODULE executable = GetModuleHandleW(nullptr);
    if (!IsInsideImage(executable, kSDLEngineSetupDestructorRva, sizeof(kSDLEngineSetupDestructorSignature))) {
        Logger::Instance().Write(LogLevel::Error, "hpl_lifecycle install_failed reason=invalid_image_range");
        return false;
    }

    auto* target = reinterpret_cast<std::byte*>(executable) + kSDLEngineSetupDestructorRva;
    if (std::memcmp(target, kSDLEngineSetupDestructorSignature, sizeof(kSDLEngineSetupDestructorSignature)) != 0) {
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_lifecycle install_failed reason=signature_mismatch rva=0x%llx",
            static_cast<unsigned long long>(kSDLEngineSetupDestructorRva));
        return false;
    }

    MH_STATUS status = MH_CreateHook(
        target,
        reinterpret_cast<void*>(&HookSDLEngineSetupDestructor),
        reinterpret_cast<void**>(&g_originalDestructor));
    if (status != MH_OK && status != MH_ERROR_ALREADY_CREATED) {
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_lifecycle install_failed reason=create_hook status=%s",
            MH_StatusToString(status));
        return false;
    }
    status = MH_EnableHook(target);
    if (status != MH_OK && status != MH_ERROR_ENABLED) {
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_lifecycle install_failed reason=enable_hook status=%s",
            MH_StatusToString(status));
        return false;
    }

    g_destructorTarget = target;
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_lifecycle install_ok function=cSDLEngineSetup_destructor rva=0x%llx target=%p policy=openxr_before_hpl_graphics",
        static_cast<unsigned long long>(kSDLEngineSetupDestructorRva),
        target);
    return true;
}

void RemoveHPLLifecycle()
{
    std::lock_guard lock(g_installMutex);
    if (g_destructorTarget != nullptr) {
        MH_DisableHook(g_destructorTarget);
        MH_RemoveHook(g_destructorTarget);
    }
    g_destructorTarget = nullptr;
    g_originalDestructor = nullptr;
    g_openxr = nullptr;
    Logger::Instance().Write(LogLevel::Info, "hpl_lifecycle removed");
}

void LogHPLLifecycleSummary()
{
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_lifecycle_summary installed=%d destructorCalls=%llu preGraphicsShutdowns=%llu",
        g_destructorTarget != nullptr ? 1 : 0,
        static_cast<unsigned long long>(g_destructorCalls.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_preGraphicsShutdowns.load(std::memory_order_relaxed)));
}

} // namespace somavr
