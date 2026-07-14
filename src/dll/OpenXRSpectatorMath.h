#pragma once

#include <cstdint>

namespace somavr::spectator_math {

enum class AspectMode : uint8_t {
    Fit,
    Fill,
    Stretch,
};

struct BlitLayout {
    int sourceX0 = 0;
    int sourceY0 = 0;
    int sourceX1 = 0;
    int sourceY1 = 0;
    int destinationX0 = 0;
    int destinationY0 = 0;
    int destinationX1 = 0;
    int destinationY1 = 0;
    bool clearDestination = false;
};

bool ComputeBlitLayout(
    int sourceWidth,
    int sourceHeight,
    int destinationWidth,
    int destinationHeight,
    AspectMode mode,
    BlitLayout& layout);

} // namespace somavr::spectator_math
