#include "OpenXRSpectatorMath.h"

#include <algorithm>
#include <cmath>

namespace somavr::spectator_math {

bool ComputeBlitLayout(
    int sourceWidth,
    int sourceHeight,
    int destinationWidth,
    int destinationHeight,
    AspectMode mode,
    BlitLayout& layout)
{
    layout = {};
    if (sourceWidth <= 0 || sourceHeight <= 0
        || destinationWidth <= 0 || destinationHeight <= 0) {
        return false;
    }

    layout.sourceX1 = sourceWidth;
    layout.sourceY1 = sourceHeight;
    layout.destinationX1 = destinationWidth;
    layout.destinationY1 = destinationHeight;
    if (mode == AspectMode::Stretch) {
        return true;
    }

    const double sourceAspect = static_cast<double>(sourceWidth)
        / static_cast<double>(sourceHeight);
    const double destinationAspect = static_cast<double>(destinationWidth)
        / static_cast<double>(destinationHeight);
    if (mode == AspectMode::Fit) {
        layout.clearDestination = true;
        if (sourceAspect > destinationAspect) {
            const int height = std::clamp(
                static_cast<int>(std::lround(destinationWidth / sourceAspect)),
                1,
                destinationHeight);
            layout.destinationY0 = (destinationHeight - height) / 2;
            layout.destinationY1 = layout.destinationY0 + height;
        } else {
            const int width = std::clamp(
                static_cast<int>(std::lround(destinationHeight * sourceAspect)),
                1,
                destinationWidth);
            layout.destinationX0 = (destinationWidth - width) / 2;
            layout.destinationX1 = layout.destinationX0 + width;
        }
        return true;
    }

    if (sourceAspect > destinationAspect) {
        const int width = std::clamp(
            static_cast<int>(std::lround(sourceHeight * destinationAspect)),
            1,
            sourceWidth);
        layout.sourceX0 = (sourceWidth - width) / 2;
        layout.sourceX1 = layout.sourceX0 + width;
    } else {
        const int height = std::clamp(
            static_cast<int>(std::lround(sourceWidth / destinationAspect)),
            1,
            sourceHeight);
        layout.sourceY0 = (sourceHeight - height) / 2;
        layout.sourceY1 = layout.sourceY0 + height;
    }
    return true;
}

} // namespace somavr::spectator_math
