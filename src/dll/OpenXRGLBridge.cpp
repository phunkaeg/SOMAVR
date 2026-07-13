#include "OpenXRGLBridge.h"

#if defined(SOMAVR_ENABLE_OPENXR)

#include "Logger.h"

#include <Windows.h>
#include <gl/GL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace somavr {
namespace {

constexpr uint32_t kGlFramebuffer = 0x8D40;
constexpr uint32_t kGlReadFramebuffer = 0x8CA8;
constexpr uint32_t kGlDrawFramebuffer = 0x8CA9;
constexpr uint32_t kGlReadFramebufferBinding = 0x8CAA;
constexpr uint32_t kGlDrawFramebufferBinding = 0x8CA6;
constexpr uint32_t kGlColorAttachment0 = 0x8CE0;
constexpr uint32_t kGlFramebufferComplete = 0x8CD5;
constexpr uint32_t kGlTexture2D = 0x0DE1;
constexpr uint32_t kGlTextureBinding2D = 0x8069;
constexpr uint32_t kGlClampToEdge = 0x812F;
constexpr uint32_t kGlReadBuffer = 0x0C02;
constexpr uint32_t kGlDrawBuffer = 0x0C01;
constexpr uint32_t kGlBack = 0x0405;
constexpr uint32_t kGlViewport = 0x0BA2;
constexpr uint32_t kGlScissorTest = 0x0C11;
constexpr uint32_t kGlColorBufferBit = 0x00004000;
constexpr uint32_t kGlLinear = 0x2601;
constexpr int64_t kGlSrgb8Alpha8 = 0x8C43;
constexpr int64_t kGlRgba8 = 0x8058;
constexpr int64_t kGlRgba16f = 0x881A;

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
    default: return "UNKNOWN";
    }
}

} // namespace

bool OpenXRGLBridge::Initialize(
    XrSession session,
    const std::vector<XrViewConfigurationView>& views,
    const std::vector<int64_t>& formats,
    int resolutionScalePercent)
{
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

    session_ = session;
    eyes_.reserve(2);
    for (uint32_t eyeIndex = 0; eyeIndex < 2; ++eyeIndex) {
        if (!CreateEyeSwapchain(session, views[eyeIndex], eyeIndex, resolutionScalePercent)) {
            Shutdown();
            return false;
        }
    }

    Logger::Instance().Write(
        LogLevel::Info,
        "openxr_gl_bridge ready eyes=%zu format=0x%llx(%s) resolutionScalePercent=%d",
        eyes_.size(),
        static_cast<unsigned long long>(colorFormat_),
        GlFormatName(colorFormat_),
        resolutionScalePercent);
    return true;
}

void OpenXRGLBridge::Shutdown()
{
    const bool canDeleteFramebuffers = glDeleteFramebuffers_ != nullptr && wglGetCurrentContext() != nullptr;
    for (EyeSwapchain& eye : eyes_) {
        if (canDeleteFramebuffers && eye.cacheFramebuffer != 0) {
            glDeleteFramebuffers_(1, &eye.cacheFramebuffer);
            eye.cacheFramebuffer = 0;
        }
        if (wglGetCurrentContext() != nullptr && eye.cacheTexture != 0) {
            glDeleteTextures(1, &eye.cacheTexture);
            eye.cacheTexture = 0;
        }
        if (canDeleteFramebuffers && !eye.framebuffers.empty()) {
            glDeleteFramebuffers_(static_cast<int32_t>(eye.framebuffers.size()), eye.framebuffers.data());
        }
        if (eye.handle != XR_NULL_HANDLE) {
            xrDestroySwapchain(eye.handle);
            eye.handle = XR_NULL_HANDLE;
        }
    }
    eyes_.clear();
    session_ = XR_NULL_HANDLE;
    colorFormat_ = 0;
}

bool OpenXRGLBridge::CopyBackbufferToEye(uint32_t eyeIndex)
{
    if (!Ready() || eyeIndex >= eyes_.size()) {
        return false;
    }

    EyeSwapchain& eye = eyes_[eyeIndex];
    XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
    uint32_t imageIndex = 0;
    XrResult result = xrAcquireSwapchainImage(eye.handle, &acquireInfo, &imageIndex);
    if (XR_FAILED(result)) {
        Logger::Instance().Write(
            LogLevel::Warn,
            "openxr_swapchain acquire_failed eye=%u result=%d",
            eyeIndex,
            static_cast<int>(result));
        return false;
    }

    XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
    waitInfo.timeout = XR_INFINITE_DURATION;
    result = xrWaitSwapchainImage(eye.handle, &waitInfo);
    if (XR_FAILED(result)) {
        Logger::Instance().Write(
            LogLevel::Warn,
            "openxr_swapchain wait_failed eye=%u image=%u result=%d",
            eyeIndex,
            imageIndex,
            static_cast<int>(result));
        return false;
    }

    const bool copied = CopyBackbufferToImage(eye, imageIndex);
    glFlush();

    XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
    result = xrReleaseSwapchainImage(eye.handle, &releaseInfo);
    if (XR_FAILED(result)) {
        Logger::Instance().Write(
            LogLevel::Warn,
            "openxr_swapchain release_failed eye=%u image=%u result=%d",
            eyeIndex,
            imageIndex,
            static_cast<int>(result));
        return false;
    }
    return copied;
}

bool OpenXRGLBridge::CaptureBackbufferToCache(uint32_t eyeIndex)
{
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

    eye.cacheValid = true;
    return true;
}

bool OpenXRGLBridge::CopyCacheToEye(uint32_t eyeIndex)
{
    if (!Ready() || eyeIndex >= eyes_.size() || !eyes_[eyeIndex].cacheValid) {
        return false;
    }

    EyeSwapchain& eye = eyes_[eyeIndex];
    XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
    uint32_t imageIndex = 0;
    XrResult result = xrAcquireSwapchainImage(eye.handle, &acquireInfo, &imageIndex);
    if (XR_FAILED(result)) {
        Logger::Instance().Write(
            LogLevel::Warn,
            "openxr_swapchain cache_acquire_failed eye=%u result=%d",
            eyeIndex,
            static_cast<int>(result));
        return false;
    }

    XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
    waitInfo.timeout = XR_INFINITE_DURATION;
    result = xrWaitSwapchainImage(eye.handle, &waitInfo);
    if (XR_FAILED(result)) {
        Logger::Instance().Write(
            LogLevel::Warn,
            "openxr_swapchain cache_wait_failed eye=%u image=%u result=%d",
            eyeIndex,
            imageIndex,
            static_cast<int>(result));
        return false;
    }

    const bool copied = CopyCacheToImage(eye, imageIndex);
    glFlush();

    XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
    result = xrReleaseSwapchainImage(eye.handle, &releaseInfo);
    if (XR_FAILED(result)) {
        Logger::Instance().Write(
            LogLevel::Warn,
            "openxr_swapchain cache_release_failed eye=%u image=%u result=%d",
            eyeIndex,
            imageIndex,
            static_cast<int>(result));
        return false;
    }
    return copied;
}

bool OpenXRGLBridge::StereoCachesReady() const
{
    return Ready()
        && eyes_[0].cacheValid
        && eyes_[1].cacheValid;
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

int64_t OpenXRGLBridge::ColorFormat() const
{
    return colorFormat_;
}

bool OpenXRGLBridge::ResolveFunctions()
{
    glGenFramebuffers_ = ResolveGlProc<GlGenFramebuffersFn>("glGenFramebuffers");
    glDeleteFramebuffers_ = ResolveGlProc<GlDeleteFramebuffersFn>("glDeleteFramebuffers");
    glBindFramebuffer_ = ResolveGlProc<GlBindFramebufferFn>("glBindFramebuffer");
    glFramebufferTexture2D_ = ResolveGlProc<GlFramebufferTexture2DFn>("glFramebufferTexture2D");
    glCheckFramebufferStatus_ = ResolveGlProc<GlCheckFramebufferStatusFn>("glCheckFramebufferStatus");
    glBlitFramebuffer_ = ResolveGlProc<GlBlitFramebufferFn>("glBlitFramebuffer");

    const bool ready = glGenFramebuffers_ != nullptr
        && glDeleteFramebuffers_ != nullptr
        && glBindFramebuffer_ != nullptr
        && glFramebufferTexture2D_ != nullptr
        && glCheckFramebufferStatus_ != nullptr
        && glBlitFramebuffer_ != nullptr;
    Logger::Instance().Write(
        ready ? LogLevel::Info : LogLevel::Warn,
        "openxr_gl_functions ready=%d genFbo=%d deleteFbo=%d bindFbo=%d attachTexture=%d checkFbo=%d blitFbo=%d",
        ready ? 1 : 0,
        glGenFramebuffers_ != nullptr ? 1 : 0,
        glDeleteFramebuffers_ != nullptr ? 1 : 0,
        glBindFramebuffer_ != nullptr ? 1 : 0,
        glFramebufferTexture2D_ != nullptr ? 1 : 0,
        glCheckFramebufferStatus_ != nullptr ? 1 : 0,
        glBlitFramebuffer_ != nullptr ? 1 : 0);
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
    glGetIntegerv(kGlReadFramebufferBinding, &savedReadFramebuffer);
    glGetIntegerv(kGlDrawFramebufferBinding, &savedDrawFramebuffer);

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
    glBindFramebuffer_(kGlDrawFramebuffer, static_cast<uint32_t>(savedDrawFramebuffer));
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

    Logger::Instance().Write(
        LogLevel::Info,
        "openxr_swapchain created eye=%u size=%dx%d images=%u format=0x%llx(%s)",
        eyeIndex,
        eye.width,
        eye.height,
        imageCount,
        static_cast<unsigned long long>(colorFormat_),
        GlFormatName(colorFormat_));
    eyes_.push_back(std::move(eye));
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
    glDrawBuffer(kGlColorAttachment0);
    const uint32_t status = glCheckFramebufferStatus_(kGlFramebuffer);

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
        return false;
    }

    Logger::Instance().Write(
        LogLevel::Info,
        "openxr_eye_cache created eye=%u size=%dx%d texture=%u framebuffer=%u",
        eyeIndex,
        eye.width,
        eye.height,
        eye.cacheTexture,
        eye.cacheFramebuffer);
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

} // namespace somavr

#endif
