#include "HPLMenuBridge.h"

#include "HPLMenuMath.h"
#include "Logger.h"

#include <Windows.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <mutex>

namespace somavr {
namespace {

Config g_config;
std::mutex g_mutex;
HWND g_window = nullptr;
bool g_active = false;
bool g_smoothed = false;
float g_smoothedX = 0.5f;
float g_smoothedY = 0.5f;
std::atomic<uint64_t> g_updates = 0;
std::atomic<uint64_t> g_applied = 0;
std::atomic<uint64_t> g_poseFallbacks = 0;
std::atomic<uint64_t> g_windowFallbacks = 0;
std::atomic<uint64_t> g_cursorFailures = 0;

BOOL CALLBACK FindWindowForProcess(HWND window, LPARAM parameter)
{
    DWORD processId = 0;
    GetWindowThreadProcessId(window, &processId);
    if (processId != GetCurrentProcessId()
        || !IsWindowVisible(window)
        || GetWindow(window, GW_OWNER) != nullptr) {
        return TRUE;
    }
    RECT client{};
    if (!GetClientRect(window, &client)
        || client.right <= client.left
        || client.bottom <= client.top) {
        return TRUE;
    }
    *reinterpret_cast<HWND*>(parameter) = window;
    return FALSE;
}

HWND ResolveGameWindow()
{
    if (g_window != nullptr && IsWindow(g_window)) return g_window;
    g_window = nullptr;

    HWND foreground = GetForegroundWindow();
    DWORD processId = 0;
    if (foreground != nullptr) GetWindowThreadProcessId(foreground, &processId);
    if (foreground != nullptr && processId == GetCurrentProcessId()) {
        g_window = foreground;
        return g_window;
    }

    EnumWindows(FindWindowForProcess, reinterpret_cast<LPARAM>(&g_window));
    return g_window;
}

} // namespace

bool InstallHPLMenuBridge(const Config& config)
{
    std::lock_guard lock(g_mutex);
    g_config = config;
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_menu_bridge install_ok enabled=%d horizontalFovDegrees=%.2f verticalFovDegrees=%.2f smoothing=%.3f policy=paused_game_head_relative_aim_to_native_cursor",
        config.hplControllerMenuPointer ? 1 : 0,
        config.hplControllerMenuPointerHorizontalDegrees,
        config.hplControllerMenuPointerVerticalDegrees,
        config.hplControllerMenuPointerSmoothing);
    return true;
}

bool UpdateHPLMenuPointer(
    const OpenXRHeadPose& headPose,
    const OpenXRControllerPose& aimPose)
{
    std::lock_guard lock(g_mutex);
    g_updates.fetch_add(1, std::memory_order_relaxed);
    if (!g_config.hplControllerMenuPointer
        || !headPose.valid
        || !aimPose.valid
        || !aimPose.orientationTracked) {
        g_poseFallbacks.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    menu_math::MenuPointerPosition pointer;
    if (!menu_math::ProjectAimToMenu(
            {
                headPose.orientationX,
                headPose.orientationY,
                headPose.orientationZ,
                headPose.orientationW,
            },
            {
                aimPose.orientationX,
                aimPose.orientationY,
                aimPose.orientationZ,
                aimPose.orientationW,
            },
            g_config.hplControllerMenuPointerHorizontalDegrees,
            g_config.hplControllerMenuPointerVerticalDegrees,
            pointer)) {
        g_poseFallbacks.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    const float blend = std::clamp(g_config.hplControllerMenuPointerSmoothing, 0.0f, 1.0f);
    if (!g_smoothed || !g_active) {
        g_smoothedX = pointer.x;
        g_smoothedY = pointer.y;
        g_smoothed = true;
    } else {
        g_smoothedX += (pointer.x - g_smoothedX) * blend;
        g_smoothedY += (pointer.y - g_smoothedY) * blend;
    }

    HWND window = ResolveGameWindow();
    RECT client{};
    POINT origin{};
    if (window == nullptr
        || !GetClientRect(window, &client)
        || !ClientToScreen(window, &origin)
        || client.right <= client.left
        || client.bottom <= client.top) {
        g_windowFallbacks.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    const int x = origin.x + static_cast<int>(std::lround(g_smoothedX * static_cast<float>(width - 1)));
    const int y = origin.y + static_cast<int>(std::lround(g_smoothedY * static_cast<float>(height - 1)));
    if (!SetCursorPos(x, y)) {
        g_cursorFailures.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    g_active = true;
    const uint64_t applied = g_applied.fetch_add(1, std::memory_order_relaxed) + 1;
    const uint64_t interval = static_cast<uint64_t>(std::max(g_config.hplControllerLogInterval, 1));
    if (applied <= 8 || applied % interval == 0) {
        Logger::Instance().Write(
            LogLevel::Info,
            "hpl_menu_pointer applied=%llu normalized=%.4f,%.4f screen=%d,%d client=%dx%d window=%p",
            static_cast<unsigned long long>(applied),
            g_smoothedX,
            g_smoothedY,
            x,
            y,
            width,
            height,
            window);
    }
    return true;
}

void DeactivateHPLMenuPointer()
{
    std::lock_guard lock(g_mutex);
    g_active = false;
    g_smoothed = false;
}

void LogHPLMenuBridgeSummary()
{
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_menu_bridge_summary enabled=%d active=%d updates=%llu applied=%llu poseFallbacks=%llu windowFallbacks=%llu cursorFailures=%llu window=%p",
        g_config.hplControllerMenuPointer ? 1 : 0,
        g_active ? 1 : 0,
        static_cast<unsigned long long>(g_updates.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_applied.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_poseFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_windowFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_cursorFailures.load(std::memory_order_relaxed)),
        g_window);
}

void RemoveHPLMenuBridge()
{
    std::lock_guard lock(g_mutex);
    g_window = nullptr;
    g_active = false;
    g_smoothed = false;
    g_config = {};
    Logger::Instance().Write(LogLevel::Info, "hpl_menu_bridge removed");
}

} // namespace somavr
