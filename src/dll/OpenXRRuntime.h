#pragma once

#include <Windows.h>

#include <cstdint>
#include <memory>
#include <string>

namespace somavr {

struct OpenXRHeadPose {
    bool valid = false;
    uint64_t gameFrame = 0;
    float positionX = 0.0f;
    float positionY = 0.0f;
    float positionZ = 0.0f;
    float orientationX = 0.0f;
    float orientationY = 0.0f;
    float orientationZ = 0.0f;
    float orientationW = 1.0f;
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
        int resolutionScalePercent);
    void OnOpenGLContext(HDC deviceContext, HGLRC glContext);
    void OnFrameBoundary(HDC deviceContext, HGLRC glContext, uint64_t frameIndex);
    bool RequestManualStart();
    void Shutdown();

    std::string SummaryString() const;
    std::string ViewSummaryString() const;
    bool GetLatestHeadPose(OpenXRHeadPose& pose) const;
    bool GetLatestStereoViews(OpenXRStereoViewSnapshot& views) const;
    void SetStereoSubmissionEnabled(bool enabled);
    bool MarkRenderedStereoEye(uint32_t eyeIndex, const OpenXREyeView& view);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace somavr
