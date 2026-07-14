#include "HPLHudBridge.h"

#include "HPLCompatibilityProbe.h"
#include "Logger.h"
#include "OpenGLHooks.h"
#include "OpenXRRuntime.h"

#include <Windows.h>
#include <gl/GL.h>

#include <MinHook.h>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <vector>

namespace somavr {
namespace {

constexpr uintptr_t kGuiSetRenderRva = 0x213970;
constexpr uintptr_t kGetGameHudSetRva = 0x0cc9b0;
constexpr GLenum kGlDrawFramebufferBinding = 0x8ca6;
constexpr GLenum kGlCurrentProgram = 0x8b8d;

using GuiSetRenderFn = void (*)(void*, void*);
using GlGetIntegervFn = void(APIENTRY*)(GLenum, GLint*);

Config g_config;
OpenXRRuntime* g_openxr = nullptr;
GuiSetRenderFn g_originalGuiSetRender = nullptr;
void** g_gameContextSlot = nullptr;
GlGetIntegervFn g_glGetIntegerv = nullptr;
void* g_hookTarget = nullptr;
std::mutex g_mutex;
std::vector<void*> g_seenGuiSets;
std::atomic<uint64_t> g_renderCalls = 0;
std::atomic<uint64_t> g_gameHudMatches = 0;
std::atomic<uint64_t> g_captureAttempts = 0;
std::atomic<uint64_t> g_captureStarts = 0;
std::atomic<uint64_t> g_captureCompletions = 0;
std::atomic<uint64_t> g_captureFallbacks = 0;

bool IsInsideImage(HMODULE module, uintptr_t rva, size_t bytes)
{
    if (module == nullptr) {
        return false;
    }
    const auto* base = reinterpret_cast<const std::byte*>(module);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        return false;
    }
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        return false;
    }
    const size_t imageSize = nt->OptionalHeader.SizeOfImage;
    return rva < imageSize && bytes <= imageSize - rva;
}

bool IsReadable(const void* address, size_t size)
{
    if (address == nullptr || size == 0) {
        return false;
    }
    MEMORY_BASIC_INFORMATION memory = {};
    if (VirtualQuery(address, &memory, sizeof(memory)) == 0
        || memory.State != MEM_COMMIT
        || (memory.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) {
        return false;
    }
    const uintptr_t start = reinterpret_cast<uintptr_t>(address);
    const uintptr_t end = start + size;
    const uintptr_t regionEnd = reinterpret_cast<uintptr_t>(memory.BaseAddress) + memory.RegionSize;
    return end >= start && end <= regionEnd;
}

template <typename T>
bool ReadField(const void* base, size_t offset, T& value)
{
    if (base == nullptr) {
        return false;
    }
    const auto* address = reinterpret_cast<const std::byte*>(base) + offset;
    if (!IsReadable(address, sizeof(T))) {
        return false;
    }
    std::memcpy(&value, address, sizeof(T));
    return true;
}

bool ResolveGameHudIdentity(HMODULE executable)
{
    if (!IsInsideImage(executable, kGetGameHudSetRva, 12)) {
        return false;
    }
    const auto* target = reinterpret_cast<const uint8_t*>(executable) + kGetGameHudSetRva;
    static constexpr uint8_t kPrefix[] = {0x48, 0x8b, 0x05};
    static constexpr uint8_t kSuffix[] = {0x48, 0x8b, 0x40, 0x50, 0xc3};
    if (std::memcmp(target, kPrefix, sizeof(kPrefix)) != 0
        || std::memcmp(target + 7, kSuffix, sizeof(kSuffix)) != 0) {
        return false;
    }
    int32_t displacement = 0;
    std::memcpy(&displacement, target + 3, sizeof(displacement));
    g_gameContextSlot = reinterpret_cast<void**>(
        reinterpret_cast<uintptr_t>(target + 7) + displacement);
    return IsReadable(g_gameContextSlot, sizeof(void*));
}

void ReadGlIds(GLint& framebuffer, GLint& program)
{
    framebuffer = 0;
    program = 0;
    if (g_glGetIntegerv != nullptr && wglGetCurrentContext() != nullptr) {
        g_glGetIntegerv(kGlDrawFramebufferBinding, &framebuffer);
        g_glGetIntegerv(kGlCurrentProgram, &program);
    }
}

void HookGuiSetRender(void* guiSet, void* renderTarget)
{
    const uint64_t call = g_renderCalls.fetch_add(1, std::memory_order_relaxed) + 1;
    const uint64_t frame = GetOpenGLRenderFrameHint();
    void* gameContext = nullptr;
    void* gameHudSet = nullptr;
    float hudVirtualCenterWidth = 0.0f;
    float hudVirtualCenterHeight = 0.0f;
    float hudVirtualWidth = 0.0f;
    float hudVirtualHeight = 0.0f;
    float hudVirtualStartX = 0.0f;
    float hudVirtualStartY = 0.0f;
    float hudVirtualStartZ = 0.0f;
    float hudCenterScreenWidth = 0.0f;
    float hudCenterScreenHeight = 0.0f;
    float hudCenterScreenStartX = 0.0f;
    float hudCenterScreenStartY = 0.0f;
    float hudCenterScreenStartZ = 0.0f;
    if (g_gameContextSlot != nullptr
        && ReadField(g_gameContextSlot, 0, gameContext)
        && gameContext != nullptr) {
        ReadField(gameContext, 0x50, gameHudSet);
        ReadField(gameContext, 0x58, hudVirtualCenterWidth);
        ReadField(gameContext, 0x5c, hudVirtualCenterHeight);
        ReadField(gameContext, 0x60, hudVirtualWidth);
        ReadField(gameContext, 0x64, hudVirtualHeight);
        ReadField(gameContext, 0x70, hudVirtualStartX);
        ReadField(gameContext, 0x74, hudVirtualStartY);
        ReadField(gameContext, 0x78, hudVirtualStartZ);
        ReadField(gameContext, 0x7c, hudCenterScreenWidth);
        ReadField(gameContext, 0x80, hudCenterScreenHeight);
        ReadField(gameContext, 0x84, hudCenterScreenStartX);
        ReadField(gameContext, 0x88, hudCenterScreenStartY);
        ReadField(gameContext, 0x8c, hudCenterScreenStartZ);
    }
    const bool isGameHud = guiSet != nullptr && guiSet == gameHudSet;
    if (isGameHud) {
        g_gameHudMatches.fetch_add(1, std::memory_order_relaxed);
    }

    GLint framebufferBefore = 0;
    GLint programBefore = 0;
    ReadGlIds(framebufferBefore, programBefore);
    const OpenGLTelemetrySnapshot telemetryBefore = GetOpenGLTelemetrySnapshot();

    bool captureStarted = false;
    if (isGameHud && g_config.openxrHudLayer && g_openxr != nullptr) {
        g_captureAttempts.fetch_add(1, std::memory_order_relaxed);
        captureStarted = g_openxr->BeginHudCapture(frame);
        if (captureStarted) {
            g_captureStarts.fetch_add(1, std::memory_order_relaxed);
        } else {
            g_captureFallbacks.fetch_add(1, std::memory_order_relaxed);
        }
    }

    g_originalGuiSetRender(guiSet, renderTarget);
    bool captureCompleted = false;
    if (captureStarted) {
        captureCompleted = g_openxr->EndHudCapture(frame);
        if (captureCompleted) {
            g_captureCompletions.fetch_add(1, std::memory_order_relaxed);
        } else {
            g_captureFallbacks.fetch_add(1, std::memory_order_relaxed);
        }
    }

    const OpenGLTelemetrySnapshot telemetryAfter = GetOpenGLTelemetrySnapshot();
    GLint framebufferAfter = 0;
    GLint programAfter = 0;
    ReadGlIds(framebufferAfter, programAfter);

    bool newlySeen = false;
    {
        std::lock_guard lock(g_mutex);
        if (std::find(g_seenGuiSets.begin(), g_seenGuiSets.end(), guiSet) == g_seenGuiSets.end()
            && g_seenGuiSets.size() < 256) {
            g_seenGuiSets.push_back(guiSet);
            newlySeen = true;
        }
    }
    const uint64_t interval = static_cast<uint64_t>(std::max(g_config.hplCompatibilityLogInterval, 1));
    if (newlySeen || call <= 16 || call % interval == 0 || (isGameHud && captureStarted != captureCompleted)) {
        uint8_t is3d = 0;
        uint8_t depthLayer = 0;
        float virtualWidth = 0.0f;
        float virtualHeight = 0.0f;
        float offsetX = 0.0f;
        float offsetY = 0.0f;
        float depthMin = 0.0f;
        float depthMax = 0.0f;
        int32_t priority = 0;
        ReadField(guiSet, 0x139, is3d);
        ReadField(guiSet, 0x138, depthLayer);
        ReadField(guiSet, 0x100, virtualWidth);
        ReadField(guiSet, 0x104, virtualHeight);
        ReadField(guiSet, 0x108, offsetX);
        ReadField(guiSet, 0x10c, offsetY);
        ReadField(guiSet, 0x110, depthMin);
        ReadField(guiSet, 0x114, depthMax);
        ReadField(guiSet, 0x188, priority);
        Logger::Instance().Write(
            LogLevel::Info,
            "hpl_gui_set frame=%llu call=%llu stage=%s set=%p target=%p gameHud=%d gameHudSet=%p is3d=%d depthLayer=%d virtualSize=%.1f,%.1f offset=%.1f,%.1f depthRange=%.3f,%.3f priority=%d hudMetrics={virtualCenterSize=%.1f,%.1f virtualSize=%.1f,%.1f virtualStart=%.1f,%.1f,%.1f centerScreenSize=%.1f,%.1f centerScreenStart=%.1f,%.1f,%.1f} hudCapture={enabled=%d started=%d completed=%d} calls={drawElements=%llu drawArrays=%llu framebuffer=%llu program=%llu} gl={fbo=%d->%d program=%d->%d}",
            static_cast<unsigned long long>(frame),
            static_cast<unsigned long long>(call),
            GetHPLRenderStageName(GetActiveHPLRenderStage()),
            guiSet,
            renderTarget,
            isGameHud ? 1 : 0,
            gameHudSet,
            is3d != 0 ? 1 : 0,
            depthLayer != 0 ? 1 : 0,
            virtualWidth,
            virtualHeight,
            offsetX,
            offsetY,
            depthMin,
            depthMax,
            priority,
            hudVirtualCenterWidth,
            hudVirtualCenterHeight,
            hudVirtualWidth,
            hudVirtualHeight,
            hudVirtualStartX,
            hudVirtualStartY,
            hudVirtualStartZ,
            hudCenterScreenWidth,
            hudCenterScreenHeight,
            hudCenterScreenStartX,
            hudCenterScreenStartY,
            hudCenterScreenStartZ,
            g_config.openxrHudLayer ? 1 : 0,
            captureStarted ? 1 : 0,
            captureCompleted ? 1 : 0,
            static_cast<unsigned long long>(telemetryAfter.drawElements - telemetryBefore.drawElements),
            static_cast<unsigned long long>(telemetryAfter.drawArrays - telemetryBefore.drawArrays),
            static_cast<unsigned long long>(telemetryAfter.framebufferBinds - telemetryBefore.framebufferBinds),
            static_cast<unsigned long long>(telemetryAfter.programUses - telemetryBefore.programUses),
            framebufferBefore,
            framebufferAfter,
            programBefore,
            programAfter);
    }
}

} // namespace

bool InstallHPLHudBridge(const Config& config, OpenXRRuntime* openxr)
{
    std::lock_guard lock(g_mutex);
    if (!config.hplRenderStageProbe && !config.openxrHudLayer) {
        Logger::Instance().Write(LogLevel::Info, "hpl_hud_bridge install_skipped enabled=0");
        return true;
    }
    if (g_hookTarget != nullptr) {
        return true;
    }

    g_config = config;
    g_openxr = openxr;
    HMODULE executable = GetModuleHandleW(nullptr);
    const bool identityResolved = ResolveGameHudIdentity(executable);
    if (!identityResolved) {
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_hud_bridge install_failed reason=game_hud_identity rva=0x%llx",
            static_cast<unsigned long long>(kGetGameHudSetRva));
        return false;
    }

    const HMODULE opengl32 = GetModuleHandleW(L"opengl32.dll");
    g_glGetIntegerv = opengl32 != nullptr
        ? reinterpret_cast<GlGetIntegervFn>(GetProcAddress(opengl32, "glGetIntegerv"))
        : nullptr;

    static constexpr uint8_t kGuiSetRenderSignature[] = {
        0x48, 0x89, 0x5c, 0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x18,
        0x48, 0x89, 0x7c, 0x24, 0x20, 0x55,
    };
    if (!IsInsideImage(executable, kGuiSetRenderRva, sizeof(kGuiSetRenderSignature))) {
        return false;
    }
    void* target = reinterpret_cast<std::byte*>(executable) + kGuiSetRenderRva;
    if (std::memcmp(target, kGuiSetRenderSignature, sizeof(kGuiSetRenderSignature)) != 0) {
        Logger::Instance().Write(LogLevel::Error, "hpl_hud_bridge install_failed reason=render_signature");
        return false;
    }
    MH_STATUS status = MH_CreateHook(
        target,
        reinterpret_cast<void*>(&HookGuiSetRender),
        reinterpret_cast<void**>(&g_originalGuiSetRender));
    if (status != MH_OK) {
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_hud_bridge install_failed reason=create_hook status=%s",
            MH_StatusToString(status));
        return false;
    }
    status = MH_EnableHook(target);
    if (status != MH_OK && status != MH_ERROR_ENABLED) {
        MH_RemoveHook(target);
        g_originalGuiSetRender = nullptr;
        Logger::Instance().Write(
            LogLevel::Error,
            "hpl_hud_bridge install_failed reason=enable_hook status=%s",
            MH_StatusToString(status));
        return false;
    }
    g_hookTarget = target;
    Logger::Instance().Write(
        LogLevel::Warn,
        "hpl_hud_bridge installed rva=0x%llx identityRva=0x%llx layer=%d size=%dx%d distance=%.3f widthMeters=%.3f verticalOffset=%.3f maxAgeFrames=%d",
        static_cast<unsigned long long>(kGuiSetRenderRva),
        static_cast<unsigned long long>(kGetGameHudSetRva),
        config.openxrHudLayer ? 1 : 0,
        config.openxrHudWidthPixels,
        config.openxrHudHeightPixels,
        config.openxrHudDistanceMeters,
        config.openxrHudWidthMeters,
        config.openxrHudVerticalOffsetMeters,
        config.openxrHudMaxAgeFrames);
    return true;
}

void LogHPLHudBridgeSummary()
{
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_hud_summary calls=%llu gameHudMatches=%llu captureAttempts=%llu captureStarts=%llu captureCompletions=%llu captureFallbacks=%llu installed=%d",
        static_cast<unsigned long long>(g_renderCalls.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_gameHudMatches.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_captureAttempts.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_captureStarts.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_captureCompletions.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_captureFallbacks.load(std::memory_order_relaxed)),
        g_hookTarget != nullptr ? 1 : 0);
}

void RemoveHPLHudBridge()
{
    std::lock_guard lock(g_mutex);
    if (g_hookTarget != nullptr) {
        MH_DisableHook(g_hookTarget);
        MH_RemoveHook(g_hookTarget);
        g_hookTarget = nullptr;
    }
    g_originalGuiSetRender = nullptr;
    g_gameContextSlot = nullptr;
    g_glGetIntegerv = nullptr;
    g_openxr = nullptr;
    g_seenGuiSets.clear();
    Logger::Instance().Write(LogLevel::Info, "hpl_hud_bridge removed");
}

} // namespace somavr
