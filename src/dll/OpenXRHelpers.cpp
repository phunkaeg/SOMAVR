#include "OpenXRHelpers.h"

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <sstream>

namespace somavr::xr_helpers {
namespace {

const char* GlFormatName(int64_t format)
{
    switch (format) {
    case 0x1908: return "GL_RGBA";
    case 0x8058: return "GL_RGBA8";
    case 0x8059: return "GL_RGB10_A2";
    case 0x8814: return "GL_RGBA32F";
    case 0x881A: return "GL_RGBA16F";
    case 0x8C43: return "GL_SRGB8_ALPHA8";
    case 0x81A5: return "GL_DEPTH_COMPONENT16";
    case 0x81A6: return "GL_DEPTH_COMPONENT24";
    case 0x81A7: return "GL_DEPTH_COMPONENT32";
    case 0x8CAC: return "GL_DEPTH_COMPONENT32F";
    case 0x88F0: return "GL_DEPTH24_STENCIL8";
    case 0x8CAD: return "GL_DEPTH32F_STENCIL8";
    default: return "";
    }
}

} // namespace

std::string XrResultString(XrResult result)
{
    switch (result) {
    case XR_SUCCESS: return "XR_SUCCESS";
    case XR_TIMEOUT_EXPIRED: return "XR_TIMEOUT_EXPIRED";
    case XR_SESSION_LOSS_PENDING: return "XR_SESSION_LOSS_PENDING";
    case XR_EVENT_UNAVAILABLE: return "XR_EVENT_UNAVAILABLE";
    case XR_SPACE_BOUNDS_UNAVAILABLE: return "XR_SPACE_BOUNDS_UNAVAILABLE";
    case XR_ERROR_VALIDATION_FAILURE: return "XR_ERROR_VALIDATION_FAILURE";
    case XR_ERROR_RUNTIME_FAILURE: return "XR_ERROR_RUNTIME_FAILURE";
    case XR_ERROR_OUT_OF_MEMORY: return "XR_ERROR_OUT_OF_MEMORY";
    case XR_ERROR_API_VERSION_UNSUPPORTED: return "XR_ERROR_API_VERSION_UNSUPPORTED";
    case XR_ERROR_INITIALIZATION_FAILED: return "XR_ERROR_INITIALIZATION_FAILED";
    case XR_ERROR_FUNCTION_UNSUPPORTED: return "XR_ERROR_FUNCTION_UNSUPPORTED";
    case XR_ERROR_FEATURE_UNSUPPORTED: return "XR_ERROR_FEATURE_UNSUPPORTED";
    case XR_ERROR_EXTENSION_NOT_PRESENT: return "XR_ERROR_EXTENSION_NOT_PRESENT";
    case XR_ERROR_LIMIT_REACHED: return "XR_ERROR_LIMIT_REACHED";
    case XR_ERROR_SIZE_INSUFFICIENT: return "XR_ERROR_SIZE_INSUFFICIENT";
    case XR_ERROR_HANDLE_INVALID: return "XR_ERROR_HANDLE_INVALID";
    case XR_ERROR_INSTANCE_LOST: return "XR_ERROR_INSTANCE_LOST";
    case XR_ERROR_SESSION_RUNNING: return "XR_ERROR_SESSION_RUNNING";
    case XR_ERROR_SESSION_NOT_RUNNING: return "XR_ERROR_SESSION_NOT_RUNNING";
    case XR_ERROR_SESSION_LOST: return "XR_ERROR_SESSION_LOST";
    case XR_ERROR_RUNTIME_UNAVAILABLE: return "XR_ERROR_RUNTIME_UNAVAILABLE";
    case XR_ERROR_FORM_FACTOR_UNSUPPORTED: return "XR_ERROR_FORM_FACTOR_UNSUPPORTED";
    case XR_ERROR_FORM_FACTOR_UNAVAILABLE: return "XR_ERROR_FORM_FACTOR_UNAVAILABLE";
    case XR_ERROR_SYSTEM_INVALID: return "XR_ERROR_SYSTEM_INVALID";
    case XR_ERROR_VIEW_CONFIGURATION_TYPE_UNSUPPORTED: return "XR_ERROR_VIEW_CONFIGURATION_TYPE_UNSUPPORTED";
    case XR_ERROR_ENVIRONMENT_BLEND_MODE_UNSUPPORTED: return "XR_ERROR_ENVIRONMENT_BLEND_MODE_UNSUPPORTED";
    case XR_ERROR_NAME_DUPLICATED: return "XR_ERROR_NAME_DUPLICATED";
    case XR_ERROR_NAME_INVALID: return "XR_ERROR_NAME_INVALID";
    case XR_ERROR_SWAPCHAIN_FORMAT_UNSUPPORTED: return "XR_ERROR_SWAPCHAIN_FORMAT_UNSUPPORTED";
    default: break;
    }

    std::ostringstream oss;
    oss << "XR_RESULT(" << static_cast<int>(result) << ")";
    return oss.str();
}

std::string XrVersionString(XrVersion version)
{
    std::ostringstream oss;
    oss << XR_VERSION_MAJOR(version) << '.'
        << XR_VERSION_MINOR(version) << '.'
        << XR_VERSION_PATCH(version);
    return oss.str();
}

const char* ViewConfigurationTypeName(XrViewConfigurationType type)
{
    switch (type) {
    case XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO: return "PRIMARY_MONO";
    case XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO: return "PRIMARY_STEREO";
    default: return "UNKNOWN";
    }
}

const char* EnvironmentBlendModeName(XrEnvironmentBlendMode mode)
{
    switch (mode) {
    case XR_ENVIRONMENT_BLEND_MODE_OPAQUE: return "OPAQUE";
    case XR_ENVIRONMENT_BLEND_MODE_ADDITIVE: return "ADDITIVE";
    case XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND: return "ALPHA_BLEND";
    default: return "UNKNOWN";
    }
}

const char* ReferenceSpaceTypeName(XrReferenceSpaceType type)
{
    switch (type) {
    case XR_REFERENCE_SPACE_TYPE_VIEW: return "VIEW";
    case XR_REFERENCE_SPACE_TYPE_LOCAL: return "LOCAL";
    case XR_REFERENCE_SPACE_TYPE_STAGE: return "STAGE";
    default: return "UNKNOWN";
    }
}

const char* SessionStateName(XrSessionState state)
{
    switch (state) {
    case XR_SESSION_STATE_UNKNOWN: return "UNKNOWN";
    case XR_SESSION_STATE_IDLE: return "IDLE";
    case XR_SESSION_STATE_READY: return "READY";
    case XR_SESSION_STATE_SYNCHRONIZED: return "SYNCHRONIZED";
    case XR_SESSION_STATE_VISIBLE: return "VISIBLE";
    case XR_SESSION_STATE_FOCUSED: return "FOCUSED";
    case XR_SESSION_STATE_STOPPING: return "STOPPING";
    case XR_SESSION_STATE_LOSS_PENDING: return "LOSS_PENDING";
    case XR_SESSION_STATE_EXITING: return "EXITING";
    default: return "UNKNOWN";
    }
}

std::string GlFormatString(int64_t format)
{
    std::ostringstream oss;
    oss << "0x" << std::hex << std::uppercase << static_cast<unsigned long long>(format);
    const char* name = GlFormatName(format);
    if (name[0] != '\0') {
        oss << '(' << name << ')';
    }
    return oss.str();
}

bool ExtensionPresent(const std::vector<XrExtensionProperties>& extensions, const char* name)
{
    return std::any_of(extensions.begin(), extensions.end(), [&](const XrExtensionProperties& properties) {
        return std::strcmp(properties.extensionName, name) == 0;
    });
}

std::string ExtensionSample(const std::vector<XrExtensionProperties>& extensions)
{
    std::ostringstream oss;
    const size_t limit = std::min<size_t>(extensions.size(), 12);
    for (size_t i = 0; i < limit; ++i) {
        if (i != 0) {
            oss << ',';
        }
        oss << extensions[i].extensionName;
    }
    if (extensions.size() > limit) {
        oss << ",...";
    }
    return oss.str();
}

std::string SwapchainFormatSample(const std::vector<int64_t>& formats)
{
    std::ostringstream oss;
    const size_t limit = std::min<size_t>(formats.size(), 16);
    for (size_t i = 0; i < limit; ++i) {
        if (i != 0) {
            oss << ',';
        }
        oss << GlFormatString(formats[i]);
    }
    if (formats.size() > limit) {
        oss << ",...";
    }
    return oss.str();
}

OpenXREyeView ToEyeView(const XrView& view, uint64_t gameFrame)
{
    OpenXREyeView result;
    result.valid = true;
    result.gameFrame = gameFrame;
    result.positionX = view.pose.position.x;
    result.positionY = view.pose.position.y;
    result.positionZ = view.pose.position.z;
    result.orientationX = view.pose.orientation.x;
    result.orientationY = view.pose.orientation.y;
    result.orientationZ = view.pose.orientation.z;
    result.orientationW = view.pose.orientation.w;
    result.angleLeft = view.fov.angleLeft;
    result.angleRight = view.fov.angleRight;
    result.angleUp = view.fov.angleUp;
    result.angleDown = view.fov.angleDown;
    return result;
}

XrPosef ToXrPose(const OpenXREyeView& view)
{
    XrPosef pose{};
    pose.position = {view.positionX, view.positionY, view.positionZ};
    pose.orientation = {
        view.orientationX,
        view.orientationY,
        view.orientationZ,
        view.orientationW,
    };
    return pose;
}

XrFovf ToXrFov(const OpenXREyeView& view)
{
    return {view.angleLeft, view.angleRight, view.angleUp, view.angleDown};
}

} // namespace somavr::xr_helpers
