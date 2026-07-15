#include "HPLContactHapticsMath.h"

#include <algorithm>
#include <cmath>

namespace somavr::contact_haptics_math {

Decision Evaluate(
    State& state,
    bool eligible,
    float speed,
    float distance,
    uint64_t nowMs,
    const Settings& settings)
{
    Decision decision;
    if (!eligible) {
        decision.reject = RejectReason::Ineligible;
        return decision;
    }
    if (!std::isfinite(speed) || !std::isfinite(distance)) {
        decision.reject = RejectReason::NonFinite;
        return decision;
    }

    const float minSpeed = std::max(0.0f, settings.minSpeed);
    const float maxSpeed = std::max(minSpeed + 0.001f, settings.maxSpeed);
    if (std::abs(speed) < minSpeed) {
        decision.reject = RejectReason::BelowSpeed;
        return decision;
    }
    if (distance < 0.0f || distance > std::max(0.0f, settings.maxDistance)) {
        decision.reject = RejectReason::TooFar;
        return decision;
    }
    if (state.hasPulsed && nowMs >= state.lastPulseMs
        && nowMs - state.lastPulseMs < settings.cooldownMs) {
        decision.reject = RejectReason::Cooldown;
        return decision;
    }

    const float normalized = std::clamp((std::abs(speed) - minSpeed) / (maxSpeed - minSpeed), 0.0f, 1.0f);
    const float low = std::clamp(settings.minAmplitude, 0.0f, 1.0f);
    const float high = std::clamp(std::max(low, settings.maxAmplitude), 0.0f, 1.0f);
    decision.pulse = high > 0.0f && settings.durationMs > 0;
    decision.amplitude = low + (high - low) * normalized;
    decision.durationMs = std::clamp(settings.durationMs, 1, 1000);
    if (decision.pulse) {
        state.hasPulsed = true;
        state.lastPulseMs = nowMs;
    }
    return decision;
}

} // namespace somavr::contact_haptics_math
