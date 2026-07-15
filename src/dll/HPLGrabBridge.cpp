#include "HPLGrabBridge.h"

#include "HPLCameraBridge.h"
#include "HPLGrabMath.h"
#include "HPLInteractionBridge.h"
#include "HPLPlayerState.h"
#include "HPLTwoHandMath.h"
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
constexpr uintptr_t kAddImpulseThunkRva = 0x49c720;
constexpr uint8_t kPidVectorOutputSignature[] = {
    0x48, 0x89, 0x5c, 0x24, 0x08,
    0x48, 0x89, 0x74, 0x24, 0x10,
    0x57,
    0x48, 0x83, 0xec, 0x30,
    0x48, 0x63, 0x81, 0x84, 0x00, 0x00, 0x00,
};
constexpr uint8_t kAddImpulseThunkSignature[] = {
    0x48, 0x8b, 0x01,
    0xff, 0xa0, 0x30, 0x01, 0x00, 0x00,
    0xcc, 0xcc, 0xcc,
};
constexpr int kGrabPlayerState = 1;
constexpr int kSlidePlayerState = 4;
constexpr int kSwingDoorPlayerState = 5;
constexpr int kLeverPlayerState = 6;
constexpr size_t kPidP = 0x18;
constexpr size_t kPidI = 0x1c;
constexpr size_t kPidD = 0x20;

using PidVectorOutputFn = float* (*)(void*, float*, const float*, float);
using PhysicsBodyImpulseFn = void (*)(void*, const float*);

struct GrabAnchor {
    bool valid = false;
    void* pid = nullptr;
    void* player = nullptr;
    void* camera = nullptr;
    float relativeX = 0.0f;
    float relativeY = 0.0f;
    float relativeZ = 0.0f;
    camera_math::Quaternion gripOrientation{};
    bool gripOrientationTracked = false;
    bool twoHandActive = false;
    camera_math::Vector3 twoHandAnchorDirection{};
    uint64_t lastInputFrame = 0;
};

struct PendingThrow {
    bool armed = false;
    uint64_t deadlineMs = 0;
    uint64_t gameFrame = 0;
    OpenXRControllerPose gripPose{};
};

struct SlideAnchor {
    bool valid = false;
    void* body = nullptr;
    void* joint = nullptr;
    uint64_t hitSequence = 0;
    uint64_t lastInputFrame = 0;
    camera_math::Vector3 pin{};
    camera_math::Vector3 lastGripPosition{};
};

struct RotateAnchor {
    bool valid = false;
    void* body = nullptr;
    void* joint = nullptr;
    uint64_t hitSequence = 0;
    uint64_t lastInputFrame = 0;
    camera_math::Vector3 pin{};
    camera_math::Vector3 pivot{};
    camera_math::Vector3 hitPoint{};
    camera_math::Vector3 initialGripPosition{};
    camera_math::Vector3 lastGripPosition{};
};

Config g_config;
OpenXRRuntime* g_openxr = nullptr;
PidVectorOutputFn g_originalPidOutput = nullptr;
void* g_pidOutputTarget = nullptr;
void* g_addImpulseTarget = nullptr;
uint8_t g_addImpulseOriginal[sizeof(kAddImpulseThunkSignature)]{};
GrabAnchor g_anchor;
PendingThrow g_pendingThrow;
SlideAnchor g_slideAnchor;
RotateAnchor g_rotateAnchor;
std::mutex g_installMutex;
std::mutex g_stateMutex;
std::atomic<uint64_t> g_calls = 0;
std::atomic<uint64_t> g_forcePidMatches = 0;
std::atomic<uint64_t> g_torquePidMatches = 0;
std::atomic<uint64_t> g_anchors = 0;
std::atomic<uint64_t> g_substitutions = 0;
std::atomic<uint64_t> g_fallbackState = 0;
std::atomic<uint64_t> g_fallbackPid = 0;
std::atomic<uint64_t> g_fallbackPose = 0;
std::atomic<uint64_t> g_fallbackStale = 0;
std::atomic<uint64_t> g_fallbackCamera = 0;
std::atomic<uint64_t> g_rotationSubstitutions = 0;
std::atomic<uint64_t> g_twoHandGrabCandidates = 0;
std::atomic<uint64_t> g_twoHandGrabEngagements = 0;
std::atomic<uint64_t> g_twoHandGrabSubstitutions = 0;
std::atomic<uint64_t> g_twoHandGrabReleases = 0;
std::atomic<uint64_t> g_twoHandGrabFallbacks = 0;
std::atomic<uint64_t> g_throwArms = 0;
std::atomic<uint64_t> g_throwRedirects = 0;
std::atomic<uint64_t> g_throwFallbacks = 0;
std::atomic<uint64_t> g_slidePidMatches = 0;
std::atomic<uint64_t> g_slideAnchors = 0;
std::atomic<uint64_t> g_slideSubstitutions = 0;
std::atomic<uint64_t> g_slideFallbackHit = 0;
std::atomic<uint64_t> g_slideFallbackJoint = 0;
std::atomic<uint64_t> g_slideFallbackPose = 0;
std::atomic<uint64_t> g_rotatePidMatches = 0;
std::atomic<uint64_t> g_rotateAnchors = 0;
std::atomic<uint64_t> g_rotateSubstitutions = 0;
std::atomic<uint64_t> g_rotateFallbackHit = 0;
std::atomic<uint64_t> g_rotateFallbackJoint = 0;
std::atomic<uint64_t> g_rotateFallbackPose = 0;

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

const OpenXRHandInput* SelectDominantHand(
    const OpenXRInputSnapshot& input,
    uint32_t* selectedHandIndex);

bool IsSlideForcePid(const void* pid)
{
    if (pid == nullptr) return false;
    const float p = ReadFloat(pid, kPidP);
    const float i = ReadFloat(pid, kPidI);
    const float d = ReadFloat(pid, kPidD);
    return std::isfinite(p) && std::isfinite(i) && std::isfinite(d)
        && std::fabs(p - 6.0f) <= 0.01f
        && std::fabs(i) <= 0.001f
        && std::fabs(d - 0.1f) <= 0.01f;
}

bool IsRotateTorquePid(const void* pid)
{
    if (pid == nullptr) return false;
    const float p = ReadFloat(pid, kPidP);
    const float i = ReadFloat(pid, kPidI);
    const float d = ReadFloat(pid, kPidD);
    return std::isfinite(p) && std::isfinite(i) && std::isfinite(d)
        && std::fabs(p - 10.0f) <= 0.01f
        && std::fabs(i) <= 0.001f
        && std::fabs(d - 1.0f) <= 0.01f;
}

bool ReadMemory(const void* source, void* destination, size_t bytes)
{
    if (source == nullptr || destination == nullptr || bytes == 0) return false;
    SIZE_T bytesRead = 0;
    return ReadProcessMemory(
        GetCurrentProcess(), source, destination, bytes, &bytesRead) != FALSE
        && bytesRead == bytes;
}

bool ResolveSlideJointPin(uint64_t inputFrame, SlideAnchor& anchor)
{
    HPLInteractionHitSnapshot hit;
    GetHPLInteractionHitSnapshot(hit);
    if (hit.body == nullptr || hit.sequence == 0 || inputFrame < hit.gameFrame
        || inputFrame - hit.gameFrame > 120) {
        g_slideFallbackHit.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    void** jointsBegin = nullptr;
    void** jointsEnd = nullptr;
    if (!ReadMemory(static_cast<const std::byte*>(hit.body) + 0x168, &jointsBegin, sizeof(jointsBegin))
        || !ReadMemory(static_cast<const std::byte*>(hit.body) + 0x170, &jointsEnd, sizeof(jointsEnd))
        || jointsBegin == nullptr || jointsEnd <= jointsBegin || jointsEnd - jointsBegin > 32) {
        g_slideFallbackJoint.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    void* joint = nullptr;
    camera_math::Vector3 pin;
    if (!ReadMemory(jointsBegin, &joint, sizeof(joint)) || joint == nullptr
        || !ReadMemory(static_cast<const std::byte*>(joint) + 0xe8, &pin, sizeof(pin))) {
        g_slideFallbackJoint.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    const float length = std::sqrt(pin.x * pin.x + pin.y * pin.y + pin.z * pin.z);
    if (!std::isfinite(length) || length < 0.5f || length > 1.5f) {
        g_slideFallbackJoint.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    const float inverseLength = 1.0f / length;
    anchor.valid = true;
    anchor.body = hit.body;
    anchor.joint = joint;
    anchor.hitSequence = hit.sequence;
    anchor.pin = {pin.x * inverseLength, pin.y * inverseLength, pin.z * inverseLength};
    const uint64_t anchored = g_slideAnchors.fetch_add(1, std::memory_order_relaxed) + 1;
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_slide_anchor count=%llu frame=%llu hitFrame=%llu hitSequence=%llu body=%p joint=%p pin=%.5f,%.5f,%.5f policy=interaction_body_joint0",
        static_cast<unsigned long long>(anchored),
        static_cast<unsigned long long>(inputFrame),
        static_cast<unsigned long long>(hit.gameFrame),
        static_cast<unsigned long long>(hit.sequence),
        hit.body,
        joint,
        anchor.pin.x, anchor.pin.y, anchor.pin.z);
    return true;
}

bool ResolveRotateJoint(
    uint64_t inputFrame,
    const camera_math::Vector3& gripPosition,
    RotateAnchor& anchor)
{
    HPLInteractionHitSnapshot hit;
    GetHPLInteractionHitSnapshot(hit);
    if (hit.body == nullptr || hit.sequence == 0 || inputFrame < hit.gameFrame
        || inputFrame - hit.gameFrame > 120) {
        g_rotateFallbackHit.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    void** jointsBegin = nullptr;
    void** jointsEnd = nullptr;
    if (!ReadMemory(static_cast<const std::byte*>(hit.body) + 0x168, &jointsBegin, sizeof(jointsBegin))
        || !ReadMemory(static_cast<const std::byte*>(hit.body) + 0x170, &jointsEnd, sizeof(jointsEnd))
        || jointsBegin == nullptr || jointsEnd <= jointsBegin || jointsEnd - jointsBegin > 32) {
        g_rotateFallbackJoint.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    void* joint = nullptr;
    camera_math::Vector3 pin{};
    camera_math::Vector3 pivot{};
    if (!ReadMemory(jointsBegin, &joint, sizeof(joint)) || joint == nullptr
        || !ReadMemory(static_cast<const std::byte*>(joint) + 0xe8, &pin, sizeof(pin))
        || !ReadMemory(static_cast<const std::byte*>(joint) + 0xf4, &pivot, sizeof(pivot))) {
        g_rotateFallbackJoint.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    const float pinLength = std::sqrt(pin.x * pin.x + pin.y * pin.y + pin.z * pin.z);
    const camera_math::Vector3 hitPoint{hit.worldX, hit.worldY, hit.worldZ};
    const float radiusX = hitPoint.x - pivot.x;
    const float radiusY = hitPoint.y - pivot.y;
    const float radiusZ = hitPoint.z - pivot.z;
    const float hitRadius = std::sqrt(
        radiusX * radiusX + radiusY * radiusY + radiusZ * radiusZ);
    if (!std::isfinite(pinLength) || pinLength < 0.5f || pinLength > 1.5f
        || !std::isfinite(pivot.x) || !std::isfinite(pivot.y) || !std::isfinite(pivot.z)
        || !std::isfinite(hitRadius) || hitRadius < 0.05f || hitRadius > 20.0f) {
        g_rotateFallbackJoint.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    const float inversePinLength = 1.0f / pinLength;
    anchor.valid = true;
    anchor.body = hit.body;
    anchor.joint = joint;
    anchor.hitSequence = hit.sequence;
    anchor.pin = {
        pin.x * inversePinLength,
        pin.y * inversePinLength,
        pin.z * inversePinLength,
    };
    anchor.pivot = pivot;
    anchor.hitPoint = hitPoint;
    anchor.initialGripPosition = gripPosition;
    anchor.lastGripPosition = gripPosition;
    anchor.lastInputFrame = inputFrame;
    const uint64_t anchored = g_rotateAnchors.fetch_add(1, std::memory_order_relaxed) + 1;
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_rotate_anchor count=%llu frame=%llu hitFrame=%llu hitSequence=%llu body=%p joint=%p pin=%.5f,%.5f,%.5f pivot=%.4f,%.4f,%.4f hit=%.4f,%.4f,%.4f radius=%.4f policy=controller_delta_applied_to_native_hit_point_about_joint_pivot",
        static_cast<unsigned long long>(anchored),
        static_cast<unsigned long long>(inputFrame),
        static_cast<unsigned long long>(hit.gameFrame),
        static_cast<unsigned long long>(hit.sequence),
        hit.body,
        joint,
        anchor.pin.x, anchor.pin.y, anchor.pin.z,
        pivot.x, pivot.y, pivot.z,
        hitPoint.x, hitPoint.y, hitPoint.z,
        hitRadius);
    return true;
}

bool ResolveRotateControllerMotion(
    float timeStep,
    camera_math::Vector3& position,
    camera_math::Vector3& velocity,
    bool& velocityValid,
    uint64_t& inputFrame)
{
    OpenXRInputSnapshot input;
    uint32_t handIndex = 1;
    const OpenXRHandInput* hand = nullptr;
    if (g_openxr == nullptr || !g_openxr->GetLatestInput(input) || !input.active
        || (hand = SelectDominantHand(input, &handIndex)) == nullptr
        || !hand->gripPose.valid || !hand->gripPose.positionTracked) {
        g_rotateFallbackPose.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    HPLTrackedPoseWorld grip;
    if (!ResolveHPLTrackedPoseWorld(hand->gripPose, input.gameFrame, grip)
        || !grip.positionTracked) {
        g_rotateFallbackPose.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    inputFrame = input.gameFrame;
    position = {grip.positionX, grip.positionY, grip.positionZ};
    velocityValid = hand->gripPose.linearVelocityValid
        && ResolveHPLReferenceVectorWorld(
            hand->gripPose.linearVelocityX,
            hand->gripPose.linearVelocityY,
            hand->gripPose.linearVelocityZ,
            true,
            velocity.x,
            velocity.y,
            velocity.z);
    if (!velocityValid && g_rotateAnchor.valid
        && g_rotateAnchor.lastInputFrame != 0
        && input.gameFrame != g_rotateAnchor.lastInputFrame
        && std::isfinite(timeStep) && timeStep > 0.0001f) {
        const float inverseTime = 1.0f / timeStep;
        velocity = {
            (position.x - g_rotateAnchor.lastGripPosition.x) * inverseTime,
            (position.y - g_rotateAnchor.lastGripPosition.y) * inverseTime,
            (position.z - g_rotateAnchor.lastGripPosition.z) * inverseTime,
        };
        velocityValid = true;
    }
    return true;
}

bool ResolveSlideControllerTarget(
    float timeStep,
    camera_math::Vector3& targetVelocity,
    uint64_t& inputFrame)
{
    OpenXRInputSnapshot input;
    uint32_t handIndex = 1;
    const OpenXRHandInput* hand = nullptr;
    if (g_openxr == nullptr || !g_openxr->GetLatestInput(input) || !input.active
        || (hand = SelectDominantHand(input, &handIndex)) == nullptr
        || !hand->gripPose.valid || !hand->gripPose.positionTracked) {
        g_slideFallbackPose.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    inputFrame = input.gameFrame;
    HPLTrackedPoseWorld grip;
    if (!ResolveHPLTrackedPoseWorld(hand->gripPose, input.gameFrame, grip)
        || !grip.positionTracked) {
        g_slideFallbackPose.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    camera_math::Vector3 velocity{};
    bool velocityValid = hand->gripPose.linearVelocityValid
        && ResolveHPLReferenceVectorWorld(
            hand->gripPose.linearVelocityX,
            hand->gripPose.linearVelocityY,
            hand->gripPose.linearVelocityZ,
            false,
            velocity.x,
            velocity.y,
            velocity.z);
    const camera_math::Vector3 position{grip.positionX, grip.positionY, grip.positionZ};
    if (!velocityValid && g_slideAnchor.valid
        && g_slideAnchor.lastInputFrame != 0
        && input.gameFrame != g_slideAnchor.lastInputFrame
        && std::isfinite(timeStep) && timeStep > 0.0001f) {
        const float inverseTime = 1.0f / timeStep;
        velocity = {
            (position.x - g_slideAnchor.lastGripPosition.x) * inverseTime,
            (position.y - g_slideAnchor.lastGripPosition.y) * inverseTime,
            (position.z - g_slideAnchor.lastGripPosition.z) * inverseTime,
        };
        velocityValid = true;
    }
    g_slideAnchor.lastInputFrame = input.gameFrame;
    g_slideAnchor.lastGripPosition = position;
    if (!velocityValid) {
        g_slideFallbackPose.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    const float scale = g_config.hplControllerSlideVelocityScale
        * std::max(g_config.hplWorldScale, 0.001f);
    targetVelocity = {velocity.x * scale, velocity.y * scale, velocity.z * scale};
    return std::isfinite(targetVelocity.x)
        && std::isfinite(targetVelocity.y)
        && std::isfinite(targetVelocity.z);
}

void ResetSlideAnchor()
{
    g_slideAnchor = {};
}

void ResetRotateAnchor()
{
    g_rotateAnchor = {};
}

const OpenXRHandInput* SelectDominantHand(
    const OpenXRInputSnapshot& input,
    uint32_t* selectedHandIndex = nullptr)
{
    uint32_t handIndex = g_config.hplControllerDominantHand == "left" ? 0u : 1u;
    const OpenXRHandInput* preferred = handIndex == 0 ? &input.left : &input.right;
    if (preferred->active) {
        if (selectedHandIndex != nullptr) *selectedHandIndex = handIndex;
        return preferred;
    }
    if (!g_config.hplControllerOneHandFallback) return nullptr;
    handIndex ^= 1u;
    const OpenXRHandInput* fallback = handIndex == 0 ? &input.left : &input.right;
    if (fallback->active && selectedHandIndex != nullptr) *selectedHandIndex = handIndex;
    return fallback->active ? fallback : nullptr;
}

bool ResolveTwoHandGrabBasis(
    const OpenXRInputSnapshot& input,
    uint32_t dominantHandIndex,
    two_hand_math::TwoHandBasis& basis,
    bool& requested,
    float& supportSqueeze)
{
    basis = {};
    requested = false;
    supportSqueeze = 0.0f;
    if (!g_config.hplControllerTwoHandGrabRotation) return false;

    const OpenXRHandInput& dominant = dominantHandIndex == 0 ? input.left : input.right;
    const OpenXRHandInput& support = dominantHandIndex == 0 ? input.right : input.left;
    supportSqueeze = support.squeeze;
    requested = support.active
        && supportSqueeze >= g_config.hplControllerTwoHandSqueezeThreshold;
    if (!requested) return false;

    g_twoHandGrabCandidates.fetch_add(1, std::memory_order_relaxed);
    HPLTrackedPoseWorld dominantGrip{};
    HPLTrackedPoseWorld supportGrip{};
    const bool valid = dominant.gripPose.valid
        && dominant.gripPose.orientationTracked
        && dominant.gripPose.positionTracked
        && support.gripPose.valid
        && support.gripPose.orientationTracked
        && support.gripPose.positionTracked
        && ResolveHPLTrackedPoseWorld(dominant.gripPose, input.gameFrame, dominantGrip)
        && ResolveHPLTrackedPoseWorld(support.gripPose, input.gameFrame, supportGrip)
        && dominantGrip.orientationTracked
        && dominantGrip.positionTracked
        && supportGrip.positionTracked
        && two_hand_math::BuildTwoHandBasis(
            {dominantGrip.positionX, dominantGrip.positionY, dominantGrip.positionZ},
            {dominantGrip.forwardX, dominantGrip.forwardY, dominantGrip.forwardZ},
            {dominantGrip.upX, dominantGrip.upY, dominantGrip.upZ},
            {supportGrip.positionX, supportGrip.positionY, supportGrip.positionZ},
            g_config.hplControllerTwoHandDirectionBlend,
            g_config.hplControllerTwoHandMinSeparationMeters * g_config.hplWorldScale,
            g_config.hplControllerTwoHandMaxSeparationMeters * g_config.hplWorldScale,
            basis);
    if (!valid) g_twoHandGrabFallbacks.fetch_add(1, std::memory_order_relaxed);
    return valid;
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
    float& z,
    OpenXRControllerPose& gripPose,
    uint64_t& inputFrame)
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
    gripPose = hand->gripPose;
    inputFrame = input.gameFrame;
    return std::isfinite(x) && std::isfinite(y) && std::isfinite(z);
}

float* HookPidVectorOutput(void* pid, float* output, const float* error, float timeStep)
{
    const uint64_t call = g_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    if ((!g_config.hplControllerSlideDirectVelocity
            && !g_config.hplControllerRotateDirectVelocity
            && !g_config.hplControllerGrabTranslation
            && !g_config.hplControllerGrabRotation
            && !g_config.hplControllerTwoHandGrabRotation)
        || error == nullptr
        || !std::isfinite(error[0]) || !std::isfinite(error[1]) || !std::isfinite(error[2])) {
        return g_originalPidOutput(pid, output, error, timeStep);
    }

    HPLPlayerStateSnapshot player;
    if (!GetHPLPlayerStateSnapshot(player)
        || !player.playerValid
        || player.authoredCameraActive) {
        g_fallbackState.fetch_add(1, std::memory_order_relaxed);
        ResetAnchor();
        ResetSlideAnchor();
        ResetRotateAnchor();
        return g_originalPidOutput(pid, output, error, timeStep);
    }

    if (player.playerStateId == kSlidePlayerState
        && g_config.hplControllerSlideDirectVelocity) {
        ResetAnchor();
        ResetRotateAnchor();
        if (!IsSlideForcePid(pid)) {
            return g_originalPidOutput(pid, output, error, timeStep);
        }
        g_slidePidMatches.fetch_add(1, std::memory_order_relaxed);
        uint64_t inputFrame = 0;
        camera_math::Vector3 targetVelocity{};
        if (!ResolveSlideControllerTarget(timeStep, targetVelocity, inputFrame)) {
            return g_originalPidOutput(pid, output, error, timeStep);
        }
        if (!g_slideAnchor.valid && !ResolveSlideJointPin(inputFrame, g_slideAnchor)) {
            return g_originalPidOutput(pid, output, error, timeStep);
        }
        const float targetAlongPin = targetVelocity.x * g_slideAnchor.pin.x
            + targetVelocity.y * g_slideAnchor.pin.y
            + targetVelocity.z * g_slideAnchor.pin.z;
        const float maxSpeed = g_config.hplControllerSlideMaxVelocityMetersPerSecond
            * std::max(g_config.hplWorldScale, 0.001f);
        const float targetSpeed = std::clamp(targetAlongPin, -maxSpeed, maxSpeed);
        const float modifiedError[3] = {
            error[0] + g_slideAnchor.pin.x * targetSpeed,
            error[1] + g_slideAnchor.pin.y * targetSpeed,
            error[2] + g_slideAnchor.pin.z * targetSpeed,
        };
        const uint64_t substitution = g_slideSubstitutions.fetch_add(
            1, std::memory_order_relaxed) + 1;
        if (substitution <= 12
            || substitution % static_cast<uint64_t>(
                std::max(g_config.hplControllerLogInterval, 1)) == 0) {
            Logger::Instance().Write(
                LogLevel::Info,
                "hpl_slide_target call=%llu applied=1 frame=%llu body=%p joint=%p pin=%.5f,%.5f,%.5f controllerVelocity=%.4f,%.4f,%.4f targetSpeed=%.4f nativeError=%.4f,%.4f,%.4f modifiedError=%.4f,%.4f,%.4f route=controller_world_velocity_projected_to_native_joint_pid",
                static_cast<unsigned long long>(call),
                static_cast<unsigned long long>(inputFrame),
                g_slideAnchor.body,
                g_slideAnchor.joint,
                g_slideAnchor.pin.x, g_slideAnchor.pin.y, g_slideAnchor.pin.z,
                targetVelocity.x, targetVelocity.y, targetVelocity.z,
                targetSpeed,
                error[0], error[1], error[2],
                modifiedError[0], modifiedError[1], modifiedError[2]);
        }
        return g_originalPidOutput(pid, output, modifiedError, timeStep);
    }
    ResetSlideAnchor();
    if ((player.playerStateId == kSwingDoorPlayerState
            || player.playerStateId == kLeverPlayerState)
        && g_config.hplControllerRotateDirectVelocity) {
        ResetAnchor();
        if (!IsRotateTorquePid(pid)) {
            return g_originalPidOutput(pid, output, error, timeStep);
        }
        g_rotatePidMatches.fetch_add(1, std::memory_order_relaxed);
        uint64_t inputFrame = 0;
        camera_math::Vector3 gripPosition{};
        camera_math::Vector3 gripVelocity{};
        bool velocityValid = false;
        if (!ResolveRotateControllerMotion(
                timeStep, gripPosition, gripVelocity, velocityValid, inputFrame)) {
            return g_originalPidOutput(pid, output, error, timeStep);
        }
        if (!g_rotateAnchor.valid
            && !ResolveRotateJoint(inputFrame, gripPosition, g_rotateAnchor)) {
            return g_originalPidOutput(pid, output, error, timeStep);
        }
        if (!velocityValid) {
            g_rotateAnchor.lastGripPosition = gripPosition;
            g_rotateAnchor.lastInputFrame = inputFrame;
            g_rotateFallbackPose.fetch_add(1, std::memory_order_relaxed);
            return g_originalPidOutput(pid, output, error, timeStep);
        }
        const camera_math::Vector3 virtualPoint{
            g_rotateAnchor.hitPoint.x
                + gripPosition.x - g_rotateAnchor.initialGripPosition.x,
            g_rotateAnchor.hitPoint.y
                + gripPosition.y - g_rotateAnchor.initialGripPosition.y,
            g_rotateAnchor.hitPoint.z
                + gripPosition.z - g_rotateAnchor.initialGripPosition.z,
        };
        const float targetSpeed = grab_math::ResolveHingeAngularVelocity(
            g_rotateAnchor.pivot,
            virtualPoint,
            gripVelocity,
            g_rotateAnchor.pin,
            g_config.hplControllerRotateVelocityScale,
            g_config.hplControllerRotateMaxAngularSpeed);
        g_rotateAnchor.lastGripPosition = gripPosition;
        g_rotateAnchor.lastInputFrame = inputFrame;
        const float modifiedError[3] = {
            error[0] + g_rotateAnchor.pin.x * targetSpeed,
            error[1] + g_rotateAnchor.pin.y * targetSpeed,
            error[2] + g_rotateAnchor.pin.z * targetSpeed,
        };
        const uint64_t substitution = g_rotateSubstitutions.fetch_add(
            1, std::memory_order_relaxed) + 1;
        if (substitution <= 12
            || substitution % static_cast<uint64_t>(
                std::max(g_config.hplControllerLogInterval, 1)) == 0) {
            Logger::Instance().Write(
                LogLevel::Info,
                "hpl_rotate_target call=%llu applied=1 state=%d frame=%llu body=%p joint=%p pin=%.5f,%.5f,%.5f pivot=%.4f,%.4f,%.4f virtualPoint=%.4f,%.4f,%.4f controllerVelocity=%.4f,%.4f,%.4f targetAngularSpeed=%.4f nativeError=%.4f,%.4f,%.4f modifiedError=%.4f,%.4f,%.4f route=controller_world_arc_about_native_joint_pivot",
                static_cast<unsigned long long>(call),
                player.playerStateId,
                static_cast<unsigned long long>(inputFrame),
                g_rotateAnchor.body,
                g_rotateAnchor.joint,
                g_rotateAnchor.pin.x, g_rotateAnchor.pin.y, g_rotateAnchor.pin.z,
                g_rotateAnchor.pivot.x, g_rotateAnchor.pivot.y, g_rotateAnchor.pivot.z,
                virtualPoint.x, virtualPoint.y, virtualPoint.z,
                gripVelocity.x, gripVelocity.y, gripVelocity.z,
                targetSpeed,
                error[0], error[1], error[2],
                modifiedError[0], modifiedError[1], modifiedError[2]);
        }
        return g_originalPidOutput(pid, output, modifiedError, timeStep);
    }
    ResetRotateAnchor();
    if (player.playerStateId != kGrabPlayerState) {
        g_fallbackState.fetch_add(1, std::memory_order_relaxed);
        ResetAnchor();
        return g_originalPidOutput(pid, output, error, timeStep);
    }

    if (IsGrabTorquePid(pid)) {
        g_torquePidMatches.fetch_add(1, std::memory_order_relaxed);
        if (!g_config.hplControllerGrabRotation
            && !g_config.hplControllerTwoHandGrabRotation) {
            return g_originalPidOutput(pid, output, error, timeStep);
        }

        OpenXRInputSnapshot input;
        const OpenXRHandInput* hand = nullptr;
        uint32_t dominantHandIndex = 1;
        GrabAnchor anchor;
        {
            std::lock_guard lock(g_stateMutex);
            anchor = g_anchor;
        }
        if (!anchor.valid || anchor.player != player.player || anchor.camera != player.camera
            || !anchor.gripOrientationTracked || g_openxr == nullptr
            || !g_openxr->GetLatestInput(input) || !input.active
            || (input.gameFrame > anchor.lastInputFrame
                && input.gameFrame - anchor.lastInputFrame > 4)
            || (hand = SelectDominantHand(input, &dominantHandIndex)) == nullptr
            || !hand->gripPose.valid || !hand->gripPose.orientationTracked) {
            g_fallbackPose.fetch_add(1, std::memory_order_relaxed);
            return g_originalPidOutput(pid, output, error, timeStep);
        }

        const camera_math::Quaternion current{
            hand->gripPose.orientationX,
            hand->gripPose.orientationY,
            hand->gripPose.orientationZ,
            hand->gripPose.orientationW,
        };
        two_hand_math::TwoHandBasis twoHandBasis{};
        bool twoHandRequested = false;
        float supportSqueeze = 0.0f;
        const bool twoHandValid = ResolveTwoHandGrabBasis(
            input,
            dominantHandIndex,
            twoHandBasis,
            twoHandRequested,
            supportSqueeze);
        bool modeTransition = false;
        {
            std::lock_guard lock(g_stateMutex);
            if (twoHandValid && !g_anchor.twoHandActive) {
                g_anchor.twoHandActive = true;
                g_anchor.twoHandAnchorDirection = twoHandBasis.forward;
                modeTransition = true;
                g_twoHandGrabEngagements.fetch_add(1, std::memory_order_relaxed);
            } else if (!twoHandValid && g_anchor.twoHandActive) {
                g_anchor.twoHandActive = false;
                g_anchor.twoHandAnchorDirection = {};
                g_anchor.gripOrientation = current;
                modeTransition = true;
                g_twoHandGrabReleases.fetch_add(1, std::memory_order_relaxed);
            }
            anchor = g_anchor;
        }
        if (modeTransition) {
            Logger::Instance().Write(
                LogLevel::Info,
                "hpl_grab_two_hand transition=%s frame=%llu dominant=%s support=%s squeeze=%.3f separation=%.4f policy=reanchor_before_torque",
                twoHandValid ? "engaged" : "released",
                static_cast<unsigned long long>(input.gameFrame),
                dominantHandIndex == 0 ? "left" : "right",
                dominantHandIndex == 0 ? "right" : "left",
                supportSqueeze,
                twoHandBasis.separation);
            return g_originalPidOutput(pid, output, error, timeStep);
        }
        if (!twoHandValid && !g_config.hplControllerGrabRotation) {
            return g_originalPidOutput(pid, output, error, timeStep);
        }

        camera_math::Vector3 correction{};
        if (twoHandValid) {
            correction = two_hand_math::ResolveDirectionAngularTargetVelocity(
                anchor.twoHandAnchorDirection,
                twoHandBasis.forward,
                g_config.hplControllerGrabRotationGain,
                g_config.hplControllerGrabRotationSign,
                g_config.hplControllerGrabMaxAngularSpeed);
            g_twoHandGrabSubstitutions.fetch_add(1, std::memory_order_relaxed);
        } else {
            const camera_math::Vector3 referenceCorrection =
                grab_math::ResolveAngularTargetVelocity(
                    anchor.gripOrientation,
                    current,
                    g_config.hplControllerGrabRotationGain,
                    g_config.hplControllerGrabRotationSign,
                    g_config.hplControllerGrabMaxAngularSpeed);
            if (!ResolveHPLReferenceVectorWorld(
                    referenceCorrection.x,
                    referenceCorrection.y,
                    referenceCorrection.z,
                    false,
                    correction.x,
                    correction.y,
                    correction.z)) {
                g_fallbackCamera.fetch_add(1, std::memory_order_relaxed);
                return g_originalPidOutput(pid, output, error, timeStep);
            }
        }
        const float modifiedError[3] = {
            error[0] + correction.x,
            error[1] + correction.y,
            error[2] + correction.z,
        };
        const uint64_t substitution = g_rotationSubstitutions.fetch_add(1, std::memory_order_relaxed) + 1;
        if (substitution <= 8
            || substitution % static_cast<uint64_t>(std::max(g_config.hplControllerLogInterval, 1)) == 0) {
            Logger::Instance().Write(
                LogLevel::Info,
                "hpl_grab_rotation call=%llu applied=1 mode=%s nativeError=%.4f,%.4f,%.4f controllerTarget=%.4f,%.4f,%.4f modifiedError=%.4f,%.4f,%.4f gain=%.2f maxSpeed=%.2f supportSqueeze=%.3f separation=%.4f",
                static_cast<unsigned long long>(call),
                twoHandValid ? "two_hand_direction" : "dominant_grip_orientation",
                error[0], error[1], error[2],
                correction.x, correction.y, correction.z,
                modifiedError[0], modifiedError[1], modifiedError[2],
                g_config.hplControllerGrabRotationGain,
                g_config.hplControllerGrabMaxAngularSpeed,
                supportSqueeze,
                twoHandBasis.separation);
        }
        return g_originalPidOutput(pid, output, modifiedError, timeStep);
    }

    if (!IsGrabForcePid(pid)) {
        g_fallbackPid.fetch_add(1, std::memory_order_relaxed);
        return g_originalPidOutput(pid, output, error, timeStep);
    }
    g_forcePidMatches.fetch_add(1, std::memory_order_relaxed);
    if (!g_config.hplControllerGrabTranslation) {
        bool anchorReady = false;
        {
            std::lock_guard lock(g_stateMutex);
            anchorReady = g_anchor.valid;
        }
        if (anchorReady) {
            return g_originalPidOutput(pid, output, error, timeStep);
        }
    }

    float relativeX = 0.0f;
    float relativeY = 0.0f;
    float relativeZ = 0.0f;
    OpenXRControllerPose gripPose;
    uint64_t inputFrame = 0;
    if (!ResolveGrabRelativePosition(
            player,
            relativeX,
            relativeY,
            relativeZ,
            gripPose,
            inputFrame)) {
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
            || g_anchor.camera != player.camera
            || (inputFrame > g_anchor.lastInputFrame
                && inputFrame - g_anchor.lastInputFrame > 4)) {
            g_anchor.valid = true;
            g_anchor.pid = pid;
            g_anchor.player = player.player;
            g_anchor.camera = player.camera;
            g_anchor.relativeX = relativeX;
            g_anchor.relativeY = relativeY;
            g_anchor.relativeZ = relativeZ;
            g_anchor.gripOrientation = {
                gripPose.orientationX,
                gripPose.orientationY,
                gripPose.orientationZ,
                gripPose.orientationW,
            };
            g_anchor.gripOrientationTracked = gripPose.orientationTracked;
            g_anchor.twoHandActive = false;
            g_anchor.twoHandAnchorDirection = {};
            g_anchor.lastInputFrame = inputFrame;
            anchored = true;
        } else {
            deltaX = relativeX - g_anchor.relativeX;
            deltaY = relativeY - g_anchor.relativeY;
            deltaZ = relativeZ - g_anchor.relativeZ;
            g_anchor.lastInputFrame = inputFrame;
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

void CallNativeAddImpulse(void* body, const float* impulse)
{
    if (body == nullptr || impulse == nullptr) return;
    auto** vtable = *reinterpret_cast<void***>(body);
    if (vtable == nullptr) return;
    auto* method = reinterpret_cast<PhysicsBodyImpulseFn>(vtable[0x130 / sizeof(void*)]);
    if (method != nullptr && reinterpret_cast<void*>(method) != g_addImpulseTarget) {
        method(body, impulse);
    }
}

void HookAddImpulse(void* body, const float* impulse)
{
    PendingThrow pending;
    bool passThrough = false;
    {
        std::lock_guard lock(g_stateMutex);
        if (!g_pendingThrow.armed || GetTickCount64() > g_pendingThrow.deadlineMs) {
            g_pendingThrow = {};
            passThrough = true;
        } else {
            pending = g_pendingThrow;
            g_pendingThrow = {};
        }
    }
    if (passThrough) {
        CallNativeAddImpulse(body, impulse);
        return;
    }

    HPLPlayerStateSnapshot player;
    if (impulse == nullptr || !g_config.hplControllerThrowRedirect
        || !GetHPLPlayerStateSnapshot(player) || !player.playerValid
        || player.playerStateId != kGrabPlayerState
        || !std::isfinite(impulse[0]) || !std::isfinite(impulse[1]) || !std::isfinite(impulse[2])) {
        g_throwFallbacks.fetch_add(1, std::memory_order_relaxed);
        CallNativeAddImpulse(body, impulse);
        return;
    }

    const float nativeMagnitude = std::sqrt(
        impulse[0] * impulse[0] + impulse[1] * impulse[1] + impulse[2] * impulse[2]);
    if (!std::isfinite(nativeMagnitude) || nativeMagnitude < 1.0e-6f) {
        g_throwFallbacks.fetch_add(1, std::memory_order_relaxed);
        CallNativeAddImpulse(body, impulse);
        return;
    }

    float directionX = 0.0f;
    float directionY = 0.0f;
    float directionZ = 0.0f;
    float controllerSpeed = 0.0f;
    const OpenXRControllerPose& pose = pending.gripPose;
    if (pose.linearVelocityValid) {
        controllerSpeed = std::sqrt(
            pose.linearVelocityX * pose.linearVelocityX
            + pose.linearVelocityY * pose.linearVelocityY
            + pose.linearVelocityZ * pose.linearVelocityZ);
        if (std::isfinite(controllerSpeed)
            && controllerSpeed >= g_config.hplControllerThrowVelocityThreshold) {
            ResolveHPLReferenceVectorWorld(
                pose.linearVelocityX,
                pose.linearVelocityY,
                pose.linearVelocityZ,
                false,
                directionX,
                directionY,
                directionZ);
        }
    }

    const char* source = "velocity";
    float directionLength = std::sqrt(
        directionX * directionX + directionY * directionY + directionZ * directionZ);
    if (!std::isfinite(directionLength) || directionLength < 1.0e-6f) {
        HPLTrackedPoseWorld worldGrip;
        if (!ResolveHPLTrackedPoseWorld(pose, pending.gameFrame, worldGrip)) {
            g_throwFallbacks.fetch_add(1, std::memory_order_relaxed);
            CallNativeAddImpulse(body, impulse);
            return;
        }
        directionX = worldGrip.forwardX;
        directionY = worldGrip.forwardY;
        directionZ = worldGrip.forwardZ;
        directionLength = std::sqrt(
            directionX * directionX + directionY * directionY + directionZ * directionZ);
        source = "grip_forward";
    }
    if (!std::isfinite(directionLength) || directionLength < 1.0e-6f) {
        g_throwFallbacks.fetch_add(1, std::memory_order_relaxed);
        CallNativeAddImpulse(body, impulse);
        return;
    }

    float velocityScale = 1.0f;
    if (g_config.hplControllerThrowVelocityScale
        && controllerSpeed >= g_config.hplControllerThrowVelocityThreshold) {
        velocityScale = std::clamp(
            controllerSpeed / std::max(g_config.hplControllerThrowVelocityReference, 0.1f),
            0.5f,
            1.5f);
    }
    const float scale = nativeMagnitude * velocityScale / directionLength;
    const float redirected[3] = {
        directionX * scale,
        directionY * scale,
        directionZ * scale,
    };
    const uint64_t redirects = g_throwRedirects.fetch_add(1, std::memory_order_relaxed) + 1;
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_controller_throw applied=1 count=%llu body=%p source=%s controllerSpeed=%.3f nativeImpulse=%.4f,%.4f,%.4f redirectedImpulse=%.4f,%.4f,%.4f velocityScale=%.3f",
        static_cast<unsigned long long>(redirects),
        body,
        source,
        controllerSpeed,
        impulse[0], impulse[1], impulse[2],
        redirected[0], redirected[1], redirected[2],
        velocityScale);
    CallNativeAddImpulse(body, redirected);
}

bool WriteCodeBytes(void* target, const void* bytes, size_t size)
{
    DWORD oldProtect = 0;
    if (target == nullptr || bytes == nullptr || size == 0
        || !VirtualProtect(target, size, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        return false;
    }
    std::memcpy(target, bytes, size);
    FlushInstructionCache(GetCurrentProcess(), target, size);
    DWORD ignored = 0;
    VirtualProtect(target, size, oldProtect, &ignored);
    return true;
}

bool InstallAddImpulsePatch(HMODULE executable)
{
    if (!IsInsideImage(executable, kAddImpulseThunkRva, sizeof(kAddImpulseThunkSignature))) {
        return false;
    }
    auto* target = reinterpret_cast<uint8_t*>(executable) + kAddImpulseThunkRva;
    if (std::memcmp(target, kAddImpulseThunkSignature, sizeof(kAddImpulseThunkSignature)) != 0) {
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_grab_bridge impulse_patch_failed reason=signature_mismatch rva=0x%llx",
            static_cast<unsigned long long>(kAddImpulseThunkRva));
        return false;
    }
    std::memcpy(g_addImpulseOriginal, target, sizeof(g_addImpulseOriginal));
    uint8_t jump[sizeof(kAddImpulseThunkSignature)] = {0x48, 0xb8};
    const uintptr_t hook = reinterpret_cast<uintptr_t>(&HookAddImpulse);
    std::memcpy(jump + 2, &hook, sizeof(hook));
    jump[10] = 0xff;
    jump[11] = 0xe0;
    if (!WriteCodeBytes(target, jump, sizeof(jump))) {
        return false;
    }
    g_addImpulseTarget = target;
    return true;
}

void RestoreAddImpulsePatch()
{
    if (g_addImpulseTarget != nullptr) {
        WriteCodeBytes(g_addImpulseTarget, g_addImpulseOriginal, sizeof(g_addImpulseOriginal));
        g_addImpulseTarget = nullptr;
    }
}

} // namespace

bool InstallHPLGrabBridge(const Config& config, OpenXRRuntime* openxr)
{
    std::lock_guard lock(g_installMutex);
    g_config = config;
    g_openxr = openxr;
    const bool pidEnabled = config.hplControllerSlideDirectVelocity
        || config.hplControllerRotateDirectVelocity
        || config.hplControllerGrabTranslation
        || config.hplControllerGrabRotation
        || config.hplControllerTwoHandGrabRotation;
    if (!pidEnabled && !config.hplControllerThrowRedirect) {
        Logger::Instance().Write(LogLevel::Info, "hpl_grab_bridge disabled config=0");
        return true;
    }

    HMODULE executable = GetModuleHandleW(nullptr);
    if (config.hplControllerThrowRedirect && !InstallAddImpulsePatch(executable)) {
        Logger::Instance().Write(LogLevel::Error, "hpl_grab_bridge install_failed reason=impulse_patch");
        return false;
    }
    if (!pidEnabled) {
        Logger::Instance().Write(
            LogLevel::Info,
            "hpl_grab_bridge install_ok pid=0 throwRedirect=1 addImpulseRva=0x%llx",
            static_cast<unsigned long long>(kAddImpulseThunkRva));
        return true;
    }
    if (!IsInsideImage(executable, kPidVectorOutputRva, sizeof(kPidVectorOutputSignature))) {
        Logger::Instance().Write(LogLevel::Error, "hpl_grab_bridge install_failed reason=invalid_image_range");
        RestoreAddImpulsePatch();
        return false;
    }
    auto* target = reinterpret_cast<std::byte*>(executable) + kPidVectorOutputRva;
    if (std::memcmp(target, kPidVectorOutputSignature, sizeof(kPidVectorOutputSignature)) != 0) {
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_grab_bridge install_failed reason=signature_mismatch rva=0x%llx",
            static_cast<unsigned long long>(kPidVectorOutputRva));
        RestoreAddImpulsePatch();
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
        RestoreAddImpulsePatch();
        return false;
    }
    status = MH_EnableHook(target);
    if (status != MH_OK && status != MH_ERROR_ENABLED) {
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_grab_bridge install_failed reason=enable_hook status=%s",
            MH_StatusToString(status));
        MH_RemoveHook(target);
        RestoreAddImpulsePatch();
        return false;
    }
    g_pidOutputTarget = target;
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_grab_bridge install_ok pidOutputRva=0x%llx target=%p playerState=1 translation=%d rotation=%d twoHandRotation=%d slideDirectVelocity=%d slideVelocityScale=%.3f slideMaxVelocityMetersPerSecond=%.3f rotateDirectVelocity=%d rotateVelocityScale=%.3f rotateMaxAngularSpeed=%.3f twoHand={squeeze=%.3f separationMeters=%.3f,%.3f blend=%.3f} throwRedirect=%d addImpulseRva=0x%llx pidGains={grabForce=400,0,40 grabTorque=40,0,0.4|0.1 slideForce=6,0,0.1 rotateTorque=10,0,1} translationScale=%.3f maxOffsetMeters=%.3f rotationGain=%.2f rotationSign=%.1f maxAngularSpeed=%.2f policy=modify_target_error_preserve_native_pid_and_impulse",
        static_cast<unsigned long long>(kPidVectorOutputRva),
        target,
        config.hplControllerGrabTranslation ? 1 : 0,
        config.hplControllerGrabRotation ? 1 : 0,
        config.hplControllerTwoHandGrabRotation ? 1 : 0,
        config.hplControllerSlideDirectVelocity ? 1 : 0,
        config.hplControllerSlideVelocityScale,
        config.hplControllerSlideMaxVelocityMetersPerSecond,
        config.hplControllerRotateDirectVelocity ? 1 : 0,
        config.hplControllerRotateVelocityScale,
        config.hplControllerRotateMaxAngularSpeed,
        config.hplControllerTwoHandSqueezeThreshold,
        config.hplControllerTwoHandMinSeparationMeters,
        config.hplControllerTwoHandMaxSeparationMeters,
        config.hplControllerTwoHandDirectionBlend,
        config.hplControllerThrowRedirect ? 1 : 0,
        static_cast<unsigned long long>(kAddImpulseThunkRva),
        config.hplControllerGrabTranslationScale,
        config.hplControllerGrabMaxOffsetMeters,
        config.hplControllerGrabRotationGain,
        config.hplControllerGrabRotationSign,
        config.hplControllerGrabMaxAngularSpeed);
    return true;
}

void ArmHPLControllerThrow(const OpenXRControllerPose& gripPose, uint64_t gameFrame)
{
    if (!g_config.hplControllerThrowRedirect || !gripPose.valid) return;
    std::lock_guard lock(g_stateMutex);
    g_pendingThrow.armed = true;
    g_pendingThrow.deadlineMs = GetTickCount64() + 350;
    g_pendingThrow.gameFrame = gameFrame;
    g_pendingThrow.gripPose = gripPose;
    g_throwArms.fetch_add(1, std::memory_order_relaxed);
}

void RemoveHPLGrabBridge()
{
    std::lock_guard lock(g_installMutex);
    RestoreAddImpulsePatch();
    if (g_pidOutputTarget != nullptr) {
        MH_DisableHook(g_pidOutputTarget);
        MH_RemoveHook(g_pidOutputTarget);
    }
    g_pidOutputTarget = nullptr;
    g_originalPidOutput = nullptr;
    g_openxr = nullptr;
    ResetAnchor();
    ResetSlideAnchor();
    ResetRotateAnchor();
    {
        std::lock_guard stateLock(g_stateMutex);
        g_pendingThrow = {};
    }
    Logger::Instance().Write(LogLevel::Info, "hpl_grab_bridge removed");
}

void LogHPLGrabBridgeSummary()
{
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_grab_bridge_summary installed=%d pidInstalled=%d impulsePatchInstalled=%d calls=%llu forcePidMatches=%llu torquePidMatches=%llu anchors=%llu translationSubstitutions=%llu rotationSubstitutions=%llu slide={pidMatches=%llu anchors=%llu substitutions=%llu fallbackHit=%llu fallbackJoint=%llu fallbackPose=%llu} rotate={pidMatches=%llu anchors=%llu substitutions=%llu fallbackHit=%llu fallbackJoint=%llu fallbackPose=%llu} twoHand={candidates=%llu engagements=%llu substitutions=%llu releases=%llu fallbacks=%llu} throwArms=%llu throwRedirects=%llu throwFallbacks=%llu fallbackState=%llu fallbackPid=%llu fallbackPose=%llu fallbackStale=%llu fallbackCamera=%llu",
        (g_pidOutputTarget != nullptr || g_addImpulseTarget != nullptr) ? 1 : 0,
        g_pidOutputTarget != nullptr ? 1 : 0,
        g_addImpulseTarget != nullptr ? 1 : 0,
        static_cast<unsigned long long>(g_calls.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_forcePidMatches.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_torquePidMatches.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_anchors.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_substitutions.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_rotationSubstitutions.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_slidePidMatches.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_slideAnchors.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_slideSubstitutions.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_slideFallbackHit.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_slideFallbackJoint.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_slideFallbackPose.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_rotatePidMatches.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_rotateAnchors.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_rotateSubstitutions.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_rotateFallbackHit.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_rotateFallbackJoint.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_rotateFallbackPose.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_twoHandGrabCandidates.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_twoHandGrabEngagements.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_twoHandGrabSubstitutions.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_twoHandGrabReleases.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_twoHandGrabFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_throwArms.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_throwRedirects.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_throwFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_fallbackState.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_fallbackPid.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_fallbackPose.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_fallbackStale.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_fallbackCamera.load(std::memory_order_relaxed)));
}

} // namespace somavr
