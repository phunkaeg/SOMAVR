#pragma once

#include "HPLDualRenderDiagnostics.h"

#include <cstdint>

namespace somavr {

struct HPLArmRenderDiagnosticsSummary {
    uint64_t captureAttempts = 0;
    uint64_t captures = 0;
    uint64_t captureMisses = 0;
    uint64_t pairComparisons = 0;
    uint64_t coherentPairs = 0;
    uint64_t inputMismatches = 0;
    uint64_t inRenderMutations = 0;
    uint64_t interPassMutations = 0;
    uint64_t eyePoseMismatches = 0;
    uint64_t palettePairComparisons = 0;
    uint64_t paletteOutputMismatches = 0;
    uint64_t paletteFirstPassUpdates = 0;
    uint64_t paletteReplayPassUpdates = 0;
    uint64_t paletteInterPassMutations = 0;
    uint64_t paletteUnavailable = 0;
};

void BeginHPLArmRenderPassCapture(
    uint64_t frame,
    uint64_t attempt,
    HPLDualRenderPass pass);
void EndHPLArmRenderPassCapture(int eye, uint64_t poseFrame);
HPLArmRenderDiagnosticsSummary GetHPLArmRenderDiagnosticsSummary();
void ResetHPLArmRenderDiagnostics();

} // namespace somavr
