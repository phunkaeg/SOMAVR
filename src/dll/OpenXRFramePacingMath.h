#pragma once

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

} // namespace somavr::openxr_frame_pacing_math
