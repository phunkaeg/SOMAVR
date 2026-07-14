#include "HPLInputBridge.h"

#include "HPLCameraBridge.h"
#include "HPLPlayerState.h"
#include "Logger.h"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <mutex>

namespace somavr {
namespace {

struct ButtonState {
    bool down = false;
};

struct BridgeState {
    ButtonState forward;
    ButtonState backward;
    ButtonState left;
    ButtonState right;
    ButtonState interact;
    ButtonState sprint;
    bool snapLatched = false;
    bool recenterLatched = false;
    uint64_t recenterStartMs = 0;
    uint64_t lastFrame = 0;
    uint64_t lastTickMs = 0;
    double smoothTurnRemainder = 0.0;
    bool authoredCameraSuppressed = false;
    bool oneHandFallbackActive = false;
};

struct ControllerRoles {
    uint32_t dominantHand = 1;
    uint32_t supportHand = 0;
    uint32_t movementHand = 0;
    uint32_t turnHand = 1;
    bool oneHand = false;
    float moveX = 0.0f;
    float moveY = 0.0f;
    float turnX = 0.0f;
};

Config g_config;
OpenXRRuntime* g_openxr = nullptr;
BridgeState g_state;
std::mutex g_mutex;
std::atomic<uint64_t> g_updates = 0;
std::atomic<uint64_t> g_activeUpdates = 0;
std::atomic<uint64_t> g_sentEvents = 0;
std::atomic<uint64_t> g_sendFailures = 0;
std::atomic<uint64_t> g_staleInputFrames = 0;
std::atomic<uint64_t> g_recenterRequests = 0;
std::atomic<uint64_t> g_hapticRequests = 0;
std::atomic<uint64_t> g_hapticApplied = 0;
std::atomic<uint64_t> g_oneHandFallbackFrames = 0;

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

void ReleaseGameplayInputs()
{
    SetKey(g_state.forward, 'W', false);
    SetKey(g_state.backward, 'S', false);
    SetKey(g_state.left, 'A', false);
    SetKey(g_state.right, 'D', false);
    SetMouseButton(g_state.interact, false);
    SetKey(g_state.sprint, VK_LSHIFT, false);
    g_state.snapLatched = false;
    g_state.smoothTurnRemainder = 0.0;
}

const OpenXRHandInput& HandInput(const OpenXRInputSnapshot& input, uint32_t hand)
{
    return hand == 0 ? input.left : input.right;
}

ControllerRoles ResolveControllerRoles(const OpenXRInputSnapshot& input)
{
    ControllerRoles roles;
    roles.dominantHand = g_config.hplControllerDominantHand == "left" ? 0u : 1u;
    roles.supportHand = roles.dominantHand ^ 1u;

    const bool leftOnly = input.left.active && !input.right.active;
    const bool rightOnly = input.right.active && !input.left.active;
    if (g_config.hplControllerOneHandFallback && (leftOnly || rightOnly)) {
        roles.oneHand = true;
        roles.dominantHand = leftOnly ? 0u : 1u;
        roles.supportHand = roles.dominantHand;
        roles.movementHand = roles.dominantHand;
        roles.turnHand = roles.dominantHand;
        roles.moveX = leftOnly ? input.moveX : input.turnX;
        roles.moveY = leftOnly ? input.moveY : input.turnY;
        roles.turnX = 0.0f;
        return roles;
    }

    roles.movementHand = g_config.hplControllerSwapSticks ? 1u : 0u;
    roles.turnHand = roles.movementHand ^ 1u;
    roles.moveX = g_config.hplControllerSwapSticks ? input.turnX : input.moveX;
    roles.moveY = g_config.hplControllerSwapSticks ? input.turnY : input.moveY;
    roles.turnX = g_config.hplControllerSwapSticks ? input.moveX : input.turnX;
    return roles;
}

void PulseHaptic(uint32_t hand, const char* reason)
{
    if (!g_config.hplControllerHaptics || g_openxr == nullptr) {
        return;
    }
    g_hapticRequests.fetch_add(1, std::memory_order_relaxed);
    if (g_openxr->RequestHapticPulse(
            hand,
            g_config.hplControllerHapticAmplitude,
            g_config.hplControllerHapticDurationMs,
            reason)) {
        g_hapticApplied.fetch_add(1, std::memory_order_relaxed);
    }
}

void ReleaseAll()
{
    ReleaseGameplayInputs();
    g_state.recenterStartMs = 0;
    g_state.recenterLatched = false;
}

void ApplyLocomotion(const ControllerRoles& roles)
{
    const float press = g_config.hplControllerMoveDeadzone;
    const float release = std::min(g_config.hplControllerMoveReleaseDeadzone, press);
    SetKey(g_state.forward, 'W', UpdateAxisButton(g_state.forward, roles.moveY, press, release));
    SetKey(g_state.backward, 'S', UpdateAxisButton(g_state.backward, -roles.moveY, press, release));
    SetKey(g_state.right, 'D', UpdateAxisButton(g_state.right, roles.moveX, press, release));
    SetKey(g_state.left, 'A', UpdateAxisButton(g_state.left, -roles.moveX, press, release));
}

void ApplyTurn(const ControllerRoles& roles, uint64_t nowMs)
{
    const float turn = roles.turnX;
    if (g_config.hplControllerSnapTurn) {
        const float releaseThreshold = std::min(
            g_config.hplControllerTurnReleaseDeadzone,
            g_config.hplControllerTurnDeadzone);
        if (!g_state.snapLatched && std::fabs(turn) >= g_config.hplControllerTurnDeadzone) {
            SendMouseMove(turn > 0.0f ? g_config.hplControllerSnapTurnPixels : -g_config.hplControllerSnapTurnPixels);
            PulseHaptic(roles.turnHand, "snap_turn");
            if (g_openxr != nullptr && g_config.hplControllerComfortBlackoutFrames > 0) {
                g_openxr->RequestComfortBlackout(
                    static_cast<uint32_t>(g_config.hplControllerComfortBlackoutFrames),
                    "snap_turn");
            }
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

void ApplySystemActions(const OpenXRInputSnapshot& input, const ControllerRoles& roles, uint64_t nowMs)
{
    if (g_config.hplControllerMenu && input.menu && input.menuChanged) {
        TapKey(VK_ESCAPE);
        PulseHaptic(input.left.active ? 0u : roles.dominantHand, "menu");
    }

    const OpenXRHandInput& dominant = HandInput(input, roles.dominantHand);
    const bool twoHandChord = input.left.squeeze >= 0.85f && input.right.squeeze >= 0.85f;
    const bool oneHandChord = roles.oneHand && dominant.primary && dominant.secondary;
    const bool recenterChord = g_config.hplControllerRecenterChord && (twoHandChord || oneHandChord);
    if (!recenterChord) {
        g_state.recenterStartMs = 0;
        g_state.recenterLatched = false;
    } else if (!g_state.recenterLatched) {
        if (g_state.recenterStartMs == 0) g_state.recenterStartMs = nowMs;
        if (nowMs - g_state.recenterStartMs >= static_cast<uint64_t>(g_config.hplControllerRecenterHoldMs)) {
            if (RequestHPLRecenter(roles.oneHand ? "controller_face_chord" : "controller_grip_chord")) {
                g_recenterRequests.fetch_add(1, std::memory_order_relaxed);
                PulseHaptic(roles.dominantHand, "recenter");
                if (!roles.oneHand) PulseHaptic(roles.supportHand, "recenter");
            }
            g_state.recenterLatched = true;
        }
    }
}

void ApplyGameplayActions(const OpenXRInputSnapshot& input, const ControllerRoles& roles)
{
    const OpenXRHandInput& dominant = HandInput(input, roles.dominantHand);
    const OpenXRHandInput& support = HandInput(input, roles.supportHand);
    const bool recenterChord = roles.oneHand && dominant.primary && dominant.secondary;
    SetKey(g_state.sprint, VK_LSHIFT, !roles.oneHand && support.trigger >= 0.75f);
    if (!recenterChord && dominant.primary && dominant.primaryChanged) {
        TapKey(VK_SPACE);
        PulseHaptic(roles.dominantHand, "jump");
    }
    if (!recenterChord && dominant.secondary && dominant.secondaryChanged) {
        TapKey(VK_LCONTROL);
        PulseHaptic(roles.dominantHand, "crouch");
    }
    if (g_config.hplControllerInteraction) {
        const bool interact = dominant.select || dominant.trigger >= 0.75f;
        if (interact && !g_state.interact.down) {
            PulseHaptic(roles.dominantHand, "interaction");
        }
        SetMouseButton(g_state.interact, interact);
    }
}

} // namespace

bool InstallHPLInputBridge(const Config& config, OpenXRRuntime* openxr)
{
    std::lock_guard lock(g_mutex);
    g_config = config;
    g_openxr = openxr;
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_input_bridge install_ok enabled=%d moveDeadzone=%.2f turnMode=%s turnDeadzone=%.2f interaction=%d menu=%d recenterChord=%d haptics=%d hapticAmplitude=%.2f hapticDurationMs=%d dominantHand=%s swapSticks=%d oneHandFallback=%d suppressAuthoredCamera=%d comfortBlackoutFrames=%d maxInputAgeFrames=%d",
        config.hplControllerInput ? 1 : 0,
        config.hplControllerMoveDeadzone,
        config.hplControllerSnapTurn ? "snap" : "smooth",
        config.hplControllerTurnDeadzone,
        config.hplControllerInteraction ? 1 : 0,
        config.hplControllerMenu ? 1 : 0,
        config.hplControllerRecenterChord ? 1 : 0,
        config.hplControllerHaptics ? 1 : 0,
        config.hplControllerHapticAmplitude,
        config.hplControllerHapticDurationMs,
        config.hplControllerDominantHand.c_str(),
        config.hplControllerSwapSticks ? 1 : 0,
        config.hplControllerOneHandFallback ? 1 : 0,
        config.hplControllerSuppressDuringAuthoredCamera ? 1 : 0,
        config.hplControllerComfortBlackoutFrames,
        config.hplControllerMaxInputAgeFrames);
    return true;
}

void UpdateHPLInputBridge(uint64_t frameIndex)
{
    std::lock_guard lock(g_mutex);
    if (g_state.lastFrame == frameIndex) return;
    g_state.lastFrame = frameIndex;
    g_updates.fetch_add(1, std::memory_order_relaxed);

    OpenXRInputSnapshot input;
    HPLPlayerStateSnapshot player;
    GetHPLPlayerStateSnapshot(player);
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
    const ControllerRoles roles = ResolveControllerRoles(input);
    if (roles.oneHand) g_oneHandFallbackFrames.fetch_add(1, std::memory_order_relaxed);
    if (roles.oneHand != g_state.oneHandFallbackActive) {
        Logger::Instance().Write(
            roles.oneHand ? LogLevel::Warn : LogLevel::Info,
            "hpl_controller one_hand_fallback=%d activeHand=%s movementStick=%s turnSuppressed=%d",
            roles.oneHand ? 1 : 0,
            roles.dominantHand == 0 ? "left" : "right",
            roles.movementHand == 0 ? "left" : "right",
            roles.oneHand ? 1 : 0);
        g_state.oneHandFallbackActive = roles.oneHand;
    }
    const bool suppressGameplay = g_config.hplControllerSuppressDuringAuthoredCamera
        && player.authoredCameraActive;
    if (suppressGameplay) {
        ReleaseGameplayInputs();
    } else {
        ApplyLocomotion(roles);
        ApplyTurn(roles, nowMs);
        ApplyGameplayActions(input, roles);
    }
    ApplySystemActions(input, roles, nowMs);
    g_state.lastTickMs = nowMs;

    if (suppressGameplay != g_state.authoredCameraSuppressed) {
        Logger::Instance().Write(
            suppressGameplay ? LogLevel::Warn : LogLevel::Info,
            "hpl_controller_authored_policy frame=%llu suppressed=%d rotateMode=%d cameraUpdateActive=%d playerState=%d moveState=%d",
            static_cast<unsigned long long>(frameIndex),
            suppressGameplay ? 1 : 0,
            player.cameraRotateMode,
            player.cameraUpdateActive ? 1 : 0,
            player.playerStateId,
            player.moveStateId);
        g_state.authoredCameraSuppressed = suppressGameplay;
    }

    const uint64_t interval = static_cast<uint64_t>(std::max(g_config.hplControllerLogInterval, 1));
    if (frameIndex % interval == 0) {
        Logger::Instance().Write(
            LogLevel::Info,
            "hpl_controller frame=%llu inputFrame=%llu age=%llu move=%.3f,%.3f keys=%d%d%d%d run=%d jump=%d crouch=%d turn=%.3f mode=%s interact=%d menu=%d recenterChord=%d playerState=%d moveState=%d authoredCamera=%d gameplaySuppressed=%d",
            static_cast<unsigned long long>(frameIndex),
            static_cast<unsigned long long>(input.gameFrame),
            static_cast<unsigned long long>(age),
            roles.moveX, roles.moveY,
            g_state.forward.down ? 1 : 0, g_state.backward.down ? 1 : 0,
            g_state.left.down ? 1 : 0, g_state.right.down ? 1 : 0,
            g_state.sprint.down ? 1 : 0, input.jump ? 1 : 0, input.crouch ? 1 : 0,
            roles.turnX, g_config.hplControllerSnapTurn ? "snap" : "smooth",
            g_state.interact.down ? 1 : 0, input.menu ? 1 : 0,
            g_state.recenterStartMs != 0 ? 1 : 0,
            player.playerStateId, player.moveStateId,
            player.authoredCameraActive ? 1 : 0,
            suppressGameplay ? 1 : 0);
    }
}

void RemoveHPLInputBridge()
{
    std::lock_guard lock(g_mutex);
    ReleaseAll();
    g_openxr = nullptr;
    g_state = {};
    Logger::Instance().Write(LogLevel::Info, "hpl_input_bridge removed");
}

void LogHPLInputBridgeSummary()
{
    HPLPlayerStateSnapshot player;
    GetHPLPlayerStateSnapshot(player);
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_input_bridge_summary installed=%d updates=%llu activeUpdates=%llu sentEvents=%llu sendFailures=%llu staleInputFrames=%llu recenterRequests=%llu hapticRequests=%llu hapticApplied=%llu oneHandFallbackFrames=%llu authoredCameraSuppress=%d player=%p camera=%p body=%p playerState=%d moveState=%d",
        g_openxr != nullptr ? 1 : 0,
        static_cast<unsigned long long>(g_updates.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_activeUpdates.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_sentEvents.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_sendFailures.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_staleInputFrames.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_recenterRequests.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_hapticRequests.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_hapticApplied.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_oneHandFallbackFrames.load(std::memory_order_relaxed)),
        g_state.authoredCameraSuppressed ? 1 : 0,
        player.player, player.camera, player.characterBody,
        player.playerStateId, player.moveStateId);
}

} // namespace somavr
