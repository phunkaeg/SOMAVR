#include "HPLPostEffectResourceMath.h"

#include <algorithm>
#include <vector>

namespace somavr::post_effect_resource_math {
namespace {

template <typename T>
void HashValue(uint64_t& hash, T value)
{
    constexpr uint64_t kPrime = 1099511628211ull;
    for (size_t i = 0; i < sizeof(T); ++i) {
        hash ^= static_cast<uint8_t>(value >> (i * 8));
        hash *= kPrime;
    }
}

} // namespace

uint64_t HashResourceFootprint(
    const TextureResource* textures,
    size_t textureCount,
    const FramebufferResource* framebuffers,
    size_t framebufferCount)
{
    std::vector<TextureResource> orderedTextures;
    std::vector<FramebufferResource> orderedFramebuffers;
    if (textures != nullptr && textureCount != 0) {
        orderedTextures.assign(textures, textures + textureCount);
    }
    if (framebuffers != nullptr && framebufferCount != 0) {
        orderedFramebuffers.assign(framebuffers, framebuffers + framebufferCount);
    }
    std::sort(orderedTextures.begin(), orderedTextures.end(), [](const auto& left, const auto& right) {
        if (left.target != right.target) return left.target < right.target;
        if (left.texture != right.texture) return left.texture < right.texture;
        if (left.width != right.width) return left.width < right.width;
        if (left.height != right.height) return left.height < right.height;
        if (left.depth != right.depth) return left.depth < right.depth;
        return left.internalFormat < right.internalFormat;
    });
    std::sort(orderedFramebuffers.begin(), orderedFramebuffers.end(), [](const auto& left, const auto& right) {
        if (left.target != right.target) return left.target < right.target;
        return left.framebuffer < right.framebuffer;
    });

    uint64_t hash = 1469598103934665603ull;
    HashValue(hash, static_cast<uint64_t>(orderedTextures.size()));
    for (const TextureResource& resource : orderedTextures) {
        HashValue(hash, resource.target);
        HashValue(hash, resource.texture);
        HashValue(hash, static_cast<uint32_t>(resource.width));
        HashValue(hash, static_cast<uint32_t>(resource.height));
        HashValue(hash, static_cast<uint32_t>(resource.depth));
        HashValue(hash, static_cast<uint32_t>(resource.internalFormat));
    }
    HashValue(hash, static_cast<uint64_t>(orderedFramebuffers.size()));
    for (const FramebufferResource& resource : orderedFramebuffers) {
        HashValue(hash, resource.target);
        HashValue(hash, resource.framebuffer);
    }
    return hash;
}

EyeResourceOwnership ClassifyEyeResourceOwnership(
    bool leftSeen,
    uint64_t leftSignature,
    bool rightSeen,
    uint64_t rightSignature)
{
    if (!leftSeen || !rightSeen) return EyeResourceOwnership::Unknown;
    return leftSignature == rightSignature
        ? EyeResourceOwnership::Shared
        : EyeResourceOwnership::EyeDistinct;
}

const char* EyeResourceOwnershipName(EyeResourceOwnership ownership)
{
    switch (ownership) {
    case EyeResourceOwnership::Shared: return "shared_across_eyes";
    case EyeResourceOwnership::EyeDistinct: return "eye_distinct";
    default: return "unknown_pending_both_eyes";
    }
}

} // namespace somavr::post_effect_resource_math
