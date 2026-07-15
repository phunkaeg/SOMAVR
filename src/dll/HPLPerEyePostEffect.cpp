#include "HPLPerEyePostEffect.h"

#include "Config.h"
#include "HPLPerEyePostEffectMath.h"
#include "Logger.h"

#include <Windows.h>

#include <atomic>
#include <cstddef>
#include <cstring>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

namespace somavr {
namespace {

using per_eye_post_effect_math::Bank;
using per_eye_post_effect_math::ResourcePair;

constexpr size_t kFramebufferOffset = 0x50;
constexpr size_t kTextureOffset = 0x58;
constexpr size_t kClearOffset = 0xa0;

struct EffectState {
    Bank bank;
    bool stereoActive = false;
};

struct ActivePass {
    bool active = false;
    void* effect = nullptr;
    int eyeIndex = -1;
    uint64_t poseFrame = 0;
};

bool g_configured = false;
bool g_faulted = false;
HPLImageTrailResourceFn g_createResources = nullptr;
HPLImageTrailResourceFn g_destroyResources = nullptr;
std::mutex g_mutex;
std::unordered_map<void*, EffectState> g_effects;
thread_local ActivePass g_activePass;
std::atomic<uint64_t> g_allocations = 0;
std::atomic<uint64_t> g_restores = 0;
std::atomic<uint64_t> g_captures = 0;
std::atomic<uint64_t> g_resets = 0;
std::atomic<uint64_t> g_releases = 0;
std::atomic<uint64_t> g_failures = 0;

bool IsAccessible(const void* address, size_t size, bool writable)
{
    if (address == nullptr || size == 0) return false;
    MEMORY_BASIC_INFORMATION info{};
    if (VirtualQuery(address, &info, sizeof(info)) == 0 || info.State != MEM_COMMIT
        || (info.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) {
        return false;
    }
    const DWORD protection = info.Protect & 0xff;
    const bool readable = protection == PAGE_READONLY || protection == PAGE_READWRITE
        || protection == PAGE_WRITECOPY || protection == PAGE_EXECUTE_READ
        || protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY;
    const bool writeable = protection == PAGE_READWRITE || protection == PAGE_WRITECOPY
        || protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY;
    const uintptr_t start = reinterpret_cast<uintptr_t>(address);
    const uintptr_t end = reinterpret_cast<uintptr_t>(info.BaseAddress) + info.RegionSize;
    return readable && (!writable || writeable) && start <= end && size <= end - start;
}

template <typename T>
bool ReadField(const void* object, size_t offset, T& value)
{
    const void* address = static_cast<const uint8_t*>(object) + offset;
    if (!IsAccessible(address, sizeof(T), false)) return false;
    std::memcpy(&value, address, sizeof(T));
    return true;
}

template <typename T>
bool WriteField(void* object, size_t offset, const T& value)
{
    void* address = static_cast<uint8_t*>(object) + offset;
    if (!IsAccessible(address, sizeof(T), true)) return false;
    std::memcpy(address, &value, sizeof(T));
    return true;
}

bool ReadLiveState(void* effect, ResourcePair& resources, bool& clear)
{
    void* framebuffer = nullptr;
    void* texture = nullptr;
    uint8_t clearByte = 0;
    if (!ReadField(effect, kFramebufferOffset, framebuffer)
        || !ReadField(effect, kTextureOffset, texture)
        || !ReadField(effect, kClearOffset, clearByte)) {
        return false;
    }
    resources.framebuffer = reinterpret_cast<uintptr_t>(framebuffer);
    resources.texture = reinterpret_cast<uintptr_t>(texture);
    clear = clearByte != 0;
    return per_eye_post_effect_math::IsValid(resources);
}

bool WriteLiveState(void* effect, const ResourcePair& resources, bool clear)
{
    void* framebuffer = reinterpret_cast<void*>(resources.framebuffer);
    void* texture = reinterpret_cast<void*>(resources.texture);
    const uint8_t clearByte = clear ? 1 : 0;
    return per_eye_post_effect_math::IsValid(resources)
        && WriteField(effect, kFramebufferOffset, framebuffer)
        && WriteField(effect, kTextureOffset, texture)
        && WriteField(effect, kClearOffset, clearByte);
}

void Fault(const char* reason, void* effect)
{
    g_faulted = true;
    g_activePass = {};
    const uint64_t failure = g_failures.fetch_add(1, std::memory_order_relaxed) + 1;
    Logger::Instance().Write(
        LogLevel::Error,
        "hpl_per_eye_post_effect fault=1 reason=%s effect=%p failures=%llu fallback=named_effect_suppression",
        reason,
        effect,
        static_cast<unsigned long long>(failure));
}

bool CreateBank(void* effect, uint64_t calibrationGeneration)
{
    ResourcePair primary{};
    bool primaryClear = true;
    if (!ReadLiveState(effect, primary, primaryClear)) {
        Fault("primary_resources_unreadable", effect);
        return false;
    }

    g_createResources(effect);
    ResourcePair secondary{};
    bool secondaryClear = true;
    if (!ReadLiveState(effect, secondary, secondaryClear)) {
        WriteLiveState(effect, primary, primaryClear);
        Fault("secondary_resources_unreadable", effect);
        return false;
    }

    EffectState state;
    if (!per_eye_post_effect_math::Initialize(
            state.bank,
            reinterpret_cast<uintptr_t>(effect),
            primary,
            primaryClear,
            secondary,
            calibrationGeneration)
        || !WriteLiveState(effect, primary, primaryClear)) {
        if (secondary.framebuffer != primary.framebuffer
            && secondary.texture != primary.texture) {
            WriteLiveState(effect, secondary, true);
            g_destroyResources(effect);
        }
        WriteLiveState(effect, primary, primaryClear);
        Fault("resource_bank_initialization_failed", effect);
        return false;
    }

    state.stereoActive = true;
    g_effects.emplace(effect, state);
    const uint64_t allocation = g_allocations.fetch_add(1, std::memory_order_relaxed) + 1;
    Logger::Instance().Write(
        LogLevel::Warn,
        "hpl_per_eye_post_effect allocated=%llu effect=%p leftFramebuffer=%p leftTexture=%p rightFramebuffer=%p rightTexture=%p clearLeft=%d clearRight=1",
        static_cast<unsigned long long>(allocation),
        effect,
        reinterpret_cast<void*>(primary.framebuffer),
        reinterpret_cast<void*>(primary.texture),
        reinterpret_cast<void*>(secondary.framebuffer),
        reinterpret_cast<void*>(secondary.texture),
        primaryClear ? 1 : 0);
    return true;
}

void RestorePrimary(EffectState& state, void* effect, bool forceClear)
{
    if (forceClear) {
        for (auto& eye : state.bank.eyes) {
            eye.clear = true;
            eye.lastPoseFrame = 0;
        }
    }
    const auto& primary = state.bank.eyes[0];
    if (WriteLiveState(effect, primary.resources, forceClear ? true : primary.clear)) {
        state.bank.boundEye = 0;
        state.bank.pendingEye = -1;
        state.bank.pendingPoseFrame = 0;
    }
}

} // namespace

void InitializeHPLPerEyePostEffect(
    const Config& config,
    HPLImageTrailResourceFn createResources,
    HPLImageTrailResourceFn destroyResources)
{
    std::lock_guard lock(g_mutex);
    g_configured = config.hplPerEyeImageTrailControl;
    g_faulted = false;
    g_createResources = createResources;
    g_destroyResources = destroyResources;
    g_effects.clear();
    g_activePass = {};
    g_allocations.store(0, std::memory_order_relaxed);
    g_restores.store(0, std::memory_order_relaxed);
    g_captures.store(0, std::memory_order_relaxed);
    g_resets.store(0, std::memory_order_relaxed);
    g_releases.store(0, std::memory_order_relaxed);
    g_failures.store(0, std::memory_order_relaxed);
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_per_eye_post_effect initialized configured=%d createReady=%d destroyReady=%d policy=image_trail_resource_pair_and_clear_flag maxPoseFrameGap=%llu",
        g_configured ? 1 : 0,
        g_createResources != nullptr ? 1 : 0,
        g_destroyResources != nullptr ? 1 : 0,
        static_cast<unsigned long long>(per_eye_post_effect_math::kMaxPoseFrameGap));
}

bool IsHPLPerEyeImageTrailAvailable()
{
    std::lock_guard lock(g_mutex);
    return g_configured && !g_faulted && g_createResources != nullptr && g_destroyResources != nullptr;
}

void BeginHPLPerEyePostEffect(
    void* effect,
    bool imageTrail,
    bool stereoEligible,
    int eyeIndex,
    uint64_t poseFrame,
    uint64_t calibrationGeneration)
{
    g_activePass = {};
    if (!imageTrail || effect == nullptr) return;

    std::lock_guard lock(g_mutex);
    if (!g_configured || g_faulted || g_createResources == nullptr || g_destroyResources == nullptr) {
        return;
    }

    auto found = g_effects.find(effect);
    if (!stereoEligible || (eyeIndex != 0 && eyeIndex != 1) || poseFrame == 0) {
        if (found != g_effects.end() && found->second.stereoActive) {
            RestorePrimary(found->second, effect, true);
            found->second.stereoActive = false;
        }
        return;
    }
    if (found == g_effects.end()) {
        if (!CreateBank(effect, calibrationGeneration)) return;
        found = g_effects.find(effect);
        if (found == g_effects.end()) {
            Fault("resource_bank_missing_after_creation", effect);
            return;
        }
    }
    if (!found->second.stereoActive) {
        RestorePrimary(found->second, effect, true);
        found->second.stereoActive = true;
    }

    ResourcePair live{};
    bool liveClear = true;
    if (!ReadLiveState(effect, live, liveClear)) {
        Fault("live_resources_unreadable", effect);
        return;
    }
    const auto prepared = per_eye_post_effect_math::Prepare(
        found->second.bank,
        reinterpret_cast<uintptr_t>(effect),
        eyeIndex,
        poseFrame,
        calibrationGeneration,
        live);
    if (!prepared.valid || !WriteLiveState(effect, prepared.resources, prepared.clear)) {
        RestorePrimary(found->second, effect, true);
        Fault("eye_resource_restore_failed", effect);
        return;
    }

    if (prepared.reset) g_resets.fetch_add(1, std::memory_order_relaxed);
    const uint64_t restore = g_restores.fetch_add(1, std::memory_order_relaxed) + 1;
    if (prepared.reset || restore <= 8 || restore % 300 == 0) {
        Logger::Instance().Write(
            LogLevel::Info,
            "hpl_per_eye_post_effect restore=%llu effect=%p eye=%d poseFrame=%llu calibration=%llu framebuffer=%p texture=%p clear=%d reset=%d",
            static_cast<unsigned long long>(restore),
            effect,
            eyeIndex,
            static_cast<unsigned long long>(poseFrame),
            static_cast<unsigned long long>(calibrationGeneration),
            reinterpret_cast<void*>(prepared.resources.framebuffer),
            reinterpret_cast<void*>(prepared.resources.texture),
            prepared.clear ? 1 : 0,
            prepared.reset ? 1 : 0);
    }
    g_activePass = {true, effect, eyeIndex, poseFrame};
}

void EndHPLPerEyePostEffect(void* effect)
{
    const ActivePass pass = g_activePass;
    g_activePass = {};
    if (!pass.active) return;

    std::lock_guard lock(g_mutex);
    auto found = g_effects.find(effect);
    ResourcePair live{};
    bool clear = true;
    if (effect != pass.effect || found == g_effects.end() || !ReadLiveState(effect, live, clear)
        || !per_eye_post_effect_math::Commit(
            found->second.bank,
            reinterpret_cast<uintptr_t>(effect),
            pass.eyeIndex,
            pass.poseFrame,
            live,
            clear)) {
        if (found != g_effects.end()) RestorePrimary(found->second, effect, true);
        Fault("eye_resource_capture_failed", effect);
        return;
    }
    const uint64_t capture = g_captures.fetch_add(1, std::memory_order_relaxed) + 1;
    if (capture <= 8 || capture % 300 == 0) {
        Logger::Instance().Write(
            LogLevel::Info,
            "hpl_per_eye_post_effect capture=%llu effect=%p eye=%d poseFrame=%llu framebuffer=%p texture=%p clear=%d",
            static_cast<unsigned long long>(capture),
            effect,
            pass.eyeIndex,
            static_cast<unsigned long long>(pass.poseFrame),
            reinterpret_cast<void*>(live.framebuffer),
            reinterpret_cast<void*>(live.texture),
            clear ? 1 : 0);
    }
}

void DestroyHPLPerEyeImageTrail(void* effect)
{
    if (g_destroyResources == nullptr) return;

    EffectState state;
    bool tracked = false;
    {
        std::lock_guard lock(g_mutex);
        const auto found = g_effects.find(effect);
        if (found != g_effects.end()) {
            state = found->second;
            g_effects.erase(found);
            tracked = true;
        }
    }
    if (!tracked) {
        g_destroyResources(effect);
        return;
    }

    WriteLiveState(effect, state.bank.eyes[1].resources, state.bank.eyes[1].clear);
    g_destroyResources(effect);
    WriteLiveState(effect, state.bank.eyes[0].resources, state.bank.eyes[0].clear);
    g_destroyResources(effect);
    g_releases.fetch_add(2, std::memory_order_relaxed);
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_per_eye_post_effect destroy effect=%p releasedPairs=2",
        effect);
}

HPLPerEyePostEffectStatus GetHPLPerEyePostEffectStatus()
{
    std::lock_guard lock(g_mutex);
    return {
        g_configured,
        g_configured && !g_faulted && g_createResources != nullptr && g_destroyResources != nullptr,
        g_faulted,
        static_cast<uint64_t>(g_effects.size()),
        g_allocations.load(std::memory_order_relaxed),
        g_restores.load(std::memory_order_relaxed),
        g_captures.load(std::memory_order_relaxed),
        g_resets.load(std::memory_order_relaxed),
        g_releases.load(std::memory_order_relaxed),
        g_failures.load(std::memory_order_relaxed),
    };
}

void RemoveHPLPerEyePostEffect()
{
    std::vector<std::pair<void*, EffectState>> effects;
    {
        std::lock_guard lock(g_mutex);
        effects.reserve(g_effects.size());
        for (const auto& entry : g_effects) effects.push_back(entry);
        g_effects.clear();
        g_activePass = {};
    }

    if (g_destroyResources != nullptr) {
        for (auto& [effect, state] : effects) {
            WriteLiveState(effect, state.bank.eyes[1].resources, state.bank.eyes[1].clear);
            g_destroyResources(effect);
            RestorePrimary(state, effect, true);
            g_releases.fetch_add(1, std::memory_order_relaxed);
        }
    }
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_per_eye_post_effect removed trackedEffects=%llu secondaryResourcesReleased=%llu primaryPolicy=restored_for_native_destruction",
        static_cast<unsigned long long>(effects.size()),
        static_cast<unsigned long long>(g_releases.load(std::memory_order_relaxed)));
    g_configured = false;
    g_faulted = false;
    g_createResources = nullptr;
    g_destroyResources = nullptr;
}

} // namespace somavr
