#include "HPLInputBridge.h"

#include "HPLCameraBridge.h"
#include "HPLInputMath.h"
#include "HPLMenuBridge.h"
#include "HPLNativeLocomotion.h"
#include "HPLPhysicalCrouchMath.h"
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
    ButtonState rotate;
    ButtonState sprint;
    bool snapLatched = false;
    bool recenterLatched = false;
    uint64_t recenterStartMs = 0;
    uint64_t lastFrame = 0;
    uint64_t lastTickMs = 0;
    double smoothTurnRemainder = 0.0;
    bool gameplaySuppressed = false;
    bool oneHandFallbackActive = false;
    bool nativeMovementActive = false;
    bool nativeTurnActive = false;
    bool paused = false;
    bool menuPointerActive = false;
    bool menuClickLatchedUntilRelease = false;
    crouch_math::PhysicalCrouchState physicalCrouch{};
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

constexpr int kNormalPlayerState = 0;
constexpr int kGrabPlayerState = 1;
constexpr int kPushPlayerState = 2;
constexpr int kLastPhysicalManipulationState = 7;

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
std::atomic<uint64_t> g_worldAimPoseSamples = 0;
std::atomic<uint64_t> g_worldGripPoseSamples = 0;
std::atomic<uint64_t> g_nativeMovementFrames = 0;
std::atomic<uint64_t> g_nativeTurnEvents = 0;
std::atomic<uint64_t> g_semanticMovementFallbackFrames = 0;
std::atomic<uint64_t> g_flashlightActions = 0;
std::atomic<uint64_t> g_inventoryActions = 0;
std::atomic<uint64_t> g_pausedFrames = 0;
std::atomic<uint64_t> g_menuPointerFrames = 0;
std::atomic<uint64_t> g_headRelativeMovementFrames = 0;
std::atomic<uint64_t> g_physicalCrouchEntries = 0;
std::atomic<uint64_t> g_physicalCrouchExits = 0;
std::atomic<uint64_t> g_manipulationRotateFrames = 0;
std::atomic<uint64_t> g_nativeThrowActions = 0;

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

void ReleaseMovementInputs()
{
    SetKey(g_state.forward, 'W', false);
    SetKey(g_state.backward, 'S', false);
    SetKey(g_state.left, 'A', false);
    SetKey(g_state.right, 'D', false);
}

void SetMiddleMouseButton(ButtonState& state, bool down)
{
    if (state.down == down) return;
    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = down ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP;
    SendInputs(&input, 1);
    state.down = down;
}

void TapMouseButton(DWORD downFlag, DWORD upFlag)
{
    std::array<INPUT, 2> inputs{};
    inputs[0].type = INPUT_MOUSE;
    inputs[0].mi.dwFlags = downFlag;
    inputs[1].type = INPUT_MOUSE;
    inputs[1].mi.dwFlags = upFlag;
    SendInputs(inputs.data(), static_cast<UINT>(inputs.size()));
}

void ReleaseGameplayInputs()
{
    ReleaseMovementInputs();
    SetMouseButton(g_state.interact, false);
    SetMiddleMouseButton(g_state.rotate, false);
    SetKey(g_state.sprint, VK_LSHIFT, false);
    g_state.snapLatched = false;
    g_state.smoothTurnRemainder = 0.0;
}

void ReleaseGameplayExceptPointer()
{
    ReleaseMovementInputs();
    SetKey(g_state.sprint, VK_LSHIFT, false);
    SetMiddleMouseButton(g_state.rotate, false);
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
    DeactivateHPLMenuPointer();
    g_state.menuPointerActive = false;
    g_state.recenterStartMs = 0;
    g_state.recenterLatched = false;
    g_state.menuClickLatchedUntilRelease = false;
}

bool ApplyLocomotion(
    const ControllerRoles& roles,
    const HPLPlayerStateSnapshot& player,
    const HPLCameraBridgeStatus& camera)
{
    input_math::Axis2 movement{roles.moveX, roles.moveY};
    if (g_config.hplControllerMovementReference == "head" && camera.headWorldRotationValid) {
        movement = input_math::ApplyHeadRelativeMovement(
            movement.x,
            movement.y,
            {
                camera.headWorldRotationX,
                camera.headWorldRotationY,
                camera.headWorldRotationZ,
                camera.headWorldRotationW,
            });
        g_headRelativeMovementFrames.fetch_add(1, std::memory_order_relaxed);
    }
    const input_math::Axis2 nativeMove = input_math::ApplyRadialDeadzone(
        movement.x, movement.y, g_config.hplControllerMoveDeadzone);
    if (ApplyHPLNativeMovement(player, nativeMove.x, nativeMove.y))
    {
        ReleaseMovementInputs();
        g_nativeMovementFrames.fetch_add(1, std::memory_order_relaxed);
        return true;
    }

    g_semanticMovementFallbackFrames.fetch_add(1, std::memory_order_relaxed);
    const float press = g_config.hplControllerMoveDeadzone;
    const float release = std::min(g_config.hplControllerMoveReleaseDeadzone, press);
    SetKey(g_state.forward, 'W', UpdateAxisButton(g_state.forward, movement.y, press, release));
    SetKey(g_state.backward, 'S', UpdateAxisButton(g_state.backward, -movement.y, press, release));
    SetKey(g_state.right, 'D', UpdateAxisButton(g_state.right, movement.x, press, release));
    SetKey(g_state.left, 'A', UpdateAxisButton(g_state.left, -movement.x, press, release));
    return false;
}

bool ApplyPausedMenuActions(const OpenXRInputSnapshot& input, const ControllerRoles& roles)
{
    ReleaseGameplayExceptPointer();
    const OpenXRHandInput& dominant = HandInput(input, roles.dominantHand);
    OpenXRHeadPose headPose;
    const bool pointerActive = g_config.hplControllerMenuPointer
        && g_openxr != nullptr
        && g_openxr->GetLatestHeadPose(headPose)
        && UpdateHPLMenuPointer(headPose, dominant.aimPose);
    if (!pointerActive) {
        SetMouseButton(g_state.interact, false);
        DeactivateHPLMenuPointer();
        return false;
    }

    const bool pressed = dominant.select || dominant.trigger >= 0.75f;
    if (pressed && !g_state.interact.down) {
        PulseHaptic(roles.dominantHand, "menu_click");
    }
    if (pressed) g_state.menuClickLatchedUntilRelease = true;
    SetMouseButton(g_state.interact, pressed);
    g_menuPointerFrames.fetch_add(1, std::memory_order_relaxed);
    return true;
}

bool ApplyTurn(const ControllerRoles& roles, const HPLPlayerStateSnapshot& player, uint64_t nowMs)
{
    const float turn = roles.turnX;
    if (g_config.hplControllerSnapTurn) {
        const float releaseThreshold = std::min(
            g_config.hplControllerTurnReleaseDeadzone,
            g_config.hplControllerTurnDeadzone);
        if (!g_state.snapLatched && std::fabs(turn) >= g_config.hplControllerTurnDeadzone) {
            const float direction = turn > 0.0f ? 1.0f : -1.0f;
            const float radians = input_math::DegreesToRadians(
                direction * g_config.hplControllerSnapTurnDegrees * g_config.hplControllerNativeTurnSign);
            const bool nativeTurn = ApplyHPLNativeTurn(player, radians);
            if (!nativeTurn)
                SendMouseMove(turn > 0.0f ? g_config.hplControllerSnapTurnPixels : -g_config.hplControllerSnapTurnPixels);
            else
                g_nativeTurnEvents.fetch_add(1, std::memory_order_relaxed);
            PulseHaptic(roles.turnHand, "snap_turn");
            if (g_openxr != nullptr && g_config.hplControllerComfortBlackoutFrames > 0) {
                g_openxr->RequestComfortBlackout(
                    static_cast<uint32_t>(g_config.hplControllerComfortBlackoutFrames),
                    "snap_turn");
            }
            g_state.snapLatched = true;
            return nativeTurn;
        } else if (g_state.snapLatched && std::fabs(turn) <= releaseThreshold) {
            g_state.snapLatched = false;
        }
        return CanApplyHPLNativeTurn(player);
    }

    const uint64_t elapsedMs = g_state.lastTickMs == 0 ? 0 : std::min<uint64_t>(nowMs - g_state.lastTickMs, 100);
    if (std::fabs(turn) < g_config.hplControllerTurnDeadzone || elapsedMs == 0)
        return CanApplyHPLNativeTurn(player);
    const float radians = input_math::DegreesToRadians(
        turn * g_config.hplControllerSmoothTurnDegreesPerSecond
        * static_cast<float>(elapsedMs) / 1000.0f * g_config.hplControllerNativeTurnSign);
    if (ApplyHPLNativeTurn(player, radians))
    {
        g_state.smoothTurnRemainder = 0.0;
        g_nativeTurnEvents.fetch_add(1, std::memory_order_relaxed);
        return true;
    }
    g_state.smoothTurnRemainder += static_cast<double>(turn)
        * static_cast<double>(g_config.hplControllerSmoothTurnPixelsPerSecond)
        * static_cast<double>(elapsedMs) / 1000.0;
    const int pixels = static_cast<int>(std::trunc(g_state.smoothTurnRemainder));
    g_state.smoothTurnRemainder -= pixels;
    SendMouseMove(pixels);
    return false;
}

void ApplySystemActions(
    const OpenXRInputSnapshot& input,
    const ControllerRoles& roles,
    const HPLPlayerStateSnapshot& player,
    uint64_t nowMs)
{
    if (g_config.hplControllerMenu && input.menu && input.menuChanged) {
        TapKey(VK_ESCAPE);
        PulseHaptic(input.left.active ? 0u : roles.dominantHand, "menu");
    }

    const OpenXRHandInput& dominant = HandInput(input, roles.dominantHand);
    const bool twoHandChord = input.left.squeeze >= 0.85f && input.right.squeeze >= 0.85f;
    const bool oneHandChord = roles.oneHand && dominant.primary && dominant.secondary;
    const bool recenterChord = g_config.hplControllerRecenterChord
        && player.playerStateId == kNormalPlayerState
        && (twoHandChord || oneHandChord);
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

bool ApplyPhysicalCrouch(
    const HPLPlayerStateSnapshot& player,
    const HPLCameraBridgeStatus& camera,
    uint32_t hapticHand)
{
    if (!g_config.hplControllerPhysicalCrouch) return false;
    if (player.playerStateId != kNormalPlayerState || player.moveStateId != 0) return true;

    OpenXRHeadPose head;
    if (g_openxr == nullptr || !g_openxr->GetLatestHeadPose(head)) return false;
    const crouch_math::PhysicalCrouchUpdate update = crouch_math::UpdatePhysicalCrouch(
        g_state.physicalCrouch,
        head.positionY,
        head.valid && head.orientationTracked && head.positionTracked,
        camera.calibrationGeneration,
        g_config.hplControllerPhysicalCrouchEnterMeters,
        g_config.hplControllerPhysicalCrouchExitMeters);
    if (update == crouch_math::PhysicalCrouchUpdate::Invalid) return false;
    if (update == crouch_math::PhysicalCrouchUpdate::Calibrated) {
        Logger::Instance().Write(
            LogLevel::Info,
            "hpl_physical_crouch calibrated standingHeight=%.4f generation=%llu enterDrop=%.3f exitDrop=%.3f",
            g_state.physicalCrouch.standingHeight,
            static_cast<unsigned long long>(camera.calibrationGeneration),
            g_config.hplControllerPhysicalCrouchEnterMeters,
            g_config.hplControllerPhysicalCrouchExitMeters);
    } else if (update == crouch_math::PhysicalCrouchUpdate::Enter) {
        TapKey(VK_LCONTROL);
        PulseHaptic(hapticHand, "physical_crouch_enter");
        g_physicalCrouchEntries.fetch_add(1, std::memory_order_relaxed);
        Logger::Instance().Write(
            LogLevel::Info,
            "hpl_physical_crouch command=enter standingHeight=%.4f headHeight=%.4f",
            g_state.physicalCrouch.standingHeight,
            head.positionY);
    } else if (update == crouch_math::PhysicalCrouchUpdate::Exit) {
        TapKey(VK_LCONTROL);
        PulseHaptic(hapticHand, "physical_crouch_exit");
        g_physicalCrouchExits.fetch_add(1, std::memory_order_relaxed);
        Logger::Instance().Write(
            LogLevel::Info,
            "hpl_physical_crouch command=exit standingHeight=%.4f headHeight=%.4f",
            g_state.physicalCrouch.standingHeight,
            head.positionY);
    }
    return true;
}

void ApplyGameplayActions(
    const OpenXRInputSnapshot& input,
    const ControllerRoles& roles,
    const HPLPlayerStateSnapshot& player,
    const HPLCameraBridgeStatus& camera)
{
    const OpenXRHandInput& dominant = HandInput(input, roles.dominantHand);
    const OpenXRHandInput& support = HandInput(input, roles.supportHand);
    const bool recenterChord = roles.oneHand && dominant.primary && dominant.secondary;
    const bool manipulationState = player.playerStateId >= kGrabPlayerState
        && player.playerStateId <= kLastPhysicalManipulationState;
    const bool throwState = player.playerStateId == kGrabPlayerState
        || player.playerStateId == kPushPlayerState;
    SetKey(
        g_state.sprint,
        VK_LSHIFT,
        !manipulationState && !roles.oneHand && support.trigger >= 0.75f);

    if (g_config.hplControllerManipulationMappings && throwState
        && !recenterChord && dominant.primary && dominant.primaryChanged) {
        SetMouseButton(g_state.interact, false);
        TapMouseButton(MOUSEEVENTF_RIGHTDOWN, MOUSEEVENTF_RIGHTUP);
        g_state.menuClickLatchedUntilRelease = true;
        PulseHaptic(roles.dominantHand, "native_throw");
        g_nativeThrowActions.fetch_add(1, std::memory_order_relaxed);
        Logger::Instance().Write(
            LogLevel::Info,
            "hpl_native_throw requested playerState=%d hand=%s linearVelocityValid=%d linearVelocity=%.4f,%.4f,%.4f angularVelocityValid=%d angularVelocity=%.4f,%.4f,%.4f route=right_mouse_native_action",
            player.playerStateId,
            roles.dominantHand == 0 ? "left" : "right",
            dominant.gripPose.linearVelocityValid ? 1 : 0,
            dominant.gripPose.linearVelocityX,
            dominant.gripPose.linearVelocityY,
            dominant.gripPose.linearVelocityZ,
            dominant.gripPose.angularVelocityValid ? 1 : 0,
            dominant.gripPose.angularVelocityX,
            dominant.gripPose.angularVelocityY,
            dominant.gripPose.angularVelocityZ);
    } else if (!manipulationState && !recenterChord
        && dominant.primary && dominant.primaryChanged) {
        TapKey(VK_SPACE);
        PulseHaptic(roles.dominantHand, "jump");
    }
    const bool physicalCrouchOwns = ApplyPhysicalCrouch(player, camera, roles.dominantHand);
    if (!manipulationState && !physicalCrouchOwns && !recenterChord
        && dominant.secondary && dominant.secondaryChanged) {
        TapKey(VK_LCONTROL);
        PulseHaptic(roles.dominantHand, "crouch");
    }
    const bool rotate = g_config.hplControllerManipulationMappings
        && manipulationState
        && !roles.oneHand
        && support.squeeze >= 0.75f;
    if (rotate && !g_state.rotate.down) PulseHaptic(roles.supportHand, "interaction_rotate");
    SetMiddleMouseButton(g_state.rotate, rotate);
    if (rotate) g_manipulationRotateFrames.fetch_add(1, std::memory_order_relaxed);
    if (!roles.oneHand && g_config.hplControllerFlashlight && support.primary && support.primaryChanged) {
        TapKey('F');
        PulseHaptic(roles.supportHand, "flashlight");
        g_flashlightActions.fetch_add(1, std::memory_order_relaxed);
    }
    if (!roles.oneHand && g_config.hplControllerInventory && support.secondary && support.secondaryChanged) {
        TapKey(VK_TAB);
        PulseHaptic(roles.supportHand, "inventory");
        g_inventoryActions.fetch_add(1, std::memory_order_relaxed);
    }
    if (g_config.hplControllerInteraction) {
        bool interact = dominant.select || dominant.trigger >= 0.75f;
        if (g_state.menuClickLatchedUntilRelease) {
            if (!interact) g_state.menuClickLatchedUntilRelease = false;
            interact = false;
        }
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
        "hpl_input_bridge install_ok enabled=%d moveDeadzone=%.2f nativeLocomotion=%d movementReference=%s physicalCrouch=%d physicalCrouchThresholds=%.3f,%.3f turnMode=%s turnDeadzone=%.2f nativeTurn=%d snapDegrees=%.1f smoothDegreesPerSecond=%.1f nativeTurnSign=%.1f interaction=%d flashlight=%d inventory=%d menu=%d menuPointer=%d recenterChord=%d haptics=%d hapticAmplitude=%.2f hapticDurationMs=%d dominantHand=%s swapSticks=%d oneHandFallback=%d manipulationMappings=%d suppressAuthoredCamera=%d comfortBlackoutFrames=%d maxInputAgeFrames=%d",
        config.hplControllerInput ? 1 : 0,
        config.hplControllerMoveDeadzone,
        config.hplControllerNativeLocomotion ? 1 : 0,
        config.hplControllerMovementReference.c_str(),
        config.hplControllerPhysicalCrouch ? 1 : 0,
        config.hplControllerPhysicalCrouchEnterMeters,
        config.hplControllerPhysicalCrouchExitMeters,
        config.hplControllerSnapTurn ? "snap" : "smooth",
        config.hplControllerTurnDeadzone,
        config.hplControllerNativeTurn ? 1 : 0,
        config.hplControllerSnapTurnDegrees,
        config.hplControllerSmoothTurnDegreesPerSecond,
        config.hplControllerNativeTurnSign,
        config.hplControllerInteraction ? 1 : 0,
        config.hplControllerFlashlight ? 1 : 0,
        config.hplControllerInventory ? 1 : 0,
        config.hplControllerMenu ? 1 : 0,
        config.hplControllerMenuPointer ? 1 : 0,
        config.hplControllerRecenterChord ? 1 : 0,
        config.hplControllerHaptics ? 1 : 0,
        config.hplControllerHapticAmplitude,
        config.hplControllerHapticDurationMs,
        config.hplControllerDominantHand.c_str(),
        config.hplControllerSwapSticks ? 1 : 0,
        config.hplControllerOneHandFallback ? 1 : 0,
        config.hplControllerManipulationMappings ? 1 : 0,
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
    bool paused = false;
    const bool pauseStateValid = GetHPLGamePausedState(paused);
    const bool suppressForAuthoredCamera = g_config.hplControllerSuppressDuringAuthoredCamera
        && player.authoredCameraActive;
    const bool suppressGameplay = suppressForAuthoredCamera || paused;
    const bool previousPaused = g_state.paused;
    const bool previousMenuPointerActive = g_state.menuPointerActive;
    if (paused) {
        g_pausedFrames.fetch_add(1, std::memory_order_relaxed);
        g_state.menuPointerActive = ApplyPausedMenuActions(input, roles);
        g_state.nativeMovementActive = false;
        g_state.nativeTurnActive = false;
    } else if (suppressForAuthoredCamera) {
        ReleaseGameplayInputs();
        DeactivateHPLMenuPointer();
        g_state.menuPointerActive = false;
        g_state.nativeMovementActive = false;
        g_state.nativeTurnActive = false;
    } else {
        DeactivateHPLMenuPointer();
        g_state.menuPointerActive = false;
        const bool nativeMovement = ApplyLocomotion(roles, player, camera);
        const bool nativeTurn = ApplyTurn(roles, player, nowMs);
        if (nativeMovement != g_state.nativeMovementActive || nativeTurn != g_state.nativeTurnActive)
        {
            Logger::Instance().Write(
                LogLevel::Info,
                "hpl_controller_native_route frame=%llu movement=%s turn=%s playerState=%d moveState=%d body=%p",
                static_cast<unsigned long long>(frameIndex),
                nativeMovement ? "native_analog" : "semantic_keys",
                nativeTurn ? "native_radians" : "semantic_mouse",
                player.playerStateId, player.moveStateId, player.characterBody);
        }
        g_state.nativeMovementActive = nativeMovement;
        g_state.nativeTurnActive = nativeTurn;
        ApplyGameplayActions(input, roles, player, camera);
    }
    ApplySystemActions(input, roles, player, nowMs);
    g_state.lastTickMs = nowMs;

    if (suppressGameplay != g_state.gameplaySuppressed
        || paused != previousPaused
        || g_state.menuPointerActive != previousMenuPointerActive) {
        Logger::Instance().Write(
            suppressGameplay ? LogLevel::Warn : LogLevel::Info,
            "hpl_controller_gameplay_policy frame=%llu suppressed=%d authoredCamera=%d paused=%d pauseGateValid=%d menuPointer=%d rotateMode=%d cameraUpdateActive=%d playerState=%d moveState=%d",
            static_cast<unsigned long long>(frameIndex),
            suppressGameplay ? 1 : 0,
            suppressForAuthoredCamera ? 1 : 0,
            paused ? 1 : 0,
            pauseStateValid ? 1 : 0,
            g_state.menuPointerActive ? 1 : 0,
            player.cameraRotateMode,
            player.cameraUpdateActive ? 1 : 0,
            player.playerStateId,
            player.moveStateId);
        g_state.gameplaySuppressed = suppressGameplay;
    }
    g_state.paused = paused;

    const uint64_t interval = static_cast<uint64_t>(std::max(g_config.hplControllerLogInterval, 1));
    if (frameIndex % interval == 0) {
        const OpenXRHandInput& dominant = HandInput(input, roles.dominantHand);
        HPLTrackedPoseWorld worldAim;
        HPLTrackedPoseWorld worldGrip;
        const bool aimValid = ResolveHPLTrackedPoseWorld(dominant.aimPose, input.gameFrame, worldAim);
        const bool gripValid = ResolveHPLTrackedPoseWorld(dominant.gripPose, input.gameFrame, worldGrip);
        if (aimValid) g_worldAimPoseSamples.fetch_add(1, std::memory_order_relaxed);
        if (gripValid) g_worldGripPoseSamples.fetch_add(1, std::memory_order_relaxed);
        Logger::Instance().Write(
            LogLevel::Info,
            "hpl_controller frame=%llu inputFrame=%llu age=%llu dominant=%s oneHand=%d move=%.3f,%.3f movementReference=%s movementRoute=%s keys=%d%d%d%d run=%d jump=%d crouch=%d physicalCrouch=%d rotate=%d turn=%.3f mode=%s turnRoute=%s interact=%d menu=%d recenterChord=%d playerState=%d moveState=%d authoredCamera=%d paused=%d menuPointer=%d gameplaySuppressed=%d worldAim={valid=%d tracked=%d%d pos=%.4f,%.4f,%.4f forward=%.5f,%.5f,%.5f} worldGrip={valid=%d tracked=%d%d pos=%.4f,%.4f,%.4f forward=%.5f,%.5f,%.5f linearVelocityValid=%d linearVelocity=%.4f,%.4f,%.4f angularVelocityValid=%d angularVelocity=%.4f,%.4f,%.4f}",
            static_cast<unsigned long long>(frameIndex),
            static_cast<unsigned long long>(input.gameFrame),
            static_cast<unsigned long long>(age),
            roles.dominantHand == 0 ? "left" : "right",
            roles.oneHand ? 1 : 0,
            roles.moveX, roles.moveY,
            g_config.hplControllerMovementReference.c_str(),
            g_state.nativeMovementActive ? "native_analog" : "semantic_keys",
            g_state.forward.down ? 1 : 0, g_state.backward.down ? 1 : 0,
            g_state.left.down ? 1 : 0, g_state.right.down ? 1 : 0,
            g_state.sprint.down ? 1 : 0, input.jump ? 1 : 0, input.crouch ? 1 : 0,
            g_state.physicalCrouch.crouched ? 1 : 0,
            g_state.rotate.down ? 1 : 0,
            roles.turnX, g_config.hplControllerSnapTurn ? "snap" : "smooth",
            g_state.nativeTurnActive ? "native_radians" : "semantic_mouse",
            g_state.interact.down ? 1 : 0, input.menu ? 1 : 0,
            g_state.recenterStartMs != 0 ? 1 : 0,
            player.playerStateId, player.moveStateId,
            player.authoredCameraActive ? 1 : 0,
            paused ? 1 : 0,
            g_state.menuPointerActive ? 1 : 0,
            suppressGameplay ? 1 : 0,
            aimValid ? 1 : 0,
            worldAim.orientationTracked ? 1 : 0,
            worldAim.positionTracked ? 1 : 0,
            worldAim.positionX, worldAim.positionY, worldAim.positionZ,
            worldAim.forwardX, worldAim.forwardY, worldAim.forwardZ,
            gripValid ? 1 : 0,
            worldGrip.orientationTracked ? 1 : 0,
            worldGrip.positionTracked ? 1 : 0,
            worldGrip.positionX, worldGrip.positionY, worldGrip.positionZ,
            worldGrip.forwardX, worldGrip.forwardY, worldGrip.forwardZ,
            dominant.gripPose.linearVelocityValid ? 1 : 0,
            dominant.gripPose.linearVelocityX,
            dominant.gripPose.linearVelocityY,
            dominant.gripPose.linearVelocityZ,
            dominant.gripPose.angularVelocityValid ? 1 : 0,
            dominant.gripPose.angularVelocityX,
            dominant.gripPose.angularVelocityY,
            dominant.gripPose.angularVelocityZ);
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
        "hpl_input_bridge_summary installed=%d updates=%llu activeUpdates=%llu sentEvents=%llu sendFailures=%llu staleInputFrames=%llu recenterRequests=%llu hapticRequests=%llu hapticApplied=%llu oneHandFallbackFrames=%llu worldAimPoseSamples=%llu worldGripPoseSamples=%llu nativeMovementFrames=%llu headRelativeMovementFrames=%llu nativeTurnEvents=%llu semanticMovementFallbackFrames=%llu physicalCrouchEntries=%llu physicalCrouchExits=%llu manipulationRotateFrames=%llu nativeThrowActions=%llu flashlightActions=%llu inventoryActions=%llu pausedFrames=%llu menuPointerFrames=%llu gameplaySuppressed=%d paused=%d menuPointerActive=%d physicalCrouch=%d rotate=%d player=%p camera=%p body=%p playerState=%d moveState=%d",
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
        static_cast<unsigned long long>(g_worldAimPoseSamples.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_worldGripPoseSamples.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_nativeMovementFrames.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_headRelativeMovementFrames.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_nativeTurnEvents.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_semanticMovementFallbackFrames.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_physicalCrouchEntries.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_physicalCrouchExits.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_manipulationRotateFrames.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_nativeThrowActions.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_flashlightActions.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_inventoryActions.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_pausedFrames.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_menuPointerFrames.load(std::memory_order_relaxed)),
        g_state.gameplaySuppressed ? 1 : 0,
        g_state.paused ? 1 : 0,
        g_state.menuPointerActive ? 1 : 0,
        g_state.physicalCrouch.crouched ? 1 : 0,
        g_state.rotate.down ? 1 : 0,
        player.player, player.camera, player.characterBody,
        player.playerStateId, player.moveStateId);
}

} // namespace somavr
