#include "HPLSubtitleMath.h"

#include <algorithm>
#include <cmath>

namespace somavr::subtitle_math {

bool BuildSubtitleLayout(
    const SubtitleLayout& nativeLayout,
    const SubtitleLayoutSettings& settings,
    SubtitleLayout& output)
{
    if (!std::isfinite(nativeLayout.maxTextWidth)
        || !std::isfinite(nativeLayout.textY)
        || !std::isfinite(nativeLayout.fontSize)
        || !std::isfinite(nativeLayout.shadowOffset)
        || !std::isfinite(settings.widthScale)
        || !std::isfinite(settings.fontScale)
        || !std::isfinite(settings.verticalOffset)
        || nativeLayout.maxTextWidth < 64.0f
        || nativeLayout.maxTextWidth > 8192.0f
        || std::abs(nativeLayout.textY) > 8192.0f
        || nativeLayout.fontSize < 4.0f
        || nativeLayout.fontSize > 512.0f
        || nativeLayout.shadowOffset < 0.0f
        || nativeLayout.shadowOffset > 128.0f
        || settings.widthScale < 0.5f
        || settings.widthScale > 2.0f
        || settings.fontScale < 0.5f
        || settings.fontScale > 2.0f
        || std::abs(settings.verticalOffset) > 2048.0f) {
        return false;
    }

    output.maxTextWidth = std::clamp(
        nativeLayout.maxTextWidth * settings.widthScale, 64.0f, 8192.0f);
    output.textY = std::clamp(
        nativeLayout.textY + settings.verticalOffset, -8192.0f, 8192.0f);
    output.fontSize = std::clamp(
        nativeLayout.fontSize * settings.fontScale, 4.0f, 512.0f);
    output.shadowOffset = std::clamp(
        nativeLayout.shadowOffset * settings.fontScale, 0.0f, 128.0f);
    return true;
}

} // namespace somavr::subtitle_math
