#pragma once

#include <cstdint>

namespace somavr::contact_haptics_math {

enum class RejectReason {
    None,
    Ineligible,
    NonFinite,
    BelowSpeed,
    TooFar,
    Cooldown,
};

struct Settings {
    float minSpeed = 0.5f;
    float maxSpeed = 5.0f;
    float maxDistance = 0.75f;
    float minAmplitude = 0.08f;
    float maxAmplitude = 0.55f;
    int durationMs = 35;
    uint64_t cooldownMs = 45;
};

struct State {
    bool hasPulsed = false;
    uint64_t lastPulseMs = 0;
};

struct Decision {
    bool pulse = false;
    float amplitude = 0.0f;
    int durationMs = 0;
    RejectReason reject = RejectReason::None;
};

Decision Evaluate(
    State& state,
    bool eligible,
    float speed,
    float distance,
    uint64_t nowMs,
    const Settings& settings);

} // namespace somavr::contact_haptics_math
