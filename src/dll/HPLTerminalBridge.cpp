#include "HPLTerminalBridge.h"

#include "HPLMenuMath.h"
#include "Logger.h"

#include <Windows.h>

#include <MinHook.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>

namespace somavr {
namespace {

constexpr uintptr_t kGetCurrentImGuiRva = 0x0cca70;
constexpr uintptr_t kGetGameHudImGuiRva = 0x0cca90;
constexpr uintptr_t kImGuiGetSetRva = 0x071f20;
constexpr uintptr_t kImGuiSendMousePositionRva = 0x2f0b10;
constexpr uintptr_t kImGuiSendMouseVirtualPositionRva = 0x2f0c90;

constexpr uint8_t kGetCurrentImGuiSignature[] = {
    0x48, 0x8b, 0x05, 0x69, 0x5b, 0x6c, 0x00,
    0x48, 0x8b, 0x80, 0xe8, 0x00, 0x00, 0x00,
    0x48, 0x8b, 0x80, 0x68, 0x01, 0x00, 0x00, 0xc3,
};
constexpr uint8_t kGetGameHudImGuiSignature[] = {
    0x48, 0x8b, 0x05, 0x49, 0x5b, 0x6c, 0x00,
    0x48, 0x8b, 0x80, 0xe8, 0x00, 0x00, 0x00,
    0x48, 0x8b, 0x80, 0x60, 0x01, 0x00, 0x00, 0xc3,
};
constexpr uint8_t kImGuiGetSetSignature[] = {
    0x48, 0x8b, 0x41, 0x18, 0xc3,
};
constexpr uint8_t kSendMousePositionSignature[] = {
    0x48, 0x89, 0x5c, 0x24, 0x10, 0x57, 0x48, 0x83,
    0xec, 0x40, 0x8b, 0x81, 0x3c, 0x4f, 0x00, 0x00,
};
constexpr uint8_t kSendMouseVirtualPositionSignature[] = {
    0x8b, 0x81, 0x3c, 0x4f, 0x00, 0x00,
    0x89, 0x81, 0x44, 0x4f, 0x00, 0x00,
    0x8b, 0x81, 0x40, 0x4f, 0x00, 0x00,
};

struct Vector2f {
    float x = 0.0f;
    float y = 0.0f;
};

using GetImGuiFn = void* (*)();
using ImGuiGetSetFn = void* (*)(void* imGui);
using SendMouseVirtualPositionFn = void (*)(void* imGui, const Vector2f* position, const Vector2f* relative);

Config g_config;
GetImGuiFn g_getCurrentImGui = nullptr;
GetImGuiFn g_getGameHudImGui = nullptr;
ImGuiGetSetFn g_imGuiGetSet = nullptr;
SendMouseVirtualPositionFn g_originalSendMouseVirtualPosition = nullptr;
void* g_sendMouseVirtualPositionTarget = nullptr;
std::mutex g_installMutex;
std::mutex g_updateMutex;
std::atomic<bool> g_active = false;
std::atomic<float> g_normalizedX = 0.5f;
std::atomic<float> g_normalizedY = 0.5f;
std::atomic<uint64_t> g_generation = 0;
bool g_smoothed = false;
float g_smoothedX = 0.5f;
float g_smoothedY = 0.5f;
uint64_t g_hookGeneration = 0;
bool g_previousVirtualValid = false;
Vector2f g_previousVirtual{};
std::atomic<uint64_t> g_updates = 0;
std::atomic<uint64_t> g_projected = 0;
std::atomic<uint64_t> g_hookCalls = 0;
std::atomic<uint64_t> g_applied = 0;
std::atomic<uint64_t> g_inactiveFallbacks = 0;
std::atomic<uint64_t> g_ownerFallbacks = 0;
std::atomic<uint64_t> g_layoutFallbacks = 0;

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
        GetCurrentProcess(), source, destination, bytes, &bytesRead) != FALSE
        && bytesRead == bytes;
}

template <typename T>
bool ReadField(const void* base, size_t offset, T& value)
{
    return ReadMemory(reinterpret_cast<const std::byte*>(base) + offset, &value, sizeof(value));
}

void HookSendMouseVirtualPosition(void* imGui, const Vector2f* position, const Vector2f* relative)
{
    const uint64_t call = g_hookCalls.fetch_add(1, std::memory_order_relaxed) + 1;
    if (!g_active.load(std::memory_order_acquire)
        || g_getCurrentImGui == nullptr
        || g_getGameHudImGui == nullptr
        || g_imGuiGetSet == nullptr) {
        g_inactiveFallbacks.fetch_add(1, std::memory_order_relaxed);
        g_originalSendMouseVirtualPosition(imGui, position, relative);
        return;
    }

    void* currentImGui = g_getCurrentImGui();
    void* gameHudImGui = g_getGameHudImGui();
    if (imGui == nullptr || imGui != currentImGui || imGui == gameHudImGui) {
        g_ownerFallbacks.fetch_add(1, std::memory_order_relaxed);
        g_originalSendMouseVirtualPosition(imGui, position, relative);
        return;
    }

    void* guiSet = g_imGuiGetSet(imGui);
    float width = 0.0f;
    float height = 0.0f;
    float offsetX = 0.0f;
    float offsetY = 0.0f;
    uint8_t is3d = 0;
    if (guiSet == nullptr
        || !ReadField(guiSet, 0x100, width)
        || !ReadField(guiSet, 0x104, height)
        || !ReadField(guiSet, 0x108, offsetX)
        || !ReadField(guiSet, 0x10c, offsetY)
        || !ReadField(guiSet, 0x139, is3d)
        || is3d == 0
        || !std::isfinite(width) || !std::isfinite(height)
        || !std::isfinite(offsetX) || !std::isfinite(offsetY)
        || width <= 1.0f || height <= 1.0f
        || width > 32768.0f || height > 32768.0f) {
        g_layoutFallbacks.fetch_add(1, std::memory_order_relaxed);
        g_originalSendMouseVirtualPosition(imGui, position, relative);
        return;
    }

    const Vector2f virtualPosition{
        std::clamp(g_normalizedX.load(std::memory_order_relaxed), 0.0f, 1.0f) * width - offsetX,
        std::clamp(g_normalizedY.load(std::memory_order_relaxed), 0.0f, 1.0f) * height - offsetY,
    };
    const uint64_t generation = g_generation.load(std::memory_order_acquire);
    Vector2f virtualRelative{};
    if (g_previousVirtualValid && generation == g_hookGeneration) {
        virtualRelative.x = virtualPosition.x - g_previousVirtual.x;
        virtualRelative.y = virtualPosition.y - g_previousVirtual.y;
    }
    g_hookGeneration = generation;
    g_previousVirtual = virtualPosition;
    g_previousVirtualValid = true;
    g_originalSendMouseVirtualPosition(imGui, &virtualPosition, &virtualRelative);

    const uint64_t applied = g_applied.fetch_add(1, std::memory_order_relaxed) + 1;
    const uint64_t interval = static_cast<uint64_t>(std::max(g_config.hplControllerLogInterval, 1));
    if (applied <= 8 || applied % interval == 0) {
        Logger::Instance().Write(
            LogLevel::Info,
            "hpl_terminal_pointer applied=%llu hookCall=%llu imGui=%p set=%p virtual=%.2f,%.2f relative=%.2f,%.2f size=%.1f,%.1f offset=%.1f,%.1f",
            static_cast<unsigned long long>(applied),
            static_cast<unsigned long long>(call),
            imGui,
            guiSet,
            virtualPosition.x,
            virtualPosition.y,
            virtualRelative.x,
            virtualRelative.y,
            width,
            height,
            offsetX,
            offsetY);
    }
}

} // namespace

bool InstallHPLTerminalBridge(const Config& config)
{
    std::lock_guard lock(g_installMutex);
    g_config = config;
    if (!config.hplControllerInput || !config.hplControllerTerminalPointer) {
        Logger::Instance().Write(
            LogLevel::Info,
            "hpl_terminal_bridge disabled controller=%d terminalPointer=%d",
            config.hplControllerInput ? 1 : 0,
            config.hplControllerTerminalPointer ? 1 : 0);
        return true;
    }
    if (g_sendMouseVirtualPositionTarget != nullptr) return true;

    HMODULE executable = GetModuleHandleW(nullptr);
    struct Guard {
        uintptr_t rva;
        const uint8_t* signature;
        size_t size;
    };
    const Guard guards[] = {
        {kGetCurrentImGuiRva, kGetCurrentImGuiSignature, sizeof(kGetCurrentImGuiSignature)},
        {kGetGameHudImGuiRva, kGetGameHudImGuiSignature, sizeof(kGetGameHudImGuiSignature)},
        {kImGuiGetSetRva, kImGuiGetSetSignature, sizeof(kImGuiGetSetSignature)},
        {kImGuiSendMousePositionRva, kSendMousePositionSignature, sizeof(kSendMousePositionSignature)},
        {kImGuiSendMouseVirtualPositionRva, kSendMouseVirtualPositionSignature, sizeof(kSendMouseVirtualPositionSignature)},
    };
    auto* base = reinterpret_cast<std::byte*>(executable);
    for (const Guard& guard : guards) {
        if (!IsInsideImage(executable, guard.rva, guard.size)
            || std::memcmp(base + guard.rva, guard.signature, guard.size) != 0) {
            Logger::Instance().Write(
                LogLevel::Error,
                "hpl_terminal_bridge install_failed reason=signature_mismatch rva=0x%llx",
                static_cast<unsigned long long>(guard.rva));
            return false;
        }
    }

    g_getCurrentImGui = reinterpret_cast<GetImGuiFn>(base + kGetCurrentImGuiRva);
    g_getGameHudImGui = reinterpret_cast<GetImGuiFn>(base + kGetGameHudImGuiRva);
    g_imGuiGetSet = reinterpret_cast<ImGuiGetSetFn>(base + kImGuiGetSetRva);
    void* target = base + kImGuiSendMouseVirtualPositionRva;
    MH_STATUS status = MH_CreateHook(
        target,
        reinterpret_cast<void*>(&HookSendMouseVirtualPosition),
        reinterpret_cast<void**>(&g_originalSendMouseVirtualPosition));
    if (status != MH_OK && status != MH_ERROR_ALREADY_CREATED) {
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_terminal_bridge install_failed reason=create_hook status=%s",
            MH_StatusToString(status));
        return false;
    }
    status = MH_EnableHook(target);
    if (status != MH_OK && status != MH_ERROR_ENABLED) {
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_terminal_bridge install_failed reason=enable_hook status=%s",
            MH_StatusToString(status));
        MH_RemoveHook(target);
        g_originalSendMouseVirtualPosition = nullptr;
        return false;
    }
    g_sendMouseVirtualPositionTarget = target;
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_terminal_bridge install_ok sendMousePositionRva=0x%llx sendMouseVirtualRva=0x%llx hooked=sendMouseVirtual horizontalFovDegrees=%.2f verticalFovDegrees=%.2f smoothing=%.3f policy=terminal_state_current_3d_imgui_only",
        static_cast<unsigned long long>(kImGuiSendMousePositionRva),
        static_cast<unsigned long long>(kImGuiSendMouseVirtualPositionRva),
        config.hplControllerTerminalPointerHorizontalDegrees,
        config.hplControllerTerminalPointerVerticalDegrees,
        config.hplControllerTerminalPointerSmoothing);
    return true;
}

bool UpdateHPLTerminalPointer(
    const OpenXRHeadPose& headPose,
    const OpenXRControllerPose& aimPose)
{
    std::lock_guard lock(g_updateMutex);
    g_updates.fetch_add(1, std::memory_order_relaxed);
    if (!g_config.hplControllerTerminalPointer
        || !headPose.valid || !headPose.orientationTracked
        || !aimPose.valid || !aimPose.orientationTracked) {
        DeactivateHPLTerminalPointer();
        return false;
    }

    menu_math::MenuPointerPosition pointer;
    if (!menu_math::ProjectAimToMenu(
            {headPose.orientationX, headPose.orientationY, headPose.orientationZ, headPose.orientationW},
            {aimPose.orientationX, aimPose.orientationY, aimPose.orientationZ, aimPose.orientationW},
            g_config.hplControllerTerminalPointerHorizontalDegrees,
            g_config.hplControllerTerminalPointerVerticalDegrees,
            pointer)) {
        DeactivateHPLTerminalPointer();
        return false;
    }

    const float blend = std::clamp(g_config.hplControllerTerminalPointerSmoothing, 0.0f, 1.0f);
    if (!g_smoothed || !g_active.load(std::memory_order_relaxed)) {
        g_smoothedX = pointer.x;
        g_smoothedY = pointer.y;
        g_smoothed = true;
        g_generation.fetch_add(1, std::memory_order_release);
    } else {
        g_smoothedX += (pointer.x - g_smoothedX) * blend;
        g_smoothedY += (pointer.y - g_smoothedY) * blend;
    }
    g_normalizedX.store(g_smoothedX, std::memory_order_relaxed);
    g_normalizedY.store(g_smoothedY, std::memory_order_relaxed);
    g_active.store(true, std::memory_order_release);
    g_projected.fetch_add(1, std::memory_order_relaxed);
    return true;
}

void DeactivateHPLTerminalPointer()
{
    const bool wasActive = g_active.exchange(false, std::memory_order_acq_rel);
    if (wasActive) g_generation.fetch_add(1, std::memory_order_release);
    g_smoothed = false;
}

void LogHPLTerminalBridgeSummary()
{
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_terminal_bridge_summary enabled=%d active=%d updates=%llu projected=%llu hookCalls=%llu applied=%llu fallbacks={inactive=%llu owner=%llu layout=%llu}",
        g_config.hplControllerTerminalPointer ? 1 : 0,
        g_active.load(std::memory_order_relaxed) ? 1 : 0,
        static_cast<unsigned long long>(g_updates.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_projected.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_hookCalls.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_applied.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_inactiveFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_ownerFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_layoutFallbacks.load(std::memory_order_relaxed)));
}

void RemoveHPLTerminalBridge()
{
    std::lock_guard lock(g_installMutex);
    DeactivateHPLTerminalPointer();
    if (g_sendMouseVirtualPositionTarget != nullptr) {
        MH_DisableHook(g_sendMouseVirtualPositionTarget);
        MH_RemoveHook(g_sendMouseVirtualPositionTarget);
    }
    g_sendMouseVirtualPositionTarget = nullptr;
    g_originalSendMouseVirtualPosition = nullptr;
    g_imGuiGetSet = nullptr;
    g_getGameHudImGui = nullptr;
    g_getCurrentImGui = nullptr;
    Logger::Instance().Write(LogLevel::Info, "hpl_terminal_bridge removed");
}

} // namespace somavr
