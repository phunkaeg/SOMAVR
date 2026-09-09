#include "OpenXRGLBridge.h"

#if defined(SOMAVR_ENABLE_OPENXR)
#include "Config.h"
#include "Logger.h"
#include "OpenGLOwnership.h"

#include <gl/GL.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace somavr {
namespace {
constexpr GLenum kRead = 0x8CA8, kDraw = 0x8CA9;
constexpr GLenum kReadBinding = 0x8CAA, kDrawBinding = 0x8CA6;
constexpr GLenum kRenderbuffer = 0x8D41, kRenderbufferBinding = 0x8CA7;
constexpr GLenum kDepthAttachment = 0x8D00, kObjectType = 0x8CD0, kObjectName = 0x8CD1;
constexpr GLenum kPackBuffer = 0x88EB, kPackBufferBinding = 0x88ED;
using BindFn = void(APIENTRY*)(uint32_t, uint32_t);

struct ScopedDepthBindings {
    BindFn bindFramebuffer;
    BindFn bindRenderbuffer;
    GLint read = 0, draw = 0, renderbuffer = 0;
    GLboolean scissor = GL_FALSE;
    ScopedDepthBindings(BindFn framebufferFn, BindFn renderbufferFn)
        : bindFramebuffer(framebufferFn), bindRenderbuffer(renderbufferFn)
    {
        glGetIntegerv(kReadBinding, &read);
        glGetIntegerv(kDrawBinding, &draw);
        glGetIntegerv(kRenderbufferBinding, &renderbuffer);
        scissor = glIsEnabled(GL_SCISSOR_TEST);
    }
    ~ScopedDepthBindings()
    {
        bindFramebuffer(kRead, read);
        bindFramebuffer(kDraw, draw);
        bindRenderbuffer(kRenderbuffer, renderbuffer);
        if (scissor) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
    }
};

struct ScopedDepthReadback {
    BindFn bindBuffer;
    GLint buffer = 0, alignment = 4, rowLength = 0, skipRows = 0, skipPixels = 0, swapBytes = 0;
    GLfloat depthScale = 1, depthBias = 0;
    explicit ScopedDepthReadback(BindFn bind) : bindBuffer(bind)
    {
        glGetIntegerv(kPackBufferBinding, &buffer);
        glGetIntegerv(GL_PACK_ALIGNMENT, &alignment);
        glGetIntegerv(GL_PACK_ROW_LENGTH, &rowLength);
        glGetIntegerv(GL_PACK_SKIP_ROWS, &skipRows);
        glGetIntegerv(GL_PACK_SKIP_PIXELS, &skipPixels);
        glGetIntegerv(GL_PACK_SWAP_BYTES, &swapBytes);
        glGetFloatv(GL_DEPTH_SCALE, &depthScale);
        glGetFloatv(GL_DEPTH_BIAS, &depthBias);
        bindBuffer(kPackBuffer, 0);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glPixelStorei(GL_PACK_ROW_LENGTH, 0);
        glPixelStorei(GL_PACK_SKIP_ROWS, 0);
        glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
        glPixelStorei(GL_PACK_SWAP_BYTES, GL_FALSE);
        glPixelTransferf(GL_DEPTH_SCALE, 1);
        glPixelTransferf(GL_DEPTH_BIAS, 0);
    }
    ~ScopedDepthReadback()
    {
        bindBuffer(kPackBuffer, buffer);
        glPixelStorei(GL_PACK_ALIGNMENT, alignment);
        glPixelStorei(GL_PACK_ROW_LENGTH, rowLength);
        glPixelStorei(GL_PACK_SKIP_ROWS, skipRows);
        glPixelStorei(GL_PACK_SKIP_PIXELS, skipPixels);
        glPixelStorei(GL_PACK_SWAP_BYTES, swapBytes);
        glPixelTransferf(GL_DEPTH_SCALE, depthScale);
        glPixelTransferf(GL_DEPTH_BIAS, depthBias);
    }
};
}

void OpenXRGLBridge::InvalidateSceneDepth(uint32_t eyeIndex)
{
    if (eyeIndex >= eyes_.size()) return;
    eyes_[eyeIndex].depthCacheValid = false;
    eyes_[eyeIndex].sceneDepth = {};
}

bool OpenXRGLBridge::CaptureSceneDepthToCache(uint32_t eyeIndex, uint64_t renderSerial,
    uint64_t poseFrame, const depth_math::CompositionDepthRange& range)
{
    ScopedOwnOpenGLWork ownGl;
    if (!Ready() || !depthCaptureProbeEnabled_ || eyeIndex >= eyes_.size()) return false;
    EyeSwapchain& eye = eyes_[eyeIndex];
    InvalidateSceneDepth(eyeIndex);
    const uint64_t attempt = ++eye.depthProbeSamples;
    depth_math::SceneDepthSource source{};
    const char* reason = "functions_unavailable";
    GLenum error = GL_NO_ERROR;
    if (glGetFramebufferAttachmentParameteriv_ && glBindRenderbuffer_
        && glGetRenderbufferParameteriv_ && eye.depthCacheTexture != 0) {
        ScopedDepthBindings restore(glBindFramebuffer_, glBindRenderbuffer_);
        source.framebuffer = static_cast<uint32_t>(restore.draw);
        glGetIntegerv(GL_VIEWPORT, source.viewport.data());
        glGetFloatv(GL_DEPTH_RANGE, source.depthRange.data());
        error = glGetError();
        reason = error == GL_NO_ERROR ? "default_framebuffer" : "preexisting_gl_error";
        if (error == GL_NO_ERROR && source.framebuffer != 0) {
            source.complete = glCheckFramebufferStatus_(kDraw) == 0x8CD5;
            GLint type = 0, object = 0;
            glGetFramebufferAttachmentParameteriv_(kDraw, kDepthAttachment, kObjectType, &type);
            glGetFramebufferAttachmentParameteriv_(kDraw, kDepthAttachment, kObjectName, &object);
            source.objectType = type;
            source.object = object;
            if (type == kRenderbuffer && object != 0) {
                glBindRenderbuffer_(kRenderbuffer, object);
                glGetRenderbufferParameteriv_(kRenderbuffer, 0x8D42, &source.width);
                glGetRenderbufferParameteriv_(kRenderbuffer, 0x8D43, &source.height);
                glGetRenderbufferParameteriv_(kRenderbuffer, 0x8D44, &source.format);
                glGetRenderbufferParameteriv_(kRenderbuffer, 0x8CAB, &source.samples);
            }
            error = glGetError();
            reason = error == GL_NO_ERROR
                ? depth_math::ValidateSceneDepthSource(source, eye.depthCacheFormat) : "attachment_query_error";
            if (std::strcmp(reason, "accepted") == 0) {
                glDisable(GL_SCISSOR_TEST);
                glBindFramebuffer_(kRead, source.framebuffer);
                glBindFramebuffer_(kDraw, eye.cacheFramebuffer);
                glBlitFramebuffer_(0, 0, source.width, source.height,
                    0, 0, eye.width, eye.height, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
                error = glGetError();
                if (error != GL_NO_ERROR) reason = "depth_blit_error";
            }
        }
    }
    eye.sceneDepth = {renderSerial, poseFrame, source, range,
        std::strcmp(reason, "accepted") == 0 && renderSerial != 0 && poseFrame != 0};
    if (attempt <= 8 || attempt % 120 == 0) {
        Logger::Instance().Write(eye.sceneDepth.captured ? LogLevel::Info : LogLevel::Warn,
            "openxr_scene_depth_capture attempt=%llu eye=%u serial=%llu poseFrame=%llu result=%s context=%p fbo=%u objectType=0x%x object=%u format=0x%x cacheFormat=0x%llx size=%dx%d samples=%d viewport=%d,%d,%d,%d range=%.4f,%.4f clipMeters=%.5f,%.3f glError=0x%x policy=pre_post_exact_player_renderbuffer_only",
            static_cast<unsigned long long>(attempt), eyeIndex,
            static_cast<unsigned long long>(renderSerial), static_cast<unsigned long long>(poseFrame),
            reason, wglGetCurrentContext(), source.framebuffer, source.objectType, source.object,
            source.format, static_cast<unsigned long long>(eye.depthCacheFormat), source.width, source.height,
            source.samples, source.viewport[0], source.viewport[1], source.viewport[2], source.viewport[3],
            source.depthRange[0], source.depthRange[1], range.nearMeters, range.farMeters, error);
    }
    return eye.sceneDepth.captured;
}

bool OpenXRGLBridge::DumpEyeDepthCache(uint32_t eyeIndex, uint64_t frameIndex,
    uint64_t sequence, uint32_t sampleIndex, uint64_t poseFrame)
{
    ScopedOwnOpenGLWork ownGl;
    if (eyeIndex >= eyes_.size()) return false;
    const EyeSwapchain& eye = eyes_[eyeIndex];
    if (!eye.depthCacheValid || !eye.sceneDepth.captured || eye.sceneDepth.poseFrame != poseFrame
        || !glBindBuffer_ || !glBindRenderbuffer_ || eye.width <= 0 || eye.height <= 0
        || static_cast<uint64_t>(eye.width) * eye.height > 16'777'216) {
        Logger::Instance().Write(LogLevel::Warn,
            "openxr_scene_depth_dump skipped eye=%u seq=%llu sample=%u matched=%d poseFrame=%llu depthPose=%llu",
            eyeIndex, static_cast<unsigned long long>(sequence), sampleIndex, eye.depthCacheValid ? 1 : 0,
            static_cast<unsigned long long>(poseFrame), static_cast<unsigned long long>(eye.sceneDepth.poseFrame));
        return false;
    }
    std::vector<float> pixels(static_cast<size_t>(eye.width) * eye.height);
    GLenum error;
    {
        ScopedDepthBindings restore(glBindFramebuffer_, glBindRenderbuffer_);
        ScopedDepthReadback pack(glBindBuffer_);
        glBindFramebuffer_(kRead, eye.cacheFramebuffer);
        for (int i = 0; i < 16 && glGetError() != GL_NO_ERROR; ++i) {}
        glReadPixels(0, 0, eye.width, eye.height, GL_DEPTH_COMPONENT, GL_FLOAT, pixels.data());
        error = glGetError();
    }
    if (error != GL_NO_ERROR) return false;
    float minimum = 1, maximum = 0;
    size_t farPixels = 0, invalidPixels = 0;
    for (float depth : pixels) {
        if (!std::isfinite(depth) || depth < 0 || depth > 1) { ++invalidPixels; continue; }
        minimum = std::min(minimum, depth);
        maximum = std::max(maximum, depth);
        farPixels += depth == 1.0f;
    }
    const auto directory = LogPath().parent_path() / "eye-captures";
    std::error_code ec;
    std::filesystem::create_directories(directory, ec);
    if (ec) return false;
    const std::string stem = std::string(eyeIndex == 0 ? "eye_left" : "eye_right")
        + "_seq" + std::to_string(sequence) + "_frame" + std::to_string(frameIndex)
        + "_sample" + std::to_string(sampleIndex);
    // PFM is bottom-up, little endian for negative scale. RGB dumps are top-down.
    std::ofstream data(directory / (stem + "_depth.pfm"), std::ios::binary);
    data << "Pf\n" << eye.width << ' ' << eye.height << "\n-1.0\n";
    data.write(reinterpret_cast<const char*>(pixels.data()), pixels.size() * sizeof(float));
    data.close();
    const auto& stamp = eye.sceneDepth;
    std::ofstream metadata(directory / (stem + "_depth.json"));
    metadata.precision(9);
    metadata << "{\n  \"eye\": " << eyeIndex << ", \"frame\": " << frameIndex
        << ", \"poseFrame\": " << poseFrame << ", \"serial\": " << stamp.renderSerial
        << ",\n  \"width\": " << eye.width << ", \"height\": " << eye.height
        << ", \"origin\": \"bottom_left\", \"encoding\": \"normal_gl_depth_0_1\""
        << ",\n  \"nearMeters\": " << stamp.range.nearMeters << ", \"farMeters\": " << stamp.range.farMeters
        << ",\n  \"sourceFbo\": " << stamp.source.framebuffer << ", \"sourceObject\": " << stamp.source.object
        << ", \"sourceFormat\": " << stamp.source.format
        << ",\n  \"minimum\": " << minimum << ", \"maximum\": " << maximum
        << ", \"farPixels\": " << farPixels << ", \"invalidPixels\": " << invalidPixels << "\n}\n";
    metadata.close();
    const bool written = data.good() && metadata.good();
    Logger::Instance().Write(written ? LogLevel::Info : LogLevel::Warn,
        "openxr_scene_depth_dump eye=%u seq=%llu sample=%u poseFrame=%llu written=%d min=%.7f max=%.7f farPixels=%llu invalidPixels=%llu totalPixels=%llu",
        eyeIndex, static_cast<unsigned long long>(sequence), sampleIndex, static_cast<unsigned long long>(poseFrame),
        written ? 1 : 0, minimum, maximum, static_cast<unsigned long long>(farPixels),
        static_cast<unsigned long long>(invalidPixels), static_cast<unsigned long long>(pixels.size()));
    return written;
}
} // namespace somavr
#endif
