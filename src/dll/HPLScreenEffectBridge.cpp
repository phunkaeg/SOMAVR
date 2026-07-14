#include "HPLScreenEffectBridge.h"

#include "Config.h"
#include "HPLCameraBridge.h"
#include "HPLScreenEffectMath.h"
#include "Logger.h"

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
#include <mutex>
#include <string>
#include <unordered_map>

namespace somavr {
namespace {

constexpr uintptr_t kCreateBillboardRva = 0x24a2f0;
constexpr uintptr_t kDestroyBillboardRva = 0x252700;
constexpr uintptr_t kEntitySetPositionRva = 0x2936c0;
constexpr uintptr_t kBillboardSetSizeRva = 0x291700;
constexpr float kAuthoredScreenDistance = 0.15f;
constexpr float kMinimumDistanceScale = 0.25f;
constexpr float kMaximumDistanceScale = 32.0f;
constexpr size_t kNativeStringInlineCapacity = 15;
constexpr size_t kMaxBillboardNameLength = 127;
constexpr size_t kMaxTrackedBillboards = 64;

constexpr uint8_t kCreateBillboardSignature[] = {
    0x48, 0x89, 0x5c, 0x24, 0x10, 0x48, 0x89, 0x6c,
    0x24, 0x18, 0x56, 0x57, 0x41, 0x54, 0x48, 0x83,
    0xec, 0x40,
};
constexpr uint8_t kDestroyBillboardSignature[] = {
    0x48, 0x89, 0x5c, 0x24, 0x08, 0x48, 0x89, 0x74,
    0x24, 0x10, 0x57, 0x48, 0x83, 0xec, 0x20,
};
constexpr uint8_t kEntitySetPositionSignature[] = {
    0x48, 0x89, 0x5c, 0x24, 0x08, 0x57, 0x48, 0x83,
    0xec, 0x20, 0xf3, 0x0f, 0x10, 0x42, 0x08,
};
constexpr uint8_t kBillboardSetSizeSignature[] = {
    0x40, 0x53, 0x48, 0x83, 0xec, 0x20, 0x8b, 0x02,
    0x48, 0x8b, 0xd9, 0x89, 0x41, 0x68,
};

using CreateBillboardFn = void* (*)(
    void* world,
    const void* nativeName,
    const float* size,
    int billboardType,
    const void* nativeMaterial,
    bool isStatic);
using DestroyBillboardFn = void (*)(void* world, void* billboard);
using EntitySetPositionFn = void (*)(void* entity, const float* position);
using BillboardSetSizeFn = void (*)(void* billboard, const float* size);

struct NativeStringLayout {
    std::array<std::byte, 16> storage{};
    uint64_t size = 0;
    uint64_t capacity = 0;
};

struct TrackedBillboard {
    screen_effect_math::Size2 nativeSize;
    bool scaled = false;
};

Config g_config;
CreateBillboardFn g_originalCreateBillboard = nullptr;
DestroyBillboardFn g_originalDestroyBillboard = nullptr;
EntitySetPositionFn g_originalSetPosition = nullptr;
BillboardSetSizeFn g_originalSetSize = nullptr;
void* g_createTarget = nullptr;
void* g_destroyTarget = nullptr;
void* g_positionTarget = nullptr;
void* g_sizeTarget = nullptr;
std::mutex g_installMutex;
std::mutex g_billboardMutex;
std::unordered_map<void*, TrackedBillboard> g_billboards;
std::array<std::atomic<void*>, kMaxTrackedBillboards> g_billboardSlots{};
std::atomic<uint64_t> g_createCalls = 0;
std::atomic<uint64_t> g_destroyCalls = 0;
std::atomic<uint64_t> g_identityMatches = 0;
std::atomic<uint64_t> g_identityReadFailures = 0;
std::atomic<uint64_t> g_positionCalls = 0;
std::atomic<uint64_t> g_positionOverrides = 0;
std::atomic<uint64_t> g_positionFallbacks = 0;
std::atomic<uint64_t> g_sizeCalls = 0;
std::atomic<uint64_t> g_sizeOverrides = 0;
std::atomic<uint64_t> g_sizeTransitions = 0;
std::atomic<uint64_t> g_peakTracked = 0;

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
        GetCurrentProcess(), source, destination, bytes, &bytesRead) != FALSE
        && bytesRead == bytes;
}

bool ReadNativeString(const void* nativeString, std::string& value)
{
    value.clear();
    NativeStringLayout layout;
    if (!ReadMemory(nativeString, &layout, sizeof(layout))
        || layout.size > kMaxBillboardNameLength
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

bool IsScreenParticleName(const std::string& name)
{
    constexpr char prefix[] = "Screen Particle";
    constexpr size_t prefixLength = sizeof(prefix) - 1;
    if (name.size() <= prefixLength
        || std::memcmp(name.data(), prefix, prefixLength) != 0) {
        return false;
    }
    return std::all_of(name.begin() + prefixLength, name.end(), [](unsigned char ch) {
        return std::isdigit(ch) != 0;
    });
}

float DistanceScale()
{
    const float desiredWorldDistance =
        g_config.hplScreenEffectDistanceMeters * g_config.hplWorldScale;
    return std::clamp(
        desiredWorldDistance / kAuthoredScreenDistance,
        kMinimumDistanceScale,
        kMaximumDistanceScale);
}

bool IsControlActive(const HPLCameraBridgeStatus& camera)
{
    return g_config.hplScreenEffectControl && camera.trackingEnabled;
}

void UpdatePeakTracked(uint64_t tracked)
{
    uint64_t peak = g_peakTracked.load(std::memory_order_relaxed);
    while (tracked > peak
        && !g_peakTracked.compare_exchange_weak(
            peak, tracked, std::memory_order_relaxed, std::memory_order_relaxed)) {
    }
}

bool IsTrackedBillboard(void* billboard)
{
    return std::any_of(g_billboardSlots.begin(), g_billboardSlots.end(), [billboard](const auto& slot) {
        return slot.load(std::memory_order_acquire) == billboard;
    });
}

bool TrackBillboard(void* billboard, const TrackedBillboard& state, uint64_t& tracked)
{
    std::lock_guard lock(g_billboardMutex);
    for (auto& slot : g_billboardSlots) {
        void* empty = nullptr;
        if (slot.compare_exchange_strong(
                empty, billboard, std::memory_order_release, std::memory_order_relaxed)) {
            g_billboards[billboard] = state;
            tracked = static_cast<uint64_t>(g_billboards.size());
            return true;
        }
    }
    tracked = static_cast<uint64_t>(g_billboards.size());
    return false;
}

bool UntrackBillboard(void* billboard)
{
    for (auto& slot : g_billboardSlots) {
        void* expected = billboard;
        slot.compare_exchange_strong(
            expected, nullptr, std::memory_order_release, std::memory_order_relaxed);
    }
    std::lock_guard lock(g_billboardMutex);
    return g_billboards.erase(billboard) != 0;
}

void SyncSizeForTrackingState(
    void* billboard,
    const TrackedBillboard& snapshot,
    bool shouldScale,
    float distanceScale)
{
    if (snapshot.scaled == shouldScale || g_originalSetSize == nullptr) return;

    screen_effect_math::Size2 output = snapshot.nativeSize;
    if (shouldScale
        && !screen_effect_math::ScaleBillboardSize(
            snapshot.nativeSize, distanceScale, output)) {
        return;
    }
    const float size[2] = {output.width, output.height};
    g_originalSetSize(billboard, size);
    {
        std::lock_guard lock(g_billboardMutex);
        const auto found = g_billboards.find(billboard);
        if (found != g_billboards.end()) found->second.scaled = shouldScale;
    }
    g_sizeTransitions.fetch_add(1, std::memory_order_relaxed);
}

void* HookCreateBillboard(
    void* world,
    const void* nativeName,
    const float* size,
    int billboardType,
    const void* nativeMaterial,
    bool isStatic)
{
    g_createCalls.fetch_add(1, std::memory_order_relaxed);
    std::string name;
    const bool nameValid = ReadNativeString(nativeName, name);
    if (!nameValid) g_identityReadFailures.fetch_add(1, std::memory_order_relaxed);
    const bool screenParticle = nameValid && IsScreenParticleName(name);

    screen_effect_math::Size2 nativeSize;
    const bool sizeValid = size != nullptr
        && std::isfinite(size[0]) && std::isfinite(size[1])
        && size[0] > 0.0f && size[1] > 0.0f;
    if (sizeValid) nativeSize = {size[0], size[1]};

    void* billboard = g_originalCreateBillboard(
        world,
        nativeName,
        size,
        billboardType,
        nativeMaterial,
        isStatic);

    if (billboard != nullptr && screenParticle && sizeValid) {
        uint64_t tracked = 0;
        const bool registered = TrackBillboard(billboard, {nativeSize, false}, tracked);
        UpdatePeakTracked(tracked);
        const uint64_t match = g_identityMatches.fetch_add(1, std::memory_order_relaxed) + 1;
        Logger::Instance().Write(
            registered ? LogLevel::Info : LogLevel::Warn,
            "hpl_screen_effect event=create sequence=%llu billboard=%p name=\"%s\" nativeSize=%.4f,%.4f distanceScale=%.3f registered=%d tracked=%llu policy=defer_scale_until_position",
            static_cast<unsigned long long>(match),
            billboard,
            name.c_str(),
            nativeSize.width,
            nativeSize.height,
            DistanceScale(),
            registered ? 1 : 0,
            static_cast<unsigned long long>(tracked));
    }
    return billboard;
}

void HookDestroyBillboard(void* world, void* billboard)
{
    g_destroyCalls.fetch_add(1, std::memory_order_relaxed);
    const bool tracked = UntrackBillboard(billboard);
    if (tracked) {
        Logger::Instance().Write(
            LogLevel::Info,
            "hpl_screen_effect event=destroy billboard=%p",
            billboard);
    }
    g_originalDestroyBillboard(world, billboard);
}

void HookEntitySetPosition(void* entity, const float* position)
{
    TrackedBillboard billboard;
    bool tracked = false;
    if (IsTrackedBillboard(entity)) {
        std::lock_guard lock(g_billboardMutex);
        const auto found = g_billboards.find(entity);
        if (found != g_billboards.end()) {
            billboard = found->second;
            tracked = true;
        }
    }
    if (!tracked) {
        g_originalSetPosition(entity, position);
        return;
    }

    g_positionCalls.fetch_add(1, std::memory_order_relaxed);
    const HPLCameraBridgeStatus camera = GetHPLCameraBridgeStatus();
    const bool active = IsControlActive(camera);
    const float distanceScale = DistanceScale();
    SyncSizeForTrackingState(entity, billboard, active, distanceScale);
    if (!active) {
        g_originalSetPosition(entity, position);
        return;
    }
    if (!camera.cameraWorldPositionValid || position == nullptr) {
        g_positionFallbacks.fetch_add(1, std::memory_order_relaxed);
        g_originalSetPosition(entity, position);
        return;
    }

    camera_math::Vector3 output;
    if (!screen_effect_math::ScaleCameraRelativePosition(
            {camera.cameraWorldPositionX, camera.cameraWorldPositionY, camera.cameraWorldPositionZ},
            {position[0], position[1], position[2]},
            distanceScale,
            output)) {
        g_positionFallbacks.fetch_add(1, std::memory_order_relaxed);
        g_originalSetPosition(entity, position);
        return;
    }
    const float scaledPosition[3] = {output.x, output.y, output.z};
    g_positionOverrides.fetch_add(1, std::memory_order_relaxed);
    g_originalSetPosition(entity, scaledPosition);
}

void HookBillboardSetSize(void* billboard, const float* size)
{
    TrackedBillboard snapshot;
    bool tracked = false;
    const bool sizeValid = size != nullptr
        && std::isfinite(size[0]) && std::isfinite(size[1])
        && size[0] > 0.0f && size[1] > 0.0f;
    if (IsTrackedBillboard(billboard)) {
        std::lock_guard lock(g_billboardMutex);
        const auto found = g_billboards.find(billboard);
        if (found != g_billboards.end()) {
            tracked = true;
            if (sizeValid) found->second.nativeSize = {size[0], size[1]};
            snapshot = found->second;
        }
    }
    if (!tracked || !sizeValid) {
        g_originalSetSize(billboard, size);
        return;
    }

    g_sizeCalls.fetch_add(1, std::memory_order_relaxed);
    const bool active = IsControlActive(GetHPLCameraBridgeStatus());
    screen_effect_math::Size2 output = snapshot.nativeSize;
    if (active
        && screen_effect_math::ScaleBillboardSize(snapshot.nativeSize, DistanceScale(), output)) {
        g_sizeOverrides.fetch_add(1, std::memory_order_relaxed);
    }
    {
        std::lock_guard lock(g_billboardMutex);
        const auto found = g_billboards.find(billboard);
        if (found != g_billboards.end()) found->second.scaled = active;
    }
    const float adjustedSize[2] = {output.width, output.height};
    g_originalSetSize(billboard, adjustedSize);
}

bool InstallHook(void* target, void* hook, void** original, void*& installed, const char* name)
{
    MH_STATUS status = MH_CreateHook(target, hook, original);
    if (status != MH_OK) {
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_screen_effect_bridge install_failed reason=create_hook name=%s status=%s",
            name,
            MH_StatusToString(status));
        return false;
    }
    status = MH_EnableHook(target);
    if (status != MH_OK && status != MH_ERROR_ENABLED) {
        MH_RemoveHook(target);
        *original = nullptr;
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_screen_effect_bridge install_failed reason=enable_hook name=%s status=%s",
            name,
            MH_StatusToString(status));
        return false;
    }
    installed = target;
    return true;
}

void RemoveHook(void*& target)
{
    if (target == nullptr) return;
    MH_DisableHook(target);
    MH_RemoveHook(target);
    target = nullptr;
}

void RollbackHooks()
{
    RemoveHook(g_sizeTarget);
    RemoveHook(g_positionTarget);
    RemoveHook(g_destroyTarget);
    RemoveHook(g_createTarget);
    g_originalCreateBillboard = nullptr;
    g_originalDestroyBillboard = nullptr;
    g_originalSetPosition = nullptr;
    g_originalSetSize = nullptr;
}

} // namespace

bool InstallHPLScreenEffectBridge(const Config& config)
{
    std::lock_guard lock(g_installMutex);
    g_config = config;
    if (!config.hplScreenEffectControl) {
        Logger::Instance().Write(LogLevel::Info, "hpl_screen_effect_bridge disabled config=0");
        return true;
    }
    if (g_createTarget != nullptr) return true;

    HMODULE executable = GetModuleHandleW(nullptr);
    const auto* base = reinterpret_cast<const std::byte*>(executable);
    const bool rangesValid =
        IsInsideImage(executable, kCreateBillboardRva, sizeof(kCreateBillboardSignature))
        && IsInsideImage(executable, kDestroyBillboardRva, sizeof(kDestroyBillboardSignature))
        && IsInsideImage(executable, kEntitySetPositionRva, sizeof(kEntitySetPositionSignature))
        && IsInsideImage(executable, kBillboardSetSizeRva, sizeof(kBillboardSetSizeSignature));
    if (!rangesValid) {
        Logger::Instance().Write(
            LogLevel::Error, "hpl_screen_effect_bridge install_failed reason=invalid_image_range");
        return false;
    }
    const bool signaturesValid =
        std::memcmp(base + kCreateBillboardRva, kCreateBillboardSignature,
            sizeof(kCreateBillboardSignature)) == 0
        && std::memcmp(base + kDestroyBillboardRva, kDestroyBillboardSignature,
            sizeof(kDestroyBillboardSignature)) == 0
        && std::memcmp(base + kEntitySetPositionRva, kEntitySetPositionSignature,
            sizeof(kEntitySetPositionSignature)) == 0
        && std::memcmp(base + kBillboardSetSizeRva, kBillboardSetSizeSignature,
            sizeof(kBillboardSetSizeSignature)) == 0;
    if (!signaturesValid) {
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_screen_effect_bridge install_failed reason=signature_mismatch createRva=0x%llx destroyRva=0x%llx positionRva=0x%llx sizeRva=0x%llx",
            static_cast<unsigned long long>(kCreateBillboardRva),
            static_cast<unsigned long long>(kDestroyBillboardRva),
            static_cast<unsigned long long>(kEntitySetPositionRva),
            static_cast<unsigned long long>(kBillboardSetSizeRva));
        return false;
    }

    if (!InstallHook(
            const_cast<std::byte*>(base + kCreateBillboardRva),
            reinterpret_cast<void*>(&HookCreateBillboard),
            reinterpret_cast<void**>(&g_originalCreateBillboard),
            g_createTarget,
            "CreateBillboard")
        || !InstallHook(
            const_cast<std::byte*>(base + kDestroyBillboardRva),
            reinterpret_cast<void*>(&HookDestroyBillboard),
            reinterpret_cast<void**>(&g_originalDestroyBillboard),
            g_destroyTarget,
            "DestroyBillboard")
        || !InstallHook(
            const_cast<std::byte*>(base + kEntitySetPositionRva),
            reinterpret_cast<void*>(&HookEntitySetPosition),
            reinterpret_cast<void**>(&g_originalSetPosition),
            g_positionTarget,
            "EntitySetPosition")
        || !InstallHook(
            const_cast<std::byte*>(base + kBillboardSetSizeRva),
            reinterpret_cast<void*>(&HookBillboardSetSize),
            reinterpret_cast<void**>(&g_originalSetSize),
            g_sizeTarget,
            "BillboardSetSize")) {
        RollbackHooks();
        return false;
    }

    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_screen_effect_bridge install_ok identity=Screen_Particle_decimal nativeDistance=%.3f targetDistanceMeters=%.3f worldScale=%.3f distanceScale=%.3f createRva=0x%llx destroyRva=0x%llx positionRva=0x%llx sizeRva=0x%llx",
        kAuthoredScreenDistance,
        config.hplScreenEffectDistanceMeters,
        config.hplWorldScale,
        DistanceScale(),
        static_cast<unsigned long long>(kCreateBillboardRva),
        static_cast<unsigned long long>(kDestroyBillboardRva),
        static_cast<unsigned long long>(kEntitySetPositionRva),
        static_cast<unsigned long long>(kBillboardSetSizeRva));
    return true;
}

void RemoveHPLScreenEffectBridge()
{
    std::lock_guard lock(g_installMutex);
    RollbackHooks();
    {
        std::lock_guard billboardLock(g_billboardMutex);
        g_billboards.clear();
    }
    for (auto& slot : g_billboardSlots) {
        slot.store(nullptr, std::memory_order_release);
    }
    Logger::Instance().Write(LogLevel::Info, "hpl_screen_effect_bridge removed");
}

void LogHPLScreenEffectBridgeSummary()
{
    size_t tracked = 0;
    {
        std::lock_guard lock(g_billboardMutex);
        tracked = g_billboards.size();
    }
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_screen_effect_bridge_summary installed=%d createCalls=%llu destroyCalls=%llu identityMatches=%llu identityReadFailures=%llu tracked=%llu peakTracked=%llu positionCalls=%llu positionOverrides=%llu positionFallbacks=%llu sizeCalls=%llu sizeOverrides=%llu sizeTransitions=%llu",
        g_createTarget != nullptr ? 1 : 0,
        static_cast<unsigned long long>(g_createCalls.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_destroyCalls.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_identityMatches.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_identityReadFailures.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(tracked),
        static_cast<unsigned long long>(g_peakTracked.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_positionCalls.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_positionOverrides.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_positionFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_sizeCalls.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_sizeOverrides.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_sizeTransitions.load(std::memory_order_relaxed)));
}

} // namespace somavr
