#pragma once

namespace somavr::depth_math {

struct CompositionDepthRange {
    float minDepth = 0.0f;
    float maxDepth = 1.0f;
    float nearMeters = 0.0f;
    float farMeters = 0.0f;
};

bool BuildStandardDepthRange(
    float nearWorld,
    float farWorld,
    float worldUnitsPerMeter,
    CompositionDepthRange& range);

} // namespace somavr::depth_math
