#pragma once

#include "Config.h"
#include "HPLCameraMath.h"
#include "OpenXRRuntime.h"

#include <cstdint>

namespace somavr {

bool InstallHPLInputBridge(const Config& config, OpenXRRuntime* openxr);
void UpdateHPLInputBridge(uint64_t frameIndex);
bool GetHPLVirtualTorsoYaw(camera_math::Quaternion& yaw);
void RemoveHPLInputBridge();
void LogHPLInputBridgeSummary();

} // namespace somavr
