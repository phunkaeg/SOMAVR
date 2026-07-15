#pragma once

#include <string>
#include <string_view>

namespace somavr {

struct Config;

enum class ComfortPreset {
    Custom,
    Minimal,
    Balanced,
    Maximum,
    Invalid,
};

ComfortPreset ParseComfortPreset(std::string_view value);
const char* ComfortPresetName(ComfortPreset preset);
bool ApplyComfortPreset(Config& config, ComfortPreset preset);

} // namespace somavr
