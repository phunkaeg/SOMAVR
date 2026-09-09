// somavr_xrsim64: OpenGL capture compositor for unattended SOMAVR tests.
//
// This is intentionally a test runtime, not a second renderer in somavr.dll.
// It reads the OpenGL swapchain images submitted to xrEndFrame, composites the
// projection and quad layers into two inspectable eye images, and writes PNG +
// JSON evidence. Ordinary frames do no readback work.
//
// The control/frame/action machinery is adapted from bioshock-trilogy-vr's
// MIT-licensed bvr_xrsim32. This compositor is SOMAVR-specific: it uses the
// application's WGL share group and restores every GL state item it changes.

#include "xrsim_internal.h"

#include <wincodec.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <deque>
#include <map>
#include <share.h>
#include <string>
#include <thread>
#include <vector>

namespace xrsim {
namespace {

constexpr GLenum kGlPixelPackBuffer = 0x88EB;
constexpr GLenum kGlPixelPackBufferBinding = 0x88ED;
using GlBindBufferProc = void(APIENTRY*)(GLenum, GLuint);

HDC g_deviceContext = nullptr;
HGLRC g_glContext = nullptr;
bool g_ready = false;
bool g_disabled = false;
std::atomic<uint32_t> g_lastLayerCount{0};
std::atomic<uint32_t> g_lastProjViews{0};

struct EncodeJob {
    std::vector<uint8_t> pixels[2]; // top-down BGRA
    uint32_t width = 0;
    uint32_t height = 0;
    std::string baseName;
    std::string json;
};

std::mutex g_queueMutex;
std::condition_variable g_queueCv;
std::deque<EncodeJob> g_queue;
std::thread g_encodeThread;
std::atomic<bool> g_encodeRunning{false};

struct LayerStat {
    uint32_t pixelsCovered[2] = {0, 0};
};

struct TexturePixels {
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<uint8_t> rgba; // OpenGL bottom-up row order
};

struct GlReadbackScope {
    GLint texture = 0;
    GLint packAlignment = 4;
    GLint packRowLength = 0;
    GLint packSkipRows = 0;
    GLint packSkipPixels = 0;
    GLint pixelPackBuffer = 0;
    GlBindBufferProc bindBuffer = nullptr;

    GlReadbackScope() {
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
        glGetIntegerv(GL_PACK_ALIGNMENT, &packAlignment);
        glGetIntegerv(GL_PACK_ROW_LENGTH, &packRowLength);
        glGetIntegerv(GL_PACK_SKIP_ROWS, &packSkipRows);
        glGetIntegerv(GL_PACK_SKIP_PIXELS, &packSkipPixels);
        bindBuffer = reinterpret_cast<GlBindBufferProc>(wglGetProcAddress("glBindBuffer"));
        if (bindBuffer) {
            glGetIntegerv(kGlPixelPackBufferBinding, &pixelPackBuffer);
            bindBuffer(kGlPixelPackBuffer, 0);
        }
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glPixelStorei(GL_PACK_ROW_LENGTH, 0);
        glPixelStorei(GL_PACK_SKIP_ROWS, 0);
        glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
    }

    ~GlReadbackScope() {
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture));
        glPixelStorei(GL_PACK_ALIGNMENT, packAlignment);
        glPixelStorei(GL_PACK_ROW_LENGTH, packRowLength);
        glPixelStorei(GL_PACK_SKIP_ROWS, packSkipRows);
        glPixelStorei(GL_PACK_SKIP_PIXELS, packSkipPixels);
        if (bindBuffer) bindBuffer(kGlPixelPackBuffer, static_cast<GLuint>(pixelPackBuffer));
    }
};

bool write_png(const wchar_t* path, const uint8_t* bgra, uint32_t width, uint32_t height) {
    IWICImagingFactory* factory = nullptr;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&factory))))
        return false;

    bool ok = false;
    IWICStream* stream = nullptr;
    IWICBitmapEncoder* encoder = nullptr;
    IWICBitmapFrameEncode* frame = nullptr;
    IPropertyBag2* props = nullptr;
    if (SUCCEEDED(factory->CreateStream(&stream)) &&
        SUCCEEDED(stream->InitializeFromFilename(path, GENERIC_WRITE)) &&
        SUCCEEDED(factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder)) &&
        SUCCEEDED(encoder->Initialize(stream, WICBitmapEncoderNoCache)) &&
        SUCCEEDED(encoder->CreateNewFrame(&frame, &props)) &&
        SUCCEEDED(frame->Initialize(props))) {
        WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
        if (SUCCEEDED(frame->SetSize(width, height)) &&
            SUCCEEDED(frame->SetPixelFormat(&format)) &&
            SUCCEEDED(frame->WritePixels(height, width * 4, width * height * 4,
                                         const_cast<BYTE*>(bgra))) &&
            SUCCEEDED(frame->Commit()) && SUCCEEDED(encoder->Commit())) {
            ok = true;
        }
    }
    if (props) props->Release();
    if (frame) frame->Release();
    if (encoder) encoder->Release();
    if (stream) stream->Release();
    factory->Release();
    return ok;
}

void write_sbs(const wchar_t* path, const EncodeJob& job) {
    const uint32_t width = job.width;
    const uint32_t height = job.height;
    std::vector<uint8_t> sbs(static_cast<size_t>(width) * 2 * height * 4);
    for (uint32_t y = 0; y < height; ++y) {
        memcpy(&sbs[(static_cast<size_t>(y) * width * 2) * 4],
               &job.pixels[0][static_cast<size_t>(y) * width * 4], width * 4);
        memcpy(&sbs[(static_cast<size_t>(y) * width * 2 + width) * 4],
               &job.pixels[1][static_cast<size_t>(y) * width * 4], width * 4);
    }
    write_png(path, sbs.data(), width * 2, height);
}

void encode_thread_proc() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    for (;;) {
        EncodeJob job;
        {
            std::unique_lock<std::mutex> lock(g_queueMutex);
            g_queueCv.wait_for(lock, std::chrono::milliseconds(200), [] {
                return !g_queue.empty() || !g_encodeRunning.load();
            });
            if (g_queue.empty()) {
                if (!g_encodeRunning.load()) break;
                continue;
            }
            job = std::move(g_queue.front());
            g_queue.pop_front();
        }

        wchar_t base[MAX_PATH];
        swprintf_s(base, L"%s\\capture\\%hs", log::dir(), job.baseName.c_str());
        wchar_t path[MAX_PATH];
        swprintf_s(path, L"%s_left.png", base);
        const bool leftOk = write_png(path, job.pixels[0].data(), job.width, job.height);
        swprintf_s(path, L"%s_right.png", base);
        const bool rightOk = write_png(path, job.pixels[1].data(), job.width, job.height);
        swprintf_s(path, L"%s_sbs.png", base);
        write_sbs(path, job);
        swprintf_s(path, L"%s.json", base);
        FILE* file = _wfsopen(path, L"w", _SH_DENYNO);
        if (file) {
            fputs(job.json.c_str(), file);
            fclose(file);
        }

        char utf8[MAX_PATH];
        WideCharToMultiByte(CP_UTF8, 0, base, -1, utf8, MAX_PATH, nullptr, nullptr);
        strncpy_s(g.lastCapturePath, utf8, _TRUNCATE);
        g.captureSeq.fetch_add(1);
        XRSIM_LOG("xrsim: capture written %hs (%ux%u left=%d right=%d)",
                  job.baseName.c_str(), job.width, job.height, leftOk ? 1 : 0,
                  rightOk ? 1 : 0);
    }
    CoUninitialize();
}

bool read_texture(GLuint texture, uint32_t width, uint32_t height, TexturePixels& out) {
    if (!texture || !width || !height) return false;
    out.width = width;
    out.height = height;
    out.rgba.resize(static_cast<size_t>(width) * height * 4);
    while (glGetError() != GL_NO_ERROR) {}
    glBindTexture(GL_TEXTURE_2D, texture);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, out.rgba.data());
    const GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        XRSIM_LOG("xrsim: glGetTexImage failed texture=%u size=%ux%u error=0x%04x",
                  texture, width, height, static_cast<unsigned>(error));
        out = TexturePixels{};
        return false;
    }
    return true;
}

TexturePixels* cached_texture(XrSwapchain swapchain,
                              std::map<GLuint, TexturePixels>& cache) {
    uint32_t width = 0, height = 0;
    const GLuint texture = swapchain_last_image(swapchain, &width, &height);
    if (!texture) return nullptr;
    auto [it, inserted] = cache.emplace(texture, TexturePixels{});
    if (inserted && !read_texture(texture, width, height, it->second)) {
        cache.erase(it);
        return nullptr;
    }
    return &it->second;
}

void sample_subimage(const TexturePixels& source, const XrRect2Di& rect,
                     uint32_t outWidth, uint32_t outHeight,
                     std::vector<uint8_t>& outBgra) {
    outBgra.assign(static_cast<size_t>(outWidth) * outHeight * 4, 0);
    if (!outWidth || !outHeight || source.rgba.empty()) return;
    const int32_t rw = rect.extent.width > 0 ? rect.extent.width : static_cast<int32_t>(source.width);
    const int32_t rh = rect.extent.height > 0 ? rect.extent.height : static_cast<int32_t>(source.height);
    const int32_t ox = rect.offset.x;
    const int32_t oy = rect.offset.y;
    for (uint32_t y = 0; y < outHeight; ++y) {
        const int32_t localY = std::min(rh - 1, static_cast<int32_t>(
            (static_cast<uint64_t>(y) * static_cast<uint32_t>(rh)) / outHeight));
        const int32_t sourceY = std::clamp(oy + rh - 1 - localY, 0,
                                           static_cast<int32_t>(source.height) - 1);
        for (uint32_t x = 0; x < outWidth; ++x) {
            const int32_t localX = std::min(rw - 1, static_cast<int32_t>(
                (static_cast<uint64_t>(x) * static_cast<uint32_t>(rw)) / outWidth));
            const int32_t sourceX = std::clamp(ox + localX, 0,
                                               static_cast<int32_t>(source.width) - 1);
            const size_t src = (static_cast<size_t>(sourceY) * source.width + sourceX) * 4;
            const size_t dst = (static_cast<size_t>(y) * outWidth + x) * 4;
            outBgra[dst + 0] = source.rgba[src + 2];
            outBgra[dst + 1] = source.rgba[src + 1];
            outBgra[dst + 2] = source.rgba[src + 0];
            outBgra[dst + 3] = source.rgba[src + 3];
        }
    }
}

Pose eye_world_pose(const FrameSnapshot& snap, int eye) {
    Pose offset = pose_identity();
    offset.p.x = (eye == 0 ? -0.5f : 0.5f) * snap.rig.ipdM;
    return pose_mul(snap.headWorld, offset);
}

bool project_point(const Pose& eyeWorld, const Fov& fov, const Vec3& world,
                   float& outX, float& outY) {
    const Pose inverseEye = pose_inverse(eyeWorld);
    const Vec3 eye = v3_add(inverseEye.p, quat_rotate(inverseEye.q, world));
    if (eye.z >= -0.001f) return false;
    const float tx = eye.x / -eye.z;
    const float ty = eye.y / -eye.z;
    const float left = tanf(fov.angleLeft);
    const float right = tanf(fov.angleRight);
    const float up = tanf(fov.angleUp);
    const float down = tanf(fov.angleDown);
    if (right - left < 1e-5f || up - down < 1e-5f) return false;
    outX = (tx - left) / (right - left);
    outY = (up - ty) / (up - down);
    return true;
}

void alpha_blit(const std::vector<uint8_t>& source, uint32_t sourceWidth,
                uint32_t sourceHeight, std::vector<uint8_t>& destination,
                uint32_t destWidth, uint32_t destHeight,
                int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                XrCompositionLayerFlags flags, uint32_t& pixelsCovered) {
    if (source.empty() || x1 <= x0 || y1 <= y0) return;
    const int32_t clipX0 = std::max<int32_t>(0, x0);
    const int32_t clipY0 = std::max<int32_t>(0, y0);
    const int32_t clipX1 = std::min<int32_t>(static_cast<int32_t>(destWidth), x1);
    const int32_t clipY1 = std::min<int32_t>(static_cast<int32_t>(destHeight), y1);
    if (clipX1 <= clipX0 || clipY1 <= clipY0) return;
    const bool blend = (flags & XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT) != 0;
    const bool unpremultiplied =
        (flags & XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT) != 0;
    for (int32_t y = clipY0; y < clipY1; ++y) {
        const uint32_t sy = std::min(sourceHeight - 1, static_cast<uint32_t>(
            static_cast<int64_t>(y - y0) * sourceHeight / (y1 - y0)));
        for (int32_t x = clipX0; x < clipX1; ++x) {
            const uint32_t sx = std::min(sourceWidth - 1, static_cast<uint32_t>(
                static_cast<int64_t>(x - x0) * sourceWidth / (x1 - x0)));
            const size_t src = (static_cast<size_t>(sy) * sourceWidth + sx) * 4;
            const size_t dst = (static_cast<size_t>(y) * destWidth + x) * 4;
            const uint32_t alpha = blend ? source[src + 3] : 255;
            if (alpha == 0) continue;
            for (int c = 0; c < 3; ++c) {
                const uint32_t sourceColor = source[src + c];
                const uint32_t contribution =
                    (!blend || unpremultiplied) ? sourceColor * alpha / 255 : sourceColor;
                destination[dst + c] = static_cast<uint8_t>(std::min<uint32_t>(
                    255, contribution + destination[dst + c] * (255 - alpha) / 255));
            }
            destination[dst + 3] = 255;
            ++pixelsCovered;
        }
    }
}

void compose_projection(int eye, const SimLayer& layer, uint32_t width, uint32_t height,
                        std::map<GLuint, TexturePixels>& cache,
                        std::vector<uint8_t>& output, LayerStat& stat) {
    if (layer.viewCount == 0) return;
    const uint32_t viewIndex = std::min<uint32_t>(eye, layer.viewCount - 1);
    const auto& view = layer.views[viewIndex];
    TexturePixels* source = cached_texture(view.subImage.swapchain, cache);
    if (!source) return;
    sample_subimage(*source, view.subImage.imageRect, width, height, output);
    stat.pixelsCovered[eye] = width * height;
}

void compose_quad(int eye, const SimSubmission& sub, const SimLayer& layer,
                  uint32_t width, uint32_t height,
                  std::map<GLuint, TexturePixels>& cache,
                  std::vector<uint8_t>& output, LayerStat& stat) {
    if ((layer.eyeVisibility == XR_EYE_VISIBILITY_LEFT && eye != 0) ||
        (layer.eyeVisibility == XR_EYE_VISIBILITY_RIGHT && eye != 1))
        return;
    SimSpace* space = space_get(layer.space);
    if (!space) return;
    Pose spaceWorld = pose_identity();
    bool tracked = true;
    if (!space_pose(*space, sub.snap, spaceWorld, tracked) || !tracked) return;
    const Pose quadWorld = pose_mul(spaceWorld, from_xr(layer.pose));
    const Pose eyeWorld = eye_world_pose(sub.snap, eye);
    const Fov& fov = sub.snap.rig.fov[eye];

    float minX = 2.0f, minY = 2.0f, maxX = -1.0f, maxY = -1.0f;
    for (int cy = 0; cy < 2; ++cy) {
        for (int cx = 0; cx < 2; ++cx) {
            Vec3 corner{(cx ? 0.5f : -0.5f) * layer.size.width,
                        (cy ? 0.5f : -0.5f) * layer.size.height, 0.0f};
            const Vec3 world = v3_add(quadWorld.p, quat_rotate(quadWorld.q, corner));
            float px = 0.0f, py = 0.0f;
            if (!project_point(eyeWorld, fov, world, px, py)) return;
            minX = std::min(minX, px);
            minY = std::min(minY, py);
            maxX = std::max(maxX, px);
            maxY = std::max(maxY, py);
        }
    }

    const int32_t x0 = static_cast<int32_t>(std::floor(minX * width));
    const int32_t y0 = static_cast<int32_t>(std::floor(minY * height));
    const int32_t x1 = static_cast<int32_t>(std::ceil(maxX * width));
    const int32_t y1 = static_cast<int32_t>(std::ceil(maxY * height));
    if (x1 <= x0 || y1 <= y0) return;
    TexturePixels* texture = cached_texture(layer.sub.swapchain, cache);
    if (!texture) return;
    const uint32_t sourceWidth = static_cast<uint32_t>(std::max(1, layer.sub.imageRect.extent.width));
    const uint32_t sourceHeight = static_cast<uint32_t>(std::max(1, layer.sub.imageRect.extent.height));
    std::vector<uint8_t> source;
    sample_subimage(*texture, layer.sub.imageRect, sourceWidth, sourceHeight, source);
    alpha_blit(source, sourceWidth, sourceHeight, output, width, height,
               x0, y0, x1, y1, layer.flags, stat.pixelsCovered[eye]);
}

void calculate_image_stats(const std::vector<uint8_t>& pixels, uint32_t width, uint32_t height,
                           double& meanLuma, double& nonBlackPercent) {
    uint64_t lumaSum = 0;
    uint64_t nonBlack = 0;
    const uint64_t count = static_cast<uint64_t>(width) * height;
    for (uint64_t i = 0; i < count; ++i) {
        const uint32_t b = pixels[i * 4 + 0];
        const uint32_t g2 = pixels[i * 4 + 1];
        const uint32_t r = pixels[i * 4 + 2];
        const uint32_t luma = (r * 77 + g2 * 151 + b * 28) >> 8;
        lumaSum += luma;
        if (luma > 8) ++nonBlack;
    }
    meanLuma = count ? static_cast<double>(lumaSum) / count : 0.0;
    nonBlackPercent = count ? 100.0 * static_cast<double>(nonBlack) / count : 0.0;
}

std::string build_json(const SimSubmission& sub, const std::vector<LayerStat>& stats,
                       const char* baseName, uint32_t width, uint32_t height,
                       const double* meanLuma, const double* nonBlackPercent) {
    char buffer[4096];
    std::string out;
    out.reserve(12000);
    const Rig& rig = sub.snap.rig;
    const Pose eyes[2] = {eye_world_pose(sub.snap, 0), eye_world_pose(sub.snap, 1)};
    const float eyeSeparation = v3_len(v3_sub(eyes[1].p, eyes[0].p));
    float headYaw = 0.0f, headPitch = 0.0f, headRoll = 0.0f;
    quat_to_ypr(sub.snap.headWorld.q, headYaw, headPitch, headRoll);

    sprintf_s(buffer,
              "{\n  \"name\": \"%s\",\n  \"captureMode\": \"raw-projection-plus-cpu-quads\",\n"
              "  \"frameIndex\": %llu,\n  \"displayTimeNs\": %lld,\n"
              "  \"sessionState\": \"%s\",\n  \"width\": %u,\n  \"height\": %u,\n",
              baseName, static_cast<unsigned long long>(sub.frameIndex),
              static_cast<long long>(sub.displayTime), session_state_name(sub.snap.state),
              width, height);
    out += buffer;
    sprintf_s(buffer,
              "  \"gate\": {\"waited\": %llu, \"begun\": %llu, \"ended\": %llu, "
              "\"discarded\": %u, \"outOfOrder\": %u},\n",
              static_cast<unsigned long long>(g_gate.waited.load()),
              static_cast<unsigned long long>(g_gate.begun.load()),
              static_cast<unsigned long long>(g_gate.ended.load()),
              g_gate.discarded.load(), g_gate.outOfOrder.load());
    out += buffer;
    sprintf_s(buffer,
              "  \"head\": {\"pos\": [%.5f, %.5f, %.5f], \"quat\": [%.5f, %.5f, %.5f, %.5f], "
              "\"ypr\": [%.3f, %.3f, %.3f]},\n",
              sub.snap.headWorld.p.x, sub.snap.headWorld.p.y, sub.snap.headWorld.p.z,
              sub.snap.headWorld.q.x, sub.snap.headWorld.q.y, sub.snap.headWorld.q.z,
              sub.snap.headWorld.q.w, rad2deg(headYaw), rad2deg(headPitch), rad2deg(headRoll));
    out += buffer;
    out += "  \"views\": [\n";
    for (int eye = 0; eye < 2; ++eye) {
        sprintf_s(buffer,
                  "    {\"eye\": \"%s\", \"pos\": [%.5f, %.5f, %.5f], "
                  "\"fovDeg\": {\"l\": %.3f, \"r\": %.3f, \"u\": %.3f, \"d\": %.3f}}%s\n",
                  eye == 0 ? "left" : "right", eyes[eye].p.x, eyes[eye].p.y, eyes[eye].p.z,
                  rad2deg(rig.fov[eye].angleLeft), rad2deg(rig.fov[eye].angleRight),
                  rad2deg(rig.fov[eye].angleUp), rad2deg(rig.fov[eye].angleDown),
                  eye == 0 ? "," : "");
        out += buffer;
    }
    out += "  ],\n";
    sprintf_s(buffer,
              "  \"controls\": {\"a\": %s, \"b\": %s, \"x\": %s, \"y\": %s, "
              "\"menu\": %s, \"trigL\": %.3f, \"trigR\": %.3f, \"gripL\": %.3f, "
              "\"gripR\": %.3f, \"stickL\": [%.3f, %.3f], \"stickR\": [%.3f, %.3f]},\n",
              rig.btnA ? "true" : "false", rig.btnB ? "true" : "false",
              rig.btnX ? "true" : "false", rig.btnY ? "true" : "false",
              rig.menu ? "true" : "false", rig.trigger[0], rig.trigger[1],
              rig.squeeze[0], rig.squeeze[1], rig.stick[0][0], rig.stick[0][1],
              rig.stick[1][0], rig.stick[1][1]);
    out += buffer;
    sprintf_s(buffer, "  \"layerCount\": %u,\n  \"layers\": [\n", sub.layerCount);
    out += buffer;
    for (uint32_t i = 0; i < sub.layerCount; ++i) {
        const SimLayer& layer = sub.layers[i];
        const bool projection = layer.type == XR_TYPE_COMPOSITION_LAYER_PROJECTION;
        SimSpace* space = space_get(layer.space);
        const char* spaceName = "unknown";
        if (space) {
            spaceName = space->isAction ? "action"
                : (space->refType == XR_REFERENCE_SPACE_TYPE_VIEW ? "view"
                   : space->refType == XR_REFERENCE_SPACE_TYPE_STAGE ? "stage" : "local");
        }
        if (projection) {
            sprintf_s(buffer,
                      "    {\"i\": %u, \"type\": \"projection\", \"space\": \"%s\", "
                      "\"viewCount\": %u, \"pixelsCoveredL\": %u, \"pixelsCoveredR\": %u}%s\n",
                      i, spaceName, layer.viewCount, stats[i].pixelsCovered[0],
                      stats[i].pixelsCovered[1], i + 1 < sub.layerCount ? "," : "");
        } else {
            sprintf_s(buffer,
                      "    {\"i\": %u, \"type\": \"quad\", \"space\": \"%s\", "
                      "\"sizeM\": [%.4f, %.4f], \"pixelsCoveredL\": %u, "
                      "\"pixelsCoveredR\": %u}%s\n",
                      i, spaceName, layer.size.width, layer.size.height,
                      stats[i].pixelsCovered[0], stats[i].pixelsCovered[1],
                      i + 1 < sub.layerCount ? "," : "");
        }
        out += buffer;
    }
    out += "  ],\n";

    double claimTanH = 0.0;
    for (uint32_t i = 0; i < sub.layerCount; ++i) {
        if (sub.layers[i].type == XR_TYPE_COMPOSITION_LAYER_PROJECTION &&
            sub.layers[i].viewCount > 0) {
            const XrFovf& fov = sub.layers[i].views[0].fov;
            claimTanH = (tan(-fov.angleLeft) + tan(fov.angleRight)) * 0.5;
            break;
        }
    }
    const double eyeTanH =
        (tan(-rig.fov[0].angleLeft) + tan(rig.fov[0].angleRight)) * 0.5;
    sprintf_s(buffer,
              "  \"derived\": {\"eyeSeparationM\": %.6f, \"ipdM\": %.6f, "
              "\"claimTanH\": %.5f, \"eyeTanH\": %.5f, \"claimRatioH\": %.5f, "
              "\"aimRayDots\": 0, \"aimRayMaxDevDeg\": 0.0, \"aimRayMeanDevDeg\": 0.0},\n",
              eyeSeparation, rig.ipdM, claimTanH, eyeTanH,
              eyeTanH > 0.0 ? claimTanH / eyeTanH : 0.0);
    out += buffer;
    sprintf_s(buffer,
              "  \"stats\": {\"meanLumaL\": %.2f, \"meanLumaR\": %.2f, "
              "\"nonBlackPctL\": %.2f, \"nonBlackPctR\": %.2f}\n}\n",
              meanLuma[0], meanLuma[1], nonBlackPercent[0], nonBlackPercent[1]);
    out += buffer;
    return out;
}

} // namespace

uint32_t compositor_last_layer_count() { return g_lastLayerCount.load(); }
uint32_t compositor_last_projection_views() { return g_lastProjViews.load(); }

void compositor_init(HDC deviceContext, HGLRC glContext) {
    if (g_ready) return;
    g_deviceContext = deviceContext;
    g_glContext = glContext;
    g_disabled = false;
    g_encodeRunning.store(true);
    g_encodeThread = std::thread(encode_thread_proc);
    g_ready = true;
    XRSIM_LOG("xrsim: OpenGL capture compositor ready hdc=%p hglrc=%p",
              static_cast<void*>(deviceContext), static_cast<void*>(glContext));
}

void compositor_shutdown() {
    if (g_encodeRunning.exchange(false)) {
        g_queueCv.notify_all();
        if (g_encodeThread.joinable()) g_encodeThread.join();
    }
    {
        std::lock_guard<std::mutex> lock(g_queueMutex);
        g_queue.clear();
    }
    g_ready = false;
    g_disabled = false;
    g_deviceContext = nullptr;
    g_glContext = nullptr;
}

void compositor_note_layers(const SimSubmission& sub) {
    g_lastLayerCount.store(sub.layerCount);
    uint32_t projectionViews = 0;
    for (uint32_t i = 0; i < sub.layerCount; ++i) {
        if (sub.layers[i].type == XR_TYPE_COMPOSITION_LAYER_PROJECTION)
            projectionViews = sub.layers[i].viewCount;
    }
    g_lastProjViews.store(projectionViews);
}

void compositor_on_end_frame(const SimSubmission& sub, bool capture) {
    if (!capture || !g_ready || g_disabled) return;
    if (!g_glContext || wglGetCurrentContext() != g_glContext) {
        XRSIM_LOG_ONCE("xrsim: capture skipped - xrEndFrame has no matching WGL context");
        return;
    }

    const uint32_t width = std::max<uint32_t>(64, g.captureWidth.load());
    const uint32_t height = std::max<uint32_t>(64, g.captureHeight.load());
    EncodeJob job;
    job.width = width;
    job.height = height;
    std::vector<LayerStat> stats(sub.layerCount);
    std::map<GLuint, TexturePixels> textureCache;

    GlReadbackScope glScope;
    for (int eye = 0; eye < 2; ++eye) {
        job.pixels[eye].assign(static_cast<size_t>(width) * height * 4, 0);
        for (uint32_t i = 0; i < sub.layerCount; ++i) {
            const SimLayer& layer = sub.layers[i];
            if (layer.type == XR_TYPE_COMPOSITION_LAYER_PROJECTION) {
                compose_projection(eye, layer, width, height, textureCache,
                                   job.pixels[eye], stats[i]);
            } else if (layer.type == XR_TYPE_COMPOSITION_LAYER_QUAD &&
                       g.captureLayers.load()) {
                compose_quad(eye, sub, layer, width, height, textureCache,
                             job.pixels[eye], stats[i]);
            }
        }
    }

    double meanLuma[2] = {0.0, 0.0};
    double nonBlack[2] = {0.0, 0.0};
    for (int eye = 0; eye < 2; ++eye)
        calculate_image_stats(job.pixels[eye], width, height, meanLuma[eye], nonBlack[eye]);

    char baseName[128];
    if (g.captureTag[0])
        sprintf_s(baseName, "%s", g.captureTag);
    else
        sprintf_s(baseName, "f%06llu", static_cast<unsigned long long>(sub.frameIndex));
    job.baseName = baseName;
    job.json = build_json(sub, stats, baseName, width, height, meanLuma, nonBlack);

    {
        std::lock_guard<std::mutex> lock(g_queueMutex);
        if (g_queue.size() >= 4) g_queue.pop_front();
        g_queue.push_back(std::move(job));
    }
    g_queueCv.notify_one();
}

} // namespace xrsim
