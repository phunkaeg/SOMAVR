#include "HPLPlayerState.h"

#include "HPLCameraBridge.h"
#include "Logger.h"

#include <Windows.h>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>

namespace somavr
{
namespace
{

constexpr uintptr_t kGetPlayerRva = 0x0cc860;
constexpr uintptr_t kGetCurrentStateIdRva = 0x155050;
constexpr uintptr_t kGetCurrentMoveStateIdRva = 0x155090;
constexpr size_t kPlayerCameraOffset = 0x168;
constexpr size_t kPlayerCharacterBodyOffset = 0x170;
constexpr size_t kCameraRotateModeOffset = 0x6c;
constexpr size_t kCharacterBodyCameraUpdateActiveOffset = 0x1e8;
constexpr int kEulerAnglesRotateMode = 0;

using GetPlayerFn = void* (*)();
using GetPlayerStateIdFn = int (*)(void* player);

Config g_config;
uintptr_t g_executableBase = 0;
GetPlayerFn g_getPlayer = nullptr;
GetPlayerStateIdFn g_getPlayerStateId = nullptr;
GetPlayerStateIdFn g_getMoveStateId = nullptr;
void** g_gameContextSlot = nullptr;
HPLPlayerStateSnapshot g_snapshot;
std::mutex g_mutex;
std::atomic<uint64_t> g_updates = 0;
std::atomic<uint64_t> g_transitions = 0;

bool IsInsideImage(HMODULE module, uintptr_t rva, size_t bytes)
{
    if (module == nullptr)
        return false;
    const auto* base = reinterpret_cast<const std::byte*>(module);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE)
        return false;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    return nt->Signature == IMAGE_NT_SIGNATURE && rva < nt->OptionalHeader.SizeOfImage &&
           bytes <= nt->OptionalHeader.SizeOfImage - rva;
}

bool MatchGetterSignature(const std::byte* address)
{
    constexpr uint8_t prefix[] = {0x48, 0x8b, 0x05};
    constexpr uint8_t suffix[] = {0x48, 0x8b, 0x80, 0x40, 0x01, 0x00, 0x00, 0xc3};
    return std::memcmp(address, prefix, sizeof(prefix)) == 0 && std::memcmp(address + 7, suffix, sizeof(suffix)) == 0;
}

bool MatchStateGetterSignature(const std::byte* address, uint32_t playerOffset, uint32_t stateOffset)
{
    return address[0] == std::byte{0x48} && address[1] == std::byte{0x8b} && address[2] == std::byte{0x81} &&
           std::memcmp(address + 3, &playerOffset, sizeof(playerOffset)) == 0 && address[7] == std::byte{0x48} &&
           address[8] == std::byte{0x85} && address[9] == std::byte{0xc0} && address[12] == std::byte{0x8b} &&
           address[13] == std::byte{0x80} && std::memcmp(address + 14, &stateOffset, sizeof(stateOffset)) == 0;
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

template <typename T> bool ReadField(const void* object, size_t offset, T& value)
{
    if (object == nullptr)
        return false;
    const auto* address = static_cast<const std::byte*>(object) + offset;
    if (!IsReadable(address, sizeof(value)))
        return false;
    std::memcpy(&value, address, sizeof(value));
    return true;
}

bool StateChanged(const HPLPlayerStateSnapshot& left, const HPLPlayerStateSnapshot& right)
{
    return left.player != right.player || left.camera != right.camera || left.characterBody != right.characterBody ||
           left.playerStateId != right.playerStateId || left.moveStateId != right.moveStateId ||
           left.cameraRotateMode != right.cameraRotateMode || left.cameraUpdateActive != right.cameraUpdateActive ||
           left.authoredCameraActive != right.authoredCameraActive;
}

} // namespace

bool InstallHPLPlayerState(const Config& config)
{
    std::lock_guard lock(g_mutex);
    g_config = config;
    HMODULE executable = GetModuleHandleW(nullptr);
    if (!IsInsideImage(executable, kGetPlayerRva, 15) || !IsInsideImage(executable, kGetCurrentStateIdRva, 20) ||
        !IsInsideImage(executable, kGetCurrentMoveStateIdRva, 20))
    {
        Logger::Instance().Write(LogLevel::Error, "hpl_player_state install_failed reason=invalid_image_range");
        return false;
    }

    g_executableBase = reinterpret_cast<uintptr_t>(executable);
    const auto* getter = reinterpret_cast<const std::byte*>(g_executableBase + kGetPlayerRva);
    const auto* stateGetter = reinterpret_cast<const std::byte*>(g_executableBase + kGetCurrentStateIdRva);
    const auto* moveStateGetter = reinterpret_cast<const std::byte*>(g_executableBase + kGetCurrentMoveStateIdRva);
    if (!MatchGetterSignature(getter) || !MatchStateGetterSignature(stateGetter, 0x1d8, 0x160) ||
        !MatchStateGetterSignature(moveStateGetter, 0x200, 0x158))
    {
        Logger::Instance().Write(LogLevel::Error, "hpl_player_state install_failed reason=signature_mismatch");
        return false;
    }

    int32_t gameContextDisplacement = 0;
    std::memcpy(&gameContextDisplacement, getter + 3, sizeof(gameContextDisplacement));
    g_gameContextSlot = reinterpret_cast<void**>(const_cast<std::byte*>(getter + 7) + gameContextDisplacement);
    if (!IsReadable(g_gameContextSlot, sizeof(*g_gameContextSlot)))
    {
        g_gameContextSlot = nullptr;
        Logger::Instance().Write(LogLevel::Error, "hpl_player_state install_failed reason=invalid_game_context_slot");
        return false;
    }

    g_getPlayer = reinterpret_cast<GetPlayerFn>(const_cast<std::byte*>(getter));
    g_getPlayerStateId = reinterpret_cast<GetPlayerStateIdFn>(const_cast<std::byte*>(stateGetter));
    g_getMoveStateId = reinterpret_cast<GetPlayerStateIdFn>(const_cast<std::byte*>(moveStateGetter));
    g_snapshot = {};
    g_snapshot.installed = true;
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_player_state install_ok getPlayerRva=0x%llx playerStateRva=0x%llx moveStateRva=0x%llx "
        "cameraRotateModeOffset=0x%llx bodyCameraUpdateOffset=0x%llx eulerMode=%d",
        static_cast<unsigned long long>(kGetPlayerRva), static_cast<unsigned long long>(kGetCurrentStateIdRva),
        static_cast<unsigned long long>(kGetCurrentMoveStateIdRva),
        static_cast<unsigned long long>(kCameraRotateModeOffset),
        static_cast<unsigned long long>(kCharacterBodyCameraUpdateActiveOffset), kEulerAnglesRotateMode);
    return true;
}

void UpdateHPLPlayerState(uint64_t frameIndex)
{
    std::lock_guard lock(g_mutex);
    if (g_getPlayer == nullptr || g_snapshot.frame == frameIndex)
        return;

    HPLPlayerStateSnapshot next;
    next.installed = true;
    next.frame = frameIndex;
    void* gameContext = nullptr;
    ReadField(g_gameContextSlot, 0, gameContext);
    next.player = IsReadable(gameContext, 0x148) ? g_getPlayer() : nullptr;
    next.playerValid = IsReadable(next.player, 0x208);
    if (next.playerValid)
    {
        ReadField(next.player, kPlayerCameraOffset, next.camera);
        ReadField(next.player, kPlayerCharacterBodyOffset, next.characterBody);
        void* playerStateObject = nullptr;
        void* moveStateObject = nullptr;
        ReadField(next.player, 0x1d8, playerStateObject);
        ReadField(next.player, 0x200, moveStateObject);
        if (playerStateObject == nullptr || IsReadable(playerStateObject, 0x164))
        {
            next.playerStateId = g_getPlayerStateId(next.player);
        }
        if (moveStateObject == nullptr || IsReadable(moveStateObject, 0x15c))
        {
            next.moveStateId = g_getMoveStateId(next.player);
        }

        bool cameraUpdateActive = false;
        const bool rotateModeValid = ReadField(next.camera, kCameraRotateModeOffset, next.cameraRotateMode);
        const bool cameraUpdateValid =
            ReadField(next.characterBody, kCharacterBodyCameraUpdateActiveOffset, cameraUpdateActive);
        next.cameraControlValid = rotateModeValid && cameraUpdateValid;
        next.cameraUpdateActive = cameraUpdateActive;
        next.authoredCameraActive =
            next.cameraControlValid && (next.cameraRotateMode != kEulerAnglesRotateMode || !next.cameraUpdateActive);
    }

    const bool changed = StateChanged(g_snapshot, next);
    if (changed)
        g_transitions.fetch_add(1, std::memory_order_relaxed);
    const uint64_t interval = static_cast<uint64_t>(std::max(g_config.hplControllerLogInterval, 1));
    if (changed || frameIndex == 1 || frameIndex % interval == 0)
    {
        const HPLCameraBridgeStatus cameraStatus = GetHPLCameraBridgeStatus();
        Logger::Instance().Write(
            next.authoredCameraActive ? LogLevel::Warn : LogLevel::Info,
            "hpl_player_state frame=%llu player=%p camera=%p body=%p playerState=%d moveState=%d controlValid=%d "
            "rotateMode=%d cameraUpdateActive=%d authoredCamera=%d activeCamera=%p cameraMatch=%d tracking=%d "
            "stereo=%d transition=%d",
            static_cast<unsigned long long>(frameIndex), next.player, next.camera, next.characterBody,
            next.playerStateId, next.moveStateId, next.cameraControlValid ? 1 : 0, next.cameraRotateMode,
            next.cameraUpdateActive ? 1 : 0, next.authoredCameraActive ? 1 : 0, cameraStatus.activeCamera,
            next.camera != nullptr && next.camera == cameraStatus.activeCamera ? 1 : 0,
            cameraStatus.trackingEnabled ? 1 : 0, cameraStatus.stereoEnabled ? 1 : 0, changed ? 1 : 0);
    }

    g_snapshot = next;
    g_updates.fetch_add(1, std::memory_order_relaxed);
}

bool GetHPLPlayerStateSnapshot(HPLPlayerStateSnapshot& snapshot)
{
    std::lock_guard lock(g_mutex);
    snapshot = g_snapshot;
    return snapshot.installed;
}

void LogHPLPlayerStateSummary()
{
    std::lock_guard lock(g_mutex);
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_player_state_summary installed=%d updates=%llu transitions=%llu frame=%llu player=%p camera=%p body=%p "
        "playerState=%d moveState=%d rotateMode=%d cameraUpdateActive=%d authoredCamera=%d",
        g_snapshot.installed ? 1 : 0, static_cast<unsigned long long>(g_updates.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_transitions.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_snapshot.frame), g_snapshot.player, g_snapshot.camera,
        g_snapshot.characterBody, g_snapshot.playerStateId, g_snapshot.moveStateId, g_snapshot.cameraRotateMode,
        g_snapshot.cameraUpdateActive ? 1 : 0, g_snapshot.authoredCameraActive ? 1 : 0);
}

void RemoveHPLPlayerState()
{
    std::lock_guard lock(g_mutex);
    g_getPlayer = nullptr;
    g_getPlayerStateId = nullptr;
    g_getMoveStateId = nullptr;
    g_gameContextSlot = nullptr;
    g_snapshot = {};
    Logger::Instance().Write(LogLevel::Info, "hpl_player_state removed");
}

} // namespace somavr
