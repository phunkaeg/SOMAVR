#pragma once

#include "Config.h"
#include "OpenXRRuntime.h"

#include <cstdint>

namespace somavr {

struct OpenGLTelemetrySnapshot {
    uint64_t drawElements = 0;
    uint64_t drawArrays = 0;
    uint64_t viewportCalls = 0;
    uint64_t framebufferBinds = 0;
    uint64_t programUses = 0;
    uint64_t clears = 0;
};

bool InstallOpenGLHooks(const Config& config, OpenXRRuntime* openxr);
void RemoveOpenGLHooks();
void LogOpenGLProofSummary();
uint64_t GetOpenGLRenderFrameHint();
OpenGLTelemetrySnapshot GetOpenGLTelemetrySnapshot();

} // namespace somavr
