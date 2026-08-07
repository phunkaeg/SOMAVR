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

struct TerminalClearSuppressionSnapshot {
    bool active = false;
    uint32_t targetFramebuffer = 0;
    uint64_t frame = 0;
    uint64_t armedCaptures = 0;
    uint64_t suppressedColorClears = 0;
    uint64_t forwardedDepthStencilClears = 0;
    uint64_t framebufferMismatches = 0;
    uint64_t threadMismatches = 0;
};

bool InstallOpenGLHooks(const Config& config, OpenXRRuntime* openxr);
void RemoveOpenGLHooks();
void LogOpenGLProofSummary();
uint64_t GetOpenGLRenderFrameHint();
OpenGLTelemetrySnapshot GetOpenGLTelemetrySnapshot();
void BeginTerminalCaptureGuard(uint64_t frame, uint32_t targetFramebuffer);
void EndTerminalCaptureGuard();
void BeginTerminalColorClearSuppression(uint64_t frame, uint32_t targetFramebuffer);
void EndTerminalColorClearSuppression();
TerminalClearSuppressionSnapshot GetTerminalClearSuppressionSnapshot();
uint64_t GetTerminalOffscreenScissorBypassCount();
void BeginTerminalDrawStateProbe(
    uint64_t sequence,
    uint64_t startFrame,
    uint32_t durationFrames);
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
