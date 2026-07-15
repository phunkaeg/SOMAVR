#include "Config.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>

namespace somavr {
namespace {

std::string Trim(std::string value)
{
    const auto isSpace = [](unsigned char ch) { return std::isspace(ch) != 0; };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), [&](char ch) {
        return !isSpace(static_cast<unsigned char>(ch));
    }));
    value.erase(std::find_if(value.rbegin(), value.rend(), [&](char ch) {
        return !isSpace(static_cast<unsigned char>(ch));
    }).base(), value.end());
    return value;
}

std::string Lower(std::string value)
{
    for (char& ch : value) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return value;
}

bool ParseBool(const std::string& value, bool fallback)
{
    const std::string v = Lower(Trim(value));
    if (v == "1" || v == "true" || v == "yes" || v == "on") {
        return true;
    }
    if (v == "0" || v == "false" || v == "no" || v == "off") {
        return false;
    }
    return fallback;
}

int ParseInt(const std::string& value, int fallback, int minValue, int maxValue)
{
    try {
        const int parsed = std::stoi(Trim(value));
        return std::clamp(parsed, minValue, maxValue);
    } catch (...) {
        return fallback;
    }
}

float ParseFloat(const std::string& value, float fallback, float minValue, float maxValue)
{
    try {
        const float parsed = std::stof(Trim(value));
        if (!std::isfinite(parsed)) {
            return fallback;
        }
        return std::clamp(parsed, minValue, maxValue);
    } catch (...) {
        return fallback;
    }
}

} // namespace

bool ConfigManager::Initialize()
{
    path_ = ConfigPath();

    std::error_code ec;
    std::filesystem::create_directories(path_.parent_path(), ec);
    if (!std::filesystem::exists(path_, ec)) {
        WriteDefaultConfig();
    }

    LoadFromFile();
    return true;
}

const Config& ConfigManager::Get() const
{
    return config_;
}

const std::filesystem::path& ConfigManager::Path() const
{
    return path_;
}

void ConfigManager::WriteDefaultConfig() const
{
    std::ofstream out(path_, std::ios::out | std::ios::trunc);
    if (!out) {
        return;
    }

    out
        << "# SOMAVR generated config\n"
        << "# Experimental OpenXR and native camera paths are disabled by default.\n\n"
        << "[Logging]\n"
        << "Level=info\n\n"
        << "[Hooks]\n"
        << "SwapBuffers=1\n"
        << "WglMakeCurrent=1\n"
        << "FixedFunctionMatrices=1\n"
        << "UniformMatrices=1\n"
        << "Viewport=1\n"
        << "DrawCalls=1\n"
        << "Framebuffer=1\n"
        << "FrameSummaryInterval=120\n"
        << "MatrixSampleLimitPerFrame=32\n"
        << "UniformNameLogLimit=256\n"
        << "UniformMatrixLogLimit=256\n"
        << "UniformMatrixProjectionOnly=1\n"
        << "MatrixCapture=0\n"
        << "MatrixCaptureFrames=120\n"
        << "MatrixCaptureStackDepth=8\n"
        << "MatrixCaptureMaxSites=64\n"
        << "MatrixCaptureSamplesPerUniform=4\n"
        << "RenderDiagnosticCapture=0\n"
        << "RenderDiagnosticFrames=4\n"
        << "RenderDiagnosticMaxPrograms=128\n"
        << "RenderDiagnosticMaxDraws=8192\n"
        << "HPLCameraBridge=0\n"
        << "HPLLifecycleShutdown=0\n"
        << "HPLProjectionCenterControl=0\n"
        << "HPLProjectionCenteredDefault=0\n"
        << "HPLRoomscaleControl=0\n"
        << "HPLRoomscaleEnabledDefault=1\n"
        << "HPLRoomscaleVertical=1\n"
        << "HPLRoomscaleSafety=0\n"
        << "HPLRoomscaleSafetyDynamic=0\n"
        << "HPLRoomscaleSafetyClearanceMeters=0.02\n"
        << "HPLRoomscaleSafetyIterations=6\n"
        << "HPLRoomscaleSafetyRadiusMeters=0.09\n"
        << "HPLRoomscaleSafetyVerticalRadiusMeters=0.12\n"
        << "HPLRoomscaleSafetyRadialSamples=6\n"
        << "HPLRoomscaleBodyReconciliation=0\n"
        << "HPLRoomscaleBodyReconciliationThresholdMeters=0.45\n"
        << "HPLRoomscaleBodyReconciliationTargetMeters=0.25\n"
        << "HPLRoomscaleBodyReconciliationMaxStepMeters=0.015\n"
        << "HPLRoomscaleBodyReconciliationHoldFrames=30\n"
        << "HPLEyeHeightOffsetMeters=0.0\n"
        << "HPLRecenterControl=0\n"
        << "HPLReflectionFadeControl=0\n"
        << "HPLNativeCameraRollSuppression=0\n"
        << "HPLComfortCameraAddControl=0\n"
        << "HPLComfortSuppressHeadBob=1\n"
        << "HPLComfortSuppressCameraShake=1\n"
        << "HPLComfortSuppressSway=0\n"
        << "HPLComfortCameraRollControl=0\n"
        << "HPLComfortSuppressScriptRoll=0\n"
        << "HPLComfortSuppressLeanRoll=1\n"
        << "HPLComfortSuppressMoveRoll=1\n"
        << "HPLComfortSuppressClimbRoll=1\n"
        << "HPLComfortDepthOfFieldControl=0\n"
        << "HPLComfortOpticsControl=0\n"
        << "HPLComfortSuppressFOV=1\n"
        << "HPLComfortSuppressFOVMultiplier=1\n"
        << "HPLComfortSuppressAspectMultiplier=1\n"
        << "HPLLoadingScreenControl=0\n"
        << "HPLLoadingScreenExitBlackoutFrames=2\n"
        << "HPLVideoLifecycleProbe=0\n"
        << "HPLScreenEffectControl=0\n"
        << "HPLScreenEffectDistanceMeters=1.5\n"
        << "HPLSubtitleControl=0\n"
        << "HPLSubtitleWidthScale=0.9\n"
        << "HPLSubtitleFontScale=1.15\n"
        << "HPLSubtitleVerticalOffset=0.0\n"
        << "HPLComfortLogInterval=120\n"
        << "HPLCameraLogInterval=120\n"
        << "HPLStereoAFR=0\n"
        << "HPLWorldScale=1.0\n"
        << "HPLRenderStageProbe=0\n"
        << "HPLDualRenderReplayProbe=0\n"
        << "HPLDualRenderAutoProbe=0\n"
        << "HPLDualRenderAutoProbeCount=3\n"
        << "HPLDualRenderAutoProbeDelayFrames=180\n"
        << "HPLDualRenderAutoProbeIntervalFrames=180\n"
        << "HPLPerEyePerformanceTelemetry=0\n"
        << "HPLPerEyeGpuTelemetry=0\n"
        << "HPLGpuQueryPoolSize=128\n"
        << "HPLAudioListenerProbe=0\n"
        << "HPLAudioListenerCorrection=0\n"
        << "HPLAudioListenerTranslation=0\n"
        << "HPLPostEffectControl=0\n"
        << "HPLPostEffectResourceProbe=0\n"
        << "HPLPostEffectBypassDefault=0\n"
        << "HPLPostEffectDisableImageTrail=1\n"
        << "HPLPostEffectDisableVideoDistortion=1\n"
        << "HPLPostEffectDisableChromaticAberration=1\n"
        << "HPLPostEffectDisableRadialBlur=1\n"
        << "HPLShadowJitterControl=0\n"
        << "HPLShadowJitterSuppressedDefault=0\n"
        << "HPLCompatibilityLogInterval=120\n"
        << "ForceDisableVsync=0\n\n"
        << "[OpenXR]\n"
        << "# Requires configuring with -DSOMAVR_ENABLE_OPENXR=ON.\n"
        << "Probe=0\n"
        << "SessionProbe=0\n"
        << "ReleaseAfterProbe=1\n"
        << "BootstrapFrame=0\n"
        << "HoldFrames=0\n"
        << "ManualStart=0\n"
        << "FrameSubmit=0\n"
        << "MirrorBackbuffer=1\n"
        << "DesktopMirrorEye=native\n"
        << "DesktopMirrorAspect=fit\n"
        << "DepthCompositionProbe=0\n"
        << "DepthCompositionSubmit=0\n"
        << "ResolutionScalePercent=100\n"
        << "ReferenceSpace=local\n"
        << "InputEnabled=0\n"
        << "InputLogInterval=120\n"
        << "RecoveryEnabled=1\n"
        << "RecoveryDelayFrames=120\n"
        << "TrackingHoldFrames=30\n"
        << "TrackingRecoveryBlackoutFrames=2\n"
        << "HudLayer=0\n"
        << "HudWidthPixels=1600\n"
        << "HudHeightPixels=900\n"
        << "HudDistanceMeters=1.5\n"
        << "HudWidthMeters=1.6\n"
        << "HudVerticalOffsetMeters=0.0\n"
        << "HudMaxAgeFrames=2\n"
        << "HudSuppressCenterCrosshair=0\n"
        << "HudCrosshairClearRadiusPixels=48\n"
        << "HudCapturePausedMenu=0\n\n"
        << "InteractionReticle=0\n"
        << "InteractionReticleSemantic=0\n"
        << "InteractionReticleNativeIcons=0\n"
        << "InteractionReticleSizePixels=64\n"
        << "InteractionReticleAngularSizeDegrees=0.75\n"
        << "InteractionReticleMinSizeMeters=0.008\n"
        << "InteractionReticleMaxSizeMeters=0.08\n"
        << "InteractionReticleMinDistanceMeters=0.15\n"
        << "InteractionReticleMaxDistanceMeters=8.0\n"
        << "InteractionReticleMaxAgeFrames=2\n\n"
        << "[Controller]\n"
        << "Enabled=0\n"
        << "MoveDeadzone=0.35\n"
        << "MoveReleaseDeadzone=0.25\n"
        << "NativeLocomotion=1\n"
        << "MovementReference=body\n"
        << "PhysicalCrouch=0\n"
        << "PhysicalCrouchEnterMeters=0.35\n"
        << "PhysicalCrouchExitMeters=0.25\n"
        << "SnapTurn=1\n"
        << "TurnDeadzone=0.65\n"
        << "TurnReleaseDeadzone=0.35\n"
        << "SnapTurnPixels=420\n"
        << "SmoothTurnPixelsPerSecond=900\n"
        << "NativeTurn=1\n"
        << "SnapTurnDegrees=30\n"
        << "SmoothTurnDegreesPerSecond=120\n"
        << "NativeTurnSign=-1\n"
        << "Interaction=1\n"
        << "Flashlight=1\n"
        << "Inventory=1\n"
        << "Menu=1\n"
        << "MenuPointer=1\n"
        << "MenuPointerHorizontalDegrees=70\n"
        << "MenuPointerVerticalDegrees=50\n"
        << "MenuPointerSmoothing=0.35\n"
        << "TerminalPointer=1\n"
        << "TerminalPointerHorizontalDegrees=70\n"
        << "TerminalPointerVerticalDegrees=50\n"
        << "TerminalPointerSmoothing=0.35\n"
        << "RecenterChord=1\n"
        << "Haptics=1\n"
        << "HapticAmplitude=0.35\n"
        << "HapticDurationMs=30\n"
        << "FocusHaptics=0\n"
        << "FocusHapticAmplitude=0.12\n"
        << "FocusHapticDurationMs=15\n"
        << "FocusHapticCooldownFrames=15\n"
        << "DominantHand=right\n"
        << "SwapSticks=0\n"
        << "OneHandFallback=1\n"
        << "SuppressDuringAuthoredCamera=1\n"
        << "InteractionRay=0\n"
        << "InteractionRayOriginTolerance=0.75\n"
        << "GrabTranslation=0\n"
        << "GrabTranslationScale=1.0\n"
        << "GrabMaxOffsetMeters=0.75\n"
        << "GrabRotation=0\n"
        << "GrabRotationGain=100.0\n"
        << "GrabRotationSign=1.0\n"
        << "GrabMaxAngularSpeed=6.0\n"
        << "TwoHandHudObject=0\n"
        << "TwoHandGrabRotation=0\n"
        << "TwoHandSqueezeThreshold=0.75\n"
        << "TwoHandMinSeparationMeters=0.08\n"
        << "TwoHandMaxSeparationMeters=1.2\n"
        << "TwoHandDirectionBlend=1.0\n"
        << "ThrowRedirect=0\n"
        << "ThrowVelocityScale=0\n"
        << "ThrowVelocityThreshold=0.35\n"
        << "ThrowVelocityReference=2.0\n"
        << "ManipulationMappings=1\n"
        << "ManipulationMotion=0\n"
        << "ManipulationMotionPixelsPerMeter=900\n"
        << "ManipulationMotionDeadzoneMeters=0.0005\n"
        << "ManipulationMotionMaxPixelsPerFrame=80\n"
        << "ManipulationMotionHorizontalSign=1\n"
        << "ManipulationMotionVerticalSign=-1\n"
        << "HandTrackingProbe=0\n"
        << "HandControllerRoot=0\n"
        << "HandRootOffsetX=0.0\n"
        << "HandRootOffsetY=-0.075\n"
        << "HandRootOffsetZ=0.0\n"
        << "HandRootPitchDegrees=0.0\n"
        << "HandRootYawDegrees=0.0\n"
        << "HandRootRollDegrees=0.0\n"
        << "ControllerHudObject=0\n"
        << "HudObjectOffsetX=0.0\n"
        << "HudObjectOffsetY=0.0\n"
        << "HudObjectOffsetZ=0.0\n"
        << "HudObjectPitchDegrees=0.0\n"
        << "HudObjectYawDegrees=0.0\n"
        << "HudObjectRollDegrees=0.0\n"
        << "ControllerFlashlightAim=0\n"
        << "ControllerFlashlightGameplayRay=0\n"
        << "FlashlightOffsetX=0.0\n"
        << "FlashlightOffsetY=0.0\n"
        << "FlashlightOffsetZ=0.03\n"
        << "FlashlightPitchDegrees=0.0\n"
        << "FlashlightYawDegrees=0.0\n"
        << "FlashlightRollDegrees=0.0\n"
        << "ComfortBlackoutFrames=2\n"
        << "StateTransitionBlackoutFrames=0\n"
        << "RecenterHoldMs=900\n"
        << "MaxInputAgeFrames=8\n"
        << "LogInterval=120\n";
}

void ConfigManager::LoadFromFile()
{
    std::ifstream in(path_);
    if (!in) {
        return;
    }

    std::string section;
    std::string line;
    while (std::getline(in, line)) {
        const size_t comment = line.find_first_of("#;");
        if (comment != std::string::npos) {
            line.erase(comment);
        }
        line = Trim(line);
        if (line.empty()) {
            continue;
        }

        if (line.front() == '[' && line.back() == ']') {
            section = Lower(Trim(line.substr(1, line.size() - 2)));
            continue;
        }

        const size_t equals = line.find('=');
        if (equals == std::string::npos) {
            continue;
        }

        const std::string key = Lower(Trim(line.substr(0, equals)));
        const std::string value = Trim(line.substr(equals + 1));

        if (section == "logging") {
            if (key == "level") {
                config_.logLevel = Logger::ParseLevel(value, config_.logLevel);
            }
            continue;
        }

        if (section == "hooks") {
            if (key == "swapbuffers") config_.hookSwapBuffers = ParseBool(value, config_.hookSwapBuffers);
            else if (key == "wglmakecurrent") config_.hookWglMakeCurrent = ParseBool(value, config_.hookWglMakeCurrent);
            else if (key == "fixedfunctionmatrices") config_.hookFixedFunctionMatrices = ParseBool(value, config_.hookFixedFunctionMatrices);
            else if (key == "uniformmatrices") config_.hookUniformMatrices = ParseBool(value, config_.hookUniformMatrices);
            else if (key == "viewport") config_.hookViewport = ParseBool(value, config_.hookViewport);
            else if (key == "drawcalls") config_.hookDrawCalls = ParseBool(value, config_.hookDrawCalls);
            else if (key == "framebuffer") config_.hookFramebuffer = ParseBool(value, config_.hookFramebuffer);
            else if (key == "framerate" || key == "framesummaryinterval") config_.frameSummaryInterval = ParseInt(value, config_.frameSummaryInterval, 1, 600);
            else if (key == "matrixsamplelimitperframe") config_.matrixSampleLimitPerFrame = ParseInt(value, config_.matrixSampleLimitPerFrame, 0, 256);
            else if (key == "uniformnameloglimit") config_.uniformNameLogLimit = ParseInt(value, config_.uniformNameLogLimit, 0, 4096);
            else if (key == "uniformmatrixloglimit") config_.uniformMatrixLogLimit = ParseInt(value, config_.uniformMatrixLogLimit, 0, 4096);
            else if (key == "uniformmatrixprojectiononly") config_.uniformMatrixProjectionOnly = ParseBool(value, config_.uniformMatrixProjectionOnly);
            else if (key == "matrixcapture") config_.matrixCaptureEnabled = ParseBool(value, config_.matrixCaptureEnabled);
            else if (key == "matrixcaptureframes") config_.matrixCaptureFrames = ParseInt(value, config_.matrixCaptureFrames, 1, 3600);
            else if (key == "matrixcapturestackdepth") config_.matrixCaptureStackDepth = ParseInt(value, config_.matrixCaptureStackDepth, 1, 32);
            else if (key == "matrixcapturemaxsites") config_.matrixCaptureMaxSites = ParseInt(value, config_.matrixCaptureMaxSites, 1, 512);
            else if (key == "matrixcapturesamplesperuniform") config_.matrixCaptureSamplesPerUniform = ParseInt(value, config_.matrixCaptureSamplesPerUniform, 0, 32);
            else if (key == "renderdiagnosticcapture") config_.renderDiagnosticCapture = ParseBool(value, config_.renderDiagnosticCapture);
            else if (key == "renderdiagnosticframes") config_.renderDiagnosticFrames = ParseInt(value, config_.renderDiagnosticFrames, 2, 120);
            else if (key == "renderdiagnosticmaxprograms") config_.renderDiagnosticMaxPrograms = ParseInt(value, config_.renderDiagnosticMaxPrograms, 1, 1024);
            else if (key == "renderdiagnosticmaxdraws") config_.renderDiagnosticMaxDraws = ParseInt(value, config_.renderDiagnosticMaxDraws, 1, 100000);
            else if (key == "hplcamerabridge") config_.hplCameraBridge = ParseBool(value, config_.hplCameraBridge);
            else if (key == "hpllifecycleshutdown") config_.hplLifecycleShutdown = ParseBool(value, config_.hplLifecycleShutdown);
            else if (key == "hplprojectioncentercontrol") config_.hplProjectionCenterControl = ParseBool(value, config_.hplProjectionCenterControl);
            else if (key == "hplprojectioncentereddefault") config_.hplProjectionCenteredDefault = ParseBool(value, config_.hplProjectionCenteredDefault);
            else if (key == "hplroomscalecontrol") config_.hplRoomscaleControl = ParseBool(value, config_.hplRoomscaleControl);
            else if (key == "hplroomscaleenableddefault") config_.hplRoomscaleEnabledDefault = ParseBool(value, config_.hplRoomscaleEnabledDefault);
            else if (key == "hplroomscalevertical") config_.hplRoomscaleVertical = ParseBool(value, config_.hplRoomscaleVertical);
            else if (key == "hplroomscalesafety") config_.hplRoomscaleSafety = ParseBool(value, config_.hplRoomscaleSafety);
            else if (key == "hplroomscalesafetydynamic") config_.hplRoomscaleSafetyDynamic = ParseBool(value, config_.hplRoomscaleSafetyDynamic);
            else if (key == "hplroomscalesafetyclearancemeters") config_.hplRoomscaleSafetyClearanceMeters = ParseFloat(value, config_.hplRoomscaleSafetyClearanceMeters, 0.0f, 1.0f);
            else if (key == "hplroomscalesafetyiterations") config_.hplRoomscaleSafetyIterations = ParseInt(value, config_.hplRoomscaleSafetyIterations, 1, 12);
            else if (key == "hplroomscalesafetyradiusmeters") config_.hplRoomscaleSafetyRadiusMeters = ParseFloat(value, config_.hplRoomscaleSafetyRadiusMeters, 0.0f, 0.5f);
            else if (key == "hplroomscalesafetyverticalradiusmeters") config_.hplRoomscaleSafetyVerticalRadiusMeters = ParseFloat(value, config_.hplRoomscaleSafetyVerticalRadiusMeters, 0.0f, 0.5f);
            else if (key == "hplroomscalesafetyradialsamples") config_.hplRoomscaleSafetyRadialSamples = ParseInt(value, config_.hplRoomscaleSafetyRadialSamples, 0, 16);
            else if (key == "hplroomscalebodyreconciliation") config_.hplRoomscaleBodyReconciliation = ParseBool(value, config_.hplRoomscaleBodyReconciliation);
            else if (key == "hplroomscalebodyreconciliationthresholdmeters") config_.hplRoomscaleBodyReconciliationThresholdMeters = ParseFloat(value, config_.hplRoomscaleBodyReconciliationThresholdMeters, 0.05f, 2.0f);
            else if (key == "hplroomscalebodyreconciliationtargetmeters") config_.hplRoomscaleBodyReconciliationTargetMeters = ParseFloat(value, config_.hplRoomscaleBodyReconciliationTargetMeters, 0.0f, 1.95f);
            else if (key == "hplroomscalebodyreconciliationmaxstepmeters") config_.hplRoomscaleBodyReconciliationMaxStepMeters = ParseFloat(value, config_.hplRoomscaleBodyReconciliationMaxStepMeters, 0.001f, 0.1f);
            else if (key == "hplroomscalebodyreconciliationholdframes") config_.hplRoomscaleBodyReconciliationHoldFrames = ParseInt(value, config_.hplRoomscaleBodyReconciliationHoldFrames, 1, 600);
            else if (key == "hpleyeheightoffsetmeters") config_.hplEyeHeightOffsetMeters = ParseFloat(value, config_.hplEyeHeightOffsetMeters, -2.0f, 2.0f);
            else if (key == "hplrecentercontrol") config_.hplRecenterControl = ParseBool(value, config_.hplRecenterControl);
            else if (key == "hplreflectionfadecontrol") config_.hplReflectionFadeControl = ParseBool(value, config_.hplReflectionFadeControl);
            else if (key == "hplnativecamerarollsuppression") config_.hplNativeCameraRollSuppression = ParseBool(value, config_.hplNativeCameraRollSuppression);
            else if (key == "hplcomfortcameraaddcontrol") config_.hplComfortCameraAddControl = ParseBool(value, config_.hplComfortCameraAddControl);
            else if (key == "hplcomfortsuppressheadbob") config_.hplComfortSuppressHeadBob = ParseBool(value, config_.hplComfortSuppressHeadBob);
            else if (key == "hplcomfortsuppresscamerashake") config_.hplComfortSuppressCameraShake = ParseBool(value, config_.hplComfortSuppressCameraShake);
            else if (key == "hplcomfortsuppresssway") config_.hplComfortSuppressSway = ParseBool(value, config_.hplComfortSuppressSway);
            else if (key == "hplcomfortcamerarollcontrol") config_.hplComfortCameraRollControl = ParseBool(value, config_.hplComfortCameraRollControl);
            else if (key == "hplcomfortsuppressscriptroll") config_.hplComfortSuppressScriptRoll = ParseBool(value, config_.hplComfortSuppressScriptRoll);
            else if (key == "hplcomfortsuppressleanroll") config_.hplComfortSuppressLeanRoll = ParseBool(value, config_.hplComfortSuppressLeanRoll);
            else if (key == "hplcomfortsuppressmoveroll") config_.hplComfortSuppressMoveRoll = ParseBool(value, config_.hplComfortSuppressMoveRoll);
            else if (key == "hplcomfortsuppressclimbroll") config_.hplComfortSuppressClimbRoll = ParseBool(value, config_.hplComfortSuppressClimbRoll);
            else if (key == "hplcomfortdepthoffieldcontrol") config_.hplComfortDepthOfFieldControl = ParseBool(value, config_.hplComfortDepthOfFieldControl);
            else if (key == "hplcomfortopticscontrol") config_.hplComfortOpticsControl = ParseBool(value, config_.hplComfortOpticsControl);
            else if (key == "hplcomfortsuppressfov") config_.hplComfortSuppressFov = ParseBool(value, config_.hplComfortSuppressFov);
            else if (key == "hplcomfortsuppressfovmultiplier") config_.hplComfortSuppressFovMultiplier = ParseBool(value, config_.hplComfortSuppressFovMultiplier);
            else if (key == "hplcomfortsuppressaspectmultiplier") config_.hplComfortSuppressAspectMultiplier = ParseBool(value, config_.hplComfortSuppressAspectMultiplier);
            else if (key == "hplloadingscreencontrol") config_.hplLoadingScreenControl = ParseBool(value, config_.hplLoadingScreenControl);
            else if (key == "hplloadingscreenexitblackoutframes") config_.hplLoadingScreenExitBlackoutFrames = ParseInt(value, config_.hplLoadingScreenExitBlackoutFrames, 0, 120);
            else if (key == "hplvideolifecycleprobe") config_.hplVideoLifecycleProbe = ParseBool(value, config_.hplVideoLifecycleProbe);
            else if (key == "hplscreeneffectcontrol") config_.hplScreenEffectControl = ParseBool(value, config_.hplScreenEffectControl);
            else if (key == "hplscreeneffectdistancemeters") config_.hplScreenEffectDistanceMeters = ParseFloat(value, config_.hplScreenEffectDistanceMeters, 0.25f, 5.0f);
            else if (key == "hplsubtitlecontrol") config_.hplSubtitleControl = ParseBool(value, config_.hplSubtitleControl);
            else if (key == "hplsubtitlewidthscale") config_.hplSubtitleWidthScale = ParseFloat(value, config_.hplSubtitleWidthScale, 0.5f, 2.0f);
            else if (key == "hplsubtitlefontscale") config_.hplSubtitleFontScale = ParseFloat(value, config_.hplSubtitleFontScale, 0.5f, 2.0f);
            else if (key == "hplsubtitleverticaloffset") config_.hplSubtitleVerticalOffset = ParseFloat(value, config_.hplSubtitleVerticalOffset, -2048.0f, 2048.0f);
            else if (key == "hplcomfortloginterval") config_.hplComfortLogInterval = ParseInt(value, config_.hplComfortLogInterval, 1, 100000);
            else if (key == "hplcameraloginterval") config_.hplCameraLogInterval = ParseInt(value, config_.hplCameraLogInterval, 1, 100000);
            else if (key == "hplstereoafr") config_.hplStereoAfr = ParseBool(value, config_.hplStereoAfr);
            else if (key == "hplworldscale") config_.hplWorldScale = ParseFloat(value, config_.hplWorldScale, 0.1f, 10.0f);
            else if (key == "hplrenderstageprobe") config_.hplRenderStageProbe = ParseBool(value, config_.hplRenderStageProbe);
            else if (key == "hpldualrenderreplayprobe") config_.hplDualRenderReplayProbe = ParseBool(value, config_.hplDualRenderReplayProbe);
            else if (key == "hpldualrenderautoprobe") config_.hplDualRenderAutoProbe = ParseBool(value, config_.hplDualRenderAutoProbe);
            else if (key == "hpldualrenderautoprobecount") config_.hplDualRenderAutoProbeCount = ParseInt(value, config_.hplDualRenderAutoProbeCount, 1, 10);
            else if (key == "hpldualrenderautoprobedelayframes") config_.hplDualRenderAutoProbeDelayFrames = ParseInt(value, config_.hplDualRenderAutoProbeDelayFrames, 30, 100000);
            else if (key == "hpldualrenderautoprobeintervalframes") config_.hplDualRenderAutoProbeIntervalFrames = ParseInt(value, config_.hplDualRenderAutoProbeIntervalFrames, 30, 100000);
            else if (key == "hplpereyeperformancetelemetry") config_.hplPerEyePerformanceTelemetry = ParseBool(value, config_.hplPerEyePerformanceTelemetry);
            else if (key == "hplpereyegputelemetry") config_.hplPerEyeGpuTelemetry = ParseBool(value, config_.hplPerEyeGpuTelemetry);
            else if (key == "hplgpuquerypoolsize") config_.hplGpuQueryPoolSize = ParseInt(value, config_.hplGpuQueryPoolSize, 16, 512);
            else if (key == "hplaudiolistenerprobe") config_.hplAudioListenerProbe = ParseBool(value, config_.hplAudioListenerProbe);
            else if (key == "hplaudiolistenercorrection") config_.hplAudioListenerCorrection = ParseBool(value, config_.hplAudioListenerCorrection);
            else if (key == "hplaudiolistenertranslation") config_.hplAudioListenerTranslation = ParseBool(value, config_.hplAudioListenerTranslation);
            else if (key == "hplposteffectcontrol") config_.hplPostEffectControl = ParseBool(value, config_.hplPostEffectControl);
            else if (key == "hplposteffectresourceprobe") config_.hplPostEffectResourceProbe = ParseBool(value, config_.hplPostEffectResourceProbe);
            else if (key == "hplposteffectbypassdefault") config_.hplPostEffectBypassDefault = ParseBool(value, config_.hplPostEffectBypassDefault);
            else if (key == "hplposteffectdisableimagetrail") config_.hplPostEffectDisableImageTrail = ParseBool(value, config_.hplPostEffectDisableImageTrail);
            else if (key == "hplposteffectdisablevideodistortion") config_.hplPostEffectDisableVideoDistortion = ParseBool(value, config_.hplPostEffectDisableVideoDistortion);
            else if (key == "hplposteffectdisablechromaticaberration") config_.hplPostEffectDisableChromaticAberration = ParseBool(value, config_.hplPostEffectDisableChromaticAberration);
            else if (key == "hplposteffectdisableradialblur") config_.hplPostEffectDisableRadialBlur = ParseBool(value, config_.hplPostEffectDisableRadialBlur);
            else if (key == "hplshadowjittercontrol") config_.hplShadowJitterControl = ParseBool(value, config_.hplShadowJitterControl);
            else if (key == "hplshadowjittersuppresseddefault") config_.hplShadowJitterSuppressedDefault = ParseBool(value, config_.hplShadowJitterSuppressedDefault);
            else if (key == "hplcompatibilityloginterval") config_.hplCompatibilityLogInterval = ParseInt(value, config_.hplCompatibilityLogInterval, 1, 100000);
            else if (key == "forcedisablevsync") config_.forceDisableVsync = ParseBool(value, config_.forceDisableVsync);
            continue;
        }

        if (section == "openxr") {
            if (key == "probe") {
                config_.openxrProbe = ParseBool(value, config_.openxrProbe);
            } else if (key == "sessionprobe") {
                config_.openxrSessionProbe = ParseBool(value, config_.openxrSessionProbe);
            } else if (key == "releaseafterprobe") {
                config_.openxrReleaseAfterProbe = ParseBool(value, config_.openxrReleaseAfterProbe);
            } else if (key == "bootstrapframe") {
                config_.openxrBootstrapFrame = ParseInt(value, config_.openxrBootstrapFrame, 0, 1000000);
            } else if (key == "holdframes") {
                config_.openxrHoldFrames = ParseInt(value, config_.openxrHoldFrames, 0, 1000000);
            } else if (key == "manualstart" || key == "startonf8") {
                config_.openxrManualStart = ParseBool(value, config_.openxrManualStart);
            } else if (key == "framesubmit") {
                config_.openxrFrameSubmit = ParseBool(value, config_.openxrFrameSubmit);
            } else if (key == "mirrorbackbuffer") {
                config_.openxrMirrorBackbuffer = ParseBool(value, config_.openxrMirrorBackbuffer);
            } else if (key == "desktopmirroreye") {
                const std::string eye = Lower(Trim(value));
                if (eye == "native" || eye == "left" || eye == "right") {
                    config_.openxrDesktopMirrorEye = eye;
                }
            } else if (key == "desktopmirroraspect") {
                const std::string aspect = Lower(Trim(value));
                if (aspect == "fit" || aspect == "fill" || aspect == "stretch") {
                    config_.openxrDesktopMirrorAspect = aspect;
                }
            } else if (key == "depthcompositionprobe") {
                config_.openxrDepthCompositionProbe = ParseBool(value, config_.openxrDepthCompositionProbe);
            } else if (key == "depthcompositionsubmit") {
                config_.openxrDepthCompositionSubmit = ParseBool(value, config_.openxrDepthCompositionSubmit);
            } else if (key == "resolutionscalepercent") {
                config_.openxrResolutionScalePercent = ParseInt(value, config_.openxrResolutionScalePercent, 25, 200);
            } else if (key == "referencespace") {
                const std::string referenceSpace = Lower(Trim(value));
                if (referenceSpace == "local" || referenceSpace == "stage") {
                    config_.openxrReferenceSpace = referenceSpace;
                }
            } else if (key == "inputenabled") {
                config_.openxrInputEnabled = ParseBool(value, config_.openxrInputEnabled);
            } else if (key == "inputloginterval") {
                config_.openxrInputLogInterval = ParseInt(value, config_.openxrInputLogInterval, 1, 100000);
            } else if (key == "recoveryenabled") {
                config_.openxrRecoveryEnabled = ParseBool(value, config_.openxrRecoveryEnabled);
            } else if (key == "recoverydelayframes") {
                config_.openxrRecoveryDelayFrames = ParseInt(value, config_.openxrRecoveryDelayFrames, 1, 100000);
            } else if (key == "trackingholdframes") {
                config_.openxrTrackingHoldFrames = ParseInt(value, config_.openxrTrackingHoldFrames, 0, 600);
            } else if (key == "trackingrecoveryblackoutframes") {
                config_.openxrTrackingRecoveryBlackoutFrames = ParseInt(value, config_.openxrTrackingRecoveryBlackoutFrames, 0, 120);
            } else if (key == "hudlayer") {
                config_.openxrHudLayer = ParseBool(value, config_.openxrHudLayer);
            } else if (key == "hudwidthpixels") {
                config_.openxrHudWidthPixels = ParseInt(value, config_.openxrHudWidthPixels, 256, 4096);
            } else if (key == "hudheightpixels") {
                config_.openxrHudHeightPixels = ParseInt(value, config_.openxrHudHeightPixels, 256, 4096);
            } else if (key == "huddistancemeters") {
                config_.openxrHudDistanceMeters = ParseFloat(value, config_.openxrHudDistanceMeters, 0.25f, 10.0f);
            } else if (key == "hudwidthmeters") {
                config_.openxrHudWidthMeters = ParseFloat(value, config_.openxrHudWidthMeters, 0.25f, 10.0f);
            } else if (key == "hudverticaloffsetmeters") {
                config_.openxrHudVerticalOffsetMeters = ParseFloat(value, config_.openxrHudVerticalOffsetMeters, -5.0f, 5.0f);
            } else if (key == "hudmaxageframes") {
                config_.openxrHudMaxAgeFrames = ParseInt(value, config_.openxrHudMaxAgeFrames, 0, 30);
            } else if (key == "hudsuppresscentercrosshair") {
                config_.openxrHudSuppressCenterCrosshair = ParseBool(value, config_.openxrHudSuppressCenterCrosshair);
            } else if (key == "hudcrosshairclearradiuspixels") {
                config_.openxrHudCrosshairClearRadiusPixels = ParseInt(value, config_.openxrHudCrosshairClearRadiusPixels, 4, 256);
            } else if (key == "hudcapturepausedmenu") {
                config_.openxrHudCapturePausedMenu = ParseBool(value, config_.openxrHudCapturePausedMenu);
            } else if (key == "interactionreticle") {
                config_.openxrInteractionReticle = ParseBool(value, config_.openxrInteractionReticle);
            } else if (key == "interactionreticlesemantic") {
                config_.openxrInteractionReticleSemantic = ParseBool(value, config_.openxrInteractionReticleSemantic);
            } else if (key == "interactionreticlenativeicons") {
                config_.openxrInteractionReticleNativeIcons = ParseBool(value, config_.openxrInteractionReticleNativeIcons);
            } else if (key == "interactionreticlesizepixels") {
                config_.openxrInteractionReticleSizePixels = ParseInt(value, config_.openxrInteractionReticleSizePixels, 32, 512);
            } else if (key == "interactionreticleangularsizedegrees") {
                config_.openxrInteractionReticleAngularSizeDegrees = ParseFloat(value, config_.openxrInteractionReticleAngularSizeDegrees, 0.1f, 5.0f);
            } else if (key == "interactionreticleminsizemeters") {
                config_.openxrInteractionReticleMinSizeMeters = ParseFloat(value, config_.openxrInteractionReticleMinSizeMeters, 0.001f, 0.5f);
            } else if (key == "interactionreticlemaxsizemeters") {
                config_.openxrInteractionReticleMaxSizeMeters = ParseFloat(value, config_.openxrInteractionReticleMaxSizeMeters, 0.001f, 1.0f);
            } else if (key == "interactionreticlemindistancemeters") {
                config_.openxrInteractionReticleMinDistanceMeters = ParseFloat(value, config_.openxrInteractionReticleMinDistanceMeters, 0.01f, 10.0f);
            } else if (key == "interactionreticlemaxdistancemeters") {
                config_.openxrInteractionReticleMaxDistanceMeters = ParseFloat(value, config_.openxrInteractionReticleMaxDistanceMeters, 0.1f, 100.0f);
            } else if (key == "interactionreticlemaxageframes") {
                config_.openxrInteractionReticleMaxAgeFrames = ParseInt(value, config_.openxrInteractionReticleMaxAgeFrames, 0, 30);
            } else if (key == "statuspanel") {
                config_.openxrStatusPanel = ParseBool(value, config_.openxrStatusPanel);
            } else if (key == "statuspanelwidthpixels") {
                config_.openxrStatusPanelWidthPixels = ParseInt(value, config_.openxrStatusPanelWidthPixels, 512, 4096);
            } else if (key == "statuspanelheightpixels") {
                config_.openxrStatusPanelHeightPixels = ParseInt(value, config_.openxrStatusPanelHeightPixels, 256, 4096);
            } else if (key == "statuspaneldistancemeters") {
                config_.openxrStatusPanelDistanceMeters = ParseFloat(value, config_.openxrStatusPanelDistanceMeters, 0.25f, 10.0f);
            } else if (key == "statuspanelwidthmeters") {
                config_.openxrStatusPanelWidthMeters = ParseFloat(value, config_.openxrStatusPanelWidthMeters, 0.25f, 5.0f);
            } else if (key == "statuspanelverticaloffsetmeters") {
                config_.openxrStatusPanelVerticalOffsetMeters = ParseFloat(value, config_.openxrStatusPanelVerticalOffsetMeters, -5.0f, 5.0f);
            }
            continue;
        }

        if (section == "controller") {
            if (key == "enabled") config_.hplControllerInput = ParseBool(value, config_.hplControllerInput);
            else if (key == "movedeadzone") config_.hplControllerMoveDeadzone = ParseFloat(value, config_.hplControllerMoveDeadzone, 0.05f, 0.95f);
            else if (key == "movereleasedeadzone") config_.hplControllerMoveReleaseDeadzone = ParseFloat(value, config_.hplControllerMoveReleaseDeadzone, 0.0f, 0.9f);
            else if (key == "nativelocomotion") config_.hplControllerNativeLocomotion = ParseBool(value, config_.hplControllerNativeLocomotion);
            else if (key == "movementreference") {
                const std::string reference = Lower(Trim(value));
                if (reference == "body" || reference == "head") config_.hplControllerMovementReference = reference;
            }
            else if (key == "physicalcrouch") config_.hplControllerPhysicalCrouch = ParseBool(value, config_.hplControllerPhysicalCrouch);
            else if (key == "physicalcrouchentermeters") config_.hplControllerPhysicalCrouchEnterMeters = ParseFloat(value, config_.hplControllerPhysicalCrouchEnterMeters, 0.10f, 1.20f);
            else if (key == "physicalcrouchexitmeters") config_.hplControllerPhysicalCrouchExitMeters = ParseFloat(value, config_.hplControllerPhysicalCrouchExitMeters, 0.05f, 1.10f);
            else if (key == "snapturn") config_.hplControllerSnapTurn = ParseBool(value, config_.hplControllerSnapTurn);
            else if (key == "turndeadzone") config_.hplControllerTurnDeadzone = ParseFloat(value, config_.hplControllerTurnDeadzone, 0.05f, 0.95f);
            else if (key == "turnreleasedeadzone") config_.hplControllerTurnReleaseDeadzone = ParseFloat(value, config_.hplControllerTurnReleaseDeadzone, 0.0f, 0.9f);
            else if (key == "snapturnpixels") config_.hplControllerSnapTurnPixels = ParseInt(value, config_.hplControllerSnapTurnPixels, 1, 4000);
            else if (key == "smoothturnpixelspersecond") config_.hplControllerSmoothTurnPixelsPerSecond = ParseFloat(value, config_.hplControllerSmoothTurnPixelsPerSecond, 1.0f, 5000.0f);
            else if (key == "nativeturn") config_.hplControllerNativeTurn = ParseBool(value, config_.hplControllerNativeTurn);
            else if (key == "snapturndegrees") config_.hplControllerSnapTurnDegrees = ParseFloat(value, config_.hplControllerSnapTurnDegrees, 1.0f, 180.0f);
            else if (key == "smoothturndegreespersecond") config_.hplControllerSmoothTurnDegreesPerSecond = ParseFloat(value, config_.hplControllerSmoothTurnDegreesPerSecond, 1.0f, 720.0f);
            else if (key == "nativeturnsign") config_.hplControllerNativeTurnSign = ParseFloat(value, config_.hplControllerNativeTurnSign, -1.0f, 1.0f);
            else if (key == "interaction") config_.hplControllerInteraction = ParseBool(value, config_.hplControllerInteraction);
            else if (key == "flashlight") config_.hplControllerFlashlight = ParseBool(value, config_.hplControllerFlashlight);
            else if (key == "inventory") config_.hplControllerInventory = ParseBool(value, config_.hplControllerInventory);
            else if (key == "menu") config_.hplControllerMenu = ParseBool(value, config_.hplControllerMenu);
            else if (key == "menupointer") config_.hplControllerMenuPointer = ParseBool(value, config_.hplControllerMenuPointer);
            else if (key == "menupointerhorizontaldegrees") config_.hplControllerMenuPointerHorizontalDegrees = ParseFloat(value, config_.hplControllerMenuPointerHorizontalDegrees, 10.0f, 170.0f);
            else if (key == "menupointerverticaldegrees") config_.hplControllerMenuPointerVerticalDegrees = ParseFloat(value, config_.hplControllerMenuPointerVerticalDegrees, 10.0f, 170.0f);
            else if (key == "menupointersmoothing") config_.hplControllerMenuPointerSmoothing = ParseFloat(value, config_.hplControllerMenuPointerSmoothing, 0.01f, 1.0f);
            else if (key == "terminalpointer") config_.hplControllerTerminalPointer = ParseBool(value, config_.hplControllerTerminalPointer);
            else if (key == "terminalpointerhorizontaldegrees") config_.hplControllerTerminalPointerHorizontalDegrees = ParseFloat(value, config_.hplControllerTerminalPointerHorizontalDegrees, 10.0f, 170.0f);
            else if (key == "terminalpointerverticaldegrees") config_.hplControllerTerminalPointerVerticalDegrees = ParseFloat(value, config_.hplControllerTerminalPointerVerticalDegrees, 10.0f, 170.0f);
            else if (key == "terminalpointersmoothing") config_.hplControllerTerminalPointerSmoothing = ParseFloat(value, config_.hplControllerTerminalPointerSmoothing, 0.01f, 1.0f);
            else if (key == "recenterchord") config_.hplControllerRecenterChord = ParseBool(value, config_.hplControllerRecenterChord);
            else if (key == "haptics") config_.hplControllerHaptics = ParseBool(value, config_.hplControllerHaptics);
            else if (key == "hapticamplitude") config_.hplControllerHapticAmplitude = ParseFloat(value, config_.hplControllerHapticAmplitude, 0.0f, 1.0f);
            else if (key == "hapticdurationms") config_.hplControllerHapticDurationMs = ParseInt(value, config_.hplControllerHapticDurationMs, 1, 1000);
            else if (key == "focushaptics") config_.hplControllerFocusHaptics = ParseBool(value, config_.hplControllerFocusHaptics);
            else if (key == "focushapticamplitude") config_.hplControllerFocusHapticAmplitude = ParseFloat(value, config_.hplControllerFocusHapticAmplitude, 0.0f, 1.0f);
            else if (key == "focushapticdurationms") config_.hplControllerFocusHapticDurationMs = ParseInt(value, config_.hplControllerFocusHapticDurationMs, 1, 1000);
            else if (key == "focushapticcooldownframes") config_.hplControllerFocusHapticCooldownFrames = ParseInt(value, config_.hplControllerFocusHapticCooldownFrames, 0, 600);
            else if (key == "dominanthand") {
                const std::string dominantHand = Lower(Trim(value));
                if (dominantHand == "left" || dominantHand == "right") config_.hplControllerDominantHand = dominantHand;
            }
            else if (key == "swapsticks") config_.hplControllerSwapSticks = ParseBool(value, config_.hplControllerSwapSticks);
            else if (key == "onehandfallback") config_.hplControllerOneHandFallback = ParseBool(value, config_.hplControllerOneHandFallback);
            else if (key == "suppressduringauthoredcamera") config_.hplControllerSuppressDuringAuthoredCamera = ParseBool(value, config_.hplControllerSuppressDuringAuthoredCamera);
            else if (key == "interactionray") config_.hplControllerInteractionRay = ParseBool(value, config_.hplControllerInteractionRay);
            else if (key == "interactionrayorigintolerance") config_.hplControllerInteractionRayOriginTolerance = ParseFloat(value, config_.hplControllerInteractionRayOriginTolerance, 0.05f, 10.0f);
            else if (key == "grabtranslation") config_.hplControllerGrabTranslation = ParseBool(value, config_.hplControllerGrabTranslation);
            else if (key == "grabtranslationscale") config_.hplControllerGrabTranslationScale = ParseFloat(value, config_.hplControllerGrabTranslationScale, 0.1f, 3.0f);
            else if (key == "grabmaxoffsetmeters") config_.hplControllerGrabMaxOffsetMeters = ParseFloat(value, config_.hplControllerGrabMaxOffsetMeters, 0.05f, 3.0f);
            else if (key == "grabrotation") config_.hplControllerGrabRotation = ParseBool(value, config_.hplControllerGrabRotation);
            else if (key == "grabrotationgain") config_.hplControllerGrabRotationGain = ParseFloat(value, config_.hplControllerGrabRotationGain, 0.1f, 200.0f);
            else if (key == "grabrotationsign") config_.hplControllerGrabRotationSign = ParseFloat(value, config_.hplControllerGrabRotationSign, -1.0f, 1.0f);
            else if (key == "grabmaxangularspeed") config_.hplControllerGrabMaxAngularSpeed = ParseFloat(value, config_.hplControllerGrabMaxAngularSpeed, 0.1f, 20.0f);
            else if (key == "twohandhudobject") config_.hplControllerTwoHandHudObject = ParseBool(value, config_.hplControllerTwoHandHudObject);
            else if (key == "twohandgrabrotation") config_.hplControllerTwoHandGrabRotation = ParseBool(value, config_.hplControllerTwoHandGrabRotation);
            else if (key == "twohandsqueezethreshold") config_.hplControllerTwoHandSqueezeThreshold = ParseFloat(value, config_.hplControllerTwoHandSqueezeThreshold, 0.1f, 1.0f);
            else if (key == "twohandminseparationmeters") config_.hplControllerTwoHandMinSeparationMeters = ParseFloat(value, config_.hplControllerTwoHandMinSeparationMeters, 0.01f, 1.0f);
            else if (key == "twohandmaxseparationmeters") config_.hplControllerTwoHandMaxSeparationMeters = ParseFloat(value, config_.hplControllerTwoHandMaxSeparationMeters, 0.05f, 3.0f);
            else if (key == "twohanddirectionblend") config_.hplControllerTwoHandDirectionBlend = ParseFloat(value, config_.hplControllerTwoHandDirectionBlend, 0.0f, 1.0f);
            else if (key == "throwredirect") config_.hplControllerThrowRedirect = ParseBool(value, config_.hplControllerThrowRedirect);
            else if (key == "throwvelocityscale") config_.hplControllerThrowVelocityScale = ParseBool(value, config_.hplControllerThrowVelocityScale);
            else if (key == "throwvelocitythreshold") config_.hplControllerThrowVelocityThreshold = ParseFloat(value, config_.hplControllerThrowVelocityThreshold, 0.0f, 5.0f);
            else if (key == "throwvelocityreference") config_.hplControllerThrowVelocityReference = ParseFloat(value, config_.hplControllerThrowVelocityReference, 0.1f, 10.0f);
            else if (key == "manipulationmappings") config_.hplControllerManipulationMappings = ParseBool(value, config_.hplControllerManipulationMappings);
            else if (key == "manipulationmotion") config_.hplControllerManipulationMotion = ParseBool(value, config_.hplControllerManipulationMotion);
            else if (key == "manipulationmotionpixelspermeter") config_.hplControllerManipulationMotionPixelsPerMeter = ParseFloat(value, config_.hplControllerManipulationMotionPixelsPerMeter, 10.0f, 10000.0f);
            else if (key == "manipulationmotiondeadzonemeters") config_.hplControllerManipulationMotionDeadzoneMeters = ParseFloat(value, config_.hplControllerManipulationMotionDeadzoneMeters, 0.0f, 0.05f);
            else if (key == "manipulationmotionmaxpixelsperframe") config_.hplControllerManipulationMotionMaxPixelsPerFrame = ParseInt(value, config_.hplControllerManipulationMotionMaxPixelsPerFrame, 1, 1000);
            else if (key == "manipulationmotionhorizontalsign") config_.hplControllerManipulationMotionHorizontalSign = ParseFloat(value, config_.hplControllerManipulationMotionHorizontalSign, -1.0f, 1.0f);
            else if (key == "manipulationmotionverticalsign") config_.hplControllerManipulationMotionVerticalSign = ParseFloat(value, config_.hplControllerManipulationMotionVerticalSign, -1.0f, 1.0f);
            else if (key == "handtrackingprobe") config_.hplHandTrackingProbe = ParseBool(value, config_.hplHandTrackingProbe);
            else if (key == "handcontrollerroot") config_.hplHandControllerRoot = ParseBool(value, config_.hplHandControllerRoot);
            else if (key == "handrootoffsetx") config_.hplHandRootOffsetX = ParseFloat(value, config_.hplHandRootOffsetX, -5.0f, 5.0f);
            else if (key == "handrootoffsety") config_.hplHandRootOffsetY = ParseFloat(value, config_.hplHandRootOffsetY, -5.0f, 5.0f);
            else if (key == "handrootoffsetz") config_.hplHandRootOffsetZ = ParseFloat(value, config_.hplHandRootOffsetZ, -5.0f, 5.0f);
            else if (key == "handrootpitchdegrees") config_.hplHandRootPitchDegrees = ParseFloat(value, config_.hplHandRootPitchDegrees, -180.0f, 180.0f);
            else if (key == "handrootyawdegrees") config_.hplHandRootYawDegrees = ParseFloat(value, config_.hplHandRootYawDegrees, -180.0f, 180.0f);
            else if (key == "handrootrolldegrees") config_.hplHandRootRollDegrees = ParseFloat(value, config_.hplHandRootRollDegrees, -180.0f, 180.0f);
            else if (key == "controllerhudobject") config_.hplControllerHudObject = ParseBool(value, config_.hplControllerHudObject);
            else if (key == "hudobjectoffsetx") config_.hplHudObjectOffsetX = ParseFloat(value, config_.hplHudObjectOffsetX, -5.0f, 5.0f);
            else if (key == "hudobjectoffsety") config_.hplHudObjectOffsetY = ParseFloat(value, config_.hplHudObjectOffsetY, -5.0f, 5.0f);
            else if (key == "hudobjectoffsetz") config_.hplHudObjectOffsetZ = ParseFloat(value, config_.hplHudObjectOffsetZ, -5.0f, 5.0f);
            else if (key == "hudobjectpitchdegrees") config_.hplHudObjectPitchDegrees = ParseFloat(value, config_.hplHudObjectPitchDegrees, -180.0f, 180.0f);
            else if (key == "hudobjectyawdegrees") config_.hplHudObjectYawDegrees = ParseFloat(value, config_.hplHudObjectYawDegrees, -180.0f, 180.0f);
            else if (key == "hudobjectrolldegrees") config_.hplHudObjectRollDegrees = ParseFloat(value, config_.hplHudObjectRollDegrees, -180.0f, 180.0f);
            else if (key == "controllerflashlightaim") config_.hplControllerFlashlightAim = ParseBool(value, config_.hplControllerFlashlightAim);
            else if (key == "controllerflashlightgameplayray") config_.hplControllerFlashlightGameplayRay = ParseBool(value, config_.hplControllerFlashlightGameplayRay);
            else if (key == "flashlightoffsetx") config_.hplFlashlightOffsetX = ParseFloat(value, config_.hplFlashlightOffsetX, -5.0f, 5.0f);
            else if (key == "flashlightoffsety") config_.hplFlashlightOffsetY = ParseFloat(value, config_.hplFlashlightOffsetY, -5.0f, 5.0f);
            else if (key == "flashlightoffsetz") config_.hplFlashlightOffsetZ = ParseFloat(value, config_.hplFlashlightOffsetZ, -5.0f, 5.0f);
            else if (key == "flashlightpitchdegrees") config_.hplFlashlightPitchDegrees = ParseFloat(value, config_.hplFlashlightPitchDegrees, -180.0f, 180.0f);
            else if (key == "flashlightyawdegrees") config_.hplFlashlightYawDegrees = ParseFloat(value, config_.hplFlashlightYawDegrees, -180.0f, 180.0f);
            else if (key == "flashlightrolldegrees") config_.hplFlashlightRollDegrees = ParseFloat(value, config_.hplFlashlightRollDegrees, -180.0f, 180.0f);
            else if (key == "comfortblackoutframes") config_.hplControllerComfortBlackoutFrames = ParseInt(value, config_.hplControllerComfortBlackoutFrames, 0, 120);
            else if (key == "statetransitionblackoutframes") config_.hplControllerStateTransitionBlackoutFrames = ParseInt(value, config_.hplControllerStateTransitionBlackoutFrames, 0, 120);
            else if (key == "recenterholdms") config_.hplControllerRecenterHoldMs = ParseInt(value, config_.hplControllerRecenterHoldMs, 250, 5000);
            else if (key == "maxinputageframes") config_.hplControllerMaxInputAgeFrames = ParseInt(value, config_.hplControllerMaxInputAgeFrames, 1, 300);
            else if (key == "loginterval") config_.hplControllerLogInterval = ParseInt(value, config_.hplControllerLogInterval, 1, 100000);
        }
    }
}

} // namespace somavr
