#include "HPLArmRenderDiagnostics.h"

#include "HPLHandsBridge.h"
#include "Logger.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace somavr {
namespace {

constexpr uint64_t kInitialCaptureAttempts = 8;
constexpr uint64_t kCaptureInterval = 30;

struct SnapshotDifference {
    uint32_t localNodes = 0;
    uint32_t worldNodes = 0;
    bool rootLocal = false;
    bool rootWorld = false;
    float maxAbsDelta = 0.0f;

    bool Any() const
    {
        return localNodes != 0 || worldNodes != 0 || rootLocal || rootWorld;
    }
};

struct PaletteDifference {
    uint32_t matrices = 0;
    bool validity = false;
    bool count = false;
    float maxAbsDelta = 0.0f;

    bool Any() const
    {
        return matrices != 0 || validity || count;
    }
};

struct PassCapture {
    bool active = false;
    uint64_t frame = 0;
    uint64_t attempt = 0;
    HPLDualRenderPass pass = HPLDualRenderPass::None;
    int eye = -1;
    uint64_t poseFrame = 0;
    HPLArmRenderSnapshot before{};
    HPLArmRenderSnapshot after{};
};

thread_local PassCapture g_activeCapture;
thread_local PassCapture g_firstEyeCapture;
std::atomic<uint64_t> g_captureAttempts = 0;
std::atomic<uint64_t> g_captures = 0;
std::atomic<uint64_t> g_captureMisses = 0;
std::atomic<uint64_t> g_pairComparisons = 0;
std::atomic<uint64_t> g_coherentPairs = 0;
std::atomic<uint64_t> g_inputMismatches = 0;
std::atomic<uint64_t> g_inRenderMutations = 0;
std::atomic<uint64_t> g_interPassMutations = 0;
std::atomic<uint64_t> g_eyePoseMismatches = 0;
std::atomic<uint64_t> g_palettePairComparisons = 0;
std::atomic<uint64_t> g_paletteOutputMismatches = 0;
std::atomic<uint64_t> g_paletteFirstPassUpdates = 0;
std::atomic<uint64_t> g_paletteReplayPassUpdates = 0;
std::atomic<uint64_t> g_paletteInterPassMutations = 0;
std::atomic<uint64_t> g_paletteUnavailable = 0;

bool ShouldCapture(uint64_t attempt)
{
    return attempt != 0
        && (attempt <= kInitialCaptureAttempts || attempt % kCaptureInterval == 0);
}

uint64_t HashBytes(const void* data, size_t size, uint64_t hash = 1469598103934665603ull)
{
    const auto* bytes = static_cast<const uint8_t*>(data);
    for (size_t index = 0; index < size; ++index) {
        hash ^= bytes[index];
        hash *= 1099511628211ull;
    }
    return hash;
}

uint64_t LocalHash(const HPLArmRenderSnapshot& snapshot)
{
    uint64_t hash = HashBytes(snapshot.rootLocal.data(), sizeof(snapshot.rootLocal));
    return HashBytes(snapshot.local.data(), sizeof(snapshot.local), hash);
}

uint64_t WorldHash(const HPLArmRenderSnapshot& snapshot)
{
    uint64_t hash = HashBytes(snapshot.rootWorld.data(), sizeof(snapshot.rootWorld));
    return HashBytes(snapshot.world.data(), sizeof(snapshot.world), hash);
}

void AccumulateMatrixDifference(
    const std::array<float, 16>& left,
    const std::array<float, 16>& right,
    bool& changed,
    float& maxAbsDelta)
{
    changed = std::memcmp(left.data(), right.data(), sizeof(left)) != 0;
    if (!changed) return;
    for (size_t index = 0; index < left.size(); ++index) {
        const float delta = std::fabs(left[index] - right[index]);
        if (std::isfinite(delta)) maxAbsDelta = (std::max)(maxAbsDelta, delta);
    }
}

SnapshotDifference CompareSnapshots(
    const HPLArmRenderSnapshot& left,
    const HPLArmRenderSnapshot& right)
{
    SnapshotDifference difference;
    AccumulateMatrixDifference(
        left.rootLocal, right.rootLocal,
        difference.rootLocal, difference.maxAbsDelta);
    AccumulateMatrixDifference(
        left.rootWorld, right.rootWorld,
        difference.rootWorld, difference.maxAbsDelta);
    for (size_t hand = 0; hand < kHPLArmRenderDiagnosticHandCount; ++hand) {
        for (size_t node = 0; node < kHPLArmRenderDiagnosticNodeCount; ++node) {
            bool localChanged = false;
            bool worldChanged = false;
            AccumulateMatrixDifference(
                left.local[hand][node], right.local[hand][node],
                localChanged, difference.maxAbsDelta);
            AccumulateMatrixDifference(
                left.world[hand][node], right.world[hand][node],
                worldChanged, difference.maxAbsDelta);
            if (localChanged) ++difference.localNodes;
            if (worldChanged) ++difference.worldNodes;
        }
    }
    return difference;
}

PaletteDifference ComparePalettes(
    const HPLArmRenderSnapshot& left,
    const HPLArmRenderSnapshot& right)
{
    PaletteDifference difference;
    difference.validity = left.paletteValid != right.paletteValid;
    difference.count = left.paletteCount != right.paletteCount;
    if (!left.paletteValid || !right.paletteValid) return difference;

    const size_t count = (std::min)(left.paletteCount, right.paletteCount);
    for (size_t index = 0; index < count; ++index) {
        bool changed = false;
        AccumulateMatrixDifference(
            left.palette[index], right.palette[index],
            changed, difference.maxAbsDelta);
        if (changed) ++difference.matrices;
    }
    return difference;
}

const char* PassName(HPLDualRenderPass pass)
{
    switch (pass) {
    case HPLDualRenderPass::FirstEye: return "first_eye";
    case HPLDualRenderPass::ReplayEye: return "replay_eye";
    default: return "none";
    }
}

void LogCaptureMiss(const PassCapture& capture, const char* phase)
{
    const uint64_t miss = g_captureMisses.fetch_add(1, std::memory_order_relaxed) + 1;
    if (miss <= 4 || miss % 60 == 0) {
        Logger::Instance().Write(
            LogLevel::Info,
            "hpl_arm_render_capture miss=%llu frame=%llu attempt=%llu pass=%s phase=%s reason=no_complete_retained_arm_chain policy=diagnostic_only",
            static_cast<unsigned long long>(miss),
            static_cast<unsigned long long>(capture.frame),
            static_cast<unsigned long long>(capture.attempt),
            PassName(capture.pass),
            phase);
    }
}

} // namespace

void BeginHPLArmRenderPassCapture(
    uint64_t frame,
    uint64_t attempt,
    HPLDualRenderPass pass)
{
    g_activeCapture = {};
    if (pass == HPLDualRenderPass::None || !ShouldCapture(attempt)) return;

    g_captureAttempts.fetch_add(1, std::memory_order_relaxed);
    g_activeCapture.active = true;
    g_activeCapture.frame = frame;
    g_activeCapture.attempt = attempt;
    g_activeCapture.pass = pass;
    if (!CaptureHPLArmRenderSnapshot(g_activeCapture.before)) {
        LogCaptureMiss(g_activeCapture, "before");
    }
}

void EndHPLArmRenderPassCapture(int eye, uint64_t poseFrame)
{
    if (!g_activeCapture.active) return;

    g_activeCapture.eye = eye;
    g_activeCapture.poseFrame = poseFrame;
    if (!CaptureHPLArmRenderSnapshot(g_activeCapture.after)) {
        LogCaptureMiss(g_activeCapture, "after");
    }
    if (g_activeCapture.before.valid && g_activeCapture.after.valid) {
        g_captures.fetch_add(1, std::memory_order_relaxed);
        const SnapshotDifference mutation = CompareSnapshots(
            g_activeCapture.before, g_activeCapture.after);
        if (mutation.Any()) {
            g_inRenderMutations.fetch_add(1, std::memory_order_relaxed);
        }
    }

    if (g_activeCapture.pass == HPLDualRenderPass::FirstEye) {
        g_firstEyeCapture = g_activeCapture;
        g_activeCapture = {};
        return;
    }

    if (g_activeCapture.pass == HPLDualRenderPass::ReplayEye
        && g_firstEyeCapture.active
        && g_firstEyeCapture.attempt == g_activeCapture.attempt
        && g_firstEyeCapture.before.valid
        && g_firstEyeCapture.after.valid
        && g_activeCapture.before.valid
        && g_activeCapture.after.valid) {
        g_pairComparisons.fetch_add(1, std::memory_order_relaxed);
        const SnapshotDifference inputDifference = CompareSnapshots(
            g_firstEyeCapture.before, g_activeCapture.before);
        const SnapshotDifference firstMutation = CompareSnapshots(
            g_firstEyeCapture.before, g_firstEyeCapture.after);
        const SnapshotDifference interPassDifference = CompareSnapshots(
            g_firstEyeCapture.after, g_activeCapture.before);
        const SnapshotDifference replayMutation = CompareSnapshots(
            g_activeCapture.before, g_activeCapture.after);
        const PaletteDifference firstPaletteUpdate = ComparePalettes(
            g_firstEyeCapture.before, g_firstEyeCapture.after);
        const PaletteDifference interPassPalette = ComparePalettes(
            g_firstEyeCapture.after, g_activeCapture.before);
        const PaletteDifference replayPaletteUpdate = ComparePalettes(
            g_activeCapture.before, g_activeCapture.after);
        const PaletteDifference outputPaletteDifference = ComparePalettes(
            g_firstEyeCapture.after, g_activeCapture.after);
        const bool sameEntity = g_firstEyeCapture.before.entity
            == g_activeCapture.before.entity;
        const bool eyePoseCoherent = g_firstEyeCapture.eye >= 0
            && g_activeCapture.eye >= 0
            && g_firstEyeCapture.eye != g_activeCapture.eye
            && g_firstEyeCapture.poseFrame == g_activeCapture.poseFrame;
        const bool inputCoherent = sameEntity && !inputDifference.Any();
        if (inputCoherent) {
            g_coherentPairs.fetch_add(1, std::memory_order_relaxed);
        } else {
            g_inputMismatches.fetch_add(1, std::memory_order_relaxed);
        }
        if (interPassDifference.Any()) {
            g_interPassMutations.fetch_add(1, std::memory_order_relaxed);
        }
        if (!eyePoseCoherent) {
            g_eyePoseMismatches.fetch_add(1, std::memory_order_relaxed);
        }
        const bool paletteAvailable = g_firstEyeCapture.before.paletteValid
            && g_firstEyeCapture.after.paletteValid
            && g_activeCapture.before.paletteValid
            && g_activeCapture.after.paletteValid;
        if (paletteAvailable) {
            g_palettePairComparisons.fetch_add(1, std::memory_order_relaxed);
            if (outputPaletteDifference.Any()) {
                g_paletteOutputMismatches.fetch_add(1, std::memory_order_relaxed);
            }
            if (firstPaletteUpdate.Any()) {
                g_paletteFirstPassUpdates.fetch_add(1, std::memory_order_relaxed);
            }
            if (replayPaletteUpdate.Any()) {
                g_paletteReplayPassUpdates.fetch_add(1, std::memory_order_relaxed);
            }
            if (interPassPalette.Any()) {
                g_paletteInterPassMutations.fetch_add(1, std::memory_order_relaxed);
            }
        } else {
            g_paletteUnavailable.fetch_add(1, std::memory_order_relaxed);
        }

        Logger::Instance().Write(
            inputCoherent && eyePoseCoherent ? LogLevel::Info : LogLevel::Warn,
            "hpl_arm_render_pair frame=%llu attempt=%llu firstEye=%d replayEye=%d firstPoseFrame=%llu replayPoseFrame=%llu playerFrames=%llu/%llu entitySame=%d meshSame=%d input={localNodes=%u worldNodes=%u rootLocal=%d rootWorld=%d maxDelta=%.7f} firstRenderMutation={localNodes=%u worldNodes=%u rootLocal=%d rootWorld=%d} interPass={localNodes=%u worldNodes=%u rootLocal=%d rootWorld=%d} replayRenderMutation={localNodes=%u worldNodes=%u rootLocal=%d rootWorld=%d} palette={available=%d count=%zu/%zu firstUpdate=%u interPass=%u replayUpdate=%u outputMismatch=%u maxOutputDelta=%.7f} hashes={firstLocal=0x%llx firstWorld=0x%llx replayLocal=0x%llx replayWorld=0x%llx} result=%s policy=diagnostic_only_no_pose_or_palette_mutation",
            static_cast<unsigned long long>(g_activeCapture.frame),
            static_cast<unsigned long long>(g_activeCapture.attempt),
            g_firstEyeCapture.eye,
            g_activeCapture.eye,
            static_cast<unsigned long long>(g_firstEyeCapture.poseFrame),
            static_cast<unsigned long long>(g_activeCapture.poseFrame),
            static_cast<unsigned long long>(g_firstEyeCapture.before.playerFrame),
            static_cast<unsigned long long>(g_activeCapture.before.playerFrame),
            sameEntity ? 1 : 0,
            g_firstEyeCapture.before.mesh == g_activeCapture.before.mesh ? 1 : 0,
            inputDifference.localNodes,
            inputDifference.worldNodes,
            inputDifference.rootLocal ? 1 : 0,
            inputDifference.rootWorld ? 1 : 0,
            inputDifference.maxAbsDelta,
            firstMutation.localNodes,
            firstMutation.worldNodes,
            firstMutation.rootLocal ? 1 : 0,
            firstMutation.rootWorld ? 1 : 0,
            interPassDifference.localNodes,
            interPassDifference.worldNodes,
            interPassDifference.rootLocal ? 1 : 0,
            interPassDifference.rootWorld ? 1 : 0,
            replayMutation.localNodes,
            replayMutation.worldNodes,
            replayMutation.rootLocal ? 1 : 0,
            replayMutation.rootWorld ? 1 : 0,
            paletteAvailable ? 1 : 0,
            g_firstEyeCapture.after.paletteCount,
            g_activeCapture.after.paletteCount,
            firstPaletteUpdate.matrices,
            interPassPalette.matrices,
            replayPaletteUpdate.matrices,
            outputPaletteDifference.matrices,
            outputPaletteDifference.maxAbsDelta,
            static_cast<unsigned long long>(LocalHash(g_firstEyeCapture.before)),
            static_cast<unsigned long long>(WorldHash(g_firstEyeCapture.before)),
            static_cast<unsigned long long>(LocalHash(g_activeCapture.before)),
            static_cast<unsigned long long>(WorldHash(g_activeCapture.before)),
            inputCoherent && eyePoseCoherent && paletteAvailable
                && !outputPaletteDifference.Any()
                ? "same_pose_same_arm_inputs_same_skin_palette"
                : inputCoherent && eyePoseCoherent && paletteAvailable
                    && outputPaletteDifference.Any()
                    ? "skin_palette_diverged_between_eyes"
                    : inputCoherent && eyePoseCoherent && !paletteAvailable
                        ? "same_pose_same_arm_inputs_palette_unavailable"
                : !eyePoseCoherent
                    ? "eye_or_pose_pair_mismatch"
                    : "arm_inputs_changed_between_eyes");
    }

    g_firstEyeCapture = {};
    g_activeCapture = {};
}

HPLArmRenderDiagnosticsSummary GetHPLArmRenderDiagnosticsSummary()
{
    return {
        g_captureAttempts.load(std::memory_order_relaxed),
        g_captures.load(std::memory_order_relaxed),
        g_captureMisses.load(std::memory_order_relaxed),
        g_pairComparisons.load(std::memory_order_relaxed),
        g_coherentPairs.load(std::memory_order_relaxed),
        g_inputMismatches.load(std::memory_order_relaxed),
        g_inRenderMutations.load(std::memory_order_relaxed),
        g_interPassMutations.load(std::memory_order_relaxed),
        g_eyePoseMismatches.load(std::memory_order_relaxed),
        g_palettePairComparisons.load(std::memory_order_relaxed),
        g_paletteOutputMismatches.load(std::memory_order_relaxed),
        g_paletteFirstPassUpdates.load(std::memory_order_relaxed),
        g_paletteReplayPassUpdates.load(std::memory_order_relaxed),
        g_paletteInterPassMutations.load(std::memory_order_relaxed),
        g_paletteUnavailable.load(std::memory_order_relaxed),
    };
}

void ResetHPLArmRenderDiagnostics()
{
    g_activeCapture = {};
    g_firstEyeCapture = {};
    g_captureAttempts.store(0, std::memory_order_relaxed);
    g_captures.store(0, std::memory_order_relaxed);
    g_captureMisses.store(0, std::memory_order_relaxed);
    g_pairComparisons.store(0, std::memory_order_relaxed);
    g_coherentPairs.store(0, std::memory_order_relaxed);
    g_inputMismatches.store(0, std::memory_order_relaxed);
    g_inRenderMutations.store(0, std::memory_order_relaxed);
    g_interPassMutations.store(0, std::memory_order_relaxed);
    g_eyePoseMismatches.store(0, std::memory_order_relaxed);
    g_palettePairComparisons.store(0, std::memory_order_relaxed);
    g_paletteOutputMismatches.store(0, std::memory_order_relaxed);
    g_paletteFirstPassUpdates.store(0, std::memory_order_relaxed);
    g_paletteReplayPassUpdates.store(0, std::memory_order_relaxed);
    g_paletteInterPassMutations.store(0, std::memory_order_relaxed);
    g_paletteUnavailable.store(0, std::memory_order_relaxed);
}

} // namespace somavr
