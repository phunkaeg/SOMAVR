#include "OpenXRRuntime.h"

#include "Logger.h"
#include "HPLCameraBridge.h"
#include "HPLHudMath.h"
#include "OpenXRGLBridge.h"
#include "OpenXRComfortVignetteMath.h"
#include "OpenXRStatusPanelMath.h"

#include <Windows.h>
#include <Unknwn.h>
#include <gl/GL.h>

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
#include <utility>
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
#include "OpenXRDepthMath.h"
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

constexpr uint64_t kViewConfigurationCheckIntervalFrames = 300;

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
        const std::string& desktopMirrorEye,
        const std::string& desktopMirrorAspect,
        bool depthCompositionProbe,
        bool depthCompositionSubmit,
        int resolutionScalePercent,
        const std::string& referenceSpace,
        bool inputEnabled,
        int inputLogInterval,
        bool recoveryEnabled,
        int recoveryDelayFrames,
        int trackingHoldFrames,
        int trackingRecoveryBlackoutFrames,
        bool hudLayerEnabled,
        const std::string& hudShape,
        float hudCylinderAngleDegrees,
        int hudWidthPixels,
        int hudHeightPixels,
        float hudDistanceMeters,
        float hudWidthMeters,
        float hudVerticalOffsetMeters,
        int hudMaxAgeFrames,
        bool hudSuppressCenterCrosshair,
        int hudCrosshairClearRadiusPixels,
        bool interactionReticleEnabled,
        bool interactionReticleSemanticEnabled,
        bool interactionReticleNativeIconsEnabled,
        int interactionReticleSizePixels,
        float interactionReticleAngularSizeDegrees,
        float interactionReticleMinSizeMeters,
        float interactionReticleMaxSizeMeters,
        float interactionReticleMinDistanceMeters,
        float interactionReticleMaxDistanceMeters,
        int interactionReticleMaxAgeFrames,
        bool statusPanelEnabled,
        int statusPanelWidthPixels,
        int statusPanelHeightPixels,
        float statusPanelDistanceMeters,
        float statusPanelWidthMeters,
        float statusPanelVerticalOffsetMeters,
        const OpenXRComfortVignetteSettings& comfortVignette)
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
        desktopMirrorEye_ = desktopMirrorEye == "left" || desktopMirrorEye == "right"
            ? desktopMirrorEye : "native";
        desktopMirrorEyeIndex_ = desktopMirrorEye_ == "left" ? 0
            : desktopMirrorEye_ == "right" ? 1 : -1;
        desktopMirrorAspect_ = desktopMirrorAspect == "fill" || desktopMirrorAspect == "stretch"
            ? desktopMirrorAspect : "fit";
        desktopMirrorAspectMode_ = desktopMirrorAspect_ == "fill"
            ? spectator_math::AspectMode::Fill
            : desktopMirrorAspect_ == "stretch"
                ? spectator_math::AspectMode::Stretch
                : spectator_math::AspectMode::Fit;
        depthCompositionProbeEnabled_ = depthCompositionProbe;
        depthCompositionSubmitEnabled_ = depthCompositionSubmit;
        depthExtensionAvailable_ = false;
        depthExtensionEnabled_ = false;
        depthCapabilityLogged_ = false;
        resolutionScalePercent_ = std::clamp(resolutionScalePercent, 25, 200);
        requestedReferenceSpace_ = referenceSpace == "stage" ? "stage" : "local";
        inputEnabled_ = inputEnabled;
        inputLogInterval_ = std::max(inputLogInterval, 1);
        recoveryEnabled_ = recoveryEnabled;
        recoveryDelayFrames_ = std::max(recoveryDelayFrames, 1);
        trackingHoldFrames_ = std::max(trackingHoldFrames, 0);
        trackingRecoveryBlackoutFrames_ = std::max(trackingRecoveryBlackoutFrames, 0);
        hudLayerEnabled_ = hudLayerEnabled;
        hudCylinderRequested_ = hudShape == "cylinder";
        hudCylinderAngleDegrees_ = std::clamp(hudCylinderAngleDegrees, 15.0f, 180.0f);
        hudCylinderExtensionAvailable_ = false;
        hudCylinderExtensionEnabled_ = false;
        hudCylinderSubmissionDisabled_ = false;
        hudWidthPixels_ = std::clamp(hudWidthPixels, 256, 4096);
        hudHeightPixels_ = std::clamp(hudHeightPixels, 256, 4096);
        hudDistanceMeters_ = std::clamp(hudDistanceMeters, 0.25f, 10.0f);
        hudWidthMeters_ = std::clamp(hudWidthMeters, 0.25f, 10.0f);
        hudVerticalOffsetMeters_ = std::clamp(hudVerticalOffsetMeters, -5.0f, 5.0f);
        hudMaxAgeFrames_ = std::clamp(hudMaxAgeFrames, 0, 30);
        hudSuppressCenterCrosshair_ = hudSuppressCenterCrosshair;
        hudCrosshairClearRadiusPixels_ = std::clamp(hudCrosshairClearRadiusPixels, 4, 256);
        interactionReticleEnabled_ = interactionReticleEnabled;
        interactionReticleSemanticEnabled_ = interactionReticleSemanticEnabled;
        interactionReticleNativeIconsEnabled_ = interactionReticleNativeIconsEnabled;
        interactionReticleSizePixels_ = std::clamp(interactionReticleSizePixels, 32, 512);
        interactionReticleAngularSizeDegrees_ = std::clamp(interactionReticleAngularSizeDegrees, 0.1f, 5.0f);
        interactionReticleMinSizeMeters_ = std::clamp(interactionReticleMinSizeMeters, 0.001f, 0.5f);
        interactionReticleMaxSizeMeters_ = std::clamp(
            interactionReticleMaxSizeMeters,
            interactionReticleMinSizeMeters_,
            1.0f);
        interactionReticleMinDistanceMeters_ = std::clamp(interactionReticleMinDistanceMeters, 0.01f, 10.0f);
        interactionReticleMaxDistanceMeters_ = std::clamp(
            interactionReticleMaxDistanceMeters,
            interactionReticleMinDistanceMeters_,
            100.0f);
        interactionReticleMaxAgeFrames_ = std::clamp(interactionReticleMaxAgeFrames, 0, 30);
        interactionReticleSubmissionSuspended_ = false;
        interactionReticleConsecutiveFailures_ = 0;
        interactionReticleState_ = {};
        statusPanelEnabled_ = statusPanelEnabled;
        statusPanelWidthPixels_ = std::clamp(statusPanelWidthPixels, 512, 4096);
        statusPanelHeightPixels_ = std::clamp(statusPanelHeightPixels, 256, 4096);
        statusPanelDistanceMeters_ = std::clamp(statusPanelDistanceMeters, 0.25f, 10.0f);
        statusPanelWidthMeters_ = std::clamp(statusPanelWidthMeters, 0.25f, 5.0f);
        statusPanelVerticalOffsetMeters_ = std::clamp(statusPanelVerticalOffsetMeters, -5.0f, 5.0f);
        statusPanelState_ = {};
        comfortVignetteConfigured_ = comfortVignette.enabled;
        comfortVignetteEnabled_ = comfortVignette.enabled;
        comfortVignetteSizePixels_ = std::clamp(comfortVignette.sizePixels, 32, 1024);
        comfortVignetteDistanceMeters_ = std::clamp(
            comfortVignette.distanceMeters, 0.10f, 2.0f);
        comfortVignetteWidthMeters_ = std::clamp(
            comfortVignette.widthMeters, 0.25f, 4.0f);
        comfortVignetteStrength_ = std::clamp(comfortVignette.strength, 0.0f, 1.0f);
        comfortVignetteInnerRadius_ = std::clamp(
            comfortVignette.innerRadius, 0.0f, 0.98f);
        comfortVignetteFadeMilliseconds_ = std::clamp(
            comfortVignette.fadeMilliseconds, 0, 2000);
        comfortVignetteMaxMotionAgeFrames_ = std::clamp(
            comfortVignette.maxMotionAgeFrames, 0, 120);
        comfortVignetteTarget_ = 0.0f;
        comfortVignetteLevel_ = 0.0f;
        comfortVignetteMotionFrame_ = 0;
        hudRuntimeVisible_ = hudLayerEnabled_;
        interactionReticleRuntimeVisible_ = interactionReticleEnabled_;
        hudSubmissionSuspended_ = false;
        hudConsecutiveFailures_ = 0;
        comfortVignetteLevel_ = 0.0f;
        manualStartArmed_ = false;
        manualStartLogged_ = false;
        manualStartKeyDown_ = false;
        manualStartFrame_ = 0;
        unavailableLogged_ = false;
        desktopMirrorFrames_ = 0;
        desktopMirrorFailures_ = 0;
        Logger::Instance().Write(
            LogLevel::Info,
            "openxr_status_panel_config enabled=%d size=%dx%d distance=%.3f widthMeters=%.3f verticalOffset=%.3f key=F1 controllerChord=menu_plus_secondary",
            statusPanelEnabled_ ? 1 : 0,
            statusPanelWidthPixels_,
            statusPanelHeightPixels_,
            statusPanelDistanceMeters_,
            statusPanelWidthMeters_,
            statusPanelVerticalOffsetMeters_);
        Logger::Instance().Write(
            LogLevel::Info,
            "openxr_comfort_vignette_config configured=%d enabled=%d size=%d distance=%.3f widthMeters=%.3f strength=%.3f innerRadius=%.3f fadeMs=%d maxMotionAgeFrames=%d",
            comfortVignetteConfigured_ ? 1 : 0,
            comfortVignetteEnabled_ ? 1 : 0,
            comfortVignetteSizePixels_,
            comfortVignetteDistanceMeters_,
            comfortVignetteWidthMeters_,
            comfortVignetteStrength_,
            comfortVignetteInnerRadius_,
            comfortVignetteFadeMilliseconds_,
            comfortVignetteMaxMotionAgeFrames_);
        Logger::Instance().Write(
            LogLevel::Info,
            "openxr_config buildOpenXR=1 enabled=%d sessionProbe=%d releaseAfterProbe=%d bootstrapFrame=%llu holdFrames=%llu manualStart=%d key=F8 frameSubmit=%d mirrorBackbuffer=%d desktopMirrorEye=%s desktopMirrorAspect=%s depth={probe=%d submit=%d} resolutionScalePercent=%d referenceSpace=%s input=%d inputLogInterval=%d recovery=%d recoveryDelayFrames=%d trackingHoldFrames=%d trackingRecoveryBlackoutFrames=%d hud={enabled=%d shape=%s cylinderAngleDegrees=%.3f size=%dx%d distance=%.3f widthMeters=%.3f verticalOffset=%.3f maxAgeFrames=%d suppressCenterCrosshair=%d crosshairClearRadiusPixels=%d} reticle={enabled=%d semantic=%d nativeIcons=%d pixels=%d angularDeg=%.3f sizeMeters=%.4f..%.4f distanceMeters=%.3f..%.3f maxAgeFrames=%d}",
            enabled_ ? 1 : 0,
            sessionProbeEnabled_ ? 1 : 0,
            releaseAfterProbeEnabled_ ? 1 : 0,
            static_cast<unsigned long long>(bootstrapFrame_),
            static_cast<unsigned long long>(holdFrames_),
            manualStartEnabled_ ? 1 : 0,
            frameSubmitEnabled_ ? 1 : 0,
            mirrorBackbufferEnabled_ ? 1 : 0,
            desktopMirrorEye_.c_str(),
            desktopMirrorAspect_.c_str(),
            depthCompositionProbeEnabled_ ? 1 : 0,
            depthCompositionSubmitEnabled_ ? 1 : 0,
            resolutionScalePercent_,
            requestedReferenceSpace_.c_str(),
            inputEnabled_ ? 1 : 0,
            inputLogInterval_,
            recoveryEnabled_ ? 1 : 0,
            recoveryDelayFrames_,
            trackingHoldFrames_,
            trackingRecoveryBlackoutFrames_,
            hudLayerEnabled_ ? 1 : 0,
            hudCylinderRequested_ ? "cylinder" : "quad",
            hudCylinderAngleDegrees_,
            hudWidthPixels_,
            hudHeightPixels_,
            hudDistanceMeters_,
            hudWidthMeters_,
            hudVerticalOffsetMeters_,
            hudMaxAgeFrames_,
            hudSuppressCenterCrosshair_ ? 1 : 0,
            hudCrosshairClearRadiusPixels_,
            interactionReticleEnabled_ ? 1 : 0,
            interactionReticleSemanticEnabled_ ? 1 : 0,
            interactionReticleNativeIconsEnabled_ ? 1 : 0,
            interactionReticleSizePixels_,
            interactionReticleAngularSizeDegrees_,
            interactionReticleMinSizeMeters_,
            interactionReticleMaxSizeMeters_,
            interactionReticleMinDistanceMeters_,
            interactionReticleMaxDistanceMeters_,
            interactionReticleMaxAgeFrames_);

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
        const HPLCameraBridgeStatus cameraStatus = GetHPLCameraBridgeStatus();
        std::lock_guard lock(mutex_);
        currentGameFrame_ = frameIndex;
        latestHdc_ = deviceContext;
        latestGlContext_ = glContext;
        if (!enabled_ || failed_) {
            return;
        }

        const bool graphicsBindingChanged = session_ != XR_NULL_HANDLE
            && sessionGlContext_ != nullptr
            && glContext != nullptr
            && (sessionGlContext_ != glContext || sessionHdc_ != deviceContext);
        if (graphicsBindingChanged) {
            ++glContextChangeEvents_;
            Logger::Instance().Write(
                recoveryEnabled_ ? LogLevel::Warn : LogLevel::Error,
                "openxr_graphics_binding changed frame=%llu oldHdc=%s oldHglrc=%s newHdc=%s newHglrc=%s recovery=%d events=%llu",
                static_cast<unsigned long long>(frameIndex),
                HexPointer(sessionHdc_).c_str(),
                HexPointer(sessionGlContext_).c_str(),
                HexPointer(deviceContext).c_str(),
                HexPointer(glContext).c_str(),
                recoveryEnabled_ ? 1 : 0,
                static_cast<unsigned long long>(glContextChangeEvents_));
            if (recoveryEnabled_) {
                // Old-context GL names must not be deleted while the replacement
                // context is current; the owning context will release them.
                glBridge_.Shutdown(false);
                recoveryRequested_ = true;
                BeginRuntimeRecoveryLocked(frameIndex);
            } else {
                frameSubmitFailed_ = true;
            }
            return;
        }

        UpdateManualStartLocked(frameIndex);

        if (session_ != XR_NULL_HANDLE) {
            PollEventsLocked(frameIndex);
            if (recoveryRequested_) {
                BeginRuntimeRecoveryLocked(frameIndex);
                return;
            }
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
            if (failed_ && recoveryEnabled_ && runtimeRecoveries_ != 0) {
                BeginRuntimeRecoveryLocked(frameIndex);
                return;
            }
        }

        LogDepthCapabilityLocked(glContext, frameIndex, cameraStatus);

        if (sessionRunning_ && frameResourcesReady_ && !frameSubmitFailed_
            && !RefreshViewConfigurationLocked(frameIndex)) {
            frameSubmitFailed_ = true;
            return;
        }

        if (sessionRunning_ && frameResourcesReady_ && !frameSubmitFailed_) {
            SubmitFrameLocked(frameIndex, cameraStatus);
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
            sessionHdc_ = nullptr;
            sessionGlContext_ = nullptr;
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
        recoveryRequested_ = false;
        comfortBlackoutUntilFrame_ = 0;
        presentationBlackoutActive_ = false;
        trackingDegraded_ = false;
        trackingLost_ = false;
        hudSubmissionSuspended_ = false;
        hudConsecutiveFailures_ = 0;
        interactionReticleState_ = {};
        interactionReticleSubmissionSuspended_ = false;
        interactionReticleConsecutiveFailures_ = 0;
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
            << " openxrDesktopMirrorEye=" << desktopMirrorEye_
            << " openxrDesktopMirrorAspect=" << desktopMirrorAspect_
            << " openxrDesktopMirrorFrames=" << static_cast<unsigned long long>(desktopMirrorFrames_)
            << " openxrDesktopMirrorFailures=" << static_cast<unsigned long long>(desktopMirrorFailures_)
            << " openxrDepthCompositionProbe=" << (depthCompositionProbeEnabled_ ? 1 : 0)
            << " openxrDepthCompositionSubmit=" << (depthCompositionSubmitEnabled_ ? 1 : 0)
            << " openxrDepthExtensionAvailable=" << (depthExtensionAvailable_ ? 1 : 0)
            << " openxrDepthExtensionEnabled=" << (depthExtensionEnabled_ ? 1 : 0)
            << " openxrDepthCapabilityLogged=" << (depthCapabilityLogged_ ? 1 : 0)
            << " openxrDepthSwapchainsReady=" << (glBridge_.DepthSwapchainsReady() ? 1 : 0)
            << " openxrDepthCachesReady=" << (glBridge_.DepthCachesReady() ? 1 : 0)
            << " openxrDepthSubmittedFrames=" << static_cast<unsigned long long>(depthSubmittedFrameCount_)
            << " openxrDepthSubmissionFailures=" << static_cast<unsigned long long>(depthSubmissionFailures_)
            << " openxrResolutionScalePercent=" << resolutionScalePercent_
            << " openxrReferenceSpaceRequested=" << requestedReferenceSpace_
            << " openxrReferenceSpaceSelected=" << ReferenceSpaceTypeName(selectedReferenceSpace_)
            << " openxrFrameResourcesReady=" << (frameResourcesReady_ ? 1 : 0)
            << " openxrFrameSubmitFailed=" << (frameSubmitFailed_ ? 1 : 0)
            << " openxrRecoveryEnabled=" << (recoveryEnabled_ ? 1 : 0)
            << " openxrRecoveryPending=" << (recoveryRequested_ ? 1 : 0)
            << " openxrRecoveries=" << static_cast<unsigned long long>(runtimeRecoveries_)
            << " openxrGlContextChanges=" << static_cast<unsigned long long>(glContextChangeEvents_)
            << " openxrViewResourceChecks=" << static_cast<unsigned long long>(viewResourceChecks_)
            << " openxrViewResourceRebuilds=" << static_cast<unsigned long long>(viewResourceRebuilds_)
            << " openxrTrackingHoldFrames=" << trackingHoldFrames_
            << " openxrTrackingRecoveryBlackoutFrames=" << trackingRecoveryBlackoutFrames_
            << " openxrTrackingDegraded=" << (trackingDegraded_ ? 1 : 0)
            << " openxrTrackingLost=" << (trackingLost_ ? 1 : 0)
            << " openxrTrackingInvalidFrames=" << static_cast<unsigned long long>(trackingInvalidFrames_)
            << " openxrTrackingLossEvents=" << static_cast<unsigned long long>(trackingLossEvents_)
            << " openxrTrackingRestoreEvents=" << static_cast<unsigned long long>(trackingRestoreEvents_)
            << " openxrStereoCacheInvalidations=" << static_cast<unsigned long long>(stereoCacheInvalidations_)
            << " openxrComfortBlackoutUntilFrame=" << static_cast<unsigned long long>(comfortBlackoutUntilFrame_)
            << " openxrComfortBlackoutRequests=" << static_cast<unsigned long long>(comfortBlackoutRequests_)
            << " openxrComfortBlackoutFrames=" << static_cast<unsigned long long>(comfortBlackoutFrames_)
            << " openxrComfortVignetteConfigured=" << (comfortVignetteConfigured_ ? 1 : 0)
            << " openxrComfortVignetteEnabled=" << (comfortVignetteEnabled_ ? 1 : 0)
            << " openxrComfortVignetteReady=" << (glBridge_.ComfortVignetteReady() ? 1 : 0)
            << " openxrComfortVignetteLevel=" << comfortVignetteLevel_
            << " openxrComfortVignetteSubmittedFrames="
                << static_cast<unsigned long long>(comfortVignetteSubmittedFrames_)
            << " openxrComfortVignetteSubmissionFailures="
                << static_cast<unsigned long long>(comfortVignetteSubmissionFailures_)
            << " openxrPresentationBlackout=" << (presentationBlackoutActive_ ? 1 : 0)
            << " openxrPresentationBlackoutTransitions=" << static_cast<unsigned long long>(presentationBlackoutTransitions_)
            << " openxrPresentationBlackoutFrames=" << static_cast<unsigned long long>(presentationBlackoutFrames_)
            << " openxrSessionRunning=" << (sessionRunning_ ? 1 : 0)
            << " openxrStereoSubmission=" << (stereoSubmissionEnabled_ ? 1 : 0)
            << " openxrStereoCapturedEyes=" << static_cast<unsigned long long>(stereoCapturedEyeCount_)
            << " openxrStereoSubmittedFrames=" << static_cast<unsigned long long>(stereoSubmittedFrameCount_)
            << " openxrHudLayer=" << (hudLayerEnabled_ ? 1 : 0)
            << " openxrHudShapeRequested=" << (hudCylinderRequested_ ? "cylinder" : "quad")
            << " openxrHudShapeEffective=" << (hudCylinderRequested_
                && hudCylinderExtensionEnabled_ && !hudCylinderSubmissionDisabled_
                    ? "cylinder" : "quad")
            << " openxrHudCylinderAngleDegrees=" << hudCylinderAngleDegrees_
            << " openxrHudCylinderExtensionAvailable=" << (hudCylinderExtensionAvailable_ ? 1 : 0)
            << " openxrHudCylinderExtensionEnabled=" << (hudCylinderExtensionEnabled_ ? 1 : 0)
            << " openxrHudCylinderSubmissionDisabled=" << (hudCylinderSubmissionDisabled_ ? 1 : 0)
            << " openxrHudRuntimeVisible=" << (hudRuntimeVisible_ ? 1 : 0)
            << " openxrHudReady=" << (glBridge_.HudReady() ? 1 : 0)
            << " openxrHudSuspended=" << (hudSubmissionSuspended_ ? 1 : 0)
            << " openxrHudCaptureStarts=" << static_cast<unsigned long long>(hudCaptureStarts_)
            << " openxrHudCaptureCompletions=" << static_cast<unsigned long long>(hudCaptureCompletions_)
            << " openxrHudSubmittedFrames=" << static_cast<unsigned long long>(hudSubmittedFrames_)
            << " openxrHudSubmissionFailures=" << static_cast<unsigned long long>(hudSubmissionFailures_)
            << " openxrInteractionReticle=" << (interactionReticleEnabled_ ? 1 : 0)
            << " openxrInteractionReticleRuntimeVisible=" << (interactionReticleRuntimeVisible_ ? 1 : 0)
            << " openxrInteractionReticleSemantic=" << (interactionReticleSemanticEnabled_ ? 1 : 0)
            << " openxrInteractionReticleNativeIcons=" << (interactionReticleNativeIconsEnabled_ ? 1 : 0)
            << " openxrInteractionReticleReady=" << (glBridge_.InteractionReticleReady() ? 1 : 0)
            << " openxrInteractionReticleValid=" << (interactionReticleState_.valid ? 1 : 0)
            << " openxrInteractionReticleSemanticValid=" << (interactionReticleState_.semanticValid ? 1 : 0)
            << " openxrInteractionReticleSemanticState=" << interactionReticleState_.semanticState
            << " openxrInteractionReticleSuspended=" << (interactionReticleSubmissionSuspended_ ? 1 : 0)
            << " openxrInteractionReticleUpdates=" << static_cast<unsigned long long>(interactionReticleUpdates_)
            << " openxrInteractionReticleSemanticUpdates=" << static_cast<unsigned long long>(interactionReticleSemanticUpdates_)
            << " openxrInteractionReticleSemanticRejects=" << static_cast<unsigned long long>(interactionReticleSemanticRejects_)
            << " openxrInteractionReticleClears=" << static_cast<unsigned long long>(interactionReticleClears_)
            << " openxrInteractionReticleExpired=" << static_cast<unsigned long long>(interactionReticleExpired_)
            << " openxrInteractionReticleSubmittedFrames=" << static_cast<unsigned long long>(interactionReticleSubmittedFrames_)
            << " openxrInteractionReticleSubmissionFailures=" << static_cast<unsigned long long>(interactionReticleSubmissionFailures_)
            << " openxrStatusPanel=" << (statusPanelEnabled_ ? 1 : 0)
            << " openxrStatusPanelReady=" << (glBridge_.StatusPanelReady() ? 1 : 0)
            << " openxrStatusPanelVisible=" << (statusPanelState_.visible ? 1 : 0)
            << " openxrStatusPanelSubmittedFrames=" << static_cast<unsigned long long>(statusPanelSubmittedFrames_)
            << " openxrStatusPanelSubmissionFailures=" << static_cast<unsigned long long>(statusPanelSubmissionFailures_)
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
            << "openxrPoseValid=" << (PoseUsableLocked() ? 1 : 0)
            << " openxrPoseRawValid=" << (latestPoseValid_ ? 1 : 0)
            << " openxrTrackingDegraded=" << (trackingDegraded_ ? 1 : 0)
            << " openxrTrackingLost=" << (trackingLost_ ? 1 : 0)
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
        pose.valid = PoseUsableLocked();
        pose.orientationTracked = !trackingDegraded_
            && (latestViewStateFlags_ & XR_VIEW_STATE_ORIENTATION_TRACKED_BIT) != 0;
        pose.positionTracked = !trackingDegraded_
            && (latestViewStateFlags_ & XR_VIEW_STATE_POSITION_TRACKED_BIT) != 0;
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
        if (!PoseUsableLocked() || locatedViews_.size() < 2) {
            return false;
        }

        views.valid = true;
        views.gameFrame = latestPoseGameFrame_;
        views.head.valid = true;
        views.head.orientationTracked = !trackingDegraded_ &&
            (latestViewStateFlags_ & XR_VIEW_STATE_ORIENTATION_TRACKED_BIT) != 0;
        views.head.positionTracked = !trackingDegraded_ &&
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

    void SetInteractionReticle(const OpenXRInteractionReticleState& state)
    {
        std::lock_guard lock(mutex_);
        if (!interactionReticleEnabled_
            || !state.valid
            || state.handIndex >= 2
            || !state.aimPose.valid
            || !state.aimPose.orientationTracked
            || !state.aimPose.positionTracked
            || !std::isfinite(state.distanceMeters)
            || state.distanceMeters < interactionReticleMinDistanceMeters_
            || state.distanceMeters > interactionReticleMaxDistanceMeters_) {
            if (interactionReticleState_.valid) {
                interactionReticleState_ = {};
                ++interactionReticleClears_;
            }
            return;
        }
        interactionReticleState_ = state;
        interactionReticleState_.semanticValid = !interactionReticleSemanticEnabled_;
        interactionReticleState_.semanticState = interactionReticleSemanticEnabled_ ? 0 : 1;
        ++interactionReticleUpdates_;
    }

    void SetInteractionReticleSemantic(int crosshairState)
    {
        std::lock_guard lock(mutex_);
        if (!interactionReticleEnabled_ || !interactionReticleSemanticEnabled_) {
            return;
        }
        if (!interactionReticleState_.valid || crosshairState <= 0 || crosshairState >= 35) {
            if (interactionReticleState_.valid && crosshairState == 0) {
                interactionReticleState_ = {};
                ++interactionReticleClears_;
            } else if (interactionReticleState_.valid) {
                interactionReticleState_.semanticValid = false;
                interactionReticleState_.semanticState = 0;
            }
            ++interactionReticleSemanticRejects_;
            return;
        }
        interactionReticleState_.semanticValid = true;
        interactionReticleState_.semanticState = crosshairState;
        ++interactionReticleSemanticUpdates_;
    }

    void ClearInteractionReticle()
    {
        std::lock_guard lock(mutex_);
        if (interactionReticleState_.valid) {
            interactionReticleState_ = {};
            ++interactionReticleClears_;
        }
    }

    void SetStatusPanel(const OpenXRStatusPanelState& state)
    {
        std::lock_guard lock(mutex_);
        if (!statusPanelEnabled_) return;
        statusPanelState_ = state;
        statusPanelState_.selectedAction = std::clamp(
            statusPanelState_.selectedAction, 0, status_panel_math::kActionCount - 1);
    }

    void SetHudRuntimeVisible(bool visible)
    {
        std::lock_guard lock(mutex_);
        hudRuntimeVisible_ = hudLayerEnabled_ && visible;
        Logger::Instance().Write(LogLevel::Info,
            "openxr_hud runtime_visible=%d configured=%d",
            hudRuntimeVisible_ ? 1 : 0, hudLayerEnabled_ ? 1 : 0);
    }

    bool ToggleHudLayerShape()
    {
        std::lock_guard lock(mutex_);
        if (!hudLayerEnabled_ || !hudCylinderExtensionEnabled_) {
            Logger::Instance().Write(
                LogLevel::Warn,
                "openxr_hud shape_toggle unavailable configured=%d extensionEnabled=%d",
                hudLayerEnabled_ ? 1 : 0,
                hudCylinderExtensionEnabled_ ? 1 : 0);
            return false;
        }
        hudCylinderRequested_ = !hudCylinderRequested_;
        if (hudCylinderRequested_) hudCylinderSubmissionDisabled_ = false;
        Logger::Instance().Write(
            LogLevel::Info,
            "openxr_hud shape_toggle effective=%s angleDegrees=%.3f",
            hudCylinderRequested_ ? "cylinder" : "quad",
            hudCylinderAngleDegrees_);
        return true;
    }

    OpenXRHudLayerShapeStatus GetHudLayerShapeStatus() const
    {
        std::lock_guard lock(mutex_);
        OpenXRHudLayerShapeStatus status;
        status.cylinderAvailable = hudLayerEnabled_
            && hudCylinderExtensionEnabled_
            && !hudCylinderSubmissionDisabled_;
        status.cylinderActive = hudCylinderRequested_ && status.cylinderAvailable;
        return status;
    }

    void SetComfortMotionIntensity(float intensity, uint64_t gameFrame)
    {
        std::lock_guard lock(mutex_);
        if (!std::isfinite(intensity)) intensity = 0.0f;
        comfortVignetteTarget_ = std::clamp(intensity, 0.0f, 1.0f);
        comfortVignetteMotionFrame_ = gameFrame;
    }

    bool ToggleComfortVignette()
    {
        std::lock_guard lock(mutex_);
        if (!glBridge_.ComfortVignetteReady()) {
            Logger::Instance().Write(
                LogLevel::Warn,
                "openxr_comfort_vignette toggle_unavailable resourcesReady=0 configured=%d",
                comfortVignetteConfigured_ ? 1 : 0);
            return false;
        }
        comfortVignetteEnabled_ = !comfortVignetteEnabled_;
        if (!comfortVignetteEnabled_) {
            comfortVignetteTarget_ = 0.0f;
            comfortVignetteLevel_ = 0.0f;
        }
        Logger::Instance().Write(
            LogLevel::Info,
            "openxr_comfort_vignette toggle enabled=%d",
            comfortVignetteEnabled_ ? 1 : 0);
        return true;
    }

    OpenXRComfortVignetteStatus GetComfortVignetteStatus() const
    {
        std::lock_guard lock(mutex_);
        OpenXRComfortVignetteStatus status;
        status.available = glBridge_.ComfortVignetteReady();
        status.enabled = comfortVignetteEnabled_;
        status.active = comfortVignetteEnabled_ && comfortVignetteLevel_ > 0.001f;
        status.level = comfortVignetteLevel_;
        return status;
    }

    void SetInteractionReticleRuntimeVisible(bool visible)
    {
        std::lock_guard lock(mutex_);
        interactionReticleRuntimeVisible_ = interactionReticleEnabled_ && visible;
        Logger::Instance().Write(LogLevel::Info,
            "openxr_interaction_reticle runtime_visible=%d configured=%d",
            interactionReticleRuntimeVisible_ ? 1 : 0, interactionReticleEnabled_ ? 1 : 0);
    }

    bool RequestHapticPulse(uint32_t hand, float amplitude, int durationMs, const char* reason)
    {
        std::lock_guard lock(mutex_);
        if (!inputEnabled_ || !sessionRunning_ || sessionState_ != XR_SESSION_STATE_FOCUSED) {
            return false;
        }
        const bool applied = input_.ApplyHaptic(session_, hand, amplitude, durationMs);
        if (applied) {
            const bool gameplay = reason != nullptr
                && std::strcmp(reason, "native_gameplay_rumble") == 0;
            const uint64_t gameplayLog = gameplay ? ++gameplayHapticRequestCount_ : 0;
            if (!gameplay || gameplayLog <= 8 || gameplayLog % 64 == 0) {
                Logger::Instance().Write(
                    LogLevel::Info,
                    "openxr_haptic applied hand=%u amplitude=%.3f durationMs=%d reason=%s frame=%llu",
                    hand,
                    std::clamp(amplitude, 0.0f, 1.0f),
                    std::max(durationMs, 1),
                    reason != nullptr ? reason : "unspecified",
                    static_cast<unsigned long long>(currentGameFrame_));
            }
        }
        return applied;
    }

    bool StopHaptic(uint32_t hand, const char* reason)
    {
        std::lock_guard lock(mutex_);
        if (!inputEnabled_ || !sessionRunning_ || sessionState_ != XR_SESSION_STATE_FOCUSED) {
            return false;
        }
        const bool stopped = input_.StopHaptic(session_, hand);
        if (stopped) {
            Logger::Instance().Write(LogLevel::Info,
                "openxr_haptic stopped hand=%u reason=%s frame=%llu",
                hand, reason != nullptr ? reason : "unspecified",
                static_cast<unsigned long long>(currentGameFrame_));
        }
        return stopped;
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

    bool CapturePendingStereoEye(uint64_t frameIndex, const char* source)
    {
        std::lock_guard lock(mutex_);
        return CapturePendingStereoEyeLocked(frameIndex, source);
    }

    void InvalidateStereoCaches(const char* reason)
    {
        std::lock_guard lock(mutex_);
        glBridge_.InvalidateStereoCaches();
        glBridge_.InvalidateHudCapture();
        pendingRenderedEyeValid_ = false;
        renderedStereoViewValid_[0] = false;
        renderedStereoViewValid_[1] = false;
        stereoWarmupLogged_ = false;
        ++stereoCacheInvalidations_;
        Logger::Instance().Write(
            LogLevel::Warn,
            "openxr_stereo_cache invalidated reason=%s count=%llu frame=%llu",
            reason != nullptr ? reason : "unspecified",
            static_cast<unsigned long long>(stereoCacheInvalidations_),
            static_cast<unsigned long long>(currentGameFrame_));
    }

    void RequestComfortBlackout(uint32_t frames, const char* reason)
    {
        if (frames == 0) {
            return;
        }
        std::lock_guard lock(mutex_);
        const uint64_t requestedUntil = currentGameFrame_ + frames;
        comfortBlackoutUntilFrame_ = std::max(comfortBlackoutUntilFrame_, requestedUntil);
        ++comfortBlackoutRequests_;
        Logger::Instance().Write(
            LogLevel::Info,
            "openxr_comfort_blackout requested reason=%s frames=%u currentFrame=%llu untilFrame=%llu requests=%llu",
            reason != nullptr ? reason : "unspecified",
            frames,
            static_cast<unsigned long long>(currentGameFrame_),
            static_cast<unsigned long long>(comfortBlackoutUntilFrame_),
            static_cast<unsigned long long>(comfortBlackoutRequests_));
    }

    void SetPresentationBlackout(bool active, const char* reason)
    {
        std::lock_guard lock(mutex_);
        if (presentationBlackoutActive_ == active) return;
        presentationBlackoutActive_ = active;
        ++presentationBlackoutTransitions_;
        Logger::Instance().Write(
            active ? LogLevel::Warn : LogLevel::Info,
            "openxr_presentation_blackout active=%d reason=%s frame=%llu transitions=%llu blackFrames=%llu",
            active ? 1 : 0,
            reason != nullptr ? reason : "unspecified",
            static_cast<unsigned long long>(currentGameFrame_),
            static_cast<unsigned long long>(presentationBlackoutTransitions_),
            static_cast<unsigned long long>(presentationBlackoutFrames_));
    }

    bool BeginHudCapture(uint64_t frameIndex)
    {
        std::lock_guard lock(mutex_);
        if (!hudLayerEnabled_
            || hudSubmissionSuspended_
            || !frameSubmitEnabled_
            || !sessionRunning_
            || !stereoSubmissionEnabled_
            || (sessionState_ != XR_SESSION_STATE_VISIBLE
                && sessionState_ != XR_SESSION_STATE_FOCUSED)
            || !frameResourcesReady_
            || !glBridge_.HudReady()) {
            return false;
        }
        const bool started = glBridge_.BeginHudCapture(frameIndex);
        if (started) {
            ++hudCaptureStarts_;
        }
        return started;
    }

    bool EndHudCapture(uint64_t frameIndex, bool suppressCenterCrosshair)
    {
        std::lock_guard lock(mutex_);
        const bool completed = glBridge_.EndHudCapture(frameIndex, suppressCenterCrosshair);
        if (completed) {
            ++hudCaptureCompletions_;
        }
        return completed;
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

        std::vector<const char*> enabledExtensions = {
            XR_KHR_OPENGL_ENABLE_EXTENSION_NAME,
        };
        depthExtensionEnabled_ = false;
        if ((depthCompositionProbeEnabled_ || depthCompositionSubmitEnabled_)
            && depthExtensionAvailable_) {
            enabledExtensions.push_back("XR_KHR_composition_layer_depth");
            depthExtensionEnabled_ = true;
        }
        hudCylinderExtensionEnabled_ = false;
        if (hudLayerEnabled_ && hudCylinderExtensionAvailable_) {
            enabledExtensions.push_back(XR_KHR_COMPOSITION_LAYER_CYLINDER_EXTENSION_NAME);
            hudCylinderExtensionEnabled_ = true;
        }
        createInfo.enabledExtensionCount = static_cast<uint32_t>(enabledExtensions.size());
        createInfo.enabledExtensionNames = enabledExtensions.data();

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
            sessionHdc_ = nullptr;
            sessionGlContext_ = nullptr;
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

    void BeginRuntimeRecoveryLocked(uint64_t frameIndex)
    {
        const bool resumeStereo = stereoSubmissionEnabled_;
        if (session_ != XR_NULL_HANDLE) {
            input_.ShutdownSession();
            DestroyFrameResourcesLocked();
            xrDestroySession(session_);
            session_ = XR_NULL_HANDLE;
            sessionHdc_ = nullptr;
            sessionGlContext_ = nullptr;
        } else {
            DestroyFrameResourcesLocked();
        }
        if (instance_ != XR_NULL_HANDLE) {
            input_.Shutdown();
            xrDestroyInstance(instance_);
            instance_ = XR_NULL_HANDLE;
        }

        systemId_ = XR_NULL_SYSTEM_ID;
        sessionState_ = XR_SESSION_STATE_UNKNOWN;
        initialized_ = false;
        attempted_ = true;
        failed_ = false;
        sessionAttempted_ = false;
        sessionCreated_ = false;
        sessionRunning_ = false;
        frameSubmitFailed_ = false;
        pendingRenderedEyeValid_ = false;
        renderedStereoViewValid_[0] = false;
        renderedStereoViewValid_[1] = false;
        latestPoseValid_ = false;
        depthExtensionEnabled_ = false;
        hudCylinderExtensionEnabled_ = false;
        hudCylinderSubmissionDisabled_ = false;
        trackingDegraded_ = false;
        trackingLost_ = false;
        consecutiveFrameFailures_ = 0;
        stereoCaptureFailures_ = 0;
        stereoSubmissionEnabled_ = resumeStereo;
        retryFrame_ = frameIndex + static_cast<uint64_t>(recoveryDelayFrames_);
        recoveryRequested_ = false;
        ++runtimeRecoveries_;
        Logger::Instance().Write(
            LogLevel::Warn,
            "openxr_recovery scheduled frame=%llu retryFrame=%llu delayFrames=%d resumeStereo=%d count=%llu",
            static_cast<unsigned long long>(frameIndex),
            static_cast<unsigned long long>(retryFrame_),
            recoveryDelayFrames_,
            resumeStereo ? 1 : 0,
            static_cast<unsigned long long>(runtimeRecoveries_));
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
        depthExtensionAvailable_ = ExtensionPresent(extensions, "XR_KHR_composition_layer_depth");
        hudCylinderExtensionAvailable_ = ExtensionPresent(
            extensions, XR_KHR_COMPOSITION_LAYER_CYLINDER_EXTENSION_NAME);
        Logger::Instance().Write(
            LogLevel::Info,
            "openxr_extensions count=%u khrOpenGL=%d khrWin32Time=%d khrCompositionLayerDepth=%d khrCompositionLayerCylinder=%d depthProbeRequested=%d depthSubmitRequested=%d hudCylinderRequested=%d sample=\"%s\"",
            extensionCount,
            hasOpenGL ? 1 : 0,
            hasWin32Time ? 1 : 0,
            depthExtensionAvailable_ ? 1 : 0,
            hudCylinderExtensionAvailable_ ? 1 : 0,
            depthCompositionProbeEnabled_ ? 1 : 0,
            depthCompositionSubmitEnabled_ ? 1 : 0,
            hudCylinderRequested_ ? 1 : 0,
            ExtensionSample(extensions).c_str());

        if (hudCylinderRequested_ && !hudCylinderExtensionAvailable_) {
            Logger::Instance().Write(
                LogLevel::Warn,
                "openxr_hud shape_fallback requested=cylinder effective=quad reason=extension_unavailable");
        }

        if (!hasOpenGL) {
            Logger::Instance().Write(LogLevel::Warn, "openxr_bootstrap missing_required_extension name=%s", XR_KHR_OPENGL_ENABLE_EXTENSION_NAME);
        }
        return hasOpenGL;
    }

    void LogDepthCapabilityLocked(
        HGLRC glContext,
        uint64_t frameIndex,
        const HPLCameraBridgeStatus& camera)
    {
        if ((!depthCompositionProbeEnabled_ && !depthCompositionSubmitEnabled_)
            || depthCapabilityLogged_
            || glContext == nullptr || wglGetCurrentContext() != glContext) {
            return;
        }
        if (!camera.projectionParametersValid) {
            return;
        }
        GLint depthBits = 0;
        GLdouble depthRange[2] = {};
        glGetIntegerv(GL_DEPTH_BITS, &depthBits);
        glGetDoublev(GL_DEPTH_RANGE, depthRange);
        Logger::Instance().Write(
            LogLevel::Info,
            "openxr_depth_capability frame=%llu probeRequested=%d submitRequested=%d extensionAvailable=%d extensionEnabled=%d glDepthBits=%d glDepthRange=%.6f,%.6f hplProjectionValid=1 hplProjectionType=%d hplNear=%.6f hplFar=%.6f worldUnitsPerMeter=%.6f submissionImplemented=1",
            static_cast<unsigned long long>(frameIndex),
            depthCompositionProbeEnabled_ ? 1 : 0,
            depthCompositionSubmitEnabled_ ? 1 : 0,
            depthExtensionAvailable_ ? 1 : 0,
            depthExtensionEnabled_ ? 1 : 0,
            depthBits,
            depthRange[0],
            depthRange[1],
            camera.projectionType,
            camera.nearPlane,
            camera.farPlane,
            camera.worldUnitsPerMeter);
        depthCapabilityLogged_ = true;
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
        sessionHdc_ = deviceContext;
        sessionGlContext_ = glContext;
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
        supportedReferenceSpaces_ = spaces;

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
        hudSubmissionSuspended_ = false;
        hudConsecutiveFailures_ = 0;

        const bool stageSupported = std::find(
            supportedReferenceSpaces_.begin(),
            supportedReferenceSpaces_.end(),
            XR_REFERENCE_SPACE_TYPE_STAGE) != supportedReferenceSpaces_.end();
        selectedReferenceSpace_ = requestedReferenceSpace_ == "stage" && stageSupported
            ? XR_REFERENCE_SPACE_TYPE_STAGE
            : XR_REFERENCE_SPACE_TYPE_LOCAL;
        if (requestedReferenceSpace_ == "stage" && !stageSupported) {
            Logger::Instance().Write(
                LogLevel::Warn,
                "openxr_reference_space fallback requested=STAGE selected=LOCAL reason=unsupported");
        }

        XrReferenceSpaceCreateInfo spaceInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
        spaceInfo.referenceSpaceType = selectedReferenceSpace_;
        spaceInfo.poseInReferenceSpace.orientation.w = 1.0f;
        XrResult result = xrCreateReferenceSpace(session_, &spaceInfo, &appSpace_);
        if (XR_FAILED(result)) {
            Logger::Instance().Write(
                LogLevel::Warn,
                "openxr_reference_space create_failed requested=%s selected=%s result=%s",
                requestedReferenceSpace_.c_str(),
                ReferenceSpaceTypeName(selectedReferenceSpace_),
                XrResultString(result).c_str());
            return false;
        }

        bool createHudResources = hudLayerEnabled_;
        bool createStatusPanelResources = statusPanelEnabled_;
        bool createComfortVignetteResources = comfortVignetteConfigured_ || statusPanelEnabled_;
        if (createHudResources || createStatusPanelResources || createComfortVignetteResources) {
            XrReferenceSpaceCreateInfo viewSpaceInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
            viewSpaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
            viewSpaceInfo.poseInReferenceSpace.orientation.w = 1.0f;
            result = xrCreateReferenceSpace(session_, &viewSpaceInfo, &viewSpace_);
            if (XR_FAILED(result)) {
                const bool hudRequested = createHudResources;
                const bool panelRequested = createStatusPanelResources;
                const bool vignetteRequested = createComfortVignetteResources;
                createHudResources = false;
                createStatusPanelResources = false;
                createComfortVignetteResources = false;
                hudSubmissionSuspended_ = hudRequested;
                Logger::Instance().Write(
                    LogLevel::Warn,
                    "openxr_view_space_layers disabled reason=view_space_create_failed hud=%d statusPanel=%d comfortVignette=%d result=%s",
                    hudRequested ? 1 : 0,
                    panelRequested ? 1 : 0,
                    vignetteRequested ? 1 : 0,
                    XrResultString(result).c_str());
            }
        }

        if (!glBridge_.Initialize(
                session_,
                viewConfigurationViews_,
                swapchainFormats_,
                resolutionScalePercent_,
                depthExtensionEnabled_
                    && (depthCompositionProbeEnabled_ || depthCompositionSubmitEnabled_),
                depthExtensionEnabled_ && depthCompositionSubmitEnabled_,
                createHudResources,
                hudWidthPixels_,
                hudHeightPixels_,
                hudSuppressCenterCrosshair_,
                hudCrosshairClearRadiusPixels_,
                interactionReticleEnabled_,
                interactionReticleNativeIconsEnabled_,
                interactionReticleSizePixels_,
                createStatusPanelResources,
                statusPanelWidthPixels_,
                statusPanelHeightPixels_,
                createComfortVignetteResources,
                comfortVignetteSizePixels_)) {
            xrDestroySpace(appSpace_);
            appSpace_ = XR_NULL_HANDLE;
            if (viewSpace_ != XR_NULL_HANDLE) {
                xrDestroySpace(viewSpace_);
                viewSpace_ = XR_NULL_HANDLE;
            }
            Logger::Instance().Write(LogLevel::Warn, "openxr_frame_resources gl_bridge_failed");
            return false;
        }

        locatedViews_.resize(glBridge_.EyeCount());
        pendingLocatedViews_.resize(glBridge_.EyeCount());
        for (XrView& view : locatedViews_) {
            view.type = XR_TYPE_VIEW;
        }
        for (XrView& view : pendingLocatedViews_) {
            view.type = XR_TYPE_VIEW;
        }

        Logger::Instance().Write(
            LogLevel::Info,
            "openxr_frame_resources ready requestedSpace=%s selectedSpace=%s eyes=%u mirrorBackbuffer=%d",
            requestedReferenceSpace_.c_str(),
            ReferenceSpaceTypeName(selectedReferenceSpace_),
            glBridge_.EyeCount(),
            mirrorBackbufferEnabled_ ? 1 : 0);
        nextViewConfigurationCheckFrame_ = currentGameFrame_ + kViewConfigurationCheckIntervalFrames;
        return true;
    }

    bool RefreshViewConfigurationLocked(uint64_t frameIndex)
    {
        if (frameIndex < nextViewConfigurationCheckFrame_) {
            return true;
        }
        nextViewConfigurationCheckFrame_ = frameIndex + kViewConfigurationCheckIntervalFrames;
        ++viewResourceChecks_;

        uint32_t viewCount = 0;
        XrResult result = xrEnumerateViewConfigurationViews(
            instance_,
            systemId_,
            XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
            0,
            &viewCount,
            nullptr);
        if (XR_FAILED(result) || viewCount < 2) {
            Logger::Instance().Write(
                LogLevel::Warn,
                "openxr_view_resources refresh_enumerate_count_failed frame=%llu result=%s count=%u",
                static_cast<unsigned long long>(frameIndex),
                XrResultString(result).c_str(),
                viewCount);
            return true;
        }

        std::vector<XrViewConfigurationView> refreshed(viewCount);
        for (XrViewConfigurationView& view : refreshed) {
            view.type = XR_TYPE_VIEW_CONFIGURATION_VIEW;
        }
        result = xrEnumerateViewConfigurationViews(
            instance_,
            systemId_,
            XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
            viewCount,
            &viewCount,
            refreshed.data());
        if (XR_FAILED(result)) {
            Logger::Instance().Write(
                LogLevel::Warn,
                "openxr_view_resources refresh_enumerate_failed frame=%llu result=%s",
                static_cast<unsigned long long>(frameIndex),
                XrResultString(result).c_str());
            return true;
        }
        refreshed.resize(viewCount);

        const auto differs = [](const XrViewConfigurationView& left,
                                const XrViewConfigurationView& right) {
            return left.recommendedImageRectWidth != right.recommendedImageRectWidth
                || left.maxImageRectWidth != right.maxImageRectWidth
                || left.recommendedImageRectHeight != right.recommendedImageRectHeight
                || left.maxImageRectHeight != right.maxImageRectHeight
                || left.recommendedSwapchainSampleCount != right.recommendedSwapchainSampleCount
                || left.maxSwapchainSampleCount != right.maxSwapchainSampleCount;
        };
        bool changed = refreshed.size() != viewConfigurationViews_.size();
        if (!changed) {
            for (size_t index = 0; index < refreshed.size(); ++index) {
                if (differs(refreshed[index], viewConfigurationViews_[index])) {
                    changed = true;
                    break;
                }
            }
        }
        if (!changed) {
            if (viewResourceChecks_ <= 2 || viewResourceChecks_ % 20 == 0) {
                Logger::Instance().Write(
                    LogLevel::Info,
                    "openxr_view_resources stable frame=%llu checks=%llu eyes=%zu",
                    static_cast<unsigned long long>(frameIndex),
                    static_cast<unsigned long long>(viewResourceChecks_),
                    refreshed.size());
            }
            return true;
        }

        const uint32_t oldWidth = viewConfigurationViews_.empty()
            ? 0 : viewConfigurationViews_[0].recommendedImageRectWidth;
        const uint32_t oldHeight = viewConfigurationViews_.empty()
            ? 0 : viewConfigurationViews_[0].recommendedImageRectHeight;
        const uint32_t newWidth = refreshed.empty() ? 0 : refreshed[0].recommendedImageRectWidth;
        const uint32_t newHeight = refreshed.empty() ? 0 : refreshed[0].recommendedImageRectHeight;
        Logger::Instance().Write(
            LogLevel::Warn,
            "openxr_view_resources changed frame=%llu oldEyes=%zu newEyes=%zu oldRecommended=%ux%u newRecommended=%ux%u policy=rebuild_frame_resources",
            static_cast<unsigned long long>(frameIndex),
            viewConfigurationViews_.size(),
            refreshed.size(),
            oldWidth,
            oldHeight,
            newWidth,
            newHeight);

        DestroyFrameResourcesLocked();
        viewConfigurationViews_ = std::move(refreshed);
        viewCount_ = static_cast<uint32_t>(viewConfigurationViews_.size());
        frameResourcesReady_ = CreateFrameResourcesLocked();
        frameSubmitFailed_ = !frameResourcesReady_;
        ++viewResourceRebuilds_;
        Logger::Instance().Write(
            frameResourcesReady_ ? LogLevel::Info : LogLevel::Error,
            "openxr_view_resources rebuild_complete frame=%llu ready=%d rebuilds=%llu",
            static_cast<unsigned long long>(frameIndex),
            frameResourcesReady_ ? 1 : 0,
            static_cast<unsigned long long>(viewResourceRebuilds_));
        return frameResourcesReady_;
    }

    void DestroyFrameResourcesLocked()
    {
        glBridge_.Shutdown();
        if (viewSpace_ != XR_NULL_HANDLE) {
            xrDestroySpace(viewSpace_);
            viewSpace_ = XR_NULL_HANDLE;
        }
        if (appSpace_ != XR_NULL_HANDLE) {
            xrDestroySpace(appSpace_);
            appSpace_ = XR_NULL_HANDLE;
        }
        locatedViews_.clear();
        pendingLocatedViews_.clear();
        pendingRenderedEyeValid_ = false;
        renderedStereoViewValid_[0] = false;
        renderedStereoViewValid_[1] = false;
        stereoWarmupLogged_ = false;
        interactionReticleState_ = {};
        interactionReticleSubmissionSuspended_ = false;
        interactionReticleConsecutiveFailures_ = 0;
        nextViewConfigurationCheckFrame_ = 0;
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
        if (recoveryEnabled_
            && (result == XR_SESSION_LOSS_PENDING
                || result == XR_ERROR_SESSION_LOST
                || result == XR_ERROR_INSTANCE_LOST)) {
            recoveryRequested_ = true;
            Logger::Instance().Write(
                LogLevel::Warn,
                "openxr_recovery requested source=frame_failure operation=%s result=%s frame=%llu",
                operation,
                XrResultString(result).c_str(),
                static_cast<unsigned long long>(frameIndex));
        }
    }

    uint64_t PoseAgeFramesLocked() const
    {
        return currentGameFrame_ >= latestPoseGameFrame_
            ? currentGameFrame_ - latestPoseGameFrame_
            : 0;
    }

    bool PoseUsableLocked() const
    {
        return latestPoseValid_
            && PoseAgeFramesLocked() <= static_cast<uint64_t>(trackingHoldFrames_);
    }

    void RecordTrackingInvalidLocked(uint64_t frameIndex, XrResult result, XrViewStateFlags flags)
    {
        ++trackingInvalidFrames_;
        if (!trackingDegraded_) {
            trackingDegraded_ = true;
            trackingDegradedStartFrame_ = frameIndex;
            Logger::Instance().Write(
                LogLevel::Warn,
                "openxr_tracking degraded frame=%llu poseAgeFrames=%llu holdFrames=%d result=%s flags=0x%llx",
                static_cast<unsigned long long>(frameIndex),
                static_cast<unsigned long long>(PoseAgeFramesLocked()),
                trackingHoldFrames_,
                XrResultString(result).c_str(),
                static_cast<unsigned long long>(flags));
        }
        if (!trackingLost_ && !PoseUsableLocked()) {
            trackingLost_ = true;
            ++trackingLossEvents_;
            Logger::Instance().Write(
                LogLevel::Warn,
                "openxr_tracking lost frame=%llu degradedFrames=%llu poseAgeFrames=%llu lossEvents=%llu action=zero_layers",
                static_cast<unsigned long long>(frameIndex),
                static_cast<unsigned long long>(frameIndex - trackingDegradedStartFrame_ + 1),
                static_cast<unsigned long long>(PoseAgeFramesLocked()),
                static_cast<unsigned long long>(trackingLossEvents_));
        }
    }

    void RecordTrackingRestoredLocked(uint64_t frameIndex)
    {
        if (!trackingDegraded_) {
            return;
        }
        const uint64_t degradedFrames = frameIndex - trackingDegradedStartFrame_ + 1;
        ++trackingRestoreEvents_;
        if (trackingRecoveryBlackoutFrames_ > 0) {
            comfortBlackoutUntilFrame_ = std::max(
                comfortBlackoutUntilFrame_,
                frameIndex + static_cast<uint64_t>(trackingRecoveryBlackoutFrames_ - 1));
            ++comfortBlackoutRequests_;
        }
        Logger::Instance().Write(
            LogLevel::Info,
            "openxr_tracking restored frame=%llu degradedFrames=%llu wasLost=%d restoreEvents=%llu recoveryBlackoutFrames=%d",
            static_cast<unsigned long long>(frameIndex),
            static_cast<unsigned long long>(degradedFrames),
            trackingLost_ ? 1 : 0,
            static_cast<unsigned long long>(trackingRestoreEvents_),
            trackingRecoveryBlackoutFrames_);
        trackingDegraded_ = false;
        trackingLost_ = false;
    }

    bool CapturePendingStereoEyeLocked(uint64_t frameIndex, const char* source)
    {
        if (!stereoSubmissionEnabled_ || !pendingRenderedEyeValid_ || !glBridge_.Ready()) {
            return false;
        }

        const char* captureSource = source != nullptr ? source : "unspecified";
        const uint32_t eyeIndex = pendingRenderedEye_;
        const OpenXREyeView renderedView = pendingRenderedView_;
        const bool captured = glBridge_.CaptureBackbufferToCache(eyeIndex);
        pendingRenderedEyeValid_ = false;
        if (captured) {
            renderedStereoViews_[eyeIndex] = renderedView;
            renderedStereoViewValid_[eyeIndex] = true;
            lastCapturedStereoEye_ = eyeIndex;
            ++stereoCapturedEyeCount_;
            stereoCaptureFailures_ = 0;
            if (stereoCapturedEyeCount_ <= 2
                || (stereoCapturedEyeCount_ % 120) == 0
                || std::strcmp(captureSource, "frame_boundary") != 0) {
                Logger::Instance().Write(
                    LogLevel::Info,
                    "openxr_stereo_cache captured=%llu eye=%u poseFrame=%llu cachesReady=%d source=%s frame=%llu",
                    static_cast<unsigned long long>(stereoCapturedEyeCount_),
                    eyeIndex,
                    static_cast<unsigned long long>(renderedView.gameFrame),
                    glBridge_.StereoCachesReady() ? 1 : 0,
                    captureSource,
                    static_cast<unsigned long long>(frameIndex));
            }
            return true;
        }

        ++stereoCaptureFailures_;
        if (stereoCaptureFailures_ <= 4) {
            Logger::Instance().Write(
                LogLevel::Warn,
                "openxr_stereo_cache capture_failed eye=%u consecutive=%u source=%s frame=%llu",
                eyeIndex,
                stereoCaptureFailures_,
                captureSource,
                static_cast<unsigned long long>(frameIndex));
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
        return false;
    }

    void SubmitFrameLocked(uint64_t frameIndex, const HPLCameraBridgeStatus& cameraStatus)
    {
        CapturePendingStereoEyeLocked(frameIndex, "frame_boundary");

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

        input_.Sync(session_, appSpace_, frameState.predictedDisplayTime, frameIndex);
        const bool comfortMotionFresh = comfortVignetteMotionFrame_ != 0
            && frameIndex >= comfortVignetteMotionFrame_
            && frameIndex - comfortVignetteMotionFrame_
                <= static_cast<uint64_t>(comfortVignetteMaxMotionAgeFrames_);
        const float comfortTarget = comfortVignetteEnabled_
            && comfortMotionFresh
            && !statusPanelState_.visible
                ? comfortVignetteTarget_ : 0.0f;
        const float displayPeriodSeconds = frameState.predictedDisplayPeriod > 0
            ? static_cast<float>(static_cast<double>(frameState.predictedDisplayPeriod) / 1.0e9)
            : 1.0f / 90.0f;
        comfortVignetteLevel_ = comfort_vignette_math::AdvanceEnvelope(
            comfortVignetteLevel_,
            comfortTarget,
            displayPeriodSeconds,
            comfortVignetteFadeMilliseconds_);

        std::array<XrCompositionLayerProjectionView, 2> projectionViews{};
        std::array<XrCompositionLayerDepthInfoKHR, 2> depthViews{};
        XrCompositionLayerProjection projectionLayer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
        XrCompositionLayerQuad hudLayer{XR_TYPE_COMPOSITION_LAYER_QUAD};
        XrCompositionLayerCylinderKHR hudCylinderLayer{XR_TYPE_COMPOSITION_LAYER_CYLINDER_KHR};
        XrCompositionLayerQuad interactionReticleLayer{XR_TYPE_COMPOSITION_LAYER_QUAD};
        XrCompositionLayerQuad statusPanelLayer{XR_TYPE_COMPOSITION_LAYER_QUAD};
        XrCompositionLayerQuad comfortVignetteLayer{XR_TYPE_COMPOSITION_LAYER_QUAD};
        const XrCompositionLayerBaseHeader* layers[5] = {};
        uint32_t layerCount = 0;
        uint32_t locatedViewCount = 0;
        XrViewState viewState{XR_TYPE_VIEW_STATE};
        bool submittedStereo = false;
        bool submittedDepth = false;
        bool submittedHud = false;
        bool submittedHudCylinder = false;
        bool submittedInteractionReticle = false;
        bool submittedStatusPanel = false;
        bool submittedComfortVignette = false;
        bool viewsLocatedValid = false;
        for (XrView& view : pendingLocatedViews_) {
            view = {XR_TYPE_VIEW};
        }

        if (frameState.shouldRender == XR_TRUE && mirrorBackbufferEnabled_ && glBridge_.Ready()) {
            XrViewLocateInfo locateInfo{XR_TYPE_VIEW_LOCATE_INFO};
            locateInfo.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
            locateInfo.displayTime = frameState.predictedDisplayTime;
            locateInfo.space = appSpace_;
            result = xrLocateViews(
                session_,
                &locateInfo,
                &viewState,
                static_cast<uint32_t>(pendingLocatedViews_.size()),
                &locatedViewCount,
                pendingLocatedViews_.data());

            const XrViewStateFlags requiredFlags =
                XR_VIEW_STATE_ORIENTATION_VALID_BIT | XR_VIEW_STATE_POSITION_VALID_BIT;
            const bool viewsValid = XR_SUCCEEDED(result)
                && locatedViewCount == glBridge_.EyeCount()
                && (viewState.viewStateFlags & requiredFlags) == requiredFlags;
            viewsLocatedValid = viewsValid;

            bool copied = viewsValid;
            if (!viewsValid) {
                RecordTrackingInvalidLocked(frameIndex, result, viewState.viewStateFlags);
                if (XR_FAILED(result)) {
                    RecordFrameFailureLocked("xrLocateViews", result, frameIndex);
                }
            } else {
                locatedViews_ = pendingLocatedViews_;
                RecordTrackingRestoredLocked(frameIndex);
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
                    depth_math::CompositionDepthRange depthRange;
                    bool depthFrameReady = stereoReady
                        && depthCompositionSubmitEnabled_
                        && depthExtensionEnabled_
                        && cameraStatus.projectionParametersValid
                        && cameraStatus.projectionType == 0
                        && glBridge_.DepthCachesReady()
                        && glBridge_.DepthSwapchainsReady()
                        && depth_math::BuildStandardDepthRange(
                            cameraStatus.nearPlane,
                            cameraStatus.farPlane,
                            cameraStatus.worldUnitsPerMeter,
                            depthRange);
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

                        if (depthFrameReady && !glBridge_.CopyDepthCacheToEye(eyeIndex)) {
                            depthFrameReady = false;
                            ++depthSubmissionFailures_;
                            if (depthSubmissionFailures_ <= 4
                                || depthSubmissionFailures_ % 120 == 0) {
                                Logger::Instance().Write(
                                    LogLevel::Warn,
                                    "openxr_depth_submission copy_failed frame=%llu eye=%u failures=%llu fallback=color_only",
                                    static_cast<unsigned long long>(frameIndex),
                                    eyeIndex,
                                    static_cast<unsigned long long>(depthSubmissionFailures_));
                            }
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
                    if (copied && depthFrameReady) {
                        for (uint32_t eyeIndex = 0; eyeIndex < glBridge_.EyeCount(); ++eyeIndex) {
                            const OpenXRGLBridge::EyeSwapchain& eye = glBridge_.Eye(eyeIndex);
                            XrCompositionLayerDepthInfoKHR& depthView = depthViews[eyeIndex];
                            depthView.type = XR_TYPE_COMPOSITION_LAYER_DEPTH_INFO_KHR;
                            depthView.subImage.swapchain = eye.depthHandle;
                            depthView.subImage.imageRect.offset = {0, 0};
                            depthView.subImage.imageRect.extent = {eye.width, eye.height};
                            depthView.subImage.imageArrayIndex = 0;
                            depthView.minDepth = depthRange.minDepth;
                            depthView.maxDepth = depthRange.maxDepth;
                            depthView.nearZ = depthRange.nearMeters;
                            depthView.farZ = depthRange.farMeters;
                            projectionViews[eyeIndex].next = &depthView;
                        }
                        submittedDepth = true;
                    }
                    submittedStereo = copied && stereoReady;
                }
            }

            if (copied) {
                projectionLayer.space = appSpace_;
                projectionLayer.viewCount = glBridge_.EyeCount();
                projectionLayer.views = projectionViews.data();
                layers[0] = reinterpret_cast<const XrCompositionLayerBaseHeader*>(&projectionLayer);
                layerCount = 1;
            }
        }

        if (frameState.shouldRender == XR_TRUE
            && viewsLocatedValid
            && hudLayerEnabled_
            && hudRuntimeVisible_
            && !hudSubmissionSuspended_
            && stereoSubmissionEnabled_
            && viewSpace_ != XR_NULL_HANDLE
            && glBridge_.HudCaptureFresh(frameIndex, static_cast<uint64_t>(hudMaxAgeFrames_))) {
            const float hudAspect = static_cast<float>(glBridge_.Hud().width)
                / static_cast<float>(glBridge_.Hud().height);
            const bool useCylinder = hudCylinderRequested_
                && hudCylinderExtensionEnabled_
                && !hudCylinderSubmissionDisabled_;
            const XrCompositionLayerBaseHeader* hudLayerHeader = nullptr;
            bool poseValid = false;
            if (useCylinder) {
                hud_math::HudCylinderPose cylinderPose;
                poseValid = hud_math::BuildHeadLockedCylinderPose(
                    {}, {}, hudDistanceMeters_, hudVerticalOffsetMeters_, hudWidthMeters_,
                    hudAspect, hudCylinderAngleDegrees_, cylinderPose);
                if (poseValid) {
                    hudCylinderLayer.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
                    hudCylinderLayer.space = viewSpace_;
                    hudCylinderLayer.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
                    hudCylinderLayer.pose.orientation = {
                        cylinderPose.orientation.x,
                        cylinderPose.orientation.y,
                        cylinderPose.orientation.z,
                        cylinderPose.orientation.w,
                    };
                    hudCylinderLayer.pose.position = {
                        cylinderPose.position.x,
                        cylinderPose.position.y,
                        cylinderPose.position.z,
                    };
                    hudCylinderLayer.radius = cylinderPose.radiusMeters;
                    hudCylinderLayer.centralAngle = cylinderPose.centralAngleRadians;
                    hudCylinderLayer.aspectRatio = cylinderPose.aspectRatio;
                    hudCylinderLayer.subImage.swapchain = glBridge_.Hud().handle;
                    hudCylinderLayer.subImage.imageRect.offset = {0, 0};
                    hudCylinderLayer.subImage.imageRect.extent = {
                        glBridge_.Hud().width,
                        glBridge_.Hud().height,
                    };
                    hudCylinderLayer.subImage.imageArrayIndex = 0;
                    hudLayerHeader = reinterpret_cast<const XrCompositionLayerBaseHeader*>(
                        &hudCylinderLayer);
                }
            } else {
                hud_math::HudQuadPose quadPose;
                poseValid = hud_math::BuildHeadLockedQuadPose(
                    {}, {}, hudDistanceMeters_, hudVerticalOffsetMeters_, hudWidthMeters_,
                    hudAspect, quadPose);
                if (poseValid) {
                    hudLayer.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
                    hudLayer.space = viewSpace_;
                    hudLayer.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
                    hudLayer.pose.orientation = {
                        quadPose.orientation.x,
                        quadPose.orientation.y,
                        quadPose.orientation.z,
                        quadPose.orientation.w,
                    };
                    hudLayer.pose.position = {
                        quadPose.position.x,
                        quadPose.position.y,
                        quadPose.position.z,
                    };
                    hudLayer.size = {quadPose.widthMeters, quadPose.heightMeters};
                    hudLayer.subImage.swapchain = glBridge_.Hud().handle;
                    hudLayer.subImage.imageRect.offset = {0, 0};
                    hudLayer.subImage.imageRect.extent = {
                        glBridge_.Hud().width,
                        glBridge_.Hud().height,
                    };
                    hudLayer.subImage.imageArrayIndex = 0;
                    hudLayerHeader = reinterpret_cast<const XrCompositionLayerBaseHeader*>(&hudLayer);
                }
            }
            if (poseValid && glBridge_.CopyHudCaptureToSwapchain()) {
                layers[layerCount++] = hudLayerHeader;
                submittedHud = true;
                submittedHudCylinder = useCylinder;
                hudConsecutiveFailures_ = 0;
                ++hudSubmittedFrames_;
            } else {
                ++hudSubmissionFailures_;
                ++hudConsecutiveFailures_;
                if (hudConsecutiveFailures_ >= 4) {
                    hudSubmissionSuspended_ = true;
                    Logger::Instance().Write(
                        LogLevel::Error,
                        "openxr_hud suspended reason=submission_failures consecutive=%u total=%llu fallback=native_backbuffer",
                        hudConsecutiveFailures_,
                        static_cast<unsigned long long>(hudSubmissionFailures_));
                }
            }
        }

        const bool interactionReticleFresh = interactionReticleState_.valid
            && frameIndex >= interactionReticleState_.gameFrame
            && frameIndex - interactionReticleState_.gameFrame
                <= static_cast<uint64_t>(interactionReticleMaxAgeFrames_);
        if (interactionReticleState_.valid
            && frameIndex >= interactionReticleState_.gameFrame
            && !interactionReticleFresh) {
            interactionReticleState_ = {};
            ++interactionReticleExpired_;
        }
        if (frameState.shouldRender == XR_TRUE
            && layerCount > 0
            && interactionReticleEnabled_
            && interactionReticleRuntimeVisible_
            && !interactionReticleSubmissionSuspended_
            && stereoSubmissionEnabled_
            && appSpace_ != XR_NULL_HANDLE
            && interactionReticleFresh
            && interactionReticleState_.semanticValid
            && glBridge_.InteractionReticleReady()) {
            const float distanceMeters = interactionReticleState_.distanceMeters;
            float reticleSizeMeters = 0.0f;
            const bool sizeValid = hud_math::ComputeAngularQuadSize(
                distanceMeters,
                interactionReticleAngularSizeDegrees_,
                interactionReticleMinSizeMeters_,
                interactionReticleMaxSizeMeters_,
                reticleSizeMeters);
            const OpenXRControllerPose& aim = interactionReticleState_.aimPose;
            hud_math::InteractionReticleColor reticleColor;
            const bool colorValid = hud_math::ComputeInteractionReticleColor(
                interactionReticleState_.semanticState,
                reticleColor);
            hud_math::HudQuadPose reticlePose;
            const bool poseValid = sizeValid && colorValid && hud_math::BuildHeadLockedQuadPose(
                {aim.positionX, aim.positionY, aim.positionZ},
                {aim.orientationX, aim.orientationY, aim.orientationZ, aim.orientationW},
                distanceMeters,
                0.0f,
                reticleSizeMeters,
                1.0f,
                reticlePose);
            if (poseValid && glBridge_.DrawInteractionReticleToSwapchain(
                    interactionReticleState_.semanticState,
                    reticleColor.red,
                    reticleColor.green,
                    reticleColor.blue,
                    reticleColor.alpha)) {
                interactionReticleLayer.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
                interactionReticleLayer.space = appSpace_;
                interactionReticleLayer.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
                interactionReticleLayer.pose.orientation = {
                    reticlePose.orientation.x,
                    reticlePose.orientation.y,
                    reticlePose.orientation.z,
                    reticlePose.orientation.w,
                };
                interactionReticleLayer.pose.position = {
                    reticlePose.position.x,
                    reticlePose.position.y,
                    reticlePose.position.z,
                };
                interactionReticleLayer.size = {reticlePose.widthMeters, reticlePose.heightMeters};
                interactionReticleLayer.subImage.swapchain = glBridge_.InteractionReticle().handle;
                interactionReticleLayer.subImage.imageRect.offset = {0, 0};
                interactionReticleLayer.subImage.imageRect.extent = {
                    glBridge_.InteractionReticle().width,
                    glBridge_.InteractionReticle().height,
                };
                interactionReticleLayer.subImage.imageArrayIndex = 0;
                layers[layerCount++] = reinterpret_cast<const XrCompositionLayerBaseHeader*>(
                    &interactionReticleLayer);
                submittedInteractionReticle = true;
                interactionReticleConsecutiveFailures_ = 0;
                ++interactionReticleSubmittedFrames_;
            } else {
                ++interactionReticleSubmissionFailures_;
                ++interactionReticleConsecutiveFailures_;
                if (interactionReticleConsecutiveFailures_ >= 4) {
                    interactionReticleSubmissionSuspended_ = true;
                    Logger::Instance().Write(
                        LogLevel::Error,
                        "openxr_interaction_reticle suspended reason=submission_failures consecutive=%u total=%llu",
                        interactionReticleConsecutiveFailures_,
                        static_cast<unsigned long long>(interactionReticleSubmissionFailures_));
                }
            }
        }

        if (frameState.shouldRender == XR_TRUE
            && layerCount > 0
            && statusPanelEnabled_
            && statusPanelState_.visible
            && stereoSubmissionEnabled_
            && viewSpace_ != XR_NULL_HANDLE
            && glBridge_.StatusPanelReady()) {
            status_panel_math::PanelModel model;
            model.visible = true;
            model.selectedAction = statusPanelState_.selectedAction;
            model.trackingEnabled = statusPanelState_.trackingEnabled;
            model.stereoEnabled = statusPanelState_.stereoEnabled;
            model.roomscaleEnabled = statusPanelState_.roomscaleEnabled;
            model.projectionCentered = statusPanelState_.projectionCentered;
            model.dualRenderReady = statusPanelState_.dualRenderReady;
            model.continuousDualRender = statusPanelState_.continuousDualRender;
            model.viewHistoryConfigured = statusPanelState_.viewHistoryConfigured;
            model.viewHistoryActive = statusPanelState_.viewHistoryActive;
            model.viewHistoryFaulted = statusPanelState_.viewHistoryFaulted;
            model.hudVisible = statusPanelState_.hudVisible;
            model.hudCylinderAvailable = hudCylinderExtensionEnabled_
                && !hudCylinderSubmissionDisabled_;
            model.hudCylinderActive = hudCylinderRequested_
                && model.hudCylinderAvailable;
            model.reticleVisible = statusPanelState_.reticleVisible;
            model.comfortVignetteAvailable = glBridge_.ComfortVignetteReady();
            model.comfortVignetteEnabled = comfortVignetteEnabled_;
            model.inputAvailable = statusPanelState_.inputAvailable;
            model.controllerTracked = statusPanelState_.controllerTracked;
            model.authoredCameraActive = statusPanelState_.authoredCameraActive;
            model.playerState = statusPanelState_.playerState;
            model.gameFrame = statusPanelState_.gameFrame;

            hud_math::HudQuadPose quadPose;
            const OpenXRGLBridge::StatusPanelSwapchain& panel = glBridge_.StatusPanel();
            const bool rasterized = status_panel_math::RasterizePanel(
                model, panel.width, panel.height, statusPanelPixels_);
            const bool poseValid = hud_math::BuildHeadLockedQuadPose(
                {},
                {},
                statusPanelDistanceMeters_,
                statusPanelVerticalOffsetMeters_,
                statusPanelWidthMeters_,
                static_cast<float>(panel.width) / static_cast<float>(panel.height),
                quadPose);
            if (rasterized && poseValid && glBridge_.DrawStatusPanelToSwapchain(statusPanelPixels_)) {
                statusPanelLayer.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
                statusPanelLayer.space = viewSpace_;
                statusPanelLayer.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
                statusPanelLayer.pose.orientation = {
                    quadPose.orientation.x,
                    quadPose.orientation.y,
                    quadPose.orientation.z,
                    quadPose.orientation.w,
                };
                statusPanelLayer.pose.position = {
                    quadPose.position.x,
                    quadPose.position.y,
                    quadPose.position.z,
                };
                statusPanelLayer.size = {quadPose.widthMeters, quadPose.heightMeters};
                statusPanelLayer.subImage.swapchain = panel.handle;
                statusPanelLayer.subImage.imageRect.offset = {0, 0};
                statusPanelLayer.subImage.imageRect.extent = {panel.width, panel.height};
                statusPanelLayer.subImage.imageArrayIndex = 0;
                layers[layerCount++] = reinterpret_cast<const XrCompositionLayerBaseHeader*>(
                    &statusPanelLayer);
                submittedStatusPanel = true;
                ++statusPanelSubmittedFrames_;
            } else {
                ++statusPanelSubmissionFailures_;
                if (statusPanelSubmissionFailures_ <= 4
                    || statusPanelSubmissionFailures_ % 120 == 0) {
                    Logger::Instance().Write(
                        LogLevel::Warn,
                        "openxr_status_panel submission_failed frame=%llu failures=%llu raster=%d pose=%d",
                        static_cast<unsigned long long>(frameIndex),
                        static_cast<unsigned long long>(statusPanelSubmissionFailures_),
                        rasterized ? 1 : 0,
                        poseValid ? 1 : 0);
                }
            }
        }

        if (frameState.shouldRender == XR_TRUE
            && layerCount > 0
            && comfortVignetteEnabled_
            && comfortVignetteLevel_ > 0.001f
            && stereoSubmissionEnabled_
            && viewSpace_ != XR_NULL_HANDLE
            && glBridge_.ComfortVignetteReady()) {
            const OpenXRGLBridge::ComfortVignetteSwapchain& vignette =
                glBridge_.ComfortVignette();
            hud_math::HudQuadPose quadPose;
            const bool rasterized = comfort_vignette_math::Rasterize(
                vignette.width,
                comfortVignetteLevel_,
                comfortVignetteStrength_,
                comfortVignetteInnerRadius_,
                comfortVignettePixels_);
            const bool poseValid = hud_math::BuildHeadLockedQuadPose(
                {},
                {},
                comfortVignetteDistanceMeters_,
                0.0f,
                comfortVignetteWidthMeters_,
                1.0f,
                quadPose);
            if (rasterized && poseValid
                && glBridge_.DrawComfortVignetteToSwapchain(comfortVignettePixels_)) {
                comfortVignetteLayer.layerFlags =
                    XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
                comfortVignetteLayer.space = viewSpace_;
                comfortVignetteLayer.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
                comfortVignetteLayer.pose.orientation = {
                    quadPose.orientation.x,
                    quadPose.orientation.y,
                    quadPose.orientation.z,
                    quadPose.orientation.w,
                };
                comfortVignetteLayer.pose.position = {
                    quadPose.position.x,
                    quadPose.position.y,
                    quadPose.position.z,
                };
                comfortVignetteLayer.size = {quadPose.widthMeters, quadPose.heightMeters};
                comfortVignetteLayer.subImage.swapchain = vignette.handle;
                comfortVignetteLayer.subImage.imageRect.offset = {0, 0};
                comfortVignetteLayer.subImage.imageRect.extent = {
                    vignette.width,
                    vignette.height,
                };
                comfortVignetteLayer.subImage.imageArrayIndex = 0;
                layers[layerCount++] = reinterpret_cast<const XrCompositionLayerBaseHeader*>(
                    &comfortVignetteLayer);
                submittedComfortVignette = true;
                ++comfortVignetteSubmittedFrames_;
            } else {
                ++comfortVignetteSubmissionFailures_;
                if (comfortVignetteSubmissionFailures_ <= 4
                    || comfortVignetteSubmissionFailures_ % 120 == 0) {
                    Logger::Instance().Write(
                        LogLevel::Warn,
                        "openxr_comfort_vignette submission_failed frame=%llu failures=%llu raster=%d pose=%d",
                        static_cast<unsigned long long>(frameIndex),
                        static_cast<unsigned long long>(comfortVignetteSubmissionFailures_),
                        rasterized ? 1 : 0,
                        poseValid ? 1 : 0);
                }
            }
        }

        const bool comfortBlackout = comfortBlackoutUntilFrame_ != 0
            && frameIndex <= comfortBlackoutUntilFrame_;
        const bool presentationBlackout = presentationBlackoutActive_;
        if (comfortBlackout || presentationBlackout) {
            layerCount = 0;
            if (comfortBlackout) ++comfortBlackoutFrames_;
            if (presentationBlackout) ++presentationBlackoutFrames_;
        }
        if (!comfortBlackout && comfortBlackoutUntilFrame_ != 0) {
            Logger::Instance().Write(
                LogLevel::Info,
                "openxr_comfort_blackout complete frame=%llu totalBlackFrames=%llu",
                static_cast<unsigned long long>(frameIndex),
                static_cast<unsigned long long>(comfortBlackoutFrames_));
            comfortBlackoutUntilFrame_ = 0;
        }

        XrFrameEndInfo endInfo{XR_TYPE_FRAME_END_INFO};
        endInfo.displayTime = frameState.predictedDisplayTime;
        endInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
        endInfo.layerCount = layerCount;
        endInfo.layers = layerCount > 0 ? layers : nullptr;
        result = xrEndFrame(session_, &endInfo);
        if (XR_FAILED(result)) {
            if (submittedHudCylinder
                && (result == XR_ERROR_LAYER_INVALID
                    || result == XR_ERROR_VALIDATION_FAILURE)) {
                hudCylinderSubmissionDisabled_ = true;
                Logger::Instance().Write(
                    LogLevel::Warn,
                    "openxr_hud shape_fallback requested=cylinder effective=quad reason=xrEndFrame result=%s frame=%llu",
                    XrResultString(result).c_str(),
                    static_cast<unsigned long long>(frameIndex));
            }
            RecordFrameFailureLocked("xrEndFrame", result, frameIndex);
            return;
        }

        if (desktopMirrorEyeIndex_ >= 0
            && !presentationBlackout
            && stereoSubmissionEnabled_
            && glBridge_.StereoCachesReady()) {
            const bool mirrored = glBridge_.CopyCacheToBackbuffer(
                static_cast<uint32_t>(desktopMirrorEyeIndex_),
                desktopMirrorAspectMode_);
            if (mirrored) {
                ++desktopMirrorFrames_;
                if (desktopMirrorFrames_ == 1 || desktopMirrorFrames_ % 600 == 0) {
                    Logger::Instance().Write(
                        LogLevel::Info,
                        "openxr_desktop_mirror applied frame=%llu eye=%s aspect=%s frames=%llu failures=%llu",
                        static_cast<unsigned long long>(frameIndex),
                        desktopMirrorEye_.c_str(),
                        desktopMirrorAspect_.c_str(),
                        static_cast<unsigned long long>(desktopMirrorFrames_),
                        static_cast<unsigned long long>(desktopMirrorFailures_));
                }
            } else {
                ++desktopMirrorFailures_;
                if (desktopMirrorFailures_ <= 2 || desktopMirrorFailures_ % 120 == 0) {
                    Logger::Instance().Write(
                        LogLevel::Warn,
                        "openxr_desktop_mirror failed frame=%llu eye=%s aspect=%s failures=%llu fallback=native_backbuffer",
                        static_cast<unsigned long long>(frameIndex),
                        desktopMirrorEye_.c_str(),
                        desktopMirrorAspect_.c_str(),
                        static_cast<unsigned long long>(desktopMirrorFailures_));
                }
            }
        }

        consecutiveFrameFailures_ = 0;
        ++completedXrFrameCount_;
        if (layerCount > 0) {
            ++submittedFrameCount_;
            lastSubmittedGameFrame_ = frameIndex;
            if (submittedStereo) {
                ++stereoSubmittedFrameCount_;
            }
            if (submittedDepth) {
                ++depthSubmittedFrameCount_;
            }
        }

        if (completedXrFrameCount_ == 1 || (completedXrFrameCount_ % 300) == 0) {
            Logger::Instance().Write(
                LogLevel::Info,
                "openxr_frame ok gameFrame=%llu xrFrame=%llu shouldRender=%d layers=%u views=%u stereo=%d depth=%d hud=%d hudShape=%s reticle=%d statusPanel=%d comfortVignette=%d comfortVignetteLevel=%.3f spectatorFrames=%llu stereoCaptured=%llu stereoSubmitted=%llu depthSubmitted=%llu depthFailures=%llu hudSubmitted=%llu reticleSubmitted=%llu panelSubmitted=%llu vignetteSubmitted=%llu predictedDisplayTime=%lld leftPos=%.4f,%.4f,%.4f rightPos=%.4f,%.4f,%.4f",
                static_cast<unsigned long long>(frameIndex),
                static_cast<unsigned long long>(completedXrFrameCount_),
                frameState.shouldRender == XR_TRUE ? 1 : 0,
                layerCount,
                locatedViewCount,
                submittedStereo ? 1 : 0,
                submittedDepth ? 1 : 0,
                submittedHud ? 1 : 0,
                submittedHud ? (submittedHudCylinder ? "cylinder" : "quad") : "none",
                submittedInteractionReticle ? 1 : 0,
                submittedStatusPanel ? 1 : 0,
                submittedComfortVignette ? 1 : 0,
                comfortVignetteLevel_,
                static_cast<unsigned long long>(desktopMirrorFrames_),
                static_cast<unsigned long long>(stereoCapturedEyeCount_),
                static_cast<unsigned long long>(stereoSubmittedFrameCount_),
                static_cast<unsigned long long>(depthSubmittedFrameCount_),
                static_cast<unsigned long long>(depthSubmissionFailures_),
                static_cast<unsigned long long>(hudSubmittedFrames_),
                static_cast<unsigned long long>(interactionReticleSubmittedFrames_),
                static_cast<unsigned long long>(statusPanelSubmittedFrames_),
                static_cast<unsigned long long>(comfortVignetteSubmittedFrames_),
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
                        if (recoveryEnabled_) {
                            recoveryRequested_ = true;
                            Logger::Instance().Write(
                                LogLevel::Warn,
                                "openxr_recovery requested source=session_state state=%s frame=%llu",
                                SessionStateName(state->state),
                                static_cast<unsigned long long>(frameIndex));
                            return;
                        }
                    }
                }
            } else if (event.type == XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED) {
                input_.LogInteractionProfiles(session_, frameIndex, "runtime_event");
            } else if (event.type == XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING) {
                frameSubmitFailed_ = true;
                sessionRunning_ = false;
                if (recoveryEnabled_) {
                    recoveryRequested_ = true;
                    Logger::Instance().Write(
                        LogLevel::Warn,
                        "openxr_recovery requested source=instance_loss frame=%llu",
                        static_cast<unsigned long long>(frameIndex));
                    return;
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
    std::string desktopMirrorEye_ = "native";
    std::string desktopMirrorAspect_ = "fit";
    bool depthCompositionProbeEnabled_ = false;
    bool depthCompositionSubmitEnabled_ = false;
    bool depthExtensionAvailable_ = false;
    bool depthExtensionEnabled_ = false;
    bool depthCapabilityLogged_ = false;
    int desktopMirrorEyeIndex_ = -1;
    spectator_math::AspectMode desktopMirrorAspectMode_ = spectator_math::AspectMode::Fit;
    uint64_t desktopMirrorFrames_ = 0;
    uint64_t desktopMirrorFailures_ = 0;
    int resolutionScalePercent_ = 100;
    std::string requestedReferenceSpace_ = "local";
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
    bool recoveryEnabled_ = true;
    bool recoveryRequested_ = false;
    int recoveryDelayFrames_ = 120;
    int trackingHoldFrames_ = 30;
    int trackingRecoveryBlackoutFrames_ = 2;
    bool hudLayerEnabled_ = false;
    bool hudCylinderRequested_ = false;
    bool hudCylinderExtensionAvailable_ = false;
    bool hudCylinderExtensionEnabled_ = false;
    bool hudCylinderSubmissionDisabled_ = false;
    float hudCylinderAngleDegrees_ = 70.0f;
    bool hudSubmissionSuspended_ = false;
    int hudWidthPixels_ = 1600;
    int hudHeightPixels_ = 900;
    float hudDistanceMeters_ = 1.5f;
    float hudWidthMeters_ = 1.6f;
    float hudVerticalOffsetMeters_ = 0.0f;
    int hudMaxAgeFrames_ = 2;
    bool hudSuppressCenterCrosshair_ = false;
    int hudCrosshairClearRadiusPixels_ = 48;
    bool interactionReticleEnabled_ = false;
    bool interactionReticleSemanticEnabled_ = false;
    bool interactionReticleNativeIconsEnabled_ = false;
    bool interactionReticleSubmissionSuspended_ = false;
    int interactionReticleSizePixels_ = 64;
    float interactionReticleAngularSizeDegrees_ = 0.75f;
    float interactionReticleMinSizeMeters_ = 0.008f;
    float interactionReticleMaxSizeMeters_ = 0.08f;
    float interactionReticleMinDistanceMeters_ = 0.15f;
    float interactionReticleMaxDistanceMeters_ = 8.0f;
    int interactionReticleMaxAgeFrames_ = 2;
    bool hudRuntimeVisible_ = false;
    bool interactionReticleRuntimeVisible_ = false;
    bool statusPanelEnabled_ = false;
    int statusPanelWidthPixels_ = 1024;
    int statusPanelHeightPixels_ = 512;
    float statusPanelDistanceMeters_ = 1.25f;
    float statusPanelWidthMeters_ = 1.15f;
    float statusPanelVerticalOffsetMeters_ = 0.0f;
    bool comfortVignetteConfigured_ = false;
    bool comfortVignetteEnabled_ = false;
    int comfortVignetteSizePixels_ = 256;
    float comfortVignetteDistanceMeters_ = 0.30f;
    float comfortVignetteWidthMeters_ = 1.0f;
    float comfortVignetteStrength_ = 0.60f;
    float comfortVignetteInnerRadius_ = 0.50f;
    int comfortVignetteFadeMilliseconds_ = 250;
    int comfortVignetteMaxMotionAgeFrames_ = 8;
    float comfortVignetteTarget_ = 0.0f;
    float comfortVignetteLevel_ = 0.0f;
    bool trackingDegraded_ = false;
    bool trackingLost_ = false;
    uint64_t retryFrame_ = 0;
    uint64_t releaseFrame_ = 0;
    uint64_t sessionCreatedFrame_ = 0;
    uint64_t manualStartFrame_ = 0;
    uint64_t completedXrFrameCount_ = 0;
    uint64_t submittedFrameCount_ = 0;
    uint64_t lastSubmittedGameFrame_ = 0;
    uint64_t currentGameFrame_ = 0;
    uint64_t gameplayHapticRequestCount_ = 0;
    uint32_t consecutiveFrameFailures_ = 0;
    uint32_t frameErrorLogCount_ = 0;
    uint32_t pendingRenderedEye_ = 0;
    uint32_t lastCapturedStereoEye_ = 0;
    uint32_t stereoCaptureFailures_ = 0;
    uint64_t stereoCapturedEyeCount_ = 0;
    uint64_t stereoSubmittedFrameCount_ = 0;
    uint64_t depthSubmittedFrameCount_ = 0;
    uint64_t depthSubmissionFailures_ = 0;
    uint32_t hudConsecutiveFailures_ = 0;
    uint64_t hudCaptureStarts_ = 0;
    uint64_t hudCaptureCompletions_ = 0;
    uint64_t hudSubmittedFrames_ = 0;
    uint64_t hudSubmissionFailures_ = 0;
    uint32_t interactionReticleConsecutiveFailures_ = 0;
    uint64_t interactionReticleUpdates_ = 0;
    uint64_t interactionReticleSemanticUpdates_ = 0;
    uint64_t interactionReticleSemanticRejects_ = 0;
    uint64_t interactionReticleClears_ = 0;
    uint64_t interactionReticleExpired_ = 0;
    uint64_t interactionReticleSubmittedFrames_ = 0;
    uint64_t interactionReticleSubmissionFailures_ = 0;
    uint64_t statusPanelSubmittedFrames_ = 0;
    uint64_t statusPanelSubmissionFailures_ = 0;
    uint64_t comfortVignetteMotionFrame_ = 0;
    uint64_t comfortVignetteSubmittedFrames_ = 0;
    uint64_t comfortVignetteSubmissionFailures_ = 0;
    uint64_t runtimeRecoveries_ = 0;
    uint64_t glContextChangeEvents_ = 0;
    uint64_t viewResourceChecks_ = 0;
    uint64_t viewResourceRebuilds_ = 0;
    uint64_t nextViewConfigurationCheckFrame_ = 0;
    uint64_t trackingInvalidFrames_ = 0;
    uint64_t trackingLossEvents_ = 0;
    uint64_t trackingRestoreEvents_ = 0;
    uint64_t trackingDegradedStartFrame_ = 0;
    uint64_t stereoCacheInvalidations_ = 0;
    uint64_t comfortBlackoutUntilFrame_ = 0;
    uint64_t comfortBlackoutRequests_ = 0;
    uint64_t comfortBlackoutFrames_ = 0;
    bool presentationBlackoutActive_ = false;
    uint64_t presentationBlackoutTransitions_ = 0;
    uint64_t presentationBlackoutFrames_ = 0;
    OpenXREyeView pendingRenderedView_{};
    OpenXREyeView renderedStereoViews_[2] = {};
    OpenXRInteractionReticleState interactionReticleState_{};
    OpenXRStatusPanelState statusPanelState_{};
    std::vector<uint8_t> statusPanelPixels_;
    std::vector<uint8_t> comfortVignettePixels_;
    bool latestPoseValid_ = false;
    uint64_t latestPoseGameFrame_ = 0;
    XrViewStateFlags latestViewStateFlags_ = 0;
    float latestIpdMeters_ = 0.0f;
    XrPosef latestHeadPose_{};
    XrPosef latestLeftEyePose_{};
    XrPosef latestRightEyePose_{};
    HDC latestHdc_ = nullptr;
    HGLRC latestGlContext_ = nullptr;
    HDC sessionHdc_ = nullptr;
    HGLRC sessionGlContext_ = nullptr;
    XrInstance instance_ = XR_NULL_HANDLE;
    XrSystemId systemId_ = XR_NULL_SYSTEM_ID;
    XrSession session_ = XR_NULL_HANDLE;
    XrSpace appSpace_ = XR_NULL_HANDLE;
    XrSpace viewSpace_ = XR_NULL_HANDLE;
    XrReferenceSpaceType selectedReferenceSpace_ = XR_REFERENCE_SPACE_TYPE_LOCAL;
    HMODULE loaderModule_ = nullptr;
    XrSessionState sessionState_ = XR_SESSION_STATE_UNKNOWN;
    uint32_t eventLogCount_ = 0;
    uint32_t viewCount_ = 0;
    uint32_t swapchainFormatCount_ = 0;
    std::vector<XrViewConfigurationView> viewConfigurationViews_;
    std::vector<int64_t> swapchainFormats_;
    std::vector<XrView> locatedViews_;
    std::vector<XrView> pendingLocatedViews_;
    std::vector<XrReferenceSpaceType> supportedReferenceSpaces_;
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
        const std::string& desktopMirrorEye,
        const std::string& desktopMirrorAspect,
        bool depthCompositionProbe,
        bool depthCompositionSubmit,
        int resolutionScalePercent,
        const std::string& referenceSpace,
        bool inputEnabled,
        int inputLogInterval,
        bool recoveryEnabled,
        int recoveryDelayFrames,
        int trackingHoldFrames,
        int trackingRecoveryBlackoutFrames,
        bool hudLayerEnabled,
        const std::string& hudShape,
        float hudCylinderAngleDegrees,
        int hudWidthPixels,
        int hudHeightPixels,
        float hudDistanceMeters,
        float hudWidthMeters,
        float hudVerticalOffsetMeters,
        int hudMaxAgeFrames,
        bool hudSuppressCenterCrosshair,
        int hudCrosshairClearRadiusPixels,
        bool interactionReticleEnabled,
        bool interactionReticleSemanticEnabled,
        bool interactionReticleNativeIconsEnabled,
        int interactionReticleSizePixels,
        float interactionReticleAngularSizeDegrees,
        float interactionReticleMinSizeMeters,
        float interactionReticleMaxSizeMeters,
        float interactionReticleMinDistanceMeters,
        float interactionReticleMaxDistanceMeters,
        int interactionReticleMaxAgeFrames,
        bool statusPanelEnabled,
        int statusPanelWidthPixels,
        int statusPanelHeightPixels,
        float statusPanelDistanceMeters,
        float statusPanelWidthMeters,
        float statusPanelVerticalOffsetMeters,
        const OpenXRComfortVignetteSettings& comfortVignette)
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
        desktopMirrorEye_ = desktopMirrorEye;
        desktopMirrorAspect_ = desktopMirrorAspect;
        depthCompositionProbeEnabled_ = depthCompositionProbe;
        depthCompositionSubmitEnabled_ = depthCompositionSubmit;
        resolutionScalePercent_ = resolutionScalePercent;
        referenceSpace_ = referenceSpace;
        inputEnabled_ = inputEnabled;
        inputLogInterval_ = inputLogInterval;
        recoveryEnabled_ = recoveryEnabled;
        recoveryDelayFrames_ = recoveryDelayFrames;
        trackingHoldFrames_ = trackingHoldFrames;
        trackingRecoveryBlackoutFrames_ = trackingRecoveryBlackoutFrames;
        hudLayerEnabled_ = hudLayerEnabled;
        hudCylinderRequested_ = hudShape == "cylinder";
        hudCylinderAngleDegrees_ = hudCylinderAngleDegrees;
        statusPanelEnabled_ = statusPanelEnabled;
        comfortVignetteEnabled_ = comfortVignette.enabled;
        manualStartArmed_ = false;
        unavailableLogged_ = false;
        Logger::Instance().Write(
            LogLevel::Info,
            "openxr_config buildOpenXR=0 enabled=%d sessionProbe=%d releaseAfterProbe=%d bootstrapFrame=%llu holdFrames=%llu manualStart=%d key=F8 frameSubmit=%d mirrorBackbuffer=%d desktopMirrorEye=%s desktopMirrorAspect=%s depth={probe=%d submit=%d} resolutionScalePercent=%d referenceSpace=%s input=%d inputLogInterval=%d recovery=%d recoveryDelayFrames=%d trackingHoldFrames=%d trackingRecoveryBlackoutFrames=%d hud={enabled=%d shape=%s cylinderAngleDegrees=%.3f size=%dx%d distance=%.3f widthMeters=%.3f verticalOffset=%.3f maxAgeFrames=%d suppressCenterCrosshair=%d crosshairClearRadiusPixels=%d} reticle={enabled=%d semantic=%d nativeIcons=%d pixels=%d angularDeg=%.3f sizeMeters=%.4f..%.4f distanceMeters=%.3f..%.3f maxAgeFrames=%d}",
            enabled_ ? 1 : 0,
            sessionProbeEnabled_ ? 1 : 0,
            releaseAfterProbeEnabled_ ? 1 : 0,
            static_cast<unsigned long long>(bootstrapFrame_),
            static_cast<unsigned long long>(holdFrames_),
            manualStartEnabled_ ? 1 : 0,
            frameSubmitEnabled_ ? 1 : 0,
            mirrorBackbufferEnabled_ ? 1 : 0,
            desktopMirrorEye_.c_str(),
            desktopMirrorAspect_.c_str(),
            depthCompositionProbeEnabled_ ? 1 : 0,
            depthCompositionSubmitEnabled_ ? 1 : 0,
            resolutionScalePercent_,
            referenceSpace_.c_str(),
            inputEnabled_ ? 1 : 0,
            inputLogInterval_,
            recoveryEnabled_ ? 1 : 0,
            recoveryDelayFrames_,
            trackingHoldFrames_,
            trackingRecoveryBlackoutFrames_,
            hudLayerEnabled_ ? 1 : 0,
            hudCylinderRequested_ ? "cylinder" : "quad",
            hudCylinderAngleDegrees_,
            hudWidthPixels,
            hudHeightPixels,
            hudDistanceMeters,
            hudWidthMeters,
            hudVerticalOffsetMeters,
            hudMaxAgeFrames,
            hudSuppressCenterCrosshair ? 1 : 0,
            hudCrosshairClearRadiusPixels,
            interactionReticleEnabled ? 1 : 0,
            interactionReticleSemanticEnabled ? 1 : 0,
            interactionReticleNativeIconsEnabled ? 1 : 0,
            interactionReticleSizePixels,
            interactionReticleAngularSizeDegrees,
            interactionReticleMinSizeMeters,
            interactionReticleMaxSizeMeters,
            interactionReticleMinDistanceMeters,
            interactionReticleMaxDistanceMeters,
            interactionReticleMaxAgeFrames);
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
            << " openxrHudLayer=" << (hudLayerEnabled_ ? 1 : 0)
            << " openxrHudShapeRequested=" << (hudCylinderRequested_ ? "cylinder" : "quad")
            << " openxrHudShapeEffective=quad"
            << " openxrHudCylinderAngleDegrees=" << hudCylinderAngleDegrees_
            << " openxrComfortVignetteEnabled=" << (comfortVignetteEnabled_ ? 1 : 0)
            << " openxrDepthCompositionProbe=" << (depthCompositionProbeEnabled_ ? 1 : 0)
            << " openxrDepthCompositionSubmit=" << (depthCompositionSubmitEnabled_ ? 1 : 0)
            << " openxrMirrorBackbuffer=" << (mirrorBackbufferEnabled_ ? 1 : 0)
            << " openxrResolutionScalePercent=" << resolutionScalePercent_
            << " openxrInputEnabled=" << (inputEnabled_ ? 1 : 0)
            << " openxrRecoveryEnabled=" << (recoveryEnabled_ ? 1 : 0)
            << " openxrRecoveryPending=0 openxrRecoveries=0 openxrStereoCacheInvalidations=0"
            << " openxrComfortBlackoutUntilFrame=0 openxrComfortBlackoutRequests=0 openxrComfortBlackoutFrames=0"
            << " openxrPresentationBlackout=0 openxrPresentationBlackoutTransitions=0 openxrPresentationBlackoutFrames=0"
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

    bool RequestHapticPulse(uint32_t, float, int, const char*) { return false; }
    bool StopHaptic(uint32_t, const char*) { return false; }
    void SetInteractionReticle(const OpenXRInteractionReticleState&) {}
    void SetInteractionReticleSemantic(int) {}
    void ClearInteractionReticle() {}
    void SetStatusPanel(const OpenXRStatusPanelState&) {}
    void SetHudRuntimeVisible(bool) {}
    bool ToggleHudLayerShape() { return false; }
    OpenXRHudLayerShapeStatus GetHudLayerShapeStatus() const { return {}; }
    void SetInteractionReticleRuntimeVisible(bool) {}
    void SetComfortMotionIntensity(float, uint64_t) {}
    bool ToggleComfortVignette() { return false; }
    OpenXRComfortVignetteStatus GetComfortVignetteStatus() const
    {
        OpenXRComfortVignetteStatus status;
        status.enabled = comfortVignetteEnabled_;
        return status;
    }

    void SetStereoSubmissionEnabled(bool) {}
    bool MarkRenderedStereoEye(uint32_t, const OpenXREyeView&) { return false; }
    bool CapturePendingStereoEye(uint64_t, const char*) { return false; }
    void InvalidateStereoCaches(const char*) {}
    void RequestComfortBlackout(uint32_t, const char*) {}
    void SetPresentationBlackout(bool, const char*) {}
    bool BeginHudCapture(uint64_t) { return false; }
    bool EndHudCapture(uint64_t, bool) { return false; }

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
    bool recoveryEnabled_ = true;
    int recoveryDelayFrames_ = 120;
    int trackingHoldFrames_ = 30;
    int trackingRecoveryBlackoutFrames_ = 2;
    bool hudLayerEnabled_ = false;
    bool hudCylinderRequested_ = false;
    float hudCylinderAngleDegrees_ = 70.0f;
    bool statusPanelEnabled_ = false;
    bool comfortVignetteEnabled_ = false;
    bool mirrorBackbufferEnabled_ = true;
    std::string desktopMirrorEye_ = "native";
    std::string desktopMirrorAspect_ = "fit";
    bool depthCompositionProbeEnabled_ = false;
    bool depthCompositionSubmitEnabled_ = false;
    int resolutionScalePercent_ = 100;
    std::string referenceSpace_ = "local";
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
    const std::string& desktopMirrorEye,
    const std::string& desktopMirrorAspect,
    bool depthCompositionProbe,
    bool depthCompositionSubmit,
    int resolutionScalePercent,
    const std::string& referenceSpace,
    bool inputEnabled,
    int inputLogInterval,
    bool recoveryEnabled,
    int recoveryDelayFrames,
    int trackingHoldFrames,
    int trackingRecoveryBlackoutFrames,
    bool hudLayerEnabled,
    const std::string& hudShape,
    float hudCylinderAngleDegrees,
    int hudWidthPixels,
    int hudHeightPixels,
    float hudDistanceMeters,
    float hudWidthMeters,
    float hudVerticalOffsetMeters,
    int hudMaxAgeFrames,
    bool hudSuppressCenterCrosshair,
    int hudCrosshairClearRadiusPixels,
    bool interactionReticleEnabled,
    bool interactionReticleSemanticEnabled,
    bool interactionReticleNativeIconsEnabled,
    int interactionReticleSizePixels,
    float interactionReticleAngularSizeDegrees,
    float interactionReticleMinSizeMeters,
    float interactionReticleMaxSizeMeters,
    float interactionReticleMinDistanceMeters,
    float interactionReticleMaxDistanceMeters,
    int interactionReticleMaxAgeFrames,
    bool statusPanelEnabled,
    int statusPanelWidthPixels,
    int statusPanelHeightPixels,
    float statusPanelDistanceMeters,
    float statusPanelWidthMeters,
    float statusPanelVerticalOffsetMeters,
    const OpenXRComfortVignetteSettings& comfortVignette)
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
        desktopMirrorEye,
        desktopMirrorAspect,
        depthCompositionProbe,
        depthCompositionSubmit,
        resolutionScalePercent,
        referenceSpace,
        inputEnabled,
        inputLogInterval,
        recoveryEnabled,
        recoveryDelayFrames,
        trackingHoldFrames,
        trackingRecoveryBlackoutFrames,
        hudLayerEnabled,
        hudShape,
        hudCylinderAngleDegrees,
        hudWidthPixels,
        hudHeightPixels,
        hudDistanceMeters,
        hudWidthMeters,
        hudVerticalOffsetMeters,
        hudMaxAgeFrames,
        hudSuppressCenterCrosshair,
        hudCrosshairClearRadiusPixels,
        interactionReticleEnabled,
        interactionReticleSemanticEnabled,
        interactionReticleNativeIconsEnabled,
        interactionReticleSizePixels,
        interactionReticleAngularSizeDegrees,
        interactionReticleMinSizeMeters,
        interactionReticleMaxSizeMeters,
        interactionReticleMinDistanceMeters,
        interactionReticleMaxDistanceMeters,
        interactionReticleMaxAgeFrames,
        statusPanelEnabled,
        statusPanelWidthPixels,
        statusPanelHeightPixels,
        statusPanelDistanceMeters,
        statusPanelWidthMeters,
        statusPanelVerticalOffsetMeters,
        comfortVignette);
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

void OpenXRRuntime::SetInteractionReticle(const OpenXRInteractionReticleState& state)
{
    impl_->SetInteractionReticle(state);
}

void OpenXRRuntime::SetInteractionReticleSemantic(int crosshairState)
{
    impl_->SetInteractionReticleSemantic(crosshairState);
}

void OpenXRRuntime::ClearInteractionReticle()
{
    impl_->ClearInteractionReticle();
}

void OpenXRRuntime::SetStatusPanel(const OpenXRStatusPanelState& state)
{
    impl_->SetStatusPanel(state);
}

void OpenXRRuntime::SetHudRuntimeVisible(bool visible)
{
    impl_->SetHudRuntimeVisible(visible);
}

bool OpenXRRuntime::ToggleHudLayerShape()
{
    return impl_->ToggleHudLayerShape();
}

OpenXRHudLayerShapeStatus OpenXRRuntime::GetHudLayerShapeStatus() const
{
    return impl_->GetHudLayerShapeStatus();
}

void OpenXRRuntime::SetInteractionReticleRuntimeVisible(bool visible)
{
    impl_->SetInteractionReticleRuntimeVisible(visible);
}

void OpenXRRuntime::SetComfortMotionIntensity(float intensity, uint64_t gameFrame)
{
    impl_->SetComfortMotionIntensity(intensity, gameFrame);
}

bool OpenXRRuntime::ToggleComfortVignette()
{
    return impl_->ToggleComfortVignette();
}

OpenXRComfortVignetteStatus OpenXRRuntime::GetComfortVignetteStatus() const
{
    return impl_->GetComfortVignetteStatus();
}

bool OpenXRRuntime::RequestHapticPulse(uint32_t hand, float amplitude, int durationMs, const char* reason)
{
    return impl_->RequestHapticPulse(hand, amplitude, durationMs, reason);
}

bool OpenXRRuntime::StopHaptic(uint32_t hand, const char* reason)
{
    return impl_->StopHaptic(hand, reason);
}

void OpenXRRuntime::SetStereoSubmissionEnabled(bool enabled)
{
    impl_->SetStereoSubmissionEnabled(enabled);
}

bool OpenXRRuntime::MarkRenderedStereoEye(uint32_t eyeIndex, const OpenXREyeView& view)
{
    return impl_->MarkRenderedStereoEye(eyeIndex, view);
}

bool OpenXRRuntime::CapturePendingStereoEye(uint64_t frameIndex, const char* source)
{
    return impl_->CapturePendingStereoEye(frameIndex, source);
}

void OpenXRRuntime::InvalidateStereoCaches(const char* reason)
{
    impl_->InvalidateStereoCaches(reason);
}

void OpenXRRuntime::RequestComfortBlackout(uint32_t frames, const char* reason)
{
    impl_->RequestComfortBlackout(frames, reason);
}

void OpenXRRuntime::SetPresentationBlackout(bool active, const char* reason)
{
    impl_->SetPresentationBlackout(active, reason);
}

bool OpenXRRuntime::BeginHudCapture(uint64_t frameIndex)
{
    return impl_->BeginHudCapture(frameIndex);
}

bool OpenXRRuntime::EndHudCapture(uint64_t frameIndex, bool suppressCenterCrosshair)
{
    return impl_->EndHudCapture(frameIndex, suppressCenterCrosshair);
}

} // namespace somavr
