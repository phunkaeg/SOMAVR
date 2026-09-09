#pragma once

#include "Config.h"
#include "OpenXRRuntime.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace somavr {

constexpr size_t kHPLArmRenderDiagnosticHandCount = 2;
constexpr size_t kHPLArmRenderDiagnosticNodeCount = 15;
constexpr size_t kHPLArmRenderDiagnosticMaxBoneCount = 128;

struct HPLArmRenderSnapshot {
    bool valid = false;
    void* entity = nullptr;
    void* mesh = nullptr;
    uint64_t playerFrame = 0;
    bool paletteValid = false;
    size_t paletteCount = 0;
    std::array<float, 16> rootLocal{};
    std::array<float, 16> rootWorld{};
    std::array<std::array<std::array<float, 16>, kHPLArmRenderDiagnosticNodeCount>,
        kHPLArmRenderDiagnosticHandCount> local{};
    std::array<std::array<std::array<float, 16>, kHPLArmRenderDiagnosticNodeCount>,
        kHPLArmRenderDiagnosticHandCount> world{};
    std::array<std::array<float, 16>, kHPLArmRenderDiagnosticMaxBoneCount> palette{};
};

bool InstallHPLHandsBridge(const Config& config, OpenXRRuntime* openxr);
bool ResolveHPLControllerBeamDistance(
    const OpenXRControllerPose& aimPose,
    uint64_t gameFrame,
    float maxDistanceMeters,
    float& distanceMeters);
void UpdateHPLHandsBridge(uint64_t frameIndex);
bool CaptureHPLArmRenderSnapshot(HPLArmRenderSnapshot& snapshot);
void RemoveHPLHandsBridge();
void LogHPLHandsBridgeSummary();

} // namespace somavr
