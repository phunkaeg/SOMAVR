#pragma once

#if defined(SOMAVR_ENABLE_OPENXR)

#include <Windows.h>
#include <Unknwn.h>

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

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
        std::vector<XrSwapchainImageOpenGLKHR> images;
        std::vector<uint32_t> framebuffers;
        uint32_t cacheTexture = 0;
        uint32_t cacheFramebuffer = 0;
        bool cacheValid = false;
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

    bool Initialize(
        XrSession session,
        const std::vector<XrViewConfigurationView>& views,
        const std::vector<int64_t>& formats,
        int resolutionScalePercent,
        bool hudLayerEnabled,
        int hudWidth,
        int hudHeight,
        bool suppressCenterCrosshair,
        int crosshairClearRadiusPixels,
        bool interactionReticleEnabled,
        bool interactionReticleNativeIconsEnabled,
        int interactionReticleSizePixels);
    void Shutdown();

    bool CopyBackbufferToEye(uint32_t eyeIndex);
    bool CaptureBackbufferToCache(uint32_t eyeIndex);
    bool CopyCacheToEye(uint32_t eyeIndex);
    void InvalidateStereoCaches();
    bool StereoCachesReady() const;
    bool Ready() const;
    uint32_t EyeCount() const;
    const EyeSwapchain& Eye(uint32_t eyeIndex) const;
    int64_t ColorFormat() const;
    bool BeginHudCapture(uint64_t frameIndex);
    bool EndHudCapture(uint64_t frameIndex);
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
    bool CreateHudSwapchain(XrSession session, int width, int height);
    bool CreateHudCaptureTarget();
    bool CopyHudCaptureToImage(uint32_t imageIndex);
    bool CreateInteractionReticleSwapchain(XrSession session, int sizePixels);
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
    std::vector<EyeSwapchain> eyes_;
    HudSwapchain hud_;
    ReticleSwapchain interactionReticle_;

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
