#pragma once

#include "Config.h"
#include "OpenXRRuntime.h"

#include <cstdint>

namespace somavr {

bool InstallHPLHandsBridge(const Config& config, OpenXRRuntime* openxr);
bool ResolveHPLControllerBeamDistance(
    const OpenXRControllerPose& aimPose,
    uint64_t gameFrame,
    float maxDistanceMeters,
    float& distanceMeters);
void UpdateHPLHandsBridge(uint64_t frameIndex);
void RemoveHPLHandsBridge();
void LogHPLHandsBridgeSummary();

} // namespace somavr
