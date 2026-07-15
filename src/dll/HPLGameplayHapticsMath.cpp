#include "HPLGameplayHapticsMath.h"

#include <algorithm>
#include <cmath>

namespace somavr::gameplay_haptics_math {

Decision Update(
    State& state,
    float nativeStrength,
    float nativeDurationSeconds,
    uint64_t nowMs,
    const Settings& settings)
{
    Decision decision;
    const bool valid = std::isfinite(nativeStrength) && std::isfinite(nativeDurationSeconds);
    const float amplitude = valid
        ? std::clamp(nativeStrength * settings.amplitudeScale, 0.0f, 1.0f)
        : 0.0f;
    const bool requested = amplitude >= settings.minAmplitude && nativeDurationSeconds > 0.0f;

    if (!requested) {
        if (state.active) decision.action = Action::Stop;
        state = {};
        return decision;
    }

    const bool rising = !state.active;
    const bool stronger = amplitude >= state.amplitude + settings.retriggerDelta;
    const bool refresh = state.active && nowMs - state.lastPulseMs >= settings.refreshMs;
    state.active = true;
    state.amplitude = amplitude;
    if (!rising && !stronger && !refresh) return decision;

    state.lastPulseMs = nowMs;
    decision.action = Action::Pulse;
    decision.amplitude = amplitude;
    const float boundedDurationSeconds = std::min(
        nativeDurationSeconds, static_cast<float>(std::max(settings.segmentMs, 1)) / 1000.0f);
    const int remainingMs = std::max(
        1, static_cast<int>(std::lround(boundedDurationSeconds * 1000.0f)));
    decision.durationMs = std::min(std::max(settings.segmentMs, 1), remainingMs);
    return decision;
}

} // namespace somavr::gameplay_haptics_math
