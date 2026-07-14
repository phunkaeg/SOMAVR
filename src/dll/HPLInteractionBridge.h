#pragma once

#include "Config.h"
#include "OpenXRRuntime.h"

#include <cstdint>

namespace somavr {

struct HPLInteractionHitSnapshot {
    bool valid = false;
    uint64_t sequence = 0;
    uint64_t gameFrame = 0;
    uint32_t handIndex = 1;
    float distance = 0.0f;
    float worldX = 0.0f;
    float worldY = 0.0f;
    float worldZ = 0.0f;
    void* entity = nullptr;
    void* body = nullptr;
};

bool InstallHPLInteractionBridge(const Config& config, OpenXRRuntime* openxr);
void RemoveHPLInteractionBridge();
void LogHPLInteractionBridgeSummary();
bool GetHPLInteractionHitSnapshot(HPLInteractionHitSnapshot& snapshot);
void PublishHPLInteractionCrosshairState(int crosshairState);

} // namespace somavr
