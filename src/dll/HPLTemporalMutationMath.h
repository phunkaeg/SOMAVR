#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace somavr::temporal_mutation_math {

constexpr size_t kMaxChangedSpans = 8;

struct ChangedSpan {
    size_t offset = 0;
    size_t length = 0;
};

struct MutationSummary {
    size_t size = 0;
    size_t changedBytes = 0;
    uint64_t beforeHash = 0;
    uint64_t afterHash = 0;
    std::array<ChangedSpan, kMaxChangedSpans> spans{};
    size_t spanCount = 0;
    bool spansTruncated = false;
};

uint64_t HashBytes(const uint8_t* bytes, size_t size);
MutationSummary SummarizeMutation(
    const uint8_t* before,
    const uint8_t* after,
    size_t size);
bool HasEquivalentMutationPattern(
    const MutationSummary& first,
    const MutationSummary& second);

} // namespace somavr::temporal_mutation_math
