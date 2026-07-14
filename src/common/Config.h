#pragma once

#include "Logger.h"

#include <filesystem>
#include <string>

namespace somavr {

struct Config {
    LogLevel logLevel = LogLevel::Info;

    bool hookSwapBuffers = true;
    bool hookWglMakeCurrent = true;
    bool hookFixedFunctionMatrices = true;
    bool hookUniformMatrices = true;
    bool hookViewport = true;
    bool hookDrawCalls = true;
    bool hookFramebuffer = true;

    bool openxrProbe = false;
    bool openxrSessionProbe = false;
    bool openxrReleaseAfterProbe = true;
    int openxrBootstrapFrame = 0;
    int openxrHoldFrames = 0;
    bool openxrManualStart = false;
    bool openxrFrameSubmit = false;
    bool openxrMirrorBackbuffer = true;
    int openxrResolutionScalePercent = 100;
    bool openxrInputEnabled = false;
    int openxrInputLogInterval = 120;
    bool forceDisableVsync = false;

    int frameSummaryInterval = 120;
    int matrixSampleLimitPerFrame = 32;
    int uniformNameLogLimit = 256;
    int uniformMatrixLogLimit = 256;
    bool uniformMatrixProjectionOnly = true;
    bool matrixCaptureEnabled = false;
    int matrixCaptureFrames = 120;
    int matrixCaptureStackDepth = 8;
    int matrixCaptureMaxSites = 64;
    int matrixCaptureSamplesPerUniform = 4;
    bool renderDiagnosticCapture = false;
    int renderDiagnosticFrames = 4;
    int renderDiagnosticMaxPrograms = 128;
    int renderDiagnosticMaxDraws = 8192;
    bool hplCameraBridge = false;
    bool hplLifecycleShutdown = false;
    bool hplProjectionCenterControl = false;
    bool hplProjectionCenteredDefault = false;
    bool hplRoomscaleControl = false;
    bool hplRoomscaleEnabledDefault = true;
    bool hplRoomscaleVertical = true;
    float hplEyeHeightOffsetMeters = 0.0f;
    bool hplRecenterControl = false;
    bool hplReflectionFadeControl = false;
    int hplCameraLogInterval = 120;
    bool hplStereoAfr = false;
    float hplWorldScale = 1.0f;
    bool hplRenderStageProbe = false;
    bool hplAudioListenerProbe = false;
    bool hplAudioListenerCorrection = false;
    bool hplPostEffectControl = false;
    bool hplPostEffectBypassDefault = false;
    bool hplShadowJitterControl = false;
    bool hplShadowJitterSuppressedDefault = false;
    int hplCompatibilityLogInterval = 120;
};

class ConfigManager {
public:
    bool Initialize();

    const Config& Get() const;
    const std::filesystem::path& Path() const;

private:
    void WriteDefaultConfig() const;
    void LoadFromFile();

    Config config_;
    std::filesystem::path path_;
};

} // namespace somavr
