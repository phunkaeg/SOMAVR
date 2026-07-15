#include "ConfigPreset.h"

#include "Config.h"

#include <algorithm>
#include <cctype>

namespace somavr {
namespace {

std::string Lower(std::string_view value)
{
    size_t begin = 0;
    size_t end = value.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(value[begin])) != 0) ++begin;
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) --end;
    std::string result(value.substr(begin, end - begin));
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return result;
}

void ApplyNamedPostPolicy(Config& config, bool allNamedEffects)
{
    config.hplPostEffectControl = true;
    config.hplPostEffectDisableImageTrail = true;
    config.hplPostEffectDisableVideoDistortion = allNamedEffects;
    config.hplPostEffectDisableChromaticAberration = allNamedEffects;
    config.hplPostEffectDisableRadialBlur = allNamedEffects;
}

} // namespace

ComfortPreset ParseComfortPreset(std::string_view value)
{
    const std::string normalized = Lower(value);
    if (normalized == "custom") return ComfortPreset::Custom;
    if (normalized == "minimal") return ComfortPreset::Minimal;
    if (normalized == "balanced") return ComfortPreset::Balanced;
    if (normalized == "maximum") return ComfortPreset::Maximum;
    return ComfortPreset::Invalid;
}

const char* ComfortPresetName(ComfortPreset preset)
{
    switch (preset) {
    case ComfortPreset::Minimal: return "minimal";
    case ComfortPreset::Balanced: return "balanced";
    case ComfortPreset::Maximum: return "maximum";
    case ComfortPreset::Custom:
    case ComfortPreset::Invalid:
    default: return "custom";
    }
}

bool ApplyComfortPreset(Config& config, ComfortPreset preset)
{
    config.comfortPreset = ComfortPresetName(preset);
    if (preset == ComfortPreset::Custom) return true;
    if (preset == ComfortPreset::Invalid) return false;

    const bool balanced = preset == ComfortPreset::Balanced;
    const bool maximum = preset == ComfortPreset::Maximum;
    config.hplControllerSnapTurn = balanced || maximum;
    config.hplControllerSnapTurnDegrees = 30.0f;
    config.hplControllerComfortBlackoutFrames = maximum ? 4 : balanced ? 2 : 0;
    config.hplControllerStateTransitionBlackoutFrames = maximum ? 4 : balanced ? 2 : 0;
    config.openxrTrackingRecoveryBlackoutFrames = maximum ? 4 : balanced ? 2 : 1;
    config.hplLoadingScreenExitBlackoutFrames = maximum ? 4 : balanced ? 2 : 0;

    config.hplComfortCameraAddControl = balanced || maximum;
    config.hplComfortSuppressHeadBob = balanced || maximum;
    config.hplComfortSuppressCameraShake = balanced || maximum;
    config.hplComfortSuppressSway = maximum;
    config.hplComfortCameraRollControl = balanced || maximum;
    config.hplComfortSuppressScriptRoll = maximum;
    config.hplComfortSuppressLeanRoll = balanced || maximum;
    config.hplComfortSuppressMoveRoll = balanced || maximum;
    config.hplComfortSuppressClimbRoll = balanced || maximum;
    config.hplComfortDepthOfFieldControl = balanced || maximum;
    config.hplComfortOpticsControl = balanced || maximum;
    config.hplComfortSuppressFov = balanced || maximum;
    config.hplComfortSuppressFovMultiplier = balanced || maximum;
    config.hplComfortSuppressAspectMultiplier = balanced || maximum;
    config.hplScreenEffectControl = balanced || maximum;
    config.openxrComfortVignette = balanced || maximum;
    config.openxrComfortVignetteStrength = maximum ? 0.75f : balanced ? 0.60f : 0.0f;
    config.openxrComfortVignetteInnerRadius = maximum ? 0.42f : 0.50f;
    config.openxrComfortVignetteFadeMilliseconds = maximum ? 180 : 250;
    ApplyNamedPostPolicy(config, balanced || maximum);
    return true;
}

} // namespace somavr
