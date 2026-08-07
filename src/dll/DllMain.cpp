#include "Config.h"
#include "BuildInfo.h"
#include "CrashHandler.h"
#include "HPLCameraBridge.h"
#include "HPLComfortBridge.h"
#include "HPLCompatibilityProbe.h"
#include "HPLContactHapticsBridge.h"
#include "HPLCrosshairBridge.h"
#include "HPLLifecycle.h"
#include "HPLInputBridge.h"
#include "HPLInteractionBridge.h"
#include "HPLGrabBridge.h"
#include "HPLGameplayHapticsBridge.h"
#include "HPLHandsBridge.h"
#include "HPLHudBridge.h"
#include "HPLMenuBridge.h"
#include "HPLNativeLocomotion.h"
#include "HPLPlayerState.h"
#include "HPLPresentationBridge.h"
#include "HPLScreenEffectBridge.h"
#include "HPLSubtitleBridge.h"
#include "HPLStatusPanelBridge.h"
#include "HPLTerminalBridge.h"
#include "HPLUserModuleBridge.h"
#include "Logger.h"
#include "OpenGLHooks.h"
#include "OpenXRRuntime.h"

#include <Windows.h>
#include <TlHelp32.h>

#include <memory>
#include <string>

namespace {

HMODULE g_module = nullptr;
HANDLE g_stopEvent = nullptr;
HANDLE g_workerThread = nullptr;
std::unique_ptr<somavr::ConfigManager> g_config;
std::unique_ptr<somavr::OpenXRRuntime> g_openxr;

bool IsOnlyCurrentThreadRemaining()
{
    const DWORD processId = GetCurrentProcessId();
    const DWORD currentThreadId = GetCurrentThreadId();
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return false;
    }

    THREADENTRY32 entry = {};
    entry.dwSize = sizeof(entry);
    DWORD processThreadCount = 0;
    bool currentThreadFound = false;
    if (Thread32First(snapshot, &entry)) {
        do {
            if (entry.th32OwnerProcessID == processId) {
                ++processThreadCount;
                currentThreadFound |= entry.th32ThreadID == currentThreadId;
            }
        } while (Thread32Next(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return currentThreadFound && processThreadCount == 1;
}

DWORD WINAPI WorkerThreadProc(LPVOID)
{
    somavr::InitializeWorkRoot(g_module);
    somavr::Logger::Instance().Initialize(somavr::LogPath(), somavr::LogLevel::Info);
    somavr::Logger::Instance().Write(
        somavr::LogLevel::Info,
        "runtime_paths root=%s source=%s module=%s",
        somavr::WorkRoot().string().c_str(),
        somavr::WorkRootSource().c_str(),
        somavr::ModulePath(g_module).c_str());
    somavr::LogBuildIdentity(g_module);
    somavr::InitializeCrashHandler(g_module, somavr::WorkRoot() / "logs" / "dumps");

    g_config = std::make_unique<somavr::ConfigManager>();
    if (!g_config->Initialize()) {
        somavr::Logger::Instance().Write(somavr::LogLevel::Error, "config_init failed");
        somavr::LogCrashHandlerSummary();
        somavr::ShutdownCrashHandler();
        somavr::Logger::Instance().Shutdown();
        return 1;
    }

    somavr::Logger::Instance().SetLevel(g_config->Get().logLevel);
    somavr::Logger::Instance().Write(
        somavr::LogLevel::Info,
        "config_applied path=%s logLevel=%s parsedKeyHash=0x%016llx accepted=%u unknownKeys=%u unknownSections=%u",
        g_config->Path().string().c_str(),
        somavr::Logger::LevelName(g_config->Get().logLevel),
        static_cast<unsigned long long>(g_config->ParsedKeyHash()),
        g_config->AcceptedKeyCount(),
        g_config->UnknownKeyCount(),
        g_config->UnknownSectionCount());
    somavr::Logger::Instance().Write(
        somavr::LogLevel::Info,
        "hook_config comfortPreset=%s frameSummaryInterval=%d matrixSampleLimitPerFrame=%d uniformNameLogLimit=%d uniformMatrixLogLimit=%d uniformMatrixProjectionOnly=%d matrixCapture=%d matrixCaptureFrames=%d matrixCaptureStackDepth=%d matrixCaptureMaxSites=%d matrixCaptureSamplesPerUniform=%d renderDiagnosticCapture=%d renderDiagnosticFrames=%d renderDiagnosticMaxPrograms=%d renderDiagnosticMaxDraws=%d hplCameraBridge=%d hplLifecycleShutdown=%d hplProjectionCenterControl=%d hplProjectionCenteredDefault=%d hplRoomscaleControl=%d hplRoomscaleEnabledDefault=%d hplRoomscaleVertical=%d hplRoomscaleSafety=%d hplRoomscaleSafetyClearanceMeters=%.3f hplRoomscaleSafetyRadiusMeters=%.3f hplRoomscaleSafetyVerticalRadiusMeters=%.3f hplRoomscaleSafetyRadialSamples=%d hplRoomscaleSafetyIterations=%d hplRoomscaleBodyReconciliation=%d hplRoomscaleBodyReconciliationThresholdMeters=%.3f hplRoomscaleBodyReconciliationTargetMeters=%.3f hplRoomscaleBodyReconciliationMaxStepMeters=%.3f hplRoomscaleBodyReconciliationHoldFrames=%d hplEyeHeightOffsetMeters=%.4f hplReflectionFadeControl=%d hplNativeCameraRollSuppression=%d hplCameraLogInterval=%d hplStereoAfr=%d hplWorldScale=%.4f hplRenderStageProbe=%d hplDualRenderReplayProbe=%d hplDualRenderAutoProbe=%d hplDualRenderAutoProbeCount=%d hplDualRenderAutoProbeDelayFrames=%d hplDualRenderAutoProbeIntervalFrames=%d hplDualRenderContinuousControl=%d hplDualRenderContinuousDefault=%d hplPerEyeViewHistoryControl=%d hplPerEyeImageTrailControl=%d hplToneMappingFrameControl=%d hplPerEyeSSAOTemporalControl=%d hplSSAOFrameOwnerControl=%d hplPerEyePerformanceTelemetry=%d hplAudioListenerProbe=%d hplAudioListenerCorrection=%d hplPostEffectControl=%d hplPostEffectResourceProbe=%d hplPostEffectBypassDefault=%d hplPostEffectDisableImageTrail=%d hplPostEffectDisableChromaticAberration=%d hplPostEffectDisableRadialBlur=%d hplShadowJitterControl=%d hplShadowJitterSuppressedDefault=%d hplCompatibilityLogInterval=%d openxrProbe=%d openxrSessionProbe=%d openxrReleaseAfterProbe=%d openxrBootstrapFrame=%d openxrHoldFrames=%d openxrManualStart=%d openxrFrameSubmit=%d openxrMirrorBackbuffer=%d openxrDesktopMirrorEye=%s openxrDesktopMirrorAspect=%s openxrResolutionScalePercent=%d openxrReferenceSpace=%s openxrInputEnabled=%d openxrInputLogInterval=%d openxrRecoveryEnabled=%d openxrRecoveryDelayFrames=%d openxrTrackingHoldFrames=%d openxrTrackingRecoveryBlackoutFrames=%d",
        g_config->Get().comfortPreset.c_str(),
        g_config->Get().frameSummaryInterval,
        g_config->Get().matrixSampleLimitPerFrame,
        g_config->Get().uniformNameLogLimit,
        g_config->Get().uniformMatrixLogLimit,
        g_config->Get().uniformMatrixProjectionOnly ? 1 : 0,
        g_config->Get().matrixCaptureEnabled ? 1 : 0,
        g_config->Get().matrixCaptureFrames,
        g_config->Get().matrixCaptureStackDepth,
        g_config->Get().matrixCaptureMaxSites,
        g_config->Get().matrixCaptureSamplesPerUniform,
        g_config->Get().renderDiagnosticCapture ? 1 : 0,
        g_config->Get().renderDiagnosticFrames,
        g_config->Get().renderDiagnosticMaxPrograms,
        g_config->Get().renderDiagnosticMaxDraws,
        g_config->Get().hplCameraBridge ? 1 : 0,
        g_config->Get().hplLifecycleShutdown ? 1 : 0,
        g_config->Get().hplProjectionCenterControl ? 1 : 0,
        g_config->Get().hplProjectionCenteredDefault ? 1 : 0,
        g_config->Get().hplRoomscaleControl ? 1 : 0,
        g_config->Get().hplRoomscaleEnabledDefault ? 1 : 0,
        g_config->Get().hplRoomscaleVertical ? 1 : 0,
        g_config->Get().hplRoomscaleSafety ? 1 : 0,
        g_config->Get().hplRoomscaleSafetyClearanceMeters,
        g_config->Get().hplRoomscaleSafetyRadiusMeters,
        g_config->Get().hplRoomscaleSafetyVerticalRadiusMeters,
        g_config->Get().hplRoomscaleSafetyRadialSamples,
        g_config->Get().hplRoomscaleSafetyIterations,
        g_config->Get().hplRoomscaleBodyReconciliation ? 1 : 0,
        g_config->Get().hplRoomscaleBodyReconciliationThresholdMeters,
        g_config->Get().hplRoomscaleBodyReconciliationTargetMeters,
        g_config->Get().hplRoomscaleBodyReconciliationMaxStepMeters,
        g_config->Get().hplRoomscaleBodyReconciliationHoldFrames,
        g_config->Get().hplEyeHeightOffsetMeters,
        g_config->Get().hplReflectionFadeControl ? 1 : 0,
        g_config->Get().hplNativeCameraRollSuppression ? 1 : 0,
        g_config->Get().hplCameraLogInterval,
        g_config->Get().hplStereoAfr ? 1 : 0,
        g_config->Get().hplWorldScale,
        g_config->Get().hplRenderStageProbe ? 1 : 0,
        g_config->Get().hplDualRenderReplayProbe ? 1 : 0,
        g_config->Get().hplDualRenderAutoProbe ? 1 : 0,
        g_config->Get().hplDualRenderAutoProbeCount,
        g_config->Get().hplDualRenderAutoProbeDelayFrames,
        g_config->Get().hplDualRenderAutoProbeIntervalFrames,
        g_config->Get().hplDualRenderContinuousControl ? 1 : 0,
        g_config->Get().hplDualRenderContinuousDefault ? 1 : 0,
        g_config->Get().hplPerEyeViewHistoryControl ? 1 : 0,
        g_config->Get().hplPerEyeImageTrailControl ? 1 : 0,
        g_config->Get().hplToneMappingFrameControl ? 1 : 0,
        g_config->Get().hplPerEyeSSAOTemporalControl ? 1 : 0,
        g_config->Get().hplSSAOFrameOwnerControl ? 1 : 0,
        g_config->Get().hplPerEyePerformanceTelemetry ? 1 : 0,
        g_config->Get().hplAudioListenerProbe ? 1 : 0,
        g_config->Get().hplAudioListenerCorrection ? 1 : 0,
        g_config->Get().hplPostEffectControl ? 1 : 0,
        g_config->Get().hplPostEffectResourceProbe ? 1 : 0,
        g_config->Get().hplPostEffectBypassDefault ? 1 : 0,
        g_config->Get().hplPostEffectDisableImageTrail ? 1 : 0,
        g_config->Get().hplPostEffectDisableChromaticAberration ? 1 : 0,
        g_config->Get().hplPostEffectDisableRadialBlur ? 1 : 0,
        g_config->Get().hplShadowJitterControl ? 1 : 0,
        g_config->Get().hplShadowJitterSuppressedDefault ? 1 : 0,
        g_config->Get().hplCompatibilityLogInterval,
        g_config->Get().openxrProbe ? 1 : 0,
        g_config->Get().openxrSessionProbe ? 1 : 0,
        g_config->Get().openxrReleaseAfterProbe ? 1 : 0,
        g_config->Get().openxrBootstrapFrame,
        g_config->Get().openxrHoldFrames,
        g_config->Get().openxrManualStart ? 1 : 0,
        g_config->Get().openxrFrameSubmit ? 1 : 0,
        g_config->Get().openxrMirrorBackbuffer ? 1 : 0,
        g_config->Get().openxrDesktopMirrorEye.c_str(),
        g_config->Get().openxrDesktopMirrorAspect.c_str(),
        g_config->Get().openxrResolutionScalePercent,
        g_config->Get().openxrReferenceSpace.c_str(),
        g_config->Get().openxrInputEnabled ? 1 : 0,
        g_config->Get().openxrInputLogInterval,
        g_config->Get().openxrRecoveryEnabled ? 1 : 0,
        g_config->Get().openxrRecoveryDelayFrames,
        g_config->Get().openxrTrackingHoldFrames,
        g_config->Get().openxrTrackingRecoveryBlackoutFrames);
    somavr::Logger::Instance().Write(
        somavr::LogLevel::Info,
        "controller_config enabled=%d moveDeadzone=%.2f moveRelease=%.2f nativeLocomotion=%d movementReference=%s physicalCrouch=%d physicalCrouchThresholds=%.3f,%.3f turnMode=%s turnDeadzone=%.2f turnRelease=%.2f snapPixels=%d smoothPixelsPerSecond=%.1f nativeTurn=%d snapDegrees=%.1f smoothDegreesPerSecond=%.1f nativeTurnSign=%.1f interaction=%d interactionBothHands=%d flashlight=%d inventory=%d menu=%d menuPointer=%d menuPointerFov=%.1f,%.1f menuPointerSmoothing=%.3f recenterChord=%d haptics=%d hapticAmplitude=%.2f hapticDurationMs=%d dominantHand=%s swapSticks=%d oneHandFallback=%d suppressAuthoredCamera=%d interactionRay=%d interactionRayOriginTolerance=%.3f grabTranslation=%d grabTranslationScale=%.3f grabMaxOffsetMeters=%.3f grabRotation=%d grabRotationGain=%.2f grabRotationSign=%.1f grabMaxAngularSpeed=%.2f twoHandHudObject=%d twoHandGrabRotation=%d twoHandSqueeze=%.3f twoHandSeparationMeters=%.3f,%.3f twoHandBlend=%.3f throwRedirect=%d throwVelocityScale=%d throwVelocityThreshold=%.3f throwVelocityReference=%.3f manipulationMappings=%d handTrackingProbe=%d handControllerRoot=%d handWristRotation=%d handFreezePose=%d handShoulderVerticalOffsetMeters=%.3f handShoulderBackOffsetMeters=%.3f handElbowDownMeters=%.3f handErgonomics=%d handShoulderReach=%d handShoulderReachStart=%.3f handShoulderReachMaxMeters=%.3f handMaxSwivelDegreesPerFrame=%.2f handRootOffset=%.4f,%.4f,%.4f handRootRotationDegrees=%.2f,%.2f,%.2f controllerHudObject=%d hudObjectOffset=%.4f,%.4f,%.4f hudObjectRotationDegrees=%.2f,%.2f,%.2f flashlightAim=%d flashlightOffset=%.4f,%.4f,%.4f flashlightRotationDegrees=%.2f,%.2f,%.2f comfortBlackoutFrames=%d recenterHoldMs=%d maxInputAgeFrames=%d logInterval=%d",
        g_config->Get().hplControllerInput ? 1 : 0,
        g_config->Get().hplControllerMoveDeadzone,
        g_config->Get().hplControllerMoveReleaseDeadzone,
        g_config->Get().hplControllerNativeLocomotion ? 1 : 0,
        g_config->Get().hplControllerMovementReference.c_str(),
        g_config->Get().hplControllerPhysicalCrouch ? 1 : 0,
        g_config->Get().hplControllerPhysicalCrouchEnterMeters,
        g_config->Get().hplControllerPhysicalCrouchExitMeters,
        g_config->Get().hplControllerSnapTurn ? "snap" : "smooth",
        g_config->Get().hplControllerTurnDeadzone,
        g_config->Get().hplControllerTurnReleaseDeadzone,
        g_config->Get().hplControllerSnapTurnPixels,
        g_config->Get().hplControllerSmoothTurnPixelsPerSecond,
        g_config->Get().hplControllerNativeTurn ? 1 : 0,
        g_config->Get().hplControllerSnapTurnDegrees,
        g_config->Get().hplControllerSmoothTurnDegreesPerSecond,
        g_config->Get().hplControllerNativeTurnSign,
        g_config->Get().hplControllerInteraction ? 1 : 0,
        g_config->Get().hplControllerInteractionBothHands ? 1 : 0,
        g_config->Get().hplControllerFlashlight ? 1 : 0,
        g_config->Get().hplControllerInventory ? 1 : 0,
        g_config->Get().hplControllerMenu ? 1 : 0,
        g_config->Get().hplControllerMenuPointer ? 1 : 0,
        g_config->Get().hplControllerMenuPointerHorizontalDegrees,
        g_config->Get().hplControllerMenuPointerVerticalDegrees,
        g_config->Get().hplControllerMenuPointerSmoothing,
        g_config->Get().hplControllerRecenterChord ? 1 : 0,
        g_config->Get().hplControllerHaptics ? 1 : 0,
        g_config->Get().hplControllerHapticAmplitude,
        g_config->Get().hplControllerHapticDurationMs,
        g_config->Get().hplControllerDominantHand.c_str(),
        g_config->Get().hplControllerSwapSticks ? 1 : 0,
        g_config->Get().hplControllerOneHandFallback ? 1 : 0,
        g_config->Get().hplControllerSuppressDuringAuthoredCamera ? 1 : 0,
        g_config->Get().hplControllerInteractionRay ? 1 : 0,
        g_config->Get().hplControllerInteractionRayOriginTolerance,
        g_config->Get().hplControllerGrabTranslation ? 1 : 0,
        g_config->Get().hplControllerGrabTranslationScale,
        g_config->Get().hplControllerGrabMaxOffsetMeters,
        g_config->Get().hplControllerGrabRotation ? 1 : 0,
        g_config->Get().hplControllerGrabRotationGain,
        g_config->Get().hplControllerGrabRotationSign,
        g_config->Get().hplControllerGrabMaxAngularSpeed,
        g_config->Get().hplControllerTwoHandHudObject ? 1 : 0,
        g_config->Get().hplControllerTwoHandGrabRotation ? 1 : 0,
        g_config->Get().hplControllerTwoHandSqueezeThreshold,
        g_config->Get().hplControllerTwoHandMinSeparationMeters,
        g_config->Get().hplControllerTwoHandMaxSeparationMeters,
        g_config->Get().hplControllerTwoHandDirectionBlend,
        g_config->Get().hplControllerThrowRedirect ? 1 : 0,
        g_config->Get().hplControllerThrowVelocityScale ? 1 : 0,
        g_config->Get().hplControllerThrowVelocityThreshold,
        g_config->Get().hplControllerThrowVelocityReference,
        g_config->Get().hplControllerManipulationMappings ? 1 : 0,
        g_config->Get().hplHandTrackingProbe ? 1 : 0,
        g_config->Get().hplHandControllerRoot ? 1 : 0,
        g_config->Get().hplHandWristRotation ? 1 : 0,
        g_config->Get().hplHandFreezePose ? 1 : 0,
        g_config->Get().hplHandShoulderVerticalOffsetMeters,
        g_config->Get().hplHandShoulderBackOffsetMeters,
        g_config->Get().hplHandArmIKElbowDownMeters,
        g_config->Get().hplHandArmIKErgonomics ? 1 : 0,
        g_config->Get().hplHandShoulderReachCompensation ? 1 : 0,
        g_config->Get().hplHandShoulderReachStart,
        g_config->Get().hplHandShoulderReachMaxMeters,
        g_config->Get().hplHandArmIKMaxSwivelDegreesPerFrame,
        g_config->Get().hplHandRootOffsetX,
        g_config->Get().hplHandRootOffsetY,
        g_config->Get().hplHandRootOffsetZ,
        g_config->Get().hplHandRootPitchDegrees,
        g_config->Get().hplHandRootYawDegrees,
        g_config->Get().hplHandRootRollDegrees,
        g_config->Get().hplControllerHudObject ? 1 : 0,
        g_config->Get().hplHudObjectOffsetX,
        g_config->Get().hplHudObjectOffsetY,
        g_config->Get().hplHudObjectOffsetZ,
        g_config->Get().hplHudObjectPitchDegrees,
        g_config->Get().hplHudObjectYawDegrees,
        g_config->Get().hplHudObjectRollDegrees,
        g_config->Get().hplControllerFlashlightAim ? 1 : 0,
        g_config->Get().hplFlashlightOffsetX,
        g_config->Get().hplFlashlightOffsetY,
        g_config->Get().hplFlashlightOffsetZ,
        g_config->Get().hplFlashlightPitchDegrees,
        g_config->Get().hplFlashlightYawDegrees,
        g_config->Get().hplFlashlightRollDegrees,
        g_config->Get().hplControllerComfortBlackoutFrames,
        g_config->Get().hplControllerRecenterHoldMs,
        g_config->Get().hplControllerMaxInputAgeFrames,
        g_config->Get().hplControllerLogInterval);
    somavr::Logger::Instance().Write(
        somavr::LogLevel::Info,
        "gameplay_haptics_config enabled=%d amplitudeScale=%.3f minAmplitude=%.3f retriggerDelta=%.3f refreshMs=%d segmentMs=%d",
        g_config->Get().hplControllerGameplayHaptics ? 1 : 0,
        g_config->Get().hplControllerGameplayHapticAmplitudeScale,
        g_config->Get().hplControllerGameplayHapticMinAmplitude,
        g_config->Get().hplControllerGameplayHapticRetriggerDelta,
        g_config->Get().hplControllerGameplayHapticRefreshMs,
        g_config->Get().hplControllerGameplayHapticSegmentMs);
    somavr::Logger::Instance().Write(
        somavr::LogLevel::Info,
        "hud_config enabled=%d shape=%s cylinderAngleDegrees=%.3f size=%dx%d distanceMeters=%.3f widthMeters=%.3f verticalOffsetMeters=%.3f maxAgeFrames=%d suppressCenterCrosshair=%d crosshairClearRadiusPixels=%d",
        g_config->Get().openxrHudLayer ? 1 : 0,
        g_config->Get().openxrHudShape.c_str(),
        g_config->Get().openxrHudCylinderAngleDegrees,
        g_config->Get().openxrHudWidthPixels,
        g_config->Get().openxrHudHeightPixels,
        g_config->Get().openxrHudDistanceMeters,
        g_config->Get().openxrHudWidthMeters,
        g_config->Get().openxrHudVerticalOffsetMeters,
        g_config->Get().openxrHudMaxAgeFrames,
        g_config->Get().openxrHudSuppressCenterCrosshair ? 1 : 0,
        g_config->Get().openxrHudCrosshairClearRadiusPixels);
    somavr::Logger::Instance().Write(
        somavr::LogLevel::Info,
        "interaction_reticle_config enabled=%d semantic=%d nativeIcons=%d pixels=%d angularSizeDegrees=%.3f sizeMeters=%.4f..%.4f distanceMeters=%.3f..%.3f maxAgeFrames=%d focusHaptics=%d focusHapticAmplitude=%.3f focusHapticDurationMs=%d focusHapticCooldownFrames=%d",
        g_config->Get().openxrInteractionReticle ? 1 : 0,
        g_config->Get().openxrInteractionReticleSemantic ? 1 : 0,
        g_config->Get().openxrInteractionReticleNativeIcons ? 1 : 0,
        g_config->Get().openxrInteractionReticleSizePixels,
        g_config->Get().openxrInteractionReticleAngularSizeDegrees,
        g_config->Get().openxrInteractionReticleMinSizeMeters,
        g_config->Get().openxrInteractionReticleMaxSizeMeters,
        g_config->Get().openxrInteractionReticleMinDistanceMeters,
        g_config->Get().openxrInteractionReticleMaxDistanceMeters,
        g_config->Get().openxrInteractionReticleMaxAgeFrames,
        g_config->Get().hplControllerFocusHaptics ? 1 : 0,
        g_config->Get().hplControllerFocusHapticAmplitude,
        g_config->Get().hplControllerFocusHapticDurationMs,
        g_config->Get().hplControllerFocusHapticCooldownFrames);
    somavr::Logger::Instance().Write(
        somavr::LogLevel::Info,
        "comfort_config cameraAddControl=%d suppressHeadBob=%d suppressCameraShake=%d suppressSway=%d cameraRollControl=%d suppressRoll={script=%d lean=%d move=%d climb=%d} depthOfFieldControl=%d opticsControl=%d suppressOptics={fov=%d fovMultiplier=%d aspectMultiplier=%d} stateTransitionBlackoutFrames=%d videoDistortion=%d loadingScreenControl=%d loadingScreenExitBlackoutFrames=%d videoLifecycleProbe=%d logInterval=%d",
        g_config->Get().hplComfortCameraAddControl ? 1 : 0,
        g_config->Get().hplComfortSuppressHeadBob ? 1 : 0,
        g_config->Get().hplComfortSuppressCameraShake ? 1 : 0,
        g_config->Get().hplComfortSuppressSway ? 1 : 0,
        g_config->Get().hplComfortCameraRollControl ? 1 : 0,
        g_config->Get().hplComfortSuppressScriptRoll ? 1 : 0,
        g_config->Get().hplComfortSuppressLeanRoll ? 1 : 0,
        g_config->Get().hplComfortSuppressMoveRoll ? 1 : 0,
        g_config->Get().hplComfortSuppressClimbRoll ? 1 : 0,
        g_config->Get().hplComfortDepthOfFieldControl ? 1 : 0,
        g_config->Get().hplComfortOpticsControl ? 1 : 0,
        g_config->Get().hplComfortSuppressFov ? 1 : 0,
        g_config->Get().hplComfortSuppressFovMultiplier ? 1 : 0,
        g_config->Get().hplComfortSuppressAspectMultiplier ? 1 : 0,
        g_config->Get().hplControllerStateTransitionBlackoutFrames,
        g_config->Get().hplPostEffectDisableVideoDistortion ? 1 : 0,
        g_config->Get().hplLoadingScreenControl ? 1 : 0,
        g_config->Get().hplLoadingScreenExitBlackoutFrames,
        g_config->Get().hplVideoLifecycleProbe ? 1 : 0,
        g_config->Get().hplComfortLogInterval);

    g_openxr = std::make_unique<somavr::OpenXRRuntime>();
    somavr::OpenXRComfortVignetteSettings comfortVignette;
    comfortVignette.enabled = g_config->Get().openxrComfortVignette;
    comfortVignette.sizePixels = g_config->Get().openxrComfortVignetteSizePixels;
    comfortVignette.distanceMeters = g_config->Get().openxrComfortVignetteDistanceMeters;
    comfortVignette.widthMeters = g_config->Get().openxrComfortVignetteWidthMeters;
    comfortVignette.strength = g_config->Get().openxrComfortVignetteStrength;
    comfortVignette.innerRadius = g_config->Get().openxrComfortVignetteInnerRadius;
    comfortVignette.fadeMilliseconds =
        g_config->Get().openxrComfortVignetteFadeMilliseconds;
    comfortVignette.maxMotionAgeFrames = g_config->Get().hplControllerMaxInputAgeFrames;
    somavr::OpenXRFoveationSettings foveation;
    foveation.enabled = g_config->Get().openxrFoveation;
    foveation.level = g_config->Get().openxrFoveationLevel;
    foveation.dynamic = g_config->Get().openxrFoveationDynamic;
    foveation.verticalOffset = g_config->Get().openxrFoveationVerticalOffset;
    g_openxr->Configure(
        g_config->Get().openxrProbe,
        g_config->Get().openxrSessionProbe,
        g_config->Get().openxrReleaseAfterProbe,
        static_cast<uint64_t>(g_config->Get().openxrBootstrapFrame),
        static_cast<uint64_t>(g_config->Get().openxrHoldFrames),
        g_config->Get().openxrManualStart,
        g_config->Get().openxrFrameSubmit,
        g_config->Get().openxrMirrorBackbuffer,
        g_config->Get().openxrDesktopMirrorEye,
        g_config->Get().openxrDesktopMirrorAspect,
        g_config->Get().openxrDepthCompositionProbe,
        g_config->Get().openxrDepthCompositionSubmit,
        foveation,
        g_config->Get().openxrResolutionScalePercent,
        g_config->Get().openxrReferenceSpace,
        g_config->Get().openxrInputEnabled,
        g_config->Get().openxrInputLogInterval,
        g_config->Get().openxrRecoveryEnabled,
        g_config->Get().openxrRecoveryDelayFrames,
        g_config->Get().openxrTrackingHoldFrames,
        g_config->Get().openxrTrackingRecoveryBlackoutFrames,
        g_config->Get().openxrHudLayer,
        g_config->Get().openxrHudShape,
        g_config->Get().openxrHudCylinderAngleDegrees,
        g_config->Get().openxrHudWidthPixels,
        g_config->Get().openxrHudHeightPixels,
        g_config->Get().openxrHudDistanceMeters,
        g_config->Get().openxrHudWidthMeters,
        g_config->Get().openxrHudVerticalOffsetMeters,
        g_config->Get().openxrHudMaxAgeFrames,
        g_config->Get().openxrHudSuppressCenterCrosshair,
        g_config->Get().openxrHudCrosshairClearRadiusPixels,
        g_config->Get().openxrInteractionReticle,
        g_config->Get().openxrInteractionReticleSemantic,
        g_config->Get().openxrInteractionReticleNativeIcons,
        g_config->Get().openxrInteractionReticleSizePixels,
        g_config->Get().openxrInteractionReticleAngularSizeDegrees,
        g_config->Get().openxrInteractionReticleMinSizeMeters,
        g_config->Get().openxrInteractionReticleMaxSizeMeters,
        g_config->Get().openxrInteractionReticleMinDistanceMeters,
        g_config->Get().openxrInteractionReticleMaxDistanceMeters,
        g_config->Get().openxrInteractionReticleMaxAgeFrames,
        g_config->Get().openxrStatusPanel,
        g_config->Get().openxrStatusPanelWidthPixels,
        g_config->Get().openxrStatusPanelHeightPixels,
        g_config->Get().openxrStatusPanelDistanceMeters,
        g_config->Get().openxrStatusPanelWidthMeters,
        g_config->Get().openxrStatusPanelVerticalOffsetMeters,
        comfortVignette);

    if (!somavr::InstallOpenGLHooks(g_config->Get(), g_openxr.get())) {
        somavr::Logger::Instance().Write(somavr::LogLevel::Error, "opengl_hooks install_failed");
    }
    if (!somavr::InstallHPLCameraBridge(g_config->Get(), g_openxr.get())) {
        somavr::Logger::Instance().Write(somavr::LogLevel::Error, "hpl_camera_bridge install_failed");
    }
    if (!somavr::InstallHPLPlayerState(g_config->Get())) {
        somavr::Logger::Instance().Write(somavr::LogLevel::Error, "hpl_player_state install_failed");
    }
    if (!somavr::InstallHPLComfortBridge(g_config->Get())) {
        somavr::Logger::Instance().Write(somavr::LogLevel::Error, "hpl_comfort_bridge install_failed");
    }
    if (!somavr::InstallHPLPresentationBridge(g_config->Get(), g_openxr.get())) {
        somavr::Logger::Instance().Write(somavr::LogLevel::Error, "hpl_presentation_bridge install_failed");
    }
    if (!somavr::InstallHPLScreenEffectBridge(g_config->Get())) {
        somavr::Logger::Instance().Write(somavr::LogLevel::Error, "hpl_screen_effect_bridge install_failed");
    }
    if (!somavr::InstallHPLSubtitleBridge(g_config->Get())) {
        somavr::Logger::Instance().Write(somavr::LogLevel::Error, "hpl_subtitle_bridge install_failed");
    }
    if (!somavr::InstallHPLNativeLocomotion(g_config->Get())) {
        somavr::Logger::Instance().Write(somavr::LogLevel::Error, "hpl_native_locomotion install_failed");
    }
    if (!somavr::InstallHPLMenuBridge(g_config->Get())) {
        somavr::Logger::Instance().Write(somavr::LogLevel::Error, "hpl_menu_bridge install_failed");
    }
    if (!somavr::InstallHPLTerminalBridge(g_config->Get(), g_openxr.get())) {
        somavr::Logger::Instance().Write(somavr::LogLevel::Error, "hpl_terminal_bridge install_failed");
    }
    if (!somavr::InstallHPLStatusPanelBridge(g_config->Get(), g_openxr.get())) {
        somavr::Logger::Instance().Write(somavr::LogLevel::Error, "hpl_status_panel install_failed");
    }
    if (!somavr::InstallHPLInputBridge(g_config->Get(), g_openxr.get())) {
        somavr::Logger::Instance().Write(somavr::LogLevel::Error, "hpl_input_bridge install_failed");
    }
    if (!somavr::InstallHPLInteractionBridge(g_config->Get(), g_openxr.get())) {
        somavr::Logger::Instance().Write(somavr::LogLevel::Error, "hpl_interaction_bridge install_failed");
    }
    if (!somavr::InstallHPLGameplayHapticsBridge(g_config->Get(), g_openxr.get())) {
        somavr::Logger::Instance().Write(somavr::LogLevel::Error, "hpl_gameplay_haptics install_failed");
    }
    if (!somavr::InstallHPLContactHapticsBridge(g_config->Get(), g_openxr.get())) {
        somavr::Logger::Instance().Write(somavr::LogLevel::Error, "hpl_contact_haptics install_failed");
    }
    if (!somavr::InstallHPLCrosshairBridge(g_config->Get())) {
        somavr::Logger::Instance().Write(somavr::LogLevel::Error, "hpl_crosshair_bridge install_failed");
    }
    if (!somavr::InstallHPLUserModuleBridge(g_config->Get())) {
        somavr::Logger::Instance().Write(somavr::LogLevel::Error, "hpl_user_module_bridge install_failed");
    }
    if (!somavr::InstallHPLGrabBridge(g_config->Get(), g_openxr.get())) {
        somavr::Logger::Instance().Write(somavr::LogLevel::Error, "hpl_grab_bridge install_failed");
    }
    if (!somavr::InstallHPLHandsBridge(g_config->Get(), g_openxr.get())) {
        somavr::Logger::Instance().Write(somavr::LogLevel::Error, "hpl_hands_bridge install_failed");
    }
    if (!somavr::InstallHPLHudBridge(g_config->Get(), g_openxr.get())) {
        somavr::Logger::Instance().Write(somavr::LogLevel::Error, "hpl_hud_bridge install_failed");
    }
    if (!somavr::InstallHPLCompatibilityProbe(g_config->Get(), g_openxr.get())) {
        somavr::Logger::Instance().Write(somavr::LogLevel::Error, "hpl_compat_probe install_failed");
    }
    if (!somavr::InstallHPLLifecycle(g_config->Get(), g_openxr.get())) {
        somavr::Logger::Instance().Write(somavr::LogLevel::Error, "hpl_lifecycle install_failed");
    }

    bool orphanedWorker = false;
    const HANDLE stopEvent = g_stopEvent;
    for (;;) {
        const DWORD waitResult = stopEvent != nullptr
            ? WaitForSingleObject(stopEvent, 500)
            : WAIT_FAILED;
        if (waitResult != WAIT_TIMEOUT) {
            if (waitResult == WAIT_FAILED) {
                somavr::Logger::Instance().Write(
                    somavr::LogLevel::Warn,
                    "process_lifetime stop_wait_failed error=%lu action=exit_worker",
                    static_cast<unsigned long>(GetLastError()));
            }
            break;
        }
        if (IsOnlyCurrentThreadRemaining()) {
            orphanedWorker = true;
            somavr::Logger::Instance().Write(
                somavr::LogLevel::Warn,
                "process_lifetime orphaned_worker_detected action=return_without_runtime_teardown");
            break;
        }
        somavr::MaintainCrashHandler();
    }

    if (orphanedWorker) {
        return 0;
    }

    somavr::LogHPLLifecycleSummary();
    somavr::RemoveHPLLifecycle();
    somavr::LogHPLCompatibilityProbeSummary();
    somavr::RemoveHPLCompatibilityProbe();
    somavr::LogHPLHudBridgeSummary();
    somavr::RemoveHPLHudBridge();
    somavr::LogHPLHandsBridgeSummary();
    somavr::RemoveHPLHandsBridge();
    somavr::LogHPLGrabBridgeSummary();
    somavr::RemoveHPLGrabBridge();
    somavr::LogHPLCrosshairBridgeSummary();
    somavr::RemoveHPLCrosshairBridge();
    somavr::LogHPLUserModuleBridgeSummary();
    somavr::RemoveHPLUserModuleBridge();
    somavr::LogHPLInteractionBridgeSummary();
    somavr::RemoveHPLInteractionBridge();
    somavr::LogHPLContactHapticsBridgeSummary();
    somavr::RemoveHPLContactHapticsBridge();
    somavr::LogHPLGameplayHapticsBridgeSummary();
    somavr::RemoveHPLGameplayHapticsBridge();
    somavr::LogHPLPresentationBridgeSummary();
    somavr::RemoveHPLPresentationBridge();
    somavr::LogHPLScreenEffectBridgeSummary();
    somavr::RemoveHPLScreenEffectBridge();
    somavr::LogHPLSubtitleBridgeSummary();
    somavr::RemoveHPLSubtitleBridge();
    somavr::LogHPLComfortBridgeSummary();
    somavr::RemoveHPLComfortBridge();
    somavr::LogHPLCameraBridgeSummary();
    somavr::LogHPLInputBridgeSummary();
    somavr::RemoveHPLInputBridge();
    somavr::LogHPLStatusPanelBridgeSummary();
    somavr::RemoveHPLStatusPanelBridge();
    somavr::LogHPLTerminalBridgeSummary();
    somavr::RemoveHPLTerminalBridge();
    somavr::LogHPLMenuBridgeSummary();
    somavr::RemoveHPLMenuBridge();
    somavr::LogHPLNativeLocomotionSummary();
    somavr::RemoveHPLNativeLocomotion();
    somavr::LogHPLPlayerStateSummary();
    somavr::RemoveHPLPlayerState();
    somavr::RemoveHPLCameraBridge();
    somavr::LogOpenGLProofSummary();
    somavr::RemoveOpenGLHooks();
    if (g_openxr) {
        g_openxr->Shutdown();
    }
    somavr::LogCrashHandlerSummary();
    somavr::ShutdownCrashHandler();
    somavr::Logger::Instance().Write(somavr::LogLevel::Info, "somavr.dll worker exiting");
    somavr::Logger::Instance().Shutdown();
    HANDLE ownedStopEvent = static_cast<HANDLE>(InterlockedExchangePointer(
        reinterpret_cast<void* volatile*>(&g_stopEvent), nullptr));
    if (ownedStopEvent != nullptr) {
        CloseHandle(ownedStopEvent);
    }
    return 0;
}

} // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = module;
        DisableThreadLibraryCalls(module);

        g_stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (g_stopEvent != nullptr) {
            g_workerThread = CreateThread(nullptr, 0, WorkerThreadProc, nullptr, 0, nullptr);
            if (g_workerThread != nullptr) {
                CloseHandle(g_workerThread);
                g_workerThread = nullptr;
            }
        }
    } else if (reason == DLL_PROCESS_DETACH) {
        if (g_stopEvent != nullptr) {
            SetEvent(g_stopEvent);
        }
    }

    return TRUE;
}
