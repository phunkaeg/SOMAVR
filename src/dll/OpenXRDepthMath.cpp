#include "OpenXRDepthMath.h"

#include <cmath>

namespace somavr::depth_math {

const char* ValidateSceneDepthSource(const SceneDepthSource& source, int64_t cacheFormat)
{
    if (source.framebuffer == 0) return "default_framebuffer";
    if (!source.complete) return "incomplete_framebuffer";
    // The proven HPL source is a single-sample renderbuffer, not its R16F color proxy.
    if (source.objectType != 0x8D41 || source.object == 0) return "unproven_attachment_type";
    if (source.format != cacheFormat || (cacheFormat != 0x88F0 && cacheFormat != 0x81A6
        && cacheFormat != 0x8CAC && cacheFormat != 0x8CAD)) return "depth_format_mismatch";
    if (source.samples != 0) return "multisample_source";
    if (source.width <= 0 || source.height <= 0 || source.viewport[0] != 0
        || source.viewport[1] != 0 || source.viewport[2] != source.width
        || source.viewport[3] != source.height) return "viewport_attachment_mismatch";
    if (source.depthRange[0] != 0.0f || source.depthRange[1] != 1.0f) return "nonstandard_depth_range";
    return "accepted";
}

bool SceneDepthMatchesColor(const SceneDepthStamp& stamp, uint64_t renderSerial,
    uint64_t poseFrame, const std::array<int32_t, 4>& colorViewport)
{
    return stamp.captured && renderSerial != 0 && poseFrame != 0
        && stamp.renderSerial == renderSerial && stamp.poseFrame == poseFrame
        && stamp.source.viewport == colorViewport;
}

bool BuildStandardDepthRange(
    float nearWorld,
    float farWorld,
    float worldUnitsPerMeter,
    CompositionDepthRange& range)
{
    range = {};
    if (!std::isfinite(nearWorld)
        || !std::isfinite(farWorld)
        || !std::isfinite(worldUnitsPerMeter)
        || nearWorld <= 0.0f
        || farWorld <= nearWorld
        || worldUnitsPerMeter <= 0.0f) {
        return false;
    }

    range.minDepth = 0.0f;
    range.maxDepth = 1.0f;
    range.nearMeters = nearWorld / worldUnitsPerMeter;
    range.farMeters = farWorld / worldUnitsPerMeter;
    return std::isfinite(range.nearMeters)
        && std::isfinite(range.farMeters)
        && range.nearMeters > 0.0f
        && range.farMeters > range.nearMeters;
}

} // namespace somavr::depth_math
