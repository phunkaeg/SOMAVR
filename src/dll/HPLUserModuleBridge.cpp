#include "HPLUserModuleBridge.h"

#include "Config.h"
#include "HPLPresentationBridge.h"
#include "Logger.h"
#include "NativeMemoryAccess.h"

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
constexpr int kPlayerHandsModuleId = 18;
constexpr int kOpenInventoryAction = 12;
constexpr uintptr_t kScriptDispatchLifecycleRva = 0x154c40;
constexpr uint8_t kScriptDispatchLifecycleSignature[] = {
    0x48, 0x83, 0xec, 0x28,
    0x80, 0x79, 0x32, 0x00,
    0x4c, 0x8b, 0xc1,
    0x0f, 0x84, 0x0f, 0x01, 0x00, 0x00,
    0x80, 0x79, 0x30, 0x00,
    0x74, 0x0e,
    0x83, 0xfa, 0x04,
    0x7c, 0x09,
    0x83, 0xfa, 0x06,
};
constexpr size_t kScriptObjectOffset = 0xe8;
constexpr size_t kUpdateableSubobjectOffset = 0x110;
constexpr int kUpdateCallbackId = 4;

using UserModuleOnActionFn = void (*)(void* userModule, int action, bool pressed);
using ScriptDispatchLifecycleFn = void (*)(void* updateable, int callbackId, float timeStep);

UserModuleOnActionFn g_originalOnAction = nullptr;
ScriptDispatchLifecycleFn g_originalDispatchLifecycle = nullptr;
void* g_onActionTarget = nullptr;
void* g_dispatchLifecycleTarget = nullptr;
std::mutex g_installMutex;
std::atomic<uint64_t> g_calls = 0;
std::atomic<uint64_t> g_moduleIdReadFailures = 0;
std::atomic<uint64_t> g_inventoryActions = 0;
std::atomic<uint64_t> g_inventoryOpenEvents = 0;
std::atomic<uint64_t> g_lifecycleCalls = 0;
std::atomic<uint64_t> g_handsIdCandidates = 0;
std::atomic<uint64_t> g_handsVtableMatches = 0;
std::atomic<uint64_t> g_handsOwnerChanges = 0;
std::atomic<uint64_t> g_handsScriptObjectReads = 0;
std::atomic<void*> g_handsModule = nullptr;
std::atomic<void*> g_handsScriptObject = nullptr;

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

size_t CountExecutablePatternMatches(HMODULE module, const uint8_t* pattern, size_t patternSize)
{
    if (module == nullptr || pattern == nullptr || patternSize == 0) return 0;
    const auto* base = reinterpret_cast<const uint8_t*>(module);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return 0;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return 0;

    size_t matches = 0;
    const IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION(nt);
    for (uint16_t index = 0; index < nt->FileHeader.NumberOfSections; ++index, ++section) {
        if ((section->Characteristics & IMAGE_SCN_MEM_EXECUTE) == 0) continue;
        const size_t sectionSize = static_cast<size_t>(section->Misc.VirtualSize);
        if (sectionSize < patternSize
            || section->VirtualAddress > nt->OptionalHeader.SizeOfImage
            || sectionSize > nt->OptionalHeader.SizeOfImage - section->VirtualAddress) {
            continue;
        }
        const uint8_t* bytes = base + section->VirtualAddress;
        for (size_t offset = 0; offset <= sectionSize - patternSize; ++offset) {
            if (std::memcmp(bytes + offset, pattern, patternSize) == 0) {
                ++matches;
            }
        }
    }
    return matches;
}

bool ValidateHookSignature(
    HMODULE executable,
    uintptr_t rva,
    const uint8_t* signature,
    size_t signatureSize,
    const char* lane)
{
    if (!IsInsideImage(executable, rva, signatureSize)) {
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_user_module_bridge install_failed lane=%s reason=invalid_image_range rva=0x%llx",
            lane,
            static_cast<unsigned long long>(rva));
        return false;
    }
    const size_t matches = CountExecutablePatternMatches(
        executable, signature, signatureSize);
    if (matches != 1) {
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_user_module_bridge install_failed lane=%s reason=signature_not_unique matches=%zu rva=0x%llx",
            lane,
            matches,
            static_cast<unsigned long long>(rva));
        return false;
    }
    const auto* target = reinterpret_cast<const uint8_t*>(executable) + rva;
    if (std::memcmp(target, signature, signatureSize) != 0) {
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_user_module_bridge install_failed lane=%s reason=signature_mismatch rva=0x%llx",
            lane,
            static_cast<unsigned long long>(rva));
        return false;
    }
    return true;
}

bool ReadModuleId(const void* userModule, int& moduleId)
{
    return native_memory::TryReadField(userModule, kModuleIdOffset, moduleId);
}

bool IsLuxUserModule(const void* updateable)
{
    uintptr_t vtable = 0;
    const auto base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    return base != 0 && native_memory::TryRead(updateable, vtable)
        && vtable == base + 0x68fbb8;
}

void ObservePlayerHandsModule(void* updateable, int callbackId)
{
    g_lifecycleCalls.fetch_add(1, std::memory_order_relaxed);
    if (callbackId != kUpdateCallbackId || updateable == nullptr) return;
    if (!IsLuxUserModule(updateable)) return;
    void* module = reinterpret_cast<std::byte*>(updateable) - kUpdateableSubobjectOffset;
    uintptr_t primaryVtable = 0;
    if (!native_memory::TryRead(module, primaryVtable)
        || primaryVtable != reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr)) + 0x68fcc8) return;
    int moduleId = -1;
    if (!ReadModuleId(module, moduleId) || moduleId != kPlayerHandsModuleId) return;
    g_handsIdCandidates.fetch_add(1, std::memory_order_relaxed);
    g_handsVtableMatches.fetch_add(1, std::memory_order_relaxed);

    void* scriptObject = nullptr;
    if (native_memory::TryReadField(module, kScriptObjectOffset, scriptObject)) {
        g_handsScriptObjectReads.fetch_add(1, std::memory_order_relaxed);
    }

    void* previous = g_handsModule.exchange(module, std::memory_order_relaxed);
    g_handsScriptObject.store(scriptObject, std::memory_order_relaxed);
    if (previous != module) {
        const uint64_t change = g_handsOwnerChanges.fetch_add(1, std::memory_order_relaxed) + 1;
        Logger::Instance().Write(
            LogLevel::Info,
            "hpl_player_hands_module owner_acquired change=%llu module=%p moduleId=%d scriptObject=%p callback=%d policy=read_only_owner_discovery",
            static_cast<unsigned long long>(change),
            module,
            moduleId,
            scriptObject,
            callbackId);
    }
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

void HookScriptDispatchLifecycle(void* updateable, int callbackId, float timeStep)
{
    ObservePlayerHandsModule(updateable, callbackId);
    g_originalDispatchLifecycle(updateable, callbackId, timeStep);
}

bool InstallHook(
    void* target,
    void* detour,
    void** original,
    const char* lane)
{
    MH_STATUS status = MH_CreateHook(target, detour, original);
    if (status != MH_OK) {
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_user_module_bridge install_failed lane=%s reason=create_hook status=%s",
            lane,
            MH_StatusToString(status));
        *original = nullptr;
        return false;
    }
    status = MH_EnableHook(target);
    if (status != MH_OK && status != MH_ERROR_ENABLED) {
        MH_RemoveHook(target);
        *original = nullptr;
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_user_module_bridge install_failed lane=%s reason=enable_hook status=%s",
            lane,
            MH_StatusToString(status));
        return false;
    }
    return true;
}

} // namespace

bool InstallHPLUserModuleBridge(const Config& config)
{
    std::lock_guard lock(g_installMutex);
    const bool inventoryEnabled = config.hplInventoryPresentationControl;
    const bool handsOwnerProbeEnabled = config.hplHandTrackingProbe;
    if (!inventoryEnabled && !handsOwnerProbeEnabled) {
        Logger::Instance().Write(
            LogLevel::Info,
            "hpl_user_module_bridge disabled inventoryPresentation=0 handsOwnerProbe=0");
        return true;
    }

    HMODULE executable = GetModuleHandleW(nullptr);
    bool inventoryInstalled = !inventoryEnabled || g_onActionTarget != nullptr;
    bool handsOwnerInstalled = !handsOwnerProbeEnabled || g_dispatchLifecycleTarget != nullptr;
    const bool inventorySignatureValid = inventoryEnabled
        && g_onActionTarget == nullptr
        && ValidateHookSignature(
            executable,
            kUserModuleOnActionRva,
            kUserModuleOnActionSignature,
            sizeof(kUserModuleOnActionSignature),
            "inventory");
    if (inventorySignatureValid) {
        auto* target = reinterpret_cast<std::byte*>(executable) + kUserModuleOnActionRva;
        if (InstallHook(
                target,
                reinterpret_cast<void*>(&HookUserModuleOnAction),
                reinterpret_cast<void**>(&g_originalOnAction),
                "inventory")) {
            g_onActionTarget = target;
            inventoryInstalled = true;
        } else {
            inventoryInstalled = false;
        }
    }

    const bool handsOwnerSignatureValid = handsOwnerProbeEnabled
        && g_dispatchLifecycleTarget == nullptr
        && ValidateHookSignature(
            executable,
            kScriptDispatchLifecycleRva,
            kScriptDispatchLifecycleSignature,
            sizeof(kScriptDispatchLifecycleSignature),
            "hands_owner");
    if (handsOwnerSignatureValid) {
        auto* lifecycleTarget = reinterpret_cast<std::byte*>(executable)
            + kScriptDispatchLifecycleRva;
        if (InstallHook(
                lifecycleTarget,
                reinterpret_cast<void*>(&HookScriptDispatchLifecycle),
                reinterpret_cast<void**>(&g_originalDispatchLifecycle),
                "hands_owner")) {
            g_dispatchLifecycleTarget = lifecycleTarget;
            handsOwnerInstalled = true;
        } else {
            handsOwnerInstalled = false;
        }
    }
    if (inventoryEnabled && g_onActionTarget == nullptr && !inventorySignatureValid) {
        inventoryInstalled = false;
    }
    if (handsOwnerProbeEnabled && g_dispatchLifecycleTarget == nullptr
        && !handsOwnerSignatureValid) {
        handsOwnerInstalled = false;
    }

    const bool installed = inventoryInstalled && handsOwnerInstalled;

    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_user_module_bridge install_%s inventoryEnabled=%d inventoryInstalled=%d handsOwnerProbeEnabled=%d handsOwnerInstalled=%d onActionRva=0x%llx lifecycleRva=0x%llx moduleIdOffset=0x%llx inventoryModuleId=%d handsModuleId=%d openInventoryAction=%d policy=independent_exact_user_module_action_plus_read_only_hands_owner",
        installed ? "ok" : "partial",
        inventoryEnabled ? 1 : 0,
        inventoryInstalled ? 1 : 0,
        handsOwnerProbeEnabled ? 1 : 0,
        handsOwnerInstalled ? 1 : 0,
        static_cast<unsigned long long>(kUserModuleOnActionRva),
        static_cast<unsigned long long>(kScriptDispatchLifecycleRva),
        static_cast<unsigned long long>(kModuleIdOffset),
        kInventoryModuleId,
        kPlayerHandsModuleId,
        kOpenInventoryAction);
    return installed;
}

void RemoveHPLUserModuleBridge()
{
    std::lock_guard lock(g_installMutex);
    if (g_onActionTarget != nullptr) {
        MH_DisableHook(g_onActionTarget);
        MH_RemoveHook(g_onActionTarget);
    }
    if (g_dispatchLifecycleTarget != nullptr) {
        MH_DisableHook(g_dispatchLifecycleTarget);
        MH_RemoveHook(g_dispatchLifecycleTarget);
    }
    g_onActionTarget = nullptr;
    g_originalOnAction = nullptr;
    g_dispatchLifecycleTarget = nullptr;
    g_originalDispatchLifecycle = nullptr;
    g_handsModule.store(nullptr, std::memory_order_relaxed);
    g_handsScriptObject.store(nullptr, std::memory_order_relaxed);
    Logger::Instance().Write(LogLevel::Info, "hpl_user_module_bridge removed");
}

void LogHPLUserModuleBridgeSummary()
{
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_user_module_bridge_summary inventoryInstalled=%d handsOwnerInstalled=%d calls=%llu moduleIdReadFailures=%llu inventoryActions=%llu inventoryOpenEvents=%llu lifecycleCalls=%llu handsIdCandidates=%llu handsVtableMatches=%llu handsOwnerChanges=%llu handsScriptObjectReads=%llu handsModule=%p handsScriptObject=%p",
        g_onActionTarget != nullptr ? 1 : 0,
        g_dispatchLifecycleTarget != nullptr ? 1 : 0,
        static_cast<unsigned long long>(g_calls.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_moduleIdReadFailures.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_inventoryActions.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_inventoryOpenEvents.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_lifecycleCalls.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_handsIdCandidates.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_handsVtableMatches.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_handsOwnerChanges.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_handsScriptObjectReads.load(std::memory_order_relaxed)),
        g_handsModule.load(std::memory_order_relaxed),
        g_handsScriptObject.load(std::memory_order_relaxed));
}

} // namespace somavr
