#include "HPLStatusPanelBridge.h"

#include "HPLDualRenderControl.h"
#include "HPLPerEyeViewHistory.h"
#include "Logger.h"
#include "OpenXRStatusPanelMath.h"

#include <Windows.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <mutex>

namespace somavr {
namespace {

struct StatusPanelBridgeState {
    bool installed = false;
    bool visible = false;
    bool f1Down = false;
    bool navigationLatched = false;
    bool triggerDown = false;
    bool hudVisible = false;
    bool reticleVisible = false;
    int selectedAction = 0;
    uint64_t lastFrame = 0;
};

std::mutex g_mutex;
Config g_config;
OpenXRRuntime* g_openxr = nullptr;
StatusPanelBridgeState g_state;
std::atomic<uint64_t> g_updates{0};
std::atomic<uint64_t> g_visibleFrames{0};
std::atomic<uint64_t> g_openCount{0};
std::atomic<uint64_t> g_actions{0};

const OpenXRHandInput& DominantHand(const OpenXRInputSnapshot& input, int dominantHand)
{
    return dominantHand == 0 ? input.left : input.right;
}

void Publish(
    uint64_t frameIndex,
    const OpenXRInputSnapshot* input,
    const HPLPlayerStateSnapshot& player,
    const HPLCameraBridgeStatus& camera)
{
    if (g_openxr == nullptr) return;
    OpenXRStatusPanelState panel;
    panel.visible = g_state.visible;
    panel.selectedAction = g_state.selectedAction;
    panel.trackingEnabled = camera.trackingEnabled;
    panel.stereoEnabled = camera.stereoEnabled;
    panel.roomscaleEnabled = camera.roomscaleEnabled;
    panel.projectionCentered = camera.projectionCentered;
    const HPLDualRenderControlStatus dualRender = GetHPLDualRenderControlStatus();
    panel.dualRenderReady = dualRender.ready;
    panel.continuousDualRender = dualRender.enabled;
    const HPLPerEyeViewHistoryStatus viewHistory = GetHPLPerEyeViewHistoryStatus();
    panel.viewHistoryConfigured = viewHistory.configured;
    panel.viewHistoryActive = viewHistory.active;
    panel.viewHistoryFaulted = viewHistory.faulted;
    panel.hudVisible = g_state.hudVisible;
    panel.reticleVisible = g_state.reticleVisible;
    panel.inputAvailable = input != nullptr && input->active;
    panel.controllerTracked = input != nullptr
        && ((input->left.aimPose.valid && input->left.aimPose.orientationTracked)
            || (input->right.aimPose.valid && input->right.aimPose.orientationTracked));
    panel.authoredCameraActive = player.authoredCameraActive;
    panel.playerState = player.playerStateId;
    panel.gameFrame = frameIndex;
    g_openxr->SetStatusPanel(panel);
}

void SetVisible(bool visible, const char* source)
{
    if (visible == g_state.visible) return;
    g_state.visible = visible;
    g_state.navigationLatched = false;
    g_state.triggerDown = false;
    if (visible) g_openCount.fetch_add(1, std::memory_order_relaxed);
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_status_panel visible=%d source=%s selected=%d inputPolicy=%s",
        visible ? 1 : 0,
        source,
        g_state.selectedAction,
        visible ? "exclusive" : "gameplay");
}

void ActivateSelected(HPLCameraBridgeStatus& camera)
{
    switch (g_state.selectedAction) {
    case 0:
        RequestHPLRecenter("vr_status_panel");
        break;
    case 1:
        SetHPLRoomscaleEnabled(!camera.roomscaleEnabled, "vr_status_panel");
        camera = GetHPLCameraBridgeStatus();
        break;
    case 2:
        SetHPLProjectionCentered(!camera.projectionCentered, "vr_status_panel");
        camera = GetHPLCameraBridgeStatus();
        break;
    case 3:
    {
        const HPLDualRenderControlStatus dualRender = GetHPLDualRenderControlStatus();
        if (dualRender.ready) {
            SetHPLContinuousDualRenderEnabled(!dualRender.enabled, "vr_status_panel");
        }
        break;
    }
    case 4:
        g_state.hudVisible = !g_state.hudVisible;
        if (g_openxr != nullptr) g_openxr->SetHudRuntimeVisible(g_state.hudVisible);
        break;
    case 5:
        if (g_openxr != nullptr) g_openxr->ToggleHudLayerShape();
        break;
    case 6:
        g_state.reticleVisible = !g_state.reticleVisible;
        if (g_openxr != nullptr) {
            g_openxr->SetInteractionReticleRuntimeVisible(g_state.reticleVisible);
        }
        break;
    case 7:
        SetVisible(false, "close_action");
        break;
    default:
        break;
    }
    g_actions.fetch_add(1, std::memory_order_relaxed);
    const HPLDualRenderControlStatus dualRender = GetHPLDualRenderControlStatus();
    const OpenXRHudLayerShapeStatus hudShape = g_openxr != nullptr
        ? g_openxr->GetHudLayerShapeStatus() : OpenXRHudLayerShapeStatus{};
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_status_panel action=%d roomscale=%d centered=%d continuousDualRender=%d dualRenderReady=%d hud=%d hudCylinderAvailable=%d hudCylinderActive=%d reticle=%d visible=%d",
        g_state.selectedAction,
        camera.roomscaleEnabled ? 1 : 0,
        camera.projectionCentered ? 1 : 0,
        dualRender.enabled ? 1 : 0,
        dualRender.ready ? 1 : 0,
        g_state.hudVisible ? 1 : 0,
        hudShape.cylinderAvailable ? 1 : 0,
        hudShape.cylinderActive ? 1 : 0,
        g_state.reticleVisible ? 1 : 0,
        g_state.visible ? 1 : 0);
}

} // namespace

bool InstallHPLStatusPanelBridge(const Config& config, OpenXRRuntime* openxr)
{
    std::lock_guard lock(g_mutex);
    g_config = config;
    g_openxr = openxr;
    g_state = {};
    g_state.installed = config.openxrStatusPanel && openxr != nullptr;
    g_state.hudVisible = config.openxrHudLayer;
    g_state.reticleVisible = config.openxrInteractionReticle;
    g_updates.store(0, std::memory_order_relaxed);
    g_visibleFrames.store(0, std::memory_order_relaxed);
    g_openCount.store(0, std::memory_order_relaxed);
    g_actions.store(0, std::memory_order_relaxed);
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_status_panel install enabled=%d key=F1 controllerChord=menu_plus_secondary actions=%d hudDefault=%d reticleDefault=%d",
        g_state.installed ? 1 : 0,
        status_panel_math::kActionCount,
        g_state.hudVisible ? 1 : 0,
        g_state.reticleVisible ? 1 : 0);
    return true;
}

bool UpdateHPLStatusPanelBridge(
    uint64_t frameIndex,
    const OpenXRInputSnapshot* input,
    int dominantHand,
    const HPLPlayerStateSnapshot& player,
    const HPLCameraBridgeStatus& initialCamera)
{
    std::lock_guard lock(g_mutex);
    if (!g_state.installed || g_state.lastFrame == frameIndex) return g_state.visible;
    g_state.lastFrame = frameIndex;
    g_updates.fetch_add(1, std::memory_order_relaxed);

    HPLCameraBridgeStatus camera = initialCamera;
    const bool f1Down = (GetAsyncKeyState(VK_F1) & 0x8000) != 0;
    const bool f1Pressed = f1Down && !g_state.f1Down;
    g_state.f1Down = f1Down;

    bool consumed = g_state.visible;
    bool openedThisFrame = false;
    if (f1Pressed) {
        SetVisible(!g_state.visible, "F1");
        consumed = true;
        openedThisFrame = g_state.visible;
    }

    const OpenXRHandInput* dominant = input != nullptr ? &DominantHand(*input, dominantHand) : nullptr;
    const bool controllerChord = input != nullptr
        && input->menu && input->menuChanged && dominant != nullptr && dominant->secondary;
    if (controllerChord && !g_state.visible) {
        SetVisible(true, "controller_chord");
        consumed = true;
        openedThisFrame = true;
    }

    if (g_state.visible && input != nullptr) {
        if (!openedThisFrame && input->menu && input->menuChanged) {
            SetVisible(false, "controller_menu");
            consumed = true;
        } else {
            const float navigation = input->moveY;
            if (std::abs(navigation) < 0.35f) {
                g_state.navigationLatched = false;
            } else if (!g_state.navigationLatched) {
                const int direction = navigation > 0.0f ? -1 : 1;
                g_state.selectedAction = (g_state.selectedAction + direction
                    + status_panel_math::kActionCount) % status_panel_math::kActionCount;
                g_state.navigationLatched = true;
            }

            const bool triggerDown = dominant != nullptr && dominant->trigger >= 0.75f;
            const bool activate = dominant != nullptr
                && ((dominant->select && dominant->selectChanged)
                    || (triggerDown && !g_state.triggerDown));
            g_state.triggerDown = triggerDown;
            if (activate) ActivateSelected(camera);
        }
    }

    if (g_state.visible) g_visibleFrames.fetch_add(1, std::memory_order_relaxed);
    Publish(frameIndex, input, player, camera);
    return consumed || g_state.visible;
}

void LogHPLStatusPanelBridgeSummary()
{
    std::lock_guard lock(g_mutex);
    const HPLDualRenderControlStatus dualRender = GetHPLDualRenderControlStatus();
    const OpenXRHudLayerShapeStatus hudShape = g_openxr != nullptr
        ? g_openxr->GetHudLayerShapeStatus() : OpenXRHudLayerShapeStatus{};
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_status_panel_summary installed=%d visible=%d selected=%d updates=%llu visibleFrames=%llu opens=%llu actions=%llu continuousDualRender=%d dualRenderReady=%d hud=%d hudCylinderAvailable=%d hudCylinderActive=%d reticle=%d",
        g_state.installed ? 1 : 0,
        g_state.visible ? 1 : 0,
        g_state.selectedAction,
        static_cast<unsigned long long>(g_updates.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_visibleFrames.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_openCount.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_actions.load(std::memory_order_relaxed)),
        dualRender.enabled ? 1 : 0,
        dualRender.ready ? 1 : 0,
        g_state.hudVisible ? 1 : 0,
        hudShape.cylinderAvailable ? 1 : 0,
        hudShape.cylinderActive ? 1 : 0,
        g_state.reticleVisible ? 1 : 0);
}

void RemoveHPLStatusPanelBridge()
{
    std::lock_guard lock(g_mutex);
    if (g_openxr != nullptr) g_openxr->SetStatusPanel({});
    g_openxr = nullptr;
    g_state = {};
    Logger::Instance().Write(LogLevel::Info, "hpl_status_panel removed");
}

} // namespace somavr
