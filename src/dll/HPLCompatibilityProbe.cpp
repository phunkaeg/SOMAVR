#include "HPLCompatibilityProbe.h"

#include "HPLCameraBridge.h"
#include "HPLCameraMath.h"
#include "Logger.h"
#include "OpenGLHooks.h"

#include <Windows.h>
#include <gl/GL.h>

#include <MinHook.h>

#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
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
constexpr uintptr_t kRenderPostPostEffectRva = 0x1f1480;
constexpr uintptr_t kRenderScreenGuiRva = 0x2981e0;
constexpr uintptr_t kPostEffectHasActiveEffectsRva = 0x33b8f0;
constexpr uintptr_t kAudioListenerUpdateRva = 0x289340;

constexpr GLenum kGLCurrentProgram = 0x8b8d;
constexpr GLenum kGLDrawFramebufferBinding = 0x8ca6;
constexpr GLenum kGLReadFramebufferBinding = 0x8caa;
constexpr GLenum kGLViewport = 0x0ba2;

constexpr size_t kListenerUpOffset = 0x50;
constexpr size_t kListenerForwardOffset = 0x5c;
constexpr size_t kListenerPositionOffset = 0x74;
constexpr size_t kListenerVelocityOffset = 0x80;

using GlGetIntegervFn = void(APIENTRY*)(GLenum, GLint*);
using RenderViewportFn = void (*)(void*, void*, float, uint64_t);
using RenderWorldFn = void (*)(void*, float, void*, void*, void*, void*, bool, void*);
using RenderWorldCallbacksFn = void (*)(void*, void*, void*, float);
using RenderPostEffectsFn = void (*)(void*, float, void*, void*, void*);
using RenderPostPostEffectFn = void (*)(void*, void*, void*, void*);
using RenderScreenGuiFn = void (*)(void*, void*, float);
using PostEffectHasActiveEffectsFn = bool (*)(void*);
using AudioListenerUpdateFn = void (*)(void*);

enum class Stage : size_t {
    Viewport,
    World,
    WorldCallbacks,
    PostEffects,
    PostPostEffect,
    ScreenGui,
    Count,
};

struct GLState {
    bool valid = false;
    GLint drawFramebuffer = 0;
    GLint readFramebuffer = 0;
    GLint program = 0;
    GLint viewport[4] = {};
};

struct StageSample {
    bool enabled = false;
    Stage stage = Stage::Viewport;
    uint64_t frame = 0;
    uint64_t sequence = 0;
    uint64_t call = 0;
    void* viewport = nullptr;
    uint64_t renderMask = 0;
    LARGE_INTEGER start = {};
    GLState before = {};
};

struct FrameSampleBudget {
    uint64_t frame = UINT64_MAX;
    uint32_t count = 0;
};

Config g_config;
OpenXRRuntime* g_openxr = nullptr;
uintptr_t g_executableBase = 0;
GlGetIntegervFn g_glGetIntegerv = nullptr;
std::vector<void*> g_hookTargets;
std::mutex g_installMutex;
std::mutex g_sampleMutex;
std::array<FrameSampleBudget, static_cast<size_t>(Stage::Count)> g_sampleBudgets;
std::array<std::atomic<uint64_t>, static_cast<size_t>(Stage::Count)> g_stageCalls = {};
std::atomic<uint64_t> g_audioCalls = 0;
std::atomic<uint64_t> g_audioPoseSamples = 0;
std::atomic<uint64_t> g_audioCorrections = 0;
std::atomic<uint64_t> g_postEffectQueries = 0;
std::atomic<uint64_t> g_postEffectBypasses = 0;
std::atomic<bool> g_postEffectBypassEnabled = false;
std::atomic<bool> g_f12Down = false;

RenderViewportFn g_originalRenderViewport = nullptr;
RenderWorldFn g_originalRenderWorld = nullptr;
RenderWorldCallbacksFn g_originalRenderWorldCallbacks = nullptr;
RenderPostEffectsFn g_originalRenderPostEffects = nullptr;
RenderPostPostEffectFn g_originalRenderPostPostEffect = nullptr;
RenderScreenGuiFn g_originalRenderScreenGui = nullptr;
PostEffectHasActiveEffectsFn g_originalPostEffectHasActiveEffects = nullptr;
AudioListenerUpdateFn g_originalAudioListenerUpdate = nullptr;

thread_local uint64_t g_traceFrame = UINT64_MAX;
thread_local uint64_t g_traceSequence = 0;
thread_local void* g_activeViewport = nullptr;
thread_local uint64_t g_activeRenderMask = 0;

const char* StageName(Stage stage)
{
    switch (stage) {
    case Stage::Viewport: return "viewport";
    case Stage::World: return "world";
    case Stage::WorldCallbacks: return "world_callbacks";
    case Stage::PostEffects: return "post_effects";
    case Stage::PostPostEffect: return "post_post_effect";
    case Stage::ScreenGui: return "screen_gui";
    default: return "unknown";
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
    state.valid = true;
    return state;
}

bool ConsumeSampleBudget(Stage stage, uint64_t frame, uint64_t call)
{
    if (call <= 2) {
        return true;
    }

    const uint64_t interval = static_cast<uint64_t>(g_config.hplCompatibilityLogInterval);
    if (interval == 0 || frame % interval != 0) {
        return false;
    }

    std::lock_guard lock(g_sampleMutex);
    FrameSampleBudget& budget = g_sampleBudgets[static_cast<size_t>(stage)];
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

StageSample BeginStage(Stage stage, void* viewport = nullptr, uint64_t renderMask = 0)
{
    StageSample sample;
    sample.stage = stage;
    sample.frame = GetOpenGLRenderFrameHint();
    if (g_traceFrame != sample.frame) {
        g_traceFrame = sample.frame;
        g_traceSequence = 0;
    }
    sample.sequence = ++g_traceSequence;
    sample.call = g_stageCalls[static_cast<size_t>(stage)].fetch_add(1, std::memory_order_relaxed) + 1;
    sample.viewport = viewport != nullptr ? viewport : g_activeViewport;
    sample.renderMask = renderMask != 0 ? renderMask : g_activeRenderMask;
    sample.enabled = ConsumeSampleBudget(stage, sample.frame, sample.call);
    if (sample.enabled) {
        sample.before = ReadGLState();
        QueryPerformanceCounter(&sample.start);
    }
    return sample;
}

void EndStage(const StageSample& sample)
{
    if (!sample.enabled) {
        return;
    }

    LARGE_INTEGER end = {};
    LARGE_INTEGER frequency = {};
    QueryPerformanceCounter(&end);
    QueryPerformanceFrequency(&frequency);
    const double durationUs = frequency.QuadPart > 0
        ? static_cast<double>(end.QuadPart - sample.start.QuadPart) * 1000000.0
            / static_cast<double>(frequency.QuadPart)
        : 0.0;
    const GLState after = ReadGLState();

    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_render_stage frame=%llu sequence=%llu stage=%s call=%llu viewport=%p mask=0x%llx durationUs=%.2f glValid=%d,%d glBefore=%d,%d,%d,%d,%d,%d,%d glAfter=%d,%d,%d,%d,%d,%d,%d",
        static_cast<unsigned long long>(sample.frame),
        static_cast<unsigned long long>(sample.sequence),
        StageName(sample.stage),
        static_cast<unsigned long long>(sample.call),
        sample.viewport,
        static_cast<unsigned long long>(sample.renderMask),
        durationUs,
        sample.before.valid ? 1 : 0,
        after.valid ? 1 : 0,
        sample.before.drawFramebuffer,
        sample.before.readFramebuffer,
        sample.before.program,
        sample.before.viewport[0],
        sample.before.viewport[1],
        sample.before.viewport[2],
        sample.before.viewport[3],
        after.drawFramebuffer,
        after.readFramebuffer,
        after.program,
        after.viewport[0],
        after.viewport[1],
        after.viewport[2],
        after.viewport[3]);
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
    void* previousViewport = g_activeViewport;
    const uint64_t previousMask = g_activeRenderMask;
    g_activeViewport = viewport;
    g_activeRenderMask = renderMask;

    const StageSample sample = BeginStage(Stage::Viewport, viewport, renderMask);
    g_originalRenderViewport(scene, viewport, frameTime, renderMask);
    EndStage(sample);

    g_activeViewport = previousViewport;
    g_activeRenderMask = previousMask;
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
    const StageSample sample = BeginStage(Stage::World);
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
    const StageSample sample = BeginStage(Stage::WorldCallbacks, viewport);
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
    const StageSample sample = BeginStage(Stage::PostEffects);
    g_originalRenderPostEffects(composite, frameTime, frustum, inputTexture, renderTarget);
    EndStage(sample);
}

void HookRenderPostPostEffect(void* renderer, void* frustum, void* renderTarget, void* settings)
{
    const StageSample sample = BeginStage(Stage::PostPostEffect);
    g_originalRenderPostPostEffect(renderer, frustum, renderTarget, settings);
    EndStage(sample);
}

void HookRenderScreenGui(void* scene, void* viewport, float frameTime)
{
    const StageSample sample = BeginStage(Stage::ScreenGui, viewport);
    g_originalRenderScreenGui(scene, viewport, frameTime);
    EndStage(sample);
}

bool HookPostEffectHasActiveEffects(void* composite)
{
    const uint64_t call = g_postEffectQueries.fetch_add(1, std::memory_order_relaxed) + 1;
    const bool f12Down = (GetAsyncKeyState(VK_F12) & 0x8000) != 0;
    const bool previousF12Down = g_f12Down.exchange(f12Down, std::memory_order_relaxed);
    if (f12Down && !previousF12Down) {
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

    g_originalAudioListenerUpdate(soundSystem);

    if (correctionApplied) {
        WriteVector(soundSystem, kListenerForwardOffset, forward);
        WriteVector(soundSystem, kListenerUpOffset, up);
    }

    if (sample) {
        g_audioPoseSamples.fetch_add(1, std::memory_order_relaxed);
        Logger::Instance().Write(
            LogLevel::Info,
            "hpl_audio_listener frame=%llu call=%llu finite=%d tracking=%d stereo=%d correction=%d listenerPos=%.5f,%.5f,%.5f listenerVel=%.5f,%.5f,%.5f listenerForward=%.5f,%.5f,%.5f listenerUp=%.5f,%.5f,%.5f committedForward=%.5f,%.5f,%.5f committedUp=%.5f,%.5f,%.5f headDeltaValid=%d headDeltaFrame=%llu headDeltaQuat=%.6f,%.6f,%.6f,%.6f hmdValid=%d hmdFrame=%llu hmdPos=%.5f,%.5f,%.5f hmdQuat=%.6f,%.6f,%.6f,%.6f",
            static_cast<unsigned long long>(GetOpenGLRenderFrameHint()),
            static_cast<unsigned long long>(call),
            IsFinite(position) && IsFinite(velocity) && IsFinite(forward) && IsFinite(up) ? 1 : 0,
            cameraStatus.trackingEnabled ? 1 : 0,
            cameraStatus.stereoEnabled ? 1 : 0,
            correctionApplied ? 1 : 0,
            position.x,
            position.y,
            position.z,
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

bool InstallHPLCompatibilityProbe(const Config& config, OpenXRRuntime* openxr)
{
    std::lock_guard lock(g_installMutex);
    if (!config.hplRenderStageProbe
        && !config.hplAudioListenerProbe
        && !config.hplPostEffectControl) {
        Logger::Instance().Write(LogLevel::Info, "hpl_compat_probe install_skipped enabled=0");
        return true;
    }
    if (!g_hookTargets.empty()) {
        return true;
    }

    g_config = config;
    if (g_config.hplAudioListenerCorrection && !ValidateAudioRotationMath()) {
        g_config.hplAudioListenerCorrection = false;
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_audio_listener correction_disabled reason=rotation_self_test_failed");
    }
    g_openxr = openxr;
    g_postEffectBypassEnabled.store(config.hplPostEffectBypassDefault, std::memory_order_relaxed);
    g_f12Down.store(false, std::memory_order_relaxed);
    HMODULE executable = GetModuleHandleW(nullptr);
    g_executableBase = reinterpret_cast<uintptr_t>(executable);

    HMODULE opengl32 = GetModuleHandleW(L"opengl32.dll");
    if (opengl32 != nullptr) {
        g_glGetIntegerv = reinterpret_cast<GlGetIntegervFn>(GetProcAddress(opengl32, "glGetIntegerv"));
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
    static constexpr uint8_t kRenderPostPostEffectSignature[] = {
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

    size_t installed = 0;
    if (config.hplRenderStageProbe) {
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
        installed += InstallHook(executable, kRenderPostPostEffectRva, kRenderPostPostEffectSignature,
            sizeof(kRenderPostPostEffectSignature), "post_post_effect", reinterpret_cast<void*>(&HookRenderPostPostEffect),
            reinterpret_cast<void**>(&g_originalRenderPostPostEffect));
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

    Logger::Instance().Write(
        installed > 0 ? LogLevel::Warn : LogLevel::Error,
        "hpl_compat_probe install_complete renderStages=%d audioListener=%d audioCorrection=%d postEffectControl=%d postEffectBypass=%d postEffectKey=F12 installed=%llu requested=%d logInterval=%d base=%p",
        config.hplRenderStageProbe ? 1 : 0,
        config.hplAudioListenerProbe ? 1 : 0,
        g_config.hplAudioListenerCorrection ? 1 : 0,
        config.hplPostEffectControl ? 1 : 0,
        config.hplPostEffectBypassDefault ? 1 : 0,
        static_cast<unsigned long long>(installed),
        (config.hplRenderStageProbe ? 6 : 0)
            + (config.hplAudioListenerProbe ? 1 : 0)
            + (config.hplPostEffectControl ? 1 : 0),
        config.hplCompatibilityLogInterval,
        executable);
    return installed > 0;
}

void LogHPLCompatibilityProbeSummary()
{
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_compat_summary viewport=%llu world=%llu worldCallbacks=%llu postEffects=%llu postPostEffect=%llu screenGui=%llu audioUpdates=%llu audioSamples=%llu audioCorrections=%llu postEffectQueries=%llu postEffectBypasses=%llu postEffectBypassEnabled=%d installedHooks=%llu",
        static_cast<unsigned long long>(g_stageCalls[static_cast<size_t>(Stage::Viewport)].load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_stageCalls[static_cast<size_t>(Stage::World)].load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_stageCalls[static_cast<size_t>(Stage::WorldCallbacks)].load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_stageCalls[static_cast<size_t>(Stage::PostEffects)].load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_stageCalls[static_cast<size_t>(Stage::PostPostEffect)].load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_stageCalls[static_cast<size_t>(Stage::ScreenGui)].load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_audioCalls.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_audioPoseSamples.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_audioCorrections.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_postEffectQueries.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_postEffectBypasses.load(std::memory_order_relaxed)),
        g_postEffectBypassEnabled.load(std::memory_order_relaxed) ? 1 : 0,
        static_cast<unsigned long long>(g_hookTargets.size()));
}

void RemoveHPLCompatibilityProbe()
{
    std::lock_guard lock(g_installMutex);
    for (void* target : g_hookTargets) {
        MH_DisableHook(target);
        MH_RemoveHook(target);
    }
    g_hookTargets.clear();
    g_originalRenderViewport = nullptr;
    g_originalRenderWorld = nullptr;
    g_originalRenderWorldCallbacks = nullptr;
    g_originalRenderPostEffects = nullptr;
    g_originalRenderPostPostEffect = nullptr;
    g_originalRenderScreenGui = nullptr;
    g_originalPostEffectHasActiveEffects = nullptr;
    g_originalAudioListenerUpdate = nullptr;
    g_postEffectBypassEnabled.store(false, std::memory_order_relaxed);
    g_f12Down.store(false, std::memory_order_relaxed);
    g_openxr = nullptr;
    g_glGetIntegerv = nullptr;
    g_executableBase = 0;
    Logger::Instance().Write(LogLevel::Info, "hpl_compat_probe removed");
}

} // namespace somavr
