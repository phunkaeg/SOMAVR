#include "HPLDualRenderControl.h"

#include "Config.h"
#include "Logger.h"

#include <atomic>
#include <mutex>

namespace somavr {
namespace {

std::mutex g_mutex;
bool g_configured = false;
bool g_defaultEnabled = false;
std::atomic<bool> g_ready = false;
std::atomic<bool> g_enabled = false;
std::atomic<uint64_t> g_changes = 0;
std::atomic<uint64_t> g_rejections = 0;

} // namespace

void InitializeHPLDualRenderControl(const Config& config)
{
    std::lock_guard lock(g_mutex);
    g_configured = config.hplDualRenderContinuousControl;
    g_defaultEnabled = config.hplDualRenderContinuousDefault;
    g_ready.store(false, std::memory_order_relaxed);
    g_enabled.store(false, std::memory_order_relaxed);
    g_changes.store(0, std::memory_order_relaxed);
    g_rejections.store(0, std::memory_order_relaxed);
}

void SetHPLDualRenderControlReady(bool ready)
{
    std::lock_guard lock(g_mutex);
    const bool effectiveReady = g_configured && ready;
    g_ready.store(effectiveReady, std::memory_order_relaxed);
    g_enabled.store(effectiveReady && g_defaultEnabled, std::memory_order_relaxed);
    Logger::Instance().Write(
        effectiveReady ? LogLevel::Warn : LogLevel::Info,
        "hpl_dual_render_control ready=%d configured=%d defaultEnabled=%d enabled=%d policy=explicit_opt_in_fail_closed",
        effectiveReady ? 1 : 0,
        g_configured ? 1 : 0,
        g_defaultEnabled ? 1 : 0,
        g_enabled.load(std::memory_order_relaxed) ? 1 : 0);
}

bool SetHPLContinuousDualRenderEnabled(bool enabled, const char* source)
{
    std::lock_guard lock(g_mutex);
    if (!g_configured || !g_ready.load(std::memory_order_relaxed)) {
        const uint64_t rejection = g_rejections.fetch_add(1, std::memory_order_relaxed) + 1;
        Logger::Instance().Write(
            LogLevel::Warn,
            "hpl_dual_render_control rejected=1 requested=%d source=%s configured=%d ready=%d rejections=%llu",
            enabled ? 1 : 0,
            source != nullptr ? source : "unknown",
            g_configured ? 1 : 0,
            g_ready.load(std::memory_order_relaxed) ? 1 : 0,
            static_cast<unsigned long long>(rejection));
        return false;
    }

    const bool previous = g_enabled.exchange(enabled, std::memory_order_relaxed);
    if (previous != enabled) {
        const uint64_t change = g_changes.fetch_add(1, std::memory_order_relaxed) + 1;
        Logger::Instance().Write(
            LogLevel::Warn,
            "hpl_dual_render_control enabled=%d previous=%d source=%s changes=%llu",
            enabled ? 1 : 0,
            previous ? 1 : 0,
            source != nullptr ? source : "unknown",
            static_cast<unsigned long long>(change));
    }
    return true;
}

bool IsHPLContinuousDualRenderEnabled()
{
    return g_ready.load(std::memory_order_relaxed)
        && g_enabled.load(std::memory_order_relaxed);
}

HPLDualRenderControlStatus GetHPLDualRenderControlStatus()
{
    std::lock_guard lock(g_mutex);
    return {
        g_configured,
        g_ready.load(std::memory_order_relaxed),
        g_enabled.load(std::memory_order_relaxed),
        g_defaultEnabled,
        g_changes.load(std::memory_order_relaxed),
        g_rejections.load(std::memory_order_relaxed),
    };
}

void RemoveHPLDualRenderControl()
{
    std::lock_guard lock(g_mutex);
    g_enabled.store(false, std::memory_order_relaxed);
    g_ready.store(false, std::memory_order_relaxed);
    g_configured = false;
    g_defaultEnabled = false;
}

} // namespace somavr
