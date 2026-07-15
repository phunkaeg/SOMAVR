#include "HPLTemporalMutationMath.h"

namespace somavr::temporal_mutation_math {

uint64_t HashBytes(const uint8_t* bytes, size_t size)
{
    constexpr uint64_t kFnvOffset = 14695981039346656037ull;
    constexpr uint64_t kFnvPrime = 1099511628211ull;
    uint64_t hash = kFnvOffset;
    if (bytes == nullptr) {
        return hash;
    }
    for (size_t index = 0; index < size; ++index) {
        hash ^= bytes[index];
        hash *= kFnvPrime;
    }
    return hash;
}

MutationSummary SummarizeMutation(
    const uint8_t* before,
    const uint8_t* after,
    size_t size)
{
    MutationSummary summary;
    summary.size = size;
    summary.beforeHash = HashBytes(before, size);
    summary.afterHash = HashBytes(after, size);
    if (before == nullptr || after == nullptr) {
        return summary;
    }

    size_t index = 0;
    while (index < size) {
        if (before[index] == after[index]) {
            ++index;
            continue;
        }

        const size_t start = index;
        while (index < size && before[index] != after[index]) {
            ++summary.changedBytes;
            ++index;
        }
        if (summary.spanCount < summary.spans.size()) {
            summary.spans[summary.spanCount++] = {start, index - start};
        } else {
            summary.spansTruncated = true;
        }
    }
    return summary;
}

bool HasEquivalentMutationPattern(
    const MutationSummary& first,
    const MutationSummary& second)
{
    if (first.size != second.size
        || first.changedBytes != second.changedBytes
        || first.spanCount != second.spanCount
        || first.spansTruncated != second.spansTruncated) {
        return false;
    }
    for (size_t index = 0; index < first.spanCount; ++index) {
        if (first.spans[index].offset != second.spans[index].offset
            || first.spans[index].length != second.spans[index].length) {
            return false;
        }
    }
    return true;
}

} // namespace somavr::temporal_mutation_math
