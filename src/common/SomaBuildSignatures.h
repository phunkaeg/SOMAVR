#pragma once

#include <cstddef>
#include <cstdint>

namespace somavr::soma_signatures {

inline constexpr uintptr_t kGetClosestEntityRva = 0x0cd750;
inline constexpr uintptr_t kGetClosestEntityRaycastRva = 0x1438c0;
inline constexpr uint8_t kGetClosestEntity[] = {
    0x48, 0x89, 0x5c, 0x24, 0x08,
    0x57,
    0x48, 0x83, 0xec, 0x50,
    0x48, 0x8b, 0xbc, 0x24, 0x88, 0x00, 0x00, 0x00,
};
inline constexpr uint8_t kGetClosestEntityRaycast[] = {
    0x40, 0x57, 0x48, 0x83, 0xec, 0x60,
    0x48, 0x8b, 0x05, 0x13, 0xed, 0x64, 0x00,
    0x48, 0x8b, 0xf9, 0x4d, 0x8b, 0xd0,
};
inline constexpr size_t kRaycastGameContextDisplacementOffset = 9;
inline constexpr size_t kRaycastGameContextNextInstructionOffset = 13;

static_assert(
    kRaycastGameContextNextInstructionOffset <= sizeof(kGetClosestEntityRaycast));
static_assert(
    kRaycastGameContextNextInstructionOffset
        == kRaycastGameContextDisplacementOffset + sizeof(int32_t));
static_assert(
    kGetClosestEntityRaycast[kRaycastGameContextDisplacementOffset - 3] == 0x48
    && kGetClosestEntityRaycast[kRaycastGameContextDisplacementOffset - 2] == 0x8b
    && kGetClosestEntityRaycast[kRaycastGameContextDisplacementOffset - 1] == 0x05);

} // namespace somavr::soma_signatures
