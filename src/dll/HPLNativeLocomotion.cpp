#include "HPLNativeLocomotion.h"

#include "Logger.h"

#include <Windows.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>

namespace somavr
{
namespace
{

constexpr uintptr_t kCharacterBodyMoveRva = 0x2375f0;
constexpr uintptr_t kCharacterBodyAddYawRva = 0x237460;
constexpr uintptr_t kGetGamePausedRva = 0x0ccc90;
constexpr int kNormalPlayerState = 0;
constexpr int kNormalMoveState = 0;
constexpr int kForwardDirection = 0;
constexpr int kRightDirection = 1;

using CharacterBodyMoveFn = void (*)(void*, int, float);
using CharacterBodyAddYawFn = void (*)(void*, float);
using GetGamePausedFn = bool (*)();

Config g_config;
CharacterBodyMoveFn g_move = nullptr;
CharacterBodyAddYawFn g_addYaw = nullptr;
GetGamePausedFn g_getGamePaused = nullptr;
std::mutex g_mutex;
std::atomic<uint64_t> g_moveFrames = 0;
std::atomic<uint64_t> g_moveCalls = 0;
std::atomic<uint64_t> g_turnCalls = 0;
std::atomic<uint64_t> g_stateFallbacks = 0;
std::atomic<uint64_t> g_invalidBodyFallbacks = 0;
std::atomic<uint64_t> g_pausedFallbacks = 0;

bool IsInsideImage(HMODULE module, uintptr_t rva, size_t bytes)
{
    if (module == nullptr)
        return false;
    const auto* base = reinterpret_cast<const std::byte*>(module);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE)
        return false;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE)
        return false;
    return rva <= nt->OptionalHeader.SizeOfImage && bytes <= nt->OptionalHeader.SizeOfImage - rva;
}

bool IsReadable(const void* address, size_t bytes)
{
    if (address == nullptr)
        return false;
    MEMORY_BASIC_INFORMATION info{};
    if (VirtualQuery(address, &info, sizeof(info)) != sizeof(info))
        return false;
    if (info.State != MEM_COMMIT || (info.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0)
        return false;
    const uintptr_t start = reinterpret_cast<uintptr_t>(address);
    const uintptr_t end = reinterpret_cast<uintptr_t>(info.BaseAddress) + info.RegionSize;
    return start <= end && bytes <= end - start;
}

bool NormalStateOwnsBody(const HPLPlayerStateSnapshot& player)
{
    if (!player.playerValid || player.authoredCameraActive || player.playerStateId != kNormalPlayerState ||
        player.moveStateId != kNormalMoveState)
    {
        g_stateFallbacks.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    if (!IsReadable(player.characterBody, 0xd8))
    {
        g_invalidBodyFallbacks.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    if (g_getGamePaused == nullptr || g_getGamePaused())
    {
        g_pausedFallbacks.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    return true;
}

} // namespace

bool InstallHPLNativeLocomotion(const Config& config)
{
    std::lock_guard lock(g_mutex);
    g_config = config;

    HMODULE executable = GetModuleHandleW(nullptr);
    static constexpr uint8_t kMoveSignature[] = {
        0x48, 0x63, 0xc2, 0xf3, 0x0f, 0x58, 0x94, 0x81, 0x94, 0x00, 0x00, 0x00,
        0xf3, 0x0f, 0x11, 0x94, 0x81, 0x94, 0x00, 0x00, 0x00,
    };
    static constexpr uint8_t kAddYawSignature[] = {
        0xf3, 0x0f, 0x58, 0x89, 0xd4, 0x00, 0x00, 0x00,
        0xf3, 0x0f, 0x11, 0x89, 0xd4, 0x00, 0x00, 0x00, 0xc3,
    };
    static constexpr uint8_t kGetGamePausedSignature[] = {
        0x48, 0x8b, 0x05, 0x49, 0x59, 0x6c, 0x00,
        0x48, 0x8b, 0x88, 0xc8, 0x00, 0x00, 0x00,
        0x0f, 0xb6, 0x81, 0xd4, 0x02, 0x00, 0x00, 0xc3,
    };
    if (!IsInsideImage(executable, kCharacterBodyMoveRva, sizeof(kMoveSignature)) ||
        !IsInsideImage(executable, kCharacterBodyAddYawRva, sizeof(kAddYawSignature)) ||
        !IsInsideImage(executable, kGetGamePausedRva, sizeof(kGetGamePausedSignature)))
    {
        Logger::Instance().Write(LogLevel::Error, "hpl_native_locomotion install_failed reason=invalid_image_range");
        return false;
    }

    const auto* base = reinterpret_cast<const uint8_t*>(executable);
    const bool moveSignatureValid =
        std::memcmp(base + kCharacterBodyMoveRva, kMoveSignature, sizeof(kMoveSignature)) == 0;
    const bool turnSignatureValid =
        std::memcmp(base + kCharacterBodyAddYawRva, kAddYawSignature, sizeof(kAddYawSignature)) == 0;
    const bool pauseSignatureValid =
        std::memcmp(base + kGetGamePausedRva, kGetGamePausedSignature, sizeof(kGetGamePausedSignature)) == 0;
    if (moveSignatureValid && pauseSignatureValid)
        g_move = reinterpret_cast<CharacterBodyMoveFn>(const_cast<uint8_t*>(base + kCharacterBodyMoveRva));
    if (turnSignatureValid && pauseSignatureValid)
        g_addYaw = reinterpret_cast<CharacterBodyAddYawFn>(const_cast<uint8_t*>(base + kCharacterBodyAddYawRva));
    if (pauseSignatureValid)
        g_getGamePaused = reinterpret_cast<GetGamePausedFn>(const_cast<uint8_t*>(base + kGetGamePausedRva));

    Logger::Instance().Write(
        moveSignatureValid && turnSignatureValid && pauseSignatureValid ? LogLevel::Info : LogLevel::Warn,
        "hpl_native_locomotion install_complete movementEnabled=%d turnEnabled=%d moveRva=0x%llx moveSignature=%d addYawRva=0x%llx addYawSignature=%d getGamePausedRva=0x%llx pauseSignature=%d policy=unpaused_normal_state_only_with_semantic_fallback",
        config.hplControllerNativeLocomotion ? 1 : 0,
        config.hplControllerNativeTurn ? 1 : 0,
        static_cast<unsigned long long>(kCharacterBodyMoveRva),
        moveSignatureValid ? 1 : 0,
        static_cast<unsigned long long>(kCharacterBodyAddYawRva),
        turnSignatureValid ? 1 : 0,
        static_cast<unsigned long long>(kGetGamePausedRva),
        pauseSignatureValid ? 1 : 0);
    return (!config.hplControllerNativeLocomotion || (moveSignatureValid && pauseSignatureValid)) &&
        (!config.hplControllerNativeTurn || (turnSignatureValid && pauseSignatureValid));
}

bool CanApplyHPLNativeMovement(const HPLPlayerStateSnapshot& player)
{
    return g_config.hplControllerNativeLocomotion && g_move != nullptr && NormalStateOwnsBody(player);
}

bool CanApplyHPLNativeTurn(const HPLPlayerStateSnapshot& player)
{
    return g_config.hplControllerNativeTurn && g_addYaw != nullptr && NormalStateOwnsBody(player);
}

bool ApplyHPLNativeMovement(const HPLPlayerStateSnapshot& player, float right, float forward)
{
    if (!CanApplyHPLNativeMovement(player) || !std::isfinite(right) || !std::isfinite(forward))
        return false;
    right = std::clamp(right, -1.0f, 1.0f);
    forward = std::clamp(forward, -1.0f, 1.0f);
    if (std::fabs(forward) > 0.001f)
    {
        g_move(player.characterBody, kForwardDirection, forward);
        g_moveCalls.fetch_add(1, std::memory_order_relaxed);
    }
    if (std::fabs(right) > 0.001f)
    {
        g_move(player.characterBody, kRightDirection, right);
        g_moveCalls.fetch_add(1, std::memory_order_relaxed);
    }
    g_moveFrames.fetch_add(1, std::memory_order_relaxed);
    return true;
}

bool ApplyHPLNativeTurn(const HPLPlayerStateSnapshot& player, float radians)
{
    if (!CanApplyHPLNativeTurn(player) || !std::isfinite(radians) || std::fabs(radians) > 3.141593f)
        return false;
    if (std::fabs(radians) > 0.000001f)
    {
        g_addYaw(player.characterBody, radians);
        g_turnCalls.fetch_add(1, std::memory_order_relaxed);
    }
    return true;
}

void LogHPLNativeLocomotionSummary()
{
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_native_locomotion_summary movementReady=%d turnReady=%d pauseGateReady=%d moveFrames=%llu moveCalls=%llu turnCalls=%llu stateFallbacks=%llu invalidBodyFallbacks=%llu pausedFallbacks=%llu",
        g_move != nullptr ? 1 : 0,
        g_addYaw != nullptr ? 1 : 0,
        g_getGamePaused != nullptr ? 1 : 0,
        static_cast<unsigned long long>(g_moveFrames.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_moveCalls.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_turnCalls.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_stateFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_invalidBodyFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_pausedFallbacks.load(std::memory_order_relaxed)));
}

void RemoveHPLNativeLocomotion()
{
    std::lock_guard lock(g_mutex);
    g_move = nullptr;
    g_addYaw = nullptr;
    g_getGamePaused = nullptr;
    g_config = {};
    Logger::Instance().Write(LogLevel::Info, "hpl_native_locomotion removed");
}

} // namespace somavr
