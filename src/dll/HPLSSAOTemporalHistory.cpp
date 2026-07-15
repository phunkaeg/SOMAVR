#include "HPLSSAOTemporalHistory.h"

#include "Config.h"
#include "HPLSSAOTemporalMath.h"
#include "Logger.h"

#include <Windows.h>
#include <gl/GL.h>

#include <array>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace somavr {
namespace {

constexpr GLenum kGLTexture2D = 0x0de1;
constexpr GLenum kGLTextureBinding2D = 0x8069;
constexpr GLenum kGLTextureWidth = 0x1000;
constexpr GLenum kGLTextureHeight = 0x1001;
constexpr GLenum kGLTextureInternalFormat = 0x1003;
constexpr GLenum kGLTextureMinFilter = 0x2801;
constexpr GLenum kGLTextureMagFilter = 0x2800;
constexpr GLenum kGLTextureWrapS = 0x2802;
constexpr GLenum kGLTextureWrapT = 0x2803;
constexpr GLint kGLLinear = 0x2601;
constexpr GLint kGLClampToEdge = 0x812f;

using GlCopyImageSubDataFn = void(APIENTRY*)(
    GLuint, GLenum, GLint, GLint, GLint, GLint,
    GLuint, GLenum, GLint, GLint, GLint, GLint,
    GLsizei, GLsizei, GLsizei);
using GlTexStorage2DFn = void(APIENTRY*)(GLenum, GLsizei, GLenum, GLsizei, GLsizei);

struct TextureIdentity {
    GLuint name = 0;
    GLenum target = 0;
    GLint width = 0;
    GLint height = 0;
    GLint internalFormat = 0;
    HGLRC context = nullptr;
};

struct RendererState {
    ssao_temporal_math::State schedule;
    void* nativeTexture = nullptr;
    TextureIdentity identity;
    std::array<GLuint, 2> histories{};
};

struct ActivePass {
    bool active = false;
    void* renderer = nullptr;
    void* nativeTexture = nullptr;
    int eyeIndex = -1;
    uint64_t poseFrame = 0;
};

bool g_configured = false;
bool g_faulted = false;
std::mutex g_mutex;
std::unordered_map<void*, RendererState> g_renderers;
std::unordered_map<void*, TextureIdentity> g_textureIdentities;
std::unordered_set<void*> g_knownNativeTextures;
thread_local ActivePass g_activePass;
thread_local void* g_bindingNativeTexture = nullptr;
GlCopyImageSubDataFn g_glCopyImageSubData = nullptr;
GlTexStorage2DFn g_glTexStorage2D = nullptr;
std::atomic<uint64_t> g_allocations = 0;
std::atomic<uint64_t> g_restores = 0;
std::atomic<uint64_t> g_commits = 0;
std::atomic<uint64_t> g_seeds = 0;
std::atomic<uint64_t> g_resets = 0;
std::atomic<uint64_t> g_failures = 0;

bool ValidProc(PROC proc)
{
    const auto value = reinterpret_cast<uintptr_t>(proc);
    return proc != nullptr && value != 1 && value != 2 && value != 3 && value != static_cast<uintptr_t>(-1);
}

bool ResolveFunctions()
{
    if (g_glCopyImageSubData != nullptr && g_glTexStorage2D != nullptr) return true;
    const PROC copy = wglGetProcAddress("glCopyImageSubData");
    const PROC storage = wglGetProcAddress("glTexStorage2D");
    if (!ValidProc(copy) || !ValidProc(storage)) return false;
    g_glCopyImageSubData = reinterpret_cast<GlCopyImageSubDataFn>(copy);
    g_glTexStorage2D = reinterpret_cast<GlTexStorage2DFn>(storage);
    return true;
}

bool SameIdentity(const TextureIdentity& left, const TextureIdentity& right)
{
    return left.name == right.name && left.target == right.target
        && left.width == right.width && left.height == right.height
        && left.internalFormat == right.internalFormat && left.context == right.context;
}

void DeleteHistories(RendererState& state)
{
    if ((state.histories[0] != 0 || state.histories[1] != 0)
        && wglGetCurrentContext() != nullptr
        && wglGetCurrentContext() == state.identity.context) {
        glDeleteTextures(static_cast<GLsizei>(state.histories.size()), state.histories.data());
    }
    state.histories = {};
    ssao_temporal_math::Reset(state.schedule);
}

bool Copy(GLuint source, GLuint destination, const TextureIdentity& identity)
{
    if (source == 0 || destination == 0 || identity.target != kGLTexture2D
        || identity.width <= 0 || identity.height <= 0 || !ResolveFunctions()) {
        return false;
    }
    g_glCopyImageSubData(
        source, identity.target, 0, 0, 0, 0,
        destination, identity.target, 0, 0, 0, 0,
        identity.width, identity.height, 1);
    return true;
}

bool CreateHistories(RendererState& state)
{
    const TextureIdentity& identity = state.identity;
    if (identity.name == 0 || identity.target != kGLTexture2D
        || identity.width <= 0 || identity.height <= 0 || identity.internalFormat == 0
        || identity.context == nullptr || identity.context != wglGetCurrentContext()
        || !ResolveFunctions()) {
        return false;
    }

    GLint previousTexture = 0;
    glGetIntegerv(kGLTextureBinding2D, &previousTexture);
    glGenTextures(static_cast<GLsizei>(state.histories.size()), state.histories.data());
    for (GLuint texture : state.histories) {
        if (texture == 0) continue;
        glBindTexture(kGLTexture2D, texture);
        g_glTexStorage2D(kGLTexture2D, 1, static_cast<GLenum>(identity.internalFormat), identity.width, identity.height);
        glTexParameteri(kGLTexture2D, kGLTextureMinFilter, kGLLinear);
        glTexParameteri(kGLTexture2D, kGLTextureMagFilter, kGLLinear);
        glTexParameteri(kGLTexture2D, kGLTextureWrapS, kGLClampToEdge);
        glTexParameteri(kGLTexture2D, kGLTextureWrapT, kGLClampToEdge);
    }
    glBindTexture(kGLTexture2D, static_cast<GLuint>(previousTexture));
    if (state.histories[0] == 0 || state.histories[1] == 0) {
        DeleteHistories(state);
        return false;
    }
    g_allocations.fetch_add(2, std::memory_order_relaxed);
    return true;
}

void Fault(const char* reason)
{
    g_faulted = true;
    g_activePass = {};
    const uint64_t failures = g_failures.fetch_add(1, std::memory_order_relaxed) + 1;
    Logger::Instance().Write(
        LogLevel::Error,
        "hpl_ssao_temporal fault=1 reason=%s failures=%llu fallback=native_shared_history",
        reason,
        static_cast<unsigned long long>(failures));
}

} // namespace

void InitializeHPLSSAOTemporalHistory(const Config& config)
{
    std::lock_guard lock(g_mutex);
    g_configured = config.hplPerEyeSSAOTemporalControl;
    g_faulted = false;
    g_renderers.clear();
    g_textureIdentities.clear();
    g_knownNativeTextures.clear();
    g_activePass = {};
    g_bindingNativeTexture = nullptr;
    g_glCopyImageSubData = nullptr;
    g_glTexStorage2D = nullptr;
    g_allocations.store(0, std::memory_order_relaxed);
    g_restores.store(0, std::memory_order_relaxed);
    g_commits.store(0, std::memory_order_relaxed);
    g_seeds.store(0, std::memory_order_relaxed);
    g_resets.store(0, std::memory_order_relaxed);
    g_failures.store(0, std::memory_order_relaxed);
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_ssao_temporal initialized configured=%d policy=per_eye_gpu_history nativeTextureOffset=0xe78 copy=glCopyImageSubData",
        g_configured ? 1 : 0);
}

void BeginHPLSSAOTemporalPass(
    void* renderer,
    void* nativeHistoryTexture,
    bool stereoEligible,
    int eyeIndex,
    uint64_t poseFrame,
    uint64_t calibrationGeneration)
{
    g_activePass = {};
    if (!g_configured || g_faulted || renderer == nullptr || nativeHistoryTexture == nullptr) return;

    std::lock_guard lock(g_mutex);
    RendererState& state = g_renderers[renderer];
    g_knownNativeTextures.insert(nativeHistoryTexture);
    if (state.nativeTexture != nullptr && state.nativeTexture != nativeHistoryTexture) {
        DeleteHistories(state);
        state.identity = {};
        g_resets.fetch_add(1, std::memory_order_relaxed);
    }
    state.nativeTexture = nativeHistoryTexture;
    const auto cachedIdentity = g_textureIdentities.find(nativeHistoryTexture);
    if (cachedIdentity != g_textureIdentities.end()) {
        if (state.identity.name != 0 && !SameIdentity(state.identity, cachedIdentity->second)) {
            DeleteHistories(state);
            g_resets.fetch_add(1, std::memory_order_relaxed);
        }
        state.identity = cachedIdentity->second;
    }
    const bool ready = state.histories[0] != 0 && state.histories[1] != 0
        && state.identity.context == wglGetCurrentContext();
    const auto begin = ssao_temporal_math::Begin(
        state.schedule,
        stereoEligible,
        eyeIndex,
        poseFrame,
        calibrationGeneration,
        ready);
    if (begin.reset) {
        g_resets.fetch_add(1, std::memory_order_relaxed);
    }
    if (begin.action == ssao_temporal_math::BeginAction::None) return;

    if (begin.action == ssao_temporal_math::BeginAction::Restore
        && !Copy(state.histories[eyeIndex], state.identity.name, state.identity)) {
        Fault("restore_copy_failed");
        return;
    }
    if (begin.action == ssao_temporal_math::BeginAction::Restore) {
        g_restores.fetch_add(1, std::memory_order_relaxed);
    }
    g_activePass = {true, renderer, nativeHistoryTexture, eyeIndex, poseFrame};
}

void EndHPLSSAOTemporalPass(void* renderer)
{
    const ActivePass pass = g_activePass;
    g_activePass = {};
    if (!pass.active) return;

    std::lock_guard lock(g_mutex);
    const auto found = g_renderers.find(renderer);
    if (renderer != pass.renderer || found == g_renderers.end()) {
        Fault("tracked_renderer_missing");
        return;
    }
    RendererState& state = found->second;
    const bool newlyCreated = state.histories[0] == 0 || state.histories[1] == 0;
    if (newlyCreated && !CreateHistories(state)) {
        Fault("gpu_history_allocation_failed");
        return;
    }
    if (newlyCreated) {
        if (!Copy(state.identity.name, state.histories[0], state.identity)
            || !Copy(state.identity.name, state.histories[1], state.identity)) {
            Fault("initial_history_seed_failed");
            return;
        }
        ssao_temporal_math::SeedBoth(state.schedule, pass.poseFrame);
        g_seeds.fetch_add(2, std::memory_order_relaxed);
        Logger::Instance().Write(
            LogLevel::Warn,
            "hpl_ssao_temporal allocated renderer=%p native=%p glTexture=%u size=%dx%d format=0x%x leftHistory=%u rightHistory=%u policy=seed_both_from_first_observed_result",
            renderer,
            state.nativeTexture,
            state.identity.name,
            state.identity.width,
            state.identity.height,
            state.identity.internalFormat,
            state.histories[0],
            state.histories[1]);
    } else if (!Copy(state.identity.name, state.histories[pass.eyeIndex], state.identity)) {
        Fault("commit_copy_failed");
        return;
    }
    if (!ssao_temporal_math::Commit(state.schedule, pass.eyeIndex, pass.poseFrame)) {
        Fault("schedule_commit_failed");
        return;
    }
    g_commits.fetch_add(1, std::memory_order_relaxed);
}

void BeginHPLSSAOTemporalTextureBind(void* nativeTexture)
{
    g_bindingNativeTexture = g_configured ? nativeTexture : nullptr;
}

void EndHPLSSAOTemporalTextureBind()
{
    g_bindingNativeTexture = nullptr;
}

void ObserveHPLSSAOTemporalGLBind(uint32_t target, uint32_t texture)
{
    if (!g_configured || g_bindingNativeTexture == nullptr
        || target != kGLTexture2D || texture == 0) {
        return;
    }
    {
        std::lock_guard lock(g_mutex);
        if (g_knownNativeTextures.find(g_bindingNativeTexture) == g_knownNativeTextures.end()) {
            return;
        }
    }
    TextureIdentity observed;
    observed.name = texture;
    observed.target = target;
    observed.context = wglGetCurrentContext();
    glGetTexLevelParameteriv(target, 0, kGLTextureWidth, &observed.width);
    glGetTexLevelParameteriv(target, 0, kGLTextureHeight, &observed.height);
    glGetTexLevelParameteriv(target, 0, kGLTextureInternalFormat, &observed.internalFormat);

    std::lock_guard lock(g_mutex);
    g_textureIdentities[g_bindingNativeTexture] = observed;
    if (!g_activePass.active || g_bindingNativeTexture != g_activePass.nativeTexture) return;
    const auto found = g_renderers.find(g_activePass.renderer);
    if (found == g_renderers.end()) return;
    RendererState& state = found->second;
    if (state.identity.name != 0 && !SameIdentity(state.identity, observed)) {
        DeleteHistories(state);
        g_resets.fetch_add(1, std::memory_order_relaxed);
    }
    state.identity = observed;
}

HPLSSAOTemporalHistoryStatus GetHPLSSAOTemporalHistoryStatus()
{
    std::lock_guard lock(g_mutex);
    return {
        g_configured,
        g_configured && !g_faulted,
        g_faulted,
        static_cast<uint64_t>(g_renderers.size()),
        g_allocations.load(std::memory_order_relaxed),
        g_restores.load(std::memory_order_relaxed),
        g_commits.load(std::memory_order_relaxed),
        g_seeds.load(std::memory_order_relaxed),
        g_resets.load(std::memory_order_relaxed),
        g_failures.load(std::memory_order_relaxed),
    };
}

void RemoveHPLSSAOTemporalHistory()
{
    const HPLSSAOTemporalHistoryStatus status = GetHPLSSAOTemporalHistoryStatus();
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_ssao_temporal removed trackedRenderers=%llu allocations=%llu restores=%llu commits=%llu seeds=%llu resets=%llu failures=%llu",
        static_cast<unsigned long long>(status.trackedRenderers),
        static_cast<unsigned long long>(status.allocations),
        static_cast<unsigned long long>(status.restores),
        static_cast<unsigned long long>(status.commits),
        static_cast<unsigned long long>(status.seeds),
        static_cast<unsigned long long>(status.resets),
        static_cast<unsigned long long>(status.failures));
    std::lock_guard lock(g_mutex);
    for (auto& entry : g_renderers) DeleteHistories(entry.second);
    g_renderers.clear();
    g_textureIdentities.clear();
    g_knownNativeTextures.clear();
    g_activePass = {};
    g_bindingNativeTexture = nullptr;
    g_configured = false;
    g_faulted = false;
}

} // namespace somavr
