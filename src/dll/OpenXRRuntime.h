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

struct OpenXRTerminalPointerState {
    bool valid = false;
    uint64_t gameFrame = 0;
    float normalizedX = 0.5f;
    float normalizedY = 0.5f;
    float sizeScale = 1.0f;
};

struct OpenXRControllerAimGuideState {
    bool valid = false;
    uint64_t gameFrame = 0;
    uint32_t handIndex = 1;
    float lengthMeters = 1.2f;
    float alpha = 0.05f;
    bool interactable = false;
    OpenXRControllerPose aimPose{};
};

struct OpenXRStatusPanelState {
    bool visible = false;
    int selectedAction = 0;
    bool trackingEnabled = false;
    bool stereoEnabled = false;
    bool roomscaleEnabled = false;
    bool projectionCentered = false;
    bool dualRenderReady = false;
    bool continuousDualRender = false;
    bool viewHistoryConfigured = false;
    bool viewHistoryActive = false;
    bool viewHistoryFaulted = false;
    bool hudVisible = false;
    bool reticleVisible = false;
    bool inputAvailable = false;
    bool controllerTracked = false;
    bool authoredCameraActive = false;
    int playerState = -1;
    uint64_t gameFrame = 0;
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

struct OpenXRHudLayerShapeStatus {
    bool cylinderAvailable = false;
    bool cylinderActive = false;
};

struct OpenXRComfortVignetteSettings {
    bool enabled = false;
    int sizePixels = 256;
    float distanceMeters = 0.30f;
    float widthMeters = 1.0f;
    float strength = 0.60f;
    float innerRadius = 0.50f;
    int fadeMilliseconds = 250;
    int maxMotionAgeFrames = 8;
};

struct OpenXRFoveationSettings {
    bool enabled = false;
    int level = 2;
    bool dynamic = false;
    float verticalOffset = 0.0f;
};

struct OpenXRComfortVignetteStatus {
    bool available = false;
    bool enabled = false;
    bool active = false;
    float level = 0.0f;
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
        bool depthCompositionSubmit,
        const OpenXRFoveationSettings& foveation,
        int resolutionScalePercent,
        const std::string& referenceSpace,
        bool inputEnabled,
        int inputLogInterval,
        bool recoveryEnabled,
        int recoveryDelayFrames,
        int trackingHoldFrames,
        int trackingRecoveryBlackoutFrames,
        bool hudLayerEnabled,
        const std::string& hudShape,
        float hudCylinderAngleDegrees,
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
        int interactionReticleMaxAgeFrames,
        bool statusPanelEnabled,
        int statusPanelWidthPixels,
        int statusPanelHeightPixels,
        float statusPanelDistanceMeters,
        float statusPanelWidthMeters,
        float statusPanelVerticalOffsetMeters,
        const OpenXRComfortVignetteSettings& comfortVignette);
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
    bool StopHaptic(uint32_t hand, const char* reason);
    void SetStereoSubmissionEnabled(bool enabled);
    bool MarkRenderedStereoEye(uint32_t eyeIndex, const OpenXREyeView& view);
    bool CapturePendingStereoEye(uint64_t frameIndex, const char* source);
    void InvalidateStereoCaches(const char* reason);
    void RequestComfortBlackout(uint32_t frames, const char* reason);
    void SetPresentationBlackout(bool active, const char* reason);
    bool BeginHudCapture(uint64_t frameIndex, bool preservePreviousFrame = false);
    bool EndHudCapture(uint64_t frameIndex, bool suppressCenterCrosshair);
    bool BeginTerminalHudCapture(
        uint64_t frameIndex,
        int width,
        int height,
        bool preservePreviousFrame,
        bool preserveDirtyRects);
    bool EndTerminalHudCapture(uint64_t frameIndex);
    bool CaptureFramebufferToHud(
        uint64_t frameIndex,
        uint32_t sourceFramebuffer,
        int sourceX,
        int sourceY,
        int sourceWidth,
        int sourceHeight);
    bool DumpHudCapture(
        uint64_t frameIndex,
        uint64_t sequence,
        uint32_t sampleIndex,
        const char* reason);
    bool DumpTerminalHudCapture(
        uint64_t frameIndex,
        uint64_t sequence,
        uint32_t sampleIndex,
        const char* reason);
    void SetInteractionReticle(const OpenXRInteractionReticleState& state);
    void SetInteractionReticleSemantic(int crosshairState);
    void ClearInteractionReticle();
    void SetTerminalPointer(const OpenXRTerminalPointerState& state);
    void ClearTerminalPointer();
    void SetControllerAimGuide(const OpenXRControllerAimGuideState& state);
    void ClearControllerAimGuide(uint32_t handIndex);
    void ClearControllerAimGuide();
    void SetStatusPanel(const OpenXRStatusPanelState& state);
    void SetHudRuntimeVisible(bool visible);
    void SetDesktopMirrorNativeBackbuffer(bool enabled);
    bool ToggleHudLayerShape();
    OpenXRHudLayerShapeStatus GetHudLayerShapeStatus() const;
    void SetInteractionReticleRuntimeVisible(bool visible);
    void SetComfortMotionIntensity(float intensity, uint64_t gameFrame);
    bool ToggleComfortVignette();
    OpenXRComfortVignetteStatus GetComfortVignetteStatus() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace somavr
