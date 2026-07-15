#include "HPLUserModuleBridge.h"

#include "Config.h"
#include "HPLPresentationBridge.h"
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

constexpr uintptr_t kUserModuleOnActionRva = 0x1378e0;
constexpr uint8_t kUserModuleOnActionSignature[] = {
    0x48, 0x83, 0xec, 0x28,
    0x48, 0x8b, 0x89, 0x90, 0x00, 0x00, 0x00,
    0x48, 0x85, 0xc9,
    0x74, 0x05,
    0xe8, 0x4b, 0x21, 0xff, 0xff,
    0x48, 0x83, 0xc4, 0x28,
    0xc3,
};
constexpr size_t kModuleIdOffset = 0x158;
constexpr int kInventoryModuleId = 15;
constexpr int kOpenInventoryAction = 12;

using UserModuleOnActionFn = void (*)(void* userModule, int action, bool pressed);

UserModuleOnActionFn g_originalOnAction = nullptr;
void* g_onActionTarget = nullptr;
std::mutex g_installMutex;
std::atomic<uint64_t> g_calls = 0;
std::atomic<uint64_t> g_moduleIdReadFailures = 0;
std::atomic<uint64_t> g_inventoryActions = 0;
std::atomic<uint64_t> g_inventoryOpenEvents = 0;

bool IsInsideImage(HMODULE module, uintptr_t rva, size_t bytes)
{
    if (module == nullptr) return false;
    const auto* base = reinterpret_cast<const std::byte*>(module);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    return nt->Signature == IMAGE_NT_SIGNATURE
        && rva <= nt->OptionalHeader.SizeOfImage
        && bytes <= nt->OptionalHeader.SizeOfImage - rva;
}

bool ReadModuleId(const void* userModule, int& moduleId)
{
    if (userModule == nullptr) return false;
    SIZE_T bytesRead = 0;
    return ReadProcessMemory(
        GetCurrentProcess(),
        reinterpret_cast<const std::byte*>(userModule) + kModuleIdOffset,
        &moduleId,
        sizeof(moduleId),
        &bytesRead) != FALSE
        && bytesRead == sizeof(moduleId);
}

void HookUserModuleOnAction(void* userModule, int action, bool pressed)
{
    const uint64_t call = g_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    int moduleId = -1;
    const bool moduleIdValid = ReadModuleId(userModule, moduleId);
    if (!moduleIdValid) {
        g_moduleIdReadFailures.fetch_add(1, std::memory_order_relaxed);
    }
    const bool inventoryAction = moduleIdValid
        && moduleId == kInventoryModuleId
        && action == kOpenInventoryAction;
    if (inventoryAction) {
        g_inventoryActions.fetch_add(1, std::memory_order_relaxed);
    }

    g_originalOnAction(userModule, action, pressed);

    if (inventoryAction && pressed) {
        PublishHPLInventoryOpen();
        const uint64_t event = g_inventoryOpenEvents.fetch_add(1, std::memory_order_relaxed) + 1;
        Logger::Instance().Write(
            LogLevel::Info,
            "hpl_user_module_action call=%llu event=%llu moduleId=%d action=%d pressed=1 route=inventory_presentation",
            static_cast<unsigned long long>(call),
            static_cast<unsigned long long>(event),
            moduleId,
            action);
    }
}

} // namespace

bool InstallHPLUserModuleBridge(const Config& config)
{
    std::lock_guard lock(g_installMutex);
    if (!config.hplInventoryPresentationControl) {
        Logger::Instance().Write(
            LogLevel::Info,
            "hpl_user_module_bridge disabled inventoryPresentation=0");
        return true;
    }
    if (g_onActionTarget != nullptr) return true;

    HMODULE executable = GetModuleHandleW(nullptr);
    if (!IsInsideImage(executable, kUserModuleOnActionRva, sizeof(kUserModuleOnActionSignature))) {
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_user_module_bridge install_failed reason=invalid_image_range rva=0x%llx",
            static_cast<unsigned long long>(kUserModuleOnActionRva));
        return false;
    }
    auto* target = reinterpret_cast<std::byte*>(executable) + kUserModuleOnActionRva;
    if (std::memcmp(target, kUserModuleOnActionSignature, sizeof(kUserModuleOnActionSignature)) != 0) {
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_user_module_bridge install_failed reason=signature_mismatch rva=0x%llx",
            static_cast<unsigned long long>(kUserModuleOnActionRva));
        return false;
    }

    MH_STATUS status = MH_CreateHook(
        target,
        reinterpret_cast<void*>(&HookUserModuleOnAction),
        reinterpret_cast<void**>(&g_originalOnAction));
    if (status != MH_OK) {
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_user_module_bridge install_failed reason=create_hook status=%s",
            MH_StatusToString(status));
        g_originalOnAction = nullptr;
        return false;
    }
    status = MH_EnableHook(target);
    if (status != MH_OK && status != MH_ERROR_ENABLED) {
        MH_RemoveHook(target);
        g_originalOnAction = nullptr;
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_user_module_bridge install_failed reason=enable_hook status=%s",
            MH_StatusToString(status));
        return false;
    }

    g_onActionTarget = target;
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_user_module_bridge install_ok onActionRva=0x%llx moduleIdOffset=0x%llx inventoryModuleId=%d openInventoryAction=%d policy=exact_user_module_action",
        static_cast<unsigned long long>(kUserModuleOnActionRva),
        static_cast<unsigned long long>(kModuleIdOffset),
        kInventoryModuleId,
        kOpenInventoryAction);
    return true;
}

void RemoveHPLUserModuleBridge()
{
    std::lock_guard lock(g_installMutex);
    if (g_onActionTarget != nullptr) {
        MH_DisableHook(g_onActionTarget);
        MH_RemoveHook(g_onActionTarget);
    }
    g_onActionTarget = nullptr;
    g_originalOnAction = nullptr;
    Logger::Instance().Write(LogLevel::Info, "hpl_user_module_bridge removed");
}

void LogHPLUserModuleBridgeSummary()
{
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_user_module_bridge_summary installed=%d calls=%llu moduleIdReadFailures=%llu inventoryActions=%llu inventoryOpenEvents=%llu",
        g_onActionTarget != nullptr ? 1 : 0,
        static_cast<unsigned long long>(g_calls.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_moduleIdReadFailures.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_inventoryActions.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_inventoryOpenEvents.load(std::memory_order_relaxed)));
}

} // namespace somavr
