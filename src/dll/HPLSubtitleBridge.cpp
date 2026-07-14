#include "HPLSubtitleBridge.h"

#include "HPLCameraBridge.h"
#include "HPLSubtitleMath.h"
#include "Logger.h"

#include <Windows.h>

#include <MinHook.h>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>

namespace somavr {
namespace {

constexpr uintptr_t kVoiceSubtitleRenderRva = 0x1c8dd0;
constexpr size_t kVoiceOwnerOffset = 0x10;
constexpr size_t kMaxTextWidthOffset = 0x174;
constexpr size_t kTextYOffset = 0x178;
constexpr size_t kFontSizeOffset = 0x17c;
constexpr size_t kShadowOffset = 0x180;

constexpr uint8_t kVoiceSubtitleRenderSignature[] = {
    0x40, 0x55, 0x56, 0x41, 0x54, 0x41, 0x56,
    0x48, 0x8d, 0xac, 0x24, 0x48, 0xff, 0xff, 0xff,
    0x48, 0x81, 0xec, 0xb8, 0x01, 0x00, 0x00,
    0x45, 0x33, 0xf6, 0x48, 0x8b, 0xf1,
};

using VoiceSubtitleRenderFn = void (*)(void* subtitleRenderer);

Config g_config;
VoiceSubtitleRenderFn g_originalVoiceSubtitleRender = nullptr;
void* g_hookTarget = nullptr;
std::mutex g_installMutex;
std::atomic<uint64_t> g_renderCalls = 0;
std::atomic<uint64_t> g_activeCalls = 0;
std::atomic<uint64_t> g_overrides = 0;
std::atomic<uint64_t> g_inactiveFallbacks = 0;
std::atomic<uint64_t> g_invalidRendererFallbacks = 0;
std::atomic<uint64_t> g_invalidLayoutFallbacks = 0;
thread_local bool g_insideHook = false;

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

bool IsReadableWritable(const void* address, size_t bytes)
{
    if (address == nullptr || bytes == 0) return false;
    MEMORY_BASIC_INFORMATION memory{};
    if (VirtualQuery(address, &memory, sizeof(memory)) == 0
        || memory.State != MEM_COMMIT
        || (memory.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) {
        return false;
    }
    const DWORD protection = memory.Protect & 0xff;
    if (protection != PAGE_READWRITE
        && protection != PAGE_WRITECOPY
        && protection != PAGE_EXECUTE_READWRITE
        && protection != PAGE_EXECUTE_WRITECOPY) {
        return false;
    }
    const uintptr_t start = reinterpret_cast<uintptr_t>(address);
    const uintptr_t end = start + bytes;
    const uintptr_t regionEnd = reinterpret_cast<uintptr_t>(memory.BaseAddress) + memory.RegionSize;
    return end >= start && end <= regionEnd;
}

bool ReadOwner(void* subtitleRenderer, void*& owner)
{
    owner = nullptr;
    if (!IsReadableWritable(subtitleRenderer, kVoiceOwnerOffset + sizeof(owner))) return false;
    std::memcpy(
        &owner,
        reinterpret_cast<const std::byte*>(subtitleRenderer) + kVoiceOwnerOffset,
        sizeof(owner));
    return owner != nullptr
        && IsReadableWritable(owner, kShadowOffset + sizeof(float));
}

subtitle_math::SubtitleLayout ReadLayout(const void* owner)
{
    subtitle_math::SubtitleLayout layout;
    const auto* bytes = reinterpret_cast<const std::byte*>(owner);
    std::memcpy(&layout.maxTextWidth, bytes + kMaxTextWidthOffset, sizeof(float));
    std::memcpy(&layout.textY, bytes + kTextYOffset, sizeof(float));
    std::memcpy(&layout.fontSize, bytes + kFontSizeOffset, sizeof(float));
    std::memcpy(&layout.shadowOffset, bytes + kShadowOffset, sizeof(float));
    return layout;
}

void WriteLayout(void* owner, const subtitle_math::SubtitleLayout& layout)
{
    auto* bytes = reinterpret_cast<std::byte*>(owner);
    std::memcpy(bytes + kMaxTextWidthOffset, &layout.maxTextWidth, sizeof(float));
    std::memcpy(bytes + kTextYOffset, &layout.textY, sizeof(float));
    std::memcpy(bytes + kFontSizeOffset, &layout.fontSize, sizeof(float));
    std::memcpy(bytes + kShadowOffset, &layout.shadowOffset, sizeof(float));
}

void HookVoiceSubtitleRender(void* subtitleRenderer)
{
    g_renderCalls.fetch_add(1, std::memory_order_relaxed);
    if (g_insideHook) {
        g_originalVoiceSubtitleRender(subtitleRenderer);
        return;
    }

    const HPLCameraBridgeStatus camera = GetHPLCameraBridgeStatus();
    if (!camera.trackingEnabled || !camera.stereoEnabled) {
        g_inactiveFallbacks.fetch_add(1, std::memory_order_relaxed);
        g_originalVoiceSubtitleRender(subtitleRenderer);
        return;
    }
    g_activeCalls.fetch_add(1, std::memory_order_relaxed);

    void* owner = nullptr;
    if (!ReadOwner(subtitleRenderer, owner)) {
        g_invalidRendererFallbacks.fetch_add(1, std::memory_order_relaxed);
        g_originalVoiceSubtitleRender(subtitleRenderer);
        return;
    }

    const subtitle_math::SubtitleLayout nativeLayout = ReadLayout(owner);
    subtitle_math::SubtitleLayout vrLayout;
    if (!subtitle_math::BuildSubtitleLayout(
            nativeLayout,
            {
                g_config.hplSubtitleWidthScale,
                g_config.hplSubtitleFontScale,
                g_config.hplSubtitleVerticalOffset,
            },
            vrLayout)) {
        g_invalidLayoutFallbacks.fetch_add(1, std::memory_order_relaxed);
        g_originalVoiceSubtitleRender(subtitleRenderer);
        return;
    }

    g_insideHook = true;
    WriteLayout(owner, vrLayout);
    g_originalVoiceSubtitleRender(subtitleRenderer);
    WriteLayout(owner, nativeLayout);
    g_insideHook = false;
    const uint64_t overrideCount = g_overrides.fetch_add(1, std::memory_order_relaxed) + 1;
    const uint64_t interval = static_cast<uint64_t>(
        std::max(g_config.hplCompatibilityLogInterval, 1));
    if (overrideCount <= 8 || overrideCount % interval == 0) {
        Logger::Instance().Write(
            LogLevel::Info,
            "hpl_subtitle_layout applied=%llu owner=%p native={width=%.2f y=%.2f font=%.2f shadow=%.2f} vr={width=%.2f y=%.2f font=%.2f shadow=%.2f} restored=1",
            static_cast<unsigned long long>(overrideCount),
            owner,
            nativeLayout.maxTextWidth,
            nativeLayout.textY,
            nativeLayout.fontSize,
            nativeLayout.shadowOffset,
            vrLayout.maxTextWidth,
            vrLayout.textY,
            vrLayout.fontSize,
            vrLayout.shadowOffset);
    }
}

} // namespace

bool InstallHPLSubtitleBridge(const Config& config)
{
    std::lock_guard lock(g_installMutex);
    g_config = config;
    if (!config.hplSubtitleControl) {
        Logger::Instance().Write(LogLevel::Info, "hpl_subtitle_bridge disabled config=0");
        return true;
    }
    if (g_hookTarget != nullptr) return true;

    HMODULE executable = GetModuleHandleW(nullptr);
    if (!IsInsideImage(
            executable, kVoiceSubtitleRenderRva, sizeof(kVoiceSubtitleRenderSignature))) {
        Logger::Instance().Write(
            LogLevel::Error, "hpl_subtitle_bridge install_failed reason=invalid_image_range");
        return false;
    }
    auto* target = reinterpret_cast<std::byte*>(executable) + kVoiceSubtitleRenderRva;
    if (std::memcmp(
            target, kVoiceSubtitleRenderSignature, sizeof(kVoiceSubtitleRenderSignature)) != 0) {
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_subtitle_bridge install_failed reason=signature_mismatch rva=0x%llx",
            static_cast<unsigned long long>(kVoiceSubtitleRenderRva));
        return false;
    }

    MH_STATUS status = MH_CreateHook(
        target,
        reinterpret_cast<void*>(&HookVoiceSubtitleRender),
        reinterpret_cast<void**>(&g_originalVoiceSubtitleRender));
    if (status != MH_OK) {
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_subtitle_bridge install_failed reason=create_hook status=%s",
            MH_StatusToString(status));
        return false;
    }
    status = MH_EnableHook(target);
    if (status != MH_OK && status != MH_ERROR_ENABLED) {
        MH_RemoveHook(target);
        g_originalVoiceSubtitleRender = nullptr;
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_subtitle_bridge install_failed reason=enable_hook status=%s",
            MH_StatusToString(status));
        return false;
    }
    g_hookTarget = target;
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_subtitle_bridge install_ok rva=0x%llx widthScale=%.3f fontScale=%.3f verticalOffset=%.2f policy=active_stereo_native_draw_scoped_restore",
        static_cast<unsigned long long>(kVoiceSubtitleRenderRva),
        config.hplSubtitleWidthScale,
        config.hplSubtitleFontScale,
        config.hplSubtitleVerticalOffset);
    return true;
}

void LogHPLSubtitleBridgeSummary()
{
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_subtitle_bridge_summary installed=%d calls=%llu activeCalls=%llu overrides=%llu inactiveFallbacks=%llu invalidRendererFallbacks=%llu invalidLayoutFallbacks=%llu",
        g_hookTarget != nullptr ? 1 : 0,
        static_cast<unsigned long long>(g_renderCalls.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_activeCalls.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_overrides.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_inactiveFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_invalidRendererFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_invalidLayoutFallbacks.load(std::memory_order_relaxed)));
}

void RemoveHPLSubtitleBridge()
{
    std::lock_guard lock(g_installMutex);
    if (g_hookTarget != nullptr) {
        MH_DisableHook(g_hookTarget);
        MH_RemoveHook(g_hookTarget);
        g_hookTarget = nullptr;
    }
    g_originalVoiceSubtitleRender = nullptr;
    g_config = {};
    Logger::Instance().Write(LogLevel::Info, "hpl_subtitle_bridge removed");
}

} // namespace somavr
