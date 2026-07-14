#include "OpenXRInput.h"

#include "Logger.h"
#include "OpenXRHelpers.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <iomanip>
#include <sstream>

namespace somavr {
namespace {

using xr_helpers::XrResultString;

bool StringToPath(XrInstance instance, const char* text, XrPath& path)
{
    const XrResult result = xrStringToPath(instance, text, &path);
    if (XR_FAILED(result)) {
        Logger::Instance().Write(
            LogLevel::Warn,
            "openxr_input path_failed path=%s result=%s",
            text,
            XrResultString(result).c_str());
        return false;
    }
    return true;
}

bool CreateAction(
    XrActionSet actionSet,
    XrActionType type,
    const char* name,
    const char* localizedName,
    const XrPath* subactionPaths,
    uint32_t subactionCount,
    XrAction& action)
{
    XrActionCreateInfo info{XR_TYPE_ACTION_CREATE_INFO};
    info.actionType = type;
    strcpy_s(info.actionName, name);
    strcpy_s(info.localizedActionName, localizedName);
    info.countSubactionPaths = subactionCount;
    info.subactionPaths = subactionPaths;
    const XrResult result = xrCreateAction(actionSet, &info, &action);
    if (XR_FAILED(result)) {
        Logger::Instance().Write(
            LogLevel::Warn,
            "openxr_input create_action_failed action=%s result=%s",
            name,
            XrResultString(result).c_str());
        return false;
    }
    return true;
}

void ReadVector2(XrSession session, XrAction action, XrPath hand, float& x, float& y, bool& active)
{
    XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
    getInfo.action = action;
    getInfo.subactionPath = hand;
    XrActionStateVector2f state{XR_TYPE_ACTION_STATE_VECTOR2F};
    if (XR_SUCCEEDED(xrGetActionStateVector2f(session, &getInfo, &state)) && state.isActive == XR_TRUE) {
        x = state.currentState.x;
        y = state.currentState.y;
        active = true;
    }
}

void ReadBoolean(XrSession session, XrAction action, XrPath hand, bool& value, bool& changed, bool& active)
{
    XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
    getInfo.action = action;
    getInfo.subactionPath = hand;
    XrActionStateBoolean state{XR_TYPE_ACTION_STATE_BOOLEAN};
    if (XR_SUCCEEDED(xrGetActionStateBoolean(session, &getInfo, &state)) && state.isActive == XR_TRUE) {
        value = state.currentState == XR_TRUE;
        changed = state.changedSinceLastSync == XR_TRUE;
        active = true;
    }
}

void ReadFloat(XrSession session, XrAction action, XrPath hand, float& value, bool& active)
{
    XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
    getInfo.action = action;
    getInfo.subactionPath = hand;
    XrActionStateFloat state{XR_TYPE_ACTION_STATE_FLOAT};
    if (XR_SUCCEEDED(xrGetActionStateFloat(session, &getInfo, &state)) && state.isActive == XR_TRUE) {
        value = state.currentState;
        active = true;
    }
}

void LocatePose(XrSpace actionSpace, XrSpace baseSpace, XrTime time, OpenXRControllerPose& pose)
{
    if (actionSpace == XR_NULL_HANDLE || baseSpace == XR_NULL_HANDLE) {
        return;
    }
    XrSpaceLocation location{XR_TYPE_SPACE_LOCATION};
    if (XR_FAILED(xrLocateSpace(actionSpace, baseSpace, time, &location))) {
        return;
    }
    const XrSpaceLocationFlags validFlags =
        XR_SPACE_LOCATION_ORIENTATION_VALID_BIT | XR_SPACE_LOCATION_POSITION_VALID_BIT;
    pose.valid = (location.locationFlags & validFlags) == validFlags;
    pose.orientationTracked = (location.locationFlags & XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT) != 0;
    pose.positionTracked = (location.locationFlags & XR_SPACE_LOCATION_POSITION_TRACKED_BIT) != 0;
    pose.positionX = location.pose.position.x;
    pose.positionY = location.pose.position.y;
    pose.positionZ = location.pose.position.z;
    pose.orientationX = location.pose.orientation.x;
    pose.orientationY = location.pose.orientation.y;
    pose.orientationZ = location.pose.orientation.z;
    pose.orientationW = location.pose.orientation.w;
}

} // namespace

bool OpenXRInput::Initialize(XrInstance instance, bool enabled, int logInterval)
{
    enabled_ = enabled;
    logInterval_ = std::max(logInterval, 1);
    instance_ = instance;
    if (!enabled_) {
        return true;
    }

    if (!StringToPath(instance_, "/user/hand/left", handPaths_[0])
        || !StringToPath(instance_, "/user/hand/right", handPaths_[1])) {
        return false;
    }

    XrActionSetCreateInfo setInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
    strcpy_s(setInfo.actionSetName, "somavr_gameplay");
    strcpy_s(setInfo.localizedActionSetName, "SOMAVR Gameplay");
    setInfo.priority = 0;
    XrResult result = xrCreateActionSet(instance_, &setInfo, &actionSet_);
    if (XR_FAILED(result)) {
        Logger::Instance().Write(LogLevel::Warn, "openxr_input create_action_set_failed result=%s", XrResultString(result).c_str());
        return false;
    }

    const bool actionsOk =
        CreateAction(actionSet_, XR_ACTION_TYPE_VECTOR2F_INPUT, "move", "Move", &handPaths_[0], 1, moveAction_)
        && CreateAction(actionSet_, XR_ACTION_TYPE_VECTOR2F_INPUT, "turn", "Turn", &handPaths_[1], 1, turnAction_)
        && CreateAction(actionSet_, XR_ACTION_TYPE_BOOLEAN_INPUT, "select", "Select", handPaths_, 2, selectAction_)
        && CreateAction(actionSet_, XR_ACTION_TYPE_FLOAT_INPUT, "trigger", "Trigger", handPaths_, 2, triggerAction_)
        && CreateAction(actionSet_, XR_ACTION_TYPE_FLOAT_INPUT, "squeeze", "Squeeze", handPaths_, 2, squeezeAction_)
        && CreateAction(actionSet_, XR_ACTION_TYPE_BOOLEAN_INPUT, "menu", "Menu", &handPaths_[0], 1, menuAction_)
        && CreateAction(actionSet_, XR_ACTION_TYPE_BOOLEAN_INPUT, "jump", "Primary Action", handPaths_, 2, jumpAction_)
        && CreateAction(actionSet_, XR_ACTION_TYPE_BOOLEAN_INPUT, "crouch", "Secondary Action", handPaths_, 2, crouchAction_)
        && CreateAction(actionSet_, XR_ACTION_TYPE_POSE_INPUT, "grip_pose", "Grip Pose", handPaths_, 2, gripPoseAction_)
        && CreateAction(actionSet_, XR_ACTION_TYPE_POSE_INPUT, "aim_pose", "Aim Pose", handPaths_, 2, aimPoseAction_)
        && CreateAction(actionSet_, XR_ACTION_TYPE_VIBRATION_OUTPUT, "haptic", "Haptic", handPaths_, 2, hapticAction_);
    if (!actionsOk) {
        Shutdown();
        return false;
    }

    auto path = [&](const char* value) {
        XrPath resultPath = XR_NULL_PATH;
        StringToPath(instance_, value, resultPath);
        return resultPath;
    };
    const std::array<XrActionSuggestedBinding, 9> simpleBindings{{
        {selectAction_, path("/user/hand/left/input/select/click")},
        {selectAction_, path("/user/hand/right/input/select/click")},
        {menuAction_, path("/user/hand/left/input/menu/click")},
        {gripPoseAction_, path("/user/hand/left/input/grip/pose")},
        {gripPoseAction_, path("/user/hand/right/input/grip/pose")},
        {aimPoseAction_, path("/user/hand/left/input/aim/pose")},
        {aimPoseAction_, path("/user/hand/right/input/aim/pose")},
        {hapticAction_, path("/user/hand/left/output/haptic")},
        {hapticAction_, path("/user/hand/right/output/haptic")},
    }};
    SuggestBindings(instance_, "/interaction_profiles/khr/simple_controller", simpleBindings.data(), static_cast<uint32_t>(simpleBindings.size()));

    const std::array<XrActionSuggestedBinding, 19> touchBindings{{
        {moveAction_, path("/user/hand/left/input/thumbstick")},
        {turnAction_, path("/user/hand/right/input/thumbstick")},
        {selectAction_, path("/user/hand/left/input/trigger/click")},
        {selectAction_, path("/user/hand/right/input/trigger/click")},
        {triggerAction_, path("/user/hand/left/input/trigger/value")},
        {triggerAction_, path("/user/hand/right/input/trigger/value")},
        {squeezeAction_, path("/user/hand/left/input/squeeze/value")},
        {squeezeAction_, path("/user/hand/right/input/squeeze/value")},
        {menuAction_, path("/user/hand/left/input/menu/click")},
        {jumpAction_, path("/user/hand/left/input/x/click")},
        {crouchAction_, path("/user/hand/left/input/y/click")},
        {jumpAction_, path("/user/hand/right/input/a/click")},
        {crouchAction_, path("/user/hand/right/input/b/click")},
        {gripPoseAction_, path("/user/hand/left/input/grip/pose")},
        {gripPoseAction_, path("/user/hand/right/input/grip/pose")},
        {aimPoseAction_, path("/user/hand/left/input/aim/pose")},
        {aimPoseAction_, path("/user/hand/right/input/aim/pose")},
        {hapticAction_, path("/user/hand/left/output/haptic")},
        {hapticAction_, path("/user/hand/right/output/haptic")},
    }};
    SuggestBindings(instance_, "/interaction_profiles/oculus/touch_controller", touchBindings.data(), static_cast<uint32_t>(touchBindings.size()));

    const std::array<XrActionSuggestedBinding, 18> indexBindings{{
        {moveAction_, path("/user/hand/left/input/thumbstick")},
        {turnAction_, path("/user/hand/right/input/thumbstick")},
        {selectAction_, path("/user/hand/left/input/trigger/click")},
        {selectAction_, path("/user/hand/right/input/trigger/click")},
        {triggerAction_, path("/user/hand/left/input/trigger/value")},
        {triggerAction_, path("/user/hand/right/input/trigger/value")},
        {squeezeAction_, path("/user/hand/left/input/squeeze/force")},
        {squeezeAction_, path("/user/hand/right/input/squeeze/force")},
        {jumpAction_, path("/user/hand/left/input/a/click")},
        {crouchAction_, path("/user/hand/left/input/b/click")},
        {jumpAction_, path("/user/hand/right/input/a/click")},
        {crouchAction_, path("/user/hand/right/input/b/click")},
        {gripPoseAction_, path("/user/hand/left/input/grip/pose")},
        {gripPoseAction_, path("/user/hand/right/input/grip/pose")},
        {aimPoseAction_, path("/user/hand/left/input/aim/pose")},
        {aimPoseAction_, path("/user/hand/right/input/aim/pose")},
        {hapticAction_, path("/user/hand/left/output/haptic")},
        {hapticAction_, path("/user/hand/right/output/haptic")},
    }};
    SuggestBindings(instance_, "/interaction_profiles/valve/index_controller", indexBindings.data(), static_cast<uint32_t>(indexBindings.size()));

    const std::array<XrActionSuggestedBinding, 11> motionBindings{{
        {moveAction_, path("/user/hand/left/input/thumbstick")},
        {turnAction_, path("/user/hand/right/input/thumbstick")},
        {triggerAction_, path("/user/hand/left/input/trigger/value")},
        {triggerAction_, path("/user/hand/right/input/trigger/value")},
        {menuAction_, path("/user/hand/left/input/menu/click")},
        {gripPoseAction_, path("/user/hand/left/input/grip/pose")},
        {gripPoseAction_, path("/user/hand/right/input/grip/pose")},
        {aimPoseAction_, path("/user/hand/left/input/aim/pose")},
        {aimPoseAction_, path("/user/hand/right/input/aim/pose")},
        {hapticAction_, path("/user/hand/left/output/haptic")},
        {hapticAction_, path("/user/hand/right/output/haptic")},
    }};
    SuggestBindings(instance_, "/interaction_profiles/microsoft/motion_controller", motionBindings.data(), static_cast<uint32_t>(motionBindings.size()));

    initialized_ = true;
    Logger::Instance().Write(LogLevel::Info, "openxr_input initialized actions=11 profiles=4 haptics=1 logInterval=%d", logInterval_);
    return true;
}

bool OpenXRInput::SuggestBindings(
    XrInstance instance,
    const char* profile,
    const XrActionSuggestedBinding* bindings,
    uint32_t count)
{
    XrPath profilePath = XR_NULL_PATH;
    if (!StringToPath(instance, profile, profilePath)) {
        return false;
    }
    XrInteractionProfileSuggestedBinding info{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
    info.interactionProfile = profilePath;
    info.countSuggestedBindings = count;
    info.suggestedBindings = bindings;
    const XrResult result = xrSuggestInteractionProfileBindings(instance, &info);
    Logger::Instance().Write(
        XR_SUCCEEDED(result) ? LogLevel::Info : LogLevel::Warn,
        "openxr_input bindings profile=%s count=%u result=%s",
        profile,
        count,
        XrResultString(result).c_str());
    return XR_SUCCEEDED(result);
}

bool OpenXRInput::CreatePoseSpace(XrSession session, XrAction action, XrPath hand, XrSpace& space)
{
    XrActionSpaceCreateInfo info{XR_TYPE_ACTION_SPACE_CREATE_INFO};
    info.action = action;
    info.subactionPath = hand;
    info.poseInActionSpace.orientation.w = 1.0f;
    return XR_SUCCEEDED(xrCreateActionSpace(session, &info, &space));
}

bool OpenXRInput::AttachSession(XrSession session)
{
    if (!enabled_ || !initialized_) {
        return true;
    }
    XrSessionActionSetsAttachInfo attachInfo{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
    attachInfo.countActionSets = 1;
    attachInfo.actionSets = &actionSet_;
    XrResult result = xrAttachSessionActionSets(session, &attachInfo);
    if (XR_FAILED(result)) {
        Logger::Instance().Write(LogLevel::Warn, "openxr_input attach_failed result=%s", XrResultString(result).c_str());
        return false;
    }

    bool spacesOk = true;
    for (uint32_t hand = 0; hand < 2; ++hand) {
        spacesOk &= CreatePoseSpace(session, gripPoseAction_, handPaths_[hand], gripSpaces_[hand]);
        spacesOk &= CreatePoseSpace(session, aimPoseAction_, handPaths_[hand], aimSpaces_[hand]);
    }
    attached_ = spacesOk;
    Logger::Instance().Write(
        spacesOk ? LogLevel::Info : LogLevel::Warn,
        "openxr_input session_attached attached=%d gripSpaces=%d,%d aimSpaces=%d,%d",
        attached_ ? 1 : 0,
        gripSpaces_[0] != XR_NULL_HANDLE ? 1 : 0,
        gripSpaces_[1] != XR_NULL_HANDLE ? 1 : 0,
        aimSpaces_[0] != XR_NULL_HANDLE ? 1 : 0,
        aimSpaces_[1] != XR_NULL_HANDLE ? 1 : 0);
    return attached_;
}

void OpenXRInput::Sync(XrSession session, XrSpace baseSpace, XrTime displayTime, uint64_t gameFrame)
{
    if (!attached_) {
        return;
    }

    XrActiveActionSet activeSet{};
    activeSet.actionSet = actionSet_;
    XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
    syncInfo.countActiveActionSets = 1;
    syncInfo.activeActionSets = &activeSet;
    const XrResult result = xrSyncActions(session, &syncInfo);
    if (XR_FAILED(result)) {
        ++syncFailureCount_;
        snapshot_ = {};
        snapshot_.available = true;
        snapshot_.gameFrame = gameFrame;
        if (result == XR_SESSION_NOT_FOCUSED) {
            if (!focusSuppressed_) {
                focusSuppressed_ = true;
                ++focusLossCount_;
                Logger::Instance().Write(
                    LogLevel::Info,
                    "openxr_input focus_lost frame=%llu releases=immediate count=%llu",
                    static_cast<unsigned long long>(gameFrame),
                    static_cast<unsigned long long>(focusLossCount_));
            }
        } else if (syncFailureCount_ <= 8) {
            Logger::Instance().Write(
                LogLevel::Warn,
                "openxr_input sync_failed frame=%llu result=%s failures=%llu",
                static_cast<unsigned long long>(gameFrame),
                XrResultString(result).c_str(),
                static_cast<unsigned long long>(syncFailureCount_));
        }
        return;
    }

    if (focusSuppressed_) {
        focusSuppressed_ = false;
        ++focusRestoreCount_;
        Logger::Instance().Write(
            LogLevel::Info,
            "openxr_input focus_restored frame=%llu count=%llu",
            static_cast<unsigned long long>(gameFrame),
            static_cast<unsigned long long>(focusRestoreCount_));
    }

    OpenXRInputSnapshot next{};
    next.available = true;
    next.gameFrame = gameFrame;
    bool moveActive = false;
    bool turnActive = false;
    ReadVector2(session, moveAction_, handPaths_[0], next.moveX, next.moveY, moveActive);
    ReadVector2(session, turnAction_, handPaths_[1], next.turnX, next.turnY, turnActive);
    ReadBoolean(session, selectAction_, handPaths_[0], next.left.select, next.left.selectChanged, next.left.active);
    ReadBoolean(session, selectAction_, handPaths_[1], next.right.select, next.right.selectChanged, next.right.active);
    ReadFloat(session, triggerAction_, handPaths_[0], next.left.trigger, next.left.active);
    ReadFloat(session, triggerAction_, handPaths_[1], next.right.trigger, next.right.active);
    ReadFloat(session, squeezeAction_, handPaths_[0], next.left.squeeze, next.left.active);
    ReadFloat(session, squeezeAction_, handPaths_[1], next.right.squeeze, next.right.active);
    ReadBoolean(session, menuAction_, handPaths_[0], next.menu, next.menuChanged, next.left.active);
    ReadBoolean(session, jumpAction_, handPaths_[0], next.left.primary, next.left.primaryChanged, next.left.active);
    ReadBoolean(session, jumpAction_, handPaths_[1], next.right.primary, next.right.primaryChanged, next.right.active);
    ReadBoolean(session, crouchAction_, handPaths_[0], next.left.secondary, next.left.secondaryChanged, next.left.active);
    ReadBoolean(session, crouchAction_, handPaths_[1], next.right.secondary, next.right.secondaryChanged, next.right.active);
    next.jump = next.left.primary || next.right.primary;
    next.jumpChanged = next.left.primaryChanged || next.right.primaryChanged;
    next.crouch = next.left.secondary || next.right.secondary;
    next.crouchChanged = next.left.secondaryChanged || next.right.secondaryChanged;
    LocatePose(gripSpaces_[0], baseSpace, displayTime, next.left.gripPose);
    LocatePose(gripSpaces_[1], baseSpace, displayTime, next.right.gripPose);
    LocatePose(aimSpaces_[0], baseSpace, displayTime, next.left.aimPose);
    LocatePose(aimSpaces_[1], baseSpace, displayTime, next.right.aimPose);
    next.left.active |= next.left.gripPose.valid || next.left.aimPose.valid;
    next.right.active |= next.right.gripPose.valid || next.right.aimPose.valid;
    next.active = moveActive || turnActive || next.left.active || next.right.active;
    snapshot_ = next;
    ++syncCount_;

    if (syncCount_ == 1 || syncCount_ % static_cast<uint64_t>(logInterval_) == 0
        || next.left.selectChanged || next.right.selectChanged || next.menuChanged
        || next.jumpChanged || next.crouchChanged) {
        Logger::Instance().Write(
            LogLevel::Info,
            "openxr_input state frame=%llu active=%d move=%.3f,%.3f turn=%.3f,%.3f select=%d,%d trigger=%.3f,%.3f squeeze=%.3f,%.3f primary=%d,%d secondary=%d,%d menu=%d gripValid=%d,%d aimValid=%d,%d",
            static_cast<unsigned long long>(gameFrame),
            next.active ? 1 : 0,
            next.moveX,
            next.moveY,
            next.turnX,
            next.turnY,
            next.left.select ? 1 : 0,
            next.right.select ? 1 : 0,
            next.left.trigger,
            next.right.trigger,
            next.left.squeeze,
            next.right.squeeze,
            next.left.primary ? 1 : 0,
            next.right.primary ? 1 : 0,
            next.left.secondary ? 1 : 0,
            next.right.secondary ? 1 : 0,
            next.menu ? 1 : 0,
            next.left.gripPose.valid ? 1 : 0,
            next.right.gripPose.valid ? 1 : 0,
            next.left.aimPose.valid ? 1 : 0,
            next.right.aimPose.valid ? 1 : 0);
    }
}

bool OpenXRInput::ApplyHaptic(XrSession session, uint32_t hand, float amplitude, int durationMs)
{
    if (!attached_ || session == XR_NULL_HANDLE || hand >= 2 || hapticAction_ == XR_NULL_HANDLE) {
        return false;
    }

    XrHapticActionInfo actionInfo{XR_TYPE_HAPTIC_ACTION_INFO};
    actionInfo.action = hapticAction_;
    actionInfo.subactionPath = handPaths_[hand];
    XrHapticVibration vibration{XR_TYPE_HAPTIC_VIBRATION};
    vibration.duration = static_cast<XrDuration>(std::max(durationMs, 1)) * 1000000;
    vibration.frequency = XR_FREQUENCY_UNSPECIFIED;
    vibration.amplitude = std::clamp(amplitude, 0.0f, 1.0f);
    const XrResult result = xrApplyHapticFeedback(
        session,
        &actionInfo,
        reinterpret_cast<const XrHapticBaseHeader*>(&vibration));
    ++hapticRequestCount_;
    if (XR_FAILED(result)) {
        ++hapticFailureCount_;
        if (hapticFailureCount_ <= 8) {
            Logger::Instance().Write(
                LogLevel::Warn,
                "openxr_input haptic_failed hand=%u result=%s failures=%llu",
                hand,
                XrResultString(result).c_str(),
                static_cast<unsigned long long>(hapticFailureCount_));
        }
        return false;
    }
    return true;
}

void OpenXRInput::ShutdownSession()
{
    for (XrSpace& space : gripSpaces_) {
        if (space != XR_NULL_HANDLE) xrDestroySpace(space);
        space = XR_NULL_HANDLE;
    }
    for (XrSpace& space : aimSpaces_) {
        if (space != XR_NULL_HANDLE) xrDestroySpace(space);
        space = XR_NULL_HANDLE;
    }
    attached_ = false;
    focusSuppressed_ = false;
    snapshot_ = {};
}

void OpenXRInput::Shutdown()
{
    ShutdownSession();
    if (actionSet_ != XR_NULL_HANDLE) {
        xrDestroyActionSet(actionSet_);
        actionSet_ = XR_NULL_HANDLE;
    }
    initialized_ = false;
    instance_ = XR_NULL_HANDLE;
}

OpenXRInputSnapshot OpenXRInput::Snapshot() const
{
    return snapshot_;
}

std::string OpenXRInput::SummaryString() const
{
    std::ostringstream oss;
    oss << "openxrInputEnabled=" << (enabled_ ? 1 : 0)
        << " openxrInputInitialized=" << (initialized_ ? 1 : 0)
        << " openxrInputAttached=" << (attached_ ? 1 : 0)
        << " openxrInputSyncs=" << syncCount_
        << " openxrInputSyncFailures=" << syncFailureCount_
        << " openxrInputFocusLosses=" << focusLossCount_
        << " openxrInputFocusRestores=" << focusRestoreCount_
        << " openxrHapticRequests=" << hapticRequestCount_
        << " openxrHapticFailures=" << hapticFailureCount_
        << " openxrInputLastFrame=" << snapshot_.gameFrame
        << " openxrInputActive=" << (snapshot_.active ? 1 : 0);
    return oss.str();
}

} // namespace somavr
