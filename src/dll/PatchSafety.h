#pragma once

#include <cstddef>
#include <cstdint>

namespace somavr::patch_safety {

constexpr bool InstructionPointerOverlapsPatch(
    uintptr_t instructionPointer,
    uintptr_t patchStart,
    size_t patchSize)
{
    return patchSize != 0
        && instructionPointer >= patchStart
        && instructionPointer - patchStart < patchSize;
}

} // namespace somavr::patch_safety
