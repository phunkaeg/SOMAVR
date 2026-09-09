#include "OpenXRGLBridge.h"
#include "OpenGLOwnership.h"
#include "Logger.h"
#include <gl/GL.h>
#include <cmath>
#include <iostream>

// No XR instance/session is created: exercise production copies in an owned,
// hidden 64x64 WGL context. These unrelated terminal paths are never called.
namespace somavr {
void BeginTerminalColorClearSuppression(uint64_t, uint32_t) {}
void EndTerminalColorClearSuppression() {}
void BeginTerminalCaptureGuard(uint64_t, uint32_t, int, int, int, int, int, int) {}
void EndTerminalCaptureGuard() {}

struct SceneDepthTestAccess {
    static bool Initialize(OpenXRGLBridge& bridge)
    {
        if (!bridge.ResolveFunctions()) return false;
        bridge.session_ = reinterpret_cast<XrSession>(1);
        bridge.depthCaptureProbeEnabled_ = true;
        bridge.eyes_.resize(2);
        for (uint32_t i = 0; i < 2; ++i) {
            auto& eye = bridge.eyes_[i];
            eye.width = eye.height = 64;
            eye.format = 0x8058;
            if (!bridge.CreateEyeCache(eye, i)) return false;
        }
        return true;
    }
    static void Finish(OpenXRGLBridge& bridge)
    {
        bridge.session_ = XR_NULL_HANDLE;
        bridge.Shutdown();
    }
};
}

namespace {
template<class Fn> Fn Proc(const char* name) { return reinterpret_cast<Fn>(wglGetProcAddress(name)); }
using Bind = void(APIENTRY*)(GLenum, GLuint);
using Gen = void(APIENTRY*)(GLsizei, GLuint*);
using Storage = void(APIENTRY*)(GLenum, GLenum, GLsizei, GLsizei);
using Attach = void(APIENTRY*)(GLenum, GLenum, GLenum, GLuint);
int Check(bool ok, const char* message)
{
    if (!ok) std::cerr << "FAILED: " << message << '\n';
    return ok ? 0 : 1;
}
}

int main()
{
    using namespace somavr;
    WNDCLASSW wc{};
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"SOMAVR_DEPTH_TEST";
    RegisterClassW(&wc);
    HWND window = CreateWindowW(wc.lpszClassName, L"SOMAVR depth test", WS_POPUP,
        0, 0, 64, 64, nullptr, nullptr, wc.hInstance, nullptr);
    HDC dc = window ? GetDC(window) : nullptr;
    PIXELFORMATDESCRIPTOR pfd{};
    pfd.nSize = sizeof(pfd); pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA; pfd.cColorBits = 32; pfd.cDepthBits = 24; pfd.cStencilBits = 8;
    const int format = dc ? ChoosePixelFormat(dc, &pfd) : 0;
    if (!format || !SetPixelFormat(dc, format, &pfd)) return 77;
    HGLRC context = wglCreateContext(dc);
    if (!context || !wglMakeCurrent(dc, context)) return 77;
    InitializeWorkRoot(GetModuleHandleW(nullptr));
    Logger::Instance().Initialize(LogPath().parent_path() / "scene-depth-tests.log", LogLevel::Info);
    OpenXRGLBridge bridge;
    if (!SceneDepthTestAccess::Initialize(bridge)) return 77;
    auto bindFbo = Proc<Bind>("glBindFramebuffer");
    auto genFbo = Proc<Gen>("glGenFramebuffers");
    auto genRb = Proc<Gen>("glGenRenderbuffers");
    auto bindRb = Proc<Bind>("glBindRenderbuffer");
    auto storage = Proc<Storage>("glRenderbufferStorage");
    auto attach = Proc<Attach>("glFramebufferRenderbuffer");
    auto genBuffers = Proc<Gen>("glGenBuffers");
    auto bindBuffer = Proc<Bind>("glBindBuffer");
    if (!bindFbo || !genFbo || !genRb || !bindRb || !storage || !attach || !genBuffers || !bindBuffer) return 77;
    GLuint fbo = 0, rb = 0, pbo = 0;
    genFbo(1, &fbo); genRb(1, &rb); genBuffers(1, &pbo);
    bindRb(0x8D41, rb); storage(0x8D41, 0x88F0, 64, 64);
    bindFbo(0x8D40, fbo); attach(0x8D40, 0x821A, 0x8D41, rb);
    glReadBuffer(GL_NONE); glDrawBuffer(GL_NONE); glViewport(0, 0, 64, 64);
    glDepthMask(GL_TRUE); glDisable(GL_SCISSOR_TEST); glClearDepth(.8); glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_SCISSOR_TEST); glScissor(0, 0, 32, 64); glClearDepth(.2); glClear(GL_DEPTH_BUFFER_BIT);
    glScissor(3, 4, 1, 1); bindFbo(0x8CA8, 0);
    depth_math::CompositionDepthRange range{0, 1, .03f, 1000};
    int failures = 0;
    failures += Check(bridge.CaptureSceneDepthToCache(0, 1, 42, range), "capture real scene D24S8");
    GLint read = -1, draw = -1, rbo = -1, box[4]{};
    glGetIntegerv(0x8CAA, &read); glGetIntegerv(0x8CA6, &draw); glGetIntegerv(0x8CA7, &rbo);
    glGetIntegerv(GL_SCISSOR_BOX, box);
    failures += Check(read == 0 && draw == static_cast<GLint>(fbo) && rbo == static_cast<GLint>(rb)
        && glIsEnabled(GL_SCISSOR_TEST) && box[0] == 3 && box[1] == 4 && box[2] == 1 && box[3] == 1
        && g_ownOpenGLDepth == 0, "source bindings, scissor and own-GL scope restore");
    glDisable(GL_SCISSOR_TEST); glClearDepth(.6); glClear(GL_DEPTH_BUFFER_BIT);
    failures += Check(bridge.CaptureSceneDepthToCache(1, 2, 42, range), "second eye captures reused source");
    const auto sample = [&](uint32_t eye, int x) {
        float depth = -1;
        bindFbo(0x8CA8, bridge.Eye(eye).cacheFramebuffer);
        glReadPixels(x, 32, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);
        return depth;
    };
    failures += Check(std::fabs(sample(0, 8) - .2f) < 1e-5f
        && std::fabs(sample(0, 56) - .8f) < 1e-5f && std::fabs(sample(1, 8) - .6f) < 1e-5f,
        "eye caches preserve different depth pixels across source reuse");
    bindFbo(0x8D40, 0); glDrawBuffer(GL_BACK); glReadBuffer(GL_BACK);
    glClearDepth(1); glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
    failures += Check(bridge.CaptureBackbufferToCache(0, 1, 42) && bridge.Eye(0).depthCacheValid
        && std::fabs(sample(0, 8) - .2f) < 1e-5f, "desktop color copy cannot overwrite scene depth");
    bindBuffer(0x88EB, pbo);
    glPixelStorei(GL_PACK_ALIGNMENT, 8); glPixelStorei(GL_PACK_ROW_LENGTH, 123);
    glPixelStorei(GL_PACK_SKIP_ROWS, 2); glPixelStorei(GL_PACK_SKIP_PIXELS, 3);
    glPixelStorei(GL_PACK_SWAP_BYTES, GL_TRUE);
    glPixelTransferf(GL_DEPTH_SCALE, .5f); glPixelTransferf(GL_DEPTH_BIAS, .25f);
    failures += Check(bridge.DumpEyeDepthCache(0, 42, 1, 0, 42), "full depth diagnostic reads with hostile pack state");
    GLint restoredPbo = 0, rowLength = 0, skip = 0, swap = 0;
    glGetIntegerv(0x88ED, &restoredPbo); glGetIntegerv(GL_PACK_ROW_LENGTH, &rowLength);
    glGetIntegerv(GL_PACK_SKIP_PIXELS, &skip); glGetIntegerv(GL_PACK_SWAP_BYTES, &swap);
    failures += Check(restoredPbo == static_cast<GLint>(pbo) && rowLength == 123 && skip == 3 && swap == GL_TRUE,
        "readback restores PBO and pixel-pack layout");
    GLfloat restoredScale = 0, restoredBias = 0;
    glGetFloatv(GL_DEPTH_SCALE, &restoredScale); glGetFloatv(GL_DEPTH_BIAS, &restoredBias);
    failures += Check(restoredScale == .5f && restoredBias == .25f, "readback restores depth transfer state");
    glPixelTransferf(GL_DEPTH_SCALE, 1); glPixelTransferf(GL_DEPTH_BIAS, 0);
    bindBuffer(0x88EB, 0); glPixelStorei(GL_PACK_ROW_LENGTH, 0);
    glPixelStorei(GL_PACK_SKIP_ROWS, 0); glPixelStorei(GL_PACK_SKIP_PIXELS, 0); glPixelStorei(GL_PACK_SWAP_BYTES, 0);
    failures += Check(bridge.CaptureBackbufferToCache(0, 3, 42) && !bridge.Eye(0).depthCacheValid,
        "same pose but different render generation rejects old depth");
    bindFbo(0x8D40, 0);
    failures += Check(!bridge.CaptureSceneDepthToCache(0, 4, 42, range)
        && !bridge.Eye(0).sceneDepth.captured, "default framebuffer fails closed");
    bridge.InvalidateStereoCaches();
    failures += Check(!bridge.DepthCachesReady(), "invalidation clears depth ownership");
    failures += Check(glGetError() == GL_NO_ERROR, "no GL errors left by test");
    SceneDepthTestAccess::Finish(bridge);
    Logger::Instance().Shutdown();
    wglMakeCurrent(nullptr, nullptr); wglDeleteContext(context);
    ReleaseDC(window, dc); DestroyWindow(window);
    std::cout << "Scene depth GL failures: " << failures << '\n';
    return failures ? 1 : 0;
}
