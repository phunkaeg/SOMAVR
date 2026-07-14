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
};

struct OpenXRHandInput {
    bool active = false;
    bool select = false;
    bool selectChanged = false;
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
        int resolutionScalePercent,
        const std::string& referenceSpace,
        bool inputEnabled,
        int inputLogInterval,
        bool recoveryEnabled,
        int recoveryDelayFrames);
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

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace somavr
