#include "HPLHandsBridge.h"

#include "HPLCameraBridge.h"
#include "HPLArmIKMath.h"
#include "HPLAuthoredInteractionBridge.h"
#include "HPLFlashlightMath.h"
#include "HPLEntityCalibrationProfiles.h"
#include "HPLHandsMath.h"
#include "HPLInteractionBridge.h"
#include "HPLInputBridge.h"
#include "HPLNativeLocomotion.h"
#include "HPLPlayerState.h"
#include "HPLReadMath.h"
#include "HPLTwoHandMath.h"
#include "Logger.h"
#include "SomaBuildSignatures.h"

#include <Windows.h>

#include <MinHook.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace somavr {
namespace {

constexpr uintptr_t kLuxEntitySetMatrixRva = 0x0bcd90;
constexpr uint8_t kLuxEntitySetMatrixSignature[] = {
    0x40, 0x53,
    0x48, 0x81, 0xec, 0xa0, 0x00, 0x00, 0x00,
    0x48, 0x8b, 0x01,
    0x48, 0x8b, 0xd9,
    0xc6, 0x81, 0x62, 0x04, 0x00, 0x00, 0x01,
};
constexpr uintptr_t kLuxEntityGetNameRva = 0x00fb60;
constexpr uint8_t kLuxEntityGetNameSignature[] = {
    0x48, 0x8d, 0x81, 0x20, 0x01, 0x00, 0x00,
    0xc3,
};
constexpr uintptr_t kLuxEntityGetMeshEntityRva = 0x165270;
constexpr uint8_t kLuxEntityGetMeshEntitySignature[] = {
    0x48, 0x8b, 0x01,
    0xff, 0xa0, 0xd8, 0x00, 0x00, 0x00,
};
constexpr uintptr_t kMeshEntityGetBoneStateFromNameRva = 0x200980;
constexpr uint8_t kMeshEntityGetBoneStateFromNameSignature[] = {
    0x40, 0x53,
    0x48, 0x83, 0xec, 0x20,
    0x48, 0x8b, 0xd9,
};
constexpr uintptr_t kLuxMapDestroyEntityRva = 0x127a70;
constexpr uint8_t kLuxMapDestroyEntitySignature[] = {
    0x48, 0x85, 0xd2,
    0x0f, 0x84, 0xbe, 0x00, 0x00, 0x00,
    0x48, 0x89, 0x5c, 0x24, 0x08,
    0x48, 0x89, 0x54, 0x24, 0x10,
    0x57,
    0x48, 0x83, 0xec, 0x20,
};
constexpr uintptr_t kLuxEntitySetActiveRva = 0x0b3700;
constexpr uint8_t kLuxEntitySetActiveSignature[] = {
    0x40, 0x53, 0x57, 0x48, 0x83, 0xec, 0x28, 0x0f,
    0xb6, 0xfa, 0x48, 0x8b, 0xd9, 0x38, 0x91, 0x61,
    0x04, 0x00, 0x00,
};
constexpr uintptr_t kGetClosestBodyRva = 0x0cd7d0;
constexpr uint8_t kGetClosestBodySignature[] = {
    0x48, 0x83, 0xec, 0x38,
    0x48, 0x8b, 0x44, 0x24, 0x60,
    0x4c, 0x8b, 0xc2,
    0x48, 0x8b, 0xd1,
};
constexpr size_t kNativeStringInlineCapacity = 15;
constexpr size_t kMaxNativeNameLength = 127;
constexpr size_t kNodeLocalMatrixOffset = 0x44;
constexpr size_t kNodeWorldMatrixOffset = 0x84;
constexpr size_t kNodeUsePreTransformOffset = 0xc4;
constexpr size_t kNodeUsePostTransformOffset = 0xc5;
constexpr size_t kNodePostTransformOffset = 0x108;
constexpr size_t kNodeParentOffset = 0x180;
constexpr size_t kMeshBoneMatricesOffset = 0x340;
constexpr size_t kMaxIdentityCache = 4096;
constexpr size_t kMaxIdentityLogs = 16;
constexpr uint32_t kSkeletonProbeBurstFrames = 12;
constexpr int kNormalPlayerState = 0;
constexpr int kNormalMoveState = 0;
constexpr char kMedicineEntityName[] = "Tracer_Fluid_HudObject";
constexpr float kQuarterScale = 0.25f;
constexpr float kQuarterScaleTolerance = 0.04f;
constexpr float kUniformScaleTolerance = 0.04f;
constexpr size_t kArmPoseNodeCount = 34;
constexpr size_t kArmPoseClavicleIndex = 0;
constexpr size_t kArmPoseShoulderIndex = 2;
constexpr size_t kArmPoseElbowIndex = 9;
constexpr size_t kArmPoseWristIndex = 14;
constexpr size_t kArmPoseIndexRootIndex = 18;
constexpr size_t kArmPoseMiddleRootIndex = 22;
constexpr size_t kArmPoseRingRootIndex = 26;
constexpr size_t kArmPosePinkyRootIndex = 30;
constexpr float kMedicineArmRootY = 0.6643875f;
constexpr float kMedicineArmRootZ = -0.0431139f;
constexpr float kNeutralArmRootZ = -0.0002f;
constexpr float kMedicineArmRootTranslationTolerance = 0.01f;
constexpr const char* kArmPoseNodeNames[2][kArmPoseNodeCount] = {
    {
        "j_L_Clavicle", "j_L_Shoulder",
        "j_L_Arm_1", "j_L_Arm_2", "j_L_Arm_3", "j_L_Arm_4", "j_L_Arm_5",
        "j_L_Elbow_1", "j_L_Elbow_2",
        "j_L_Arm_6", "j_L_Arm_7", "j_L_Arm_8", "j_L_Arm_9", "j_L_Arm_10",
        "j_L_Wrist",
        "j_L_Thumb_1", "j_L_Thumb_2", "j_L_Thumb_3",
        "j_L_Index_1", "j_L_Index_2", "j_L_Index_3", "j_L_Index_4",
        "j_L_Middle_1", "j_L_Middle_2", "j_L_Middle_3", "j_L_Middle_4",
        "j_L_Ring_1", "j_L_Ring_2", "j_L_Ring_3", "j_L_Ring_4",
        "j_L_Pinky_1", "j_L_Pinky_2", "j_L_Pinky_3", "j_L_Pinky_4",
    },
    {
        "j_R_Clavicle", "j_R_Shoulder",
        "j_R_Arm_1", "j_R_Arm_2", "j_R_Arm_3", "j_R_Arm_4", "j_R_Arm_5",
        "j_R_Elbow_1", "j_R_Elbow_2",
        "j_R_Arm_6", "j_R_Arm_7", "j_R_Arm_8", "j_R_Arm_9", "j_R_Arm_10",
        "j_R_Wrist",
        "j_R_Thumb_1", "j_R_Thumb_2", "j_R_Thumb_3",
        "j_R_Index_1", "j_R_Index_2", "j_R_Index_3", "j_R_Index_4",
        "j_R_Middle_1", "j_R_Middle_2", "j_R_Middle_3", "j_R_Middle_4",
        "j_R_Ring_1", "j_R_Ring_2", "j_R_Ring_3", "j_R_Ring_4",
        "j_R_Pinky_1", "j_R_Pinky_2", "j_R_Pinky_3", "j_R_Pinky_4",
    },
};

using LuxEntitySetMatrixFn = void (*)(void* entity, const float* matrix);
using LuxEntityGetNameFn = const void* (*)(void* entity);
using LuxEntityGetMeshEntityFn = void* (*)(void* entity);
using MeshEntityGetBoneStateFromNameFn = void* (*)(void* meshEntity, const void* nativeName);
using NodeSetUsePostTransformFn = void (*)(void* node, bool enabled);
using NodeSetPostTransformFn = void (*)(void* node, const float* matrix);
using NodeApplyPostAnimTransformFn = void (*)(void* node, bool setChildrenUpdated);
using NodeSetMatrixFn = void (*)(void* node, const float* matrix, bool setChildrenUpdated);
using LuxMapDestroyEntityFn = void (*)(void* map, void* entity);
using LuxEntitySetActiveFn = void (*)(void* entity, bool active);
using GetClosestBodyFn = void* (*)(
    const float* start,
    const float* direction,
    float rayLength,
    float* outDistance,
    float* outSurfaceNormal);

struct NativeStringLayout {
    std::array<std::byte, 16> storage{};
    uint64_t size = 0;
    uint64_t capacity = 0;
};

struct NativeVectorLayout {
    const std::byte* begin = nullptr;
    const std::byte* end = nullptr;
    const std::byte* capacity = nullptr;
};

struct EntityIdentity {
    bool playerHands = false;
    bool hudObject = false;
    bool socketedHudObject = false;
    bool flashlight = false;
    bool readObject = false;
    std::string name;
    entity_calibration::ResolvedProfile calibrationProfile{};
};

struct FlashlightPoseCache {
    bool valid = false;
    uint64_t frame = 0;
    std::array<float, 16> matrix{};
};

struct ReadPresentationAnchor {
    bool presentationSeeded = false;
    bool warmupStarted = false;
    bool presentationOffsetValid = false;
    bool objectOrientationValid = false;
    bool manipulated = false;
    bool rotateActive = false;
    uint64_t firstFrame = 0;
    uint64_t lastFrame = 0;
    uint64_t session = 0;
    std::array<float, 16> sourceMatrix{};
    camera_math::Vector3 sourceCameraPosition{};
    camera_math::Vector3 presentationOffset{};
    camera_math::Quaternion gripOrientation{};
    camera_math::Quaternion objectOrientation{};
    bool viewOrientationValid = false;
    camera_math::Quaternion viewOrientation{};
};

struct WristTrackingFrame {
    bool stateEligible = false;
    uint64_t playerFrame = 0;
    uint64_t inputFrame = 0;
    uint64_t inputAge = UINT64_MAX;
    std::array<bool, 2> eligible{};
    std::array<HPLTrackedPoseWorld, 2> targets{};
    HPLCameraBridgeStatus camera{};
    std::array<hands_math::WristPositionGoal, 2> goals{};
};

struct RetainedHandsState {
    bool valid = false;
    bool suspended = false;
    bool wakeRequested = false;
    void* entity = nullptr;
    void* mesh = nullptr;
    uint64_t lastNativeFrame = 0;
    uint64_t lastSyntheticFrame = 0;
    std::array<float, 16> matrix{};
    bool shoulderAnchorPositionValid = false;
    bool shoulderAnchorUsesTrackedHead = false;
    camera_math::Vector3 shoulderAnchorPosition{};
    bool bodyYawValid = false;
    camera_math::Quaternion bodyYaw{};
};

struct ArmPoseAnchor {
    bool valid = false;
    void* entity = nullptr;
    std::array<void*, kArmPoseNodeCount> nodes{};
    std::array<std::array<float, 16>, kArmPoseNodeCount> localMatrices{};
};

struct SharedArmRootPoseAnchor {
    bool valid = false;
    void* entity = nullptr;
    void* node = nullptr;
    uint64_t lastRestoreFrame = UINT64_MAX;
    std::array<float, 16> localMatrix{};
};

struct ArmErgonomicState {
    bool valid = false;
    void* entity = nullptr;
    uint64_t lastFrame = 0;
    camera_math::Vector3 shoulderOffsetLocal{};
    camera_math::Vector3 elbowDirectionLocal{};
};

enum class RetainedHandsMode {
    Invalid,
    Suspended,
    Active,
};

struct WristOrientationAnchor {
    bool valid = false;
    void* entity = nullptr;
    uint64_t inputFrame = 0;
    bool geometricPalmBasis = false;
    camera_math::Quaternion controllerToWrist{};
};

struct SocketedPropAnchor {
    bool valid = false;
    uint64_t seedFrame = 0;
    std::array<float, 16> gripToProp{};
};

Config g_config;
OpenXRRuntime* g_openxr = nullptr;
LuxEntitySetMatrixFn g_originalSetMatrix = nullptr;
LuxEntityGetNameFn g_getEntityName = nullptr;
LuxEntityGetMeshEntityFn g_getMeshEntity = nullptr;
MeshEntityGetBoneStateFromNameFn g_getBoneStateFromName = nullptr;
NodeSetUsePostTransformFn g_setUsePostTransform = nullptr;
NodeSetPostTransformFn g_setPostTransform = nullptr;
NodeApplyPostAnimTransformFn g_applyPostAnimTransform = nullptr;
NodeSetMatrixFn g_nodeSetMatrix = nullptr;
LuxMapDestroyEntityFn g_originalDestroyEntity = nullptr;
LuxEntitySetActiveFn g_originalSetActive = nullptr;
GetClosestBodyFn g_originalGetClosestBody = nullptr;
void* g_setMatrixTarget = nullptr;
void* g_destroyEntityTarget = nullptr;
void* g_setActiveTarget = nullptr;
void* g_getClosestBodyTarget = nullptr;
std::mutex g_installMutex;
std::mutex g_identityMutex;
entity_calibration::Store g_calibrationProfiles;
std::mutex g_skeletonProbeMutex;
std::unordered_map<void*, EntityIdentity> g_identityCache;
std::unordered_set<void*> g_readCandidateEntities;
std::unordered_map<void*, ReadPresentationAnchor> g_readPresentationAnchors;
void* g_readPresentationOwner = nullptr;
bool g_readPresentationStateActive = false;
uint64_t g_readPresentationSession = 0;
std::mutex g_socketedPropMutex;
std::unordered_map<void*, SocketedPropAnchor> g_socketedPropAnchors;
std::atomic<uint64_t> g_calls = 0;
std::atomic<uint64_t> g_identityReads = 0;
std::atomic<uint64_t> g_identityReadFailures = 0;
std::atomic<uint64_t> g_identityLogs = 0;
std::atomic<uint64_t> g_destroyEntityCalls = 0;
std::atomic<uint64_t> g_identityInvalidations = 0;
std::atomic<uint64_t> g_playerHandsIdentities = 0;
std::atomic<uint64_t> g_playerHandsCalls = 0;
std::atomic<uint64_t> g_hudObjectIdentities = 0;
std::atomic<uint64_t> g_socketedHudObjectIdentities = 0;
std::atomic<uint64_t> g_hudObjectCalls = 0;
std::atomic<uint64_t> g_hudObjectOverrideAttempts = 0;
std::atomic<uint64_t> g_hudObjectOverrides = 0;
std::atomic<uint64_t> g_hudObjectScaleFallbacks = 0;
std::atomic<uint64_t> g_hudObjectStateFallbacks = 0;
std::atomic<uint64_t> g_hudObjectAuthoredFallbacks = 0;
std::atomic<uint64_t> g_hudObjectPoseFallbacks = 0;
std::atomic<uint64_t> g_hudObjectStaleFallbacks = 0;
std::atomic<uint64_t> g_hudObjectMathFallbacks = 0;
std::atomic<uint64_t> g_twoHandHudCandidates = 0;
std::atomic<uint64_t> g_twoHandHudOverrides = 0;
std::atomic<uint64_t> g_twoHandHudFallbacks = 0;
std::atomic<uint64_t> g_flashlightIdentities = 0;
std::atomic<uint64_t> g_flashlightCalls = 0;
std::atomic<uint64_t> g_flashlightOverrideAttempts = 0;
std::atomic<uint64_t> g_flashlightOverrides = 0;
std::atomic<uint64_t> g_flashlightStateFallbacks = 0;
std::atomic<uint64_t> g_flashlightAuthoredFallbacks = 0;
std::atomic<uint64_t> g_flashlightPoseFallbacks = 0;
std::atomic<uint64_t> g_flashlightStaleFallbacks = 0;
std::atomic<uint64_t> g_flashlightMathFallbacks = 0;
std::mutex g_flashlightPoseMutex;
FlashlightPoseCache g_flashlightPoseCache;
std::atomic<uint64_t> g_flashlightGameplayRayCalls = 0;
std::atomic<uint64_t> g_flashlightGameplayRayCandidates = 0;
std::atomic<uint64_t> g_flashlightGameplayRayRedirects = 0;
std::atomic<uint64_t> g_flashlightGameplayRayHits = 0;
std::atomic<uint64_t> g_flashlightGameplayRayOriginFallbacks = 0;
std::atomic<uint64_t> g_flashlightGameplayRayPoseFallbacks = 0;
std::atomic<uint64_t> g_flashlightGameplayRayStaleFallbacks = 0;
std::atomic<uint64_t> g_flashlightGameplayRayMathFallbacks = 0;
std::atomic<uint64_t> g_controllerBeamRayQueries = 0;
std::atomic<uint64_t> g_controllerBeamRayHits = 0;
std::atomic<uint64_t> g_matrixReadFailures = 0;
std::atomic<uint64_t> g_quarterScaleSamples = 0;
std::atomic<uint64_t> g_fullScaleSamples = 0;
std::atomic<uint64_t> g_otherScaleSamples = 0;
std::atomic<uint64_t> g_trackedGripSamples = 0;
std::atomic<uint64_t> g_authoredCameraSamples = 0;
std::atomic<uint64_t> g_rootOverrideAttempts = 0;
std::atomic<uint64_t> g_rootOverrides = 0;
std::atomic<uint64_t> g_rootScaleFallbacks = 0;
std::atomic<uint64_t> g_rootStateFallbacks = 0;
std::atomic<uint64_t> g_rootAuthoredFallbacks = 0;
std::atomic<uint64_t> g_rootPoseFallbacks = 0;
std::atomic<uint64_t> g_rootStaleFallbacks = 0;
std::atomic<uint64_t> g_rootMathFallbacks = 0;
std::atomic<uint64_t> g_readCandidateCalls = 0;
std::atomic<uint64_t> g_readCandidateUnique = 0;
std::atomic<uint64_t> g_readPresentationOverrides = 0;
std::atomic<uint64_t> g_readPresentationFallbacks = 0;
std::atomic<uint64_t> g_socketedPropAnchorSeeds = 0;
std::atomic<uint64_t> g_socketedPropOverrides = 0;
std::atomic<uint64_t> g_socketedPropFallbacks = 0;
std::atomic<uint64_t> g_skeletonProbeSamples = 0;
std::atomic<uint64_t> g_skeletonMeshChanges = 0;
std::atomic<uint64_t> g_skeletonBonesFound = 0;
std::atomic<uint64_t> g_skeletonBonesMissing = 0;
std::atomic<uint64_t> g_skeletonBoneReadFailures = 0;
std::atomic<uint64_t> g_skeletonStateChanges = 0;
std::atomic<uint64_t> g_skeletonPostCandidates = 0;
std::atomic<uint64_t> g_skeletonPostCandidateFailures = 0;
std::atomic<uint64_t> g_handScaleAttempts = 0;
std::atomic<uint64_t> g_handScaleNormalizations = 0;
std::atomic<uint64_t> g_handScaleNativeFull = 0;
std::atomic<uint64_t> g_handScaleFallbacks = 0;
std::atomic<uint64_t> g_handShoulderOffsets = 0;
std::atomic<uint64_t> g_bodyAnchorScaleRepairs = 0;
std::atomic<uint64_t> g_wristFrameAttempts = 0;
std::atomic<uint64_t> g_wristFramesApplied = 0;
std::atomic<uint64_t> g_wristApplications = 0;
std::atomic<uint64_t> g_wristRotationAnchorSeeds = 0;
std::atomic<uint64_t> g_wristRotationApplications = 0;
std::atomic<uint64_t> g_wristRotationFallbacks = 0;
std::atomic<uint64_t> g_wristGeometricAnchorSeeds = 0;
std::atomic<uint64_t> g_wristLegacyAnchorSeeds = 0;
std::atomic<uint64_t> g_wristControllerBasisFallbacks = 0;
std::atomic<uint64_t> g_wristNativeBasisNonFiniteFallbacks = 0;
std::atomic<uint64_t> g_wristNativeBasisOrthogonalityFallbacks = 0;
std::atomic<uint64_t> g_wristNativeBasisHandednessFallbacks = 0;
std::atomic<uint64_t> g_wristNativeQuaternionFallbacks = 0;
std::atomic<uint64_t> g_wristReadFallbacks = 0;
std::atomic<uint64_t> g_wristHierarchyFallbacks = 0;
std::atomic<uint64_t> g_wristAuthoredPostFallbacks = 0;
std::atomic<uint64_t> g_wristMathFallbacks = 0;
std::atomic<uint64_t> g_wristStateFallbacks = 0;
std::atomic<uint64_t> g_wristPoseFallbacks = 0;
std::atomic<uint64_t> g_wristStaleFallbacks = 0;
std::atomic<uint64_t> g_armIkFrameAttempts = 0;
std::atomic<uint64_t> g_armIkFramesApplied = 0;
std::atomic<uint64_t> g_armIkApplications = 0;
std::atomic<uint64_t> g_armIkBaseRestores = 0;
std::atomic<uint64_t> g_armPoseSeeds = 0;
std::atomic<uint64_t> g_armRootPoseSeeds = 0;
std::atomic<uint64_t> g_armRootPoseAuthoredSeedCorrections = 0;
std::atomic<uint64_t> g_armRootPoseRestores = 0;
std::atomic<uint64_t> g_armRootPoseDriftCorrections = 0;
std::atomic<uint64_t> g_armRootPoseFallbacks = 0;
std::atomic<uint64_t> g_armIkReachClamps = 0;
std::atomic<uint64_t> g_armIkReadFallbacks = 0;
std::atomic<uint64_t> g_armIkHierarchyFallbacks = 0;
std::atomic<uint64_t> g_armIkAuthoredPostFallbacks = 0;
std::atomic<uint64_t> g_armIkMathFallbacks = 0;
std::atomic<uint64_t> g_armIkErgonomicFallbacks = 0;
std::atomic<uint64_t> g_armIkShoulderCompensations = 0;
std::atomic<uint64_t> g_armIkElbowHistoryUses = 0;
std::atomic<uint64_t> g_armIkElbowSingularityBlends = 0;
std::atomic<uint64_t> g_armIkElbowSwivelLimits = 0;
std::mutex g_retainedHandsMutex;
RetainedHandsState g_retainedHands;
std::atomic<uint64_t> g_handsActiveCalls = 0;
std::atomic<uint64_t> g_handsActiveSuppressions = 0;
std::atomic<uint64_t> g_handsRetainedFrames = 0;
std::atomic<uint64_t> g_handsRetainedFallbacks = 0;
std::atomic<uint64_t> g_handsRetainedInvalidations = 0;
std::atomic<uint64_t> g_handsNativeSeeds = 0;
std::atomic<uint64_t> g_handsWakeRequests = 0;
std::mutex g_armIkMutationMutex;
void* g_lastArmIkMutationEntity = nullptr;
uint64_t g_lastArmIkMutationFrame = UINT64_MAX;
std::mutex g_wristMutationMutex;
void* g_lastWristMutationEntity = nullptr;
uint64_t g_lastWristMutationFrame = UINT64_MAX;
std::array<WristOrientationAnchor, 2> g_wristOrientationAnchors{};
std::array<ArmPoseAnchor, 2> g_armPoseAnchors{};
SharedArmRootPoseAnchor g_sharedArmRootPoseAnchor{};
std::array<ArmErgonomicState, 2> g_armErgonomicStates{};
void* g_lastSkeletonEntity = nullptr;
void* g_lastSkeletonMesh = nullptr;
uint64_t g_nextSkeletonProbeFrame = 0;
uint64_t g_lastSkeletonProbeFrame = UINT64_MAX;
int g_lastSkeletonPlayerState = -1;
uint32_t g_skeletonProbeBurstRemaining = 0;

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

bool ReadMemory(const void* source, void* destination, size_t bytes)
{
    if (source == nullptr || destination == nullptr || bytes == 0) return false;
    SIZE_T bytesRead = 0;
    return ReadProcessMemory(
        GetCurrentProcess(),
        source,
        destination,
        bytes,
        &bytesRead) != FALSE
        && bytesRead == bytes;
}

bool ReadNativeString(const void* nativeString, std::string& value)
{
    value.clear();
    NativeStringLayout layout;
    if (!ReadMemory(nativeString, &layout, sizeof(layout))
        || layout.size > kMaxNativeNameLength
        || layout.capacity < layout.size
        || layout.capacity > 1024 * 1024) {
        return false;
    }

    const char* source = nullptr;
    if (layout.capacity <= kNativeStringInlineCapacity) {
        source = reinterpret_cast<const char*>(layout.storage.data());
    } else {
        std::memcpy(&source, layout.storage.data(), sizeof(source));
    }
    if (source == nullptr && layout.size != 0) return false;

    value.resize(static_cast<size_t>(layout.size));
    return layout.size == 0 || ReadMemory(source, value.data(), value.size());
}

void CountWristNativeBasisFallback(camera_math::RotationBasisValidation validation)
{
    using camera_math::RotationBasisValidation;
    switch (validation) {
    case RotationBasisValidation::NonFinite:
    case RotationBasisValidation::DegenerateColumn:
        g_wristNativeBasisNonFiniteFallbacks.fetch_add(1, std::memory_order_relaxed);
        break;
    case RotationBasisValidation::NonOrthogonal:
        g_wristNativeBasisOrthogonalityFallbacks.fetch_add(1, std::memory_order_relaxed);
        break;
    case RotationBasisValidation::ImproperHandedness:
        g_wristNativeBasisHandednessFallbacks.fetch_add(1, std::memory_order_relaxed);
        break;
    case RotationBasisValidation::QuaternionFailure:
        g_wristNativeQuaternionFallbacks.fetch_add(1, std::memory_order_relaxed);
        break;
    case RotationBasisValidation::Valid:
        break;
    }
}

NativeStringLayout MakeInlineNativeString(const char* value)
{
    NativeStringLayout nativeString;
    const size_t length = std::min(std::strlen(value), kNativeStringInlineCapacity);
    std::memcpy(nativeString.storage.data(), value, length);
    nativeString.size = static_cast<uint64_t>(length);
    nativeString.capacity = kNativeStringInlineCapacity;
    return nativeString;
}

void ProbePlayerHandsSkeleton(void* entity)
{
    if (!g_config.hplHandTrackingProbe || g_getMeshEntity == nullptr || g_getBoneStateFromName == nullptr)
        return;

    HPLPlayerStateSnapshot player{};
    GetHPLPlayerStateSnapshot(player);
    void* meshEntity = g_getMeshEntity(entity);
    const uint64_t interval = static_cast<uint64_t>(std::max(g_config.hplControllerLogInterval, 1));
    bool meshChanged = false;
    bool stateChanged = false;
    uint32_t burstRemaining = 0;
    {
        std::lock_guard lock(g_skeletonProbeMutex);
        meshChanged = entity != g_lastSkeletonEntity || meshEntity != g_lastSkeletonMesh;
        stateChanged = player.playerStateId != g_lastSkeletonPlayerState;
        if (player.frame == g_lastSkeletonProbeFrame && !meshChanged)
            return;
        if (meshChanged || stateChanged) {
            g_skeletonProbeBurstRemaining = kSkeletonProbeBurstFrames;
        }
        if (!meshChanged
            && !stateChanged
            && g_skeletonProbeBurstRemaining == 0
            && player.frame < g_nextSkeletonProbeFrame) {
            return;
        }
        g_lastSkeletonEntity = entity;
        g_lastSkeletonMesh = meshEntity;
        g_lastSkeletonPlayerState = player.playerStateId;
        g_lastSkeletonProbeFrame = player.frame;
        if (g_skeletonProbeBurstRemaining > 0) {
            --g_skeletonProbeBurstRemaining;
            g_nextSkeletonProbeFrame = player.frame + 1;
        } else {
            g_nextSkeletonProbeFrame = player.frame + interval;
        }
        burstRemaining = g_skeletonProbeBurstRemaining;
    }

    const uint64_t sample = g_skeletonProbeSamples.fetch_add(1, std::memory_order_relaxed) + 1;
    if (meshChanged)
        g_skeletonMeshChanges.fetch_add(1, std::memory_order_relaxed);
    if (stateChanged)
        g_skeletonStateChanges.fetch_add(1, std::memory_order_relaxed);
    Logger::Instance().Write(
        meshEntity != nullptr ? LogLevel::Info : LogLevel::Warn,
        "hpl_hands_skeleton sample=%llu frame=%llu entity=%p mesh=%p meshChanged=%d "
        "stateChanged=%d burstRemaining=%u playerState=%d playerStateName=%s "
        "policy=passive_bilateral_transform_burst",
        static_cast<unsigned long long>(sample),
        static_cast<unsigned long long>(player.frame),
        entity,
        meshEntity,
        meshChanged ? 1 : 0,
        stateChanged ? 1 : 0,
        burstRemaining,
        player.playerStateId,
        HPLPlayerStateName(player.playerStateId));
    if (meshEntity == nullptr)
        return;

    std::array<HPLTrackedPoseWorld, 2> controllerGripWorld{};
    std::array<bool, 2> controllerGripValid{};
    uint64_t controllerInputFrame = 0;
    if (g_openxr != nullptr) {
        OpenXRInputSnapshot input{};
        if (g_openxr->GetLatestInput(input) && input.active) {
            controllerInputFrame = input.gameFrame;
            const OpenXRHandInput* hands[] = {&input.left, &input.right};
            for (uint32_t handIndex = 0; handIndex < 2; ++handIndex) {
                controllerGripValid[handIndex] = hands[handIndex]->gripPose.valid
                    && ResolveHPLTrackedPoseWorld(
                        hands[handIndex]->gripPose,
                        input.gameFrame,
                        controllerGripWorld[handIndex])
                    && controllerGripWorld[handIndex].positionTracked
                    && controllerGripWorld[handIndex].orientationTracked;
            }
        }
    }

    std::array<void*, 2> expectedWristParents{};
    constexpr const char* expectedWristParentNames[] = {
        "j_L_Arm_10",
        "j_R_Arm_10",
    };
    for (size_t handIndex = 0; handIndex < expectedWristParents.size(); ++handIndex) {
        NativeStringLayout nativeName = MakeInlineNativeString(
            expectedWristParentNames[handIndex]);
        expectedWristParents[handIndex] = g_getBoneStateFromName(meshEntity, &nativeName);
    }

    constexpr const char* boneNames[] = {
        "j_L_Wrist",
        "j_R_Wrist",
        "Socket_L_Hand",
        "Socket_R_Hand",
        "Socket_Camera",
    };
    for (const char* boneName : boneNames)
    {
        NativeStringLayout nativeName = MakeInlineNativeString(boneName);
        void* boneState = g_getBoneStateFromName(meshEntity, &nativeName);
        if (boneState == nullptr)
        {
            g_skeletonBonesMissing.fetch_add(1, std::memory_order_relaxed);
            Logger::Instance().Write(
                LogLevel::Warn,
                "hpl_hands_bone sample=%llu name=%s found=0",
                static_cast<unsigned long long>(sample),
                boneName);
            continue;
        }

        std::array<float, 16> localMatrix{};
        std::array<float, 16> worldMatrix{};
        std::array<float, 16> postMatrix{};
        std::array<float, 16> parentWorldMatrix{};
        void* parent = nullptr;
        uint8_t usePreTransform = 0;
        uint8_t usePostTransform = 0;
        const bool localMatrixValid = ReadMemory(
            static_cast<const std::byte*>(boneState) + kNodeLocalMatrixOffset,
            localMatrix.data(),
            sizeof(localMatrix));
        const bool worldMatrixValid = ReadMemory(
            static_cast<const std::byte*>(boneState) + kNodeWorldMatrixOffset,
            worldMatrix.data(),
            sizeof(worldMatrix));
        const bool postMatrixValid = ReadMemory(
            static_cast<const std::byte*>(boneState) + kNodePostTransformOffset,
            postMatrix.data(),
            sizeof(postMatrix));
        const bool parentValid = ReadMemory(
            static_cast<const std::byte*>(boneState) + kNodeParentOffset,
            &parent,
            sizeof(parent))
            && parent != nullptr;
        const bool parentWorldValid = parentValid
            && ReadMemory(
                static_cast<const std::byte*>(parent) + kNodeWorldMatrixOffset,
                parentWorldMatrix.data(),
                sizeof(parentWorldMatrix));
        const bool flagsValid =
            ReadMemory(
                static_cast<const std::byte*>(boneState) + kNodeUsePreTransformOffset,
                &usePreTransform,
                sizeof(usePreTransform))
            && ReadMemory(
                static_cast<const std::byte*>(boneState) + kNodeUsePostTransformOffset,
                &usePostTransform,
                sizeof(usePostTransform));
        const bool matricesValid = localMatrixValid && worldMatrixValid
            && postMatrixValid && parentWorldValid;
        if (!matricesValid || !flagsValid)
            g_skeletonBoneReadFailures.fetch_add(1, std::memory_order_relaxed);
        else
            g_skeletonBonesFound.fetch_add(1, std::memory_order_relaxed);
        int controllerHand = -1;
        if (std::strcmp(boneName, "j_L_Wrist") == 0) controllerHand = 0;
        else if (std::strcmp(boneName, "j_R_Wrist") == 0) controllerHand = 1;
        const bool controllerTargetValid = controllerHand >= 0
            && controllerGripValid[static_cast<size_t>(controllerHand)];
        const HPLTrackedPoseWorld controllerTarget = controllerTargetValid
            ? controllerGripWorld[static_cast<size_t>(controllerHand)]
            : HPLTrackedPoseWorld{};
        const float targetDx = worldMatrixValid && controllerTargetValid
            ? controllerTarget.positionX - worldMatrix[3] : 0.0f;
        const float targetDy = worldMatrixValid && controllerTargetValid
            ? controllerTarget.positionY - worldMatrix[7] : 0.0f;
        const float targetDz = worldMatrixValid && controllerTargetValid
            ? controllerTarget.positionZ - worldMatrix[11] : 0.0f;
        const float targetDistance = worldMatrixValid && controllerTargetValid
            ? std::sqrt(targetDx * targetDx + targetDy * targetDy + targetDz * targetDz)
            : -1.0f;
        const bool parentMatchesExpected = controllerHand >= 0
            && parent == expectedWristParents[static_cast<size_t>(controllerHand)];
        std::array<float, 16> postCandidate{};
        float postCandidateWorldError = -1.0f;
        bool postCandidateValid = false;
        if (controllerTargetValid && matricesValid && usePostTransform == 0) {
            std::array<float, 16> desiredWorld = worldMatrix;
            desiredWorld[3] = controllerTarget.positionX;
            desiredWorld[7] = controllerTarget.positionY;
            desiredWorld[11] = controllerTarget.positionZ;
            postCandidateValid = hands_math::BuildPostTransformForWorldTarget(
                parentWorldMatrix, localMatrix, desiredWorld, postCandidate);
            if (postCandidateValid) {
                const std::array<float, 16> reconstructedWorld =
                    camera_math::MatrixMultiply(
                        camera_math::MatrixMultiply(parentWorldMatrix, postCandidate),
                        localMatrix);
                const float errorX = reconstructedWorld[3] - desiredWorld[3];
                const float errorY = reconstructedWorld[7] - desiredWorld[7];
                const float errorZ = reconstructedWorld[11] - desiredWorld[11];
                postCandidateWorldError =
                    std::sqrt(errorX * errorX + errorY * errorY + errorZ * errorZ);
                g_skeletonPostCandidates.fetch_add(1, std::memory_order_relaxed);
            } else {
                g_skeletonPostCandidateFailures.fetch_add(1, std::memory_order_relaxed);
            }
        }
        Logger::Instance().Write(
            matricesValid && flagsValid ? LogLevel::Info : LogLevel::Warn,
            "hpl_hands_bone sample=%llu frame=%llu name=%s found=1 bone=%p parent=%p "
            "matricesValid=%d flagsValid=%d parentExpected=%p parentMatch=%d "
            "localPos=%.4f,%.4f,%.4f parentWorldPos=%.4f,%.4f,%.4f "
            "worldPos=%.4f,%.4f,%.4f worldBasis={right=%.4f,%.4f,%.4f up=%.4f,%.4f,%.4f forward=%.4f,%.4f,%.4f} "
            "usePre=%d usePost=%d authoredPostPos=%.4f,%.4f,%.4f controllerHand=%s "
            "controllerTargetValid=%d controllerInputFrame=%llu targetPos=%.4f,%.4f,%.4f "
            "targetBasis={forward=%.4f,%.4f,%.4f up=%.4f,%.4f,%.4f} "
            "targetDelta=%.4f,%.4f,%.4f targetDistance=%.4f "
            "postCandidate={valid=%d translation=%.4f,%.4f,%.4f worldError=%.7f mode=position_only_uncalibrated} "
            "policy=passive_bilateral_wrist_post_candidate_probe",
            static_cast<unsigned long long>(sample),
            static_cast<unsigned long long>(player.frame),
            boneName,
            boneState,
            parent,
            matricesValid ? 1 : 0,
            flagsValid ? 1 : 0,
            controllerHand >= 0
                ? expectedWristParents[static_cast<size_t>(controllerHand)] : nullptr,
            parentMatchesExpected ? 1 : 0,
            localMatrixValid ? localMatrix[3] : 0.0f,
            localMatrixValid ? localMatrix[7] : 0.0f,
            localMatrixValid ? localMatrix[11] : 0.0f,
            parentWorldValid ? parentWorldMatrix[3] : 0.0f,
            parentWorldValid ? parentWorldMatrix[7] : 0.0f,
            parentWorldValid ? parentWorldMatrix[11] : 0.0f,
            worldMatrixValid ? worldMatrix[3] : 0.0f,
            worldMatrixValid ? worldMatrix[7] : 0.0f,
            worldMatrixValid ? worldMatrix[11] : 0.0f,
            worldMatrixValid ? worldMatrix[0] : 0.0f,
            worldMatrixValid ? worldMatrix[4] : 0.0f,
            worldMatrixValid ? worldMatrix[8] : 0.0f,
            worldMatrixValid ? worldMatrix[1] : 0.0f,
            worldMatrixValid ? worldMatrix[5] : 0.0f,
            worldMatrixValid ? worldMatrix[9] : 0.0f,
            worldMatrixValid ? worldMatrix[2] : 0.0f,
            worldMatrixValid ? worldMatrix[6] : 0.0f,
            worldMatrixValid ? worldMatrix[10] : 0.0f,
            usePreTransform != 0 ? 1 : 0,
            usePostTransform != 0 ? 1 : 0,
            postMatrixValid ? postMatrix[3] : 0.0f,
            postMatrixValid ? postMatrix[7] : 0.0f,
            postMatrixValid ? postMatrix[11] : 0.0f,
            controllerHand == 0 ? "left" : (controllerHand == 1 ? "right" : "none"),
            controllerTargetValid ? 1 : 0,
            static_cast<unsigned long long>(controllerInputFrame),
            controllerTarget.positionX,
            controllerTarget.positionY,
            controllerTarget.positionZ,
            controllerTarget.forwardX,
            controllerTarget.forwardY,
            controllerTarget.forwardZ,
            controllerTarget.upX,
            controllerTarget.upY,
            controllerTarget.upZ,
            targetDx,
            targetDy,
            targetDz,
            targetDistance,
            postCandidateValid ? 1 : 0,
            postCandidateValid ? postCandidate[3] : 0.0f,
            postCandidateValid ? postCandidate[7] : 0.0f,
            postCandidateValid ? postCandidate[11] : 0.0f,
            postCandidateWorldError);
    }
}

bool StartsWithPlayerHands(const std::string& name)
{
    constexpr char prefix[] = "PlayerHands_";
    return name.size() >= sizeof(prefix) - 1
        && std::memcmp(name.data(), prefix, sizeof(prefix) - 1) == 0;
}

bool EndsWithHudObject(const std::string& name)
{
    constexpr char suffix[] = "_HudObject";
    return name.size() >= sizeof(suffix) - 1
        && std::memcmp(
            name.data() + name.size() - (sizeof(suffix) - 1),
            suffix,
            sizeof(suffix) - 1) == 0;
}

bool IsReadPresentationCandidate(const EntityIdentity& identity);

EntityIdentity ResolveIdentity(void* entity)
{
    {
        std::lock_guard lock(g_identityMutex);
        const auto found = g_identityCache.find(entity);
        if (found != g_identityCache.end()) return found->second;
    }

    EntityIdentity identity;
    const void* nativeName = g_getEntityName != nullptr ? g_getEntityName(entity) : nullptr;
    g_identityReads.fetch_add(1, std::memory_order_relaxed);
    if (!ReadNativeString(nativeName, identity.name)) {
        g_identityReadFailures.fetch_add(1, std::memory_order_relaxed);
    } else {
        identity.playerHands = StartsWithPlayerHands(identity.name);
        identity.hudObject = identity.name == "HudObject";
        identity.socketedHudObject = EndsWithHudObject(identity.name);
        identity.flashlight = identity.name == "Flashlight";
        identity.readObject = IsReadPresentationCandidate(identity);
        const bool calibrationRelevant = identity.playerHands || identity.hudObject
            || identity.socketedHudObject || identity.flashlight || identity.readObject;
        if (calibrationRelevant) {
            entity_calibration::Capabilities capabilities;
            capabilities.playerHands = identity.playerHands;
            capabilities.hudObject = identity.hudObject;
            capabilities.socketedHudObject = identity.socketedHudObject;
            capabilities.flashlight = identity.flashlight;
            capabilities.readObject = identity.readObject;
            identity.calibrationProfile = g_calibrationProfiles.Resolve(
                identity.name, capabilities);
        }
        if (identity.playerHands) {
            g_playerHandsIdentities.fetch_add(1, std::memory_order_relaxed);
        }
        if (identity.flashlight) {
            g_flashlightIdentities.fetch_add(1, std::memory_order_relaxed);
        }
        if (identity.hudObject) {
            g_hudObjectIdentities.fetch_add(1, std::memory_order_relaxed);
        }
        if (identity.socketedHudObject) {
            g_socketedHudObjectIdentities.fetch_add(1, std::memory_order_relaxed);
        }
        const uint64_t logIndex = g_identityLogs.fetch_add(1, std::memory_order_relaxed);
        const bool relevant = identity.playerHands || identity.hudObject
            || identity.socketedHudObject || identity.flashlight || identity.readObject;
        if (logIndex < kMaxIdentityLogs || relevant) {
            Logger::Instance().Write(
                relevant ? LogLevel::Warn : LogLevel::Info,
                "hpl_entity_identity entity=%p name=%s playerHands=%d hudObject=%d socketedHudObject=%d flashlight=%d readObject=%d profile={valid=%d id=%llu family=%s loaded=%d key=%s} nameObject=%p",
                entity,
                identity.name.c_str(),
                identity.playerHands ? 1 : 0,
                identity.hudObject ? 1 : 0,
                identity.socketedHudObject ? 1 : 0,
                identity.flashlight ? 1 : 0,
                identity.readObject ? 1 : 0,
                identity.calibrationProfile.valid ? 1 : 0,
                static_cast<unsigned long long>(identity.calibrationProfile.id),
                identity.calibrationProfile.valid
                    ? entity_calibration::Store::FamilyName(
                        identity.calibrationProfile.family) : "none",
                identity.calibrationProfile.loadedFromDisk ? 1 : 0,
                identity.calibrationProfile.valid
                    ? identity.calibrationProfile.key.c_str() : "none",
                nativeName);
        }
    }

    std::lock_guard lock(g_identityMutex);
    if (g_identityCache.size() < kMaxIdentityCache || identity.playerHands
        || identity.hudObject || identity.socketedHudObject || identity.flashlight
        || identity.readObject) {
        g_identityCache[entity] = identity;
    }
    return identity;
}

float ColumnLength(const std::array<float, 16>& matrix, size_t column)
{
    return std::sqrt(
        matrix[column] * matrix[column]
        + matrix[column + 4] * matrix[column + 4]
        + matrix[column + 8] * matrix[column + 8]);
}

const OpenXRHandInput* SelectDominantHand(
    const OpenXRInputSnapshot& input,
    uint32_t& handIndex,
    bool useInteractionOwner = true)
{
    handIndex = g_config.hplControllerDominantHand == "left" ? 0u : 1u;
    uint32_t interactionHand = handIndex;
    if (useInteractionOwner
        && GetHPLInteractionOwnerHand(input.gameFrame, 120, interactionHand)) {
        const OpenXRHandInput* owner = interactionHand == 0 ? &input.left : &input.right;
        if (owner->active) {
            handIndex = interactionHand;
            return owner;
        }
    }
    const OpenXRHandInput* preferred = handIndex == 0 ? &input.left : &input.right;
    if (preferred->active) return preferred;
    if (!g_config.hplControllerOneHandFallback) return nullptr;
    handIndex ^= 1u;
    const OpenXRHandInput* fallback = handIndex == 0 ? &input.left : &input.right;
    return fallback->active ? fallback : nullptr;
}

bool IsTrackedHandsState(const HPLPlayerStateSnapshot& player)
{
    return (player.playerStateId >= static_cast<int>(HPLPlayerStateKind::Normal)
            && player.playerStateId <= static_cast<int>(HPLPlayerStateKind::Tear))
        || player.playerStateId == static_cast<int>(HPLPlayerStateKind::Terminal)
        || player.playerStateId == static_cast<int>(HPLPlayerStateKind::Read)
        || player.playerStateId == static_cast<int>(HPLPlayerStateKind::MovingButton);
}

bool IsHandScaleEligible(
    bool playerSnapshotValid,
    const HPLPlayerStateSnapshot& player,
    const HPLCameraBridgeStatus& camera)
{
    bool paused = false;
    return !g_config.hplHandControllerRoot
        && playerSnapshotValid
        && player.playerValid
        && IsTrackedHandsState(player)
        && player.moveStateId == kNormalMoveState
        && !player.authoredCameraActive
        && GetHPLGamePausedState(paused)
        && !paused
        && camera.trackingEnabled;
}

bool ResolveWristTrackingFrame(
    bool playerSnapshotValid,
    const HPLPlayerStateSnapshot& player,
    const HPLCameraBridgeStatus& camera,
    WristTrackingFrame& frame)
{
    frame = {};
    frame.playerFrame = player.frame;
    frame.camera = camera;
    bool paused = false;
    if (g_config.hplHandControllerRoot
        || !playerSnapshotValid
        || !player.playerValid
        || !IsTrackedHandsState(player)
        || player.moveStateId != kNormalMoveState
        || player.authoredCameraActive
        || !GetHPLGamePausedState(paused)
        || paused) {
        g_wristStateFallbacks.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    if (g_openxr == nullptr || !camera.trackingEnabled) {
        g_wristPoseFallbacks.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    OpenXRInputSnapshot input{};
    if (!g_openxr->GetLatestInput(input) || !input.active) {
        g_wristPoseFallbacks.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    frame.inputFrame = input.gameFrame;
    if (player.frame != 0 && input.gameFrame != 0) {
        frame.inputAge = player.frame >= input.gameFrame
            ? player.frame - input.gameFrame
            : 0;
    }
    if (frame.inputAge > static_cast<uint64_t>(g_config.hplControllerMaxInputAgeFrames)) {
        g_wristStaleFallbacks.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    frame.stateEligible = true;
    const OpenXRHandInput* hands[] = {&input.left, &input.right};
    for (size_t handIndex = 0; handIndex < frame.targets.size(); ++handIndex) {
        const OpenXRHandInput& hand = *hands[handIndex];
        if (!hand.active
            || !hand.gripPose.valid
            || !ResolveHPLTrackedPoseWorld(
                hand.gripPose,
                input.gameFrame,
                frame.targets[handIndex])
            || !frame.targets[handIndex].positionTracked
            || !frame.targets[handIndex].orientationTracked) {
            g_wristPoseFallbacks.fetch_add(1, std::memory_order_relaxed);
            continue;
        }
        camera_math::Vector3 calibrationForward{
            frame.targets[handIndex].forwardX, 0.0f, frame.targets[handIndex].forwardZ};
        if (camera.headSceneOrientationValid) {
            calibrationForward = camera_math::RotateVector(
                camera_math::YawOnly(camera.headSceneOrientation), {0.0f, 0.0f, -1.0f});
        }
        const camera_math::Vector3 offsets = g_config.hplHandWristPosition
            ? camera_math::Vector3{g_config.hplHandWristOutwardOffsetMeters,
                g_config.hplHandWristVerticalOffsetMeters, g_config.hplHandWristViewForwardOffsetMeters}
            : camera_math::Vector3{};
        frame.eligible[handIndex] = hands_math::BuildCalibratedWristGoal(
            {frame.targets[handIndex].positionX, frame.targets[handIndex].positionY,
                frame.targets[handIndex].positionZ},
            calibrationForward, offsets, camera.worldUnitsPerMeter, handIndex == 0,
            frame.goals[handIndex]);
        if (!frame.eligible[handIndex]) g_wristMathFallbacks.fetch_add(1, std::memory_order_relaxed);
    }
    return frame.eligible[0] || frame.eligible[1];
}

RetainedHandsMode ResolveRetainedHandsMode()
{
    if (!g_config.hplHandAlwaysVisible) return RetainedHandsMode::Invalid;
    HPLPlayerStateSnapshot player;
    const HPLCameraBridgeStatus camera = GetHPLCameraBridgeStatus();
    bool paused = false;
    if (!GetHPLPlayerStateSnapshot(player)
        || !player.playerValid
        || !player.cameraControlValid
        || !player.characterBodyCameraValid) {
        return RetainedHandsMode::Invalid;
    }
    if (!GetHPLGamePausedState(paused)
        || paused
        || !IsTrackedHandsState(player)
        || player.moveStateId != kNormalMoveState
        || player.authoredCameraActive
        || !camera.trackingEnabled) {
        return RetainedHandsMode::Suspended;
    }
    return RetainedHandsMode::Active;
}

bool ShouldSuppressNativeHandsHide()
{
    return ResolveRetainedHandsMode() != RetainedHandsMode::Invalid
        && GetHPLCameraBridgeStatus().trackingEnabled;
}

void ResetWristOrientationAnchors(void* entity = nullptr)
{
    std::lock_guard lock(g_wristMutationMutex);
    for (WristOrientationAnchor& anchor : g_wristOrientationAnchors) {
        if (entity == nullptr || anchor.entity == entity) anchor = {};
    }
}

void ResetArmPoseAnchors(void* entity = nullptr)
{
    std::lock_guard lock(g_armIkMutationMutex);
    if (entity == nullptr || g_sharedArmRootPoseAnchor.entity == entity) {
        g_sharedArmRootPoseAnchor = {};
    }
    for (size_t index = 0; index < g_armPoseAnchors.size(); ++index) {
        if (entity == nullptr || g_armPoseAnchors[index].entity == entity) {
            g_armPoseAnchors[index] = {};
            g_armErgonomicStates[index] = {};
        }
    }
}

void RestoreArmPoseAnchors(void* entity)
{
    std::array<ArmPoseAnchor, 2> anchors{};
    SharedArmRootPoseAnchor rootAnchor{};
    {
        std::lock_guard lock(g_armIkMutationMutex);
        if (entity == nullptr || g_sharedArmRootPoseAnchor.entity == entity) {
            rootAnchor = g_sharedArmRootPoseAnchor;
        }
        for (size_t index = 0; index < g_armPoseAnchors.size(); ++index) {
            if (entity == nullptr || g_armPoseAnchors[index].entity == entity) {
                anchors[index] = g_armPoseAnchors[index];
            }
        }
    }
    if (g_nodeSetMatrix == nullptr) return;
    if (rootAnchor.valid) {
        g_nodeSetMatrix(rootAnchor.node, rootAnchor.localMatrix.data(), true);
    }
    for (const ArmPoseAnchor& anchor : anchors) {
        if (!anchor.valid) continue;
        const size_t restoreCount = g_config.hplHandFreezePose
            ? anchor.nodes.size() : kArmPoseWristIndex + 1;
        for (size_t index = 0; index < restoreCount; ++index) {
            g_nodeSetMatrix(
                anchor.nodes[index], anchor.localMatrices[index].data(), true);
        }
    }
}

bool ResolveHandsBodyYaw(
    const HPLCameraBridgeStatus& camera,
    camera_math::Quaternion& bodyYaw)
{
    if (GetHPLVirtualTorsoYaw(bodyYaw)) return true;
    if (camera.nativeCameraBasisValid) {
        camera_math::Quaternion nativeCameraOrientation{};
        if (camera_math::QuaternionFromForwardUp(
                {camera.nativeCameraForwardX, 0.0f, camera.nativeCameraForwardZ},
                {0.0f, 1.0f, 0.0f},
                nativeCameraOrientation)) {
            bodyYaw = camera_math::YawOnly(nativeCameraOrientation);
            return std::isfinite(bodyYaw.x) && std::isfinite(bodyYaw.y)
                && std::isfinite(bodyYaw.z) && std::isfinite(bodyYaw.w);
        }
    }
    return false;
}

bool ResolveShoulderAnchorPosition(
    const HPLCameraBridgeStatus& camera,
    camera_math::Vector3& position,
    bool& usesTrackedHead)
{
    usesTrackedHead = false;
    if (camera.headWorldPositionValid) {
        position = {
            camera.headWorldPositionX,
            camera.headWorldPositionY,
            camera.headWorldPositionZ,
        };
        usesTrackedHead = true;
        return true;
    }
    if (!camera.cameraWorldPositionValid) return false;
    position = {
        camera.cameraWorldPositionX,
        camera.cameraWorldPositionY,
        camera.cameraWorldPositionZ,
    };
    return true;
}

bool BuildBodyAnchoredHandsMatrix(
    const RetainedHandsState& retained,
    const HPLCameraBridgeStatus& camera,
    std::array<float, 16>& matrix,
    camera_math::Quaternion& bodyYaw,
    camera_math::Vector3& shoulderAnchorPosition,
    bool& usesTrackedHead)
{
    matrix = retained.matrix;
    if (!retained.shoulderAnchorPositionValid || !retained.bodyYawValid
        || !ResolveShoulderAnchorPosition(
            camera, shoulderAnchorPosition, usesTrackedHead)
        || !ResolveHandsBodyYaw(camera, bodyYaw)) {
        return false;
    }
    return hands_math::BuildBodyAnchoredRootMatrix(
        retained.matrix,
        retained.shoulderAnchorPosition,
        retained.bodyYaw,
        shoulderAnchorPosition,
        bodyYaw,
        matrix);
}

void ResetWristOrientationAnchor(void* entity, size_t handIndex)
{
    if (handIndex >= g_wristOrientationAnchors.size()) return;
    std::lock_guard lock(g_wristMutationMutex);
    WristOrientationAnchor& anchor = g_wristOrientationAnchors[handIndex];
    if (entity == nullptr || anchor.entity == entity) anchor = {};
}

void InvalidateRetainedHands(const char* reason)
{
    void* entity = nullptr;
    bool invalidated = false;
    {
        std::lock_guard lock(g_retainedHandsMutex);
        if (g_retainedHands.valid) {
            entity = g_retainedHands.entity;
            g_retainedHands = {};
            invalidated = true;
        }
    }
    if (!invalidated) return;
    ResetWristOrientationAnchors(entity);
    ResetArmPoseAnchors(entity);
    const uint64_t count = g_handsRetainedInvalidations.fetch_add(
        1, std::memory_order_relaxed) + 1;
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_hands_visibility invalidated=%llu entity=%p reason=%s policy=fail_closed_before_native_teardown",
        static_cast<unsigned long long>(count),
        entity,
        reason != nullptr ? reason : "unspecified");
}

bool HasRetainedPlayerHandsSeed(void* entity)
{
    std::lock_guard lock(g_retainedHandsMutex);
    return g_retainedHands.valid
        && g_retainedHands.entity == entity
        && g_retainedHands.mesh != nullptr;
}

void HookLuxEntitySetActive(void* entity, bool active)
{
    g_handsActiveCalls.fetch_add(1, std::memory_order_relaxed);
    // SetActive is called while Lux entities are still being constructed. Reading and
    // caching the name here can permanently store the pre-name identity and hide the
    // later PlayerHands_* SetMatrix call from the hand bridge.
    if (!active && HasRetainedPlayerHandsSeed(entity)) {
        if (ShouldSuppressNativeHandsHide()) {
            active = true;
            g_handsActiveSuppressions.fetch_add(1, std::memory_order_relaxed);
        } else if (ResolveRetainedHandsMode() == RetainedHandsMode::Invalid) {
            InvalidateRetainedHands("native_set_active_ineligible");
        }
    }
    g_originalSetActive(entity, active);
}

camera_math::Vector3 MatrixPosition(const std::array<float, 16>& matrix)
{
    return {matrix[3], matrix[7], matrix[11]};
}

float Distance(
    const camera_math::Vector3& left,
    const camera_math::Vector3& right)
{
    const float x = left.x - right.x;
    const float y = left.y - right.y;
    const float z = left.z - right.z;
    return std::sqrt(x*x + y*y + z*z);
}

bool ResolveReadPresentationView(
    const HPLCameraBridgeStatus& camera,
    camera_math::Vector3& position,
    camera_math::Vector3& forward,
    camera_math::Quaternion& orientation,
    bool& usesTrackedHead)
{
    usesTrackedHead = camera.headWorldPositionValid;
    if (usesTrackedHead) {
        position = {
            camera.headWorldPositionX,
            camera.headWorldPositionY,
            camera.headWorldPositionZ,
        };
    } else if (camera.cameraWorldPositionValid) {
        position = {
            camera.cameraWorldPositionX,
            camera.cameraWorldPositionY,
            camera.cameraWorldPositionZ,
        };
    } else {
        return false;
    }
    if (camera.headSceneOrientationValid) {
        orientation = camera.headSceneOrientation;
    } else if (camera.nativeCameraBasisValid
        && camera_math::QuaternionFromForwardUp(
            {camera.nativeCameraForwardX, camera.nativeCameraForwardY,
                camera.nativeCameraForwardZ},
            {camera.nativeCameraUpX, camera.nativeCameraUpY, camera.nativeCameraUpZ},
            orientation)) {
    } else {
        return false;
    }
    forward = camera_math::RotateVector(orientation, {0.0f, 0.0f, -1.0f});
    return std::isfinite(position.x) && std::isfinite(position.y)
        && std::isfinite(position.z) && std::isfinite(forward.x)
        && std::isfinite(forward.y) && std::isfinite(forward.z);
}

bool IsReadPresentationCandidate(const EntityIdentity& identity)
{
    if (identity.name.empty() || identity.playerHands || identity.hudObject
        || identity.socketedHudObject || identity.flashlight) {
        return false;
    }
    std::string lower = identity.name;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char value) {
        return static_cast<char>(std::tolower(value));
    });
    return lower.find("arm") == std::string::npos
        && lower.find("hand") == std::string::npos
        && lower.find("hudobject") == std::string::npos;
}

uint64_t UpdateReadPresentationSession(const HPLPlayerStateSnapshot& player)
{
    const bool readActive = player.playerValid
        && player.playerStateId == static_cast<int>(HPLPlayerStateKind::Read);
    bool transition = false;
    uint64_t session = 0;
    {
        std::lock_guard lock(g_identityMutex);
        if (readActive != g_readPresentationStateActive) {
            g_readPresentationStateActive = readActive;
            g_readPresentationAnchors.clear();
            g_readPresentationOwner = nullptr;
            transition = true;
            if (readActive) ++g_readPresentationSession;
        }
        session = g_readPresentationSession;
    }
    if (transition) {
        Logger::Instance().Write(
            LogLevel::Info,
            "hpl_read_session session=%llu frame=%llu active=%d policy=clear_latched_native_anchors_on_read_state_boundary",
            static_cast<unsigned long long>(session),
            static_cast<unsigned long long>(player.frame),
            readActive ? 1 : 0);
    }
    return session;
}

bool ApplyArmNodeToward(void* node, const camera_math::Vector3& currentEndpoint,
    const camera_math::Vector3& desiredEndpoint)
{
    if (node == nullptr) return false;
    std::array<float, 16> local{};
    std::array<float, 16> world{};
    std::array<float, 16> parentWorld{};
    std::array<float, 16> authoredPost{};
    void* parent = nullptr;
    uint8_t authoredUsePost = 0;
    if (!ReadMemory(static_cast<const std::byte*>(node) + kNodeLocalMatrixOffset,
            local.data(), sizeof(local))
        || !ReadMemory(static_cast<const std::byte*>(node) + kNodeWorldMatrixOffset,
            world.data(), sizeof(world))
        || !ReadMemory(static_cast<const std::byte*>(node) + kNodeParentOffset,
            &parent, sizeof(parent))
        || parent == nullptr
        || !ReadMemory(static_cast<const std::byte*>(parent) + kNodeWorldMatrixOffset,
            parentWorld.data(), sizeof(parentWorld))
        || !ReadMemory(static_cast<const std::byte*>(node) + kNodePostTransformOffset,
            authoredPost.data(), sizeof(authoredPost))
        || !ReadMemory(static_cast<const std::byte*>(node) + kNodeUsePostTransformOffset,
            &authoredUsePost, sizeof(authoredUsePost))) {
        g_armIkReadFallbacks.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    if (authoredUsePost != 0) {
        g_armIkAuthoredPostFallbacks.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    std::array<float, 16> desiredWorld{};
    std::array<float, 16> post{};
    if (!arm_ik_math::RotateWorldMatrixToward(
            world, currentEndpoint, desiredEndpoint,
            g_config.hplHandArmIKBlend, desiredWorld)
        || !hands_math::BuildPostTransformForWorldTarget(
            parentWorld, local, desiredWorld, post)) {
        g_armIkMathFallbacks.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    g_setPostTransform(node, post.data());
    g_setUsePostTransform(node, true);
    g_applyPostAnimTransform(node, true);
    g_setPostTransform(node, authoredPost.data());
    g_setUsePostTransform(node, false);
    return true;
}

bool ResolveArmPoseNodes(
    void* mesh,
    size_t handIndex,
    std::array<void*, kArmPoseNodeCount>& nodes)
{
    if (mesh == nullptr || handIndex >= 2) return false;
    const char* const* names = kArmPoseNodeNames[handIndex];
    for (size_t index = 0; index < nodes.size(); ++index) {
        NativeStringLayout name = MakeInlineNativeString(names[index]);
        nodes[index] = g_getBoneStateFromName(mesh, &name);
        if (nodes[index] == nullptr) return false;
    }
    return true;
}

bool RestoreOrSeedArmPose(
    void* entity,
    size_t handIndex,
    const std::array<void*, kArmPoseNodeCount>& nodes)
{
    if (handIndex >= g_armPoseAnchors.size() || g_nodeSetMatrix == nullptr) return false;
    for (void* node : nodes) {
        uint8_t usePost = 0;
        if (node == nullptr
            || !ReadMemory(static_cast<const std::byte*>(node) + kNodeUsePostTransformOffset,
                &usePost, sizeof(usePost))) {
            g_armIkReadFallbacks.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
        if (usePost != 0) {
            g_armIkAuthoredPostFallbacks.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
    }

    std::lock_guard lock(g_armIkMutationMutex);
    ArmPoseAnchor& anchor = g_armPoseAnchors[handIndex];
    const bool seed = !anchor.valid || anchor.entity != entity || anchor.nodes != nodes;
    if (seed) {
        anchor = {};
        anchor.entity = entity;
        anchor.nodes = nodes;
        for (size_t index = 0; index < nodes.size(); ++index) {
            if (!ReadMemory(
                    static_cast<const std::byte*>(nodes[index]) + kNodeLocalMatrixOffset,
                    anchor.localMatrices[index].data(),
                    sizeof(anchor.localMatrices[index]))) {
                anchor = {};
                g_armIkReadFallbacks.fetch_add(1, std::memory_order_relaxed);
                return false;
            }
        }
        anchor.valid = true;
        const uint64_t seedCount = g_armPoseSeeds.fetch_add(
            1, std::memory_order_relaxed) + 1;
        Logger::Instance().Write(
            LogLevel::Info,
            "hpl_arm_pose_seed seed=%llu entity=%p hand=%s nodes=%zu freezePose=%d policy=first_stable_full_chain_local_pose",
            static_cast<unsigned long long>(seedCount),
            entity,
            handIndex == 0 ? "left" : "right",
            nodes.size(),
            g_config.hplHandFreezePose ? 1 : 0);
        if (g_config.hplHandTrackingProbe) {
            for (size_t index = 0; index < nodes.size(); ++index) {
                void* parent = nullptr;
                std::array<float, 16> world{};
                std::array<float, 16> parentWorld{};
                const bool parentValid = ReadMemory(
                    static_cast<const std::byte*>(nodes[index]) + kNodeParentOffset,
                    &parent,
                    sizeof(parent));
                const bool worldValid = ReadMemory(
                    static_cast<const std::byte*>(nodes[index]) + kNodeWorldMatrixOffset,
                    world.data(),
                    sizeof(world));
                const bool parentWorldValid = parentValid && parent != nullptr
                    && ReadMemory(
                        static_cast<const std::byte*>(parent) + kNodeWorldMatrixOffset,
                        parentWorld.data(),
                        sizeof(parentWorld));
                const auto parentIt = std::find(nodes.begin(), nodes.end(), parent);
                const int parentIndex = parentIt == nodes.end()
                    ? -1 : static_cast<int>(std::distance(nodes.begin(), parentIt));
                float segmentLength = -1.0f;
                if (worldValid && parentWorldValid) {
                    const float dx = world[3] - parentWorld[3];
                    const float dy = world[7] - parentWorld[7];
                    const float dz = world[11] - parentWorld[11];
                    segmentLength = std::sqrt(dx * dx + dy * dy + dz * dz);
                }
                const char* role = index == kArmPoseShoulderIndex
                    ? "solver_shoulder"
                    : (index == kArmPoseElbowIndex
                        ? "solver_elbow"
                        : (index == kArmPoseWristIndex
                            ? "solver_wrist"
                            : (index < kArmPoseWristIndex
                                ? "intermediate_or_twist"
                                : "finger")));
                Logger::Instance().Write(
                    worldValid && parentValid ? LogLevel::Info : LogLevel::Warn,
                    "hpl_arm_hierarchy seed=%llu hand=%s index=%zu name=%s node=%p parent=%p parentIndex=%d role=%s localPos=%.5f,%.5f,%.5f worldPos=%.5f,%.5f,%.5f parentSegment=%.5f evidence=named_node_and_runtime_parent releasedAssetWeightsKnown=1 runtimeSkinWeightsObserved=0 policy=separate_joint_hierarchy_from_deform_ownership",
                    static_cast<unsigned long long>(seedCount),
                    handIndex == 0 ? "left" : "right",
                    index,
                    kArmPoseNodeNames[handIndex][index],
                    nodes[index],
                    parent,
                    parentIndex,
                    role,
                    anchor.localMatrices[index][3],
                    anchor.localMatrices[index][7],
                    anchor.localMatrices[index][11],
                    worldValid ? world[3] : 0.0f,
                    worldValid ? world[7] : 0.0f,
                    worldValid ? world[11] : 0.0f,
                    segmentLength);
            }
        }
    } else {
        const size_t restoreCount = g_config.hplHandFreezePose
            ? nodes.size() : kArmPoseWristIndex + 1;
        for (size_t index = 0; index < restoreCount; ++index) {
            g_nodeSetMatrix(
                nodes[index], anchor.localMatrices[index].data(), true);
        }
    }
    g_armIkBaseRestores.fetch_add(1, std::memory_order_relaxed);
    return true;
}

bool RestoreOrSeedSharedArmRootPose(
    void* entity,
    void* node,
    uint64_t frame)
{
    if (entity == nullptr || node == nullptr || g_nodeSetMatrix == nullptr) return false;
    uint8_t usePost = 0;
    std::array<float, 16> currentLocal{};
    if (!ReadMemory(static_cast<const std::byte*>(node) + kNodeUsePostTransformOffset,
            &usePost, sizeof(usePost))
        || !ReadMemory(static_cast<const std::byte*>(node) + kNodeLocalMatrixOffset,
            currentLocal.data(), sizeof(currentLocal))
        || usePost != 0) {
        g_armRootPoseFallbacks.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    std::lock_guard lock(g_armIkMutationMutex);
    SharedArmRootPoseAnchor& anchor = g_sharedArmRootPoseAnchor;
    if (!anchor.valid || anchor.entity != entity) {
        anchor = {};
        anchor.valid = true;
        anchor.entity = entity;
        anchor.node = node;
        anchor.lastRestoreFrame = frame;
        anchor.localMatrix = currentLocal;
        const bool medicineAuthoredSeed =
            std::fabs(currentLocal[3]) <= kMedicineArmRootTranslationTolerance
            && std::fabs(currentLocal[7] - kMedicineArmRootY)
                <= kMedicineArmRootTranslationTolerance
            && std::fabs(currentLocal[11] - kMedicineArmRootZ)
                <= kMedicineArmRootTranslationTolerance;
        if (medicineAuthoredSeed) {
            anchor.localMatrix[3] = 0.0f;
            anchor.localMatrix[7] = 0.0f;
            anchor.localMatrix[11] = kNeutralArmRootZ;
            g_nodeSetMatrix(node, anchor.localMatrix.data(), true);
            g_armRootPoseAuthoredSeedCorrections.fetch_add(
                1, std::memory_order_relaxed);
        }
        const uint64_t seed = g_armRootPoseSeeds.fetch_add(
            1, std::memory_order_relaxed) + 1;
        Logger::Instance().Write(
            medicineAuthoredSeed ? LogLevel::Warn : LogLevel::Info,
            "hpl_arm_root_pose_seed seed=%llu frame=%llu entity=%p node=%p incomingTranslation=%.4f,%.4f,%.4f anchorTranslation=%.4f,%.4f,%.4f authoredCorrection=%d policy=reject_known_medicine_authored_lift_then_restore_shared_clavicle_parent_local_pose",
            static_cast<unsigned long long>(seed),
            static_cast<unsigned long long>(frame),
            entity,
            node,
            currentLocal[3], currentLocal[7], currentLocal[11],
            anchor.localMatrix[3], anchor.localMatrix[7], anchor.localMatrix[11],
            medicineAuthoredSeed ? 1 : 0);
        return true;
    }
    if (anchor.node != node) {
        g_armRootPoseFallbacks.fetch_add(1, std::memory_order_relaxed);
        return false;
    }
    if (anchor.lastRestoreFrame == frame) return true;

    const float dx = currentLocal[3] - anchor.localMatrix[3];
    const float dy = currentLocal[7] - anchor.localMatrix[7];
    const float dz = currentLocal[11] - anchor.localMatrix[11];
    const float drift = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (std::isfinite(drift) && drift > 0.05f) {
        const uint64_t correction = g_armRootPoseDriftCorrections.fetch_add(
            1, std::memory_order_relaxed) + 1;
        if (correction <= 12 || correction % 300 == 0) {
            Logger::Instance().Write(
                LogLevel::Warn,
                "hpl_arm_root_pose_drift correction=%llu frame=%llu entity=%p node=%p current=%.4f,%.4f,%.4f anchor=%.4f,%.4f,%.4f delta=%.4f,%.4f,%.4f distance=%.4f policy=restore_shared_root_before_arm_chain",
                static_cast<unsigned long long>(correction),
                static_cast<unsigned long long>(frame),
                entity,
                node,
                currentLocal[3], currentLocal[7], currentLocal[11],
                anchor.localMatrix[3], anchor.localMatrix[7], anchor.localMatrix[11],
                dx, dy, dz, drift);
        }
    }
    g_nodeSetMatrix(node, anchor.localMatrix.data(), true);
    anchor.lastRestoreFrame = frame;
    g_armRootPoseRestores.fetch_add(1, std::memory_order_relaxed);
    return true;
}

void SeedWristOrientationAnchor(
    void* entity,
    size_t handIndex,
    const HPLTrackedPoseWorld& target,
    const std::array<void*, kArmPoseNodeCount>& poseNodes,
    const std::array<float, 16>& wristWorld,
    uint64_t inputFrame)
{
    if (!g_config.hplHandWristRotation
        || handIndex >= g_wristOrientationAnchors.size()) return;
    camera_math::Quaternion wristOrientation{};
    camera_math::Quaternion controllerOrientation{};
    const bool controllerBasisValid = camera_math::QuaternionFromForwardUp(
            {target.forwardX, target.forwardY, target.forwardZ},
            {target.upX, target.upY, target.upZ},
            controllerOrientation);
    camera_math::RotationBasisValidation wristBasisValidation =
        camera_math::RotationBasisValidation::Valid;
    const bool wristBasisValid = camera_math::QuaternionFromRotationMatrix(
        wristWorld, wristOrientation, &wristBasisValidation);
    if (!controllerBasisValid || !wristBasisValid) {
        if (!controllerBasisValid) {
            g_wristControllerBasisFallbacks.fetch_add(1, std::memory_order_relaxed);
        }
        if (!wristBasisValid) CountWristNativeBasisFallback(wristBasisValidation);
        g_armIkReadFallbacks.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    camera_math::Quaternion controllerToWrist{};
    bool geometricPalmBasis = false;
    std::array<float, 16> indexRootWorld{};
    std::array<float, 16> middleRootWorld{};
    std::array<float, 16> ringRootWorld{};
    std::array<float, 16> pinkyRootWorld{};
    if (ReadMemory(static_cast<const std::byte*>(poseNodes[kArmPoseIndexRootIndex])
            + kNodeWorldMatrixOffset, indexRootWorld.data(), sizeof(indexRootWorld))
        && ReadMemory(static_cast<const std::byte*>(poseNodes[kArmPoseMiddleRootIndex])
            + kNodeWorldMatrixOffset, middleRootWorld.data(), sizeof(middleRootWorld))
        && ReadMemory(static_cast<const std::byte*>(poseNodes[kArmPoseRingRootIndex])
            + kNodeWorldMatrixOffset, ringRootWorld.data(), sizeof(ringRootWorld))
        && ReadMemory(static_cast<const std::byte*>(poseNodes[kArmPosePinkyRootIndex])
            + kNodeWorldMatrixOffset, pinkyRootWorld.data(), sizeof(pinkyRootWorld))
        && hands_math::BuildPalmToWristOrientation(
            wristWorld, indexRootWorld, middleRootWorld, ringRootWorld,
            pinkyRootWorld, controllerToWrist)) {
        geometricPalmBasis = true;
    } else {
        controllerToWrist = camera_math::Normalize(camera_math::Multiply(
            camera_math::Conjugate(controllerOrientation), wristOrientation));
    }
    controllerToWrist = hands_math::ApplyControllerWristCalibration(
        controllerToWrist,
        g_config.hplHandWristRollDegrees,
        g_config.hplHandWristPitchDegrees);
    std::lock_guard lock(g_wristMutationMutex);
    WristOrientationAnchor& anchor = g_wristOrientationAnchors[handIndex];
    if (!anchor.valid || anchor.entity != entity) {
        anchor.valid = true;
        anchor.entity = entity;
        anchor.inputFrame = inputFrame;
        anchor.geometricPalmBasis = geometricPalmBasis;
        anchor.controllerToWrist = controllerToWrist;
        g_wristRotationAnchorSeeds.fetch_add(1, std::memory_order_relaxed);
        (geometricPalmBasis
            ? g_wristGeometricAnchorSeeds
            : g_wristLegacyAnchorSeeds).fetch_add(1, std::memory_order_relaxed);
        Logger::Instance().Write(
            geometricPalmBasis ? LogLevel::Info : LogLevel::Warn,
            "hpl_wrist_orientation_seed entity=%p hand=%s inputFrame=%llu mode=%s offsetQuaternion=%.5f,%.5f,%.5f,%.5f rollDegrees=%.2f pitchDegrees=%.2f policy=deterministic_model_palm_to_grip_basis",
            entity,
            handIndex == 0 ? "left" : "right",
            static_cast<unsigned long long>(inputFrame),
            geometricPalmBasis ? "geometric_palm" : "legacy_takeover_fallback",
            controllerToWrist.x, controllerToWrist.y,
            controllerToWrist.z, controllerToWrist.w,
            g_config.hplHandWristRollDegrees, g_config.hplHandWristPitchDegrees);
    }
}

void ApplyPlayerHandsArmIK(void* entity, WristTrackingFrame& tracking)
{
    if (!g_config.hplHandArmIK || !tracking.stateEligible
        || g_getMeshEntity == nullptr || g_getBoneStateFromName == nullptr
        || g_setUsePostTransform == nullptr || g_setPostTransform == nullptr
        || g_applyPostAnimTransform == nullptr) {
        return;
    }
    {
        std::lock_guard lock(g_armIkMutationMutex);
        if (g_lastArmIkMutationEntity == entity
            && g_lastArmIkMutationFrame == tracking.playerFrame) return;
        g_lastArmIkMutationEntity = entity;
        g_lastArmIkMutationFrame = tracking.playerFrame;
    }

    const uint64_t frameAttempt = g_armIkFrameAttempts.fetch_add(
        1, std::memory_order_relaxed) + 1;
    void* mesh = g_getMeshEntity(entity);
    if (mesh == nullptr) {
        g_armIkReadFallbacks.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    const HPLCameraBridgeStatus& camera = tracking.camera;
    const float worldUnitsPerMeter = camera.worldUnitsPerMeter;
    camera_math::Quaternion bodyYaw{};
    const bool bodyYawValid = ResolveHandsBodyYaw(camera, bodyYaw);
    const camera_math::Vector3 bodyForward = camera_math::RotateVector(
        bodyYaw, {0.0f, 0.0f, -1.0f});
    const camera_math::Vector3 worldUp{0.0f, 1.0f, 0.0f};
    const bool ergonomicFrame = g_config.hplHandArmIKErgonomics
        && bodyYawValid;
    uint32_t applied = 0;
    for (size_t handIndex = 0; handIndex < 2; ++handIndex) {
        if (!tracking.eligible[handIndex]) continue;
        std::array<void*, kArmPoseNodeCount> poseNodes{};
        if (!ResolveArmPoseNodes(mesh, handIndex, poseNodes)) {
            g_armIkHierarchyFallbacks.fetch_add(1, std::memory_order_relaxed);
            continue;
        }
        void* clavicle = poseNodes[kArmPoseClavicleIndex];
        void* shoulder = poseNodes[kArmPoseShoulderIndex];
        void* elbow = poseNodes[kArmPoseElbowIndex];
        void* wrist = poseNodes[kArmPoseWristIndex];
        void* sharedArmRoot = nullptr;
        if (!ReadMemory(static_cast<const std::byte*>(clavicle) + kNodeParentOffset,
                &sharedArmRoot, sizeof(sharedArmRoot))
            || sharedArmRoot == nullptr
            || !RestoreOrSeedSharedArmRootPose(
                entity, sharedArmRoot, tracking.playerFrame)) {
            g_armRootPoseFallbacks.fetch_add(1, std::memory_order_relaxed);
            continue;
        }
        if (!RestoreOrSeedArmPose(entity, handIndex, poseNodes)) continue;

        std::array<float, 16> shoulderWorld{};
        std::array<float, 16> elbowWorld{};
        std::array<float, 16> wristWorld{};
        if (!ReadMemory(static_cast<const std::byte*>(shoulder) + kNodeWorldMatrixOffset,
                shoulderWorld.data(), sizeof(shoulderWorld))
            || !ReadMemory(static_cast<const std::byte*>(elbow) + kNodeWorldMatrixOffset,
                elbowWorld.data(), sizeof(elbowWorld))
            || !ReadMemory(static_cast<const std::byte*>(wrist) + kNodeWorldMatrixOffset,
                wristWorld.data(), sizeof(wristWorld))) {
            g_armIkReadFallbacks.fetch_add(1, std::memory_order_relaxed);
            continue;
        }

        camera_math::Vector3 shoulderPosition = MatrixPosition(shoulderWorld);
        camera_math::Vector3 nativeElbow = MatrixPosition(elbowWorld);
        camera_math::Vector3 nativeWrist = MatrixPosition(wristWorld);
        const camera_math::Vector3 neutralShoulder = shoulderPosition;
        const float upperLength = Distance(shoulderPosition, nativeElbow);
        const float lowerLength = Distance(nativeElbow, nativeWrist);
        SeedWristOrientationAnchor(
            entity,
            handIndex,
            tracking.targets[handIndex],
            poseNodes,
            wristWorld,
            tracking.inputFrame);
        const camera_math::Vector3 target = tracking.goals[handIndex].requested;

        ArmErgonomicState previousErgonomic{};
        bool previousErgonomicValid = false;
        {
            std::lock_guard lock(g_armIkMutationMutex);
            const ArmErgonomicState& current = g_armErgonomicStates[handIndex];
            previousErgonomicValid = current.valid && current.entity == entity
                && tracking.playerFrame >= current.lastFrame
                && tracking.playerFrame - current.lastFrame <= 4;
            if (previousErgonomicValid) previousErgonomic = current;
        }

        arm_ik_math::ShoulderReachSolution shoulderReach{};
        bool shoulderReachValid = false;
        bool shoulderReachApplied = false;
        if (ergonomicFrame && g_config.hplHandShoulderReachCompensation) {
            const float reachFull = std::min(
                1.0f,
                std::max(
                    g_config.hplHandArmIKMaxReach,
                    g_config.hplHandShoulderReachStart + 0.01f));
            const camera_math::Vector3* previousOffset = previousErgonomicValid
                ? &previousErgonomic.shoulderOffsetLocal : nullptr;
            shoulderReachValid = arm_ik_math::ComputeReachShoulderTarget(
                shoulderPosition,
                target,
                bodyForward,
                worldUp,
                handIndex == 0,
                upperLength + lowerLength,
                g_config.hplHandShoulderReachStart,
                reachFull,
                g_config.hplHandShoulderReachMaxMeters * worldUnitsPerMeter,
                0.35f,
                previousOffset,
                shoulderReach);
            if (shoulderReachValid && shoulderReach.applied
                && ApplyArmNodeToward(
                    clavicle, shoulderPosition, shoulderReach.target)) {
                if (ReadMemory(
                        static_cast<const std::byte*>(shoulder) + kNodeWorldMatrixOffset,
                        shoulderWorld.data(), sizeof(shoulderWorld))
                    && ReadMemory(
                        static_cast<const std::byte*>(elbow) + kNodeWorldMatrixOffset,
                        elbowWorld.data(), sizeof(elbowWorld))
                    && ReadMemory(
                        static_cast<const std::byte*>(wrist) + kNodeWorldMatrixOffset,
                        wristWorld.data(), sizeof(wristWorld))) {
                    shoulderPosition = MatrixPosition(shoulderWorld);
                    nativeElbow = MatrixPosition(elbowWorld);
                    nativeWrist = MatrixPosition(wristWorld);
                    shoulderReachApplied = true;
                    g_armIkShoulderCompensations.fetch_add(
                        1, std::memory_order_relaxed);
                } else {
                    g_armIkReadFallbacks.fetch_add(1, std::memory_order_relaxed);
                }
            } else if (!shoulderReachValid) {
                g_armIkErgonomicFallbacks.fetch_add(1, std::memory_order_relaxed);
            }
        }

        camera_math::Vector3 elbowPole = nativeElbow;
        elbowPole.y -= g_config.hplHandArmIKElbowDownMeters * worldUnitsPerMeter;
        arm_ik_math::ElbowPoleSolution ergonomicPole{};
        bool ergonomicPoleValid = false;
        if (ergonomicFrame) {
            const camera_math::Vector3* previousDirection = previousErgonomicValid
                ? &previousErgonomic.elbowDirectionLocal : nullptr;
            ergonomicPoleValid = arm_ik_math::ComputeErgonomicElbowPole(
                shoulderPosition,
                target,
                nativeElbow,
                bodyForward,
                worldUp,
                handIndex == 0,
                std::clamp(
                    g_config.hplHandArmIKElbowDownMeters / 0.10f,
                    0.0f, 5.0f),
                std::max(
                    upperLength + lowerLength,
                    g_config.hplHandArmIKElbowDownMeters * worldUnitsPerMeter),
                g_config.hplHandArmIKMaxSwivelDegreesPerFrame,
                previousDirection,
                ergonomicPole);
            if (ergonomicPoleValid) {
                elbowPole = ergonomicPole.pole;
                if (ergonomicPole.historyUsed) {
                    g_armIkElbowHistoryUses.fetch_add(1, std::memory_order_relaxed);
                }
                if (ergonomicPole.singularityBlend > 0.0f) {
                    g_armIkElbowSingularityBlends.fetch_add(
                        1, std::memory_order_relaxed);
                }
                if (ergonomicPole.swivelLimited) {
                    g_armIkElbowSwivelLimits.fetch_add(1, std::memory_order_relaxed);
                }
            } else {
                g_armIkErgonomicFallbacks.fetch_add(1, std::memory_order_relaxed);
            }
        }
        arm_ik_math::TwoBoneSolution solution;
        if (!arm_ik_math::SolveTwoBone(
                shoulderPosition, target, elbowPole,
                upperLength, lowerLength,
                g_config.hplHandArmIKMaxReach, solution)) {
            g_armIkMathFallbacks.fetch_add(1, std::memory_order_relaxed);
            continue;
        }
        if (solution.reachClamped) {
            g_armIkReachClamps.fetch_add(1, std::memory_order_relaxed);
        }
        if (!ApplyArmNodeToward(shoulder, nativeElbow, solution.elbow)) continue;

        if (!ReadMemory(static_cast<const std::byte*>(elbow) + kNodeWorldMatrixOffset,
                elbowWorld.data(), sizeof(elbowWorld))
            || !ReadMemory(static_cast<const std::byte*>(wrist) + kNodeWorldMatrixOffset,
                wristWorld.data(), sizeof(wristWorld))) {
            g_armIkReadFallbacks.fetch_add(1, std::memory_order_relaxed);
            continue;
        }
        const camera_math::Vector3 rotatedWrist = MatrixPosition(wristWorld);
        if (!ApplyArmNodeToward(elbow, rotatedWrist, solution.wrist)) continue;
        hands_math::CommitWristIKGoal(solution.wrist, tracking.goals[handIndex]);

        if (ergonomicPoleValid) {
            std::lock_guard lock(g_armIkMutationMutex);
            ArmErgonomicState& state = g_armErgonomicStates[handIndex];
            state.valid = true;
            state.entity = entity;
            state.lastFrame = tracking.playerFrame;
            state.shoulderOffsetLocal = shoulderReachApplied
                ? shoulderReach.offsetLocal : camera_math::Vector3{};
            state.elbowDirectionLocal = ergonomicPole.directionLocal;
        }

        ++applied;
        const uint64_t application = g_armIkApplications.fetch_add(
            1, std::memory_order_relaxed) + 1;
        const uint64_t interval = static_cast<uint64_t>(
            std::max(g_config.hplControllerLogInterval, 1));
        if (application <= 12 || application % interval == 0) {
            Logger::Instance().Write(
                LogLevel::Info,
                "hpl_arm_ik application=%llu frameAttempt=%llu frame=%llu hand=%s entity=%p mesh=%p clavicle=%p shoulder=%p elbow=%p wrist=%p lengths=%.4f,%.4f requestedReach=%.4f solvedReach=%.4f reachClamped=%d target=%.4f,%.4f,%.4f neutralShoulder=%.4f,%.4f,%.4f solvedShoulder=%.4f,%.4f,%.4f shoulderReach={valid=%d applied=%d ratio=%.4f blend=%.4f offset=%.4f,%.4f,%.4f} solvedElbow=%.4f,%.4f,%.4f elbowErgonomics={enabled=%d valid=%d direction=%.4f,%.4f,%.4f history=%d crossMagnitude=%.4f singularityBlend=%.4f crossFallback=%d swivelLimited=%d nativeFallback=%d maxDegreesPerFrame=%.2f} elbowDownMeters=%.3f blend=%.3f policy=body_yaw_reach_gated_clavicle_torso_local_cross_product_elbow_two_bone_post",
                static_cast<unsigned long long>(application),
                static_cast<unsigned long long>(frameAttempt),
                static_cast<unsigned long long>(tracking.playerFrame),
                handIndex == 0 ? "left" : "right", entity, mesh,
                clavicle, shoulder, elbow, wrist, upperLength, lowerLength,
                solution.requestedDistance, solution.solvedDistance,
                solution.reachClamped ? 1 : 0,
                target.x, target.y, target.z,
                neutralShoulder.x, neutralShoulder.y, neutralShoulder.z,
                shoulderPosition.x, shoulderPosition.y, shoulderPosition.z,
                shoulderReachValid ? 1 : 0,
                shoulderReachApplied ? 1 : 0,
                shoulderReach.reachRatio,
                shoulderReach.blend,
                shoulderReach.offsetWorld.x,
                shoulderReach.offsetWorld.y,
                shoulderReach.offsetWorld.z,
                solution.elbow.x, solution.elbow.y, solution.elbow.z,
                ergonomicFrame ? 1 : 0,
                ergonomicPoleValid ? 1 : 0,
                ergonomicPole.directionWorld.x,
                ergonomicPole.directionWorld.y,
                ergonomicPole.directionWorld.z,
                ergonomicPole.historyUsed ? 1 : 0,
                ergonomicPole.crossMagnitude,
                ergonomicPole.singularityBlend,
                ergonomicPole.crossFallbackUsed ? 1 : 0,
                ergonomicPole.swivelLimited ? 1 : 0,
                ergonomicPole.nativeFallbackUsed ? 1 : 0,
                g_config.hplHandArmIKMaxSwivelDegreesPerFrame,
                g_config.hplHandArmIKElbowDownMeters,
                g_config.hplHandArmIKBlend);
        }
    }
    if (applied > 0) g_armIkFramesApplied.fetch_add(1, std::memory_order_relaxed);
}

void ApplyPlayerHandsWristPositions(
    void* entity,
    const WristTrackingFrame& tracking,
    bool rootScaleNormalized)
{
    if (!tracking.stateEligible) {
        ResetWristOrientationAnchors(entity);
        return;
    }
    if ((!g_config.hplHandWristPosition && !g_config.hplHandWristRotation)
        || g_getMeshEntity == nullptr
        || g_getBoneStateFromName == nullptr
        || g_setUsePostTransform == nullptr
        || g_setPostTransform == nullptr
        || g_applyPostAnimTransform == nullptr) {
        return;
    }

    {
        std::lock_guard lock(g_wristMutationMutex);
        if (g_lastWristMutationEntity == entity
            && g_lastWristMutationFrame == tracking.playerFrame) {
            return;
        }
        g_lastWristMutationEntity = entity;
        g_lastWristMutationFrame = tracking.playerFrame;
    }
    const uint64_t frameAttempt = g_wristFrameAttempts.fetch_add(
        1, std::memory_order_relaxed) + 1;
    void* meshEntity = g_getMeshEntity(entity);
    if (meshEntity == nullptr) {
        g_wristReadFallbacks.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    constexpr const char* wristNames[] = {"j_L_Wrist", "j_R_Wrist"};
    constexpr const char* parentNames[] = {"j_L_Arm_10", "j_R_Arm_10"};
    uint32_t appliedThisFrame = 0;
    for (size_t handIndex = 0; handIndex < 2; ++handIndex) {
        if (!tracking.eligible[handIndex]) {
            ResetWristOrientationAnchor(entity, handIndex);
            continue;
        }
        NativeStringLayout wristName = MakeInlineNativeString(wristNames[handIndex]);
        NativeStringLayout parentName = MakeInlineNativeString(parentNames[handIndex]);
        void* wrist = g_getBoneStateFromName(meshEntity, &wristName);
        void* expectedParent = g_getBoneStateFromName(meshEntity, &parentName);
        if (wrist == nullptr || expectedParent == nullptr) {
            g_wristHierarchyFallbacks.fetch_add(1, std::memory_order_relaxed);
            continue;
        }

        std::array<float, 16> localMatrix{};
        std::array<float, 16> worldMatrix{};
        std::array<float, 16> parentWorldMatrix{};
        std::array<float, 16> authoredPostMatrix{};
        void* parent = nullptr;
        uint8_t authoredUsePost = 0;
        const bool readable = ReadMemory(
                static_cast<const std::byte*>(wrist) + kNodeLocalMatrixOffset,
                localMatrix.data(), sizeof(localMatrix))
            && ReadMemory(
                static_cast<const std::byte*>(wrist) + kNodeWorldMatrixOffset,
                worldMatrix.data(), sizeof(worldMatrix))
            && ReadMemory(
                static_cast<const std::byte*>(wrist) + kNodeParentOffset,
                &parent, sizeof(parent))
            && ReadMemory(
                static_cast<const std::byte*>(expectedParent) + kNodeWorldMatrixOffset,
                parentWorldMatrix.data(), sizeof(parentWorldMatrix))
            && ReadMemory(
                static_cast<const std::byte*>(wrist) + kNodePostTransformOffset,
                authoredPostMatrix.data(), sizeof(authoredPostMatrix))
            && ReadMemory(
                static_cast<const std::byte*>(wrist) + kNodeUsePostTransformOffset,
                &authoredUsePost, sizeof(authoredUsePost));
        if (!readable) {
            g_wristReadFallbacks.fetch_add(1, std::memory_order_relaxed);
            continue;
        }
        if (parent != expectedParent) {
            g_wristHierarchyFallbacks.fetch_add(1, std::memory_order_relaxed);
            continue;
        }
        if (authoredUsePost != 0) {
            g_wristAuthoredPostFallbacks.fetch_add(1, std::memory_order_relaxed);
            continue;
        }

        std::array<float, 16> desiredWorld = worldMatrix;
        if (g_config.hplHandWristPosition) {
            desiredWorld[3] = tracking.goals[handIndex].selected.x;
            desiredWorld[7] = tracking.goals[handIndex].selected.y;
            desiredWorld[11] = tracking.goals[handIndex].selected.z;
        }
        bool orientationAnchorSeeded = false;
        bool orientationApplied = false;
        bool geometricPalmBasis = false;
        if (g_config.hplHandWristRotation) {
            camera_math::Quaternion controllerOrientation;
            camera_math::Quaternion wristOrientation;
            const bool controllerBasisValid = camera_math::QuaternionFromForwardUp(
                    {
                        tracking.targets[handIndex].forwardX,
                        tracking.targets[handIndex].forwardY,
                        tracking.targets[handIndex].forwardZ,
                    },
                    {
                        tracking.targets[handIndex].upX,
                        tracking.targets[handIndex].upY,
                        tracking.targets[handIndex].upZ,
                    },
                    controllerOrientation);
            camera_math::RotationBasisValidation wristBasisValidation =
                camera_math::RotationBasisValidation::Valid;
            const bool wristBasisValid = camera_math::QuaternionFromRotationMatrix(
                worldMatrix, wristOrientation, &wristBasisValidation);
            const bool orientationReadable = controllerBasisValid && wristBasisValid;
            if (!controllerBasisValid) {
                g_wristControllerBasisFallbacks.fetch_add(1, std::memory_order_relaxed);
            }
            if (!wristBasisValid) CountWristNativeBasisFallback(wristBasisValidation);
            if (orientationReadable) {
                WristOrientationAnchor anchor;
                {
                    std::lock_guard lock(g_wristMutationMutex);
                    WristOrientationAnchor& stored = g_wristOrientationAnchors[handIndex];
                    if (!stored.valid || stored.entity != entity) {
                        stored.valid = true;
                        stored.entity = entity;
                        stored.inputFrame = tracking.inputFrame;
                        stored.geometricPalmBasis = false;
                        stored.controllerToWrist = camera_math::Normalize(
                            camera_math::Multiply(
                                camera_math::Conjugate(controllerOrientation),
                                wristOrientation));
                        stored.controllerToWrist = hands_math::ApplyControllerWristCalibration(
                            stored.controllerToWrist,
                            g_config.hplHandWristRollDegrees,
                            g_config.hplHandWristPitchDegrees);
                        orientationAnchorSeeded = true;
                        g_wristRotationAnchorSeeds.fetch_add(1, std::memory_order_relaxed);
                        g_wristLegacyAnchorSeeds.fetch_add(1, std::memory_order_relaxed);
                    }
                    anchor = stored;
                }
                geometricPalmBasis = anchor.geometricPalmBasis;
                std::array<float, 16> trackedWorld{};
                const camera_math::Vector3 trackedPosition{
                    desiredWorld[3], desiredWorld[7], desiredWorld[11]};
                orientationApplied = hands_math::BuildTrackedWristWorldMatrixFromOffset(
                    trackedPosition,
                    {
                        tracking.targets[handIndex].forwardX,
                        tracking.targets[handIndex].forwardY,
                        tracking.targets[handIndex].forwardZ,
                    },
                    {
                        tracking.targets[handIndex].upX,
                        tracking.targets[handIndex].upY,
                        tracking.targets[handIndex].upZ,
                    },
                    anchor.controllerToWrist,
                    worldMatrix,
                    trackedWorld);
                if (orientationApplied) {
                    desiredWorld = trackedWorld;
                    g_wristRotationApplications.fetch_add(1, std::memory_order_relaxed);
                }
            }
            if (!orientationApplied) {
                g_wristRotationFallbacks.fetch_add(1, std::memory_order_relaxed);
            }
        }
        std::array<float, 16> postTransform{};
        if (!hands_math::BuildPostTransformForWorldTarget(
                parentWorldMatrix, localMatrix, desiredWorld, postTransform)) {
            g_wristMathFallbacks.fetch_add(1, std::memory_order_relaxed);
            continue;
        }

        g_setPostTransform(wrist, postTransform.data());
        g_setUsePostTransform(wrist, true);
        g_applyPostAnimTransform(wrist, true);
        g_setPostTransform(wrist, authoredPostMatrix.data());
        g_setUsePostTransform(wrist, false);

        uint8_t restoredUsePost = 1;
        std::array<float, 16> restoredPostMatrix{};
        const bool restored = ReadMemory(
                static_cast<const std::byte*>(wrist) + kNodeUsePostTransformOffset,
                &restoredUsePost, sizeof(restoredUsePost))
            && ReadMemory(
                static_cast<const std::byte*>(wrist) + kNodePostTransformOffset,
                restoredPostMatrix.data(), sizeof(restoredPostMatrix))
            && restoredUsePost == authoredUsePost
            && std::memcmp(
                restoredPostMatrix.data(),
                authoredPostMatrix.data(),
                sizeof(authoredPostMatrix)) == 0;
        if (!restored) {
            g_wristReadFallbacks.fetch_add(1, std::memory_order_relaxed);
        }

        const float dx = desiredWorld[3] - worldMatrix[3];
        const float dy = desiredWorld[7] - worldMatrix[7];
        const float dz = desiredWorld[11] - worldMatrix[11];
        const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
        const uint64_t application = g_wristApplications.fetch_add(
            1, std::memory_order_relaxed) + 1;
        ++appliedThisFrame;
        const uint64_t interval = static_cast<uint64_t>(
            std::max(g_config.hplControllerLogInterval, 1));
        if (application <= 12 || application % interval == 0) {
            std::array<float, 16> actualWorld{};
            const bool actualValid = ReadMemory(
                static_cast<const std::byte*>(wrist) + kNodeWorldMatrixOffset,
                actualWorld.data(), sizeof(actualWorld));
            const auto& goal = tracking.goals[handIndex];
            Logger::Instance().Write(LogLevel::Info,
                "hpl_wrist_goal frame=%llu inputFrame=%llu hand=%s entity=%p ikSolved=%d requested=%.4f,%.4f,%.4f selected=%.4f,%.4f,%.4f actualValid=%d actual=%.4f,%.4f,%.4f clampMeters=%.5f residualMeters=%.5f policy=single_calibrated_goal_before_ik",
                static_cast<unsigned long long>(tracking.playerFrame),
                static_cast<unsigned long long>(tracking.inputFrame),
                handIndex == 0 ? "left" : "right", entity, goal.ikSolved ? 1 : 0,
                goal.requested.x, goal.requested.y, goal.requested.z,
                goal.selected.x, goal.selected.y, goal.selected.z,
                actualValid ? 1 : 0, actualWorld[3], actualWorld[7], actualWorld[11],
                Distance(goal.requested, goal.selected) / tracking.camera.worldUnitsPerMeter,
                actualValid ? Distance(MatrixPosition(actualWorld), MatrixPosition(desiredWorld))
                    / tracking.camera.worldUnitsPerMeter : -1.0f);
            Logger::Instance().Write(
                restored ? LogLevel::Info : LogLevel::Warn,
                "hpl_wrist_position application=%llu frameAttempt=%llu frame=%llu inputFrame=%llu "
                "entity=%p mesh=%p hand=%s wrist=%p parent=%p rootScaleNormalized=%d "
                "nativePos=%.4f,%.4f,%.4f targetPos=%.4f,%.4f,%.4f distance=%.4f "
                "postTranslation=%.4f,%.4f,%.4f rotationRequested=%d rotationApplied=%d "
                "orientationAnchorSeeded=%d orientationBasis=%s authoredUsePost=%d restored=%d "
                "policy=physical_states_transient_position_and_geometric_palm_anchored_controller_rotation_post",
                static_cast<unsigned long long>(application),
                static_cast<unsigned long long>(frameAttempt),
                static_cast<unsigned long long>(tracking.playerFrame),
                static_cast<unsigned long long>(tracking.inputFrame),
                entity,
                meshEntity,
                handIndex == 0 ? "left" : "right",
                wrist,
                parent,
                rootScaleNormalized ? 1 : 0,
                worldMatrix[3], worldMatrix[7], worldMatrix[11],
                desiredWorld[3], desiredWorld[7], desiredWorld[11],
                distance,
                postTransform[3], postTransform[7], postTransform[11],
                g_config.hplHandWristRotation ? 1 : 0,
                orientationApplied ? 1 : 0,
                orientationAnchorSeeded ? 1 : 0,
                geometricPalmBasis ? "geometric_palm" : "legacy_takeover_fallback",
                authoredUsePost != 0 ? 1 : 0,
                restored ? 1 : 0);
        }
    }
    if (appliedThisFrame > 0) {
        g_wristFramesApplied.fetch_add(1, std::memory_order_relaxed);
    }
}

void CacheFlashlightPose(const std::array<float, 16>& matrix, uint64_t frame)
{
    std::lock_guard lock(g_flashlightPoseMutex);
    g_flashlightPoseCache.valid = true;
    g_flashlightPoseCache.frame = frame;
    g_flashlightPoseCache.matrix = matrix;
}

void InvalidateFlashlightPose()
{
    std::lock_guard lock(g_flashlightPoseMutex);
    g_flashlightPoseCache = {};
}

bool ReadFlashlightPose(FlashlightPoseCache& cache)
{
    std::lock_guard lock(g_flashlightPoseMutex);
    cache = g_flashlightPoseCache;
    return cache.valid;
}

bool BuildStableSocketedPropMatrix(
    void* entity,
    const std::array<float, 16>& nativeMatrix,
    std::array<float, 16>& output)
{
    if (!g_config.hplHandSocketedPropStabilization || g_openxr == nullptr) return false;

    OpenXRInputSnapshot input{};
    HPLTrackedPoseWorld rightGrip{};
    HPLPlayerStateSnapshot player{};
    const HPLCameraBridgeStatus camera = GetHPLCameraBridgeStatus();
    if (!g_openxr->GetLatestInput(input)
        || !input.active
        || !input.right.active
        || !input.right.gripPose.valid
        || !ResolveHPLTrackedPoseWorld(input.right.gripPose, input.gameFrame, rightGrip)
        || !rightGrip.positionTracked
        || !rightGrip.orientationTracked
        || !camera.trackingEnabled
        || !GetHPLPlayerStateSnapshot(player)
        || !player.playerValid) {
        return false;
    }
    const uint64_t inputAge = player.frame >= input.gameFrame
        ? player.frame - input.gameFrame : 0;
    if (inputAge > static_cast<uint64_t>(g_config.hplControllerMaxInputAgeFrames)) {
        return false;
    }

    camera_math::Quaternion gripOrientation{};
    if (!camera_math::QuaternionFromForwardUp(
            {rightGrip.forwardX, rightGrip.forwardY, rightGrip.forwardZ},
            {rightGrip.upX, rightGrip.upY, rightGrip.upZ},
            gripOrientation)) {
        return false;
    }
    std::array<float, 16> gripMatrix = camera_math::RotationMatrix(gripOrientation);
    gripMatrix[3] = rightGrip.positionX;
    gripMatrix[7] = rightGrip.positionY;
    gripMatrix[11] = rightGrip.positionZ;

    std::lock_guard lock(g_socketedPropMutex);
    SocketedPropAnchor& anchor = g_socketedPropAnchors[entity];
    if (!anchor.valid) {
        const float dx = nativeMatrix[3] - rightGrip.positionX;
        const float dy = nativeMatrix[7] - rightGrip.positionY;
        const float dz = nativeMatrix[11] - rightGrip.positionZ;
        const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
        const float seedRadius = 0.50f * std::max(camera.worldUnitsPerMeter, 0.001f);
        std::array<float, 16> inverseGrip{};
        if (!std::isfinite(distance)
            || distance > seedRadius
            || !hands_math::InvertAffineMatrix(gripMatrix, inverseGrip)) {
            return false;
        }
        anchor.valid = true;
        anchor.seedFrame = player.frame;
        anchor.gripToProp = camera_math::MatrixMultiply(inverseGrip, nativeMatrix);
        const uint64_t seed = g_socketedPropAnchorSeeds.fetch_add(
            1, std::memory_order_relaxed) + 1;
        Logger::Instance().Write(
            LogLevel::Info,
            "hpl_socketed_prop_anchor seed=%llu frame=%llu entity=%p name=%s hand=right distance=%.4f policy=preserve_initial_grip_relative_transform",
            static_cast<unsigned long long>(seed),
            static_cast<unsigned long long>(player.frame),
            entity,
            kMedicineEntityName,
            distance);
    }
    output = camera_math::MatrixMultiply(gripMatrix, anchor.gripToProp);
    return true;
}

bool IsFiniteVector(const float* value)
{
    return value != nullptr
        && std::isfinite(value[0])
        && std::isfinite(value[1])
        && std::isfinite(value[2]);
}

void* HookGetClosestBody(
    const float* start,
    const float* direction,
    float rayLength,
    float* outDistance,
    float* outSurfaceNormal)
{
    const uint64_t call = g_flashlightGameplayRayCalls.fetch_add(1, std::memory_order_relaxed) + 1;
    if (!g_config.hplControllerFlashlightGameplayRay
        || !g_config.hplControllerFlashlightAim
        || !IsFiniteVector(start)
        || !IsFiniteVector(direction)
        || !std::isfinite(rayLength)
        || rayLength < 5.0f
        || rayLength > 20.0f) {
        return g_originalGetClosestBody(start, direction, rayLength, outDistance, outSurfaceNormal);
    }
    const uint64_t candidate = g_flashlightGameplayRayCandidates.fetch_add(1, std::memory_order_relaxed) + 1;
    const HPLCameraBridgeStatus camera = GetHPLCameraBridgeStatus();
    const float dx = start[0] - camera.cameraWorldPositionX;
    const float dy = start[1] - camera.cameraWorldPositionY;
    const float dz = start[2] - camera.cameraWorldPositionZ;
    const float originDelta = std::sqrt(dx * dx + dy * dy + dz * dz);
    const float originTolerance = std::max(0.35f * g_config.hplWorldScale, 0.05f);
    if (!camera.trackingEnabled || !camera.cameraWorldPositionValid
        || !camera.nativeCameraBasisValid || !std::isfinite(originDelta)
        || originDelta > originTolerance) {
        g_flashlightGameplayRayOriginFallbacks.fetch_add(1, std::memory_order_relaxed);
        return g_originalGetClosestBody(start, direction, rayLength, outDistance, outSurfaceNormal);
    }

    HPLPlayerStateSnapshot player;
    FlashlightPoseCache cache;
    if (!GetHPLPlayerStateSnapshot(player) || !player.playerValid || !ReadFlashlightPose(cache)) {
        g_flashlightGameplayRayPoseFallbacks.fetch_add(1, std::memory_order_relaxed);
        return g_originalGetClosestBody(start, direction, rayLength, outDistance, outSurfaceNormal);
    }
    const uint64_t poseAge = player.frame >= cache.frame ? player.frame - cache.frame : 0;
    if (cache.frame == 0
        || poseAge > static_cast<uint64_t>(g_config.hplControllerMaxInputAgeFrames)) {
        g_flashlightGameplayRayStaleFallbacks.fetch_add(1, std::memory_order_relaxed);
        return g_originalGetClosestBody(start, direction, rayLength, outDistance, outSurfaceNormal);
    }

    const camera_math::Vector3 nativeForward{
        camera.nativeCameraForwardX, camera.nativeCameraForwardY, camera.nativeCameraForwardZ};
    const camera_math::Vector3 nativeUp{
        camera.nativeCameraUpX, camera.nativeCameraUpY, camera.nativeCameraUpZ};
    const camera_math::Vector3 flashlightForward{
        -cache.matrix[2], -cache.matrix[6], -cache.matrix[10]};
    const camera_math::Vector3 flashlightUp{
        cache.matrix[1], cache.matrix[5], cache.matrix[9]};
    camera_math::Vector3 redirectedDirection;
    if (!flashlight_math::RedirectConeDirection(
            {direction[0], direction[1], direction[2]},
            nativeForward,
            nativeUp,
            flashlightForward,
            flashlightUp,
            redirectedDirection)) {
        g_flashlightGameplayRayMathFallbacks.fetch_add(1, std::memory_order_relaxed);
        return g_originalGetClosestBody(start, direction, rayLength, outDistance, outSurfaceNormal);
    }
    const float redirectedStart[3] = {cache.matrix[3], cache.matrix[7], cache.matrix[11]};
    const float redirected[3] = {
        redirectedDirection.x, redirectedDirection.y, redirectedDirection.z};
    void* body = g_originalGetClosestBody(
        redirectedStart, redirected, rayLength, outDistance, outSurfaceNormal);
    const uint64_t redirect = g_flashlightGameplayRayRedirects.fetch_add(1, std::memory_order_relaxed) + 1;
    if (body != nullptr) {
        g_flashlightGameplayRayHits.fetch_add(1, std::memory_order_relaxed);
    }
    const uint64_t interval = static_cast<uint64_t>(std::max(g_config.hplControllerLogInterval, 1));
    if (redirect <= 12 || redirect % interval == 0) {
        Logger::Instance().Write(
            LogLevel::Info,
            "hpl_flashlight_gameplay_ray call=%llu candidate=%llu redirected=%llu frame=%llu poseAge=%llu hit=%d rayLength=%.3f originDelta=%.4f nativeStart=%.4f,%.4f,%.4f redirectedStart=%.4f,%.4f,%.4f nativeDirection=%.5f,%.5f,%.5f redirectedDirection=%.5f,%.5f,%.5f policy=preserve_random_cone",
            static_cast<unsigned long long>(call),
            static_cast<unsigned long long>(candidate),
            static_cast<unsigned long long>(redirect),
            static_cast<unsigned long long>(player.frame),
            static_cast<unsigned long long>(poseAge),
            body != nullptr ? 1 : 0,
            rayLength,
            originDelta,
            start[0], start[1], start[2],
            redirectedStart[0], redirectedStart[1], redirectedStart[2],
            direction[0], direction[1], direction[2],
            redirected[0], redirected[1], redirected[2]);
    }
    return body;
}

void HookLuxEntitySetMatrix(void* entity, const float* matrixPointer)
{
    const uint64_t call = g_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    const EntityIdentity identity = ResolveIdentity(entity);
    const float* submittedMatrix = matrixPointer;
    std::array<float, 16> controllerMatrix{};
    std::array<float, 16> normalizedHandsMatrix{};
    WristTrackingFrame wristTracking{};
    bool rootScaleNormalized = false;
    bool rootAlreadyFullScale = false;
    bool shoulderOffsetApplied = false;
    if (identity.playerHands) {
        const uint64_t handCall = g_playerHandsCalls.fetch_add(1, std::memory_order_relaxed) + 1;
        std::array<float, 16> matrix{};
        if (!ReadMemory(matrixPointer, matrix.data(), sizeof(matrix))) {
            g_matrixReadFailures.fetch_add(1, std::memory_order_relaxed);
        } else {
            const float scaleX = ColumnLength(matrix, 0);
            const float scaleY = ColumnLength(matrix, 1);
            const float scaleZ = ColumnLength(matrix, 2);
            const float averageScale = (scaleX + scaleY + scaleZ) / 3.0f;
            const char* scaleMode = "other";
            if (std::fabs(averageScale - kQuarterScale) <= kQuarterScaleTolerance) {
                scaleMode = "quarter";
                g_quarterScaleSamples.fetch_add(1, std::memory_order_relaxed);
            } else if (std::fabs(averageScale - 1.0f) <= 0.1f) {
                scaleMode = "full";
                g_fullScaleSamples.fetch_add(1, std::memory_order_relaxed);
            } else {
                g_otherScaleSamples.fetch_add(1, std::memory_order_relaxed);
            }

            HPLPlayerStateSnapshot player;
            const bool playerSnapshotValid = GetHPLPlayerStateSnapshot(player);
            if (player.authoredCameraActive) {
                g_authoredCameraSamples.fetch_add(1, std::memory_order_relaxed);
            }
            const HPLCameraBridgeStatus camera = GetHPLCameraBridgeStatus();
            if (g_config.hplHandScaleNormalization || g_config.hplHandWristPosition
                || g_config.hplHandWristRotation || g_config.hplHandArmIK) {
                ResolveWristTrackingFrame(
                    playerSnapshotValid,
                    player,
                    camera,
                    wristTracking);
            }

            const bool scaleEligible = IsHandScaleEligible(
                playerSnapshotValid, player, camera);
            if (g_config.hplHandScaleNormalization && scaleEligible) {
                const uint64_t scaleAttempt = g_handScaleAttempts.fetch_add(
                    1, std::memory_order_relaxed) + 1;
                if (std::fabs(averageScale - g_config.hplHandTargetScale)
                    <= kQuarterScaleTolerance) {
                    rootAlreadyFullScale = true;
                    g_handScaleNativeFull.fetch_add(1, std::memory_order_relaxed);
                } else if (hands_math::NormalizeUniformScale(
                        matrix,
                        kQuarterScale,
                        g_config.hplHandTargetScale,
                        kQuarterScaleTolerance,
                        normalizedHandsMatrix)) {
                    submittedMatrix = normalizedHandsMatrix.data();
                    rootScaleNormalized = true;
                    g_handScaleNormalizations.fetch_add(1, std::memory_order_relaxed);
                } else {
                    g_handScaleFallbacks.fetch_add(1, std::memory_order_relaxed);
                }
                if ((rootScaleNormalized || rootAlreadyFullScale)
                    && g_config.hplHandArmIK
                    && (std::fabs(g_config.hplHandShoulderVerticalOffsetMeters) > 1.0e-5f
                        || std::fabs(g_config.hplHandShoulderBackOffsetMeters) > 1.0e-5f)) {
                    if (!rootScaleNormalized) normalizedHandsMatrix = matrix;
                    const float worldUnitsPerMeter = std::max(camera.worldUnitsPerMeter, 0.001f);
                    normalizedHandsMatrix[7] +=
                        g_config.hplHandShoulderVerticalOffsetMeters * worldUnitsPerMeter;
                    if (camera.nativeCameraBasisValid) {
                        normalizedHandsMatrix[3] -= camera.nativeCameraForwardX
                            * g_config.hplHandShoulderBackOffsetMeters * worldUnitsPerMeter;
                        normalizedHandsMatrix[11] -= camera.nativeCameraForwardZ
                            * g_config.hplHandShoulderBackOffsetMeters * worldUnitsPerMeter;
                    }
                    submittedMatrix = normalizedHandsMatrix.data();
                    shoulderOffsetApplied = true;
                    g_handShoulderOffsets.fetch_add(1, std::memory_order_relaxed);
                }
                const uint64_t interval = static_cast<uint64_t>(
                    std::max(g_config.hplControllerLogInterval, 1));
                if (scaleAttempt <= 12 || scaleAttempt % interval == 0) {
                    Logger::Instance().Write(
                        rootScaleNormalized || rootAlreadyFullScale
                            ? LogLevel::Info : LogLevel::Warn,
                        "hpl_hands_scale attempt=%llu frame=%llu entity=%p sourceScale=%.4f,%.4f,%.4f "
                        "targetScale=%.4f normalized=%d nativeFullScale=%d rootRotationPreserved=1 "
                        "shoulderOffsetApplied=%d shoulderVerticalOffsetMeters=%.3f shoulderBackOffsetMeters=%.3f "
                        "policy=physical_state_root_scale_independent_of_controller_availability",
                        static_cast<unsigned long long>(scaleAttempt),
                        static_cast<unsigned long long>(player.frame),
                        entity,
                        scaleX, scaleY, scaleZ,
                        g_config.hplHandTargetScale,
                        rootScaleNormalized ? 1 : 0,
                        rootAlreadyFullScale ? 1 : 0,
                        shoulderOffsetApplied ? 1 : 0,
                        g_config.hplHandShoulderVerticalOffsetMeters,
                        g_config.hplHandShoulderBackOffsetMeters);
                }
            }

            OpenXRInputSnapshot input;
            HPLTrackedPoseWorld worldGrip;
            uint32_t handIndex = 1;
            bool gripValid = false;
            uint64_t inputAge = UINT64_MAX;
            if (g_openxr != nullptr
                && g_openxr->GetLatestInput(input)
                && input.active) {
                const OpenXRHandInput* hand = SelectDominantHand(input, handIndex);
                gripValid = hand != nullptr
                    && hand->gripPose.valid
                    && ResolveHPLTrackedPoseWorld(hand->gripPose, input.gameFrame, worldGrip)
                    && worldGrip.orientationTracked
                    && worldGrip.positionTracked;
                if (player.frame != 0 && input.gameFrame != 0) {
                    inputAge = player.frame >= input.gameFrame
                        ? player.frame - input.gameFrame
                        : 0;
                }
            }
            if (gripValid) g_trackedGripSamples.fetch_add(1, std::memory_order_relaxed);

            const float positionX = matrix[3];
            const float positionY = matrix[7];
            const float positionZ = matrix[11];
            const float cameraDx = camera.cameraWorldPositionValid
                ? positionX - camera.cameraWorldPositionX : 0.0f;
            const float cameraDy = camera.cameraWorldPositionValid
                ? positionY - camera.cameraWorldPositionY : 0.0f;
            const float cameraDz = camera.cameraWorldPositionValid
                ? positionZ - camera.cameraWorldPositionZ : 0.0f;
            const float cameraDistance = camera.cameraWorldPositionValid
                ? std::sqrt(cameraDx * cameraDx + cameraDy * cameraDy + cameraDz * cameraDz) : -1.0f;
            const float gripDx = gripValid ? positionX - worldGrip.positionX : 0.0f;
            const float gripDy = gripValid ? positionY - worldGrip.positionY : 0.0f;
            const float gripDz = gripValid ? positionZ - worldGrip.positionZ : 0.0f;
            const float gripDistance = gripValid
                ? std::sqrt(gripDx * gripDx + gripDy * gripDy + gripDz * gripDz) : -1.0f;

            bool rootOverridden = false;
            const bool uniformScale = std::fabs(scaleX - averageScale) <= kUniformScaleTolerance
                && std::fabs(scaleY - averageScale) <= kUniformScaleTolerance
                && std::fabs(scaleZ - averageScale) <= kUniformScaleTolerance;
            if (g_config.hplHandControllerRoot) {
                g_rootOverrideAttempts.fetch_add(1, std::memory_order_relaxed);
                if (!uniformScale
                    || std::fabs(averageScale - kQuarterScale) > kQuarterScaleTolerance) {
                    g_rootScaleFallbacks.fetch_add(1, std::memory_order_relaxed);
                } else if (player.authoredCameraActive) {
                    g_rootAuthoredFallbacks.fetch_add(1, std::memory_order_relaxed);
                } else if (!playerSnapshotValid
                    || !player.playerValid
                    || player.playerStateId != kNormalPlayerState
                    || player.moveStateId != kNormalMoveState) {
                    g_rootStateFallbacks.fetch_add(1, std::memory_order_relaxed);
                } else if (!gripValid || !camera.trackingEnabled) {
                    g_rootPoseFallbacks.fetch_add(1, std::memory_order_relaxed);
                } else if (inputAge > static_cast<uint64_t>(g_config.hplControllerMaxInputAgeFrames)) {
                    g_rootStaleFallbacks.fetch_add(1, std::memory_order_relaxed);
                } else {
                    const hands_math::HandRootCalibration calibration{
                        {
                            g_config.hplHandRootOffsetX,
                            g_config.hplHandRootOffsetY,
                            g_config.hplHandRootOffsetZ,
                        },
                        {
                            g_config.hplHandRootPitchDegrees,
                            g_config.hplHandRootYawDegrees,
                            g_config.hplHandRootRollDegrees,
                        },
                    };
                    rootOverridden = hands_math::BuildControllerHandMatrix(
                        {worldGrip.positionX, worldGrip.positionY, worldGrip.positionZ},
                        {worldGrip.forwardX, worldGrip.forwardY, worldGrip.forwardZ},
                        {worldGrip.upX, worldGrip.upY, worldGrip.upZ},
                        averageScale,
                        calibration,
                        controllerMatrix);
                    if (rootOverridden) {
                        submittedMatrix = controllerMatrix.data();
                        g_rootOverrides.fetch_add(1, std::memory_order_relaxed);
                    } else {
                        g_rootMathFallbacks.fetch_add(1, std::memory_order_relaxed);
                    }
                }
            }

            const uint64_t interval = static_cast<uint64_t>(std::max(g_config.hplControllerLogInterval, 1));
            if (handCall <= 12 || handCall % interval == 0) {
                Logger::Instance().Write(
                    LogLevel::Info,
                    "hpl_hands_pose call=%llu totalCall=%llu entity=%p name=%s matrix=%p pos=%.4f,%.4f,%.4f scale=%.4f,%.4f,%.4f scaleMode=%s right=%.5f,%.5f,%.5f up=%.5f,%.5f,%.5f forward=%.5f,%.5f,%.5f cameraValid=%d cameraDistance=%.4f gripValid=%d gripHand=%s gripPos=%.4f,%.4f,%.4f gripForward=%.5f,%.5f,%.5f gripDistance=%.4f inputAge=%llu rootRequested=%d rootOverridden=%d rootPos=%.4f,%.4f,%.4f scaleNormalizationRequested=%d scaleNormalized=%d targetScale=%.4f wristPositionRequested=%d wristEligible=%d authoredCamera=%d playerState=%d moveState=%d",
                    static_cast<unsigned long long>(handCall),
                    static_cast<unsigned long long>(call),
                    entity,
                    identity.name.c_str(),
                    matrixPointer,
                    positionX, positionY, positionZ,
                    scaleX, scaleY, scaleZ,
                    scaleMode,
                    matrix[0], matrix[4], matrix[8],
                    matrix[1], matrix[5], matrix[9],
                    matrix[2], matrix[6], matrix[10],
                    camera.cameraWorldPositionValid ? 1 : 0,
                    cameraDistance,
                    gripValid ? 1 : 0,
                    handIndex == 0 ? "left" : "right",
                    worldGrip.positionX, worldGrip.positionY, worldGrip.positionZ,
                    worldGrip.forwardX, worldGrip.forwardY, worldGrip.forwardZ,
                    gripDistance,
                    static_cast<unsigned long long>(inputAge),
                    g_config.hplHandControllerRoot ? 1 : 0,
                    rootOverridden ? 1 : 0,
                    rootOverridden ? controllerMatrix[3] : positionX,
                    rootOverridden ? controllerMatrix[7] : positionY,
                    rootOverridden ? controllerMatrix[11] : positionZ,
                    g_config.hplHandScaleNormalization ? 1 : 0,
                    rootScaleNormalized ? 1 : 0,
                    g_config.hplHandTargetScale,
                    g_config.hplHandWristPosition ? 1 : 0,
                    wristTracking.stateEligible ? 1 : 0,
                    player.authoredCameraActive ? 1 : 0,
                    player.playerStateId,
                    player.moveStateId);
            }
        }
    } else if (identity.socketedHudObject
        && identity.name == kMedicineEntityName
        && g_config.hplHandSocketedPropStabilization) {
        std::array<float, 16> nativeMatrix{};
        if (ReadMemory(matrixPointer, nativeMatrix.data(), sizeof(nativeMatrix))
            && BuildStableSocketedPropMatrix(entity, nativeMatrix, controllerMatrix)) {
            submittedMatrix = controllerMatrix.data();
            const uint64_t overrideCount = g_socketedPropOverrides.fetch_add(
                1, std::memory_order_relaxed) + 1;
            const uint64_t interval = static_cast<uint64_t>(
                std::max(g_config.hplControllerLogInterval, 1));
            if (overrideCount <= 8 || overrideCount % interval == 0) {
                Logger::Instance().Write(
                    LogLevel::Info,
                    "hpl_socketed_prop_stabilized override=%llu entity=%p name=%s nativePos=%.4f,%.4f,%.4f finalPos=%.4f,%.4f,%.4f policy=right_grip_relative_transform",
                    static_cast<unsigned long long>(overrideCount),
                    entity,
                    identity.name.c_str(),
                    nativeMatrix[3], nativeMatrix[7], nativeMatrix[11],
                    controllerMatrix[3], controllerMatrix[7], controllerMatrix[11]);
            }
        } else {
            g_socketedPropFallbacks.fetch_add(1, std::memory_order_relaxed);
        }
    } else if (identity.hudObject) {
        const uint64_t hudObjectCall = g_hudObjectCalls.fetch_add(1, std::memory_order_relaxed) + 1;
        std::array<float, 16> matrix{};
        const bool matrixValid = ReadMemory(matrixPointer, matrix.data(), sizeof(matrix));
        if (!matrixValid) {
            g_matrixReadFailures.fetch_add(1, std::memory_order_relaxed);
        }
        const float scaleX = matrixValid ? ColumnLength(matrix, 0) : 0.0f;
        const float scaleY = matrixValid ? ColumnLength(matrix, 1) : 0.0f;
        const float scaleZ = matrixValid ? ColumnLength(matrix, 2) : 0.0f;
        const float averageScale = (scaleX + scaleY + scaleZ) / 3.0f;
        const bool uniformScale = matrixValid
            && std::fabs(scaleX - averageScale) <= kUniformScaleTolerance
            && std::fabs(scaleY - averageScale) <= kUniformScaleTolerance
            && std::fabs(scaleZ - averageScale) <= kUniformScaleTolerance;

        HPLPlayerStateSnapshot player{};
        const bool playerSnapshotValid = GetHPLPlayerStateSnapshot(player);
        const HPLCameraBridgeStatus camera = GetHPLCameraBridgeStatus();
        OpenXRInputSnapshot input{};
        HPLTrackedPoseWorld worldGrip{};
        HPLTrackedPoseWorld worldSupport{};
        two_hand_math::TwoHandBasis twoHandBasis{};
        uint32_t handIndex = 1;
        uint32_t supportHandIndex = 0;
        bool gripValid = false;
        bool twoHandRequested = false;
        bool twoHandValid = false;
        float supportSqueeze = 0.0f;
        uint64_t inputAge = UINT64_MAX;
        if (g_openxr != nullptr && g_openxr->GetLatestInput(input) && input.active) {
            const OpenXRHandInput* hand = SelectDominantHand(input, handIndex);
            gripValid = hand != nullptr
                && hand->gripPose.valid
                && ResolveHPLTrackedPoseWorld(hand->gripPose, input.gameFrame, worldGrip)
                && worldGrip.orientationTracked
                && worldGrip.positionTracked;
            supportHandIndex = handIndex ^ 1u;
            const OpenXRHandInput* support = supportHandIndex == 0 ? &input.left : &input.right;
            supportSqueeze = support->squeeze;
            twoHandRequested = g_config.hplControllerTwoHandHudObject
                && gripValid
                && support->active
                && supportSqueeze >= g_config.hplControllerTwoHandSqueezeThreshold;
            if (twoHandRequested) {
                g_twoHandHudCandidates.fetch_add(1, std::memory_order_relaxed);
                twoHandValid = support->gripPose.valid
                    && support->gripPose.orientationTracked
                    && support->gripPose.positionTracked
                    && ResolveHPLTrackedPoseWorld(
                        support->gripPose, input.gameFrame, worldSupport)
                    && worldSupport.positionTracked
                    && two_hand_math::BuildTwoHandBasis(
                        {worldGrip.positionX, worldGrip.positionY, worldGrip.positionZ},
                        {worldGrip.forwardX, worldGrip.forwardY, worldGrip.forwardZ},
                        {worldGrip.upX, worldGrip.upY, worldGrip.upZ},
                        {worldSupport.positionX, worldSupport.positionY, worldSupport.positionZ},
                        g_config.hplControllerTwoHandDirectionBlend,
                        g_config.hplControllerTwoHandMinSeparationMeters
                            * g_config.hplWorldScale,
                        g_config.hplControllerTwoHandMaxSeparationMeters
                            * g_config.hplWorldScale,
                        twoHandBasis);
                if (!twoHandValid) {
                    g_twoHandHudFallbacks.fetch_add(1, std::memory_order_relaxed);
                }
            }
            if (player.frame != 0 && input.gameFrame != 0) {
                inputAge = player.frame >= input.gameFrame ? player.frame - input.gameFrame : 0;
            }
        }

        bool hudObjectOverridden = false;
        if (g_config.hplControllerHudObject) {
            g_hudObjectOverrideAttempts.fetch_add(1, std::memory_order_relaxed);
            if (!uniformScale || averageScale <= 0.0f) {
                g_hudObjectScaleFallbacks.fetch_add(1, std::memory_order_relaxed);
            } else if (!playerSnapshotValid || !player.playerValid) {
                g_hudObjectStateFallbacks.fetch_add(1, std::memory_order_relaxed);
            } else if (player.authoredCameraActive
                && g_config.hplControllerSuppressDuringAuthoredCamera) {
                g_hudObjectAuthoredFallbacks.fetch_add(1, std::memory_order_relaxed);
            } else if (!gripValid || !camera.trackingEnabled) {
                g_hudObjectPoseFallbacks.fetch_add(1, std::memory_order_relaxed);
            } else if (inputAge > static_cast<uint64_t>(g_config.hplControllerMaxInputAgeFrames)) {
                g_hudObjectStaleFallbacks.fetch_add(1, std::memory_order_relaxed);
            } else {
                const hands_math::HudObjectCalibration calibration{
                    {
                        g_config.hplHudObjectOffsetX,
                        g_config.hplHudObjectOffsetY,
                        g_config.hplHudObjectOffsetZ,
                    },
                    {
                        g_config.hplHudObjectPitchDegrees,
                        g_config.hplHudObjectYawDegrees,
                        g_config.hplHudObjectRollDegrees,
                    },
                };
                hudObjectOverridden = hands_math::BuildControllerHudObjectMatrix(
                    {worldGrip.positionX, worldGrip.positionY, worldGrip.positionZ},
                    twoHandValid
                        ? twoHandBasis.forward
                        : camera_math::Vector3{
                            worldGrip.forwardX, worldGrip.forwardY, worldGrip.forwardZ},
                    twoHandValid
                        ? twoHandBasis.up
                        : camera_math::Vector3{
                            worldGrip.upX, worldGrip.upY, worldGrip.upZ},
                    averageScale,
                    calibration,
                    controllerMatrix);
                if (hudObjectOverridden) {
                    submittedMatrix = controllerMatrix.data();
                    g_hudObjectOverrides.fetch_add(1, std::memory_order_relaxed);
                    if (twoHandValid) {
                        g_twoHandHudOverrides.fetch_add(1, std::memory_order_relaxed);
                    }
                } else {
                    g_hudObjectMathFallbacks.fetch_add(1, std::memory_order_relaxed);
                }
            }
        }

        const uint64_t interval = static_cast<uint64_t>(std::max(g_config.hplControllerLogInterval, 1));
        if (hudObjectCall <= 12 || hudObjectCall % interval == 0) {
            Logger::Instance().Write(
                LogLevel::Info,
                "hpl_hud_object_pose call=%llu entity=%p matrixValid=%d nativePos=%.4f,%.4f,%.4f scale=%.4f,%.4f,%.4f gripValid=%d gripHand=%s gripPos=%.4f,%.4f,%.4f inputAge=%llu requested=%d overridden=%d finalPos=%.4f,%.4f,%.4f twoHand={enabled=%d requested=%d valid=%d supportHand=%s squeeze=%.3f separation=%.4f blend=%.3f} tracking=%d authoredCamera=%d playerState=%d moveState=%d",
                static_cast<unsigned long long>(hudObjectCall),
                entity,
                matrixValid ? 1 : 0,
                matrixValid ? matrix[3] : 0.0f,
                matrixValid ? matrix[7] : 0.0f,
                matrixValid ? matrix[11] : 0.0f,
                scaleX, scaleY, scaleZ,
                gripValid ? 1 : 0,
                handIndex == 0 ? "left" : "right",
                worldGrip.positionX, worldGrip.positionY, worldGrip.positionZ,
                static_cast<unsigned long long>(inputAge),
                g_config.hplControllerHudObject ? 1 : 0,
                hudObjectOverridden ? 1 : 0,
                hudObjectOverridden ? controllerMatrix[3] : (matrixValid ? matrix[3] : 0.0f),
                hudObjectOverridden ? controllerMatrix[7] : (matrixValid ? matrix[7] : 0.0f),
                hudObjectOverridden ? controllerMatrix[11] : (matrixValid ? matrix[11] : 0.0f),
                g_config.hplControllerTwoHandHudObject ? 1 : 0,
                twoHandRequested ? 1 : 0,
                twoHandValid ? 1 : 0,
                supportHandIndex == 0 ? "left" : "right",
                supportSqueeze,
                twoHandBasis.separation,
                g_config.hplControllerTwoHandDirectionBlend,
                camera.trackingEnabled ? 1 : 0,
                player.authoredCameraActive ? 1 : 0,
                player.playerStateId,
                player.moveStateId);
        }
    } else if (identity.flashlight) {
        const uint64_t flashlightCall = g_flashlightCalls.fetch_add(1, std::memory_order_relaxed) + 1;
        HPLPlayerStateSnapshot player{};
        const bool playerSnapshotValid = GetHPLPlayerStateSnapshot(player);
        const HPLCameraBridgeStatus camera = GetHPLCameraBridgeStatus();

        OpenXRInputSnapshot input;
        HPLTrackedPoseWorld worldAim;
        uint32_t handIndex = 1;
        bool aimValid = false;
        uint64_t inputAge = UINT64_MAX;
        if (g_openxr != nullptr
            && g_openxr->GetLatestInput(input)
            && input.active) {
            const OpenXRHandInput* hand = SelectDominantHand(input, handIndex, false);
            aimValid = hand != nullptr
                && hand->aimPose.valid
                && ResolveHPLTrackedPoseWorld(hand->aimPose, input.gameFrame, worldAim)
                && worldAim.orientationTracked
                && worldAim.positionTracked;
            if (player.frame != 0 && input.gameFrame != 0) {
                inputAge = player.frame >= input.gameFrame
                    ? player.frame - input.gameFrame
                    : 0;
            }
        }

        bool flashlightOverridden = false;
        if (g_config.hplControllerFlashlightAim) {
            g_flashlightOverrideAttempts.fetch_add(1, std::memory_order_relaxed);
            if (!playerSnapshotValid || !player.playerValid) {
                g_flashlightStateFallbacks.fetch_add(1, std::memory_order_relaxed);
            } else if (player.authoredCameraActive
                && g_config.hplControllerSuppressDuringAuthoredCamera) {
                g_flashlightAuthoredFallbacks.fetch_add(1, std::memory_order_relaxed);
            } else if (!aimValid || !camera.trackingEnabled) {
                g_flashlightPoseFallbacks.fetch_add(1, std::memory_order_relaxed);
            } else if (inputAge > static_cast<uint64_t>(g_config.hplControllerMaxInputAgeFrames)) {
                g_flashlightStaleFallbacks.fetch_add(1, std::memory_order_relaxed);
            } else {
                const flashlight_math::FlashlightCalibration calibration{
                    {
                        g_config.hplFlashlightOffsetX,
                        g_config.hplFlashlightOffsetY,
                        g_config.hplFlashlightOffsetZ,
                    },
                    {
                        g_config.hplFlashlightPitchDegrees,
                        g_config.hplFlashlightYawDegrees,
                        g_config.hplFlashlightRollDegrees,
                    },
                };
                flashlightOverridden = flashlight_math::BuildControllerFlashlightMatrix(
                    {worldAim.positionX, worldAim.positionY, worldAim.positionZ},
                    {worldAim.forwardX, worldAim.forwardY, worldAim.forwardZ},
                    {worldAim.upX, worldAim.upY, worldAim.upZ},
                    calibration,
                    controllerMatrix);
                if (flashlightOverridden) {
                    submittedMatrix = controllerMatrix.data();
                    g_flashlightOverrides.fetch_add(1, std::memory_order_relaxed);
                } else {
                    g_flashlightMathFallbacks.fetch_add(1, std::memory_order_relaxed);
                }
            }
        }
        if (flashlightOverridden) {
            CacheFlashlightPose(controllerMatrix, input.gameFrame);
        } else {
            InvalidateFlashlightPose();
        }

        const uint64_t interval = static_cast<uint64_t>(std::max(g_config.hplControllerLogInterval, 1));
        if (flashlightCall <= 12 || flashlightCall % interval == 0) {
            std::array<float, 16> nativeMatrix{};
            const bool matrixValid = ReadMemory(matrixPointer, nativeMatrix.data(), sizeof(nativeMatrix));
            Logger::Instance().Write(
                LogLevel::Info,
                "hpl_flashlight_pose call=%llu entity=%p matrix=%p nativeMatrixValid=%d nativePos=%.4f,%.4f,%.4f aimValid=%d aimHand=%s aimPos=%.4f,%.4f,%.4f aimForward=%.5f,%.5f,%.5f inputAge=%llu requested=%d overridden=%d finalPos=%.4f,%.4f,%.4f authoredCamera=%d playerState=%d moveState=%d",
                static_cast<unsigned long long>(flashlightCall),
                entity,
                matrixPointer,
                matrixValid ? 1 : 0,
                matrixValid ? nativeMatrix[3] : 0.0f,
                matrixValid ? nativeMatrix[7] : 0.0f,
                matrixValid ? nativeMatrix[11] : 0.0f,
                aimValid ? 1 : 0,
                handIndex == 0 ? "left" : "right",
                worldAim.positionX, worldAim.positionY, worldAim.positionZ,
                worldAim.forwardX, worldAim.forwardY, worldAim.forwardZ,
                static_cast<unsigned long long>(inputAge),
                g_config.hplControllerFlashlightAim ? 1 : 0,
                flashlightOverridden ? 1 : 0,
                flashlightOverridden ? controllerMatrix[3] : (matrixValid ? nativeMatrix[3] : 0.0f),
                flashlightOverridden ? controllerMatrix[7] : (matrixValid ? nativeMatrix[7] : 0.0f),
                flashlightOverridden ? controllerMatrix[11] : (matrixValid ? nativeMatrix[11] : 0.0f),
                player.authoredCameraActive ? 1 : 0,
                player.playerStateId,
                player.moveStateId);
        }
    } else {
        HPLPlayerStateSnapshot player{};
        if (GetHPLPlayerStateSnapshot(player) && player.playerValid) {
            const uint64_t readSession = UpdateReadPresentationSession(player);
            if (player.playerStateId == static_cast<int>(HPLPlayerStateKind::Read)) {
                g_readCandidateCalls.fetch_add(1, std::memory_order_relaxed);
                std::array<float, 16> matrix{};
                const HPLCameraBridgeStatus camera = GetHPLCameraBridgeStatus();
                if (ReadMemory(matrixPointer, matrix.data(), sizeof(matrix)) &&
                    camera.cameraWorldPositionValid) {
                    const float dx = matrix[3] - camera.cameraWorldPositionX;
                    const float dy = matrix[7] - camera.cameraWorldPositionY;
                    const float dz = matrix[11] - camera.cameraWorldPositionZ;
                    const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
                    bool existingAnchor = false;
                    bool presentationOwner = false;
                    {
                        std::lock_guard lock(g_identityMutex);
                        const auto found = g_readPresentationAnchors.find(entity);
                        existingAnchor = found != g_readPresentationAnchors.end() &&
                                         found->second.session == readSession;
                        if (g_readPresentationOwner == nullptr
                            && IsReadPresentationCandidate(identity)
                            && std::isfinite(distance)
                            && distance > 0.05f && distance <= 1.5f) {
                            g_readPresentationOwner = entity;
                        }
                        presentationOwner = g_readPresentationOwner == entity;
                    }
                    if (g_config.hplControllerReadPresentation && presentationOwner
                        && std::isfinite(distance) &&
                        distance > 0.05f && (distance <= 1.5f || existingAnchor)) {
                        OpenXRInputSnapshot input{};
                        HPLTrackedPoseWorld worldGrip{};
                        camera_math::Quaternion gripOrientation{};
                        uint32_t handIndex = 1;
                        const OpenXRHandInput* hand = nullptr;
                        const bool gripValid =
                            g_openxr != nullptr && g_openxr->GetLatestInput(input) &&
                            input.active &&
                            (hand = SelectDominantHand(input, handIndex)) != nullptr &&
                            hand->gripPose.valid && hand->gripPose.orientationTracked &&
                            ResolveHPLTrackedPoseWorld(hand->gripPose, input.gameFrame,
                                                       worldGrip) &&
                            worldGrip.orientationTracked &&
                            camera_math::QuaternionFromForwardUp(
                                {
                                    worldGrip.forwardX,
                                    worldGrip.forwardY,
                                    worldGrip.forwardZ,
                                },
                                {
                                    worldGrip.upX,
                                    worldGrip.upY,
                                    worldGrip.upZ,
                                },
                                gripOrientation);
                        std::array<float, 16> anchoredSourceMatrix{};
                        camera_math::Vector3 sourceCameraPosition{};
                        camera_math::Vector3 presentationOffset{};
                        camera_math::Vector3 currentPresentationPosition{};
                        camera_math::Vector3 currentPresentationForward{};
                        camera_math::Quaternion currentViewOrientation{};
                        camera_math::Quaternion presentationOrientation{};
                        bool presentationUsesTrackedHead = false;
                        const bool presentationViewValid = ResolveReadPresentationView(
                            camera,
                            currentPresentationPosition,
                            currentPresentationForward,
                            currentViewOrientation,
                            presentationUsesTrackedHead);
                        bool usePresentationOrientation = false;
                        bool rotateActive = false;
                        bool anchorSeeded = false;
                        bool presentationSettled = false;
                        bool presentationReady = false;
                        uint64_t warmupAge = 0;
                        {
                            std::lock_guard lock(g_identityMutex);
                            ReadPresentationAnchor& anchor = g_readPresentationAnchors[entity];
                            if (anchor.session != readSession) {
                                anchor = {};
                                anchor.session = readSession;
                            }
                            if (presentationViewValid && anchor.viewOrientationValid
                                && (anchor.presentationSeeded || anchor.manipulated)) {
                                anchor.objectOrientation = read_math::ResolveRelativeOrientation(
                                    anchor.viewOrientation, currentViewOrientation,
                                    anchor.objectOrientation);
                                anchor.gripOrientation = read_math::ResolveRelativeOrientation(
                                    anchor.viewOrientation, currentViewOrientation,
                                    anchor.gripOrientation);
                            }
                            if (presentationViewValid) {
                                anchor.viewOrientation = currentViewOrientation;
                                anchor.viewOrientationValid = true;
                            }
                            if (!anchor.presentationSeeded) {
                                if (!anchor.warmupStarted) {
                                    anchor.warmupStarted = true;
                                    anchor.firstFrame = player.frame;
                                }
                                if (!anchor.presentationOffsetValid
                                    && presentationViewValid) {
                                    anchor.sourceMatrix = matrix;
                                    anchor.sourceCameraPosition =
                                        currentPresentationPosition;
                                    anchor.presentationOffsetValid =
                                        presentationViewValid
                                        && read_math::BuildStablePresentationOffset(
                                            currentPresentationForward,
                                            Distance(
                                                MatrixPosition(matrix),
                                                currentPresentationPosition),
                                            g_config.hplControllerReadObjectDistanceScale,
                                            anchor.presentationOffset);
                                }
                                if (!anchor.manipulated) {
                                    camera_math::Quaternion currentOrientation{};
                                    if (camera_math::QuaternionFromRotationMatrix(
                                            matrix, currentOrientation)) {
                                        anchor.objectOrientation = currentOrientation;
                                        anchor.objectOrientationValid = true;
                                    }
                                }
                                warmupAge = player.frame >= anchor.firstFrame
                                    ? player.frame - anchor.firstFrame : 0;
                                if (warmupAge >= static_cast<uint64_t>(
                                        g_config.hplControllerReadObjectSettleFrames)) {
                                    anchor.presentationSeeded = true;
                                    anchorSeeded = true;
                                }
                            }
                            warmupAge = anchor.warmupStarted
                                && player.frame >= anchor.firstFrame
                                ? player.frame - anchor.firstFrame : 0;
                            presentationSettled = anchor.presentationSeeded;
                            presentationReady = anchor.presentationOffsetValid;
                            anchoredSourceMatrix = anchor.sourceMatrix;
                            sourceCameraPosition = anchor.sourceCameraPosition;
                            // Latch distance once, but resolve direction in the current player view.
                            presentationReady = presentationReady && presentationViewValid
                                && read_math::BuildStablePresentationOffset(
                                    currentPresentationForward,
                                    Distance({}, anchor.presentationOffset), 1.0f,
                                    presentationOffset);
                            const float squeeze = hand != nullptr ? hand->squeeze : 0.0f;
                            const bool rotateRequested = presentationSettled
                                && gripValid
                                && squeeze >= (anchor.rotateActive ? 0.55f : 0.75f);
                            if (rotateRequested && anchor.objectOrientationValid) {
                                if (!anchor.rotateActive) {
                                    anchor.gripOrientation = gripOrientation;
                                }
                                anchor.objectOrientation = read_math::ResolveRelativeOrientation(
                                    anchor.gripOrientation, gripOrientation,
                                    anchor.objectOrientation);
                                anchor.gripOrientation = gripOrientation;
                                anchor.manipulated = true;
                                anchor.rotateActive = true;
                            } else {
                                anchor.rotateActive = false;
                            }
                            anchor.lastFrame = player.frame;
                            rotateActive = anchor.rotateActive;
                            if (anchor.objectOrientationValid) {
                                presentationOrientation = anchor.objectOrientation;
                                usePresentationOrientation = true;
                            }
                        }
                        if (presentationReady && presentationViewValid
                            && read_math::BuildReadPresentationMatrix(
                                anchoredSourceMatrix, g_config.hplControllerReadObjectScale,
                                usePresentationOrientation ? &presentationOrientation : nullptr,
                                controllerMatrix)) {
                            const camera_math::Vector3 presentationPosition{
                                currentPresentationPosition.x + presentationOffset.x,
                                currentPresentationPosition.y + presentationOffset.y,
                                currentPresentationPosition.z + presentationOffset.z,
                            };
                            controllerMatrix[3] = presentationPosition.x;
                            controllerMatrix[7] = presentationPosition.y;
                            controllerMatrix[11] = presentationPosition.z;
                            submittedMatrix = controllerMatrix.data();
                            const uint64_t overrideCount = g_readPresentationOverrides.fetch_add(
                                                               1, std::memory_order_relaxed) +
                                                           1;
                            const uint64_t interval = static_cast<uint64_t>(
                                std::max(g_config.hplControllerLogInterval, 1));
                            if (overrideCount <= 8 || overrideCount % interval == 0) {
                                Logger::Instance().Write(
                                    LogLevel::Info,
                                    "hpl_read_presentation override=%llu session=%llu frame=%llu "
                                    "entity=%p name=%s "
                                    "incomingDistance=%.4f sourceDistance=%.4f anchorSeeded=%d settled=%d entryOverride=%d settleAge=%llu settleFrames=%d "
                                    "configuredDistanceScale=%.3f scaleMultiplier=%.3f "
                                    "gripValid=%d gripHand=%s "
                                    "squeeze=%.3f rotateActive=%d orientationOverride=%d "
                                    "viewAnchor=%s finalPos=%.4f,%.4f,%.4f "
                                    "policy=latched_distance_current_scene_view_settling_native_orientation_non_recursive_controller_orientation",
                                    static_cast<unsigned long long>(overrideCount),
                                    static_cast<unsigned long long>(readSession),
                                    static_cast<unsigned long long>(player.frame), entity,
                                    identity.name.c_str(), distance,
                                    Distance(MatrixPosition(anchoredSourceMatrix),
                                             sourceCameraPosition),
                                    anchorSeeded ? 1 : 0,
                                    presentationSettled ? 1 : 0,
                                    presentationSettled ? 0 : 1,
                                    static_cast<unsigned long long>(warmupAge),
                                    g_config.hplControllerReadObjectSettleFrames,
                                    g_config.hplControllerReadObjectDistanceScale,
                                    g_config.hplControllerReadObjectScale, gripValid ? 1 : 0,
                                    handIndex == 0 ? "left" : "right",
                                    hand != nullptr ? hand->squeeze : 0.0f, rotateActive ? 1 : 0,
                                    usePresentationOrientation ? 1 : 0,
                                    presentationUsesTrackedHead ? "tracked_head" : "camera",
                                    controllerMatrix[3],
                                    controllerMatrix[7], controllerMatrix[11]);
                            }
                        } else if (presentationReady) {
                            g_readPresentationFallbacks.fetch_add(1, std::memory_order_relaxed);
                        }
                    }
                    bool firstCandidate = false;
                    {
                        std::lock_guard lock(g_identityMutex);
                        if (g_readCandidateEntities.size() < 64 && distance <= 1.5f) {
                            firstCandidate = g_readCandidateEntities.insert(entity).second;
                        }
                    }
                    if (firstCandidate) {
                        const uint64_t unique =
                            g_readCandidateUnique.fetch_add(1, std::memory_order_relaxed) + 1;
                        Logger::Instance().Write(
                            LogLevel::Info,
                            "hpl_read_entity_candidate unique=%llu frame=%llu entity=%p name=%s "
                            "cameraDistance=%.4f "
                            "pos=%.4f,%.4f,%.4f scale=%.4f,%.4f,%.4f "
                            "presentationOwner=%d candidateEligible=%d "
                            "policy=unique_non_special_set_matrix_within_1.5_world_units",
                            static_cast<unsigned long long>(unique),
                            static_cast<unsigned long long>(player.frame), entity,
                            identity.name.c_str(), distance, matrix[3], matrix[7], matrix[11],
                            ColumnLength(matrix, 0), ColumnLength(matrix, 1),
                            ColumnLength(matrix, 2),
                            presentationOwner ? 1 : 0,
                            IsReadPresentationCandidate(identity) ? 1 : 0);
                    }
                }
            }
        }
    }

    std::array<float, 16> bodyAnchoredHandsMatrix{};
    bool bodyAnchorApplied = false;
    if (identity.playerHands && ResolveRetainedHandsMode() == RetainedHandsMode::Active) {
        RetainedHandsState retained{};
        {
            std::lock_guard lock(g_retainedHandsMutex);
            if (g_retainedHands.valid && g_retainedHands.entity == entity) {
                retained = g_retainedHands;
            }
        }
        camera_math::Quaternion bodyYaw{};
        camera_math::Vector3 shoulderAnchorPosition{};
        bool shoulderAnchorUsesTrackedHead = false;
        if (retained.valid && BuildBodyAnchoredHandsMatrix(
                retained,
                GetHPLCameraBridgeStatus(),
                bodyAnchoredHandsMatrix,
                bodyYaw,
                shoulderAnchorPosition,
                shoulderAnchorUsesTrackedHead)) {
            std::array<float, 16> acceptedHandsMatrix{};
            std::array<float, 16> scaleReconciledHandsMatrix{};
            const bool acceptedMatrixValid = ReadMemory(
                submittedMatrix,
                acceptedHandsMatrix.data(),
                sizeof(acceptedHandsMatrix));
            const float acceptedScale = acceptedMatrixValid
                ? (ColumnLength(acceptedHandsMatrix, 0)
                    + ColumnLength(acceptedHandsMatrix, 1)
                    + ColumnLength(acceptedHandsMatrix, 2)) / 3.0f
                : 0.0f;
            const float anchoredScale = (ColumnLength(bodyAnchoredHandsMatrix, 0)
                + ColumnLength(bodyAnchoredHandsMatrix, 1)
                + ColumnLength(bodyAnchoredHandsMatrix, 2)) / 3.0f;
            if (acceptedMatrixValid
                && std::fabs(acceptedScale - anchoredScale) > kQuarterScaleTolerance
                && hands_math::ApplyRootBasisScale(
                    acceptedHandsMatrix,
                    bodyAnchoredHandsMatrix,
                    scaleReconciledHandsMatrix)) {
                bodyAnchoredHandsMatrix = scaleReconciledHandsMatrix;
                const uint64_t repair = g_bodyAnchorScaleRepairs.fetch_add(
                    1, std::memory_order_relaxed) + 1;
                const uint64_t interval = static_cast<uint64_t>(
                    std::max(g_config.hplControllerLogInterval, 1));
                if (repair <= 8 || repair % interval == 0) {
                    Logger::Instance().Write(
                        LogLevel::Warn,
                        "hpl_hands_body_anchor_scale repair=%llu entity=%p acceptedScale=%.4f staleRetainedScale=%.4f finalScale=%.4f policy=body_anchor_pose_cannot_override_current_scale",
                        static_cast<unsigned long long>(repair),
                        entity,
                        acceptedScale,
                        anchoredScale,
                        (ColumnLength(bodyAnchoredHandsMatrix, 0)
                            + ColumnLength(bodyAnchoredHandsMatrix, 1)
                            + ColumnLength(bodyAnchoredHandsMatrix, 2)) / 3.0f);
                }
            }
            submittedMatrix = bodyAnchoredHandsMatrix.data();
            bodyAnchorApplied = true;
        }
    }
    g_originalSetMatrix(entity, submittedMatrix);
    HPLPlayerStateSnapshot authoredPlayer;
    GetHPLPlayerStateSnapshot(authoredPlayer);
    ObserveHPLAuthoredInteractionEntity(
        identity.name, entity, submittedMatrix, authoredPlayer.frame);
    if (identity.playerHands) {
        const HPLCameraBridgeStatus retainedCamera = GetHPLCameraBridgeStatus();
        void* retainedMesh = g_getMeshEntity != nullptr ? g_getMeshEntity(entity) : nullptr;
        const RetainedHandsMode retentionMode = ResolveRetainedHandsMode();
        bool firstNativeSeed = false;
        bool firstNativeSeedUsesTrackedHead = false;
        if (retentionMode != RetainedHandsMode::Invalid) {
            camera_math::Quaternion retainedBodyYaw{};
            const bool retainedBodyYawValid = ResolveHandsBodyYaw(
                retainedCamera, retainedBodyYaw);
            camera_math::Vector3 retainedShoulderAnchor{};
            bool retainedShoulderAnchorUsesTrackedHead = false;
            const bool retainedShoulderAnchorValid = ResolveShoulderAnchorPosition(
                retainedCamera,
                retainedShoulderAnchor,
                retainedShoulderAnchorUsesTrackedHead);
            std::lock_guard retainedLock(g_retainedHandsMutex);
            firstNativeSeed = !g_retainedHands.valid
                || g_retainedHands.entity != entity
                || g_retainedHands.mesh != retainedMesh;
            if (firstNativeSeed) {
                firstNativeSeedUsesTrackedHead =
                    retainedShoulderAnchorUsesTrackedHead;
                g_retainedHands = {};
                g_retainedHands.valid = retainedMesh != nullptr;
                g_retainedHands.suspended = retentionMode == RetainedHandsMode::Suspended;
                g_retainedHands.entity = entity;
                g_retainedHands.mesh = retainedMesh;
                std::memcpy(
                    g_retainedHands.matrix.data(), submittedMatrix,
                    sizeof(g_retainedHands.matrix));
                g_retainedHands.shoulderAnchorPositionValid =
                    retainedShoulderAnchorValid;
                g_retainedHands.shoulderAnchorUsesTrackedHead =
                    retainedShoulderAnchorUsesTrackedHead;
                g_retainedHands.shoulderAnchorPosition = retainedShoulderAnchor;
                g_retainedHands.bodyYawValid = retainedBodyYawValid;
                g_retainedHands.bodyYaw = retainedBodyYaw;
            } else if (retentionMode == RetainedHandsMode::Active) {
                g_retainedHands.suspended = false;
                std::memcpy(
                    g_retainedHands.matrix.data(), submittedMatrix,
                    sizeof(g_retainedHands.matrix));
                g_retainedHands.shoulderAnchorPositionValid =
                    retainedShoulderAnchorValid;
                g_retainedHands.shoulderAnchorUsesTrackedHead =
                    retainedShoulderAnchorUsesTrackedHead;
                g_retainedHands.shoulderAnchorPosition = retainedShoulderAnchor;
                g_retainedHands.bodyYawValid = retainedBodyYawValid;
                g_retainedHands.bodyYaw = retainedBodyYaw;
            }
            g_retainedHands.lastNativeFrame = authoredPlayer.frame;
        }
        if (firstNativeSeed && retainedMesh != nullptr) {
            ResetWristOrientationAnchors(entity);
            const uint64_t seed = g_handsNativeSeeds.fetch_add(
                1, std::memory_order_relaxed) + 1;
            Logger::Instance().Write(
                LogLevel::Warn,
                "hpl_hands_native_seed seed=%llu frame=%llu entity=%p mesh=%p name=%s bodyAnchor=%d trackedHeadAnchor=%d policy=visibility_retention_now_armed",
                static_cast<unsigned long long>(seed),
                static_cast<unsigned long long>(authoredPlayer.frame),
                entity,
                retainedMesh,
                identity.name.c_str(),
                bodyAnchorApplied ? 1 : 0,
                firstNativeSeedUsesTrackedHead ? 1 : 0);
        }
    }
    if (identity.playerHands) {
        ApplyPlayerHandsArmIK(entity, wristTracking);
        ApplyPlayerHandsWristPositions(
            entity,
            wristTracking,
            rootScaleNormalized || rootAlreadyFullScale);
        ProbePlayerHandsSkeleton(entity);
    }
}

void HookLuxMapDestroyEntity(void* map, void* entity)
{
    const uint64_t call = g_destroyEntityCalls.fetch_add(1, std::memory_order_relaxed) + 1;
    EntityIdentity invalidatedIdentity;
    bool invalidated = false;
    {
        std::lock_guard lock(g_identityMutex);
        const auto found = g_identityCache.find(entity);
        if (found != g_identityCache.end()) {
            invalidatedIdentity = found->second;
            g_identityCache.erase(found);
            invalidated = true;
        }
        g_readCandidateEntities.erase(entity);
        g_readPresentationAnchors.erase(entity);
        if (g_readPresentationOwner == entity) g_readPresentationOwner = nullptr;
    }
    {
        std::lock_guard socketLock(g_socketedPropMutex);
        g_socketedPropAnchors.erase(entity);
    }
    if (invalidated) {
        g_identityInvalidations.fetch_add(1, std::memory_order_relaxed);
    }
    InvalidateHPLAuthoredInteractionEntity(entity);
    const bool relevant = invalidatedIdentity.playerHands
        || invalidatedIdentity.hudObject
        || invalidatedIdentity.socketedHudObject
        || invalidatedIdentity.flashlight;
    if (invalidatedIdentity.playerHands)
    {
        {
            std::lock_guard retainedLock(g_retainedHandsMutex);
            if (g_retainedHands.entity == entity) g_retainedHands = {};
        }
        {
            std::lock_guard skeletonLock(g_skeletonProbeMutex);
            if (g_lastSkeletonEntity == entity)
            {
                g_lastSkeletonEntity = nullptr;
                g_lastSkeletonMesh = nullptr;
                g_nextSkeletonProbeFrame = 0;
                g_lastSkeletonProbeFrame = UINT64_MAX;
                g_lastSkeletonPlayerState = -1;
                g_skeletonProbeBurstRemaining = 0;
            }
        }
        {
            std::lock_guard wristLock(g_wristMutationMutex);
            if (g_lastWristMutationEntity == entity) {
                g_lastWristMutationEntity = nullptr;
                g_lastWristMutationFrame = UINT64_MAX;
            }
        }
        ResetWristOrientationAnchors(entity);
    }
    if (call <= 12 || relevant) {
        Logger::Instance().Write(
            LogLevel::Info,
            "hpl_hands_entity_destroy call=%llu map=%p entity=%p cacheInvalidated=%d relevant=%d name=%s policy=evict_before_native_destroy_queue",
            static_cast<unsigned long long>(call),
            map,
            entity,
            invalidated ? 1 : 0,
            relevant ? 1 : 0,
            invalidatedIdentity.name.empty() ? "<uncached>" : invalidatedIdentity.name.c_str());
    }
    g_originalDestroyEntity(map, entity);
}

} // namespace

bool ResolveHPLControllerBeamDistance(
    const OpenXRControllerPose& aimPose,
    uint64_t gameFrame,
    float maxDistanceMeters,
    float& distanceMeters)
{
    distanceMeters = 0.0f;
    if (!g_config.hplControllerAimGuideSceneDepth
        || g_originalGetClosestBody == nullptr
        || !aimPose.valid
        || !aimPose.positionTracked
        || !aimPose.orientationTracked
        || !std::isfinite(maxDistanceMeters)
        || maxDistanceMeters <= 0.05f) {
        return false;
    }
    HPLTrackedPoseWorld worldAim{};
    const HPLCameraBridgeStatus camera = GetHPLCameraBridgeStatus();
    if (!camera.trackingEnabled
        || !ResolveHPLTrackedPoseWorld(aimPose, gameFrame, worldAim)
        || !worldAim.positionTracked
        || !worldAim.orientationTracked) {
        return false;
    }
    const float worldUnitsPerMeter = std::max(camera.worldUnitsPerMeter, 0.001f);
    const float rayLength = maxDistanceMeters * worldUnitsPerMeter;
    const float start[3] = {worldAim.positionX, worldAim.positionY, worldAim.positionZ};
    const float direction[3] = {worldAim.forwardX, worldAim.forwardY, worldAim.forwardZ};
    float hitDistance = rayLength;
    float surfaceNormal[3]{};
    g_controllerBeamRayQueries.fetch_add(1, std::memory_order_relaxed);
    void* body = g_originalGetClosestBody(
        start, direction, rayLength, &hitDistance, surfaceNormal);
    if (body == nullptr
        || !std::isfinite(hitDistance)
        || hitDistance <= 0.0f
        || hitDistance > rayLength) {
        return false;
    }
    distanceMeters = hitDistance / worldUnitsPerMeter;
    g_controllerBeamRayHits.fetch_add(1, std::memory_order_relaxed);
    return std::isfinite(distanceMeters) && distanceMeters > 0.0f;
}

bool InstallHPLHandsBridge(const Config& config, OpenXRRuntime* openxr)
{
    std::lock_guard lock(g_installMutex);
    g_config = config;
    g_openxr = openxr;
    ConfigureHPLAuthoredInteractionBridge(config, openxr);
    const bool skeletonAccessRequested = config.hplHandTrackingProbe
        || config.hplHandWristPosition || config.hplHandWristRotation
        || config.hplHandArmIK;
    const bool postMutationRequested = config.hplHandWristPosition
        || config.hplHandWristRotation || config.hplHandArmIK;
    const bool closestBodyRequested = config.hplControllerFlashlightGameplayRay
        || config.hplControllerAimGuideSceneDepth;
    if (!config.hplHandTrackingProbe
        && !config.hplHandControllerRoot
        && !config.hplHandScaleNormalization
        && !config.hplHandWristPosition
        && !config.hplHandWristRotation
        && !config.hplHandArmIK
        && !config.hplHandAlwaysVisible
        && !config.hplAuthoredInteractions
        && !config.hplControllerHudObject
        && !config.hplControllerReadPresentation
        && !config.hplControllerFlashlightAim
        && !closestBodyRequested) {
        Logger::Instance().Write(LogLevel::Info, "hpl_hands_bridge disabled config=0");
        return true;
    }
    if (g_setMatrixTarget != nullptr) return true;
    g_calibrationProfiles.Initialize(
        config, WorkRoot() / "somavr_entity_profiles.ini");

    HMODULE executable = GetModuleHandleW(nullptr);
    if (!IsInsideImage(executable, kLuxEntitySetMatrixRva, sizeof(kLuxEntitySetMatrixSignature))
        || !IsInsideImage(executable, kLuxEntityGetNameRva, sizeof(kLuxEntityGetNameSignature))
        || (skeletonAccessRequested
            && (!IsInsideImage(executable, kLuxEntityGetMeshEntityRva, sizeof(kLuxEntityGetMeshEntitySignature))
                || !IsInsideImage(executable, kMeshEntityGetBoneStateFromNameRva,
                    sizeof(kMeshEntityGetBoneStateFromNameSignature))))
        || (postMutationRequested
            && (!IsInsideImage(executable, soma_signatures::kNodeSetUsePostTransformRva,
                    sizeof(soma_signatures::kNodeSetUsePostTransform))
                || !IsInsideImage(executable, soma_signatures::kNodeSetPostTransformRva,
                    sizeof(soma_signatures::kNodeSetPostTransform))
                || !IsInsideImage(executable, soma_signatures::kNodeSetMatrixRva,
                    sizeof(soma_signatures::kNodeSetMatrix))
                || !IsInsideImage(executable, soma_signatures::kNodeApplyPostAnimTransformRva,
                    sizeof(soma_signatures::kNodeApplyPostAnimTransform))))
        || !IsInsideImage(executable, kLuxMapDestroyEntityRva, sizeof(kLuxMapDestroyEntitySignature))
        || (closestBodyRequested
            && !IsInsideImage(executable, kGetClosestBodyRva, sizeof(kGetClosestBodySignature)))) {
        Logger::Instance().Write(LogLevel::Error, "hpl_hands_bridge install_failed reason=invalid_image_range");
        return false;
    }

    auto* setMatrixTarget = reinterpret_cast<std::byte*>(executable) + kLuxEntitySetMatrixRva;
    auto* getNameTarget = reinterpret_cast<std::byte*>(executable) + kLuxEntityGetNameRva;
    auto* getMeshEntityTarget = reinterpret_cast<std::byte*>(executable) + kLuxEntityGetMeshEntityRva;
    auto* getBoneStateFromNameTarget =
        reinterpret_cast<std::byte*>(executable) + kMeshEntityGetBoneStateFromNameRva;
    auto* setUsePostTransformTarget =
        reinterpret_cast<std::byte*>(executable)
        + soma_signatures::kNodeSetUsePostTransformRva;
    auto* setPostTransformTarget =
        reinterpret_cast<std::byte*>(executable)
        + soma_signatures::kNodeSetPostTransformRva;
    auto* nodeSetMatrixTarget = reinterpret_cast<std::byte*>(executable)
        + soma_signatures::kNodeSetMatrixRva;
    auto* applyPostAnimTransformTarget =
        reinterpret_cast<std::byte*>(executable)
        + soma_signatures::kNodeApplyPostAnimTransformRva;
    auto* destroyEntityTarget = reinterpret_cast<std::byte*>(executable) + kLuxMapDestroyEntityRva;
    auto* getClosestBodyTarget = reinterpret_cast<std::byte*>(executable) + kGetClosestBodyRva;
    if (std::memcmp(setMatrixTarget, kLuxEntitySetMatrixSignature, sizeof(kLuxEntitySetMatrixSignature)) != 0
        || std::memcmp(getNameTarget, kLuxEntityGetNameSignature, sizeof(kLuxEntityGetNameSignature)) != 0
        || (skeletonAccessRequested
            && (std::memcmp(getMeshEntityTarget, kLuxEntityGetMeshEntitySignature,
                    sizeof(kLuxEntityGetMeshEntitySignature)) != 0
                || std::memcmp(getBoneStateFromNameTarget, kMeshEntityGetBoneStateFromNameSignature,
                    sizeof(kMeshEntityGetBoneStateFromNameSignature)) != 0))
        || (postMutationRequested
            && (std::memcmp(setUsePostTransformTarget,
                    soma_signatures::kNodeSetUsePostTransform,
                    sizeof(soma_signatures::kNodeSetUsePostTransform)) != 0
                || std::memcmp(setPostTransformTarget,
                    soma_signatures::kNodeSetPostTransform,
                    sizeof(soma_signatures::kNodeSetPostTransform)) != 0
                || std::memcmp(nodeSetMatrixTarget,
                    soma_signatures::kNodeSetMatrix,
                    sizeof(soma_signatures::kNodeSetMatrix)) != 0
                || std::memcmp(applyPostAnimTransformTarget,
                    soma_signatures::kNodeApplyPostAnimTransform,
                    sizeof(soma_signatures::kNodeApplyPostAnimTransform)) != 0))
        || std::memcmp(destroyEntityTarget, kLuxMapDestroyEntitySignature,
            sizeof(kLuxMapDestroyEntitySignature)) != 0
        || (closestBodyRequested
            && std::memcmp(getClosestBodyTarget, kGetClosestBodySignature,
                sizeof(kGetClosestBodySignature)) != 0)) {
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_hands_bridge install_failed reason=signature_mismatch setMatrixRva=0x%llx getNameRva=0x%llx destroyEntityRva=0x%llx getClosestBodyRva=0x%llx gameplayRay=%d wristMutation=%d",
            static_cast<unsigned long long>(kLuxEntitySetMatrixRva),
            static_cast<unsigned long long>(kLuxEntityGetNameRva),
            static_cast<unsigned long long>(kLuxMapDestroyEntityRva),
            static_cast<unsigned long long>(kGetClosestBodyRva),
            closestBodyRequested ? 1 : 0,
            postMutationRequested ? 1 : 0);
        return false;
    }
    g_getEntityName = reinterpret_cast<LuxEntityGetNameFn>(getNameTarget);
    if (skeletonAccessRequested)
    {
        g_getMeshEntity = reinterpret_cast<LuxEntityGetMeshEntityFn>(getMeshEntityTarget);
        g_getBoneStateFromName =
            reinterpret_cast<MeshEntityGetBoneStateFromNameFn>(getBoneStateFromNameTarget);
    }
    if (postMutationRequested) {
        g_setUsePostTransform = reinterpret_cast<NodeSetUsePostTransformFn>(
            setUsePostTransformTarget);
        g_setPostTransform = reinterpret_cast<NodeSetPostTransformFn>(
            setPostTransformTarget);
        g_nodeSetMatrix = reinterpret_cast<NodeSetMatrixFn>(nodeSetMatrixTarget);
        g_applyPostAnimTransform = reinterpret_cast<NodeApplyPostAnimTransformFn>(
            applyPostAnimTransformTarget);
    }

    MH_STATUS status = MH_CreateHook(
        setMatrixTarget,
        reinterpret_cast<void*>(&HookLuxEntitySetMatrix),
        reinterpret_cast<void**>(&g_originalSetMatrix));
    if (status != MH_OK && status != MH_ERROR_ALREADY_CREATED) {
        g_getEntityName = nullptr;
        g_getMeshEntity = nullptr;
        g_getBoneStateFromName = nullptr;
        g_setUsePostTransform = nullptr;
        g_setPostTransform = nullptr;
        g_nodeSetMatrix = nullptr;
        g_applyPostAnimTransform = nullptr;
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_hands_bridge install_failed reason=create_hook status=%s",
            MH_StatusToString(status));
        return false;
    }
    status = MH_EnableHook(setMatrixTarget);
    if (status != MH_OK && status != MH_ERROR_ENABLED) {
        MH_RemoveHook(setMatrixTarget);
        g_originalSetMatrix = nullptr;
        g_getEntityName = nullptr;
        g_getMeshEntity = nullptr;
        g_getBoneStateFromName = nullptr;
        g_setUsePostTransform = nullptr;
        g_setPostTransform = nullptr;
        g_nodeSetMatrix = nullptr;
        g_applyPostAnimTransform = nullptr;
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_hands_bridge install_failed reason=enable_hook status=%s",
            MH_StatusToString(status));
        return false;
    }

    g_setMatrixTarget = setMatrixTarget;
    status = MH_CreateHook(
        destroyEntityTarget,
        reinterpret_cast<void*>(&HookLuxMapDestroyEntity),
        reinterpret_cast<void**>(&g_originalDestroyEntity));
    if (status != MH_OK && status != MH_ERROR_ALREADY_CREATED) {
        MH_DisableHook(setMatrixTarget);
        MH_RemoveHook(setMatrixTarget);
        g_setMatrixTarget = nullptr;
        g_originalSetMatrix = nullptr;
        g_getEntityName = nullptr;
        g_getMeshEntity = nullptr;
        g_getBoneStateFromName = nullptr;
        g_setUsePostTransform = nullptr;
        g_setPostTransform = nullptr;
        g_nodeSetMatrix = nullptr;
        g_applyPostAnimTransform = nullptr;
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_hands_bridge install_failed reason=create_destroy_entity_hook status=%s",
            MH_StatusToString(status));
        return false;
    }
    status = MH_EnableHook(destroyEntityTarget);
    if (status != MH_OK && status != MH_ERROR_ENABLED) {
        MH_RemoveHook(destroyEntityTarget);
        g_originalDestroyEntity = nullptr;
        MH_DisableHook(setMatrixTarget);
        MH_RemoveHook(setMatrixTarget);
        g_setMatrixTarget = nullptr;
        g_originalSetMatrix = nullptr;
        g_getEntityName = nullptr;
        g_getMeshEntity = nullptr;
        g_getBoneStateFromName = nullptr;
        g_setUsePostTransform = nullptr;
        g_setPostTransform = nullptr;
        g_nodeSetMatrix = nullptr;
        g_applyPostAnimTransform = nullptr;
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_hands_bridge install_failed reason=enable_destroy_entity_hook status=%s",
            MH_StatusToString(status));
        return false;
    }
    g_destroyEntityTarget = destroyEntityTarget;
    if (closestBodyRequested) {
        status = MH_CreateHook(
            getClosestBodyTarget,
            reinterpret_cast<void*>(&HookGetClosestBody),
            reinterpret_cast<void**>(&g_originalGetClosestBody));
        if (status != MH_OK && status != MH_ERROR_ALREADY_CREATED) {
            MH_DisableHook(destroyEntityTarget);
            MH_RemoveHook(destroyEntityTarget);
            MH_DisableHook(setMatrixTarget);
            MH_RemoveHook(setMatrixTarget);
            g_destroyEntityTarget = nullptr;
            g_originalDestroyEntity = nullptr;
            g_setMatrixTarget = nullptr;
            g_originalSetMatrix = nullptr;
            g_getEntityName = nullptr;
            g_getMeshEntity = nullptr;
            g_getBoneStateFromName = nullptr;
            g_setUsePostTransform = nullptr;
            g_setPostTransform = nullptr;
            g_nodeSetMatrix = nullptr;
            g_applyPostAnimTransform = nullptr;
            Logger::Instance().Write(
                LogLevel::Error,
                "hpl_hands_bridge install_failed reason=create_gameplay_ray_hook status=%s",
                MH_StatusToString(status));
            return false;
        }
        status = MH_EnableHook(getClosestBodyTarget);
        if (status != MH_OK && status != MH_ERROR_ENABLED) {
            MH_RemoveHook(getClosestBodyTarget);
            g_originalGetClosestBody = nullptr;
            MH_DisableHook(destroyEntityTarget);
            MH_RemoveHook(destroyEntityTarget);
            MH_DisableHook(setMatrixTarget);
            MH_RemoveHook(setMatrixTarget);
            g_destroyEntityTarget = nullptr;
            g_originalDestroyEntity = nullptr;
            g_setMatrixTarget = nullptr;
            g_originalSetMatrix = nullptr;
            g_getEntityName = nullptr;
            g_getMeshEntity = nullptr;
            g_getBoneStateFromName = nullptr;
            g_setUsePostTransform = nullptr;
            g_setPostTransform = nullptr;
            g_nodeSetMatrix = nullptr;
            g_applyPostAnimTransform = nullptr;
            Logger::Instance().Write(
                LogLevel::Error,
                "hpl_hands_bridge install_failed reason=enable_gameplay_ray_hook status=%s",
                MH_StatusToString(status));
            return false;
        }
        g_getClosestBodyTarget = getClosestBodyTarget;
    }
    if (config.hplHandAlwaysVisible) {
        auto* setActiveTarget = reinterpret_cast<std::byte*>(executable) + kLuxEntitySetActiveRva;
        bool visibilityHookValid =
            IsInsideImage(executable, kLuxEntitySetActiveRva, sizeof(kLuxEntitySetActiveSignature))
            && std::memcmp(setActiveTarget, kLuxEntitySetActiveSignature,
                sizeof(kLuxEntitySetActiveSignature)) == 0;
        if (visibilityHookValid) {
            status = MH_CreateHook(
                setActiveTarget,
                reinterpret_cast<void*>(&HookLuxEntitySetActive),
                reinterpret_cast<void**>(&g_originalSetActive));
            visibilityHookValid = status == MH_OK || status == MH_ERROR_ALREADY_CREATED;
        }
        if (visibilityHookValid) {
            status = MH_EnableHook(setActiveTarget);
            visibilityHookValid = status == MH_OK || status == MH_ERROR_ENABLED;
        }
        if (visibilityHookValid) {
            g_setActiveTarget = setActiveTarget;
        } else {
            MH_DisableHook(setActiveTarget);
            MH_RemoveHook(setActiveTarget);
            g_originalSetActive = nullptr;
            Logger::Instance().Write(
                LogLevel::Warn,
                "hpl_hands_visibility install_fallback reason=signature_or_hook_failure setActiveRva=0x%llx policy=continue_without_retention",
                static_cast<unsigned long long>(kLuxEntitySetActiveRva));
        }
    }
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_hands_bridge install_ok setMatrixRva=0x%llx getNameRva=0x%llx getMeshEntityRva=0x%llx "
        "getBoneStateFromNameRva=0x%llx setUsePostRva=0x%llx setPostRva=0x%llx nodeSetMatrixRva=0x%llx applyPostRva=0x%llx "
        "destroyEntityRva=0x%llx getClosestBodyRva=0x%llx probe=%d controllerRoot=%d "
        "scaleNormalization=%d wristPosition=%d wristRotation=%d armIK=%d alwaysVisible=%d freezePose=%d targetScale=%.3f shoulderVerticalOffsetMeters=%.3f shoulderBackOffsetMeters=%.3f elbowDownMeters=%.3f ergonomics=%d shoulderReach=%d shoulderReachStart=%.3f shoulderReachMaxMeters=%.3f maxSwivelDegreesPerFrame=%.2f armIKBlend=%.3f armIKMaxReach=%.3f authoredInteractions=%d medicineInteraction=%d controllerHudObject=%d twoHandHudObject=%d readPresentation=%d "
        "readScale={distance=%.2f authoredScaleMultiplier=%.2f} twoHand={squeeze=%.3f separationMeters=%.3f,%.3f blend=%.3f} flashlightAim=%d flashlightGameplayRay=%d handOffset=%.4f,%.4f,%.4f handRotationDegrees=%.2f,%.2f,%.2f hudObjectOffset=%.4f,%.4f,%.4f hudObjectRotationDegrees=%.2f,%.2f,%.2f flashlightOffset=%.4f,%.4f,%.4f flashlightRotationDegrees=%.2f,%.2f,%.2f policy=exact_identity_guarded_tracked_pose lifecyclePolicy=evict_on_native_destroy gameplayRayPolicy=ray_length_camera_origin_preserve_cone cacheLimit=%llu identityLogLimit=%llu",
        static_cast<unsigned long long>(kLuxEntitySetMatrixRva),
        static_cast<unsigned long long>(kLuxEntityGetNameRva),
        static_cast<unsigned long long>(kLuxEntityGetMeshEntityRva),
        static_cast<unsigned long long>(kMeshEntityGetBoneStateFromNameRva),
        static_cast<unsigned long long>(soma_signatures::kNodeSetUsePostTransformRva),
        static_cast<unsigned long long>(soma_signatures::kNodeSetPostTransformRva),
        static_cast<unsigned long long>(soma_signatures::kNodeSetMatrixRva),
        static_cast<unsigned long long>(soma_signatures::kNodeApplyPostAnimTransformRva),
        static_cast<unsigned long long>(kLuxMapDestroyEntityRva),
        static_cast<unsigned long long>(kGetClosestBodyRva),
        config.hplHandTrackingProbe ? 1 : 0,
        config.hplHandControllerRoot ? 1 : 0,
        config.hplHandScaleNormalization ? 1 : 0,
        config.hplHandWristPosition ? 1 : 0,
        config.hplHandWristRotation ? 1 : 0,
        config.hplHandArmIK ? 1 : 0,
        config.hplHandAlwaysVisible ? 1 : 0,
        config.hplHandFreezePose ? 1 : 0,
        config.hplHandTargetScale,
        config.hplHandShoulderVerticalOffsetMeters,
        config.hplHandShoulderBackOffsetMeters,
        config.hplHandArmIKElbowDownMeters,
        config.hplHandArmIKErgonomics ? 1 : 0,
        config.hplHandShoulderReachCompensation ? 1 : 0,
        config.hplHandShoulderReachStart,
        config.hplHandShoulderReachMaxMeters,
        config.hplHandArmIKMaxSwivelDegreesPerFrame,
        config.hplHandArmIKBlend,
        config.hplHandArmIKMaxReach,
        config.hplAuthoredInteractions ? 1 : 0,
        config.hplMedicineInteraction ? 1 : 0,
        config.hplControllerHudObject ? 1 : 0,
        config.hplControllerTwoHandHudObject ? 1 : 0,
        config.hplControllerReadPresentation ? 1 : 0,
        config.hplControllerReadObjectDistanceScale,
        config.hplControllerReadObjectScale,
        config.hplControllerTwoHandSqueezeThreshold,
        config.hplControllerTwoHandMinSeparationMeters,
        config.hplControllerTwoHandMaxSeparationMeters,
        config.hplControllerTwoHandDirectionBlend,
        config.hplControllerFlashlightAim ? 1 : 0,
        config.hplControllerFlashlightGameplayRay ? 1 : 0,
        config.hplHandRootOffsetX,
        config.hplHandRootOffsetY,
        config.hplHandRootOffsetZ,
        config.hplHandRootPitchDegrees,
        config.hplHandRootYawDegrees,
        config.hplHandRootRollDegrees,
        config.hplHudObjectOffsetX,
        config.hplHudObjectOffsetY,
        config.hplHudObjectOffsetZ,
        config.hplHudObjectPitchDegrees,
        config.hplHudObjectYawDegrees,
        config.hplHudObjectRollDegrees,
        config.hplFlashlightOffsetX,
        config.hplFlashlightOffsetY,
        config.hplFlashlightOffsetZ,
        config.hplFlashlightPitchDegrees,
        config.hplFlashlightYawDegrees,
        config.hplFlashlightRollDegrees,
        static_cast<unsigned long long>(kMaxIdentityCache),
        static_cast<unsigned long long>(kMaxIdentityLogs));
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_hands_presentation_config wristRollDegrees=%.2f socketedPropStabilization=%d beamSceneDepth=%d closestBodyHook=%d persistentHandsPolicy=retain_after_first_native_creation",
        config.hplHandWristRollDegrees,
        config.hplHandSocketedPropStabilization ? 1 : 0,
        config.hplControllerAimGuideSceneDepth ? 1 : 0,
        closestBodyRequested ? 1 : 0);
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_hand_calibration wristPitchDegrees=%.2f wristOutwardOffsetMeters=%.3f wristVerticalOffsetMeters=%.3f wristViewForwardOffsetMeters=%.3f readSettleFrames=%d readHandsTracked=1 alwaysVisiblePolicy=reactivate_native_created_player_hands",
        config.hplHandWristPitchDegrees,
        config.hplHandWristOutwardOffsetMeters,
        config.hplHandWristVerticalOffsetMeters,
        config.hplHandWristViewForwardOffsetMeters,
        config.hplControllerReadObjectSettleFrames);
    return true;
}

void UpdateHPLHandsBridge(uint64_t frameIndex)
{
    if (!g_config.hplHandAlwaysVisible || g_originalSetMatrix == nullptr) {
        return;
    }
    const RetainedHandsMode retentionMode = ResolveRetainedHandsMode();
    if (retentionMode == RetainedHandsMode::Invalid) {
        InvalidateRetainedHands("synthetic_update_lifecycle_invalid");
        return;
    }
    if (retentionMode == RetainedHandsMode::Suspended) {
        void* suspendedEntity = nullptr;
        bool transition = false;
        {
            std::lock_guard lock(g_retainedHandsMutex);
            if (g_retainedHands.valid && !g_retainedHands.suspended) {
                g_retainedHands.suspended = true;
                suspendedEntity = g_retainedHands.entity;
                transition = true;
            }
        }
        if (transition) {
            RestoreArmPoseAnchors(suspendedEntity);
            Logger::Instance().Write(
                LogLevel::Info,
                "hpl_hands_visibility suspended=1 frame=%llu entity=%p policy=retain_root_wrist_and_full_pose_calibration_across_transient_transition",
                static_cast<unsigned long long>(frameIndex),
                suspendedEntity);
        }
        return;
    }

    RetainedHandsState retained;
    {
        std::lock_guard lock(g_retainedHandsMutex);
        retained = g_retainedHands;
        if (g_retainedHands.valid) g_retainedHands.suspended = false;
    }
    if (!retained.valid || retained.entity == nullptr || retained.mesh == nullptr
        || retained.lastSyntheticFrame == frameIndex
        || (retained.lastNativeFrame != 0
            && frameIndex <= retained.lastNativeFrame + 1)) {
        if (!retained.valid) {
            g_handsRetainedFallbacks.fetch_add(1, std::memory_order_relaxed);
        }
        return;
    }

    if (!retained.wakeRequested
        && g_originalSetActive != nullptr) {
        // Only SetActive is originated here. SetActive is the iLuxEntity-level
        // API the game itself uses, and it performs the visibility work on the
        // engine entities it owns.
        //
        // Do NOT add a SetVisible call here. SetVisible requires an iEntity3D:
        // it dispatches through vtable slot 0xd0 and then dereferences the
        // pointer at +8. Neither pointer retained here satisfies that.
        //   - retained.mesh is the cMeshEntity from GetMeshEntity (iLuxEntity
        //     vtable +0xd8); its vtable has 14 slots, so slot 0xd0 read past the
        //     end into adjacent string data and called it. (crash, 3x)
        //   - retained.entity is the iLuxEntity, which only wraps an engine
        //     entity. Its vtable is long enough that slot 0xd0 dispatches some
        //     unrelated virtual, and +8 is not a parent pointer there (observed
        //     0x101, two packed bools), so the deref faulted. (crash, 1x)
        g_originalSetActive(retained.entity, true);
        retained.wakeRequested = true;
        const uint64_t wake = g_handsWakeRequests.fetch_add(
            1, std::memory_order_relaxed) + 1;
        Logger::Instance().Write(
            LogLevel::Info,
            "hpl_hands_visibility wake=%llu frame=%llu entity=%p mesh=%p policy=reactivate_native_created_player_hands_when_vr_tracking_becomes_eligible",
            static_cast<unsigned long long>(wake),
            static_cast<unsigned long long>(frameIndex),
            retained.entity,
            retained.mesh);
    }

    HPLPlayerStateSnapshot player;
    const HPLCameraBridgeStatus camera = GetHPLCameraBridgeStatus();
    if (!GetHPLPlayerStateSnapshot(player) || !player.playerValid
        || !camera.cameraWorldPositionValid) {
        g_handsRetainedFallbacks.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    std::array<float, 16> matrix{};
    camera_math::Quaternion bodyYaw{};
    camera_math::Vector3 shoulderAnchorPosition{};
    bool shoulderAnchorUsesTrackedHead = false;
    const bool bodyAnchorApplied = BuildBodyAnchoredHandsMatrix(
        retained,
        camera,
        matrix,
        bodyYaw,
        shoulderAnchorPosition,
        shoulderAnchorUsesTrackedHead);
    if (!bodyAnchorApplied) {
        matrix = retained.matrix;
        bool fallbackUsesTrackedHead = false;
        if (retained.shoulderAnchorPositionValid
            && ResolveShoulderAnchorPosition(
                camera, shoulderAnchorPosition, fallbackUsesTrackedHead)) {
            matrix[3] += shoulderAnchorPosition.x
                - retained.shoulderAnchorPosition.x;
            matrix[7] += shoulderAnchorPosition.y
                - retained.shoulderAnchorPosition.y;
            matrix[11] += shoulderAnchorPosition.z
                - retained.shoulderAnchorPosition.z;
            shoulderAnchorUsesTrackedHead = fallbackUsesTrackedHead;
        }
    }
    g_originalSetMatrix(retained.entity, matrix.data());

    WristTrackingFrame tracking;
    ResolveWristTrackingFrame(true, player, camera, tracking);
    ApplyPlayerHandsArmIK(retained.entity, tracking);
    ApplyPlayerHandsWristPositions(retained.entity, tracking, true);

    {
        std::lock_guard lock(g_retainedHandsMutex);
        if (g_retainedHands.entity == retained.entity) {
            g_retainedHands.matrix = matrix;
            g_retainedHands.lastSyntheticFrame = frameIndex;
            g_retainedHands.wakeRequested = retained.wakeRequested;
            g_retainedHands.shoulderAnchorPositionValid = true;
            g_retainedHands.shoulderAnchorUsesTrackedHead =
                shoulderAnchorUsesTrackedHead;
            g_retainedHands.shoulderAnchorPosition = shoulderAnchorPosition;
            g_retainedHands.bodyYawValid = bodyAnchorApplied;
            g_retainedHands.bodyYaw = bodyYaw;
        }
    }
    const uint64_t retainedFrame = g_handsRetainedFrames.fetch_add(
        1, std::memory_order_relaxed) + 1;
    const uint64_t interval = static_cast<uint64_t>(
        std::max(g_config.hplControllerLogInterval, 1));
    if (retainedFrame <= 8 || retainedFrame % interval == 0) {
        Logger::Instance().Write(
            LogLevel::Info,
            "hpl_hands_visibility retainedFrame=%llu frame=%llu entity=%p mesh=%p nativeAge=%llu bodyAnchor=%d trackedHeadAnchor=%d anchor=%.4f,%.4f,%.4f translatedRoot=%.4f,%.4f,%.4f policy=native_seed_physical_gameplay_hmd_position_body_yaw_matrix_visibility_retained_by_native_hooks",
            static_cast<unsigned long long>(retainedFrame),
            static_cast<unsigned long long>(frameIndex),
            retained.entity, retained.mesh,
            static_cast<unsigned long long>(
                frameIndex >= retained.lastNativeFrame
                    ? frameIndex - retained.lastNativeFrame : 0),
            bodyAnchorApplied ? 1 : 0,
            shoulderAnchorUsesTrackedHead ? 1 : 0,
            shoulderAnchorPosition.x,
            shoulderAnchorPosition.y,
            shoulderAnchorPosition.z,
            matrix[3], matrix[7], matrix[11]);
    }
}

bool HasHPLRetainedHandsSeed()
{
    std::lock_guard lock(g_retainedHandsMutex);
    return g_retainedHands.valid && g_retainedHands.entity != nullptr
        && g_retainedHands.mesh != nullptr;
}

bool CaptureHPLArmRenderSnapshot(HPLArmRenderSnapshot& snapshot)
{
    snapshot = {};

    RetainedHandsState retained{};
    {
        std::lock_guard lock(g_retainedHandsMutex);
        retained = g_retainedHands;
    }
    if (!retained.valid || retained.entity == nullptr) return false;

    SharedArmRootPoseAnchor rootAnchor{};
    std::array<ArmPoseAnchor, kHPLArmRenderDiagnosticHandCount> armAnchors{};
    {
        std::lock_guard lock(g_armIkMutationMutex);
        rootAnchor = g_sharedArmRootPoseAnchor;
        armAnchors = g_armPoseAnchors;
    }
    if (!rootAnchor.valid || rootAnchor.entity != retained.entity
        || rootAnchor.node == nullptr) {
        return false;
    }
    for (const ArmPoseAnchor& anchor : armAnchors) {
        if (!anchor.valid || anchor.entity != retained.entity) return false;
        for (size_t index = 0; index < kHPLArmRenderDiagnosticNodeCount; ++index) {
            if (anchor.nodes[index] == nullptr) return false;
        }
    }

    HPLPlayerStateSnapshot player{};
    GetHPLPlayerStateSnapshot(player);
    snapshot.entity = retained.entity;
    snapshot.mesh = retained.mesh;
    snapshot.playerFrame = player.frame;
    NativeVectorLayout boneMatrices{};
    if (retained.mesh != nullptr
        && ReadMemory(
            static_cast<const std::byte*>(retained.mesh) + kMeshBoneMatricesOffset,
            &boneMatrices, sizeof(boneMatrices))
        && boneMatrices.begin != nullptr) {
        const uintptr_t begin = reinterpret_cast<uintptr_t>(boneMatrices.begin);
        const uintptr_t end = reinterpret_cast<uintptr_t>(boneMatrices.end);
        const uintptr_t capacity = reinterpret_cast<uintptr_t>(boneMatrices.capacity);
        const size_t bytes = end >= begin
            ? static_cast<size_t>(end - begin) : 0;
        const size_t matrixBytes = sizeof(snapshot.palette[0]);
        const size_t count = matrixBytes != 0 ? bytes / matrixBytes : 0;
        if (end >= begin
            && capacity >= end
            && bytes % matrixBytes == 0
            && count > 0
            && count <= snapshot.palette.size()
            && ReadMemory(
                boneMatrices.begin, snapshot.palette.data(), bytes)) {
            snapshot.paletteValid = true;
            snapshot.paletteCount = count;
        }
    }
    if (!ReadMemory(
            static_cast<const std::byte*>(rootAnchor.node) + kNodeLocalMatrixOffset,
            snapshot.rootLocal.data(), sizeof(snapshot.rootLocal))
        || !ReadMemory(
            static_cast<const std::byte*>(rootAnchor.node) + kNodeWorldMatrixOffset,
            snapshot.rootWorld.data(), sizeof(snapshot.rootWorld))) {
        snapshot = {};
        return false;
    }
    for (size_t hand = 0; hand < armAnchors.size(); ++hand) {
        for (size_t index = 0; index < kHPLArmRenderDiagnosticNodeCount; ++index) {
            const auto* node = static_cast<const std::byte*>(
                armAnchors[hand].nodes[index]);
            if (!ReadMemory(
                    node + kNodeLocalMatrixOffset,
                    snapshot.local[hand][index].data(),
                    sizeof(snapshot.local[hand][index]))
                || !ReadMemory(
                    node + kNodeWorldMatrixOffset,
                    snapshot.world[hand][index].data(),
                    sizeof(snapshot.world[hand][index]))) {
                snapshot = {};
                return false;
            }
        }
    }
    snapshot.valid = true;
    return true;
}

void RemoveHPLHandsBridge()
{
    std::lock_guard lock(g_installMutex);
    const bool profilesSaved = g_calibrationProfiles.Save();
    Logger::Instance().Write(
        profilesSaved ? LogLevel::Info : LogLevel::Warn,
        "hpl_entity_profiles save=%d %s",
        profilesSaved ? 1 : 0,
        g_calibrationProfiles.SummaryString().c_str());
    if (g_setActiveTarget != nullptr) {
        MH_DisableHook(g_setActiveTarget);
        MH_RemoveHook(g_setActiveTarget);
    }
    if (g_getClosestBodyTarget != nullptr) {
        MH_DisableHook(g_getClosestBodyTarget);
        MH_RemoveHook(g_getClosestBodyTarget);
    }
    if (g_destroyEntityTarget != nullptr) {
        MH_DisableHook(g_destroyEntityTarget);
        MH_RemoveHook(g_destroyEntityTarget);
    }
    if (g_setMatrixTarget != nullptr) {
        MH_DisableHook(g_setMatrixTarget);
        MH_RemoveHook(g_setMatrixTarget);
    }
    g_setMatrixTarget = nullptr;
    g_setActiveTarget = nullptr;
    g_destroyEntityTarget = nullptr;
    g_getClosestBodyTarget = nullptr;
    g_originalSetMatrix = nullptr;
    g_originalSetActive = nullptr;
    g_originalDestroyEntity = nullptr;
    g_originalGetClosestBody = nullptr;
    g_getEntityName = nullptr;
    g_getMeshEntity = nullptr;
    g_getBoneStateFromName = nullptr;
    g_setUsePostTransform = nullptr;
    g_setPostTransform = nullptr;
    g_nodeSetMatrix = nullptr;
    g_applyPostAnimTransform = nullptr;
    g_openxr = nullptr;
    ResetHPLAuthoredInteractionBridge();
    InvalidateFlashlightPose();
    {
        std::lock_guard identityLock(g_identityMutex);
        g_identityCache.clear();
        g_readCandidateEntities.clear();
        g_readPresentationAnchors.clear();
        g_readPresentationStateActive = false;
        g_readPresentationSession = 0;
    }
    {
        std::lock_guard socketLock(g_socketedPropMutex);
        g_socketedPropAnchors.clear();
    }
    {
        std::lock_guard skeletonLock(g_skeletonProbeMutex);
        g_lastSkeletonEntity = nullptr;
        g_lastSkeletonMesh = nullptr;
        g_nextSkeletonProbeFrame = 0;
        g_lastSkeletonProbeFrame = UINT64_MAX;
        g_lastSkeletonPlayerState = -1;
        g_skeletonProbeBurstRemaining = 0;
    }
    {
        std::lock_guard retainedLock(g_retainedHandsMutex);
        g_retainedHands = {};
    }
    {
        std::lock_guard armLock(g_armIkMutationMutex);
        g_lastArmIkMutationEntity = nullptr;
        g_lastArmIkMutationFrame = UINT64_MAX;
        g_armPoseAnchors = {};
        g_armErgonomicStates = {};
    }
    {
        std::lock_guard wristLock(g_wristMutationMutex);
        g_lastWristMutationEntity = nullptr;
        g_lastWristMutationFrame = UINT64_MAX;
        g_wristOrientationAnchors = {};
    }
    g_calibrationProfiles.Reset();
    Logger::Instance().Write(LogLevel::Info, "hpl_hands_bridge removed");
}

void LogHPLHandsBridgeSummary()
{
    size_t cachedIdentities = 0;
    {
        std::lock_guard lock(g_identityMutex);
        cachedIdentities = g_identityCache.size();
    }
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_entity_profiles_summary %s",
        g_calibrationProfiles.SummaryString().c_str());
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_socketed_prop_summary configured=%d anchors=%llu overrides=%llu fallbacks=%llu wristRollDegrees=%.2f beamSceneDepth=%d beamRayQueries=%llu beamRayHits=%llu",
        g_config.hplHandSocketedPropStabilization ? 1 : 0,
        static_cast<unsigned long long>(
            g_socketedPropAnchorSeeds.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(
            g_socketedPropOverrides.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(
            g_socketedPropFallbacks.load(std::memory_order_relaxed)),
        g_config.hplHandWristRollDegrees,
        g_config.hplControllerAimGuideSceneDepth ? 1 : 0,
        static_cast<unsigned long long>(
            g_controllerBeamRayQueries.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(
            g_controllerBeamRayHits.load(std::memory_order_relaxed)));
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_hands_bridge_summary installed=%d destroyLifecycleInstalled=%d gameplayRayInstalled=%d calls=%llu destroyEntityCalls=%llu identityInvalidations=%llu cachedIdentities=%llu identityReads=%llu identityReadFailures=%llu playerHandsIdentities=%llu playerHandsCalls=%llu skeleton={samples=%llu meshChanges=%llu stateChanges=%llu bonesFound=%llu bonesMissing=%llu readFailures=%llu postCandidates=%llu postCandidateFailures=%llu} handPrototype={scaleAttempts=%llu scaleNormalizations=%llu scaleNativeFull=%llu scaleFallbacks=%llu bodyAnchorScaleRepairs=%llu wristFrameAttempts=%llu wristFramesApplied=%llu wristApplications=%llu wristReadFallbacks=%llu wristHierarchyFallbacks=%llu wristAuthoredPostFallbacks=%llu wristMathFallbacks=%llu wristStateFallbacks=%llu wristPoseFallbacks=%llu wristStaleFallbacks=%llu} hudObjectIdentities=%llu socketedHudObjectIdentities=%llu hudObjectCalls=%llu hudObjectOverrideAttempts=%llu hudObjectOverrides=%llu hudObjectScaleFallbacks=%llu hudObjectStateFallbacks=%llu hudObjectAuthoredFallbacks=%llu hudObjectPoseFallbacks=%llu hudObjectStaleFallbacks=%llu hudObjectMathFallbacks=%llu twoHandHudCandidates=%llu twoHandHudOverrides=%llu twoHandHudFallbacks=%llu flashlightIdentities=%llu flashlightCalls=%llu matrixReadFailures=%llu quarterScaleSamples=%llu fullScaleSamples=%llu otherScaleSamples=%llu trackedGripSamples=%llu authoredCameraSamples=%llu rootOverrideAttempts=%llu rootOverrides=%llu rootScaleFallbacks=%llu rootStateFallbacks=%llu rootAuthoredFallbacks=%llu rootPoseFallbacks=%llu rootStaleFallbacks=%llu rootMathFallbacks=%llu readCandidates={calls=%llu unique=%llu presentationOverrides=%llu presentationFallbacks=%llu} flashlightOverrideAttempts=%llu flashlightOverrides=%llu flashlightStateFallbacks=%llu flashlightAuthoredFallbacks=%llu flashlightPoseFallbacks=%llu flashlightStaleFallbacks=%llu flashlightMathFallbacks=%llu gameplayRayCalls=%llu gameplayRayCandidates=%llu gameplayRayRedirects=%llu gameplayRayHits=%llu gameplayRayOriginFallbacks=%llu gameplayRayPoseFallbacks=%llu gameplayRayStaleFallbacks=%llu gameplayRayMathFallbacks=%llu",
        g_setMatrixTarget != nullptr ? 1 : 0,
        g_destroyEntityTarget != nullptr ? 1 : 0,
        g_getClosestBodyTarget != nullptr ? 1 : 0,
        static_cast<unsigned long long>(g_calls.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_destroyEntityCalls.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_identityInvalidations.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(cachedIdentities),
        static_cast<unsigned long long>(g_identityReads.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_identityReadFailures.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_playerHandsIdentities.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_playerHandsCalls.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_skeletonProbeSamples.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_skeletonMeshChanges.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_skeletonStateChanges.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_skeletonBonesFound.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_skeletonBonesMissing.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_skeletonBoneReadFailures.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_skeletonPostCandidates.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_skeletonPostCandidateFailures.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_handScaleAttempts.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_handScaleNormalizations.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_handScaleNativeFull.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_handScaleFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_bodyAnchorScaleRepairs.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_wristFrameAttempts.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_wristFramesApplied.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_wristApplications.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_wristReadFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_wristHierarchyFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_wristAuthoredPostFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_wristMathFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_wristStateFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_wristPoseFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_wristStaleFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_hudObjectIdentities.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_socketedHudObjectIdentities.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_hudObjectCalls.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_hudObjectOverrideAttempts.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_hudObjectOverrides.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_hudObjectScaleFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_hudObjectStateFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_hudObjectAuthoredFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_hudObjectPoseFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_hudObjectStaleFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_hudObjectMathFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_twoHandHudCandidates.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_twoHandHudOverrides.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_twoHandHudFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_flashlightIdentities.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_flashlightCalls.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_matrixReadFailures.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_quarterScaleSamples.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_fullScaleSamples.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_otherScaleSamples.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_trackedGripSamples.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_authoredCameraSamples.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_rootOverrideAttempts.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_rootOverrides.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_rootScaleFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_rootStateFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_rootAuthoredFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_rootPoseFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_rootStaleFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_rootMathFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_readCandidateCalls.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_readCandidateUnique.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_readPresentationOverrides.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_readPresentationFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_flashlightOverrideAttempts.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_flashlightOverrides.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_flashlightStateFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_flashlightAuthoredFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_flashlightPoseFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_flashlightStaleFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_flashlightMathFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_flashlightGameplayRayCalls.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_flashlightGameplayRayCandidates.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_flashlightGameplayRayRedirects.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_flashlightGameplayRayHits.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_flashlightGameplayRayOriginFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_flashlightGameplayRayPoseFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_flashlightGameplayRayStaleFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_flashlightGameplayRayMathFallbacks.load(std::memory_order_relaxed)));
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_arm_body_summary armIK={enabled=%d ergonomics=%d shoulderReach=%d shoulderVerticalOffsetMeters=%.3f shoulderOffsets=%llu frameAttempts=%llu framesApplied=%llu applications=%llu reachClamps=%llu ergonomicFallbacks=%llu shoulderCompensations=%llu elbowHistoryUses=%llu elbowSingularityBlends=%llu elbowSwivelLimits=%llu readFallbacks=%llu hierarchyFallbacks=%llu authoredPostFallbacks=%llu mathFallbacks=%llu sharedRootSeeds=%llu sharedRootAuthoredSeedCorrections=%llu sharedRootRestores=%llu sharedRootDriftCorrections=%llu sharedRootFallbacks=%llu} wristRotation={enabled=%d anchorSeeds=%llu geometricSeeds=%llu legacySeeds=%llu applications=%llu fallbacks=%llu controllerBasisFallbacks=%llu nativeBasisFallbacks={nonFiniteOrDegenerate=%llu nonOrthogonal=%llu improperHandedness=%llu quaternion=%llu}} persistentHands={enabled=%d hooksInstalled=%d nativeSeeds=%llu wakeRequests=%llu activeCalls=%llu activeSuppressions=%llu retainedFrames=%llu fallbacks=%llu invalidations=%llu lifecyclePolicy=native_active_hook_plus_matrix_only_synthetic_updates}",
        g_config.hplHandArmIK ? 1 : 0,
        g_config.hplHandArmIKErgonomics ? 1 : 0,
        g_config.hplHandShoulderReachCompensation ? 1 : 0,
        g_config.hplHandShoulderVerticalOffsetMeters,
        static_cast<unsigned long long>(g_handShoulderOffsets.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_armIkFrameAttempts.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_armIkFramesApplied.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_armIkApplications.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_armIkReachClamps.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_armIkErgonomicFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_armIkShoulderCompensations.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_armIkElbowHistoryUses.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_armIkElbowSingularityBlends.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_armIkElbowSwivelLimits.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_armIkReadFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_armIkHierarchyFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_armIkAuthoredPostFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_armIkMathFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_armRootPoseSeeds.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_armRootPoseAuthoredSeedCorrections.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_armRootPoseRestores.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_armRootPoseDriftCorrections.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_armRootPoseFallbacks.load(std::memory_order_relaxed)),
        g_config.hplHandWristRotation ? 1 : 0,
        static_cast<unsigned long long>(g_wristRotationAnchorSeeds.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_wristGeometricAnchorSeeds.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_wristLegacyAnchorSeeds.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_wristRotationApplications.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_wristRotationFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_wristControllerBasisFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_wristNativeBasisNonFiniteFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_wristNativeBasisOrthogonalityFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_wristNativeBasisHandednessFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_wristNativeQuaternionFallbacks.load(std::memory_order_relaxed)),
        g_config.hplHandAlwaysVisible ? 1 : 0,
        g_setActiveTarget != nullptr ? 1 : 0,
        static_cast<unsigned long long>(g_handsNativeSeeds.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_handsWakeRequests.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_handsActiveCalls.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_handsActiveSuppressions.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_handsRetainedFrames.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_handsRetainedFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_handsRetainedInvalidations.load(std::memory_order_relaxed)));
    LogHPLAuthoredInteractionSummary();
}

} // namespace somavr
