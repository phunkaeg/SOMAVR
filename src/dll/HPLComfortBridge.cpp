#include "HPLComfortBridge.h"

#include "HPLCameraBridge.h"
#include "HPLComfortMath.h"
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
constexpr uint8_t kSetCameraPosAddSignature[] = {
    0x40, 0x53,
    0x48, 0x83, 0xec, 0x20,
    0x49, 0x8b, 0xd8,
    0xe8, 0x72, 0xf4, 0xff, 0xff,
    0xc6, 0x40, 0x1c, 0x00,
};

using SetCameraPosAddFn = void (*)(void* player, int type, const float* vector);

Config g_config;
SetCameraPosAddFn g_originalSetCameraPosAdd = nullptr;
void* g_setCameraPosAddTarget = nullptr;
std::mutex g_installMutex;
std::atomic<uint64_t> g_calls = 0;
std::atomic<uint64_t> g_suppressed = 0;
std::atomic<uint64_t> g_bobSuppressed = 0;
std::atomic<uint64_t> g_shakeSuppressed = 0;
std::atomic<uint64_t> g_swaySuppressed = 0;
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

void HookSetCameraPosAdd(void* player, int type, const float* vector)
{
    const uint64_t call = g_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    const HPLCameraBridgeStatus camera = GetHPLCameraBridgeStatus();
    const bool suppress = camera.trackingEnabled
        && comfort_math::ShouldSuppressCameraAdd(
            type,
            g_config.hplComfortSuppressHeadBob,
            g_config.hplComfortSuppressCameraShake,
            g_config.hplComfortSuppressSway);
    if (!suppress) {
        if (!camera.trackingEnabled) {
            g_trackingInactive.fetch_add(1, std::memory_order_relaxed);
        }
        g_originalSetCameraPosAdd(player, type, vector);
        return;
    }

    constexpr std::array<float, 3> kZero = {0.0f, 0.0f, 0.0f};
    const uint64_t suppressed = g_suppressed.fetch_add(1, std::memory_order_relaxed) + 1;
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
    default:
        break;
    }
    if (suppressed <= 8
        || suppressed % static_cast<uint64_t>(g_config.hplComfortLogInterval) == 0) {
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

} // namespace

bool InstallHPLComfortBridge(const Config& config)
{
    std::lock_guard lock(g_installMutex);
    g_config = config;
    if (!config.hplComfortCameraAddControl) {
        Logger::Instance().Write(LogLevel::Info, "hpl_comfort_bridge disabled config=0");
        return true;
    }
    if (g_setCameraPosAddTarget != nullptr) return true;

    HMODULE executable = GetModuleHandleW(nullptr);
    if (!IsInsideImage(executable, kSetCameraPosAddRva, sizeof(kSetCameraPosAddSignature))) {
        Logger::Instance().Write(LogLevel::Error, "hpl_comfort_bridge install_failed reason=invalid_image_range");
        return false;
    }
    auto* target = reinterpret_cast<std::byte*>(executable) + kSetCameraPosAddRva;
    if (std::memcmp(target, kSetCameraPosAddSignature, sizeof(kSetCameraPosAddSignature)) != 0) {
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_comfort_bridge install_failed reason=signature_mismatch rva=0x%llx",
            static_cast<unsigned long long>(kSetCameraPosAddRva));
        return false;
    }

    MH_STATUS status = MH_CreateHook(
        target,
        reinterpret_cast<void*>(&HookSetCameraPosAdd),
        reinterpret_cast<void**>(&g_originalSetCameraPosAdd));
    if (status != MH_OK && status != MH_ERROR_ALREADY_CREATED) {
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_comfort_bridge install_failed reason=create_hook status=%s",
            MH_StatusToString(status));
        return false;
    }
    status = MH_EnableHook(target);
    if (status != MH_OK && status != MH_ERROR_ENABLED) {
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_comfort_bridge install_failed reason=enable_hook status=%s",
            MH_StatusToString(status));
        return false;
    }

    g_setCameraPosAddTarget = target;
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_comfort_bridge install_ok function=SetCameraPosAdd rva=0x%llx target=%p bob=%d shake=%d sway=%d policy=vr_active_semantic_zero",
        static_cast<unsigned long long>(kSetCameraPosAddRva),
        target,
        config.hplComfortSuppressHeadBob ? 1 : 0,
        config.hplComfortSuppressCameraShake ? 1 : 0,
        config.hplComfortSuppressSway ? 1 : 0);
    return true;
}

void RemoveHPLComfortBridge()
{
    std::lock_guard lock(g_installMutex);
    if (g_setCameraPosAddTarget != nullptr) {
        MH_DisableHook(g_setCameraPosAddTarget);
        MH_RemoveHook(g_setCameraPosAddTarget);
    }
    g_setCameraPosAddTarget = nullptr;
    g_originalSetCameraPosAdd = nullptr;
    Logger::Instance().Write(LogLevel::Info, "hpl_comfort_bridge removed");
}

void LogHPLComfortBridgeSummary()
{
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_comfort_bridge_summary installed=%d calls=%llu suppressed=%llu bob=%llu shake=%llu sway=%llu trackingInactive=%llu",
        g_setCameraPosAddTarget != nullptr ? 1 : 0,
        static_cast<unsigned long long>(g_calls.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_suppressed.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_bobSuppressed.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_shakeSuppressed.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_swaySuppressed.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_trackingInactive.load(std::memory_order_relaxed)));
}

} // namespace somavr
