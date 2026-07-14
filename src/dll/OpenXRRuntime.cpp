#include "OpenXRRuntime.h"

#include "Logger.h"
#include "OpenXRGLBridge.h"

#include <Windows.h>
#include <Unknwn.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

#if defined(SOMAVR_ENABLE_OPENXR)
#ifndef XR_USE_PLATFORM_WIN32
#define XR_USE_PLATFORM_WIN32
#endif
#ifndef XR_USE_GRAPHICS_API_OPENGL
#define XR_USE_GRAPHICS_API_OPENGL
#endif
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include "OpenXRHelpers.h"
#include "OpenXRInput.h"
#endif

namespace somavr {
namespace {

std::string HexPointer(const void* value)
{
    std::ostringstream oss;
    oss << "0x" << std::hex << reinterpret_cast<uintptr_t>(value);
    return oss.str();
}

#if defined(SOMAVR_ENABLE_OPENXR)

using xr_helpers::EnvironmentBlendModeName;
using xr_helpers::ExtensionPresent;
using xr_helpers::ExtensionSample;
using xr_helpers::GlFormatString;
using xr_helpers::ReferenceSpaceTypeName;
using xr_helpers::SessionStateName;
using xr_helpers::SwapchainFormatSample;
using xr_helpers::ToEyeView;
using xr_helpers::ToXrFov;
using xr_helpers::ToXrPose;
using xr_helpers::ViewConfigurationTypeName;
using xr_helpers::XrResultString;
using xr_helpers::XrVersionString;

std::wstring Win32ErrorMessage(DWORD error)
{
    wchar_t* buffer = nullptr;
    const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    const DWORD size = FormatMessageW(flags, nullptr, error, 0, reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
    if (size == 0 || buffer == nullptr) {
        return L"unknown error";
    }

    std::wstring message(buffer, size);
    LocalFree(buffer);
    while (!message.empty() && (message.back() == L'\r' || message.back() == L'\n')) {
        message.pop_back();
    }
    return message;
}

std::filesystem::path CurrentModuleDirectory()
{
    HMODULE module = nullptr;
    if (!GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&CurrentModuleDirectory),
            &module)) {
        return {};
    }

    wchar_t path[MAX_PATH] = {};
    const DWORD size = GetModuleFileNameW(module, path, MAX_PATH);
    if (size == 0 || size == MAX_PATH) {
        return {};
    }
    return std::filesystem::path(std::wstring(path, size)).parent_path();
}

#endif

} // namespace

#if defined(SOMAVR_ENABLE_OPENXR)

struct OpenXRRuntime::Impl {
    void Configure(
        bool enabled,
        bool sessionProbe,
        bool releaseAfterProbe,
        uint64_t bootstrapFrame,
        uint64_t holdFrames,
        bool manualStart,
        bool frameSubmit,
        bool mirrorBackbuffer,
        int resolutionScalePercent,
        bool inputEnabled,
        int inputLogInterval)
    {
        std::lock_guard lock(mutex_);
        enabled_ = enabled;
        sessionProbeEnabled_ = sessionProbe;
        releaseAfterProbeEnabled_ = releaseAfterProbe;
        bootstrapFrame_ = bootstrapFrame;
        holdFrames_ = holdFrames;
        manualStartEnabled_ = manualStart;
        frameSubmitEnabled_ = frameSubmit;
        mirrorBackbufferEnabled_ = mirrorBackbuffer;
        resolutionScalePercent_ = std::clamp(resolutionScalePercent, 25, 200);
        inputEnabled_ = inputEnabled;
        inputLogInterval_ = std::max(inputLogInterval, 1);
        manualStartArmed_ = false;
        manualStartLogged_ = false;
        manualStartKeyDown_ = false;
        manualStartFrame_ = 0;
        unavailableLogged_ = false;
        Logger::Instance().Write(
            LogLevel::Info,
            "openxr_config buildOpenXR=1 enabled=%d sessionProbe=%d releaseAfterProbe=%d bootstrapFrame=%llu holdFrames=%llu manualStart=%d key=F8 frameSubmit=%d mirrorBackbuffer=%d resolutionScalePercent=%d input=%d inputLogInterval=%d",
            enabled_ ? 1 : 0,
            sessionProbeEnabled_ ? 1 : 0,
            releaseAfterProbeEnabled_ ? 1 : 0,
            static_cast<unsigned long long>(bootstrapFrame_),
            static_cast<unsigned long long>(holdFrames_),
            manualStartEnabled_ ? 1 : 0,
            frameSubmitEnabled_ ? 1 : 0,
            mirrorBackbufferEnabled_ ? 1 : 0,
            resolutionScalePercent_,
            inputEnabled_ ? 1 : 0,
            inputLogInterval_);

        if (frameSubmitEnabled_ && !sessionProbeEnabled_) {
            Logger::Instance().Write(
                LogLevel::Warn,
                "openxr_config frame_submit_requires_session_probe frameSubmit=1 sessionProbe=0");
        }
    }

    void OnOpenGLContext(HDC deviceContext, HGLRC glContext)
    {
        std::lock_guard lock(mutex_);
        latestHdc_ = deviceContext;
        latestGlContext_ = glContext;
        if (!enabled_ || initialized_ || failed_ || attempted_) {
            return;
        }
        if (IsManualStartPendingLocked(0)) {
            return;
        }
        if (bootstrapFrame_ > 0) {
            LogDeferredBootstrapLocked(0);
            return;
        }
        AttemptBootstrapLocked(deviceContext, glContext, 0);
    }

    void OnFrameBoundary(HDC deviceContext, HGLRC glContext, uint64_t frameIndex)
    {
        std::lock_guard lock(mutex_);
        currentGameFrame_ = frameIndex;
        latestHdc_ = deviceContext;
        latestGlContext_ = glContext;
        if (!enabled_ || failed_) {
            return;
        }

        UpdateManualStartLocked(frameIndex);

        if (session_ != XR_NULL_HANDLE) {
            PollEventsLocked(frameIndex);
            if (releaseAfterProbeEnabled_ && releaseFrame_ != 0 && frameIndex >= releaseFrame_) {
                ReleaseRuntimeAfterProbeLocked("hold_complete");
            }
        }

        if (!initialized_ && (!attempted_ || (retryFrame_ != 0 && frameIndex >= retryFrame_))) {
            if (IsManualStartPendingLocked(frameIndex)) {
                return;
            }
            if (bootstrapFrame_ > 0 && frameIndex < bootstrapFrame_) {
                LogDeferredBootstrapLocked(frameIndex);
                return;
            }
            AttemptBootstrapLocked(deviceContext, glContext, frameIndex);
        }

        if (sessionRunning_ && frameResourcesReady_ && !frameSubmitFailed_) {
            SubmitFrameLocked(frameIndex);
        }
    }

    bool RequestManualStart()
    {
        std::lock_guard lock(mutex_);
        if (!enabled_ || failed_) {
            return false;
        }
        if (initialized_ || attempted_ || manualStartArmed_) {
            return true;
        }
        manualStartArmed_ = true;
        manualStartLogged_ = true;
        manualStartFrame_ = 0;
        Logger::Instance().Write(
            LogLevel::Info,
            "openxr_manual_start triggered source=api key=F10 frame=pending hdc=%s hglrc=%s",
            HexPointer(latestHdc_).c_str(),
            HexPointer(latestGlContext_).c_str());
        return true;
    }

    void Shutdown()
    {
        std::lock_guard lock(mutex_);
        if (session_ != XR_NULL_HANDLE) {
            input_.ShutdownSession();
            DestroyFrameResourcesLocked();
            xrDestroySession(session_);
            session_ = XR_NULL_HANDLE;
        } else {
            DestroyFrameResourcesLocked();
        }
        if (instance_ != XR_NULL_HANDLE) {
            input_.Shutdown();
            xrDestroyInstance(instance_);
            instance_ = XR_NULL_HANDLE;
        }
        initialized_ = false;
        attempted_ = false;
        sessionAttempted_ = false;
        sessionCreated_ = false;
        sessionHeldAfterProbe_ = false;
        sessionRunning_ = false;
        stereoSubmissionEnabled_ = false;
        pendingRenderedEyeValid_ = false;
        renderedStereoViewValid_[0] = false;
        renderedStereoViewValid_[1] = false;
        releaseFrame_ = 0;
    }

    std::string SummaryString() const
    {
        std::lock_guard lock(mutex_);
        std::ostringstream oss;
        oss << "openxrEnabled=" << (enabled_ ? 1 : 0)
            << " openxrBuild=1"
            << " openxrInitialized=" << (initialized_ ? 1 : 0)
            << " openxrAttempted=" << (attempted_ ? 1 : 0)
            << " openxrFailed=" << (failed_ ? 1 : 0)
            << " openxrLoaderLoaded=" << (loaderModule_ != nullptr ? 1 : 0)
            << " openxrInstanceAlive=" << (instance_ != XR_NULL_HANDLE ? 1 : 0)
            << " openxrInstanceReleasedAfterProbe=" << (instanceReleasedAfterProbe_ ? 1 : 0)
            << " openxrSessionProbe=" << (sessionProbeEnabled_ ? 1 : 0)
            << " openxrReleaseAfterProbe=" << (releaseAfterProbeEnabled_ ? 1 : 0)
            << " openxrBootstrapFrame=" << static_cast<unsigned long long>(bootstrapFrame_)
            << " openxrHoldFrames=" << static_cast<unsigned long long>(holdFrames_)
            << " openxrManualStart=" << (manualStartEnabled_ ? 1 : 0)
            << " openxrManualStartArmed=" << (manualStartArmed_ ? 1 : 0)
            << " openxrManualStartFrame=" << static_cast<unsigned long long>(manualStartFrame_)
            << " openxrFrameSubmit=" << (frameSubmitEnabled_ ? 1 : 0)
            << " openxrMirrorBackbuffer=" << (mirrorBackbufferEnabled_ ? 1 : 0)
            << " openxrResolutionScalePercent=" << resolutionScalePercent_
            << " openxrFrameResourcesReady=" << (frameResourcesReady_ ? 1 : 0)
            << " openxrFrameSubmitFailed=" << (frameSubmitFailed_ ? 1 : 0)
            << " openxrSessionRunning=" << (sessionRunning_ ? 1 : 0)
            << " openxrStereoSubmission=" << (stereoSubmissionEnabled_ ? 1 : 0)
            << " openxrStereoCapturedEyes=" << static_cast<unsigned long long>(stereoCapturedEyeCount_)
            << " openxrStereoSubmittedFrames=" << static_cast<unsigned long long>(stereoSubmittedFrameCount_)
            << " openxrSubmittedFrames=" << static_cast<unsigned long long>(submittedFrameCount_)
            << " openxrLastSubmittedGameFrame=" << static_cast<unsigned long long>(lastSubmittedGameFrame_)
            << " openxrSessionAttempted=" << (sessionAttempted_ ? 1 : 0)
            << " openxrSessionCreated=" << (sessionCreated_ ? 1 : 0)
            << " openxrSessionAlive=" << (session_ != XR_NULL_HANDLE ? 1 : 0)
            << " openxrSessionReleasedAfterProbe=" << (sessionReleasedAfterProbe_ ? 1 : 0)
            << " openxrSessionHeldAfterProbe=" << (sessionHeldAfterProbe_ ? 1 : 0)
            << " openxrReleaseFrame=" << static_cast<unsigned long long>(releaseFrame_)
            << " openxrSessionCreatedFrame=" << static_cast<unsigned long long>(sessionCreatedFrame_)
            << " openxrSessionState=" << SessionStateName(sessionState_)
            << " openxrSystemId=" << static_cast<unsigned long long>(systemId_)
            << " openxrViews=" << viewCount_
            << " openxrSwapchainFormats=" << swapchainFormatCount_
            << " openxrHdc=" << HexPointer(latestHdc_)
            << " openxrGlContext=" << HexPointer(latestGlContext_);
        oss << " " << input_.SummaryString();
        return oss.str();
    }

    std::string ViewSummaryString() const
    {
        std::lock_guard lock(mutex_);
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(5)
            << "openxrPoseValid=" << (latestPoseValid_ ? 1 : 0)
            << " openxrPoseGameFrame=" << static_cast<unsigned long long>(latestPoseGameFrame_)
            << " openxrPoseAgeFrames=" << static_cast<unsigned long long>(
                currentGameFrame_ >= latestPoseGameFrame_ ? currentGameFrame_ - latestPoseGameFrame_ : 0)
            << " openxrViewStateFlags=0x" << std::hex << static_cast<unsigned long long>(latestViewStateFlags_) << std::dec
            << " openxrIpdMeters=" << latestIpdMeters_
            << " openxrHeadPosition="
            << latestHeadPose_.position.x << ","
            << latestHeadPose_.position.y << ","
            << latestHeadPose_.position.z
            << " openxrHeadOrientation="
            << latestHeadPose_.orientation.x << ","
            << latestHeadPose_.orientation.y << ","
            << latestHeadPose_.orientation.z << ","
            << latestHeadPose_.orientation.w;
        return oss.str();
    }

    bool GetLatestHeadPose(OpenXRHeadPose& pose) const
    {
        std::lock_guard lock(mutex_);
        pose = {};
        pose.valid = latestPoseValid_;
        pose.orientationTracked = (latestViewStateFlags_ & XR_VIEW_STATE_ORIENTATION_TRACKED_BIT) != 0;
        pose.positionTracked = (latestViewStateFlags_ & XR_VIEW_STATE_POSITION_TRACKED_BIT) != 0;
        pose.gameFrame = latestPoseGameFrame_;
        pose.sampleAgeFrames = currentGameFrame_ >= latestPoseGameFrame_
            ? currentGameFrame_ - latestPoseGameFrame_
            : 0;
        pose.positionX = latestHeadPose_.position.x;
        pose.positionY = latestHeadPose_.position.y;
        pose.positionZ = latestHeadPose_.position.z;
        pose.orientationX = latestHeadPose_.orientation.x;
        pose.orientationY = latestHeadPose_.orientation.y;
        pose.orientationZ = latestHeadPose_.orientation.z;
        pose.orientationW = latestHeadPose_.orientation.w;
        return pose.valid;
    }

    bool GetLatestStereoViews(OpenXRStereoViewSnapshot& views) const
    {
        std::lock_guard lock(mutex_);
        views = {};
        if (!latestPoseValid_ || locatedViews_.size() < 2) {
            return false;
        }

        views.valid = true;
        views.gameFrame = latestPoseGameFrame_;
        views.head.valid = true;
        views.head.orientationTracked =
            (latestViewStateFlags_ & XR_VIEW_STATE_ORIENTATION_TRACKED_BIT) != 0;
        views.head.positionTracked =
            (latestViewStateFlags_ & XR_VIEW_STATE_POSITION_TRACKED_BIT) != 0;
        views.head.gameFrame = latestPoseGameFrame_;
        views.head.sampleAgeFrames = currentGameFrame_ >= latestPoseGameFrame_
            ? currentGameFrame_ - latestPoseGameFrame_
            : 0;
        views.head.positionX = latestHeadPose_.position.x;
        views.head.positionY = latestHeadPose_.position.y;
        views.head.positionZ = latestHeadPose_.position.z;
        views.head.orientationX = latestHeadPose_.orientation.x;
        views.head.orientationY = latestHeadPose_.orientation.y;
        views.head.orientationZ = latestHeadPose_.orientation.z;
        views.head.orientationW = latestHeadPose_.orientation.w;
        views.eyes[0] = ToEyeView(locatedViews_[0], latestPoseGameFrame_);
        views.eyes[1] = ToEyeView(locatedViews_[1], latestPoseGameFrame_);
        return true;
    }

    bool GetLatestInput(OpenXRInputSnapshot& input) const
    {
        std::lock_guard lock(mutex_);
        input = input_.Snapshot();
        return input.available;
    }

    void SetStereoSubmissionEnabled(bool enabled)
    {
        std::lock_guard lock(mutex_);
        if (stereoSubmissionEnabled_ == enabled) {
            return;
        }

        stereoSubmissionEnabled_ = enabled;
        pendingRenderedEyeValid_ = false;
        renderedStereoViewValid_[0] = false;
        renderedStereoViewValid_[1] = false;
        stereoCaptureFailures_ = 0;
        stereoWarmupLogged_ = false;
        Logger::Instance().Write(
            LogLevel::Warn,
            "openxr_stereo_submission enabled=%d mode=alternating_eye_cache",
            enabled ? 1 : 0);
    }

    bool MarkRenderedStereoEye(uint32_t eyeIndex, const OpenXREyeView& view)
    {
        std::lock_guard lock(mutex_);
        if (!stereoSubmissionEnabled_ || eyeIndex >= 2 || !view.valid) {
            return false;
        }
        pendingRenderedEye_ = eyeIndex;
        pendingRenderedView_ = view;
        pendingRenderedEyeValid_ = true;
        return true;
    }

private:
    void LogDeferredBootstrapLocked(uint64_t frameIndex)
    {
        if (bootstrapDeferredLogged_) {
            return;
        }
        bootstrapDeferredLogged_ = true;
        Logger::Instance().Write(
            LogLevel::Info,
            "openxr_bootstrap deferred_until_frame targetFrame=%llu currentFrame=%llu hdc=%s hglrc=%s",
            static_cast<unsigned long long>(bootstrapFrame_),
            static_cast<unsigned long long>(frameIndex),
            HexPointer(latestHdc_).c_str(),
            HexPointer(latestGlContext_).c_str());
    }

    void LogManualStartWaitingLocked(uint64_t frameIndex)
    {
        if (manualStartLogged_) {
            return;
        }
        manualStartLogged_ = true;
        Logger::Instance().Write(
            LogLevel::Info,
            "openxr_manual_start waiting key=F8 currentFrame=%llu hdc=%s hglrc=%s",
            static_cast<unsigned long long>(frameIndex),
            HexPointer(latestHdc_).c_str(),
            HexPointer(latestGlContext_).c_str());
    }

    void UpdateManualStartLocked(uint64_t frameIndex)
    {
        if (!manualStartEnabled_ || manualStartArmed_ || attempted_ || initialized_ || failed_) {
            return;
        }

        const bool keyDown = (GetAsyncKeyState(VK_F8) & 0x8000) != 0;
        if (keyDown && !manualStartKeyDown_) {
            manualStartArmed_ = true;
            manualStartFrame_ = frameIndex;
            Logger::Instance().Write(
                LogLevel::Info,
                "openxr_manual_start triggered key=F8 frame=%llu hdc=%s hglrc=%s",
                static_cast<unsigned long long>(frameIndex),
                HexPointer(latestHdc_).c_str(),
                HexPointer(latestGlContext_).c_str());
        }
        manualStartKeyDown_ = keyDown;
    }

    bool IsManualStartPendingLocked(uint64_t frameIndex)
    {
        if (!manualStartEnabled_ || manualStartArmed_) {
            return false;
        }

        LogManualStartWaitingLocked(frameIndex);
        return true;
    }

    void AttemptBootstrapLocked(HDC deviceContext, HGLRC glContext, uint64_t frameIndex)
    {
        attempted_ = true;
        retryFrame_ = 0;

        if (deviceContext == nullptr || glContext == nullptr) {
            if (!unavailableLogged_) {
                unavailableLogged_ = true;
                Logger::Instance().Write(LogLevel::Info, "openxr_bootstrap waiting_for_gl_context");
            }
            retryFrame_ = frameIndex + 300;
            return;
        }

        if (!EnsureOpenXRLoaderLoadedLocked()) {
            failed_ = true;
            return;
        }

        if (!CheckInstanceExtensionsLocked()) {
            failed_ = true;
            return;
        }

        XrInstanceCreateInfo createInfo{XR_TYPE_INSTANCE_CREATE_INFO};
        strcpy_s(createInfo.applicationInfo.applicationName, "SOMAVR");
        createInfo.applicationInfo.applicationVersion = 2;
        strcpy_s(createInfo.applicationInfo.engineName, "SOMAVR-HPL3-XRProbe");
        createInfo.applicationInfo.engineVersion = 2;
        createInfo.applicationInfo.apiVersion = XR_API_VERSION_1_0;

        const char* extensions[] = {
            XR_KHR_OPENGL_ENABLE_EXTENSION_NAME,
        };
        createInfo.enabledExtensionCount = 1;
        createInfo.enabledExtensionNames = extensions;

        XrResult result = xrCreateInstance(&createInfo, &instance_);
        if (XR_FAILED(result)) {
            Logger::Instance().Write(
                LogLevel::Warn,
                "openxr_bootstrap create_instance_failed result=%s",
                XrResultString(result).c_str());
            failed_ = true;
            return;
        }

        XrInstanceProperties properties{XR_TYPE_INSTANCE_PROPERTIES};
        result = xrGetInstanceProperties(instance_, &properties);
        if (XR_SUCCEEDED(result)) {
            Logger::Instance().Write(
                LogLevel::Info,
                "openxr_instance runtime=\"%s\" runtimeVersion=%s",
                properties.runtimeName,
                XrVersionString(properties.runtimeVersion).c_str());
        }

        if (!input_.Initialize(instance_, inputEnabled_, inputLogInterval_)) {
            Logger::Instance().Write(
                LogLevel::Warn,
                "openxr_input unavailable fallback=headset_only");
        }

        XrSystemGetInfo systemInfo{XR_TYPE_SYSTEM_GET_INFO};
        systemInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
        result = xrGetSystem(instance_, &systemInfo, &systemId_);
        if (XR_FAILED(result)) {
            Logger::Instance().Write(
                LogLevel::Warn,
                "openxr_bootstrap get_system_failed result=%s",
                XrResultString(result).c_str());
            if (result == XR_ERROR_FORM_FACTOR_UNAVAILABLE) {
                retryFrame_ = frameIndex + 300;
                input_.Shutdown();
                xrDestroyInstance(instance_);
                instance_ = XR_NULL_HANDLE;
                attempted_ = false;
                return;
            }
            failed_ = true;
            return;
        }

        LogSystemPropertiesLocked();

        PFN_xrGetOpenGLGraphicsRequirementsKHR getRequirements = nullptr;
        result = xrGetInstanceProcAddr(
            instance_,
            "xrGetOpenGLGraphicsRequirementsKHR",
            reinterpret_cast<PFN_xrVoidFunction*>(&getRequirements));
        if (XR_FAILED(result) || getRequirements == nullptr) {
            Logger::Instance().Write(
                LogLevel::Warn,
                "openxr_bootstrap opengl_requirements_proc_failed result=%s",
                XrResultString(result).c_str());
            failed_ = true;
            return;
        }

        XrGraphicsRequirementsOpenGLKHR requirements{XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR};
        result = getRequirements(instance_, systemId_, &requirements);
        if (XR_FAILED(result)) {
            Logger::Instance().Write(
                LogLevel::Warn,
                "openxr_bootstrap opengl_requirements_failed result=%s",
                XrResultString(result).c_str());
            failed_ = true;
            return;
        }

        initialized_ = true;
        Logger::Instance().Write(
            LogLevel::Info,
            "openxr_bootstrap requirements_ok minGL=%s maxGL=%s hdc=%s hglrc=%s",
            XrVersionString(requirements.minApiVersionSupported).c_str(),
            XrVersionString(requirements.maxApiVersionSupported).c_str(),
            HexPointer(deviceContext).c_str(),
            HexPointer(glContext).c_str());

        LogViewConfigurationLocked();
        LogEnvironmentBlendModesLocked();

        if (sessionProbeEnabled_) {
            const bool sessionOk = ProbeSessionLocked(deviceContext, glContext, frameIndex);
            if (releaseAfterProbeEnabled_) {
                if (sessionOk && holdFrames_ > 0) {
                    sessionHeldAfterProbe_ = true;
                    releaseFrame_ = frameIndex + holdFrames_;
                    Logger::Instance().Write(
                        LogLevel::Info,
                        "openxr_runtime hold_after_probe frame=%llu holdFrames=%llu releaseFrame=%llu",
                        static_cast<unsigned long long>(frameIndex),
                        static_cast<unsigned long long>(holdFrames_),
                        static_cast<unsigned long long>(releaseFrame_));
                } else {
                    ReleaseRuntimeAfterProbeLocked(sessionOk ? "session_probe_complete" : "session_probe_failed");
                }
            }
        } else {
            Logger::Instance().Write(LogLevel::Info, "openxr_session_probe skipped enabled=0");
            if (releaseAfterProbeEnabled_) {
                ReleaseRuntimeAfterProbeLocked("session_probe_disabled");
            }
        }
    }

    void ReleaseRuntimeAfterProbeLocked(const char* reason)
    {
        if (session_ == XR_NULL_HANDLE && instance_ == XR_NULL_HANDLE) {
            return;
        }

        bool releasedSession = false;
        bool releasedInstance = false;
        if (session_ != XR_NULL_HANDLE) {
            input_.ShutdownSession();
            DestroyFrameResourcesLocked();
            xrDestroySession(session_);
            session_ = XR_NULL_HANDLE;
            sessionRunning_ = false;
            sessionReleasedAfterProbe_ = true;
            releaseFrame_ = 0;
            releasedSession = true;
        }
        if (instance_ != XR_NULL_HANDLE) {
            input_.Shutdown();
            xrDestroyInstance(instance_);
            instance_ = XR_NULL_HANDLE;
            instanceReleasedAfterProbe_ = true;
            releasedInstance = true;
        }

        Logger::Instance().Write(
            LogLevel::Info,
            "openxr_runtime released_after_probe reason=%s releasedSession=%d releasedInstance=%d",
            reason,
            releasedSession ? 1 : 0,
            releasedInstance ? 1 : 0);
        if (std::strcmp(reason, "session_probe_disabled") == 0) {
            Logger::Instance().Write(LogLevel::Info, "openxr_instance released_after_static_probe reason=session_probe_disabled");
        }
    }

    bool EnsureOpenXRLoaderLoadedLocked()
    {
        if (loaderModule_ != nullptr) {
            return true;
        }

        if (HMODULE existing = GetModuleHandleW(L"openxr_loader.dll")) {
            loaderModule_ = existing;
            Logger::Instance().Write(LogLevel::Info, "openxr_loader already_loaded module=%s", HexPointer(loaderModule_).c_str());
            return true;
        }

        const std::filesystem::path moduleDir = CurrentModuleDirectory();
        const std::filesystem::path loaderPath = moduleDir / L"openxr_loader.dll";
        loaderLoadAttempted_ = true;

        Logger::Instance().Write(LogLevel::Info, "openxr_loader_load attempt path=%s", loaderPath.string().c_str());
        loaderModule_ = LoadLibraryW(loaderPath.c_str());
        if (loaderModule_ == nullptr) {
            const DWORD error = GetLastError();
            Logger::Instance().Write(
                LogLevel::Error,
                "openxr_loader_load failed path=%s lastError=%lu message=\"%s\"",
                loaderPath.string().c_str(),
                static_cast<unsigned long>(error),
                ToUtf8(Win32ErrorMessage(error)).c_str());
            return false;
        }

        Logger::Instance().Write(
            LogLevel::Info,
            "openxr_loader_load ok path=%s module=%s",
            loaderPath.string().c_str(),
            HexPointer(loaderModule_).c_str());
        return true;
    }

    bool CheckInstanceExtensionsLocked()
    {
        uint32_t extensionCount = 0;
        XrResult result = xrEnumerateInstanceExtensionProperties(nullptr, 0, &extensionCount, nullptr);
        if (XR_FAILED(result)) {
            Logger::Instance().Write(
                LogLevel::Warn,
                "openxr_extensions enumerate_count_failed result=%s",
                XrResultString(result).c_str());
            return true;
        }

        std::vector<XrExtensionProperties> extensions(extensionCount);
        for (XrExtensionProperties& extension : extensions) {
            extension.type = XR_TYPE_EXTENSION_PROPERTIES;
        }
        result = xrEnumerateInstanceExtensionProperties(nullptr, extensionCount, &extensionCount, extensions.data());
        if (XR_FAILED(result)) {
            Logger::Instance().Write(
                LogLevel::Warn,
                "openxr_extensions enumerate_failed result=%s",
                XrResultString(result).c_str());
            return true;
        }
        extensions.resize(extensionCount);

        const bool hasOpenGL = ExtensionPresent(extensions, XR_KHR_OPENGL_ENABLE_EXTENSION_NAME);
        const bool hasWin32Time = ExtensionPresent(extensions, "XR_KHR_win32_convert_performance_counter_time");
        Logger::Instance().Write(
            LogLevel::Info,
            "openxr_extensions count=%u khrOpenGL=%d khrWin32Time=%d sample=\"%s\"",
            extensionCount,
            hasOpenGL ? 1 : 0,
            hasWin32Time ? 1 : 0,
            ExtensionSample(extensions).c_str());

        if (!hasOpenGL) {
            Logger::Instance().Write(LogLevel::Warn, "openxr_bootstrap missing_required_extension name=%s", XR_KHR_OPENGL_ENABLE_EXTENSION_NAME);
        }
        return hasOpenGL;
    }

    void LogSystemPropertiesLocked()
    {
        XrSystemProperties properties{XR_TYPE_SYSTEM_PROPERTIES};
        const XrResult result = xrGetSystemProperties(instance_, systemId_, &properties);
        if (XR_FAILED(result)) {
            Logger::Instance().Write(
                LogLevel::Warn,
                "openxr_system_properties failed result=%s",
                XrResultString(result).c_str());
            return;
        }

        Logger::Instance().Write(
            LogLevel::Info,
            "openxr_system name=\"%s\" vendorId=%u maxWidth=%u maxHeight=%u maxLayers=%u orientationTracking=%d positionTracking=%d",
            properties.systemName,
            properties.vendorId,
            properties.graphicsProperties.maxSwapchainImageWidth,
            properties.graphicsProperties.maxSwapchainImageHeight,
            properties.graphicsProperties.maxLayerCount,
            properties.trackingProperties.orientationTracking ? 1 : 0,
            properties.trackingProperties.positionTracking ? 1 : 0);
    }

    void LogViewConfigurationLocked()
    {
        uint32_t configCount = 0;
        XrResult result = xrEnumerateViewConfigurations(instance_, systemId_, 0, &configCount, nullptr);
        if (XR_FAILED(result)) {
            Logger::Instance().Write(
                LogLevel::Warn,
                "openxr_view_configurations enumerate_count_failed result=%s",
                XrResultString(result).c_str());
            return;
        }

        std::vector<XrViewConfigurationType> configs(configCount);
        result = xrEnumerateViewConfigurations(instance_, systemId_, configCount, &configCount, configs.data());
        if (XR_FAILED(result)) {
            Logger::Instance().Write(
                LogLevel::Warn,
                "openxr_view_configurations enumerate_failed result=%s",
                XrResultString(result).c_str());
            return;
        }
        configs.resize(configCount);

        std::ostringstream configText;
        bool hasStereo = false;
        for (size_t i = 0; i < configs.size(); ++i) {
            if (i != 0) {
                configText << ",";
            }
            configText << ViewConfigurationTypeName(configs[i]) << "(" << static_cast<int>(configs[i]) << ")";
            if (configs[i] == XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO) {
                hasStereo = true;
            }
        }

        Logger::Instance().Write(
            LogLevel::Info,
            "openxr_view_configurations count=%u primaryStereo=%d configs=\"%s\"",
            configCount,
            hasStereo ? 1 : 0,
            configText.str().c_str());
        if (!hasStereo) {
            return;
        }

        uint32_t viewCount = 0;
        result = xrEnumerateViewConfigurationViews(
            instance_,
            systemId_,
            XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
            0,
            &viewCount,
            nullptr);
        if (XR_FAILED(result)) {
            Logger::Instance().Write(
                LogLevel::Warn,
                "openxr_view_configuration_views enumerate_count_failed result=%s",
                XrResultString(result).c_str());
            return;
        }

        std::vector<XrViewConfigurationView> views(viewCount);
        for (XrViewConfigurationView& view : views) {
            view.type = XR_TYPE_VIEW_CONFIGURATION_VIEW;
        }

        result = xrEnumerateViewConfigurationViews(
            instance_,
            systemId_,
            XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
            viewCount,
            &viewCount,
            views.data());
        if (XR_FAILED(result)) {
            Logger::Instance().Write(
                LogLevel::Warn,
                "openxr_view_configuration_views enumerate_failed result=%s",
                XrResultString(result).c_str());
            return;
        }
        views.resize(viewCount);
        viewConfigurationViews_ = views;
        viewCount_ = viewCount;

        for (uint32_t i = 0; i < viewCount; ++i) {
            const XrViewConfigurationView& view = views[i];
            Logger::Instance().Write(
                LogLevel::Info,
                "openxr_view index=%u recommended=%ux%u max=%ux%u recommendedSamples=%u maxSamples=%u",
                i,
                view.recommendedImageRectWidth,
                view.recommendedImageRectHeight,
                view.maxImageRectWidth,
                view.maxImageRectHeight,
                view.recommendedSwapchainSampleCount,
                view.maxSwapchainSampleCount);
        }
    }

    void LogEnvironmentBlendModesLocked()
    {
        uint32_t modeCount = 0;
        XrResult result = xrEnumerateEnvironmentBlendModes(
            instance_,
            systemId_,
            XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
            0,
            &modeCount,
            nullptr);
        if (XR_FAILED(result)) {
            Logger::Instance().Write(
                LogLevel::Warn,
                "openxr_blend_modes enumerate_count_failed result=%s",
                XrResultString(result).c_str());
            return;
        }

        std::vector<XrEnvironmentBlendMode> modes(modeCount);
        result = xrEnumerateEnvironmentBlendModes(
            instance_,
            systemId_,
            XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
            modeCount,
            &modeCount,
            modes.data());
        if (XR_FAILED(result)) {
            Logger::Instance().Write(
                LogLevel::Warn,
                "openxr_blend_modes enumerate_failed result=%s",
                XrResultString(result).c_str());
            return;
        }
        modes.resize(modeCount);

        std::ostringstream text;
        for (size_t i = 0; i < modes.size(); ++i) {
            if (i != 0) {
                text << ",";
            }
            text << EnvironmentBlendModeName(modes[i]) << "(" << static_cast<int>(modes[i]) << ")";
        }

        Logger::Instance().Write(LogLevel::Info, "openxr_blend_modes count=%u modes=\"%s\"", modeCount, text.str().c_str());
    }

    bool ProbeSessionLocked(HDC deviceContext, HGLRC glContext, uint64_t frameIndex)
    {
        sessionAttempted_ = true;

        XrGraphicsBindingOpenGLWin32KHR graphicsBinding{XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR};
        graphicsBinding.hDC = deviceContext;
        graphicsBinding.hGLRC = glContext;

        XrSessionCreateInfo sessionInfo{XR_TYPE_SESSION_CREATE_INFO};
        sessionInfo.next = &graphicsBinding;
        sessionInfo.systemId = systemId_;

        XrSession session = XR_NULL_HANDLE;
        const XrResult result = xrCreateSession(instance_, &sessionInfo, &session);
        if (XR_FAILED(result)) {
            Logger::Instance().Write(
                LogLevel::Warn,
                "openxr_session_probe create_failed result=%s hdc=%s hglrc=%s",
                XrResultString(result).c_str(),
                HexPointer(deviceContext).c_str(),
                HexPointer(glContext).c_str());
            return false;
        }

        session_ = session;
        sessionCreated_ = true;
        sessionCreatedFrame_ = frameIndex;
        Logger::Instance().Write(
            LogLevel::Info,
            "openxr_session_probe ok frame=%llu hdc=%s hglrc=%s",
            static_cast<unsigned long long>(frameIndex),
            HexPointer(deviceContext).c_str(),
            HexPointer(glContext).c_str());

        if (!input_.AttachSession(session_)) {
            Logger::Instance().Write(
                LogLevel::Warn,
                "openxr_input session_attach_unavailable fallback=headset_only");
        }

        LogReferenceSpacesLocked();
        LogSwapchainFormatsLocked();
        if (frameSubmitEnabled_) {
            frameResourcesReady_ = CreateFrameResourcesLocked();
            frameSubmitFailed_ = !frameResourcesReady_;
        }
        PollEventsLocked(frameIndex);
        return true;
    }

    void LogReferenceSpacesLocked()
    {
        uint32_t spaceCount = 0;
        XrResult result = xrEnumerateReferenceSpaces(session_, 0, &spaceCount, nullptr);
        if (XR_FAILED(result)) {
            Logger::Instance().Write(
                LogLevel::Warn,
                "openxr_reference_spaces enumerate_count_failed result=%s",
                XrResultString(result).c_str());
            return;
        }

        std::vector<XrReferenceSpaceType> spaces(spaceCount);
        result = xrEnumerateReferenceSpaces(session_, spaceCount, &spaceCount, spaces.data());
        if (XR_FAILED(result)) {
            Logger::Instance().Write(
                LogLevel::Warn,
                "openxr_reference_spaces enumerate_failed result=%s",
                XrResultString(result).c_str());
            return;
        }
        spaces.resize(spaceCount);

        std::ostringstream text;
        for (size_t i = 0; i < spaces.size(); ++i) {
            if (i != 0) {
                text << ",";
            }
            text << ReferenceSpaceTypeName(spaces[i]) << "(" << static_cast<int>(spaces[i]) << ")";
        }

        Logger::Instance().Write(LogLevel::Info, "openxr_reference_spaces count=%u spaces=\"%s\"", spaceCount, text.str().c_str());
    }

    void LogSwapchainFormatsLocked()
    {
        uint32_t formatCount = 0;
        XrResult result = xrEnumerateSwapchainFormats(session_, 0, &formatCount, nullptr);
        if (XR_FAILED(result)) {
            Logger::Instance().Write(
                LogLevel::Warn,
                "openxr_swapchain_formats enumerate_count_failed result=%s",
                XrResultString(result).c_str());
            return;
        }

        std::vector<int64_t> formats(formatCount);
        result = xrEnumerateSwapchainFormats(session_, formatCount, &formatCount, formats.data());
        if (XR_FAILED(result)) {
            Logger::Instance().Write(
                LogLevel::Warn,
                "openxr_swapchain_formats enumerate_failed result=%s",
                XrResultString(result).c_str());
            return;
        }
        formats.resize(formatCount);
        swapchainFormats_ = formats;
        swapchainFormatCount_ = formatCount;

        Logger::Instance().Write(
            LogLevel::Info,
            "openxr_swapchain_formats count=%u sample=\"%s\"",
            formatCount,
            SwapchainFormatSample(formats).c_str());
    }

    bool CreateFrameResourcesLocked()
    {
        if (session_ == XR_NULL_HANDLE || viewConfigurationViews_.size() < 2 || swapchainFormats_.empty()) {
            Logger::Instance().Write(
                LogLevel::Warn,
                "openxr_frame_resources unavailable session=%d views=%zu formats=%zu",
                session_ != XR_NULL_HANDLE ? 1 : 0,
                viewConfigurationViews_.size(),
                swapchainFormats_.size());
            return false;
        }

        XrReferenceSpaceCreateInfo spaceInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
        spaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
        spaceInfo.poseInReferenceSpace.orientation.w = 1.0f;
        XrResult result = xrCreateReferenceSpace(session_, &spaceInfo, &localSpace_);
        if (XR_FAILED(result)) {
            Logger::Instance().Write(
                LogLevel::Warn,
                "openxr_reference_space create_failed type=LOCAL result=%s",
                XrResultString(result).c_str());
            return false;
        }

        if (!glBridge_.Initialize(
                session_,
                viewConfigurationViews_,
                swapchainFormats_,
                resolutionScalePercent_)) {
            xrDestroySpace(localSpace_);
            localSpace_ = XR_NULL_HANDLE;
            Logger::Instance().Write(LogLevel::Warn, "openxr_frame_resources gl_bridge_failed");
            return false;
        }

        locatedViews_.resize(glBridge_.EyeCount());
        for (XrView& view : locatedViews_) {
            view.type = XR_TYPE_VIEW;
        }

        Logger::Instance().Write(
            LogLevel::Info,
            "openxr_frame_resources ready space=LOCAL eyes=%u mirrorBackbuffer=%d",
            glBridge_.EyeCount(),
            mirrorBackbufferEnabled_ ? 1 : 0);
        return true;
    }

    void DestroyFrameResourcesLocked()
    {
        glBridge_.Shutdown();
        if (localSpace_ != XR_NULL_HANDLE) {
            xrDestroySpace(localSpace_);
            localSpace_ = XR_NULL_HANDLE;
        }
        locatedViews_.clear();
        pendingRenderedEyeValid_ = false;
        renderedStereoViewValid_[0] = false;
        renderedStereoViewValid_[1] = false;
        stereoWarmupLogged_ = false;
        frameResourcesReady_ = false;
    }

    void BeginSessionLocked(uint64_t frameIndex)
    {
        if (!frameSubmitEnabled_ || !frameResourcesReady_ || sessionRunning_ || session_ == XR_NULL_HANDLE) {
            return;
        }

        XrSessionBeginInfo beginInfo{XR_TYPE_SESSION_BEGIN_INFO};
        beginInfo.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        const XrResult result = xrBeginSession(session_, &beginInfo);
        if (XR_FAILED(result)) {
            Logger::Instance().Write(
                LogLevel::Warn,
                "openxr_session_begin failed frame=%llu result=%s",
                static_cast<unsigned long long>(frameIndex),
                XrResultString(result).c_str());
            frameSubmitFailed_ = true;
            return;
        }

        sessionRunning_ = true;
        Logger::Instance().Write(
            LogLevel::Info,
            "openxr_session_begin ok frame=%llu viewConfig=PRIMARY_STEREO",
            static_cast<unsigned long long>(frameIndex));
    }

    void RecordFrameFailureLocked(const char* operation, XrResult result, uint64_t frameIndex)
    {
        ++consecutiveFrameFailures_;
        if (frameErrorLogCount_ < 16) {
            Logger::Instance().Write(
                LogLevel::Warn,
                "openxr_frame failure operation=%s gameFrame=%llu result=%s consecutive=%u",
                operation,
                static_cast<unsigned long long>(frameIndex),
                XrResultString(result).c_str(),
                consecutiveFrameFailures_);
            ++frameErrorLogCount_;
        }
        if (consecutiveFrameFailures_ >= 60 && !frameSubmitFailed_) {
            frameSubmitFailed_ = true;
            Logger::Instance().Write(
                LogLevel::Error,
                "openxr_frame submission_suspended gameFrame=%llu consecutiveFailures=%u",
                static_cast<unsigned long long>(frameIndex),
                consecutiveFrameFailures_);
        }
    }

    void SubmitFrameLocked(uint64_t frameIndex)
    {
        if (stereoSubmissionEnabled_ && pendingRenderedEyeValid_ && glBridge_.Ready()) {
            const uint32_t eyeIndex = pendingRenderedEye_;
            if (glBridge_.CaptureBackbufferToCache(eyeIndex)) {
                renderedStereoViews_[eyeIndex] = pendingRenderedView_;
                renderedStereoViewValid_[eyeIndex] = true;
                lastCapturedStereoEye_ = eyeIndex;
                ++stereoCapturedEyeCount_;
                stereoCaptureFailures_ = 0;
                if (stereoCapturedEyeCount_ <= 2 || (stereoCapturedEyeCount_ % 120) == 0) {
                    Logger::Instance().Write(
                        LogLevel::Info,
                        "openxr_stereo_cache captured=%llu eye=%u poseFrame=%llu cachesReady=%d",
                        static_cast<unsigned long long>(stereoCapturedEyeCount_),
                        eyeIndex,
                        static_cast<unsigned long long>(pendingRenderedView_.gameFrame),
                        glBridge_.StereoCachesReady() ? 1 : 0);
                }
            } else {
                ++stereoCaptureFailures_;
                if (stereoCaptureFailures_ <= 4) {
                    Logger::Instance().Write(
                        LogLevel::Warn,
                        "openxr_stereo_cache capture_failed eye=%u consecutive=%u",
                        eyeIndex,
                        stereoCaptureFailures_);
                }
                if (stereoCaptureFailures_ >= 8) {
                    stereoSubmissionEnabled_ = false;
                    renderedStereoViewValid_[0] = false;
                    renderedStereoViewValid_[1] = false;
                    Logger::Instance().Write(
                        LogLevel::Error,
                        "openxr_stereo_submission suspended reason=cache_capture_failures consecutive=%u",
                        stereoCaptureFailures_);
                }
            }
            pendingRenderedEyeValid_ = false;
        }

        XrFrameWaitInfo waitInfo{XR_TYPE_FRAME_WAIT_INFO};
        XrFrameState frameState{XR_TYPE_FRAME_STATE};
        XrResult result = xrWaitFrame(session_, &waitInfo, &frameState);
        if (XR_FAILED(result)) {
            RecordFrameFailureLocked("xrWaitFrame", result, frameIndex);
            return;
        }

        XrFrameBeginInfo beginInfo{XR_TYPE_FRAME_BEGIN_INFO};
        result = xrBeginFrame(session_, &beginInfo);
        if (XR_FAILED(result)) {
            RecordFrameFailureLocked("xrBeginFrame", result, frameIndex);
            return;
        }

        input_.Sync(session_, localSpace_, frameState.predictedDisplayTime, frameIndex);

        std::array<XrCompositionLayerProjectionView, 2> projectionViews{};
        XrCompositionLayerProjection projectionLayer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
        const XrCompositionLayerBaseHeader* layers[1] = {};
        uint32_t layerCount = 0;
        uint32_t locatedViewCount = 0;
        XrViewState viewState{XR_TYPE_VIEW_STATE};
        bool submittedStereo = false;

        if (frameState.shouldRender == XR_TRUE && mirrorBackbufferEnabled_ && glBridge_.Ready()) {
            XrViewLocateInfo locateInfo{XR_TYPE_VIEW_LOCATE_INFO};
            locateInfo.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
            locateInfo.displayTime = frameState.predictedDisplayTime;
            locateInfo.space = localSpace_;
            result = xrLocateViews(
                session_,
                &locateInfo,
                &viewState,
                static_cast<uint32_t>(locatedViews_.size()),
                &locatedViewCount,
                locatedViews_.data());

            const XrViewStateFlags requiredFlags =
                XR_VIEW_STATE_ORIENTATION_VALID_BIT | XR_VIEW_STATE_POSITION_VALID_BIT;
            const bool viewsValid = XR_SUCCEEDED(result)
                && locatedViewCount == glBridge_.EyeCount()
                && (viewState.viewStateFlags & requiredFlags) == requiredFlags;

            bool copied = viewsValid;
            if (!viewsValid) {
                RecordFrameFailureLocked(
                    "xrLocateViews",
                    XR_FAILED(result) ? result : XR_ERROR_VALIDATION_FAILURE,
                    frameIndex);
            } else {
                latestLeftEyePose_ = locatedViews_[0].pose;
                latestRightEyePose_ = locatedViews_[1].pose;
                latestHeadPose_.position = {
                    (latestLeftEyePose_.position.x + latestRightEyePose_.position.x) * 0.5f,
                    (latestLeftEyePose_.position.y + latestRightEyePose_.position.y) * 0.5f,
                    (latestLeftEyePose_.position.z + latestRightEyePose_.position.z) * 0.5f,
                };
                latestHeadPose_.orientation = latestLeftEyePose_.orientation;
                const float eyeDx = latestRightEyePose_.position.x - latestLeftEyePose_.position.x;
                const float eyeDy = latestRightEyePose_.position.y - latestLeftEyePose_.position.y;
                const float eyeDz = latestRightEyePose_.position.z - latestLeftEyePose_.position.z;
                latestIpdMeters_ = std::sqrt(eyeDx * eyeDx + eyeDy * eyeDy + eyeDz * eyeDz);
                latestViewStateFlags_ = viewState.viewStateFlags;
                latestPoseGameFrame_ = frameIndex;
                latestPoseValid_ = true;

                const bool stereoReady = stereoSubmissionEnabled_
                    && glBridge_.StereoCachesReady()
                    && renderedStereoViewValid_[0]
                    && renderedStereoViewValid_[1];
                if (stereoSubmissionEnabled_ && !stereoReady) {
                    copied = false;
                    if (!stereoWarmupLogged_) {
                        stereoWarmupLogged_ = true;
                        Logger::Instance().Write(
                            LogLevel::Info,
                            "openxr_stereo_submission warming_up leftReady=%d rightReady=%d",
                            renderedStereoViewValid_[0] ? 1 : 0,
                            renderedStereoViewValid_[1] ? 1 : 0);
                    }
                }

                if (copied) {
                    for (uint32_t eyeIndex = 0; eyeIndex < glBridge_.EyeCount(); ++eyeIndex) {
                        const bool eyeCopied = stereoReady
                            ? glBridge_.CopyCacheToEye(eyeIndex)
                            : glBridge_.CopyBackbufferToEye(eyeIndex);
                        if (!eyeCopied) {
                            copied = false;
                            RecordFrameFailureLocked(
                                stereoReady ? "copyStereoCache" : "copyBackbuffer",
                                XR_ERROR_RUNTIME_FAILURE,
                                frameIndex);
                            break;
                        }

                        const OpenXRGLBridge::EyeSwapchain& eye = glBridge_.Eye(eyeIndex);
                        XrCompositionLayerProjectionView& projectionView = projectionViews[eyeIndex];
                        projectionView.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
                        projectionView.pose = stereoReady
                            ? ToXrPose(renderedStereoViews_[eyeIndex])
                            : locatedViews_[eyeIndex].pose;
                        projectionView.fov = stereoReady
                            ? ToXrFov(renderedStereoViews_[eyeIndex])
                            : locatedViews_[eyeIndex].fov;
                        projectionView.subImage.swapchain = eye.handle;
                        projectionView.subImage.imageRect.offset = {0, 0};
                        projectionView.subImage.imageRect.extent = {eye.width, eye.height};
                        projectionView.subImage.imageArrayIndex = 0;
                    }
                    submittedStereo = copied && stereoReady;
                }
            }

            if (copied) {
                projectionLayer.space = localSpace_;
                projectionLayer.viewCount = glBridge_.EyeCount();
                projectionLayer.views = projectionViews.data();
                layers[0] = reinterpret_cast<const XrCompositionLayerBaseHeader*>(&projectionLayer);
                layerCount = 1;
            }
        }

        XrFrameEndInfo endInfo{XR_TYPE_FRAME_END_INFO};
        endInfo.displayTime = frameState.predictedDisplayTime;
        endInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
        endInfo.layerCount = layerCount;
        endInfo.layers = layerCount > 0 ? layers : nullptr;
        result = xrEndFrame(session_, &endInfo);
        if (XR_FAILED(result)) {
            RecordFrameFailureLocked("xrEndFrame", result, frameIndex);
            return;
        }

        consecutiveFrameFailures_ = 0;
        ++completedXrFrameCount_;
        if (layerCount > 0) {
            ++submittedFrameCount_;
            lastSubmittedGameFrame_ = frameIndex;
            if (submittedStereo) {
                ++stereoSubmittedFrameCount_;
            }
        }

        if (completedXrFrameCount_ == 1 || (completedXrFrameCount_ % 300) == 0) {
            Logger::Instance().Write(
                LogLevel::Info,
                "openxr_frame ok gameFrame=%llu xrFrame=%llu shouldRender=%d layers=%u views=%u stereo=%d stereoCaptured=%llu stereoSubmitted=%llu predictedDisplayTime=%lld leftPos=%.4f,%.4f,%.4f rightPos=%.4f,%.4f,%.4f",
                static_cast<unsigned long long>(frameIndex),
                static_cast<unsigned long long>(completedXrFrameCount_),
                frameState.shouldRender == XR_TRUE ? 1 : 0,
                layerCount,
                locatedViewCount,
                submittedStereo ? 1 : 0,
                static_cast<unsigned long long>(stereoCapturedEyeCount_),
                static_cast<unsigned long long>(stereoSubmittedFrameCount_),
                static_cast<long long>(frameState.predictedDisplayTime),
                locatedViewCount > 0 ? locatedViews_[0].pose.position.x : 0.0f,
                locatedViewCount > 0 ? locatedViews_[0].pose.position.y : 0.0f,
                locatedViewCount > 0 ? locatedViews_[0].pose.position.z : 0.0f,
                locatedViewCount > 1 ? locatedViews_[1].pose.position.x : 0.0f,
                locatedViewCount > 1 ? locatedViews_[1].pose.position.y : 0.0f,
                locatedViewCount > 1 ? locatedViews_[1].pose.position.z : 0.0f);
        }
    }

    void PollEventsLocked(uint64_t frameIndex)
    {
        if (instance_ == XR_NULL_HANDLE) {
            return;
        }

        for (uint32_t i = 0; i < 16; ++i) {
            XrEventDataBuffer event{XR_TYPE_EVENT_DATA_BUFFER};
            const XrResult result = xrPollEvent(instance_, &event);
            if (result == XR_EVENT_UNAVAILABLE) {
                return;
            }
            if (XR_FAILED(result)) {
                if (eventLogCount_ < 64) {
                    Logger::Instance().Write(
                        LogLevel::Warn,
                        "openxr_event poll_failed frame=%llu result=%s",
                        static_cast<unsigned long long>(frameIndex),
                        XrResultString(result).c_str());
                    ++eventLogCount_;
                }
                return;
            }

            if (event.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
                const auto* state = reinterpret_cast<const XrEventDataSessionStateChanged*>(&event);
                sessionState_ = state->state;
                if (eventLogCount_ < 64) {
                    Logger::Instance().Write(
                        LogLevel::Info,
                        "openxr_event frame=%llu type=session_state_changed state=%s session=%s time=%lld",
                        static_cast<unsigned long long>(frameIndex),
                        SessionStateName(state->state),
                        HexPointer(reinterpret_cast<void*>(state->session)).c_str(),
                        static_cast<long long>(state->time));
                    ++eventLogCount_;
                }

                if (state->session == session_) {
                    if (state->state == XR_SESSION_STATE_READY) {
                        BeginSessionLocked(frameIndex);
                    } else if (state->state == XR_SESSION_STATE_STOPPING && sessionRunning_) {
                        const XrResult endResult = xrEndSession(session_);
                        Logger::Instance().Write(
                            XR_SUCCEEDED(endResult) ? LogLevel::Info : LogLevel::Warn,
                            "openxr_session_end frame=%llu result=%s",
                            static_cast<unsigned long long>(frameIndex),
                            XrResultString(endResult).c_str());
                        sessionRunning_ = false;
                    } else if (state->state == XR_SESSION_STATE_EXITING
                        || state->state == XR_SESSION_STATE_LOSS_PENDING) {
                        sessionRunning_ = false;
                        frameSubmitFailed_ = true;
                    }
                }
            } else {
                if (eventLogCount_ < 64) {
                    Logger::Instance().Write(
                        LogLevel::Info,
                        "openxr_event frame=%llu type=%d",
                        static_cast<unsigned long long>(frameIndex),
                        static_cast<int>(event.type));
                    ++eventLogCount_;
                }
            }
        }
    }

    mutable std::mutex mutex_;
    bool enabled_ = false;
    bool sessionProbeEnabled_ = true;
    bool releaseAfterProbeEnabled_ = true;
    uint64_t bootstrapFrame_ = 0;
    uint64_t holdFrames_ = 0;
    bool manualStartEnabled_ = false;
    bool manualStartArmed_ = false;
    bool manualStartLogged_ = false;
    bool manualStartKeyDown_ = false;
    bool frameSubmitEnabled_ = false;
    bool inputEnabled_ = false;
    int inputLogInterval_ = 120;
    bool mirrorBackbufferEnabled_ = true;
    int resolutionScalePercent_ = 100;
    bool frameResourcesReady_ = false;
    bool frameSubmitFailed_ = false;
    bool sessionRunning_ = false;
    bool stereoSubmissionEnabled_ = false;
    bool pendingRenderedEyeValid_ = false;
    bool renderedStereoViewValid_[2] = {};
    bool stereoWarmupLogged_ = false;
    bool attempted_ = false;
    bool initialized_ = false;
    bool failed_ = false;
    bool unavailableLogged_ = false;
    bool bootstrapDeferredLogged_ = false;
    bool loaderLoadAttempted_ = false;
    bool instanceReleasedAfterProbe_ = false;
    bool sessionReleasedAfterProbe_ = false;
    bool sessionHeldAfterProbe_ = false;
    bool sessionAttempted_ = false;
    bool sessionCreated_ = false;
    uint64_t retryFrame_ = 0;
    uint64_t releaseFrame_ = 0;
    uint64_t sessionCreatedFrame_ = 0;
    uint64_t manualStartFrame_ = 0;
    uint64_t completedXrFrameCount_ = 0;
    uint64_t submittedFrameCount_ = 0;
    uint64_t lastSubmittedGameFrame_ = 0;
    uint64_t currentGameFrame_ = 0;
    uint32_t consecutiveFrameFailures_ = 0;
    uint32_t frameErrorLogCount_ = 0;
    uint32_t pendingRenderedEye_ = 0;
    uint32_t lastCapturedStereoEye_ = 0;
    uint32_t stereoCaptureFailures_ = 0;
    uint64_t stereoCapturedEyeCount_ = 0;
    uint64_t stereoSubmittedFrameCount_ = 0;
    OpenXREyeView pendingRenderedView_{};
    OpenXREyeView renderedStereoViews_[2] = {};
    bool latestPoseValid_ = false;
    uint64_t latestPoseGameFrame_ = 0;
    XrViewStateFlags latestViewStateFlags_ = 0;
    float latestIpdMeters_ = 0.0f;
    XrPosef latestHeadPose_{};
    XrPosef latestLeftEyePose_{};
    XrPosef latestRightEyePose_{};
    HDC latestHdc_ = nullptr;
    HGLRC latestGlContext_ = nullptr;
    XrInstance instance_ = XR_NULL_HANDLE;
    XrSystemId systemId_ = XR_NULL_SYSTEM_ID;
    XrSession session_ = XR_NULL_HANDLE;
    XrSpace localSpace_ = XR_NULL_HANDLE;
    HMODULE loaderModule_ = nullptr;
    XrSessionState sessionState_ = XR_SESSION_STATE_UNKNOWN;
    uint32_t eventLogCount_ = 0;
    uint32_t viewCount_ = 0;
    uint32_t swapchainFormatCount_ = 0;
    std::vector<XrViewConfigurationView> viewConfigurationViews_;
    std::vector<int64_t> swapchainFormats_;
    std::vector<XrView> locatedViews_;
    OpenXRGLBridge glBridge_;
    OpenXRInput input_;
};

#else

struct OpenXRRuntime::Impl {
    void Configure(
        bool enabled,
        bool sessionProbe,
        bool releaseAfterProbe,
        uint64_t bootstrapFrame,
        uint64_t holdFrames,
        bool manualStart,
        bool frameSubmit,
        bool mirrorBackbuffer,
        int resolutionScalePercent,
        bool inputEnabled,
        int inputLogInterval)
    {
        std::lock_guard lock(mutex_);
        enabled_ = enabled;
        sessionProbeEnabled_ = sessionProbe;
        releaseAfterProbeEnabled_ = releaseAfterProbe;
        bootstrapFrame_ = bootstrapFrame;
        holdFrames_ = holdFrames;
        manualStartEnabled_ = manualStart;
        frameSubmitEnabled_ = frameSubmit;
        mirrorBackbufferEnabled_ = mirrorBackbuffer;
        resolutionScalePercent_ = resolutionScalePercent;
        inputEnabled_ = inputEnabled;
        inputLogInterval_ = inputLogInterval;
        manualStartArmed_ = false;
        unavailableLogged_ = false;
        Logger::Instance().Write(
            LogLevel::Info,
            "openxr_config buildOpenXR=0 enabled=%d sessionProbe=%d releaseAfterProbe=%d bootstrapFrame=%llu holdFrames=%llu manualStart=%d key=F8 frameSubmit=%d mirrorBackbuffer=%d resolutionScalePercent=%d input=%d inputLogInterval=%d",
            enabled_ ? 1 : 0,
            sessionProbeEnabled_ ? 1 : 0,
            releaseAfterProbeEnabled_ ? 1 : 0,
            static_cast<unsigned long long>(bootstrapFrame_),
            static_cast<unsigned long long>(holdFrames_),
            manualStartEnabled_ ? 1 : 0,
            frameSubmitEnabled_ ? 1 : 0,
            mirrorBackbufferEnabled_ ? 1 : 0,
            resolutionScalePercent_,
            inputEnabled_ ? 1 : 0,
            inputLogInterval_);
    }

    void OnOpenGLContext(HDC deviceContext, HGLRC glContext)
    {
        std::lock_guard lock(mutex_);
        latestHdc_ = deviceContext;
        latestGlContext_ = glContext;
        LogUnavailableLocked();
    }

    void OnFrameBoundary(HDC deviceContext, HGLRC glContext, uint64_t)
    {
        std::lock_guard lock(mutex_);
        latestHdc_ = deviceContext;
        latestGlContext_ = glContext;
        LogUnavailableLocked();
    }

    bool RequestManualStart()
    {
        std::lock_guard lock(mutex_);
        LogUnavailableLocked();
        return false;
    }

    void Shutdown() {}

    std::string SummaryString() const
    {
        std::lock_guard lock(mutex_);
        std::ostringstream oss;
        oss << "openxrEnabled=" << (enabled_ ? 1 : 0)
            << " openxrBuild=0"
            << " openxrInitialized=0"
            << " openxrAttempted=0"
            << " openxrFailed=" << (enabled_ ? 1 : 0)
            << " openxrSessionProbe=" << (sessionProbeEnabled_ ? 1 : 0)
            << " openxrReleaseAfterProbe=" << (releaseAfterProbeEnabled_ ? 1 : 0)
            << " openxrBootstrapFrame=" << static_cast<unsigned long long>(bootstrapFrame_)
            << " openxrHoldFrames=" << static_cast<unsigned long long>(holdFrames_)
            << " openxrManualStart=" << (manualStartEnabled_ ? 1 : 0)
            << " openxrManualStartArmed=" << (manualStartArmed_ ? 1 : 0)
            << " openxrManualStartFrame=0"
            << " openxrFrameSubmit=" << (frameSubmitEnabled_ ? 1 : 0)
            << " openxrMirrorBackbuffer=" << (mirrorBackbufferEnabled_ ? 1 : 0)
            << " openxrResolutionScalePercent=" << resolutionScalePercent_
            << " openxrInputEnabled=" << (inputEnabled_ ? 1 : 0)
            << " openxrFrameResourcesReady=0"
            << " openxrFrameSubmitFailed=" << (enabled_ && frameSubmitEnabled_ ? 1 : 0)
            << " openxrSessionRunning=0"
            << " openxrSubmittedFrames=0"
            << " openxrLastSubmittedGameFrame=0"
            << " openxrSessionAttempted=0"
            << " openxrSessionCreated=0"
            << " openxrSessionAlive=0"
            << " openxrSessionReleasedAfterProbe=0"
            << " openxrSessionHeldAfterProbe=0"
            << " openxrReleaseFrame=0"
            << " openxrSessionCreatedFrame=0"
            << " openxrHdc=" << HexPointer(latestHdc_)
            << " openxrGlContext=" << HexPointer(latestGlContext_);
        return oss.str();
    }

    std::string ViewSummaryString() const
    {
        return "openxrPoseValid=0 openxrPoseGameFrame=0 openxrViewStateFlags=0x0 openxrIpdMeters=0 openxrHeadPosition=0,0,0 openxrHeadOrientation=0,0,0,1";
    }

    bool GetLatestHeadPose(OpenXRHeadPose& pose) const
    {
        pose = {};
        return false;
    }

    bool GetLatestStereoViews(OpenXRStereoViewSnapshot& views) const
    {
        views = {};
        return false;
    }

    bool GetLatestInput(OpenXRInputSnapshot& input) const
    {
        input = {};
        return false;
    }

    void SetStereoSubmissionEnabled(bool) {}
    bool MarkRenderedStereoEye(uint32_t, const OpenXREyeView&) { return false; }

private:
    void LogUnavailableLocked()
    {
        if (!enabled_ || unavailableLogged_) {
            return;
        }
        unavailableLogged_ = true;
        Logger::Instance().Write(
            LogLevel::Error,
            "openxr_bootstrap unavailable: build_without_openxr; use build-openxr\\Release\\somavr.dll or set [OpenXR] Probe=0");
    }

    mutable std::mutex mutex_;
    bool enabled_ = false;
    bool sessionProbeEnabled_ = true;
    bool releaseAfterProbeEnabled_ = true;
    uint64_t bootstrapFrame_ = 0;
    uint64_t holdFrames_ = 0;
    bool manualStartEnabled_ = false;
    bool manualStartArmed_ = false;
    bool frameSubmitEnabled_ = false;
    bool inputEnabled_ = false;
    int inputLogInterval_ = 120;
    bool mirrorBackbufferEnabled_ = true;
    int resolutionScalePercent_ = 100;
    bool unavailableLogged_ = false;
    HDC latestHdc_ = nullptr;
    HGLRC latestGlContext_ = nullptr;
};

#endif

OpenXRRuntime::OpenXRRuntime() : impl_(std::make_unique<Impl>()) {}
OpenXRRuntime::~OpenXRRuntime() = default;

void OpenXRRuntime::Configure(
    bool enabled,
    bool sessionProbe,
    bool releaseAfterProbe,
    uint64_t bootstrapFrame,
    uint64_t holdFrames,
    bool manualStart,
    bool frameSubmit,
    bool mirrorBackbuffer,
    int resolutionScalePercent,
    bool inputEnabled,
    int inputLogInterval)
{
    impl_->Configure(
        enabled,
        sessionProbe,
        releaseAfterProbe,
        bootstrapFrame,
        holdFrames,
        manualStart,
        frameSubmit,
        mirrorBackbuffer,
        resolutionScalePercent,
        inputEnabled,
        inputLogInterval);
}

void OpenXRRuntime::OnOpenGLContext(HDC deviceContext, HGLRC glContext)
{
    impl_->OnOpenGLContext(deviceContext, glContext);
}

void OpenXRRuntime::OnFrameBoundary(HDC deviceContext, HGLRC glContext, uint64_t frameIndex)
{
    impl_->OnFrameBoundary(deviceContext, glContext, frameIndex);
}

bool OpenXRRuntime::RequestManualStart()
{
    return impl_->RequestManualStart();
}

void OpenXRRuntime::Shutdown()
{
    impl_->Shutdown();
}

std::string OpenXRRuntime::SummaryString() const
{
    return impl_->SummaryString();
}

std::string OpenXRRuntime::ViewSummaryString() const
{
    return impl_->ViewSummaryString();
}

bool OpenXRRuntime::GetLatestHeadPose(OpenXRHeadPose& pose) const
{
    return impl_->GetLatestHeadPose(pose);
}

bool OpenXRRuntime::GetLatestStereoViews(OpenXRStereoViewSnapshot& views) const
{
    return impl_->GetLatestStereoViews(views);
}

bool OpenXRRuntime::GetLatestInput(OpenXRInputSnapshot& input) const
{
    return impl_->GetLatestInput(input);
}

void OpenXRRuntime::SetStereoSubmissionEnabled(bool enabled)
{
    impl_->SetStereoSubmissionEnabled(enabled);
}

bool OpenXRRuntime::MarkRenderedStereoEye(uint32_t eyeIndex, const OpenXREyeView& view)
{
    return impl_->MarkRenderedStereoEye(eyeIndex, view);
}

} // namespace somavr
