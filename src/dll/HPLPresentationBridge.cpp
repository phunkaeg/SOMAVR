#include "HPLPresentationBridge.h"

#include "Config.h"
#include "Logger.h"
#include "OpenXRRuntime.h"

#include <Windows.h>

#include <MinHook.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_set>

namespace somavr {
namespace {

constexpr uintptr_t kGameContextSlotRva = 0x7925e0;
constexpr uintptr_t kIsLoadingScreenVisibleRva = 0x0ccdb0;
constexpr uintptr_t kCreateVideoRva = 0x488fa0;
constexpr uintptr_t kDestroyVideoRva = 0x488fd0;
constexpr size_t kNativeStringInlineCapacity = 15;
constexpr size_t kMaxVideoNameLength = 1024;

constexpr uint8_t kIsLoadingScreenVisibleSignature[] = {
    0x48, 0x8b, 0x0d, 0x29, 0x58, 0x6c, 0x00,
    0x48, 0x8b, 0x81, 0x48, 0x01, 0x00, 0x00,
    0x80, 0xb8,
};
constexpr uint8_t kCreateVideoSignature[] = {
    0x48, 0x89, 0x4c, 0x24, 0x08,
    0x48, 0x83, 0xec, 0x28,
    0x48, 0x8b, 0x0d, 0x08, 0xef, 0x31, 0x00,
};
constexpr uint8_t kDestroyVideoSignature[] = {
    0x48, 0x89, 0x4c, 0x24, 0x08,
    0x48, 0x83, 0xec, 0x38,
    0x48, 0x8b, 0x0d, 0xd8, 0xee, 0x31, 0x00,
};

struct NativeStringLayout {
    std::array<std::byte, 16> storage{};
    uint64_t size = 0;
    uint64_t capacity = 0;
};

using IsLoadingScreenVisibleFn = bool (*)();
using CreateVideoFn = void* (*)(const void* nativeName);
using DestroyVideoFn = void (*)(void* videoStream);

Config g_config;
OpenXRRuntime* g_openxr = nullptr;
IsLoadingScreenVisibleFn g_isLoadingScreenVisible = nullptr;
CreateVideoFn g_originalCreateVideo = nullptr;
DestroyVideoFn g_originalDestroyVideo = nullptr;
void* g_createVideoTarget = nullptr;
void* g_destroyVideoTarget = nullptr;
std::mutex g_installMutex;
std::mutex g_videoMutex;
std::unordered_set<void*> g_activeVideos;
std::atomic<bool> g_loadingActive = false;
std::atomic<bool> g_loadingInitialized = false;
std::atomic<uint64_t> g_lastUpdateFrame = 0;
std::atomic<uint64_t> g_loadingQueries = 0;
std::atomic<uint64_t> g_loadingQueryUnavailable = 0;
std::atomic<uint64_t> g_loadingEntries = 0;
std::atomic<uint64_t> g_loadingExits = 0;
std::atomic<uint64_t> g_videoCreates = 0;
std::atomic<uint64_t> g_videoCreateFailures = 0;
std::atomic<uint64_t> g_videoDestroys = 0;
std::atomic<uint64_t> g_videoNameReadFailures = 0;
std::atomic<uint64_t> g_videoPeakActive = 0;

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
        || layout.size > kMaxVideoNameLength
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

bool IsLoadingQueryReady()
{
    HMODULE executable = GetModuleHandleW(nullptr);
    if (!IsInsideImage(executable, kGameContextSlotRva, sizeof(void*))) return false;
    const auto* base = reinterpret_cast<const std::byte*>(executable);
    void* gameContext = nullptr;
    if (!ReadMemory(base + kGameContextSlotRva, &gameContext, sizeof(gameContext))
        || gameContext == nullptr) {
        return false;
    }
    void* loadingOwner = nullptr;
    void* screenOwner = nullptr;
    return ReadMemory(reinterpret_cast<const std::byte*>(gameContext) + 0x148,
               &loadingOwner, sizeof(loadingOwner))
        && ReadMemory(reinterpret_cast<const std::byte*>(gameContext) + 0x128,
            &screenOwner, sizeof(screenOwner))
        && loadingOwner != nullptr
        && screenOwner != nullptr;
}

void UpdatePeakActive(uint64_t active)
{
    uint64_t peak = g_videoPeakActive.load(std::memory_order_relaxed);
    while (active > peak
        && !g_videoPeakActive.compare_exchange_weak(
            peak, active, std::memory_order_relaxed, std::memory_order_relaxed)) {
    }
}

void* HookCreateVideo(const void* nativeName)
{
    std::string name;
    const bool nameValid = ReadNativeString(nativeName, name);
    if (!nameValid) g_videoNameReadFailures.fetch_add(1, std::memory_order_relaxed);

    void* stream = g_originalCreateVideo(nativeName);
    const uint64_t create = g_videoCreates.fetch_add(1, std::memory_order_relaxed) + 1;
    uint64_t active = 0;
    if (stream != nullptr) {
        std::lock_guard lock(g_videoMutex);
        g_activeVideos.insert(stream);
        active = static_cast<uint64_t>(g_activeVideos.size());
    } else {
        g_videoCreateFailures.fetch_add(1, std::memory_order_relaxed);
    }
    UpdatePeakActive(active);
    Logger::Instance().Write(
        stream != nullptr ? LogLevel::Info : LogLevel::Warn,
        "hpl_video_lifecycle event=create sequence=%llu stream=%p active=%llu nameValid=%d name=\"%s\" policy=probe_only",
        static_cast<unsigned long long>(create),
        stream,
        static_cast<unsigned long long>(active),
        nameValid ? 1 : 0,
        nameValid ? name.c_str() : "<unreadable>");
    return stream;
}

void HookDestroyVideo(void* videoStream)
{
    const uint64_t destroy = g_videoDestroys.fetch_add(1, std::memory_order_relaxed) + 1;
    bool known = false;
    uint64_t active = 0;
    {
        std::lock_guard lock(g_videoMutex);
        known = g_activeVideos.erase(videoStream) != 0;
        active = static_cast<uint64_t>(g_activeVideos.size());
    }
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_video_lifecycle event=destroy sequence=%llu stream=%p known=%d active=%llu policy=probe_only",
        static_cast<unsigned long long>(destroy),
        videoStream,
        known ? 1 : 0,
        static_cast<unsigned long long>(active));
    g_originalDestroyVideo(videoStream);
}

bool InstallMinHook(
    void* target, void* hook, void** original, void*& installedTarget, const char* name)
{
    MH_STATUS status = MH_CreateHook(target, hook, original);
    if (status != MH_OK) {
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_presentation_bridge install_failed reason=create_hook name=%s status=%s",
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
            "hpl_presentation_bridge install_failed reason=enable_hook name=%s status=%s",
            name,
            MH_StatusToString(status));
        return false;
    }
    installedTarget = target;
    return true;
}

void RemoveMinHook(void*& target)
{
    if (target == nullptr) return;
    MH_DisableHook(target);
    MH_RemoveHook(target);
    target = nullptr;
}

void RollbackHooks()
{
    RemoveMinHook(g_destroyVideoTarget);
    RemoveMinHook(g_createVideoTarget);
    g_originalCreateVideo = nullptr;
    g_originalDestroyVideo = nullptr;
}

} // namespace

bool InstallHPLPresentationBridge(const Config& config, OpenXRRuntime* openxr)
{
    std::lock_guard lock(g_installMutex);
    g_config = config;
    g_openxr = openxr;
    if (!config.hplLoadingScreenControl && !config.hplVideoLifecycleProbe) {
        Logger::Instance().Write(LogLevel::Info, "hpl_presentation_bridge disabled config=0");
        return true;
    }
    if (g_isLoadingScreenVisible != nullptr || g_createVideoTarget != nullptr
        || g_destroyVideoTarget != nullptr) {
        return true;
    }

    HMODULE executable = GetModuleHandleW(nullptr);
    const auto* base = reinterpret_cast<const std::byte*>(executable);
    const bool rangeValid = (!config.hplLoadingScreenControl
            || IsInsideImage(executable, kIsLoadingScreenVisibleRva,
                sizeof(kIsLoadingScreenVisibleSignature)))
        && (!config.hplVideoLifecycleProbe
            || (IsInsideImage(executable, kCreateVideoRva, sizeof(kCreateVideoSignature))
                && IsInsideImage(executable, kDestroyVideoRva,
                    sizeof(kDestroyVideoSignature))));
    if (!rangeValid) {
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_presentation_bridge install_failed reason=invalid_image_range");
        return false;
    }

    const bool signaturesValid = (!config.hplLoadingScreenControl
            || std::memcmp(base + kIsLoadingScreenVisibleRva,
                kIsLoadingScreenVisibleSignature,
                sizeof(kIsLoadingScreenVisibleSignature)) == 0)
        && (!config.hplVideoLifecycleProbe
            || (std::memcmp(base + kCreateVideoRva, kCreateVideoSignature,
                    sizeof(kCreateVideoSignature)) == 0
                && std::memcmp(base + kDestroyVideoRva, kDestroyVideoSignature,
                    sizeof(kDestroyVideoSignature)) == 0));
    if (!signaturesValid) {
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_presentation_bridge install_failed reason=signature_mismatch loadRva=0x%llx createVideoRva=0x%llx destroyVideoRva=0x%llx",
            static_cast<unsigned long long>(kIsLoadingScreenVisibleRva),
            static_cast<unsigned long long>(kCreateVideoRva),
            static_cast<unsigned long long>(kDestroyVideoRva));
        return false;
    }

    if (config.hplLoadingScreenControl) {
        g_isLoadingScreenVisible = reinterpret_cast<IsLoadingScreenVisibleFn>(
            const_cast<std::byte*>(base + kIsLoadingScreenVisibleRva));
    }
    if (config.hplVideoLifecycleProbe
        && (!InstallMinHook(
                const_cast<std::byte*>(base + kCreateVideoRva),
                reinterpret_cast<void*>(&HookCreateVideo),
                reinterpret_cast<void**>(&g_originalCreateVideo),
                g_createVideoTarget,
                "CreateVideo")
            || !InstallMinHook(
                const_cast<std::byte*>(base + kDestroyVideoRva),
                reinterpret_cast<void*>(&HookDestroyVideo),
                reinterpret_cast<void**>(&g_originalDestroyVideo),
                g_destroyVideoTarget,
                "DestroyVideo"))) {
        RollbackHooks();
        g_isLoadingScreenVisible = nullptr;
        return false;
    }

    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_presentation_bridge install_ok loadingControl=%d loadRva=0x%llx exitBlackoutFrames=%d videoProbe=%d createVideoRva=0x%llx destroyVideoRva=0x%llx",
        config.hplLoadingScreenControl ? 1 : 0,
        static_cast<unsigned long long>(kIsLoadingScreenVisibleRva),
        config.hplLoadingScreenExitBlackoutFrames,
        config.hplVideoLifecycleProbe ? 1 : 0,
        static_cast<unsigned long long>(kCreateVideoRva),
        static_cast<unsigned long long>(kDestroyVideoRva));
    return true;
}

void RemoveHPLPresentationBridge()
{
    std::lock_guard lock(g_installMutex);
    if (g_loadingActive.exchange(false, std::memory_order_relaxed) && g_openxr != nullptr) {
        g_openxr->SetPresentationBlackout(false, "loading_screen_bridge_removed");
    }
    RollbackHooks();
    g_isLoadingScreenVisible = nullptr;
    g_openxr = nullptr;
    g_loadingInitialized.store(false, std::memory_order_relaxed);
    {
        std::lock_guard videoLock(g_videoMutex);
        g_activeVideos.clear();
    }
    Logger::Instance().Write(LogLevel::Info, "hpl_presentation_bridge removed");
}

void UpdateHPLPresentationBridge(uint64_t frameIndex)
{
    if (g_isLoadingScreenVisible == nullptr
        || g_lastUpdateFrame.exchange(frameIndex, std::memory_order_relaxed) == frameIndex) {
        return;
    }
    if (!IsLoadingQueryReady()) {
        g_loadingQueryUnavailable.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    const bool visible = g_isLoadingScreenVisible();
    g_loadingQueries.fetch_add(1, std::memory_order_relaxed);
    const bool initialized = g_loadingInitialized.exchange(true, std::memory_order_relaxed);
    const bool previous = g_loadingActive.exchange(visible, std::memory_order_relaxed);
    if (initialized && visible == previous) return;

    if (visible) {
        const uint64_t entry = g_loadingEntries.fetch_add(1, std::memory_order_relaxed) + 1;
        if (g_openxr != nullptr) {
            g_openxr->InvalidateStereoCaches("loading_screen_enter");
            g_openxr->SetPresentationBlackout(true, "loading_screen");
        }
        Logger::Instance().Write(
            LogLevel::Warn,
            "hpl_loading_screen frame=%llu visible=1 entry=%llu action=blackout_and_invalidate",
            static_cast<unsigned long long>(frameIndex),
            static_cast<unsigned long long>(entry));
    } else if (initialized && previous) {
        const uint64_t exit = g_loadingExits.fetch_add(1, std::memory_order_relaxed) + 1;
        if (g_openxr != nullptr) {
            g_openxr->SetPresentationBlackout(false, "loading_screen_complete");
            g_openxr->InvalidateStereoCaches("loading_screen_exit");
            g_openxr->RequestComfortBlackout(
                static_cast<uint32_t>(g_config.hplLoadingScreenExitBlackoutFrames),
                "loading_screen_exit");
        }
        Logger::Instance().Write(
            LogLevel::Info,
            "hpl_loading_screen frame=%llu visible=0 exit=%llu action=rearm_stereo exitBlackoutFrames=%d",
            static_cast<unsigned long long>(frameIndex),
            static_cast<unsigned long long>(exit),
            g_config.hplLoadingScreenExitBlackoutFrames);
    } else {
        Logger::Instance().Write(
            LogLevel::Info,
            "hpl_loading_screen frame=%llu visible=0 state=initialized",
            static_cast<unsigned long long>(frameIndex));
    }
}

bool IsHPLLoadingScreenActive()
{
    return g_loadingActive.load(std::memory_order_relaxed);
}

void LogHPLPresentationBridgeSummary()
{
    uint64_t activeVideos = 0;
    {
        std::lock_guard lock(g_videoMutex);
        activeVideos = static_cast<uint64_t>(g_activeVideos.size());
    }
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_presentation_bridge_summary loadingInstalled=%d loadingActive=%d loadingQueries=%llu queryUnavailable=%llu entries=%llu exits=%llu videoProbeInstalled=%d creates=%llu createFailures=%llu destroys=%llu nameReadFailures=%llu activeVideos=%llu peakActiveVideos=%llu",
        g_isLoadingScreenVisible != nullptr ? 1 : 0,
        g_loadingActive.load(std::memory_order_relaxed) ? 1 : 0,
        static_cast<unsigned long long>(g_loadingQueries.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_loadingQueryUnavailable.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_loadingEntries.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_loadingExits.load(std::memory_order_relaxed)),
        g_createVideoTarget != nullptr && g_destroyVideoTarget != nullptr ? 1 : 0,
        static_cast<unsigned long long>(g_videoCreates.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_videoCreateFailures.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_videoDestroys.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_videoNameReadFailures.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(activeVideos),
        static_cast<unsigned long long>(g_videoPeakActive.load(std::memory_order_relaxed)));
}

} // namespace somavr
