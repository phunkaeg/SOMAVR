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
void BeginPostEffectResourceCapture(
    uint64_t frame,
    uint64_t sequence,
    int eye,
    uint64_t poseFrame,
    const char* effectName,
    void* effect,
    void* inputTexture,
    void* renderTarget,
    bool lastEffect,
    bool forceCapture);
void EndPostEffectResourceCapture(void* outputTexture);

} // namespace somavr
