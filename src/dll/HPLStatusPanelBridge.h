#pragma once

#include "Config.h"
#include "HPLCameraBridge.h"
#include "HPLPlayerState.h"
#include "OpenXRRuntime.h"

#include <cstdint>

namespace somavr {

bool InstallHPLStatusPanelBridge(const Config& config, OpenXRRuntime* openxr);
bool UpdateHPLStatusPanelBridge(
    uint64_t frameIndex,
    const OpenXRInputSnapshot* input,
    int dominantHand,
    const HPLPlayerStateSnapshot& player,
    const HPLCameraBridgeStatus& camera);
void LogHPLStatusPanelBridgeSummary();
void RemoveHPLStatusPanelBridge();

} // namespace somavr
