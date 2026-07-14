#include "HPLInputBridge.h"

#include "HPLCameraBridge.h"
#include "Logger.h"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>

namespace somavr {
namespace {

constexpr uintptr_t kGetPlayerRva = 0x0cc860;
constexpr uintptr_t kGetCurrentStateIdRva = 0x155050;
constexpr uintptr_t kGetCurrentMoveStateIdRva = 0x155090;
constexpr size_t kPlayerCameraOffset = 0x168;
constexpr size_t kPlayerCharacterBodyOffset = 0x170;

using GetPlayerFn = void* (*)();
using GetPlayerStateIdFn = int (*)(void* player);

struct ButtonState {
    bool down = false;
};

struct BridgeState {
    ButtonState forward;
    ButtonState backward;
    ButtonState left;
    ButtonState right;
    ButtonState interact;
    bool snapLatched = false;
    bool recenterLatched = false;
    uint64_t recenterStartMs = 0;
    uint64_t lastFrame = 0;
    uint64_t lastTickMs = 0;
    double smoothTurnRemainder = 0.0;
    void* lastPlayer = nullptr;
    void* lastCamera = nullptr;
    void* lastBody = nullptr;
    int lastPlayerState = -2;
    int lastMoveState = -2;
};

Config g_config;
OpenXRRuntime* g_openxr = nullptr;
uintptr_t g_executableBase = 0;
GetPlayerFn g_getPlayer = nullptr;
GetPlayerStateIdFn g_getPlayerStateId = nullptr;
GetPlayerStateIdFn g_getMoveStateId = nullptr;
void** g_gameContextSlot = nullptr;
BridgeState g_state;
std::mutex g_mutex;
std::atomic<uint64_t> g_updates = 0;
std::atomic<uint64_t> g_activeUpdates = 0;
std::atomic<uint64_t> g_sentEvents = 0;
std::atomic<uint64_t> g_sendFailures = 0;
std::atomic<uint64_t> g_staleInputFrames = 0;
std::atomic<uint64_t> g_recenterRequests = 0;

bool IsInsideImage(HMODULE module, uintptr_t rva, size_t bytes)
{
    if (module == nullptr) return false;
    const auto* base = reinterpret_cast<const std::byte*>(module);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    return nt->Signature == IMAGE_NT_SIGNATURE
        && rva < nt->OptionalHeader.SizeOfImage
        && bytes <= nt->OptionalHeader.SizeOfImage - rva;
}

bool MatchGetterSignature(const std::byte* address)
{
    constexpr uint8_t prefix[] = {0x48, 0x8b, 0x05};
    constexpr uint8_t suffix[] = {0x48, 0x8b, 0x80, 0x40, 0x01, 0x00, 0x00, 0xc3};
    return std::memcmp(address, prefix, sizeof(prefix)) == 0
        && std::memcmp(address + 7, suffix, sizeof(suffix)) == 0;
}

bool MatchStateGetterSignature(const std::byte* address, uint32_t playerOffset, uint32_t stateOffset)
{
    return address[0] == std::byte{0x48} && address[1] == std::byte{0x8b}
        && address[2] == std::byte{0x81}
        && std::memcmp(address + 3, &playerOffset, sizeof(playerOffset)) == 0
        && address[7] == std::byte{0x48} && address[8] == std::byte{0x85}
        && address[9] == std::byte{0xc0}
        && address[12] == std::byte{0x8b} && address[13] == std::byte{0x80}
        && std::memcmp(address + 14, &stateOffset, sizeof(stateOffset)) == 0;
}

bool IsReadable(const void* address, size_t bytes)
{
    if (address == nullptr) return false;
    MEMORY_BASIC_INFORMATION info{};
    if (VirtualQuery(address, &info, sizeof(info)) != sizeof(info)) return false;
    if (info.State != MEM_COMMIT || (info.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) return false;
    const uintptr_t start = reinterpret_cast<uintptr_t>(address);
    const uintptr_t end = reinterpret_cast<uintptr_t>(info.BaseAddress) + info.RegionSize;
    return start <= end && bytes <= end - start;
}

template <typename T>
T ReadField(const void* object, size_t offset)
{
    T value{};
    const auto* address = static_cast<const std::byte*>(object) + offset;
    if (IsReadable(address, sizeof(value))) std::memcpy(&value, address, sizeof(value));
    return value;
}

uint64_t TickMs()
{
    return GetTickCount64();
}

bool SendInputs(INPUT* inputs, UINT count)
{
    if (count == 0) return true;
    const UINT sent = SendInput(count, inputs, sizeof(INPUT));
    g_sentEvents.fetch_add(sent, std::memory_order_relaxed);
    if (sent != count) {
        g_sendFailures.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    return true;
}

void SetKey(ButtonState& state, WORD virtualKey, bool down)
{
    if (state.down == down) return;
    INPUT input{};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = virtualKey;
    input.ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP;
    SendInputs(&input, 1);
    state.down = down;
}

void SetMouseButton(ButtonState& state, bool down)
{
    if (state.down == down) return;
    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
    SendInputs(&input, 1);
    state.down = down;
}

void SendMouseMove(int dx)
{
    if (dx == 0) return;
    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dx = dx;
    input.mi.dwFlags = MOUSEEVENTF_MOVE;
    SendInputs(&input, 1);
}

void TapKey(WORD virtualKey)
{
    std::array<INPUT, 2> inputs{};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = virtualKey;
    inputs[1] = inputs[0];
    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInputs(inputs.data(), static_cast<UINT>(inputs.size()));
}

bool UpdateAxisButton(ButtonState& state, float value, float pressThreshold, float releaseThreshold)
{
    return state.down ? value > releaseThreshold : value > pressThreshold;
}

void ReleaseAll()
{
    SetKey(g_state.forward, 'W', false);
    SetKey(g_state.backward, 'S', false);
    SetKey(g_state.left, 'A', false);
    SetKey(g_state.right, 'D', false);
    SetMouseButton(g_state.interact, false);
    g_state.snapLatched = false;
    g_state.recenterStartMs = 0;
}

void UpdatePlayerProbe(uint64_t frameIndex)
{
    void* gameContext = ReadField<void*>(g_gameContextSlot, 0);
    void* player = g_getPlayer != nullptr && IsReadable(gameContext, 0x148)
        ? g_getPlayer()
        : nullptr;
    void* camera = nullptr;
    void* body = nullptr;
    int playerState = -1;
    int moveState = -1;
    if (IsReadable(player, 0x208)) {
        camera = ReadField<void*>(player, kPlayerCameraOffset);
        body = ReadField<void*>(player, kPlayerCharacterBodyOffset);
        void* playerStateObject = ReadField<void*>(player, 0x1d8);
        void* moveStateObject = ReadField<void*>(player, 0x200);
        if (g_getPlayerStateId != nullptr
            && (playerStateObject == nullptr || IsReadable(playerStateObject, 0x164))) {
            playerState = g_getPlayerStateId(player);
        }
        if (g_getMoveStateId != nullptr
            && (moveStateObject == nullptr || IsReadable(moveStateObject, 0x15c))) {
            moveState = g_getMoveStateId(player);
        }
    }

    const bool changed = player != g_state.lastPlayer || camera != g_state.lastCamera
        || body != g_state.lastBody || playerState != g_state.lastPlayerState
        || moveState != g_state.lastMoveState;
    const uint64_t interval = static_cast<uint64_t>(std::max(g_config.hplControllerLogInterval, 1));
    if (changed || frameIndex == 1 || frameIndex % interval == 0) {
        const HPLCameraBridgeStatus cameraStatus = GetHPLCameraBridgeStatus();
        Logger::Instance().Write(
            LogLevel::Info,
            "hpl_player_probe frame=%llu player=%p camera=%p body=%p playerState=%d moveState=%d activeCamera=%p cameraMatch=%d tracking=%d stereo=%d",
            static_cast<unsigned long long>(frameIndex), player, camera, body, playerState, moveState,
            cameraStatus.activeCamera, camera != nullptr && camera == cameraStatus.activeCamera ? 1 : 0,
            cameraStatus.trackingEnabled ? 1 : 0, cameraStatus.stereoEnabled ? 1 : 0);
    }
    g_state.lastPlayer = player;
    g_state.lastCamera = camera;
    g_state.lastBody = body;
    g_state.lastPlayerState = playerState;
    g_state.lastMoveState = moveState;
}

void ApplyLocomotion(const OpenXRInputSnapshot& input)
{
    const float press = g_config.hplControllerMoveDeadzone;
    const float release = std::min(g_config.hplControllerMoveReleaseDeadzone, press);
    SetKey(g_state.forward, 'W', UpdateAxisButton(g_state.forward, input.moveY, press, release));
    SetKey(g_state.backward, 'S', UpdateAxisButton(g_state.backward, -input.moveY, press, release));
    SetKey(g_state.right, 'D', UpdateAxisButton(g_state.right, input.moveX, press, release));
    SetKey(g_state.left, 'A', UpdateAxisButton(g_state.left, -input.moveX, press, release));
}

void ApplyTurn(const OpenXRInputSnapshot& input, uint64_t nowMs)
{
    const float turn = input.turnX;
    if (g_config.hplControllerSnapTurn) {
        const float releaseThreshold = std::min(
            g_config.hplControllerTurnReleaseDeadzone,
            g_config.hplControllerTurnDeadzone);
        if (!g_state.snapLatched && std::fabs(turn) >= g_config.hplControllerTurnDeadzone) {
            SendMouseMove(turn > 0.0f ? g_config.hplControllerSnapTurnPixels : -g_config.hplControllerSnapTurnPixels);
            g_state.snapLatched = true;
        } else if (g_state.snapLatched && std::fabs(turn) <= releaseThreshold) {
            g_state.snapLatched = false;
        }
        return;
    }

    const uint64_t elapsedMs = g_state.lastTickMs == 0 ? 0 : std::min<uint64_t>(nowMs - g_state.lastTickMs, 100);
    if (std::fabs(turn) < g_config.hplControllerTurnDeadzone || elapsedMs == 0) return;
    g_state.smoothTurnRemainder += static_cast<double>(turn)
        * static_cast<double>(g_config.hplControllerSmoothTurnPixelsPerSecond)
        * static_cast<double>(elapsedMs) / 1000.0;
    const int pixels = static_cast<int>(std::trunc(g_state.smoothTurnRemainder));
    g_state.smoothTurnRemainder -= pixels;
    SendMouseMove(pixels);
}

void ApplyActions(const OpenXRInputSnapshot& input, uint64_t nowMs)
{
    if (g_config.hplControllerInteraction) {
        const bool interact = input.right.select || input.right.trigger >= 0.75f;
        SetMouseButton(g_state.interact, interact);
    }
    if (g_config.hplControllerMenu && input.menu && input.menuChanged) TapKey(VK_ESCAPE);

    const bool recenterChord = g_config.hplControllerRecenterChord
        && input.left.squeeze >= 0.85f && input.right.squeeze >= 0.85f;
    if (!recenterChord) {
        g_state.recenterStartMs = 0;
        g_state.recenterLatched = false;
    } else if (!g_state.recenterLatched) {
        if (g_state.recenterStartMs == 0) g_state.recenterStartMs = nowMs;
        if (nowMs - g_state.recenterStartMs >= static_cast<uint64_t>(g_config.hplControllerRecenterHoldMs)) {
            if (RequestHPLRecenter("controller_grip_chord")) {
                g_recenterRequests.fetch_add(1, std::memory_order_relaxed);
            }
            g_state.recenterLatched = true;
        }
    }
}

} // namespace

bool InstallHPLInputBridge(const Config& config, OpenXRRuntime* openxr)
{
    std::lock_guard lock(g_mutex);
    g_config = config;
    g_openxr = openxr;
    HMODULE executable = GetModuleHandleW(nullptr);
    if (!IsInsideImage(executable, kGetPlayerRva, 15)
        || !IsInsideImage(executable, kGetCurrentStateIdRva, 20)
        || !IsInsideImage(executable, kGetCurrentMoveStateIdRva, 20)) {
        Logger::Instance().Write(LogLevel::Error, "hpl_input_bridge install_failed reason=invalid_image_range");
        return false;
    }
    g_executableBase = reinterpret_cast<uintptr_t>(executable);
    const auto* getter = reinterpret_cast<const std::byte*>(g_executableBase + kGetPlayerRva);
    const auto* stateGetter = reinterpret_cast<const std::byte*>(g_executableBase + kGetCurrentStateIdRva);
    const auto* moveStateGetter = reinterpret_cast<const std::byte*>(g_executableBase + kGetCurrentMoveStateIdRva);
    if (!MatchGetterSignature(getter)
        || !MatchStateGetterSignature(stateGetter, 0x1d8, 0x160)
        || !MatchStateGetterSignature(moveStateGetter, 0x200, 0x158)) {
        Logger::Instance().Write(LogLevel::Error, "hpl_input_bridge install_failed reason=signature_mismatch");
        return false;
    }
    g_getPlayer = reinterpret_cast<GetPlayerFn>(const_cast<std::byte*>(getter));
    g_getPlayerStateId = reinterpret_cast<GetPlayerStateIdFn>(const_cast<std::byte*>(stateGetter));
    g_getMoveStateId = reinterpret_cast<GetPlayerStateIdFn>(const_cast<std::byte*>(moveStateGetter));
    int32_t gameContextDisplacement = 0;
    std::memcpy(&gameContextDisplacement, getter + 3, sizeof(gameContextDisplacement));
    g_gameContextSlot = reinterpret_cast<void**>(
        const_cast<std::byte*>(getter + 7) + gameContextDisplacement);
    if (!IsReadable(g_gameContextSlot, sizeof(*g_gameContextSlot))) {
        g_getPlayer = nullptr;
        g_getPlayerStateId = nullptr;
        g_getMoveStateId = nullptr;
        g_gameContextSlot = nullptr;
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_input_bridge install_failed reason=invalid_game_context_slot");
        return false;
    }
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_input_bridge install_ok enabled=%d getPlayerRva=0x%llx playerStateRva=0x%llx moveStateRva=0x%llx moveDeadzone=%.2f turnMode=%s turnDeadzone=%.2f interaction=%d menu=%d recenterChord=%d maxInputAgeFrames=%d",
        config.hplControllerInput ? 1 : 0,
        static_cast<unsigned long long>(kGetPlayerRva),
        static_cast<unsigned long long>(kGetCurrentStateIdRva),
        static_cast<unsigned long long>(kGetCurrentMoveStateIdRva),
        config.hplControllerMoveDeadzone,
        config.hplControllerSnapTurn ? "snap" : "smooth",
        config.hplControllerTurnDeadzone,
        config.hplControllerInteraction ? 1 : 0,
        config.hplControllerMenu ? 1 : 0,
        config.hplControllerRecenterChord ? 1 : 0,
        config.hplControllerMaxInputAgeFrames);
    return true;
}

void UpdateHPLInputBridge(uint64_t frameIndex)
{
    std::lock_guard lock(g_mutex);
    if (g_getPlayer == nullptr) return;
    if (g_state.lastFrame == frameIndex) return;
    g_state.lastFrame = frameIndex;
    g_updates.fetch_add(1, std::memory_order_relaxed);
    UpdatePlayerProbe(frameIndex);

    OpenXRInputSnapshot input;
    const HPLCameraBridgeStatus camera = GetHPLCameraBridgeStatus();
    const bool available = g_config.hplControllerInput && camera.trackingEnabled
        && g_openxr != nullptr && g_openxr->GetLatestInput(input) && input.active;
    const uint64_t age = available && frameIndex >= input.gameFrame ? frameIndex - input.gameFrame : UINT64_MAX;
    if (!available || age > static_cast<uint64_t>(g_config.hplControllerMaxInputAgeFrames)) {
        if (available) g_staleInputFrames.fetch_add(1, std::memory_order_relaxed);
        ReleaseAll();
        g_state.lastTickMs = TickMs();
        return;
    }

    g_activeUpdates.fetch_add(1, std::memory_order_relaxed);
    const uint64_t nowMs = TickMs();
    ApplyLocomotion(input);
    ApplyTurn(input, nowMs);
    ApplyActions(input, nowMs);
    g_state.lastTickMs = nowMs;

    const uint64_t interval = static_cast<uint64_t>(std::max(g_config.hplControllerLogInterval, 1));
    if (frameIndex % interval == 0) {
        Logger::Instance().Write(
            LogLevel::Info,
            "hpl_controller frame=%llu inputFrame=%llu age=%llu move=%.3f,%.3f keys=%d%d%d%d turn=%.3f mode=%s interact=%d menu=%d recenterChord=%d playerState=%d moveState=%d",
            static_cast<unsigned long long>(frameIndex),
            static_cast<unsigned long long>(input.gameFrame),
            static_cast<unsigned long long>(age),
            input.moveX, input.moveY,
            g_state.forward.down ? 1 : 0, g_state.backward.down ? 1 : 0,
            g_state.left.down ? 1 : 0, g_state.right.down ? 1 : 0,
            input.turnX, g_config.hplControllerSnapTurn ? "snap" : "smooth",
            g_state.interact.down ? 1 : 0, input.menu ? 1 : 0,
            g_state.recenterStartMs != 0 ? 1 : 0,
            g_state.lastPlayerState, g_state.lastMoveState);
    }
}

void RemoveHPLInputBridge()
{
    std::lock_guard lock(g_mutex);
    ReleaseAll();
    g_getPlayer = nullptr;
    g_getPlayerStateId = nullptr;
    g_getMoveStateId = nullptr;
    g_gameContextSlot = nullptr;
    g_openxr = nullptr;
    g_state = {};
    Logger::Instance().Write(LogLevel::Info, "hpl_input_bridge removed");
}

void LogHPLInputBridgeSummary()
{
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_input_bridge_summary installed=%d updates=%llu activeUpdates=%llu sentEvents=%llu sendFailures=%llu staleInputFrames=%llu recenterRequests=%llu lastPlayer=%p lastCamera=%p lastBody=%p playerState=%d moveState=%d",
        g_getPlayer != nullptr ? 1 : 0,
        static_cast<unsigned long long>(g_updates.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_activeUpdates.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_sentEvents.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_sendFailures.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_staleInputFrames.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_recenterRequests.load(std::memory_order_relaxed)),
        g_state.lastPlayer, g_state.lastCamera, g_state.lastBody,
        g_state.lastPlayerState, g_state.lastMoveState);
}

} // namespace somavr
