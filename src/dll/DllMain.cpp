#include "Config.h"
#include "BuildInfo.h"
#include "CrashHandler.h"
#include "HPLCameraBridge.h"
#include "HPLComfortBridge.h"
#include "HPLCompatibilityProbe.h"
#include "HPLContactHapticsBridge.h"
#include "HPLCrosshairBridge.h"
#include "HPLLifecycle.h"
#include "HPLInputBridge.h"
#include "HPLInteractionBridge.h"
#include "HPLGrabBridge.h"
#include "HPLGameplayHapticsBridge.h"
#include "HPLHandsBridge.h"
#include "HPLHudBridge.h"
#include "HPLMenuBridge.h"
#include "HPLNativeLocomotion.h"
#include "HPLPlayerState.h"
#include "HPLPresentationBridge.h"
#include "HPLScreenEffectBridge.h"
#include "HPLSubtitleBridge.h"
#include "HPLStatusPanelBridge.h"
#include "HPLTerminalBridge.h"
#include "HPLUserModuleBridge.h"
#include "Logger.h"
#include "OpenGLHooks.h"
#include "OpenXRRuntime.h"
#include "StartupConfigLog.h"

#include <Windows.h>
#include <TlHelp32.h>

#include <cstdio>
#include <filesystem>
#include <memory>
#include <string>

namespace {

HMODULE g_module = nullptr;
HANDLE g_stopEvent = nullptr;
HANDLE g_workerThread = nullptr;
std::unique_ptr<somavr::ConfigManager> g_config;
std::unique_ptr<somavr::OpenXRRuntime> g_openxr;

bool IsOnlyCurrentThreadRemaining()
{
    const DWORD processId = GetCurrentProcessId();
    const DWORD currentThreadId = GetCurrentThreadId();
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return false;
    }

    THREADENTRY32 entry = {};
    entry.dwSize = sizeof(entry);
    DWORD processThreadCount = 0;
    bool currentThreadFound = false;
    if (Thread32First(snapshot, &entry)) {
        do {
            if (entry.th32OwnerProcessID == processId) {
                ++processThreadCount;
                currentThreadFound |= entry.th32ThreadID == currentThreadId;
            }
        } while (Thread32Next(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return currentThreadFound && processThreadCount == 1;
}

// Config identity needs the file's own timestamp and size, not just its path: a
// cloud-synced or redirected copy has the right path and the wrong contents, and
// that failure looks identical to a stale build.
std::string DescribeFileStamp(const std::filesystem::path& path)
{
    WIN32_FILE_ATTRIBUTE_DATA attributes = {};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &attributes)) {
        return "mtime=unavailable bytes=0";
    }

    SYSTEMTIME utc = {};
    char stamp[64] = {};
    if (FileTimeToSystemTime(&attributes.ftLastWriteTime, &utc)) {
        snprintf(
            stamp,
            sizeof(stamp),
            "%04u-%02u-%02uT%02u:%02u:%02uZ",
            utc.wYear, utc.wMonth, utc.wDay, utc.wHour, utc.wMinute, utc.wSecond);
    } else {
        snprintf(stamp, sizeof(stamp), "unavailable");
    }

    const uint64_t bytes = (static_cast<uint64_t>(attributes.nFileSizeHigh) << 32)
        | attributes.nFileSizeLow;
    char described[128] = {};
    snprintf(
        described,
        sizeof(described),
        "mtime=%s bytes=%llu",
        stamp,
        static_cast<unsigned long long>(bytes));
    return described;
}

DWORD WINAPI WorkerThreadProc(LPVOID)
{
    somavr::InitializeWorkRoot(g_module);
    somavr::Logger::Instance().Initialize(somavr::LogPath(), somavr::LogLevel::Info);
    somavr::Logger::Instance().Write(
        somavr::LogLevel::Warn,
        "runtime_paths root=%s source=%s module=%s",
        somavr::WorkRoot().string().c_str(),
        somavr::WorkRootSource().c_str(),
        somavr::ModulePath(g_module).c_str());
    somavr::LogBuildIdentity(g_module);
    somavr::InitializeCrashHandler(g_module, somavr::WorkRoot() / "logs" / "dumps");

    g_config = std::make_unique<somavr::ConfigManager>();
    if (!g_config->Initialize()) {
        somavr::Logger::Instance().Write(somavr::LogLevel::Error, "config_init failed");
        somavr::LogCrashHandlerSummary();
        somavr::ShutdownCrashHandler();
        somavr::Logger::Instance().Shutdown();
        return 1;
    }

    somavr::Logger::Instance().SetLevel(g_config->Get().logLevel);
    somavr::Logger::Instance().Write(
        somavr::LogLevel::Warn,
        "config_applied path=%s %s logLevel=%s parsedKeyHash=0x%016llx accepted=%u unknownKeys=%u unknownSections=%u",
        g_config->Path().string().c_str(),
        DescribeFileStamp(g_config->Path()).c_str(),
        somavr::Logger::LevelName(g_config->Get().logLevel),
        static_cast<unsigned long long>(g_config->ParsedKeyHash()),
        g_config->AcceptedKeyCount(),
        g_config->UnknownKeyCount(),
        g_config->UnknownSectionCount());
    somavr::LogStartupConfig(g_config->Get());
    somavr::Logger::Instance().Write(
        somavr::LogLevel::Info,
        "gameplay_haptics_config enabled=%d amplitudeScale=%.3f minAmplitude=%.3f retriggerDelta=%.3f refreshMs=%d segmentMs=%d",
        g_config->Get().hplControllerGameplayHaptics ? 1 : 0,
        g_config->Get().hplControllerGameplayHapticAmplitudeScale,
        g_config->Get().hplControllerGameplayHapticMinAmplitude,
        g_config->Get().hplControllerGameplayHapticRetriggerDelta,
        g_config->Get().hplControllerGameplayHapticRefreshMs,
        g_config->Get().hplControllerGameplayHapticSegmentMs);
    somavr::Logger::Instance().Write(
        somavr::LogLevel::Info,
        "hud_config enabled=%d shape=%s cylinderAngleDegrees=%.3f size=%dx%d distanceMeters=%.3f widthMeters=%.3f verticalOffsetMeters=%.3f maxAgeFrames=%d suppressCenterCrosshair=%d crosshairClearRadiusPixels=%d",
        g_config->Get().openxrHudLayer ? 1 : 0,
        g_config->Get().openxrHudShape.c_str(),
        g_config->Get().openxrHudCylinderAngleDegrees,
        g_config->Get().openxrHudWidthPixels,
        g_config->Get().openxrHudHeightPixels,
        g_config->Get().openxrHudDistanceMeters,
        g_config->Get().openxrHudWidthMeters,
        g_config->Get().openxrHudVerticalOffsetMeters,
        g_config->Get().openxrHudMaxAgeFrames,
        g_config->Get().openxrHudSuppressCenterCrosshair ? 1 : 0,
        g_config->Get().openxrHudCrosshairClearRadiusPixels);
    somavr::Logger::Instance().Write(
        somavr::LogLevel::Info,
        "interaction_reticle_config enabled=%d semantic=%d nativeIcons=%d pixels=%d angularSizeDegrees=%.3f sizeMeters=%.4f..%.4f distanceMeters=%.3f..%.3f maxAgeFrames=%d focusHaptics=%d focusHapticAmplitude=%.3f focusHapticDurationMs=%d focusHapticCooldownFrames=%d",
        g_config->Get().openxrInteractionReticle ? 1 : 0,
        g_config->Get().openxrInteractionReticleSemantic ? 1 : 0,
        g_config->Get().openxrInteractionReticleNativeIcons ? 1 : 0,
        g_config->Get().openxrInteractionReticleSizePixels,
        g_config->Get().openxrInteractionReticleAngularSizeDegrees,
        g_config->Get().openxrInteractionReticleMinSizeMeters,
        g_config->Get().openxrInteractionReticleMaxSizeMeters,
        g_config->Get().openxrInteractionReticleMinDistanceMeters,
        g_config->Get().openxrInteractionReticleMaxDistanceMeters,
        g_config->Get().openxrInteractionReticleMaxAgeFrames,
        g_config->Get().hplControllerFocusHaptics ? 1 : 0,
        g_config->Get().hplControllerFocusHapticAmplitude,
        g_config->Get().hplControllerFocusHapticDurationMs,
        g_config->Get().hplControllerFocusHapticCooldownFrames);
    somavr::Logger::Instance().Write(
        somavr::LogLevel::Info,
        "comfort_config cameraAddControl=%d suppressHeadBob=%d suppressCameraShake=%d suppressSway=%d cameraRollControl=%d suppressRoll={script=%d lean=%d move=%d climb=%d} depthOfFieldControl=%d opticsControl=%d suppressOptics={fov=%d fovMultiplier=%d aspectMultiplier=%d} stateTransitionBlackoutFrames=%d videoDistortion=%d loadingScreenControl=%d loadingScreenExitBlackoutFrames=%d videoLifecycleProbe=%d logInterval=%d",
        g_config->Get().hplComfortCameraAddControl ? 1 : 0,
        g_config->Get().hplComfortSuppressHeadBob ? 1 : 0,
        g_config->Get().hplComfortSuppressCameraShake ? 1 : 0,
        g_config->Get().hplComfortSuppressSway ? 1 : 0,
        g_config->Get().hplComfortCameraRollControl ? 1 : 0,
        g_config->Get().hplComfortSuppressScriptRoll ? 1 : 0,
        g_config->Get().hplComfortSuppressLeanRoll ? 1 : 0,
        g_config->Get().hplComfortSuppressMoveRoll ? 1 : 0,
        g_config->Get().hplComfortSuppressClimbRoll ? 1 : 0,
        g_config->Get().hplComfortDepthOfFieldControl ? 1 : 0,
        g_config->Get().hplComfortOpticsControl ? 1 : 0,
        g_config->Get().hplComfortSuppressFov ? 1 : 0,
        g_config->Get().hplComfortSuppressFovMultiplier ? 1 : 0,
        g_config->Get().hplComfortSuppressAspectMultiplier ? 1 : 0,
        g_config->Get().hplControllerStateTransitionBlackoutFrames,
        g_config->Get().hplPostEffectDisableVideoDistortion ? 1 : 0,
        g_config->Get().hplLoadingScreenControl ? 1 : 0,
        g_config->Get().hplLoadingScreenExitBlackoutFrames,
        g_config->Get().hplVideoLifecycleProbe ? 1 : 0,
        g_config->Get().hplComfortLogInterval);

    g_openxr = std::make_unique<somavr::OpenXRRuntime>();
    somavr::OpenXRComfortVignetteSettings comfortVignette;
    comfortVignette.enabled = g_config->Get().openxrComfortVignette;
    comfortVignette.sizePixels = g_config->Get().openxrComfortVignetteSizePixels;
    comfortVignette.distanceMeters = g_config->Get().openxrComfortVignetteDistanceMeters;
    comfortVignette.widthMeters = g_config->Get().openxrComfortVignetteWidthMeters;
    comfortVignette.strength = g_config->Get().openxrComfortVignetteStrength;
    comfortVignette.innerRadius = g_config->Get().openxrComfortVignetteInnerRadius;
    comfortVignette.fadeMilliseconds =
        g_config->Get().openxrComfortVignetteFadeMilliseconds;
    comfortVignette.maxMotionAgeFrames = g_config->Get().hplControllerMaxInputAgeFrames;
    somavr::OpenXRFoveationSettings foveation;
    foveation.enabled = g_config->Get().openxrFoveation;
    foveation.level = g_config->Get().openxrFoveationLevel;
    foveation.dynamic = g_config->Get().openxrFoveationDynamic;
    foveation.verticalOffset = g_config->Get().openxrFoveationVerticalOffset;
    g_openxr->Configure(
        g_config->Get().openxrProbe,
        g_config->Get().openxrSessionProbe,
        g_config->Get().openxrReleaseAfterProbe,
        static_cast<uint64_t>(g_config->Get().openxrBootstrapFrame),
        static_cast<uint64_t>(g_config->Get().openxrHoldFrames),
        g_config->Get().openxrManualStart,
        g_config->Get().openxrFrameSubmit,
        g_config->Get().openxrMirrorBackbuffer,
        g_config->Get().openxrDesktopMirrorEye,
        g_config->Get().openxrDesktopMirrorAspect,
        g_config->Get().openxrDepthCompositionProbe,
        g_config->Get().openxrDepthCompositionSubmit,
        foveation,
        g_config->Get().openxrResolutionScalePercent,
        g_config->Get().openxrReferenceSpace,
        g_config->Get().openxrInputEnabled,
        g_config->Get().openxrInputLogInterval,
        g_config->Get().openxrRecoveryEnabled,
        g_config->Get().openxrRecoveryDelayFrames,
        g_config->Get().openxrTrackingHoldFrames,
        g_config->Get().openxrTrackingRecoveryBlackoutFrames,
        g_config->Get().openxrHudLayer,
        g_config->Get().openxrHudShape,
        g_config->Get().openxrHudCylinderAngleDegrees,
        g_config->Get().openxrHudWidthPixels,
        g_config->Get().openxrHudHeightPixels,
        g_config->Get().openxrHudDistanceMeters,
        g_config->Get().openxrHudWidthMeters,
        g_config->Get().openxrHudVerticalOffsetMeters,
        g_config->Get().openxrHudMaxAgeFrames,
        g_config->Get().openxrHudSuppressCenterCrosshair,
        g_config->Get().openxrHudCrosshairClearRadiusPixels,
        g_config->Get().openxrInteractionReticle,
        g_config->Get().openxrInteractionReticleSemantic,
        g_config->Get().openxrInteractionReticleNativeIcons,
        g_config->Get().openxrInteractionReticleSizePixels,
        g_config->Get().openxrInteractionReticleAngularSizeDegrees,
        g_config->Get().openxrInteractionReticleMinSizeMeters,
        g_config->Get().openxrInteractionReticleMaxSizeMeters,
        g_config->Get().openxrInteractionReticleMinDistanceMeters,
        g_config->Get().openxrInteractionReticleMaxDistanceMeters,
        g_config->Get().openxrInteractionReticleMaxAgeFrames,
        g_config->Get().openxrStatusPanel,
        g_config->Get().openxrStatusPanelWidthPixels,
        g_config->Get().openxrStatusPanelHeightPixels,
        g_config->Get().openxrStatusPanelDistanceMeters,
        g_config->Get().openxrStatusPanelWidthMeters,
        g_config->Get().openxrStatusPanelVerticalOffsetMeters,
        comfortVignette);

    if (!somavr::InstallOpenGLHooks(g_config->Get(), g_openxr.get())) {
        somavr::Logger::Instance().Write(somavr::LogLevel::Error, "opengl_hooks install_failed");
    }
    if (!somavr::InstallHPLCameraBridge(g_config->Get(), g_openxr.get())) {
        somavr::Logger::Instance().Write(somavr::LogLevel::Error, "hpl_camera_bridge install_failed");
    }
    if (!somavr::InstallHPLPlayerState(g_config->Get())) {
        somavr::Logger::Instance().Write(somavr::LogLevel::Error, "hpl_player_state install_failed");
    }
    if (!somavr::InstallHPLComfortBridge(g_config->Get())) {
        somavr::Logger::Instance().Write(somavr::LogLevel::Error, "hpl_comfort_bridge install_failed");
    }
    if (!somavr::InstallHPLPresentationBridge(g_config->Get(), g_openxr.get())) {
        somavr::Logger::Instance().Write(somavr::LogLevel::Error, "hpl_presentation_bridge install_failed");
    }
    if (!somavr::InstallHPLScreenEffectBridge(g_config->Get())) {
        somavr::Logger::Instance().Write(somavr::LogLevel::Error, "hpl_screen_effect_bridge install_failed");
    }
    if (!somavr::InstallHPLSubtitleBridge(g_config->Get())) {
        somavr::Logger::Instance().Write(somavr::LogLevel::Error, "hpl_subtitle_bridge install_failed");
    }
    if (!somavr::InstallHPLNativeLocomotion(g_config->Get())) {
        somavr::Logger::Instance().Write(somavr::LogLevel::Error, "hpl_native_locomotion install_failed");
    }
    if (!somavr::InstallHPLMenuBridge(g_config->Get())) {
        somavr::Logger::Instance().Write(somavr::LogLevel::Error, "hpl_menu_bridge install_failed");
    }
    if (!somavr::InstallHPLTerminalBridge(g_config->Get(), g_openxr.get())) {
        somavr::Logger::Instance().Write(somavr::LogLevel::Error, "hpl_terminal_bridge install_failed");
    }
    if (!somavr::InstallHPLStatusPanelBridge(g_config->Get(), g_openxr.get())) {
        somavr::Logger::Instance().Write(somavr::LogLevel::Error, "hpl_status_panel install_failed");
    }
    if (!somavr::InstallHPLInputBridge(g_config->Get(), g_openxr.get())) {
        somavr::Logger::Instance().Write(somavr::LogLevel::Error, "hpl_input_bridge install_failed");
    }
    if (!somavr::InstallHPLInteractionBridge(g_config->Get(), g_openxr.get())) {
        somavr::Logger::Instance().Write(somavr::LogLevel::Error, "hpl_interaction_bridge install_failed");
    }
    if (!somavr::InstallHPLGameplayHapticsBridge(g_config->Get(), g_openxr.get())) {
        somavr::Logger::Instance().Write(somavr::LogLevel::Error, "hpl_gameplay_haptics install_failed");
    }
    if (!somavr::InstallHPLContactHapticsBridge(g_config->Get(), g_openxr.get())) {
        somavr::Logger::Instance().Write(somavr::LogLevel::Error, "hpl_contact_haptics install_failed");
    }
    if (!somavr::InstallHPLCrosshairBridge(g_config->Get())) {
        somavr::Logger::Instance().Write(somavr::LogLevel::Error, "hpl_crosshair_bridge install_failed");
    }
    if (!somavr::InstallHPLUserModuleBridge(g_config->Get())) {
        somavr::Logger::Instance().Write(somavr::LogLevel::Error, "hpl_user_module_bridge install_failed");
    }
    if (!somavr::InstallHPLGrabBridge(g_config->Get(), g_openxr.get())) {
        somavr::Logger::Instance().Write(somavr::LogLevel::Error, "hpl_grab_bridge install_failed");
    }
    if (!somavr::InstallHPLHandsBridge(g_config->Get(), g_openxr.get())) {
        somavr::Logger::Instance().Write(somavr::LogLevel::Error, "hpl_hands_bridge install_failed");
    }
    if (!somavr::InstallHPLHudBridge(g_config->Get(), g_openxr.get())) {
        somavr::Logger::Instance().Write(somavr::LogLevel::Error, "hpl_hud_bridge install_failed");
    }
    if (!somavr::InstallHPLCompatibilityProbe(g_config->Get(), g_openxr.get())) {
        somavr::Logger::Instance().Write(somavr::LogLevel::Error, "hpl_compat_probe install_failed");
    }
    if (!somavr::InstallHPLLifecycle(g_config->Get(), g_openxr.get())) {
        somavr::Logger::Instance().Write(somavr::LogLevel::Error, "hpl_lifecycle install_failed");
    }

    bool orphanedWorker = false;
    const HANDLE stopEvent = g_stopEvent;
    for (;;) {
        const DWORD waitResult = stopEvent != nullptr
            ? WaitForSingleObject(stopEvent, 500)
            : WAIT_FAILED;
        if (waitResult != WAIT_TIMEOUT) {
            if (waitResult == WAIT_FAILED) {
                somavr::Logger::Instance().Write(
                    somavr::LogLevel::Warn,
                    "process_lifetime stop_wait_failed error=%lu action=exit_worker",
                    static_cast<unsigned long>(GetLastError()));
            }
            break;
        }
        if (IsOnlyCurrentThreadRemaining()) {
            orphanedWorker = true;
            somavr::Logger::Instance().Write(
                somavr::LogLevel::Warn,
                "process_lifetime orphaned_worker_detected action=return_without_runtime_teardown");
            break;
        }
        somavr::MaintainCrashHandler();
    }

    if (orphanedWorker) {
        return 0;
    }

    somavr::LogHPLLifecycleSummary();
    somavr::RemoveHPLLifecycle();
    somavr::LogHPLCompatibilityProbeSummary();
    somavr::RemoveHPLCompatibilityProbe();
    somavr::LogHPLHudBridgeSummary();
    somavr::RemoveHPLHudBridge();
    somavr::LogHPLHandsBridgeSummary();
    somavr::RemoveHPLHandsBridge();
    somavr::LogHPLGrabBridgeSummary();
    somavr::RemoveHPLGrabBridge();
    somavr::LogHPLCrosshairBridgeSummary();
    somavr::RemoveHPLCrosshairBridge();
    somavr::LogHPLUserModuleBridgeSummary();
    somavr::RemoveHPLUserModuleBridge();
    somavr::LogHPLInteractionBridgeSummary();
    somavr::RemoveHPLInteractionBridge();
    somavr::LogHPLContactHapticsBridgeSummary();
    somavr::RemoveHPLContactHapticsBridge();
    somavr::LogHPLGameplayHapticsBridgeSummary();
    somavr::RemoveHPLGameplayHapticsBridge();
    somavr::LogHPLPresentationBridgeSummary();
    somavr::RemoveHPLPresentationBridge();
    somavr::LogHPLScreenEffectBridgeSummary();
    somavr::RemoveHPLScreenEffectBridge();
    somavr::LogHPLSubtitleBridgeSummary();
    somavr::RemoveHPLSubtitleBridge();
    somavr::LogHPLComfortBridgeSummary();
    somavr::RemoveHPLComfortBridge();
    somavr::LogHPLCameraBridgeSummary();
    somavr::LogHPLInputBridgeSummary();
    somavr::RemoveHPLInputBridge();
    somavr::LogHPLStatusPanelBridgeSummary();
    somavr::RemoveHPLStatusPanelBridge();
    somavr::LogHPLTerminalBridgeSummary();
    somavr::RemoveHPLTerminalBridge();
    somavr::LogHPLMenuBridgeSummary();
    somavr::RemoveHPLMenuBridge();
    somavr::LogHPLNativeLocomotionSummary();
    somavr::RemoveHPLNativeLocomotion();
    somavr::LogHPLPlayerStateSummary();
    somavr::RemoveHPLPlayerState();
    somavr::RemoveHPLCameraBridge();
    somavr::LogOpenGLProofSummary();
    somavr::RemoveOpenGLHooks();
    if (g_openxr) {
        g_openxr->Shutdown();
    }
    somavr::LogCrashHandlerSummary();
    somavr::ShutdownCrashHandler();
    somavr::Logger::Instance().Write(somavr::LogLevel::Info, "somavr.dll worker exiting");
    somavr::Logger::Instance().Shutdown();
    // Only the side that wins this exchange may touch the handle. Winning here
    // means detach never ran, so nobody is waiting on it and closing is safe.
    HANDLE ownedStopEvent = static_cast<HANDLE>(InterlockedExchangePointer(
        reinterpret_cast<void* volatile*>(&g_stopEvent), nullptr));
    if (ownedStopEvent != nullptr) {
        CloseHandle(ownedStopEvent);
    }
    return 0;
}

} // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = module;
        DisableThreadLibraryCalls(module);

        g_stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (g_stopEvent != nullptr) {
            g_workerThread = CreateThread(nullptr, 0, WorkerThreadProc, nullptr, 0, nullptr);
            if (g_workerThread != nullptr) {
                CloseHandle(g_workerThread);
                g_workerThread = nullptr;
            }
        }
    } else if (reason == DLL_PROCESS_DETACH) {
        // Claim the handle the same way the worker's exit path does, so this
        // SetEvent cannot land on a handle the worker just closed - a closed
        // value can be recycled, and signalling an unrelated object is worse
        // than not signalling at all. Deliberately do not close it here: the
        // worker may still be blocked on its cached copy. Whichever side loses
        // the exchange leaves the handle alone, so one event is leaked when
        // detach wins, which is the path where the module is going away anyway.
        HANDLE stopEvent = static_cast<HANDLE>(InterlockedExchangePointer(
            reinterpret_cast<void* volatile*>(&g_stopEvent), nullptr));
        if (stopEvent != nullptr) {
            SetEvent(stopEvent);
        }
    }

    return TRUE;
}
