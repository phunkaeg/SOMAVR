#pragma once

#include <Windows.h>

#include <cstdint>
#include <memory>
#include <string>

namespace somavr {

struct OpenXRHeadPose {
    bool valid = false;
    bool orientationTracked = false;
    bool positionTracked = false;
    uint64_t gameFrame = 0;
    uint64_t sampleAgeFrames = 0;
    float positionX = 0.0f;
    float positionY = 0.0f;
    float positionZ = 0.0f;
    float orientationX = 0.0f;
    float orientationY = 0.0f;
    float orientationZ = 0.0f;
    float orientationW = 1.0f;
};

struct OpenXRControllerPose {
    bool valid = false;
    bool orientationTracked = false;
    bool positionTracked = false;
    float positionX = 0.0f;
    float positionY = 0.0f;
    float positionZ = 0.0f;
    float orientationX = 0.0f;
    float orientationY = 0.0f;
    float orientationZ = 0.0f;
    float orientationW = 1.0f;
    bool linearVelocityValid = false;
    bool angularVelocityValid = false;
    float linearVelocityX = 0.0f;
    float linearVelocityY = 0.0f;
    float linearVelocityZ = 0.0f;
    float angularVelocityX = 0.0f;
    float angularVelocityY = 0.0f;
    float angularVelocityZ = 0.0f;
};

struct OpenXRHandInput {
    bool active = false;
    bool select = false;
    bool selectChanged = false;
    bool primary = false;
    bool primaryChanged = false;
    bool secondary = false;
    bool secondaryChanged = false;
    float trigger = 0.0f;
    float squeeze = 0.0f;
    OpenXRControllerPose gripPose{};
    OpenXRControllerPose aimPose{};
};

struct OpenXRInputSnapshot {
    bool available = false;
    bool active = false;
    uint64_t gameFrame = 0;
    float moveX = 0.0f;
    float moveY = 0.0f;
    float turnX = 0.0f;
    float turnY = 0.0f;
    bool menu = false;
    bool menuChanged = false;
    bool jump = false;
    bool jumpChanged = false;
    bool crouch = false;
    bool crouchChanged = false;
    OpenXRHandInput left{};
    OpenXRHandInput right{};
};

struct OpenXRInteractionReticleState {
    bool valid = false;
    bool semanticValid = false;
    int semanticState = 0;
    uint64_t gameFrame = 0;
    uint32_t handIndex = 1;
    float distanceMeters = 0.0f;
    OpenXRControllerPose aimPose{};
};

struct OpenXREyeView {
    bool valid = false;
    uint64_t gameFrame = 0;
    float positionX = 0.0f;
    float positionY = 0.0f;
    float positionZ = 0.0f;
    float orientationX = 0.0f;
    float orientationY = 0.0f;
    float orientationZ = 0.0f;
    float orientationW = 1.0f;
    float angleLeft = 0.0f;
    float angleRight = 0.0f;
    float angleUp = 0.0f;
    float angleDown = 0.0f;
};

struct OpenXRStereoViewSnapshot {
    bool valid = false;
    uint64_t gameFrame = 0;
    OpenXRHeadPose head;
    OpenXREyeView eyes[2];
};

class OpenXRRuntime {
public:
    OpenXRRuntime();
    ~OpenXRRuntime();

    void Configure(
        bool enabled,
        bool sessionProbe,
        bool releaseAfterProbe,
        uint64_t bootstrapFrame,
        uint64_t holdFrames,
        bool manualStart,
        bool frameSubmit,
        bool mirrorBackbuffer,
        const std::string& desktopMirrorEye,
        const std::string& desktopMirrorAspect,
        bool depthCompositionProbe,
        int resolutionScalePercent,
        const std::string& referenceSpace,
        bool inputEnabled,
        int inputLogInterval,
        bool recoveryEnabled,
        int recoveryDelayFrames,
        int trackingHoldFrames,
        int trackingRecoveryBlackoutFrames,
        bool hudLayerEnabled,
        int hudWidthPixels,
        int hudHeightPixels,
        float hudDistanceMeters,
        float hudWidthMeters,
        float hudVerticalOffsetMeters,
        int hudMaxAgeFrames,
        bool hudSuppressCenterCrosshair,
        int hudCrosshairClearRadiusPixels,
        bool interactionReticleEnabled,
        bool interactionReticleSemanticEnabled,
        bool interactionReticleNativeIconsEnabled,
        int interactionReticleSizePixels,
        float interactionReticleAngularSizeDegrees,
        float interactionReticleMinSizeMeters,
        float interactionReticleMaxSizeMeters,
        float interactionReticleMinDistanceMeters,
        float interactionReticleMaxDistanceMeters,
        int interactionReticleMaxAgeFrames);
    void OnOpenGLContext(HDC deviceContext, HGLRC glContext);
    void OnFrameBoundary(HDC deviceContext, HGLRC glContext, uint64_t frameIndex);
    bool RequestManualStart();
    void Shutdown();

    std::string SummaryString() const;
    std::string ViewSummaryString() const;
    bool GetLatestHeadPose(OpenXRHeadPose& pose) const;
    bool GetLatestStereoViews(OpenXRStereoViewSnapshot& views) const;
    bool GetLatestInput(OpenXRInputSnapshot& input) const;
    bool RequestHapticPulse(uint32_t hand, float amplitude, int durationMs, const char* reason);
    void SetStereoSubmissionEnabled(bool enabled);
    bool MarkRenderedStereoEye(uint32_t eyeIndex, const OpenXREyeView& view);
    void InvalidateStereoCaches(const char* reason);
    void RequestComfortBlackout(uint32_t frames, const char* reason);
    void SetPresentationBlackout(bool active, const char* reason);
    bool BeginHudCapture(uint64_t frameIndex);
    bool EndHudCapture(uint64_t frameIndex);
    void SetInteractionReticle(const OpenXRInteractionReticleState& state);
    void SetInteractionReticleSemantic(int crosshairState);
    void ClearInteractionReticle();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace somavr
