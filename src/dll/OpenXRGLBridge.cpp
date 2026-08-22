#include "OpenXRGLBridge.h"

#if defined(SOMAVR_ENABLE_OPENXR)

#include "Logger.h"
#include "OpenGLHooks.h"
#include "OpenGLOwnership.h"

#include <Windows.h>
#include <gl/GL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <type_traits>
#include <utility>

namespace somavr {
namespace {

constexpr uint32_t kGlFramebuffer = 0x8D40;
constexpr uint32_t kGlReadFramebuffer = 0x8CA8;
constexpr uint32_t kGlDrawFramebuffer = 0x8CA9;
constexpr uint32_t kGlReadFramebufferBinding = 0x8CAA;
constexpr uint32_t kGlDrawFramebufferBinding = 0x8CA6;
constexpr uint32_t kGlColorAttachment0 = 0x8CE0;
constexpr uint32_t kGlDepthAttachment = 0x8D00;
constexpr uint32_t kGlFramebufferComplete = 0x8CD5;
constexpr uint32_t kGlTexture2D = 0x0DE1;
constexpr uint32_t kGlTextureBinding2D = 0x8069;
constexpr uint32_t kGlClampToEdge = 0x812F;
constexpr uint32_t kGlReadBuffer = 0x0C02;
constexpr uint32_t kGlDrawBuffer = 0x0C01;
constexpr uint32_t kGlBack = 0x0405;
constexpr uint32_t kGlViewport = 0x0BA2;
constexpr uint32_t kGlScissorTest = 0x0C11;
constexpr uint32_t kGlScissorBox = 0x0C10;
constexpr uint32_t kGlColorClearValue = 0x0C22;
constexpr uint32_t kGlColorWriteMask = 0x0C23;
constexpr uint32_t kGlColorBufferBit = 0x00004000;
constexpr uint32_t kGlDepthBufferBit = 0x00000100;
constexpr uint32_t kGlDepthBits = 0x0D56;
constexpr uint32_t kGlStencilBits = 0x0D57;
constexpr uint32_t kGlDepthComponent = 0x1902;
constexpr int32_t kGlDepthComponent24 = 0x81A6;
constexpr int64_t kGlDepthComponent32f = 0x8CAC;
constexpr int64_t kGlDepth24Stencil8 = 0x88F0;
constexpr int64_t kGlDepth32fStencil8 = 0x8CAD;
constexpr uint32_t kGlDepthStencil = 0x84F9;
constexpr uint32_t kGlUnsignedInt248 = 0x84FA;
constexpr uint32_t kGlFloat32UnsignedInt248Rev = 0x8DAD;
constexpr uint32_t kGlDepthStencilAttachment = 0x821A;
constexpr uint32_t kGlUnpackAlignment = 0x0CF5;
constexpr uint32_t kGlPackAlignment = 0x0D05;
constexpr uint32_t kGlPackRowLength = 0x0D02;
constexpr uint32_t kGlPackSkipRows = 0x0D03;
constexpr uint32_t kGlPackSkipPixels = 0x0D04;
constexpr uint32_t kGlPixelPackBuffer = 0x88EB;
constexpr uint32_t kGlPixelPackBufferBinding = 0x88ED;
constexpr uint32_t kGlLinear = 0x2601;
constexpr uint32_t kGlNearest = 0x2600;
constexpr uint32_t kGlTimestamp = 0x8E28;
constexpr uint32_t kGlQueryResult = 0x8866;
constexpr uint32_t kGlQueryResultAvailable = 0x8867;
constexpr int64_t kGlSrgb8Alpha8 = 0x8C43;
constexpr int64_t kGlRgba8 = 0x8058;
constexpr int64_t kGlRgba16f = 0x881A;
constexpr XrDuration kSwapchainWaitTimeout = 50'000'000;

int64_t TransferQpcNow()
{
    LARGE_INTEGER value{};
    QueryPerformanceCounter(&value);
    return value.QuadPart;
}

uint64_t TransferQpcDeltaMicroseconds(int64_t start, int64_t end)
{
    static const int64_t frequency = [] {
        LARGE_INTEGER value{};
        QueryPerformanceFrequency(&value);
        return value.QuadPart;
    }();
    if (frequency <= 0 || end <= start) {
        return 0;
    }
    return static_cast<uint64_t>(((end - start) * 1'000'000) / frequency);
}

void AccumulateTransferPhase(
    OpenXRGLBridge::TransferPhaseTiming& destination,
    uint64_t valueUs)
{
    destination.latestUs = valueUs;
    destination.totalUs += valueUs;
    destination.maxUs = std::max(destination.maxUs, valueUs);
}

class ScopedSwapchainTransferTiming {
public:
    ScopedSwapchainTransferTiming(
        OpenXRGLBridge::SwapchainTransferTiming& destination,
        OpenXRGLBridge::ColorTransferSource source)
        : destination_(destination), source_(source), startQpc_(TransferQpcNow())
    {
    }

    ~ScopedSwapchainTransferTiming()
    {
        destination_.latestSource = source_;
        ++destination_.attempts;
        if (succeeded_) {
            ++destination_.successes;
        } else {
            ++destination_.failures;
        }
        AccumulateTransferPhase(destination_.acquire, acquireUs_);
        AccumulateTransferPhase(destination_.wait, waitUs_);
        AccumulateTransferPhase(destination_.copy, copyUs_);
        AccumulateTransferPhase(destination_.flush, flushUs_);
        AccumulateTransferPhase(destination_.release, releaseUs_);
        AccumulateTransferPhase(
            destination_.total,
            TransferQpcDeltaMicroseconds(startQpc_, TransferQpcNow()));
    }

    template <typename Callback>
    auto MeasureAcquire(Callback&& callback)
    {
        return Measure(acquireUs_, std::forward<Callback>(callback));
    }

    template <typename Callback>
    auto MeasureWait(Callback&& callback)
    {
        return Measure(waitUs_, std::forward<Callback>(callback));
    }

    template <typename Callback>
    auto MeasureCopy(Callback&& callback)
    {
        return Measure(copyUs_, std::forward<Callback>(callback));
    }

    template <typename Callback>
    void MeasureFlush(Callback&& callback)
    {
        Measure(flushUs_, std::forward<Callback>(callback));
    }

    template <typename Callback>
    auto MeasureRelease(Callback&& callback)
    {
        return Measure(releaseUs_, std::forward<Callback>(callback));
    }

    void SetSucceeded(bool succeeded)
    {
        succeeded_ = succeeded;
    }

    void AddCopyElapsed(uint64_t valueUs)
    {
        copyUs_ += valueUs;
    }

private:
    template <typename Callback>
    static auto Measure(uint64_t& destinationUs, Callback&& callback)
    {
        const int64_t startQpc = TransferQpcNow();
        if constexpr (std::is_void_v<std::invoke_result_t<Callback>>) {
            std::forward<Callback>(callback)();
            destinationUs += TransferQpcDeltaMicroseconds(startQpc, TransferQpcNow());
        } else {
            auto result = std::forward<Callback>(callback)();
            destinationUs += TransferQpcDeltaMicroseconds(startQpc, TransferQpcNow());
            return result;
        }
    }

    OpenXRGLBridge::SwapchainTransferTiming& destination_;
    OpenXRGLBridge::ColorTransferSource source_;
    int64_t startQpc_ = 0;
    uint64_t acquireUs_ = 0;
    uint64_t waitUs_ = 0;
    uint64_t copyUs_ = 0;
    uint64_t flushUs_ = 0;
    uint64_t releaseUs_ = 0;
    bool succeeded_ = false;
};

// The OpenXR runtime issues its own GL work inside xrAcquireSwapchainImage,
// xrWaitSwapchainImage and xrReleaseSwapchainImage. TheDarkModVR carries four
// separate workarounds for a runtime that "does not properly reset the GL
// context after certain calls" and one for a runtime that "does not properly
// set viewport and scissor before copying render textures", so this is observed
// behaviour, not a theoretical concern.
//
// The per-blit save/restore inside the copy helpers sits *between* acquire and
// release, so it cannot see either. This guard brackets the whole transaction:
// anything the runtime clobbers on the way in is what we capture, and anything
// it clobbers on release is undone before the game's next draw.
class ScopedRuntimeGlState final {
public:
    using BindFramebufferFn = void(APIENTRY*)(uint32_t, uint32_t);

    explicit ScopedRuntimeGlState(BindFramebufferFn bindFramebuffer)
        : bindFramebuffer_(bindFramebuffer)
    {
        glGetIntegerv(kGlReadFramebufferBinding, &readFramebuffer_);
        glGetIntegerv(kGlDrawFramebufferBinding, &drawFramebuffer_);
        glGetIntegerv(kGlViewport, viewport_);
        glGetIntegerv(kGlScissorBox, scissorBox_);
        scissorEnabled_ = glIsEnabled(kGlScissorTest) == GL_TRUE;
    }

    ~ScopedRuntimeGlState()
    {
        if (bindFramebuffer_ != nullptr) {
            bindFramebuffer_(kGlReadFramebuffer, static_cast<uint32_t>(readFramebuffer_));
            bindFramebuffer_(kGlDrawFramebuffer, static_cast<uint32_t>(drawFramebuffer_));
        }
        glViewport(viewport_[0], viewport_[1], viewport_[2], viewport_[3]);
        glScissor(scissorBox_[0], scissorBox_[1], scissorBox_[2], scissorBox_[3]);
        if (scissorEnabled_) {
            glEnable(kGlScissorTest);
        } else {
            glDisable(kGlScissorTest);
        }
    }

    ScopedRuntimeGlState(const ScopedRuntimeGlState&) = delete;
    ScopedRuntimeGlState& operator=(const ScopedRuntimeGlState&) = delete;

private:
    BindFramebufferFn bindFramebuffer_ = nullptr;
    int32_t readFramebuffer_ = 0;
    int32_t drawFramebuffer_ = 0;
    int32_t viewport_[4] = {};
    int32_t scissorBox_[4] = {};
    bool scissorEnabled_ = false;
};

bool IsInvalidWglProc(PROC proc)
{
    const uintptr_t value = reinterpret_cast<uintptr_t>(proc);
    return proc == nullptr || value == 1 || value == 2 || value == 3 || value == std::numeric_limits<uintptr_t>::max();
}

template <typename T>
T ResolveGlProc(const char* name)
{
    PROC proc = wglGetProcAddress(name);
    if (IsInvalidWglProc(proc)) {
        const HMODULE opengl32 = GetModuleHandleW(L"opengl32.dll");
        proc = opengl32 != nullptr ? GetProcAddress(opengl32, name) : nullptr;
    }
    return reinterpret_cast<T>(proc);
}

const char* GlFormatName(int64_t format)
{
    switch (format) {
    case kGlSrgb8Alpha8: return "GL_SRGB8_ALPHA8";
    case kGlRgba8: return "GL_RGBA8";
    case kGlRgba16f: return "GL_RGBA16F";
    case kGlDepthComponent32f: return "GL_DEPTH_COMPONENT32F";
    case kGlDepthComponent24: return "GL_DEPTH_COMPONENT24";
    case kGlDepth24Stencil8: return "GL_DEPTH24_STENCIL8";
    case kGlDepth32fStencil8: return "GL_DEPTH32F_STENCIL8";
    default: return "UNKNOWN";
    }
}

void AppendU16(std::vector<uint8_t>& bytes, uint16_t value)
{
    bytes.push_back(static_cast<uint8_t>(value));
    bytes.push_back(static_cast<uint8_t>(value >> 8));
}

void AppendU32(std::vector<uint8_t>& bytes, uint32_t value)
{
    bytes.push_back(static_cast<uint8_t>(value));
    bytes.push_back(static_cast<uint8_t>(value >> 8));
    bytes.push_back(static_cast<uint8_t>(value >> 16));
    bytes.push_back(static_cast<uint8_t>(value >> 24));
}

bool WriteRgbBmp(
    const std::filesystem::path& path,
    int32_t width,
    int32_t height,
    const std::vector<uint8_t>& rgbaPixels,
    bool alphaOnly)
{
    if (width <= 0 || height <= 0
        || rgbaPixels.size() != static_cast<size_t>(width) * height * 4) {
        return false;
    }

    const uint32_t rowBytes = static_cast<uint32_t>(width) * 3;
    const uint32_t rowStride = (rowBytes + 3u) & ~3u;
    const uint32_t pixelBytes = rowStride * static_cast<uint32_t>(height);
    std::vector<uint8_t> bmp;
    bmp.reserve(54u + pixelBytes);
    bmp.push_back('B');
    bmp.push_back('M');
    AppendU32(bmp, 54u + pixelBytes);
    AppendU16(bmp, 0);
    AppendU16(bmp, 0);
    AppendU32(bmp, 54);
    AppendU32(bmp, 40);
    AppendU32(bmp, static_cast<uint32_t>(width));
    AppendU32(bmp, static_cast<uint32_t>(height));
    AppendU16(bmp, 1);
    AppendU16(bmp, 24);
    AppendU32(bmp, 0);
    AppendU32(bmp, pixelBytes);
    AppendU32(bmp, 2835);
    AppendU32(bmp, 2835);
    AppendU32(bmp, 0);
    AppendU32(bmp, 0);

    for (int32_t y = 0; y < height; ++y) {
        const size_t sourceRow = static_cast<size_t>(y) * width * 4;
        for (int32_t x = 0; x < width; ++x) {
            const size_t source = sourceRow + static_cast<size_t>(x) * 4;
            if (alphaOnly) {
                const uint8_t alpha = rgbaPixels[source + 3];
                bmp.push_back(alpha);
                bmp.push_back(alpha);
                bmp.push_back(alpha);
            } else {
                bmp.push_back(rgbaPixels[source + 2]);
                bmp.push_back(rgbaPixels[source + 1]);
                bmp.push_back(rgbaPixels[source + 0]);
            }
        }
        bmp.resize(bmp.size() + rowStride - rowBytes, 0);
    }

    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) {
        return false;
    }
    stream.write(reinterpret_cast<const char*>(bmp.data()), static_cast<std::streamsize>(bmp.size()));
    return stream.good();
}

} // namespace

bool OpenXRGLBridge::Initialize(
    XrSession session,
    const std::vector<XrViewConfigurationView>& views,
    const std::vector<int64_t>& formats,
    int resolutionScalePercent,
    bool foveationSwapchainEnabled,
    bool depthCaptureProbeEnabled,
    bool depthCompositionSubmitEnabled,
    bool hudLayerEnabled,
    int hudWidth,
    int hudHeight,
    bool suppressCenterCrosshair,
    int crosshairClearRadiusPixels,
    bool interactionReticleEnabled,
    bool interactionReticleNativeIconsEnabled,
    int interactionReticleSizePixels,
    bool statusPanelEnabled,
    int statusPanelWidthPixels,
    int statusPanelHeightPixels,
    bool comfortVignetteEnabled,
    int comfortVignetteSizePixels)
{
    ScopedOwnOpenGLWork ownGl;
    Shutdown();

    if (session == XR_NULL_HANDLE || views.size() < 2 || formats.empty() || wglGetCurrentContext() == nullptr) {
        Logger::Instance().Write(
            LogLevel::Warn,
            "openxr_gl_bridge initialize_invalid session=%d views=%zu formats=%zu currentContext=%d",
            session != XR_NULL_HANDLE ? 1 : 0,
            views.size(),
            formats.size(),
            wglGetCurrentContext() != nullptr ? 1 : 0);
        return false;
    }

    if (!ResolveFunctions()) {
        return false;
    }

    constexpr std::array<int64_t, 3> preferredFormats = {
        kGlSrgb8Alpha8,
        kGlRgba8,
        kGlRgba16f,
    };
    for (const int64_t preferred : preferredFormats) {
        if (std::find(formats.begin(), formats.end(), preferred) != formats.end()) {
            colorFormat_ = preferred;
            break;
        }
    }
    if (colorFormat_ == 0) {
        Logger::Instance().Write(LogLevel::Warn, "openxr_gl_bridge no_supported_color_format");
        return false;
    }

    depthCompositionSubmitEnabled_ = depthCompositionSubmitEnabled;
    if (depthCompositionSubmitEnabled_) {
        int32_t sourceDepthBits = 0;
        int32_t sourceStencilBits = 0;
        glGetIntegerv(kGlDepthBits, &sourceDepthBits);
        glGetIntegerv(kGlStencilBits, &sourceStencilBits);
        const std::array<int64_t, 4> preferredDepthFormats = sourceStencilBits > 0
            ? std::array<int64_t, 4>{
                kGlDepth24Stencil8,
                kGlDepth32fStencil8,
                kGlDepthComponent24,
                kGlDepthComponent32f,
            }
            : std::array<int64_t, 4>{
                kGlDepthComponent24,
                kGlDepthComponent32f,
                kGlDepth24Stencil8,
                kGlDepth32fStencil8,
            };
        for (const int64_t preferred : preferredDepthFormats) {
            if (std::find(formats.begin(), formats.end(), preferred) != formats.end()) {
                depthFormat_ = preferred;
                break;
            }
        }
        if (depthFormat_ == 0) {
            depthCompositionSubmitEnabled_ = false;
            Logger::Instance().Write(
                LogLevel::Warn,
                "openxr_depth_submission disabled reason=no_supported_depth_format");
        } else {
            Logger::Instance().Write(
                LogLevel::Info,
                "openxr_depth_submission format_selected sourceDepthBits=%d sourceStencilBits=%d format=0x%llx(%s)",
                sourceDepthBits,
                sourceStencilBits,
                static_cast<unsigned long long>(depthFormat_),
                GlFormatName(depthFormat_));
        }
    }

    session_ = session;
    foveationSwapchainEnabled_ = foveationSwapchainEnabled;
    depthCaptureProbeEnabled_ = depthCaptureProbeEnabled || depthCompositionSubmitEnabled_;
    suppressCenterCrosshair_ = suppressCenterCrosshair;
    crosshairClearRadiusPixels_ = std::clamp(crosshairClearRadiusPixels, 4, 256);
    interactionReticleNativeIconsEnabled_ = interactionReticleNativeIconsEnabled;
    eyes_.reserve(2);
    for (uint32_t eyeIndex = 0; eyeIndex < 2; ++eyeIndex) {
        if (!CreateEyeSwapchain(session, views[eyeIndex], eyeIndex, resolutionScalePercent)) {
            Shutdown();
            return false;
        }
    }
    if (hudLayerEnabled && !CreateHudSwapchain(session, hudWidth, hudHeight)) {
        Logger::Instance().Write(
            LogLevel::Warn,
            "openxr_hud disabled reason=swapchain_creation_failed requested=%dx%d",
            hudWidth,
            hudHeight);
    }
    if (interactionReticleEnabled
        && !CreateInteractionReticleSwapchain(session, interactionReticleSizePixels)) {
        Logger::Instance().Write(
            LogLevel::Warn,
            "openxr_interaction_reticle disabled reason=swapchain_creation_failed requestedSize=%d",
            interactionReticleSizePixels);
    }
    if (interactionReticleEnabled
        && !CreateControllerAimGuideSwapchain(session, interactionReticleSizePixels)) {
        Logger::Instance().Write(
            LogLevel::Warn,
            "openxr_controller_aim_guide disabled reason=swapchain_creation_failed requestedSize=%d",
            interactionReticleSizePixels);
    }
    if (InteractionReticleReady() && interactionReticleNativeIconsEnabled_) {
        LoadInteractionReticleAssets();
    }
    if (statusPanelEnabled
        && !CreateStatusPanelSwapchain(session, statusPanelWidthPixels, statusPanelHeightPixels)) {
        Logger::Instance().Write(
            LogLevel::Warn,
            "openxr_status_panel disabled reason=swapchain_creation_failed requested=%dx%d",
            statusPanelWidthPixels,
            statusPanelHeightPixels);
    }
    if (comfortVignetteEnabled
        && !CreateComfortVignetteSwapchain(session, comfortVignetteSizePixels)) {
        Logger::Instance().Write(
            LogLevel::Warn,
            "openxr_comfort_vignette disabled reason=swapchain_creation_failed requestedSize=%d",
            comfortVignetteSizePixels);
    }

    Logger::Instance().Write(
        LogLevel::Info,
        "openxr_gl_bridge ready eyes=%zu format=0x%llx(%s) resolutionScalePercent=%d foveationSwapchains=%d depthCaptureProbe=%d depthSubmitRequested=%d depthFormat=0x%llx(%s) depthCachesReady=%d depthSwapchainsReady=%d hudReady=%d hudSize=%dx%d suppressCenterCrosshair=%d crosshairClearRadiusPixels=%d interactionReticleReady=%d controllerAimGuideReady=%d reticleSize=%dx%d nativeReticleIcons=%u statusPanelReady=%d statusPanelSize=%dx%d comfortVignetteReady=%d comfortVignetteSize=%dx%d",
        eyes_.size(),
        static_cast<unsigned long long>(colorFormat_),
        GlFormatName(colorFormat_),
        resolutionScalePercent,
        foveationSwapchainEnabled_ ? 1 : 0,
        depthCaptureProbeEnabled_ ? 1 : 0,
        depthCompositionSubmitEnabled ? 1 : 0,
        static_cast<unsigned long long>(depthFormat_),
        GlFormatName(depthFormat_),
        DepthCachesReady() ? 1 : 0,
        DepthSwapchainsReady() ? 1 : 0,
        HudReady() ? 1 : 0,
        hud_.width,
        hud_.height,
        suppressCenterCrosshair_ ? 1 : 0,
        crosshairClearRadiusPixels_,
        InteractionReticleReady() ? 1 : 0,
        ControllerAimGuideReady() ? 1 : 0,
        interactionReticle_.width,
        interactionReticle_.height,
        interactionReticleAssetsLoaded_,
        StatusPanelReady() ? 1 : 0,
        statusPanel_.width,
        statusPanel_.height,
        ComfortVignetteReady() ? 1 : 0,
        comfortVignette_.width,
        comfortVignette_.height);
    return true;
}

void OpenXRGLBridge::Shutdown(bool deleteGlResources)
{
    ScopedOwnOpenGLWork ownGl;
    EndTerminalCaptureGuard();
    if (terminalColorClearSuppressionActive_) {
        EndTerminalColorClearSuppression();
        terminalColorClearSuppressionActive_ = false;
    }
    if (hudCaptureState_.active) {
        if (deleteGlResources && wglGetCurrentContext() != nullptr) {
            RestoreHudCaptureState();
        } else {
            hudCaptureState_.active = false;
        }
    }
    const bool canDeleteGlResources = deleteGlResources && wglGetCurrentContext() != nullptr;
    const bool canDeleteFramebuffers = glDeleteFramebuffers_ != nullptr && canDeleteGlResources;
    for (EyeSwapchain& eye : eyes_) {
        if (canDeleteGlResources && glDeleteQueries_ != nullptr) {
            PollGpuTiming(eye);
            for (GpuTimestampSlot& slot : eye.gpuTimestampSlots) {
                const uint32_t queries[2] = {slot.startQuery, slot.endQuery};
                if (queries[0] != 0 || queries[1] != 0) {
                    glDeleteQueries_(2, queries);
                }
                slot = {};
            }
        }
        if (canDeleteFramebuffers && eye.cacheFramebuffer != 0) {
            glDeleteFramebuffers_(1, &eye.cacheFramebuffer);
            eye.cacheFramebuffer = 0;
        }
        if (canDeleteGlResources && eye.cacheTexture != 0) {
            glDeleteTextures(1, &eye.cacheTexture);
            eye.cacheTexture = 0;
        }
        if (canDeleteGlResources && eye.depthCacheTexture != 0) {
            glDeleteTextures(1, &eye.depthCacheTexture);
            eye.depthCacheTexture = 0;
        }
        if (canDeleteFramebuffers && !eye.framebuffers.empty()) {
            glDeleteFramebuffers_(static_cast<int32_t>(eye.framebuffers.size()), eye.framebuffers.data());
        }
        if (canDeleteFramebuffers && !eye.depthFramebuffers.empty()) {
            glDeleteFramebuffers_(
                static_cast<int32_t>(eye.depthFramebuffers.size()),
                eye.depthFramebuffers.data());
        }
        if (eye.handle != XR_NULL_HANDLE) {
            xrDestroySwapchain(eye.handle);
            eye.handle = XR_NULL_HANDLE;
        }
        if (eye.depthHandle != XR_NULL_HANDLE) {
            xrDestroySwapchain(eye.depthHandle);
            eye.depthHandle = XR_NULL_HANDLE;
        }
    }
    eyes_.clear();
    if (canDeleteFramebuffers && hud_.captureFramebuffer != 0) {
        glDeleteFramebuffers_(1, &hud_.captureFramebuffer);
    }
    if (canDeleteGlResources && hud_.captureTexture != 0) {
        glDeleteTextures(1, &hud_.captureTexture);
    }
    if (canDeleteFramebuffers && hud_.terminalFramebuffer != 0) {
        glDeleteFramebuffers_(1, &hud_.terminalFramebuffer);
    }
    if (canDeleteGlResources && hud_.terminalTexture != 0) {
        glDeleteTextures(1, &hud_.terminalTexture);
    }
    if (canDeleteFramebuffers && !hud_.framebuffers.empty()) {
        glDeleteFramebuffers_(static_cast<int32_t>(hud_.framebuffers.size()), hud_.framebuffers.data());
    }
    if (hud_.handle != XR_NULL_HANDLE) {
        xrDestroySwapchain(hud_.handle);
    }
    hud_ = {};
    if (canDeleteFramebuffers && !interactionReticle_.framebuffers.empty()) {
        glDeleteFramebuffers_(
            static_cast<int32_t>(interactionReticle_.framebuffers.size()),
            interactionReticle_.framebuffers.data());
    }
    if (interactionReticle_.handle != XR_NULL_HANDLE) {
        xrDestroySwapchain(interactionReticle_.handle);
    }
    interactionReticle_ = {};
    if (canDeleteFramebuffers && !controllerAimGuide_.framebuffers.empty()) {
        glDeleteFramebuffers_(
            static_cast<int32_t>(controllerAimGuide_.framebuffers.size()),
            controllerAimGuide_.framebuffers.data());
    }
    if (controllerAimGuide_.handle != XR_NULL_HANDLE) {
        xrDestroySwapchain(controllerAimGuide_.handle);
    }
    controllerAimGuide_ = {};
    interactionReticleAssets_ = {};
    interactionReticleUploadPixels_.clear();
    interactionReticleNativeIconsEnabled_ = false;
    interactionReticleAssetsLoaded_ = 0;
    if (statusPanel_.handle != XR_NULL_HANDLE) {
        xrDestroySwapchain(statusPanel_.handle);
    }
    statusPanel_ = {};
    if (comfortVignette_.handle != XR_NULL_HANDLE) {
        xrDestroySwapchain(comfortVignette_.handle);
    }
    comfortVignette_ = {};
    hudCaptureState_ = {};
    session_ = XR_NULL_HANDLE;
    colorFormat_ = 0;
    depthFormat_ = 0;
    foveationSwapchainEnabled_ = false;
    depthCaptureProbeEnabled_ = false;
    depthCompositionSubmitEnabled_ = false;
}

bool OpenXRGLBridge::CopyBackbufferToEye(uint32_t eyeIndex)
{
    ScopedOwnOpenGLWork ownGl;
    // Brackets the whole acquire/wait/copy/release transaction, so GL
    // state the runtime clobbers on either side is restored.
    ScopedRuntimeGlState runtimeGlState(glBindFramebuffer_);
    if (!Ready() || eyeIndex >= eyes_.size()) {
        return false;
    }

    EyeSwapchain& eye = eyes_[eyeIndex];
    ScopedSwapchainTransferTiming transferTiming(
        eye.colorTransferTiming,
        ColorTransferSource::Backbuffer);
    uint32_t imageIndex = eye.acquiredColorImageIndex;
    XrResult result = XR_SUCCESS;
    if (!eye.colorImageAcquired) {
        XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
        result = transferTiming.MeasureAcquire([&] {
            return xrAcquireSwapchainImage(eye.handle, &acquireInfo, &imageIndex);
        });
        if (XR_FAILED(result)) {
            Logger::Instance().Write(
                LogLevel::Warn,
                "openxr_swapchain acquire_failed eye=%u result=%d",
                eyeIndex,
                static_cast<int>(result));
            return false;
        }
        eye.colorImageAcquired = true;
        eye.acquiredColorImageIndex = imageIndex;
    }

    XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
    waitInfo.timeout = kSwapchainWaitTimeout;
    result = transferTiming.MeasureWait([&] {
        return xrWaitSwapchainImage(eye.handle, &waitInfo);
    });
    if (XR_FAILED(result)) {
        Logger::Instance().Write(
            LogLevel::Warn,
            "openxr_swapchain wait_failed eye=%u image=%u result=%d",
            eyeIndex,
            imageIndex,
            static_cast<int>(result));
        return false;
    }

    const bool copied = transferTiming.MeasureCopy([&] {
        const int gpuSlot = BeginGpuTiming(eye, GpuTransferPhase::Submit);
        const bool result = CopyBackbufferToImage(eye, imageIndex);
        EndGpuTiming(eye, gpuSlot);
        return result;
    });
    transferTiming.MeasureFlush([] { glFlush(); });

    XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
    result = transferTiming.MeasureRelease([&] {
        return xrReleaseSwapchainImage(eye.handle, &releaseInfo);
    });
    if (XR_FAILED(result)) {
        Logger::Instance().Write(
            LogLevel::Warn,
            "openxr_swapchain release_failed eye=%u image=%u result=%d",
            eyeIndex,
            imageIndex,
            static_cast<int>(result));
        return false;
    }
    eye.colorImageAcquired = false;
    transferTiming.SetSucceeded(copied);
    return copied;
}

bool OpenXRGLBridge::CaptureBackbufferToCache(uint32_t eyeIndex)
{
    ScopedOwnOpenGLWork ownGl;
    if (!Ready() || eyeIndex >= eyes_.size()) {
        return false;
    }

    EyeSwapchain& eye = eyes_[eyeIndex];
    if (eye.cacheFramebuffer == 0) {
        return false;
    }

    int32_t viewport[4] = {};
    int32_t savedReadFramebuffer = 0;
    int32_t savedDrawFramebuffer = 0;
    int32_t savedReadBuffer = 0;
    int32_t savedDrawBuffer = 0;
    glGetIntegerv(kGlViewport, viewport);
    glGetIntegerv(kGlReadFramebufferBinding, &savedReadFramebuffer);
    glGetIntegerv(kGlDrawFramebufferBinding, &savedDrawFramebuffer);
    glGetIntegerv(kGlReadBuffer, &savedReadBuffer);
    glGetIntegerv(kGlDrawBuffer, &savedDrawBuffer);
    if (viewport[2] <= 0 || viewport[3] <= 0) {
        return false;
    }

    const GLboolean scissorEnabled = glIsEnabled(kGlScissorTest);
    if (scissorEnabled == GL_TRUE) {
        glDisable(kGlScissorTest);
    }

    glBindFramebuffer_(kGlReadFramebuffer, 0);
    glReadBuffer(kGlBack);
    glBindFramebuffer_(kGlDrawFramebuffer, eye.cacheFramebuffer);
    glDrawBuffer(kGlColorAttachment0);
    const int gpuSlot = BeginGpuTiming(eye, GpuTransferPhase::Capture);
    glBlitFramebuffer_(
        viewport[0],
        viewport[1],
        viewport[0] + viewport[2],
        viewport[1] + viewport[3],
        0,
        0,
        eye.width,
        eye.height,
        kGlColorBufferBit,
        kGlLinear);
    EndGpuTiming(eye, gpuSlot);

    if (depthCaptureProbeEnabled_ && eye.depthCacheTexture != 0) {
        int32_t depthBits = 0;
        glGetIntegerv(kGlDepthBits, &depthBits);
        const uint32_t priorError = glGetError();
        glBlitFramebuffer_(
            viewport[0],
            viewport[1],
            viewport[0] + viewport[2],
            viewport[1] + viewport[3],
            0,
            0,
            eye.width,
            eye.height,
            kGlDepthBufferBit,
            kGlNearest);
        const uint32_t depthError = glGetError();
        eye.depthCacheValid = depthBits > 0 && depthError == GL_NO_ERROR;
        ++eye.depthProbeSamples;
        if (eye.depthProbeSamples <= 4 || eye.depthProbeSamples % 120 == 0) {
            float centerDepth[16] = {};
            const int32_t sampleWidth = std::min(viewport[2], 4);
            const int32_t sampleHeight = std::min(viewport[3], 4);
            const int32_t sampleX = viewport[0] + std::max(0, (viewport[2] - sampleWidth) / 2);
            const int32_t sampleY = viewport[1] + std::max(0, (viewport[3] - sampleHeight) / 2);
            glReadPixels(
                sampleX,
                sampleY,
                sampleWidth,
                sampleHeight,
                kGlDepthComponent,
                GL_FLOAT,
                centerDepth);
            const uint32_t sampleError = glGetError();
            float minimumDepth = 1.0f;
            float maximumDepth = 0.0f;
            bool finite = true;
            for (int32_t index = 0; index < sampleWidth * sampleHeight; ++index) {
                finite = finite && std::isfinite(centerDepth[index]);
                minimumDepth = std::min(minimumDepth, centerDepth[index]);
                maximumDepth = std::max(maximumDepth, centerDepth[index]);
            }
            Logger::Instance().Write(
                eye.depthCacheValid ? LogLevel::Info : LogLevel::Warn,
                "openxr_depth_cache_probe eye=%u sample=%llu sourceSize=%dx%d cacheSize=%dx%d depthBits=%d priorError=0x%x blitError=0x%x sampleError=0x%x valid=%d centerFinite=%d centerMin=%.7f centerMax=%.7f submitEnabled=%d depthFormat=0x%llx(%s)",
                eyeIndex,
                static_cast<unsigned long long>(eye.depthProbeSamples),
                viewport[2], viewport[3],
                eye.width, eye.height,
                depthBits,
                priorError,
                depthError,
                sampleError,
                eye.depthCacheValid ? 1 : 0,
                finite ? 1 : 0,
                minimumDepth,
                maximumDepth,
                depthCompositionSubmitEnabled_ ? 1 : 0,
                static_cast<unsigned long long>(depthFormat_),
                GlFormatName(depthFormat_));
        }
    } else {
        eye.depthCacheValid = false;
    }

    glBindFramebuffer_(kGlReadFramebuffer, static_cast<uint32_t>(savedReadFramebuffer));
    glReadBuffer(static_cast<uint32_t>(savedReadBuffer));
    glBindFramebuffer_(kGlDrawFramebuffer, static_cast<uint32_t>(savedDrawFramebuffer));
    glDrawBuffer(static_cast<uint32_t>(savedDrawBuffer));
    if (scissorEnabled == GL_TRUE) {
        glEnable(kGlScissorTest);
    }

    eye.cacheValid = true;
    return true;
}

bool OpenXRGLBridge::CopyCacheToEye(uint32_t eyeIndex)
{
    ScopedOwnOpenGLWork ownGl;
    // Brackets the whole acquire/wait/copy/release transaction, so GL
    // state the runtime clobbers on either side is restored.
    ScopedRuntimeGlState runtimeGlState(glBindFramebuffer_);
    if (!Ready() || eyeIndex >= eyes_.size() || !eyes_[eyeIndex].cacheValid) {
        return false;
    }

    EyeSwapchain& eye = eyes_[eyeIndex];
    ScopedSwapchainTransferTiming transferTiming(
        eye.colorTransferTiming,
        ColorTransferSource::StereoCache);
    uint32_t imageIndex = eye.acquiredColorImageIndex;
    XrResult result = XR_SUCCESS;
    if (!eye.colorImageAcquired) {
        XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
        result = transferTiming.MeasureAcquire([&] {
            return xrAcquireSwapchainImage(eye.handle, &acquireInfo, &imageIndex);
        });
        if (XR_FAILED(result)) {
            Logger::Instance().Write(
                LogLevel::Warn,
                "openxr_swapchain cache_acquire_failed eye=%u result=%d",
                eyeIndex,
                static_cast<int>(result));
            return false;
        }
        eye.colorImageAcquired = true;
        eye.acquiredColorImageIndex = imageIndex;
    }

    XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
    waitInfo.timeout = kSwapchainWaitTimeout;
    result = transferTiming.MeasureWait([&] {
        return xrWaitSwapchainImage(eye.handle, &waitInfo);
    });
    if (XR_FAILED(result)) {
        Logger::Instance().Write(
            LogLevel::Warn,
            "openxr_swapchain cache_wait_failed eye=%u image=%u result=%d",
            eyeIndex,
            imageIndex,
            static_cast<int>(result));
        return false;
    }

    const bool copied = transferTiming.MeasureCopy([&] {
        const int gpuSlot = BeginGpuTiming(eye, GpuTransferPhase::Submit);
        const bool result = CopyCacheToImage(eye, imageIndex);
        EndGpuTiming(eye, gpuSlot);
        return result;
    });
    transferTiming.MeasureFlush([] { glFlush(); });

    XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
    result = transferTiming.MeasureRelease([&] {
        return xrReleaseSwapchainImage(eye.handle, &releaseInfo);
    });
    if (XR_FAILED(result)) {
        Logger::Instance().Write(
            LogLevel::Warn,
            "openxr_swapchain cache_release_failed eye=%u image=%u result=%d",
            eyeIndex,
            imageIndex,
            static_cast<int>(result));
        return false;
    }
    eye.colorImageAcquired = false;
    transferTiming.SetSucceeded(copied);
    return copied;
}

bool OpenXRGLBridge::ClearEyeToBlack(uint32_t eyeIndex)
{
    ScopedOwnOpenGLWork ownGl;
    // Brackets the whole acquire/wait/copy/release transaction, so GL
    // state the runtime clobbers on either side is restored.
    ScopedRuntimeGlState runtimeGlState(glBindFramebuffer_);
    if (!Ready() || eyeIndex >= eyes_.size()) return false;

    EyeSwapchain& eye = eyes_[eyeIndex];
    ScopedSwapchainTransferTiming transferTiming(
        eye.colorTransferTiming,
        ColorTransferSource::Black);
    uint32_t imageIndex = eye.acquiredColorImageIndex;
    XrResult result = XR_SUCCESS;
    if (!eye.colorImageAcquired) {
        XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
        result = transferTiming.MeasureAcquire([&] {
            return xrAcquireSwapchainImage(eye.handle, &acquireInfo, &imageIndex);
        });
        if (XR_FAILED(result)) {
            Logger::Instance().Write(
                LogLevel::Warn,
                "openxr_swapchain black_acquire_failed eye=%u result=%d",
                eyeIndex,
                static_cast<int>(result));
            return false;
        }
        eye.colorImageAcquired = true;
        eye.acquiredColorImageIndex = imageIndex;
    }

    XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
    waitInfo.timeout = kSwapchainWaitTimeout;
    result = transferTiming.MeasureWait([&] {
        return xrWaitSwapchainImage(eye.handle, &waitInfo);
    });
    if (XR_FAILED(result)) {
        Logger::Instance().Write(
            LogLevel::Warn,
            "openxr_swapchain black_wait_failed eye=%u image=%u result=%d retained=%d",
            eyeIndex,
            imageIndex,
            static_cast<int>(result),
            eye.colorImageAcquired ? 1 : 0);
        return false;
    }

    const int64_t copyStartQpc = TransferQpcNow();
    int32_t savedDrawFramebuffer = 0;
    int32_t savedViewport[4] = {};
    float savedClearColor[4] = {};
    unsigned char savedColorMask[4] = {};
    int32_t savedScissorBox[4] = {};
    glGetIntegerv(kGlDrawFramebufferBinding, &savedDrawFramebuffer);
    glGetIntegerv(kGlViewport, savedViewport);
    glGetFloatv(kGlColorClearValue, savedClearColor);
    glGetBooleanv(kGlColorWriteMask, savedColorMask);
    glGetIntegerv(kGlScissorBox, savedScissorBox);
    const GLboolean scissorEnabled = glIsEnabled(kGlScissorTest);

    glBindFramebuffer_(kGlDrawFramebuffer, eye.framebuffers[imageIndex]);
    glViewport(0, 0, eye.width, eye.height);
    if (scissorEnabled == GL_TRUE) glDisable(kGlScissorTest);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    const int gpuSlot = BeginGpuTiming(eye, GpuTransferPhase::Submit);
    glClear(kGlColorBufferBit);
    EndGpuTiming(eye, gpuSlot);

    glClearColor(
        savedClearColor[0], savedClearColor[1], savedClearColor[2], savedClearColor[3]);
    glColorMask(
        savedColorMask[0], savedColorMask[1], savedColorMask[2], savedColorMask[3]);
    glBindFramebuffer_(kGlDrawFramebuffer, static_cast<uint32_t>(savedDrawFramebuffer));
    glViewport(savedViewport[0], savedViewport[1], savedViewport[2], savedViewport[3]);
    glScissor(savedScissorBox[0], savedScissorBox[1], savedScissorBox[2], savedScissorBox[3]);
    if (scissorEnabled == GL_TRUE) glEnable(kGlScissorTest);
    transferTiming.AddCopyElapsed(
        TransferQpcDeltaMicroseconds(copyStartQpc, TransferQpcNow()));
    transferTiming.MeasureFlush([] { glFlush(); });

    XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
    result = transferTiming.MeasureRelease([&] {
        return xrReleaseSwapchainImage(eye.handle, &releaseInfo);
    });
    if (XR_FAILED(result)) {
        Logger::Instance().Write(
            LogLevel::Warn,
            "openxr_swapchain black_release_failed eye=%u image=%u result=%d",
            eyeIndex,
            imageIndex,
            static_cast<int>(result));
        return false;
    }
    eye.colorImageAcquired = false;
    transferTiming.SetSucceeded(true);
    return true;
}

bool OpenXRGLBridge::CopyDepthCacheToEye(uint32_t eyeIndex)
{
    ScopedOwnOpenGLWork ownGl;
    // Brackets the whole acquire/wait/copy/release transaction, so GL
    // state the runtime clobbers on either side is restored.
    ScopedRuntimeGlState runtimeGlState(glBindFramebuffer_);
    if (!DepthSwapchainsReady()
        || eyeIndex >= eyes_.size()
        || !eyes_[eyeIndex].depthCacheValid) {
        return false;
    }

    EyeSwapchain& eye = eyes_[eyeIndex];
    uint32_t imageIndex = eye.acquiredDepthImageIndex;
    XrResult result = XR_SUCCESS;
    if (!eye.depthImageAcquired) {
        XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
        result = xrAcquireSwapchainImage(eye.depthHandle, &acquireInfo, &imageIndex);
        if (XR_FAILED(result)) {
            Logger::Instance().Write(
                LogLevel::Warn,
                "openxr_depth_swapchain acquire_failed eye=%u result=%d",
                eyeIndex,
                static_cast<int>(result));
            return false;
        }
        eye.depthImageAcquired = true;
        eye.acquiredDepthImageIndex = imageIndex;
    }

    XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
    waitInfo.timeout = kSwapchainWaitTimeout;
    result = xrWaitSwapchainImage(eye.depthHandle, &waitInfo);
    if (XR_FAILED(result)) {
        Logger::Instance().Write(
            LogLevel::Warn,
            "openxr_depth_swapchain wait_failed eye=%u image=%u result=%d",
            eyeIndex,
            imageIndex,
            static_cast<int>(result));
        return false;
    }

    const bool copied = CopyDepthCacheToImage(eye, imageIndex);
    glFlush();

    XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
    result = xrReleaseSwapchainImage(eye.depthHandle, &releaseInfo);
    if (XR_FAILED(result)) {
        Logger::Instance().Write(
            LogLevel::Warn,
            "openxr_depth_swapchain release_failed eye=%u image=%u result=%d",
            eyeIndex,
            imageIndex,
            static_cast<int>(result));
        return false;
    }
    eye.depthImageAcquired = false;
    return copied;
}

bool OpenXRGLBridge::CopyCacheToBackbuffer(
    uint32_t eyeIndex,
    spectator_math::AspectMode aspectMode)
{
    ScopedOwnOpenGLWork ownGl;
    if (!Ready() || eyeIndex >= eyes_.size() || !eyes_[eyeIndex].cacheValid) {
        return false;
    }

    EyeSwapchain& eye = eyes_[eyeIndex];
    int32_t viewport[4] = {};
    int32_t savedReadFramebuffer = 0;
    int32_t savedDrawFramebuffer = 0;
    int32_t savedReadBuffer = 0;
    int32_t savedDrawBuffer = 0;
    float savedClearColor[4] = {};
    GLboolean savedColorMask[4] = {};
    glGetIntegerv(kGlViewport, viewport);
    glGetIntegerv(kGlReadFramebufferBinding, &savedReadFramebuffer);
    glGetIntegerv(kGlDrawFramebufferBinding, &savedDrawFramebuffer);
    glGetIntegerv(kGlReadBuffer, &savedReadBuffer);
    glGetIntegerv(kGlDrawBuffer, &savedDrawBuffer);
    glGetFloatv(kGlColorClearValue, savedClearColor);
    glGetBooleanv(kGlColorWriteMask, savedColorMask);

    spectator_math::BlitLayout layout;
    if (!spectator_math::ComputeBlitLayout(
            eye.width,
            eye.height,
            viewport[2],
            viewport[3],
            aspectMode,
            layout)) {
        return false;
    }

    const GLboolean scissorEnabled = glIsEnabled(kGlScissorTest);
    if (scissorEnabled == GL_TRUE) {
        glDisable(kGlScissorTest);
    }

    glBindFramebuffer_(kGlDrawFramebuffer, 0);
    glDrawBuffer(kGlBack);
    if (layout.clearDestination) {
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(kGlColorBufferBit);
        glClearColor(
            savedClearColor[0],
            savedClearColor[1],
            savedClearColor[2],
            savedClearColor[3]);
        glColorMask(
            savedColorMask[0],
            savedColorMask[1],
            savedColorMask[2],
            savedColorMask[3]);
    }

    glBindFramebuffer_(kGlReadFramebuffer, eye.cacheFramebuffer);
    glReadBuffer(kGlColorAttachment0);
    glBlitFramebuffer_(
        layout.sourceX0,
        layout.sourceY0,
        layout.sourceX1,
        layout.sourceY1,
        viewport[0] + layout.destinationX0,
        viewport[1] + layout.destinationY0,
        viewport[0] + layout.destinationX1,
        viewport[1] + layout.destinationY1,
        kGlColorBufferBit,
        kGlLinear);

    glBindFramebuffer_(kGlReadFramebuffer, static_cast<uint32_t>(savedReadFramebuffer));
    glReadBuffer(static_cast<uint32_t>(savedReadBuffer));
    glBindFramebuffer_(kGlDrawFramebuffer, static_cast<uint32_t>(savedDrawFramebuffer));
    glDrawBuffer(static_cast<uint32_t>(savedDrawBuffer));
    if (scissorEnabled == GL_TRUE) {
        glEnable(kGlScissorTest);
    }
    return true;
}

bool OpenXRGLBridge::BeginHudCapture(uint64_t frameIndex, bool preservePreviousFrame)
{
    ScopedOwnOpenGLWork ownGl;
    if (!HudReady()
        || hudCaptureState_.active
        || wglGetCurrentContext() == nullptr) {
        return false;
    }

    glGetIntegerv(kGlReadFramebufferBinding, &hudCaptureState_.readFramebuffer);
    glGetIntegerv(kGlDrawFramebufferBinding, &hudCaptureState_.drawFramebuffer);
    glGetIntegerv(kGlReadBuffer, &hudCaptureState_.readBuffer);
    glGetIntegerv(kGlDrawBuffer, &hudCaptureState_.drawBuffer);
    glGetIntegerv(kGlViewport, hudCaptureState_.viewport);
    glGetFloatv(kGlColorClearValue, hudCaptureState_.clearColor);
    glGetBooleanv(kGlColorWriteMask, hudCaptureState_.colorMask);
    glGetIntegerv(kGlScissorBox, hudCaptureState_.scissorBox);
    hudCaptureState_.scissorEnabled = glIsEnabled(kGlScissorTest) == GL_TRUE;
    hudCaptureState_.active = true;

    glBindFramebuffer_(kGlFramebuffer, hud_.captureFramebuffer);
    glReadBuffer(kGlColorAttachment0);
    glDrawBuffer(kGlColorAttachment0);
    glViewport(0, 0, hud_.width, hud_.height);
    glDisable(kGlScissorTest);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    const bool appendToCurrentFrame = hud_.captureValid && hud_.captureFrame == frameIndex;
    const bool retainPreviousFrame = preservePreviousFrame && hud_.captureValid;
    if (!appendToCurrentFrame && !retainPreviousFrame) {
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(kGlColorBufferBit);
    }
    glClearColor(
        hudCaptureState_.clearColor[0],
        hudCaptureState_.clearColor[1],
        hudCaptureState_.clearColor[2],
        hudCaptureState_.clearColor[3]);
    glColorMask(
        hudCaptureState_.colorMask[0],
        hudCaptureState_.colorMask[1],
        hudCaptureState_.colorMask[2],
        hudCaptureState_.colorMask[3]);
    hud_.captureValid = false;
    hud_.captureFrame = frameIndex;
    return true;
}

bool OpenXRGLBridge::EndHudCapture(uint64_t frameIndex, bool suppressCenterCrosshair)
{
    ScopedOwnOpenGLWork ownGl;
    if (!hudCaptureState_.active) {
        return false;
    }
    if (suppressCenterCrosshair_ && suppressCenterCrosshair) {
        const int radius = std::min(
            crosshairClearRadiusPixels_,
            std::max(4, std::min(hud_.width, hud_.height) / 4));
        glBindFramebuffer_(kGlDrawFramebuffer, hud_.captureFramebuffer);
        glDrawBuffer(kGlColorAttachment0);
        glEnable(kGlScissorTest);
        glScissor(hud_.width / 2 - radius, hud_.height / 2 - radius, radius * 2, radius * 2);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(kGlColorBufferBit);
    }
    RestoreHudCaptureState();
    hud_.captureFrame = frameIndex;
    hud_.captureValid = true;
    return true;
}

bool OpenXRGLBridge::BeginTerminalHudCapture(
    uint64_t frameIndex,
    int width,
    int height,
    bool preservePreviousFrame,
    bool preserveDirtyRects)
{
    ScopedOwnOpenGLWork ownGl;
    EndTerminalCaptureGuard();
    if (terminalColorClearSuppressionActive_) {
        EndTerminalColorClearSuppression();
        terminalColorClearSuppressionActive_ = false;
    }
    if (!HudReady()
        || hudCaptureState_.active
        || wglGetCurrentContext() == nullptr
        || !CreateTerminalHudCaptureTarget(width, height)) {
        return false;
    }

    glGetIntegerv(kGlReadFramebufferBinding, &hudCaptureState_.readFramebuffer);
    glGetIntegerv(kGlDrawFramebufferBinding, &hudCaptureState_.drawFramebuffer);
    glGetIntegerv(kGlReadBuffer, &hudCaptureState_.readBuffer);
    glGetIntegerv(kGlDrawBuffer, &hudCaptureState_.drawBuffer);
    glGetIntegerv(kGlViewport, hudCaptureState_.viewport);
    glGetFloatv(kGlColorClearValue, hudCaptureState_.clearColor);
    glGetBooleanv(kGlColorWriteMask, hudCaptureState_.colorMask);
    glGetIntegerv(kGlScissorBox, hudCaptureState_.scissorBox);
    hudCaptureState_.scissorEnabled = glIsEnabled(kGlScissorTest) == GL_TRUE;
    hudCaptureState_.active = true;

    glBindFramebuffer_(kGlFramebuffer, hud_.terminalFramebuffer);
    glReadBuffer(kGlColorAttachment0);
    glDrawBuffer(kGlColorAttachment0);
    glViewport(0, 0, hud_.terminalWidth, hud_.terminalHeight);
    glDisable(kGlScissorTest);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    const bool retainedSurfaceValid = preservePreviousFrame && hud_.terminalValid;
    if (!retainedSurfaceValid) {
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(kGlColorBufferBit);
    }
    glClearColor(
        hudCaptureState_.clearColor[0],
        hudCaptureState_.clearColor[1],
        hudCaptureState_.clearColor[2],
        hudCaptureState_.clearColor[3]);
    glColorMask(
        hudCaptureState_.colorMask[0],
        hudCaptureState_.colorMask[1],
        hudCaptureState_.colorMask[2],
        hudCaptureState_.colorMask[3]);
    BeginTerminalCaptureGuard(frameIndex, hud_.terminalFramebuffer);
    terminalColorClearSuppressionActive_ = preserveDirtyRects && retainedSurfaceValid;
    if (terminalColorClearSuppressionActive_) {
        BeginTerminalColorClearSuppression(frameIndex, hud_.terminalFramebuffer);
    }
    hud_.terminalValid = false;
    hud_.captureFrame = frameIndex;
    return true;
}

bool OpenXRGLBridge::EndTerminalHudCapture(uint64_t frameIndex)
{
    ScopedOwnOpenGLWork ownGl;
    EndTerminalCaptureGuard();
    if (terminalColorClearSuppressionActive_) {
        EndTerminalColorClearSuppression();
        terminalColorClearSuppressionActive_ = false;
    }
    if (!hudCaptureState_.active
        || hud_.terminalFramebuffer == 0
        || hud_.captureFramebuffer == 0) {
        return false;
    }

    glBindFramebuffer_(kGlReadFramebuffer, hud_.terminalFramebuffer);
    glReadBuffer(kGlColorAttachment0);
    glBindFramebuffer_(kGlDrawFramebuffer, hud_.captureFramebuffer);
    glDrawBuffer(kGlColorAttachment0);
    glDisable(kGlScissorTest);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(kGlColorBufferBit);
    glBlitFramebuffer_(
        0,
        0,
        hud_.terminalWidth,
        hud_.terminalHeight,
        0,
        0,
        hud_.width,
        hud_.height,
        kGlColorBufferBit,
        kGlLinear);

    hud_.terminalValid = true;
    RestoreHudCaptureState();
    hud_.captureFrame = frameIndex;
    hud_.captureValid = true;
    return true;
}

bool OpenXRGLBridge::CaptureFramebufferToHud(
    uint64_t frameIndex,
    uint32_t sourceFramebuffer,
    int sourceX,
    int sourceY,
    int sourceWidth,
    int sourceHeight)
{
    ScopedOwnOpenGLWork ownGl;
    if (sourceFramebuffer == 0
        || sourceWidth <= 0
        || sourceHeight <= 0
        || sourceWidth > 16384
        || sourceHeight > 16384
        || !BeginHudCapture(frameIndex)) {
        return false;
    }

    glBindFramebuffer_(kGlReadFramebuffer, sourceFramebuffer);
    glReadBuffer(kGlColorAttachment0);
    glBindFramebuffer_(kGlDrawFramebuffer, hud_.captureFramebuffer);
    glDrawBuffer(kGlColorAttachment0);
    glDisable(kGlScissorTest);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glBlitFramebuffer_(
        sourceX,
        sourceY,
        sourceX + sourceWidth,
        sourceY + sourceHeight,
        0,
        0,
        hud_.width,
        hud_.height,
        kGlColorBufferBit,
        kGlLinear);
    return EndHudCapture(frameIndex, false);
}

void OpenXRGLBridge::RestoreHudCaptureState()
{
    ScopedOwnOpenGLWork ownGl;
    if (!hudCaptureState_.active) {
        return;
    }
    glBindFramebuffer_(kGlReadFramebuffer, static_cast<uint32_t>(hudCaptureState_.readFramebuffer));
    glReadBuffer(static_cast<uint32_t>(hudCaptureState_.readBuffer));
    glBindFramebuffer_(kGlDrawFramebuffer, static_cast<uint32_t>(hudCaptureState_.drawFramebuffer));
    glDrawBuffer(static_cast<uint32_t>(hudCaptureState_.drawBuffer));
    glViewport(
        hudCaptureState_.viewport[0],
        hudCaptureState_.viewport[1],
        hudCaptureState_.viewport[2],
        hudCaptureState_.viewport[3]);
    glScissor(
        hudCaptureState_.scissorBox[0],
        hudCaptureState_.scissorBox[1],
        hudCaptureState_.scissorBox[2],
        hudCaptureState_.scissorBox[3]);
    glClearColor(
        hudCaptureState_.clearColor[0],
        hudCaptureState_.clearColor[1],
        hudCaptureState_.clearColor[2],
        hudCaptureState_.clearColor[3]);
    glColorMask(
        hudCaptureState_.colorMask[0],
        hudCaptureState_.colorMask[1],
        hudCaptureState_.colorMask[2],
        hudCaptureState_.colorMask[3]);
    if (hudCaptureState_.scissorEnabled) {
        glEnable(kGlScissorTest);
    } else {
        glDisable(kGlScissorTest);
    }
    hudCaptureState_.active = false;
}

bool OpenXRGLBridge::CopyHudCaptureToSwapchain()
{
    ScopedOwnOpenGLWork ownGl;
    if (!HudReady() || !hud_.captureValid) {
        return false;
    }
    uint32_t imageIndex = hud_.acquiredImageIndex;
    XrResult result = XR_SUCCESS;
    if (!hud_.imageAcquired) {
        XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
        result = xrAcquireSwapchainImage(hud_.handle, &acquireInfo, &imageIndex);
        if (XR_FAILED(result)) {
            Logger::Instance().Write(
                LogLevel::Warn,
                "openxr_hud acquire_failed result=%d",
                static_cast<int>(result));
            return false;
        }
        hud_.imageAcquired = true;
        hud_.acquiredImageIndex = imageIndex;
    }
    XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
    waitInfo.timeout = kSwapchainWaitTimeout;
    result = xrWaitSwapchainImage(hud_.handle, &waitInfo);
    if (XR_FAILED(result)) {
        Logger::Instance().Write(
            LogLevel::Warn,
            "openxr_hud wait_failed image=%u result=%d",
            imageIndex,
            static_cast<int>(result));
        return false;
    }
    const bool copied = CopyHudCaptureToImage(imageIndex);
    glFlush();
    XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
    result = xrReleaseSwapchainImage(hud_.handle, &releaseInfo);
    if (XR_FAILED(result)) {
        Logger::Instance().Write(
            LogLevel::Warn,
            "openxr_hud release_failed image=%u result=%d",
            imageIndex,
            static_cast<int>(result));
        return false;
    }
    hud_.imageAcquired = false;
    return copied;
}

bool OpenXRGLBridge::DumpHudCapture(
    uint64_t frameIndex,
    uint64_t sequence,
    uint32_t sampleIndex,
    const char* reason)
{
    ScopedOwnOpenGLWork ownGl;
    if (!hud_.captureValid) {
        return false;
    }
    return DumpCaptureFramebuffer(
        frameIndex,
        sequence,
        sampleIndex,
        reason,
        "terminal_hud",
        hud_.captureFramebuffer,
        hud_.width,
        hud_.height,
        hud_.captureFrame);
}

bool OpenXRGLBridge::DumpTerminalHudCapture(
    uint64_t frameIndex,
    uint64_t sequence,
    uint32_t sampleIndex,
    const char* reason)
{
    ScopedOwnOpenGLWork ownGl;
    if (!hud_.terminalValid) {
        return false;
    }
    return DumpCaptureFramebuffer(
        frameIndex,
        sequence,
        sampleIndex,
        reason,
        "terminal_native",
        hud_.terminalFramebuffer,
        hud_.terminalWidth,
        hud_.terminalHeight,
        hud_.captureFrame);
}

bool OpenXRGLBridge::DumpCaptureFramebuffer(
    uint64_t frameIndex,
    uint64_t sequence,
    uint32_t sampleIndex,
    const char* reason,
    const char* targetName,
    uint32_t framebuffer,
    int width,
    int height,
    uint64_t captureFrame)
{
    ScopedOwnOpenGLWork ownGl;
    if (framebuffer == 0
        || width <= 0
        || height <= 0
        || wglGetCurrentContext() == nullptr) {
        return false;
    }

    int32_t savedReadFramebuffer = 0;
    int32_t savedReadBuffer = 0;
    int32_t savedPackAlignment = 4;
    int32_t savedPackRowLength = 0;
    int32_t savedPackSkipRows = 0;
    int32_t savedPackSkipPixels = 0;
    int32_t savedPixelPackBuffer = 0;
    glGetIntegerv(kGlReadFramebufferBinding, &savedReadFramebuffer);
    glGetIntegerv(kGlReadBuffer, &savedReadBuffer);
    glGetIntegerv(kGlPackAlignment, &savedPackAlignment);
    glGetIntegerv(kGlPackRowLength, &savedPackRowLength);
    glGetIntegerv(kGlPackSkipRows, &savedPackSkipRows);
    glGetIntegerv(kGlPackSkipPixels, &savedPackSkipPixels);
    glGetIntegerv(kGlPixelPackBufferBinding, &savedPixelPackBuffer);

    using GlBindBufferFn = void(APIENTRY*)(uint32_t, uint32_t);
    const GlBindBufferFn glBindBuffer = ResolveGlProc<GlBindBufferFn>("glBindBuffer");
    if (savedPixelPackBuffer != 0 && glBindBuffer == nullptr) {
        Logger::Instance().Write(
            LogLevel::Warn,
            "terminal_hud_dump failed sequence=%llu frame=%llu sample=%u reason=pixel_pack_buffer_restore_unavailable pbo=%d",
            static_cast<unsigned long long>(sequence),
            static_cast<unsigned long long>(frameIndex),
            sampleIndex,
            savedPixelPackBuffer);
        return false;
    }

    const size_t pixelCount = static_cast<size_t>(width) * height;
    std::vector<uint8_t> pixels(pixelCount * 4);
    glBindFramebuffer_(kGlReadFramebuffer, framebuffer);
    glReadBuffer(kGlColorAttachment0);
    if (glBindBuffer != nullptr) {
        glBindBuffer(kGlPixelPackBuffer, 0);
    }
    glPixelStorei(kGlPackAlignment, 1);
    glPixelStorei(kGlPackRowLength, 0);
    glPixelStorei(kGlPackSkipRows, 0);
    glPixelStorei(kGlPackSkipPixels, 0);
    for (int errorDrain = 0; errorDrain < 16 && glGetError() != GL_NO_ERROR; ++errorDrain) {
    }
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    const uint32_t readError = glGetError();

    glPixelStorei(kGlPackAlignment, savedPackAlignment);
    glPixelStorei(kGlPackRowLength, savedPackRowLength);
    glPixelStorei(kGlPackSkipRows, savedPackSkipRows);
    glPixelStorei(kGlPackSkipPixels, savedPackSkipPixels);
    if (glBindBuffer != nullptr) {
        glBindBuffer(kGlPixelPackBuffer, static_cast<uint32_t>(savedPixelPackBuffer));
    }
    glBindFramebuffer_(kGlReadFramebuffer, static_cast<uint32_t>(savedReadFramebuffer));
    glReadBuffer(static_cast<uint32_t>(savedReadBuffer));
    if (readError != GL_NO_ERROR) {
        Logger::Instance().Write(
            LogLevel::Warn,
            "terminal_hud_dump failed sequence=%llu frame=%llu sample=%u reason=read_pixels glError=0x%x",
            static_cast<unsigned long long>(sequence),
            static_cast<unsigned long long>(frameIndex),
            sampleIndex,
            readError);
        return false;
    }

    uint64_t hash = 1469598103934665603ull;
    uint64_t alphaNonzero = 0;
    uint64_t alphaPartial = 0;
    uint8_t alphaMin = 255;
    uint8_t alphaMax = 0;
    for (size_t index = 0; index < pixels.size(); ++index) {
        hash ^= pixels[index];
        hash *= 1099511628211ull;
        if ((index & 3u) == 3u) {
            const uint8_t alpha = pixels[index];
            alphaMin = std::min(alphaMin, alpha);
            alphaMax = std::max(alphaMax, alpha);
            alphaNonzero += alpha != 0 ? 1u : 0u;
            alphaPartial += alpha != 0 && alpha != 255 ? 1u : 0u;
        }
    }

    const std::filesystem::path directory = LogPath().parent_path() / "terminal-captures";
    std::error_code ec;
    std::filesystem::create_directories(directory, ec);
    const std::string stem = std::string(targetName != nullptr ? targetName : "terminal_capture")
        + "_seq" + std::to_string(sequence)
        + "_frame" + std::to_string(frameIndex)
        + "_sample" + std::to_string(sampleIndex);
    const std::filesystem::path rgbPath = directory / (stem + "_rgb.bmp");
    const std::filesystem::path alphaPath = directory / (stem + "_alpha.bmp");
    const bool rgbWritten = WriteRgbBmp(rgbPath, width, height, pixels, false);
    const bool alphaWritten = WriteRgbBmp(alphaPath, width, height, pixels, true);
    Logger::Instance().Write(
        rgbWritten && alphaWritten ? LogLevel::Warn : LogLevel::Error,
        "terminal_hud_dump target=%s sequence=%llu frame=%llu captureFrame=%llu sample=%u reason=%s size=%dx%d fbo=%u hash=0x%016llx alpha={min=%u max=%u nonzero=%llu partial=%llu total=%llu} rgbWritten=%d alphaWritten=%d rgbPath=\"%s\" alphaPath=\"%s\"",
        targetName != nullptr ? targetName : "terminal_capture",
        static_cast<unsigned long long>(sequence),
        static_cast<unsigned long long>(frameIndex),
        static_cast<unsigned long long>(captureFrame),
        sampleIndex,
        reason != nullptr ? reason : "unspecified",
        width,
        height,
        framebuffer,
        static_cast<unsigned long long>(hash),
        static_cast<unsigned>(alphaMin),
        static_cast<unsigned>(alphaMax),
        static_cast<unsigned long long>(alphaNonzero),
        static_cast<unsigned long long>(alphaPartial),
        static_cast<unsigned long long>(pixelCount),
        rgbWritten ? 1 : 0,
        alphaWritten ? 1 : 0,
        rgbPath.string().c_str(),
        alphaPath.string().c_str());
    return rgbWritten && alphaWritten;
}

void OpenXRGLBridge::InvalidateHudCapture()
{
    hud_.captureValid = false;
    hud_.captureFrame = 0;
}

bool OpenXRGLBridge::HudReady() const
{
    return session_ != XR_NULL_HANDLE
        && hud_.handle != XR_NULL_HANDLE
        && hud_.captureTexture != 0
        && hud_.captureFramebuffer != 0
        && !hud_.framebuffers.empty();
}

bool OpenXRGLBridge::HudCaptureFresh(uint64_t frameIndex, uint64_t maxAgeFrames) const
{
    return HudReady()
        && hud_.captureValid
        && frameIndex >= hud_.captureFrame
        && frameIndex - hud_.captureFrame <= maxAgeFrames;
}

const OpenXRGLBridge::HudSwapchain& OpenXRGLBridge::Hud() const
{
    return hud_;
}

bool OpenXRGLBridge::DrawInteractionReticleToSwapchain(
    int crosshairState,
    float red,
    float green,
    float blue,
    float alpha)
{
    ScopedOwnOpenGLWork ownGl;
    if (!InteractionReticleReady()) {
        return false;
    }

    uint32_t imageIndex = interactionReticle_.acquiredImageIndex;
    XrResult result = XR_SUCCESS;
    if (!interactionReticle_.imageAcquired) {
        XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
        result = xrAcquireSwapchainImage(
            interactionReticle_.handle, &acquireInfo, &imageIndex);
        if (XR_FAILED(result)) {
            Logger::Instance().Write(
                LogLevel::Warn,
                "openxr_interaction_reticle acquire_failed result=%d",
                static_cast<int>(result));
            return false;
        }
        interactionReticle_.imageAcquired = true;
        interactionReticle_.acquiredImageIndex = imageIndex;
    }
    XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
    waitInfo.timeout = kSwapchainWaitTimeout;
    result = xrWaitSwapchainImage(interactionReticle_.handle, &waitInfo);
    if (XR_FAILED(result)) {
        Logger::Instance().Write(
            LogLevel::Warn,
            "openxr_interaction_reticle wait_failed image=%u result=%d",
            imageIndex,
            static_cast<int>(result));
        return false;
    }

    const bool drawn = DrawInteractionReticleToImage(
        imageIndex,
        crosshairState,
        red,
        green,
        blue,
        alpha);
    glFlush();
    XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
    result = xrReleaseSwapchainImage(interactionReticle_.handle, &releaseInfo);
    if (XR_FAILED(result)) {
        Logger::Instance().Write(
            LogLevel::Warn,
            "openxr_interaction_reticle release_failed image=%u result=%d",
            imageIndex,
            static_cast<int>(result));
        return false;
    }
    interactionReticle_.imageAcquired = false;
    return drawn;
}

bool OpenXRGLBridge::InteractionReticleReady() const
{
    return session_ != XR_NULL_HANDLE
        && interactionReticle_.handle != XR_NULL_HANDLE
        && !interactionReticle_.images.empty()
        && !interactionReticle_.framebuffers.empty();
}

const OpenXRGLBridge::ReticleSwapchain& OpenXRGLBridge::InteractionReticle() const
{
    return interactionReticle_;
}

bool OpenXRGLBridge::DrawControllerAimGuideToSwapchain(
    float red,
    float green,
    float blue,
    float alpha)
{
    ScopedOwnOpenGLWork ownGl;
    if (!ControllerAimGuideReady()) return false;
    std::swap(interactionReticle_, controllerAimGuide_);
    const bool drawn = DrawInteractionReticleToSwapchain(
        -1, red, green, blue, alpha);
    std::swap(interactionReticle_, controllerAimGuide_);
    return drawn;
}

bool OpenXRGLBridge::ControllerAimGuideReady() const
{
    return session_ != XR_NULL_HANDLE
        && controllerAimGuide_.handle != XR_NULL_HANDLE
        && !controllerAimGuide_.images.empty()
        && !controllerAimGuide_.framebuffers.empty();
}

const OpenXRGLBridge::ReticleSwapchain& OpenXRGLBridge::ControllerAimGuide() const
{
    return controllerAimGuide_;
}

bool OpenXRGLBridge::DrawStatusPanelToSwapchain(const std::vector<uint8_t>& rgbaPixels)
{
    ScopedOwnOpenGLWork ownGl;
    if (!StatusPanelReady()
        || rgbaPixels.size() != static_cast<size_t>(statusPanel_.width) * statusPanel_.height * 4) {
        return false;
    }

    uint32_t imageIndex = statusPanel_.acquiredImageIndex;
    XrResult result = XR_SUCCESS;
    if (!statusPanel_.imageAcquired) {
        XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
        result = xrAcquireSwapchainImage(statusPanel_.handle, &acquireInfo, &imageIndex);
        if (XR_FAILED(result)) {
            Logger::Instance().Write(LogLevel::Warn,
                "openxr_status_panel acquire_failed result=%d", static_cast<int>(result));
            return false;
        }
        statusPanel_.imageAcquired = true;
        statusPanel_.acquiredImageIndex = imageIndex;
    }
    XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
    waitInfo.timeout = kSwapchainWaitTimeout;
    result = xrWaitSwapchainImage(statusPanel_.handle, &waitInfo);
    if (XR_FAILED(result)) {
        Logger::Instance().Write(LogLevel::Warn,
            "openxr_status_panel wait_failed image=%u result=%d", imageIndex, static_cast<int>(result));
        return false;
    }

    int32_t savedTexture = 0;
    int32_t savedUnpackAlignment = 0;
    glGetIntegerv(kGlTextureBinding2D, &savedTexture);
    glGetIntegerv(kGlUnpackAlignment, &savedUnpackAlignment);
    glBindTexture(kGlTexture2D, statusPanel_.images[imageIndex].image);
    glPixelStorei(kGlUnpackAlignment, 1);
    glTexSubImage2D(
        kGlTexture2D,
        0,
        0,
        0,
        statusPanel_.width,
        statusPanel_.height,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        rgbaPixels.data());
    glPixelStorei(kGlUnpackAlignment, savedUnpackAlignment);
    glBindTexture(kGlTexture2D, static_cast<uint32_t>(savedTexture));
    glFlush();

    XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
    result = xrReleaseSwapchainImage(statusPanel_.handle, &releaseInfo);
    if (XR_FAILED(result)) {
        Logger::Instance().Write(LogLevel::Warn,
            "openxr_status_panel release_failed image=%u result=%d", imageIndex, static_cast<int>(result));
        return false;
    }
    statusPanel_.imageAcquired = false;
    return true;
}

bool OpenXRGLBridge::StatusPanelReady() const
{
    return session_ != XR_NULL_HANDLE
        && statusPanel_.handle != XR_NULL_HANDLE
        && !statusPanel_.images.empty();
}

const OpenXRGLBridge::StatusPanelSwapchain& OpenXRGLBridge::StatusPanel() const
{
    return statusPanel_;
}

bool OpenXRGLBridge::DrawComfortVignetteToSwapchain(const std::vector<uint8_t>& rgbaPixels)
{
    ScopedOwnOpenGLWork ownGl;
    if (!ComfortVignetteReady()
        || rgbaPixels.size()
            != static_cast<size_t>(comfortVignette_.width) * comfortVignette_.height * 4) {
        return false;
    }

    uint32_t imageIndex = comfortVignette_.acquiredImageIndex;
    XrResult result = XR_SUCCESS;
    if (!comfortVignette_.imageAcquired) {
        XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
        result = xrAcquireSwapchainImage(
            comfortVignette_.handle, &acquireInfo, &imageIndex);
        if (XR_FAILED(result)) {
            Logger::Instance().Write(LogLevel::Warn,
                "openxr_comfort_vignette acquire_failed result=%d", static_cast<int>(result));
            return false;
        }
        comfortVignette_.imageAcquired = true;
        comfortVignette_.acquiredImageIndex = imageIndex;
    }
    XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
    waitInfo.timeout = kSwapchainWaitTimeout;
    result = xrWaitSwapchainImage(comfortVignette_.handle, &waitInfo);
    if (XR_FAILED(result)) {
        Logger::Instance().Write(LogLevel::Warn,
            "openxr_comfort_vignette wait_failed image=%u result=%d",
            imageIndex, static_cast<int>(result));
        return false;
    }

    int32_t savedTexture = 0;
    int32_t savedUnpackAlignment = 0;
    glGetIntegerv(kGlTextureBinding2D, &savedTexture);
    glGetIntegerv(kGlUnpackAlignment, &savedUnpackAlignment);
    glBindTexture(kGlTexture2D, comfortVignette_.images[imageIndex].image);
    glPixelStorei(kGlUnpackAlignment, 1);
    glTexSubImage2D(
        kGlTexture2D,
        0,
        0,
        0,
        comfortVignette_.width,
        comfortVignette_.height,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        rgbaPixels.data());
    glPixelStorei(kGlUnpackAlignment, savedUnpackAlignment);
    glBindTexture(kGlTexture2D, static_cast<uint32_t>(savedTexture));
    glFlush();

    XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
    result = xrReleaseSwapchainImage(comfortVignette_.handle, &releaseInfo);
    if (XR_FAILED(result)) {
        Logger::Instance().Write(LogLevel::Warn,
            "openxr_comfort_vignette release_failed image=%u result=%d",
            imageIndex, static_cast<int>(result));
        return false;
    }
    comfortVignette_.imageAcquired = false;
    return true;
}

bool OpenXRGLBridge::ComfortVignetteReady() const
{
    return session_ != XR_NULL_HANDLE
        && comfortVignette_.handle != XR_NULL_HANDLE
        && !comfortVignette_.images.empty();
}

const OpenXRGLBridge::ComfortVignetteSwapchain& OpenXRGLBridge::ComfortVignette() const
{
    return comfortVignette_;
}

void OpenXRGLBridge::InvalidateStereoCaches()
{
    for (EyeSwapchain& eye : eyes_) {
        eye.cacheValid = false;
        eye.depthCacheValid = false;
    }
}

bool OpenXRGLBridge::StereoCachesReady() const
{
    return Ready()
        && eyes_[0].cacheValid
        && eyes_[1].cacheValid;
}

bool OpenXRGLBridge::DepthCachesReady() const
{
    return depthCaptureProbeEnabled_
        && eyes_.size() >= 2
        && std::all_of(eyes_.begin(), eyes_.end(), [](const EyeSwapchain& eye) {
            return eye.depthCacheTexture != 0 && eye.depthCacheValid;
        });
}

bool OpenXRGLBridge::DepthSwapchainsReady() const
{
    return depthCompositionSubmitEnabled_
        && eyes_.size() >= 2
        && std::all_of(eyes_.begin(), eyes_.end(), [](const EyeSwapchain& eye) {
            return eye.depthHandle != XR_NULL_HANDLE
                && !eye.depthImages.empty()
                && eye.depthImages.size() == eye.depthFramebuffers.size();
        });
}

bool OpenXRGLBridge::Ready() const
{
    return session_ != XR_NULL_HANDLE && eyes_.size() == 2;
}

uint32_t OpenXRGLBridge::EyeCount() const
{
    return static_cast<uint32_t>(eyes_.size());
}

const OpenXRGLBridge::EyeSwapchain& OpenXRGLBridge::Eye(uint32_t eyeIndex) const
{
    return eyes_.at(eyeIndex);
}

const OpenXRGLBridge::SwapchainTransferTiming&
OpenXRGLBridge::ColorTransferTiming(uint32_t eyeIndex) const
{
    static const SwapchainTransferTiming empty;
    return eyeIndex < eyes_.size() ? eyes_[eyeIndex].colorTransferTiming : empty;
}

const char* OpenXRGLBridge::ColorTransferSourceName(ColorTransferSource source)
{
    switch (source) {
    case ColorTransferSource::Backbuffer: return "backbuffer";
    case ColorTransferSource::StereoCache: return "stereo_cache";
    case ColorTransferSource::Black: return "black";
    default: return "none";
    }
}

int64_t OpenXRGLBridge::ColorFormat() const
{
    return colorFormat_;
}

void OpenXRGLBridge::PollGpuTiming(EyeSwapchain& eye)
{
    if (!gpuTimingAvailable_) {
        return;
    }
    for (GpuTimestampSlot& slot : eye.gpuTimestampSlots) {
        if (!slot.pending || slot.endQuery == 0) {
            continue;
        }
        int32_t available = 0;
        glGetQueryObjectiv_(slot.endQuery, kGlQueryResultAvailable, &available);
        if (available == 0) {
            continue;
        }
        uint64_t startTimestamp = 0;
        uint64_t endTimestamp = 0;
        glGetQueryObjectui64v_(slot.startQuery, kGlQueryResult, &startTimestamp);
        glGetQueryObjectui64v_(slot.endQuery, kGlQueryResult, &endTimestamp);
        slot.pending = false;
        if (endTimestamp < startTimestamp) {
            ++eye.colorTransferTiming.gpuInvalidSamples;
            continue;
        }
        const uint64_t elapsedUs = (endTimestamp - startTimestamp) / 1'000;
        if (slot.phase == GpuTransferPhase::Capture) {
            AccumulateTransferPhase(eye.colorTransferTiming.gpuCapture, elapsedUs);
            ++eye.colorTransferTiming.gpuCaptureSamples;
        } else {
            AccumulateTransferPhase(eye.colorTransferTiming.gpuSubmit, elapsedUs);
            ++eye.colorTransferTiming.gpuSubmitSamples;
        }
    }
}

int OpenXRGLBridge::BeginGpuTiming(EyeSwapchain& eye, GpuTransferPhase phase)
{
    if (!gpuTimingAvailable_) {
        return -1;
    }
    PollGpuTiming(eye);
    const uint32_t slotIndex = eye.nextGpuTimestampSlot
        % static_cast<uint32_t>(eye.gpuTimestampSlots.size());
    eye.nextGpuTimestampSlot = (slotIndex + 1)
        % static_cast<uint32_t>(eye.gpuTimestampSlots.size());
    GpuTimestampSlot& slot = eye.gpuTimestampSlots[slotIndex];
    if (slot.pending) {
        ++eye.colorTransferTiming.gpuQueryDrops;
        return -1;
    }
    if (slot.startQuery == 0 || slot.endQuery == 0) {
        return -1;
    }
    slot.phase = phase;
    glQueryCounter_(slot.startQuery, kGlTimestamp);
    return static_cast<int>(slotIndex);
}

void OpenXRGLBridge::EndGpuTiming(EyeSwapchain& eye, int slotIndex)
{
    if (!gpuTimingAvailable_
        || slotIndex < 0
        || static_cast<size_t>(slotIndex) >= eye.gpuTimestampSlots.size()) {
        return;
    }
    GpuTimestampSlot& slot = eye.gpuTimestampSlots[static_cast<size_t>(slotIndex)];
    glQueryCounter_(slot.endQuery, kGlTimestamp);
    slot.pending = true;
}

bool OpenXRGLBridge::ResolveFunctions()
{
    glGenFramebuffers_ = ResolveGlProc<GlGenFramebuffersFn>("glGenFramebuffers");
    glDeleteFramebuffers_ = ResolveGlProc<GlDeleteFramebuffersFn>("glDeleteFramebuffers");
    glBindFramebuffer_ = ResolveGlProc<GlBindFramebufferFn>("glBindFramebuffer");
    glFramebufferTexture2D_ = ResolveGlProc<GlFramebufferTexture2DFn>("glFramebufferTexture2D");
    glCheckFramebufferStatus_ = ResolveGlProc<GlCheckFramebufferStatusFn>("glCheckFramebufferStatus");
    glBlitFramebuffer_ = ResolveGlProc<GlBlitFramebufferFn>("glBlitFramebuffer");
    glGenQueries_ = ResolveGlProc<GlGenQueriesFn>("glGenQueries");
    glDeleteQueries_ = ResolveGlProc<GlDeleteQueriesFn>("glDeleteQueries");
    glQueryCounter_ = ResolveGlProc<GlQueryCounterFn>("glQueryCounter");
    glGetQueryObjectiv_ = ResolveGlProc<GlGetQueryObjectivFn>("glGetQueryObjectiv");
    glGetQueryObjectui64v_ = ResolveGlProc<GlGetQueryObjectui64vFn>("glGetQueryObjectui64v");
    gpuTimingAvailable_ = glGenQueries_ != nullptr
        && glDeleteQueries_ != nullptr
        && glQueryCounter_ != nullptr
        && glGetQueryObjectiv_ != nullptr
        && glGetQueryObjectui64v_ != nullptr;

    const bool ready = glGenFramebuffers_ != nullptr
        && glDeleteFramebuffers_ != nullptr
        && glBindFramebuffer_ != nullptr
        && glFramebufferTexture2D_ != nullptr
        && glCheckFramebufferStatus_ != nullptr
        && glBlitFramebuffer_ != nullptr;
    Logger::Instance().Write(
        ready ? LogLevel::Info : LogLevel::Warn,
        "openxr_gl_functions ready=%d genFbo=%d deleteFbo=%d bindFbo=%d attachTexture=%d checkFbo=%d blitFbo=%d gpuTimestampTiming=%d",
        ready ? 1 : 0,
        glGenFramebuffers_ != nullptr ? 1 : 0,
        glDeleteFramebuffers_ != nullptr ? 1 : 0,
        glBindFramebuffer_ != nullptr ? 1 : 0,
        glFramebufferTexture2D_ != nullptr ? 1 : 0,
        glCheckFramebufferStatus_ != nullptr ? 1 : 0,
        glBlitFramebuffer_ != nullptr ? 1 : 0,
        gpuTimingAvailable_ ? 1 : 0);
    return ready;
}

bool OpenXRGLBridge::CreateEyeSwapchain(
    XrSession session,
    const XrViewConfigurationView& view,
    uint32_t eyeIndex,
    int resolutionScalePercent)
{
    EyeSwapchain eye;
    eye.width = std::max(1, static_cast<int32_t>(std::lround(
        static_cast<double>(view.recommendedImageRectWidth) * resolutionScalePercent / 100.0)));
    eye.height = std::max(1, static_cast<int32_t>(std::lround(
        static_cast<double>(view.recommendedImageRectHeight) * resolutionScalePercent / 100.0)));
    eye.format = colorFormat_;

    XrSwapchainCreateInfo createInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO};
    XrSwapchainCreateInfoFoveationFB foveationInfo{
        XR_TYPE_SWAPCHAIN_CREATE_INFO_FOVEATION_FB};
    if (foveationSwapchainEnabled_) {
        createInfo.next = &foveationInfo;
    }
    createInfo.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
    createInfo.format = colorFormat_;
    createInfo.sampleCount = 1;
    createInfo.width = static_cast<uint32_t>(eye.width);
    createInfo.height = static_cast<uint32_t>(eye.height);
    createInfo.faceCount = 1;
    createInfo.arraySize = 1;
    createInfo.mipCount = 1;

    XrResult result = xrCreateSwapchain(session, &createInfo, &eye.handle);
    if (XR_FAILED(result)) {
        Logger::Instance().Write(
            LogLevel::Warn,
            "openxr_swapchain create_failed eye=%u size=%dx%d format=0x%llx result=%d",
            eyeIndex,
            eye.width,
            eye.height,
            static_cast<unsigned long long>(colorFormat_),
            static_cast<int>(result));
        return false;
    }

    uint32_t imageCount = 0;
    result = xrEnumerateSwapchainImages(eye.handle, 0, &imageCount, nullptr);
    if (XR_FAILED(result) || imageCount == 0) {
        Logger::Instance().Write(
            LogLevel::Warn,
            "openxr_swapchain image_count_failed eye=%u result=%d count=%u",
            eyeIndex,
            static_cast<int>(result),
            imageCount);
        xrDestroySwapchain(eye.handle);
        return false;
    }

    eye.images.resize(imageCount);
    for (XrSwapchainImageOpenGLKHR& image : eye.images) {
        image.type = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR;
    }
    result = xrEnumerateSwapchainImages(
        eye.handle,
        imageCount,
        &imageCount,
        reinterpret_cast<XrSwapchainImageBaseHeader*>(eye.images.data()));
    if (XR_FAILED(result)) {
        Logger::Instance().Write(
            LogLevel::Warn,
            "openxr_swapchain images_failed eye=%u result=%d",
            eyeIndex,
            static_cast<int>(result));
        xrDestroySwapchain(eye.handle);
        return false;
    }
    eye.images.resize(imageCount);

    int32_t savedReadFramebuffer = 0;
    int32_t savedDrawFramebuffer = 0;
    int32_t savedReadBuffer = 0;
    int32_t savedDrawBuffer = 0;
    glGetIntegerv(kGlReadFramebufferBinding, &savedReadFramebuffer);
    glGetIntegerv(kGlDrawFramebufferBinding, &savedDrawFramebuffer);
    glGetIntegerv(kGlReadBuffer, &savedReadBuffer);
    glGetIntegerv(kGlDrawBuffer, &savedDrawBuffer);

    eye.framebuffers.resize(imageCount);
    glGenFramebuffers_(static_cast<int32_t>(imageCount), eye.framebuffers.data());
    bool complete = true;
    for (uint32_t imageIndex = 0; imageIndex < imageCount; ++imageIndex) {
        glBindFramebuffer_(kGlFramebuffer, eye.framebuffers[imageIndex]);
        glFramebufferTexture2D_(
            kGlFramebuffer,
            kGlColorAttachment0,
            kGlTexture2D,
            eye.images[imageIndex].image,
            0);
        glDrawBuffer(kGlColorAttachment0);
        const uint32_t status = glCheckFramebufferStatus_(kGlFramebuffer);
        if (status != kGlFramebufferComplete) {
            Logger::Instance().Write(
                LogLevel::Warn,
                "openxr_swapchain framebuffer_incomplete eye=%u image=%u status=0x%x",
                eyeIndex,
                imageIndex,
                status);
            complete = false;
            break;
        }
    }

    glBindFramebuffer_(kGlReadFramebuffer, static_cast<uint32_t>(savedReadFramebuffer));
    glReadBuffer(static_cast<uint32_t>(savedReadBuffer));
    glBindFramebuffer_(kGlDrawFramebuffer, static_cast<uint32_t>(savedDrawFramebuffer));
    glDrawBuffer(static_cast<uint32_t>(savedDrawBuffer));
    if (!complete) {
        glDeleteFramebuffers_(static_cast<int32_t>(eye.framebuffers.size()), eye.framebuffers.data());
        xrDestroySwapchain(eye.handle);
        return false;
    }

    if (!CreateEyeCache(eye, eyeIndex)) {
        glDeleteFramebuffers_(static_cast<int32_t>(eye.framebuffers.size()), eye.framebuffers.data());
        xrDestroySwapchain(eye.handle);
        return false;
    }
    if (depthCompositionSubmitEnabled_ && !CreateDepthSwapchain(eye, eyeIndex)) {
        Logger::Instance().Write(
            LogLevel::Warn,
            "openxr_depth_submission eye_disabled eye=%u reason=swapchain_creation_failed fallback=color_only",
            eyeIndex);
    }

    if (gpuTimingAvailable_) {
        std::array<uint32_t, 16> queries{};
        glGenQueries_(static_cast<int32_t>(queries.size()), queries.data());
        const bool allocated = std::all_of(
            queries.begin(), queries.end(), [](uint32_t query) { return query != 0; });
        if (allocated) {
            eye.colorTransferTiming.gpuTimingAvailable = true;
            for (size_t slotIndex = 0; slotIndex < eye.gpuTimestampSlots.size(); ++slotIndex) {
                eye.gpuTimestampSlots[slotIndex].startQuery = queries[slotIndex * 2];
                eye.gpuTimestampSlots[slotIndex].endQuery = queries[slotIndex * 2 + 1];
            }
        } else {
            glDeleteQueries_(static_cast<int32_t>(queries.size()), queries.data());
            Logger::Instance().Write(
                LogLevel::Warn,
                "openxr_gl_gpu_timing allocation_failed eye=%u queries=%zu fallback=cpu_timing",
                eyeIndex,
                queries.size());
        }
    }

    Logger::Instance().Write(
        LogLevel::Info,
        "openxr_swapchain created eye=%u size=%dx%d images=%u format=0x%llx(%s) depthSwapchain=%d depthFormat=0x%llx(%s) gpuTiming=%d",
        eyeIndex,
        eye.width,
        eye.height,
        imageCount,
        static_cast<unsigned long long>(colorFormat_),
        GlFormatName(colorFormat_),
        eye.depthHandle != XR_NULL_HANDLE ? 1 : 0,
        static_cast<unsigned long long>(eye.depthFormat),
        GlFormatName(eye.depthFormat),
        eye.gpuTimestampSlots[0].startQuery != 0 ? 1 : 0);
    eyes_.push_back(std::move(eye));
    return true;
}

bool OpenXRGLBridge::CreateDepthSwapchain(EyeSwapchain& eye, uint32_t eyeIndex)
{
    if (session_ == XR_NULL_HANDLE || depthFormat_ == 0) {
        return false;
    }

    eye.depthFormat = depthFormat_;
    XrSwapchainCreateInfo createInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO};
    createInfo.usageFlags = XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    createInfo.format = depthFormat_;
    createInfo.sampleCount = 1;
    createInfo.width = static_cast<uint32_t>(eye.width);
    createInfo.height = static_cast<uint32_t>(eye.height);
    createInfo.faceCount = 1;
    createInfo.arraySize = 1;
    createInfo.mipCount = 1;

    XrResult result = xrCreateSwapchain(session_, &createInfo, &eye.depthHandle);
    if (XR_FAILED(result)) {
        Logger::Instance().Write(
            LogLevel::Warn,
            "openxr_depth_swapchain create_failed eye=%u size=%dx%d format=0x%llx(%s) result=%d",
            eyeIndex,
            eye.width,
            eye.height,
            static_cast<unsigned long long>(depthFormat_),
            GlFormatName(depthFormat_),
            static_cast<int>(result));
        eye.depthHandle = XR_NULL_HANDLE;
        return false;
    }

    uint32_t imageCount = 0;
    result = xrEnumerateSwapchainImages(eye.depthHandle, 0, &imageCount, nullptr);
    if (XR_FAILED(result) || imageCount == 0) {
        Logger::Instance().Write(
            LogLevel::Warn,
            "openxr_depth_swapchain image_count_failed eye=%u result=%d count=%u",
            eyeIndex,
            static_cast<int>(result),
            imageCount);
        xrDestroySwapchain(eye.depthHandle);
        eye.depthHandle = XR_NULL_HANDLE;
        return false;
    }

    eye.depthImages.resize(imageCount);
    for (XrSwapchainImageOpenGLKHR& image : eye.depthImages) {
        image.type = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR;
    }
    result = xrEnumerateSwapchainImages(
        eye.depthHandle,
        imageCount,
        &imageCount,
        reinterpret_cast<XrSwapchainImageBaseHeader*>(eye.depthImages.data()));
    if (XR_FAILED(result)) {
        Logger::Instance().Write(
            LogLevel::Warn,
            "openxr_depth_swapchain images_failed eye=%u result=%d",
            eyeIndex,
            static_cast<int>(result));
        xrDestroySwapchain(eye.depthHandle);
        eye.depthHandle = XR_NULL_HANDLE;
        eye.depthImages.clear();
        return false;
    }
    eye.depthImages.resize(imageCount);

    int32_t savedReadFramebuffer = 0;
    int32_t savedDrawFramebuffer = 0;
    int32_t savedReadBuffer = 0;
    int32_t savedDrawBuffer = 0;
    glGetIntegerv(kGlReadFramebufferBinding, &savedReadFramebuffer);
    glGetIntegerv(kGlDrawFramebufferBinding, &savedDrawFramebuffer);
    glGetIntegerv(kGlReadBuffer, &savedReadBuffer);
    glGetIntegerv(kGlDrawBuffer, &savedDrawBuffer);

    eye.depthFramebuffers.resize(imageCount);
    glGenFramebuffers_(static_cast<int32_t>(imageCount), eye.depthFramebuffers.data());
    bool complete = true;
    for (uint32_t imageIndex = 0; imageIndex < imageCount; ++imageIndex) {
        glBindFramebuffer_(kGlFramebuffer, eye.depthFramebuffers[imageIndex]);
        const uint32_t depthAttachment = depthFormat_ == kGlDepth24Stencil8
                || depthFormat_ == kGlDepth32fStencil8
            ? kGlDepthStencilAttachment
            : kGlDepthAttachment;
        glFramebufferTexture2D_(
            kGlFramebuffer,
            depthAttachment,
            kGlTexture2D,
            eye.depthImages[imageIndex].image,
            0);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
        const uint32_t status = glCheckFramebufferStatus_(kGlFramebuffer);
        if (status != kGlFramebufferComplete) {
            Logger::Instance().Write(
                LogLevel::Warn,
                "openxr_depth_swapchain framebuffer_incomplete eye=%u image=%u status=0x%x",
                eyeIndex,
                imageIndex,
                status);
            complete = false;
            break;
        }
    }

    glBindFramebuffer_(kGlReadFramebuffer, static_cast<uint32_t>(savedReadFramebuffer));
    glReadBuffer(static_cast<uint32_t>(savedReadBuffer));
    glBindFramebuffer_(kGlDrawFramebuffer, static_cast<uint32_t>(savedDrawFramebuffer));
    glDrawBuffer(static_cast<uint32_t>(savedDrawBuffer));
    if (!complete) {
        glDeleteFramebuffers_(
            static_cast<int32_t>(eye.depthFramebuffers.size()),
            eye.depthFramebuffers.data());
        eye.depthFramebuffers.clear();
        eye.depthImages.clear();
        xrDestroySwapchain(eye.depthHandle);
        eye.depthHandle = XR_NULL_HANDLE;
        return false;
    }

    Logger::Instance().Write(
        LogLevel::Info,
        "openxr_depth_swapchain created eye=%u size=%dx%d images=%u format=0x%llx(%s)",
        eyeIndex,
        eye.width,
        eye.height,
        imageCount,
        static_cast<unsigned long long>(depthFormat_),
        GlFormatName(depthFormat_));
    return true;
}

bool OpenXRGLBridge::CreateHudSwapchain(XrSession session, int width, int height)
{
    hud_ = {};
    hud_.width = std::clamp(width, 256, 4096);
    hud_.height = std::clamp(height, 256, 4096);
    hud_.format = colorFormat_;

    XrSwapchainCreateInfo createInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO};
    createInfo.createFlags = 0;
    createInfo.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
    createInfo.format = colorFormat_;
    createInfo.sampleCount = 1;
    createInfo.width = hud_.width;
    createInfo.height = hud_.height;
    createInfo.faceCount = 1;
    createInfo.arraySize = 1;
    createInfo.mipCount = 1;
    XrResult result = xrCreateSwapchain(session, &createInfo, &hud_.handle);
    if (XR_FAILED(result)) {
        Logger::Instance().Write(
            LogLevel::Warn,
            "openxr_hud create_failed size=%dx%d result=%d",
            hud_.width,
            hud_.height,
            static_cast<int>(result));
        hud_ = {};
        return false;
    }

    uint32_t imageCount = 0;
    result = xrEnumerateSwapchainImages(hud_.handle, 0, &imageCount, nullptr);
    if (XR_FAILED(result) || imageCount == 0) {
        xrDestroySwapchain(hud_.handle);
        hud_ = {};
        return false;
    }
    hud_.images.resize(imageCount);
    for (XrSwapchainImageOpenGLKHR& image : hud_.images) {
        image.type = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR;
        image.next = nullptr;
    }
    result = xrEnumerateSwapchainImages(
        hud_.handle,
        imageCount,
        &imageCount,
        reinterpret_cast<XrSwapchainImageBaseHeader*>(hud_.images.data()));
    if (XR_FAILED(result)) {
        xrDestroySwapchain(hud_.handle);
        hud_ = {};
        return false;
    }
    hud_.images.resize(imageCount);

    int32_t savedReadFramebuffer = 0;
    int32_t savedDrawFramebuffer = 0;
    int32_t savedReadBuffer = 0;
    int32_t savedDrawBuffer = 0;
    glGetIntegerv(kGlReadFramebufferBinding, &savedReadFramebuffer);
    glGetIntegerv(kGlDrawFramebufferBinding, &savedDrawFramebuffer);
    glGetIntegerv(kGlReadBuffer, &savedReadBuffer);
    glGetIntegerv(kGlDrawBuffer, &savedDrawBuffer);
    hud_.framebuffers.resize(imageCount);
    glGenFramebuffers_(static_cast<int32_t>(imageCount), hud_.framebuffers.data());
    bool complete = true;
    for (uint32_t imageIndex = 0; imageIndex < imageCount; ++imageIndex) {
        glBindFramebuffer_(kGlFramebuffer, hud_.framebuffers[imageIndex]);
        glFramebufferTexture2D_(
            kGlFramebuffer,
            kGlColorAttachment0,
            kGlTexture2D,
            hud_.images[imageIndex].image,
            0);
        glDrawBuffer(kGlColorAttachment0);
        if (glCheckFramebufferStatus_(kGlFramebuffer) != kGlFramebufferComplete) {
            complete = false;
            break;
        }
    }
    glBindFramebuffer_(kGlReadFramebuffer, static_cast<uint32_t>(savedReadFramebuffer));
    glReadBuffer(static_cast<uint32_t>(savedReadBuffer));
    glBindFramebuffer_(kGlDrawFramebuffer, static_cast<uint32_t>(savedDrawFramebuffer));
    glDrawBuffer(static_cast<uint32_t>(savedDrawBuffer));
    if (!complete || !CreateHudCaptureTarget()) {
        if (!hud_.framebuffers.empty()) {
            glDeleteFramebuffers_(static_cast<int32_t>(hud_.framebuffers.size()), hud_.framebuffers.data());
        }
        xrDestroySwapchain(hud_.handle);
        hud_ = {};
        return false;
    }

    Logger::Instance().Write(
        LogLevel::Info,
        "openxr_hud swapchain_created size=%dx%d images=%u format=0x%llx(%s) captureTexture=%u captureFbo=%u",
        hud_.width,
        hud_.height,
        imageCount,
        static_cast<unsigned long long>(hud_.format),
        GlFormatName(hud_.format),
        hud_.captureTexture,
        hud_.captureFramebuffer);
    return true;
}

bool OpenXRGLBridge::CreateInteractionReticleSwapchain(XrSession session, int sizePixels)
{
    interactionReticle_ = {};
    interactionReticle_.width = std::clamp(sizePixels, 32, 512);
    interactionReticle_.height = interactionReticle_.width;
    interactionReticle_.format = colorFormat_;

    XrSwapchainCreateInfo createInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO};
    createInfo.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
    createInfo.format = colorFormat_;
    createInfo.sampleCount = 1;
    createInfo.width = interactionReticle_.width;
    createInfo.height = interactionReticle_.height;
    createInfo.faceCount = 1;
    createInfo.arraySize = 1;
    createInfo.mipCount = 1;
    XrResult result = xrCreateSwapchain(session, &createInfo, &interactionReticle_.handle);
    if (XR_FAILED(result)) {
        Logger::Instance().Write(
            LogLevel::Warn,
            "openxr_interaction_reticle create_failed size=%d result=%d",
            interactionReticle_.width,
            static_cast<int>(result));
        interactionReticle_ = {};
        return false;
    }

    uint32_t imageCount = 0;
    result = xrEnumerateSwapchainImages(interactionReticle_.handle, 0, &imageCount, nullptr);
    if (XR_FAILED(result) || imageCount == 0) {
        xrDestroySwapchain(interactionReticle_.handle);
        interactionReticle_ = {};
        return false;
    }
    interactionReticle_.images.resize(imageCount);
    for (XrSwapchainImageOpenGLKHR& image : interactionReticle_.images) {
        image.type = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR;
        image.next = nullptr;
    }
    result = xrEnumerateSwapchainImages(
        interactionReticle_.handle,
        imageCount,
        &imageCount,
        reinterpret_cast<XrSwapchainImageBaseHeader*>(interactionReticle_.images.data()));
    if (XR_FAILED(result)) {
        xrDestroySwapchain(interactionReticle_.handle);
        interactionReticle_ = {};
        return false;
    }
    interactionReticle_.images.resize(imageCount);

    int32_t savedReadFramebuffer = 0;
    int32_t savedDrawFramebuffer = 0;
    int32_t savedReadBuffer = 0;
    int32_t savedDrawBuffer = 0;
    glGetIntegerv(kGlReadFramebufferBinding, &savedReadFramebuffer);
    glGetIntegerv(kGlDrawFramebufferBinding, &savedDrawFramebuffer);
    glGetIntegerv(kGlReadBuffer, &savedReadBuffer);
    glGetIntegerv(kGlDrawBuffer, &savedDrawBuffer);
    interactionReticle_.framebuffers.resize(imageCount);
    glGenFramebuffers_(
        static_cast<int32_t>(imageCount),
        interactionReticle_.framebuffers.data());
    bool complete = true;
    for (uint32_t imageIndex = 0; imageIndex < imageCount; ++imageIndex) {
        glBindFramebuffer_(kGlFramebuffer, interactionReticle_.framebuffers[imageIndex]);
        glFramebufferTexture2D_(
            kGlFramebuffer,
            kGlColorAttachment0,
            kGlTexture2D,
            interactionReticle_.images[imageIndex].image,
            0);
        glDrawBuffer(kGlColorAttachment0);
        if (glCheckFramebufferStatus_(kGlFramebuffer) != kGlFramebufferComplete) {
            complete = false;
            break;
        }
    }
    glBindFramebuffer_(kGlReadFramebuffer, static_cast<uint32_t>(savedReadFramebuffer));
    glReadBuffer(static_cast<uint32_t>(savedReadBuffer));
    glBindFramebuffer_(kGlDrawFramebuffer, static_cast<uint32_t>(savedDrawFramebuffer));
    glDrawBuffer(static_cast<uint32_t>(savedDrawBuffer));
    if (!complete) {
        glDeleteFramebuffers_(
            static_cast<int32_t>(interactionReticle_.framebuffers.size()),
            interactionReticle_.framebuffers.data());
        xrDestroySwapchain(interactionReticle_.handle);
        interactionReticle_ = {};
        return false;
    }

    Logger::Instance().Write(
        LogLevel::Info,
        "openxr_interaction_reticle swapchain_created size=%dx%d images=%u format=0x%llx(%s)",
        interactionReticle_.width,
        interactionReticle_.height,
        imageCount,
        static_cast<unsigned long long>(interactionReticle_.format),
        GlFormatName(interactionReticle_.format));
    return true;
}

bool OpenXRGLBridge::CreateControllerAimGuideSwapchain(XrSession session, int sizePixels)
{
    ReticleSwapchain semanticReticle = std::move(interactionReticle_);
    const bool created = CreateInteractionReticleSwapchain(session, sizePixels);
    if (created) {
        controllerAimGuide_ = std::move(interactionReticle_);
        Logger::Instance().Write(
            LogLevel::Info,
            "openxr_controller_aim_guide swapchain_created size=%dx%d images=%zu",
            controllerAimGuide_.width,
            controllerAimGuide_.height,
            controllerAimGuide_.images.size());
    }
    interactionReticle_ = std::move(semanticReticle);
    return created;
}

bool OpenXRGLBridge::CreateStatusPanelSwapchain(XrSession session, int width, int height)
{
    statusPanel_ = {};
    statusPanel_.width = std::clamp(width, 512, 4096);
    statusPanel_.height = std::clamp(height, 256, 4096);
    statusPanel_.format = colorFormat_;

    XrSwapchainCreateInfo createInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO};
    createInfo.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
    createInfo.format = colorFormat_;
    createInfo.sampleCount = 1;
    createInfo.width = statusPanel_.width;
    createInfo.height = statusPanel_.height;
    createInfo.faceCount = 1;
    createInfo.arraySize = 1;
    createInfo.mipCount = 1;
    XrResult result = xrCreateSwapchain(session, &createInfo, &statusPanel_.handle);
    if (XR_FAILED(result)) {
        statusPanel_ = {};
        return false;
    }

    uint32_t imageCount = 0;
    result = xrEnumerateSwapchainImages(statusPanel_.handle, 0, &imageCount, nullptr);
    if (XR_FAILED(result) || imageCount == 0) {
        xrDestroySwapchain(statusPanel_.handle);
        statusPanel_ = {};
        return false;
    }
    statusPanel_.images.resize(imageCount);
    for (XrSwapchainImageOpenGLKHR& image : statusPanel_.images) {
        image.type = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR;
        image.next = nullptr;
    }
    result = xrEnumerateSwapchainImages(
        statusPanel_.handle,
        imageCount,
        &imageCount,
        reinterpret_cast<XrSwapchainImageBaseHeader*>(statusPanel_.images.data()));
    if (XR_FAILED(result)) {
        xrDestroySwapchain(statusPanel_.handle);
        statusPanel_ = {};
        return false;
    }
    statusPanel_.images.resize(imageCount);
    Logger::Instance().Write(
        LogLevel::Info,
        "openxr_status_panel swapchain_created size=%dx%d images=%u format=0x%llx(%s)",
        statusPanel_.width,
        statusPanel_.height,
        imageCount,
        static_cast<unsigned long long>(statusPanel_.format),
        GlFormatName(statusPanel_.format));
    return true;
}

bool OpenXRGLBridge::CreateComfortVignetteSwapchain(XrSession session, int sizePixels)
{
    comfortVignette_ = {};
    comfortVignette_.width = std::clamp(sizePixels, 32, 1024);
    comfortVignette_.height = comfortVignette_.width;
    comfortVignette_.format = colorFormat_;

    XrSwapchainCreateInfo createInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO};
    createInfo.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
    createInfo.format = colorFormat_;
    createInfo.sampleCount = 1;
    createInfo.width = comfortVignette_.width;
    createInfo.height = comfortVignette_.height;
    createInfo.faceCount = 1;
    createInfo.arraySize = 1;
    createInfo.mipCount = 1;
    XrResult result = xrCreateSwapchain(session, &createInfo, &comfortVignette_.handle);
    if (XR_FAILED(result)) {
        comfortVignette_ = {};
        return false;
    }

    uint32_t imageCount = 0;
    result = xrEnumerateSwapchainImages(comfortVignette_.handle, 0, &imageCount, nullptr);
    if (XR_FAILED(result) || imageCount == 0) {
        xrDestroySwapchain(comfortVignette_.handle);
        comfortVignette_ = {};
        return false;
    }
    comfortVignette_.images.resize(imageCount);
    for (XrSwapchainImageOpenGLKHR& image : comfortVignette_.images) {
        image.type = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR;
        image.next = nullptr;
    }
    result = xrEnumerateSwapchainImages(
        comfortVignette_.handle,
        imageCount,
        &imageCount,
        reinterpret_cast<XrSwapchainImageBaseHeader*>(comfortVignette_.images.data()));
    if (XR_FAILED(result)) {
        xrDestroySwapchain(comfortVignette_.handle);
        comfortVignette_ = {};
        return false;
    }
    comfortVignette_.images.resize(imageCount);
    Logger::Instance().Write(
        LogLevel::Info,
        "openxr_comfort_vignette swapchain_created size=%dx%d images=%u format=0x%llx(%s)",
        comfortVignette_.width,
        comfortVignette_.height,
        imageCount,
        static_cast<unsigned long long>(comfortVignette_.format),
        GlFormatName(comfortVignette_.format));
    return true;
}

void OpenXRGLBridge::LoadInteractionReticleAssets()
{
    static constexpr std::array<const wchar_t*, 35> kAssetNames = {
        nullptr,
        L"crosshair_default.tga",
        L"crosshair_carry_one_handed.tga",
        L"crosshair_carry_two_handed.tga",
        L"crosshair_push.tga",
        L"crosshair_pull_lever.tga",
        L"crosshair_pull_lever_small.tga",
        L"crosshair_pull_sideways.tga",
        L"crosshair_pull_out.tga",
        L"crosshair_pull_door.tga",
        L"crosshair_pull_door_hatch.tga",
        L"crosshair_rotate.tga",
        L"crosshair_rotate_one_handed.tga",
        L"crosshair_push_button.tga",
        L"crosshair_pick_up.tga",
        L"crosshair_use_tool_insert.tga",
        L"crosshair_use_tool_action.tga",
        L"crosshair_terminal.tga",
        L"crosshair_datamine.tga",
        L"crosshair_climb_ladder.tga",
        L"crosshair_talk.tga",
        L"crosshair_eat.tga",
        L"crosshair_read.tga",
        L"crosshair_exit_level.tga",
        L"crosshair_climb_ledge.tga",
        L"crosshair_sit_down.tga",
        L"crosshair_default_large.tga",
        L"crosshair_default_large_clear.tga",
        L"crosshair_magnify.tga",
        L"crosshair_recharge.tga",
        L"crosshair_recharge_bad.tga",
        L"crosshair_talk_busy.tga",
        L"crosshair_pull_vertical.tga",
        L"crosshair_shock.tga",
        L"crosshair_nohints.tga",
    };

    wchar_t executablePath[32768] = {};
    const DWORD pathLength = GetModuleFileNameW(
        nullptr,
        executablePath,
        static_cast<DWORD>(std::size(executablePath)));
    if (pathLength == 0 || pathLength >= std::size(executablePath)) {
        Logger::Instance().Write(
            LogLevel::Warn,
            "openxr_interaction_reticle native_assets_failed reason=executable_path");
        return;
    }
    const std::filesystem::path assetRoot = std::filesystem::path(
        std::wstring(executablePath, pathLength)).parent_path() / L"graphics" / L"hud";
    const int canvasSize = interactionReticle_.width;
    const int drawableSize = std::max(canvasSize - std::max(canvasSize / 10, 4), 1);
    interactionReticleAssetsLoaded_ = 0;

    for (size_t state = 1; state < kAssetNames.size(); ++state) {
        std::ifstream stream(assetRoot / kAssetNames[state], std::ios::binary | std::ios::ate);
        if (!stream) continue;
        const std::streamsize fileSize = stream.tellg();
        if (fileSize < 18 || fileSize > 16 * 1024 * 1024) continue;
        stream.seekg(0, std::ios::beg);
        std::vector<uint8_t> file(static_cast<size_t>(fileSize));
        if (!stream.read(reinterpret_cast<char*>(file.data()), fileSize)) continue;

        const uint8_t idLength = file[0];
        const uint8_t colorMapType = file[1];
        const uint8_t imageType = file[2];
        const uint16_t sourceWidth = static_cast<uint16_t>(file[12] | (file[13] << 8));
        const uint16_t sourceHeight = static_cast<uint16_t>(file[14] | (file[15] << 8));
        const uint8_t bitsPerPixel = file[16];
        const size_t bytesPerPixel = bitsPerPixel / 8;
        const size_t sourceOffset = 18 + idLength;
        const size_t sourceBytes = static_cast<size_t>(sourceWidth) * sourceHeight * bytesPerPixel;
        if (colorMapType != 0
            || imageType != 2
            || (bitsPerPixel != 24 && bitsPerPixel != 32)
            || sourceWidth == 0
            || sourceHeight == 0
            || sourceOffset > file.size()
            || sourceBytes > file.size() - sourceOffset) {
            continue;
        }

        const float scale = std::min(
            static_cast<float>(drawableSize) / sourceWidth,
            static_cast<float>(drawableSize) / sourceHeight);
        const int targetWidth = std::max(1, static_cast<int>(std::lround(sourceWidth * scale)));
        const int targetHeight = std::max(1, static_cast<int>(std::lround(sourceHeight * scale)));
        const int targetX = (canvasSize - targetWidth) / 2;
        const int targetY = (canvasSize - targetHeight) / 2;
        ReticleAsset& asset = interactionReticleAssets_[state];
        asset.pixels.assign(static_cast<size_t>(canvasSize) * canvasSize * 4, 0);
        const bool topOrigin = (file[17] & 0x20) != 0;
        const bool rightOrigin = (file[17] & 0x10) != 0;
        for (int y = 0; y < targetHeight; ++y) {
            uint32_t sourceY = std::min<uint32_t>(
                static_cast<uint32_t>(y * sourceHeight / targetHeight),
                sourceHeight - 1);
            if (topOrigin) sourceY = sourceHeight - 1 - sourceY;
            for (int x = 0; x < targetWidth; ++x) {
                uint32_t sourceX = std::min<uint32_t>(
                    static_cast<uint32_t>(x * sourceWidth / targetWidth),
                    sourceWidth - 1);
                if (rightOrigin) sourceX = sourceWidth - 1 - sourceX;
                const size_t sourcePixel = sourceOffset
                    + (static_cast<size_t>(sourceY) * sourceWidth + sourceX) * bytesPerPixel;
                const size_t targetPixel = (static_cast<size_t>(targetY + y) * canvasSize
                    + static_cast<size_t>(targetX + x)) * 4;
                asset.pixels[targetPixel + 0] = file[sourcePixel + 2];
                asset.pixels[targetPixel + 1] = file[sourcePixel + 1];
                asset.pixels[targetPixel + 2] = file[sourcePixel + 0];
                asset.pixels[targetPixel + 3] = bitsPerPixel == 32 ? file[sourcePixel + 3] : 255;
            }
        }
        asset.loaded = true;
        ++interactionReticleAssetsLoaded_;
    }

    Logger::Instance().Write(
        interactionReticleAssetsLoaded_ == 34 ? LogLevel::Info : LogLevel::Warn,
        "openxr_interaction_reticle native_assets_loaded count=%u expected=34 root=%ls",
        interactionReticleAssetsLoaded_,
        assetRoot.c_str());
}

bool OpenXRGLBridge::DrawInteractionReticleToImage(
    uint32_t imageIndex,
    int crosshairState,
    float red,
    float green,
    float blue,
    float alpha)
{
    if (!InteractionReticleReady() || imageIndex >= interactionReticle_.framebuffers.size()) {
        return false;
    }

    if (crosshairState == -1) {
        const int width = interactionReticle_.width;
        const int height = interactionReticle_.height;
        interactionReticleUploadPixels_.resize(
            static_cast<size_t>(width) * static_cast<size_t>(height) * 4);
        const float colorRed = std::clamp(red, 0.0f, 1.0f);
        const float colorGreen = std::clamp(green, 0.0f, 1.0f);
        const float colorBlue = std::clamp(blue, 0.0f, 1.0f);
        const float colorAlpha = std::clamp(alpha, 0.0f, 1.0f);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const float nx = (static_cast<float>(x) + 0.5f)
                    / static_cast<float>(width) * 2.0f - 1.0f;
                const float ny = (static_cast<float>(y) + 0.5f)
                    / static_cast<float>(height) * 2.0f - 1.0f;
                const float radius = std::sqrt(nx * nx + ny * ny);
                const float glow = std::pow(
                    std::clamp(1.0f - radius, 0.0f, 1.0f), 2.2f);
                const size_t pixel = (static_cast<size_t>(y) * width + x) * 4;
                interactionReticleUploadPixels_[pixel + 0] = static_cast<uint8_t>(
                    std::lround(colorRed * 255.0f));
                interactionReticleUploadPixels_[pixel + 1] = static_cast<uint8_t>(
                    std::lround(colorGreen * 255.0f));
                interactionReticleUploadPixels_[pixel + 2] = static_cast<uint8_t>(
                    std::lround(colorBlue * 255.0f));
                interactionReticleUploadPixels_[pixel + 3] = static_cast<uint8_t>(
                    std::lround(colorAlpha * glow * 255.0f));
            }
        }

        int32_t savedTexture = 0;
        int32_t savedUnpackAlignment = 0;
        glGetIntegerv(kGlTextureBinding2D, &savedTexture);
        glGetIntegerv(kGlUnpackAlignment, &savedUnpackAlignment);
        glBindTexture(kGlTexture2D, interactionReticle_.images[imageIndex].image);
        glPixelStorei(kGlUnpackAlignment, 1);
        glTexSubImage2D(
            kGlTexture2D, 0, 0, 0, width, height,
            GL_RGBA, GL_UNSIGNED_BYTE, interactionReticleUploadPixels_.data());
        glPixelStorei(kGlUnpackAlignment, savedUnpackAlignment);
        glBindTexture(kGlTexture2D, static_cast<uint32_t>(savedTexture));
        return true;
    }

    if (interactionReticleNativeIconsEnabled_
        && crosshairState > 0
        && crosshairState < static_cast<int>(interactionReticleAssets_.size())
        && interactionReticleAssets_[crosshairState].loaded) {
        const ReticleAsset& asset = interactionReticleAssets_[crosshairState];
        interactionReticleUploadPixels_.resize(asset.pixels.size());
        const float colorRed = std::clamp(red, 0.0f, 1.0f);
        const float colorGreen = std::clamp(green, 0.0f, 1.0f);
        const float colorBlue = std::clamp(blue, 0.0f, 1.0f);
        const float colorAlpha = std::clamp(alpha, 0.0f, 1.0f);
        for (size_t pixel = 0; pixel < asset.pixels.size(); pixel += 4) {
            interactionReticleUploadPixels_[pixel + 0] = static_cast<uint8_t>(
                std::lround(asset.pixels[pixel + 0] * colorRed));
            interactionReticleUploadPixels_[pixel + 1] = static_cast<uint8_t>(
                std::lround(asset.pixels[pixel + 1] * colorGreen));
            interactionReticleUploadPixels_[pixel + 2] = static_cast<uint8_t>(
                std::lround(asset.pixels[pixel + 2] * colorBlue));
            interactionReticleUploadPixels_[pixel + 3] = static_cast<uint8_t>(
                std::lround(asset.pixels[pixel + 3] * colorAlpha));
        }

        int32_t savedTexture = 0;
        int32_t savedUnpackAlignment = 0;
        glGetIntegerv(kGlTextureBinding2D, &savedTexture);
        glGetIntegerv(kGlUnpackAlignment, &savedUnpackAlignment);
        glBindTexture(kGlTexture2D, interactionReticle_.images[imageIndex].image);
        glPixelStorei(kGlUnpackAlignment, 1);
        glTexSubImage2D(
            kGlTexture2D,
            0,
            0,
            0,
            interactionReticle_.width,
            interactionReticle_.height,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            interactionReticleUploadPixels_.data());
        glPixelStorei(kGlUnpackAlignment, savedUnpackAlignment);
        glBindTexture(kGlTexture2D, static_cast<uint32_t>(savedTexture));
        return true;
    }

    int32_t savedReadFramebuffer = 0;
    int32_t savedDrawFramebuffer = 0;
    int32_t savedReadBuffer = 0;
    int32_t savedDrawBuffer = 0;
    int32_t savedViewport[4] = {};
    int32_t savedScissorBox[4] = {};
    float savedClearColor[4] = {};
    GLboolean savedColorMask[4] = {};
    const GLboolean savedScissorEnabled = glIsEnabled(kGlScissorTest);
    glGetIntegerv(kGlReadFramebufferBinding, &savedReadFramebuffer);
    glGetIntegerv(kGlDrawFramebufferBinding, &savedDrawFramebuffer);
    glGetIntegerv(kGlReadBuffer, &savedReadBuffer);
    glGetIntegerv(kGlDrawBuffer, &savedDrawBuffer);
    glGetIntegerv(kGlViewport, savedViewport);
    glGetIntegerv(kGlScissorBox, savedScissorBox);
    glGetFloatv(kGlColorClearValue, savedClearColor);
    glGetBooleanv(kGlColorWriteMask, savedColorMask);

    glBindFramebuffer_(kGlFramebuffer, interactionReticle_.framebuffers[imageIndex]);
    glDrawBuffer(kGlColorAttachment0);
    glViewport(0, 0, interactionReticle_.width, interactionReticle_.height);
    glDisable(kGlScissorTest);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(kGlColorBufferBit);
    glEnable(kGlScissorTest);

    const int size = interactionReticle_.width;
    const int center = size / 2;
    const int arm = std::max(size / 3, 6);
    const int gap = std::max(size / 9, 2);
    const int shadowThickness = std::max(size / 10, 3);
    const int lineThickness = std::max(size / 24, 2);
    auto drawSegments = [&](int thickness, float red, float green, float blue, float alpha) {
        const int halfThickness = thickness / 2;
        glClearColor(red, green, blue, alpha);
        glScissor(center - arm, center - halfThickness, arm - gap, thickness);
        glClear(kGlColorBufferBit);
        glScissor(center + gap, center - halfThickness, arm - gap, thickness);
        glClear(kGlColorBufferBit);
        glScissor(center - halfThickness, center - arm, thickness, arm - gap);
        glClear(kGlColorBufferBit);
        glScissor(center - halfThickness, center + gap, thickness, arm - gap);
        glClear(kGlColorBufferBit);
    };
    drawSegments(shadowThickness, 0.0f, 0.0f, 0.0f, 0.75f);
    drawSegments(
        lineThickness,
        std::clamp(red, 0.0f, 1.0f),
        std::clamp(green, 0.0f, 1.0f),
        std::clamp(blue, 0.0f, 1.0f),
        std::clamp(alpha, 0.0f, 1.0f));

    glBindFramebuffer_(kGlReadFramebuffer, static_cast<uint32_t>(savedReadFramebuffer));
    glReadBuffer(static_cast<uint32_t>(savedReadBuffer));
    glBindFramebuffer_(kGlDrawFramebuffer, static_cast<uint32_t>(savedDrawFramebuffer));
    glDrawBuffer(static_cast<uint32_t>(savedDrawBuffer));
    glViewport(savedViewport[0], savedViewport[1], savedViewport[2], savedViewport[3]);
    glScissor(savedScissorBox[0], savedScissorBox[1], savedScissorBox[2], savedScissorBox[3]);
    glClearColor(savedClearColor[0], savedClearColor[1], savedClearColor[2], savedClearColor[3]);
    glColorMask(savedColorMask[0], savedColorMask[1], savedColorMask[2], savedColorMask[3]);
    if (savedScissorEnabled == GL_TRUE) {
        glEnable(kGlScissorTest);
    } else {
        glDisable(kGlScissorTest);
    }
    return true;
}

bool OpenXRGLBridge::CreateHudCaptureTarget()
{
    int32_t savedTexture = 0;
    int32_t savedReadFramebuffer = 0;
    int32_t savedDrawFramebuffer = 0;
    int32_t savedReadBuffer = 0;
    int32_t savedDrawBuffer = 0;
    glGetIntegerv(kGlTextureBinding2D, &savedTexture);
    glGetIntegerv(kGlReadFramebufferBinding, &savedReadFramebuffer);
    glGetIntegerv(kGlDrawFramebufferBinding, &savedDrawFramebuffer);
    glGetIntegerv(kGlReadBuffer, &savedReadBuffer);
    glGetIntegerv(kGlDrawBuffer, &savedDrawBuffer);

    glGenTextures(1, &hud_.captureTexture);
    glBindTexture(kGlTexture2D, hud_.captureTexture);
    glTexParameteri(kGlTexture2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(kGlTexture2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(kGlTexture2D, GL_TEXTURE_WRAP_S, kGlClampToEdge);
    glTexParameteri(kGlTexture2D, GL_TEXTURE_WRAP_T, kGlClampToEdge);
    glTexImage2D(
        kGlTexture2D,
        0,
        static_cast<int32_t>(hud_.format),
        hud_.width,
        hud_.height,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        nullptr);
    glGenFramebuffers_(1, &hud_.captureFramebuffer);
    glBindFramebuffer_(kGlFramebuffer, hud_.captureFramebuffer);
    glFramebufferTexture2D_(
        kGlFramebuffer,
        kGlColorAttachment0,
        kGlTexture2D,
        hud_.captureTexture,
        0);
    glDrawBuffer(kGlColorAttachment0);
    const uint32_t status = glCheckFramebufferStatus_(kGlFramebuffer);

    glBindFramebuffer_(kGlReadFramebuffer, static_cast<uint32_t>(savedReadFramebuffer));
    glReadBuffer(static_cast<uint32_t>(savedReadBuffer));
    glBindFramebuffer_(kGlDrawFramebuffer, static_cast<uint32_t>(savedDrawFramebuffer));
    glDrawBuffer(static_cast<uint32_t>(savedDrawBuffer));
    glBindTexture(kGlTexture2D, static_cast<uint32_t>(savedTexture));
    if (status != kGlFramebufferComplete) {
        if (hud_.captureFramebuffer != 0) {
            glDeleteFramebuffers_(1, &hud_.captureFramebuffer);
        }
        if (hud_.captureTexture != 0) {
            glDeleteTextures(1, &hud_.captureTexture);
        }
        hud_.captureFramebuffer = 0;
        hud_.captureTexture = 0;
        return false;
    }
    return true;
}

bool OpenXRGLBridge::CreateTerminalHudCaptureTarget(int width, int height)
{
    const int32_t requestedWidth = std::clamp(width, 256, 4096);
    const int32_t requestedHeight = std::clamp(height, 256, 4096);
    if (hud_.terminalFramebuffer != 0
        && hud_.terminalTexture != 0
        && hud_.terminalWidth == requestedWidth
        && hud_.terminalHeight == requestedHeight) {
        return true;
    }

    int32_t savedTexture = 0;
    int32_t savedReadFramebuffer = 0;
    int32_t savedDrawFramebuffer = 0;
    int32_t savedReadBuffer = 0;
    int32_t savedDrawBuffer = 0;
    glGetIntegerv(kGlTextureBinding2D, &savedTexture);
    glGetIntegerv(kGlReadFramebufferBinding, &savedReadFramebuffer);
    glGetIntegerv(kGlDrawFramebufferBinding, &savedDrawFramebuffer);
    glGetIntegerv(kGlReadBuffer, &savedReadBuffer);
    glGetIntegerv(kGlDrawBuffer, &savedDrawBuffer);

    if (hud_.terminalFramebuffer != 0) {
        glDeleteFramebuffers_(1, &hud_.terminalFramebuffer);
        hud_.terminalFramebuffer = 0;
    }
    if (hud_.terminalTexture != 0) {
        glDeleteTextures(1, &hud_.terminalTexture);
        hud_.terminalTexture = 0;
    }

    glGenTextures(1, &hud_.terminalTexture);
    glBindTexture(kGlTexture2D, hud_.terminalTexture);
    glTexParameteri(kGlTexture2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(kGlTexture2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(kGlTexture2D, GL_TEXTURE_WRAP_S, kGlClampToEdge);
    glTexParameteri(kGlTexture2D, GL_TEXTURE_WRAP_T, kGlClampToEdge);
    glTexImage2D(
        kGlTexture2D,
        0,
        static_cast<int32_t>(hud_.format),
        requestedWidth,
        requestedHeight,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        nullptr);
    glGenFramebuffers_(1, &hud_.terminalFramebuffer);
    glBindFramebuffer_(kGlFramebuffer, hud_.terminalFramebuffer);
    glFramebufferTexture2D_(
        kGlFramebuffer,
        kGlColorAttachment0,
        kGlTexture2D,
        hud_.terminalTexture,
        0);
    glDrawBuffer(kGlColorAttachment0);
    const uint32_t status = glCheckFramebufferStatus_(kGlFramebuffer);

    glBindFramebuffer_(kGlReadFramebuffer, static_cast<uint32_t>(savedReadFramebuffer));
    glReadBuffer(static_cast<uint32_t>(savedReadBuffer));
    glBindFramebuffer_(kGlDrawFramebuffer, static_cast<uint32_t>(savedDrawFramebuffer));
    glDrawBuffer(static_cast<uint32_t>(savedDrawBuffer));
    glBindTexture(kGlTexture2D, static_cast<uint32_t>(savedTexture));
    if (status != kGlFramebufferComplete) {
        if (hud_.terminalFramebuffer != 0) {
            glDeleteFramebuffers_(1, &hud_.terminalFramebuffer);
        }
        if (hud_.terminalTexture != 0) {
            glDeleteTextures(1, &hud_.terminalTexture);
        }
        hud_.terminalFramebuffer = 0;
        hud_.terminalTexture = 0;
        hud_.terminalWidth = 0;
        hud_.terminalHeight = 0;
        hud_.terminalValid = false;
        return false;
    }

    hud_.terminalWidth = requestedWidth;
    hud_.terminalHeight = requestedHeight;
    hud_.terminalValid = false;
    Logger::Instance().Write(
        LogLevel::Info,
        "openxr_terminal_capture_target created size=%dx%d texture=%u fbo=%u policy=logical_gui_native_size_then_upscale",
        hud_.terminalWidth,
        hud_.terminalHeight,
        hud_.terminalTexture,
        hud_.terminalFramebuffer);
    return true;
}

bool OpenXRGLBridge::CreateEyeCache(EyeSwapchain& eye, uint32_t eyeIndex)
{
    int32_t savedTexture = 0;
    int32_t savedReadFramebuffer = 0;
    int32_t savedDrawFramebuffer = 0;
    glGetIntegerv(kGlTextureBinding2D, &savedTexture);
    glGetIntegerv(kGlReadFramebufferBinding, &savedReadFramebuffer);
    glGetIntegerv(kGlDrawFramebufferBinding, &savedDrawFramebuffer);

    glGenTextures(1, &eye.cacheTexture);
    glBindTexture(kGlTexture2D, eye.cacheTexture);
    glTexParameteri(kGlTexture2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(kGlTexture2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(kGlTexture2D, GL_TEXTURE_WRAP_S, kGlClampToEdge);
    glTexParameteri(kGlTexture2D, GL_TEXTURE_WRAP_T, kGlClampToEdge);
    glTexImage2D(
        kGlTexture2D,
        0,
        static_cast<int32_t>(eye.format),
        eye.width,
        eye.height,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        nullptr);

    glGenFramebuffers_(1, &eye.cacheFramebuffer);
    glBindFramebuffer_(kGlFramebuffer, eye.cacheFramebuffer);
    glFramebufferTexture2D_(
        kGlFramebuffer,
        kGlColorAttachment0,
        kGlTexture2D,
        eye.cacheTexture,
        0);
    if (depthCaptureProbeEnabled_) {
        glGenTextures(1, &eye.depthCacheTexture);
        glBindTexture(kGlTexture2D, eye.depthCacheTexture);
        glTexParameteri(kGlTexture2D, GL_TEXTURE_MIN_FILTER, kGlNearest);
        glTexParameteri(kGlTexture2D, GL_TEXTURE_MAG_FILTER, kGlNearest);
        glTexParameteri(kGlTexture2D, GL_TEXTURE_WRAP_S, kGlClampToEdge);
        glTexParameteri(kGlTexture2D, GL_TEXTURE_WRAP_T, kGlClampToEdge);
        const int64_t cacheDepthFormat = depthCompositionSubmitEnabled_ && depthFormat_ != 0
            ? depthFormat_
            : kGlDepthComponent24;
        const bool cacheHasStencil = cacheDepthFormat == kGlDepth24Stencil8
            || cacheDepthFormat == kGlDepth32fStencil8;
        const uint32_t cacheDepthExternalFormat = cacheHasStencil
            ? kGlDepthStencil
            : kGlDepthComponent;
        const uint32_t cacheDepthType = cacheDepthFormat == kGlDepthComponent32f
            ? GL_FLOAT
            : cacheDepthFormat == kGlDepth24Stencil8
                ? kGlUnsignedInt248
                : cacheDepthFormat == kGlDepth32fStencil8
                    ? kGlFloat32UnsignedInt248Rev
                    : GL_UNSIGNED_INT;
        glTexImage2D(
            kGlTexture2D,
            0,
            static_cast<int32_t>(cacheDepthFormat),
            eye.width,
            eye.height,
            0,
            cacheDepthExternalFormat,
            cacheDepthType,
            nullptr);
        glFramebufferTexture2D_(
            kGlFramebuffer,
            cacheHasStencil ? kGlDepthStencilAttachment : kGlDepthAttachment,
            kGlTexture2D,
            eye.depthCacheTexture,
            0);
    }
    glDrawBuffer(kGlColorAttachment0);
    uint32_t status = glCheckFramebufferStatus_(kGlFramebuffer);
    if (status != kGlFramebufferComplete && eye.depthCacheTexture != 0) {
        Logger::Instance().Write(
            LogLevel::Warn,
            "openxr_depth_cache disabled eye=%u reason=framebuffer_incomplete status=0x%x",
            eyeIndex,
            status);
        glFramebufferTexture2D_(
            kGlFramebuffer,
            kGlDepthAttachment,
            kGlTexture2D,
            0,
            0);
        glDeleteTextures(1, &eye.depthCacheTexture);
        eye.depthCacheTexture = 0;
        status = glCheckFramebufferStatus_(kGlFramebuffer);
    }

    glBindFramebuffer_(kGlReadFramebuffer, static_cast<uint32_t>(savedReadFramebuffer));
    glBindFramebuffer_(kGlDrawFramebuffer, static_cast<uint32_t>(savedDrawFramebuffer));
    glBindTexture(kGlTexture2D, static_cast<uint32_t>(savedTexture));
    if (status != kGlFramebufferComplete) {
        Logger::Instance().Write(
            LogLevel::Warn,
            "openxr_eye_cache framebuffer_incomplete eye=%u status=0x%x",
            eyeIndex,
            status);
        if (eye.cacheFramebuffer != 0) {
            glDeleteFramebuffers_(1, &eye.cacheFramebuffer);
            eye.cacheFramebuffer = 0;
        }
        if (eye.cacheTexture != 0) {
            glDeleteTextures(1, &eye.cacheTexture);
            eye.cacheTexture = 0;
        }
        if (eye.depthCacheTexture != 0) {
            glDeleteTextures(1, &eye.depthCacheTexture);
            eye.depthCacheTexture = 0;
        }
        return false;
    }

    Logger::Instance().Write(
        LogLevel::Info,
        "openxr_eye_cache created eye=%u size=%dx%d texture=%u depthTexture=%u framebuffer=%u depthCaptureProbe=%d",
        eyeIndex,
        eye.width,
        eye.height,
        eye.cacheTexture,
        eye.depthCacheTexture,
        eye.cacheFramebuffer,
        depthCaptureProbeEnabled_ ? 1 : 0);
    return true;
}

bool OpenXRGLBridge::CopyBackbufferToImage(const EyeSwapchain& eye, uint32_t imageIndex)
{
    if (imageIndex >= eye.framebuffers.size()) {
        return false;
    }

    int32_t viewport[4] = {};
    int32_t savedReadFramebuffer = 0;
    int32_t savedDrawFramebuffer = 0;
    int32_t savedReadBuffer = 0;
    int32_t savedDrawBuffer = 0;
    glGetIntegerv(kGlViewport, viewport);
    glGetIntegerv(kGlReadFramebufferBinding, &savedReadFramebuffer);
    glGetIntegerv(kGlDrawFramebufferBinding, &savedDrawFramebuffer);
    glGetIntegerv(kGlReadBuffer, &savedReadBuffer);
    glGetIntegerv(kGlDrawBuffer, &savedDrawBuffer);

    if (viewport[2] <= 0 || viewport[3] <= 0) {
        return false;
    }

    const GLboolean scissorEnabled = glIsEnabled(kGlScissorTest);
    if (scissorEnabled == GL_TRUE) {
        glDisable(kGlScissorTest);
    }

    glBindFramebuffer_(kGlReadFramebuffer, 0);
    glReadBuffer(kGlBack);
    glBindFramebuffer_(kGlDrawFramebuffer, eye.framebuffers[imageIndex]);
    glDrawBuffer(kGlColorAttachment0);
    glBlitFramebuffer_(
        viewport[0],
        viewport[1],
        viewport[0] + viewport[2],
        viewport[1] + viewport[3],
        0,
        0,
        eye.width,
        eye.height,
        kGlColorBufferBit,
        kGlLinear);

    glBindFramebuffer_(kGlReadFramebuffer, static_cast<uint32_t>(savedReadFramebuffer));
    glReadBuffer(static_cast<uint32_t>(savedReadBuffer));
    glBindFramebuffer_(kGlDrawFramebuffer, static_cast<uint32_t>(savedDrawFramebuffer));
    glDrawBuffer(static_cast<uint32_t>(savedDrawBuffer));
    if (scissorEnabled == GL_TRUE) {
        glEnable(kGlScissorTest);
    }
    return true;
}

bool OpenXRGLBridge::CopyCacheToImage(const EyeSwapchain& eye, uint32_t imageIndex)
{
    if (!eye.cacheValid
        || eye.cacheFramebuffer == 0
        || imageIndex >= eye.framebuffers.size()) {
        return false;
    }

    int32_t savedReadFramebuffer = 0;
    int32_t savedDrawFramebuffer = 0;
    int32_t savedReadBuffer = 0;
    int32_t savedDrawBuffer = 0;
    glGetIntegerv(kGlReadFramebufferBinding, &savedReadFramebuffer);
    glGetIntegerv(kGlDrawFramebufferBinding, &savedDrawFramebuffer);
    glGetIntegerv(kGlReadBuffer, &savedReadBuffer);
    glGetIntegerv(kGlDrawBuffer, &savedDrawBuffer);

    const GLboolean scissorEnabled = glIsEnabled(kGlScissorTest);
    if (scissorEnabled == GL_TRUE) {
        glDisable(kGlScissorTest);
    }

    glBindFramebuffer_(kGlReadFramebuffer, eye.cacheFramebuffer);
    glReadBuffer(kGlColorAttachment0);
    glBindFramebuffer_(kGlDrawFramebuffer, eye.framebuffers[imageIndex]);
    glDrawBuffer(kGlColorAttachment0);
    glBlitFramebuffer_(
        0,
        0,
        eye.width,
        eye.height,
        0,
        0,
        eye.width,
        eye.height,
        kGlColorBufferBit,
        kGlLinear);

    glBindFramebuffer_(kGlReadFramebuffer, static_cast<uint32_t>(savedReadFramebuffer));
    glReadBuffer(static_cast<uint32_t>(savedReadBuffer));
    glBindFramebuffer_(kGlDrawFramebuffer, static_cast<uint32_t>(savedDrawFramebuffer));
    glDrawBuffer(static_cast<uint32_t>(savedDrawBuffer));
    if (scissorEnabled == GL_TRUE) {
        glEnable(kGlScissorTest);
    }
    return true;
}

bool OpenXRGLBridge::CopyDepthCacheToImage(const EyeSwapchain& eye, uint32_t imageIndex)
{
    if (!eye.depthCacheValid
        || eye.cacheFramebuffer == 0
        || imageIndex >= eye.depthFramebuffers.size()) {
        return false;
    }

    int32_t savedReadFramebuffer = 0;
    int32_t savedDrawFramebuffer = 0;
    int32_t savedReadBuffer = 0;
    int32_t savedDrawBuffer = 0;
    glGetIntegerv(kGlReadFramebufferBinding, &savedReadFramebuffer);
    glGetIntegerv(kGlDrawFramebufferBinding, &savedDrawFramebuffer);
    glGetIntegerv(kGlReadBuffer, &savedReadBuffer);
    glGetIntegerv(kGlDrawBuffer, &savedDrawBuffer);

    const GLboolean scissorEnabled = glIsEnabled(kGlScissorTest);
    if (scissorEnabled == GL_TRUE) {
        glDisable(kGlScissorTest);
    }

    glBindFramebuffer_(kGlReadFramebuffer, eye.cacheFramebuffer);
    glReadBuffer(GL_NONE);
    glBindFramebuffer_(kGlDrawFramebuffer, eye.depthFramebuffers[imageIndex]);
    glDrawBuffer(GL_NONE);
    const uint32_t priorError = glGetError();
    glBlitFramebuffer_(
        0,
        0,
        eye.width,
        eye.height,
        0,
        0,
        eye.width,
        eye.height,
        kGlDepthBufferBit,
        kGlNearest);
    const uint32_t copyError = glGetError();

    glBindFramebuffer_(kGlReadFramebuffer, static_cast<uint32_t>(savedReadFramebuffer));
    glReadBuffer(static_cast<uint32_t>(savedReadBuffer));
    glBindFramebuffer_(kGlDrawFramebuffer, static_cast<uint32_t>(savedDrawFramebuffer));
    glDrawBuffer(static_cast<uint32_t>(savedDrawBuffer));
    if (scissorEnabled == GL_TRUE) {
        glEnable(kGlScissorTest);
    }

    if (copyError != GL_NO_ERROR) {
        Logger::Instance().Write(
            LogLevel::Warn,
            "openxr_depth_swapchain copy_failed priorError=0x%x copyError=0x%x size=%dx%d",
            priorError,
            copyError,
            eye.width,
            eye.height);
        return false;
    }
    return true;
}

bool OpenXRGLBridge::CopyHudCaptureToImage(uint32_t imageIndex)
{
    if (!hud_.captureValid
        || hud_.captureFramebuffer == 0
        || imageIndex >= hud_.framebuffers.size()) {
        return false;
    }

    int32_t savedReadFramebuffer = 0;
    int32_t savedDrawFramebuffer = 0;
    int32_t savedReadBuffer = 0;
    int32_t savedDrawBuffer = 0;
    glGetIntegerv(kGlReadFramebufferBinding, &savedReadFramebuffer);
    glGetIntegerv(kGlDrawFramebufferBinding, &savedDrawFramebuffer);
    glGetIntegerv(kGlReadBuffer, &savedReadBuffer);
    glGetIntegerv(kGlDrawBuffer, &savedDrawBuffer);
    const GLboolean scissorEnabled = glIsEnabled(kGlScissorTest);
    if (scissorEnabled == GL_TRUE) {
        glDisable(kGlScissorTest);
    }

    glBindFramebuffer_(kGlReadFramebuffer, hud_.captureFramebuffer);
    glReadBuffer(kGlColorAttachment0);
    glBindFramebuffer_(kGlDrawFramebuffer, hud_.framebuffers[imageIndex]);
    glDrawBuffer(kGlColorAttachment0);
    glBlitFramebuffer_(
        0,
        0,
        hud_.width,
        hud_.height,
        0,
        0,
        hud_.width,
        hud_.height,
        kGlColorBufferBit,
        kGlLinear);

    glBindFramebuffer_(kGlReadFramebuffer, static_cast<uint32_t>(savedReadFramebuffer));
    glReadBuffer(static_cast<uint32_t>(savedReadBuffer));
    glBindFramebuffer_(kGlDrawFramebuffer, static_cast<uint32_t>(savedDrawFramebuffer));
    glDrawBuffer(static_cast<uint32_t>(savedDrawBuffer));
    if (scissorEnabled == GL_TRUE) {
        glEnable(kGlScissorTest);
    }
    return true;
}

} // namespace somavr

#endif
