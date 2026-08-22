#include "HPLHudBridge.h"

#include "HPLCompatibilityProbe.h"
#include "HPLNativeLocomotion.h"
#include "HPLPlayerState.h"
#include "HPLPresentationBridge.h"
#include "HPLTerminalMath.h"
#include "Logger.h"
#include "SomaBuildSignatures.h"
#include "OpenGLHooks.h"
#include "OpenXRRuntime.h"

#include <Windows.h>
#include <gl/GL.h>

#include <MinHook.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <vector>

namespace somavr {
namespace {

constexpr uintptr_t kGuiSetRenderRva = 0x213970;
constexpr uintptr_t kGetGameHudSetRva = 0x0cc9b0;
constexpr uintptr_t kGetCurrentImGuiRva = 0x0cca70;
constexpr uintptr_t kGetGameHudImGuiRva = 0x0cca90;
constexpr uintptr_t kImGuiGetSetRva = 0x071f20;
constexpr size_t kImGuiManagerWorldInputOffset = 0x170;
constexpr size_t kImGuiManagerFocusedWrapperOffset = 0x180;
constexpr size_t kImGuiWrapperSetOffset = 0x18;
constexpr GLenum kGlDrawFramebufferBinding = 0x8ca6;
constexpr GLenum kGlCurrentProgram = 0x8b8d;
constexpr GLenum kGlViewport = 0x0ba2;
constexpr int kDeadPlayerState = 17;
constexpr int kTerminalPlayerState = 8;
constexpr int kReadPlayerState = 10;
constexpr int kZoomAreaPlayerState = 18;
constexpr uint32_t kTerminalMissingClearFallbackSamples = 8;

using GuiSetRenderFn = void (*)(void*, void*);
using GetImGuiFn = void* (*)();
using ImGuiGetSetFn = void* (*)(void*);
using GlGetIntegervFn = void(APIENTRY*)(GLenum, GLint*);

Config g_config;
OpenXRRuntime* g_openxr = nullptr;
GuiSetRenderFn g_originalGuiSetRender = nullptr;
void** g_gameContextSlot = nullptr;
GlGetIntegervFn g_glGetIntegerv = nullptr;
GetImGuiFn g_getCurrentImGui = nullptr;
GetImGuiFn g_getGameHudImGui = nullptr;
ImGuiGetSetFn g_imGuiGetSet = nullptr;
void* g_hookTarget = nullptr;
std::mutex g_mutex;
std::vector<void*> g_seenGuiSets;
void* g_lastCurrentImGui = nullptr;
void* g_lastCurrentImGuiSet = nullptr;
int g_lastCurrentImGuiPlayerState = -1;
std::atomic<uint64_t> g_renderCalls = 0;
std::atomic<uint64_t> g_gameHudMatches = 0;
std::atomic<uint64_t> g_currentImGuiSetMatches = 0;
std::atomic<uint64_t> g_currentImGuiSurfaceFrame = 0;
std::atomic<uint64_t> g_gameHudImGuiSetMatches = 0;
std::atomic<uint64_t> g_gameHudImGuiCaptureCompletions = 0;
std::atomic<uint64_t> g_pauseQueries = 0;
std::atomic<uint64_t> g_pauseQueryFallbacks = 0;
std::atomic<uint64_t> g_pausedCurrentImGuiMatches = 0;
std::atomic<uint64_t> g_pausedMenuCaptureCompletions = 0;
std::atomic<uint64_t> g_deadCurrentImGuiMatches = 0;
std::atomic<uint64_t> g_deadCurrentImGuiCaptureCompletions = 0;
std::atomic<uint64_t> g_wakeCurrentImGuiMatches = 0;
std::atomic<uint64_t> g_wakeCurrentImGuiCaptureCompletions = 0;
std::atomic<uint64_t> g_inventoryCurrentImGuiMatches = 0;
std::atomic<uint64_t> g_inventoryCurrentImGuiCaptureCompletions = 0;
std::atomic<uint64_t> g_terminalOverlaySetMatches = 0;
std::atomic<uint64_t> g_terminalOverlayCaptureCompletions = 0;
std::atomic<uint64_t> g_terminalOverlayCaptureSamples = 0;
std::atomic<bool> g_terminalRetentionActive = false;
std::atomic<bool> g_terminalRetentionSessionInitialized = false;
std::atomic<bool> g_terminalRetentionAutoFallbackActive = false;
std::atomic<bool> g_terminalRetentionColorClearObserved = false;
std::atomic<uint32_t> g_terminalRetentionMissingClearSamples = 0;
std::atomic<uint64_t> g_terminalRetentionTransitions = 0;
std::atomic<uint64_t> g_terminalRetentionExternalResets = 0;
std::atomic<uint64_t> g_terminalRetentionAutoFallbacks = 0;
std::atomic<uint64_t> g_terminalRetentionScissorBypassBaseline = 0;
std::atomic<bool> g_terminalRetentionScissorRepairObserved = false;
std::atomic<uint64_t> g_readCurrentImGuiMatches = 0;
std::atomic<uint64_t> g_zoomCurrentImGuiMatches = 0;
std::atomic<uint64_t> g_currentImGuiPlayerStateTransitions = 0;
std::atomic<uint64_t> g_captureAttempts = 0;
std::atomic<uint64_t> g_captureStarts = 0;
std::atomic<uint64_t> g_captureCompletions = 0;
std::atomic<uint64_t> g_captureFallbacks = 0;
std::atomic<bool> g_terminalDumpF10Down = false;
std::atomic<uint64_t> g_terminalDumpSequence = 0;
std::atomic<uint32_t> g_terminalDumpFramesRemaining = 0;
std::atomic<uint32_t> g_terminalDumpSamples = 0;
std::atomic<uint32_t> g_terminalDumpFailures = 0;
std::atomic<uint64_t> g_terminalAutoProbeSessions = 0;

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
    static_assert(sizeof(kPrefix) == soma_signatures::kRipRelativeLoadDisplacementOffset);
    int32_t displacement = 0;
    std::memcpy(
        &displacement,
        target + soma_signatures::kRipRelativeLoadDisplacementOffset,
        sizeof(displacement));
    g_gameContextSlot = reinterpret_cast<void**>(
        reinterpret_cast<uintptr_t>(
            target + soma_signatures::kRipRelativeLoadNextInstructionOffset)
        + displacement);
    return IsReadable(g_gameContextSlot, sizeof(void*));
}

bool ResolveImGuiIdentity(HMODULE executable)
{
    static constexpr uint8_t kGetCurrentImGuiSignature[] = {
        0x48, 0x8b, 0x05, 0x69, 0x5b, 0x6c, 0x00,
        0x48, 0x8b, 0x80, 0xe8, 0x00, 0x00, 0x00,
        0x48, 0x8b, 0x80, 0x68, 0x01, 0x00, 0x00, 0xc3,
    };
    static constexpr uint8_t kGetGameHudImGuiSignature[] = {
        0x48, 0x8b, 0x05, 0x49, 0x5b, 0x6c, 0x00,
        0x48, 0x8b, 0x80, 0xe8, 0x00, 0x00, 0x00,
        0x48, 0x8b, 0x80, 0x60, 0x01, 0x00, 0x00, 0xc3,
    };
    static constexpr uint8_t kImGuiGetSetSignature[] = {
        0x48, 0x8b, 0x41, 0x18, 0xc3,
    };
    if (!IsInsideImage(executable, kGetCurrentImGuiRva, sizeof(kGetCurrentImGuiSignature))
        || !IsInsideImage(executable, kGetGameHudImGuiRva, sizeof(kGetGameHudImGuiSignature))
        || !IsInsideImage(executable, kImGuiGetSetRva, sizeof(kImGuiGetSetSignature))) {
        return false;
    }
    auto* base = reinterpret_cast<std::byte*>(executable);
    if (std::memcmp(base + kGetCurrentImGuiRva, kGetCurrentImGuiSignature,
            sizeof(kGetCurrentImGuiSignature)) != 0
        || std::memcmp(base + kGetGameHudImGuiRva, kGetGameHudImGuiSignature,
            sizeof(kGetGameHudImGuiSignature)) != 0
        || std::memcmp(base + kImGuiGetSetRva, kImGuiGetSetSignature,
            sizeof(kImGuiGetSetSignature)) != 0) {
        return false;
    }
    g_getCurrentImGui = reinterpret_cast<GetImGuiFn>(base + kGetCurrentImGuiRva);
    g_getGameHudImGui = reinterpret_cast<GetImGuiFn>(base + kGetGameHudImGuiRva);
    g_imGuiGetSet = reinterpret_cast<ImGuiGetSetFn>(base + kImGuiGetSetRva);
    return true;
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

void ReadGlViewport(GLint (&viewport)[4])
{
    std::fill(std::begin(viewport), std::end(viewport), 0);
    if (g_glGetIntegerv != nullptr && wglGetCurrentContext() != nullptr) {
        g_glGetIntegerv(kGlViewport, viewport);
    }
}

void HookGuiSetRender(void* guiSet, void* renderTarget)
{
    const uint64_t call = g_renderCalls.fetch_add(1, std::memory_order_relaxed) + 1;
    const uint64_t frame = GetOpenGLRenderFrameHint();
    void* gameContext = nullptr;
    void* gameHudSet = nullptr;
    void* imGuiManager = nullptr;
    void* worldInputImGui = nullptr;
    void* focusedWrapper = nullptr;
    void* terminalGuiSet = nullptr;
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
        ReadField(gameContext, 0xe8, imGuiManager);
        if (imGuiManager != nullptr) {
            ReadField(
                imGuiManager,
                kImGuiManagerWorldInputOffset,
                worldInputImGui);
            if (ReadField(
                    imGuiManager,
                    kImGuiManagerFocusedWrapperOffset,
                    focusedWrapper)
                && focusedWrapper != nullptr) {
                ReadField(focusedWrapper, kImGuiWrapperSetOffset, terminalGuiSet);
            }
        }
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
    void* currentImGui = imGuiManager != nullptr && g_getCurrentImGui != nullptr
        ? g_getCurrentImGui() : nullptr;
    void* gameHudImGui = imGuiManager != nullptr && g_getGameHudImGui != nullptr
        ? g_getGameHudImGui() : nullptr;
    void* currentImGuiSet = currentImGui != nullptr && g_imGuiGetSet != nullptr
        ? g_imGuiGetSet(currentImGui) : nullptr;
    void* gameHudImGuiSet = gameHudImGui != nullptr && g_imGuiGetSet != nullptr
        ? g_imGuiGetSet(gameHudImGui) : nullptr;
    const bool isCurrentImGuiSet = guiSet != nullptr && guiSet == currentImGuiSet;
    if (isCurrentImGuiSet) {
        g_currentImGuiSurfaceFrame.store(frame, std::memory_order_release);
    }
    const bool isGameHudImGuiSet = guiSet != nullptr && guiSet == gameHudImGuiSet;
    const bool isTerminalGuiSet = guiSet != nullptr
        && terminalGuiSet != nullptr
        && guiSet == terminalGuiSet;
    uint8_t is3d = 0;
    const bool guiDimensionalityKnown = ReadField(guiSet, 0x139, is3d);
    const bool isFlatGuiSet = guiDimensionalityKnown && is3d == 0;
    bool paused = false;
    bool pauseStateValid = false;
    if (g_config.openxrHudCapturePausedMenu && isCurrentImGuiSet) {
        g_pauseQueries.fetch_add(1, std::memory_order_relaxed);
        pauseStateValid = GetHPLGamePausedState(paused);
        if (!pauseStateValid) {
            g_pauseQueryFallbacks.fetch_add(1, std::memory_order_relaxed);
        }
    }
    const bool isPausedCurrentImGuiSet = isCurrentImGuiSet && pauseStateValid && paused;
    HPLPlayerStateSnapshot player;
    const bool playerStateValid = (isGameHud || isGameHudImGuiSet
            || isCurrentImGuiSet || isTerminalGuiSet)
        && GetHPLPlayerStateSnapshot(player)
        && player.playerValid;
    const bool terminalStateActive = g_config.hplControllerTerminalOverlay
        && playerStateValid
        && player.playerStateId == kTerminalPlayerState;
    const bool isDeadCurrentImGuiSet = g_config.hplScriptedPresentationControl
        && isCurrentImGuiSet && isFlatGuiSet && playerStateValid
        && player.playerStateId == kDeadPlayerState;
    const bool isWakeCurrentImGuiSet = g_config.hplScriptedPresentationControl
        && isCurrentImGuiSet && isFlatGuiSet && IsHPLWakePresentationActive();
    const bool isInventoryCurrentImGuiSet = g_config.hplInventoryPresentationControl
        && isCurrentImGuiSet && isFlatGuiSet && IsHPLInventoryPresentationActive();
    const bool isTerminalOverlaySet = g_config.hplControllerTerminalOverlay
        && isTerminalGuiSet
        && isFlatGuiSet
        && playerStateValid
        && player.playerStateId == kTerminalPlayerState;
    const bool isReadCurrentImGuiSet = isCurrentImGuiSet && isFlatGuiSet
        && playerStateValid && player.playerStateId == kReadPlayerState;
    const bool isZoomCurrentImGuiSet = isCurrentImGuiSet && isFlatGuiSet
        && playerStateValid && player.playerStateId == kZoomAreaPlayerState;
    if (isCurrentImGuiSet) {
        g_currentImGuiSetMatches.fetch_add(1, std::memory_order_relaxed);
    }
    if (isGameHudImGuiSet) {
        g_gameHudImGuiSetMatches.fetch_add(1, std::memory_order_relaxed);
    }
    if (isPausedCurrentImGuiSet) {
        g_pausedCurrentImGuiMatches.fetch_add(1, std::memory_order_relaxed);
    }
    if (isDeadCurrentImGuiSet) {
        g_deadCurrentImGuiMatches.fetch_add(1, std::memory_order_relaxed);
    }
    if (isWakeCurrentImGuiSet) {
        g_wakeCurrentImGuiMatches.fetch_add(1, std::memory_order_relaxed);
    }
    if (isInventoryCurrentImGuiSet) {
        g_inventoryCurrentImGuiMatches.fetch_add(1, std::memory_order_relaxed);
    }
    if (isTerminalOverlaySet) {
        g_terminalOverlaySetMatches.fetch_add(1, std::memory_order_relaxed);
    }
    if (isReadCurrentImGuiSet) {
        g_readCurrentImGuiMatches.fetch_add(1, std::memory_order_relaxed);
    }
    if (isZoomCurrentImGuiSet) {
        g_zoomCurrentImGuiMatches.fetch_add(1, std::memory_order_relaxed);
    }

    GLint framebufferBefore = 0;
    GLint programBefore = 0;
    ReadGlIds(framebufferBefore, programBefore);
    const OpenGLTelemetrySnapshot telemetryBefore = GetOpenGLTelemetrySnapshot();
    const TerminalClearSuppressionSnapshot terminalClearBefore =
        GetTerminalClearSuppressionSnapshot();

    bool captureStarted = false;
    bool captureCompleted = false;
    float terminalVirtualWidth = 1024.0f;
    float terminalVirtualHeight = 577.0f;
    if (isTerminalOverlaySet) {
        ReadField(guiSet, 0x100, terminalVirtualWidth);
        ReadField(guiSet, 0x104, terminalVirtualHeight);
    }
    const bool isDirectCapturedHudSet = isGameHud || isGameHudImGuiSet
        || isPausedCurrentImGuiSet || isDeadCurrentImGuiSet
        || isWakeCurrentImGuiSet || isInventoryCurrentImGuiSet
        || isTerminalOverlaySet;
    const bool isCapturedHudSet = isDirectCapturedHudSet;
    const bool terminalRetentionWasActive = g_terminalRetentionActive.load(
        std::memory_order_relaxed);
    const bool terminalRetentionTransition =
        (isTerminalOverlaySet && terminalStateActive && !terminalRetentionWasActive)
        || (isDirectCapturedHudSet && playerStateValid
            && !terminalStateActive && terminalRetentionWasActive);
    if (terminalRetentionTransition) {
        const bool newRetentionState = terminalStateActive;
        bool freshTerminalSession = false;
        if (newRetentionState) {
            const bool existingSession = g_terminalRetentionSessionInitialized.exchange(
                true, std::memory_order_relaxed);
            freshTerminalSession = !existingSession;
            if (!existingSession) {
                g_terminalRetentionAutoFallbackActive.store(false, std::memory_order_relaxed);
                g_terminalRetentionColorClearObserved.store(false, std::memory_order_relaxed);
                g_terminalRetentionMissingClearSamples.store(0, std::memory_order_relaxed);
                g_terminalRetentionScissorBypassBaseline.store(
                    GetTerminalOffscreenScissorBypassCount(),
                    std::memory_order_relaxed);
                g_terminalRetentionScissorRepairObserved.store(
                    false, std::memory_order_relaxed);
            }
        } else {
            g_terminalRetentionSessionInitialized.store(false, std::memory_order_relaxed);
            g_terminalRetentionAutoFallbackActive.store(false, std::memory_order_relaxed);
            g_terminalRetentionColorClearObserved.store(false, std::memory_order_relaxed);
            g_terminalRetentionMissingClearSamples.store(0, std::memory_order_relaxed);
            g_terminalRetentionScissorRepairObserved.store(false, std::memory_order_relaxed);
        }
        g_terminalRetentionActive.store(newRetentionState, std::memory_order_relaxed);
        const uint64_t transition = g_terminalRetentionTransitions.fetch_add(
            1, std::memory_order_relaxed) + 1;
        Logger::Instance().Write(
            LogLevel::Info,
            "hpl_terminal_retention transition=%llu frame=%llu active=%d previous=%d playerState=%d policy=retain_dirty_rect_surface",
            static_cast<unsigned long long>(transition),
            static_cast<unsigned long long>(frame),
            newRetentionState ? 1 : 0,
            terminalRetentionWasActive ? 1 : 0,
            player.playerStateId);
        if (newRetentionState && freshTerminalSession) {
            const uint64_t sequence = g_terminalDumpSequence.fetch_add(
                1, std::memory_order_relaxed) + 1;
            const uint64_t session = g_terminalAutoProbeSessions.fetch_add(
                1, std::memory_order_relaxed) + 1;
            BeginTerminalDrawStateProbe(
                sequence,
                GetOpenGLRenderFrameHint() + 1,
                4);
            Logger::Instance().Write(
                LogLevel::Warn,
                "terminal_draw_state auto_armed session=%llu sequence=%llu frame=%llu samples=4 outputs=log_only reason=fresh_terminal_session",
                static_cast<unsigned long long>(session),
                static_cast<unsigned long long>(sequence),
                static_cast<unsigned long long>(frame));
        }
    }
    const bool terminalRetentionPolicyEffective =
        g_config.hplControllerTerminalPreserveDirtyRects
        && !g_terminalRetentionAutoFallbackActive.load(std::memory_order_relaxed);
    const bool preservePreviousTerminalSurface = isTerminalOverlaySet
        && terminalStateActive
        && terminalRetentionWasActive
        && terminalRetentionPolicyEffective;
    if (isDirectCapturedHudSet && g_config.openxrHudLayer && g_openxr != nullptr) {
        g_captureAttempts.fetch_add(1, std::memory_order_relaxed);
        captureStarted = isTerminalOverlaySet
            ? g_openxr->BeginTerminalHudCapture(
                frame,
                static_cast<int>(std::lround(terminalVirtualWidth)),
                static_cast<int>(std::lround(terminalVirtualHeight)),
                preservePreviousTerminalSurface,
                terminalRetentionPolicyEffective)
            : g_openxr->BeginHudCapture(frame);
        if (captureStarted) {
            g_captureStarts.fetch_add(1, std::memory_order_relaxed);
        } else {
            g_captureFallbacks.fetch_add(1, std::memory_order_relaxed);
        }
    }

    g_originalGuiSetRender(guiSet, renderTarget);
    if (captureStarted) {
        GLint terminalFramebuffer = 0;
        GLint terminalProgram = 0;
        GLint terminalViewport[4] = {};
        if (isTerminalOverlaySet) {
            ReadGlIds(terminalFramebuffer, terminalProgram);
            ReadGlViewport(terminalViewport);
        }
        captureCompleted = isTerminalOverlaySet
            ? g_openxr->EndTerminalHudCapture(frame)
            : g_openxr->EndHudCapture(frame, isGameHud);
        if (captureCompleted) {
            g_captureCompletions.fetch_add(1, std::memory_order_relaxed);
            if (isGameHudImGuiSet) {
                g_gameHudImGuiCaptureCompletions.fetch_add(1, std::memory_order_relaxed);
            }
            if (isPausedCurrentImGuiSet) {
                g_pausedMenuCaptureCompletions.fetch_add(1, std::memory_order_relaxed);
            }
            if (isDeadCurrentImGuiSet) {
                g_deadCurrentImGuiCaptureCompletions.fetch_add(1, std::memory_order_relaxed);
            }
            if (isWakeCurrentImGuiSet) {
                g_wakeCurrentImGuiCaptureCompletions.fetch_add(1, std::memory_order_relaxed);
            }
            if (isInventoryCurrentImGuiSet) {
                g_inventoryCurrentImGuiCaptureCompletions.fetch_add(1, std::memory_order_relaxed);
            }
            if (isTerminalOverlaySet) {
                g_terminalOverlayCaptureCompletions.fetch_add(1, std::memory_order_relaxed);
            }
        } else {
            g_captureFallbacks.fetch_add(1, std::memory_order_relaxed);
        }
        if (isTerminalOverlaySet) {
            const TerminalClearSuppressionSnapshot terminalClearAfter =
                GetTerminalClearSuppressionSnapshot();
            const uint64_t suppressedColorClears =
                terminalClearAfter.suppressedColorClears
                - terminalClearBefore.suppressedColorClears;
            const uint64_t forwardedDepthStencilClears =
                terminalClearAfter.forwardedDepthStencilClears
                - terminalClearBefore.forwardedDepthStencilClears;
            const uint64_t framebufferMismatches =
                terminalClearAfter.framebufferMismatches
                - terminalClearBefore.framebufferMismatches;
            const uint64_t threadMismatches =
                terminalClearAfter.threadMismatches
                - terminalClearBefore.threadMismatches;
            const uint64_t captureSample = g_terminalOverlayCaptureSamples.fetch_add(
                1, std::memory_order_relaxed) + 1;
            const bool scissorRepairObserved =
                GetTerminalOffscreenScissorBypassCount()
                    > g_terminalRetentionScissorBypassBaseline.load(std::memory_order_relaxed);
            if (scissorRepairObserved) {
                const bool previouslyObserved =
                    g_terminalRetentionScissorRepairObserved.exchange(
                        true, std::memory_order_relaxed);
                if (!previouslyObserved) {
                    Logger::Instance().Write(
                        LogLevel::Info,
                        "hpl_terminal_retention_stabilized frame=%llu action=keep_retained_surface reason=offscreen_scissor_repair_observed policy=preserve_partial_gui_updates",
                        static_cast<unsigned long long>(frame));
                }
            }
            if (g_config.hplControllerTerminalPreserveDirtyRects
                && captureCompleted
                && preservePreviousTerminalSurface
                && !g_terminalRetentionAutoFallbackActive.load(std::memory_order_relaxed)) {
                if (suppressedColorClears > 0) {
                    g_terminalRetentionColorClearObserved.store(
                        true, std::memory_order_relaxed);
                } else if (!g_terminalRetentionColorClearObserved.load(
                        std::memory_order_relaxed)
                    && !g_terminalRetentionScissorRepairObserved.load(
                        std::memory_order_relaxed)) {
                    const uint32_t missingSamples =
                        g_terminalRetentionMissingClearSamples.fetch_add(
                            1, std::memory_order_relaxed) + 1;
                    if (terminal_math::ShouldFallbackToLiveTerminalFrames(
                            missingSamples,
                            kTerminalMissingClearFallbackSamples,
                            false,
                            false)) {
                        bool expected = false;
                        if (g_terminalRetentionAutoFallbackActive.compare_exchange_strong(
                                expected, true, std::memory_order_relaxed)) {
                            const uint64_t fallback =
                                g_terminalRetentionAutoFallbacks.fetch_add(
                                    1, std::memory_order_relaxed) + 1;
                            Logger::Instance().Write(
                                LogLevel::Warn,
                                "hpl_terminal_retention_fallback fallback=%llu frame=%llu samplesWithoutClear=%u action=live_frame_capture reason=no_nested_color_clear policy=avoid_permanent_black_surface",
                                static_cast<unsigned long long>(fallback),
                                static_cast<unsigned long long>(frame),
                                missingSamples);
                        }
                    }
                }
            }
            const uint64_t logInterval = static_cast<uint64_t>(
                std::max(g_config.hplControllerLogInterval, 1));
            if (captureSample <= 8 || captureSample % logInterval == 0) {
                Logger::Instance().Write(
                    LogLevel::Info,
                    "hpl_terminal_capture sample=%llu frame=%llu fbo=%d program=%d viewport=%d,%d,%d,%d virtualSize=%.1f,%.1f completed=%d retained=%d retentionActive=%d preserveDirtyRects=%d effectivePreserve=%d autoFallback=%d missingClearSamples=%u scissorRepairObserved=%d clearSuppression={color=%llu depthStencil=%llu fboMismatch=%llu threadMismatch=%llu} policy=native_size_retained_surface_with_scissor_repair_guarded_fallback",
                    static_cast<unsigned long long>(captureSample),
                    static_cast<unsigned long long>(frame),
                    terminalFramebuffer,
                    terminalProgram,
                    terminalViewport[0],
                    terminalViewport[1],
                    terminalViewport[2],
                    terminalViewport[3],
                    terminalVirtualWidth,
                    terminalVirtualHeight,
                    captureCompleted ? 1 : 0,
                    preservePreviousTerminalSurface ? 1 : 0,
                    g_terminalRetentionActive.load(std::memory_order_relaxed) ? 1 : 0,
                    g_config.hplControllerTerminalPreserveDirtyRects ? 1 : 0,
                    terminalRetentionPolicyEffective ? 1 : 0,
                    g_terminalRetentionAutoFallbackActive.load(std::memory_order_relaxed) ? 1 : 0,
                    g_terminalRetentionMissingClearSamples.load(std::memory_order_relaxed),
                    g_terminalRetentionScissorRepairObserved.load(std::memory_order_relaxed) ? 1 : 0,
                    static_cast<unsigned long long>(suppressedColorClears),
                    static_cast<unsigned long long>(forwardedDepthStencilClears),
                    static_cast<unsigned long long>(framebufferMismatches),
                    static_cast<unsigned long long>(threadMismatches));
            }

            const bool dumpKeyDown = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0
                && (GetAsyncKeyState(VK_F10) & 0x8000) != 0;
            const bool dumpKeyWasDown = g_terminalDumpF10Down.exchange(
                dumpKeyDown, std::memory_order_relaxed);
            if (dumpKeyDown && !dumpKeyWasDown) {
                const uint64_t sequence = g_terminalDumpSequence.fetch_add(
                    1, std::memory_order_relaxed) + 1;
                g_terminalDumpFramesRemaining.store(4, std::memory_order_relaxed);
                BeginTerminalDrawStateProbe(
                    sequence,
                    GetOpenGLRenderFrameHint() + 1,
                    4);
                Logger::Instance().Write(
                    LogLevel::Warn,
                    "terminal_hud_dump armed key=Ctrl+F10 sequence=%llu frame=%llu samples=4 targets=retained_native_upscaled_hud_and_draw_state outputs=rgb_alpha_bmp_and_log",
                    static_cast<unsigned long long>(sequence),
                    static_cast<unsigned long long>(frame));
            }

            const uint32_t remaining = g_terminalDumpFramesRemaining.load(
                std::memory_order_relaxed);
            if (captureCompleted && remaining > 0) {
                const uint64_t sequence = g_terminalDumpSequence.load(std::memory_order_relaxed);
                const uint32_t sampleIndex = 5u - remaining;
                const bool hudDumped = g_openxr->DumpHudCapture(
                    frame, sequence, sampleIndex, "terminal_ctrl_f10");
                const bool nativeDumped = g_openxr->DumpTerminalHudCapture(
                    frame, sequence, sampleIndex, "terminal_ctrl_f10");
                if (hudDumped && nativeDumped) {
                    g_terminalDumpSamples.fetch_add(1, std::memory_order_relaxed);
                } else {
                    g_terminalDumpFailures.fetch_add(1, std::memory_order_relaxed);
                    Logger::Instance().Write(
                        LogLevel::Warn,
                        "terminal_hud_dump incomplete sequence=%llu frame=%llu sample=%u hud=%d native=%d",
                        static_cast<unsigned long long>(sequence),
                        static_cast<unsigned long long>(frame),
                        sampleIndex,
                        hudDumped ? 1 : 0,
                        nativeDumped ? 1 : 0);
                }
                g_terminalDumpFramesRemaining.store(remaining - 1, std::memory_order_relaxed);
            }
        }
    }

    const OpenGLTelemetrySnapshot telemetryAfter = GetOpenGLTelemetrySnapshot();
    GLint framebufferAfter = 0;
    GLint programAfter = 0;
    ReadGlIds(framebufferAfter, programAfter);

    bool newlySeen = false;
    bool imGuiIdentityChanged = false;
    bool currentImGuiPlayerStateChanged = false;
    {
        std::lock_guard lock(g_mutex);
        if (std::find(g_seenGuiSets.begin(), g_seenGuiSets.end(), guiSet) == g_seenGuiSets.end()
            && g_seenGuiSets.size() < 256) {
            g_seenGuiSets.push_back(guiSet);
            newlySeen = true;
        }
        if (currentImGui != g_lastCurrentImGui || currentImGuiSet != g_lastCurrentImGuiSet) {
            g_lastCurrentImGui = currentImGui;
            g_lastCurrentImGuiSet = currentImGuiSet;
            imGuiIdentityChanged = true;
        }
        if (playerStateValid && player.playerStateId != g_lastCurrentImGuiPlayerState) {
            g_lastCurrentImGuiPlayerState = player.playerStateId;
            currentImGuiPlayerStateChanged = true;
            g_currentImGuiPlayerStateTransitions.fetch_add(1, std::memory_order_relaxed);
        }
    }
    const uint64_t interval = static_cast<uint64_t>(std::max(g_config.hplCompatibilityLogInterval, 1));
    if (newlySeen || imGuiIdentityChanged || currentImGuiPlayerStateChanged
        || call <= 16 || call % interval == 0
        || (isCapturedHudSet && captureStarted != captureCompleted)) {
        uint8_t depthLayer = 0;
        float virtualWidth = 0.0f;
        float virtualHeight = 0.0f;
        float offsetX = 0.0f;
        float offsetY = 0.0f;
        float depthMin = 0.0f;
        float depthMax = 0.0f;
        int32_t priority = 0;
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
            "hpl_gui_set frame=%llu call=%llu stage=%s set=%p target=%p gameHud=%d gameHudSet=%p imGui={current=%p currentSet=%p currentMatch=%d gameHud=%p gameHudSet=%p gameHudMatch=%d stateChanged=%d} terminal={overlayEnabled=%d worldInput=%p focusedSet=%p match=%d captured=%d} pause={enabled=%d valid=%d paused=%d capturedCurrent=%d} scripted={enabled=%d playerStateValid=%d playerState=%d dead=%d wake=%d wakeAsleep=%d inventoryEnabled=%d inventory=%d read=%d zoom=%d} is3d=%d depthLayer=%d virtualSize=%.1f,%.1f offset=%.1f,%.1f depthRange=%.3f,%.3f priority=%d hudMetrics={virtualCenterSize=%.1f,%.1f virtualSize=%.1f,%.1f virtualStart=%.1f,%.1f,%.1f centerScreenSize=%.1f,%.1f centerScreenStart=%.1f,%.1f,%.1f} hudCapture={enabled=%d started=%d completed=%d} calls={drawElements=%llu drawArrays=%llu framebuffer=%llu program=%llu} gl={fbo=%d->%d program=%d->%d}",
            static_cast<unsigned long long>(frame),
            static_cast<unsigned long long>(call),
            GetHPLRenderStageName(GetActiveHPLRenderStage()),
            guiSet,
            renderTarget,
            isGameHud ? 1 : 0,
            gameHudSet,
            currentImGui,
            currentImGuiSet,
            isCurrentImGuiSet ? 1 : 0,
            gameHudImGui,
            gameHudImGuiSet,
            isGameHudImGuiSet ? 1 : 0,
            currentImGuiPlayerStateChanged ? 1 : 0,
            g_config.hplControllerTerminalOverlay ? 1 : 0,
            worldInputImGui,
            terminalGuiSet,
            isTerminalGuiSet ? 1 : 0,
            isTerminalOverlaySet ? 1 : 0,
            g_config.openxrHudCapturePausedMenu ? 1 : 0,
            pauseStateValid ? 1 : 0,
            paused ? 1 : 0,
            isPausedCurrentImGuiSet ? 1 : 0,
            g_config.hplScriptedPresentationControl ? 1 : 0,
            playerStateValid ? 1 : 0,
            playerStateValid ? player.playerStateId : -1,
            isDeadCurrentImGuiSet ? 1 : 0,
            isWakeCurrentImGuiSet ? 1 : 0,
            IsHPLWakeAsleep() ? 1 : 0,
            g_config.hplInventoryPresentationControl ? 1 : 0,
            isInventoryCurrentImGuiSet ? 1 : 0,
            isReadCurrentImGuiSet ? 1 : 0,
            isZoomCurrentImGuiSet ? 1 : 0,
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

void ResetHPLTerminalHudRetention(const char* reason)
{
    const bool wasActive = g_terminalRetentionActive.exchange(
        false, std::memory_order_relaxed);
    const uint64_t reset = g_terminalRetentionExternalResets.fetch_add(
        1, std::memory_order_relaxed) + 1;
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_terminal_retention_reset reset=%llu previous=%d reason=%s policy=clear_then_reaccumulate",
        static_cast<unsigned long long>(reset),
        wasActive ? 1 : 0,
        reason != nullptr ? reason : "unspecified");
}

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
    const bool imGuiIdentityResolved = ResolveImGuiIdentity(executable);
    if (!imGuiIdentityResolved) {
        Logger::Instance().Write(
            LogLevel::Warn,
            "hpl_hud_bridge imgui_probe_disabled reason=signature currentRva=0x%llx gameHudRva=0x%llx getSetRva=0x%llx",
            static_cast<unsigned long long>(kGetCurrentImGuiRva),
            static_cast<unsigned long long>(kGetGameHudImGuiRva),
            static_cast<unsigned long long>(kImGuiGetSetRva));
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
        "hpl_hud_bridge installed rva=0x%llx identityRva=0x%llx imGuiProbe=%d imGuiRvas=0x%llx,0x%llx,0x%llx layer=%d pausedMenu=%d scriptedPresentation=%d inventoryPresentation=%d terminalOverlay=%d terminalPreserveDirtyRects=%d terminalOwnerOffsets=0x%zx,0x%zx,0x%zx deadState=%d size=%dx%d distance=%.3f widthMeters=%.3f verticalOffset=%.3f maxAgeFrames=%d",
        static_cast<unsigned long long>(kGuiSetRenderRva),
        static_cast<unsigned long long>(kGetGameHudSetRva),
        imGuiIdentityResolved ? 1 : 0,
        static_cast<unsigned long long>(kGetCurrentImGuiRva),
        static_cast<unsigned long long>(kGetGameHudImGuiRva),
        static_cast<unsigned long long>(kImGuiGetSetRva),
        config.openxrHudLayer ? 1 : 0,
        config.openxrHudCapturePausedMenu ? 1 : 0,
        config.hplScriptedPresentationControl ? 1 : 0,
        config.hplInventoryPresentationControl ? 1 : 0,
        config.hplControllerTerminalOverlay ? 1 : 0,
        config.hplControllerTerminalPreserveDirtyRects ? 1 : 0,
        kImGuiManagerWorldInputOffset,
        kImGuiManagerFocusedWrapperOffset,
        kImGuiWrapperSetOffset,
        kDeadPlayerState,
        config.openxrHudWidthPixels,
        config.openxrHudHeightPixels,
        config.openxrHudDistanceMeters,
        config.openxrHudWidthMeters,
        config.openxrHudVerticalOffsetMeters,
        config.openxrHudMaxAgeFrames);
    return true;
}

bool IsHPLCurrentImGuiSurfaceActive(uint64_t frameIndex, uint64_t maximumAgeFrames)
{
    const uint64_t surfaceFrame = g_currentImGuiSurfaceFrame.load(std::memory_order_acquire);
    return surfaceFrame != 0
        && frameIndex >= surfaceFrame
        && frameIndex - surfaceFrame <= maximumAgeFrames;
}

void LogHPLHudBridgeSummary()
{
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_hud_summary calls=%llu gameHudMatches=%llu currentImGuiSetMatches=%llu gameHudImGuiSetMatches=%llu gameHudImGuiCaptures=%llu pauseQueries=%llu pauseQueryFallbacks=%llu pausedCurrentImGuiMatches=%llu pausedMenuCaptures=%llu deadCurrentImGuiMatches=%llu deadCurrentImGuiCaptures=%llu wakeCurrentImGuiMatches=%llu wakeCurrentImGuiCaptures=%llu inventoryCurrentImGuiMatches=%llu inventoryCurrentImGuiCaptures=%llu terminalOverlayMatches=%llu terminalOverlayCaptures=%llu terminalRetentionActive=%d terminalRetentionTransitions=%llu terminalRetentionExternalResets=%llu terminalRetentionAutoFallbackActive=%d terminalRetentionAutoFallbacks=%llu terminalRetentionMissingClearSamples=%u terminalRetentionColorClearObserved=%d terminalAutoProbeSessions=%llu terminalDumpSequences=%llu terminalDumpSamples=%u terminalDumpFailures=%u readCurrentImGuiMatches=%llu zoomCurrentImGuiMatches=%llu currentImGuiPlayerStateTransitions=%llu lastCurrentImGuiPlayerState=%d captureAttempts=%llu captureStarts=%llu captureCompletions=%llu captureFallbacks=%llu installed=%d",
        static_cast<unsigned long long>(g_renderCalls.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_gameHudMatches.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_currentImGuiSetMatches.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_gameHudImGuiSetMatches.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_gameHudImGuiCaptureCompletions.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_pauseQueries.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_pauseQueryFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_pausedCurrentImGuiMatches.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_pausedMenuCaptureCompletions.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_deadCurrentImGuiMatches.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_deadCurrentImGuiCaptureCompletions.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_wakeCurrentImGuiMatches.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_wakeCurrentImGuiCaptureCompletions.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_inventoryCurrentImGuiMatches.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_inventoryCurrentImGuiCaptureCompletions.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_terminalOverlaySetMatches.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_terminalOverlayCaptureCompletions.load(std::memory_order_relaxed)),
        g_terminalRetentionActive.load(std::memory_order_relaxed) ? 1 : 0,
        static_cast<unsigned long long>(g_terminalRetentionTransitions.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_terminalRetentionExternalResets.load(std::memory_order_relaxed)),
        g_terminalRetentionAutoFallbackActive.load(std::memory_order_relaxed) ? 1 : 0,
        static_cast<unsigned long long>(g_terminalRetentionAutoFallbacks.load(std::memory_order_relaxed)),
        g_terminalRetentionMissingClearSamples.load(std::memory_order_relaxed),
        g_terminalRetentionColorClearObserved.load(std::memory_order_relaxed) ? 1 : 0,
        static_cast<unsigned long long>(g_terminalAutoProbeSessions.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_terminalDumpSequence.load(std::memory_order_relaxed)),
        g_terminalDumpSamples.load(std::memory_order_relaxed),
        g_terminalDumpFailures.load(std::memory_order_relaxed),
        static_cast<unsigned long long>(g_readCurrentImGuiMatches.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_zoomCurrentImGuiMatches.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_currentImGuiPlayerStateTransitions.load(std::memory_order_relaxed)),
        g_lastCurrentImGuiPlayerState,
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
    g_getCurrentImGui = nullptr;
    g_getGameHudImGui = nullptr;
    g_imGuiGetSet = nullptr;
    g_openxr = nullptr;
    g_seenGuiSets.clear();
    g_lastCurrentImGui = nullptr;
    g_lastCurrentImGuiSet = nullptr;
    g_lastCurrentImGuiPlayerState = -1;
    g_currentImGuiSurfaceFrame.store(0, std::memory_order_relaxed);
    g_terminalRetentionActive.store(false, std::memory_order_relaxed);
    g_terminalRetentionSessionInitialized.store(false, std::memory_order_relaxed);
    g_terminalRetentionAutoFallbackActive.store(false, std::memory_order_relaxed);
    g_terminalRetentionColorClearObserved.store(false, std::memory_order_relaxed);
    g_terminalRetentionMissingClearSamples.store(0, std::memory_order_relaxed);
    g_terminalRetentionScissorBypassBaseline.store(0, std::memory_order_relaxed);
    g_terminalRetentionScissorRepairObserved.store(false, std::memory_order_relaxed);
    Logger::Instance().Write(LogLevel::Info, "hpl_hud_bridge removed");
}

} // namespace somavr
