#pragma once

#include "Config.h"
#include "HPLDualRenderDiagnostics.h"
#include "OpenXRRuntime.h"

#include <cstdint>

namespace somavr {

enum class HPLRenderStage : uint8_t {
    None,
    Viewport,
    World,
    WorldCallbacks,
    PostEffects,
    PostPostEffect,
    ScreenGui,
};

bool InstallHPLCompatibilityProbe(const Config& config, OpenXRRuntime* openxr);
void LogHPLCompatibilityProbeSummary();
void RemoveHPLCompatibilityProbe();
HPLRenderStage GetActiveHPLRenderStage();
const char* GetHPLRenderStageName(HPLRenderStage stage);
HPLDualRenderPass GetActiveHPLDualRenderPass();
const char* GetHPLDualRenderPassName(HPLDualRenderPass pass);

} // namespace somavr
