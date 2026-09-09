#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_OPENGL

#include <windows.h>
#include <unknwn.h>
#include <GL/gl.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {

struct WindowGl {
    HWND window = nullptr;
    HDC dc = nullptr;
    HGLRC rc = nullptr;
};

bool CreateWindowGl(WindowGl& out) {
    WNDCLASSW wc{};
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"SOMAVR_XRSIM_SMOKE";
    RegisterClassW(&wc);
    out.window = CreateWindowW(wc.lpszClassName, L"SOMAVR xrsim smoke",
                               WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                               640, 480, nullptr, nullptr, wc.hInstance, nullptr);
    if (!out.window) return false;
    out.dc = GetDC(out.window);
    PIXELFORMATDESCRIPTOR pfd{};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.iLayerType = PFD_MAIN_PLANE;
    const int pixelFormat = ChoosePixelFormat(out.dc, &pfd);
    if (!pixelFormat || !SetPixelFormat(out.dc, pixelFormat, &pfd)) return false;
    out.rc = wglCreateContext(out.dc);
    return out.rc && wglMakeCurrent(out.dc, out.rc);
}

void DestroyWindowGl(WindowGl& gl) {
    wglMakeCurrent(nullptr, nullptr);
    if (gl.rc) wglDeleteContext(gl.rc);
    if (gl.dc && gl.window) ReleaseDC(gl.window, gl.dc);
    if (gl.window) DestroyWindow(gl.window);
}

bool Check(XrResult result, const char* operation) {
    if (XR_SUCCEEDED(result)) return true;
    std::fprintf(stderr, "%s failed: %d\n", operation, static_cast<int>(result));
    return false;
}

void FillTexture(GLuint texture, uint32_t width, uint32_t height, int eye, uint64_t frame) {
    std::vector<uint8_t> pixels(static_cast<size_t>(width) * height * 4);
    const int shift = static_cast<int>((frame / 2) % 64);
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            const size_t i = (static_cast<size_t>(y) * width + x) * 4;
            const bool grid = ((x + shift) / 32 + y / 32) % 2 == 0;
            pixels[i + 0] = static_cast<uint8_t>(eye == 0 ? (grid ? 220 : 35) : 25);
            pixels[i + 1] = static_cast<uint8_t>(grid ? 80 : 25);
            pixels[i + 2] = static_cast<uint8_t>(eye == 1 ? (grid ? 220 : 35) : 25);
            pixels[i + 3] = 255;
        }
    }
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, static_cast<GLsizei>(width),
                    static_cast<GLsizei>(height), GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
}

} // namespace

int main(int argc, char** argv) {
    uint32_t frameLimit = 360;
    bool requireMenuEdge = false;
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::strcmp(argv[i], "--frames") == 0)
            frameLimit = static_cast<uint32_t>(std::max(1, std::atoi(argv[++i])));
    }
    for (int i = 1; i < argc; ++i)
        if (std::strcmp(argv[i], "--require-menu-edge") == 0) requireMenuEdge = true;

    WindowGl gl;
    if (!CreateWindowGl(gl)) {
        std::fprintf(stderr, "WGL setup failed\n");
        return 2;
    }

    const char* extensions[] = {XR_KHR_OPENGL_ENABLE_EXTENSION_NAME};
    XrInstanceCreateInfo instanceInfo{XR_TYPE_INSTANCE_CREATE_INFO};
    std::strcpy(instanceInfo.applicationInfo.applicationName, "SOMAVR xrsim smoke");
    std::strcpy(instanceInfo.applicationInfo.engineName, "SOMAVR test");
    instanceInfo.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;
    instanceInfo.enabledExtensionCount = 1;
    instanceInfo.enabledExtensionNames = extensions;
    XrInstance instance = XR_NULL_HANDLE;
    if (!Check(xrCreateInstance(&instanceInfo, &instance), "xrCreateInstance")) {
        DestroyWindowGl(gl);
        return 3;
    }

    XrInstanceProperties instanceProps{XR_TYPE_INSTANCE_PROPERTIES};
    if (!Check(xrGetInstanceProperties(instance, &instanceProps), "xrGetInstanceProperties"))
        return 4;
    std::printf("runtime: %s\n", instanceProps.runtimeName);

    XrSystemGetInfo systemInfo{XR_TYPE_SYSTEM_GET_INFO};
    systemInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    XrSystemId systemId = XR_NULL_SYSTEM_ID;
    if (!Check(xrGetSystem(instance, &systemInfo, &systemId), "xrGetSystem")) return 5;

    XrPath leftHand = XR_NULL_PATH;
    XrPath menuClick = XR_NULL_PATH;
    XrPath touchProfile = XR_NULL_PATH;
    if (!Check(xrStringToPath(instance, "/user/hand/left", &leftHand),
               "xrStringToPath(left hand)")
        || !Check(xrStringToPath(instance, "/user/hand/left/input/menu/click", &menuClick),
                  "xrStringToPath(menu)")
        || !Check(xrStringToPath(instance, "/interaction_profiles/oculus/touch_controller",
                                &touchProfile),
                  "xrStringToPath(touch profile)"))
        return 6;

    XrActionSetCreateInfo actionSetInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
    std::strcpy(actionSetInfo.actionSetName, "xrsim_smoke");
    std::strcpy(actionSetInfo.localizedActionSetName, "xrsim smoke");
    XrActionSet actionSet = XR_NULL_HANDLE;
    if (!Check(xrCreateActionSet(instance, &actionSetInfo, &actionSet), "xrCreateActionSet"))
        return 6;
    XrActionCreateInfo actionInfo{XR_TYPE_ACTION_CREATE_INFO};
    actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
    std::strcpy(actionInfo.actionName, "menu");
    std::strcpy(actionInfo.localizedActionName, "Menu");
    actionInfo.countSubactionPaths = 1;
    actionInfo.subactionPaths = &leftHand;
    XrAction menuAction = XR_NULL_HANDLE;
    if (!Check(xrCreateAction(actionSet, &actionInfo, &menuAction), "xrCreateAction")) return 6;
    XrActionSuggestedBinding menuBinding{menuAction, menuClick};
    XrInteractionProfileSuggestedBinding suggested{
        XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
    suggested.interactionProfile = touchProfile;
    suggested.countSuggestedBindings = 1;
    suggested.suggestedBindings = &menuBinding;
    if (!Check(xrSuggestInteractionProfileBindings(instance, &suggested),
               "xrSuggestInteractionProfileBindings"))
        return 6;

    PFN_xrGetOpenGLGraphicsRequirementsKHR getRequirements = nullptr;
    if (!Check(xrGetInstanceProcAddr(
                   instance, "xrGetOpenGLGraphicsRequirementsKHR",
                   reinterpret_cast<PFN_xrVoidFunction*>(&getRequirements)),
               "xrGetInstanceProcAddr(OpenGLRequirements)"))
        return 6;
    XrGraphicsRequirementsOpenGLKHR requirements{XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR};
    if (!Check(getRequirements(instance, systemId, &requirements),
               "xrGetOpenGLGraphicsRequirementsKHR"))
        return 7;

    XrGraphicsBindingOpenGLWin32KHR binding{XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR};
    binding.hDC = gl.dc;
    binding.hGLRC = gl.rc;
    XrSessionCreateInfo sessionInfo{XR_TYPE_SESSION_CREATE_INFO};
    sessionInfo.next = &binding;
    sessionInfo.systemId = systemId;
    XrSession session = XR_NULL_HANDLE;
    if (!Check(xrCreateSession(instance, &sessionInfo, &session), "xrCreateSession")) return 8;
    XrSessionActionSetsAttachInfo attachInfo{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
    attachInfo.countActionSets = 1;
    attachInfo.actionSets = &actionSet;
    if (!Check(xrAttachSessionActionSets(session, &attachInfo), "xrAttachSessionActionSets"))
        return 8;

    XrReferenceSpaceCreateInfo spaceInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    spaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    spaceInfo.poseInReferenceSpace.orientation.w = 1.0f;
    XrSpace appSpace = XR_NULL_HANDLE;
    if (!Check(xrCreateReferenceSpace(session, &spaceInfo, &appSpace),
               "xrCreateReferenceSpace"))
        return 9;

    constexpr uint32_t kWidth = 512;
    constexpr uint32_t kHeight = 512;
    std::array<XrSwapchain, 2> swapchains{};
    std::array<std::vector<XrSwapchainImageOpenGLKHR>, 2> images;
    for (int eye = 0; eye < 2; ++eye) {
        XrSwapchainCreateInfo swapchainInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO};
        swapchainInfo.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT |
                                   XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
        swapchainInfo.format = 0x8058; // GL_RGBA8
        swapchainInfo.sampleCount = 1;
        swapchainInfo.width = kWidth;
        swapchainInfo.height = kHeight;
        swapchainInfo.faceCount = 1;
        swapchainInfo.arraySize = 1;
        swapchainInfo.mipCount = 1;
        if (!Check(xrCreateSwapchain(session, &swapchainInfo, &swapchains[eye]),
                   "xrCreateSwapchain"))
            return 10;
        uint32_t count = 0;
        xrEnumerateSwapchainImages(swapchains[eye], 0, &count, nullptr);
        images[eye].assign(count, {XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR});
        if (!Check(xrEnumerateSwapchainImages(
                       swapchains[eye], count, &count,
                       reinterpret_cast<XrSwapchainImageBaseHeader*>(images[eye].data())),
                   "xrEnumerateSwapchainImages"))
            return 11;
    }

    bool running = false;
    bool sawMenuEdge = false;
    uint64_t submitted = 0;
    for (uint32_t loop = 0; loop < frameLimit + 240 && submitted < frameLimit; ++loop) {
        XrEventDataBuffer event{XR_TYPE_EVENT_DATA_BUFFER};
        while (xrPollEvent(instance, &event) == XR_SUCCESS) {
            if (event.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
                const auto* changed = reinterpret_cast<const XrEventDataSessionStateChanged*>(&event);
                std::printf("session: %d\n", static_cast<int>(changed->state));
                if (changed->state == XR_SESSION_STATE_READY && !running) {
                    XrSessionBeginInfo begin{XR_TYPE_SESSION_BEGIN_INFO};
                    begin.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                    if (!Check(xrBeginSession(session, &begin), "xrBeginSession")) return 12;
                    running = true;
                } else if (changed->state == XR_SESSION_STATE_STOPPING && running) {
                    xrEndSession(session);
                    running = false;
                }
            }
            event = {XR_TYPE_EVENT_DATA_BUFFER};
        }
        if (!running) {
            Sleep(10);
            continue;
        }

        XrActiveActionSet activeSet{actionSet, XR_NULL_PATH};
        XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
        syncInfo.countActiveActionSets = 1;
        syncInfo.activeActionSets = &activeSet;
        const XrResult syncResult = xrSyncActions(session, &syncInfo);
        if (XR_FAILED(syncResult)) return 13;
        XrActionStateGetInfo stateInfo{XR_TYPE_ACTION_STATE_GET_INFO};
        stateInfo.action = menuAction;
        stateInfo.subactionPath = leftHand;
        XrActionStateBoolean menuState{XR_TYPE_ACTION_STATE_BOOLEAN};
        if (!Check(xrGetActionStateBoolean(session, &stateInfo, &menuState),
                   "xrGetActionStateBoolean"))
            return 13;
        if (menuState.isActive && menuState.currentState && menuState.changedSinceLastSync)
            sawMenuEdge = true;

        XrFrameWaitInfo waitInfo{XR_TYPE_FRAME_WAIT_INFO};
        XrFrameState frameState{XR_TYPE_FRAME_STATE};
        if (!Check(xrWaitFrame(session, &waitInfo, &frameState), "xrWaitFrame")) return 13;
        XrFrameBeginInfo beginInfo{XR_TYPE_FRAME_BEGIN_INFO};
        if (!Check(xrBeginFrame(session, &beginInfo), "xrBeginFrame")) return 14;

        std::array<XrView, 2> views{{{XR_TYPE_VIEW}, {XR_TYPE_VIEW}}};
        XrViewState viewState{XR_TYPE_VIEW_STATE};
        XrViewLocateInfo locateInfo{XR_TYPE_VIEW_LOCATE_INFO};
        locateInfo.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        locateInfo.displayTime = frameState.predictedDisplayTime;
        locateInfo.space = appSpace;
        uint32_t viewCount = 0;
        if (!Check(xrLocateViews(session, &locateInfo, &viewState, 2, &viewCount, views.data()),
                   "xrLocateViews"))
            return 15;

        std::array<XrCompositionLayerProjectionView, 2> projectionViews{};
        for (int eye = 0; eye < 2; ++eye) {
            XrSwapchainImageAcquireInfo acquire{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
            uint32_t imageIndex = 0;
            xrAcquireSwapchainImage(swapchains[eye], &acquire, &imageIndex);
            XrSwapchainImageWaitInfo imageWait{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
            imageWait.timeout = 1000000000LL;
            xrWaitSwapchainImage(swapchains[eye], &imageWait);
            FillTexture(images[eye][imageIndex].image, kWidth, kHeight, eye, submitted);
            XrSwapchainImageReleaseInfo release{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
            xrReleaseSwapchainImage(swapchains[eye], &release);

            projectionViews[eye] = {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW};
            projectionViews[eye].pose = views[eye].pose;
            projectionViews[eye].fov = views[eye].fov;
            projectionViews[eye].subImage.swapchain = swapchains[eye];
            projectionViews[eye].subImage.imageRect.extent = {
                static_cast<int32_t>(kWidth), static_cast<int32_t>(kHeight)};
        }
        XrCompositionLayerProjection projection{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
        projection.space = appSpace;
        projection.viewCount = 2;
        projection.views = projectionViews.data();
        const XrCompositionLayerBaseHeader* layers[] = {
            reinterpret_cast<const XrCompositionLayerBaseHeader*>(&projection)};
        XrFrameEndInfo endInfo{XR_TYPE_FRAME_END_INFO};
        endInfo.displayTime = frameState.predictedDisplayTime;
        endInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
        endInfo.layerCount = frameState.shouldRender ? 1u : 0u;
        endInfo.layers = frameState.shouldRender ? layers : nullptr;
        if (!Check(xrEndFrame(session, &endInfo), "xrEndFrame")) return 16;
        ++submitted;
    }

    std::printf("frames: %llu\n", static_cast<unsigned long long>(submitted));
    std::printf("menu-edge: %s\n", sawMenuEdge ? "yes" : "no");
    for (XrSwapchain swapchain : swapchains) xrDestroySwapchain(swapchain);
    xrDestroySpace(appSpace);
    if (running) xrEndSession(session);
    xrDestroySession(session);
    xrDestroyActionSet(actionSet);
    xrDestroyInstance(instance);
    DestroyWindowGl(gl);
    if (submitted != frameLimit) return 17;
    return requireMenuEdge && !sawMenuEdge ? 18 : 0;
}
