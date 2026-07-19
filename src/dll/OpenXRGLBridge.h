#pragma once

#if defined(SOMAVR_ENABLE_OPENXR)

#include <Windows.h>
#include <Unknwn.h>

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include "OpenXRSpectatorMath.h"

#include <array>
#include <cstdint>
#include <vector>

namespace somavr {

class OpenXRGLBridge {
public:
    struct EyeSwapchain {
        XrSwapchain handle = XR_NULL_HANDLE;
        int32_t width = 0;
        int32_t height = 0;
        int64_t format = 0;
        XrSwapchain depthHandle = XR_NULL_HANDLE;
        int64_t depthFormat = 0;
        std::vector<XrSwapchainImageOpenGLKHR> images;
        std::vector<uint32_t> framebuffers;
        std::vector<XrSwapchainImageOpenGLKHR> depthImages;
        std::vector<uint32_t> depthFramebuffers;
        uint32_t cacheTexture = 0;
        uint32_t depthCacheTexture = 0;
        uint32_t cacheFramebuffer = 0;
        bool cacheValid = false;
        bool depthCacheValid = false;
        uint64_t depthProbeSamples = 0;
    };

    struct HudSwapchain {
        XrSwapchain handle = XR_NULL_HANDLE;
        int32_t width = 0;
        int32_t height = 0;
        int64_t format = 0;
        std::vector<XrSwapchainImageOpenGLKHR> images;
        std::vector<uint32_t> framebuffers;
        uint32_t captureTexture = 0;
        uint32_t captureFramebuffer = 0;
        bool captureValid = false;
        uint64_t captureFrame = 0;
    };

    struct ReticleSwapchain {
        XrSwapchain handle = XR_NULL_HANDLE;
        int32_t width = 0;
        int32_t height = 0;
        int64_t format = 0;
        std::vector<XrSwapchainImageOpenGLKHR> images;
        std::vector<uint32_t> framebuffers;
    };

    struct StatusPanelSwapchain {
        XrSwapchain handle = XR_NULL_HANDLE;
        int32_t width = 0;
        int32_t height = 0;
        int64_t format = 0;
        std::vector<XrSwapchainImageOpenGLKHR> images;
    };

    struct ComfortVignetteSwapchain {
        XrSwapchain handle = XR_NULL_HANDLE;
        int32_t width = 0;
        int32_t height = 0;
        int64_t format = 0;
        std::vector<XrSwapchainImageOpenGLKHR> images;
    };

    bool Initialize(
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
        int comfortVignetteSizePixels);
    void Shutdown(bool deleteGlResources = true);

    bool CopyBackbufferToEye(uint32_t eyeIndex);
    bool CaptureBackbufferToCache(uint32_t eyeIndex);
    bool CopyCacheToEye(uint32_t eyeIndex);
    bool CopyDepthCacheToEye(uint32_t eyeIndex);
    bool CopyCacheToBackbuffer(uint32_t eyeIndex, spectator_math::AspectMode aspectMode);
    void InvalidateStereoCaches();
    bool StereoCachesReady() const;
    bool DepthCachesReady() const;
    bool DepthSwapchainsReady() const;
    bool Ready() const;
    uint32_t EyeCount() const;
    const EyeSwapchain& Eye(uint32_t eyeIndex) const;
    int64_t ColorFormat() const;
    bool BeginHudCapture(uint64_t frameIndex);
    bool EndHudCapture(uint64_t frameIndex, bool suppressCenterCrosshair);
    bool CaptureFramebufferToHud(
        uint64_t frameIndex,
        uint32_t sourceFramebuffer,
        int sourceX,
        int sourceY,
        int sourceWidth,
        int sourceHeight);
    bool CopyHudCaptureToSwapchain();
    void InvalidateHudCapture();
    bool HudReady() const;
    bool HudCaptureFresh(uint64_t frameIndex, uint64_t maxAgeFrames) const;
    const HudSwapchain& Hud() const;
    bool DrawInteractionReticleToSwapchain(
        int crosshairState,
        float red,
        float green,
        float blue,
        float alpha);
    bool InteractionReticleReady() const;
    const ReticleSwapchain& InteractionReticle() const;
    bool DrawControllerAimGuideToSwapchain(
        float red,
        float green,
        float blue,
        float alpha);
    bool ControllerAimGuideReady() const;
    const ReticleSwapchain& ControllerAimGuide() const;
    bool DrawStatusPanelToSwapchain(const std::vector<uint8_t>& rgbaPixels);
    bool StatusPanelReady() const;
    const StatusPanelSwapchain& StatusPanel() const;
    bool DrawComfortVignetteToSwapchain(const std::vector<uint8_t>& rgbaPixels);
    bool ComfortVignetteReady() const;
    const ComfortVignetteSwapchain& ComfortVignette() const;

private:
    bool ResolveFunctions();
    bool CreateEyeSwapchain(
        XrSession session,
        const XrViewConfigurationView& view,
        uint32_t eyeIndex,
        int resolutionScalePercent);
    bool CopyBackbufferToImage(const EyeSwapchain& eye, uint32_t imageIndex);
    bool CopyCacheToImage(const EyeSwapchain& eye, uint32_t imageIndex);
    bool CreateEyeCache(EyeSwapchain& eye, uint32_t eyeIndex);
    bool CreateDepthSwapchain(EyeSwapchain& eye, uint32_t eyeIndex);
    bool CopyDepthCacheToImage(const EyeSwapchain& eye, uint32_t imageIndex);
    bool CreateHudSwapchain(XrSession session, int width, int height);
    bool CreateHudCaptureTarget();
    bool CopyHudCaptureToImage(uint32_t imageIndex);
    bool CreateInteractionReticleSwapchain(XrSession session, int sizePixels);
    bool CreateControllerAimGuideSwapchain(XrSession session, int sizePixels);
    bool CreateStatusPanelSwapchain(XrSession session, int width, int height);
    bool CreateComfortVignetteSwapchain(XrSession session, int sizePixels);
    void LoadInteractionReticleAssets();
    bool DrawInteractionReticleToImage(
        uint32_t imageIndex,
        int crosshairState,
        float red,
        float green,
        float blue,
        float alpha);
    void RestoreHudCaptureState();

    XrSession session_ = XR_NULL_HANDLE;
    int64_t colorFormat_ = 0;
    int64_t depthFormat_ = 0;
    std::vector<EyeSwapchain> eyes_;
    HudSwapchain hud_;
    ReticleSwapchain interactionReticle_;
    ReticleSwapchain controllerAimGuide_;
    StatusPanelSwapchain statusPanel_;
    ComfortVignetteSwapchain comfortVignette_;

    struct ReticleAsset {
        std::vector<uint8_t> pixels;
        bool loaded = false;
    };
    std::array<ReticleAsset, 35> interactionReticleAssets_;
    std::vector<uint8_t> interactionReticleUploadPixels_;
    bool interactionReticleNativeIconsEnabled_ = false;
    uint32_t interactionReticleAssetsLoaded_ = 0;

    struct HudCaptureState {
        bool active = false;
        int32_t readFramebuffer = 0;
        int32_t drawFramebuffer = 0;
        int32_t readBuffer = 0;
        int32_t drawBuffer = 0;
        int32_t viewport[4] = {};
        float clearColor[4] = {};
        unsigned char colorMask[4] = {};
        int32_t scissorBox[4] = {};
        bool scissorEnabled = false;
    } hudCaptureState_;

    bool suppressCenterCrosshair_ = false;
    bool foveationSwapchainEnabled_ = false;
    bool depthCaptureProbeEnabled_ = false;
    bool depthCompositionSubmitEnabled_ = false;
    int crosshairClearRadiusPixels_ = 48;

    using GlGenFramebuffersFn = void(APIENTRY*)(int32_t, uint32_t*);
    using GlDeleteFramebuffersFn = void(APIENTRY*)(int32_t, const uint32_t*);
    using GlBindFramebufferFn = void(APIENTRY*)(uint32_t, uint32_t);
    using GlFramebufferTexture2DFn = void(APIENTRY*)(uint32_t, uint32_t, uint32_t, uint32_t, int32_t);
    using GlCheckFramebufferStatusFn = uint32_t(APIENTRY*)(uint32_t);
    using GlBlitFramebufferFn = void(APIENTRY*)(int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, uint32_t, uint32_t);

    GlGenFramebuffersFn glGenFramebuffers_ = nullptr;
    GlDeleteFramebuffersFn glDeleteFramebuffers_ = nullptr;
    GlBindFramebufferFn glBindFramebuffer_ = nullptr;
    GlFramebufferTexture2DFn glFramebufferTexture2D_ = nullptr;
    GlCheckFramebufferStatusFn glCheckFramebufferStatus_ = nullptr;
    GlBlitFramebufferFn glBlitFramebuffer_ = nullptr;
};

} // namespace somavr

#endif
