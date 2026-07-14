#include "HPLGrabBridge.h"

#include "HPLCameraBridge.h"
#include "HPLPlayerState.h"
#include "Logger.h"

#include <Windows.h>

#include <MinHook.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>

namespace somavr {
namespace {

constexpr uintptr_t kPidVectorOutputRva = 0x238750;
constexpr uint8_t kPidVectorOutputSignature[] = {
    0x48, 0x89, 0x5c, 0x24, 0x08,
    0x48, 0x89, 0x74, 0x24, 0x10,
    0x57,
    0x48, 0x83, 0xec, 0x30,
    0x48, 0x63, 0x81, 0x84, 0x00, 0x00, 0x00,
};
constexpr int kGrabPlayerState = 1;
constexpr size_t kPidP = 0x18;
constexpr size_t kPidI = 0x1c;
constexpr size_t kPidD = 0x20;

using PidVectorOutputFn = float* (*)(void*, float*, const float*, float);

struct GrabAnchor {
    bool valid = false;
    void* pid = nullptr;
    void* player = nullptr;
    void* camera = nullptr;
    float relativeX = 0.0f;
    float relativeY = 0.0f;
    float relativeZ = 0.0f;
};

Config g_config;
OpenXRRuntime* g_openxr = nullptr;
PidVectorOutputFn g_originalPidOutput = nullptr;
void* g_pidOutputTarget = nullptr;
GrabAnchor g_anchor;
std::mutex g_installMutex;
std::mutex g_stateMutex;
std::atomic<uint64_t> g_calls = 0;
std::atomic<uint64_t> g_forcePidMatches = 0;
std::atomic<uint64_t> g_torquePidMatches = 0;
std::atomic<uint64_t> g_torqueProbeSamples = 0;
std::atomic<uint64_t> g_anchors = 0;
std::atomic<uint64_t> g_substitutions = 0;
std::atomic<uint64_t> g_fallbackState = 0;
std::atomic<uint64_t> g_fallbackPid = 0;
std::atomic<uint64_t> g_fallbackPose = 0;
std::atomic<uint64_t> g_fallbackStale = 0;
std::atomic<uint64_t> g_fallbackCamera = 0;

bool IsInsideImage(HMODULE module, uintptr_t rva, size_t bytes)
{
    if (module == nullptr) return false;
    const auto* base = reinterpret_cast<const std::byte*>(module);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    return nt->Signature == IMAGE_NT_SIGNATURE
        && rva <= nt->OptionalHeader.SizeOfImage
        && bytes <= nt->OptionalHeader.SizeOfImage - rva;
}

float ReadFloat(const void* object, size_t offset)
{
    float value = 0.0f;
    std::memcpy(&value, static_cast<const std::byte*>(object) + offset, sizeof(value));
    return value;
}

bool IsGrabForcePid(const void* pid)
{
    if (pid == nullptr) return false;
    const float p = ReadFloat(pid, kPidP);
    const float i = ReadFloat(pid, kPidI);
    const float d = ReadFloat(pid, kPidD);
    return std::isfinite(p) && std::isfinite(i) && std::isfinite(d)
        && std::fabs(p - 400.0f) <= 0.05f
        && std::fabs(i) <= 0.001f
        && std::fabs(d - 40.0f) <= 0.05f;
}

bool IsGrabTorquePid(const void* pid)
{
    if (pid == nullptr) return false;
    const float p = ReadFloat(pid, kPidP);
    const float i = ReadFloat(pid, kPidI);
    const float d = ReadFloat(pid, kPidD);
    return std::isfinite(p) && std::isfinite(i) && std::isfinite(d)
        && std::fabs(p - 40.0f) <= 0.05f
        && std::fabs(i) <= 0.001f
        && (std::fabs(d - 0.4f) <= 0.01f || std::fabs(d - 0.1f) <= 0.01f);
}

const OpenXRHandInput* SelectDominantHand(const OpenXRInputSnapshot& input)
{
    const bool left = g_config.hplControllerDominantHand == "left";
    const OpenXRHandInput* preferred = left ? &input.left : &input.right;
    if (preferred->active) return preferred;
    if (!g_config.hplControllerOneHandFallback) return nullptr;
    const OpenXRHandInput* fallback = left ? &input.right : &input.left;
    return fallback->active ? fallback : nullptr;
}

void ResetAnchor()
{
    std::lock_guard lock(g_stateMutex);
    g_anchor = {};
}

bool ResolveGrabRelativePosition(
    const HPLPlayerStateSnapshot& player,
    float& x,
    float& y,
    float& z)
{
    const HPLCameraBridgeStatus camera = GetHPLCameraBridgeStatus();
    if (!camera.trackingEnabled
        || !camera.cameraWorldPositionValid
        || camera.activeCamera != player.camera) {
        g_fallbackCamera.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    OpenXRInputSnapshot input;
    if (g_openxr == nullptr || !g_openxr->GetLatestInput(input) || !input.active) {
        g_fallbackPose.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    if (camera.headPoseFrame >= input.gameFrame
        && camera.headPoseFrame - input.gameFrame
            > static_cast<uint64_t>(g_config.hplControllerMaxInputAgeFrames)) {
        g_fallbackStale.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    const OpenXRHandInput* hand = SelectDominantHand(input);
    HPLTrackedPoseWorld grip;
    if (hand == nullptr
        || !hand->gripPose.valid
        || !hand->gripPose.orientationTracked
        || !hand->gripPose.positionTracked
        || !ResolveHPLTrackedPoseWorld(hand->gripPose, input.gameFrame, grip)
        || !grip.positionTracked) {
        g_fallbackPose.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    x = grip.positionX - camera.cameraWorldPositionX;
    y = grip.positionY - camera.cameraWorldPositionY;
    z = grip.positionZ - camera.cameraWorldPositionZ;
    return std::isfinite(x) && std::isfinite(y) && std::isfinite(z);
}

float* HookPidVectorOutput(void* pid, float* output, const float* error, float timeStep)
{
    const uint64_t call = g_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    if (!g_config.hplControllerGrabTranslation || error == nullptr
        || !std::isfinite(error[0]) || !std::isfinite(error[1]) || !std::isfinite(error[2])) {
        return g_originalPidOutput(pid, output, error, timeStep);
    }
    if (!IsGrabForcePid(pid)) {
        if (IsGrabTorquePid(pid)) {
            HPLPlayerStateSnapshot torquePlayer;
            if (GetHPLPlayerStateSnapshot(torquePlayer)
                && torquePlayer.playerValid
                && torquePlayer.playerStateId == kGrabPlayerState) {
                g_torquePidMatches.fetch_add(1, std::memory_order_relaxed);
                OpenXRInputSnapshot torqueInput;
                const OpenXRHandInput* torqueHand = nullptr;
                if (g_openxr != nullptr
                    && g_openxr->GetLatestInput(torqueInput)
                    && (torqueHand = SelectDominantHand(torqueInput)) != nullptr) {
                    const uint64_t sample = g_torqueProbeSamples.fetch_add(1, std::memory_order_relaxed) + 1;
                    if (sample <= 12
                        || sample % static_cast<uint64_t>(std::max(g_config.hplControllerLogInterval, 1)) == 0) {
                        Logger::Instance().Write(
                            LogLevel::Info,
                            "hpl_grab_torque_probe call=%llu sample=%llu pid=%p nativeRotError=%.4f,%.4f,%.4f gripAngularValid=%d gripAngular=%.4f,%.4f,%.4f gripLinearValid=%d gripLinear=%.4f,%.4f,%.4f",
                            static_cast<unsigned long long>(call),
                            static_cast<unsigned long long>(sample),
                            pid,
                            error[0], error[1], error[2],
                            torqueHand->gripPose.angularVelocityValid ? 1 : 0,
                            torqueHand->gripPose.angularVelocityX,
                            torqueHand->gripPose.angularVelocityY,
                            torqueHand->gripPose.angularVelocityZ,
                            torqueHand->gripPose.linearVelocityValid ? 1 : 0,
                            torqueHand->gripPose.linearVelocityX,
                            torqueHand->gripPose.linearVelocityY,
                            torqueHand->gripPose.linearVelocityZ);
                    }
                }
            }
        }
        g_fallbackPid.fetch_add(1, std::memory_order_relaxed);
        return g_originalPidOutput(pid, output, error, timeStep);
    }
    g_forcePidMatches.fetch_add(1, std::memory_order_relaxed);

    HPLPlayerStateSnapshot player;
    if (!GetHPLPlayerStateSnapshot(player)
        || !player.playerValid
        || player.playerStateId != kGrabPlayerState
        || player.authoredCameraActive) {
        g_fallbackState.fetch_add(1, std::memory_order_relaxed);
        ResetAnchor();
        return g_originalPidOutput(pid, output, error, timeStep);
    }

    float relativeX = 0.0f;
    float relativeY = 0.0f;
    float relativeZ = 0.0f;
    if (!ResolveGrabRelativePosition(player, relativeX, relativeY, relativeZ)) {
        ResetAnchor();
        return g_originalPidOutput(pid, output, error, timeStep);
    }

    float deltaX = 0.0f;
    float deltaY = 0.0f;
    float deltaZ = 0.0f;
    bool anchored = false;
    {
        std::lock_guard lock(g_stateMutex);
        if (!g_anchor.valid
            || g_anchor.pid != pid
            || g_anchor.player != player.player
            || g_anchor.camera != player.camera) {
            g_anchor.valid = true;
            g_anchor.pid = pid;
            g_anchor.player = player.player;
            g_anchor.camera = player.camera;
            g_anchor.relativeX = relativeX;
            g_anchor.relativeY = relativeY;
            g_anchor.relativeZ = relativeZ;
            anchored = true;
        } else {
            deltaX = relativeX - g_anchor.relativeX;
            deltaY = relativeY - g_anchor.relativeY;
            deltaZ = relativeZ - g_anchor.relativeZ;
        }
    }
    if (anchored) {
        const uint64_t anchors = g_anchors.fetch_add(1, std::memory_order_relaxed) + 1;
        if (anchors <= 8) {
            Logger::Instance().Write(
                LogLevel::Info,
                "hpl_grab_anchor call=%llu pid=%p player=%p camera=%p relative=%.4f,%.4f,%.4f policy=native_pid_translation_delta",
                static_cast<unsigned long long>(call), pid, player.player, player.camera,
                relativeX, relativeY, relativeZ);
        }
        return g_originalPidOutput(pid, output, error, timeStep);
    }

    const float maxOffset = g_config.hplControllerGrabMaxOffsetMeters
        * std::max(g_config.hplWorldScale, 0.001f);
    deltaX *= g_config.hplControllerGrabTranslationScale;
    deltaY *= g_config.hplControllerGrabTranslationScale;
    deltaZ *= g_config.hplControllerGrabTranslationScale;
    const float length = std::sqrt(deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ);
    if (std::isfinite(length) && length > maxOffset && length > 0.000001f) {
        const float scale = maxOffset / length;
        deltaX *= scale;
        deltaY *= scale;
        deltaZ *= scale;
    }
    const float modifiedError[3] = {
        error[0] + deltaX,
        error[1] + deltaY,
        error[2] + deltaZ,
    };
    const uint64_t substitution = g_substitutions.fetch_add(1, std::memory_order_relaxed) + 1;
    if (substitution <= 8
        || substitution % static_cast<uint64_t>(std::max(g_config.hplControllerLogInterval, 1)) == 0) {
        Logger::Instance().Write(
            LogLevel::Info,
            "hpl_grab_target call=%llu applied=1 pid=%p nativeError=%.4f,%.4f,%.4f controllerDelta=%.4f,%.4f,%.4f modifiedError=%.4f,%.4f,%.4f maxOffset=%.3f",
            static_cast<unsigned long long>(call), pid,
            error[0], error[1], error[2],
            deltaX, deltaY, deltaZ,
            modifiedError[0], modifiedError[1], modifiedError[2],
            maxOffset);
    }
    return g_originalPidOutput(pid, output, modifiedError, timeStep);
}

} // namespace

bool InstallHPLGrabBridge(const Config& config, OpenXRRuntime* openxr)
{
    std::lock_guard lock(g_installMutex);
    g_config = config;
    g_openxr = openxr;
    if (!config.hplControllerGrabTranslation) {
        Logger::Instance().Write(LogLevel::Info, "hpl_grab_bridge disabled config=0");
        return true;
    }

    HMODULE executable = GetModuleHandleW(nullptr);
    if (!IsInsideImage(executable, kPidVectorOutputRva, sizeof(kPidVectorOutputSignature))) {
        Logger::Instance().Write(LogLevel::Error, "hpl_grab_bridge install_failed reason=invalid_image_range");
        return false;
    }
    auto* target = reinterpret_cast<std::byte*>(executable) + kPidVectorOutputRva;
    if (std::memcmp(target, kPidVectorOutputSignature, sizeof(kPidVectorOutputSignature)) != 0) {
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_grab_bridge install_failed reason=signature_mismatch rva=0x%llx",
            static_cast<unsigned long long>(kPidVectorOutputRva));
        return false;
    }

    MH_STATUS status = MH_CreateHook(
        target,
        reinterpret_cast<void*>(&HookPidVectorOutput),
        reinterpret_cast<void**>(&g_originalPidOutput));
    if (status != MH_OK && status != MH_ERROR_ALREADY_CREATED) {
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_grab_bridge install_failed reason=create_hook status=%s",
            MH_StatusToString(status));
        return false;
    }
    status = MH_EnableHook(target);
    if (status != MH_OK && status != MH_ERROR_ENABLED) {
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_grab_bridge install_failed reason=enable_hook status=%s",
            MH_StatusToString(status));
        return false;
    }
    g_pidOutputTarget = target;
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_grab_bridge install_ok pidOutputRva=0x%llx target=%p playerState=1 pidGains=400,0,40 translationScale=%.3f maxOffsetMeters=%.3f policy=modify_position_error_preserve_native_pid",
        static_cast<unsigned long long>(kPidVectorOutputRva),
        target,
        config.hplControllerGrabTranslationScale,
        config.hplControllerGrabMaxOffsetMeters);
    return true;
}

void RemoveHPLGrabBridge()
{
    std::lock_guard lock(g_installMutex);
    if (g_pidOutputTarget != nullptr) {
        MH_DisableHook(g_pidOutputTarget);
        MH_RemoveHook(g_pidOutputTarget);
    }
    g_pidOutputTarget = nullptr;
    g_originalPidOutput = nullptr;
    g_openxr = nullptr;
    ResetAnchor();
    Logger::Instance().Write(LogLevel::Info, "hpl_grab_bridge removed");
}

void LogHPLGrabBridgeSummary()
{
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_grab_bridge_summary installed=%d calls=%llu forcePidMatches=%llu torquePidMatches=%llu torqueProbeSamples=%llu anchors=%llu substitutions=%llu fallbackState=%llu fallbackPid=%llu fallbackPose=%llu fallbackStale=%llu fallbackCamera=%llu",
        g_pidOutputTarget != nullptr ? 1 : 0,
        static_cast<unsigned long long>(g_calls.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_forcePidMatches.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_torquePidMatches.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_torqueProbeSamples.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_anchors.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_substitutions.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_fallbackState.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_fallbackPid.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_fallbackPose.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_fallbackStale.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_fallbackCamera.load(std::memory_order_relaxed)));
}

} // namespace somavr
