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
    std::string openxrReferenceSpace = "local";
    bool openxrInputEnabled = false;
    int openxrInputLogInterval = 120;
    bool openxrRecoveryEnabled = true;
    int openxrRecoveryDelayFrames = 120;
    int openxrTrackingHoldFrames = 30;
    int openxrTrackingRecoveryBlackoutFrames = 2;
    bool openxrHudLayer = false;
    int openxrHudWidthPixels = 1600;
    int openxrHudHeightPixels = 900;
    float openxrHudDistanceMeters = 1.5f;
    float openxrHudWidthMeters = 1.6f;
    float openxrHudVerticalOffsetMeters = 0.0f;
    int openxrHudMaxAgeFrames = 2;
    bool openxrHudSuppressCenterCrosshair = false;
    int openxrHudCrosshairClearRadiusPixels = 48;
    bool hplControllerInput = false;
    float hplControllerMoveDeadzone = 0.35f;
    float hplControllerMoveReleaseDeadzone = 0.25f;
    bool hplControllerNativeLocomotion = true;
    std::string hplControllerMovementReference = "body";
    bool hplControllerPhysicalCrouch = false;
    float hplControllerPhysicalCrouchEnterMeters = 0.35f;
    float hplControllerPhysicalCrouchExitMeters = 0.25f;
    bool hplControllerSnapTurn = true;
    float hplControllerTurnDeadzone = 0.65f;
    float hplControllerTurnReleaseDeadzone = 0.35f;
    int hplControllerSnapTurnPixels = 420;
    float hplControllerSmoothTurnPixelsPerSecond = 900.0f;
    bool hplControllerNativeTurn = true;
    float hplControllerSnapTurnDegrees = 30.0f;
    float hplControllerSmoothTurnDegreesPerSecond = 120.0f;
    float hplControllerNativeTurnSign = -1.0f;
    bool hplControllerInteraction = true;
    bool hplControllerFlashlight = true;
    bool hplControllerInventory = true;
    bool hplControllerMenu = true;
    bool hplControllerMenuPointer = true;
    float hplControllerMenuPointerHorizontalDegrees = 70.0f;
    float hplControllerMenuPointerVerticalDegrees = 50.0f;
    float hplControllerMenuPointerSmoothing = 0.35f;
    bool hplControllerRecenterChord = true;
    bool hplControllerHaptics = true;
    float hplControllerHapticAmplitude = 0.35f;
    int hplControllerHapticDurationMs = 30;
    std::string hplControllerDominantHand = "right";
    bool hplControllerSwapSticks = false;
    bool hplControllerOneHandFallback = true;
    bool hplControllerSuppressDuringAuthoredCamera = true;
    bool hplControllerInteractionRay = false;
    float hplControllerInteractionRayOriginTolerance = 0.75f;
    bool hplControllerGrabTranslation = false;
    float hplControllerGrabTranslationScale = 1.0f;
    float hplControllerGrabMaxOffsetMeters = 0.75f;
    bool hplControllerGrabRotation = false;
    float hplControllerGrabRotationGain = 100.0f;
    float hplControllerGrabRotationSign = 1.0f;
    float hplControllerGrabMaxAngularSpeed = 6.0f;
    bool hplControllerThrowRedirect = false;
    bool hplControllerThrowVelocityScale = false;
    float hplControllerThrowVelocityThreshold = 0.35f;
    float hplControllerThrowVelocityReference = 2.0f;
    bool hplControllerManipulationMappings = true;
    bool hplHandTrackingProbe = false;
    bool hplHandControllerRoot = false;
    float hplHandRootOffsetX = 0.0f;
    float hplHandRootOffsetY = -0.075f;
    float hplHandRootOffsetZ = 0.0f;
    float hplHandRootPitchDegrees = 0.0f;
    float hplHandRootYawDegrees = 0.0f;
    float hplHandRootRollDegrees = 0.0f;
    int hplControllerComfortBlackoutFrames = 2;
    int hplControllerRecenterHoldMs = 900;
    int hplControllerMaxInputAgeFrames = 8;
    int hplControllerLogInterval = 120;
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
    bool hplNativeCameraRollSuppression = false;
    bool hplComfortCameraAddControl = false;
    bool hplComfortSuppressHeadBob = true;
    bool hplComfortSuppressCameraShake = true;
    bool hplComfortSuppressSway = false;
    int hplComfortLogInterval = 120;
    int hplCameraLogInterval = 120;
    bool hplStereoAfr = false;
    float hplWorldScale = 1.0f;
    bool hplRenderStageProbe = false;
    bool hplAudioListenerProbe = false;
    bool hplAudioListenerCorrection = false;
    bool hplAudioListenerTranslation = false;
    bool hplPostEffectControl = false;
    bool hplPostEffectBypassDefault = false;
    bool hplPostEffectDisableImageTrail = true;
    bool hplPostEffectDisableChromaticAberration = true;
    bool hplPostEffectDisableRadialBlur = true;
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
