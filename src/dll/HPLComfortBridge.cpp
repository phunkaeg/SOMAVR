#include "HPLComfortBridge.h"

#include "HPLCameraBridge.h"
#include "HPLComfortMath.h"
#include "HPLPlayerState.h"
#include "Logger.h"

#include <Windows.h>

#include <MinHook.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>

namespace somavr {
namespace {

constexpr uintptr_t kSetCameraPosAddRva = 0x159360;
constexpr uintptr_t kFadeCameraRollRva = 0x156d90;
constexpr uintptr_t kSetCameraRollRva = 0x156f00;
constexpr uintptr_t kSetDepthOfFieldActiveRva = 0x071f80;
constexpr uintptr_t kFadeCameraFovMultiplierRva = 0x155210;
constexpr uintptr_t kFadeCameraAspectMultiplierRva = 0x155230;
constexpr uintptr_t kFadeCameraFovRva = 0x155250;
constexpr size_t kDepthOfFieldActiveOffset = 0x264;
constexpr size_t kPlayerDefaultFovOffset = 0x194;
constexpr size_t kPlayerFovGoalOffset = 0x19c;
constexpr size_t kPlayerFovSpeedOffset = 0x1a0;
constexpr size_t kPlayerFovMultiplierGoalOffset = 0x38c;
constexpr size_t kPlayerFovMultiplierSpeedOffset = 0x390;
constexpr size_t kPlayerAspectMultiplierGoalOffset = 0x394;
constexpr size_t kPlayerAspectMultiplierSpeedOffset = 0x398;

constexpr uint8_t kSetCameraPosAddSignature[] = {
    0x40, 0x53,
    0x48, 0x83, 0xec, 0x20,
    0x49, 0x8b, 0xd8,
    0xe8, 0x72, 0xf4, 0xff, 0xff,
    0xc6, 0x40, 0x1c, 0x00,
};
constexpr uint8_t kFadeCameraRollSignature[] = {
    0x40, 0x56, 0x41, 0x54, 0x41, 0x55,
    0x48, 0x83, 0xec, 0x50,
    0x48, 0x8b, 0x81, 0xa8, 0x03, 0x00, 0x00,
};
constexpr uint8_t kSetCameraRollSignature[] = {
    0x40, 0x56, 0x41, 0x54, 0x41, 0x55,
    0x48, 0x83, 0xec, 0x40,
    0x48, 0x8b, 0x81, 0xa8, 0x03, 0x00, 0x00,
};
constexpr uint8_t kSetDepthOfFieldActiveSignature[] = {
    0x88, 0x91, 0x64, 0x02, 0x00, 0x00, 0xc3,
    0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc,
};
constexpr uint8_t kFadeCameraFovMultiplierSignature[] = {
    0xf3, 0x0f, 0x11, 0x89, 0x8c, 0x03, 0x00, 0x00,
    0xf3, 0x0f, 0x11, 0x91, 0x90, 0x03, 0x00, 0x00,
};
constexpr uint8_t kFadeCameraAspectMultiplierSignature[] = {
    0xf3, 0x0f, 0x11, 0x89, 0x94, 0x03, 0x00, 0x00,
    0xf3, 0x0f, 0x11, 0x91, 0x98, 0x03, 0x00, 0x00,
};
constexpr uint8_t kFadeCameraFovSignature[] = {
    0xf3, 0x0f, 0x11, 0x89, 0x9c, 0x01, 0x00, 0x00,
    0xf3, 0x0f, 0x11, 0x91, 0xa0, 0x01, 0x00, 0x00,
};

using SetCameraPosAddFn = void (*)(void* player, int type, const float* vector);
using SetCameraRollFn = void (*)(void* player, int type, float value);
using FadeCameraRollFn = void (*)(
    void* player, int type, float value, float speedMultiplier, float maximumSpeed);

Config g_config;
SetCameraPosAddFn g_originalSetCameraPosAdd = nullptr;
SetCameraRollFn g_originalSetCameraRoll = nullptr;
FadeCameraRollFn g_originalFadeCameraRoll = nullptr;
void* g_setCameraPosAddTarget = nullptr;
void* g_setCameraRollTarget = nullptr;
void* g_fadeCameraRollTarget = nullptr;
void* g_setDepthOfFieldActiveTarget = nullptr;
void* g_fadeCameraFovMultiplierTarget = nullptr;
void* g_fadeCameraAspectMultiplierTarget = nullptr;
void* g_fadeCameraFovTarget = nullptr;
std::array<uint8_t, sizeof(kSetDepthOfFieldActiveSignature)> g_depthOfFieldOriginal{};
std::array<uint8_t, sizeof(kFadeCameraFovMultiplierSignature)> g_fovMultiplierOriginal{};
std::array<uint8_t, sizeof(kFadeCameraAspectMultiplierSignature)> g_aspectMultiplierOriginal{};
std::array<uint8_t, sizeof(kFadeCameraFovSignature)> g_fovOriginal{};
std::mutex g_installMutex;
std::atomic<uint64_t> g_cameraAddCalls = 0;
std::atomic<uint64_t> g_cameraAddSuppressed = 0;
std::atomic<uint64_t> g_bobSuppressed = 0;
std::atomic<uint64_t> g_shakeSuppressed = 0;
std::atomic<uint64_t> g_swaySuppressed = 0;
std::atomic<uint64_t> g_terminalSuppressed = 0;
std::atomic<uint64_t> g_rollCalls = 0;
std::atomic<uint64_t> g_rollSuppressed = 0;
std::atomic<uint64_t> g_rollSetSuppressed = 0;
std::atomic<uint64_t> g_rollFadeSuppressed = 0;
std::atomic<uint64_t> g_dofCalls = 0;
std::atomic<uint64_t> g_dofEnableRequests = 0;
std::atomic<uint64_t> g_dofSuppressed = 0;
std::atomic<uint64_t> g_opticsCalls = 0;
std::atomic<uint64_t> g_opticsSuppressed = 0;
std::atomic<uint64_t> g_fovSuppressed = 0;
std::atomic<uint64_t> g_fovMultiplierSuppressed = 0;
std::atomic<uint64_t> g_aspectMultiplierSuppressed = 0;
std::atomic<uint64_t> g_trackingInactive = 0;

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

bool TrackingActive()
{
    return GetHPLCameraBridgeStatus().trackingEnabled;
}

bool ShouldSuppressRoll(int type)
{
    return g_config.hplComfortCameraRollControl
        && TrackingActive()
        && comfort_math::ShouldSuppressCameraRoll(
            type,
            g_config.hplComfortSuppressScriptRoll,
            g_config.hplComfortSuppressLeanRoll,
            g_config.hplComfortSuppressMoveRoll,
            g_config.hplComfortSuppressClimbRoll);
}

bool ShouldLog(uint64_t count)
{
    return count <= 8
        || count % static_cast<uint64_t>(g_config.hplComfortLogInterval) == 0;
}

void HookSetCameraPosAdd(void* player, int type, const float* vector)
{
    const uint64_t call = g_cameraAddCalls.fetch_add(1, std::memory_order_relaxed) + 1;
    const bool tracking = TrackingActive();
    const bool terminalTakeover = g_config.hplControllerTerminalDiegetic
        && type == static_cast<int>(comfort_math::CameraAddType::Terminal)
        && IsHPLPlayerStateActiveNow(8, player, nullptr);
    const bool suppress = tracking
        && (terminalTakeover || comfort_math::ShouldSuppressCameraAdd(
            type,
            g_config.hplComfortSuppressHeadBob,
            g_config.hplComfortSuppressCameraShake,
            g_config.hplComfortSuppressSway));
    if (!suppress) {
        if (!tracking) g_trackingInactive.fetch_add(1, std::memory_order_relaxed);
        g_originalSetCameraPosAdd(player, type, vector);
        return;
    }

    constexpr std::array<float, 3> kZero = {0.0f, 0.0f, 0.0f};
    const uint64_t suppressed = g_cameraAddSuppressed.fetch_add(1, std::memory_order_relaxed) + 1;
    switch (static_cast<comfort_math::CameraAddType>(type)) {
    case comfort_math::CameraAddType::Bob:
        g_bobSuppressed.fetch_add(1, std::memory_order_relaxed);
        break;
    case comfort_math::CameraAddType::Shake:
        g_shakeSuppressed.fetch_add(1, std::memory_order_relaxed);
        break;
    case comfort_math::CameraAddType::Sway:
        g_swaySuppressed.fetch_add(1, std::memory_order_relaxed);
        break;
    case comfort_math::CameraAddType::Terminal:
        g_terminalSuppressed.fetch_add(1, std::memory_order_relaxed);
        break;
    default:
        break;
    }
    if (ShouldLog(suppressed)) {
        Logger::Instance().Write(
            LogLevel::Info,
            "hpl_comfort_camera_add call=%llu suppressed=%llu type=%d typeName=%s input=%.5f,%.5f,%.5f policy=semantic_zero",
            static_cast<unsigned long long>(call),
            static_cast<unsigned long long>(suppressed),
            type,
            comfort_math::CameraAddTypeName(type),
            vector != nullptr ? vector[0] : 0.0f,
            vector != nullptr ? vector[1] : 0.0f,
            vector != nullptr ? vector[2] : 0.0f);
    }
    g_originalSetCameraPosAdd(player, type, kZero.data());
}

void HookSetCameraRoll(void* player, int type, float value)
{
    const uint64_t call = g_rollCalls.fetch_add(1, std::memory_order_relaxed) + 1;
    if (!ShouldSuppressRoll(type)) {
        g_originalSetCameraRoll(player, type, value);
        return;
    }
    const uint64_t suppressed = g_rollSuppressed.fetch_add(1, std::memory_order_relaxed) + 1;
    g_rollSetSuppressed.fetch_add(1, std::memory_order_relaxed);
    if (ShouldLog(suppressed)) {
        Logger::Instance().Write(
            LogLevel::Info,
            "hpl_comfort_camera_roll call=%llu suppressed=%llu route=set type=%d typeName=%s input=%.6f output=0 policy=semantic_zero",
            static_cast<unsigned long long>(call),
            static_cast<unsigned long long>(suppressed),
            type,
            comfort_math::CameraRollTypeName(type),
            value);
    }
    g_originalSetCameraRoll(player, type, 0.0f);
}

void HookFadeCameraRoll(
    void* player, int type, float value, float speedMultiplier, float maximumSpeed)
{
    const uint64_t call = g_rollCalls.fetch_add(1, std::memory_order_relaxed) + 1;
    if (!ShouldSuppressRoll(type)) {
        g_originalFadeCameraRoll(player, type, value, speedMultiplier, maximumSpeed);
        return;
    }
    const uint64_t suppressed = g_rollSuppressed.fetch_add(1, std::memory_order_relaxed) + 1;
    g_rollFadeSuppressed.fetch_add(1, std::memory_order_relaxed);
    if (ShouldLog(suppressed)) {
        Logger::Instance().Write(
            LogLevel::Info,
            "hpl_comfort_camera_roll call=%llu suppressed=%llu route=fade type=%d typeName=%s input=%.6f output=0 speedMultiplier=%.4f maximumSpeed=%.4f policy=semantic_zero_preserve_fade",
            static_cast<unsigned long long>(call),
            static_cast<unsigned long long>(suppressed),
            type,
            comfort_math::CameraRollTypeName(type),
            value,
            speedMultiplier,
            maximumSpeed);
    }
    g_originalFadeCameraRoll(player, type, 0.0f, speedMultiplier, maximumSpeed);
}

void HookSetDepthOfFieldActive(void* world, bool active)
{
    const uint64_t call = g_dofCalls.fetch_add(1, std::memory_order_relaxed) + 1;
    if (active) g_dofEnableRequests.fetch_add(1, std::memory_order_relaxed);
    const bool suppress = active && g_config.hplComfortDepthOfFieldControl && TrackingActive();
    if (suppress) {
        const uint64_t suppressed = g_dofSuppressed.fetch_add(1, std::memory_order_relaxed) + 1;
        if (ShouldLog(suppressed)) {
            Logger::Instance().Write(
                LogLevel::Info,
                "hpl_comfort_depth_of_field call=%llu suppressed=%llu requested=1 output=0 world=%p policy=vr_active_disable",
                static_cast<unsigned long long>(call),
                static_cast<unsigned long long>(suppressed),
                world);
        }
    }
    if (world != nullptr) {
        auto* enabled = reinterpret_cast<uint8_t*>(world) + kDepthOfFieldActiveOffset;
        *enabled = active && !suppress ? 1 : 0;
    }
}

const char* OpticsChannelName(comfort_math::OpticsChannel channel)
{
    switch (channel) {
    case comfort_math::OpticsChannel::Fov: return "fov";
    case comfort_math::OpticsChannel::FovMultiplier: return "fov_multiplier";
    case comfort_math::OpticsChannel::AspectMultiplier: return "aspect_multiplier";
    default: return "unknown";
    }
}

void ApplyOpticsTarget(
    void* player,
    comfort_math::OpticsChannel channel,
    float requestedTarget,
    float speed,
    size_t targetOffset,
    size_t speedOffset,
    bool configured,
    std::atomic<uint64_t>& channelSuppressed)
{
    const uint64_t call = g_opticsCalls.fetch_add(1, std::memory_order_relaxed) + 1;
    const bool suppress = g_config.hplComfortOpticsControl && configured && TrackingActive();
    float defaultFov = requestedTarget;
    if (channel == comfort_math::OpticsChannel::Fov && player != nullptr) {
        defaultFov = *reinterpret_cast<const float*>(
            reinterpret_cast<const std::byte*>(player) + kPlayerDefaultFovOffset);
    }
    const float output = suppress
        ? comfort_math::ResolveComfortOpticsTarget(channel, requestedTarget, defaultFov)
        : requestedTarget;
    if (suppress) {
        const uint64_t suppressed = g_opticsSuppressed.fetch_add(1, std::memory_order_relaxed) + 1;
        channelSuppressed.fetch_add(1, std::memory_order_relaxed);
        if (ShouldLog(suppressed)) {
            Logger::Instance().Write(
                LogLevel::Info,
                "hpl_comfort_optics call=%llu suppressed=%llu channel=%s requested=%.6f output=%.6f defaultFov=%.6f speed=%.6f policy=vr_native_optics",
                static_cast<unsigned long long>(call),
                static_cast<unsigned long long>(suppressed),
                OpticsChannelName(channel),
                requestedTarget,
                output,
                defaultFov,
                speed);
        }
    }
    if (player != nullptr) {
        auto* bytes = reinterpret_cast<std::byte*>(player);
        *reinterpret_cast<float*>(bytes + targetOffset) = output;
        *reinterpret_cast<float*>(bytes + speedOffset) = speed;
    }
}

void HookFadeCameraFovMultiplier(void* player, float target, float speed)
{
    ApplyOpticsTarget(
        player,
        comfort_math::OpticsChannel::FovMultiplier,
        target,
        speed,
        kPlayerFovMultiplierGoalOffset,
        kPlayerFovMultiplierSpeedOffset,
        g_config.hplComfortSuppressFovMultiplier,
        g_fovMultiplierSuppressed);
}

void HookFadeCameraAspectMultiplier(void* player, float target, float speed)
{
    ApplyOpticsTarget(
        player,
        comfort_math::OpticsChannel::AspectMultiplier,
        target,
        speed,
        kPlayerAspectMultiplierGoalOffset,
        kPlayerAspectMultiplierSpeedOffset,
        g_config.hplComfortSuppressAspectMultiplier,
        g_aspectMultiplierSuppressed);
}

void HookFadeCameraFov(void* player, float target, float speed)
{
    ApplyOpticsTarget(
        player,
        comfort_math::OpticsChannel::Fov,
        target,
        speed,
        kPlayerFovGoalOffset,
        kPlayerFovSpeedOffset,
        g_config.hplComfortSuppressFov,
        g_fovSuppressed);
}

bool WriteCodeBytes(void* target, const void* bytes, size_t size)
{
    DWORD oldProtect = 0;
    if (target == nullptr || bytes == nullptr || size == 0
        || !VirtualProtect(target, size, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        return false;
    }
    std::memcpy(target, bytes, size);
    FlushInstructionCache(GetCurrentProcess(), target, size);
    DWORD ignored = 0;
    VirtualProtect(target, size, oldProtect, &ignored);
    return true;
}

bool InstallMinHook(
    void* target, void* hook, void** original, void*& installedTarget, const char* name)
{
    MH_STATUS status = MH_CreateHook(target, hook, original);
    if (status != MH_OK) {
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_comfort_bridge install_failed reason=create_hook name=%s status=%s",
            name,
            MH_StatusToString(status));
        return false;
    }
    status = MH_EnableHook(target);
    if (status != MH_OK && status != MH_ERROR_ENABLED) {
        MH_RemoveHook(target);
        *original = nullptr;
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_comfort_bridge install_failed reason=enable_hook name=%s status=%s",
            name,
            MH_StatusToString(status));
        return false;
    }
    installedTarget = target;
    return true;
}

void RemoveMinHook(void*& target)
{
    if (target != nullptr) {
        MH_DisableHook(target);
        MH_RemoveHook(target);
        target = nullptr;
    }
}

bool InstallAbsoluteJumpPatch(
    void* target,
    void* hook,
    uint8_t* original,
    size_t size,
    void*& installedTarget)
{
    if (target == nullptr || hook == nullptr || original == nullptr || size < 12) return false;
    std::memcpy(original, target, size);
    std::array<uint8_t, 32> jump{};
    if (size > jump.size()) return false;
    jump.fill(0x90);
    jump[0] = 0x48;
    jump[1] = 0xb8;
    const uintptr_t hookAddress = reinterpret_cast<uintptr_t>(hook);
    std::memcpy(jump.data() + 2, &hookAddress, sizeof(hookAddress));
    jump[10] = 0xff;
    jump[11] = 0xe0;
    if (!WriteCodeBytes(target, jump.data(), size)) return false;
    installedTarget = target;
    return true;
}

void RemoveAbsoluteJumpPatch(
    void*& target, const uint8_t* original, size_t size)
{
    if (target == nullptr) return;
    WriteCodeBytes(target, original, size);
    target = nullptr;
}

void RollbackHooks()
{
    RemoveAbsoluteJumpPatch(
        g_fadeCameraFovTarget, g_fovOriginal.data(), g_fovOriginal.size());
    RemoveAbsoluteJumpPatch(
        g_fadeCameraAspectMultiplierTarget,
        g_aspectMultiplierOriginal.data(),
        g_aspectMultiplierOriginal.size());
    RemoveAbsoluteJumpPatch(
        g_fadeCameraFovMultiplierTarget,
        g_fovMultiplierOriginal.data(),
        g_fovMultiplierOriginal.size());
    RemoveAbsoluteJumpPatch(
        g_setDepthOfFieldActiveTarget,
        g_depthOfFieldOriginal.data(),
        g_depthOfFieldOriginal.size());
    RemoveMinHook(g_setCameraRollTarget);
    RemoveMinHook(g_fadeCameraRollTarget);
    RemoveMinHook(g_setCameraPosAddTarget);
    g_originalSetCameraPosAdd = nullptr;
    g_originalSetCameraRoll = nullptr;
    g_originalFadeCameraRoll = nullptr;
}

} // namespace

bool InstallHPLComfortBridge(const Config& config)
{
    std::lock_guard lock(g_installMutex);
    g_config = config;
    const bool cameraAddEnabled = config.hplComfortCameraAddControl
        || config.hplControllerTerminalDiegetic;
    const bool rollEnabled = config.hplComfortCameraRollControl;
    const bool dofEnabled = config.hplComfortDepthOfFieldControl;
    const bool opticsEnabled = config.hplComfortOpticsControl;
    if (!cameraAddEnabled && !rollEnabled && !dofEnabled && !opticsEnabled) {
        Logger::Instance().Write(LogLevel::Info, "hpl_comfort_bridge disabled config=0");
        return true;
    }
    if (g_setCameraPosAddTarget != nullptr || g_setCameraRollTarget != nullptr
        || g_fadeCameraRollTarget != nullptr || g_setDepthOfFieldActiveTarget != nullptr
        || g_fadeCameraFovMultiplierTarget != nullptr
        || g_fadeCameraAspectMultiplierTarget != nullptr
        || g_fadeCameraFovTarget != nullptr) {
        return true;
    }

    HMODULE executable = GetModuleHandleW(nullptr);
    const auto* base = reinterpret_cast<const std::byte*>(executable);
    const bool rangeValid = (!cameraAddEnabled
            || IsInsideImage(executable, kSetCameraPosAddRva, sizeof(kSetCameraPosAddSignature)))
        && (!rollEnabled
            || (IsInsideImage(executable, kFadeCameraRollRva, sizeof(kFadeCameraRollSignature))
                && IsInsideImage(executable, kSetCameraRollRva, sizeof(kSetCameraRollSignature))))
        && (!dofEnabled
            || IsInsideImage(executable, kSetDepthOfFieldActiveRva,
                sizeof(kSetDepthOfFieldActiveSignature)))
        && (!opticsEnabled
            || (IsInsideImage(executable, kFadeCameraFovMultiplierRva,
                    sizeof(kFadeCameraFovMultiplierSignature))
                && IsInsideImage(executable, kFadeCameraAspectMultiplierRva,
                    sizeof(kFadeCameraAspectMultiplierSignature))
                && IsInsideImage(executable, kFadeCameraFovRva,
                    sizeof(kFadeCameraFovSignature))));
    if (!rangeValid) {
        Logger::Instance().Write(LogLevel::Error, "hpl_comfort_bridge install_failed reason=invalid_image_range");
        return false;
    }

    const bool signaturesValid = (!cameraAddEnabled
            || std::memcmp(base + kSetCameraPosAddRva, kSetCameraPosAddSignature,
                sizeof(kSetCameraPosAddSignature)) == 0)
        && (!rollEnabled
            || (std::memcmp(base + kFadeCameraRollRva, kFadeCameraRollSignature,
                    sizeof(kFadeCameraRollSignature)) == 0
                && std::memcmp(base + kSetCameraRollRva, kSetCameraRollSignature,
                    sizeof(kSetCameraRollSignature)) == 0))
        && (!dofEnabled
            || std::memcmp(base + kSetDepthOfFieldActiveRva, kSetDepthOfFieldActiveSignature,
                sizeof(kSetDepthOfFieldActiveSignature)) == 0)
        && (!opticsEnabled
            || (std::memcmp(base + kFadeCameraFovMultiplierRva,
                    kFadeCameraFovMultiplierSignature,
                    sizeof(kFadeCameraFovMultiplierSignature)) == 0
                && std::memcmp(base + kFadeCameraAspectMultiplierRva,
                    kFadeCameraAspectMultiplierSignature,
                    sizeof(kFadeCameraAspectMultiplierSignature)) == 0
                && std::memcmp(base + kFadeCameraFovRva, kFadeCameraFovSignature,
                    sizeof(kFadeCameraFovSignature)) == 0));
    if (!signaturesValid) {
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_comfort_bridge install_failed reason=signature_mismatch cameraAddRva=0x%llx fadeRollRva=0x%llx setRollRva=0x%llx dofRva=0x%llx fovMulRva=0x%llx aspectMulRva=0x%llx fovRva=0x%llx",
            static_cast<unsigned long long>(kSetCameraPosAddRva),
            static_cast<unsigned long long>(kFadeCameraRollRva),
            static_cast<unsigned long long>(kSetCameraRollRva),
            static_cast<unsigned long long>(kSetDepthOfFieldActiveRva),
            static_cast<unsigned long long>(kFadeCameraFovMultiplierRva),
            static_cast<unsigned long long>(kFadeCameraAspectMultiplierRva),
            static_cast<unsigned long long>(kFadeCameraFovRva));
        return false;
    }

    if (cameraAddEnabled
        && !InstallMinHook(
            const_cast<std::byte*>(base + kSetCameraPosAddRva),
            reinterpret_cast<void*>(&HookSetCameraPosAdd),
            reinterpret_cast<void**>(&g_originalSetCameraPosAdd),
            g_setCameraPosAddTarget,
            "SetCameraPosAdd")) {
        RollbackHooks();
        return false;
    }
    if (rollEnabled
        && (!InstallMinHook(
                const_cast<std::byte*>(base + kFadeCameraRollRva),
                reinterpret_cast<void*>(&HookFadeCameraRoll),
                reinterpret_cast<void**>(&g_originalFadeCameraRoll),
                g_fadeCameraRollTarget,
                "FadeCameraRollTo")
            || !InstallMinHook(
                const_cast<std::byte*>(base + kSetCameraRollRva),
                reinterpret_cast<void*>(&HookSetCameraRoll),
                reinterpret_cast<void**>(&g_originalSetCameraRoll),
                g_setCameraRollTarget,
                "SetCameraRoll"))) {
        RollbackHooks();
        return false;
    }
    if (dofEnabled
        && !InstallAbsoluteJumpPatch(
            const_cast<std::byte*>(base + kSetDepthOfFieldActiveRva),
            reinterpret_cast<void*>(&HookSetDepthOfFieldActive),
            g_depthOfFieldOriginal.data(),
            g_depthOfFieldOriginal.size(),
            g_setDepthOfFieldActiveTarget)) {
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_comfort_bridge install_failed reason=depth_of_field_patch");
        RollbackHooks();
        return false;
    }
    if (opticsEnabled
        && (!InstallAbsoluteJumpPatch(
                const_cast<std::byte*>(base + kFadeCameraFovMultiplierRva),
                reinterpret_cast<void*>(&HookFadeCameraFovMultiplier),
                g_fovMultiplierOriginal.data(),
                g_fovMultiplierOriginal.size(),
                g_fadeCameraFovMultiplierTarget)
            || !InstallAbsoluteJumpPatch(
                const_cast<std::byte*>(base + kFadeCameraAspectMultiplierRva),
                reinterpret_cast<void*>(&HookFadeCameraAspectMultiplier),
                g_aspectMultiplierOriginal.data(),
                g_aspectMultiplierOriginal.size(),
                g_fadeCameraAspectMultiplierTarget)
            || !InstallAbsoluteJumpPatch(
                const_cast<std::byte*>(base + kFadeCameraFovRva),
                reinterpret_cast<void*>(&HookFadeCameraFov),
                g_fovOriginal.data(),
                g_fovOriginal.size(),
                g_fadeCameraFovTarget))) {
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_comfort_bridge install_failed reason=optics_patch");
        RollbackHooks();
        return false;
    }

    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_comfort_bridge install_ok cameraAdd=%d cameraAddRva=0x%llx cameraRoll=%d fadeRollRva=0x%llx setRollRva=0x%llx depthOfField=%d dofRva=0x%llx optics=%d opticsRvas={fovMul=0x%llx aspectMul=0x%llx fov=0x%llx} addPolicy={bob=%d shake=%d sway=%d terminalDiegetic=%d} rollPolicy={script=%d lean=%d move=%d climb=%d} opticsPolicy={fov=%d fovMul=%d aspectMul=%d} policy=vr_active_semantic_zero",
        cameraAddEnabled ? 1 : 0,
        static_cast<unsigned long long>(kSetCameraPosAddRva),
        rollEnabled ? 1 : 0,
        static_cast<unsigned long long>(kFadeCameraRollRva),
        static_cast<unsigned long long>(kSetCameraRollRva),
        dofEnabled ? 1 : 0,
        static_cast<unsigned long long>(kSetDepthOfFieldActiveRva),
        opticsEnabled ? 1 : 0,
        static_cast<unsigned long long>(kFadeCameraFovMultiplierRva),
        static_cast<unsigned long long>(kFadeCameraAspectMultiplierRva),
        static_cast<unsigned long long>(kFadeCameraFovRva),
        config.hplComfortSuppressHeadBob ? 1 : 0,
        config.hplComfortSuppressCameraShake ? 1 : 0,
        config.hplComfortSuppressSway ? 1 : 0,
        config.hplControllerTerminalDiegetic ? 1 : 0,
        config.hplComfortSuppressScriptRoll ? 1 : 0,
        config.hplComfortSuppressLeanRoll ? 1 : 0,
        config.hplComfortSuppressMoveRoll ? 1 : 0,
        config.hplComfortSuppressClimbRoll ? 1 : 0,
        config.hplComfortSuppressFov ? 1 : 0,
        config.hplComfortSuppressFovMultiplier ? 1 : 0,
        config.hplComfortSuppressAspectMultiplier ? 1 : 0);
    return true;
}

void RemoveHPLComfortBridge()
{
    std::lock_guard lock(g_installMutex);
    RollbackHooks();
    Logger::Instance().Write(LogLevel::Info, "hpl_comfort_bridge removed");
}

void LogHPLComfortBridgeSummary()
{
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_comfort_bridge_summary cameraAddInstalled=%d rollInstalled=%d dofInstalled=%d opticsInstalled=%d cameraAddCalls=%llu cameraAddSuppressed=%llu bob=%llu shake=%llu sway=%llu terminal=%llu rollCalls=%llu rollSuppressed=%llu rollSet=%llu rollFade=%llu dofCalls=%llu dofEnableRequests=%llu dofSuppressed=%llu opticsCalls=%llu opticsSuppressed=%llu fov=%llu fovMultiplier=%llu aspectMultiplier=%llu trackingInactive=%llu",
        g_setCameraPosAddTarget != nullptr ? 1 : 0,
        g_setCameraRollTarget != nullptr && g_fadeCameraRollTarget != nullptr ? 1 : 0,
        g_setDepthOfFieldActiveTarget != nullptr ? 1 : 0,
        g_fadeCameraFovMultiplierTarget != nullptr
                && g_fadeCameraAspectMultiplierTarget != nullptr
                && g_fadeCameraFovTarget != nullptr
            ? 1 : 0,
        static_cast<unsigned long long>(g_cameraAddCalls.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_cameraAddSuppressed.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_bobSuppressed.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_shakeSuppressed.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_swaySuppressed.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_terminalSuppressed.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_rollCalls.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_rollSuppressed.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_rollSetSuppressed.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_rollFadeSuppressed.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_dofCalls.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_dofEnableRequests.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_dofSuppressed.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_opticsCalls.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_opticsSuppressed.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_fovSuppressed.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_fovMultiplierSuppressed.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_aspectMultiplierSuppressed.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_trackingInactive.load(std::memory_order_relaxed)));
}

} // namespace somavr
