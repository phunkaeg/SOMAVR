#include "Config.h"
#include "HPLCameraBridge.h"
#include "HPLCompatibilityProbe.h"
#include "HPLLifecycle.h"
#include "HPLInputBridge.h"
#include "HPLPlayerState.h"
#include "Logger.h"
#include "OpenGLHooks.h"
#include "OpenXRRuntime.h"

#include <Windows.h>
#include <TlHelp32.h>

#include <memory>
#include <string>

#ifndef SOMAVR_BUILD_VERSION
#define SOMAVR_BUILD_VERSION "0.0.0-local"
#endif

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

DWORD WINAPI WorkerThreadProc(LPVOID)
{
    somavr::Logger::Instance().Initialize(somavr::LogPath(), somavr::LogLevel::Info);
    somavr::Logger::Instance().Write(
        somavr::LogLevel::Warn,
        "somavr.dll loaded version=%s buildOpenXR=%d built=%s %s path=%s",
        SOMAVR_BUILD_VERSION,
#if defined(SOMAVR_ENABLE_OPENXR)
        1,
#else
        0,
#endif
        __DATE__,
        __TIME__,
        somavr::ModulePath(g_module).c_str());

    g_config = std::make_unique<somavr::ConfigManager>();
    if (!g_config->Initialize()) {
        somavr::Logger::Instance().Write(somavr::LogLevel::Error, "config_init failed");
        return 1;
    }

    somavr::Logger::Instance().SetLevel(g_config->Get().logLevel);
    somavr::Logger::Instance().Write(
        somavr::LogLevel::Info,
        "config_loaded path=%s logLevel=%s",
        g_config->Path().string().c_str(),
        somavr::Logger::LevelName(g_config->Get().logLevel));
    somavr::Logger::Instance().Write(
        somavr::LogLevel::Info,
        "hook_config frameSummaryInterval=%d matrixSampleLimitPerFrame=%d uniformNameLogLimit=%d uniformMatrixLogLimit=%d uniformMatrixProjectionOnly=%d matrixCapture=%d matrixCaptureFrames=%d matrixCaptureStackDepth=%d matrixCaptureMaxSites=%d matrixCaptureSamplesPerUniform=%d renderDiagnosticCapture=%d renderDiagnosticFrames=%d renderDiagnosticMaxPrograms=%d renderDiagnosticMaxDraws=%d hplCameraBridge=%d hplLifecycleShutdown=%d hplProjectionCenterControl=%d hplProjectionCenteredDefault=%d hplRoomscaleControl=%d hplRoomscaleEnabledDefault=%d hplRoomscaleVertical=%d hplEyeHeightOffsetMeters=%.4f hplReflectionFadeControl=%d hplNativeCameraRollSuppression=%d hplCameraLogInterval=%d hplStereoAfr=%d hplWorldScale=%.4f hplRenderStageProbe=%d hplAudioListenerProbe=%d hplAudioListenerCorrection=%d hplPostEffectControl=%d hplPostEffectBypassDefault=%d hplPostEffectDisableImageTrail=%d hplPostEffectDisableChromaticAberration=%d hplPostEffectDisableRadialBlur=%d hplShadowJitterControl=%d hplShadowJitterSuppressedDefault=%d hplCompatibilityLogInterval=%d openxrProbe=%d openxrSessionProbe=%d openxrReleaseAfterProbe=%d openxrBootstrapFrame=%d openxrHoldFrames=%d openxrManualStart=%d openxrFrameSubmit=%d openxrMirrorBackbuffer=%d openxrResolutionScalePercent=%d openxrReferenceSpace=%s openxrInputEnabled=%d openxrInputLogInterval=%d openxrRecoveryEnabled=%d openxrRecoveryDelayFrames=%d",
        g_config->Get().frameSummaryInterval,
        g_config->Get().matrixSampleLimitPerFrame,
        g_config->Get().uniformNameLogLimit,
        g_config->Get().uniformMatrixLogLimit,
        g_config->Get().uniformMatrixProjectionOnly ? 1 : 0,
        g_config->Get().matrixCaptureEnabled ? 1 : 0,
        g_config->Get().matrixCaptureFrames,
        g_config->Get().matrixCaptureStackDepth,
        g_config->Get().matrixCaptureMaxSites,
        g_config->Get().matrixCaptureSamplesPerUniform,
        g_config->Get().renderDiagnosticCapture ? 1 : 0,
        g_config->Get().renderDiagnosticFrames,
        g_config->Get().renderDiagnosticMaxPrograms,
        g_config->Get().renderDiagnosticMaxDraws,
        g_config->Get().hplCameraBridge ? 1 : 0,
        g_config->Get().hplLifecycleShutdown ? 1 : 0,
        g_config->Get().hplProjectionCenterControl ? 1 : 0,
        g_config->Get().hplProjectionCenteredDefault ? 1 : 0,
        g_config->Get().hplRoomscaleControl ? 1 : 0,
        g_config->Get().hplRoomscaleEnabledDefault ? 1 : 0,
        g_config->Get().hplRoomscaleVertical ? 1 : 0,
        g_config->Get().hplEyeHeightOffsetMeters,
        g_config->Get().hplReflectionFadeControl ? 1 : 0,
        g_config->Get().hplNativeCameraRollSuppression ? 1 : 0,
        g_config->Get().hplCameraLogInterval,
        g_config->Get().hplStereoAfr ? 1 : 0,
        g_config->Get().hplWorldScale,
        g_config->Get().hplRenderStageProbe ? 1 : 0,
        g_config->Get().hplAudioListenerProbe ? 1 : 0,
        g_config->Get().hplAudioListenerCorrection ? 1 : 0,
        g_config->Get().hplPostEffectControl ? 1 : 0,
        g_config->Get().hplPostEffectBypassDefault ? 1 : 0,
        g_config->Get().hplPostEffectDisableImageTrail ? 1 : 0,
        g_config->Get().hplPostEffectDisableChromaticAberration ? 1 : 0,
        g_config->Get().hplPostEffectDisableRadialBlur ? 1 : 0,
        g_config->Get().hplShadowJitterControl ? 1 : 0,
        g_config->Get().hplShadowJitterSuppressedDefault ? 1 : 0,
        g_config->Get().hplCompatibilityLogInterval,
        g_config->Get().openxrProbe ? 1 : 0,
        g_config->Get().openxrSessionProbe ? 1 : 0,
        g_config->Get().openxrReleaseAfterProbe ? 1 : 0,
        g_config->Get().openxrBootstrapFrame,
        g_config->Get().openxrHoldFrames,
        g_config->Get().openxrManualStart ? 1 : 0,
        g_config->Get().openxrFrameSubmit ? 1 : 0,
        g_config->Get().openxrMirrorBackbuffer ? 1 : 0,
        g_config->Get().openxrResolutionScalePercent,
        g_config->Get().openxrReferenceSpace.c_str(),
        g_config->Get().openxrInputEnabled ? 1 : 0,
        g_config->Get().openxrInputLogInterval,
        g_config->Get().openxrRecoveryEnabled ? 1 : 0,
        g_config->Get().openxrRecoveryDelayFrames);
    somavr::Logger::Instance().Write(
        somavr::LogLevel::Info,
        "controller_config enabled=%d moveDeadzone=%.2f moveRelease=%.2f turnMode=%s turnDeadzone=%.2f turnRelease=%.2f snapPixels=%d smoothPixelsPerSecond=%.1f interaction=%d menu=%d recenterChord=%d haptics=%d hapticAmplitude=%.2f hapticDurationMs=%d suppressAuthoredCamera=%d comfortBlackoutFrames=%d recenterHoldMs=%d maxInputAgeFrames=%d logInterval=%d",
        g_config->Get().hplControllerInput ? 1 : 0,
        g_config->Get().hplControllerMoveDeadzone,
        g_config->Get().hplControllerMoveReleaseDeadzone,
        g_config->Get().hplControllerSnapTurn ? "snap" : "smooth",
        g_config->Get().hplControllerTurnDeadzone,
        g_config->Get().hplControllerTurnReleaseDeadzone,
        g_config->Get().hplControllerSnapTurnPixels,
        g_config->Get().hplControllerSmoothTurnPixelsPerSecond,
        g_config->Get().hplControllerInteraction ? 1 : 0,
        g_config->Get().hplControllerMenu ? 1 : 0,
        g_config->Get().hplControllerRecenterChord ? 1 : 0,
        g_config->Get().hplControllerHaptics ? 1 : 0,
        g_config->Get().hplControllerHapticAmplitude,
        g_config->Get().hplControllerHapticDurationMs,
        g_config->Get().hplControllerSuppressDuringAuthoredCamera ? 1 : 0,
        g_config->Get().hplControllerComfortBlackoutFrames,
        g_config->Get().hplControllerRecenterHoldMs,
        g_config->Get().hplControllerMaxInputAgeFrames,
        g_config->Get().hplControllerLogInterval);

    g_openxr = std::make_unique<somavr::OpenXRRuntime>();
    g_openxr->Configure(
        g_config->Get().openxrProbe,
        g_config->Get().openxrSessionProbe,
        g_config->Get().openxrReleaseAfterProbe,
        static_cast<uint64_t>(g_config->Get().openxrBootstrapFrame),
        static_cast<uint64_t>(g_config->Get().openxrHoldFrames),
        g_config->Get().openxrManualStart,
        g_config->Get().openxrFrameSubmit,
        g_config->Get().openxrMirrorBackbuffer,
        g_config->Get().openxrResolutionScalePercent,
        g_config->Get().openxrReferenceSpace,
        g_config->Get().openxrInputEnabled,
        g_config->Get().openxrInputLogInterval,
        g_config->Get().openxrRecoveryEnabled,
        g_config->Get().openxrRecoveryDelayFrames);

    if (!somavr::InstallOpenGLHooks(g_config->Get(), g_openxr.get())) {
        somavr::Logger::Instance().Write(somavr::LogLevel::Error, "opengl_hooks install_failed");
    }
    if (!somavr::InstallHPLCameraBridge(g_config->Get(), g_openxr.get())) {
        somavr::Logger::Instance().Write(somavr::LogLevel::Error, "hpl_camera_bridge install_failed");
    }
    if (!somavr::InstallHPLPlayerState(g_config->Get())) {
        somavr::Logger::Instance().Write(somavr::LogLevel::Error, "hpl_player_state install_failed");
    }
    if (!somavr::InstallHPLInputBridge(g_config->Get(), g_openxr.get())) {
        somavr::Logger::Instance().Write(somavr::LogLevel::Error, "hpl_input_bridge install_failed");
    }
    if (!somavr::InstallHPLCompatibilityProbe(g_config->Get(), g_openxr.get())) {
        somavr::Logger::Instance().Write(somavr::LogLevel::Error, "hpl_compat_probe install_failed");
    }
    if (!somavr::InstallHPLLifecycle(g_config->Get(), g_openxr.get())) {
        somavr::Logger::Instance().Write(somavr::LogLevel::Error, "hpl_lifecycle install_failed");
    }

    bool orphanedWorker = false;
    for (;;) {
        if (WaitForSingleObject(g_stopEvent, 500) == WAIT_OBJECT_0) {
            break;
        }
        if (IsOnlyCurrentThreadRemaining()) {
            orphanedWorker = true;
            somavr::Logger::Instance().Write(
                somavr::LogLevel::Warn,
                "process_lifetime orphaned_worker_detected action=return_without_runtime_teardown");
            break;
        }
    }

    if (orphanedWorker) {
        return 0;
    }

    somavr::LogHPLLifecycleSummary();
    somavr::RemoveHPLLifecycle();
    somavr::LogHPLCompatibilityProbeSummary();
    somavr::RemoveHPLCompatibilityProbe();
    somavr::LogHPLCameraBridgeSummary();
    somavr::LogHPLInputBridgeSummary();
    somavr::RemoveHPLInputBridge();
    somavr::LogHPLPlayerStateSummary();
    somavr::RemoveHPLPlayerState();
    somavr::RemoveHPLCameraBridge();
    somavr::LogOpenGLProofSummary();
    somavr::RemoveOpenGLHooks();
    if (g_openxr) {
        g_openxr->Shutdown();
    }
    somavr::Logger::Instance().Write(somavr::LogLevel::Info, "somavr.dll worker exiting");
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
        if (g_stopEvent != nullptr) {
            SetEvent(g_stopEvent);
            CloseHandle(g_stopEvent);
            g_stopEvent = nullptr;
        }
    }

    return TRUE;
}
