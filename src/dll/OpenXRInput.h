#pragma once

#include "OpenXRRuntime.h"

#include <openxr/openxr.h>

#include <cstdint>
#include <string>

namespace somavr {

class OpenXRInput {
public:
    bool Initialize(XrInstance instance, bool enabled, int logInterval);
    bool AttachSession(XrSession session);
    void Sync(XrSession session, XrSpace baseSpace, XrTime displayTime, uint64_t gameFrame);
    bool ApplyHaptic(XrSession session, uint32_t hand, float amplitude, int durationMs);
    void ShutdownSession();
    void Shutdown();

    OpenXRInputSnapshot Snapshot() const;
    std::string SummaryString() const;

private:
    bool SuggestBindings(XrInstance instance, const char* profile, const XrActionSuggestedBinding* bindings, uint32_t count);
    bool CreatePoseSpace(XrSession session, XrAction action, XrPath hand, XrSpace& space);

    bool enabled_ = false;
    bool initialized_ = false;
    bool attached_ = false;
    int logInterval_ = 120;
    uint64_t syncCount_ = 0;
    uint64_t syncFailureCount_ = 0;
    uint64_t focusLossCount_ = 0;
    uint64_t focusRestoreCount_ = 0;
    uint64_t hapticRequestCount_ = 0;
    uint64_t hapticFailureCount_ = 0;
    uint64_t gripLinearVelocitySamples_[2] = {};
    uint64_t gripAngularVelocitySamples_[2] = {};
    bool focusSuppressed_ = false;
    XrInstance instance_ = XR_NULL_HANDLE;
    XrActionSet actionSet_ = XR_NULL_HANDLE;
    XrAction moveAction_ = XR_NULL_HANDLE;
    XrAction turnAction_ = XR_NULL_HANDLE;
    XrAction selectAction_ = XR_NULL_HANDLE;
    XrAction triggerAction_ = XR_NULL_HANDLE;
    XrAction squeezeAction_ = XR_NULL_HANDLE;
    XrAction menuAction_ = XR_NULL_HANDLE;
    XrAction jumpAction_ = XR_NULL_HANDLE;
    XrAction crouchAction_ = XR_NULL_HANDLE;
    XrAction gripPoseAction_ = XR_NULL_HANDLE;
    XrAction aimPoseAction_ = XR_NULL_HANDLE;
    XrAction hapticAction_ = XR_NULL_HANDLE;
    XrPath handPaths_[2] = {XR_NULL_PATH, XR_NULL_PATH};
    XrSpace gripSpaces_[2] = {XR_NULL_HANDLE, XR_NULL_HANDLE};
    XrSpace aimSpaces_[2] = {XR_NULL_HANDLE, XR_NULL_HANDLE};
    OpenXRInputSnapshot snapshot_{};
};

} // namespace somavr
