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
            && !manager.Get().hplComfortSuppressSway,
        "reloading a manager resets stale preset state before parsing");
    std::error_code ec;
    std::filesystem::remove(configPath, ec);
    return failures == 0 ? 0 : 1;
}
