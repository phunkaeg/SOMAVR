#include "HPLCrosshairBridge.h"

#include "HPLInteractionBridge.h"
#include "HPLPresentationBridge.h"
#include "Logger.h"

#include <Windows.h>

#include <MinHook.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <mutex>
#include <string>

namespace somavr {
namespace {

constexpr uintptr_t kRunGlobalFuncRva = 0x484ea0;
constexpr uint8_t kRunGlobalFuncSignature[] = {
    0x4d, 0x8b, 0xc8,
    0x4c, 0x8b, 0xc2,
    0x48, 0x8b, 0xd1,
    0x48, 0x8b, 0x0d, 0xe8, 0x2f, 0x32, 0x00,
    0xe9, 0x5b, 0x73, 0xe1, 0xff,
};
constexpr uintptr_t kGetGlobalArgIntRva = 0x4851d0;
constexpr uint8_t kGetGlobalArgIntSignature[] = {
    0x48, 0x83, 0xec, 0x28,
    0x8b, 0xd1,
    0x48, 0x8b, 0x0d, 0xbb, 0x2c, 0x32, 0x00,
    0xe8, 0x5e, 0x4e, 0xe1, 0xff,
};
constexpr uintptr_t kGetGlobalArgFloatRva = 0x485200;
constexpr uint8_t kGetGlobalArgFloatSignature[] = {
    0x48, 0x83, 0xec, 0x28,
    0x8b, 0xd1,
    0x48, 0x8b, 0x0d, 0x8b, 0x2c, 0x32, 0x00,
    0xe8, 0x2e, 0x4e, 0xe1, 0xff,
};
constexpr uintptr_t kGetGlobalArgBoolRva = 0x485720;
constexpr uint8_t kGetGlobalArgBoolSignature[] = {
    0x40, 0x53,
    0x48, 0x83, 0xec, 0x20,
    0x8b, 0xd1,
    0x48, 0x8b, 0x0d, 0x69, 0x27, 0x32, 0x00,
    0xe8, 0x0c, 0x49, 0xe1, 0xff,
};
constexpr size_t kNativeStringInlineCapacity = 15;
constexpr size_t kMaxScriptNameLength = 127;

using RunGlobalFuncFn = bool (*)(const void* objectName, const void* className, const void* functionName);
using GetGlobalArgIntFn = int (*)(int index);
using GetGlobalArgFloatFn = float (*)(int index);
using GetGlobalArgBoolFn = bool (*)(int index);

struct NativeStringLayout {
    std::array<std::byte, 16> storage{};
    uint64_t size = 0;
    uint64_t capacity = 0;
};

Config g_config;
RunGlobalFuncFn g_originalRunGlobalFunc = nullptr;
GetGlobalArgIntFn g_getGlobalArgInt = nullptr;
GetGlobalArgFloatFn g_getGlobalArgFloat = nullptr;
GetGlobalArgBoolFn g_getGlobalArgBool = nullptr;
void* g_runGlobalFuncTarget = nullptr;
std::mutex g_installMutex;
std::atomic<uint64_t> g_calls = 0;
std::atomic<uint64_t> g_nameReadFailures = 0;
std::atomic<uint64_t> g_crosshairCalls = 0;
std::atomic<uint64_t> g_crosshairApplied = 0;
std::atomic<uint64_t> g_crosshairInvalid = 0;
std::atomic<uint64_t> g_wakeCalls = 0;
std::atomic<uint64_t> g_wakeApplied = 0;
std::atomic<uint64_t> g_wakeInvalid = 0;

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

bool ReadMemory(const void* source, void* destination, size_t bytes)
{
    if (source == nullptr || destination == nullptr || bytes == 0) return false;
    SIZE_T bytesRead = 0;
    return ReadProcessMemory(
        GetCurrentProcess(),
        source,
        destination,
        bytes,
        &bytesRead) != FALSE
        && bytesRead == bytes;
}

bool ReadNativeString(const void* nativeString, std::string& value)
{
    value.clear();
    NativeStringLayout layout;
    if (!ReadMemory(nativeString, &layout, sizeof(layout))
        || layout.size > kMaxScriptNameLength
        || layout.capacity < layout.size
        || layout.capacity > 1024 * 1024) {
        return false;
    }

    const char* source = nullptr;
    if (layout.capacity <= kNativeStringInlineCapacity) {
        source = reinterpret_cast<const char*>(layout.storage.data());
    } else {
        std::memcpy(&source, layout.storage.data(), sizeof(source));
    }
    if (source == nullptr && layout.size != 0) return false;

    value.resize(static_cast<size_t>(layout.size));
    return layout.size == 0 || ReadMemory(source, value.data(), value.size());
}

bool HookRunGlobalFunc(const void* objectName, const void* className, const void* functionName)
{
    const uint64_t call = g_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    std::string object;
    std::string classValue;
    std::string function;
    const bool namesValid = ReadNativeString(objectName, object)
        && ReadNativeString(className, classValue)
        && ReadNativeString(functionName, function);
    if (!namesValid) {
        g_nameReadFailures.fetch_add(1, std::memory_order_relaxed);
        return g_originalRunGlobalFunc(objectName, className, functionName);
    }

    const bool crosshairSetter = g_config.openxrInteractionReticle
        && g_config.openxrInteractionReticleSemantic
        && object == "LuxPlayer"
        && classValue.empty()
        && function == "_Global_SetCrosshairState";
    int state = -1;
    if (crosshairSetter && g_getGlobalArgInt != nullptr) {
        state = g_getGlobalArgInt(0);
        g_crosshairCalls.fetch_add(1, std::memory_order_relaxed);
    }
    const bool wakeSetAsleep = g_config.hplScriptedPresentationControl
        && object == "WakeHandler"
        && classValue.empty()
        && function == "_Global_SetAsleep";
    const bool wakeStart = g_config.hplScriptedPresentationControl
        && object == "WakeHandler"
        && classValue.empty()
        && function == "_Global_StartWakeup";
    bool asleep = false;
    float wakeDuration = 0.0f;
    if (wakeSetAsleep && g_getGlobalArgBool != nullptr) {
        asleep = g_getGlobalArgBool(0);
        g_wakeCalls.fetch_add(1, std::memory_order_relaxed);
    } else if (wakeStart && g_getGlobalArgFloat != nullptr) {
        wakeDuration = g_getGlobalArgFloat(0);
        g_wakeCalls.fetch_add(1, std::memory_order_relaxed);
    }

    const bool result = g_originalRunGlobalFunc(objectName, className, functionName);
    if (crosshairSetter) {
        if (result && state >= 0 && state < 35) {
            PublishHPLInteractionCrosshairState(state);
            const uint64_t applied = g_crosshairApplied.fetch_add(1, std::memory_order_relaxed) + 1;
            if (applied <= 16
                || applied % static_cast<uint64_t>(std::max(g_config.hplControllerLogInterval, 1)) == 0) {
                Logger::Instance().Write(
                    LogLevel::Info,
                    "hpl_crosshair_semantic call=%llu applied=1 state=%d name=%s",
                    static_cast<unsigned long long>(call),
                    state,
                    HPLCrosshairStateName(state));
            }
        } else {
            g_crosshairInvalid.fetch_add(1, std::memory_order_relaxed);
            Logger::Instance().Write(
                LogLevel::Warn,
                "hpl_crosshair_semantic call=%llu applied=0 runResult=%d state=%d",
                static_cast<unsigned long long>(call),
                result ? 1 : 0,
                state);
        }
    }
    if (wakeSetAsleep || wakeStart) {
        if (result && ((wakeSetAsleep && g_getGlobalArgBool != nullptr)
                || (wakeStart && g_getGlobalArgFloat != nullptr))) {
            if (wakeSetAsleep) {
                PublishHPLWakeSetAsleep(asleep);
            } else {
                PublishHPLWakeStart(wakeDuration);
            }
            const uint64_t applied = g_wakeApplied.fetch_add(1, std::memory_order_relaxed) + 1;
            Logger::Instance().Write(
                LogLevel::Info,
                "hpl_scripted_presentation call=%llu applied=%llu event=%s value=%.3f",
                static_cast<unsigned long long>(call),
                static_cast<unsigned long long>(applied),
                wakeSetAsleep ? "wake_set_asleep" : "wake_start",
                wakeSetAsleep ? (asleep ? 1.0f : 0.0f) : wakeDuration);
        } else {
            g_wakeInvalid.fetch_add(1, std::memory_order_relaxed);
            Logger::Instance().Write(
                LogLevel::Warn,
                "hpl_scripted_presentation call=%llu applied=0 event=%s runResult=%d",
                static_cast<unsigned long long>(call),
                wakeSetAsleep ? "wake_set_asleep" : "wake_start",
                result ? 1 : 0);
        }
    }
    return result;
}

} // namespace

const char* HPLCrosshairStateName(int state)
{
    static constexpr const char* kNames[] = {
        "None", "Default", "CarryOneHanded", "CarryTwoHanded", "Push",
        "PullLever", "PullLeverSmall", "PullSideways", "PullOut", "PullDoor",
        "PullDoorHatch", "Rotate", "RotateOneHanded", "PushButton", "PickUp",
        "UseToolInsert", "UseToolAction", "Terminal", "Datamine", "ClimbLadder",
        "Talk", "Eat", "Read", "ExitLevel", "ClimbLedge", "SitDown",
        "DefaultLarge", "DefaultLargeClear", "Examine", "Recharge", "RechargeBad",
        "TalkBusy", "PullVertical", "FireGun", "NoHints",
    };
    return state >= 0 && state < static_cast<int>(std::size(kNames)) ? kNames[state] : "Invalid";
}

bool InstallHPLCrosshairBridge(const Config& config)
{
    std::lock_guard lock(g_installMutex);
    g_config = config;
    const bool semanticReticle = config.openxrInteractionReticle
        && config.openxrInteractionReticleSemantic;
    if (!semanticReticle && !config.hplScriptedPresentationControl) {
        Logger::Instance().Write(
            LogLevel::Info,
            "hpl_crosshair_bridge disabled reticle=%d semantic=%d scriptedPresentation=%d",
            config.openxrInteractionReticle ? 1 : 0,
            config.openxrInteractionReticleSemantic ? 1 : 0,
            config.hplScriptedPresentationControl ? 1 : 0);
        return true;
    }
    if (g_runGlobalFuncTarget != nullptr) return true;

    HMODULE executable = GetModuleHandleW(nullptr);
    if (!IsInsideImage(executable, kRunGlobalFuncRva, sizeof(kRunGlobalFuncSignature))
        || (semanticReticle
            && !IsInsideImage(executable, kGetGlobalArgIntRva, sizeof(kGetGlobalArgIntSignature)))
        || (config.hplScriptedPresentationControl
            && (!IsInsideImage(executable, kGetGlobalArgFloatRva, sizeof(kGetGlobalArgFloatSignature))
                || !IsInsideImage(executable, kGetGlobalArgBoolRva, sizeof(kGetGlobalArgBoolSignature))))) {
        Logger::Instance().Write(LogLevel::Error, "hpl_crosshair_bridge install_failed reason=invalid_image_range");
        return false;
    }
    auto* runTarget = reinterpret_cast<std::byte*>(executable) + kRunGlobalFuncRva;
    auto* getArgTarget = reinterpret_cast<std::byte*>(executable) + kGetGlobalArgIntRva;
    auto* getFloatTarget = reinterpret_cast<std::byte*>(executable) + kGetGlobalArgFloatRva;
    auto* getBoolTarget = reinterpret_cast<std::byte*>(executable) + kGetGlobalArgBoolRva;
    if (std::memcmp(runTarget, kRunGlobalFuncSignature, sizeof(kRunGlobalFuncSignature)) != 0
        || (semanticReticle
            && std::memcmp(getArgTarget, kGetGlobalArgIntSignature, sizeof(kGetGlobalArgIntSignature)) != 0)
        || (config.hplScriptedPresentationControl
            && (std::memcmp(getFloatTarget, kGetGlobalArgFloatSignature,
                    sizeof(kGetGlobalArgFloatSignature)) != 0
                || std::memcmp(getBoolTarget, kGetGlobalArgBoolSignature,
                    sizeof(kGetGlobalArgBoolSignature)) != 0))) {
        Logger::Instance().Write(LogLevel::Error, "hpl_crosshair_bridge install_failed reason=signature_mismatch");
        return false;
    }

    g_getGlobalArgInt = semanticReticle ? reinterpret_cast<GetGlobalArgIntFn>(getArgTarget) : nullptr;
    g_getGlobalArgFloat = config.hplScriptedPresentationControl
        ? reinterpret_cast<GetGlobalArgFloatFn>(getFloatTarget) : nullptr;
    g_getGlobalArgBool = config.hplScriptedPresentationControl
        ? reinterpret_cast<GetGlobalArgBoolFn>(getBoolTarget) : nullptr;
    MH_STATUS status = MH_CreateHook(
        runTarget,
        reinterpret_cast<void*>(&HookRunGlobalFunc),
        reinterpret_cast<void**>(&g_originalRunGlobalFunc));
    if (status != MH_OK && status != MH_ERROR_ALREADY_CREATED) {
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_crosshair_bridge install_failed reason=create_hook status=%s",
            MH_StatusToString(status));
        g_getGlobalArgInt = nullptr;
        g_getGlobalArgFloat = nullptr;
        g_getGlobalArgBool = nullptr;
        return false;
    }
    status = MH_EnableHook(runTarget);
    if (status != MH_OK && status != MH_ERROR_ENABLED) {
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_crosshair_bridge install_failed reason=enable_hook status=%s",
            MH_StatusToString(status));
        g_getGlobalArgInt = nullptr;
        g_getGlobalArgFloat = nullptr;
        g_getGlobalArgBool = nullptr;
        return false;
    }

    g_runGlobalFuncTarget = runTarget;
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_crosshair_bridge install_ok runGlobalRva=0x%llx getArgIntRva=0x%llx getArgFloatRva=0x%llx getArgBoolRva=0x%llx semanticReticle=%d scriptedPresentation=%d policy=exact_registered_global_dispatch",
        static_cast<unsigned long long>(kRunGlobalFuncRva),
        static_cast<unsigned long long>(kGetGlobalArgIntRva),
        static_cast<unsigned long long>(kGetGlobalArgFloatRva),
        static_cast<unsigned long long>(kGetGlobalArgBoolRva),
        semanticReticle ? 1 : 0,
        config.hplScriptedPresentationControl ? 1 : 0);
    return true;
}

void RemoveHPLCrosshairBridge()
{
    std::lock_guard lock(g_installMutex);
    if (g_runGlobalFuncTarget != nullptr) {
        MH_DisableHook(g_runGlobalFuncTarget);
        MH_RemoveHook(g_runGlobalFuncTarget);
    }
    g_runGlobalFuncTarget = nullptr;
    g_originalRunGlobalFunc = nullptr;
    g_getGlobalArgInt = nullptr;
    g_getGlobalArgFloat = nullptr;
    g_getGlobalArgBool = nullptr;
    Logger::Instance().Write(LogLevel::Info, "hpl_crosshair_bridge removed");
}

void LogHPLCrosshairBridgeSummary()
{
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_crosshair_bridge_summary installed=%d calls=%llu nameReadFailures=%llu crosshairCalls=%llu crosshairApplied=%llu crosshairInvalid=%llu wakeCalls=%llu wakeApplied=%llu wakeInvalid=%llu",
        g_runGlobalFuncTarget != nullptr ? 1 : 0,
        static_cast<unsigned long long>(g_calls.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_nameReadFailures.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_crosshairCalls.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_crosshairApplied.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_crosshairInvalid.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_wakeCalls.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_wakeApplied.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_wakeInvalid.load(std::memory_order_relaxed)));
}

} // namespace somavr
