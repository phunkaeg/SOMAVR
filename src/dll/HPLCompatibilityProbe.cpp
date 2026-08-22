#include "HPLCompatibilityProbe.h"

#include "HPLCameraBridge.h"
#include "HPLCameraMath.h"
#include "HPLDualRenderControl.h"
#include "HPLDualRenderDiagnostics.h"
#include "HPLDualRenderMath.h"
#include "HPLPerEyeViewHistory.h"
#include "HPLPerEyePostEffect.h"
#include "HPLToneMappingFrame.h"
#include "HPLSSAOTemporalHistory.h"
#include "HPLSSAOFrameOwner.h"
#include "HPLPlayerState.h"
#include "Logger.h"
#include "OpenGLHooks.h"

#include <Windows.h>
#include <gl/GL.h>

#include <MinHook.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace somavr {
namespace {

using camera_math::Quaternion;
using camera_math::RotateVector;
using camera_math::Vector3;

constexpr uintptr_t kRenderViewportRva = 0x298630;
constexpr uintptr_t kRenderWorldRva = 0x1f9790;
constexpr uintptr_t kRenderWorldCallbacksRva = 0x297670;
constexpr uintptr_t kRenderPostEffectsRva = 0x33bd80;
constexpr uintptr_t kRenderPostEffectOneRva = 0x2d7a40;
constexpr uintptr_t kRenderPostPostEffectsRva = 0x1f1480;
constexpr uintptr_t kRenderScreenGuiRva = 0x2981e0;
constexpr uintptr_t kPostEffectHasActiveEffectsRva = 0x33b8f0;
constexpr uintptr_t kAudioListenerUpdateRva = 0x289340;
constexpr uintptr_t kImageTrailCreateResourcesRva = 0x38ae60;
constexpr uintptr_t kImageTrailDestroyResourcesRva = 0x38a8b0;
constexpr uintptr_t kSSAORenderRva = 0x3f2b50;
constexpr uintptr_t kRendererSetTextureUnitRva = 0x2aba30;
constexpr uintptr_t kSSAOTemporalPhaseRva = 0x79575c;

constexpr size_t kViewportCameraOffset = 0x18;
constexpr size_t kViewportWorldOffset = 0x20;
constexpr size_t kViewportActiveOffset = 0x28;
constexpr size_t kViewportVisibleOffset = 0x29;
constexpr size_t kViewportListenerOffset = 0x2a;
constexpr size_t kViewportRendererOffset = 0x30;
constexpr size_t kViewportPostCompositeOffset = 0x38;
constexpr size_t kViewportFramebufferOffset = 0x48;
constexpr size_t kViewportPositionOffset = 0x50;
constexpr size_t kViewportSizeOffset = 0x58;
constexpr size_t kViewportRenderSettingsOffset = 0xa8;
constexpr size_t kMaxViewportIdentities = 64;

constexpr uintptr_t kToneMappingVtableRva = 0x69b038;
constexpr uintptr_t kFxaaVtableRva = 0x6ac3b8;
constexpr uintptr_t kImageFadeFxVtableRva = 0x6ac4e8;
constexpr uintptr_t kVideoDistortionVtableRva = 0x6ac688;
constexpr uintptr_t kChromaticAberrationVtableRva = 0x6ac928;
constexpr uintptr_t kRadialBlurVtableRva = 0x6acb78;
constexpr uintptr_t kImageTrailVtableRva = 0x6acd48;

constexpr GLenum kGLCurrentProgram = 0x8b8d;
constexpr GLenum kGLDrawFramebufferBinding = 0x8ca6;
constexpr GLenum kGLReadFramebufferBinding = 0x8caa;
constexpr GLenum kGLViewport = 0x0ba2;
constexpr GLenum kGLScissorBox = 0x0c10;
constexpr GLenum kGLBlend = 0x0be2;
constexpr GLenum kGLDepthTest = 0x0b71;
constexpr GLenum kGLScissorTest = 0x0c11;
constexpr GLenum kGLDepthWriteMask = 0x0b72;
constexpr GLenum kGLColorWriteMask = 0x0c23;
constexpr GLenum kGLBlendSrcRgb = 0x80c9;
constexpr GLenum kGLBlendDstRgb = 0x80c8;
constexpr GLenum kGLBlendSrcAlpha = 0x80cb;
constexpr GLenum kGLBlendDstAlpha = 0x80ca;
constexpr GLenum kGLBlendEquationRgb = 0x8009;
constexpr GLenum kGLBlendEquationAlpha = 0x883d;
constexpr GLenum kGLTimestamp = 0x8e28;
constexpr GLenum kGLQueryResult = 0x8866;
constexpr GLenum kGLQueryResultAvailable = 0x8867;

constexpr size_t kListenerUpOffset = 0x50;
constexpr size_t kListenerForwardOffset = 0x5c;
constexpr size_t kListenerPositionOffset = 0x74;
constexpr size_t kListenerVelocityOffset = 0x80;

using GlGetIntegervFn = void(APIENTRY*)(GLenum, GLint*);
using GlGetBooleanvFn = void(APIENTRY*)(GLenum, GLboolean*);
using GlIsEnabledFn = GLboolean(APIENTRY*)(GLenum);
using GlGenQueriesFn = void(APIENTRY*)(GLsizei, GLuint*);
using GlDeleteQueriesFn = void(APIENTRY*)(GLsizei, const GLuint*);
using GlQueryCounterFn = void(APIENTRY*)(GLuint, GLenum);
using GlGetQueryObjectivFn = void(APIENTRY*)(GLuint, GLenum, GLint*);
using GlGetQueryObjectui64vFn = void(APIENTRY*)(GLuint, GLenum, uint64_t*);
using RenderViewportFn = void (*)(void*, void*, float, uint64_t);
using RenderWorldFn = void (*)(void*, float, void*, void*, void*, void*, bool, void*);
using RenderWorldCallbacksFn = void (*)(void*, void*, void*, float);
using RenderPostEffectsFn = void (*)(void*, float, void*, void*, void*);
using RenderPostEffectOneFn = void* (*)(void*, void*, void*, void*, bool);
using RenderPostPostEffectsFn = void (*)(void*, void*, void*, void*);
using RenderScreenGuiFn = void (*)(void*, void*, float);
using PostEffectHasActiveEffectsFn = bool (*)(void*);
using AudioListenerUpdateFn = void (*)(void*);
using ImageTrailResourceFn = void (*)(void*);
using SSAORenderFn = void (*)(void*);
using RendererSetTextureUnitFn = void (*)(void*, uint32_t, void*);

constexpr size_t kStageCount = 6;

struct GLState {
    bool valid = false;
    GLint drawFramebuffer = 0;
    GLint readFramebuffer = 0;
    GLint program = 0;
    GLint viewport[4] = {};
    GLint scissor[4] = {};
    GLint blendFunction[4] = {};
    GLint blendEquation[2] = {};
    GLboolean blendEnabled = GL_FALSE;
    GLboolean depthTestEnabled = GL_FALSE;
    GLboolean scissorEnabled = GL_FALSE;
    GLboolean depthWrite = GL_FALSE;
    GLboolean colorWrite[4] = {};
};

struct StageSample {
    bool enabled = false;
    bool timingEnabled = false;
    int gpuTimingSlot = -1;
    HPLRenderStage stage = HPLRenderStage::Viewport;
    HPLRenderStage previousStage = HPLRenderStage::None;
    uint64_t frame = 0;
    uint64_t sequence = 0;
    uint64_t call = 0;
    void* viewport = nullptr;
    uint64_t renderMask = 0;
    LARGE_INTEGER start = {};
    GLState before = {};
    OpenGLTelemetrySnapshot telemetryBefore = {};
};

struct GpuTimingRecord {
    GLuint startQuery = 0;
    GLuint endQuery = 0;
    bool active = false;
    bool pending = false;
    HPLRenderStage stage = HPLRenderStage::Viewport;
    int eyeIndex = -1;
    uint64_t frame = 0;
};

struct FrameSampleBudget {
    uint64_t frame = UINT64_MAX;
    uint32_t count = 0;
};

struct RenderTransactionFrame {
    bool active = false;
    bool sampled = false;
    uint64_t frame = 0;
    void* viewport = nullptr;
    uint64_t renderMask = 0;
    std::array<uint32_t, kStageCount> counts{};
    std::array<uint64_t, kStageCount> drawCalls{};
    std::array<uint64_t, kStageCount> clears{};
    std::array<uint64_t, kStageCount> durationNanoseconds{};
    std::array<HPLRenderStage, 32> order{};
    size_t orderCount = 0;
};

struct ViewportIdentity {
    void* camera = nullptr;
    void* world = nullptr;
    void* renderer = nullptr;
    void* postComposite = nullptr;
    void* framebuffer = nullptr;
    void* renderSettings = nullptr;
    int32_t position[2] = {};
    int32_t size[2] = {};
    uint8_t active = 0;
    uint8_t visible = 0;
    uint8_t listener = 0;
};

Config g_config;
OpenXRRuntime* g_openxr = nullptr;
uintptr_t g_executableBase = 0;
GlGetIntegervFn g_glGetIntegerv = nullptr;
GlGetBooleanvFn g_glGetBooleanv = nullptr;
GlIsEnabledFn g_glIsEnabled = nullptr;
GlGenQueriesFn g_glGenQueries = nullptr;
GlDeleteQueriesFn g_glDeleteQueries = nullptr;
GlQueryCounterFn g_glQueryCounter = nullptr;
GlGetQueryObjectivFn g_glGetQueryObjectiv = nullptr;
GlGetQueryObjectui64vFn g_glGetQueryObjectui64v = nullptr;
HGLRC g_gpuTimingContext = nullptr;
HGLRC g_gpuTimingUnavailableContext = nullptr;
std::vector<GpuTimingRecord> g_gpuTimingRecords;
std::vector<void*> g_hookTargets;
std::mutex g_installMutex;
std::mutex g_sampleMutex;
std::array<FrameSampleBudget, kStageCount> g_sampleBudgets;
std::array<std::atomic<uint64_t>, kStageCount> g_stageCalls = {};
std::array<std::array<std::atomic<uint64_t>, 3>, kStageCount> g_stageEyeCalls = {};
std::array<std::array<std::atomic<uint64_t>, 3>, kStageCount> g_stageEyeCpuNanoseconds = {};
std::array<std::array<std::atomic<uint64_t>, 3>, kStageCount> g_stageEyeGpuCalls = {};
std::array<std::array<std::atomic<uint64_t>, 3>, kStageCount> g_stageEyeGpuNanoseconds = {};
std::atomic<uint64_t> g_gpuTimingDropped = 0;
std::atomic<uint64_t> g_gpuTimingInvalid = 0;
std::atomic<uint64_t> g_lastPerformanceLogFrame = 0;
std::atomic<uint64_t> g_lastGpuPerformanceLogFrame = 0;
std::atomic<uint64_t> g_renderTransactionFrames = 0;
std::atomic<uint64_t> g_canonicalRenderTransactions = 0;
std::atomic<uint64_t> g_viewportIdentitySamples = 0;
std::atomic<uint64_t> g_viewportIdentityChanges = 0;
std::atomic<uint64_t> g_playerViewportCalls = 0;
std::atomic<uint64_t> g_secondaryViewportCalls = 0;
std::atomic<uint64_t> g_dualRenderArms = 0;
std::atomic<uint64_t> g_dualRenderAttempts = 0;
std::atomic<uint64_t> g_dualRenderFirstEyeCaptures = 0;
std::atomic<uint64_t> g_dualRenderReplays = 0;
std::atomic<uint64_t> g_dualRenderSamePoseOppositeEye = 0;
std::atomic<uint64_t> g_dualRenderFailures = 0;
std::atomic<uint64_t> g_dualRenderContinuousReplays = 0;
std::atomic<uint64_t> g_dualRenderContinuousSkips = 0;
std::atomic<bool> g_dualRenderArmed = false;
std::atomic<bool> g_dualRenderKeyDown = false;
std::atomic<uint64_t> g_dualRenderAutoCompleted = 0;
std::atomic<uint64_t> g_dualRenderAutoNextFrame = 0;

enum class DualRenderArmSource : uint8_t {
    None,
    Manual,
    Automatic,
    Continuous,
};

std::atomic<DualRenderArmSource> g_dualRenderArmSource = DualRenderArmSource::None;
std::mutex g_viewportIdentityMutex;
std::unordered_map<void*, ViewportIdentity> g_viewportIdentities;
int64_t g_performanceFrequency = 0;
std::atomic<uint64_t> g_audioCalls = 0;
std::atomic<uint64_t> g_audioPoseSamples = 0;
std::atomic<uint64_t> g_audioCorrections = 0;
std::atomic<uint64_t> g_audioTranslations = 0;
std::atomic<uint64_t> g_postEffectQueries = 0;
std::atomic<uint64_t> g_postEffectBypasses = 0;
std::atomic<bool> g_postEffectBypassEnabled = false;
std::atomic<bool> g_f12Down = false;
std::atomic<uint64_t> g_postEffectInventorySamples = 0;
std::atomic<uint64_t> g_postEffectRenderOneCalls = 0;
std::atomic<void*> g_postEffectIsolated = nullptr;
std::atomic<uint64_t> g_postEffectIsolationApplications = 0;
std::atomic<uint64_t> g_postEffectComfortApplications = 0;
std::atomic<uint64_t> g_postEffectComfortSuppressed = 0;
uint64_t g_lastPostEffectInventorySignature = 0;

RenderViewportFn g_originalRenderViewport = nullptr;
RenderWorldFn g_originalRenderWorld = nullptr;
RenderWorldCallbacksFn g_originalRenderWorldCallbacks = nullptr;
RenderPostEffectsFn g_originalRenderPostEffects = nullptr;
RenderPostEffectOneFn g_originalRenderPostEffectOne = nullptr;
RenderPostPostEffectsFn g_originalRenderPostPostEffects = nullptr;
RenderScreenGuiFn g_originalRenderScreenGui = nullptr;
PostEffectHasActiveEffectsFn g_originalPostEffectHasActiveEffects = nullptr;
AudioListenerUpdateFn g_originalAudioListenerUpdate = nullptr;
ImageTrailResourceFn g_originalImageTrailDestroyResources = nullptr;
SSAORenderFn g_originalSSAORender = nullptr;
RendererSetTextureUnitFn g_originalRendererSetTextureUnit = nullptr;

thread_local uint64_t g_traceFrame = UINT64_MAX;
thread_local uint64_t g_traceSequence = 0;
thread_local void* g_activeViewport = nullptr;
thread_local uint64_t g_activeRenderMask = 0;
thread_local HPLRenderStage g_activeStage = HPLRenderStage::None;
thread_local bool g_forcePostEffectResourceCapture = false;
thread_local HPLDualRenderPass g_dualRenderPass = HPLDualRenderPass::None;
thread_local uint64_t g_dualRenderAttempt = 0;
thread_local RenderTransactionFrame g_renderTransaction;

size_t StageIndex(HPLRenderStage stage)
{
    switch (stage) {
    case HPLRenderStage::Viewport: return 0;
    case HPLRenderStage::World: return 1;
    case HPLRenderStage::WorldCallbacks: return 2;
    case HPLRenderStage::PostEffects: return 3;
    case HPLRenderStage::PostPostEffect: return 4;
    case HPLRenderStage::ScreenGui: return 5;
    default: return 0;
    }
}

size_t EyeSlot(int eyeIndex)
{
    return eyeIndex == 0 ? 0 : eyeIndex == 1 ? 1 : 2;
}

double AverageStageCpuMicroseconds(size_t stageIndex, size_t eyeSlot)
{
    const uint64_t calls = g_stageEyeCalls[stageIndex][eyeSlot].load(std::memory_order_relaxed);
    const uint64_t nanoseconds = g_stageEyeCpuNanoseconds[stageIndex][eyeSlot].load(
        std::memory_order_relaxed);
    return calls != 0
        ? static_cast<double>(nanoseconds) / static_cast<double>(calls) / 1000.0
        : 0.0;
}

double AverageStageGpuMicroseconds(size_t stageIndex, size_t eyeSlot)
{
    const uint64_t calls = g_stageEyeGpuCalls[stageIndex][eyeSlot].load(std::memory_order_relaxed);
    const uint64_t nanoseconds = g_stageEyeGpuNanoseconds[stageIndex][eyeSlot].load(
        std::memory_order_relaxed);
    return calls != 0
        ? static_cast<double>(nanoseconds) / static_cast<double>(calls) / 1000.0
        : 0.0;
}

bool IsValidWglProcAddress(PROC address)
{
    const uintptr_t value = reinterpret_cast<uintptr_t>(address);
    return address != nullptr && value > 3 && value != static_cast<uintptr_t>(-1);
}

template <typename T>
T ResolveGLProc(const char* name)
{
    PROC address = wglGetProcAddress(name);
    return IsValidWglProcAddress(address) ? reinterpret_cast<T>(address) : nullptr;
}

void ResetGpuTimingState(bool deleteQueries)
{
    if (deleteQueries && g_glDeleteQueries != nullptr && !g_gpuTimingRecords.empty()) {
        std::vector<GLuint> queries;
        queries.reserve(g_gpuTimingRecords.size() * 2);
        for (const GpuTimingRecord& record : g_gpuTimingRecords) {
            queries.push_back(record.startQuery);
            queries.push_back(record.endQuery);
        }
        g_glDeleteQueries(static_cast<GLsizei>(queries.size()), queries.data());
    }
    g_gpuTimingRecords.clear();
    g_glGenQueries = nullptr;
    g_glDeleteQueries = nullptr;
    g_glQueryCounter = nullptr;
    g_glGetQueryObjectiv = nullptr;
    g_glGetQueryObjectui64v = nullptr;
    g_gpuTimingContext = nullptr;
}

bool EnsureGpuTimingReady()
{
    if (!g_config.hplPerEyeGpuTelemetry) {
        return false;
    }
    const HGLRC context = wglGetCurrentContext();
    if (context == nullptr) {
        return false;
    }
    if (context == g_gpuTimingContext && !g_gpuTimingRecords.empty()) {
        return true;
    }
    if (context == g_gpuTimingUnavailableContext) {
        return false;
    }
    if (g_gpuTimingContext != nullptr && context != g_gpuTimingContext) {
        ResetGpuTimingState(false);
    }

    g_glGenQueries = ResolveGLProc<GlGenQueriesFn>("glGenQueries");
    g_glDeleteQueries = ResolveGLProc<GlDeleteQueriesFn>("glDeleteQueries");
    g_glQueryCounter = ResolveGLProc<GlQueryCounterFn>("glQueryCounter");
    g_glGetQueryObjectiv = ResolveGLProc<GlGetQueryObjectivFn>("glGetQueryObjectiv");
    g_glGetQueryObjectui64v = ResolveGLProc<GlGetQueryObjectui64vFn>("glGetQueryObjectui64v");
    if (g_glGenQueries == nullptr || g_glDeleteQueries == nullptr
        || g_glQueryCounter == nullptr || g_glGetQueryObjectiv == nullptr
        || g_glGetQueryObjectui64v == nullptr) {
        Logger::Instance().Write(
            LogLevel::Warn,
            "hpl_per_eye_gpu unavailable context=%p reason=missing_timer_query_functions",
            context);
        ResetGpuTimingState(false);
        g_gpuTimingUnavailableContext = context;
        return false;
    }

    const size_t recordCount = static_cast<size_t>(std::clamp(g_config.hplGpuQueryPoolSize, 16, 512));
    std::vector<GLuint> queries(recordCount * 2, 0);
    g_glGenQueries(static_cast<GLsizei>(queries.size()), queries.data());
    g_gpuTimingRecords.resize(recordCount);
    for (size_t index = 0; index < recordCount; ++index) {
        g_gpuTimingRecords[index].startQuery = queries[index * 2];
        g_gpuTimingRecords[index].endQuery = queries[index * 2 + 1];
    }
    g_gpuTimingContext = context;
    g_gpuTimingUnavailableContext = nullptr;
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_per_eye_gpu ready context=%p queryPairs=%llu nonBlocking=1",
        context,
        static_cast<unsigned long long>(recordCount));
    return true;
}

void PollGpuTimingResults()
{
    if (g_glGetQueryObjectiv == nullptr || g_glGetQueryObjectui64v == nullptr) {
        return;
    }
    if (wglGetCurrentContext() != g_gpuTimingContext) {
        return;
    }
    for (GpuTimingRecord& record : g_gpuTimingRecords) {
        if (!record.pending) {
            continue;
        }
        GLint available = GL_FALSE;
        g_glGetQueryObjectiv(record.endQuery, kGLQueryResultAvailable, &available);
        if (available != GL_TRUE) {
            continue;
        }
        uint64_t start = 0;
        uint64_t end = 0;
        g_glGetQueryObjectui64v(record.startQuery, kGLQueryResult, &start);
        g_glGetQueryObjectui64v(record.endQuery, kGLQueryResult, &end);
        if (end >= start) {
            const size_t stageIndex = StageIndex(record.stage);
            const size_t eyeSlot = EyeSlot(record.eyeIndex);
            g_stageEyeGpuCalls[stageIndex][eyeSlot].fetch_add(1, std::memory_order_relaxed);
            g_stageEyeGpuNanoseconds[stageIndex][eyeSlot].fetch_add(end - start, std::memory_order_relaxed);
        } else {
            g_gpuTimingInvalid.fetch_add(1, std::memory_order_relaxed);
        }
        record.pending = false;
    }
}

int BeginGpuTiming(HPLRenderStage stage, uint64_t frame)
{
    if (!EnsureGpuTimingReady()) {
        return -1;
    }
    for (size_t index = 0; index < g_gpuTimingRecords.size(); ++index) {
        GpuTimingRecord& record = g_gpuTimingRecords[index];
        if (!record.active && !record.pending) {
            record.active = true;
            record.stage = stage;
            record.frame = frame;
            record.eyeIndex = -1;
            g_glQueryCounter(record.startQuery, kGLTimestamp);
            return static_cast<int>(index);
        }
    }
    g_gpuTimingDropped.fetch_add(1, std::memory_order_relaxed);
    return -1;
}

void EndGpuTiming(int slot, int eyeIndex)
{
    if (slot < 0 || static_cast<size_t>(slot) >= g_gpuTimingRecords.size()
        || g_glQueryCounter == nullptr) {
        return;
    }
    GpuTimingRecord& record = g_gpuTimingRecords[static_cast<size_t>(slot)];
    if (!record.active) {
        return;
    }
    g_glQueryCounter(record.endQuery, kGLTimestamp);
    record.eyeIndex = eyeIndex;
    record.active = false;
    record.pending = true;
}

void LogPerEyePerformance(uint64_t frame, bool finalSummary)
{
    if (!g_config.hplPerEyePerformanceTelemetry) {
        return;
    }
    if (!finalSummary) {
        const uint64_t previous = g_lastPerformanceLogFrame.exchange(frame, std::memory_order_relaxed);
        if (previous == frame) {
            return;
        }
    }

    constexpr HPLRenderStage kStages[kStageCount] = {
        HPLRenderStage::Viewport,
        HPLRenderStage::World,
        HPLRenderStage::WorldCallbacks,
        HPLRenderStage::PostEffects,
        HPLRenderStage::PostPostEffect,
        HPLRenderStage::ScreenGui,
    };
    for (size_t stageIndex = 0; stageIndex < kStageCount; ++stageIndex) {
        Logger::Instance().Write(
            LogLevel::Info,
            "hpl_per_eye_cpu frame=%llu final=%d stage=%s left={calls=%llu avgUs=%.2f totalMs=%.2f} right={calls=%llu avgUs=%.2f totalMs=%.2f} mono={calls=%llu avgUs=%.2f totalMs=%.2f}",
            static_cast<unsigned long long>(frame),
            finalSummary ? 1 : 0,
            GetHPLRenderStageName(kStages[stageIndex]),
            static_cast<unsigned long long>(g_stageEyeCalls[stageIndex][0].load(std::memory_order_relaxed)),
            AverageStageCpuMicroseconds(stageIndex, 0),
            static_cast<double>(g_stageEyeCpuNanoseconds[stageIndex][0].load(std::memory_order_relaxed)) / 1000000.0,
            static_cast<unsigned long long>(g_stageEyeCalls[stageIndex][1].load(std::memory_order_relaxed)),
            AverageStageCpuMicroseconds(stageIndex, 1),
            static_cast<double>(g_stageEyeCpuNanoseconds[stageIndex][1].load(std::memory_order_relaxed)) / 1000000.0,
            static_cast<unsigned long long>(g_stageEyeCalls[stageIndex][2].load(std::memory_order_relaxed)),
            AverageStageCpuMicroseconds(stageIndex, 2),
            static_cast<double>(g_stageEyeCpuNanoseconds[stageIndex][2].load(std::memory_order_relaxed)) / 1000000.0);
    }
}

void LogPerEyeGpuPerformance(uint64_t frame, bool finalSummary)
{
    if (!g_config.hplPerEyeGpuTelemetry) {
        return;
    }
    PollGpuTimingResults();
    if (!finalSummary) {
        const uint64_t previous = g_lastGpuPerformanceLogFrame.exchange(frame, std::memory_order_relaxed);
        if (previous == frame) {
            return;
        }
    }
    constexpr HPLRenderStage kStages[kStageCount] = {
        HPLRenderStage::Viewport,
        HPLRenderStage::World,
        HPLRenderStage::WorldCallbacks,
        HPLRenderStage::PostEffects,
        HPLRenderStage::PostPostEffect,
        HPLRenderStage::ScreenGui,
    };
    for (size_t stageIndex = 0; stageIndex < kStageCount; ++stageIndex) {
        Logger::Instance().Write(
            LogLevel::Info,
            "hpl_per_eye_gpu frame=%llu final=%d stage=%s left={calls=%llu avgUs=%.2f totalMs=%.2f} right={calls=%llu avgUs=%.2f totalMs=%.2f} mono={calls=%llu avgUs=%.2f totalMs=%.2f} dropped=%llu invalid=%llu",
            static_cast<unsigned long long>(frame),
            finalSummary ? 1 : 0,
            GetHPLRenderStageName(kStages[stageIndex]),
            static_cast<unsigned long long>(g_stageEyeGpuCalls[stageIndex][0].load(std::memory_order_relaxed)),
            AverageStageGpuMicroseconds(stageIndex, 0),
            static_cast<double>(g_stageEyeGpuNanoseconds[stageIndex][0].load(std::memory_order_relaxed)) / 1000000.0,
            static_cast<unsigned long long>(g_stageEyeGpuCalls[stageIndex][1].load(std::memory_order_relaxed)),
            AverageStageGpuMicroseconds(stageIndex, 1),
            static_cast<double>(g_stageEyeGpuNanoseconds[stageIndex][1].load(std::memory_order_relaxed)) / 1000000.0,
            static_cast<unsigned long long>(g_stageEyeGpuCalls[stageIndex][2].load(std::memory_order_relaxed)),
            AverageStageGpuMicroseconds(stageIndex, 2),
            static_cast<double>(g_stageEyeGpuNanoseconds[stageIndex][2].load(std::memory_order_relaxed)) / 1000000.0,
            static_cast<unsigned long long>(g_gpuTimingDropped.load(std::memory_order_relaxed)),
            static_cast<unsigned long long>(g_gpuTimingInvalid.load(std::memory_order_relaxed)));
    }
}

bool ValidateAudioRotationMath()
{
    constexpr float kEpsilon = 1.0e-4f;
    const auto isNear = [=](float left, float right) {
        return std::fabs(left - right) <= kEpsilon;
    };
    const Vector3 forward{0.0f, 0.0f, 1.0f};
    const Vector3 identityResult = RotateVector({}, forward);
    if (!isNear(identityResult.x, 0.0f)
        || !isNear(identityResult.y, 0.0f)
        || !isNear(identityResult.z, 1.0f)) {
        return false;
    }

    constexpr float kHalfSqrtTwo = 0.7071067811865475f;
    const Vector3 yawResult = RotateVector(
        {0.0f, kHalfSqrtTwo, 0.0f, kHalfSqrtTwo},
        forward);
    return isNear(yawResult.x, 1.0f)
        && isNear(yawResult.y, 0.0f)
        && isNear(yawResult.z, 0.0f);
}

bool IsInsideImage(HMODULE module, uintptr_t rva, size_t bytes)
{
    if (module == nullptr) {
        return false;
    }

    const auto* base = reinterpret_cast<const std::byte*>(module);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        return false;
    }

    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        return false;
    }

    const size_t imageSize = nt->OptionalHeader.SizeOfImage;
    return rva < imageSize && bytes <= imageSize - rva;
}

bool MatchBytes(const void* address, const uint8_t* expected, size_t size)
{
    return address != nullptr && std::memcmp(address, expected, size) == 0;
}

bool IsReadable(const void* address, size_t size)
{
    if (address == nullptr || size == 0) {
        return false;
    }

    MEMORY_BASIC_INFORMATION memory = {};
    if (VirtualQuery(address, &memory, sizeof(memory)) == 0
        || memory.State != MEM_COMMIT
        || (memory.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) {
        return false;
    }

    const uintptr_t start = reinterpret_cast<uintptr_t>(address);
    const uintptr_t end = start + size;
    const uintptr_t regionEnd = reinterpret_cast<uintptr_t>(memory.BaseAddress) + memory.RegionSize;
    return end >= start && end <= regionEnd;
}

bool IsWritable(void* address, size_t size)
{
    if (!IsReadable(address, size)) {
        return false;
    }

    MEMORY_BASIC_INFORMATION memory = {};
    VirtualQuery(address, &memory, sizeof(memory));
    constexpr DWORD kWritable = PAGE_READWRITE | PAGE_WRITECOPY
        | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
    return (memory.Protect & kWritable) != 0;
}

template <typename T>
bool ReadField(const void* object, size_t offset, T& value)
{
    if (object == nullptr) {
        return false;
    }
    const auto* address = static_cast<const std::byte*>(object) + offset;
    if (!IsReadable(address, sizeof(value))) {
        return false;
    }
    std::memcpy(&value, address, sizeof(value));
    return true;
}

template <typename T>
bool WriteField(void* object, size_t offset, const T& value)
{
    if (object == nullptr) {
        return false;
    }
    auto* address = static_cast<std::byte*>(object) + offset;
    if (!IsWritable(address, sizeof(value))) {
        return false;
    }
    std::memcpy(address, &value, sizeof(value));
    return true;
}

bool SameViewportIdentity(const ViewportIdentity& left, const ViewportIdentity& right)
{
    return left.camera == right.camera
        && left.world == right.world
        && left.renderer == right.renderer
        && left.postComposite == right.postComposite
        && left.framebuffer == right.framebuffer
        && left.renderSettings == right.renderSettings
        && left.position[0] == right.position[0]
        && left.position[1] == right.position[1]
        && left.size[0] == right.size[0]
        && left.size[1] == right.size[1]
        && left.active == right.active
        && left.visible == right.visible
        && left.listener == right.listener;
}

void ObserveViewportIdentity(void* viewport, uint64_t renderMask)
{
    if (viewport == nullptr) return;

    ViewportIdentity identity;
    const bool readable = ReadField(viewport, kViewportCameraOffset, identity.camera)
        && ReadField(viewport, kViewportWorldOffset, identity.world)
        && ReadField(viewport, kViewportActiveOffset, identity.active)
        && ReadField(viewport, kViewportVisibleOffset, identity.visible)
        && ReadField(viewport, kViewportListenerOffset, identity.listener)
        && ReadField(viewport, kViewportRendererOffset, identity.renderer)
        && ReadField(viewport, kViewportPostCompositeOffset, identity.postComposite)
        && ReadField(viewport, kViewportFramebufferOffset, identity.framebuffer)
        && ReadField(viewport, kViewportPositionOffset, identity.position)
        && ReadField(viewport, kViewportSizeOffset, identity.size)
        && ReadField(viewport, kViewportRenderSettingsOffset, identity.renderSettings);
    const uint64_t sample = g_viewportIdentitySamples.fetch_add(1, std::memory_order_relaxed) + 1;
    if (!readable) {
        if (sample <= 8) {
            Logger::Instance().Write(
                LogLevel::Warn,
                "hpl_viewport_identity sample=%llu viewport=%p readable=0 policy=native_render_unchanged",
                static_cast<unsigned long long>(sample),
                viewport);
        }
        return;
    }

    HPLPlayerStateSnapshot player{};
    const bool playerCameraKnown = GetHPLPlayerStateSnapshot(player)
        && player.playerValid
        && player.camera != nullptr;
    const HPLCameraBridgeStatus camera = GetHPLCameraBridgeStatus();
    const bool playerMatch = playerCameraKnown && identity.camera == player.camera;
    const bool activeMatch = camera.activeCamera != nullptr && identity.camera == camera.activeCamera;
    if (playerMatch) {
        g_playerViewportCalls.fetch_add(1, std::memory_order_relaxed);
    } else if (playerCameraKnown && identity.camera != nullptr) {
        g_secondaryViewportCalls.fetch_add(1, std::memory_order_relaxed);
    }

    bool first = false;
    bool changed = false;
    size_t knownCount = 0;
    {
        std::lock_guard lock(g_viewportIdentityMutex);
        const auto found = g_viewportIdentities.find(viewport);
        if (found == g_viewportIdentities.end()) {
            if (g_viewportIdentities.size() < kMaxViewportIdentities) {
                first = g_viewportIdentities.emplace(viewport, identity).second;
            }
        } else if (!SameViewportIdentity(found->second, identity)) {
            changed = true;
            found->second = identity;
            g_viewportIdentityChanges.fetch_add(1, std::memory_order_relaxed);
        }
        knownCount = g_viewportIdentities.size();
    }

    const uint64_t interval = static_cast<uint64_t>(
        std::max(g_config.hplCompatibilityLogInterval, 1));
    if (first || changed || sample <= 12 || sample % interval == 0) {
        const bool secondaryRole = !playerMatch && !activeMatch && playerCameraKnown;
        const char* role = playerMatch ? "player"
            : activeMatch ? "active_vr"
            : secondaryRole ? "secondary"
            : "unresolved";
        Logger::Instance().Write(
            first || changed || secondaryRole ? LogLevel::Warn : LogLevel::Info,
            "hpl_viewport_identity sample=%llu frame=%llu viewport=%p readable=1 role=%s first=%d changed=%d known=%llu mask=0x%llx camera=%p playerCamera=%p activeCamera=%p playerMatch=%d activeMatch=%d world=%p renderer=%p postComposite=%p framebuffer=%p renderSettings=%p position=%d,%d size=%d,%d active=%u visible=%u listener=%u policy=player_camera_only_receives_vr_controls",
            static_cast<unsigned long long>(sample),
            static_cast<unsigned long long>(GetOpenGLRenderFrameHint()),
            viewport,
            role,
            first ? 1 : 0,
            changed ? 1 : 0,
            static_cast<unsigned long long>(knownCount),
            static_cast<unsigned long long>(renderMask),
            identity.camera,
            playerCameraKnown ? player.camera : nullptr,
            camera.activeCamera,
            playerMatch ? 1 : 0,
            activeMatch ? 1 : 0,
            identity.world,
            identity.renderer,
            identity.postComposite,
            identity.framebuffer,
            identity.renderSettings,
            identity.position[0], identity.position[1],
            identity.size[0], identity.size[1],
            static_cast<unsigned int>(identity.active),
            static_cast<unsigned int>(identity.visible),
            static_cast<unsigned int>(identity.listener));
    }
}

bool IsExactPlayerViewport(void* viewport)
{
    void* viewportCamera = nullptr;
    HPLPlayerStateSnapshot player{};
    const HPLCameraBridgeStatus camera = GetHPLCameraBridgeStatus();
    return ReadField(viewport, kViewportCameraOffset, viewportCamera)
        && GetHPLPlayerStateSnapshot(player)
        && player.playerValid
        && player.camera != nullptr
        && viewportCamera == player.camera
        && viewportCamera == camera.activeCamera;
}

const char* DualRenderArmSourceName(DualRenderArmSource source)
{
    switch (source) {
    case DualRenderArmSource::Manual: return "manual";
    case DualRenderArmSource::Automatic: return "auto";
    case DualRenderArmSource::Continuous: return "continuous";
    default: return "none";
    }
}

bool ShouldLogDualRenderEvent(DualRenderArmSource source, uint64_t sequence)
{
    return source != DualRenderArmSource::Continuous
        || sequence <= 5
        || sequence % 300 == 0;
}

bool ArmDualRenderReplay(DualRenderArmSource source)
{
    bool expected = false;
    if (!g_dualRenderArmed.compare_exchange_strong(
            expected, true, std::memory_order_relaxed)) {
        return false;
    }
    g_dualRenderArmSource.store(source, std::memory_order_relaxed);
    const uint64_t arms = g_dualRenderArms.fetch_add(1, std::memory_order_relaxed) + 1;
    Logger::Instance().Write(
        LogLevel::Warn,
        "hpl_dual_render armed=1 source=%s arms=%llu policy=next_exact_player_viewport_one_frame",
        DualRenderArmSourceName(source),
        static_cast<unsigned long long>(arms));
    return true;
}

void PollDualRenderReplayHotkey()
{
    const bool keyDown = (GetAsyncKeyState(VK_F6) & 0x8000) != 0
        && (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    const bool previousKeyDown = g_dualRenderKeyDown.exchange(keyDown, std::memory_order_relaxed);
    if (!g_config.hplDualRenderReplayProbe || !keyDown || previousKeyDown) {
        return;
    }

    ArmDualRenderReplay(DualRenderArmSource::Manual);
}

void PollDualRenderAutoProbe(void* viewport)
{
    if (!g_config.hplDualRenderReplayProbe
        || !g_config.hplDualRenderAutoProbe
        || g_dualRenderArmed.load(std::memory_order_relaxed)
        || !IsExactPlayerViewport(viewport)) {
        return;
    }

    const uint64_t completed = g_dualRenderAutoCompleted.load(std::memory_order_relaxed);
    if (completed >= static_cast<uint64_t>(g_config.hplDualRenderAutoProbeCount)) {
        return;
    }
    const HPLCameraBridgeStatus camera = GetHPLCameraBridgeStatus();
    if (!camera.trackingEnabled
        || !camera.stereoEnabled
        || g_openxr == nullptr
        || (camera.stereoRenderEye != 0 && camera.stereoRenderEye != 1)
        || camera.stereoRenderPoseFrame == 0) {
        return;
    }

    const uint64_t frame = GetOpenGLRenderFrameHint();
    uint64_t nextFrame = g_dualRenderAutoNextFrame.load(std::memory_order_relaxed);
    if (nextFrame == 0) {
        nextFrame = frame + static_cast<uint64_t>(g_config.hplDualRenderAutoProbeDelayFrames);
        g_dualRenderAutoNextFrame.store(nextFrame, std::memory_order_relaxed);
        Logger::Instance().Write(
            LogLevel::Info,
            "hpl_dual_render_auto scheduled=1 frame=%llu firstSampleFrame=%llu count=%d intervalFrames=%d policy=bounded_diagnostic_only",
            static_cast<unsigned long long>(frame),
            static_cast<unsigned long long>(nextFrame),
            g_config.hplDualRenderAutoProbeCount,
            g_config.hplDualRenderAutoProbeIntervalFrames);
        return;
    }
    if (frame < nextFrame || !ArmDualRenderReplay(DualRenderArmSource::Automatic)) {
        return;
    }

    const uint64_t sample = g_dualRenderAutoCompleted.fetch_add(1, std::memory_order_relaxed) + 1;
    g_dualRenderAutoNextFrame.store(
        frame + static_cast<uint64_t>(g_config.hplDualRenderAutoProbeIntervalFrames),
        std::memory_order_relaxed);
    Logger::Instance().Write(
        LogLevel::Warn,
        "hpl_dual_render_auto sample=%llu count=%d frame=%llu",
        static_cast<unsigned long long>(sample),
        g_config.hplDualRenderAutoProbeCount,
        static_cast<unsigned long long>(frame));
}

const char* PostEffectName(void* effect)
{
    void* vtable = nullptr;
    if (!ReadField(effect, 0, vtable) || reinterpret_cast<uintptr_t>(vtable) < g_executableBase) {
        return "Unknown";
    }
    switch (reinterpret_cast<uintptr_t>(vtable) - g_executableBase) {
    case kToneMappingVtableRva: return "ToneMapping";
    case kFxaaVtableRva: return "FXAA";
    case kImageFadeFxVtableRva: return "ImageFadeFX";
    case kVideoDistortionVtableRva: return "VideoDistortion";
    case kChromaticAberrationVtableRva: return "ChromaticAberration";
    case kRadialBlurVtableRva: return "RadialBlur";
    case kImageTrailVtableRva: return "ImageTrail";
    default: return "Unknown";
    }
}

bool FindPostEffectPriority(void* composite, void* effect, int32_t& priority)
{
    void* head = nullptr;
    void* root = nullptr;
    if (!ReadField(composite, 0x328, head) || head == nullptr || !ReadField(head, 0x8, root)) {
        return false;
    }

    std::vector<void*> pending{root};
    std::vector<void*> visited;
    while (!pending.empty() && visited.size() < 128) {
        void* node = pending.back();
        pending.pop_back();
        if (node == nullptr || node == head || std::find(visited.begin(), visited.end(), node) != visited.end()) {
            continue;
        }
        uint8_t isNil = 1;
        if (!ReadField(node, 0x29, isNil) || isNil != 0) {
            continue;
        }
        visited.push_back(node);
        void* nodeEffect = nullptr;
        if (ReadField(node, 0x20, nodeEffect) && nodeEffect == effect && ReadField(node, 0x18, priority)) {
            return true;
        }
        void* left = nullptr;
        void* right = nullptr;
        if (ReadField(node, 0, left)) pending.push_back(left);
        if (ReadField(node, 0x10, right)) pending.push_back(right);
    }
    return false;
}

bool ShouldSuppressPostEffect(const char* name)
{
    const bool suppressImageTrail = g_config.hplPostEffectDisableImageTrail
        || (g_config.hplPerEyeImageTrailControl && !IsHPLPerEyeImageTrailAvailable());
    return (suppressImageTrail && std::strcmp(name, "ImageTrail") == 0)
        || (g_config.hplPostEffectDisableVideoDistortion && std::strcmp(name, "VideoDistortion") == 0)
        || (g_config.hplPostEffectDisableChromaticAberration && std::strcmp(name, "ChromaticAberration") == 0)
        || (g_config.hplPostEffectDisableRadialBlur && std::strcmp(name, "RadialBlur") == 0);
}

std::vector<void*> CollectActivePostEffects(void* composite)
{
    std::vector<void*> effects;
    void** begin = nullptr;
    void** end = nullptr;
    size_t count = 0;
    if (!ReadField(composite, 0x340, begin)
        || !ReadField(composite, 0x348, end)
        || begin == nullptr
        || reinterpret_cast<uintptr_t>(end) < reinterpret_cast<uintptr_t>(begin)
        || (reinterpret_cast<uintptr_t>(end) - reinterpret_cast<uintptr_t>(begin)) % sizeof(void*) != 0) {
        return effects;
    }
    count = (reinterpret_cast<uintptr_t>(end) - reinterpret_cast<uintptr_t>(begin)) / sizeof(void*);
    if (count > 64 || !IsReadable(begin, count * sizeof(void*))) {
        return effects;
    }

    for (size_t i = 0; i < count; ++i) {
        void* effect = begin[i];
        uint8_t disabled = 0;
        uint8_t active = 0;
        if (effect != nullptr
            && ReadField(effect, 0x30, disabled)
            && ReadField(effect, 0x31, active)
            && disabled == 0
            && active != 0) {
            effects.push_back(effect);
        }
    }
    return effects;
}

void CyclePostEffectIsolation(void* composite)
{
    const std::vector<void*> effects = CollectActivePostEffects(composite);
    if (effects.empty()) {
        g_postEffectIsolated.store(nullptr, std::memory_order_relaxed);
        Logger::Instance().Write(LogLevel::Warn, "hpl_post_effect_isolation key=Ctrl+F12 result=no_active_effects");
        return;
    }

    void* current = g_postEffectIsolated.load(std::memory_order_relaxed);
    auto it = std::find(effects.begin(), effects.end(), current);
    void* selected = it == effects.end() || ++it == effects.end() ? effects.front() : *it;
    g_postEffectIsolated.store(selected, std::memory_order_relaxed);

    void* vtable = nullptr;
    ReadField(selected, 0, vtable);
    const uintptr_t vtableRva = reinterpret_cast<uintptr_t>(vtable) >= g_executableBase
        ? reinterpret_cast<uintptr_t>(vtable) - g_executableBase
        : 0;
    Logger::Instance().Write(
        LogLevel::Warn,
        "hpl_post_effect_isolation key=Ctrl+F12 selected=%p name=%s vtable=%p vtableRva=0x%llx activeCount=%llu",
        selected,
        PostEffectName(selected),
        vtable,
        static_cast<unsigned long long>(vtableRva),
        static_cast<unsigned long long>(effects.size()));
}

std::vector<std::pair<void*, uint8_t>> ApplyPostEffectIsolation(void* composite)
{
    std::vector<std::pair<void*, uint8_t>> patches;
    void* selected = g_postEffectIsolated.load(std::memory_order_relaxed);
    if (selected == nullptr) {
        return patches;
    }

    const std::vector<void*> effects = CollectActivePostEffects(composite);
    if (std::find(effects.begin(), effects.end(), selected) == effects.end()) {
        g_postEffectIsolated.store(nullptr, std::memory_order_relaxed);
        return patches;
    }

    for (void* effect : effects) {
        if (effect == selected) {
            continue;
        }
        uint8_t active = 0;
        if (ReadField(effect, 0x31, active) && WriteField(effect, 0x31, uint8_t{0})) {
            patches.emplace_back(effect, active);
        }
    }
    if (!patches.empty()) {
        g_postEffectIsolationApplications.fetch_add(1, std::memory_order_relaxed);
    }
    return patches;
}

std::vector<std::pair<void*, uint8_t>> ApplyPostEffectComfortPolicy(void* composite)
{
    std::vector<std::pair<void*, uint8_t>> patches;
    const bool imageTrailFallback = g_config.hplPerEyeImageTrailControl
        && !IsHPLPerEyeImageTrailAvailable();
    if ((!g_config.hplPostEffectControl && !imageTrailFallback)
        || g_postEffectBypassEnabled.load(std::memory_order_relaxed)
        || g_postEffectIsolated.load(std::memory_order_relaxed) != nullptr) {
        return patches;
    }
    const HPLCameraBridgeStatus cameraStatus = GetHPLCameraBridgeStatus();
    if (!cameraStatus.trackingEnabled || !cameraStatus.stereoEnabled) {
        return patches;
    }

    for (void* effect : CollectActivePostEffects(composite)) {
        if (!ShouldSuppressPostEffect(PostEffectName(effect))) {
            continue;
        }
        uint8_t active = 0;
        if (ReadField(effect, 0x31, active) && active != 0 && WriteField(effect, 0x31, uint8_t{0})) {
            patches.emplace_back(effect, active);
        }
    }
    if (!patches.empty()) {
        g_postEffectComfortApplications.fetch_add(1, std::memory_order_relaxed);
        g_postEffectComfortSuppressed.fetch_add(patches.size(), std::memory_order_relaxed);
    }
    return patches;
}

void RestorePostEffectIsolation(const std::vector<std::pair<void*, uint8_t>>& patches)
{
    for (const auto& [effect, active] : patches) {
        WriteField(effect, 0x31, active);
    }
}

uint64_t HashInventoryValue(uint64_t hash, uintptr_t value)
{
    constexpr uint64_t kPrime = 1099511628211ull;
    for (size_t i = 0; i < sizeof(value); ++i) {
        hash ^= static_cast<uint8_t>(value >> (i * 8));
        hash *= kPrime;
    }
    return hash;
}

void LogPostEffectInventory(const StageSample& sample, void* composite, void* inputTexture, void* renderTarget)
{
    const std::vector<void*> effects = CollectActivePostEffects(composite);
    uint64_t signature = 1469598103934665603ull;
    std::ostringstream inventory;
    for (size_t i = 0; i < effects.size(); ++i) {
        void* vtable = nullptr;
        ReadField(effects[i], 0, vtable);
        const uintptr_t vtableRva = reinterpret_cast<uintptr_t>(vtable) >= g_executableBase
            ? reinterpret_cast<uintptr_t>(vtable) - g_executableBase
            : 0;
        signature = HashInventoryValue(signature, reinterpret_cast<uintptr_t>(effects[i]));
        signature = HashInventoryValue(signature, vtableRva);
        if (i != 0) {
            inventory << ';';
        }
        int32_t priority = 0;
        const bool priorityKnown = FindPostEffectPriority(composite, effects[i], priority);
        inventory << i << ':' << PostEffectName(effects[i]) << ':' << effects[i]
            << ":0x" << std::hex << vtableRva << std::dec
            << ":priority=";
        if (priorityKnown) inventory << priority;
        else inventory << "unknown";
    }

    const uint64_t count = g_postEffectInventorySamples.fetch_add(1, std::memory_order_relaxed) + 1;
    const bool changed = signature != g_lastPostEffectInventorySignature;
    const uint64_t interval = static_cast<uint64_t>(g_config.hplCompatibilityLogInterval);
    if (changed || count <= 2 || (interval != 0 && count % interval == 0)) {
        g_lastPostEffectInventorySignature = signature;
        Logger::Instance().Write(
            LogLevel::Info,
            "hpl_post_effect_inventory frame=%llu sequence=%llu sample=%llu changed=%d composite=%p input=%p target=%p activeCount=%llu isolated=%p effects=%s",
            static_cast<unsigned long long>(sample.frame),
            static_cast<unsigned long long>(sample.sequence),
            static_cast<unsigned long long>(count),
            changed ? 1 : 0,
            composite,
            inputTexture,
            renderTarget,
            static_cast<unsigned long long>(effects.size()),
            g_postEffectIsolated.load(std::memory_order_relaxed),
            inventory.str().c_str());
    }
}

bool InstallHook(
    HMODULE executable,
    uintptr_t rva,
    const uint8_t* signature,
    size_t signatureSize,
    const char* name,
    void* detour,
    void** original)
{
    if (!IsInsideImage(executable, rva, signatureSize)) {
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_compat_hook skipped name=%s rva=0x%llx reason=rva_outside_image",
            name,
            static_cast<unsigned long long>(rva));
        return false;
    }

    void* target = reinterpret_cast<std::byte*>(executable) + rva;
    if (!MatchBytes(target, signature, signatureSize)) {
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_compat_hook skipped name=%s rva=0x%llx reason=signature_mismatch",
            name,
            static_cast<unsigned long long>(rva));
        return false;
    }

    MH_STATUS status = MH_CreateHook(target, detour, original);
    if (status != MH_OK) {
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_compat_hook skipped name=%s rva=0x%llx reason=create_hook status=%s",
            name,
            static_cast<unsigned long long>(rva),
            MH_StatusToString(status));
        return false;
    }

    status = MH_EnableHook(target);
    if (status != MH_OK && status != MH_ERROR_ENABLED) {
        MH_RemoveHook(target);
        *original = nullptr;
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_compat_hook skipped name=%s rva=0x%llx reason=enable_hook status=%s",
            name,
            static_cast<unsigned long long>(rva),
            MH_StatusToString(status));
        return false;
    }

    g_hookTargets.push_back(target);
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_compat_hook installed name=%s rva=0x%llx target=%p",
        name,
        static_cast<unsigned long long>(rva),
        target);
    return true;
}

GLState ReadGLState()
{
    GLState state;
    if (g_glGetIntegerv == nullptr || wglGetCurrentContext() == nullptr) {
        return state;
    }

    g_glGetIntegerv(kGLDrawFramebufferBinding, &state.drawFramebuffer);
    g_glGetIntegerv(kGLReadFramebufferBinding, &state.readFramebuffer);
    g_glGetIntegerv(kGLCurrentProgram, &state.program);
    g_glGetIntegerv(kGLViewport, state.viewport);
    g_glGetIntegerv(kGLScissorBox, state.scissor);
    g_glGetIntegerv(kGLBlendSrcRgb, &state.blendFunction[0]);
    g_glGetIntegerv(kGLBlendDstRgb, &state.blendFunction[1]);
    g_glGetIntegerv(kGLBlendSrcAlpha, &state.blendFunction[2]);
    g_glGetIntegerv(kGLBlendDstAlpha, &state.blendFunction[3]);
    g_glGetIntegerv(kGLBlendEquationRgb, &state.blendEquation[0]);
    g_glGetIntegerv(kGLBlendEquationAlpha, &state.blendEquation[1]);
    if (g_glIsEnabled != nullptr) {
        state.blendEnabled = g_glIsEnabled(kGLBlend);
        state.depthTestEnabled = g_glIsEnabled(kGLDepthTest);
        state.scissorEnabled = g_glIsEnabled(kGLScissorTest);
    }
    if (g_glGetBooleanv != nullptr) {
        g_glGetBooleanv(kGLDepthWriteMask, &state.depthWrite);
        g_glGetBooleanv(kGLColorWriteMask, state.colorWrite);
    }
    state.valid = true;
    return state;
}

bool ConsumeSampleBudget(HPLRenderStage stage, uint64_t frame, uint64_t call)
{
    if (call <= 2) {
        return true;
    }

    const uint64_t interval = static_cast<uint64_t>(g_config.hplCompatibilityLogInterval);
    if (interval == 0 || frame % interval != 0) {
        return false;
    }

    std::lock_guard lock(g_sampleMutex);
    FrameSampleBudget& budget = g_sampleBudgets[StageIndex(stage)];
    if (budget.frame != frame) {
        budget.frame = frame;
        budget.count = 0;
    }
    if (budget.count >= 8) {
        return false;
    }
    ++budget.count;
    return true;
}

void FinalizeRenderTransaction()
{
    if (!g_renderTransaction.active) return;
    g_renderTransactionFrames.fetch_add(1, std::memory_order_relaxed);
    const bool canonical = g_renderTransaction.counts[StageIndex(HPLRenderStage::Viewport)] == 1
        && g_renderTransaction.counts[StageIndex(HPLRenderStage::World)] == 1
        && g_renderTransaction.counts[StageIndex(HPLRenderStage::WorldCallbacks)] == 1
        && g_renderTransaction.counts[StageIndex(HPLRenderStage::PostEffects)] == 1
        && g_renderTransaction.counts[StageIndex(HPLRenderStage::PostPostEffect)] == 1
        && g_renderTransaction.counts[StageIndex(HPLRenderStage::ScreenGui)] == 1;
    if (canonical) {
        g_canonicalRenderTransactions.fetch_add(1, std::memory_order_relaxed);
    }

    const uint64_t interval = static_cast<uint64_t>(
        std::max(g_config.hplCompatibilityLogInterval, 1));
    if (g_renderTransaction.frame <= 2 || g_renderTransaction.frame % interval == 0) {
        std::ostringstream order;
        for (size_t index = 0; index < g_renderTransaction.orderCount; ++index) {
            if (index != 0) order << '>';
            order << GetHPLRenderStageName(g_renderTransaction.order[index]);
        }
        const size_t world = StageIndex(HPLRenderStage::World);
        const size_t callbacks = StageIndex(HPLRenderStage::WorldCallbacks);
        const size_t post = StageIndex(HPLRenderStage::PostEffects);
        const size_t postPost = StageIndex(HPLRenderStage::PostPostEffect);
        const size_t gui = StageIndex(HPLRenderStage::ScreenGui);
        const bool renderOnlyCandidate = canonical
            && g_renderTransaction.sampled
            && g_renderTransaction.drawCalls[callbacks] == 0
            && g_renderTransaction.drawCalls[postPost] == 0;
        Logger::Instance().Write(
            LogLevel::Info,
            "hpl_render_transaction frame=%llu viewport=%p mask=0x%llx canonical=%d sampled=%d order=%s counts={viewport=%u world=%u callbacks=%u post=%u postPost=%u gui=%u} draws={world=%llu callbacks=%llu post=%llu postPost=%llu gui=%llu} clears={world=%llu callbacks=%llu post=%llu postPost=%llu gui=%llu} durationUs={world=%.2f callbacks=%.2f post=%.2f postPost=%.2f gui=%.2f} repeatCandidate=%s proof=callback_side_effects_still_require_controlled_replay",
            static_cast<unsigned long long>(g_renderTransaction.frame),
            g_renderTransaction.viewport,
            static_cast<unsigned long long>(g_renderTransaction.renderMask),
            canonical ? 1 : 0,
            g_renderTransaction.sampled ? 1 : 0,
            order.str().c_str(),
            g_renderTransaction.counts[StageIndex(HPLRenderStage::Viewport)],
            g_renderTransaction.counts[world],
            g_renderTransaction.counts[callbacks],
            g_renderTransaction.counts[post],
            g_renderTransaction.counts[postPost],
            g_renderTransaction.counts[gui],
            static_cast<unsigned long long>(g_renderTransaction.drawCalls[world]),
            static_cast<unsigned long long>(g_renderTransaction.drawCalls[callbacks]),
            static_cast<unsigned long long>(g_renderTransaction.drawCalls[post]),
            static_cast<unsigned long long>(g_renderTransaction.drawCalls[postPost]),
            static_cast<unsigned long long>(g_renderTransaction.drawCalls[gui]),
            static_cast<unsigned long long>(g_renderTransaction.clears[world]),
            static_cast<unsigned long long>(g_renderTransaction.clears[callbacks]),
            static_cast<unsigned long long>(g_renderTransaction.clears[post]),
            static_cast<unsigned long long>(g_renderTransaction.clears[postPost]),
            static_cast<unsigned long long>(g_renderTransaction.clears[gui]),
            static_cast<double>(g_renderTransaction.durationNanoseconds[world]) / 1000.0,
            static_cast<double>(g_renderTransaction.durationNanoseconds[callbacks]) / 1000.0,
            static_cast<double>(g_renderTransaction.durationNanoseconds[post]) / 1000.0,
            static_cast<double>(g_renderTransaction.durationNanoseconds[postPost]) / 1000.0,
            static_cast<double>(g_renderTransaction.durationNanoseconds[gui]) / 1000.0,
            renderOnlyCandidate ? "world_stage_only" : "none");
    }
    g_renderTransaction = {};
}

void RecordRenderTransactionStage(const StageSample& sample)
{
    if (!g_renderTransaction.active || g_renderTransaction.frame != sample.frame) {
        FinalizeRenderTransaction();
        g_renderTransaction = {};
        g_renderTransaction.active = true;
        g_renderTransaction.frame = sample.frame;
        g_renderTransaction.viewport = sample.viewport;
        g_renderTransaction.renderMask = sample.renderMask;
    }
    const size_t stageIndex = StageIndex(sample.stage);
    ++g_renderTransaction.counts[stageIndex];
    if (g_renderTransaction.orderCount < g_renderTransaction.order.size()) {
        g_renderTransaction.order[g_renderTransaction.orderCount++] = sample.stage;
    }
}

StageSample BeginStage(HPLRenderStage stage, void* viewport = nullptr, uint64_t renderMask = 0)
{
    StageSample sample;
    sample.stage = stage;
    sample.previousStage = g_activeStage;
    g_activeStage = stage;
    sample.frame = GetOpenGLRenderFrameHint();
    if (g_traceFrame != sample.frame) {
        g_traceFrame = sample.frame;
        g_traceSequence = 0;
    }
    sample.sequence = ++g_traceSequence;
    sample.call = g_stageCalls[StageIndex(stage)].fetch_add(1, std::memory_order_relaxed) + 1;
    sample.viewport = viewport != nullptr ? viewport : g_activeViewport;
    sample.renderMask = renderMask != 0 ? renderMask : g_activeRenderMask;
    RecordRenderTransactionStage(sample);
    sample.enabled = g_config.hplRenderStageProbe
        && ConsumeSampleBudget(stage, sample.frame, sample.call);
    sample.timingEnabled = g_config.hplPerEyePerformanceTelemetry;
    if (sample.enabled) {
        sample.before = ReadGLState();
        sample.telemetryBefore = GetOpenGLTelemetrySnapshot();
    }
    if (sample.enabled || sample.timingEnabled) {
        QueryPerformanceCounter(&sample.start);
    }
    sample.gpuTimingSlot = BeginGpuTiming(stage, sample.frame);
    return sample;
}

void EndStage(const StageSample& sample)
{
    LARGE_INTEGER end = {};
    if (sample.enabled || sample.timingEnabled) {
        QueryPerformanceCounter(&end);
    }
    const int64_t elapsedTicks = end.QuadPart - sample.start.QuadPart;
    const double durationUs = g_performanceFrequency > 0 && elapsedTicks >= 0
        ? static_cast<double>(end.QuadPart - sample.start.QuadPart) * 1000000.0
            / static_cast<double>(g_performanceFrequency)
        : 0.0;
    int stereoRenderEye = -1;
    if (sample.timingEnabled || sample.gpuTimingSlot >= 0) {
        stereoRenderEye = GetHPLCameraBridgeStatus().stereoRenderEye;
    }
    EndGpuTiming(sample.gpuTimingSlot, stereoRenderEye);
    if (sample.timingEnabled && g_performanceFrequency > 0 && elapsedTicks >= 0) {
        const size_t stageIndex = StageIndex(sample.stage);
        const size_t eyeSlot = EyeSlot(stereoRenderEye);
        const uint64_t durationNanoseconds = static_cast<uint64_t>(
            static_cast<long double>(elapsedTicks) * 1000000000.0L
            / static_cast<long double>(g_performanceFrequency));
        g_stageEyeCalls[stageIndex][eyeSlot].fetch_add(1, std::memory_order_relaxed);
        g_stageEyeCpuNanoseconds[stageIndex][eyeSlot].fetch_add(
            durationNanoseconds,
            std::memory_order_relaxed);
        const uint64_t interval = static_cast<uint64_t>(
            std::max(g_config.hplCompatibilityLogInterval, 1));
        if (sample.stage == HPLRenderStage::Viewport
            && sample.frame != 0
            && sample.frame % interval == 0) {
            LogPerEyePerformance(sample.frame, false);
        }
    }
    if (sample.stage == HPLRenderStage::Viewport && g_config.hplPerEyeGpuTelemetry) {
        PollGpuTimingResults();
        const uint64_t interval = static_cast<uint64_t>(
            std::max(g_config.hplCompatibilityLogInterval, 1));
        if (sample.frame != 0 && sample.frame % interval == 0) {
            LogPerEyeGpuPerformance(sample.frame, false);
        }
    }
    if (!sample.enabled) {
        g_activeStage = sample.previousStage;
        return;
    }
    const GLState after = ReadGLState();
    const OpenGLTelemetrySnapshot telemetryAfter = GetOpenGLTelemetrySnapshot();
    if (g_renderTransaction.active && g_renderTransaction.frame == sample.frame) {
        const size_t stageIndex = StageIndex(sample.stage);
        g_renderTransaction.sampled = true;
        g_renderTransaction.drawCalls[stageIndex] +=
            telemetryAfter.drawElements - sample.telemetryBefore.drawElements
            + telemetryAfter.drawArrays - sample.telemetryBefore.drawArrays;
        g_renderTransaction.clears[stageIndex] +=
            telemetryAfter.clears - sample.telemetryBefore.clears;
        if (g_performanceFrequency > 0 && elapsedTicks >= 0) {
            g_renderTransaction.durationNanoseconds[stageIndex] += static_cast<uint64_t>(
                static_cast<long double>(elapsedTicks) * 1000000000.0L
                / static_cast<long double>(g_performanceFrequency));
        }
    }

    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_render_stage frame=%llu sequence=%llu stage=%s call=%llu eye=%d viewport=%p mask=0x%llx durationUs=%.2f calls={drawElements=%llu drawArrays=%llu viewport=%llu framebuffer=%llu program=%llu clear=%llu} before={valid=%d drawFbo=%d readFbo=%d program=%d viewport=%d,%d,%d,%d scissor=%d,%d,%d,%d blend=%d depth=%d scissorEnabled=%d depthWrite=%d colorWrite=%d,%d,%d,%d blendFunc=%d,%d,%d,%d blendEq=%d,%d} after={valid=%d drawFbo=%d readFbo=%d program=%d viewport=%d,%d,%d,%d scissor=%d,%d,%d,%d blend=%d depth=%d scissorEnabled=%d depthWrite=%d colorWrite=%d,%d,%d,%d blendFunc=%d,%d,%d,%d blendEq=%d,%d}",
        static_cast<unsigned long long>(sample.frame),
        static_cast<unsigned long long>(sample.sequence),
        GetHPLRenderStageName(sample.stage),
        static_cast<unsigned long long>(sample.call),
        GetHPLCameraBridgeStatus().stereoRenderEye,
        sample.viewport,
        static_cast<unsigned long long>(sample.renderMask),
        durationUs,
        static_cast<unsigned long long>(telemetryAfter.drawElements - sample.telemetryBefore.drawElements),
        static_cast<unsigned long long>(telemetryAfter.drawArrays - sample.telemetryBefore.drawArrays),
        static_cast<unsigned long long>(telemetryAfter.viewportCalls - sample.telemetryBefore.viewportCalls),
        static_cast<unsigned long long>(telemetryAfter.framebufferBinds - sample.telemetryBefore.framebufferBinds),
        static_cast<unsigned long long>(telemetryAfter.programUses - sample.telemetryBefore.programUses),
        static_cast<unsigned long long>(telemetryAfter.clears - sample.telemetryBefore.clears),
        sample.before.valid ? 1 : 0,
        sample.before.drawFramebuffer,
        sample.before.readFramebuffer,
        sample.before.program,
        sample.before.viewport[0],
        sample.before.viewport[1],
        sample.before.viewport[2],
        sample.before.viewport[3],
        sample.before.scissor[0], sample.before.scissor[1], sample.before.scissor[2], sample.before.scissor[3],
        sample.before.blendEnabled, sample.before.depthTestEnabled, sample.before.scissorEnabled,
        sample.before.depthWrite,
        sample.before.colorWrite[0], sample.before.colorWrite[1], sample.before.colorWrite[2], sample.before.colorWrite[3],
        sample.before.blendFunction[0], sample.before.blendFunction[1], sample.before.blendFunction[2], sample.before.blendFunction[3],
        sample.before.blendEquation[0], sample.before.blendEquation[1],
        after.valid ? 1 : 0,
        after.drawFramebuffer,
        after.readFramebuffer,
        after.program,
        after.viewport[0],
        after.viewport[1],
        after.viewport[2],
        after.viewport[3],
        after.scissor[0], after.scissor[1], after.scissor[2], after.scissor[3],
        after.blendEnabled, after.depthTestEnabled, after.scissorEnabled,
        after.depthWrite,
        after.colorWrite[0], after.colorWrite[1], after.colorWrite[2], after.colorWrite[3],
        after.blendFunction[0], after.blendFunction[1], after.blendFunction[2], after.blendFunction[3],
        after.blendEquation[0], after.blendEquation[1]);
    g_activeStage = sample.previousStage;
}

Vector3 ReadVector(const void* object, size_t offset)
{
    Vector3 value;
    if (object != nullptr) {
        std::memcpy(&value, static_cast<const std::byte*>(object) + offset, sizeof(value));
    }
    return value;
}

void WriteVector(void* object, size_t offset, const Vector3& value)
{
    if (object != nullptr) {
        std::memcpy(static_cast<std::byte*>(object) + offset, &value, sizeof(value));
    }
}

bool IsFinite(const Vector3& value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

void HookRenderViewport(void* scene, void* viewport, float frameTime, uint64_t renderMask)
{
    PollDualRenderReplayHotkey();
    void* previousViewport = g_activeViewport;
    const uint64_t previousMask = g_activeRenderMask;
    g_activeViewport = viewport;
    g_activeRenderMask = renderMask;
    ObserveViewportIdentity(viewport, renderMask);
    PollDualRenderAutoProbe(viewport);
    const bool exactPlayerViewport = IsExactPlayerViewport(viewport);
    const bool armedReplayRequested = exactPlayerViewport
        && g_dualRenderArmed.load(std::memory_order_relaxed);
    const bool continuousReplayRequested = exactPlayerViewport
        && IsHPLContinuousDualRenderEnabled();
    bool stereoHistoryRequested = false;
    if (exactPlayerViewport) {
        const HPLCameraBridgeStatus cameraBeforeRender = GetHPLCameraBridgeStatus();
        stereoHistoryRequested = cameraBeforeRender.trackingEnabled
            && cameraBeforeRender.stereoEnabled;
        SetHPLPerEyeViewHistoryActive(
            stereoHistoryRequested,
            "exact_player_viewport");
    }
    const bool dualRenderRequested = armedReplayRequested || continuousReplayRequested;
    const DualRenderArmSource armSource = armedReplayRequested
        ? g_dualRenderArmSource.exchange(DualRenderArmSource::None, std::memory_order_relaxed)
        : continuousReplayRequested ? DualRenderArmSource::Continuous : DualRenderArmSource::None;
    const uint64_t attempt = dualRenderRequested
        ? g_dualRenderAttempts.fetch_add(1, std::memory_order_relaxed) + 1
        : 0;
    if (armedReplayRequested) {
        g_dualRenderArmed.store(false, std::memory_order_relaxed);
    }
    const bool diagnosticReplay = dualRenderRequested
        && armSource != DualRenderArmSource::Continuous;
    const bool previousForceResourceCapture = g_forcePostEffectResourceCapture;
    g_forcePostEffectResourceCapture = previousForceResourceCapture || diagnosticReplay;
    g_dualRenderPass = dualRenderRequested
        ? HPLDualRenderPass::FirstEye
        : HPLDualRenderPass::None;
    g_dualRenderAttempt = diagnosticReplay ? attempt : 0;

    HPLPendingStereoRenderTarget firstHistoryTarget;
    if (stereoHistoryRequested
        && GetHPLPendingStereoRenderTarget(firstHistoryTarget)) {
        BeginHPLPerEyeViewHistoryPass(
            firstHistoryTarget.eyeIndex,
            firstHistoryTarget.poseFrame,
            firstHistoryTarget.calibrationGeneration);
    }

    const StageSample sample = BeginStage(HPLRenderStage::Viewport, viewport, renderMask);
    g_originalRenderViewport(scene, viewport, frameTime, renderMask);
    EndStage(sample);
    const HPLCameraBridgeStatus firstRenderedStatus = GetHPLCameraBridgeStatus();
    EndHPLPerEyeViewHistoryPass(
        firstRenderedStatus.stereoRenderEye, firstRenderedStatus.stereoRenderPoseFrame);
    g_dualRenderPass = HPLDualRenderPass::None;

    if (dualRenderRequested) {
        const uint64_t frame = GetOpenGLRenderFrameHint();
        const HPLCameraBridgeStatus firstStatus = GetHPLCameraBridgeStatus();
        const bool eligible = dual_render_math::IsReplayEligible(
            diagnosticReplay ? g_config.hplDualRenderReplayProbe : continuousReplayRequested,
            true,
            firstStatus.trackingEnabled,
            firstStatus.stereoEnabled,
            g_openxr != nullptr,
            firstStatus.stereoRenderEye,
            firstStatus.stereoRenderPoseFrame,
            renderMask);
        if (!eligible) {
            if (armSource == DualRenderArmSource::Continuous) {
                g_dualRenderContinuousSkips.fetch_add(1, std::memory_order_relaxed);
            } else {
                g_dualRenderFailures.fetch_add(1, std::memory_order_relaxed);
            }
            if (ShouldLogDualRenderEvent(armSource, attempt)) {
                Logger::Instance().Write(
                    LogLevel::Warn,
                    "hpl_dual_render skipped attempt=%llu source=%s frame=%llu reason=eligibility tracking=%d stereo=%d runtime=%d firstEye=%d poseFrame=%llu mask=0x%llx fallback=native_afr",
                    static_cast<unsigned long long>(attempt),
                    DualRenderArmSourceName(armSource),
                    static_cast<unsigned long long>(frame),
                    firstStatus.trackingEnabled ? 1 : 0,
                    firstStatus.stereoEnabled ? 1 : 0,
                    g_openxr != nullptr ? 1 : 0,
                    firstStatus.stereoRenderEye,
                    static_cast<unsigned long long>(firstStatus.stereoRenderPoseFrame),
                    static_cast<unsigned long long>(renderMask));
            }
        } else if (!g_openxr->CapturePendingStereoEye(frame, "dual_render_first_eye")) {
            g_dualRenderFailures.fetch_add(1, std::memory_order_relaxed);
            Logger::Instance().Write(
                LogLevel::Warn,
                "hpl_dual_render skipped attempt=%llu source=%s frame=%llu reason=first_eye_capture_failed firstEye=%d poseFrame=%llu fallback=native_afr",
                static_cast<unsigned long long>(attempt),
                DualRenderArmSourceName(armSource),
                static_cast<unsigned long long>(frame),
                firstStatus.stereoRenderEye,
                static_cast<unsigned long long>(firstStatus.stereoRenderPoseFrame));
            if (armSource == DualRenderArmSource::Continuous) {
                SetHPLContinuousDualRenderEnabled(false, "first_eye_capture_failed");
            }
        } else {
            g_dualRenderFirstEyeCaptures.fetch_add(1, std::memory_order_relaxed);
            const uint64_t replayMask = dual_render_math::BuildReplayMask(renderMask);
            const OpenGLTelemetrySnapshot telemetryBefore = GetOpenGLTelemetrySnapshot();
            LARGE_INTEGER replayStart = {};
            LARGE_INTEGER replayEnd = {};
            QueryPerformanceCounter(&replayStart);
            g_activeRenderMask = replayMask;
            g_dualRenderPass = HPLDualRenderPass::ReplayEye;
            HPLPendingStereoRenderTarget replayHistoryTarget;
            if (continuousReplayRequested
                && GetHPLPendingStereoRenderTarget(replayHistoryTarget)) {
                BeginHPLPerEyeViewHistoryPass(
                    replayHistoryTarget.eyeIndex,
                    replayHistoryTarget.poseFrame,
                    replayHistoryTarget.calibrationGeneration);
            }
            const StageSample replaySample = BeginStage(
                HPLRenderStage::Viewport, viewport, replayMask);
            g_originalRenderViewport(scene, viewport, frameTime, replayMask);
            EndStage(replaySample);
            const HPLCameraBridgeStatus replayRenderedStatus = GetHPLCameraBridgeStatus();
            EndHPLPerEyeViewHistoryPass(
                replayRenderedStatus.stereoRenderEye,
                replayRenderedStatus.stereoRenderPoseFrame);
            g_dualRenderPass = HPLDualRenderPass::None;
            g_activeRenderMask = renderMask;
            QueryPerformanceCounter(&replayEnd);
            const OpenGLTelemetrySnapshot telemetryAfter = GetOpenGLTelemetrySnapshot();
            const HPLCameraBridgeStatus secondStatus = GetHPLCameraBridgeStatus();
            const bool oppositeEye = dual_render_math::IsSamePoseOppositeEye(
                firstStatus.stereoRenderEye,
                firstStatus.stereoRenderPoseFrame,
                secondStatus.stereoRenderEye,
                secondStatus.stereoRenderPoseFrame);
            const uint64_t replay = g_dualRenderReplays.fetch_add(1, std::memory_order_relaxed) + 1;
            if (oppositeEye) {
                g_dualRenderSamePoseOppositeEye.fetch_add(1, std::memory_order_relaxed);
            } else {
                g_dualRenderFailures.fetch_add(1, std::memory_order_relaxed);
                g_openxr->InvalidateStereoCaches("dual_render_eye_sequence_mismatch");
                if (armSource == DualRenderArmSource::Continuous) {
                    SetHPLContinuousDualRenderEnabled(false, "eye_sequence_mismatch");
                }
            }
            const uint64_t continuousReplay = armSource == DualRenderArmSource::Continuous
                ? g_dualRenderContinuousReplays.fetch_add(1, std::memory_order_relaxed) + 1
                : 0;
            const double durationUs = g_performanceFrequency > 0
                ? static_cast<double>(replayEnd.QuadPart - replayStart.QuadPart) * 1000000.0
                    / static_cast<double>(g_performanceFrequency)
                : 0.0;
            if (!oppositeEye || ShouldLogDualRenderEvent(armSource, continuousReplay)) {
                Logger::Instance().Write(
                    oppositeEye ? LogLevel::Warn : LogLevel::Error,
                    "hpl_dual_render replay=%llu attempt=%llu source=%s continuousReplay=%llu frame=%llu result=%s firstEye=%d secondEye=%d firstPoseFrame=%llu secondPoseFrame=%llu originalMask=0x%llx replayMask=0x%llx screenGuiSuppressed=1 postPostPhaseDuplicated=1 temporalDiagnostics=%d durationUs=%.2f draws=%llu clears=%llu secondEyePendingForFrameBoundary=%d cachesInvalidated=%d",
                    static_cast<unsigned long long>(replay),
                    static_cast<unsigned long long>(attempt),
                    DualRenderArmSourceName(armSource),
                    static_cast<unsigned long long>(continuousReplay),
                    static_cast<unsigned long long>(frame),
                    oppositeEye ? "same_pose_opposite_eye" : "eye_sequence_mismatch",
                    firstStatus.stereoRenderEye,
                    secondStatus.stereoRenderEye,
                    static_cast<unsigned long long>(firstStatus.stereoRenderPoseFrame),
                    static_cast<unsigned long long>(secondStatus.stereoRenderPoseFrame),
                    static_cast<unsigned long long>(renderMask),
                    static_cast<unsigned long long>(replayMask),
                    diagnosticReplay ? 1 : 0,
                    durationUs,
                    static_cast<unsigned long long>(
                        telemetryAfter.drawElements - telemetryBefore.drawElements
                        + telemetryAfter.drawArrays - telemetryBefore.drawArrays),
                    static_cast<unsigned long long>(telemetryAfter.clears - telemetryBefore.clears),
                    oppositeEye ? 1 : 0,
                    oppositeEye ? 0 : 1);
            }
        }
    }

    g_activeViewport = previousViewport;
    g_activeRenderMask = previousMask;
    g_forcePostEffectResourceCapture = previousForceResourceCapture;
    g_dualRenderPass = HPLDualRenderPass::None;
    g_dualRenderAttempt = 0;
}

void HookRenderWorld(
    void* renderer,
    float frameTime,
    void* frustum,
    void* world,
    void* settings,
    void* renderTarget,
    bool sendToPostEffects,
    void* callbacks)
{
    const StageSample sample = BeginStage(HPLRenderStage::World);
    g_originalRenderWorld(
        renderer,
        frameTime,
        frustum,
        world,
        settings,
        renderTarget,
        sendToPostEffects,
        callbacks);
    EndStage(sample);
}

void HookRenderWorldCallbacks(void* scene, void* viewport, void* frustum, float frameTime)
{
    const StageSample sample = BeginStage(HPLRenderStage::WorldCallbacks, viewport);
    g_originalRenderWorldCallbacks(scene, viewport, frustum, frameTime);
    EndStage(sample);
}

void HookRenderPostEffects(
    void* composite,
    float frameTime,
    void* frustum,
    void* inputTexture,
    void* renderTarget)
{
    const StageSample sample = BeginStage(HPLRenderStage::PostEffects);
    LogPostEffectInventory(sample, composite, inputTexture, renderTarget);
    std::vector<std::pair<void*, uint8_t>> patches = ApplyPostEffectIsolation(composite);
    if (patches.empty() && g_postEffectIsolated.load(std::memory_order_relaxed) == nullptr) {
        patches = ApplyPostEffectComfortPolicy(composite);
    }
    g_originalRenderPostEffects(composite, frameTime, frustum, inputTexture, renderTarget);
    RestorePostEffectIsolation(patches);
    EndStage(sample);
}

void* HookRenderPostEffectOne(
    void* effect,
    void* composite,
    void* inputTexture,
    void* renderTarget,
    bool lastEffect)
{
    g_postEffectRenderOneCalls.fetch_add(1, std::memory_order_relaxed);
    const HPLCameraBridgeStatus camera = GetHPLCameraBridgeStatus();
    const bool imageTrail = std::strcmp(PostEffectName(effect), "ImageTrail") == 0;
    const bool toneMapping = std::strcmp(PostEffectName(effect), "ToneMapping") == 0;
    const bool stereoEligible = camera.trackingEnabled
        && camera.stereoEnabled
        && IsExactPlayerViewport(g_activeViewport);
    BeginHPLPerEyePostEffect(
        effect,
        imageTrail,
        stereoEligible,
        camera.stereoRenderEye,
        camera.stereoRenderPoseFrame,
        camera.calibrationGeneration);
    BeginHPLToneMappingFrame(
        effect,
        toneMapping,
        stereoEligible,
        camera.stereoRenderEye,
        camera.stereoRenderPoseFrame,
        camera.calibrationGeneration);
    BeginPostEffectResourceCapture(
        g_traceFrame != UINT64_MAX ? g_traceFrame : GetOpenGLRenderFrameHint(),
        g_traceSequence,
        camera.stereoRenderEye,
        camera.stereoRenderPoseFrame,
        PostEffectName(effect),
        effect,
        inputTexture,
        renderTarget,
        lastEffect,
        g_forcePostEffectResourceCapture);
    void* outputTexture = g_originalRenderPostEffectOne(
        effect, composite, inputTexture, renderTarget, lastEffect);
    EndPostEffectResourceCapture(outputTexture);
    EndHPLToneMappingFrame(effect);
    EndHPLPerEyePostEffect(effect);
    return outputTexture;
}

void HookImageTrailDestroyResources(void* effect)
{
    DestroyHPLPerEyeImageTrail(effect);
}

void HookSSAORender(void* renderer)
{
    void* nativeHistoryTexture = nullptr;
    void* settings = nullptr;
    void* temporalProgram = nullptr;
    uint8_t ssaoEnabled = 0;
    if (renderer != nullptr) {
        std::memcpy(&nativeHistoryTexture, static_cast<uint8_t*>(renderer) + 0xe78, sizeof(nativeHistoryTexture));
        std::memcpy(&settings, static_cast<uint8_t*>(renderer) + 0x438, sizeof(settings));
        std::memcpy(&temporalProgram, static_cast<uint8_t*>(renderer) + 0xf48, sizeof(temporalProgram));
        if (settings != nullptr) {
            std::memcpy(&ssaoEnabled, static_cast<uint8_t*>(settings) + 0x170, sizeof(ssaoEnabled));
        }
    }
    const HPLCameraBridgeStatus camera = GetHPLCameraBridgeStatus();
    const bool stereoEligible = ssaoEnabled != 0
        && temporalProgram != nullptr
        && camera.trackingEnabled
        && camera.stereoEnabled
        && IsExactPlayerViewport(g_activeViewport);
    BeginHPLSSAOTemporalPass(
        renderer,
        nativeHistoryTexture,
        stereoEligible,
        camera.stereoRenderEye,
        camera.stereoRenderPoseFrame,
        camera.calibrationGeneration);
    BeginHPLSSAOFrameOwner(
        renderer,
        stereoEligible,
        camera.stereoRenderEye,
        camera.stereoRenderPoseFrame,
        camera.calibrationGeneration);
    g_originalSSAORender(renderer);
    EndHPLSSAOFrameOwner(renderer);
    EndHPLSSAOTemporalPass(renderer);
}

void HookRendererSetTextureUnit(void* rendererState, uint32_t unit, void* texture)
{
    BeginHPLSSAOTemporalTextureBind(texture);
    g_originalRendererSetTextureUnit(rendererState, unit, texture);
    EndHPLSSAOTemporalTextureBind();
}

void HookRenderPostPostEffects(void* renderer, void* frustum, void* renderTarget, void* settings)
{
    ObserveHPLPerEyeViewHistoryRenderer(renderer);
    BeginHPLDualRenderTemporalCapture(
        GetOpenGLRenderFrameHint(),
        g_dualRenderAttempt,
        g_dualRenderPass,
        renderer,
        settings);
    const StageSample sample = BeginStage(HPLRenderStage::PostPostEffect);
    g_originalRenderPostPostEffects(renderer, frustum, renderTarget, settings);
    EndStage(sample);
    EndHPLDualRenderTemporalCapture();
}

void HookRenderScreenGui(void* scene, void* viewport, float frameTime)
{
    const StageSample sample = BeginStage(HPLRenderStage::ScreenGui, viewport);
    g_originalRenderScreenGui(scene, viewport, frameTime);
    EndStage(sample);
}

bool HookPostEffectHasActiveEffects(void* composite)
{
    const uint64_t call = g_postEffectQueries.fetch_add(1, std::memory_order_relaxed) + 1;
    const bool f12Down = (GetAsyncKeyState(VK_F12) & 0x8000) != 0;
    const bool previousF12Down = g_f12Down.exchange(f12Down, std::memory_order_relaxed);
    if (f12Down && !previousF12Down) {
        const bool controlDown = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
        const bool shiftDown = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
        if (controlDown) {
            g_postEffectBypassEnabled.store(false, std::memory_order_relaxed);
            CyclePostEffectIsolation(composite);
        } else if (shiftDown) {
            g_postEffectBypassEnabled.store(false, std::memory_order_relaxed);
            g_postEffectIsolated.store(nullptr, std::memory_order_relaxed);
            Logger::Instance().Write(
                LogLevel::Warn,
                "hpl_post_effect_isolation key=Shift+F12 selected=none policy=normal_chain");
        } else {
            g_postEffectIsolated.store(nullptr, std::memory_order_relaxed);
            const bool enabled = !g_postEffectBypassEnabled.load(std::memory_order_relaxed);
            g_postEffectBypassEnabled.store(enabled, std::memory_order_relaxed);
            const HPLCameraBridgeStatus cameraStatus = GetHPLCameraBridgeStatus();
            Logger::Instance().Write(
                LogLevel::Warn,
                "hpl_post_effect_bypass enabled=%d key=F12 call=%llu tracking=%d stereo=%d policy=all_active_effects",
                enabled ? 1 : 0,
                static_cast<unsigned long long>(call),
                cameraStatus.trackingEnabled ? 1 : 0,
                cameraStatus.stereoEnabled ? 1 : 0);
        }
    }

    if (g_postEffectBypassEnabled.load(std::memory_order_relaxed)) {
        g_postEffectBypasses.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    return g_originalPostEffectHasActiveEffects(composite);
}

void HookAudioListenerUpdate(void* soundSystem)
{
    const uint64_t call = g_audioCalls.fetch_add(1, std::memory_order_relaxed) + 1;
    const uint64_t interval = static_cast<uint64_t>(g_config.hplCompatibilityLogInterval);
    const bool sample = call <= 2 || (interval != 0 && call % interval == 0);

    const Vector3 position = ReadVector(soundSystem, kListenerPositionOffset);
    const Vector3 velocity = ReadVector(soundSystem, kListenerVelocityOffset);
    const Vector3 forward = ReadVector(soundSystem, kListenerForwardOffset);
    const Vector3 up = ReadVector(soundSystem, kListenerUpOffset);
    Vector3 committedPosition = position;
    Vector3 committedForward = forward;
    Vector3 committedUp = up;
    OpenXRHeadPose headPose;
    const HPLCameraBridgeStatus cameraStatus = GetHPLCameraBridgeStatus();
    if (sample) {
        if (g_openxr != nullptr) {
            g_openxr->GetLatestHeadPose(headPose);
        }
    }

    bool correctionApplied = false;
    bool translationApplied = false;
    if (g_config.hplAudioListenerCorrection
        && cameraStatus.trackingEnabled
        && cameraStatus.headWorldRotationValid) {
        const Quaternion headWorldRotation{
            cameraStatus.headWorldRotationX,
            cameraStatus.headWorldRotationY,
            cameraStatus.headWorldRotationZ,
            cameraStatus.headWorldRotationW,
        };
        committedForward = RotateVector(headWorldRotation, forward);
        committedUp = RotateVector(headWorldRotation, up);
        if (IsFinite(committedForward) && IsFinite(committedUp)) {
            WriteVector(soundSystem, kListenerForwardOffset, committedForward);
            WriteVector(soundSystem, kListenerUpOffset, committedUp);
            correctionApplied = true;
            g_audioCorrections.fetch_add(1, std::memory_order_relaxed);
        }
    }
    if (g_config.hplAudioListenerTranslation
        && cameraStatus.trackingEnabled
        && cameraStatus.headWorldPositionValid) {
        committedPosition = {
            position.x + cameraStatus.headWorldOffsetX,
            position.y + cameraStatus.headWorldOffsetY,
            position.z + cameraStatus.headWorldOffsetZ,
        };
        if (IsFinite(committedPosition)) {
            WriteVector(soundSystem, kListenerPositionOffset, committedPosition);
            translationApplied = true;
            g_audioTranslations.fetch_add(1, std::memory_order_relaxed);
        }
    }

    g_originalAudioListenerUpdate(soundSystem);

    if (correctionApplied) {
        WriteVector(soundSystem, kListenerForwardOffset, forward);
        WriteVector(soundSystem, kListenerUpOffset, up);
    }
    if (translationApplied) {
        WriteVector(soundSystem, kListenerPositionOffset, position);
    }

    if (sample) {
        g_audioPoseSamples.fetch_add(1, std::memory_order_relaxed);
        Logger::Instance().Write(
            LogLevel::Info,
            "hpl_audio_listener frame=%llu call=%llu finite=%d tracking=%d stereo=%d correction=%d translation=%d listenerPos=%.5f,%.5f,%.5f committedPos=%.5f,%.5f,%.5f headWorldOffset=%.5f,%.5f,%.5f listenerVel=%.5f,%.5f,%.5f listenerForward=%.5f,%.5f,%.5f listenerUp=%.5f,%.5f,%.5f committedForward=%.5f,%.5f,%.5f committedUp=%.5f,%.5f,%.5f headDeltaValid=%d headDeltaFrame=%llu headDeltaQuat=%.6f,%.6f,%.6f,%.6f hmdValid=%d hmdFrame=%llu hmdPos=%.5f,%.5f,%.5f hmdQuat=%.6f,%.6f,%.6f,%.6f",
            static_cast<unsigned long long>(GetOpenGLRenderFrameHint()),
            static_cast<unsigned long long>(call),
            IsFinite(position) && IsFinite(velocity) && IsFinite(forward) && IsFinite(up) ? 1 : 0,
            cameraStatus.trackingEnabled ? 1 : 0,
            cameraStatus.stereoEnabled ? 1 : 0,
            correctionApplied ? 1 : 0,
            translationApplied ? 1 : 0,
            position.x,
            position.y,
            position.z,
            committedPosition.x,
            committedPosition.y,
            committedPosition.z,
            cameraStatus.headWorldOffsetX,
            cameraStatus.headWorldOffsetY,
            cameraStatus.headWorldOffsetZ,
            velocity.x,
            velocity.y,
            velocity.z,
            forward.x,
            forward.y,
            forward.z,
            up.x,
            up.y,
            up.z,
            committedForward.x,
            committedForward.y,
            committedForward.z,
            committedUp.x,
            committedUp.y,
            committedUp.z,
            cameraStatus.headWorldRotationValid ? 1 : 0,
            static_cast<unsigned long long>(cameraStatus.headPoseFrame),
            cameraStatus.headWorldRotationX,
            cameraStatus.headWorldRotationY,
            cameraStatus.headWorldRotationZ,
            cameraStatus.headWorldRotationW,
            headPose.valid ? 1 : 0,
            static_cast<unsigned long long>(headPose.gameFrame),
            headPose.positionX,
            headPose.positionY,
            headPose.positionZ,
            headPose.orientationX,
            headPose.orientationY,
            headPose.orientationZ,
            headPose.orientationW);
    }
}

} // namespace

HPLRenderStage GetActiveHPLRenderStage()
{
    return g_activeStage;
}

const char* GetHPLRenderStageName(HPLRenderStage stage)
{
    switch (stage) {
    case HPLRenderStage::Viewport: return "viewport";
    case HPLRenderStage::World: return "world";
    case HPLRenderStage::WorldCallbacks: return "world_callbacks";
    case HPLRenderStage::PostEffects: return "post_effects";
    case HPLRenderStage::PostPostEffect: return "post_post_effect";
    case HPLRenderStage::ScreenGui: return "screen_gui";
    default: return "none";
    }
}

HPLDualRenderPass GetActiveHPLDualRenderPass()
{
    return g_dualRenderPass;
}

const char* GetHPLDualRenderPassName(HPLDualRenderPass pass)
{
    switch (pass) {
    case HPLDualRenderPass::FirstEye: return "first_eye";
    case HPLDualRenderPass::ReplayEye: return "replay_eye";
    default: return "none";
    }
}

bool InstallHPLCompatibilityProbe(const Config& config, OpenXRRuntime* openxr)
{
    std::lock_guard lock(g_installMutex);
    if (!g_hookTargets.empty()) {
        return true;
    }
    InitializeHPLDualRenderControl(config);
    InitializeHPLPerEyeViewHistory(config);
    if (!config.hplRenderStageProbe
        && !config.hplDualRenderReplayProbe
        && !config.hplDualRenderContinuousControl
        && !config.hplPerEyeViewHistoryControl
        && !config.hplPerEyeImageTrailControl
        && !config.hplPerEyePerformanceTelemetry
        && !config.hplPerEyeGpuTelemetry
        && !config.hplAudioListenerProbe
        && !config.hplPostEffectResourceProbe
        && !config.hplPostEffectControl) {
        Logger::Instance().Write(LogLevel::Info, "hpl_compat_probe install_skipped enabled=0");
        return true;
    }

    g_config = config;
    g_viewportIdentitySamples.store(0, std::memory_order_relaxed);
    g_viewportIdentityChanges.store(0, std::memory_order_relaxed);
    g_playerViewportCalls.store(0, std::memory_order_relaxed);
    g_secondaryViewportCalls.store(0, std::memory_order_relaxed);
    g_dualRenderArms.store(0, std::memory_order_relaxed);
    g_dualRenderAttempts.store(0, std::memory_order_relaxed);
    g_dualRenderFirstEyeCaptures.store(0, std::memory_order_relaxed);
    g_dualRenderReplays.store(0, std::memory_order_relaxed);
    g_dualRenderSamePoseOppositeEye.store(0, std::memory_order_relaxed);
    g_dualRenderFailures.store(0, std::memory_order_relaxed);
    g_dualRenderContinuousReplays.store(0, std::memory_order_relaxed);
    g_dualRenderContinuousSkips.store(0, std::memory_order_relaxed);
    g_dualRenderArmed.store(false, std::memory_order_relaxed);
    g_dualRenderKeyDown.store(false, std::memory_order_relaxed);
    g_dualRenderAutoCompleted.store(0, std::memory_order_relaxed);
    g_dualRenderAutoNextFrame.store(0, std::memory_order_relaxed);
    g_dualRenderArmSource.store(DualRenderArmSource::None, std::memory_order_relaxed);
    ResetHPLDualRenderDiagnostics();
    g_postEffectRenderOneCalls.store(0, std::memory_order_relaxed);
    {
        std::lock_guard identityLock(g_viewportIdentityMutex);
        g_viewportIdentities.clear();
    }
    if (g_config.hplAudioListenerCorrection && !ValidateAudioRotationMath()) {
        g_config.hplAudioListenerCorrection = false;
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_audio_listener correction_disabled reason=rotation_self_test_failed");
    }
    g_openxr = openxr;
    g_postEffectBypassEnabled.store(config.hplPostEffectBypassDefault, std::memory_order_relaxed);
    g_postEffectIsolated.store(nullptr, std::memory_order_relaxed);
    g_f12Down.store(false, std::memory_order_relaxed);
    g_lastPerformanceLogFrame.store(0, std::memory_order_relaxed);
    g_lastGpuPerformanceLogFrame.store(0, std::memory_order_relaxed);
    g_gpuTimingDropped.store(0, std::memory_order_relaxed);
    g_gpuTimingInvalid.store(0, std::memory_order_relaxed);
    LARGE_INTEGER performanceFrequency = {};
    QueryPerformanceFrequency(&performanceFrequency);
    g_performanceFrequency = performanceFrequency.QuadPart;
    for (size_t stageIndex = 0; stageIndex < kStageCount; ++stageIndex) {
        g_stageCalls[stageIndex].store(0, std::memory_order_relaxed);
        for (size_t eyeSlot = 0; eyeSlot < 3; ++eyeSlot) {
            g_stageEyeCalls[stageIndex][eyeSlot].store(0, std::memory_order_relaxed);
            g_stageEyeCpuNanoseconds[stageIndex][eyeSlot].store(0, std::memory_order_relaxed);
            g_stageEyeGpuCalls[stageIndex][eyeSlot].store(0, std::memory_order_relaxed);
            g_stageEyeGpuNanoseconds[stageIndex][eyeSlot].store(0, std::memory_order_relaxed);
        }
    }
    HMODULE executable = GetModuleHandleW(nullptr);
    g_executableBase = reinterpret_cast<uintptr_t>(executable);

    HMODULE opengl32 = GetModuleHandleW(L"opengl32.dll");
    if (opengl32 != nullptr) {
        g_glGetIntegerv = reinterpret_cast<GlGetIntegervFn>(GetProcAddress(opengl32, "glGetIntegerv"));
        g_glGetBooleanv = reinterpret_cast<GlGetBooleanvFn>(GetProcAddress(opengl32, "glGetBooleanv"));
        g_glIsEnabled = reinterpret_cast<GlIsEnabledFn>(GetProcAddress(opengl32, "glIsEnabled"));
    }

    static constexpr uint8_t kRenderViewportSignature[] = {
        0x48, 0x89, 0x5c, 0x24, 0x10, 0x44, 0x89, 0x4c, 0x24, 0x20,
        0x55, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55,
    };
    static constexpr uint8_t kRenderWorldSignature[] = {
        0x40, 0x53, 0x48, 0x83, 0xec, 0x50, 0x48, 0x8b, 0x84, 0x24,
        0x98, 0x00, 0x00, 0x00,
    };
    static constexpr uint8_t kRenderWorldCallbacksSignature[] = {
        0x40, 0x53, 0x48, 0x83, 0xec, 0x50, 0x48, 0x83, 0x7a, 0x18,
        0x00, 0x49, 0x8b, 0xd8,
    };
    static constexpr uint8_t kRenderPostEffectsSignature[] = {
        0x4d, 0x85, 0xc9, 0x0f, 0x84, 0x74, 0x01, 0x00, 0x00,
        0x48, 0x89, 0x6c, 0x24, 0x20,
    };
    static constexpr uint8_t kRenderPostEffectOneSignature[] = {
        0x48, 0x89, 0x74, 0x24, 0x20, 0x57, 0x48, 0x83, 0xec, 0x70,
        0x0f, 0xb6, 0x84, 0x24, 0xa0, 0x00, 0x00, 0x00,
    };
    static constexpr uint8_t kRenderPostPostEffectsSignature[] = {
        0x48, 0x89, 0x5c, 0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x18,
        0x48, 0x89, 0x7c, 0x24, 0x20, 0x41, 0x54,
    };
    static constexpr uint8_t kRenderScreenGuiSignature[] = {
        0x48, 0x89, 0x5c, 0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x18,
        0x55, 0x48, 0x8d, 0x6c, 0x24, 0xa9,
    };
    static constexpr uint8_t kAudioListenerSignature[] = {
        0x4c, 0x8b, 0xdc, 0x41, 0x54, 0x48, 0x81, 0xec, 0x90, 0x00,
        0x00, 0x00,
    };
    static constexpr uint8_t kPostEffectHasActiveEffectsSignature[] = {
        0x48, 0x8b, 0x91, 0x40, 0x03, 0x00, 0x00,
        0x4c, 0x8b, 0x81, 0x48, 0x03, 0x00, 0x00,
    };
    static constexpr uint8_t kImageTrailCreateResourcesSignature[] = {
        0x48, 0x89, 0x5c, 0x24, 0x10, 0x57, 0x48, 0x83, 0xec, 0x60,
        0x48, 0x8b, 0xf9, 0x48, 0x8b, 0x49, 0x20,
    };
    static constexpr uint8_t kImageTrailDestroyResourcesSignature[] = {
        0x40, 0x53, 0x48, 0x83, 0xec, 0x20, 0x48, 0x8b, 0x51, 0x58,
        0x48, 0x8b, 0xd9, 0x48, 0x8b, 0x49, 0x10,
    };
    static constexpr uint8_t kSSAORenderSignature[] = {
        0x4c, 0x8b, 0xdc, 0x55, 0x56, 0x49, 0x8d, 0xab, 0x38, 0xff,
        0xff, 0xff, 0x48, 0x81, 0xec, 0xb8, 0x01, 0x00, 0x00,
    };
    static constexpr uint8_t kRendererSetTextureUnitSignature[] = {
        0x48, 0x89, 0x5c, 0x24, 0x08, 0x48, 0x89, 0x74, 0x24, 0x10,
        0x57, 0x48, 0x83, 0xec, 0x20, 0x80, 0x79, 0x58, 0x00,
    };

    size_t installed = 0;
    const bool stageHooksEnabled = config.hplRenderStageProbe
        || config.hplDualRenderReplayProbe
        || config.hplDualRenderContinuousControl
        || config.hplPerEyeViewHistoryControl
        || config.hplPerEyeImageTrailControl
        || config.hplToneMappingFrameControl
        || config.hplPerEyeSSAOTemporalControl
        || config.hplSSAOFrameOwnerControl
        || config.hplPerEyePerformanceTelemetry
        || config.hplPerEyeGpuTelemetry;
    if (stageHooksEnabled) {
        installed += InstallHook(executable, kRenderViewportRva, kRenderViewportSignature,
            sizeof(kRenderViewportSignature), "render_viewport", reinterpret_cast<void*>(&HookRenderViewport),
            reinterpret_cast<void**>(&g_originalRenderViewport));
        installed += InstallHook(executable, kRenderWorldRva, kRenderWorldSignature,
            sizeof(kRenderWorldSignature), "render_world", reinterpret_cast<void*>(&HookRenderWorld),
            reinterpret_cast<void**>(&g_originalRenderWorld));
        installed += InstallHook(executable, kRenderWorldCallbacksRva, kRenderWorldCallbacksSignature,
            sizeof(kRenderWorldCallbacksSignature), "world_callbacks", reinterpret_cast<void*>(&HookRenderWorldCallbacks),
            reinterpret_cast<void**>(&g_originalRenderWorldCallbacks));
        installed += InstallHook(executable, kRenderPostEffectsRva, kRenderPostEffectsSignature,
            sizeof(kRenderPostEffectsSignature), "post_effects", reinterpret_cast<void*>(&HookRenderPostEffects),
            reinterpret_cast<void**>(&g_originalRenderPostEffects));
        installed += InstallHook(executable, kRenderPostPostEffectsRva, kRenderPostPostEffectsSignature,
            sizeof(kRenderPostPostEffectsSignature), "post_post_effects", reinterpret_cast<void*>(&HookRenderPostPostEffects),
            reinterpret_cast<void**>(&g_originalRenderPostPostEffects));
        installed += InstallHook(executable, kRenderScreenGuiRva, kRenderScreenGuiSignature,
            sizeof(kRenderScreenGuiSignature), "screen_gui", reinterpret_cast<void*>(&HookRenderScreenGui),
            reinterpret_cast<void**>(&g_originalRenderScreenGui));
    }
    if (config.hplAudioListenerProbe) {
        installed += InstallHook(executable, kAudioListenerUpdateRva, kAudioListenerSignature,
            sizeof(kAudioListenerSignature), "audio_listener_update", reinterpret_cast<void*>(&HookAudioListenerUpdate),
            reinterpret_cast<void**>(&g_originalAudioListenerUpdate));
    }
    if (config.hplPostEffectControl) {
        installed += InstallHook(executable, kPostEffectHasActiveEffectsRva,
            kPostEffectHasActiveEffectsSignature, sizeof(kPostEffectHasActiveEffectsSignature),
            "post_effect_has_active", reinterpret_cast<void*>(&HookPostEffectHasActiveEffects),
            reinterpret_cast<void**>(&g_originalPostEffectHasActiveEffects));
    }
    if (config.hplPostEffectResourceProbe || config.hplPerEyeImageTrailControl
        || config.hplToneMappingFrameControl) {
        installed += InstallHook(executable, kRenderPostEffectOneRva,
            kRenderPostEffectOneSignature, sizeof(kRenderPostEffectOneSignature),
            "post_effect_render_one", reinterpret_cast<void*>(&HookRenderPostEffectOne),
            reinterpret_cast<void**>(&g_originalRenderPostEffectOne));
    }
    HPLImageTrailResourceFn imageTrailCreateResources = nullptr;
    if (config.hplPerEyeImageTrailControl) {
        if (IsInsideImage(executable, kImageTrailCreateResourcesRva,
                sizeof(kImageTrailCreateResourcesSignature))
            && MatchBytes(
                reinterpret_cast<std::byte*>(executable) + kImageTrailCreateResourcesRva,
                kImageTrailCreateResourcesSignature,
                sizeof(kImageTrailCreateResourcesSignature))) {
            imageTrailCreateResources = reinterpret_cast<HPLImageTrailResourceFn>(
                reinterpret_cast<std::byte*>(executable) + kImageTrailCreateResourcesRva);
        } else {
            Logger::Instance().Write(
                LogLevel::Error,
                "hpl_per_eye_post_effect create_resources_unavailable rva=0x%llx reason=signature_mismatch fallback=named_effect_suppression",
                static_cast<unsigned long long>(kImageTrailCreateResourcesRva));
        }
        installed += InstallHook(executable, kImageTrailDestroyResourcesRva,
            kImageTrailDestroyResourcesSignature, sizeof(kImageTrailDestroyResourcesSignature),
            "image_trail_destroy_resources", reinterpret_cast<void*>(&HookImageTrailDestroyResources),
            reinterpret_cast<void**>(&g_originalImageTrailDestroyResources));
    }
    if (config.hplPerEyeSSAOTemporalControl || config.hplSSAOFrameOwnerControl) {
        installed += InstallHook(executable, kSSAORenderRva,
            kSSAORenderSignature, sizeof(kSSAORenderSignature),
            "ssao_render", reinterpret_cast<void*>(&HookSSAORender),
            reinterpret_cast<void**>(&g_originalSSAORender));
    }
    if (config.hplPerEyeSSAOTemporalControl) {
        installed += InstallHook(executable, kRendererSetTextureUnitRva,
            kRendererSetTextureUnitSignature, sizeof(kRendererSetTextureUnitSignature),
            "renderer_set_texture_unit", reinterpret_cast<void*>(&HookRendererSetTextureUnit),
            reinterpret_cast<void**>(&g_originalRendererSetTextureUnit));
    }
    InitializeHPLPerEyePostEffect(
        config,
        imageTrailCreateResources,
        g_originalImageTrailDestroyResources);
    InitializeHPLToneMappingFrame(config);
    InitializeHPLSSAOTemporalHistory(config);
    InitializeHPLSSAOFrameOwner(
        config,
        reinterpret_cast<float*>(reinterpret_cast<uint8_t*>(executable) + kSSAOTemporalPhaseRva));

    SetHPLDualRenderControlReady(
        config.hplDualRenderContinuousControl && g_originalRenderViewport != nullptr);

    Logger::Instance().Write(
        installed > 0 ? LogLevel::Warn : LogLevel::Error,
        "hpl_compat_probe install_complete renderStages=%d dualRenderReplayProbe=%d dualRenderAutoProbe=%d dualRenderAutoCount=%d dualRenderAutoDelayFrames=%d dualRenderAutoIntervalFrames=%d dualRenderKey=Ctrl+F6 continuousControl=%d continuousDefault=%d continuousReady=%d perEyeViewHistory=%d perEyeImageTrail=%d perEyeImageTrailAvailable=%d toneMappingFrameControl=%d perEyeSSAOTemporal=%d ssaoFrameOwner=%d perEyeCpu=%d perEyeGpu=%d gpuQueryPairs=%d audioListener=%d audioCorrection=%d audioTranslation=%d postEffectControl=%d postEffectResourceProbe=%d postEffectResourceRva=0x%llx postEffectBypass=%d postEffectComfort={imageTrail=%d videoDistortion=%d chromaticAberration=%d radialBlur=%d} postEffectKeys=F12,Ctrl+F12,Shift+F12 installed=%llu requested=%d logInterval=%d base=%p",
        config.hplRenderStageProbe ? 1 : 0,
        config.hplDualRenderReplayProbe ? 1 : 0,
        config.hplDualRenderAutoProbe ? 1 : 0,
        config.hplDualRenderAutoProbeCount,
        config.hplDualRenderAutoProbeDelayFrames,
        config.hplDualRenderAutoProbeIntervalFrames,
        config.hplDualRenderContinuousControl ? 1 : 0,
        config.hplDualRenderContinuousDefault ? 1 : 0,
        GetHPLDualRenderControlStatus().ready ? 1 : 0,
        config.hplPerEyeViewHistoryControl ? 1 : 0,
        config.hplPerEyeImageTrailControl ? 1 : 0,
        IsHPLPerEyeImageTrailAvailable() ? 1 : 0,
        config.hplToneMappingFrameControl ? 1 : 0,
        config.hplPerEyeSSAOTemporalControl ? 1 : 0,
        config.hplSSAOFrameOwnerControl ? 1 : 0,
        config.hplPerEyePerformanceTelemetry ? 1 : 0,
        config.hplPerEyeGpuTelemetry ? 1 : 0,
        config.hplGpuQueryPoolSize,
        config.hplAudioListenerProbe ? 1 : 0,
        g_config.hplAudioListenerCorrection ? 1 : 0,
        g_config.hplAudioListenerTranslation ? 1 : 0,
        config.hplPostEffectControl ? 1 : 0,
        config.hplPostEffectResourceProbe ? 1 : 0,
        static_cast<unsigned long long>(kRenderPostEffectOneRva),
        config.hplPostEffectBypassDefault ? 1 : 0,
        config.hplPostEffectDisableImageTrail ? 1 : 0,
        config.hplPostEffectDisableVideoDistortion ? 1 : 0,
        config.hplPostEffectDisableChromaticAberration ? 1 : 0,
        config.hplPostEffectDisableRadialBlur ? 1 : 0,
        static_cast<unsigned long long>(installed),
        (stageHooksEnabled ? 6 : 0)
            + (config.hplAudioListenerProbe ? 1 : 0)
            + (config.hplPostEffectControl ? 1 : 0)
            + ((config.hplPostEffectResourceProbe || config.hplPerEyeImageTrailControl
                || config.hplToneMappingFrameControl) ? 1 : 0)
            + (config.hplPerEyeImageTrailControl ? 1 : 0)
            + ((config.hplPerEyeSSAOTemporalControl || config.hplSSAOFrameOwnerControl) ? 1 : 0)
            + (config.hplPerEyeSSAOTemporalControl ? 1 : 0),
        config.hplCompatibilityLogInterval,
        executable);
    return installed > 0;
}

void LogHPLCompatibilityProbeSummary()
{
    LogPerEyePerformance(GetOpenGLRenderFrameHint(), true);
    LogPerEyeGpuPerformance(GetOpenGLRenderFrameHint(), true);
    size_t knownViewports = 0;
    {
        std::lock_guard lock(g_viewportIdentityMutex);
        knownViewports = g_viewportIdentities.size();
    }
    const HPLDualRenderDiagnosticsSummary temporal = GetHPLDualRenderDiagnosticsSummary();
    const HPLDualRenderControlStatus continuous = GetHPLDualRenderControlStatus();
    const HPLPerEyeViewHistoryStatus viewHistory = GetHPLPerEyeViewHistoryStatus();
    const HPLPerEyePostEffectStatus perEyePostEffect = GetHPLPerEyePostEffectStatus();
    const HPLToneMappingFrameStatus toneMapping = GetHPLToneMappingFrameStatus();
    const HPLSSAOTemporalHistoryStatus ssaoTemporal = GetHPLSSAOTemporalHistoryStatus();
    const HPLSSAOFrameOwnerStatus ssaoFrameOwner = GetHPLSSAOFrameOwnerStatus();
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_per_eye_post_effect_summary configured=%d available=%d faulted=%d effects=%llu allocations=%llu restores=%llu captures=%llu resets=%llu releases=%llu failures=%llu",
        perEyePostEffect.configured ? 1 : 0,
        perEyePostEffect.available ? 1 : 0,
        perEyePostEffect.faulted ? 1 : 0,
        static_cast<unsigned long long>(perEyePostEffect.effects),
        static_cast<unsigned long long>(perEyePostEffect.allocations),
        static_cast<unsigned long long>(perEyePostEffect.restores),
        static_cast<unsigned long long>(perEyePostEffect.captures),
        static_cast<unsigned long long>(perEyePostEffect.resets),
        static_cast<unsigned long long>(perEyePostEffect.releases),
        static_cast<unsigned long long>(perEyePostEffect.failures));
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_tone_mapping_frame_summary configured=%d active=%d faulted=%d effects=%llu firstPasses=%llu replayPasses=%llu committedRestores=%llu mismatches=%llu failures=%llu",
        toneMapping.configured ? 1 : 0,
        toneMapping.active ? 1 : 0,
        toneMapping.faulted ? 1 : 0,
        static_cast<unsigned long long>(toneMapping.trackedEffects),
        static_cast<unsigned long long>(toneMapping.firstPasses),
        static_cast<unsigned long long>(toneMapping.replayPasses),
        static_cast<unsigned long long>(toneMapping.committedRestores),
        static_cast<unsigned long long>(toneMapping.mismatches),
        static_cast<unsigned long long>(toneMapping.failures));
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_ssao_temporal_summary configured=%d available=%d faulted=%d renderers=%llu allocations=%llu restores=%llu commits=%llu seeds=%llu resets=%llu failures=%llu",
        ssaoTemporal.configured ? 1 : 0,
        ssaoTemporal.available ? 1 : 0,
        ssaoTemporal.faulted ? 1 : 0,
        static_cast<unsigned long long>(ssaoTemporal.trackedRenderers),
        static_cast<unsigned long long>(ssaoTemporal.allocations),
        static_cast<unsigned long long>(ssaoTemporal.restores),
        static_cast<unsigned long long>(ssaoTemporal.commits),
        static_cast<unsigned long long>(ssaoTemporal.seeds),
        static_cast<unsigned long long>(ssaoTemporal.resets),
        static_cast<unsigned long long>(ssaoTemporal.failures));
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_ssao_frame_owner_summary configured=%d available=%d faulted=%d renderers=%llu firstPasses=%llu replayPasses=%llu committedRestores=%llu mismatches=%llu failures=%llu",
        ssaoFrameOwner.configured ? 1 : 0,
        ssaoFrameOwner.available ? 1 : 0,
        ssaoFrameOwner.faulted ? 1 : 0,
        static_cast<unsigned long long>(ssaoFrameOwner.trackedRenderers),
        static_cast<unsigned long long>(ssaoFrameOwner.firstPasses),
        static_cast<unsigned long long>(ssaoFrameOwner.replayPasses),
        static_cast<unsigned long long>(ssaoFrameOwner.committedRestores),
        static_cast<unsigned long long>(ssaoFrameOwner.mismatches),
        static_cast<unsigned long long>(ssaoFrameOwner.failures));
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_compat_summary viewport=%llu world=%llu worldCallbacks=%llu postEffects=%llu postEffectRenderOne=%llu postPostEffects=%llu screenGui=%llu renderTransactions=%llu canonicalRenderTransactions=%llu viewportIdentitySamples=%llu viewportIdentityChanges=%llu knownViewports=%llu playerViewportCalls=%llu secondaryViewportCalls=%llu dualRender={arms=%llu attempts=%llu firstEyeCaptures=%llu replays=%llu samePoseOppositeEye=%llu failures=%llu armed=%d autoCompleted=%llu continuousConfigured=%d continuousReady=%d continuousEnabled=%d continuousDefault=%d continuousReplays=%llu continuousSkips=%llu continuousChanges=%llu continuousRejections=%llu temporalCaptures=%llu temporalFailedRegions=%llu temporalCorrelatedPairs=%llu temporalEquivalentPairs=%llu viewHistoryConfigured=%d viewHistoryActive=%d viewHistoryFaulted=%d viewHistoryActivations=%llu viewHistoryResets=%llu viewHistorySeeds=%llu viewHistoryRestores=%llu viewHistoryCaptures=%llu viewHistoryFailures=%llu} audioUpdates=%llu audioSamples=%llu audioCorrections=%llu audioTranslations=%llu postEffectQueries=%llu postEffectBypasses=%llu postEffectInventorySamples=%llu postEffectIsolationApplications=%llu postEffectComfortApplications=%llu postEffectComfortSuppressed=%llu postEffectBypassEnabled=%d postEffectIsolated=%p installedHooks=%llu",
        static_cast<unsigned long long>(g_stageCalls[StageIndex(HPLRenderStage::Viewport)].load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_stageCalls[StageIndex(HPLRenderStage::World)].load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_stageCalls[StageIndex(HPLRenderStage::WorldCallbacks)].load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_stageCalls[StageIndex(HPLRenderStage::PostEffects)].load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_postEffectRenderOneCalls.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_stageCalls[StageIndex(HPLRenderStage::PostPostEffect)].load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_stageCalls[StageIndex(HPLRenderStage::ScreenGui)].load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_renderTransactionFrames.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_canonicalRenderTransactions.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_viewportIdentitySamples.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_viewportIdentityChanges.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(knownViewports),
        static_cast<unsigned long long>(g_playerViewportCalls.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_secondaryViewportCalls.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_dualRenderArms.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_dualRenderAttempts.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_dualRenderFirstEyeCaptures.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_dualRenderReplays.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_dualRenderSamePoseOppositeEye.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_dualRenderFailures.load(std::memory_order_relaxed)),
        g_dualRenderArmed.load(std::memory_order_relaxed) ? 1 : 0,
        static_cast<unsigned long long>(g_dualRenderAutoCompleted.load(std::memory_order_relaxed)),
        continuous.configured ? 1 : 0,
        continuous.ready ? 1 : 0,
        continuous.enabled ? 1 : 0,
        continuous.defaultEnabled ? 1 : 0,
        static_cast<unsigned long long>(g_dualRenderContinuousReplays.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_dualRenderContinuousSkips.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(continuous.changes),
        static_cast<unsigned long long>(continuous.rejections),
        static_cast<unsigned long long>(temporal.captures),
        static_cast<unsigned long long>(temporal.failedRegions),
        static_cast<unsigned long long>(temporal.correlatedPairs),
        static_cast<unsigned long long>(temporal.equivalentPairs),
        viewHistory.configured ? 1 : 0,
        viewHistory.active ? 1 : 0,
        viewHistory.faulted ? 1 : 0,
        static_cast<unsigned long long>(viewHistory.activations),
        static_cast<unsigned long long>(viewHistory.resets),
        static_cast<unsigned long long>(viewHistory.seeds),
        static_cast<unsigned long long>(viewHistory.restores),
        static_cast<unsigned long long>(viewHistory.captures),
        static_cast<unsigned long long>(viewHistory.failures),
        static_cast<unsigned long long>(g_audioCalls.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_audioPoseSamples.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_audioCorrections.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_audioTranslations.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_postEffectQueries.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_postEffectBypasses.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_postEffectInventorySamples.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_postEffectIsolationApplications.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_postEffectComfortApplications.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_postEffectComfortSuppressed.load(std::memory_order_relaxed)),
        g_postEffectBypassEnabled.load(std::memory_order_relaxed) ? 1 : 0,
        g_postEffectIsolated.load(std::memory_order_relaxed),
        static_cast<unsigned long long>(g_hookTargets.size()));
}

void RemoveHPLCompatibilityProbe()
{
    std::lock_guard lock(g_installMutex);
    ResetGpuTimingState(wglGetCurrentContext() == g_gpuTimingContext);
    g_gpuTimingUnavailableContext = nullptr;
    RemoveHPLSSAOFrameOwner();
    RemoveHPLSSAOTemporalHistory();
    RemoveHPLToneMappingFrame();
    RemoveHPLPerEyePostEffect();
    for (void* target : g_hookTargets) {
        MH_DisableHook(target);
        MH_RemoveHook(target);
    }
    g_hookTargets.clear();
    g_originalRenderViewport = nullptr;
    g_originalRenderWorld = nullptr;
    g_originalRenderWorldCallbacks = nullptr;
    g_originalRenderPostEffects = nullptr;
    g_originalRenderPostEffectOne = nullptr;
    g_originalRenderPostPostEffects = nullptr;
    g_originalRenderScreenGui = nullptr;
    g_originalPostEffectHasActiveEffects = nullptr;
    g_originalAudioListenerUpdate = nullptr;
    g_originalImageTrailDestroyResources = nullptr;
    g_originalSSAORender = nullptr;
    g_originalRendererSetTextureUnit = nullptr;
    g_postEffectBypassEnabled.store(false, std::memory_order_relaxed);
    g_postEffectIsolated.store(nullptr, std::memory_order_relaxed);
    g_f12Down.store(false, std::memory_order_relaxed);
    g_dualRenderArmed.store(false, std::memory_order_relaxed);
    g_dualRenderKeyDown.store(false, std::memory_order_relaxed);
    g_dualRenderArmSource.store(DualRenderArmSource::None, std::memory_order_relaxed);
    g_dualRenderAutoNextFrame.store(0, std::memory_order_relaxed);
    RemoveHPLDualRenderControl();
    RemoveHPLPerEyeViewHistory();
    ResetHPLDualRenderDiagnostics();
    g_openxr = nullptr;
    g_glGetIntegerv = nullptr;
    g_glGetBooleanv = nullptr;
    g_glIsEnabled = nullptr;
    g_lastPostEffectInventorySignature = 0;
    {
        std::lock_guard identityLock(g_viewportIdentityMutex);
        g_viewportIdentities.clear();
    }
    g_executableBase = 0;
    Logger::Instance().Write(LogLevel::Info, "hpl_compat_probe removed");
}

} // namespace somavr
