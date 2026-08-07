#include "HPLAuthoredInteractionBridge.h"

#include "HPLAuthoredInteractionMath.h"
#include "HPLCameraBridge.h"
#include "HPLPlayerState.h"
#include "Logger.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <mutex>

namespace somavr {
namespace {

using authored_interaction_math::MedicineResult;
using authored_interaction_math::MedicineSettings;
using authored_interaction_math::MedicineState;

constexpr char kMedicineEntityName[] = "Tracer_Fluid_HudObject";
constexpr uint64_t kProfileResetFrames = 300;

Config g_config;
OpenXRRuntime* g_openxr = nullptr;
std::mutex g_mutex;
MedicineState g_medicineState;
void* g_medicineEntity = nullptr;
uint64_t g_lastMedicineFrame = 0;
float g_previousLeftSqueeze = 0.0f;
HPLAuthoredInteractionEvent g_pendingEvent;
std::atomic<uint64_t> g_observations = 0;
std::atomic<uint64_t> g_profileFrames = 0;
std::atomic<uint64_t> g_capEvents = 0;
std::atomic<uint64_t> g_drinkEvents = 0;
std::atomic<uint64_t> g_poseFallbacks = 0;
std::atomic<uint64_t> g_stateFallbacks = 0;

void QueueEvent(HPLAuthoredInteractionEventType type, uint64_t frame, void* entity)
{
    g_pendingEvent = {type, frame, entity};
}

} // namespace

void ConfigureHPLAuthoredInteractionBridge(const Config& config, OpenXRRuntime* openxr)
{
    std::lock_guard lock(g_mutex);
    g_config = config;
    g_openxr = openxr;
    g_medicineState = {};
    g_medicineEntity = nullptr;
    g_lastMedicineFrame = 0;
    g_previousLeftSqueeze = 0.0f;
    g_pendingEvent = {};
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_authored_interaction configure enabled=%d medicine=%d capOffset=%.4f,%.4f,%.4f capProximityMeters=%.3f mouthProximityMeters=%.3f tipDegrees=%.1f holdFrames=%d policy=profile_events_no_script_commit",
        config.hplAuthoredInteractions ? 1 : 0,
        config.hplMedicineInteraction ? 1 : 0,
        config.hplMedicineCapOffsetX,
        config.hplMedicineCapOffsetY,
        config.hplMedicineCapOffsetZ,
        config.hplMedicineCapProximityMeters,
        config.hplMedicineMouthProximityMeters,
        config.hplMedicineDrinkTipDegrees,
        config.hplMedicineDrinkHoldFrames);
}

void ObserveHPLAuthoredInteractionEntity(
    const std::string& name,
    void* entity,
    const float* worldMatrix,
    uint64_t frame)
{
    if (!g_config.hplAuthoredInteractions || !g_config.hplMedicineInteraction
        || name != kMedicineEntityName || entity == nullptr || worldMatrix == nullptr) {
        return;
    }
    g_observations.fetch_add(1, std::memory_order_relaxed);

    std::array<float, 16> matrix{};
    std::memcpy(matrix.data(), worldMatrix, sizeof(matrix));
    HPLPlayerStateSnapshot player;
    const HPLCameraBridgeStatus camera = GetHPLCameraBridgeStatus();
    if (!GetHPLPlayerStateSnapshot(player) || !player.playerValid
        || player.playerStateId != static_cast<int>(HPLPlayerStateKind::Normal)
        || player.moveStateId != 0 || player.authoredCameraActive) {
        g_stateFallbacks.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    OpenXRInputSnapshot input;
    HPLTrackedPoseWorld leftGrip;
    if (g_openxr == nullptr || !camera.trackingEnabled
        || !g_openxr->GetLatestInput(input) || !input.active
        || !input.left.active || !input.left.gripPose.valid
        || !ResolveHPLTrackedPoseWorld(input.left.gripPose, input.gameFrame, leftGrip)
        || !leftGrip.positionTracked || !camera.headWorldPositionValid) {
        g_poseFallbacks.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    std::lock_guard lock(g_mutex);
    if (g_medicineEntity != entity
        || (g_lastMedicineFrame != 0 && frame > g_lastMedicineFrame + kProfileResetFrames)) {
        g_medicineState = {};
        g_previousLeftSqueeze = input.left.squeeze;
        g_medicineEntity = entity;
        Logger::Instance().Write(
            LogLevel::Info,
            "hpl_authored_interaction profile=medicine event=profile_started frame=%llu entity=%p",
            static_cast<unsigned long long>(frame), entity);
    }
    g_lastMedicineFrame = frame;

    const bool squeezePressed = input.left.squeeze >= 0.70f && g_previousLeftSqueeze < 0.70f;
    g_previousLeftSqueeze = input.left.squeeze;
    authored_interaction_math::MedicineFrame medicineFrame;
    medicineFrame.bottleValid = true;
    medicineFrame.bottleWorld = matrix;
    medicineFrame.leftHandValid = true;
    medicineFrame.leftHandPosition = {
        leftGrip.positionX, leftGrip.positionY, leftGrip.positionZ};
    medicineFrame.leftActionPressed =
        (input.left.selectChanged && input.left.select) || squeezePressed;
    medicineFrame.headValid = true;
    medicineFrame.headPosition = {
        camera.headWorldPositionX,
        camera.headWorldPositionY - 0.06f * camera.worldUnitsPerMeter,
        camera.headWorldPositionZ,
    };

    const float unitsPerMeter = std::max(camera.worldUnitsPerMeter, 0.001f);
    MedicineSettings settings;
    settings.capLocalOffset = {
        g_config.hplMedicineCapOffsetX,
        g_config.hplMedicineCapOffsetY,
        g_config.hplMedicineCapOffsetZ,
    };
    settings.capProximity = g_config.hplMedicineCapProximityMeters * unitsPerMeter;
    settings.mouthProximity = g_config.hplMedicineMouthProximityMeters * unitsPerMeter;
    settings.drinkTipDegrees = g_config.hplMedicineDrinkTipDegrees;
    settings.drinkHoldFrames = static_cast<uint32_t>(
        std::max(g_config.hplMedicineDrinkHoldFrames, 1));

    MedicineResult result;
    if (!authored_interaction_math::UpdateMedicine(
            g_medicineState, settings, medicineFrame, result)) {
        g_poseFallbacks.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    const uint64_t profileFrame = g_profileFrames.fetch_add(1, std::memory_order_relaxed) + 1;
    if (result.capRemoved) {
        const uint64_t event = g_capEvents.fetch_add(1, std::memory_order_relaxed) + 1;
        QueueEvent(HPLAuthoredInteractionEventType::MedicineCapRemove, frame, entity);
        g_openxr->RequestHapticPulse(0, 0.45f, 55, "medicine_cap_proximity");
        Logger::Instance().Write(
            LogLevel::Info,
            "hpl_authored_interaction profile=medicine event=cap_remove_requested count=%llu frame=%llu entity=%p capDistance=%.4f threshold=%.4f actionEdge=1 nativeCommit=0",
            static_cast<unsigned long long>(event),
            static_cast<unsigned long long>(frame), entity,
            result.capDistance, settings.capProximity);
    }
    if (result.drinkCompleted) {
        const uint64_t event = g_drinkEvents.fetch_add(1, std::memory_order_relaxed) + 1;
        QueueEvent(HPLAuthoredInteractionEventType::MedicineDrink, frame, entity);
        g_openxr->RequestHapticPulse(1, 0.35f, 90, "medicine_drink_pose");
        Logger::Instance().Write(
            LogLevel::Info,
            "hpl_authored_interaction profile=medicine event=drink_requested count=%llu frame=%llu entity=%p mouthDistance=%.4f threshold=%.4f tipDegrees=%.2f holdFrames=%u nativeCommit=0",
            static_cast<unsigned long long>(event),
            static_cast<unsigned long long>(frame), entity,
            result.mouthDistance, settings.mouthProximity,
            result.tipDegrees, settings.drinkHoldFrames);
    }
    const uint64_t interval = static_cast<uint64_t>(
        std::max(g_config.hplControllerLogInterval, 1));
    if (profileFrame <= 12 || profileFrame % interval == 0) {
        Logger::Instance().Write(
            LogLevel::Info,
            "hpl_authored_interaction profile=medicine frameSample=%llu frame=%llu entity=%p stage=%s capDistance=%.4f capInRange=%d mouthDistance=%.4f mouthInRange=%d tipDegrees=%.2f tipped=%d leftTrigger=%d leftSqueeze=%.3f",
            static_cast<unsigned long long>(profileFrame),
            static_cast<unsigned long long>(frame), entity,
            authored_interaction_math::MedicineStageName(result.stage),
            result.capDistance, result.capInRange ? 1 : 0,
            result.mouthDistance, result.mouthInRange ? 1 : 0,
            result.tipDegrees, result.tipped ? 1 : 0,
            input.left.select ? 1 : 0, input.left.squeeze);
    }
}

void InvalidateHPLAuthoredInteractionEntity(void* entity)
{
    std::lock_guard lock(g_mutex);
    if (g_medicineEntity == entity) {
        g_medicineState = {};
        g_medicineEntity = nullptr;
        g_lastMedicineFrame = 0;
        g_previousLeftSqueeze = 0.0f;
        if (g_pendingEvent.entity == entity) g_pendingEvent = {};
    }
}

bool ConsumeHPLAuthoredInteractionEvent(HPLAuthoredInteractionEvent& event)
{
    std::lock_guard lock(g_mutex);
    event = g_pendingEvent;
    g_pendingEvent = {};
    return event.type != HPLAuthoredInteractionEventType::None;
}

void LogHPLAuthoredInteractionSummary()
{
    Logger::Instance().Write(
        LogLevel::Info,
        "hpl_authored_interaction_summary enabled=%d medicine=%d observations=%llu profileFrames=%llu capEvents=%llu drinkEvents=%llu stateFallbacks=%llu poseFallbacks=%llu nativeCommitPolicy=diagnostic_events_only",
        g_config.hplAuthoredInteractions ? 1 : 0,
        g_config.hplMedicineInteraction ? 1 : 0,
        static_cast<unsigned long long>(g_observations.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_profileFrames.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_capEvents.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_drinkEvents.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_stateFallbacks.load(std::memory_order_relaxed)),
        static_cast<unsigned long long>(g_poseFallbacks.load(std::memory_order_relaxed)));
}

void ResetHPLAuthoredInteractionBridge()
{
    std::lock_guard lock(g_mutex);
    g_openxr = nullptr;
    g_medicineState = {};
    g_medicineEntity = nullptr;
    g_lastMedicineFrame = 0;
    g_previousLeftSqueeze = 0.0f;
    g_pendingEvent = {};
}

} // namespace somavr
