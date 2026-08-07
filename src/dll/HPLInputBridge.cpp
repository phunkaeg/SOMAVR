#include "HPLInputBridge.h"

#include "HPLCameraBridge.h"
#include "HPLComfortMath.h"
#include "HPLGrabBridge.h"
#include "HPLHandsBridge.h"
#include "HPLHudBridge.h"
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

#include <MinHook.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
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
    bool semanticAnalogMovementActive = false;
    bool nativeTurnActive = false;
    bool paused = false;
    bool menuPointerActive = false;
    bool terminalPointerActive = false;
    uint32_t terminalPointerHand = 1;
    bool terminalLookAwayAnchorValid = false;
    camera_math::Quaternion terminalLookAwayAnchor{};
    uint32_t terminalLookAwayFrames = 0;
    bool terminalLookAwayExitLatched = false;
    bool terminalCancelPending = false;
    uint64_t terminalCancelStartFrame = 0;
    uint64_t terminalCancelReleaseFrame = 0;
    bool menuClickLatchedUntilRelease = false;
    bool readRotateLatched = false;
    crouch_math::PhysicalCrouchState physicalCrouch{};
    bool playerStateInitialized = false;
    int lastPlayerState = -1;
    bool authoredCameraInitialized = false;
    bool lastAuthoredCameraActive = false;
    bool bodyFollowAnchorValid = false;
    bool bodyFollowActive = false;
    float bodyFollowAnchorYaw = 0.0f;
    bool bodyFollowNativeYawValid = false;
    float bodyFollowLastNativeYaw = 0.0f;
    uint64_t bodyFollowThresholdStartMs = 0;
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
constexpr int kMovingButtonPlayerState = 13;
constexpr int kDeadPlayerState = 17;
constexpr int kZoomAreaPlayerState = 18;
constexpr uint64_t kTerminalCancelHoldFrames = 3;
constexpr uintptr_t kPlayerAnalogInputRva = 0x154fb0;
constexpr uintptr_t kPlayerHelperUpdateRva = 0x15ba20;
constexpr std::array<uint8_t, 15> kPlayerAnalogInputSignature{
    0x48, 0x89, 0x5c, 0x24, 0x08, 0x48, 0x89, 0x74,
    0x24, 0x10, 0x57, 0x48, 0x83, 0xec, 0x20,
};
constexpr std::array<uint8_t, 15> kPlayerHelperUpdateSignature{
    0x4c, 0x8b, 0xdc, 0x49, 0x89, 0x5b, 0x20, 0x55,
    0x49, 0x8d, 0x6b, 0xa1, 0x48, 0x81, 0xec,
};

using PlayerAnalogInputFn = void (*)(void*, int, const float*);
using PlayerHelperUpdateFn = void (*)(void*, float);

struct PendingManipulationAnalog {
    void* player = nullptr;
    int playerState = -1;
    int x = 0;
    int y = 0;
    uint64_t inputFrame = 0;
};

struct PendingMovementAnalog {
    void* player = nullptr;
    int playerState = -1;
    float x = 0.0f;
    float y = 0.0f;
    uint64_t inputFrame = 0;
};

Config g_config;
OpenXRRuntime* g_openxr = nullptr;
PlayerAnalogInputFn g_originalPlayerAnalogInput = nullptr;
void* g_playerAnalogInputTarget = nullptr;
PlayerHelperUpdateFn g_originalPlayerHelperUpdate = nullptr;
void* g_playerHelperUpdateTarget = nullptr;
BridgeState g_state;
std::mutex g_mutex;
std::mutex g_pendingAnalogMutex;
PendingManipulationAnalog g_pendingAnalog;
std::mutex g_pendingMovementMutex;
PendingMovementAnalog g_pendingMovement;
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
std::atomic<uint64_t> g_terminalLookAwayExits = 0;
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
std::atomic<uint64_t> g_manipulationNativeAnalogDispatches = 0;
std::atomic<uint64_t> g_manipulationMouseFallbacks = 0;
std::atomic<uint64_t> g_manipulationAnalogStaleDrops = 0;
std::atomic<uint64_t> g_manipulationAnalogContextDeferrals = 0;
std::atomic<uint64_t> g_movementNativeAnalogDispatches = 0;
std::atomic<uint64_t> g_movementAnalogStaleDrops = 0;
std::atomic<uint64_t> g_movementAnalogContextDeferrals = 0;
std::atomic<uint64_t> g_nativeThrowActions = 0;
std::atomic<uint64_t> g_playerStateTransitions = 0;
std::atomic<uint64_t> g_playerStateBlackouts = 0;
std::atomic<uint64_t> g_authoredCameraTransitions = 0;
std::atomic<uint64_t> g_bodyFollowEntries = 0;
std::atomic<uint64_t> g_bodyFollowSteps = 0;
std::atomic<uint64_t> g_bodyFollowTurnFailures = 0;
std::atomic<bool> g_virtualTorsoYawValid = false;
std::atomic<float> g_virtualTorsoYawRadians = 0.0f;

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

bool IsInsideImage(HMODULE module, uintptr_t rva, size_t bytes)
{
    if (module == nullptr || bytes == 0) return false;
    const auto* base = reinterpret_cast<const uint8_t*>(module);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0) return false;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    return nt->Signature == IMAGE_NT_SIGNATURE
        && rva < nt->OptionalHeader.SizeOfImage
        && bytes <= nt->OptionalHeader.SizeOfImage - rva;
}

bool ResolvePlayerAnalogInputTarget()
{
    HMODULE executable = GetModuleHandleW(nullptr);
    if (!IsInsideImage(
            executable,
            kPlayerAnalogInputRva,
            kPlayerAnalogInputSignature.size())) {
        return false;
    }
    const auto* address = reinterpret_cast<const uint8_t*>(executable)
        + kPlayerAnalogInputRva;
    if (std::memcmp(
            address,
            kPlayerAnalogInputSignature.data(),
            kPlayerAnalogInputSignature.size()) != 0) {
        return false;
    }
    g_playerAnalogInputTarget = const_cast<uint8_t*>(address);
    g_originalPlayerAnalogInput = reinterpret_cast<PlayerAnalogInputFn>(
        g_playerAnalogInputTarget);
    return true;
}

bool ResolvePlayerHelperUpdateTarget()
{
    HMODULE executable = GetModuleHandleW(nullptr);
    if (!IsInsideImage(
            executable,
            kPlayerHelperUpdateRva,
            kPlayerHelperUpdateSignature.size())) {
        return false;
    }
    const auto* address = reinterpret_cast<const uint8_t*>(executable)
        + kPlayerHelperUpdateRva;
    if (std::memcmp(
            address,
            kPlayerHelperUpdateSignature.data(),
            kPlayerHelperUpdateSignature.size()) != 0) {
        return false;
    }
    g_playerHelperUpdateTarget = const_cast<uint8_t*>(address);
    return true;
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
    case 13: return "moving_button";
    default: return "none";
    }
}

void ClearPendingManipulationAnalog()
{
    std::lock_guard lock(g_pendingAnalogMutex);
    g_pendingAnalog = {};
}

void ClearPendingMovementAnalog()
{
    std::lock_guard lock(g_pendingMovementMutex);
    g_pendingMovement = {};
}

bool PlayerStateAllowsSemanticLocomotion(int playerState)
{
    return playerState == kNormalPlayerState || playerState == kGrabPlayerState;
}

const char* PlayerStateName(int state);

bool ReadPointer(const void* address, void*& value)
{
    SIZE_T bytesRead = 0;
    value = nullptr;
    return address != nullptr
        && ReadProcessMemory(
            GetCurrentProcess(), address, &value, sizeof(value), &bytesRead) != FALSE
        && bytesRead == sizeof(value);
}

bool DispatchPendingManipulationAnalog(void* player, void* analogOwner)
{
    if (g_originalPlayerAnalogInput == nullptr
        || player == nullptr
        || analogOwner == nullptr) {
        return false;
    }
    PendingManipulationAnalog pending;
    {
        std::lock_guard lock(g_pendingAnalogMutex);
        if (g_pendingAnalog.player != player
            || (g_pendingAnalog.x == 0 && g_pendingAnalog.y == 0)) {
            return false;
        }
        pending = g_pendingAnalog;
    }

    if (!IsHPLPlayerStateActiveNow(pending.playerState, player, nullptr)) {
        g_manipulationAnalogStaleDrops.fetch_add(1, std::memory_order_relaxed);
        ClearPendingManipulationAnalog();
        return false;
    }

    void* stateScript = nullptr;
    void* scriptContextManager = nullptr;
    if (!ReadPointer(
            reinterpret_cast<const uint8_t*>(analogOwner) + 0xc8,
            stateScript)
        || stateScript == nullptr
        || !ReadPointer(
            reinterpret_cast<const uint8_t*>(stateScript) + 0x10,
            scriptContextManager)
        || scriptContextManager == nullptr) {
        const uint64_t deferral = g_manipulationAnalogContextDeferrals.fetch_add(
            1, std::memory_order_relaxed) + 1;
        if (deferral <= 8
            || deferral % static_cast<uint64_t>(
                std::max(g_config.hplControllerLogInterval, 1)) == 0) {
            Logger::Instance().Write(
                LogLevel::Info,
                "hpl_manipulation_native_input deferred=%llu state=%s(%d) player=%p analogOwner=%p stateScript=%p context=%p route=player_helper_update_context_not_ready",
                static_cast<unsigned long long>(deferral),
                PhysicalManipulationStateName(pending.playerState),
                pending.playerState,
                player,
                analogOwner,
                stateScript,
                scriptContextManager);
        }
        return false;
    }

    {
        std::lock_guard lock(g_pendingAnalogMutex);
        if (g_pendingAnalog.player != pending.player
            || g_pendingAnalog.playerState != pending.playerState
            || g_pendingAnalog.inputFrame != pending.inputFrame) {
            return false;
        }
        pending = g_pendingAnalog;
        g_pendingAnalog = {};
    }

    const float controllerAmount[3] = {
        static_cast<float>(pending.x),
        static_cast<float>(pending.y),
        0.0f,
    };
    const uint64_t dispatch = g_manipulationNativeAnalogDispatches.fetch_add(
        1, std::memory_order_relaxed) + 1;
    if (dispatch <= 16
        || dispatch % static_cast<uint64_t>(
            std::max(g_config.hplControllerLogInterval, 1)) == 0) {
        Logger::Instance().Write(
            LogLevel::Info,
            "hpl_manipulation_native_input dispatch=%llu state=%s(%d) player=%p analogOwner=%p stateScript=%p context=%p inputFrame=%llu amount=%.1f,%.1f route=player_helper_update_0x15ba20_to_analog_0x154fb0",
            static_cast<unsigned long long>(dispatch),
            PhysicalManipulationStateName(pending.playerState),
            pending.playerState,
            player,
            analogOwner,
            stateScript,
            scriptContextManager,
            static_cast<unsigned long long>(pending.inputFrame),
            controllerAmount[0],
            controllerAmount[1]);
    }
    g_originalPlayerAnalogInput(analogOwner, 0, controllerAmount);
    return true;
}

bool DispatchPendingMovementAnalog(void* player, void* analogOwner)
{
    if (g_originalPlayerAnalogInput == nullptr
        || player == nullptr
        || analogOwner == nullptr) {
        return false;
    }

    PendingMovementAnalog pending;
    {
        std::lock_guard lock(g_pendingMovementMutex);
        if (g_pendingMovement.player != player
            || (g_pendingMovement.x == 0.0f && g_pendingMovement.y == 0.0f)) {
            return false;
        }
        pending = g_pendingMovement;
    }

    if (!PlayerStateAllowsSemanticLocomotion(pending.playerState)
        || !IsHPLPlayerStateActiveNow(pending.playerState, player, nullptr)) {
        g_movementAnalogStaleDrops.fetch_add(1, std::memory_order_relaxed);
        ClearPendingMovementAnalog();
        return false;
    }

    void* stateScript = nullptr;
    void* scriptContextManager = nullptr;
    if (!ReadPointer(
            reinterpret_cast<const uint8_t*>(analogOwner) + 0xc8,
            stateScript)
        || stateScript == nullptr
        || !ReadPointer(
            reinterpret_cast<const uint8_t*>(stateScript) + 0x10,
            scriptContextManager)
        || scriptContextManager == nullptr) {
        const uint64_t deferral = g_movementAnalogContextDeferrals.fetch_add(
            1, std::memory_order_relaxed) + 1;
        if (deferral <= 8
            || deferral % static_cast<uint64_t>(
                std::max(g_config.hplControllerLogInterval, 1)) == 0) {
            Logger::Instance().Write(
                LogLevel::Info,
                "hpl_movement_native_input deferred=%llu state=%s(%d) player=%p analogOwner=%p stateScript=%p context=%p route=player_helper_update_context_not_ready",
                static_cast<unsigned long long>(deferral),
                PlayerStateName(pending.playerState),
                pending.playerState,
                player,
                analogOwner,
                stateScript,
                scriptContextManager);
        }
        return false;
    }

    {
        std::lock_guard lock(g_pendingMovementMutex);
        if (g_pendingMovement.player != pending.player
            || g_pendingMovement.playerState != pending.playerState
            || g_pendingMovement.inputFrame != pending.inputFrame) {
            return false;
        }
        pending = g_pendingMovement;
        g_pendingMovement = {};
    }

    const float controllerAmount[3] = {pending.x, pending.y, 0.0f};
    const uint64_t dispatch = g_movementNativeAnalogDispatches.fetch_add(
        1, std::memory_order_relaxed) + 1;
    if (dispatch <= 16
        || dispatch % static_cast<uint64_t>(
            std::max(g_config.hplControllerLogInterval, 1)) == 0) {
        Logger::Instance().Write(
            LogLevel::Info,
            "hpl_movement_native_input dispatch=%llu state=%s(%d) player=%p analogOwner=%p stateScript=%p context=%p inputFrame=%llu amount=%.4f,%.4f route=player_helper_update_0x15ba20_to_analog_0x154fb0_type_1",
            static_cast<unsigned long long>(dispatch),
            PlayerStateName(pending.playerState),
            pending.playerState,
            player,
            analogOwner,
            stateScript,
            scriptContextManager,
            static_cast<unsigned long long>(pending.inputFrame),
            controllerAmount[0],
            controllerAmount[1]);
    }
    g_originalPlayerAnalogInput(analogOwner, 1, controllerAmount);
    return true;
}

void HookPlayerHelperUpdate(void* helper, float deltaTime)
{
    if (helper != nullptr) {
        auto* player = reinterpret_cast<uint8_t*>(helper) - 0x110;
        DispatchPendingMovementAnalog(player, helper);
        DispatchPendingManipulationAnalog(player, helper);
    }
    g_originalPlayerHelperUpdate(helper, deltaTime);
}

bool QueueManipulationAnalog(
    void* player,
    int playerState,
    int x,
    int y,
    uint64_t inputFrame)
{
    if (g_originalPlayerAnalogInput == nullptr
        || g_originalPlayerHelperUpdate == nullptr
        || player == nullptr
        || (x == 0 && y == 0)) {
        return false;
    }

    {
        std::lock_guard lock(g_pendingAnalogMutex);
        if (g_pendingAnalog.player != nullptr
            && (g_pendingAnalog.player != player
                || g_pendingAnalog.playerState != playerState)) {
            g_pendingAnalog = {};
        }
        const int maximumQueued = std::max(
            g_config.hplControllerManipulationMotionMaxPixelsPerFrame * 4,
            80);
        g_pendingAnalog.player = player;
        g_pendingAnalog.playerState = playerState;
        g_pendingAnalog.x = std::clamp(
            g_pendingAnalog.x + x, -maximumQueued, maximumQueued);
        g_pendingAnalog.y = std::clamp(
            g_pendingAnalog.y + y, -maximumQueued, maximumQueued);
        g_pendingAnalog.inputFrame = inputFrame;
    }
    return true;
}

bool QueueMovementAnalog(
    void* player,
    int playerState,
    float x,
    float y,
    uint64_t inputFrame)
{
    if (g_originalPlayerAnalogInput == nullptr
        || g_originalPlayerHelperUpdate == nullptr
        || player == nullptr) {
        return false;
    }

    std::lock_guard lock(g_pendingMovementMutex);
    if (x == 0.0f && y == 0.0f) {
        g_pendingMovement = {};
        return true;
    }
    g_pendingMovement.player = player;
    g_pendingMovement.playerState = playerState;
    g_pendingMovement.x = x;
    g_pendingMovement.y = y;
    g_pendingMovement.inputFrame = inputFrame;
    return true;
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
    ClearPendingManipulationAnalog();
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

void ResetPhysicalBodyFollow()
{
    g_state.bodyFollowAnchorValid = false;
    g_state.bodyFollowActive = false;
    g_state.bodyFollowNativeYawValid = false;
    g_state.bodyFollowThresholdStartMs = 0;
    g_virtualTorsoYawValid.store(false, std::memory_order_release);
}

void ReleaseGameplayInputs()
{
    ReleaseMovementInputs();
    ClearPendingMovementAnalog();
    g_state.semanticAnalogMovementActive = false;
    SetMouseButton(g_state.interact, false);
    SetMiddleMouseButton(g_state.rotate, false);
    SetRightMouseButton(g_state.cancel, false);
    SetKey(g_state.sprint, VK_LSHIFT, false);
    g_state.snapLatched = false;
    g_state.smoothTurnRemainder = 0.0;
    ResetManipulationMotion();
    g_state.readRotateLatched = false;
    ResetPhysicalBodyFollow();
}

void ReleaseGameplayExceptPointer()
{
    ReleaseMovementInputs();
    ClearPendingMovementAnalog();
    g_state.semanticAnalogMovementActive = false;
    SetKey(g_state.sprint, VK_LSHIFT, false);
    SetMiddleMouseButton(g_state.rotate, false);
    SetRightMouseButton(g_state.cancel, false);
    g_state.snapLatched = false;
    g_state.smoothTurnRemainder = 0.0;
    ResetManipulationMotion();
    ResetPhysicalBodyFollow();
}

void ReleaseGameplayForTerminalPointer()
{
    ReleaseMovementInputs();
    ClearPendingMovementAnalog();
    g_state.semanticAnalogMovementActive = false;
    SetKey(g_state.sprint, VK_LSHIFT, false);
    SetMiddleMouseButton(g_state.rotate, false);
    g_state.snapLatched = false;
    g_state.smoothTurnRemainder = 0.0;
    ResetManipulationMotion();
    ResetPhysicalBodyFollow();
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
    g_state.terminalLookAwayAnchorValid = false;
    g_state.terminalLookAwayFrames = 0;
    g_state.terminalLookAwayExitLatched = false;
    g_state.terminalCancelPending = false;
    g_state.terminalCancelStartFrame = 0;
    g_state.terminalCancelReleaseFrame = 0;
    g_state.recenterStartMs = 0;
    g_state.recenterLatched = false;
    g_state.menuClickLatchedUntilRelease = false;
    ResetPhysicalBodyFollow();
    if (g_openxr != nullptr) g_openxr->ClearControllerAimGuide();
}

bool ApplyLocomotion(
    const OpenXRInputSnapshot& input,
    const ControllerRoles& roles,
    const HPLPlayerStateSnapshot& player,
    const HPLCameraBridgeStatus& camera)
{
    g_state.semanticAnalogMovementActive = false;
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
    const bool physicalInteractionState =
        (player.playerStateId >= kWheelPlayerState
            && player.playerStateId <= kLastPhysicalManipulationState)
        || player.playerStateId == kMovingButtonPlayerState;
    if ((physicalInteractionState
            && ApplyHPLInteractionMovement(player, nativeMove.x, nativeMove.y))
        || ApplyHPLNativeMovement(player, nativeMove.x, nativeMove.y))
    {
        ClearPendingMovementAnalog();
        ReleaseMovementInputs();
        g_nativeMovementFrames.fetch_add(1, std::memory_order_relaxed);
        return true;
    }

    if (PlayerStateAllowsSemanticLocomotion(player.playerStateId)
        && player.moveStateId == 0
        && QueueMovementAnalog(
            player.player,
            player.playerStateId,
            nativeMove.x,
            nativeMove.y,
            input.gameFrame)) {
        ReleaseMovementInputs();
        g_state.semanticAnalogMovementActive = true;
        return true;
    }

    ClearPendingMovementAnalog();
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

void ResetTerminalLookAwayTracking()
{
    SetRightMouseButton(g_state.cancel, false);
    g_state.terminalLookAwayAnchorValid = false;
    g_state.terminalLookAwayFrames = 0;
    g_state.terminalLookAwayExitLatched = false;
    g_state.terminalCancelPending = false;
    g_state.terminalCancelStartFrame = 0;
    g_state.terminalCancelReleaseFrame = 0;
}

void BeginTerminalCancel(uint64_t inputFrame, uint32_t hand, const char* source)
{
    if (!g_state.terminalCancelPending) {
        g_state.terminalCancelStartFrame = inputFrame;
        Logger::Instance().Write(
            LogLevel::Warn,
            "hpl_terminal_cancel begin frame=%llu hand=%s source=%s holdFrames=%llu route=held_right_mouse_button",
            static_cast<unsigned long long>(inputFrame),
            hand == 0 ? "left" : "right",
            source,
            static_cast<unsigned long long>(kTerminalCancelHoldFrames));
    }
    g_state.terminalCancelPending = true;
    g_state.terminalCancelReleaseFrame = std::max(
        g_state.terminalCancelReleaseFrame,
        inputFrame + kTerminalCancelHoldFrames);
    SetRightMouseButton(g_state.cancel, true);
}

bool ShouldExitTerminalForLookAway(
    const OpenXRHeadPose& headPose,
    int playerState,
    uint64_t inputFrame)
{
    if (!g_config.hplControllerTerminalLookAwayExit
        || playerState != kTerminalPlayerState
        || !headPose.valid
        || !headPose.orientationTracked) {
        if (playerState != kTerminalPlayerState) {
            ResetTerminalLookAwayTracking();
        } else {
            g_state.terminalLookAwayFrames = 0;
        }
        return false;
    }

    const camera_math::Quaternion orientation{
        headPose.orientationX,
        headPose.orientationY,
        headPose.orientationZ,
        headPose.orientationW,
    };
    if (!g_state.terminalLookAwayAnchorValid) {
        g_state.terminalLookAwayAnchor = camera_math::Normalize(orientation);
        g_state.terminalLookAwayAnchorValid = true;
        g_state.terminalLookAwayFrames = 0;
        g_state.terminalLookAwayExitLatched = false;
        Logger::Instance().Write(
            LogLevel::Info,
            "hpl_terminal_lookaway anchor frame=%llu thresholdDegrees=%.1f dwellFrames=%d",
            static_cast<unsigned long long>(inputFrame),
            g_config.hplControllerTerminalLookAwayDegrees,
            g_config.hplControllerTerminalLookAwayFrames);
        return false;
    }

    const float angleDegrees = input_math::QuaternionAngularDistanceDegrees(
        g_state.terminalLookAwayAnchor, orientation);
    if (!std::isfinite(angleDegrees)
        || angleDegrees < g_config.hplControllerTerminalLookAwayDegrees) {
        g_state.terminalLookAwayFrames = 0;
        return false;
    }
    if (g_state.terminalLookAwayFrames < UINT32_MAX) {
        ++g_state.terminalLookAwayFrames;
    }
    if (g_state.terminalLookAwayExitLatched
        || g_state.terminalLookAwayFrames
            < static_cast<uint32_t>(g_config.hplControllerTerminalLookAwayFrames)) {
        return false;
    }

    g_state.terminalLookAwayExitLatched = true;
    const uint64_t exit = g_terminalLookAwayExits.fetch_add(
        1, std::memory_order_relaxed) + 1;
    Logger::Instance().Write(
        LogLevel::Warn,
        "hpl_terminal_lookaway exit=%llu frame=%llu angleDegrees=%.2f thresholdDegrees=%.1f dwellFrames=%u route=native_interact_cancel",
        static_cast<unsigned long long>(exit),
        static_cast<unsigned long long>(inputFrame),
        angleDegrees,
        g_config.hplControllerTerminalLookAwayDegrees,
        g_state.terminalLookAwayFrames);
    return true;
}

bool ApplyTerminalPointerActions(
    const OpenXRInputSnapshot& input,
    const ControllerRoles& roles,
    int playerState)
{
    ReleaseGameplayForTerminalPointer();
    DeactivateHPLMenuPointer();
    const bool leftPressed = InteractionPressed(input.left);
    const bool rightPressed = InteractionPressed(input.right);
    uint32_t pointerHand = g_state.terminalPointerActive
        ? g_state.terminalPointerHand : roles.dominantHand;
    if (leftPressed != rightPressed) pointerHand = leftPressed ? 0u : 1u;
    if (!HandInput(input, pointerHand).active) pointerHand = roles.dominantHand;
    g_state.terminalPointerHand = pointerHand;
    const OpenXRHandInput& hand = HandInput(input, pointerHand);
    const bool controllerCancelHeld = input.right.primary || input.right.secondary;
    const bool controllerCancelPressed = controllerCancelHeld
        && (input.right.primaryChanged || input.right.secondaryChanged);
    if (controllerCancelPressed) {
        BeginTerminalCancel(input.gameFrame, 1, "right_controller_a_or_b");
        PulseHaptic(pointerHand, "inspection_exit");
        g_inspectionExitActions.fetch_add(1, std::memory_order_relaxed);
    }
    OpenXRHeadPose headPose;
    const bool headPoseValid = g_openxr != nullptr
        && g_openxr->GetLatestHeadPose(headPose);
    if (headPoseValid
        && ShouldExitTerminalForLookAway(headPose, playerState, input.gameFrame)) {
        SetMouseButton(g_state.interact, false);
        DeactivateHPLTerminalPointer();
        BeginTerminalCancel(input.gameFrame, pointerHand, "look_away");
        PulseHaptic(pointerHand, "terminal_lookaway_exit");
        return false;
    }
    const bool syntheticCancelHeld = g_state.terminalCancelPending
        && input.gameFrame <= g_state.terminalCancelReleaseFrame;
    SetRightMouseButton(g_state.cancel, controllerCancelHeld || syntheticCancelHeld);
    if (g_state.terminalCancelPending || controllerCancelHeld) {
        SetMouseButton(g_state.interact, false);
        DeactivateHPLTerminalPointer();
        return false;
    }
    const bool pointerActive = g_config.hplControllerTerminalPointer
        && headPoseValid
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
        guide.alpha = g_config.hplControllerAimGuideIdleAlpha;
        float sceneDistanceMeters = 0.0f;
        if (guide.valid
            && ResolveHPLControllerBeamDistance(
                hand.aimPose,
                input.gameFrame,
                guide.lengthMeters,
                sceneDistanceMeters)) {
            guide.lengthMeters = std::min(
                guide.lengthMeters,
                std::max(sceneDistanceMeters - 0.015f, 0.05f));
        }
        if (hit.valid && hit.handIndex == handIndex
            && input.gameFrame >= hit.gameFrame
            && input.gameFrame - hit.gameFrame <= 4) {
            guide.interactable = true;
            guide.alpha = g_config.hplControllerAimGuideInteractableAlpha;
            guide.lengthMeters = std::min(
                guide.lengthMeters,
                std::clamp(
                    hit.distance / std::max(g_config.hplWorldScale, 0.001f),
                    0.05f,
                    20.0f));
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

bool ApplyPhysicalBodyFollow(
    const ControllerRoles& roles,
    const HPLPlayerStateSnapshot& player,
    const HPLCameraBridgeStatus& camera,
    uint64_t nowMs)
{
    if (!g_config.hplControllerPhysicalBodyFollow
        || !camera.trackingEnabled
        || !camera.headWorldRotationValid
        || !player.playerValid
        || player.authoredCameraActive
        || player.playerStateId != kNormalPlayerState
        || player.moveStateId != 0) {
        ResetPhysicalBodyFollow();
        return false;
    }
    bool paused = false;
    if (!GetHPLGamePausedState(paused) || paused) {
        ResetPhysicalBodyFollow();
        return false;
    }

    float headYaw = 0.0f;
    if (!input_math::ResolveHorizontalYaw(
            {
                camera.headWorldRotationX,
                camera.headWorldRotationY,
                camera.headWorldRotationZ,
                camera.headWorldRotationW,
            },
            headYaw)) {
        return false;
    }
    float nativeYaw = headYaw;
    bool nativeYawValid = false;
    if (camera.nativeCameraBasisValid) {
        camera_math::Quaternion nativeOrientation{};
        nativeYawValid = camera_math::QuaternionFromForwardUp(
                {camera.nativeCameraForwardX, 0.0f, camera.nativeCameraForwardZ},
                {0.0f, 1.0f, 0.0f},
                nativeOrientation)
            && input_math::ResolveHorizontalYaw(nativeOrientation, nativeYaw);
    }
    if (!g_state.bodyFollowAnchorValid) {
        g_state.bodyFollowAnchorValid = true;
        g_state.bodyFollowAnchorYaw = nativeYawValid ? nativeYaw : headYaw;
        g_state.bodyFollowNativeYawValid = nativeYawValid;
        g_state.bodyFollowLastNativeYaw = nativeYaw;
        g_virtualTorsoYawRadians.store(
            g_state.bodyFollowAnchorYaw, std::memory_order_relaxed);
        g_virtualTorsoYawValid.store(true, std::memory_order_release);
        return false;
    }

    if (nativeYawValid) {
        if (g_state.bodyFollowNativeYawValid) {
            const float nativeDelta = input_math::WrapRadians(
                nativeYaw - g_state.bodyFollowLastNativeYaw);
            g_state.bodyFollowAnchorYaw = input_math::WrapRadians(
                g_state.bodyFollowAnchorYaw + nativeDelta);
        }
        g_state.bodyFollowNativeYawValid = true;
        g_state.bodyFollowLastNativeYaw = nativeYaw;
    }
    g_virtualTorsoYawRadians.store(
        g_state.bodyFollowAnchorYaw, std::memory_order_relaxed);
    g_virtualTorsoYawValid.store(true, std::memory_order_release);

    if (std::fabs(roles.turnX) >= g_config.hplControllerTurnReleaseDeadzone) {
        g_state.bodyFollowActive = false;
        g_state.bodyFollowThresholdStartMs = 0;
        return false;
    }

    const float error = input_math::WrapRadians(
        headYaw - g_state.bodyFollowAnchorYaw);
    const float threshold = input_math::DegreesToRadians(
        g_config.hplControllerPhysicalBodyFollowThresholdDegrees);
    const float release = input_math::DegreesToRadians(
        std::min(
            g_config.hplControllerPhysicalBodyFollowReleaseDegrees,
            g_config.hplControllerPhysicalBodyFollowThresholdDegrees));
    if (!g_state.bodyFollowActive) {
        if (std::fabs(error) < threshold) {
            g_state.bodyFollowThresholdStartMs = 0;
            return false;
        }
        if (g_state.bodyFollowThresholdStartMs == 0) {
            g_state.bodyFollowThresholdStartMs = nowMs;
            return false;
        }
        if (nowMs - g_state.bodyFollowThresholdStartMs
            < static_cast<uint64_t>(g_config.hplControllerPhysicalBodyFollowDelayMs)) {
            return false;
        }
        g_state.bodyFollowActive = true;
        const uint64_t entry = g_bodyFollowEntries.fetch_add(
            1, std::memory_order_relaxed) + 1;
        Logger::Instance().Write(
            LogLevel::Info,
            "hpl_physical_body_follow entry=%llu frame=%llu headYawDegrees=%.2f errorDegrees=%.2f thresholdDegrees=%.2f releaseDegrees=%.2f speedDegreesPerSecond=%.2f delayMs=%d",
            static_cast<unsigned long long>(entry),
            static_cast<unsigned long long>(player.frame),
            headYaw * 57.2957795f,
            error * 57.2957795f,
            g_config.hplControllerPhysicalBodyFollowThresholdDegrees,
            g_config.hplControllerPhysicalBodyFollowReleaseDegrees,
            g_config.hplControllerPhysicalBodyFollowDegreesPerSecond,
            g_config.hplControllerPhysicalBodyFollowDelayMs);
    }

    if (std::fabs(error) <= release) {
        g_state.bodyFollowActive = false;
        g_state.bodyFollowThresholdStartMs = 0;
        return false;
    }
    const uint64_t elapsedMs = g_state.lastTickMs == 0
        ? 0 : std::min<uint64_t>(nowMs - g_state.lastTickMs, 100);
    const float physicalStep = input_math::ComputeBodyFollowStepRadians(
        error,
        g_config.hplControllerPhysicalBodyFollowReleaseDegrees,
        g_config.hplControllerPhysicalBodyFollowDegreesPerSecond,
        elapsedMs);
    if (std::fabs(physicalStep) <= 1.0e-6f) return false;
    g_state.bodyFollowAnchorYaw = input_math::WrapRadians(
        g_state.bodyFollowAnchorYaw + physicalStep);
    g_virtualTorsoYawRadians.store(
        g_state.bodyFollowAnchorYaw, std::memory_order_relaxed);
    const uint64_t step = g_bodyFollowSteps.fetch_add(1, std::memory_order_relaxed) + 1;
    if (step <= 8 || step % 120 == 0) {
        Logger::Instance().Write(
            LogLevel::Info,
            "hpl_physical_body_follow step=%llu frame=%llu errorDegrees=%.2f appliedDegrees=%.3f residualDegrees=%.2f policy=yaw_only_delayed_rate_limited_virtual_torso_follow_no_camera_turn",
            static_cast<unsigned long long>(step),
            static_cast<unsigned long long>(player.frame),
            error * 57.2957795f,
            physicalStep * 57.2957795f,
            input_math::WrapRadians(error - physicalStep) * 57.2957795f);
    }
    return true;
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
    const bool physicalState = ((player.playerStateId >= kWheelPlayerState
        && player.playerStateId <= kLastPhysicalManipulationState)
        || player.playerStateId == kMovingButtonPlayerState)
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

    const bool nativeAnalogQueued = QueueManipulationAnalog(
        player.player,
        player.playerStateId,
        outputX,
        outputY,
        input.gameFrame);
    if (!nativeAnalogQueued) {
        SendMouseMove(outputX, outputY);
        g_manipulationMouseFallbacks.fetch_add(1, std::memory_order_relaxed);
    }
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
            "hpl_manipulation_motion event=%llu state=%s(%d) hand=%s inputFrame=%llu displacementMeters=%.5f,%.5f rotationRadians=%.5f,%.5f analogAmount=%d,%d route=%s scale=%.1f signs=%.1f,%.1f",
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
            nativeAnalogQueued
                ? "queued_native_input_phase_substitution_0x154fb0"
                : "windows_mouse_fallback",
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
    const bool manipulationState = (player.playerStateId >= kGrabPlayerState
        && player.playerStateId <= kLastPhysicalManipulationState)
        || player.playerStateId == kMovingButtonPlayerState;
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

bool GetHPLVirtualTorsoYaw(camera_math::Quaternion& yaw)
{
    if (!g_virtualTorsoYawValid.load(std::memory_order_acquire)) return false;
    const float radians = g_virtualTorsoYawRadians.load(std::memory_order_relaxed);
    if (!std::isfinite(radians)) return false;
    const float halfYaw = radians * 0.5f;
    yaw = {0.0f, std::sin(halfYaw), 0.0f, std::cos(halfYaw)};
    return true;
}

bool InstallHPLInputBridge(const Config& config, OpenXRRuntime* openxr)
{
    std::lock_guard lock(g_mutex);
    g_config = config;
    g_openxr = openxr;
    bool nativeSemanticAnalog = false;
    if (ResolvePlayerAnalogInputTarget() && ResolvePlayerHelperUpdateTarget()) {
        MH_STATUS status = MH_CreateHook(
            g_playerHelperUpdateTarget,
            reinterpret_cast<void*>(&HookPlayerHelperUpdate),
            reinterpret_cast<void**>(&g_originalPlayerHelperUpdate));
        if (status == MH_OK
            || (status == MH_ERROR_ALREADY_CREATED
                && g_originalPlayerHelperUpdate != nullptr)) {
            status = MH_EnableHook(g_playerHelperUpdateTarget);
            nativeSemanticAnalog = status == MH_OK || status == MH_ERROR_ENABLED;
        }
        if (!nativeSemanticAnalog) {
            Logger::Instance().Write(
                LogLevel::Warn,
                "hpl_input_bridge native_semantic_hook_unavailable status=%s fallback=windows_input",
                MH_StatusToString(status));
            MH_RemoveHook(g_playerHelperUpdateTarget);
            g_playerHelperUpdateTarget = nullptr;
            g_originalPlayerHelperUpdate = nullptr;
            g_originalPlayerAnalogInput = nullptr;
        }
    }
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_input_bridge install_ok enabled=%d moveDeadzone=%.2f nativeLocomotion=%d movementReference=%s physicalCrouch=%d physicalCrouchThresholds=%.3f,%.3f turnMode=%s turnDeadzone=%.2f nativeTurn=%d snapDegrees=%.1f smoothDegreesPerSecond=%.1f nativeTurnSign=%.1f interaction=%d aimGuide=%d aimGuideLength=%.2f flashlight=%d inventory=%d menu=%d menuPointer=%d terminalPointer=%d terminalOverlay=%d terminalLookAway=%d,%.1f,%d recenterChord=%d haptics=%d hapticAmplitude=%.2f hapticDurationMs=%d dominantHand=%s swapSticks=%d oneHandFallback=%d manipulationMappings=%d manipulationMotion=%d nativeSemanticAnalog=%d nativeSemanticAnalogRva=0x%llx nativeInputPhaseRva=0x%llx manipulationMotionScale=%.1f slideScale=%.1f readRotationScale=%.1f slideDirectVelocity=%d manipulationMotionDeadzone=%.4f manipulationMotionCap=%d manipulationMotionSigns=%.1f,%.1f suppressAuthoredCamera=%d comfortBlackoutFrames=%d stateTransitionBlackoutFrames=%d maxInputAgeFrames=%d comfortVignette=%d comfortVignetteSmoothTurn=%d",
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
        config.hplControllerTerminalOverlay ? 1 : 0,
        config.hplControllerTerminalLookAwayExit ? 1 : 0,
        config.hplControllerTerminalLookAwayDegrees,
        config.hplControllerTerminalLookAwayFrames,
        config.hplControllerRecenterChord ? 1 : 0,
        config.hplControllerHaptics ? 1 : 0,
        config.hplControllerHapticAmplitude,
        config.hplControllerHapticDurationMs,
        config.hplControllerDominantHand.c_str(),
        config.hplControllerSwapSticks ? 1 : 0,
        config.hplControllerOneHandFallback ? 1 : 0,
        config.hplControllerManipulationMappings ? 1 : 0,
        config.hplControllerManipulationMotion ? 1 : 0,
        nativeSemanticAnalog ? 1 : 0,
        static_cast<unsigned long long>(kPlayerAnalogInputRva),
        static_cast<unsigned long long>(kPlayerHelperUpdateRva),
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
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_body_follow_config enabled=%d thresholdDegrees=%.2f releaseDegrees=%.2f speedDegreesPerSecond=%.2f delayMs=%d aimGuideSceneDepth=%d policy=virtual_torso_yaw_never_mutates_game_camera_for_physical_turn",
        config.hplControllerPhysicalBodyFollow ? 1 : 0,
        config.hplControllerPhysicalBodyFollowThresholdDegrees,
        config.hplControllerPhysicalBodyFollowReleaseDegrees,
        config.hplControllerPhysicalBodyFollowDegreesPerSecond,
        config.hplControllerPhysicalBodyFollowDelayMs,
        config.hplControllerAimGuideSceneDepth ? 1 : 0);
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_interaction_presence_config locomotionDuringInteractions=%d aimGuideAlpha=%.3f,%.3f aimGuideLengthMeters=%.2f",
        config.hplControllerLocomotionDuringInteractions ? 1 : 0,
        config.hplControllerAimGuideIdleAlpha,
        config.hplControllerAimGuideInteractableAlpha,
        config.hplControllerAimGuideLengthMeters);
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
    const bool nativeMenuCursorVisible = IsHPLNativeMenuCursorVisible();
    const bool currentImGuiSurface = IsHPLCurrentImGuiSurfaceActive(frameIndex, 4);
    const bool mainMenuSurface = !paused
        && !player.authoredCameraActive
        && ((nativeMenuCursorVisible
                && (!player.playerValid
                    || player.playerStateId == kNormalPlayerState))
            || (currentImGuiSurface
                && (!player.playerValid
                    || player.playerStateId == kNormalPlayerState))
            || !player.playerValid || !player.cameraControlValid);
    const bool suppressForAuthoredCamera = g_config.hplControllerSuppressDuringAuthoredCamera
        && player.authoredCameraActive;
    const bool suppressGameplay = suppressForAuthoredCamera || paused || mainMenuSurface;
    const bool previousPaused = g_state.paused;
    const bool previousMenuPointerActive = g_state.menuPointerActive;
    const bool previousTerminalPointerActive = g_state.terminalPointerActive;
    float comfortMotionIntensity = 0.0f;
    const bool terminalState = player.playerValid
        && (player.playerStateId == kTerminalPlayerState
            || player.playerStateId == kHandheldTerminalPlayerState);
    if (!terminalState) {
        if (g_state.terminalCancelPending) {
            Logger::Instance().Write(
                LogLevel::Info,
                "hpl_terminal_cancel completed frame=%llu latencyFrames=%llu route=held_right_mouse_button",
                static_cast<unsigned long long>(frameIndex),
                static_cast<unsigned long long>(
                    frameIndex >= g_state.terminalCancelStartFrame
                        ? frameIndex - g_state.terminalCancelStartFrame : 0));
        }
        ResetTerminalLookAwayTracking();
    }
    if (g_openxr != nullptr) {
        g_openxr->SetDesktopMirrorNativeBackbuffer(paused || mainMenuSurface);
    }
    UpdateControllerAimGuide(input, roles, false);
    if (paused || mainMenuSurface) {
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
        g_state.terminalPointerActive = ApplyTerminalPointerActions(
            input, roles, player.playerStateId);
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
        bool nativeTurn = turnAllowed ? ApplyTurn(roles, player, nowMs) : false;
        const bool physicalBodyFollow = turnAllowed
            && ApplyPhysicalBodyFollow(roles, player, camera, nowMs);
        nativeTurn = nativeTurn || physicalBodyFollow;
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
                g_state.semanticAnalogMovementActive
                    ? "native_player_analog"
                    : (nativeMovement ? "native_body" : "semantic_keys"),
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
        if (physicalBodyFollow) {
            comfortMotionIntensity = std::max(comfortMotionIntensity, 0.20f);
        }
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
            "hpl_controller_gameplay_policy frame=%llu suppressed=%d authoredCamera=%d paused=%d pauseGateValid=%d mainMenu=%d nativeCursor=%d currentImGui=%d menuPointer=%d terminalPointer=%d rotateMode=%d cameraUpdateActive=%d playerState=%d moveState=%d",
            static_cast<unsigned long long>(frameIndex),
            suppressGameplay ? 1 : 0,
            suppressForAuthoredCamera ? 1 : 0,
            paused ? 1 : 0,
            pauseStateValid ? 1 : 0,
            mainMenuSurface ? 1 : 0,
            nativeMenuCursorVisible ? 1 : 0,
            currentImGuiSurface ? 1 : 0,
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
            g_state.semanticAnalogMovementActive
                ? "native_player_analog"
                : (g_state.nativeMovementActive ? "native_body" : "semantic_keys"),
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
    if (g_playerHelperUpdateTarget != nullptr) {
        MH_DisableHook(g_playerHelperUpdateTarget);
        MH_RemoveHook(g_playerHelperUpdateTarget);
    }
    g_openxr = nullptr;
    g_playerHelperUpdateTarget = nullptr;
    g_originalPlayerHelperUpdate = nullptr;
    g_playerAnalogInputTarget = nullptr;
    g_originalPlayerAnalogInput = nullptr;
    ClearPendingManipulationAnalog();
    ClearPendingMovementAnalog();
    g_virtualTorsoYawRadians.store(0.0f, std::memory_order_relaxed);
    g_virtualTorsoYawValid.store(false, std::memory_order_release);
    g_state = {};
    Logger::Instance().Write(LogLevel::Info, "hpl_input_bridge removed");
}

void LogHPLInputBridgeSummary()
{
    HPLPlayerStateSnapshot player;
    GetHPLPlayerStateSnapshot(player);
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_physical_body_follow_summary configured=%d entries=%llu steps=%llu turnFailures=%llu active=%d anchorValid=%d virtualTorsoValid=%d thresholdDegrees=%.2f releaseDegrees=%.2f speedDegreesPerSecond=%.2f delayMs=%d policy=no_game_camera_mutation",
        g_config.hplControllerPhysicalBodyFollow ? 1 : 0,
        static_cast<unsigned long long>(g_bodyFollowEntries.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_bodyFollowSteps.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_bodyFollowTurnFailures.load(std::memory_order_relaxed)),
        g_state.bodyFollowActive ? 1 : 0,
        g_state.bodyFollowAnchorValid ? 1 : 0,
        g_virtualTorsoYawValid.load(std::memory_order_acquire) ? 1 : 0,
        g_config.hplControllerPhysicalBodyFollowThresholdDegrees,
        g_config.hplControllerPhysicalBodyFollowReleaseDegrees,
        g_config.hplControllerPhysicalBodyFollowDegreesPerSecond,
        g_config.hplControllerPhysicalBodyFollowDelayMs);
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_input_bridge_summary installed=%d updates=%llu activeUpdates=%llu sentEvents=%llu sendFailures=%llu staleInputFrames=%llu loadingSuppressedFrames=%llu recenterRequests=%llu hapticRequests=%llu hapticApplied=%llu oneHandFallbackFrames=%llu worldAimPoseSamples=%llu worldGripPoseSamples=%llu nativeMovementFrames=%llu movementNativeDispatches=%llu movementStaleDrops=%llu movementContextDeferrals=%llu headRelativeMovementFrames=%llu controllerRelativeMovementFrames=%llu controllerReferenceAttempts=%llu controllerReferenceApplied=%llu controllerReferenceFallbacks=%llu controllerDirectionSamples=%llu controllerAimGuideFrames=%llu inspectionExitActions=%llu nativeTurnEvents=%llu semanticMovementFallbackFrames=%llu physicalCrouchEntries=%llu physicalCrouchExits=%llu manipulationRotateFrames=%llu manipulationMotionFrames=%llu manipulationMotionEntries=%llu manipulationMotionEvents=%llu manipulationMotionTrackingLosses=%llu manipulationMotionSessionSummaries=%llu manipulationMotionPixels=%lld,%lld manipulationNativeDispatches=%llu manipulationMouseFallbacks=%llu manipulationStaleDrops=%llu manipulationContextDeferrals=%llu nativeThrowActions=%llu flashlightActions=%llu inventoryActions=%llu pausedFrames=%llu menuPointerFrames=%llu terminalPointerFrames=%llu terminalLookAwayExits=%llu gameOverContinueActions=%llu playerStateTransitions=%llu authoredCameraTransitions=%llu playerStateBlackouts=%llu gameplaySuppressed=%d paused=%d menuPointerActive=%d terminalPointerActive=%d physicalCrouch=%d rotate=%d manipulationMotionActive=%d manipulationMotionState=%d manipulationMotionLast=%d,%d player=%p camera=%p body=%p playerState=%d moveState=%d",
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
        static_cast<unsigned long long>(g_movementNativeAnalogDispatches.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_movementAnalogStaleDrops.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_movementAnalogContextDeferrals.load(std::memory_order_relaxed)),
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
        static_cast<unsigned long long>(g_manipulationNativeAnalogDispatches.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_manipulationMouseFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_manipulationAnalogStaleDrops.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_manipulationAnalogContextDeferrals.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_nativeThrowActions.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_flashlightActions.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_inventoryActions.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_pausedFrames.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_menuPointerFrames.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_terminalPointerFrames.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_terminalLookAwayExits.load(std::memory_order_relaxed)),
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
