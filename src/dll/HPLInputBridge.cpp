#include "HPLInputBridge.h"

#include "HPLCameraBridge.h"
#include "HPLComfortMath.h"
#include "HPLGrabBridge.h"
#include "HPLInputMath.h"
#include "HPLInteractionBridge.h"
#include "HPLMenuBridge.h"
#include "HPLNativeLocomotion.h"
#include "HPLPhysicalCrouchMath.h"
#include "HPLPlayerState.h"
#include "HPLStatusPanelBridge.h"
#include "HPLPresentationBridge.h"
#include "HPLTerminalBridge.h"
#include "Logger.h"
#include "OpenXRComfortVignetteMath.h"

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
    ButtonState cancel;
    ButtonState sprint;
    bool snapLatched = false;
    bool recenterLatched = false;
    uint64_t recenterStartMs = 0;
    uint64_t lastFrame = 0;
    uint64_t lastTickMs = 0;
    double smoothTurnRemainder = 0.0;
    bool manipulationMotionActive = false;
    int manipulationMotionState = -1;
    camera_math::Vector3 manipulationHandRelativePosition{};
    camera_math::Quaternion manipulationGripLocalOrientation{};
    input_math::ManipulationMotionState manipulationMotionAccumulator{};
    input_math::ManipulationMotionState manipulationRotationAccumulator{};
    int manipulationMotionLastX = 0;
    int manipulationMotionLastY = 0;
    uint64_t manipulationMotionStartFrame = 0;
    uint32_t manipulationMotionHand = 1;
    uint64_t manipulationMotionSessionFrames = 0;
    uint64_t manipulationMotionSessionEvents = 0;
    float manipulationMotionSessionRightMeters = 0.0f;
    float manipulationMotionSessionUpMeters = 0.0f;
    float manipulationMotionSessionYawRadians = 0.0f;
    float manipulationMotionSessionPitchRadians = 0.0f;
    float manipulationMotionSessionAbsoluteMeters = 0.0f;
    float manipulationMotionSessionMaxMeters = 0.0f;
    int64_t manipulationMotionSessionPixelsX = 0;
    int64_t manipulationMotionSessionPixelsY = 0;
    bool gameplaySuppressed = false;
    bool oneHandFallbackActive = false;
    bool nativeMovementActive = false;
    bool nativeTurnActive = false;
    bool paused = false;
    bool menuPointerActive = false;
    bool terminalPointerActive = false;
    uint32_t terminalPointerHand = 1;
    bool menuClickLatchedUntilRelease = false;
    bool readRotateLatched = false;
    crouch_math::PhysicalCrouchState physicalCrouch{};
    bool playerStateInitialized = false;
    int lastPlayerState = -1;
    bool authoredCameraInitialized = false;
    bool lastAuthoredCameraActive = false;
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
constexpr int kWheelPlayerState = 3;
constexpr int kSlidePlayerState = 4;
constexpr int kLastPhysicalManipulationState = 7;
constexpr int kTerminalPlayerState = 8;
constexpr int kHandheldTerminalPlayerState = 9;
constexpr int kReadPlayerState = 10;
constexpr int kDeadPlayerState = 17;
constexpr int kZoomAreaPlayerState = 18;

Config g_config;
OpenXRRuntime* g_openxr = nullptr;
BridgeState g_state;
std::mutex g_mutex;
std::atomic<uint64_t> g_updates = 0;
std::atomic<uint64_t> g_activeUpdates = 0;
std::atomic<uint64_t> g_sentEvents = 0;
std::atomic<uint64_t> g_sendFailures = 0;
std::atomic<uint64_t> g_staleInputFrames = 0;
std::atomic<uint64_t> g_loadingSuppressedFrames = 0;
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
std::atomic<uint64_t> g_terminalPointerFrames = 0;
std::atomic<uint64_t> g_gameOverContinueActions = 0;
std::atomic<uint64_t> g_headRelativeMovementFrames = 0;
std::atomic<uint64_t> g_controllerRelativeMovementFrames = 0;
std::atomic<uint64_t> g_controllerReferenceAttempts = 0;
std::atomic<uint64_t> g_controllerReferenceApplied = 0;
std::atomic<uint64_t> g_controllerReferenceFallbacks = 0;
std::atomic<uint64_t> g_controllerDirectionSamples = 0;
std::atomic<uint64_t> g_controllerAimGuideFrames = 0;
std::atomic<uint64_t> g_inspectionExitActions = 0;
std::atomic<uint64_t> g_physicalCrouchEntries = 0;
std::atomic<uint64_t> g_physicalCrouchExits = 0;
std::atomic<uint64_t> g_manipulationRotateFrames = 0;
std::atomic<uint64_t> g_manipulationMotionFrames = 0;
std::atomic<uint64_t> g_manipulationMotionEntries = 0;
std::atomic<uint64_t> g_manipulationMotionEvents = 0;
std::atomic<uint64_t> g_manipulationMotionTrackingLosses = 0;
std::atomic<int64_t> g_manipulationMotionPixelsX = 0;
std::atomic<int64_t> g_manipulationMotionPixelsY = 0;
std::atomic<uint64_t> g_manipulationMotionSessionSummaries = 0;
std::atomic<uint64_t> g_nativeThrowActions = 0;
std::atomic<uint64_t> g_playerStateTransitions = 0;
std::atomic<uint64_t> g_playerStateBlackouts = 0;
std::atomic<uint64_t> g_authoredCameraTransitions = 0;

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

void SendMouseMove(int dx, int dy = 0)
{
    if (dx == 0 && dy == 0) return;
    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dx = dx;
    input.mi.dy = dy;
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

const char* PhysicalManipulationStateName(int state)
{
    switch (state) {
    case 3: return "wheel";
    case 4: return "slide";
    case 5: return "swing_door";
    case 6: return "lever";
    case 7: return "tear";
    case 10: return "read";
    default: return "none";
    }
}

const char* PlayerStateName(int state)
{
    switch (state) {
    case 0: return "normal";
    case 1: return "grab";
    case 2: return "push";
    case 3: return "wheel";
    case 4: return "slide";
    case 5: return "swing_door";
    case 6: return "lever";
    case 7: return "tear";
    case 8: return "terminal";
    case 9: return "handheld_terminal";
    case 10: return "read";
    case 11: return "ladder";
    case 12: return "climb_ledge";
    case 13: return "moving_button";
    case 14: return "interactive_camera_animation";
    case 15: return "sit";
    case 16: return "conversation";
    case 17: return "dead";
    case 18: return "zoom_area";
    case 19: return "custom_controls";
    case 20: return "null";
    default: return "unknown";
    }
}

void ApplyStateTransitionComfort(
    const HPLPlayerStateSnapshot& player,
    const HPLCameraBridgeStatus& camera,
    uint64_t frameIndex)
{
    if (!player.playerValid || player.playerStateId < 0) {
        g_state.playerStateInitialized = false;
        g_state.lastPlayerState = -1;
        g_state.authoredCameraInitialized = false;
        g_state.lastAuthoredCameraActive = false;
        return;
    }

    bool stateChanged = false;
    int previousState = player.playerStateId;
    if (!g_state.playerStateInitialized) {
        g_state.playerStateInitialized = true;
        g_state.lastPlayerState = player.playerStateId;
    } else if (g_state.lastPlayerState != player.playerStateId) {
        stateChanged = true;
        previousState = g_state.lastPlayerState;
        g_state.lastPlayerState = player.playerStateId;
    }

    bool authoredCameraChanged = false;
    bool previousAuthoredCameraActive = player.authoredCameraActive;
    if (!player.cameraControlValid) {
        g_state.authoredCameraInitialized = false;
    } else if (!g_state.authoredCameraInitialized) {
        g_state.authoredCameraInitialized = true;
        g_state.lastAuthoredCameraActive = player.authoredCameraActive;
    } else if (g_state.lastAuthoredCameraActive != player.authoredCameraActive) {
        authoredCameraChanged = true;
        previousAuthoredCameraActive = g_state.lastAuthoredCameraActive;
        g_state.lastAuthoredCameraActive = player.authoredCameraActive;
    }

    if (!stateChanged && !authoredCameraChanged) return;

    const uint64_t stateTransition = stateChanged
        ? g_playerStateTransitions.fetch_add(1, std::memory_order_relaxed) + 1
        : g_playerStateTransitions.load(std::memory_order_relaxed);
    const uint64_t ownershipTransition = authoredCameraChanged
        ? g_authoredCameraTransitions.fetch_add(1, std::memory_order_relaxed) + 1
        : g_authoredCameraTransitions.load(std::memory_order_relaxed);
    const bool highMotionStateTransition = stateChanged
        && comfort_math::ShouldBlackoutPlayerStateTransition(previousState, player.playerStateId);
    const bool authoredCameraTransition = authoredCameraChanged
        && comfort_math::ShouldBlackoutAuthoredCameraTransition(
            previousAuthoredCameraActive, player.authoredCameraActive);
    const bool requested = (highMotionStateTransition || authoredCameraTransition)
        && camera.trackingEnabled
        && g_openxr != nullptr
        && g_config.hplControllerStateTransitionBlackoutFrames > 0;
    if (requested) {
        g_openxr->RequestComfortBlackout(
            static_cast<uint32_t>(g_config.hplControllerStateTransitionBlackoutFrames),
            authoredCameraTransition ? "authored_camera_transition" : "player_state_transition");
        g_playerStateBlackouts.fetch_add(1, std::memory_order_relaxed);
    }
    Logger::Instance().Write(
        requested ? LogLevel::Warn : LogLevel::Info,
        "hpl_player_state_comfort frame=%llu stateTransition=%llu ownershipTransition=%llu stateChanged=%d previous=%s(%d) current=%s(%d) highMotion=%d authoredCameraChanged=%d authoredPrevious=%d authoredCurrent=%d tracking=%d blackoutRequested=%d blackoutFrames=%d",
        static_cast<unsigned long long>(frameIndex),
        static_cast<unsigned long long>(stateTransition),
        static_cast<unsigned long long>(ownershipTransition),
        stateChanged ? 1 : 0,
        PlayerStateName(previousState),
        previousState,
        PlayerStateName(player.playerStateId),
        player.playerStateId,
        highMotionStateTransition ? 1 : 0,
        authoredCameraChanged ? 1 : 0,
        previousAuthoredCameraActive ? 1 : 0,
        player.authoredCameraActive ? 1 : 0,
        camera.trackingEnabled ? 1 : 0,
        requested ? 1 : 0,
        g_config.hplControllerStateTransitionBlackoutFrames);
}

void ResetManipulationMotion(const char* reason = "reset")
{
    if (g_state.manipulationMotionActive) {
        const uint64_t summary = g_manipulationMotionSessionSummaries.fetch_add(
            1, std::memory_order_relaxed) + 1;
        Logger::Instance().Write(
            LogLevel::Info,
            "hpl_manipulation_session summary=%llu reason=%s state=%s(%d) hand=%s startInputFrame=%llu frames=%llu events=%llu displacementMeters={signed=%.5f,%.5f absolute=%.5f maxFrame=%.5f} rotationRadians={yaw=%.5f pitch=%.5f} mousePixels=%lld,%lld",
            static_cast<unsigned long long>(summary),
            reason != nullptr ? reason : "reset",
            PhysicalManipulationStateName(g_state.manipulationMotionState),
            g_state.manipulationMotionState,
            g_state.manipulationMotionHand == 0 ? "left" : "right",
            static_cast<unsigned long long>(g_state.manipulationMotionStartFrame),
            static_cast<unsigned long long>(g_state.manipulationMotionSessionFrames),
            static_cast<unsigned long long>(g_state.manipulationMotionSessionEvents),
            g_state.manipulationMotionSessionRightMeters,
            g_state.manipulationMotionSessionUpMeters,
            g_state.manipulationMotionSessionAbsoluteMeters,
            g_state.manipulationMotionSessionMaxMeters,
            g_state.manipulationMotionSessionYawRadians,
            g_state.manipulationMotionSessionPitchRadians,
            static_cast<long long>(g_state.manipulationMotionSessionPixelsX),
            static_cast<long long>(g_state.manipulationMotionSessionPixelsY));
    }
    g_state.manipulationMotionActive = false;
    g_state.manipulationMotionState = -1;
    g_state.manipulationHandRelativePosition = {};
    g_state.manipulationGripLocalOrientation = {};
    g_state.manipulationMotionAccumulator = {};
    g_state.manipulationRotationAccumulator = {};
    g_state.manipulationMotionLastX = 0;
    g_state.manipulationMotionLastY = 0;
    g_state.manipulationMotionStartFrame = 0;
    g_state.manipulationMotionHand = 1;
    g_state.manipulationMotionSessionFrames = 0;
    g_state.manipulationMotionSessionEvents = 0;
    g_state.manipulationMotionSessionRightMeters = 0.0f;
    g_state.manipulationMotionSessionUpMeters = 0.0f;
    g_state.manipulationMotionSessionYawRadians = 0.0f;
    g_state.manipulationMotionSessionPitchRadians = 0.0f;
    g_state.manipulationMotionSessionAbsoluteMeters = 0.0f;
    g_state.manipulationMotionSessionMaxMeters = 0.0f;
    g_state.manipulationMotionSessionPixelsX = 0;
    g_state.manipulationMotionSessionPixelsY = 0;
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

void SetRightMouseButton(ButtonState& state, bool down)
{
    if (state.down == down) return;
    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = down ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;
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
    SetRightMouseButton(g_state.cancel, false);
    SetKey(g_state.sprint, VK_LSHIFT, false);
    g_state.snapLatched = false;
    g_state.smoothTurnRemainder = 0.0;
    ResetManipulationMotion();
    g_state.readRotateLatched = false;
}

void ReleaseGameplayExceptPointer()
{
    ReleaseMovementInputs();
    SetKey(g_state.sprint, VK_LSHIFT, false);
    SetMiddleMouseButton(g_state.rotate, false);
    SetRightMouseButton(g_state.cancel, false);
    g_state.snapLatched = false;
    g_state.smoothTurnRemainder = 0.0;
    ResetManipulationMotion();
}

const OpenXRHandInput& HandInput(const OpenXRInputSnapshot& input, uint32_t hand)
{
    return hand == 0 ? input.left : input.right;
}

bool InteractionPressed(const OpenXRHandInput& hand)
{
    return hand.active && (hand.select || hand.trigger >= 0.75f);
}

uint32_t ResolveInteractionActionHand(
    const OpenXRInputSnapshot& input,
    const ControllerRoles& roles)
{
    const bool leftPressed = InteractionPressed(input.left);
    const bool rightPressed = InteractionPressed(input.right);
    if (leftPressed != rightPressed) return leftPressed ? 0u : 1u;

    uint32_t owner = roles.dominantHand;
    if (GetHPLInteractionOwnerHand(input.gameFrame, 120, owner)
        && HandInput(input, owner).active) {
        return owner;
    }
    return roles.dominantHand;
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
    DeactivateHPLTerminalPointer();
    g_state.menuPointerActive = false;
    g_state.terminalPointerActive = false;
    g_state.recenterStartMs = 0;
    g_state.recenterLatched = false;
    g_state.menuClickLatchedUntilRelease = false;
    if (g_openxr != nullptr) g_openxr->ClearControllerAimGuide();
}

bool ApplyLocomotion(
    const OpenXRInputSnapshot& input,
    const ControllerRoles& roles,
    const HPLPlayerStateSnapshot& player,
    const HPLCameraBridgeStatus& camera)
{
    input_math::Axis2 movement{roles.moveX, roles.moveY};
    const input_math::Axis2 rawMovement = movement;
    bool controllerReferenceAttempted = false;
    bool controllerReferenceApplied = false;
    uint64_t controllerReferenceFallback = 0;
    const char* controllerReferenceSource = "none";
    camera_math::Quaternion rawHeadOrientation{};
    camera_math::Quaternion headWorld{};
    camera_math::Quaternion controllerOrientation{};
    camera_math::Quaternion controllerRelative{};
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
    } else if (g_config.hplControllerMovementReference == "controller") {
        controllerReferenceAttempted = true;
        g_controllerReferenceAttempts.fetch_add(1, std::memory_order_relaxed);
        const OpenXRHandInput& movementHand = HandInput(input, roles.movementHand);
        const bool useAim = movementHand.aimPose.valid
            && movementHand.aimPose.orientationTracked;
        const OpenXRControllerPose& reference = useAim
            ? movementHand.aimPose : movementHand.gripPose;
        controllerReferenceSource = useAim ? "aim" : "grip";
        OpenXRHeadPose rawHead;
        if (reference.valid && reference.orientationTracked
            && camera.headWorldRotationValid
            && g_openxr != nullptr
            && g_openxr->GetLatestHeadPose(rawHead)
            && rawHead.valid
            && rawHead.orientationTracked) {
            headWorld = {
                camera.headWorldRotationX,
                camera.headWorldRotationY,
                camera.headWorldRotationZ,
                camera.headWorldRotationW,
            };
            rawHeadOrientation = {
                rawHead.orientationX,
                rawHead.orientationY,
                rawHead.orientationZ,
                rawHead.orientationW,
            };
            controllerOrientation = {
                reference.orientationX,
                reference.orientationY,
                reference.orientationZ,
                reference.orientationW,
            };
            const camera_math::Quaternion neutralInverse = camera_math::Multiply(
                headWorld, camera_math::Conjugate(rawHeadOrientation));
            controllerRelative = camera_math::Multiply(
                neutralInverse, controllerOrientation);
            movement = input_math::ApplyHeadRelativeMovement(
                movement.x,
                movement.y,
                controllerRelative);
            g_controllerRelativeMovementFrames.fetch_add(1, std::memory_order_relaxed);
            g_controllerReferenceApplied.fetch_add(1, std::memory_order_relaxed);
            controllerReferenceApplied = true;
        } else {
            controllerReferenceFallback = g_controllerReferenceFallbacks.fetch_add(
                1, std::memory_order_relaxed) + 1;
        }
    }
    const input_math::Axis2 nativeMove = input_math::ApplyRadialDeadzone(
        movement.x, movement.y, g_config.hplControllerMoveDeadzone);
    const float rawMagnitude = std::sqrt(
        rawMovement.x * rawMovement.x + rawMovement.y * rawMovement.y);
    if (controllerReferenceAttempted
        && rawMagnitude >= g_config.hplControllerMoveDeadzone) {
        const uint64_t sample = g_controllerDirectionSamples.fetch_add(
            1, std::memory_order_relaxed) + 1;
        const uint64_t interval = static_cast<uint64_t>(
            std::max(g_config.hplControllerLogInterval, 1));
        const bool boundedFallback = !controllerReferenceApplied
            && (controllerReferenceFallback <= 8
                || controllerReferenceFallback % interval == 0);
        if (sample <= 12 || sample % interval == 0 || boundedFallback) {
            Logger::Instance().Write(
                controllerReferenceApplied ? LogLevel::Info : LogLevel::Warn,
                "hpl_controller_direction sample=%llu inputFrame=%llu hand=%s source=%s applied=%d rawStick=%.4f,%.4f transformed=%.4f,%.4f deadzoned=%.4f,%.4f headWorldQuat=%.5f,%.5f,%.5f,%.5f rawHeadQuat=%.5f,%.5f,%.5f,%.5f controllerQuat=%.5f,%.5f,%.5f,%.5f relativeQuat=%.5f,%.5f,%.5f,%.5f nativeCameraBasisValid=%d nativeCameraForward=%.5f,%.5f,%.5f",
                static_cast<unsigned long long>(sample),
                static_cast<unsigned long long>(input.gameFrame),
                roles.movementHand == 0 ? "left" : "right",
                controllerReferenceSource,
                controllerReferenceApplied ? 1 : 0,
                rawMovement.x, rawMovement.y,
                movement.x, movement.y,
                nativeMove.x, nativeMove.y,
                headWorld.x, headWorld.y, headWorld.z, headWorld.w,
                rawHeadOrientation.x, rawHeadOrientation.y,
                rawHeadOrientation.z, rawHeadOrientation.w,
                controllerOrientation.x, controllerOrientation.y,
                controllerOrientation.z, controllerOrientation.w,
                controllerRelative.x, controllerRelative.y,
                controllerRelative.z, controllerRelative.w,
                camera.nativeCameraBasisValid ? 1 : 0,
                camera.nativeCameraForwardX, camera.nativeCameraForwardY,
                camera.nativeCameraForwardZ);
        }
    }
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

bool ApplyTerminalPointerActions(const OpenXRInputSnapshot& input, const ControllerRoles& roles)
{
    ReleaseGameplayExceptPointer();
    DeactivateHPLMenuPointer();
    const bool leftPressed = InteractionPressed(input.left);
    const bool rightPressed = InteractionPressed(input.right);
    uint32_t pointerHand = g_state.terminalPointerActive
        ? g_state.terminalPointerHand : roles.dominantHand;
    if (leftPressed != rightPressed) pointerHand = leftPressed ? 0u : 1u;
    if (!HandInput(input, pointerHand).active) pointerHand = roles.dominantHand;
    g_state.terminalPointerHand = pointerHand;
    const OpenXRHandInput& hand = HandInput(input, pointerHand);
    if (hand.secondary && hand.secondaryChanged) {
        TapMouseButton(MOUSEEVENTF_RIGHTDOWN, MOUSEEVENTF_RIGHTUP);
        PulseHaptic(pointerHand, "inspection_exit");
        g_inspectionExitActions.fetch_add(1, std::memory_order_relaxed);
    }
    OpenXRHeadPose headPose;
    const bool pointerActive = g_config.hplControllerTerminalPointer
        && g_openxr != nullptr
        && g_openxr->GetLatestHeadPose(headPose)
        && UpdateHPLTerminalPointer(headPose, hand.aimPose, input.gameFrame);
    if (!pointerActive) {
        SetMouseButton(g_state.interact, false);
        DeactivateHPLTerminalPointer();
        return false;
    }

    const bool pressed = InteractionPressed(hand);
    if (pressed && !g_state.interact.down) {
        PulseHaptic(pointerHand, "terminal_click");
    }
    SetMouseButton(g_state.interact, pressed);
    g_terminalPointerFrames.fetch_add(1, std::memory_order_relaxed);
    return true;
}

void UpdateControllerAimGuide(
    const OpenXRInputSnapshot& input,
    const ControllerRoles& roles,
    bool visible,
    float lengthMeters = 0.0f)
{
    if (g_openxr == nullptr || !g_config.hplControllerAimGuide || !visible) {
        if (g_openxr != nullptr) g_openxr->ClearControllerAimGuide();
        return;
    }
    HPLInteractionHitSnapshot hit;
    GetHPLInteractionHitSnapshot(hit);
    for (uint32_t handIndex = 0; handIndex < 2; ++handIndex) {
        if (!g_config.hplControllerInteractionBothHands
            && handIndex != roles.dominantHand) {
            g_openxr->ClearControllerAimGuide(handIndex);
            continue;
        }
        const OpenXRHandInput& hand = HandInput(input, handIndex);
        OpenXRControllerAimGuideState guide;
        guide.valid = hand.active
            && hand.aimPose.valid
            && hand.aimPose.orientationTracked
            && hand.aimPose.positionTracked;
        guide.gameFrame = input.gameFrame;
        guide.handIndex = handIndex;
        guide.lengthMeters = lengthMeters > 0.0f
            ? lengthMeters
            : g_config.hplControllerAimGuideLengthMeters;
        if (hit.valid && hit.handIndex == handIndex
            && input.gameFrame >= hit.gameFrame
            && input.gameFrame - hit.gameFrame <= 4) {
            guide.lengthMeters = std::clamp(
                hit.distance / std::max(g_config.hplWorldScale, 0.001f),
                0.3f,
                20.0f);
        }
        guide.aimPose = hand.aimPose;
        if (guide.valid) {
            g_openxr->SetControllerAimGuide(guide);
            g_controllerAimGuideFrames.fetch_add(1, std::memory_order_relaxed);
        } else {
            g_openxr->ClearControllerAimGuide(handIndex);
        }
    }
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

void ApplyControllerManipulationMotion(
    const OpenXRInputSnapshot& input,
    const ControllerRoles& roles,
    const HPLPlayerStateSnapshot& player)
{
    const OpenXRHandInput& dominant = HandInput(input, roles.dominantHand);
    const bool physicalState = player.playerStateId >= kWheelPlayerState
        && player.playerStateId <= kLastPhysicalManipulationState
        && !(player.playerStateId == kSlidePlayerState
            && g_config.hplControllerSlideDirectVelocity)
        && !((player.playerStateId == 5 || player.playerStateId == 6)
            && g_config.hplControllerRotateDirectVelocity);
    if (player.playerStateId != kReadPlayerState) {
        g_state.readRotateLatched = false;
    } else if (g_state.readRotateLatched) {
        g_state.readRotateLatched = dominant.squeeze >= 0.55f;
    } else {
        g_state.readRotateLatched = dominant.squeeze >= 0.75f;
    }
    const bool inspectionState = player.playerStateId == kReadPlayerState
        && g_state.readRotateLatched
        && !g_config.hplControllerReadPresentation;
    if (!g_config.hplControllerManipulationMotion
        || (!physicalState && !inspectionState)
        || g_openxr == nullptr) {
        ResetManipulationMotion("state_or_config_inactive");
        return;
    }

    OpenXRHeadPose head;
    if (!dominant.active || !dominant.gripPose.valid
        || !dominant.gripPose.orientationTracked || !dominant.gripPose.positionTracked
        || !g_openxr->GetLatestHeadPose(head) || !head.valid
        || !head.orientationTracked || !head.positionTracked
        || head.sampleAgeFrames > static_cast<uint64_t>(g_config.hplControllerMaxInputAgeFrames)) {
        if (g_state.manipulationMotionActive) {
            g_manipulationMotionTrackingLosses.fetch_add(1, std::memory_order_relaxed);
            Logger::Instance().Write(
                LogLevel::Warn,
                "hpl_manipulation_motion tracking_lost state=%s(%d) hand=%s inputFrame=%llu",
                PhysicalManipulationStateName(player.playerStateId),
                player.playerStateId,
                roles.dominantHand == 0 ? "left" : "right",
                static_cast<unsigned long long>(input.gameFrame));
        }
        ResetManipulationMotion("tracking_lost");
        return;
    }

    const camera_math::Vector3 handRelativePosition{
        dominant.gripPose.positionX - head.positionX,
        dominant.gripPose.positionY - head.positionY,
        dominant.gripPose.positionZ - head.positionZ,
    };
    const camera_math::Quaternion headOrientation{
        head.orientationX,
        head.orientationY,
        head.orientationZ,
        head.orientationW,
    };
    const camera_math::Quaternion gripOrientation{
        dominant.gripPose.orientationX,
        dominant.gripPose.orientationY,
        dominant.gripPose.orientationZ,
        dominant.gripPose.orientationW,
    };
    const camera_math::Quaternion gripLocalOrientation = camera_math::Normalize(gripOrientation);
    if (!g_state.manipulationMotionActive
        || g_state.manipulationMotionState != player.playerStateId) {
        ResetManipulationMotion("state_changed");
        g_state.manipulationMotionActive = true;
        g_state.manipulationMotionState = player.playerStateId;
        g_state.manipulationHandRelativePosition = handRelativePosition;
        g_state.manipulationGripLocalOrientation = gripLocalOrientation;
        g_state.manipulationMotionStartFrame = input.gameFrame;
        g_state.manipulationMotionHand = roles.dominantHand;
        g_manipulationMotionEntries.fetch_add(1, std::memory_order_relaxed);
        Logger::Instance().Write(
            LogLevel::Info,
            "hpl_manipulation_motion entered state=%s(%d) hand=%s inputFrame=%llu relativePosition=%.4f,%.4f,%.4f route=%s",
            PhysicalManipulationStateName(player.playerStateId),
            player.playerStateId,
            roles.dominantHand == 0 ? "left" : "right",
            static_cast<unsigned long long>(input.gameFrame),
            handRelativePosition.x,
            handRelativePosition.y,
            handRelativePosition.z,
            player.playerStateId == kReadPlayerState
                ? "reference_space_grip_orientation_to_native_look"
                : "head_relative_grip_translation_to_native_look");
        return;
    }

    input_math::ManipulationMouseDelta motion;
    input_math::ManipulationRotationDelta rotation;
    if (player.playerStateId == kReadPlayerState) {
        rotation = input_math::ComputeManipulationRotationDelta(
            g_state.manipulationGripLocalOrientation,
            gripLocalOrientation,
            g_config.hplControllerManipulationReadPixelsPerRadian,
            g_config.hplControllerManipulationMotionMaxPixelsPerFrame,
            g_config.hplControllerManipulationMotionHorizontalSign,
            g_config.hplControllerManipulationMotionVerticalSign,
            g_state.manipulationRotationAccumulator);
    } else {
        const float pixelsPerMeter = player.playerStateId == kSlidePlayerState
            ? g_config.hplControllerManipulationSlidePixelsPerMeter
            : g_config.hplControllerManipulationMotionPixelsPerMeter;
        motion = input_math::ComputeManipulationMouseDelta(
            g_state.manipulationHandRelativePosition,
            handRelativePosition,
            headOrientation,
            pixelsPerMeter,
            g_config.hplControllerManipulationMotionDeadzoneMeters,
            g_config.hplControllerManipulationMotionMaxPixelsPerFrame,
            g_config.hplControllerManipulationMotionHorizontalSign,
            g_config.hplControllerManipulationMotionVerticalSign,
            g_state.manipulationMotionAccumulator);
    }
    const float motionMagnitude = std::sqrt(
        motion.rightMeters * motion.rightMeters + motion.upMeters * motion.upMeters);
    if (std::isfinite(motionMagnitude)
        && motionMagnitude > g_config.hplControllerManipulationMotionDeadzoneMeters) {
        g_state.manipulationHandRelativePosition = handRelativePosition;
        g_state.manipulationMotionSessionRightMeters += motion.rightMeters;
        g_state.manipulationMotionSessionUpMeters += motion.upMeters;
        g_state.manipulationMotionSessionAbsoluteMeters += motionMagnitude;
        g_state.manipulationMotionSessionMaxMeters = std::max(
            g_state.manipulationMotionSessionMaxMeters, motionMagnitude);
    }
    g_state.manipulationGripLocalOrientation = gripLocalOrientation;
    g_state.manipulationMotionSessionYawRadians += rotation.yawRadians;
    g_state.manipulationMotionSessionPitchRadians += rotation.pitchRadians;
    const int outputX = rotation.x != 0 || rotation.y != 0 ? rotation.x : motion.x;
    const int outputY = rotation.x != 0 || rotation.y != 0 ? rotation.y : motion.y;
    g_state.manipulationMotionLastX = outputX;
    g_state.manipulationMotionLastY = outputY;
    ++g_state.manipulationMotionSessionFrames;
    g_manipulationMotionFrames.fetch_add(1, std::memory_order_relaxed);
    if (outputX == 0 && outputY == 0) return;

    SendMouseMove(outputX, outputY);
    const uint64_t event = g_manipulationMotionEvents.fetch_add(1, std::memory_order_relaxed) + 1;
    g_manipulationMotionPixelsX.fetch_add(outputX, std::memory_order_relaxed);
    g_manipulationMotionPixelsY.fetch_add(outputY, std::memory_order_relaxed);
    ++g_state.manipulationMotionSessionEvents;
    g_state.manipulationMotionSessionPixelsX += outputX;
    g_state.manipulationMotionSessionPixelsY += outputY;
    const uint64_t interval = static_cast<uint64_t>(std::max(g_config.hplControllerLogInterval, 1));
    if (event <= 16 || event % interval == 0) {
        Logger::Instance().Write(
            LogLevel::Info,
            "hpl_manipulation_motion event=%llu state=%s(%d) hand=%s inputFrame=%llu displacementMeters=%.5f,%.5f rotationRadians=%.5f,%.5f mouseDelta=%d,%d scale=%.1f signs=%.1f,%.1f",
            static_cast<unsigned long long>(event),
            PhysicalManipulationStateName(player.playerStateId),
            player.playerStateId,
            roles.dominantHand == 0 ? "left" : "right",
            static_cast<unsigned long long>(input.gameFrame),
            motion.rightMeters,
            motion.upMeters,
            rotation.yawRadians,
            rotation.pitchRadians,
            outputX,
            outputY,
            player.playerStateId == kReadPlayerState
                ? g_config.hplControllerManipulationReadPixelsPerRadian
                : g_config.hplControllerManipulationMotionPixelsPerMeter,
            g_config.hplControllerManipulationMotionHorizontalSign,
            g_config.hplControllerManipulationMotionVerticalSign);
    }
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
    const bool inspectionState = player.playerStateId == kReadPlayerState
        || player.playerStateId == kZoomAreaPlayerState;
    const bool throwState = player.playerStateId == kGrabPlayerState
        || player.playerStateId == kPushPlayerState;
    SetKey(
        g_state.sprint,
        VK_LSHIFT,
        !manipulationState && !roles.oneHand && support.trigger >= 0.75f);

    if (g_config.hplControllerManipulationMappings && throwState
        && !recenterChord && dominant.primary && dominant.primaryChanged) {
        SetMouseButton(g_state.interact, false);
        if (player.playerStateId == kGrabPlayerState) {
            ArmHPLControllerThrow(dominant.gripPose, input.gameFrame);
        }
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
    } else if (!manipulationState && !inspectionState && !recenterChord
        && dominant.primary && dominant.primaryChanged) {
        TapKey(VK_SPACE);
        PulseHaptic(roles.dominantHand, "jump");
    }
    const bool physicalCrouchOwns = ApplyPhysicalCrouch(player, camera, roles.dominantHand);
    const bool inspectionExitHeld = input.right.primary || input.right.secondary;
    const bool inspectionExitPressed = (input.right.primary && input.right.primaryChanged)
        || (input.right.secondary && input.right.secondaryChanged);
    if (inspectionState && !recenterChord && inspectionExitPressed) {
        SetMouseButton(g_state.interact, false);
        g_state.menuClickLatchedUntilRelease = true;
        PulseHaptic(1, "inspection_exit");
        g_inspectionExitActions.fetch_add(1, std::memory_order_relaxed);
    } else if (!manipulationState && !inspectionState && !physicalCrouchOwns && !recenterChord
        && dominant.secondary && dominant.secondaryChanged) {
        TapKey(VK_LCONTROL);
        PulseHaptic(roles.dominantHand, "crouch");
    }
    SetRightMouseButton(
        g_state.cancel,
        inspectionState && !recenterChord && inspectionExitHeld);
    const bool rotate = g_config.hplControllerManipulationMappings
        && (manipulationState || player.playerStateId == kReadPlayerState)
        && (player.playerStateId == kReadPlayerState
            ? g_state.readRotateLatched
            : dominant.squeeze >= 0.75f);
    if (rotate && !g_state.rotate.down) PulseHaptic(roles.dominantHand, "interaction_rotate");
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
        const uint32_t interactionHand = ResolveInteractionActionHand(input, roles);
        bool interact = InteractionPressed(HandInput(input, interactionHand));
        if (g_state.menuClickLatchedUntilRelease) {
            if (!interact) g_state.menuClickLatchedUntilRelease = false;
            interact = false;
        }
        if (interact && !g_state.interact.down) {
            PulseHaptic(interactionHand, "interaction");
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
        "hpl_input_bridge install_ok enabled=%d moveDeadzone=%.2f nativeLocomotion=%d movementReference=%s physicalCrouch=%d physicalCrouchThresholds=%.3f,%.3f turnMode=%s turnDeadzone=%.2f nativeTurn=%d snapDegrees=%.1f smoothDegreesPerSecond=%.1f nativeTurnSign=%.1f interaction=%d aimGuide=%d aimGuideLength=%.2f flashlight=%d inventory=%d menu=%d menuPointer=%d terminalPointer=%d recenterChord=%d haptics=%d hapticAmplitude=%.2f hapticDurationMs=%d dominantHand=%s swapSticks=%d oneHandFallback=%d manipulationMappings=%d manipulationMotion=%d manipulationMotionScale=%.1f slideScale=%.1f readRotationScale=%.1f slideDirectVelocity=%d manipulationMotionDeadzone=%.4f manipulationMotionCap=%d manipulationMotionSigns=%.1f,%.1f suppressAuthoredCamera=%d comfortBlackoutFrames=%d stateTransitionBlackoutFrames=%d maxInputAgeFrames=%d comfortVignette=%d comfortVignetteSmoothTurn=%d",
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
        config.hplControllerAimGuide ? 1 : 0,
        config.hplControllerAimGuideLengthMeters,
        config.hplControllerFlashlight ? 1 : 0,
        config.hplControllerInventory ? 1 : 0,
        config.hplControllerMenu ? 1 : 0,
        config.hplControllerMenuPointer ? 1 : 0,
        config.hplControllerTerminalPointer ? 1 : 0,
        config.hplControllerRecenterChord ? 1 : 0,
        config.hplControllerHaptics ? 1 : 0,
        config.hplControllerHapticAmplitude,
        config.hplControllerHapticDurationMs,
        config.hplControllerDominantHand.c_str(),
        config.hplControllerSwapSticks ? 1 : 0,
        config.hplControllerOneHandFallback ? 1 : 0,
        config.hplControllerManipulationMappings ? 1 : 0,
        config.hplControllerManipulationMotion ? 1 : 0,
        config.hplControllerManipulationMotionPixelsPerMeter,
        config.hplControllerManipulationSlidePixelsPerMeter,
        config.hplControllerManipulationReadPixelsPerRadian,
        config.hplControllerSlideDirectVelocity ? 1 : 0,
        config.hplControllerManipulationMotionDeadzoneMeters,
        config.hplControllerManipulationMotionMaxPixelsPerFrame,
        config.hplControllerManipulationMotionHorizontalSign,
        config.hplControllerManipulationMotionVerticalSign,
        config.hplControllerSuppressDuringAuthoredCamera ? 1 : 0,
        config.hplControllerComfortBlackoutFrames,
        config.hplControllerStateTransitionBlackoutFrames,
        config.hplControllerMaxInputAgeFrames,
        config.openxrComfortVignette ? 1 : 0,
        config.openxrComfortVignetteSmoothTurn && !config.hplControllerSnapTurn ? 1 : 0);
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
    ApplyStateTransitionComfort(player, camera, frameIndex);
    const bool loadingScreenActive = IsHPLLoadingScreenActive();
    if (loadingScreenActive) {
        g_loadingSuppressedFrames.fetch_add(1, std::memory_order_relaxed);
    } else {
        ApplyHPLRoomscaleBodyReconciliation(player, camera);
    }
    const bool rawInputAvailable = g_openxr != nullptr
        && g_openxr->GetLatestInput(input) && input.active;
    const bool available = !loadingScreenActive
        && g_config.hplControllerInput && camera.trackingEnabled
        && rawInputAvailable;
    const uint64_t age = available && frameIndex >= input.gameFrame ? frameIndex - input.gameFrame : UINT64_MAX;
    if (!available || age > static_cast<uint64_t>(g_config.hplControllerMaxInputAgeFrames)) {
        if (available) g_staleInputFrames.fetch_add(1, std::memory_order_relaxed);
        const int dominantHand = g_config.hplControllerDominantHand == "left" ? 0 : 1;
        UpdateHPLStatusPanelBridge(
            frameIndex,
            rawInputAvailable ? &input : nullptr,
            dominantHand,
            player,
            camera);
        if (g_openxr != nullptr) g_openxr->SetComfortMotionIntensity(0.0f, frameIndex);
        ReleaseAll();
        g_state.lastTickMs = TickMs();
        return;
    }

    g_activeUpdates.fetch_add(1, std::memory_order_relaxed);
    const uint64_t nowMs = TickMs();
    ControllerRoles roles = ResolveControllerRoles(input);
    const bool interactionLockState = (player.playerStateId >= kGrabPlayerState
        && player.playerStateId <= kLastPhysicalManipulationState)
        || player.playerStateId == kReadPlayerState;
    const uint32_t lockCandidate = ResolveInteractionActionHand(input, roles);
    SetHPLInteractionOwnerLock(interactionLockState, lockCandidate, input.gameFrame);
    uint32_t interactionOwner = roles.dominantHand;
    const bool interactionOwnedState = player.playerStateId >= kGrabPlayerState
        && player.playerStateId <= kReadPlayerState;
    if (interactionOwnedState
        && GetHPLInteractionOwnerHand(input.gameFrame, 120, interactionOwner)
        && HandInput(input, interactionOwner).active) {
        roles.dominantHand = interactionOwner;
        roles.supportHand = roles.oneHand ? interactionOwner : (interactionOwner ^ 1u);
    }
    if (UpdateHPLStatusPanelBridge(
            frameIndex,
            &input,
            roles.dominantHand,
            player,
            camera)) {
        if (g_openxr != nullptr) g_openxr->SetComfortMotionIntensity(0.0f, frameIndex);
        ReleaseAll();
        DeactivateHPLMenuPointer();
        DeactivateHPLTerminalPointer();
        g_state.menuPointerActive = false;
        g_state.terminalPointerActive = false;
        g_state.nativeMovementActive = false;
        g_state.nativeTurnActive = false;
        g_state.lastTickMs = TickMs();
        return;
    }
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
    const bool previousTerminalPointerActive = g_state.terminalPointerActive;
    float comfortMotionIntensity = 0.0f;
    UpdateControllerAimGuide(input, roles, false);
    if (paused) {
        g_pausedFrames.fetch_add(1, std::memory_order_relaxed);
        DeactivateHPLTerminalPointer();
        g_state.terminalPointerActive = false;
        g_state.menuPointerActive = ApplyPausedMenuActions(input, roles);
        g_state.nativeMovementActive = false;
        g_state.nativeTurnActive = false;
    } else if (player.playerValid && player.playerStateId == kDeadPlayerState) {
        ReleaseGameplayInputs();
        DeactivateHPLMenuPointer();
        DeactivateHPLTerminalPointer();
        g_state.menuPointerActive = false;
        g_state.terminalPointerActive = false;
        g_state.nativeMovementActive = false;
        g_state.nativeTurnActive = false;
        const OpenXRHandInput& dominant = HandInput(input, roles.dominantHand);
        const bool continuePressed = (dominant.primary && dominant.primaryChanged)
            || (dominant.select && dominant.selectChanged);
        if (continuePressed) {
            TapKey(VK_SPACE);
            PulseHaptic(roles.dominantHand, "game_over_continue");
            const uint64_t action = g_gameOverContinueActions.fetch_add(1, std::memory_order_relaxed) + 1;
            Logger::Instance().Write(
                LogLevel::Info,
                "hpl_game_over_continue action=%llu hand=%s route=jump_semantic playerState=%d",
                static_cast<unsigned long long>(action),
                roles.dominantHand == 0 ? "left" : "right",
                player.playerStateId);
        }
    } else if (player.playerValid
        && (player.playerStateId == kTerminalPlayerState
            || player.playerStateId == kHandheldTerminalPlayerState)
        && g_config.hplControllerTerminalPointer) {
        g_state.menuPointerActive = false;
        UpdateControllerAimGuide(
            input, roles, true, g_config.hplControllerTerminalRayLengthMeters);
        g_state.terminalPointerActive = ApplyTerminalPointerActions(input, roles);
        g_state.nativeMovementActive = false;
        g_state.nativeTurnActive = false;
    } else if (suppressForAuthoredCamera) {
        ReleaseGameplayInputs();
        DeactivateHPLMenuPointer();
        DeactivateHPLTerminalPointer();
        g_state.menuPointerActive = false;
        g_state.terminalPointerActive = false;
        g_state.nativeMovementActive = false;
        g_state.nativeTurnActive = false;
    } else {
        DeactivateHPLMenuPointer();
        DeactivateHPLTerminalPointer();
        g_state.menuPointerActive = false;
        g_state.terminalPointerActive = false;
        UpdateControllerAimGuide(input, roles, true);
        const bool nativeMovement = ApplyLocomotion(input, roles, player, camera);
        const bool turnAllowed = !player.playerValid
            || player.playerStateId == kNormalPlayerState;
        const bool nativeTurn = turnAllowed ? ApplyTurn(roles, player, nowMs) : false;
        if (!turnAllowed) {
            g_state.snapLatched = false;
            g_state.smoothTurnRemainder = 0.0;
        }
        ApplyControllerManipulationMotion(input, roles, player);
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
        comfortMotionIntensity = comfort_vignette_math::ComputeMotionIntensity(
            roles.moveX,
            roles.moveY,
            roles.turnX,
            g_config.openxrComfortVignetteSmoothTurn && !g_config.hplControllerSnapTurn,
            g_config.hplControllerMoveDeadzone,
            g_config.hplControllerTurnDeadzone);
        ApplyGameplayActions(input, roles, player, camera);
    }
    if (g_openxr != nullptr) {
        g_openxr->SetComfortMotionIntensity(comfortMotionIntensity, frameIndex);
    }
    ApplySystemActions(input, roles, player, nowMs);
    g_state.lastTickMs = nowMs;

    if (suppressGameplay != g_state.gameplaySuppressed
        || paused != previousPaused
        || g_state.menuPointerActive != previousMenuPointerActive
        || g_state.terminalPointerActive != previousTerminalPointerActive) {
        Logger::Instance().Write(
            suppressGameplay ? LogLevel::Warn : LogLevel::Info,
            "hpl_controller_gameplay_policy frame=%llu suppressed=%d authoredCamera=%d paused=%d pauseGateValid=%d menuPointer=%d terminalPointer=%d rotateMode=%d cameraUpdateActive=%d playerState=%d moveState=%d",
            static_cast<unsigned long long>(frameIndex),
            suppressGameplay ? 1 : 0,
            suppressForAuthoredCamera ? 1 : 0,
            paused ? 1 : 0,
            pauseStateValid ? 1 : 0,
            g_state.menuPointerActive ? 1 : 0,
            g_state.terminalPointerActive ? 1 : 0,
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
            "hpl_controller frame=%llu inputFrame=%llu age=%llu dominant=%s oneHand=%d move=%.3f,%.3f movementReference=%s movementRoute=%s keys=%d%d%d%d run=%d jump=%d crouch=%d physicalCrouch=%d rotate=%d turn=%.3f mode=%s turnRoute=%s interact=%d menu=%d recenterChord=%d playerState=%d moveState=%d authoredCamera=%d paused=%d menuPointer=%d terminalPointer=%d gameplaySuppressed=%d worldAim={valid=%d tracked=%d%d pos=%.4f,%.4f,%.4f forward=%.5f,%.5f,%.5f} worldGrip={valid=%d tracked=%d%d pos=%.4f,%.4f,%.4f forward=%.5f,%.5f,%.5f linearVelocityValid=%d linearVelocity=%.4f,%.4f,%.4f angularVelocityValid=%d angularVelocity=%.4f,%.4f,%.4f}",
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
            g_state.terminalPointerActive ? 1 : 0,
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
        "hpl_input_bridge_summary installed=%d updates=%llu activeUpdates=%llu sentEvents=%llu sendFailures=%llu staleInputFrames=%llu loadingSuppressedFrames=%llu recenterRequests=%llu hapticRequests=%llu hapticApplied=%llu oneHandFallbackFrames=%llu worldAimPoseSamples=%llu worldGripPoseSamples=%llu nativeMovementFrames=%llu headRelativeMovementFrames=%llu controllerRelativeMovementFrames=%llu controllerReferenceAttempts=%llu controllerReferenceApplied=%llu controllerReferenceFallbacks=%llu controllerDirectionSamples=%llu controllerAimGuideFrames=%llu inspectionExitActions=%llu nativeTurnEvents=%llu semanticMovementFallbackFrames=%llu physicalCrouchEntries=%llu physicalCrouchExits=%llu manipulationRotateFrames=%llu manipulationMotionFrames=%llu manipulationMotionEntries=%llu manipulationMotionEvents=%llu manipulationMotionTrackingLosses=%llu manipulationMotionSessionSummaries=%llu manipulationMotionPixels=%lld,%lld nativeThrowActions=%llu flashlightActions=%llu inventoryActions=%llu pausedFrames=%llu menuPointerFrames=%llu terminalPointerFrames=%llu gameOverContinueActions=%llu playerStateTransitions=%llu authoredCameraTransitions=%llu playerStateBlackouts=%llu gameplaySuppressed=%d paused=%d menuPointerActive=%d terminalPointerActive=%d physicalCrouch=%d rotate=%d manipulationMotionActive=%d manipulationMotionState=%d manipulationMotionLast=%d,%d player=%p camera=%p body=%p playerState=%d moveState=%d",
        g_openxr != nullptr ? 1 : 0,
        static_cast<unsigned long long>(g_updates.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_activeUpdates.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_sentEvents.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_sendFailures.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_staleInputFrames.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_loadingSuppressedFrames.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_recenterRequests.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_hapticRequests.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_hapticApplied.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_oneHandFallbackFrames.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_worldAimPoseSamples.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_worldGripPoseSamples.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_nativeMovementFrames.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_headRelativeMovementFrames.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_controllerRelativeMovementFrames.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_controllerReferenceAttempts.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_controllerReferenceApplied.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_controllerReferenceFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_controllerDirectionSamples.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_controllerAimGuideFrames.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_inspectionExitActions.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_nativeTurnEvents.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_semanticMovementFallbackFrames.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_physicalCrouchEntries.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_physicalCrouchExits.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_manipulationRotateFrames.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_manipulationMotionFrames.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_manipulationMotionEntries.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_manipulationMotionEvents.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_manipulationMotionTrackingLosses.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_manipulationMotionSessionSummaries.load(std::memory_order_relaxed)),
        static_cast<long long>(g_manipulationMotionPixelsX.load(std::memory_order_relaxed)),
        static_cast<long long>(g_manipulationMotionPixelsY.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_nativeThrowActions.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_flashlightActions.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_inventoryActions.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_pausedFrames.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_menuPointerFrames.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_terminalPointerFrames.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_gameOverContinueActions.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_playerStateTransitions.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_authoredCameraTransitions.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_playerStateBlackouts.load(std::memory_order_relaxed)),
        g_state.gameplaySuppressed ? 1 : 0,
        g_state.paused ? 1 : 0,
        g_state.menuPointerActive ? 1 : 0,
        g_state.terminalPointerActive ? 1 : 0,
        g_state.physicalCrouch.crouched ? 1 : 0,
        g_state.rotate.down ? 1 : 0,
        g_state.manipulationMotionActive ? 1 : 0,
        g_state.manipulationMotionState,
        g_state.manipulationMotionLastX,
        g_state.manipulationMotionLastY,
        player.player, player.camera, player.characterBody,
        player.playerStateId, player.moveStateId);
}

} // namespace somavr
