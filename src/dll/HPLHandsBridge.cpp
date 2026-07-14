#include "HPLHandsBridge.h"

#include "HPLCameraBridge.h"
#include "HPLPlayerState.h"
#include "Logger.h"

#include <Windows.h>

#include <MinHook.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>

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
constexpr size_t kNativeStringInlineCapacity = 15;
constexpr size_t kMaxNativeNameLength = 127;
constexpr size_t kMaxIdentityCache = 4096;
constexpr size_t kMaxIdentityLogs = 16;

using LuxEntitySetMatrixFn = void (*)(void* entity, const float* matrix);
using LuxEntityGetNameFn = const void* (*)(void* entity);

struct NativeStringLayout {
    std::array<std::byte, 16> storage{};
    uint64_t size = 0;
    uint64_t capacity = 0;
};

struct EntityIdentity {
    bool playerHands = false;
    std::string name;
};

Config g_config;
OpenXRRuntime* g_openxr = nullptr;
LuxEntitySetMatrixFn g_originalSetMatrix = nullptr;
LuxEntityGetNameFn g_getEntityName = nullptr;
void* g_setMatrixTarget = nullptr;
std::mutex g_installMutex;
std::mutex g_identityMutex;
std::unordered_map<void*, EntityIdentity> g_identityCache;
std::atomic<uint64_t> g_calls = 0;
std::atomic<uint64_t> g_identityReads = 0;
std::atomic<uint64_t> g_identityReadFailures = 0;
std::atomic<uint64_t> g_identityLogs = 0;
std::atomic<uint64_t> g_playerHandsIdentities = 0;
std::atomic<uint64_t> g_playerHandsCalls = 0;
std::atomic<uint64_t> g_matrixReadFailures = 0;
std::atomic<uint64_t> g_quarterScaleSamples = 0;
std::atomic<uint64_t> g_fullScaleSamples = 0;
std::atomic<uint64_t> g_otherScaleSamples = 0;
std::atomic<uint64_t> g_trackedGripSamples = 0;
std::atomic<uint64_t> g_authoredCameraSamples = 0;

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

bool StartsWithPlayerHands(const std::string& name)
{
    constexpr char prefix[] = "PlayerHands_";
    return name.size() >= sizeof(prefix) - 1
        && std::memcmp(name.data(), prefix, sizeof(prefix) - 1) == 0;
}

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
        if (identity.playerHands) {
            g_playerHandsIdentities.fetch_add(1, std::memory_order_relaxed);
        }
        const uint64_t logIndex = g_identityLogs.fetch_add(1, std::memory_order_relaxed);
        if (logIndex < kMaxIdentityLogs || identity.playerHands) {
            Logger::Instance().Write(
                identity.playerHands ? LogLevel::Warn : LogLevel::Info,
                "hpl_hands_identity entity=%p name=%s playerHands=%d nameObject=%p",
                entity,
                identity.name.c_str(),
                identity.playerHands ? 1 : 0,
                nativeName);
        }
    }

    std::lock_guard lock(g_identityMutex);
    if (g_identityCache.size() < kMaxIdentityCache || identity.playerHands) {
        g_identityCache[entity] = identity;
    }
    return identity;
}

float RowLength(const std::array<float, 16>& matrix, size_t row)
{
    const size_t offset = row * 4;
    return std::sqrt(
        matrix[offset] * matrix[offset]
        + matrix[offset + 1] * matrix[offset + 1]
        + matrix[offset + 2] * matrix[offset + 2]);
}

const OpenXRHandInput* SelectDominantHand(const OpenXRInputSnapshot& input, uint32_t& handIndex)
{
    handIndex = g_config.hplControllerDominantHand == "left" ? 0u : 1u;
    const OpenXRHandInput* preferred = handIndex == 0 ? &input.left : &input.right;
    if (preferred->active) return preferred;
    if (!g_config.hplControllerOneHandFallback) return nullptr;
    handIndex ^= 1u;
    const OpenXRHandInput* fallback = handIndex == 0 ? &input.left : &input.right;
    return fallback->active ? fallback : nullptr;
}

void HookLuxEntitySetMatrix(void* entity, const float* matrixPointer)
{
    const uint64_t call = g_calls.fetch_add(1, std::memory_order_relaxed) + 1;
    const EntityIdentity identity = ResolveIdentity(entity);
    if (identity.playerHands) {
        const uint64_t handCall = g_playerHandsCalls.fetch_add(1, std::memory_order_relaxed) + 1;
        std::array<float, 16> matrix{};
        if (!ReadMemory(matrixPointer, matrix.data(), sizeof(matrix))) {
            g_matrixReadFailures.fetch_add(1, std::memory_order_relaxed);
        } else {
            const float scaleX = RowLength(matrix, 0);
            const float scaleY = RowLength(matrix, 1);
            const float scaleZ = RowLength(matrix, 2);
            const float averageScale = (scaleX + scaleY + scaleZ) / 3.0f;
            const char* scaleMode = "other";
            if (std::fabs(averageScale - 0.25f) <= 0.04f) {
                scaleMode = "quarter";
                g_quarterScaleSamples.fetch_add(1, std::memory_order_relaxed);
            } else if (std::fabs(averageScale - 1.0f) <= 0.1f) {
                scaleMode = "full";
                g_fullScaleSamples.fetch_add(1, std::memory_order_relaxed);
            } else {
                g_otherScaleSamples.fetch_add(1, std::memory_order_relaxed);
            }

            HPLPlayerStateSnapshot player;
            GetHPLPlayerStateSnapshot(player);
            if (player.authoredCameraActive) {
                g_authoredCameraSamples.fetch_add(1, std::memory_order_relaxed);
            }
            const HPLCameraBridgeStatus camera = GetHPLCameraBridgeStatus();

            OpenXRInputSnapshot input;
            HPLTrackedPoseWorld worldGrip;
            uint32_t handIndex = 1;
            bool gripValid = false;
            if (g_openxr != nullptr
                && g_openxr->GetLatestInput(input)
                && input.active) {
                const OpenXRHandInput* hand = SelectDominantHand(input, handIndex);
                gripValid = hand != nullptr
                    && hand->gripPose.valid
                    && ResolveHPLTrackedPoseWorld(hand->gripPose, input.gameFrame, worldGrip)
                    && worldGrip.orientationTracked
                    && worldGrip.positionTracked;
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

            const uint64_t interval = static_cast<uint64_t>(std::max(g_config.hplControllerLogInterval, 1));
            if (handCall <= 12 || handCall % interval == 0) {
                Logger::Instance().Write(
                    LogLevel::Info,
                    "hpl_hands_pose call=%llu totalCall=%llu entity=%p name=%s matrix=%p pos=%.4f,%.4f,%.4f scale=%.4f,%.4f,%.4f scaleMode=%s right=%.5f,%.5f,%.5f up=%.5f,%.5f,%.5f forward=%.5f,%.5f,%.5f cameraValid=%d cameraDistance=%.4f gripValid=%d gripHand=%s gripPos=%.4f,%.4f,%.4f gripForward=%.5f,%.5f,%.5f gripDistance=%.4f authoredCamera=%d playerState=%d moveState=%d",
                    static_cast<unsigned long long>(handCall),
                    static_cast<unsigned long long>(call),
                    entity,
                    identity.name.c_str(),
                    matrixPointer,
                    positionX, positionY, positionZ,
                    scaleX, scaleY, scaleZ,
                    scaleMode,
                    matrix[0], matrix[1], matrix[2],
                    matrix[4], matrix[5], matrix[6],
                    matrix[8], matrix[9], matrix[10],
                    camera.cameraWorldPositionValid ? 1 : 0,
                    cameraDistance,
                    gripValid ? 1 : 0,
                    handIndex == 0 ? "left" : "right",
                    worldGrip.positionX, worldGrip.positionY, worldGrip.positionZ,
                    worldGrip.forwardX, worldGrip.forwardY, worldGrip.forwardZ,
                    gripDistance,
                    player.authoredCameraActive ? 1 : 0,
                    player.playerStateId,
                    player.moveStateId);
            }
        }
    }

    g_originalSetMatrix(entity, matrixPointer);
}

} // namespace

bool InstallHPLHandsBridge(const Config& config, OpenXRRuntime* openxr)
{
    std::lock_guard lock(g_installMutex);
    g_config = config;
    g_openxr = openxr;
    if (!config.hplHandTrackingProbe) {
        Logger::Instance().Write(LogLevel::Info, "hpl_hands_bridge disabled config=0");
        return true;
    }
    if (g_setMatrixTarget != nullptr) return true;

    HMODULE executable = GetModuleHandleW(nullptr);
    if (!IsInsideImage(executable, kLuxEntitySetMatrixRva, sizeof(kLuxEntitySetMatrixSignature))
        || !IsInsideImage(executable, kLuxEntityGetNameRva, sizeof(kLuxEntityGetNameSignature))) {
        Logger::Instance().Write(LogLevel::Error, "hpl_hands_bridge install_failed reason=invalid_image_range");
        return false;
    }

    auto* setMatrixTarget = reinterpret_cast<std::byte*>(executable) + kLuxEntitySetMatrixRva;
    auto* getNameTarget = reinterpret_cast<std::byte*>(executable) + kLuxEntityGetNameRva;
    if (std::memcmp(setMatrixTarget, kLuxEntitySetMatrixSignature, sizeof(kLuxEntitySetMatrixSignature)) != 0
        || std::memcmp(getNameTarget, kLuxEntityGetNameSignature, sizeof(kLuxEntityGetNameSignature)) != 0) {
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_hands_bridge install_failed reason=signature_mismatch setMatrixRva=0x%llx getNameRva=0x%llx",
            static_cast<unsigned long long>(kLuxEntitySetMatrixRva),
            static_cast<unsigned long long>(kLuxEntityGetNameRva));
        return false;
    }
    g_getEntityName = reinterpret_cast<LuxEntityGetNameFn>(getNameTarget);

    MH_STATUS status = MH_CreateHook(
        setMatrixTarget,
        reinterpret_cast<void*>(&HookLuxEntitySetMatrix),
        reinterpret_cast<void**>(&g_originalSetMatrix));
    if (status != MH_OK && status != MH_ERROR_ALREADY_CREATED) {
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_hands_bridge install_failed reason=create_hook status=%s",
            MH_StatusToString(status));
        return false;
    }
    status = MH_EnableHook(setMatrixTarget);
    if (status != MH_OK && status != MH_ERROR_ENABLED) {
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_hands_bridge install_failed reason=enable_hook status=%s",
            MH_StatusToString(status));
        return false;
    }

    g_setMatrixTarget = setMatrixTarget;
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_hands_bridge install_ok setMatrixRva=0x%llx getNameRva=0x%llx policy=identity_and_pose_probe_only cacheLimit=%llu identityLogLimit=%llu",
        static_cast<unsigned long long>(kLuxEntitySetMatrixRva),
        static_cast<unsigned long long>(kLuxEntityGetNameRva),
        static_cast<unsigned long long>(kMaxIdentityCache),
        static_cast<unsigned long long>(kMaxIdentityLogs));
    return true;
}

void RemoveHPLHandsBridge()
{
    std::lock_guard lock(g_installMutex);
    if (g_setMatrixTarget != nullptr) {
        MH_DisableHook(g_setMatrixTarget);
        MH_RemoveHook(g_setMatrixTarget);
    }
    g_setMatrixTarget = nullptr;
    g_originalSetMatrix = nullptr;
    g_getEntityName = nullptr;
    g_openxr = nullptr;
    {
        std::lock_guard identityLock(g_identityMutex);
        g_identityCache.clear();
    }
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
        "hpl_hands_bridge_summary installed=%d calls=%llu cachedIdentities=%llu identityReads=%llu identityReadFailures=%llu playerHandsIdentities=%llu playerHandsCalls=%llu matrixReadFailures=%llu quarterScaleSamples=%llu fullScaleSamples=%llu otherScaleSamples=%llu trackedGripSamples=%llu authoredCameraSamples=%llu",
        g_setMatrixTarget != nullptr ? 1 : 0,
        static_cast<unsigned long long>(g_calls.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(cachedIdentities),
        static_cast<unsigned long long>(g_identityReads.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_identityReadFailures.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_playerHandsIdentities.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_playerHandsCalls.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_matrixReadFailures.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_quarterScaleSamples.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_fullScaleSamples.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_otherScaleSamples.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_trackedGripSamples.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_authoredCameraSamples.load(std::memory_order_relaxed)));
}

} // namespace somavr
