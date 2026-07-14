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
        << "HPLEyeHeightOffsetMeters=0.0\n"
        << "HPLRecenterControl=0\n"
        << "HPLReflectionFadeControl=0\n"
        << "HPLNativeCameraRollSuppression=0\n"
        << "HPLCameraLogInterval=120\n"
        << "HPLStereoAFR=0\n"
        << "HPLWorldScale=1.0\n"
        << "HPLRenderStageProbe=0\n"
        << "HPLAudioListenerProbe=0\n"
        << "HPLAudioListenerCorrection=0\n"
        << "HPLAudioListenerTranslation=0\n"
        << "HPLPostEffectControl=0\n"
        << "HPLPostEffectBypassDefault=0\n"
        << "HPLPostEffectDisableImageTrail=1\n"
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
        << "ResolutionScalePercent=100\n"
        << "ReferenceSpace=local\n"
        << "InputEnabled=0\n"
        << "InputLogInterval=120\n"
        << "RecoveryEnabled=1\n"
        << "RecoveryDelayFrames=120\n"
        << "TrackingHoldFrames=30\n"
        << "TrackingRecoveryBlackoutFrames=2\n\n"
        << "[Controller]\n"
        << "Enabled=0\n"
        << "MoveDeadzone=0.35\n"
        << "MoveReleaseDeadzone=0.25\n"
        << "SnapTurn=1\n"
        << "TurnDeadzone=0.65\n"
        << "TurnReleaseDeadzone=0.35\n"
        << "SnapTurnPixels=420\n"
        << "SmoothTurnPixelsPerSecond=900\n"
        << "Interaction=1\n"
        << "Menu=1\n"
        << "RecenterChord=1\n"
        << "Haptics=1\n"
        << "HapticAmplitude=0.35\n"
        << "HapticDurationMs=30\n"
        << "DominantHand=right\n"
        << "SwapSticks=0\n"
        << "OneHandFallback=1\n"
        << "SuppressDuringAuthoredCamera=1\n"
        << "InteractionRay=0\n"
        << "InteractionRayOriginTolerance=0.75\n"
        << "ComfortBlackoutFrames=2\n"
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
            else if (key == "hpleyeheightoffsetmeters") config_.hplEyeHeightOffsetMeters = ParseFloat(value, config_.hplEyeHeightOffsetMeters, -2.0f, 2.0f);
            else if (key == "hplrecentercontrol") config_.hplRecenterControl = ParseBool(value, config_.hplRecenterControl);
            else if (key == "hplreflectionfadecontrol") config_.hplReflectionFadeControl = ParseBool(value, config_.hplReflectionFadeControl);
            else if (key == "hplnativecamerarollsuppression") config_.hplNativeCameraRollSuppression = ParseBool(value, config_.hplNativeCameraRollSuppression);
            else if (key == "hplcameraloginterval") config_.hplCameraLogInterval = ParseInt(value, config_.hplCameraLogInterval, 1, 100000);
            else if (key == "hplstereoafr") config_.hplStereoAfr = ParseBool(value, config_.hplStereoAfr);
            else if (key == "hplworldscale") config_.hplWorldScale = ParseFloat(value, config_.hplWorldScale, 0.1f, 10.0f);
            else if (key == "hplrenderstageprobe") config_.hplRenderStageProbe = ParseBool(value, config_.hplRenderStageProbe);
            else if (key == "hplaudiolistenerprobe") config_.hplAudioListenerProbe = ParseBool(value, config_.hplAudioListenerProbe);
            else if (key == "hplaudiolistenercorrection") config_.hplAudioListenerCorrection = ParseBool(value, config_.hplAudioListenerCorrection);
            else if (key == "hplaudiolistenertranslation") config_.hplAudioListenerTranslation = ParseBool(value, config_.hplAudioListenerTranslation);
            else if (key == "hplposteffectcontrol") config_.hplPostEffectControl = ParseBool(value, config_.hplPostEffectControl);
            else if (key == "hplposteffectbypassdefault") config_.hplPostEffectBypassDefault = ParseBool(value, config_.hplPostEffectBypassDefault);
            else if (key == "hplposteffectdisableimagetrail") config_.hplPostEffectDisableImageTrail = ParseBool(value, config_.hplPostEffectDisableImageTrail);
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
            }
            continue;
        }

        if (section == "controller") {
            if (key == "enabled") config_.hplControllerInput = ParseBool(value, config_.hplControllerInput);
            else if (key == "movedeadzone") config_.hplControllerMoveDeadzone = ParseFloat(value, config_.hplControllerMoveDeadzone, 0.05f, 0.95f);
            else if (key == "movereleasedeadzone") config_.hplControllerMoveReleaseDeadzone = ParseFloat(value, config_.hplControllerMoveReleaseDeadzone, 0.0f, 0.9f);
            else if (key == "snapturn") config_.hplControllerSnapTurn = ParseBool(value, config_.hplControllerSnapTurn);
            else if (key == "turndeadzone") config_.hplControllerTurnDeadzone = ParseFloat(value, config_.hplControllerTurnDeadzone, 0.05f, 0.95f);
            else if (key == "turnreleasedeadzone") config_.hplControllerTurnReleaseDeadzone = ParseFloat(value, config_.hplControllerTurnReleaseDeadzone, 0.0f, 0.9f);
            else if (key == "snapturnpixels") config_.hplControllerSnapTurnPixels = ParseInt(value, config_.hplControllerSnapTurnPixels, 1, 4000);
            else if (key == "smoothturnpixelspersecond") config_.hplControllerSmoothTurnPixelsPerSecond = ParseFloat(value, config_.hplControllerSmoothTurnPixelsPerSecond, 1.0f, 5000.0f);
            else if (key == "interaction") config_.hplControllerInteraction = ParseBool(value, config_.hplControllerInteraction);
            else if (key == "menu") config_.hplControllerMenu = ParseBool(value, config_.hplControllerMenu);
            else if (key == "recenterchord") config_.hplControllerRecenterChord = ParseBool(value, config_.hplControllerRecenterChord);
            else if (key == "haptics") config_.hplControllerHaptics = ParseBool(value, config_.hplControllerHaptics);
            else if (key == "hapticamplitude") config_.hplControllerHapticAmplitude = ParseFloat(value, config_.hplControllerHapticAmplitude, 0.0f, 1.0f);
            else if (key == "hapticdurationms") config_.hplControllerHapticDurationMs = ParseInt(value, config_.hplControllerHapticDurationMs, 1, 1000);
            else if (key == "dominanthand") {
                const std::string dominantHand = Lower(Trim(value));
                if (dominantHand == "left" || dominantHand == "right") config_.hplControllerDominantHand = dominantHand;
            }
            else if (key == "swapsticks") config_.hplControllerSwapSticks = ParseBool(value, config_.hplControllerSwapSticks);
            else if (key == "onehandfallback") config_.hplControllerOneHandFallback = ParseBool(value, config_.hplControllerOneHandFallback);
            else if (key == "suppressduringauthoredcamera") config_.hplControllerSuppressDuringAuthoredCamera = ParseBool(value, config_.hplControllerSuppressDuringAuthoredCamera);
            else if (key == "interactionray") config_.hplControllerInteractionRay = ParseBool(value, config_.hplControllerInteractionRay);
            else if (key == "interactionrayorigintolerance") config_.hplControllerInteractionRayOriginTolerance = ParseFloat(value, config_.hplControllerInteractionRayOriginTolerance, 0.05f, 10.0f);
            else if (key == "comfortblackoutframes") config_.hplControllerComfortBlackoutFrames = ParseInt(value, config_.hplControllerComfortBlackoutFrames, 0, 120);
            else if (key == "recenterholdms") config_.hplControllerRecenterHoldMs = ParseInt(value, config_.hplControllerRecenterHoldMs, 250, 5000);
            else if (key == "maxinputageframes") config_.hplControllerMaxInputAgeFrames = ParseInt(value, config_.hplControllerMaxInputAgeFrames, 1, 300);
            else if (key == "loginterval") config_.hplControllerLogInterval = ParseInt(value, config_.hplControllerLogInterval, 1, 100000);
        }
    }
}

} // namespace somavr
