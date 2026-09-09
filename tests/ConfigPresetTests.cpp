#include "Config.h"
#include "ConfigPreset.h"

#include <iostream>
#include <filesystem>
#include <fstream>

namespace {

int Check(bool condition, const char* message)
{
    if (condition) return 0;
    std::cerr << "FAILED: " << message << '\n';
    return 1;
}

} // namespace

int main()
{
    using namespace somavr;
    int failures = 0;
    failures += Check(ParseComfortPreset("  BALANCED ") == ComfortPreset::Balanced,
        "preset parsing is trimmed and case insensitive");
    failures += Check(ParseComfortPreset("unknown") == ComfortPreset::Invalid,
        "unknown preset is rejected");

    Config minimal;
    failures += Check(ApplyComfortPreset(minimal, ComfortPreset::Minimal)
            && !minimal.hplControllerSnapTurn
            && minimal.hplControllerComfortBlackoutFrames == 0
            && !minimal.hplComfortCameraAddControl
            && minimal.hplPostEffectControl
            && minimal.hplPostEffectDisableImageTrail
            && !minimal.hplPerEyeImageTrailControl
            && !minimal.hplPostEffectDisableVideoDistortion,
        "minimal preset keeps only the worst temporal effect suppressed");

    Config balanced;
    failures += Check(ApplyComfortPreset(balanced, ComfortPreset::Balanced)
            && balanced.hplControllerSnapTurn
            && balanced.hplControllerComfortBlackoutFrames == 2
            && balanced.hplComfortSuppressHeadBob
            && balanced.hplComfortCameraRollControl
            && !balanced.hplComfortSuppressScriptRoll
            && balanced.hplComfortOpticsControl
            && balanced.openxrComfortVignette
            && balanced.openxrComfortVignetteStrength == 0.60f
            && balanced.hplPostEffectDisableRadialBlur,
        "balanced preset enables established comfort policy without script roll");

    Config maximum;
    failures += Check(ApplyComfortPreset(maximum, ComfortPreset::Maximum)
            && maximum.hplControllerComfortBlackoutFrames == 4
            && maximum.hplComfortSuppressSway
            && maximum.hplComfortSuppressScriptRoll
            && maximum.openxrComfortVignette
            && maximum.openxrComfortVignetteInnerRadius == 0.42f
            && maximum.openxrTrackingRecoveryBlackoutFrames == 4,
        "maximum preset enables the strongest bounded comfort policy");

    maximum.hplControllerComfortBlackoutFrames = 9;
    failures += Check(maximum.hplControllerComfortBlackoutFrames == 9,
        "explicit settings can override a previously applied preset");

    const std::filesystem::path configPath =
        std::filesystem::temp_directory_path() / "somavr-config-preset-test.ini";
    {
        std::ofstream out(configPath, std::ios::trunc);
        out << "[Comfort]\nPreset=maximum\n"
            << "[Hooks]\nHPLComfortSuppressScriptRoll=0\nHPLPerEyeImageTrailControl=1\nHPLToneMappingFrameControl=1\nHPLPerEyeSSAOTemporalControl=1\nHPLSSAOFrameOwnerControl=1\n"
            << "[Controller]\nComfortBlackoutFrames=7\n"
            << "MovementReference=controller\nInteractionBothHands=0\nAimGuide=1\nAimGuideLengthMeters=9\nAimGuideSceneDepth=1\n"
            << "PhysicalBodyFollow=1\nPhysicalBodyFollowThresholdDegrees=999\nPhysicalBodyFollowReleaseDegrees=999\nPhysicalBodyFollowDegreesPerSecond=999\nPhysicalBodyFollowDelayMs=9999\n"
            << "TerminalPointer=0\nTerminalDiegetic=0\nTerminalPreserveDirtyRects=0\nTerminalRayPointer=0\nTerminalRayLengthMeters=99\n"
            << "TerminalLookAwayExit=0\nTerminalLookAwayDegrees=999\nTerminalLookAwayFrames=999\n"
            << "ManipulationSlidePixelsPerMeter=99999\n"
            << "HandScaleNormalization=1\nHandWristPosition=1\nHandWristRotation=1\nHandWristRollDegrees=-999\nHandArmIK=1\nHandAlwaysVisible=1\nHandFreezePose=1\nHandTargetScale=9\n"
            << "HandShoulderVerticalOffsetMeters=-9\nHandShoulderBackOffsetMeters=9\nHandArmIKElbowDownMeters=9\n"
            << "HandArmIKErgonomics=1\nHandShoulderReachCompensation=1\nHandShoulderReachStart=0\nHandShoulderReachMaxMeters=9\nHandArmIKMaxSwivelDegreesPerFrame=999\n"
            << "HandArmIKBlend=9\nHandArmIKMaxReach=0\nAuthoredInteractions=1\nMedicineInteraction=1\nHandSocketedPropStabilization=1\n"
            << "MedicineCapProximityMeters=9\nMedicineMouthProximityMeters=0\nMedicineDrinkTipDegrees=999\nMedicineDrinkHoldFrames=999\n"
            << "ContactHaptics=1\nContactHapticMinSpeed=-1\nContactHapticMaxSpeed=99\n"
            << "ContactHapticMaxDistanceMeters=9\nContactHapticMinAmplitude=-1\n"
            << "ContactHapticMaxAmplitude=2\nContactHapticDurationMs=999\nContactHapticCooldownMs=-1\n"
            << "[OpenXR]\nHudShape=CYLINDER\nHudCylinderAngleDegrees=80\n"
            << "Foveation=1\nFoveationLevel=9\nFoveationDynamic=1\nFoveationVerticalOffset=-1.5\n"
            << "ComfortVignette=0\nComfortVignetteStrength=0.33\n";
    }
    ConfigManager manager;
    failures += Check(manager.InitializeAtPath(configPath)
            && manager.Get().comfortPreset == "maximum"
            && manager.Get().hplComfortSuppressSway
            && !manager.Get().hplComfortSuppressScriptRoll
            && manager.Get().hplPerEyeImageTrailControl
            && manager.Get().hplToneMappingFrameControl
            && manager.Get().hplPerEyeSSAOTemporalControl
            && manager.Get().hplSSAOFrameOwnerControl
            && manager.Get().hplControllerComfortBlackoutFrames == 7
            && manager.Get().hplControllerMovementReference == "controller"
            && !manager.Get().hplControllerInteractionBothHands
            && manager.Get().hplControllerAimGuide
            && manager.Get().hplControllerAimGuideLengthMeters == 4.0f
            && manager.Get().hplControllerAimGuideSceneDepth
            && manager.Get().hplControllerPhysicalBodyFollow
            && manager.Get().hplControllerPhysicalBodyFollowThresholdDegrees == 120.0f
            && manager.Get().hplControllerPhysicalBodyFollowReleaseDegrees == 60.0f
            && manager.Get().hplControllerPhysicalBodyFollowDegreesPerSecond == 120.0f
            && manager.Get().hplControllerPhysicalBodyFollowDelayMs == 2000
            && !manager.Get().hplControllerTerminalPointer
            && !manager.Get().hplControllerTerminalDiegetic
            && !manager.Get().hplControllerTerminalPreserveDirtyRects
            && !manager.Get().hplControllerTerminalRayPointer
            && manager.Get().hplControllerTerminalRayLengthMeters == 20.0f
            && !manager.Get().hplControllerTerminalLookAwayExit
            && manager.Get().hplControllerTerminalLookAwayDegrees == 150.0f
            && manager.Get().hplControllerTerminalLookAwayFrames == 120
            && manager.Get().hplControllerManipulationSlidePixelsPerMeter == 20000.0f
            && manager.Get().hplHandScaleNormalization
            && manager.Get().hplHandWristPosition
            && manager.Get().hplHandWristRotation
            && manager.Get().hplHandWristRollDegrees == -180.0f
            && manager.Get().hplHandArmIK
            && manager.Get().hplHandAlwaysVisible
            && manager.Get().hplHandFreezePose
            && manager.Get().hplHandTargetScale == 2.0f
            && manager.Get().hplHandShoulderVerticalOffsetMeters == -1.0f
            && manager.Get().hplHandShoulderBackOffsetMeters == 0.5f
            && manager.Get().hplHandArmIKElbowDownMeters == 0.5f
            && manager.Get().hplHandArmIKErgonomics
            && manager.Get().hplHandShoulderReachCompensation
            && manager.Get().hplHandShoulderReachStart == 0.5f
            && manager.Get().hplHandShoulderReachMaxMeters == 0.15f
            && manager.Get().hplHandArmIKMaxSwivelDegreesPerFrame == 90.0f
            && manager.Get().hplHandArmIKBlend == 1.0f
            && manager.Get().hplHandArmIKMaxReach == 0.5f
            && manager.Get().hplAuthoredInteractions
            && manager.Get().hplMedicineInteraction
            && manager.Get().hplHandSocketedPropStabilization
            && manager.Get().hplMedicineCapProximityMeters == 1.0f
            && manager.Get().hplMedicineMouthProximityMeters == 0.01f
            && manager.Get().hplMedicineDrinkTipDegrees == 180.0f
            && manager.Get().hplMedicineDrinkHoldFrames == 300
            && manager.Get().hplControllerContactHaptics
            && manager.Get().hplControllerContactHapticMinSpeed == 0.0f
            && manager.Get().hplControllerContactHapticMaxSpeed == 50.0f
            && manager.Get().hplControllerContactHapticMaxDistanceMeters == 3.0f
            && manager.Get().hplControllerContactHapticMinAmplitude == 0.0f
            && manager.Get().hplControllerContactHapticMaxAmplitude == 1.0f
            && manager.Get().hplControllerContactHapticDurationMs == 250
            && manager.Get().hplControllerContactHapticCooldownMs == 0
            && manager.Get().openxrHudShape == "cylinder"
            && manager.Get().openxrHudCylinderAngleDegrees == 80.0f
            && manager.Get().openxrFoveation
            && manager.Get().openxrFoveationLevel == 3
            && manager.Get().openxrFoveationDynamic
            && manager.Get().openxrFoveationVerticalOffset == -1.0f
            && !manager.Get().openxrComfortVignette
            && manager.Get().openxrComfortVignetteStrength == 0.33f,
        "explicit INI keys override preset values after the pre-scan");
    {
        std::ofstream out(configPath, std::ios::trunc);
        out << "[Comfort]\nPreset=custom\n";
    }
    failures += Check(manager.InitializeAtPath(configPath)
            && manager.Get().comfortPreset == "custom"
            && !manager.Get().hplControllerContactHaptics
            && manager.Get().hplControllerMovementReference == "body"
            && !manager.Get().hplControllerAimGuide
            && !manager.Get().hplControllerAimGuideSceneDepth
            && !manager.Get().hplControllerPhysicalBodyFollow
            && manager.Get().hplControllerTerminalPointer
            && manager.Get().hplControllerTerminalDiegetic
            && manager.Get().hplControllerTerminalPreserveDirtyRects
            && manager.Get().hplControllerTerminalRayPointer
            && manager.Get().hplControllerTerminalRayLengthMeters == 8.0f
            && manager.Get().hplControllerTerminalLookAwayExit
            && manager.Get().hplControllerTerminalLookAwayDegrees == 65.0f
            && manager.Get().hplControllerTerminalLookAwayFrames == 8
            && manager.Get().hplControllerManipulationSlidePixelsPerMeter == 2700.0f
            && !manager.Get().hplHandScaleNormalization
            && !manager.Get().hplHandWristPosition
            && !manager.Get().hplHandWristRotation
            && manager.Get().hplHandWristRollDegrees == 0.0f
            && !manager.Get().hplHandSocketedPropStabilization
            && !manager.Get().hplHandFreezePose
            && manager.Get().hplHandTargetScale == 1.0f
            && manager.Get().hplHandShoulderVerticalOffsetMeters == -0.30f
            && manager.Get().hplHandShoulderBackOffsetMeters == 0.10f
            && manager.Get().hplHandArmIKElbowDownMeters == 0.10f
            && !manager.Get().hplHandArmIKErgonomics
            && !manager.Get().hplHandShoulderReachCompensation
            && manager.Get().hplHandShoulderReachStart == 0.85f
            && manager.Get().hplHandShoulderReachMaxMeters == 0.05f
            && manager.Get().hplHandArmIKMaxSwivelDegreesPerFrame == 10.0f
            && !manager.Get().hplComfortSuppressSway,
        "reloading a manager resets stale preset state before parsing");
#if defined(SOMAVR_RELEASE_CONFIG_PATH)
    ConfigManager releaseManager;
    failures += Check(
        releaseManager.InitializeAtPath(SOMAVR_RELEASE_CONFIG_PATH)
            && releaseManager.AcceptedKeyCount() >= 250
            && releaseManager.UnknownKeyCount() == 0
            && releaseManager.UnknownSectionCount() == 0,
        "tracked release config is fully recognized by the runtime parser");
    failures += Check(releaseManager.Get().hplHandWristPitchDegrees == -45.0f
            && releaseManager.Get().hplHandWristRollDegrees == -90.0f
            && releaseManager.Get().hplControllerReadObjectDistanceScale == 1.2f
            && releaseManager.Get().hplControllerReadObjectScale == 2.0f,
        "release profile selects neutral wrist pitch and closer double-size story inspection");
#endif
    {
        std::ofstream out(configPath, std::ios::trunc);
        out << "[UnknownSection]\nMystery=1\n[Hooks]\nDefinitelyNotASetting=1\n";
    }
    ConfigManager unknownManager;
    failures += Check(
        unknownManager.InitializeAtPath(configPath)
            && unknownManager.UnknownSectionCount() == 1
            && unknownManager.UnknownKeyCount() == 2,
        "unknown config sections and keys are counted instead of silently ignored");
    std::error_code ec;
    std::filesystem::remove(configPath, ec);
    return failures == 0 ? 0 : 1;
}
