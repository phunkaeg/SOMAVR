#pragma once

#include <array>
#include <cstdint>

namespace somavr::depth_math {

struct CompositionDepthRange {
    float minDepth = 0.0f;
    float maxDepth = 1.0f;
    float nearMeters = 0.0f;
    float farMeters = 0.0f;
};

struct SceneDepthSource {
    uint32_t framebuffer = 0;
    uint32_t objectType = 0;
    uint32_t object = 0;
    int32_t format = 0;
    int32_t width = 0;
    int32_t height = 0;
    int32_t samples = 0;
    std::array<int32_t, 4> viewport{};
    std::array<float, 2> depthRange{};
    bool complete = false;
};

struct SceneDepthStamp {
    uint64_t renderSerial = 0;
    uint64_t poseFrame = 0;
    SceneDepthSource source{};
    CompositionDepthRange range{};
    bool captured = false;
};

const char* ValidateSceneDepthSource(const SceneDepthSource& source, int64_t cacheFormat);
bool SceneDepthMatchesColor(const SceneDepthStamp& stamp, uint64_t renderSerial,
    uint64_t poseFrame, const std::array<int32_t, 4>& colorViewport);

bool BuildStandardDepthRange(
    float nearWorld,
    float farWorld,
    float worldUnitsPerMeter,
    CompositionDepthRange& range);

} // namespace somavr::depth_math
