#include "OpenGLHooks.h"

#include "HPLCameraBridge.h"
#include "HPLCompatibilityProbe.h"
#include "HPLInputBridge.h"
#include "HPLHandsBridge.h"
#include "HPLPostEffectResourceMath.h"
#include "HPLPresentationBridge.h"
#include "HPLSSAOTemporalHistory.h"
#include "HPLTerminalMath.h"
#include "HPLPlayerState.h"
#include "Logger.h"
#include "OpenGLMatrixAnalysis.h"
#include "OpenGLOwnership.h"

#include <Windows.h>
#include <gl/GL.h>

#include <MinHook.h>

#include <algorithm>
#include <atomic>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <intrin.h>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#ifndef GLchar
using GLchar = char;
#endif

namespace somavr {
namespace {

using gl_matrix::MatrixSummary;
using gl_matrix::MatrixSummaryText;
using gl_matrix::MatrixValuesText;
using gl_matrix::SummarizeMatrix;

constexpr GLenum kGLMatrixMode = 0x0BA0;
constexpr GLenum kGLModelView = 0x1700;
constexpr GLenum kGLProjection = 0x1701;
constexpr GLenum kGLViewport = 0x0BA2;
constexpr GLenum kGLModelViewMatrix = 0x0BA6;
constexpr GLenum kGLProjectionMatrix = 0x0BA7;
constexpr GLenum kGLVendor = 0x1F00;
constexpr GLenum kGLRenderer = 0x1F01;
constexpr GLenum kGLVersion = 0x1F02;
constexpr GLenum kGLShadingLanguageVersion = 0x8B8C;
constexpr GLenum kGLCurrentProgram = 0x8B8D;
constexpr GLenum kGLFramebufferBinding = 0x8CA6;
constexpr GLenum kGLScissorBox = 0x0C10;
constexpr GLenum kGLTextureBinding2D = 0x8069;
constexpr GLenum kGLBlendSrcRgb = 0x80C9;
constexpr GLenum kGLBlendDstRgb = 0x80C8;
constexpr GLenum kGLActiveUniforms = 0x8B86;
constexpr GLenum kGLActiveUniformMaxLength = 0x8B87;
constexpr GLenum kGLAttachedShaders = 0x8B85;
constexpr GLenum kGLShaderType = 0x8B4F;
constexpr GLenum kGLShaderSourceLength = 0x8B88;
constexpr GLenum kGLActiveUniformBlocks = 0x8A36;
constexpr GLenum kGLUniformBlockBinding = 0x8A3F;
constexpr GLenum kGLUniformBlockDataSize = 0x8A40;
constexpr GLenum kGLUniformBlockNameLength = 0x8A41;
constexpr GLenum kGLUniformBlockActiveUniforms = 0x8A42;
constexpr GLenum kGLUniformBlockIndex = 0x8A3A;
constexpr GLenum kGLUniformOffset = 0x8A3B;
constexpr GLenum kGLUniformArrayStride = 0x8A3C;
constexpr GLenum kGLUniformMatrixStride = 0x8A3D;
constexpr GLenum kGLUniformIsRowMajor = 0x8A3E;
constexpr GLenum kGLUniformBufferBinding = 0x8A28;
constexpr GLenum kGLUniformBufferStart = 0x8A29;
constexpr GLenum kGLUniformBufferSize = 0x8A2A;
constexpr GLenum kGLTextureWidth = 0x1000;
constexpr GLenum kGLTextureHeight = 0x1001;
constexpr GLenum kGLTextureInternalFormat = 0x1003;
constexpr GLenum kGLTextureDepth = 0x8071;
constexpr GLenum kGLTextureCubeMap = 0x8513;
constexpr GLenum kGLTextureCubeMapPositiveX = 0x8515;
constexpr GLenum kGLQueryResult = 0x8866;
constexpr GLenum kGLQueryResultAvailable = 0x8867;
constexpr size_t kMaxOcclusionQueryStates = 4096;
constexpr size_t kMaxFramebufferCopyStates = 4096;

using SwapBuffersFn = BOOL(WINAPI*)(HDC);
using WglMakeCurrentFn = BOOL(WINAPI*)(HDC, HGLRC);
using WglGetProcAddressFn = PROC(WINAPI*)(LPCSTR);
using WglSwapIntervalEXTFn = BOOL(WINAPI*)(int);

using GlGetStringFn = const GLubyte*(APIENTRY*)(GLenum);
using GlGetIntegervFn = void(APIENTRY*)(GLenum, GLint*);
using GlGetFloatvFn = void(APIENTRY*)(GLenum, GLfloat*);
using GlGetTexLevelParameterivFn = void(APIENTRY*)(GLenum, GLint, GLenum, GLint*);
using GlBindTextureFn = void(APIENTRY*)(GLenum, GLuint);
using GlCopyTexSubImage2DFn = void(APIENTRY*)(
    GLenum, GLint, GLint, GLint, GLint, GLint, GLsizei, GLsizei);
using GlMatrixModeFn = void(APIENTRY*)(GLenum);
using GlLoadMatrixfFn = void(APIENTRY*)(const GLfloat*);
using GlViewportFn = void(APIENTRY*)(GLint, GLint, GLsizei, GLsizei);
using GlDrawElementsFn = void(APIENTRY*)(GLenum, GLsizei, GLenum, const void*);
using GlDrawArraysFn = void(APIENTRY*)(GLenum, GLint, GLsizei);
using GlClearFn = void(APIENTRY*)(GLbitfield);
using GlUniformMatrix4fvFn = void(APIENTRY*)(GLint, GLsizei, GLboolean, const GLfloat*);
using GlUniform2fFn = void(APIENTRY*)(GLint, GLfloat, GLfloat);
using GlUniform2fvFn = void(APIENTRY*)(GLint, GLsizei, const GLfloat*);
using GlGetUniformLocationFn = GLint(APIENTRY*)(GLuint, const GLchar*);
using GlUseProgramFn = void(APIENTRY*)(GLuint);
using GlBindFramebufferFn = void(APIENTRY*)(GLenum, GLuint);
using GlGetProgramivFn = void(APIENTRY*)(GLuint, GLenum, GLint*);
using GlGetActiveUniformFn = void(APIENTRY*)(GLuint, GLuint, GLsizei, GLsizei*, GLint*, GLenum*, GLchar*);
using GlGetAttachedShadersFn = void(APIENTRY*)(GLuint, GLsizei, GLsizei*, GLuint*);
using GlGetShaderivFn = void(APIENTRY*)(GLuint, GLenum, GLint*);
using GlGetShaderSourceFn = void(APIENTRY*)(GLuint, GLsizei, GLsizei*, GLchar*);
using GlGetActiveUniformsivFn = void(APIENTRY*)(GLuint, GLsizei, const GLuint*, GLenum, GLint*);
using GlGetActiveUniformBlockivFn = void(APIENTRY*)(GLuint, GLuint, GLenum, GLint*);
using GlGetActiveUniformBlockNameFn = void(APIENTRY*)(GLuint, GLuint, GLsizei, GLsizei*, GLchar*);
using GlGetIntegeriVFn = void(APIENTRY*)(GLenum, GLuint, GLint*);
using GlGetInteger64iVFn = void(APIENTRY*)(GLenum, GLuint, int64_t*);
using GlGetNamedBufferSubDataFn = void(APIENTRY*)(GLuint, intptr_t, ptrdiff_t, void*);
using GlNamedBufferSubDataFn = void(APIENTRY*)(GLuint, intptr_t, ptrdiff_t, const void*);
using GlBeginQueryFn = void(APIENTRY*)(GLenum, GLuint);
using GlEndQueryFn = void(APIENTRY*)(GLenum);
using GlGetQueryObjectivFn = void(APIENTRY*)(GLuint, GLenum, GLint*);
using GlGetQueryObjectuivFn = void(APIENTRY*)(GLuint, GLenum, GLuint*);
using GlGetQueryObjectui64vFn = void(APIENTRY*)(GLuint, GLenum, uint64_t*);

struct LastSamples {
    MatrixSummary fixedProjection = {};
    MatrixSummary uniformProjection = {};
    std::string uniformName;
    GLuint uniformProgram = 0;
    GLint uniformLocation = -1;
};

struct ReflectionFadeLocation {
    bool scanned = false;
    bool found = false;
    GLint blockIndex = -1;
    GLint offset = -1;
};

struct ReflectionFadePatch {
    bool active = false;
    GLuint buffer = 0;
    intptr_t offset = 0;
    std::array<float, 2> authored = {};
};

constexpr size_t kMaxPostEffectTextures = 32;
constexpr size_t kMaxPostEffectFramebuffers = 16;

struct PostEffectResourceCapture {
    bool active = false;
    uint64_t frame = 0;
    uint64_t sequence = 0;
    int eye = -1;
    uint64_t poseFrame = 0;
    const char* effectName = "Unknown";
    void* effect = nullptr;
    void* inputTexture = nullptr;
    void* renderTarget = nullptr;
    bool lastEffect = false;
    std::array<post_effect_resource_math::TextureResource, kMaxPostEffectTextures> textures{};
    size_t textureCount = 0;
    std::array<post_effect_resource_math::FramebufferResource, kMaxPostEffectFramebuffers> framebuffers{};
    size_t framebufferCount = 0;
    bool textureOverflow = false;
    bool framebufferOverflow = false;
};

struct PostEffectEyeResourceState {
    std::array<uint64_t, 2> signatures{};
    std::array<uint64_t, 2> poseFrames{};
    std::array<bool, 2> seen{};
    std::array<bool, 2> resourcesObserved{};
    post_effect_resource_math::EyeResourceOwnership ownership =
        post_effect_resource_math::EyeResourceOwnership::Unknown;
    uint64_t attempts = 0;
    uint64_t calls = 0;
};

struct OcclusionQueryState {
    GLenum target = 0;
    bool active = false;
    uint64_t lastBeginFrame = 0;
    uint64_t firstEyeFrame = 0;
    uint64_t replayEyeFrame = 0;
    HPLDualRenderPass lastPass = HPLDualRenderPass::None;
    uint64_t begins = 0;
    uint64_t results = 0;
};

struct FramebufferCopyState {
    uint64_t firstEyeFrame = 0;
    uint64_t replayEyeFrame = 0;
    uint64_t copies = 0;
};

Config g_config = {};
OpenXRRuntime* g_openxr = nullptr;

std::mutex g_installMutex;
std::mutex g_sampleMutex;
std::mutex g_extensionHookMutex;
std::mutex g_uniformNameMutex;
std::mutex g_matrixCaptureMutex;
std::mutex g_renderDiagnosticMutex;
std::mutex g_reflectionFadeMutex;
std::mutex g_postEffectResourceMutex;
std::mutex g_occlusionQueryMutex;
std::mutex g_framebufferCopyMutex;

bool g_minHookInitialized = false;
bool g_hooksInstalled = false;
LastSamples g_lastSamples;
std::unordered_set<void*> g_hookedExtensionTargets;
std::unordered_map<uint64_t, std::string> g_uniformNames;

SwapBuffersFn g_originalSwapBuffers = nullptr;
WglMakeCurrentFn g_originalWglMakeCurrent = nullptr;
WglGetProcAddressFn g_originalWglGetProcAddress = nullptr;
WglSwapIntervalEXTFn g_originalWglSwapIntervalEXT = nullptr;

GlGetStringFn g_glGetString = nullptr;
GlGetIntegervFn g_glGetIntegerv = nullptr;
GlGetFloatvFn g_glGetFloatv = nullptr;
GlGetTexLevelParameterivFn g_glGetTexLevelParameteriv = nullptr;
GlBindTextureFn g_originalGlBindTexture = nullptr;
GlCopyTexSubImage2DFn g_originalGlCopyTexSubImage2D = nullptr;
GlMatrixModeFn g_originalGlMatrixMode = nullptr;
GlLoadMatrixfFn g_originalGlLoadMatrixf = nullptr;
GlViewportFn g_originalGlViewport = nullptr;
GlDrawElementsFn g_originalGlDrawElements = nullptr;
GlDrawArraysFn g_originalGlDrawArrays = nullptr;
GlClearFn g_originalGlClear = nullptr;
GlUniformMatrix4fvFn g_originalGlUniformMatrix4fv = nullptr;
GlUniform2fFn g_originalGlUniform2f = nullptr;
GlUniform2fvFn g_originalGlUniform2fv = nullptr;
GlGetUniformLocationFn g_originalGlGetUniformLocation = nullptr;
GlUseProgramFn g_originalGlUseProgram = nullptr;
GlBindFramebufferFn g_originalGlBindFramebuffer = nullptr;
GlGetProgramivFn g_glGetProgramiv = nullptr;
GlGetActiveUniformFn g_glGetActiveUniform = nullptr;
GlGetAttachedShadersFn g_glGetAttachedShaders = nullptr;
GlGetShaderivFn g_glGetShaderiv = nullptr;
GlGetShaderSourceFn g_glGetShaderSource = nullptr;
GlGetActiveUniformsivFn g_glGetActiveUniformsiv = nullptr;
GlGetActiveUniformBlockivFn g_glGetActiveUniformBlockiv = nullptr;
GlGetActiveUniformBlockNameFn g_glGetActiveUniformBlockName = nullptr;
GlGetIntegeriVFn g_glGetIntegeriV = nullptr;
GlGetInteger64iVFn g_glGetInteger64iV = nullptr;
GlGetNamedBufferSubDataFn g_glGetNamedBufferSubData = nullptr;
GlNamedBufferSubDataFn g_glNamedBufferSubData = nullptr;
GlBeginQueryFn g_originalGlBeginQuery = nullptr;
GlEndQueryFn g_originalGlEndQuery = nullptr;
GlGetQueryObjectivFn g_originalGlGetQueryObjectiv = nullptr;
GlGetQueryObjectuivFn g_originalGlGetQueryObjectuiv = nullptr;
GlGetQueryObjectui64vFn g_originalGlGetQueryObjectui64v = nullptr;

bool g_occlusionQueryTelemetryEnabled = false;
std::unordered_map<GLuint, OcclusionQueryState> g_occlusionQueryStates;
std::unordered_map<GLenum, GLuint> g_activeOcclusionQueries;
std::atomic<uint64_t> g_occlusionQueryBegins = 0;
std::atomic<uint64_t> g_occlusionQueryEnds = 0;
std::atomic<uint64_t> g_occlusionQueryResults = 0;
std::atomic<uint64_t> g_occlusionQueryAvailabilityChecks = 0;
std::atomic<uint64_t> g_occlusionQueryFirstEyeBegins = 0;
std::atomic<uint64_t> g_occlusionQueryReplayEyeBegins = 0;
std::atomic<uint64_t> g_occlusionQuerySameFrameReuses = 0;
std::atomic<uint64_t> g_occlusionQueryTargetConflicts = 0;
std::atomic<uint64_t> g_occlusionQueryUnmatchedEnds = 0;
std::atomic<uint64_t> g_occlusionQueryStateOverflows = 0;
std::unordered_map<GLuint, FramebufferCopyState> g_framebufferCopyStates;
std::atomic<uint64_t> g_framebufferCopies = 0;
std::atomic<uint64_t> g_framebufferCopyPixels = 0;
std::atomic<uint64_t> g_framebufferCopyFirstEye = 0;
std::atomic<uint64_t> g_framebufferCopyReplayEye = 0;
std::atomic<uint64_t> g_framebufferCopyCrossEyeReuses = 0;
std::atomic<uint64_t> g_framebufferCopyTextureReadFailures = 0;
std::atomic<uint64_t> g_framebufferCopyStateOverflows = 0;

void APIENTRY HookGlBeginQuery(GLenum target, GLuint query);
void APIENTRY HookGlEndQuery(GLenum target);
void APIENTRY HookGlGetQueryObjectiv(GLuint query, GLenum pname, GLint* value);
void APIENTRY HookGlGetQueryObjectuiv(GLuint query, GLenum pname, GLuint* value);
void APIENTRY HookGlGetQueryObjectui64v(GLuint query, GLenum pname, uint64_t* value);
void APIENTRY HookGlCopyTexSubImage2D(
    GLenum target,
    GLint level,
    GLint xOffset,
    GLint yOffset,
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height);

std::atomic<uint64_t> g_frameIndex = 0;
std::atomic<uint64_t> g_swapCount = 0;
std::atomic<uint64_t> g_wglMakeCurrentCount = 0;
std::atomic<uint64_t> g_drawElementsThisFrame = 0;
std::atomic<uint64_t> g_drawArraysThisFrame = 0;
std::atomic<uint64_t> g_matrixLoadsThisFrame = 0;
std::atomic<uint64_t> g_uniformMatricesThisFrame = 0;
std::atomic<uint64_t> g_viewportCallsThisFrame = 0;
std::atomic<uint64_t> g_framebufferBindsThisFrame = 0;
std::atomic<uint64_t> g_totalDrawElements = 0;
std::atomic<uint64_t> g_totalDrawArrays = 0;
std::atomic<uint64_t> g_totalViewportCalls = 0;
std::atomic<uint64_t> g_totalFramebufferBinds = 0;
std::atomic<uint64_t> g_totalProgramUses = 0;
std::atomic<uint64_t> g_totalClears = 0;
std::atomic<bool> g_terminalClearSuppressionActive = false;
std::atomic<GLuint> g_terminalClearTargetFramebuffer = 0;
std::atomic<DWORD> g_terminalClearOwnerThread = 0;
std::atomic<uint64_t> g_terminalClearFrame = 0;
std::atomic<uint64_t> g_terminalClearArmedCaptures = 0;
std::atomic<uint64_t> g_terminalColorClearsSuppressed = 0;
std::atomic<uint64_t> g_terminalDepthStencilClearsForwarded = 0;
std::atomic<uint64_t> g_terminalClearFramebufferMismatches = 0;
std::atomic<uint64_t> g_terminalClearThreadMismatches = 0;
std::atomic<uint64_t> g_terminalDrawProbeSequence = 0;
std::atomic<uint64_t> g_terminalDrawProbeStartFrame = 0;
std::atomic<uint64_t> g_terminalDrawProbeEndFrame = 0;
std::atomic<uint32_t> g_terminalDrawProbeSamples = 0;
std::atomic<bool> g_terminalCaptureGuardActive = false;
std::atomic<GLuint> g_terminalCaptureGuardFramebuffer = 0;
std::atomic<uint64_t> g_terminalCaptureGuardFrame = 0;
std::atomic<uint64_t> g_terminalOffscreenScissorBypasses = 0;
std::atomic<uint32_t> g_matrixSamplesThisFrame = 0;
std::atomic<uint32_t> g_uniformNameLogs = 0;
std::atomic<uint32_t> g_uniformMatrixLogs = 0;
std::atomic<uint32_t> g_shadowUniformNameLogs = 0;
std::atomic<uint32_t> g_shadowUniformSampleLogs = 0;
std::atomic<uint64_t> g_shadowJitterUploads = 0;
std::atomic<uint64_t> g_shadowJitterOverrides = 0;
std::atomic<bool> g_shadowJitterSuppressed = false;
std::atomic<bool> g_shadowJitterF7Down = false;
std::atomic<GLenum> g_currentMatrixMode = kGLModelView;
std::atomic<GLuint> g_currentProgram = 0;
std::atomic<GLuint> g_currentFramebuffer = 0;
std::atomic<GLint> g_currentViewportX = 0;
std::atomic<GLint> g_currentViewportY = 0;
std::atomic<GLsizei> g_currentViewportWidth = 0;
std::atomic<GLsizei> g_currentViewportHeight = 0;
std::atomic<DWORD> g_renderThreadId = 0;
std::atomic<uint64_t> g_matrixCaptureEndFrame = 0;
std::atomic<uint64_t> g_matrixCaptureSequence = 0;
std::atomic<bool> g_matrixCaptureF9Down = false;
std::atomic<uint64_t> g_renderDiagnosticEndFrame = 0;
std::atomic<uint64_t> g_renderDiagnosticSequence = 0;
std::atomic<bool> g_renderDiagnosticF6Down = false;
std::atomic<bool> g_reflectionFadeF3Down = false;
std::atomic<bool> g_reflectionFadeBypassed = false;
std::atomic<uint64_t> g_postEffectResourceCaptures = 0;
std::atomic<uint64_t> g_postEffectResourceLogs = 0;
std::atomic<uint64_t> g_postEffectTextureResources = 0;
std::atomic<uint64_t> g_postEffectFramebufferResources = 0;
std::atomic<uint64_t> g_postEffectSharedClassifications = 0;
std::atomic<uint64_t> g_postEffectDistinctClassifications = 0;
thread_local PostEffectResourceCapture g_postEffectResourceCapture;
std::unordered_map<void*, PostEffectEyeResourceState> g_postEffectEyeResources;
std::atomic<uint64_t> g_reflectionFadePatches = 0;

uint64_t g_matrixCaptureUploads = 0;
uint64_t g_matrixCaptureSiteLogs = 0;
uint64_t g_matrixCaptureSampleLogs = 0;

void RecordTerminalDrawState(const char* kind, GLenum mode, GLsizei count)
{
    const uint64_t frame = g_frameIndex.load(std::memory_order_relaxed);
    const uint64_t startFrame = g_terminalDrawProbeStartFrame.load(std::memory_order_relaxed);
    const uint64_t endFrame = g_terminalDrawProbeEndFrame.load(std::memory_order_relaxed);
    if (startFrame == 0 || frame < startFrame || frame > endFrame
        || !g_terminalClearSuppressionActive.load(std::memory_order_acquire)) {
        return;
    }
    const uint32_t sample = g_terminalDrawProbeSamples.fetch_add(
        1, std::memory_order_relaxed) + 1;
    if (sample > 128 || g_glGetIntegerv == nullptr) return;

    GLint framebuffer = 0;
    GLint program = 0;
    GLint viewport[4] = {};
    GLint scissor[4] = {};
    GLint texture2D = 0;
    GLint blendSrc = 0;
    GLint blendDst = 0;
    g_glGetIntegerv(kGLFramebufferBinding, &framebuffer);
    g_glGetIntegerv(kGLCurrentProgram, &program);
    g_glGetIntegerv(kGLViewport, viewport);
    g_glGetIntegerv(kGLScissorBox, scissor);
    g_glGetIntegerv(kGLTextureBinding2D, &texture2D);
    g_glGetIntegerv(kGLBlendSrcRgb, &blendSrc);
    g_glGetIntegerv(kGLBlendDstRgb, &blendDst);
    const GLuint target = g_terminalClearTargetFramebuffer.load(std::memory_order_relaxed);
    Logger::Instance().Write(
        framebuffer == static_cast<GLint>(target) ? LogLevel::Info : LogLevel::Warn,
        "terminal_draw_state sequence=%llu sample=%u frame=%llu kind=%s mode=0x%x count=%d fbo=%d targetFbo=%u targetMatch=%d program=%d viewport=%d,%d,%d,%d scissor={enabled=%d box=%d,%d,%d,%d} texture2D=%d blend={enabled=%d src=0x%x dst=0x%x}",
        static_cast<unsigned long long>(
            g_terminalDrawProbeSequence.load(std::memory_order_relaxed)),
        sample,
        static_cast<unsigned long long>(frame),
        kind,
        static_cast<unsigned>(mode),
        count,
        framebuffer,
        target,
        framebuffer == static_cast<GLint>(target) ? 1 : 0,
        program,
        viewport[0], viewport[1], viewport[2], viewport[3],
        glIsEnabled(GL_SCISSOR_TEST) == GL_TRUE ? 1 : 0,
        scissor[0], scissor[1], scissor[2], scissor[3],
        texture2D,
        glIsEnabled(GL_BLEND) == GL_TRUE ? 1 : 0,
        blendSrc,
        blendDst);
}

bool DisableOffscreenTerminalScissor()
{
    if (!g_terminalCaptureGuardActive.load(std::memory_order_acquire)
        || g_glGetIntegerv == nullptr
        || glIsEnabled(GL_SCISSOR_TEST) != GL_TRUE) {
        return false;
    }
    GLint framebuffer = 0;
    GLint viewport[4] = {};
    GLint scissor[4] = {};
    g_glGetIntegerv(kGLFramebufferBinding, &framebuffer);
    g_glGetIntegerv(kGLViewport, viewport);
    g_glGetIntegerv(kGLScissorBox, scissor);
    const GLuint target = g_terminalCaptureGuardFramebuffer.load(std::memory_order_relaxed);
    if (framebuffer != static_cast<GLint>(target)) return false;

    const bool intersects = scissor[2] > 0 && scissor[3] > 0
        && scissor[0] < viewport[0] + viewport[2]
        && scissor[1] < viewport[1] + viewport[3]
        && scissor[0] + scissor[2] > viewport[0]
        && scissor[1] + scissor[3] > viewport[1];
    if (intersects) return false;

    glDisable(GL_SCISSOR_TEST);
    const uint64_t bypass = g_terminalOffscreenScissorBypasses.fetch_add(
        1, std::memory_order_relaxed) + 1;
    const uint64_t interval = static_cast<uint64_t>(
        std::max(g_config.hplControllerLogInterval, 1));
    if (bypass <= 8 || bypass % interval == 0) {
        Logger::Instance().Write(
            LogLevel::Warn,
            "terminal_scissor_bypass count=%llu frame=%llu fbo=%d viewport=%d,%d,%d,%d scissor=%d,%d,%d,%d policy=disable_only_fully_offscreen_capture_scissor",
            static_cast<unsigned long long>(bypass),
            static_cast<unsigned long long>(
                g_terminalCaptureGuardFrame.load(std::memory_order_relaxed)),
            framebuffer,
            viewport[0], viewport[1], viewport[2], viewport[3],
            scissor[0], scissor[1], scissor[2], scissor[3]);
    }
    return true;
}
std::unordered_set<std::string> g_matrixCaptureSites;
std::unordered_map<uint64_t, uint32_t> g_matrixCaptureSampleMasks;
std::filesystem::path g_renderDiagnosticPath;
std::ofstream g_renderDiagnosticDrawStream;
std::ofstream g_renderDiagnosticMatrixStream;
std::unordered_set<GLuint> g_renderDiagnosticPrograms;
std::unordered_set<uint64_t> g_renderDiagnosticBufferSnapshots;
std::unordered_map<GLuint, ReflectionFadeLocation> g_reflectionFadeLocations;
uint64_t g_renderDiagnosticDraws = 0;
uint64_t g_renderDiagnosticMatrices = 0;

HDC g_lastLoggedHdc = nullptr;
HGLRC g_lastLoggedGlContext = nullptr;

std::string HexPointer(const void* value)
{
    std::ostringstream oss;
    oss << "0x" << std::hex << reinterpret_cast<uintptr_t>(value);
    return oss.str();
}

bool IsValidWglProc(PROC proc)
{
    const uintptr_t value = reinterpret_cast<uintptr_t>(proc);
    return proc != nullptr && value > 3 && value != 0xffffffffffffffffull;
}

const char* SafeGlString(GLenum name)
{
    if (g_glGetString == nullptr) {
        return "";
    }
    const GLubyte* value = g_glGetString(name);
    return value != nullptr ? reinterpret_cast<const char*>(value) : "";
}

uint64_t UniformKey(GLuint program, GLint location)
{
    return (static_cast<uint64_t>(program) << 32) | static_cast<uint32_t>(location);
}

std::string UniformNameFor(GLuint program, GLint location)
{
    std::lock_guard lock(g_uniformNameMutex);
    const auto it = g_uniformNames.find(UniformKey(program, location));
    if (it == g_uniformNames.end()) {
        return {};
    }
    return it->second;
}

bool NameEquals(const char* left, const char* right)
{
    return left != nullptr && right != nullptr && std::strcmp(left, right) == 0;
}

bool IsCameraMatrixUniform(const std::string& name)
{
    return name.find("Projection") != std::string::npos
        || name.find("View") != std::string::npos;
}

bool IsShadowRelevantUniform(const std::string& name)
{
    return name.find("Shadow") != std::string::npos
        || name.find("Split") != std::string::npos;
}

bool IsShadowJitterRadiusUniform(const std::string& name)
{
    return name == "avShadowMapOffsetMul";
}

bool RenderDiagnosticActive()
{
    const uint64_t endFrame = g_renderDiagnosticEndFrame.load(std::memory_order_relaxed);
    return endFrame != 0 && g_frameIndex.load(std::memory_order_relaxed) < endFrame;
}

std::string ShaderFeatureSummary(std::string source)
{
    std::transform(source.begin(), source.end(), source.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });

    std::string result;
    const auto add = [&](const char* name) {
        if (!result.empty()) {
            result += ",";
        }
        result += name;
    };
    if (source.find("shadow") != std::string::npos) add("shadow");
    if (source.find("reflect") != std::string::npos) add("reflection");
    if (source.find("envmap") != std::string::npos || source.find("samplercube") != std::string::npos) add("environment");
    if (source.find("temporal") != std::string::npos || source.find("previous") != std::string::npos) add("temporal");
    if (source.find("water") != std::string::npos || source.find("refraction") != std::string::npos) add("water");
    return result.empty() ? "unclassified" : result;
}

void DumpRenderDiagnosticProgram(GLuint program)
{
    if (!RenderDiagnosticActive() || program == 0) {
        return;
    }

    std::lock_guard lock(g_renderDiagnosticMutex);
    if (g_renderDiagnosticPrograms.contains(program)
        || g_renderDiagnosticPrograms.size() >= static_cast<size_t>(g_config.renderDiagnosticMaxPrograms)) {
        return;
    }
    g_renderDiagnosticPrograms.insert(program);

    const std::filesystem::path outputPath =
        g_renderDiagnosticPath / ("program_" + std::to_string(program) + ".txt");
    std::ofstream out(outputPath, std::ios::out | std::ios::trunc);
    if (!out) {
        Logger::Instance().Write(
            LogLevel::Warn,
            "render_diag_program dump_failed program=%u path=%s",
            program,
            outputPath.string().c_str());
        return;
    }

    out << "program=" << program << "\n";
    std::string combinedSource;
    GLint uniformCount = 0;
    GLint uniformNameMax = 0;
    if (g_glGetProgramiv != nullptr && g_glGetActiveUniform != nullptr) {
        g_glGetProgramiv(program, kGLActiveUniforms, &uniformCount);
        g_glGetProgramiv(program, kGLActiveUniformMaxLength, &uniformNameMax);
        uniformCount = std::clamp(uniformCount, 0, 16384);
        uniformNameMax = std::clamp(uniformNameMax, 1, 16384);
        std::vector<GLchar> name(static_cast<size_t>(uniformNameMax), 0);
        out << "active_uniforms=" << uniformCount << "\n\n[uniforms]\n";
        for (GLint index = 0; index < uniformCount; ++index) {
            GLsizei nameLength = 0;
            GLint size = 0;
            GLenum type = 0;
            g_glGetActiveUniform(
                program,
                static_cast<GLuint>(index),
                uniformNameMax,
                &nameLength,
                &size,
                &type,
                name.data());
            const GLint location = g_originalGlGetUniformLocation != nullptr
                ? g_originalGlGetUniformLocation(program, name.data())
                : -1;
            GLint blockIndex = -1;
            GLint blockOffset = -1;
            GLint arrayStride = 0;
            GLint matrixStride = 0;
            GLint rowMajor = 0;
            if (g_glGetActiveUniformsiv != nullptr) {
                const GLuint uniformIndex = static_cast<GLuint>(index);
                g_glGetActiveUniformsiv(program, 1, &uniformIndex, kGLUniformBlockIndex, &blockIndex);
                g_glGetActiveUniformsiv(program, 1, &uniformIndex, kGLUniformOffset, &blockOffset);
                g_glGetActiveUniformsiv(program, 1, &uniformIndex, kGLUniformArrayStride, &arrayStride);
                g_glGetActiveUniformsiv(program, 1, &uniformIndex, kGLUniformMatrixStride, &matrixStride);
                g_glGetActiveUniformsiv(program, 1, &uniformIndex, kGLUniformIsRowMajor, &rowMajor);
            }
            out << index << " location=" << location
                << " size=" << size
                << " type=0x" << std::hex << type << std::dec
                << " block=" << blockIndex
                << " offset=" << blockOffset
                << " arrayStride=" << arrayStride
                << " matrixStride=" << matrixStride
                << " rowMajor=" << rowMajor
                << " name=" << std::string(name.data(), static_cast<size_t>(std::max<GLsizei>(nameLength, 0)))
                << "\n";
            combinedSource.append(name.data(), static_cast<size_t>(std::max<GLsizei>(nameLength, 0)));
            combinedSource.push_back('\n');
        }
    } else {
        out << "active_uniforms=unavailable\n";
    }

    GLint uniformBlockCount = 0;
    if (g_glGetProgramiv != nullptr
        && g_glGetActiveUniformBlockiv != nullptr
        && g_glGetActiveUniformBlockName != nullptr) {
        g_glGetProgramiv(program, kGLActiveUniformBlocks, &uniformBlockCount);
        uniformBlockCount = std::clamp(uniformBlockCount, 0, 256);
        out << "\n[uniform_blocks]\ncount=" << uniformBlockCount << "\n";
        for (GLint blockIndex = 0; blockIndex < uniformBlockCount; ++blockIndex) {
            GLint nameLength = 0;
            GLint binding = -1;
            GLint dataSize = 0;
            GLint activeUniforms = 0;
            g_glGetActiveUniformBlockiv(program, blockIndex, kGLUniformBlockNameLength, &nameLength);
            g_glGetActiveUniformBlockiv(program, blockIndex, kGLUniformBlockBinding, &binding);
            g_glGetActiveUniformBlockiv(program, blockIndex, kGLUniformBlockDataSize, &dataSize);
            g_glGetActiveUniformBlockiv(program, blockIndex, kGLUniformBlockActiveUniforms, &activeUniforms);
            nameLength = std::clamp(nameLength, 1, 4096);
            std::vector<GLchar> blockName(static_cast<size_t>(nameLength), 0);
            GLsizei returnedLength = 0;
            g_glGetActiveUniformBlockName(
                program,
                static_cast<GLuint>(blockIndex),
                nameLength,
                &returnedLength,
                blockName.data());
            out << blockIndex
                << " binding=" << binding
                << " dataSize=" << dataSize
                << " activeUniforms=" << activeUniforms
                << " name=" << std::string(blockName.data(), static_cast<size_t>(std::max<GLsizei>(returnedLength, 0)))
                << "\n";
        }
    } else {
        out << "\n[uniform_blocks]\nunavailable\n";
    }

    GLint attachedCount = 0;
    if (g_glGetProgramiv != nullptr
        && g_glGetAttachedShaders != nullptr
        && g_glGetShaderiv != nullptr
        && g_glGetShaderSource != nullptr) {
        g_glGetProgramiv(program, kGLAttachedShaders, &attachedCount);
        attachedCount = std::clamp(attachedCount, 0, 64);
        std::vector<GLuint> shaders(static_cast<size_t>(attachedCount));
        GLsizei returnedCount = 0;
        if (attachedCount > 0) {
            g_glGetAttachedShaders(program, attachedCount, &returnedCount, shaders.data());
        }
        out << "\n[shaders]\nattached=" << returnedCount << "\n";
        for (GLsizei index = 0; index < returnedCount; ++index) {
            GLint shaderType = 0;
            GLint sourceLength = 0;
            g_glGetShaderiv(shaders[index], kGLShaderType, &shaderType);
            g_glGetShaderiv(shaders[index], kGLShaderSourceLength, &sourceLength);
            sourceLength = std::clamp(sourceLength, 0, 16 * 1024 * 1024);
            out << "\n--- shader=" << shaders[index]
                << " type=0x" << std::hex << shaderType << std::dec
                << " sourceLength=" << sourceLength << " ---\n";
            if (sourceLength > 1) {
                std::vector<GLchar> source(static_cast<size_t>(sourceLength), 0);
                GLsizei returnedLength = 0;
                g_glGetShaderSource(shaders[index], sourceLength, &returnedLength, source.data());
                const std::string shaderSource(
                    source.data(),
                    static_cast<size_t>(std::max<GLsizei>(returnedLength, 0)));
                out << shaderSource << "\n";
                combinedSource += shaderSource;
            }
        }
    } else {
        out << "\n[shaders]\nunavailable\n";
    }

    const std::string features = ShaderFeatureSummary(combinedSource);
    out << "\nfeatures=" << features << "\n";
    out.close();
    Logger::Instance().Write(
        LogLevel::Info,
        "render_diag_program program=%u uniforms=%d attachedShaders=%d features=%s path=%s",
        program,
        uniformCount,
        attachedCount,
        features.c_str(),
        outputPath.string().c_str());
}

void DumpRenderDiagnosticUniformBuffers(
    GLuint program,
    int eye,
    uint64_t poseFrame,
    uint64_t renderFrame)
{
    if (!RenderDiagnosticActive()
        || program == 0
        || eye < 0
        || g_glGetProgramiv == nullptr
        || g_glGetActiveUniformBlockiv == nullptr
        || g_glGetActiveUniformBlockName == nullptr
        || g_glGetIntegeriV == nullptr
        || g_glGetNamedBufferSubData == nullptr) {
        return;
    }

    const uint64_t snapshotKey =
        (static_cast<uint64_t>(program) << 32) | static_cast<uint32_t>(eye + 1);
    std::lock_guard lock(g_renderDiagnosticMutex);
    if (g_renderDiagnosticBufferSnapshots.contains(snapshotKey)) {
        return;
    }
    g_renderDiagnosticBufferSnapshots.insert(snapshotKey);

    GLint uniformBlockCount = 0;
    g_glGetProgramiv(program, kGLActiveUniformBlocks, &uniformBlockCount);
    uniformBlockCount = std::clamp(uniformBlockCount, 0, 256);
    if (uniformBlockCount == 0) {
        return;
    }

    const std::filesystem::path outputPath = g_renderDiagnosticPath
        / ("program_" + std::to_string(program)
            + "_eye_" + std::to_string(eye)
            + "_frame_" + std::to_string(renderFrame)
            + "_ubos.txt");
    std::ofstream out(outputPath, std::ios::out | std::ios::trunc);
    if (!out) {
        return;
    }
    out << "program=" << program
        << " eye=" << eye
        << " poseFrame=" << poseFrame
        << " renderFrame=" << renderFrame
        << " blocks=" << uniformBlockCount << "\n";

    uint64_t dumpedBlocks = 0;
    for (GLint blockIndex = 0; blockIndex < uniformBlockCount; ++blockIndex) {
        GLint nameLength = 0;
        GLint binding = -1;
        GLint dataSize = 0;
        g_glGetActiveUniformBlockiv(program, blockIndex, kGLUniformBlockNameLength, &nameLength);
        g_glGetActiveUniformBlockiv(program, blockIndex, kGLUniformBlockBinding, &binding);
        g_glGetActiveUniformBlockiv(program, blockIndex, kGLUniformBlockDataSize, &dataSize);
        nameLength = std::clamp(nameLength, 1, 4096);
        dataSize = std::clamp(dataSize, 0, 1024 * 1024);
        std::vector<GLchar> blockName(static_cast<size_t>(nameLength), 0);
        GLsizei returnedLength = 0;
        g_glGetActiveUniformBlockName(
            program,
            static_cast<GLuint>(blockIndex),
            nameLength,
            &returnedLength,
            blockName.data());

        GLint buffer = 0;
        int64_t bufferStart = 0;
        int64_t bufferSize = 0;
        if (binding >= 0) {
            g_glGetIntegeriV(kGLUniformBufferBinding, static_cast<GLuint>(binding), &buffer);
            if (g_glGetInteger64iV != nullptr) {
                g_glGetInteger64iV(kGLUniformBufferStart, static_cast<GLuint>(binding), &bufferStart);
                g_glGetInteger64iV(kGLUniformBufferSize, static_cast<GLuint>(binding), &bufferSize);
            }
        }
        out << "\n[block " << blockIndex << "] name="
            << std::string(blockName.data(), static_cast<size_t>(std::max<GLsizei>(returnedLength, 0)))
            << " binding=" << binding
            << " buffer=" << buffer
            << " bufferStart=" << bufferStart
            << " bufferSize=" << bufferSize
            << " dataSize=" << dataSize << "\n";
        if (buffer == 0 || dataSize == 0) {
            continue;
        }

        const size_t readableSize = bufferSize > 0
            ? std::min<size_t>(static_cast<size_t>(dataSize), static_cast<size_t>(bufferSize))
            : static_cast<size_t>(dataSize);
        std::vector<std::byte> data(readableSize);
        g_glGetNamedBufferSubData(
            static_cast<GLuint>(buffer),
            static_cast<intptr_t>(bufferStart),
            static_cast<ptrdiff_t>(data.size()),
            data.data());
        ++dumpedBlocks;
        out << "offset,hex,float\n";
        for (size_t offset = 0; offset + sizeof(uint32_t) <= data.size(); offset += sizeof(uint32_t)) {
            uint32_t bits = 0;
            float value = 0.0f;
            std::memcpy(&bits, data.data() + offset, sizeof(bits));
            std::memcpy(&value, data.data() + offset, sizeof(value));
            out << offset << ",0x"
                << std::hex << std::setw(8) << std::setfill('0') << bits
                << std::dec << std::setfill(' ') << ',' << std::setprecision(9) << value << "\n";
        }
    }
    out.close();
    Logger::Instance().Write(
        LogLevel::Info,
        "render_diag_ubo program=%u eye=%d poseFrame=%llu renderFrame=%llu blocks=%d dumped=%llu path=%s",
        program,
        eye,
        static_cast<unsigned long long>(poseFrame),
        static_cast<unsigned long long>(renderFrame),
        uniformBlockCount,
        static_cast<unsigned long long>(dumpedBlocks),
        outputPath.string().c_str());
}

void PollReflectionFadeControl()
{
    if (!g_config.hplReflectionFadeControl) {
        return;
    }
    const bool keyDown = (GetAsyncKeyState(VK_F3) & 0x8000) != 0;
    const bool wasDown = g_reflectionFadeF3Down.exchange(keyDown, std::memory_order_relaxed);
    if (keyDown && !wasDown) {
        const bool bypassed = !g_reflectionFadeBypassed.load(std::memory_order_relaxed);
        g_reflectionFadeBypassed.store(bypassed, std::memory_order_relaxed);
        Logger::Instance().Write(
            LogLevel::Warn,
            "reflection_fade_bypass enabled=%d key=F3 policy=force_full_reflection patches=%llu",
            bypassed ? 1 : 0,
            static_cast<unsigned long long>(g_reflectionFadePatches.load(std::memory_order_relaxed)));
    }
}

ReflectionFadeLocation FindReflectionFadeLocation(GLuint program)
{
    std::lock_guard lock(g_reflectionFadeMutex);
    ReflectionFadeLocation& cached = g_reflectionFadeLocations[program];
    if (cached.scanned) {
        return cached;
    }
    cached.scanned = true;
    if (program == 0
        || g_glGetProgramiv == nullptr
        || g_glGetActiveUniform == nullptr
        || g_glGetActiveUniformsiv == nullptr) {
        return cached;
    }

    GLint uniformCount = 0;
    GLint maxNameLength = 0;
    g_glGetProgramiv(program, kGLActiveUniforms, &uniformCount);
    g_glGetProgramiv(program, kGLActiveUniformMaxLength, &maxNameLength);
    uniformCount = std::clamp(uniformCount, 0, 4096);
    maxNameLength = std::clamp(maxNameLength, 1, 4096);
    std::vector<GLchar> name(static_cast<size_t>(maxNameLength), 0);
    for (GLint index = 0; index < uniformCount; ++index) {
        GLsizei returnedLength = 0;
        GLint size = 0;
        GLenum type = 0;
        g_glGetActiveUniform(
            program,
            static_cast<GLuint>(index),
            maxNameLength,
            &returnedLength,
            &size,
            &type,
            name.data());
        if (std::string(name.data(), static_cast<size_t>(std::max<GLsizei>(returnedLength, 0)))
            != "avReflectionFadeStartAndLength") {
            continue;
        }

        const GLuint uniformIndex = static_cast<GLuint>(index);
        g_glGetActiveUniformsiv(program, 1, &uniformIndex, kGLUniformBlockIndex, &cached.blockIndex);
        g_glGetActiveUniformsiv(program, 1, &uniformIndex, kGLUniformOffset, &cached.offset);
        cached.found = cached.blockIndex >= 0 && cached.offset >= 0;
        Logger::Instance().Write(
            LogLevel::Info,
            "reflection_fade_target program=%u block=%d offset=%d found=%d",
            program,
            cached.blockIndex,
            cached.offset,
            cached.found ? 1 : 0);
        break;
    }
    return cached;
}

ReflectionFadePatch ApplyReflectionFadeBypass(GLuint program)
{
    ReflectionFadePatch patch;
    if (!g_reflectionFadeBypassed.load(std::memory_order_relaxed)
        || g_glGetActiveUniformBlockiv == nullptr
        || g_glGetIntegeriV == nullptr
        || g_glGetNamedBufferSubData == nullptr
        || g_glNamedBufferSubData == nullptr) {
        return patch;
    }

    const ReflectionFadeLocation location = FindReflectionFadeLocation(program);
    if (!location.found) {
        return patch;
    }
    GLint binding = -1;
    g_glGetActiveUniformBlockiv(
        program,
        static_cast<GLuint>(location.blockIndex),
        kGLUniformBlockBinding,
        &binding);
    if (binding < 0) {
        return patch;
    }

    GLint buffer = 0;
    int64_t bufferStart = 0;
    g_glGetIntegeriV(kGLUniformBufferBinding, static_cast<GLuint>(binding), &buffer);
    if (g_glGetInteger64iV != nullptr) {
        g_glGetInteger64iV(kGLUniformBufferStart, static_cast<GLuint>(binding), &bufferStart);
    }
    if (buffer == 0) {
        return patch;
    }

    patch.buffer = static_cast<GLuint>(buffer);
    patch.offset = static_cast<intptr_t>(bufferStart + location.offset);
    g_glGetNamedBufferSubData(
        patch.buffer,
        patch.offset,
        static_cast<ptrdiff_t>(sizeof(patch.authored)),
        patch.authored.data());
    const std::array<float, 2> forced = { 1.0e20f, 1.0f };
    g_glNamedBufferSubData(
        patch.buffer,
        patch.offset,
        static_cast<ptrdiff_t>(sizeof(forced)),
        forced.data());
    patch.active = true;

    const uint64_t patches = g_reflectionFadePatches.fetch_add(1, std::memory_order_relaxed) + 1;
    if (patches <= 8 || patches % 1200 == 0) {
        Logger::Instance().Write(
            LogLevel::Info,
            "reflection_fade_override patch=%llu program=%u buffer=%u offset=%lld authored=%.6f,%.6f forced=1",
            static_cast<unsigned long long>(patches),
            program,
            patch.buffer,
            static_cast<long long>(patch.offset),
            patch.authored[0],
            patch.authored[1]);
    }
    return patch;
}

void RestoreReflectionFade(const ReflectionFadePatch& patch)
{
    if (!patch.active || g_glNamedBufferSubData == nullptr) {
        return;
    }
    g_glNamedBufferSubData(
        patch.buffer,
        patch.offset,
        static_cast<ptrdiff_t>(sizeof(patch.authored)),
        patch.authored.data());
}

void RecordRenderDiagnosticDraw(const char* kind, GLenum mode, GLsizei count)
{
    if (!RenderDiagnosticActive()) {
        return;
    }

    const HPLCameraBridgeStatus camera = GetHPLCameraBridgeStatus();
    const GLuint program = g_currentProgram.load(std::memory_order_relaxed);
    const uint64_t renderFrame = g_frameIndex.load(std::memory_order_relaxed) + 1;
    DumpRenderDiagnosticUniformBuffers(
        program,
        camera.stereoRenderEye,
        camera.stereoRenderPoseFrame,
        renderFrame);

    std::lock_guard lock(g_renderDiagnosticMutex);
    if (!g_renderDiagnosticDrawStream
        || g_renderDiagnosticDraws >= static_cast<uint64_t>(g_config.renderDiagnosticMaxDraws)) {
        return;
    }
    ++g_renderDiagnosticDraws;
    g_renderDiagnosticDrawStream
        << renderFrame << ','
        << g_renderDiagnosticDraws << ','
        << GetHPLRenderStageName(GetActiveHPLRenderStage()) << ','
        << camera.stereoRenderEye << ','
        << camera.stereoRenderPoseFrame << ','
        << program << ','
        << g_currentFramebuffer.load(std::memory_order_relaxed) << ','
        << g_currentViewportX.load(std::memory_order_relaxed) << ','
        << g_currentViewportY.load(std::memory_order_relaxed) << ','
        << g_currentViewportWidth.load(std::memory_order_relaxed) << ','
        << g_currentViewportHeight.load(std::memory_order_relaxed) << ','
        << kind << ",0x" << std::hex << mode << std::dec << ',' << count << '\n';
}

void RecordRenderDiagnosticMatrix(
    GLuint program,
    GLint location,
    GLsizei count,
    GLboolean transpose,
    const GLfloat* value,
    const std::string& uniformName)
{
    if (!RenderDiagnosticActive() || value == nullptr) {
        return;
    }

    std::lock_guard lock(g_renderDiagnosticMutex);
    if (!g_renderDiagnosticMatrixStream || g_renderDiagnosticMatrices >= 8192) {
        return;
    }
    const HPLCameraBridgeStatus camera = GetHPLCameraBridgeStatus();
    ++g_renderDiagnosticMatrices;
    g_renderDiagnosticMatrixStream
        << (g_frameIndex.load(std::memory_order_relaxed) + 1) << ','
        << camera.stereoRenderEye << ','
        << camera.stereoRenderPoseFrame << ','
        << program << ',' << location << ",\"" << uniformName << "\","
        << count << ',' << static_cast<unsigned>(transpose) << ",\""
        << MatrixValuesText(value) << "\"\n";
}

std::string ModuleRelativeAddress(const void* address)
{
    if (address == nullptr) {
        return "unknown+0x0";
    }

    HMODULE module = nullptr;
    if (!GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(address),
            &module)
        || module == nullptr) {
        return HexPointer(address);
    }

    wchar_t path[MAX_PATH] = {};
    const DWORD pathLength = GetModuleFileNameW(module, path, MAX_PATH);
    std::wstring moduleName = pathLength > 0 ? std::wstring(path, pathLength) : L"unknown";
    const size_t slash = moduleName.find_last_of(L"\\/");
    if (slash != std::wstring::npos) {
        moduleName.erase(0, slash + 1);
    }

    const uintptr_t rva = reinterpret_cast<uintptr_t>(address) - reinterpret_cast<uintptr_t>(module);
    std::ostringstream oss;
    oss << ToUtf8(moduleName) << "+0x" << std::hex << rva;
    return oss.str();
}

std::string CaptureStackText(void* directCaller)
{
    constexpr size_t kMaxStackDepth = 32;
    std::array<void*, kMaxStackDepth> frames{};
    const USHORT requestedDepth = static_cast<USHORT>(std::clamp(g_config.matrixCaptureStackDepth, 1, 32));
    const USHORT count = RtlCaptureStackBackTrace(2, requestedDepth, frames.data(), nullptr);

    std::ostringstream oss;
    if (directCaller != nullptr) {
        oss << ModuleRelativeAddress(directCaller);
    }
    for (USHORT i = 0; i < count; ++i) {
        const std::string frame = ModuleRelativeAddress(frames[i]);
        if (i == 0 && directCaller != nullptr && frame == ModuleRelativeAddress(directCaller)) {
            continue;
        }
        if (oss.tellp() > 0) {
            oss << ";";
        }
        oss << frame;
    }
    return oss.str();
}

bool MatrixCaptureActive()
{
    const uint64_t endFrame = g_matrixCaptureEndFrame.load(std::memory_order_relaxed);
    return endFrame != 0 && g_frameIndex.load(std::memory_order_relaxed) < endFrame;
}

__declspec(noinline) void CaptureCameraMatrix(
    GLuint program,
    GLint location,
    GLsizei count,
    GLboolean transpose,
    const GLfloat* value,
    const std::string& uniformName,
    void* directCaller)
{
    if (!g_config.matrixCaptureEnabled
        || !MatrixCaptureActive()
        || value == nullptr
        || count <= 0
        || !IsCameraMatrixUniform(uniformName)) {
        return;
    }

    const uint64_t renderFrame = g_frameIndex.load(std::memory_order_relaxed) + 1;
    const uint64_t sequence = g_matrixCaptureSequence.load(std::memory_order_relaxed);
    const std::string callerText = ModuleRelativeAddress(directCaller);
    std::ostringstream siteId;
    siteId << program << ":" << location << ":" << callerText;

    bool logSite = false;
    bool logSample = false;
    uint32_t sampleIndex = 0;
    {
        std::lock_guard lock(g_matrixCaptureMutex);
        ++g_matrixCaptureUploads;

        if (g_matrixCaptureSites.find(siteId.str()) == g_matrixCaptureSites.end()
            && g_matrixCaptureSites.size() < static_cast<size_t>(g_config.matrixCaptureMaxSites)) {
            g_matrixCaptureSites.insert(siteId.str());
            ++g_matrixCaptureSiteLogs;
            logSite = true;
        }

        const uint32_t requestedSamples = static_cast<uint32_t>(g_config.matrixCaptureSamplesPerUniform);
        const uint64_t maxSampleLogs =
            static_cast<uint64_t>(g_config.matrixCaptureMaxSites)
            * static_cast<uint64_t>(g_config.matrixCaptureSamplesPerUniform);
        const uint64_t endFrame = g_matrixCaptureEndFrame.load(std::memory_order_relaxed);
        const uint64_t startFrame = endFrame >= static_cast<uint64_t>(g_config.matrixCaptureFrames)
            ? endFrame - static_cast<uint64_t>(g_config.matrixCaptureFrames)
            : 0;
        const uint64_t elapsedFrames = renderFrame > startFrame ? renderFrame - startFrame - 1 : 0;
        const uint32_t sampleBucket = requestedSamples > 0
            ? static_cast<uint32_t>(std::min<uint64_t>(
                elapsedFrames * requestedSamples / static_cast<uint64_t>(g_config.matrixCaptureFrames),
                requestedSamples - 1))
            : 0;
        const uint32_t sampleBit = requestedSamples > 0 ? (uint32_t{1} << sampleBucket) : 0;
        uint32_t& sampleMask = g_matrixCaptureSampleMasks[UniformKey(program, location)];
        if (sampleBit != 0
            && (sampleMask & sampleBit) == 0
            && g_matrixCaptureSampleLogs < maxSampleLogs) {
            sampleMask |= sampleBit;
            sampleIndex = sampleBucket + 1;
            ++g_matrixCaptureSampleLogs;
            logSample = true;
        }
    }

    if (logSite) {
        Logger::Instance().Write(
            LogLevel::Info,
            "matrix_capture_site sequence=%llu renderFrame=%llu program=%u location=%d name=\"%s\" caller=%s stack=\"%s\"",
            static_cast<unsigned long long>(sequence),
            static_cast<unsigned long long>(renderFrame),
            program,
            location,
            uniformName.c_str(),
            callerText.c_str(),
            CaptureStackText(directCaller).c_str());
    }

    if (logSample) {
        Logger::Instance().Write(
            LogLevel::Info,
            "matrix_capture_sample sequence=%llu renderFrame=%llu sample=%u program=%u location=%d name=\"%s\" count=%d transpose=%u matrix=\"%s\"",
            static_cast<unsigned long long>(sequence),
            static_cast<unsigned long long>(renderFrame),
            sampleIndex,
            program,
            location,
            uniformName.c_str(),
            static_cast<int>(count),
            static_cast<unsigned>(transpose),
            MatrixValuesText(value).c_str());
    }
}

void UpdateMatrixCaptureHotkey(uint64_t frame)
{
    if (!g_config.matrixCaptureEnabled) {
        return;
    }

    const bool keyDown = (GetAsyncKeyState(VK_F9) & 0x8000) != 0;
    const bool wasDown = g_matrixCaptureF9Down.exchange(keyDown, std::memory_order_relaxed);
    if (keyDown && !wasDown) {
        const uint64_t sequence = g_matrixCaptureSequence.fetch_add(1, std::memory_order_relaxed) + 1;
        {
            std::lock_guard lock(g_matrixCaptureMutex);
            g_matrixCaptureUploads = 0;
            g_matrixCaptureSiteLogs = 0;
            g_matrixCaptureSampleLogs = 0;
            g_matrixCaptureSites.clear();
            g_matrixCaptureSampleMasks.clear();
        }
        g_matrixCaptureEndFrame.store(
            frame + static_cast<uint64_t>(g_config.matrixCaptureFrames),
            std::memory_order_relaxed);
        Logger::Instance().Write(
            LogLevel::Info,
            "matrix_capture armed key=F9 sequence=%llu startAfterFrame=%llu frames=%d stackDepth=%d maxSites=%d samplesPerUniform=%d sampling=window_buckets %s",
            static_cast<unsigned long long>(sequence),
            static_cast<unsigned long long>(frame),
            g_config.matrixCaptureFrames,
            g_config.matrixCaptureStackDepth,
            g_config.matrixCaptureMaxSites,
            g_config.matrixCaptureSamplesPerUniform,
            g_openxr != nullptr ? g_openxr->ViewSummaryString().c_str() : "openxrPoseValid=0");
    }

    const uint64_t endFrame = g_matrixCaptureEndFrame.load(std::memory_order_relaxed);
    if (endFrame != 0 && frame >= endFrame) {
        if (g_matrixCaptureEndFrame.exchange(0, std::memory_order_relaxed) != 0) {
            std::lock_guard lock(g_matrixCaptureMutex);
            Logger::Instance().Write(
                LogLevel::Info,
                "matrix_capture complete sequence=%llu endFrame=%llu uploads=%llu sites=%llu samples=%llu %s",
                static_cast<unsigned long long>(g_matrixCaptureSequence.load(std::memory_order_relaxed)),
                static_cast<unsigned long long>(frame),
                static_cast<unsigned long long>(g_matrixCaptureUploads),
                static_cast<unsigned long long>(g_matrixCaptureSiteLogs),
                static_cast<unsigned long long>(g_matrixCaptureSampleLogs),
                g_openxr != nullptr ? g_openxr->ViewSummaryString().c_str() : "openxrPoseValid=0");
        }
    }
}

void UpdateShadowJitterHotkey(uint64_t frame)
{
    if (!g_config.hplShadowJitterControl) {
        return;
    }

    const bool keyDown = (GetAsyncKeyState(VK_F7) & 0x8000) != 0;
    const bool wasDown = g_shadowJitterF7Down.exchange(keyDown, std::memory_order_relaxed);
    if (keyDown && !wasDown) {
        const bool enabled = !g_shadowJitterSuppressed.load(std::memory_order_relaxed);
        g_shadowJitterSuppressed.store(enabled, std::memory_order_relaxed);
        Logger::Instance().Write(
            LogLevel::Warn,
            "shadow_jitter_suppression enabled=%d key=F7 frame=%llu policy=zero_avShadowMapOffsetMul uploads=%llu overrides=%llu",
            enabled ? 1 : 0,
            static_cast<unsigned long long>(frame),
            static_cast<unsigned long long>(g_shadowJitterUploads.load(std::memory_order_relaxed)),
            static_cast<unsigned long long>(g_shadowJitterOverrides.load(std::memory_order_relaxed)));
    }
}

void UpdateRenderDiagnosticHotkey(uint64_t frame)
{
    if (!g_config.renderDiagnosticCapture) {
        return;
    }

    const bool keyDown = (GetAsyncKeyState(VK_F6) & 0x8000) != 0;
    const bool wasDown = g_renderDiagnosticF6Down.exchange(keyDown, std::memory_order_relaxed);
    if (keyDown && !wasDown) {
        const uint64_t sequence = g_renderDiagnosticSequence.fetch_add(1, std::memory_order_relaxed) + 1;
        std::lock_guard lock(g_renderDiagnosticMutex);
        g_renderDiagnosticDrawStream.close();
        g_renderDiagnosticMatrixStream.close();
        g_renderDiagnosticPrograms.clear();
        g_renderDiagnosticBufferSnapshots.clear();
        g_renderDiagnosticDraws = 0;
        g_renderDiagnosticMatrices = 0;
        g_renderDiagnosticPath = LogPath().parent_path()
            / "render-captures"
            / ("capture_" + std::to_string(sequence) + "_frame_" + std::to_string(frame));
        std::error_code ec;
        std::filesystem::create_directories(g_renderDiagnosticPath, ec);
        g_renderDiagnosticDrawStream.open(
            g_renderDiagnosticPath / "draws.csv",
            std::ios::out | std::ios::trunc);
        g_renderDiagnosticMatrixStream.open(
            g_renderDiagnosticPath / "matrices.csv",
            std::ios::out | std::ios::trunc);
        if (g_renderDiagnosticDrawStream) {
            g_renderDiagnosticDrawStream
                << "frame,draw,stage,eye,poseFrame,program,framebuffer,viewportX,viewportY,viewportWidth,viewportHeight,kind,mode,count\n";
        }
        if (g_renderDiagnosticMatrixStream) {
            g_renderDiagnosticMatrixStream
                << "frame,eye,poseFrame,program,location,name,count,transpose,matrix\n";
        }
        g_renderDiagnosticEndFrame.store(
            frame + static_cast<uint64_t>(g_config.renderDiagnosticFrames),
            std::memory_order_relaxed);
        Logger::Instance().Write(
            LogLevel::Warn,
            "render_diag armed key=F6 sequence=%llu startAfterFrame=%llu frames=%d maxPrograms=%d maxDraws=%d path=%s",
            static_cast<unsigned long long>(sequence),
            static_cast<unsigned long long>(frame),
            g_config.renderDiagnosticFrames,
            g_config.renderDiagnosticMaxPrograms,
            g_config.renderDiagnosticMaxDraws,
            g_renderDiagnosticPath.string().c_str());
    }

    const uint64_t endFrame = g_renderDiagnosticEndFrame.load(std::memory_order_relaxed);
    if (endFrame != 0 && frame >= endFrame) {
        if (g_renderDiagnosticEndFrame.exchange(0, std::memory_order_relaxed) != 0) {
            std::lock_guard lock(g_renderDiagnosticMutex);
            g_renderDiagnosticDrawStream.close();
            g_renderDiagnosticMatrixStream.close();
            Logger::Instance().Write(
                LogLevel::Warn,
                "render_diag complete sequence=%llu endFrame=%llu programs=%llu draws=%llu matrices=%llu path=%s",
                static_cast<unsigned long long>(g_renderDiagnosticSequence.load(std::memory_order_relaxed)),
                static_cast<unsigned long long>(frame),
                static_cast<unsigned long long>(g_renderDiagnosticPrograms.size()),
                static_cast<unsigned long long>(g_renderDiagnosticDraws),
                static_cast<unsigned long long>(g_renderDiagnosticMatrices),
                g_renderDiagnosticPath.string().c_str());
        }
    }
}

void APIENTRY HookGlUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value)
{
    if (IsOwnOpenGLWork()) {
        g_originalGlUniformMatrix4fv(location, count, transpose, value);
        return;
    }
    g_uniformMatricesThisFrame.fetch_add(static_cast<uint64_t>(count > 0 ? count : 1), std::memory_order_relaxed);

    const uint32_t sampleIndex = g_matrixSamplesThisFrame.fetch_add(1, std::memory_order_relaxed);
    const GLuint program = g_currentProgram.load(std::memory_order_relaxed);
    const bool captureActive = MatrixCaptureActive();
    const std::string uniformName =
        (captureActive || sampleIndex < static_cast<uint32_t>(g_config.matrixSampleLimitPerFrame))
        ? UniformNameFor(program, location)
        : std::string{};
    if (captureActive) {
        CaptureCameraMatrix(program, location, count, transpose, value, uniformName, _ReturnAddress());
    }
    RecordRenderDiagnosticMatrix(program, location, count, transpose, value, uniformName);

    if (sampleIndex < static_cast<uint32_t>(g_config.matrixSampleLimitPerFrame) && value != nullptr) {
        const MatrixSummary summary = SummarizeMatrix(value);

        const bool logCandidate =
            summary.projectionLike ||
            (!g_config.uniformMatrixProjectionOnly && !uniformName.empty());
        const bool logAllowed =
            logCandidate &&
            g_config.uniformMatrixLogLimit > 0 &&
            g_uniformMatrixLogs.fetch_add(1, std::memory_order_relaxed) <
                static_cast<uint32_t>(g_config.uniformMatrixLogLimit);

        if (logAllowed) {
            Logger::Instance().Write(
                LogLevel::Info,
                "uniform_matrix program=%u location=%d name=\"%s\" count=%d transpose=%u %s",
                program,
                location,
                uniformName.c_str(),
                static_cast<int>(count),
                static_cast<unsigned>(transpose),
                MatrixSummaryText(summary).c_str());
        }

        if (summary.projectionLike) {
            std::lock_guard lock(g_sampleMutex);
            g_lastSamples.uniformProjection = summary;
            g_lastSamples.uniformName = uniformName;
            g_lastSamples.uniformProgram = program;
            g_lastSamples.uniformLocation = location;
        }
    }

    g_originalGlUniformMatrix4fv(location, count, transpose, value);
}

void LogShadowJitterUpload(GLuint program, GLint location, GLsizei count, const GLfloat* value, bool overridden)
{
    g_shadowJitterUploads.fetch_add(1, std::memory_order_relaxed);
    if (overridden) {
        g_shadowJitterOverrides.fetch_add(1, std::memory_order_relaxed);
    }

    const uint32_t logIndex = g_shadowUniformSampleLogs.fetch_add(1, std::memory_order_relaxed);
    if (logIndex < 32 && value != nullptr) {
        Logger::Instance().Write(
            LogLevel::Info,
            "shadow_jitter_upload program=%u location=%d count=%d original=%.9f,%.9f overridden=%d submitted=%.9f,%.9f",
            program,
            location,
            static_cast<int>(count),
            value[0],
            value[1],
            overridden ? 1 : 0,
            overridden ? 0.0f : value[0],
            overridden ? 0.0f : value[1]);
    }
}

void APIENTRY HookGlUniform2f(GLint location, GLfloat v0, GLfloat v1)
{
    if (IsOwnOpenGLWork()) {
        g_originalGlUniform2f(location, v0, v1);
        return;
    }
    const GLuint program = g_currentProgram.load(std::memory_order_relaxed);
    const std::string uniformName = UniformNameFor(program, location);
    const bool isShadowJitter = IsShadowJitterRadiusUniform(uniformName);
    const bool overrideValue = isShadowJitter
        && g_config.hplShadowJitterControl
        && g_shadowJitterSuppressed.load(std::memory_order_relaxed);
    if (isShadowJitter) {
        const GLfloat values[2] = { v0, v1 };
        LogShadowJitterUpload(program, location, 1, values, overrideValue);
    }
    g_originalGlUniform2f(location, overrideValue ? 0.0f : v0, overrideValue ? 0.0f : v1);
}

void APIENTRY HookGlUniform2fv(GLint location, GLsizei count, const GLfloat* value)
{
    if (IsOwnOpenGLWork()) {
        g_originalGlUniform2fv(location, count, value);
        return;
    }
    const GLuint program = g_currentProgram.load(std::memory_order_relaxed);
    const std::string uniformName = UniformNameFor(program, location);
    const bool isShadowJitter = value != nullptr && count > 0 && IsShadowJitterRadiusUniform(uniformName);
    const bool overrideValue = isShadowJitter
        && g_config.hplShadowJitterControl
        && g_shadowJitterSuppressed.load(std::memory_order_relaxed);
    if (isShadowJitter) {
        LogShadowJitterUpload(program, location, count, value, overrideValue);
    }
    if (overrideValue && count <= 1024) {
        const std::vector<GLfloat> zeros(static_cast<size_t>(count) * 2, 0.0f);
        g_originalGlUniform2fv(location, count, zeros.data());
        return;
    }
    g_originalGlUniform2fv(location, count, value);
}

GLint APIENTRY HookGlGetUniformLocation(GLuint program, const GLchar* uniformName)
{
    if (IsOwnOpenGLWork()) {
        return g_originalGlGetUniformLocation(program, uniformName);
    }
    const GLint location = g_originalGlGetUniformLocation(program, uniformName);
    if (location >= 0 && uniformName != nullptr) {
        {
            std::lock_guard lock(g_uniformNameMutex);
            g_uniformNames[UniformKey(program, location)] = uniformName;
        }

        const bool shadowRelevant = IsShadowRelevantUniform(uniformName);
        const uint32_t logIndex = g_uniformNameLogs.fetch_add(1, std::memory_order_relaxed);
        const bool generalLogAllowed = logIndex < static_cast<uint32_t>(g_config.uniformNameLogLimit);
        const bool shadowLogAllowed = shadowRelevant
            && g_shadowUniformNameLogs.fetch_add(1, std::memory_order_relaxed) < 256;
        if (generalLogAllowed || shadowLogAllowed) {
            Logger::Instance().Write(
                LogLevel::Info,
                "uniform_location program=%u location=%d name=\"%s\" shadowRelevant=%d",
                program,
                location,
                uniformName,
                shadowRelevant ? 1 : 0);
        }
    }
    return location;
}

void APIENTRY HookGlUseProgram(GLuint program)
{
    if (IsOwnOpenGLWork()) {
        g_originalGlUseProgram(program);
        return;
    }
    g_totalProgramUses.fetch_add(1, std::memory_order_relaxed);
    g_currentProgram.store(program, std::memory_order_relaxed);
    g_originalGlUseProgram(program);
    DumpRenderDiagnosticProgram(program);
}

void APIENTRY HookGlBindFramebuffer(GLenum target, GLuint framebuffer)
{
    if (IsOwnOpenGLWork()) {
        g_originalGlBindFramebuffer(target, framebuffer);
        return;
    }
    g_framebufferBindsThisFrame.fetch_add(1, std::memory_order_relaxed);
    g_totalFramebufferBinds.fetch_add(1, std::memory_order_relaxed);
    g_currentFramebuffer.store(framebuffer, std::memory_order_relaxed);
    if (g_postEffectResourceCapture.active) {
        bool found = false;
        for (size_t i = 0; i < g_postEffectResourceCapture.framebufferCount; ++i) {
            const auto& resource = g_postEffectResourceCapture.framebuffers[i];
            if (resource.target == target && resource.framebuffer == framebuffer) {
                found = true;
                break;
            }
        }
        if (!found) {
            if (g_postEffectResourceCapture.framebufferCount
                < g_postEffectResourceCapture.framebuffers.size()) {
                g_postEffectResourceCapture.framebuffers[
                    g_postEffectResourceCapture.framebufferCount++] = {target, framebuffer};
                g_postEffectFramebufferResources.fetch_add(1, std::memory_order_relaxed);
            } else {
                g_postEffectResourceCapture.framebufferOverflow = true;
            }
        }
    }
    g_originalGlBindFramebuffer(target, framebuffer);
}

void APIENTRY HookGlBindTexture(GLenum target, GLuint texture)
{
    if (IsOwnOpenGLWork()) {
        g_originalGlBindTexture(target, texture);
        return;
    }
    g_originalGlBindTexture(target, texture);
    ObserveHPLSSAOTemporalGLBind(target, texture);
    if (!g_postEffectResourceCapture.active || texture == 0
        || g_glGetTexLevelParameteriv == nullptr) {
        return;
    }

    for (size_t i = 0; i < g_postEffectResourceCapture.textureCount; ++i) {
        const auto& resource = g_postEffectResourceCapture.textures[i];
        if (resource.target == target && resource.texture == texture) return;
    }
    if (g_postEffectResourceCapture.textureCount >= g_postEffectResourceCapture.textures.size()) {
        g_postEffectResourceCapture.textureOverflow = true;
        return;
    }

    const GLenum queryTarget = target == kGLTextureCubeMap
        ? kGLTextureCubeMapPositiveX
        : target;
    GLint width = 0;
    GLint height = 0;
    GLint depth = 0;
    GLint internalFormat = 0;
    g_glGetTexLevelParameteriv(queryTarget, 0, kGLTextureWidth, &width);
    g_glGetTexLevelParameteriv(queryTarget, 0, kGLTextureHeight, &height);
    g_glGetTexLevelParameteriv(queryTarget, 0, kGLTextureDepth, &depth);
    g_glGetTexLevelParameteriv(queryTarget, 0, kGLTextureInternalFormat, &internalFormat);
    g_postEffectResourceCapture.textures[g_postEffectResourceCapture.textureCount++] = {
        target,
        texture,
        width,
        height,
        depth,
        internalFormat,
    };
    g_postEffectTextureResources.fetch_add(1, std::memory_order_relaxed);
}

BOOL WINAPI HookWglSwapIntervalEXT(int interval)
{
    if (g_config.forceDisableVsync) {
        Logger::Instance().Write(LogLevel::Info, "wgl_swap_interval override requested=%d forced=0", interval);
        return g_originalWglSwapIntervalEXT(0);
    }
    return g_originalWglSwapIntervalEXT(interval);
}

bool HookTarget(void* target, void* detour, void** original, const char* label)
{
    if (target == nullptr) {
        Logger::Instance().Write(LogLevel::Warn, "hook_install skipped label=%s reason=null_target", label);
        return false;
    }

    MH_STATUS status = MH_CreateHook(target, detour, original);
    if (status != MH_OK && status != MH_ERROR_ALREADY_CREATED) {
        Logger::Instance().Write(
            LogLevel::Warn,
            "hook_install failed label=%s target=%s status=%s",
            label,
            HexPointer(target).c_str(),
            MH_StatusToString(status));
        return false;
    }

    status = MH_EnableHook(target);
    if (status != MH_OK && status != MH_ERROR_ENABLED) {
        Logger::Instance().Write(
            LogLevel::Warn,
            "hook_enable failed label=%s target=%s status=%s",
            label,
            HexPointer(target).c_str(),
            MH_StatusToString(status));
        return false;
    }

    Logger::Instance().Write(LogLevel::Info, "hook_install ok label=%s target=%s", label, HexPointer(target).c_str());
    return true;
}

bool HookExport(HMODULE module, const char* name, void* detour, void** original)
{
    if (module == nullptr) {
        Logger::Instance().Write(LogLevel::Warn, "hook_install skipped export=%s reason=null_module", name);
        return false;
    }

    FARPROC target = GetProcAddress(module, name);
    return HookTarget(reinterpret_cast<void*>(target), detour, original, name);
}

void MaybeInstallExtensionHook(const char* name, PROC proc)
{
    if (!IsValidWglProc(proc) || name == nullptr) {
        return;
    }

    void* detour = nullptr;
    void** original = nullptr;

    if (g_config.hookUniformMatrices &&
        (NameEquals(name, "glUniformMatrix4fv") || NameEquals(name, "glUniformMatrix4fvARB"))) {
        if (g_originalGlUniformMatrix4fv != nullptr) {
            return;
        }
        detour = reinterpret_cast<void*>(&HookGlUniformMatrix4fv);
        original = reinterpret_cast<void**>(&g_originalGlUniformMatrix4fv);
    } else if (g_config.hplShadowJitterControl &&
               (NameEquals(name, "glUniform2f") || NameEquals(name, "glUniform2fARB"))) {
        if (g_originalGlUniform2f != nullptr) {
            return;
        }
        detour = reinterpret_cast<void*>(&HookGlUniform2f);
        original = reinterpret_cast<void**>(&g_originalGlUniform2f);
    } else if (g_config.hplShadowJitterControl &&
               (NameEquals(name, "glUniform2fv") || NameEquals(name, "glUniform2fvARB"))) {
        if (g_originalGlUniform2fv != nullptr) {
            return;
        }
        detour = reinterpret_cast<void*>(&HookGlUniform2fv);
        original = reinterpret_cast<void**>(&g_originalGlUniform2fv);
    } else if (g_config.hookUniformMatrices &&
               (NameEquals(name, "glGetUniformLocation") || NameEquals(name, "glGetUniformLocationARB"))) {
        if (g_originalGlGetUniformLocation != nullptr) {
            return;
        }
        detour = reinterpret_cast<void*>(&HookGlGetUniformLocation);
        original = reinterpret_cast<void**>(&g_originalGlGetUniformLocation);
    } else if (g_config.hookUniformMatrices &&
               (NameEquals(name, "glUseProgram") || NameEquals(name, "glUseProgramObjectARB"))) {
        if (g_originalGlUseProgram != nullptr) {
            return;
        }
        detour = reinterpret_cast<void*>(&HookGlUseProgram);
        original = reinterpret_cast<void**>(&g_originalGlUseProgram);
    } else if ((g_config.hookFramebuffer || g_config.hplPostEffectResourceProbe) &&
               (NameEquals(name, "glBindFramebuffer") || NameEquals(name, "glBindFramebufferEXT"))) {
        if (g_originalGlBindFramebuffer != nullptr) {
            return;
        }
        detour = reinterpret_cast<void*>(&HookGlBindFramebuffer);
        original = reinterpret_cast<void**>(&g_originalGlBindFramebuffer);
    } else if (g_occlusionQueryTelemetryEnabled
        && (NameEquals(name, "glBeginQuery") || NameEquals(name, "glBeginQueryARB"))) {
        if (g_originalGlBeginQuery != nullptr) return;
        detour = reinterpret_cast<void*>(&HookGlBeginQuery);
        original = reinterpret_cast<void**>(&g_originalGlBeginQuery);
    } else if (g_occlusionQueryTelemetryEnabled
        && (NameEquals(name, "glEndQuery") || NameEquals(name, "glEndQueryARB"))) {
        if (g_originalGlEndQuery != nullptr) return;
        detour = reinterpret_cast<void*>(&HookGlEndQuery);
        original = reinterpret_cast<void**>(&g_originalGlEndQuery);
    } else if (g_occlusionQueryTelemetryEnabled
        && (NameEquals(name, "glGetQueryObjectiv") || NameEquals(name, "glGetQueryObjectivARB"))) {
        if (g_originalGlGetQueryObjectiv != nullptr) return;
        detour = reinterpret_cast<void*>(&HookGlGetQueryObjectiv);
        original = reinterpret_cast<void**>(&g_originalGlGetQueryObjectiv);
    } else if (g_occlusionQueryTelemetryEnabled
        && (NameEquals(name, "glGetQueryObjectuiv") || NameEquals(name, "glGetQueryObjectuivARB"))) {
        if (g_originalGlGetQueryObjectuiv != nullptr) return;
        detour = reinterpret_cast<void*>(&HookGlGetQueryObjectuiv);
        original = reinterpret_cast<void**>(&g_originalGlGetQueryObjectuiv);
    } else if (g_occlusionQueryTelemetryEnabled && NameEquals(name, "glGetQueryObjectui64v")) {
        if (g_originalGlGetQueryObjectui64v != nullptr) return;
        detour = reinterpret_cast<void*>(&HookGlGetQueryObjectui64v);
        original = reinterpret_cast<void**>(&g_originalGlGetQueryObjectui64v);
    } else if (NameEquals(name, "wglSwapIntervalEXT")) {
        if (g_originalWglSwapIntervalEXT != nullptr) {
            return;
        }
        detour = reinterpret_cast<void*>(&HookWglSwapIntervalEXT);
        original = reinterpret_cast<void**>(&g_originalWglSwapIntervalEXT);
    }

    if (detour == nullptr || original == nullptr) {
        return;
    }

    std::lock_guard lock(g_extensionHookMutex);
    void* target = reinterpret_cast<void*>(proc);
    if (g_hookedExtensionTargets.find(target) != g_hookedExtensionTargets.end()) {
        return;
    }

    if (HookTarget(target, detour, original, name)) {
        g_hookedExtensionTargets.insert(target);
    }
}

void ResolveRenderDiagnosticFunctions()
{
    if ((!g_config.renderDiagnosticCapture && !g_config.hplReflectionFadeControl)
        || g_originalWglGetProcAddress == nullptr) {
        return;
    }
    const auto resolve = [](const char* name) -> PROC {
        const PROC proc = g_originalWglGetProcAddress(name);
        return IsValidWglProc(proc) ? proc : nullptr;
    };
    if (g_glGetProgramiv == nullptr) {
        g_glGetProgramiv = reinterpret_cast<GlGetProgramivFn>(resolve("glGetProgramiv"));
    }
    if (g_glGetActiveUniform == nullptr) {
        g_glGetActiveUniform = reinterpret_cast<GlGetActiveUniformFn>(resolve("glGetActiveUniform"));
    }
    if (g_glGetAttachedShaders == nullptr) {
        g_glGetAttachedShaders = reinterpret_cast<GlGetAttachedShadersFn>(resolve("glGetAttachedShaders"));
    }
    if (g_glGetShaderiv == nullptr) {
        g_glGetShaderiv = reinterpret_cast<GlGetShaderivFn>(resolve("glGetShaderiv"));
    }
    if (g_glGetShaderSource == nullptr) {
        g_glGetShaderSource = reinterpret_cast<GlGetShaderSourceFn>(resolve("glGetShaderSource"));
    }
    if (g_glGetActiveUniformsiv == nullptr) {
        g_glGetActiveUniformsiv = reinterpret_cast<GlGetActiveUniformsivFn>(resolve("glGetActiveUniformsiv"));
    }
    if (g_glGetActiveUniformBlockiv == nullptr) {
        g_glGetActiveUniformBlockiv = reinterpret_cast<GlGetActiveUniformBlockivFn>(resolve("glGetActiveUniformBlockiv"));
    }
    if (g_glGetActiveUniformBlockName == nullptr) {
        g_glGetActiveUniformBlockName = reinterpret_cast<GlGetActiveUniformBlockNameFn>(resolve("glGetActiveUniformBlockName"));
    }
    if (g_glGetIntegeriV == nullptr) {
        g_glGetIntegeriV = reinterpret_cast<GlGetIntegeriVFn>(resolve("glGetIntegeri_v"));
    }
    if (g_glGetInteger64iV == nullptr) {
        g_glGetInteger64iV = reinterpret_cast<GlGetInteger64iVFn>(resolve("glGetInteger64i_v"));
    }
    if (g_glGetNamedBufferSubData == nullptr) {
        g_glGetNamedBufferSubData = reinterpret_cast<GlGetNamedBufferSubDataFn>(resolve("glGetNamedBufferSubData"));
    }
    if (g_glNamedBufferSubData == nullptr) {
        g_glNamedBufferSubData = reinterpret_cast<GlNamedBufferSubDataFn>(resolve("glNamedBufferSubData"));
    }
}

void InstallKnownExtensionHooks()
{
    if (g_originalWglGetProcAddress == nullptr) {
        return;
    }

    const char* names[] = {
        "glUniformMatrix4fv",
        "glUniformMatrix4fvARB",
        "glUniform2f",
        "glUniform2fARB",
        "glUniform2fv",
        "glUniform2fvARB",
        "glGetUniformLocation",
        "glGetUniformLocationARB",
        "glUseProgram",
        "glUseProgramObjectARB",
        "glBindFramebuffer",
        "glBindFramebufferEXT",
        "glBeginQuery",
        "glBeginQueryARB",
        "glEndQuery",
        "glEndQueryARB",
        "glGetQueryObjectiv",
        "glGetQueryObjectivARB",
        "glGetQueryObjectuiv",
        "glGetQueryObjectuivARB",
        "glGetQueryObjectui64v",
        "wglSwapIntervalEXT",
    };

    for (const char* name : names) {
        MaybeInstallExtensionHook(name, g_originalWglGetProcAddress(name));
    }
    ResolveRenderDiagnosticFunctions();
}

void LogContextInfo(HDC hdc, HGLRC glContext)
{
    if (glContext == nullptr) {
        return;
    }

    bool changed = false;
    {
        std::lock_guard lock(g_sampleMutex);
        changed = hdc != g_lastLoggedHdc || glContext != g_lastLoggedGlContext;
        if (changed) {
            g_lastLoggedHdc = hdc;
            g_lastLoggedGlContext = glContext;
        }
    }

    if (!changed) {
        return;
    }

    InstallKnownExtensionHooks();

    Logger::Instance().Write(
        LogLevel::Info,
        "gl_context_info hdc=%s hglrc=%s vendor=\"%s\" renderer=\"%s\" version=\"%s\" glsl=\"%s\"",
        HexPointer(hdc).c_str(),
        HexPointer(glContext).c_str(),
        SafeGlString(kGLVendor),
        SafeGlString(kGLRenderer),
        SafeGlString(kGLVersion),
        SafeGlString(kGLShadingLanguageVersion));

    if (g_openxr != nullptr) {
        g_openxr->OnOpenGLContext(hdc, glContext);
    }
}

void QueryInteger(GLenum name, GLint* outValue, size_t count)
{
    if (g_glGetIntegerv == nullptr || outValue == nullptr) {
        return;
    }

    for (size_t i = 0; i < count; ++i) {
        outValue[i] = 0;
    }
    g_glGetIntegerv(name, outValue);
}

void QueryFloat(GLenum name, GLfloat* outValue, size_t count)
{
    if (g_glGetFloatv == nullptr || outValue == nullptr) {
        return;
    }

    for (size_t i = 0; i < count; ++i) {
        outValue[i] = 0.0f;
    }
    g_glGetFloatv(name, outValue);
}

void LogFrameSummary(HDC hdc)
{
    const uint64_t frame = g_frameIndex.fetch_add(1, std::memory_order_relaxed) + 1;
    g_swapCount.fetch_add(1, std::memory_order_relaxed);
    PollReflectionFadeControl();

    const HGLRC glContext = wglGetCurrentContext();
    const HDC currentHdc = wglGetCurrentDC();
    UpdateHPLPresentationBridge(frame);
    if (g_openxr != nullptr) {
        g_openxr->OnFrameBoundary(currentHdc != nullptr ? currentHdc : hdc, glContext, frame);
    }
    UpdateHPLPlayerState(frame);
    UpdateHPLInputBridge(frame);
    UpdateHPLHandsBridge(frame);
    UpdateMatrixCaptureHotkey(frame);
    UpdateShadowJitterHotkey(frame);
    UpdateRenderDiagnosticHotkey(frame);

    if ((frame % static_cast<uint64_t>(g_config.frameSummaryInterval)) != 0) {
        g_drawElementsThisFrame.store(0, std::memory_order_relaxed);
        g_drawArraysThisFrame.store(0, std::memory_order_relaxed);
        g_matrixLoadsThisFrame.store(0, std::memory_order_relaxed);
        g_uniformMatricesThisFrame.store(0, std::memory_order_relaxed);
        g_viewportCallsThisFrame.store(0, std::memory_order_relaxed);
        g_framebufferBindsThisFrame.store(0, std::memory_order_relaxed);
        g_matrixSamplesThisFrame.store(0, std::memory_order_relaxed);
        return;
    }

    LogContextInfo(currentHdc != nullptr ? currentHdc : hdc, glContext);

    GLint viewport[4] = {};
    GLint program = static_cast<GLint>(g_currentProgram.load(std::memory_order_relaxed));
    GLint framebuffer = static_cast<GLint>(g_currentFramebuffer.load(std::memory_order_relaxed));
    QueryInteger(kGLViewport, viewport, 4);
    if (g_glGetIntegerv != nullptr) {
        GLint queriedProgram = 0;
        g_glGetIntegerv(kGLCurrentProgram, &queriedProgram);
        program = queriedProgram;

        GLint queriedFramebuffer = 0;
        g_glGetIntegerv(kGLFramebufferBinding, &queriedFramebuffer);
        framebuffer = queriedFramebuffer;
    }

    GLfloat projection[16] = {};
    QueryFloat(kGLProjectionMatrix, projection, 16);
    const MatrixSummary liveProjection = SummarizeMatrix(projection);
    if (liveProjection.projectionLike) {
        std::lock_guard lock(g_sampleMutex);
        g_lastSamples.fixedProjection = liveProjection;
    }

    LastSamples samplesCopy;
    {
        std::lock_guard lock(g_sampleMutex);
        samplesCopy = g_lastSamples;
    }

    Logger::Instance().Write(
        LogLevel::Info,
        "frame_summary frame=%llu hdc=%s hglrc=%s viewport=%d,%d,%d,%d program=%d framebuffer=%d drawsElements=%llu drawsArrays=%llu matrixLoads=%llu uniformMatrices=%llu viewportCalls=%llu framebufferBinds=%llu fixedProjection={%s} uniformProjectionName=\"%s\" uniformProjectionProgram=%u uniformProjectionLocation=%d uniformProjection={%s} %s",
        static_cast<unsigned long long>(frame),
        HexPointer(currentHdc != nullptr ? currentHdc : hdc).c_str(),
        HexPointer(glContext).c_str(),
        viewport[0],
        viewport[1],
        viewport[2],
        viewport[3],
        program,
        framebuffer,
        static_cast<unsigned long long>(g_drawElementsThisFrame.exchange(0, std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_drawArraysThisFrame.exchange(0, std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_matrixLoadsThisFrame.exchange(0, std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_uniformMatricesThisFrame.exchange(0, std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_viewportCallsThisFrame.exchange(0, std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_framebufferBindsThisFrame.exchange(0, std::memory_order_relaxed)),
        MatrixSummaryText(samplesCopy.fixedProjection).c_str(),
        samplesCopy.uniformName.c_str(),
        samplesCopy.uniformProgram,
        samplesCopy.uniformLocation,
        MatrixSummaryText(samplesCopy.uniformProjection).c_str(),
        g_openxr != nullptr ? g_openxr->SummaryString().c_str() : "openxrEnabled=0");

    g_matrixSamplesThisFrame.store(0, std::memory_order_relaxed);

}

BOOL WINAPI HookSwapBuffers(HDC hdc)
{
    LogFrameSummary(hdc);
    return g_originalSwapBuffers(hdc);
}

BOOL WINAPI HookWglMakeCurrent(HDC hdc, HGLRC glContext)
{
    const BOOL result = g_originalWglMakeCurrent(hdc, glContext);
    if (result) {
        g_wglMakeCurrentCount.fetch_add(1, std::memory_order_relaxed);
        g_renderThreadId.store(GetCurrentThreadId(), std::memory_order_relaxed);
        LogContextInfo(hdc, glContext);
    }
    return result;
}

PROC WINAPI HookWglGetProcAddress(LPCSTR name)
{
    PROC proc = g_originalWglGetProcAddress(name);
    MaybeInstallExtensionHook(name, proc);
    return proc;
}

void APIENTRY HookGlBeginQuery(GLenum target, GLuint query)
{
    if (g_originalGlBeginQuery == nullptr) return;
    g_originalGlBeginQuery(target, query);
    if (!g_occlusionQueryTelemetryEnabled || IsOwnOpenGLWork() || query == 0) return;

    const uint64_t frame = GetOpenGLRenderFrameHint();
    const HPLDualRenderPass pass = GetActiveHPLDualRenderPass();
    bool sameFrameReuse = false;
    bool targetConflict = false;
    {
        std::lock_guard lock(g_occlusionQueryMutex);
        const auto active = g_activeOcclusionQueries.find(target);
        targetConflict = active != g_activeOcclusionQueries.end() && active->second != query;
        g_activeOcclusionQueries[target] = query;

        auto state = g_occlusionQueryStates.find(query);
        if (state == g_occlusionQueryStates.end()) {
            if (g_occlusionQueryStates.size() >= kMaxOcclusionQueryStates) {
                g_occlusionQueryStateOverflows.fetch_add(1, std::memory_order_relaxed);
                return;
            }
            state = g_occlusionQueryStates.emplace(query, OcclusionQueryState{}).first;
        }
        OcclusionQueryState& queryState = state->second;
        if (pass == HPLDualRenderPass::FirstEye) {
            queryState.firstEyeFrame = frame;
        } else if (pass == HPLDualRenderPass::ReplayEye) {
            queryState.replayEyeFrame = frame;
            sameFrameReuse = queryState.firstEyeFrame == frame;
        }
        queryState.target = target;
        queryState.active = true;
        queryState.lastBeginFrame = frame;
        queryState.lastPass = pass;
        ++queryState.begins;
    }

    const uint64_t begins = g_occlusionQueryBegins.fetch_add(1, std::memory_order_relaxed) + 1;
    if (pass == HPLDualRenderPass::FirstEye) {
        g_occlusionQueryFirstEyeBegins.fetch_add(1, std::memory_order_relaxed);
    } else if (pass == HPLDualRenderPass::ReplayEye) {
        g_occlusionQueryReplayEyeBegins.fetch_add(1, std::memory_order_relaxed);
    }
    if (targetConflict) {
        g_occlusionQueryTargetConflicts.fetch_add(1, std::memory_order_relaxed);
    }
    uint64_t sameFrameReuses = 0;
    if (sameFrameReuse) {
        sameFrameReuses = g_occlusionQuerySameFrameReuses.fetch_add(1, std::memory_order_relaxed) + 1;
    }
    const uint64_t interval = static_cast<uint64_t>(
        std::max(g_config.hplCompatibilityLogInterval, 1));
    if (targetConflict || (sameFrameReuse && (sameFrameReuses <= 8 || sameFrameReuses % interval == 0))) {
        Logger::Instance().Write(
            targetConflict ? LogLevel::Warn : LogLevel::Info,
            "hpl_occlusion_query begin=%llu frame=%llu query=%u target=0x%x stage=%s pass=%s sameFrameReuse=%d sameFrameReuses=%llu targetConflict=%d",
            static_cast<unsigned long long>(begins),
            static_cast<unsigned long long>(frame),
            query,
            target,
            GetHPLRenderStageName(GetActiveHPLRenderStage()),
            GetHPLDualRenderPassName(pass),
            sameFrameReuse ? 1 : 0,
            static_cast<unsigned long long>(sameFrameReuses),
            targetConflict ? 1 : 0);
    }
}

void APIENTRY HookGlEndQuery(GLenum target)
{
    if (g_originalGlEndQuery == nullptr) return;
    if (g_occlusionQueryTelemetryEnabled && !IsOwnOpenGLWork()) {
        bool matched = false;
        {
            std::lock_guard lock(g_occlusionQueryMutex);
            const auto active = g_activeOcclusionQueries.find(target);
            if (active != g_activeOcclusionQueries.end()) {
                const auto state = g_occlusionQueryStates.find(active->second);
                if (state != g_occlusionQueryStates.end()) {
                    state->second.active = false;
                    matched = true;
                }
                g_activeOcclusionQueries.erase(active);
            }
        }
        g_occlusionQueryEnds.fetch_add(1, std::memory_order_relaxed);
        if (!matched) {
            g_occlusionQueryUnmatchedEnds.fetch_add(1, std::memory_order_relaxed);
        }
    }
    g_originalGlEndQuery(target);
}

void ObserveQueryResult(GLuint query, GLenum pname)
{
    if (!g_occlusionQueryTelemetryEnabled || IsOwnOpenGLWork() || query == 0) return;
    g_occlusionQueryResults.fetch_add(1, std::memory_order_relaxed);
    if (pname == kGLQueryResultAvailable) {
        g_occlusionQueryAvailabilityChecks.fetch_add(1, std::memory_order_relaxed);
    }
    std::lock_guard lock(g_occlusionQueryMutex);
    const auto state = g_occlusionQueryStates.find(query);
    if (state != g_occlusionQueryStates.end()) {
        ++state->second.results;
    }
}

void APIENTRY HookGlGetQueryObjectiv(GLuint query, GLenum pname, GLint* value)
{
    if (g_originalGlGetQueryObjectiv == nullptr) return;
    g_originalGlGetQueryObjectiv(query, pname, value);
    ObserveQueryResult(query, pname);
}

void APIENTRY HookGlGetQueryObjectuiv(GLuint query, GLenum pname, GLuint* value)
{
    if (g_originalGlGetQueryObjectuiv == nullptr) return;
    g_originalGlGetQueryObjectuiv(query, pname, value);
    ObserveQueryResult(query, pname);
}

void APIENTRY HookGlGetQueryObjectui64v(GLuint query, GLenum pname, uint64_t* value)
{
    if (g_originalGlGetQueryObjectui64v == nullptr) return;
    g_originalGlGetQueryObjectui64v(query, pname, value);
    ObserveQueryResult(query, pname);
}

void APIENTRY HookGlCopyTexSubImage2D(
    GLenum target,
    GLint level,
    GLint xOffset,
    GLint yOffset,
    GLint x,
    GLint y,
    GLsizei width,
    GLsizei height)
{
    if (g_occlusionQueryTelemetryEnabled && !IsOwnOpenGLWork()) {
        GLuint texture = 0;
        if (target == GL_TEXTURE_2D && g_glGetIntegerv != nullptr) {
            GLint binding = 0;
            g_glGetIntegerv(kGLTextureBinding2D, &binding);
            if (binding > 0) texture = static_cast<GLuint>(binding);
        }
        if (texture == 0) {
            g_framebufferCopyTextureReadFailures.fetch_add(1, std::memory_order_relaxed);
        }

        const uint64_t frame = GetOpenGLRenderFrameHint();
        const HPLDualRenderPass pass = GetActiveHPLDualRenderPass();
        bool crossEyeReuse = false;
        if (texture != 0) {
            std::lock_guard lock(g_framebufferCopyMutex);
            auto state = g_framebufferCopyStates.find(texture);
            if (state == g_framebufferCopyStates.end()) {
                if (g_framebufferCopyStates.size() < kMaxFramebufferCopyStates) {
                    state = g_framebufferCopyStates.emplace(
                        texture, FramebufferCopyState{}).first;
                } else {
                    g_framebufferCopyStateOverflows.fetch_add(1, std::memory_order_relaxed);
                }
            }
            if (state != g_framebufferCopyStates.end()) {
                crossEyeReuse =
                    (pass == HPLDualRenderPass::FirstEye && state->second.replayEyeFrame == frame)
                    || (pass == HPLDualRenderPass::ReplayEye && state->second.firstEyeFrame == frame);
                if (pass == HPLDualRenderPass::FirstEye) state->second.firstEyeFrame = frame;
                if (pass == HPLDualRenderPass::ReplayEye) state->second.replayEyeFrame = frame;
                ++state->second.copies;
            }
        }

        const uint64_t copies = g_framebufferCopies.fetch_add(1, std::memory_order_relaxed) + 1;
        if (width > 0 && height > 0) {
            g_framebufferCopyPixels.fetch_add(
                static_cast<uint64_t>(width) * static_cast<uint64_t>(height),
                std::memory_order_relaxed);
        }
        if (pass == HPLDualRenderPass::FirstEye) {
            g_framebufferCopyFirstEye.fetch_add(1, std::memory_order_relaxed);
        } else if (pass == HPLDualRenderPass::ReplayEye) {
            g_framebufferCopyReplayEye.fetch_add(1, std::memory_order_relaxed);
        }
        uint64_t crossEyeReuses = g_framebufferCopyCrossEyeReuses.load(std::memory_order_relaxed);
        if (crossEyeReuse) {
            crossEyeReuses = g_framebufferCopyCrossEyeReuses.fetch_add(
                1, std::memory_order_relaxed) + 1;
        }
        if (crossEyeReuse && crossEyeReuses <= 16) {
            Logger::Instance().Write(
                LogLevel::Info,
                "hpl_framebuffer_copy copy=%llu frame=%llu texture=%u target=0x%x level=%d src=%d,%d,%d,%d dst=%d,%d pass=%s crossEyeReuse=1 crossEyeReuses=%llu policy=observe_shared_scene_color_scratch",
                static_cast<unsigned long long>(copies),
                static_cast<unsigned long long>(frame),
                texture,
                static_cast<unsigned>(target),
                level,
                x,
                y,
                width,
                height,
                xOffset,
                yOffset,
                GetHPLDualRenderPassName(pass),
                static_cast<unsigned long long>(crossEyeReuses));
        }
    }

    g_originalGlCopyTexSubImage2D(
        target, level, xOffset, yOffset, x, y, width, height);
}

void APIENTRY HookGlMatrixMode(GLenum mode)
{
    if (IsOwnOpenGLWork()) {
        g_originalGlMatrixMode(mode);
        return;
    }
    g_currentMatrixMode.store(mode, std::memory_order_relaxed);
    g_originalGlMatrixMode(mode);
}

void APIENTRY HookGlLoadMatrixf(const GLfloat* matrix)
{
    if (IsOwnOpenGLWork()) {
        g_originalGlLoadMatrixf(matrix);
        return;
    }
    g_matrixLoadsThisFrame.fetch_add(1, std::memory_order_relaxed);

    GLenum matrixMode = g_currentMatrixMode.load(std::memory_order_relaxed);
    if (g_glGetIntegerv != nullptr) {
        GLint queriedMode = 0;
        g_glGetIntegerv(kGLMatrixMode, &queriedMode);
        matrixMode = static_cast<GLenum>(queriedMode);
        g_currentMatrixMode.store(matrixMode, std::memory_order_relaxed);
    }

    const uint32_t sampleIndex = g_matrixSamplesThisFrame.fetch_add(1, std::memory_order_relaxed);
    if (matrix != nullptr && sampleIndex < static_cast<uint32_t>(g_config.matrixSampleLimitPerFrame)) {
        const MatrixSummary summary = SummarizeMatrix(matrix);
        const bool projectionMode = matrixMode == kGLProjection;
        if (projectionMode || summary.projectionLike) {
            Logger::Instance().Write(
                LogLevel::Info,
                "matrix_upload kind=%s matrixMode=0x%04x %s",
                projectionMode ? "fixed_projection" : "fixed_unknown_projection_like",
                static_cast<unsigned>(matrixMode),
                MatrixSummaryText(summary).c_str());
        }

        if (projectionMode || summary.projectionLike) {
            std::lock_guard lock(g_sampleMutex);
            g_lastSamples.fixedProjection = summary;
        }
    }

    g_originalGlLoadMatrixf(matrix);
}

void APIENTRY HookGlViewport(GLint x, GLint y, GLsizei width, GLsizei height)
{
    if (IsOwnOpenGLWork()) {
        g_originalGlViewport(x, y, width, height);
        return;
    }
    g_viewportCallsThisFrame.fetch_add(1, std::memory_order_relaxed);
    g_totalViewportCalls.fetch_add(1, std::memory_order_relaxed);
    g_currentViewportX.store(x, std::memory_order_relaxed);
    g_currentViewportY.store(y, std::memory_order_relaxed);
    g_currentViewportWidth.store(width, std::memory_order_relaxed);
    g_currentViewportHeight.store(height, std::memory_order_relaxed);
    g_originalGlViewport(x, y, width, height);
}

void APIENTRY HookGlDrawElements(GLenum mode, GLsizei count, GLenum type, const void* indices)
{
    if (IsOwnOpenGLWork()) {
        g_originalGlDrawElements(mode, count, type, indices);
        return;
    }
    g_drawElementsThisFrame.fetch_add(1, std::memory_order_relaxed);
    g_totalDrawElements.fetch_add(1, std::memory_order_relaxed);
    RecordRenderDiagnosticDraw("elements", mode, count);
    RecordTerminalDrawState("elements", mode, count);
    const bool terminalScissorDisabled = DisableOffscreenTerminalScissor();
    const ReflectionFadePatch reflectionPatch = ApplyReflectionFadeBypass(
        g_currentProgram.load(std::memory_order_relaxed));
    g_originalGlDrawElements(mode, count, type, indices);
    if (terminalScissorDisabled) glEnable(GL_SCISSOR_TEST);
    RestoreReflectionFade(reflectionPatch);
}

void APIENTRY HookGlDrawArrays(GLenum mode, GLint first, GLsizei count)
{
    if (IsOwnOpenGLWork()) {
        g_originalGlDrawArrays(mode, first, count);
        return;
    }
    g_drawArraysThisFrame.fetch_add(1, std::memory_order_relaxed);
    g_totalDrawArrays.fetch_add(1, std::memory_order_relaxed);
    RecordRenderDiagnosticDraw("arrays", mode, count);
    RecordTerminalDrawState("arrays", mode, count);
    const bool terminalScissorDisabled = DisableOffscreenTerminalScissor();
    const ReflectionFadePatch reflectionPatch = ApplyReflectionFadeBypass(
        g_currentProgram.load(std::memory_order_relaxed));
    g_originalGlDrawArrays(mode, first, count);
    if (terminalScissorDisabled) glEnable(GL_SCISSOR_TEST);
    RestoreReflectionFade(reflectionPatch);
}

void APIENTRY HookGlClear(GLbitfield mask)
{
    if (IsOwnOpenGLWork()) {
        g_originalGlClear(mask);
        return;
    }
    g_totalClears.fetch_add(1, std::memory_order_relaxed);
    const bool suppressionActive =
        g_terminalClearSuppressionActive.load(std::memory_order_acquire);
    GLuint framebuffer = g_currentFramebuffer.load(std::memory_order_relaxed);
    if (suppressionActive && g_glGetIntegerv != nullptr) {
        GLint queriedFramebuffer = 0;
        g_glGetIntegerv(kGLFramebufferBinding, &queriedFramebuffer);
        framebuffer = static_cast<GLuint>(std::max(queriedFramebuffer, 0));
    }
    const DWORD currentThread = GetCurrentThreadId();
    const DWORD ownerThread = g_terminalClearOwnerThread.load(std::memory_order_relaxed);
    const GLuint targetFramebuffer =
        g_terminalClearTargetFramebuffer.load(std::memory_order_relaxed);
    const bool ownerThreadMatches = currentThread == ownerThread;
    const bool targetFramebufferMatches = framebuffer == targetFramebuffer;
    const terminal_math::ClearPolicyResult policy =
        terminal_math::ResolveRetainedSurfaceClear(
            static_cast<uint32_t>(mask),
            static_cast<uint32_t>(GL_COLOR_BUFFER_BIT),
            suppressionActive,
            ownerThreadMatches,
            targetFramebufferMatches);

    if (suppressionActive && !ownerThreadMatches) {
        g_terminalClearThreadMismatches.fetch_add(1, std::memory_order_relaxed);
    } else if (suppressionActive && !targetFramebufferMatches) {
        g_terminalClearFramebufferMismatches.fetch_add(1, std::memory_order_relaxed);
    }
    if (!policy.colorSuppressed) {
        g_originalGlClear(mask);
        return;
    }

    const uint64_t suppressed = g_terminalColorClearsSuppressed.fetch_add(
        1, std::memory_order_relaxed) + 1;
    if (policy.forwardedMask != 0) {
        g_terminalDepthStencilClearsForwarded.fetch_add(1, std::memory_order_relaxed);
        g_originalGlClear(static_cast<GLbitfield>(policy.forwardedMask));
    }
    const uint64_t interval = static_cast<uint64_t>(
        std::max(g_config.hplControllerLogInterval, 1));
    if (suppressed <= 8 || suppressed % interval == 0) {
        Logger::Instance().Write(
            LogLevel::Info,
            "terminal_color_clear_suppressed count=%llu frame=%llu thread=%lu fbo=%u "
            "requestedMask=0x%x forwardedMask=0x%x viewport=%d,%d,%d,%d "
            "policy=preserve_retained_dirty_rect_color",
            static_cast<unsigned long long>(suppressed),
            static_cast<unsigned long long>(
                g_terminalClearFrame.load(std::memory_order_relaxed)),
            static_cast<unsigned long>(currentThread),
            framebuffer,
            static_cast<unsigned>(mask),
            static_cast<unsigned>(policy.forwardedMask),
            g_currentViewportX.load(std::memory_order_relaxed),
            g_currentViewportY.load(std::memory_order_relaxed),
            g_currentViewportWidth.load(std::memory_order_relaxed),
            g_currentViewportHeight.load(std::memory_order_relaxed));
    }
}

void LoadCoreGLHelpers(HMODULE opengl32)
{
    if (opengl32 == nullptr) {
        return;
    }

    g_glGetString = reinterpret_cast<GlGetStringFn>(GetProcAddress(opengl32, "glGetString"));
    g_glGetIntegerv = reinterpret_cast<GlGetIntegervFn>(GetProcAddress(opengl32, "glGetIntegerv"));
    g_glGetFloatv = reinterpret_cast<GlGetFloatvFn>(GetProcAddress(opengl32, "glGetFloatv"));
    g_glGetTexLevelParameteriv = reinterpret_cast<GlGetTexLevelParameterivFn>(
        GetProcAddress(opengl32, "glGetTexLevelParameteriv"));
}

} // namespace

bool InstallOpenGLHooks(const Config& config, OpenXRRuntime* openxr)
{
    std::lock_guard lock(g_installMutex);
    if (g_hooksInstalled) {
        return true;
    }

    g_config = config;
    g_openxr = openxr;
    g_occlusionQueryTelemetryEnabled = config.hplDualRenderContinuousControl
        || config.hplDualRenderReplayProbe;
    g_occlusionQueryBegins.store(0, std::memory_order_relaxed);
    g_occlusionQueryEnds.store(0, std::memory_order_relaxed);
    g_occlusionQueryResults.store(0, std::memory_order_relaxed);
    g_occlusionQueryAvailabilityChecks.store(0, std::memory_order_relaxed);
    g_occlusionQueryFirstEyeBegins.store(0, std::memory_order_relaxed);
    g_occlusionQueryReplayEyeBegins.store(0, std::memory_order_relaxed);
    g_occlusionQuerySameFrameReuses.store(0, std::memory_order_relaxed);
    g_occlusionQueryTargetConflicts.store(0, std::memory_order_relaxed);
    g_occlusionQueryUnmatchedEnds.store(0, std::memory_order_relaxed);
    g_occlusionQueryStateOverflows.store(0, std::memory_order_relaxed);
    {
        std::lock_guard queryLock(g_occlusionQueryMutex);
        g_occlusionQueryStates.clear();
        g_activeOcclusionQueries.clear();
    }
    {
        std::lock_guard copyLock(g_framebufferCopyMutex);
        g_framebufferCopyStates.clear();
    }
    g_framebufferCopies.store(0, std::memory_order_relaxed);
    g_framebufferCopyPixels.store(0, std::memory_order_relaxed);
    g_framebufferCopyFirstEye.store(0, std::memory_order_relaxed);
    g_framebufferCopyReplayEye.store(0, std::memory_order_relaxed);
    g_framebufferCopyCrossEyeReuses.store(0, std::memory_order_relaxed);
    g_framebufferCopyTextureReadFailures.store(0, std::memory_order_relaxed);
    g_framebufferCopyStateOverflows.store(0, std::memory_order_relaxed);
    g_shadowJitterSuppressed.store(config.hplShadowJitterSuppressedDefault, std::memory_order_relaxed);
    g_reflectionFadeBypassed.store(false, std::memory_order_relaxed);
    g_reflectionFadeF3Down.store(false, std::memory_order_relaxed);
    g_reflectionFadePatches.store(0, std::memory_order_relaxed);
    {
        std::lock_guard reflectionLock(g_reflectionFadeMutex);
        g_reflectionFadeLocations.clear();
    }

    MH_STATUS status = MH_Initialize();
    if (status != MH_OK && status != MH_ERROR_ALREADY_INITIALIZED) {
        Logger::Instance().Write(LogLevel::Error, "minhook_init failed status=%s", MH_StatusToString(status));
        return false;
    }
    g_minHookInitialized = true;

    HMODULE gdi32 = GetModuleHandleW(L"gdi32.dll");
    if (gdi32 == nullptr) {
        gdi32 = LoadLibraryW(L"gdi32.dll");
    }

    HMODULE opengl32 = GetModuleHandleW(L"opengl32.dll");
    if (opengl32 == nullptr) {
        opengl32 = LoadLibraryW(L"opengl32.dll");
    }

    LoadCoreGLHelpers(opengl32);

    bool anyHook = false;
    if (config.hookSwapBuffers) {
        anyHook |= HookExport(gdi32, "SwapBuffers", reinterpret_cast<void*>(&HookSwapBuffers), reinterpret_cast<void**>(&g_originalSwapBuffers));
    }
    if (config.hookWglMakeCurrent) {
        anyHook |= HookExport(opengl32, "wglMakeCurrent", reinterpret_cast<void*>(&HookWglMakeCurrent), reinterpret_cast<void**>(&g_originalWglMakeCurrent));
    }

    anyHook |= HookExport(opengl32, "wglGetProcAddress", reinterpret_cast<void*>(&HookWglGetProcAddress), reinterpret_cast<void**>(&g_originalWglGetProcAddress));

    if (config.hookFixedFunctionMatrices) {
        anyHook |= HookExport(opengl32, "glMatrixMode", reinterpret_cast<void*>(&HookGlMatrixMode), reinterpret_cast<void**>(&g_originalGlMatrixMode));
        anyHook |= HookExport(opengl32, "glLoadMatrixf", reinterpret_cast<void*>(&HookGlLoadMatrixf), reinterpret_cast<void**>(&g_originalGlLoadMatrixf));
    }
    if (config.hookViewport) {
        anyHook |= HookExport(opengl32, "glViewport", reinterpret_cast<void*>(&HookGlViewport), reinterpret_cast<void**>(&g_originalGlViewport));
    }
    if (config.hookDrawCalls) {
        anyHook |= HookExport(opengl32, "glDrawElements", reinterpret_cast<void*>(&HookGlDrawElements), reinterpret_cast<void**>(&g_originalGlDrawElements));
        anyHook |= HookExport(opengl32, "glDrawArrays", reinterpret_cast<void*>(&HookGlDrawArrays), reinterpret_cast<void**>(&g_originalGlDrawArrays));
    }
    if (config.hplRenderStageProbe) {
        anyHook |= HookExport(opengl32, "glClear", reinterpret_cast<void*>(&HookGlClear), reinterpret_cast<void**>(&g_originalGlClear));
    }
    if (config.hplPostEffectResourceProbe || config.hplPerEyeSSAOTemporalControl) {
        anyHook |= HookExport(opengl32, "glBindTexture", reinterpret_cast<void*>(&HookGlBindTexture), reinterpret_cast<void**>(&g_originalGlBindTexture));
    }
    if (g_occlusionQueryTelemetryEnabled) {
        anyHook |= HookExport(
            opengl32,
            "glCopyTexSubImage2D",
            reinterpret_cast<void*>(&HookGlCopyTexSubImage2D),
            reinterpret_cast<void**>(&g_originalGlCopyTexSubImage2D));
    }

    g_hooksInstalled = anyHook;
    Logger::Instance().Write(
        LogLevel::Info,
        "opengl_hooks install_complete anyHook=%d opengl32=%s gdi32=%s shadowJitterControl=%d shadowJitterSuppressed=%d shadowJitterKey=F7 reflectionFadeControl=%d reflectionFadeKey=F3 renderDiagnostic=%d renderDiagnosticKey=F6 postEffectResourceProbe=%d textureQuery=%d occlusionQueryTelemetry=%d framebufferCopyTelemetry=%d",
        anyHook ? 1 : 0,
        HexPointer(opengl32).c_str(),
        HexPointer(gdi32).c_str(),
        config.hplShadowJitterControl ? 1 : 0,
        g_shadowJitterSuppressed.load(std::memory_order_relaxed) ? 1 : 0,
        config.hplReflectionFadeControl ? 1 : 0,
        config.renderDiagnosticCapture ? 1 : 0,
        config.hplPostEffectResourceProbe ? 1 : 0,
        g_glGetTexLevelParameteriv != nullptr ? 1 : 0,
        g_occlusionQueryTelemetryEnabled ? 1 : 0,
        g_originalGlCopyTexSubImage2D != nullptr ? 1 : 0);
    return anyHook;
}

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
    bool forceCapture)
{
    g_postEffectResourceCapture = {};
    if (!g_config.hplPostEffectResourceProbe || effect == nullptr
        || wglGetCurrentContext() == nullptr) {
        return;
    }
    bool sample = forceCapture;
    {
        std::lock_guard lock(g_postEffectResourceMutex);
        if (g_postEffectEyeResources.size() < 64
            || g_postEffectEyeResources.find(effect) != g_postEffectEyeResources.end()) {
            PostEffectEyeResourceState& state = g_postEffectEyeResources[effect];
            const uint64_t attempt = ++state.attempts;
            const uint64_t interval = static_cast<uint64_t>(
                std::max(g_config.hplCompatibilityLogInterval, 1));
            sample = sample || attempt <= 4 || attempt % interval == 0;
        }
    }
    if (!sample) return;
    g_postEffectResourceCapture.active = true;
    g_postEffectResourceCapture.frame = frame;
    g_postEffectResourceCapture.sequence = sequence;
    g_postEffectResourceCapture.eye = eye;
    g_postEffectResourceCapture.poseFrame = poseFrame;
    g_postEffectResourceCapture.effectName = effectName != nullptr ? effectName : "Unknown";
    g_postEffectResourceCapture.effect = effect;
    g_postEffectResourceCapture.inputTexture = inputTexture;
    g_postEffectResourceCapture.renderTarget = renderTarget;
    g_postEffectResourceCapture.lastEffect = lastEffect;
}

void EndPostEffectResourceCapture(void* outputTexture)
{
    if (!g_postEffectResourceCapture.active) return;
    PostEffectResourceCapture capture = g_postEffectResourceCapture;
    g_postEffectResourceCapture = {};

    const uint64_t signature = post_effect_resource_math::HashResourceFootprint(
        capture.textures.data(),
        capture.textureCount,
        capture.framebuffers.data(),
        capture.framebufferCount);
    const uint64_t captureCount = g_postEffectResourceCaptures.fetch_add(
        1, std::memory_order_relaxed) + 1;

    post_effect_resource_math::EyeResourceOwnership ownership =
        post_effect_resource_math::EyeResourceOwnership::Unknown;
    bool signatureChanged = false;
    bool ownershipChanged = false;
    bool pairComparable = false;
    uint64_t pairedPoseFrame = 0;
    uint64_t effectCalls = 0;
    {
        std::lock_guard lock(g_postEffectResourceMutex);
        if (g_postEffectEyeResources.size() < 64
            || g_postEffectEyeResources.find(capture.effect) != g_postEffectEyeResources.end()) {
            PostEffectEyeResourceState& state = g_postEffectEyeResources[capture.effect];
            effectCalls = ++state.calls;
            if (capture.eye == 0 || capture.eye == 1) {
                const size_t eye = static_cast<size_t>(capture.eye);
                signatureChanged = !state.seen[eye] || state.signatures[eye] != signature;
                state.seen[eye] = true;
                state.signatures[eye] = signature;
                state.poseFrames[eye] = capture.poseFrame;
                state.resourcesObserved[eye] = capture.textureCount != 0
                    || capture.framebufferCount != 0;
            }
            pairComparable = state.seen[0] && state.seen[1]
                && state.resourcesObserved[0] && state.resourcesObserved[1]
                && state.poseFrames[0] != 0
                && state.poseFrames[0] == state.poseFrames[1];
            ownership = state.ownership;
            if (pairComparable) {
                pairedPoseFrame = state.poseFrames[0];
                const auto pairedOwnership =
                    post_effect_resource_math::ClassifyEyeResourceOwnership(
                        true, state.signatures[0], true, state.signatures[1]);
                ownershipChanged = pairedOwnership != state.ownership;
                ownership = pairedOwnership;
                if (ownershipChanged) {
                    state.ownership = ownership;
                    if (ownership == post_effect_resource_math::EyeResourceOwnership::Shared) {
                        g_postEffectSharedClassifications.fetch_add(1, std::memory_order_relaxed);
                    } else if (ownership == post_effect_resource_math::EyeResourceOwnership::EyeDistinct) {
                        g_postEffectDistinctClassifications.fetch_add(1, std::memory_order_relaxed);
                    }
                }
            }
        }
    }

    const uint64_t interval = static_cast<uint64_t>(
        std::max(g_config.hplCompatibilityLogInterval, 1));
    const bool shouldLog = effectCalls <= 2 || signatureChanged || ownershipChanged
        || captureCount % interval == 0;
    if (!shouldLog) return;

    std::ostringstream textures;
    for (size_t i = 0; i < capture.textureCount; ++i) {
        if (i != 0) textures << ';';
        const auto& resource = capture.textures[i];
        textures << "target=0x" << std::hex << resource.target
            << ",id=" << std::dec << resource.texture
            << ",size=" << resource.width << 'x' << resource.height << 'x' << resource.depth
            << ",format=0x" << std::hex << resource.internalFormat << std::dec;
    }
    std::ostringstream framebuffers;
    for (size_t i = 0; i < capture.framebufferCount; ++i) {
        if (i != 0) framebuffers << ';';
        const auto& resource = capture.framebuffers[i];
        framebuffers << "target=0x" << std::hex << resource.target
            << ",id=" << std::dec << resource.framebuffer;
    }
    g_postEffectResourceLogs.fetch_add(1, std::memory_order_relaxed);
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_post_effect_resources frame=%llu sequence=%llu capture=%llu effectCall=%llu eye=%d poseFrame=%llu pairedPoseFrame=%llu pairComparable=%d name=%s effect=%p inputObject=%p outputObject=%p renderTargetObject=%p last=%d signature=0x%llx eyeOwnership=%s signatureChanged=%d ownershipChanged=%d textures=%llu textureOverflow=%d textureBindings={%s} framebuffers=%llu framebufferOverflow=%d framebufferBindings={%s}",
        static_cast<unsigned long long>(capture.frame),
        static_cast<unsigned long long>(capture.sequence),
        static_cast<unsigned long long>(captureCount),
        static_cast<unsigned long long>(effectCalls),
        capture.eye,
        static_cast<unsigned long long>(capture.poseFrame),
        static_cast<unsigned long long>(pairedPoseFrame),
        pairComparable ? 1 : 0,
        capture.effectName,
        capture.effect,
        capture.inputTexture,
        outputTexture,
        capture.renderTarget,
        capture.lastEffect ? 1 : 0,
        static_cast<unsigned long long>(signature),
        post_effect_resource_math::EyeResourceOwnershipName(ownership),
        signatureChanged ? 1 : 0,
        ownershipChanged ? 1 : 0,
        static_cast<unsigned long long>(capture.textureCount),
        capture.textureOverflow ? 1 : 0,
        textures.str().c_str(),
        static_cast<unsigned long long>(capture.framebufferCount),
        capture.framebufferOverflow ? 1 : 0,
        framebuffers.str().c_str());
}

void RemoveOpenGLHooks()
{
    std::lock_guard lock(g_installMutex);
    EndTerminalCaptureGuard();
    EndTerminalColorClearSuppression();
    {
        std::lock_guard diagnosticLock(g_renderDiagnosticMutex);
        g_renderDiagnosticEndFrame.store(0, std::memory_order_relaxed);
        g_renderDiagnosticDrawStream.close();
        g_renderDiagnosticMatrixStream.close();
    }
    if (g_minHookInitialized) {
        MH_DisableHook(MH_ALL_HOOKS);
        MH_Uninitialize();
        g_minHookInitialized = false;
    }
    g_hooksInstalled = false;
    {
        std::lock_guard reflectionLock(g_reflectionFadeMutex);
        g_reflectionFadeLocations.clear();
    }
    {
        std::lock_guard resourceLock(g_postEffectResourceMutex);
        g_postEffectEyeResources.clear();
    }
    {
        std::lock_guard queryLock(g_occlusionQueryMutex);
        g_occlusionQueryStates.clear();
        g_activeOcclusionQueries.clear();
    }
    {
        std::lock_guard copyLock(g_framebufferCopyMutex);
        g_framebufferCopyStates.clear();
    }
    g_framebufferCopies.store(0, std::memory_order_relaxed);
    g_framebufferCopyPixels.store(0, std::memory_order_relaxed);
    g_framebufferCopyFirstEye.store(0, std::memory_order_relaxed);
    g_framebufferCopyReplayEye.store(0, std::memory_order_relaxed);
    g_framebufferCopyCrossEyeReuses.store(0, std::memory_order_relaxed);
    g_framebufferCopyTextureReadFailures.store(0, std::memory_order_relaxed);
    g_framebufferCopyStateOverflows.store(0, std::memory_order_relaxed);
    g_occlusionQueryTelemetryEnabled = false;
    g_originalGlBeginQuery = nullptr;
    g_originalGlEndQuery = nullptr;
    g_originalGlGetQueryObjectiv = nullptr;
    g_originalGlGetQueryObjectuiv = nullptr;
    g_originalGlGetQueryObjectui64v = nullptr;
    g_originalGlCopyTexSubImage2D = nullptr;
    g_postEffectResourceCapture = {};
    Logger::Instance().Write(LogLevel::Info, "opengl_hooks removed");
}

void LogOpenGLProofSummary()
{
    LastSamples samplesCopy;
    size_t postEffectResourceEffects = 0;
    size_t occlusionQueryStates = 0;
    size_t activeOcclusionQueries = 0;
    size_t framebufferCopyTextures = 0;
    {
        std::lock_guard lock(g_sampleMutex);
        samplesCopy = g_lastSamples;
    }
    {
        std::lock_guard lock(g_postEffectResourceMutex);
        postEffectResourceEffects = g_postEffectEyeResources.size();
    }
    {
        std::lock_guard lock(g_occlusionQueryMutex);
        occlusionQueryStates = g_occlusionQueryStates.size();
        activeOcclusionQueries = g_activeOcclusionQueries.size();
    }
    {
        std::lock_guard lock(g_framebufferCopyMutex);
        framebufferCopyTextures = g_framebufferCopyStates.size();
    }

    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_occlusion_query_summary enabled=%d hooks={begin=%d end=%d resultIv=%d resultUiv=%d resultUi64v=%d} calls={begin=%llu end=%llu result=%llu availability=%llu} passes={first=%llu replay=%llu sameFrameQueryReuse=%llu} anomalies={targetConflicts=%llu unmatchedEnds=%llu stateOverflows=%llu} states=%llu activeTargets=%llu inference=%s",
        g_occlusionQueryTelemetryEnabled ? 1 : 0,
        g_originalGlBeginQuery != nullptr ? 1 : 0,
        g_originalGlEndQuery != nullptr ? 1 : 0,
        g_originalGlGetQueryObjectiv != nullptr ? 1 : 0,
        g_originalGlGetQueryObjectuiv != nullptr ? 1 : 0,
        g_originalGlGetQueryObjectui64v != nullptr ? 1 : 0,
        static_cast<unsigned long long>(g_occlusionQueryBegins.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_occlusionQueryEnds.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_occlusionQueryResults.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_occlusionQueryAvailabilityChecks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_occlusionQueryFirstEyeBegins.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_occlusionQueryReplayEyeBegins.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_occlusionQuerySameFrameReuses.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_occlusionQueryTargetConflicts.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_occlusionQueryUnmatchedEnds.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_occlusionQueryStateOverflows.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(occlusionQueryStates),
        static_cast<unsigned long long>(activeOcclusionQueries),
        g_occlusionQuerySameFrameReuses.load(std::memory_order_relaxed) != 0
            ? "shared_query_ids_across_eyes" : "not_yet_observed");
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_framebuffer_copy_summary enabled=%d hook=%d copies=%llu pixels=%llu passes={first=%llu replay=%llu} crossEyeTextureReuses=%llu textureReadFailures=%llu stateOverflows=%llu textures=%llu inference=%s",
        g_occlusionQueryTelemetryEnabled ? 1 : 0,
        g_originalGlCopyTexSubImage2D != nullptr ? 1 : 0,
        static_cast<unsigned long long>(g_framebufferCopies.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_framebufferCopyPixels.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_framebufferCopyFirstEye.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_framebufferCopyReplayEye.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_framebufferCopyCrossEyeReuses.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_framebufferCopyTextureReadFailures.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_framebufferCopyStateOverflows.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(framebufferCopyTextures),
        g_framebufferCopyCrossEyeReuses.load(std::memory_order_relaxed) != 0
            ? "shared_scene_color_texture_across_eyes" : "not_yet_observed");

    Logger::Instance().Write(
        LogLevel::Info,
        "proof_summary frames=%llu swaps=%llu wglMakeCurrent=%llu renderThread=%lu ownGlBypasses=%llu terminalClear={active=%d targetFbo=%u frame=%llu armed=%llu colorSuppressed=%llu depthStencilForwarded=%llu framebufferMismatches=%llu threadMismatches=%llu} shadowJitterControl=%d shadowJitterSuppressed=%d shadowJitterUploads=%llu shadowJitterOverrides=%llu reflectionFadeControl=%d reflectionFadeBypassed=%d reflectionFadePatches=%llu postEffectResources={enabled=%d captures=%llu logs=%llu textures=%llu framebuffers=%llu sharedClassifications=%llu distinctClassifications=%llu effects=%llu} fixedProjection={%s} uniformProjectionName=\"%s\" uniformProjectionProgram=%u uniformProjectionLocation=%d uniformProjection={%s} %s",
        static_cast<unsigned long long>(g_frameIndex.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_swapCount.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_wglMakeCurrentCount.load(std::memory_order_relaxed)),
        static_cast<unsigned long>(g_renderThreadId.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(OwnOpenGLBypassCount()),
        g_terminalClearSuppressionActive.load(std::memory_order_relaxed) ? 1 : 0,
        g_terminalClearTargetFramebuffer.load(std::memory_order_relaxed),
        static_cast<unsigned long long>(g_terminalClearFrame.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_terminalClearArmedCaptures.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_terminalColorClearsSuppressed.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_terminalDepthStencilClearsForwarded.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_terminalClearFramebufferMismatches.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_terminalClearThreadMismatches.load(std::memory_order_relaxed)),
        g_config.hplShadowJitterControl ? 1 : 0,
        g_shadowJitterSuppressed.load(std::memory_order_relaxed) ? 1 : 0,
        static_cast<unsigned long long>(g_shadowJitterUploads.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_shadowJitterOverrides.load(std::memory_order_relaxed)),
        g_config.hplReflectionFadeControl ? 1 : 0,
        g_reflectionFadeBypassed.load(std::memory_order_relaxed) ? 1 : 0,
        static_cast<unsigned long long>(g_reflectionFadePatches.load(std::memory_order_relaxed)),
        g_config.hplPostEffectResourceProbe ? 1 : 0,
        static_cast<unsigned long long>(g_postEffectResourceCaptures.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_postEffectResourceLogs.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_postEffectTextureResources.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_postEffectFramebufferResources.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_postEffectSharedClassifications.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_postEffectDistinctClassifications.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(postEffectResourceEffects),
        MatrixSummaryText(samplesCopy.fixedProjection).c_str(),
        samplesCopy.uniformName.c_str(),
        samplesCopy.uniformProgram,
        samplesCopy.uniformLocation,
        MatrixSummaryText(samplesCopy.uniformProjection).c_str(),
        g_openxr != nullptr ? g_openxr->SummaryString().c_str() : "openxrEnabled=0");
}

uint64_t GetOpenGLRenderFrameHint()
{
    return g_frameIndex.load(std::memory_order_relaxed) + 1;
}

OpenGLTelemetrySnapshot GetOpenGLTelemetrySnapshot()
{
    return {
        g_totalDrawElements.load(std::memory_order_relaxed),
        g_totalDrawArrays.load(std::memory_order_relaxed),
        g_totalViewportCalls.load(std::memory_order_relaxed),
        g_totalFramebufferBinds.load(std::memory_order_relaxed),
        g_totalProgramUses.load(std::memory_order_relaxed),
        g_totalClears.load(std::memory_order_relaxed),
    };
}

void BeginTerminalCaptureGuard(uint64_t frame, uint32_t targetFramebuffer)
{
    g_terminalCaptureGuardActive.store(false, std::memory_order_release);
    g_terminalCaptureGuardFrame.store(frame, std::memory_order_relaxed);
    g_terminalCaptureGuardFramebuffer.store(targetFramebuffer, std::memory_order_relaxed);
    g_terminalCaptureGuardActive.store(true, std::memory_order_release);
}

void EndTerminalCaptureGuard()
{
    g_terminalCaptureGuardActive.store(false, std::memory_order_release);
    g_terminalCaptureGuardFramebuffer.store(0, std::memory_order_relaxed);
}

void BeginTerminalColorClearSuppression(uint64_t frame, uint32_t targetFramebuffer)
{
    g_terminalClearSuppressionActive.store(false, std::memory_order_release);
    g_terminalClearFrame.store(frame, std::memory_order_relaxed);
    g_terminalClearTargetFramebuffer.store(targetFramebuffer, std::memory_order_relaxed);
    g_terminalClearOwnerThread.store(GetCurrentThreadId(), std::memory_order_relaxed);
    g_terminalClearArmedCaptures.fetch_add(1, std::memory_order_relaxed);
    g_terminalClearSuppressionActive.store(true, std::memory_order_release);
}

void EndTerminalColorClearSuppression()
{
    g_terminalClearSuppressionActive.store(false, std::memory_order_release);
    g_terminalClearOwnerThread.store(0, std::memory_order_relaxed);
    g_terminalClearTargetFramebuffer.store(0, std::memory_order_relaxed);
}

TerminalClearSuppressionSnapshot GetTerminalClearSuppressionSnapshot()
{
    return {
        g_terminalClearSuppressionActive.load(std::memory_order_acquire),
        g_terminalClearTargetFramebuffer.load(std::memory_order_relaxed),
        g_terminalClearFrame.load(std::memory_order_relaxed),
        g_terminalClearArmedCaptures.load(std::memory_order_relaxed),
        g_terminalColorClearsSuppressed.load(std::memory_order_relaxed),
        g_terminalDepthStencilClearsForwarded.load(std::memory_order_relaxed),
        g_terminalClearFramebufferMismatches.load(std::memory_order_relaxed),
        g_terminalClearThreadMismatches.load(std::memory_order_relaxed),
    };
}

uint64_t GetTerminalOffscreenScissorBypassCount()
{
    return g_terminalOffscreenScissorBypasses.load(std::memory_order_relaxed);
}

void BeginTerminalDrawStateProbe(
    uint64_t sequence,
    uint64_t startFrame,
    uint32_t durationFrames)
{
    const uint64_t duration = std::max<uint64_t>(durationFrames, 1);
    g_terminalDrawProbeSequence.store(sequence, std::memory_order_relaxed);
    g_terminalDrawProbeSamples.store(0, std::memory_order_relaxed);
    g_terminalDrawProbeStartFrame.store(startFrame, std::memory_order_relaxed);
    g_terminalDrawProbeEndFrame.store(startFrame + duration - 1, std::memory_order_relaxed);
    Logger::Instance().Write(
        LogLevel::Warn,
        "terminal_draw_state armed sequence=%llu startFrame=%llu durationFrames=%llu maxSamples=128 policy=focused_fbo_program_scissor_texture_blend_trace",
        static_cast<unsigned long long>(sequence),
        static_cast<unsigned long long>(startFrame),
        static_cast<unsigned long long>(duration));
}

} // namespace somavr
