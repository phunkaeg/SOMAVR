#pragma once

#include <cstdint>
#include <limits>

namespace somavr::openxr_frame_pacing_math {

enum class Decision {
    Wait,
    SkipUntilFocused,
};

constexpr Decision Decide(bool sessionRunning, bool sessionFocused, bool everFocused)
{
    if (!sessionRunning || sessionFocused || !everFocused) {
        return Decision::Wait;
    }
    return Decision::SkipUntilFocused;
}

constexpr int64_t UpcomingRenderDisplayTime(int64_t currentDisplayTime, int64_t displayPeriod)
{
    if (displayPeriod <= 0) {
        return currentDisplayTime;
    }
    if (currentDisplayTime > (std::numeric_limits<int64_t>::max)() - displayPeriod) {
        return (std::numeric_limits<int64_t>::max)();
    }
    return currentDisplayTime + displayPeriod;
}

constexpr bool RequiresFallbackProjection(bool frameBegun, uint32_t projectionLayerCount)
{
    return frameBegun && projectionLayerCount == 0;
}

} // namespace somavr::openxr_frame_pacing_math
