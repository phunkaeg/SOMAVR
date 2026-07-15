#pragma once

#include <cstdint>

namespace somavr {

enum class HPLDualRenderPass : uint8_t {
    None,
    FirstEye,
    ReplayEye,
};

struct HPLDualRenderDiagnosticsSummary {
    uint64_t captures = 0;
    uint64_t failedRegions = 0;
    uint64_t correlatedPairs = 0;
    uint64_t equivalentPairs = 0;
};

void BeginHPLDualRenderTemporalCapture(
    uint64_t frame,
    uint64_t attempt,
    HPLDualRenderPass pass,
    void* renderer,
    void* settings);
void EndHPLDualRenderTemporalCapture();
HPLDualRenderDiagnosticsSummary GetHPLDualRenderDiagnosticsSummary();
void ResetHPLDualRenderDiagnostics();

} // namespace somavr
