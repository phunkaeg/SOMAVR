#include "HPLNativeLocomotion.h"

#include "HPLRoomscaleReconciliationMath.h"
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
constexpr uintptr_t kCharacterBodySetFeetPositionRva = 0x237920;
constexpr uintptr_t kCharacterBodyGetFeetPositionRva = 0x237970;
constexpr uintptr_t kGetGamePausedRva = 0x0ccc90;
constexpr size_t kGameSubsystemOffset = 0xc8;
constexpr size_t kGamePausedOffset = 0x2d4;
constexpr size_t kCharacterBodySizeOffset = 0x134;
constexpr int kNormalPlayerState = 0;
constexpr int kNormalMoveState = 0;
constexpr int kForwardDirection = 0;
constexpr int kRightDirection = 1;

using CharacterBodyMoveFn = void (*)(void*, int, float);
using CharacterBodyAddYawFn = void (*)(void*, float);
struct Vector3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};
using CharacterBodySetFeetPositionFn = void (*)(void*, const Vector3*, bool);
using CharacterBodyGetFeetPositionFn = Vector3* (*)(void*, Vector3*);
using GetGamePausedFn = bool (*)();

Config g_config;
CharacterBodyMoveFn g_move = nullptr;
CharacterBodyAddYawFn g_addYaw = nullptr;
CharacterBodySetFeetPositionFn g_setFeetPosition = nullptr;
CharacterBodyGetFeetPositionFn g_getFeetPosition = nullptr;
GetGamePausedFn g_getGamePaused = nullptr;
void** g_gameContextSlot = nullptr;
std::mutex g_mutex;
std::atomic<uint64_t> g_moveFrames = 0;
std::atomic<uint64_t> g_moveCalls = 0;
std::atomic<uint64_t> g_interactionMoveFrames = 0;
std::atomic<uint64_t> g_interactionMoveCalls = 0;
std::atomic<uint64_t> g_turnCalls = 0;
std::atomic<uint64_t> g_stateFallbacks = 0;
std::atomic<uint64_t> g_invalidBodyFallbacks = 0;
std::atomic<uint64_t> g_pausedFallbacks = 0;
std::atomic<uint64_t> g_pauseStateReads = 0;
std::atomic<uint64_t> g_pauseStateUnavailable = 0;
std::atomic<uint64_t> g_bodyReconciliationCandidates = 0;
std::atomic<uint64_t> g_bodyReconciliationActivations = 0;
std::atomic<uint64_t> g_bodyReconciliationSteps = 0;
std::atomic<uint64_t> g_bodyReconciliationBlocked = 0;
std::atomic<uint64_t> g_bodyReconciliationCommitFailures = 0;
uint64_t g_lastBodyReconciliationPoseFrame = 0;
uint32_t g_bodyReconciliationHoldFrames = 0;
bool g_bodyReconciliationActive = false;

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

bool ReadMemory(const void* source, void* destination, size_t bytes)
{
    if (!IsReadable(source, bytes) || destination == nullptr || bytes == 0)
        return false;
    SIZE_T bytesRead = 0;
    return ReadProcessMemory(
        GetCurrentProcess(), source, destination, bytes, &bytesRead) != FALSE
        && bytesRead == bytes;
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
    bool paused = false;
    if (!GetHPLGamePausedState(paused) || paused)
    {
        g_pausedFallbacks.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    return true;
}

bool InteractionStateOwnsBody(const HPLPlayerStateSnapshot& player)
{
    const bool physicalInteraction =
        (player.playerStateId >= 3 && player.playerStateId <= 7)
        || player.playerStateId == 13;
    if (!player.playerValid || player.authoredCameraActive
        || !physicalInteraction || player.moveStateId != kNormalMoveState) {
        g_stateFallbacks.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    if (!IsReadable(player.characterBody, 0xd8)) {
        g_invalidBodyFallbacks.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    bool paused = false;
    if (!GetHPLGamePausedState(paused) || paused) {
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
    static constexpr uint8_t kSetFeetPositionSignature[] = {
        0x48, 0x83, 0xec, 0x38, 0xf3, 0x0f, 0x10, 0x02,
        0xf3, 0x0f, 0x10, 0x89, 0x38, 0x01, 0x00, 0x00,
    };
    static constexpr uint8_t kGetFeetPositionSignature[] = {
        0x8b, 0x41, 0x6c, 0xf3, 0x0f, 0x10, 0x89, 0x38,
        0x01, 0x00, 0x00, 0xf3, 0x0f, 0x10, 0x41, 0x70,
    };
    if (!IsInsideImage(executable, kCharacterBodyMoveRva, sizeof(kMoveSignature)) ||
        !IsInsideImage(executable, kCharacterBodyAddYawRva, sizeof(kAddYawSignature)) ||
        !IsInsideImage(executable, kCharacterBodySetFeetPositionRva, sizeof(kSetFeetPositionSignature)) ||
        !IsInsideImage(executable, kCharacterBodyGetFeetPositionRva, sizeof(kGetFeetPositionSignature)) ||
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
    const bool setFeetSignatureValid = std::memcmp(
        base + kCharacterBodySetFeetPositionRva,
        kSetFeetPositionSignature,
        sizeof(kSetFeetPositionSignature)) == 0;
    const bool getFeetSignatureValid = std::memcmp(
        base + kCharacterBodyGetFeetPositionRva,
        kGetFeetPositionSignature,
        sizeof(kGetFeetPositionSignature)) == 0;
    if (moveSignatureValid && pauseSignatureValid)
        g_move = reinterpret_cast<CharacterBodyMoveFn>(const_cast<uint8_t*>(base + kCharacterBodyMoveRva));
    if (turnSignatureValid && pauseSignatureValid)
        g_addYaw = reinterpret_cast<CharacterBodyAddYawFn>(const_cast<uint8_t*>(base + kCharacterBodyAddYawRva));
    if (pauseSignatureValid) {
        g_getGamePaused = reinterpret_cast<GetGamePausedFn>(const_cast<uint8_t*>(base + kGetGamePausedRva));
        int32_t gameContextDisplacement = 0;
        std::memcpy(
            &gameContextDisplacement,
            base + kGetGamePausedRva + 3,
            sizeof(gameContextDisplacement));
        g_gameContextSlot = reinterpret_cast<void**>(
            const_cast<uint8_t*>(base + kGetGamePausedRva + 7)
            + gameContextDisplacement);
        if (!IsReadable(g_gameContextSlot, sizeof(*g_gameContextSlot)))
            g_gameContextSlot = nullptr;
    }
    if (setFeetSignatureValid && getFeetSignatureValid && pauseSignatureValid) {
        g_setFeetPosition = reinterpret_cast<CharacterBodySetFeetPositionFn>(
            const_cast<uint8_t*>(base + kCharacterBodySetFeetPositionRva));
        g_getFeetPosition = reinterpret_cast<CharacterBodyGetFeetPositionFn>(
            const_cast<uint8_t*>(base + kCharacterBodyGetFeetPositionRva));
    }

    Logger::Instance().Write(
        moveSignatureValid && turnSignatureValid && pauseSignatureValid
                && setFeetSignatureValid && getFeetSignatureValid
            ? LogLevel::Info : LogLevel::Warn,
        "hpl_native_locomotion install_complete movementEnabled=%d interactionMovementEnabled=%d turnEnabled=%d bodyReconciliation=%d moveRva=0x%llx moveSignature=%d addYawRva=0x%llx addYawSignature=%d setFeetRva=0x%llx setFeetSignature=%d getFeetRva=0x%llx getFeetSignature=%d bodySizeOffset=0x%zx getGamePausedRva=0x%llx pauseSignature=%d pauseDataReady=%d gameContextSlot=%p policy=guarded_pause_data_read_then_state_owned_character_body_move",
        config.hplControllerNativeLocomotion ? 1 : 0,
        config.hplControllerLocomotionDuringInteractions ? 1 : 0,
        config.hplControllerNativeTurn ? 1 : 0,
        config.hplRoomscaleBodyReconciliation ? 1 : 0,
        static_cast<unsigned long long>(kCharacterBodyMoveRva),
        moveSignatureValid ? 1 : 0,
        static_cast<unsigned long long>(kCharacterBodyAddYawRva),
        turnSignatureValid ? 1 : 0,
        static_cast<unsigned long long>(kCharacterBodySetFeetPositionRva),
        setFeetSignatureValid ? 1 : 0,
        static_cast<unsigned long long>(kCharacterBodyGetFeetPositionRva),
        getFeetSignatureValid ? 1 : 0,
        kCharacterBodySizeOffset,
        static_cast<unsigned long long>(kGetGamePausedRva),
        pauseSignatureValid ? 1 : 0,
        g_gameContextSlot != nullptr ? 1 : 0,
        g_gameContextSlot);
    return (!(config.hplControllerNativeLocomotion
            || config.hplControllerLocomotionDuringInteractions)
            || (moveSignatureValid && pauseSignatureValid)) &&
        (!config.hplControllerNativeTurn || (turnSignatureValid && pauseSignatureValid)) &&
        (!config.hplRoomscaleBodyReconciliation
            || (setFeetSignatureValid && getFeetSignatureValid && pauseSignatureValid));
}

bool CanApplyHPLNativeMovement(const HPLPlayerStateSnapshot& player)
{
    return g_config.hplControllerNativeLocomotion && g_move != nullptr && NormalStateOwnsBody(player);
}

bool CanApplyHPLInteractionMovement(const HPLPlayerStateSnapshot& player)
{
    return g_config.hplControllerLocomotionDuringInteractions
        && g_move != nullptr && InteractionStateOwnsBody(player);
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

bool ApplyHPLInteractionMovement(
    const HPLPlayerStateSnapshot& player,
    float right,
    float forward)
{
    if (!CanApplyHPLInteractionMovement(player)
        || !std::isfinite(right) || !std::isfinite(forward)) {
        return false;
    }
    right = std::clamp(right, -1.0f, 1.0f);
    forward = std::clamp(forward, -1.0f, 1.0f);
    if (std::fabs(forward) > 0.001f) {
        g_move(player.characterBody, kForwardDirection, forward);
        g_interactionMoveCalls.fetch_add(1, std::memory_order_relaxed);
    }
    if (std::fabs(right) > 0.001f) {
        g_move(player.characterBody, kRightDirection, right);
        g_interactionMoveCalls.fetch_add(1, std::memory_order_relaxed);
    }
    g_interactionMoveFrames.fetch_add(1, std::memory_order_relaxed);
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

bool ApplyHPLRoomscaleBodyReconciliation(
    const HPLPlayerStateSnapshot& player,
    const HPLCameraBridgeStatus& camera)
{
    if (!g_config.hplRoomscaleBodyReconciliation
        || g_setFeetPosition == nullptr || g_getFeetPosition == nullptr) {
        return false;
    }
    if (camera.headPoseFrame == 0 || camera.headPoseFrame == g_lastBodyReconciliationPoseFrame) {
        return false;
    }
    g_lastBodyReconciliationPoseFrame = camera.headPoseFrame;

    if (!camera.trackingEnabled || !camera.roomscaleEnabled
        || !camera.headWorldPositionValid || !camera.roomscaleSafetyQueried
        || camera.roomscaleSafetyClamped || !NormalStateOwnsBody(player)) {
        g_bodyReconciliationHoldFrames = 0;
        g_bodyReconciliationActive = false;
        return false;
    }

    const float worldScale = std::max(camera.worldUnitsPerMeter, 0.001f);
    const float threshold = g_config.hplRoomscaleBodyReconciliationThresholdMeters * worldScale;
    const float target = g_config.hplRoomscaleBodyReconciliationTargetMeters * worldScale;
    const float maximumStep = g_config.hplRoomscaleBodyReconciliationMaxStepMeters * worldScale;
    const float distance = std::sqrt(
        camera.headWorldOffsetX * camera.headWorldOffsetX
        + camera.headWorldOffsetZ * camera.headWorldOffsetZ);
    if (!std::isfinite(distance)) {
        g_bodyReconciliationHoldFrames = 0;
        g_bodyReconciliationActive = false;
        return false;
    }

    if (!g_bodyReconciliationActive) {
        if (distance <= threshold) {
            g_bodyReconciliationHoldFrames = 0;
            return false;
        }
        g_bodyReconciliationCandidates.fetch_add(1, std::memory_order_relaxed);
        ++g_bodyReconciliationHoldFrames;
        if (g_bodyReconciliationHoldFrames
            < static_cast<uint32_t>(g_config.hplRoomscaleBodyReconciliationHoldFrames)) {
            return false;
        }
        g_bodyReconciliationActive = true;
        g_bodyReconciliationActivations.fetch_add(1, std::memory_order_relaxed);
    }

    float shiftX = 0.0f;
    float shiftZ = 0.0f;
    if (!roomscale_reconciliation_math::ComputeBodyCatchupStep(
            camera.headWorldOffsetX,
            camera.headWorldOffsetZ,
            threshold,
            target,
            maximumStep,
            true,
            shiftX,
            shiftZ)) {
        g_bodyReconciliationHoldFrames = 0;
        g_bodyReconciliationActive = false;
        return false;
    }

    if (!IsReadable(player.characterBody, kCharacterBodySizeOffset + sizeof(Vector3))) {
        g_invalidBodyFallbacks.fetch_add(1, std::memory_order_relaxed);
        g_bodyReconciliationActive = false;
        return false;
    }
    Vector3 feet;
    Vector3 size;
    g_getFeetPosition(player.characterBody, &feet);
    std::memcpy(
        &size,
        static_cast<const std::byte*>(player.characterBody) + kCharacterBodySizeOffset,
        sizeof(size));
    uint32_t probeCount = 0;
    if (!ValidateHPLRoomscaleBodyShift(
            feet.x, feet.y, feet.z,
            size.x, size.y, size.z,
            shiftX, shiftZ,
            probeCount)) {
        g_bodyReconciliationBlocked.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    if (!CommitHPLRoomscaleBodyShift(shiftX, shiftZ)) {
        g_bodyReconciliationCommitFailures.fetch_add(1, std::memory_order_relaxed);
        g_bodyReconciliationActive = false;
        return false;
    }

    const Vector3 targetFeet = {feet.x + shiftX, feet.y, feet.z + shiftZ};
    g_setFeetPosition(player.characterBody, &targetFeet, false);
    const uint64_t step = g_bodyReconciliationSteps.fetch_add(1, std::memory_order_relaxed) + 1;
    if (step <= 8 || step % 120 == 0) {
        Logger::Instance().Write(
            LogLevel::Info,
            "hpl_roomscale_body_reconciliation step=%llu poseFrame=%llu offset=%.5f,%.5f distance=%.5f shift=%.5f,%.5f feet=%.5f,%.5f,%.5f targetFeet=%.5f,%.5f,%.5f size=%.5f,%.5f,%.5f probes=%u smooth=0",
            static_cast<unsigned long long>(step),
            static_cast<unsigned long long>(camera.headPoseFrame),
            camera.headWorldOffsetX, camera.headWorldOffsetZ, distance,
            shiftX, shiftZ,
            feet.x, feet.y, feet.z,
            targetFeet.x, targetFeet.y, targetFeet.z,
            size.x, size.y, size.z,
            probeCount);
    }
    return true;
}

bool GetHPLGamePausedState(bool& paused)
{
    g_pauseStateReads.fetch_add(1, std::memory_order_relaxed);
    paused = false;
    if (g_getGamePaused == nullptr || g_gameContextSlot == nullptr) {
        g_pauseStateUnavailable.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    void* gameContext = nullptr;
    void* gameSubsystem = nullptr;
    uint8_t pausedByte = 0;
    if (!ReadMemory(g_gameContextSlot, &gameContext, sizeof(gameContext))
        || gameContext == nullptr
        || !ReadMemory(
            static_cast<const std::byte*>(gameContext) + kGameSubsystemOffset,
            &gameSubsystem,
            sizeof(gameSubsystem))
        || gameSubsystem == nullptr
        || !ReadMemory(
            static_cast<const std::byte*>(gameSubsystem) + kGamePausedOffset,
            &pausedByte,
            sizeof(pausedByte))) {
        g_pauseStateUnavailable.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    paused = pausedByte != 0;
    return true;
}

void LogHPLNativeLocomotionSummary()
{
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_native_locomotion_summary movementReady=%d interactionMovementConfigured=%d turnReady=%d bodyReconciliationReady=%d pauseGateReady=%d pauseStateReads=%llu pauseStateUnavailable=%llu moveFrames=%llu moveCalls=%llu interactionMoveFrames=%llu interactionMoveCalls=%llu turnCalls=%llu stateFallbacks=%llu invalidBodyFallbacks=%llu pausedFallbacks=%llu bodyReconciliationCandidates=%llu bodyReconciliationActivations=%llu bodyReconciliationSteps=%llu bodyReconciliationBlocked=%llu bodyReconciliationCommitFailures=%llu bodyReconciliationActive=%d bodyReconciliationHoldFrames=%u",
        g_move != nullptr ? 1 : 0,
        g_config.hplControllerLocomotionDuringInteractions ? 1 : 0,
        g_addYaw != nullptr ? 1 : 0,
        g_setFeetPosition != nullptr && g_getFeetPosition != nullptr ? 1 : 0,
        g_getGamePaused != nullptr && g_gameContextSlot != nullptr ? 1 : 0,
        static_cast<unsigned long long>(g_pauseStateReads.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_pauseStateUnavailable.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_moveFrames.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_moveCalls.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_interactionMoveFrames.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_interactionMoveCalls.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_turnCalls.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_stateFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_invalidBodyFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_pausedFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_bodyReconciliationCandidates.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_bodyReconciliationActivations.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_bodyReconciliationSteps.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_bodyReconciliationBlocked.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_bodyReconciliationCommitFailures.load(std::memory_order_relaxed)),
        g_bodyReconciliationActive ? 1 : 0,
        g_bodyReconciliationHoldFrames);
}

void RemoveHPLNativeLocomotion()
{
    std::lock_guard lock(g_mutex);
    g_move = nullptr;
    g_addYaw = nullptr;
    g_setFeetPosition = nullptr;
    g_getFeetPosition = nullptr;
    g_getGamePaused = nullptr;
    g_gameContextSlot = nullptr;
    g_lastBodyReconciliationPoseFrame = 0;
    g_bodyReconciliationHoldFrames = 0;
    g_bodyReconciliationActive = false;
    g_config = {};
    Logger::Instance().Write(LogLevel::Info, "hpl_native_locomotion removed");
}

} // namespace somavr
