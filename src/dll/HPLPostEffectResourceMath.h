#pragma once

#include <cstddef>
#include <cstdint>

namespace somavr::post_effect_resource_math {

struct TextureResource {
    uint32_t target = 0;
    uint32_t texture = 0;
    int32_t width = 0;
    int32_t height = 0;
    int32_t depth = 0;
    int32_t internalFormat = 0;
};

struct FramebufferResource {
    uint32_t target = 0;
    uint32_t framebuffer = 0;
};

enum class EyeResourceOwnership : uint8_t {
    Unknown,
    Shared,
    EyeDistinct,
};

uint64_t HashResourceFootprint(
    const TextureResource* textures,
    size_t textureCount,
    const FramebufferResource* framebuffers,
    size_t framebufferCount);

EyeResourceOwnership ClassifyEyeResourceOwnership(
    bool leftSeen,
    uint64_t leftSignature,
    bool rightSeen,
    uint64_t rightSignature);

const char* EyeResourceOwnershipName(EyeResourceOwnership ownership);

} // namespace somavr::post_effect_resource_math
