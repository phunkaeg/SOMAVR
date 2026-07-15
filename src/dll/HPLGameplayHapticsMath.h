#pragma once

#include <cstdint>

namespace somavr::gameplay_haptics_math {

enum class Action {
    None,
    Pulse,
    Stop,
};

struct Settings {
    float amplitudeScale = 0.75f;
    float minAmplitude = 0.05f;
    float retriggerDelta = 0.08f;
    uint64_t refreshMs = 80;
    int segmentMs = 100;
};

struct State {
    bool active = false;
    float amplitude = 0.0f;
    uint64_t lastPulseMs = 0;
};

struct Decision {
    Action action = Action::None;
    float amplitude = 0.0f;
    int durationMs = 0;
};

Decision Update(
    State& state,
    float nativeStrength,
    float nativeDurationSeconds,
    uint64_t nowMs,
    const Settings& settings);

} // namespace somavr::gameplay_haptics_math
