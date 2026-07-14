#pragma once

namespace somavr::subtitle_math {

struct SubtitleLayout {
    float maxTextWidth = 0.0f;
    float textY = 0.0f;
    float fontSize = 0.0f;
    float shadowOffset = 0.0f;
};

struct SubtitleLayoutSettings {
    float widthScale = 1.0f;
    float fontScale = 1.0f;
    float verticalOffset = 0.0f;
};

bool BuildSubtitleLayout(
    const SubtitleLayout& nativeLayout,
    const SubtitleLayoutSettings& settings,
    SubtitleLayout& output);

} // namespace somavr::subtitle_math
