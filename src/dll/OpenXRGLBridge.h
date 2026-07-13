#pragma once

#if defined(SOMAVR_ENABLE_OPENXR)

#include <Windows.h>
#include <Unknwn.h>

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

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

    bool Initialize(
        XrSession session,
        const std::vector<XrViewConfigurationView>& views,
        const std::vector<int64_t>& formats,
        int resolutionScalePercent);
    void Shutdown();

    bool CopyBackbufferToEye(uint32_t eyeIndex);
    bool CaptureBackbufferToCache(uint32_t eyeIndex);
    bool CopyCacheToEye(uint32_t eyeIndex);
    bool StereoCachesReady() const;
    bool Ready() const;
    uint32_t EyeCount() const;
    const EyeSwapchain& Eye(uint32_t eyeIndex) const;
    int64_t ColorFormat() const;

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

    XrSession session_ = XR_NULL_HANDLE;
    int64_t colorFormat_ = 0;
    std::vector<EyeSwapchain> eyes_;

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
