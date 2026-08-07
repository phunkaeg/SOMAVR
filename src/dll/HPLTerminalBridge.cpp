#include "HPLTerminalBridge.h"

#include "HPLCameraBridge.h"
#include "HPLMenuMath.h"
#include "HPLPlayerState.h"
#include "HPLTerminalMath.h"
#include "Logger.h"

#include <Windows.h>

#include <MinHook.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <mutex>

namespace somavr {
namespace {

constexpr uintptr_t kGetCurrentImGuiRva = 0x0cca70;
constexpr uintptr_t kGetGameHudImGuiRva = 0x0cca90;
constexpr uintptr_t kImGuiGetSetRva = 0x071f20;
constexpr uintptr_t kImGuiSendMousePositionRva = 0x2f0b10;
constexpr uintptr_t kImGuiSendMouseVirtualPositionRva = 0x2f0c90;
constexpr uintptr_t kProjectRayToVirtualRva = 0x3132d0;
constexpr uintptr_t kSetFeetPositionRva = 0x237920;
constexpr uintptr_t kRotateCameraTowardsRva = 0x1562e0;
constexpr size_t kGameContextImGuiManagerOffset = 0xe8;
constexpr size_t kImGuiManagerGameHudOffset = 0x160;
constexpr size_t kImGuiManagerWorldInputOffset = 0x170;
constexpr size_t kImGuiManagerFocusedWrapperOffset = 0x180;
constexpr size_t kImGuiManagerScreenInputFlagOffset = 0x192;
constexpr size_t kImGuiWrapperSetOffset = 0x18;
constexpr size_t kImGuiWrapperEntityOffset = 0x28;
constexpr int kTerminalPlayerState = 8;

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
constexpr uint8_t kProjectRayToVirtualSignature[] = {
    0x40, 0x55, 0x53, 0x57, 0x41, 0x56, 0x41, 0x57,
    0x48, 0x8d, 0x6c, 0x24, 0xc0, 0x48, 0x81, 0xec,
    0x40, 0x01, 0x00, 0x00,
};
constexpr uint8_t kSetFeetPositionSignature[] = {
    0x48, 0x83, 0xec, 0x38, 0xf3, 0x0f, 0x10, 0x02,
    0xf3, 0x0f, 0x10, 0x89, 0x38, 0x01, 0x00, 0x00,
};
constexpr uint8_t kRotateCameraTowardsSignature[] = {
    0x48, 0x8b, 0x54, 0x24, 0x28,
    0xf3, 0x0f, 0x11, 0x89, 0x70, 0x03, 0x00, 0x00,
    0xf3, 0x0f, 0x11, 0x91, 0x74, 0x03, 0x00, 0x00,
};

struct Vector2f {
    float x = 0.0f;
    float y = 0.0f;
};

struct Vector3f {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct TerminalInputOwner {
    void* imGui = nullptr;
    void* guiSet = nullptr;
    void* guiEntity = nullptr;
};

using GetImGuiFn = void* (*)();
using ImGuiGetSetFn = void* (*)(void* imGui);
using SendMouseVirtualPositionFn = void (*)(void* imGui, const Vector2f* position, const Vector2f* relative);
using ProjectRayToVirtualFn = bool (*)(
    void* guiEntity, const Vector3f* rayStart, const Vector3f* rayEnd, Vector2f* virtualPosition);
using SetFeetPositionFn = void (*)(void* characterBody, const Vector3f* feetPosition, bool smooth);
using RotateCameraTowardsFn = void (*)(
    void* player,
    float acceleration,
    float speed,
    float maximumSpeed,
    const Vector3f* direction,
    bool localSpace);

Config g_config;
OpenXRRuntime* g_openxr = nullptr;
GetImGuiFn g_getCurrentImGui = nullptr;
GetImGuiFn g_getGameHudImGui = nullptr;
ImGuiGetSetFn g_imGuiGetSet = nullptr;
SendMouseVirtualPositionFn g_originalSendMouseVirtualPosition = nullptr;
ProjectRayToVirtualFn g_projectRayToVirtual = nullptr;
SetFeetPositionFn g_originalSetFeetPosition = nullptr;
RotateCameraTowardsFn g_originalRotateCameraTowards = nullptr;
void** g_gameContextSlot = nullptr;
void* g_sendMouseVirtualPositionTarget = nullptr;
void* g_setFeetPositionTarget = nullptr;
void* g_rotateCameraTowardsTarget = nullptr;
std::mutex g_installMutex;
std::mutex g_updateMutex;
std::atomic<bool> g_active = false;
std::atomic<float> g_normalizedX = 0.5f;
std::atomic<float> g_normalizedY = 0.5f;
std::atomic<float> g_virtualX = 0.0f;
std::atomic<float> g_virtualY = 0.0f;
std::atomic<bool> g_directVirtualCoordinates = false;
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
std::atomic<uint64_t> g_directDispatches = 0;
std::atomic<uint64_t> g_directDispatchFailures = 0;
std::atomic<uint64_t> g_inactiveFallbacks = 0;
std::atomic<uint64_t> g_ownerFallbacks = 0;
std::atomic<uint64_t> g_layoutFallbacks = 0;
std::atomic<uint64_t> g_spatialAttempts = 0;
std::atomic<uint64_t> g_spatialHits = 0;
std::atomic<uint64_t> g_spatialMisses = 0;
std::atomic<uint64_t> g_spatialUnavailable = 0;
std::atomic<uint64_t> g_headConeFallbacks = 0;
std::atomic<uint64_t> g_feetCalls = 0;
std::atomic<uint64_t> g_feetSuppressed = 0;
std::atomic<uint64_t> g_rotateCalls = 0;
std::atomic<uint64_t> g_rotateSuppressed = 0;

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

bool TrackingActive()
{
    return GetHPLCameraBridgeStatus().trackingEnabled;
}

bool ShouldLog(uint64_t count)
{
    return count <= 8
        || count % static_cast<uint64_t>(std::max(g_config.hplControllerLogInterval, 1)) == 0;
}

bool ResolveTerminalInputOwner(TerminalInputOwner& owner)
{
    owner = {};
    void* gameContext = nullptr;
    void* imGuiManager = nullptr;
    void* focusedWrapper = nullptr;
    void* gameHud = nullptr;
    uint8_t screenInput = 1;
    return g_gameContextSlot != nullptr
        && ReadMemory(g_gameContextSlot, &gameContext, sizeof(gameContext))
        && gameContext != nullptr
        && ReadField(gameContext, kGameContextImGuiManagerOffset, imGuiManager)
        && imGuiManager != nullptr
        && ReadField(
            imGuiManager,
            kImGuiManagerScreenInputFlagOffset,
            screenInput)
        && screenInput == 0
        && ReadField(
            imGuiManager,
            kImGuiManagerWorldInputOffset,
            owner.imGui)
        && owner.imGui != nullptr
        && ReadField(
            imGuiManager,
            kImGuiManagerGameHudOffset,
            gameHud)
        && owner.imGui != gameHud
        && ReadField(
            imGuiManager,
            kImGuiManagerFocusedWrapperOffset,
            focusedWrapper)
        && focusedWrapper != nullptr
        && ReadField(focusedWrapper, kImGuiWrapperSetOffset, owner.guiSet)
        && owner.guiSet != nullptr
        && ReadField(
            focusedWrapper,
            kImGuiWrapperEntityOffset,
            owner.guiEntity)
        && owner.guiEntity != nullptr;
}

void HookSetFeetPosition(void* characterBody, const Vector3f* feetPosition, bool smooth)
{
    const uint64_t call = g_feetCalls.fetch_add(1, std::memory_order_relaxed) + 1;
    const bool suppress = g_config.hplControllerTerminalDiegetic
        && TrackingActive()
        && IsHPLPlayerStateActiveNow(kTerminalPlayerState, nullptr, characterBody);
    if (!suppress) {
        g_originalSetFeetPosition(characterBody, feetPosition, smooth);
        return;
    }

    const uint64_t suppressed = g_feetSuppressed.fetch_add(1, std::memory_order_relaxed) + 1;
    if (ShouldLog(suppressed)) {
        Logger::Instance().Write(
            LogLevel::Info,
            "hpl_terminal_diegetic suppress=feet_position call=%llu suppressed=%llu body=%p",
            static_cast<unsigned long long>(call),
            static_cast<unsigned long long>(suppressed),
            characterBody);
    }
}

void HookRotateCameraTowards(
    void* player,
    float acceleration,
    float speed,
    float maximumSpeed,
    const Vector3f* direction,
    bool localSpace)
{
    const uint64_t call = g_rotateCalls.fetch_add(1, std::memory_order_relaxed) + 1;
    const bool suppress = g_config.hplControllerTerminalDiegetic
        && TrackingActive()
        && IsHPLPlayerStateActiveNow(kTerminalPlayerState, player, nullptr);
    if (!suppress) {
        g_originalRotateCameraTowards(
            player, acceleration, speed, maximumSpeed, direction, localSpace);
        return;
    }

    const uint64_t suppressed = g_rotateSuppressed.fetch_add(1, std::memory_order_relaxed) + 1;
    if (ShouldLog(suppressed)) {
        Logger::Instance().Write(
            LogLevel::Info,
            "hpl_terminal_diegetic suppress=rotate_camera_towards call=%llu suppressed=%llu player=%p",
            static_cast<unsigned long long>(call),
            static_cast<unsigned long long>(suppressed),
            player);
    }
}

enum class SpatialProjectionResult {
    Unavailable,
    Miss,
    Hit,
};

SpatialProjectionResult ProjectControllerRayToTerminal(
    const OpenXRControllerPose& aimPose,
    uint64_t gameFrame,
    Vector2f& virtualPosition,
    TerminalInputOwner& owner)
{
    g_spatialAttempts.fetch_add(1, std::memory_order_relaxed);
    if (g_config.hplControllerTerminalOverlay
        || !g_config.hplControllerTerminalRayPointer
        || g_projectRayToVirtual == nullptr
        || g_gameContextSlot == nullptr) {
        g_spatialUnavailable.fetch_add(1, std::memory_order_relaxed);
        return SpatialProjectionResult::Unavailable;
    }

    HPLTrackedPoseWorld worldAim;
    if (!ResolveTerminalInputOwner(owner)
        || !ResolveHPLTrackedPoseWorld(aimPose, gameFrame, worldAim)
        || !worldAim.valid || !worldAim.orientationTracked || !worldAim.positionTracked) {
        g_spatialUnavailable.fetch_add(1, std::memory_order_relaxed);
        return SpatialProjectionResult::Unavailable;
    }

    const float rayLength = std::max(g_config.hplControllerTerminalRayLengthMeters, 0.5f)
        * std::max(GetHPLCameraBridgeStatus().worldUnitsPerMeter, 0.001f);
    const Vector3f rayStart{
        worldAim.positionX,
        worldAim.positionY,
        worldAim.positionZ,
    };
    const Vector3f rayEnd{
        rayStart.x + worldAim.forwardX * rayLength,
        rayStart.y + worldAim.forwardY * rayLength,
        rayStart.z + worldAim.forwardZ * rayLength,
    };
    Vector2f projected{};
    if (!g_projectRayToVirtual(owner.guiEntity, &rayStart, &rayEnd, &projected)
        || !std::isfinite(projected.x) || !std::isfinite(projected.y)) {
        g_spatialMisses.fetch_add(1, std::memory_order_relaxed);
        return SpatialProjectionResult::Miss;
    }

    virtualPosition = projected;
    g_spatialHits.fetch_add(1, std::memory_order_relaxed);
    return SpatialProjectionResult::Hit;
}

bool DispatchControllerVirtualPosition(
    bool directVirtual,
    const TerminalInputOwner* spatialOwner = nullptr)
{
    if (g_originalSendMouseVirtualPosition == nullptr) {
        return false;
    }

    TerminalInputOwner resolvedOwner{};
    if (spatialOwner == nullptr) {
        if (!ResolveTerminalInputOwner(resolvedOwner)) return false;
        spatialOwner = &resolvedOwner;
    }
    void* imGui = spatialOwner->imGui;
    void* guiSet = spatialOwner->guiSet;
    float width = 0.0f;
    float height = 0.0f;
    float offsetX = 0.0f;
    float offsetY = 0.0f;
    if (guiSet == nullptr
        || !ReadField(guiSet, 0x100, width)
        || !ReadField(guiSet, 0x104, height)
        || !ReadField(guiSet, 0x108, offsetX)
        || !ReadField(guiSet, 0x10c, offsetY)
        || !std::isfinite(width) || !std::isfinite(height)
        || width <= 1.0f || height <= 1.0f) {
        g_layoutFallbacks.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    const Vector2f position = directVirtual
        ? Vector2f{
            std::clamp(g_virtualX.load(std::memory_order_relaxed), 0.0f, width),
            std::clamp(g_virtualY.load(std::memory_order_relaxed), 0.0f, height)}
        : Vector2f{
            std::clamp(g_normalizedX.load(std::memory_order_relaxed), 0.0f, 1.0f)
                * width - offsetX,
            std::clamp(g_normalizedY.load(std::memory_order_relaxed), 0.0f, 1.0f)
                * height - offsetY};
    Vector2f relative{};
    const uint64_t generation = g_generation.load(std::memory_order_acquire);
    if (g_previousVirtualValid && generation == g_hookGeneration) {
        relative.x = position.x - g_previousVirtual.x;
        relative.y = position.y - g_previousVirtual.y;
    }
    g_hookGeneration = generation;
    g_previousVirtual = position;
    g_previousVirtualValid = true;
    g_originalSendMouseVirtualPosition(imGui, &position, &relative);

    const uint64_t dispatch = g_directDispatches.fetch_add(1, std::memory_order_relaxed) + 1;
    const uint64_t applied = g_applied.fetch_add(1, std::memory_order_relaxed) + 1;
    const uint64_t interval = static_cast<uint64_t>(std::max(g_config.hplControllerLogInterval, 1));
    if (dispatch <= 8 || dispatch % interval == 0) {
        Logger::Instance().Write(
            LogLevel::Info,
            "hpl_terminal_pointer applied=%llu dispatch=%llu route=%s imGui=%p set=%p entity=%p virtual=%.2f,%.2f relative=%.2f,%.2f size=%.1f,%.1f owner=manager_world_input_0x170",
            static_cast<unsigned long long>(applied),
            static_cast<unsigned long long>(dispatch),
            directVirtual ? "spatial_mesh_ray_direct_dispatch" : "head_cone_direct_dispatch",
            imGui,
            guiSet,
            spatialOwner->guiEntity,
            position.x, position.y,
            relative.x, relative.y,
            width, height);
    }
    return true;
}

void HookSendMouseVirtualPosition(void* imGui, const Vector2f* position, const Vector2f* relative)
{
    const uint64_t call = g_hookCalls.fetch_add(1, std::memory_order_relaxed) + 1;
    if (!g_active.load(std::memory_order_acquire)) {
        g_inactiveFallbacks.fetch_add(1, std::memory_order_relaxed);
        g_originalSendMouseVirtualPosition(imGui, position, relative);
        return;
    }

    TerminalInputOwner owner{};
    if (!ResolveTerminalInputOwner(owner)
        || imGui == nullptr
        || imGui != owner.imGui) {
        g_ownerFallbacks.fetch_add(1, std::memory_order_relaxed);
        g_originalSendMouseVirtualPosition(imGui, position, relative);
        return;
    }

    void* guiSet = owner.guiSet;
    float width = 0.0f;
    float height = 0.0f;
    float offsetX = 0.0f;
    float offsetY = 0.0f;
    if (guiSet == nullptr
        || !ReadField(guiSet, 0x100, width)
        || !ReadField(guiSet, 0x104, height)
        || !ReadField(guiSet, 0x108, offsetX)
        || !ReadField(guiSet, 0x10c, offsetY)
        || !std::isfinite(width) || !std::isfinite(height)
        || !std::isfinite(offsetX) || !std::isfinite(offsetY)
        || width <= 1.0f || height <= 1.0f
        || width > 32768.0f || height > 32768.0f) {
        g_layoutFallbacks.fetch_add(1, std::memory_order_relaxed);
        g_originalSendMouseVirtualPosition(imGui, position, relative);
        return;
    }

    const bool directVirtual = g_directVirtualCoordinates.load(std::memory_order_acquire);
    const Vector2f virtualPosition = directVirtual
        ? Vector2f{
            std::clamp(g_virtualX.load(std::memory_order_relaxed), 0.0f, width),
            std::clamp(g_virtualY.load(std::memory_order_relaxed), 0.0f, height),
        }
        : Vector2f{
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
            "hpl_terminal_pointer applied=%llu hookCall=%llu route=%s imGui=%p set=%p entity=%p virtual=%.2f,%.2f relative=%.2f,%.2f size=%.1f,%.1f offset=%.1f,%.1f owner=manager_world_input_0x170",
            static_cast<unsigned long long>(applied),
            static_cast<unsigned long long>(call),
            directVirtual ? "spatial_mesh_ray" : "head_cone_fallback",
            imGui,
            guiSet,
            owner.guiEntity,
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

bool InstallHPLTerminalBridge(const Config& config, OpenXRRuntime* openxr)
{
    std::lock_guard lock(g_installMutex);
    g_config = config;
    g_openxr = openxr;
    const bool pointerRequested = config.hplControllerInput
        && config.hplControllerTerminalPointer;
    const bool diegeticRequested = config.hplControllerTerminalDiegetic;
    if (!pointerRequested && !diegeticRequested) {
        Logger::Instance().Write(
            LogLevel::Info,
            "hpl_terminal_bridge disabled controller=%d terminalPointer=%d terminalDiegetic=%d",
            config.hplControllerInput ? 1 : 0,
            config.hplControllerTerminalPointer ? 1 : 0,
            config.hplControllerTerminalDiegetic ? 1 : 0);
        return true;
    }
    if (g_sendMouseVirtualPositionTarget != nullptr
        || g_setFeetPositionTarget != nullptr
        || g_rotateCameraTowardsTarget != nullptr) return true;

    HMODULE executable = GetModuleHandleW(nullptr);
    struct Guard {
        uintptr_t rva;
        const uint8_t* signature;
        size_t size;
    };
    auto* base = reinterpret_cast<std::byte*>(executable);
    const Guard pointerGuards[] = {
        {kGetCurrentImGuiRva, kGetCurrentImGuiSignature, sizeof(kGetCurrentImGuiSignature)},
        {kGetGameHudImGuiRva, kGetGameHudImGuiSignature, sizeof(kGetGameHudImGuiSignature)},
        {kImGuiGetSetRva, kImGuiGetSetSignature, sizeof(kImGuiGetSetSignature)},
        {kImGuiSendMousePositionRva, kSendMousePositionSignature, sizeof(kSendMousePositionSignature)},
        {kImGuiSendMouseVirtualPositionRva, kSendMouseVirtualPositionSignature, sizeof(kSendMouseVirtualPositionSignature)},
        {kProjectRayToVirtualRva, kProjectRayToVirtualSignature, sizeof(kProjectRayToVirtualSignature)},
    };
    const Guard diegeticGuards[] = {
        {kSetFeetPositionRva, kSetFeetPositionSignature, sizeof(kSetFeetPositionSignature)},
        {kRotateCameraTowardsRva, kRotateCameraTowardsSignature, sizeof(kRotateCameraTowardsSignature)},
    };
    const auto validateGuards = [&](const Guard* guards, size_t count) {
        for (size_t index = 0; index < count; ++index) {
            const Guard& guard = guards[index];
            if (!IsInsideImage(executable, guard.rva, guard.size)
                || std::memcmp(base + guard.rva, guard.signature, guard.size) != 0) {
                Logger::Instance().Write(
                    LogLevel::Error,
                    "hpl_terminal_bridge install_failed reason=signature_mismatch rva=0x%llx",
                    static_cast<unsigned long long>(guard.rva));
                return false;
            }
        }
        return true;
    };
    if ((pointerRequested && !validateGuards(pointerGuards, std::size(pointerGuards)))
        || (diegeticRequested && !validateGuards(diegeticGuards, std::size(diegeticGuards)))) {
        return false;
    }

    auto installHook = [&](void* target, void* detour, void** original, void*& installedTarget, const char* name) {
        MH_STATUS status = MH_CreateHook(target, detour, original);
        if (status != MH_OK && status != MH_ERROR_ALREADY_CREATED) {
            Logger::Instance().Write(
                LogLevel::Error,
                "hpl_terminal_bridge install_failed reason=create_hook name=%s status=%s",
                name,
                MH_StatusToString(status));
            return false;
        }
        status = MH_EnableHook(target);
        if (status != MH_OK && status != MH_ERROR_ENABLED) {
            Logger::Instance().Write(
                LogLevel::Error,
                "hpl_terminal_bridge install_failed reason=enable_hook name=%s status=%s",
                name,
                MH_StatusToString(status));
            MH_RemoveHook(target);
            *original = nullptr;
            return false;
        }
        installedTarget = target;
        return true;
    };
    const auto rollback = [&]() {
        for (void** target : {&g_rotateCameraTowardsTarget, &g_setFeetPositionTarget, &g_sendMouseVirtualPositionTarget}) {
            if (*target != nullptr) {
                MH_DisableHook(*target);
                MH_RemoveHook(*target);
                *target = nullptr;
            }
        }
        g_originalRotateCameraTowards = nullptr;
        g_originalSetFeetPosition = nullptr;
        g_originalSendMouseVirtualPosition = nullptr;
    };

    if (pointerRequested) {
        g_getCurrentImGui = reinterpret_cast<GetImGuiFn>(base + kGetCurrentImGuiRva);
        g_getGameHudImGui = reinterpret_cast<GetImGuiFn>(base + kGetGameHudImGuiRva);
        g_imGuiGetSet = reinterpret_cast<ImGuiGetSetFn>(base + kImGuiGetSetRva);
        g_projectRayToVirtual = reinterpret_cast<ProjectRayToVirtualFn>(base + kProjectRayToVirtualRva);

        int32_t gameContextDisplacement = 0;
        std::memcpy(&gameContextDisplacement, base + kGetCurrentImGuiRva + 3, sizeof(gameContextDisplacement));
        g_gameContextSlot = reinterpret_cast<void**>(
            base + kGetCurrentImGuiRva + 7 + gameContextDisplacement);
        void* gameContextProbe = nullptr;
        if (!ReadMemory(g_gameContextSlot, &gameContextProbe, sizeof(gameContextProbe))) {
            Logger::Instance().Write(
                LogLevel::Error,
                "hpl_terminal_bridge install_failed reason=invalid_game_context_slot slot=%p",
                g_gameContextSlot);
            g_gameContextSlot = nullptr;
            return false;
        }

        if (!installHook(
                base + kImGuiSendMouseVirtualPositionRva,
                reinterpret_cast<void*>(&HookSendMouseVirtualPosition),
                reinterpret_cast<void**>(&g_originalSendMouseVirtualPosition),
                g_sendMouseVirtualPositionTarget,
                "send_mouse_virtual_position")) {
            rollback();
            return false;
        }
    }

    if (diegeticRequested) {
        if (!installHook(
                base + kSetFeetPositionRva,
                reinterpret_cast<void*>(&HookSetFeetPosition),
                reinterpret_cast<void**>(&g_originalSetFeetPosition),
                g_setFeetPositionTarget,
                "set_feet_position")
            || !installHook(
                base + kRotateCameraTowardsRva,
                reinterpret_cast<void*>(&HookRotateCameraTowards),
                reinterpret_cast<void**>(&g_originalRotateCameraTowards),
                g_rotateCameraTowardsTarget,
                "rotate_camera_towards")) {
            rollback();
            return false;
        }
    }

    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_terminal_bridge install_ok pointer=%d diegetic=%d overlay=%d rayPointer=%d effectivePointer=%s rayLengthMeters=%.2f sendMouseVirtualRva=0x%llx projectRayRva=0x%llx setFeetRva=0x%llx rotateCameraRva=0x%llx fallbackFovDegrees=%.2f,%.2f fallbackSmoothing=%.3f policy=state8_manager_world_input_0x170_focused_wrapper_set_0x18",
        pointerRequested ? 1 : 0,
        diegeticRequested ? 1 : 0,
        config.hplControllerTerminalOverlay ? 1 : 0,
        config.hplControllerTerminalRayPointer ? 1 : 0,
        config.hplControllerTerminalOverlay ? "head_locked_overlay" : "world_mesh",
        config.hplControllerTerminalRayLengthMeters,
        static_cast<unsigned long long>(kImGuiSendMouseVirtualPositionRva),
        static_cast<unsigned long long>(kProjectRayToVirtualRva),
        static_cast<unsigned long long>(kSetFeetPositionRva),
        static_cast<unsigned long long>(kRotateCameraTowardsRva),
        config.hplControllerTerminalPointerHorizontalDegrees,
        config.hplControllerTerminalPointerVerticalDegrees,
        config.hplControllerTerminalPointerSmoothing);
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_terminal_surface_config hudShape=%s hudDistanceMeters=%.3f hudWidthMeters=%.3f hudCylinderAngleDegrees=%.2f pointerScale=%.2f mapping=exact_quad_or_cylinder_surface_intersection",
        config.openxrHudShape.c_str(),
        config.openxrHudDistanceMeters,
        config.openxrHudWidthMeters,
        config.openxrHudCylinderAngleDegrees,
        config.hplControllerTerminalPointerScale);
    return true;
}

bool UpdateHPLTerminalPointer(
    const OpenXRHeadPose& headPose,
    const OpenXRControllerPose& aimPose,
    uint64_t gameFrame)
{
    std::lock_guard lock(g_updateMutex);
    g_updates.fetch_add(1, std::memory_order_relaxed);
    if (!g_config.hplControllerTerminalPointer
        || !headPose.valid || !headPose.orientationTracked
        || !aimPose.valid || !aimPose.orientationTracked) {
        DeactivateHPLTerminalPointer();
        return false;
    }

    Vector2f spatialPosition;
    TerminalInputOwner spatialOwner{};
    const SpatialProjectionResult spatial = ProjectControllerRayToTerminal(
        aimPose, gameFrame, spatialPosition, spatialOwner);
    if (spatial == SpatialProjectionResult::Hit) {
        const bool newSession = !g_active.load(std::memory_order_relaxed)
            || !g_directVirtualCoordinates.load(std::memory_order_relaxed);
        g_virtualX.store(spatialPosition.x, std::memory_order_relaxed);
        g_virtualY.store(spatialPosition.y, std::memory_order_relaxed);
        g_directVirtualCoordinates.store(true, std::memory_order_release);
        g_active.store(true, std::memory_order_release);
        g_smoothed = false;
        if (newSession) g_generation.fetch_add(1, std::memory_order_release);
        g_projected.fetch_add(1, std::memory_order_relaxed);
        if (DispatchControllerVirtualPosition(true, &spatialOwner)) {
            return true;
        }
        const uint64_t failure = g_directDispatchFailures.fetch_add(
            1, std::memory_order_relaxed) + 1;
        if (ShouldLog(failure)) {
            Logger::Instance().Write(
                LogLevel::Warn,
                "hpl_terminal_pointer dispatch_failed=%llu route=spatial_mesh_ray imGui=%p set=%p entity=%p virtual=%.2f,%.2f policy=manager_world_input_owner",
                static_cast<unsigned long long>(failure),
                spatialOwner.imGui,
                spatialOwner.guiSet,
                spatialOwner.guiEntity,
                spatialPosition.x,
                spatialPosition.y);
        }
        DeactivateHPLTerminalPointer();
        return false;
    }
    if (spatial == SpatialProjectionResult::Miss) {
        DeactivateHPLTerminalPointer();
        return false;
    }

    g_headConeFallbacks.fetch_add(1, std::memory_order_relaxed);
    terminal_math::HudPointerPosition pointer;
    if (!terminal_math::ProjectAimToHudSurface(
            {headPose.orientationX, headPose.orientationY, headPose.orientationZ, headPose.orientationW},
            {aimPose.orientationX, aimPose.orientationY, aimPose.orientationZ, aimPose.orientationW},
            g_config.openxrHudShape == "cylinder",
            g_config.openxrHudCylinderAngleDegrees,
            g_config.openxrHudDistanceMeters,
            g_config.openxrHudWidthMeters,
            static_cast<float>(g_config.openxrHudWidthPixels)
                / static_cast<float>(std::max(g_config.openxrHudHeightPixels, 1)),
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
    g_directVirtualCoordinates.store(false, std::memory_order_release);
    g_active.store(true, std::memory_order_release);
    g_projected.fetch_add(1, std::memory_order_relaxed);
    if (g_openxr != nullptr && g_config.hplControllerTerminalOverlay) {
        OpenXRTerminalPointerState state;
        state.valid = true;
        state.gameFrame = gameFrame;
        state.normalizedX = std::clamp(g_smoothedX, 0.0f, 1.0f);
        state.normalizedY = std::clamp(g_smoothedY, 0.0f, 1.0f);
        state.sizeScale = g_config.hplControllerTerminalPointerScale;
        g_openxr->SetTerminalPointer(state);
    }
    if (DispatchControllerVirtualPosition(false)) return true;
    const uint64_t failure = g_directDispatchFailures.fetch_add(
        1, std::memory_order_relaxed) + 1;
    if (ShouldLog(failure)) {
        Logger::Instance().Write(
            LogLevel::Warn,
            "hpl_terminal_pointer dispatch_failed=%llu route=head_cone policy=manager_world_input_owner",
            static_cast<unsigned long long>(failure));
    }
    DeactivateHPLTerminalPointer();
    return false;
}

void DeactivateHPLTerminalPointer()
{
    const bool wasActive = g_active.exchange(false, std::memory_order_acq_rel);
    if (wasActive) g_generation.fetch_add(1, std::memory_order_release);
    g_directVirtualCoordinates.store(false, std::memory_order_release);
    g_smoothed = false;
    if (g_openxr != nullptr) g_openxr->ClearTerminalPointer();
}

void LogHPLTerminalBridgeSummary()
{
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_terminal_bridge_summary enabled=%d diegetic=%d overlay=%d rayPointer=%d active=%d updates=%llu projected=%llu hookCalls=%llu directDispatches=%llu directDispatchFailures=%llu applied=%llu spatial={attempts=%llu hits=%llu misses=%llu unavailable=%llu} headConeFallbacks=%llu takeover={feetCalls=%llu feetSuppressed=%llu rotateCalls=%llu rotateSuppressed=%llu} fallbacks={inactive=%llu owner=%llu layout=%llu}",
        g_config.hplControllerTerminalPointer ? 1 : 0,
        g_config.hplControllerTerminalDiegetic ? 1 : 0,
        g_config.hplControllerTerminalOverlay ? 1 : 0,
        g_config.hplControllerTerminalRayPointer ? 1 : 0,
        g_active.load(std::memory_order_relaxed) ? 1 : 0,
        static_cast<unsigned long long>(g_updates.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_projected.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_hookCalls.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_directDispatches.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_directDispatchFailures.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_applied.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_spatialAttempts.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_spatialHits.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_spatialMisses.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_spatialUnavailable.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_headConeFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_feetCalls.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_feetSuppressed.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_rotateCalls.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_rotateSuppressed.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_inactiveFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_ownerFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_layoutFallbacks.load(std::memory_order_relaxed)));
}

void RemoveHPLTerminalBridge()
{
    std::lock_guard lock(g_installMutex);
    DeactivateHPLTerminalPointer();
    g_openxr = nullptr;
    if (g_sendMouseVirtualPositionTarget != nullptr) {
        MH_DisableHook(g_sendMouseVirtualPositionTarget);
        MH_RemoveHook(g_sendMouseVirtualPositionTarget);
    }
    if (g_setFeetPositionTarget != nullptr) {
        MH_DisableHook(g_setFeetPositionTarget);
        MH_RemoveHook(g_setFeetPositionTarget);
    }
    if (g_rotateCameraTowardsTarget != nullptr) {
        MH_DisableHook(g_rotateCameraTowardsTarget);
        MH_RemoveHook(g_rotateCameraTowardsTarget);
    }
    g_sendMouseVirtualPositionTarget = nullptr;
    g_setFeetPositionTarget = nullptr;
    g_rotateCameraTowardsTarget = nullptr;
    g_originalSendMouseVirtualPosition = nullptr;
    g_originalSetFeetPosition = nullptr;
    g_originalRotateCameraTowards = nullptr;
    g_projectRayToVirtual = nullptr;
    g_gameContextSlot = nullptr;
    g_imGuiGetSet = nullptr;
    g_getGameHudImGui = nullptr;
    g_getCurrentImGui = nullptr;
    Logger::Instance().Write(LogLevel::Info, "hpl_terminal_bridge removed");
}

} // namespace somavr
