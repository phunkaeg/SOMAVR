#include "HPLCameraMath.h"
#include "HPLComfortMath.h"
#include "HPLFlashlightMath.h"
#include "HPLHandsMath.h"
#include "HPLGrabMath.h"
#include "HPLGameplayHapticsMath.h"
#include "HPLHudMath.h"
#include "HPLInputMath.h"
#include "HPLMenuMath.h"
#include "HPLPhysicalCrouchMath.h"
#include "HPLRoomscaleReconciliationMath.h"
#include "HPLScreenEffectMath.h"
#include "HPLSubtitleMath.h"
#include "HPLDualRenderMath.h"
#include "HPLPerEyeViewHistoryMath.h"
#include "HPLPerEyePostEffectMath.h"
#include "HPLToneMappingFrameMath.h"
#include "HPLSSAOTemporalMath.h"
#include "HPLTemporalMutationMath.h"
#include "HPLTwoHandMath.h"
#include "HPLPostEffectResourceMath.h"
#include "OpenGLMatrixAnalysis.h"
#include "OpenXRSpectatorMath.h"
#include "OpenXRDepthMath.h"
#include "OpenXRComfortVignetteMath.h"
#include "OpenXRStatusPanelMath.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace {

bool Near(float left, float right, float epsilon = 1.0e-5f)
{
    return std::abs(left - right) <= epsilon;
}

int Check(bool condition, const char* message)
{
    if (condition) {
        return 0;
    }
    std::cerr << "FAILED: " << message << '\n';
    return 1;
}

} // namespace

int main()
{
    using namespace somavr;

    int failures = 0;
    tone_mapping_frame_math::State toneState;
    auto toneRole = tone_mapping_frame_math::Begin(
        toneState, 0x6000, true, 1, 200, 9);
    failures += Check(
        toneRole == tone_mapping_frame_math::PassRole::FirstEye
            && tone_mapping_frame_math::Commit(toneState, toneRole),
        "tone mapping assigns the first eye one frame update");
    toneRole = tone_mapping_frame_math::Begin(
        toneState, 0x6000, true, 0, 200, 9);
    failures += Check(
        toneRole == tone_mapping_frame_math::PassRole::ReplayEye
            && tone_mapping_frame_math::Commit(toneState, toneRole),
        "tone mapping replays the baseline for the opposite eye");
    failures += Check(
        tone_mapping_frame_math::Begin(toneState, 0x6000, true, 0, 200, 9)
            == tone_mapping_frame_math::PassRole::None,
        "tone mapping rejects a duplicate replay in one pose frame");
    toneRole = tone_mapping_frame_math::Begin(
        toneState, 0x6000, true, 0, 201, 9);
    failures += Check(
        toneRole == tone_mapping_frame_math::PassRole::FirstEye,
        "tone mapping starts one update for the next pose frame regardless of eye order");
    failures += Check(
        tone_mapping_frame_math::Begin(toneState, 0x6000, false, 0, 201, 9)
                == tone_mapping_frame_math::PassRole::None
            && toneState.effect == 0,
        "tone mapping releases frame ownership outside eligible stereo");
    ssao_temporal_math::State ssaoState;
    auto ssaoBegin = ssao_temporal_math::Begin(ssaoState, true, 0, 300, 4, false);
    failures += Check(
        ssaoBegin.action == ssao_temporal_math::BeginAction::Observe
            && ssaoBegin.reset
            && ssao_temporal_math::Commit(ssaoState, 0, 300),
        "SSAO history observes and seeds an unallocated eye");
    ssao_temporal_math::SeedBoth(ssaoState, 300);
    ssaoBegin = ssao_temporal_math::Begin(ssaoState, true, 1, 300, 4, true);
    failures += Check(
        ssaoBegin.action == ssao_temporal_math::BeginAction::Restore
            && !ssaoBegin.reset
            && ssao_temporal_math::Commit(ssaoState, 1, 300),
        "SSAO history restores the opposite eye bank for the same pose");
    ssaoBegin = ssao_temporal_math::Begin(ssaoState, true, 0, 301, 5, true);
    failures += Check(
        ssaoBegin.action == ssao_temporal_math::BeginAction::Observe
            && ssaoBegin.reset
            && !ssaoState.seeded[0] && !ssaoState.seeded[1],
        "SSAO history invalidates both banks after calibration changes");
    ssao_temporal_math::SeedBoth(ssaoState, 301);
    ssaoBegin = ssao_temporal_math::Begin(ssaoState, false, 0, 301, 5, true);
    failures += Check(
        ssaoBegin.action == ssao_temporal_math::BeginAction::None
            && ssaoBegin.reset
            && !ssaoState.seeded[0] && !ssaoState.seeded[1],
        "SSAO history invalidates stale banks while native mono rendering owns updates");
    per_eye_post_effect_math::Bank imageTrailBank;
    const per_eye_post_effect_math::ResourcePair imageTrailLeft{0x1000, 0x2000};
    const per_eye_post_effect_math::ResourcePair imageTrailRight{0x3000, 0x4000};
    failures += Check(
        per_eye_post_effect_math::Initialize(
            imageTrailBank, 0x5000, imageTrailLeft, false, imageTrailRight, 7),
        "per-eye image trail accepts two distinct resource pairs");
    auto imageTrailPrepare = per_eye_post_effect_math::Prepare(
        imageTrailBank, 0x5000, 0, 100, 7, imageTrailLeft);
    failures += Check(
        imageTrailPrepare.valid && !imageTrailPrepare.reset
            && imageTrailPrepare.resources.framebuffer == imageTrailLeft.framebuffer
            && !imageTrailPrepare.clear,
        "per-eye image trail restores authored left-eye history");
    failures += Check(
        per_eye_post_effect_math::Commit(
            imageTrailBank, 0x5000, 0, 100, imageTrailLeft, false),
        "per-eye image trail commits left-eye clear state");
    imageTrailPrepare = per_eye_post_effect_math::Prepare(
        imageTrailBank, 0x5000, 1, 100, 7, imageTrailLeft);
    failures += Check(
        imageTrailPrepare.valid
            && imageTrailPrepare.resources.framebuffer == imageTrailRight.framebuffer
            && imageTrailPrepare.clear,
        "per-eye image trail selects isolated right-eye history");
    failures += Check(
        per_eye_post_effect_math::Commit(
            imageTrailBank, 0x5000, 1, 100, imageTrailRight, false),
        "per-eye image trail commits right-eye clear state");
    imageTrailPrepare = per_eye_post_effect_math::Prepare(
        imageTrailBank, 0x5000, 0, 101, 8, imageTrailRight);
    failures += Check(
        imageTrailPrepare.valid && imageTrailPrepare.reset && imageTrailPrepare.clear,
        "per-eye image trail clears both histories after recalibration");
    failures += Check(
        per_eye_post_effect_math::Commit(
            imageTrailBank, 0x5000, 0, 101, imageTrailLeft, false),
        "per-eye image trail commits after recalibration");
    imageTrailPrepare = per_eye_post_effect_math::Prepare(
        imageTrailBank, 0x5000, 0, 120, 8, imageTrailLeft);
    failures += Check(
        imageTrailPrepare.valid && imageTrailPrepare.reset && imageTrailPrepare.clear,
        "per-eye image trail clears history after a stale pose gap");
    failures += Check(
        !per_eye_post_effect_math::Initialize(
            imageTrailBank, 0x5000, imageTrailLeft, false, imageTrailLeft, 8),
        "per-eye image trail rejects shared resource aliases");
    gameplay_haptics_math::Settings hapticSettings;
    gameplay_haptics_math::State hapticState;
    auto hapticDecision = gameplay_haptics_math::Update(
        hapticState, 0.8f, 0.5f, 1000, hapticSettings);
    failures += Check(
        hapticDecision.action == gameplay_haptics_math::Action::Pulse
            && Near(hapticDecision.amplitude, 0.6f)
            && hapticDecision.durationMs == 100,
        "gameplay haptics start with a bounded authored pulse");
    hapticDecision = gameplay_haptics_math::Update(
        hapticState, 0.8f, 0.4f, 1040, hapticSettings);
    failures += Check(
        hapticDecision.action == gameplay_haptics_math::Action::None,
        "gameplay haptics throttle repeated frame updates");
    hapticDecision = gameplay_haptics_math::Update(
        hapticState, 0.8f, 0.3f, 1080, hapticSettings);
    failures += Check(
        hapticDecision.action == gameplay_haptics_math::Action::Pulse,
        "gameplay haptics refresh sustained effects");
    hapticDecision = gameplay_haptics_math::Update(
        hapticState, 0.0f, 0.0f, 1090, hapticSettings);
    failures += Check(
        hapticDecision.action == gameplay_haptics_math::Action::Stop && !hapticState.active,
        "gameplay haptics stop on the authored falling edge");
    hapticDecision = gameplay_haptics_math::Update(
        hapticState, 1.0f, 1.0e30f, 1100, hapticSettings);
    failures += Check(
        hapticDecision.action == gameplay_haptics_math::Action::Pulse
            && hapticDecision.durationMs == 100,
        "gameplay haptics bound extreme finite authored durations");
    const post_effect_resource_math::TextureResource postTextures[] = {
        {0x84f5u, 17u, 1920, 1080, 1, 0x8058},
        {0x0de1u, 9u, 256, 16, 1, 0x8058},
    };
    const post_effect_resource_math::TextureResource postTexturesReordered[] = {
        postTextures[1], postTextures[0],
    };
    const post_effect_resource_math::FramebufferResource postFramebuffers[] = {
        {0x8d40u, 11u}, {0x8ca9u, 12u},
    };
    const uint64_t postSignature = post_effect_resource_math::HashResourceFootprint(
        postTextures, 2, postFramebuffers, 2);
    failures += Check(
        postSignature == post_effect_resource_math::HashResourceFootprint(
            postTexturesReordered, 2, postFramebuffers, 2),
        "post-effect resource signature is independent of bind order");
    failures += Check(
        post_effect_resource_math::ClassifyEyeResourceOwnership(
            true, postSignature, true, postSignature)
            == post_effect_resource_math::EyeResourceOwnership::Shared,
        "matching eye resource signatures classify as shared");
    failures += Check(
        post_effect_resource_math::ClassifyEyeResourceOwnership(
            true, postSignature, true, postSignature + 1)
            == post_effect_resource_math::EyeResourceOwnership::EyeDistinct,
        "different eye resource signatures classify as eye-distinct");
    failures += Check(
        post_effect_resource_math::ClassifyEyeResourceOwnership(
            true, postSignature, false, 0)
            == post_effect_resource_math::EyeResourceOwnership::Unknown,
        "single-eye resource evidence remains unknown");
    two_hand_math::TwoHandBasis twoHandBasis;
    failures += Check(
        two_hand_math::BuildTwoHandBasis(
            {0.0f, 0.0f, 0.0f},
            {0.0f, 0.0f, 1.0f},
            {0.0f, 1.0f, 0.0f},
            {0.5f, 0.0f, 0.0f},
            1.0f,
            0.08f,
            1.2f,
            twoHandBasis)
            && Near(twoHandBasis.forward.x, 1.0f)
            && Near(twoHandBasis.forward.y, 0.0f)
            && Near(twoHandBasis.forward.z, 0.0f)
            && Near(twoHandBasis.up.y, 1.0f)
            && Near(twoHandBasis.separation, 0.5f),
        "two-hand basis aims from dominant grip to support grip and preserves roll up");
    failures += Check(
        !two_hand_math::BuildTwoHandBasis(
            {0.0f, 0.0f, 0.0f},
            {0.0f, 0.0f, 1.0f},
            {0.0f, 1.0f, 0.0f},
            {0.01f, 0.0f, 0.0f},
            1.0f,
            0.08f,
            1.2f,
            twoHandBasis),
        "two-hand basis rejects unsafe hand separation");
    const camera_math::Vector3 twoHandAngular =
        two_hand_math::ResolveDirectionAngularTargetVelocity(
            {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, 2.0f, 1.0f, 10.0f);
    failures += Check(
        Near(twoHandAngular.x, 0.0f)
            && Near(twoHandAngular.y, 3.14159265f, 1.0e-4f)
            && Near(twoHandAngular.z, 0.0f),
        "two-hand direction rotation returns bounded shortest-arc angular velocity");
    const camera_math::Vector3 oppositeTwoHandAngular =
        two_hand_math::ResolveDirectionAngularTargetVelocity(
            {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}, 100.0f, 1.0f, 1.5f);
    const float oppositeTwoHandSpeed = std::sqrt(
        oppositeTwoHandAngular.x * oppositeTwoHandAngular.x
        + oppositeTwoHandAngular.y * oppositeTwoHandAngular.y
        + oppositeTwoHandAngular.z * oppositeTwoHandAngular.z);
    failures += Check(
        std::isfinite(oppositeTwoHandSpeed) && Near(oppositeTwoHandSpeed, 1.5f),
        "two-hand opposite direction selects a finite capped fallback axis");
    failures += Check(
        dual_render_math::BuildReplayMask(0x7) == 0x5
            && dual_render_math::BuildReplayMask(UINT64_MAX) == (UINT64_MAX & ~2ull),
        "dual-render replay preserves world/post flags and removes screen GUI");
    failures += Check(
        dual_render_math::IsReplayEligible(true, true, true, true, true, 0, 42, 0x7)
            && !dual_render_math::IsReplayEligible(true, false, true, true, true, 0, 42, 0x7)
            && !dual_render_math::IsReplayEligible(true, true, true, true, true, 2, 42, 0x7)
            && !dual_render_math::IsReplayEligible(true, true, true, true, true, 0, 42, 0x2),
        "dual-render replay requires the tracked player world viewport and a valid eye");
    failures += Check(
        dual_render_math::IsSamePoseOppositeEye(0, 42, 1, 42)
            && dual_render_math::IsSamePoseOppositeEye(1, 42, 0, 42)
            && !dual_render_math::IsSamePoseOppositeEye(0, 42, 0, 42)
            && !dual_render_math::IsSamePoseOppositeEye(0, 42, 1, 43),
        "dual-render replay validates opposite eyes from one tracked pose");
    per_eye_view_history_math::Bank viewHistoryBank;
    per_eye_view_history_math::ViewHistoryPacket sharedHistory{};
    sharedHistory.fill(0x11);
    failures += Check(
        per_eye_view_history_math::SetActive(viewHistoryBank, true),
        "per-eye view history activates from a clean bank");
    auto viewHistoryPrepare = per_eye_view_history_math::Prepare(
        viewHistoryBank, 0x1000, 0x2000, 1, 0, 10, sharedHistory);
    failures += Check(
        viewHistoryPrepare.valid
            && viewHistoryPrepare.reset
            && viewHistoryPrepare.seeded
            && viewHistoryPrepare.restorePacket == sharedHistory,
        "per-eye view history seeds both eyes from the pre-frame shared packet");
    per_eye_view_history_math::ViewHistoryPacket leftHistory{};
    leftHistory.fill(0x22);
    failures += Check(
        per_eye_view_history_math::Commit(
            viewHistoryBank, 0x1000, 0x2000, 1, 0, 10, leftHistory),
        "per-eye view history captures the first eye packet");
    per_eye_view_history_math::ViewHistoryPacket liveAfterLeft = leftHistory;
    viewHistoryPrepare = per_eye_view_history_math::Prepare(
        viewHistoryBank, 0x1000, 0x2000, 1, 1, 10, liveAfterLeft);
    failures += Check(
        viewHistoryPrepare.valid
            && !viewHistoryPrepare.seeded
            && viewHistoryPrepare.restorePacket == sharedHistory,
        "second eye restores its seed instead of consuming the first eye current view");
    per_eye_view_history_math::ViewHistoryPacket rightHistory{};
    rightHistory.fill(0x33);
    failures += Check(
        per_eye_view_history_math::Commit(
            viewHistoryBank, 0x1000, 0x2000, 1, 1, 10, rightHistory),
        "per-eye view history captures the replay eye packet");
    viewHistoryPrepare = per_eye_view_history_math::Prepare(
        viewHistoryBank, 0x1000, 0x2000, 1, 0, 11, rightHistory);
    failures += Check(
        viewHistoryPrepare.valid && viewHistoryPrepare.restorePacket == leftHistory,
        "next frame restores the previous matrix for the matching eye");
    per_eye_view_history_math::ViewHistoryPacket recenteredHistory{};
    recenteredHistory.fill(0x3a);
    viewHistoryPrepare = per_eye_view_history_math::Prepare(
        viewHistoryBank, 0x1000, 0x2000, 2, 1, 12, recenteredHistory);
    failures += Check(
        viewHistoryPrepare.reset
            && viewHistoryPrepare.seeded
            && viewHistoryPrepare.restorePacket == recenteredHistory,
        "recenter generation change reseeds both eye histories");
    failures += Check(
        per_eye_view_history_math::Commit(
            viewHistoryBank, 0x1000, 0x2000, 2, 1, 12, rightHistory),
        "recentered eye history commits under the new generation");
    per_eye_view_history_math::ViewHistoryPacket recoveredHistory{};
    recoveredHistory.fill(0x3c);
    viewHistoryPrepare = per_eye_view_history_math::Prepare(
        viewHistoryBank,
        0x1000,
        0x2000,
        2,
        1,
        12 + per_eye_view_history_math::kMaxPoseFrameGap + 1,
        recoveredHistory);
    failures += Check(
        viewHistoryPrepare.reset
            && viewHistoryPrepare.seeded
            && viewHistoryPrepare.restorePacket == recoveredHistory,
        "long loading or tracking gap discards stale eye history");
    per_eye_view_history_math::ViewHistoryPacket replacementHistory{};
    replacementHistory.fill(0x44);
    viewHistoryPrepare = per_eye_view_history_math::Prepare(
        viewHistoryBank, 0x3000, 0x4000, 1, 1, 12, replacementHistory);
    failures += Check(
        viewHistoryPrepare.reset
            && viewHistoryPrepare.seeded
            && viewHistoryPrepare.restorePacket == replacementHistory,
        "renderer or history replacement invalidates both stale eye banks");
    failures += Check(
        per_eye_view_history_math::SetActive(viewHistoryBank, false)
            && !viewHistoryBank.active
            && !viewHistoryBank.valid[0]
            && !viewHistoryBank.valid[1],
        "per-eye view history rollback clears every cached packet");
    const std::array<uint8_t, 12> temporalBefore = {
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
    };
    std::array<uint8_t, 12> temporalAfter = temporalBefore;
    temporalAfter[2] = 20;
    temporalAfter[3] = 30;
    temporalAfter[8] = 80;
    const temporal_mutation_math::MutationSummary temporalMutation =
        temporal_mutation_math::SummarizeMutation(
            temporalBefore.data(), temporalAfter.data(), temporalBefore.size());
    failures += Check(
        temporalMutation.changedBytes == 3
            && temporalMutation.spanCount == 2
            && temporalMutation.spans[0].offset == 2
            && temporalMutation.spans[0].length == 2
            && temporalMutation.spans[1].offset == 8
            && temporalMutation.spans[1].length == 1
            && temporalMutation.beforeHash != temporalMutation.afterHash,
        "temporal mutation summary records bounded changed-byte spans");
    std::array<uint8_t, 12> equivalentAfter = temporalBefore;
    equivalentAfter[2] = 22;
    equivalentAfter[3] = 33;
    equivalentAfter[8] = 88;
    const temporal_mutation_math::MutationSummary equivalentMutation =
        temporal_mutation_math::SummarizeMutation(
            temporalBefore.data(), equivalentAfter.data(), equivalentAfter.size());
    failures += Check(
        temporal_mutation_math::HasEquivalentMutationPattern(
            temporalMutation, equivalentMutation),
        "temporal mutation equivalence compares ranges independently of values");
    subtitle_math::SubtitleLayout subtitleLayout;
    failures += Check(
        subtitle_math::BuildSubtitleLayout(
            {860.0f, 700.0f, 26.0f, 1.0f},
            {0.9f, 1.15f, 0.0f},
            subtitleLayout)
            && Near(subtitleLayout.maxTextWidth, 774.0f)
            && Near(subtitleLayout.textY, 700.0f)
            && Near(subtitleLayout.fontSize, 29.9f)
            && Near(subtitleLayout.shadowOffset, 1.15f),
        "subtitle layout scales native width, font, and shadow coherently");
    failures += Check(
        !subtitle_math::BuildSubtitleLayout(
            {0.0f, 700.0f, 26.0f, 1.0f}, {0.9f, 1.15f, 0.0f}, subtitleLayout)
            && !subtitle_math::BuildSubtitleLayout(
                {860.0f, 700.0f, 26.0f, 1.0f}, {0.1f, 1.15f, 0.0f}, subtitleLayout),
        "subtitle layout rejects malformed native and configuration values");
    failures += Check(camera_math::ValidateStereoProjectionMath(), "projection self-test");
    spectator_math::BlitLayout spectatorLayout;
    failures += Check(
        spectator_math::ComputeBlitLayout(
            2000, 1000, 1000, 1000, spectator_math::AspectMode::Fit, spectatorLayout)
            && spectatorLayout.destinationX0 == 0
            && spectatorLayout.destinationY0 == 250
            && spectatorLayout.destinationX1 == 1000
            && spectatorLayout.destinationY1 == 750
            && spectatorLayout.clearDestination,
        "spectator fit creates centered letterbox bars");
    failures += Check(
        spectator_math::ComputeBlitLayout(
            2000, 1000, 1000, 1000, spectator_math::AspectMode::Fill, spectatorLayout)
            && spectatorLayout.sourceX0 == 500
            && spectatorLayout.sourceX1 == 1500
            && spectatorLayout.destinationX1 == 1000
            && !spectatorLayout.clearDestination,
        "spectator fill crops the source symmetrically");
    failures += Check(
        spectator_math::ComputeBlitLayout(
            2000, 1000, 1000, 1000, spectator_math::AspectMode::Stretch, spectatorLayout)
            && spectatorLayout.sourceX1 == 2000
            && spectatorLayout.destinationY1 == 1000,
        "spectator stretch uses the complete source and destination");
    failures += Check(
        !spectator_math::ComputeBlitLayout(
            0, 1000, 1000, 1000, spectator_math::AspectMode::Fit, spectatorLayout),
        "spectator layout rejects invalid dimensions");
    depth_math::CompositionDepthRange depthRange;
    failures += Check(
        depth_math::BuildStandardDepthRange(0.03f, 1000.0f, 1.0f, depthRange)
            && Near(depthRange.minDepth, 0.0f)
            && Near(depthRange.maxDepth, 1.0f)
            && Near(depthRange.nearMeters, 0.03f)
            && Near(depthRange.farMeters, 1000.0f),
        "standard OpenGL depth maps HPL clip distances to OpenXR meters");
    failures += Check(
        depth_math::BuildStandardDepthRange(0.06f, 2000.0f, 2.0f, depthRange)
            && Near(depthRange.nearMeters, 0.03f)
            && Near(depthRange.farMeters, 1000.0f),
        "depth range converts configurable HPL world scale");
    failures += Check(
        !depth_math::BuildStandardDepthRange(0.0f, 1000.0f, 1.0f, depthRange)
            && !depth_math::BuildStandardDepthRange(10.0f, 1.0f, 1.0f, depthRange)
            && !depth_math::BuildStandardDepthRange(0.03f, 1000.0f, 0.0f, depthRange),
        "depth range rejects malformed clip and scale inputs");
    failures += Check(
        Near(camera_math::ComputeRoomscaleSafetyFactor(0.75f, 1.0f, 0.10f), 0.65f),
        "room-scale safety retracts collision fraction by clearance");
    failures += Check(
        Near(camera_math::ComputeRoomscaleSafetyFactor(0.05f, 1.0f, 0.10f), 0.0f),
        "room-scale safety clearance clamps before nearby geometry");
    failures += Check(
        Near(camera_math::ComputeRoomscaleSafetyFactor(2.0f, 1.0f, 0.0f), 1.0f)
            && Near(camera_math::ComputeRoomscaleSafetyFactor(0.5f, 0.0f, 0.1f), 1.0f),
        "room-scale safety bounds fractions and preserves invalid zero-distance input");
    const camera_math::Vector3 safeEyeOffset = camera_math::ReplaceTrackedHeadTranslation(
        {0.34f, 0.12f, -0.20f},
        {0.30f, 0.10f, -0.20f},
        {0.15f, 0.05f, -0.10f});
    failures += Check(
        Near(safeEyeOffset.x, 0.19f)
            && Near(safeEyeOffset.y, 0.07f)
            && Near(safeEyeOffset.z, -0.10f),
        "room-scale safety replaces shared head motion while preserving eye-relative offset");
    std::array<camera_math::Vector3, camera_math::kMaxRoomscaleSafetySamples> safetyOffsets{};
    const size_t safetyOffsetCount = camera_math::BuildRoomscaleSafetySampleOffsets(
        0.10f,
        0.12f,
        4,
        safetyOffsets);
    failures += Check(
        safetyOffsetCount == 7
            && Near(safetyOffsets[0].x, 0.0f)
            && Near(safetyOffsets[1].x, 0.10f)
            && Near(safetyOffsets[5].y, 0.12f)
            && Near(safetyOffsets[6].y, -0.12f),
        "room-scale safety builds center, radial, and vertical head-volume samples");
    failures += Check(
        camera_math::BuildRoomscaleSafetySampleOffsets(-1.0f, -1.0f, 99, safetyOffsets) == 1,
        "room-scale safety invalid radii retain only the center sample");

    float catchupX = 0.0f;
    float catchupZ = 0.0f;
    failures += Check(
        roomscale_reconciliation_math::ComputeBodyCatchupStep(
            0.60f, 0.0f, 0.45f, 0.25f, 0.015f, false, catchupX, catchupZ)
            && Near(catchupX, 0.015f) && Near(catchupZ, 0.0f),
        "room-scale body reconciliation bounds its first catch-up step");
    failures += Check(
        roomscale_reconciliation_math::ComputeBodyCatchupStep(
            0.44f, 0.0f, 0.45f, 0.25f, 0.015f, true, catchupX, catchupZ)
            && Near(catchupX, 0.015f),
        "room-scale body reconciliation hysteresis continues toward its target");
    failures += Check(
        !roomscale_reconciliation_math::ComputeBodyCatchupStep(
            0.44f, 0.0f, 0.45f, 0.25f, 0.015f, false, catchupX, catchupZ)
            && !roomscale_reconciliation_math::ComputeBodyCatchupStep(
                0.20f, 0.0f, 0.45f, 0.25f, 0.015f, true, catchupX, catchupZ),
        "room-scale body reconciliation respects activation and release thresholds");

    failures += Check(
        comfort_math::ShouldSuppressCameraAdd(1, true, true, true),
        "comfort policy suppresses head bob");
    failures += Check(
        comfort_math::ShouldSuppressCameraAdd(2, true, true, true),
        "comfort policy suppresses camera shake");
    failures += Check(
        comfort_math::ShouldSuppressCameraAdd(9, true, true, true),
        "comfort policy suppresses sway");
    failures += Check(
        !comfort_math::ShouldSuppressCameraAdd(5, true, true, true)
            && !comfort_math::ShouldSuppressCameraAdd(7, true, true, true)
            && !comfort_math::ShouldSuppressCameraAdd(10, true, true, true),
        "comfort policy preserves script lean and conversation channels");
    failures += Check(
        !comfort_math::ShouldSuppressCameraAdd(1, false, true, true)
            && !comfort_math::ShouldSuppressCameraAdd(2, true, false, true)
            && !comfort_math::ShouldSuppressCameraAdd(9, true, true, false),
        "comfort policy honors independent channel switches");

    const input_math::Axis2 centeredStick = input_math::ApplyRadialDeadzone(0.2f, 0.1f, 0.35f);
    failures += Check(Near(centeredStick.x, 0.0f) && Near(centeredStick.y, 0.0f), "radial deadzone center");
    const input_math::Axis2 fullDiagonal = input_math::ApplyRadialDeadzone(0.70710678f, 0.70710678f, 0.35f);
    failures += Check(
        Near(std::sqrt(fullDiagonal.x * fullDiagonal.x + fullDiagonal.y * fullDiagonal.y), 1.0f),
        "radial deadzone preserves full diagonal magnitude");
    const input_math::Axis2 halfStick = input_math::ApplyRadialDeadzone(0.0f, 0.675f, 0.35f);
    failures += Check(Near(halfStick.x, 0.0f) && Near(halfStick.y, 0.5f), "radial deadzone rescales magnitude");
    failures += Check(Near(input_math::DegreesToRadians(30.0f), 0.5235988f), "snap-turn degree conversion");
    constexpr float kHalfSqrtTwo = 0.7071067811865475f;
    const input_math::Axis2 headRightMovement = input_math::ApplyHeadRelativeMovement(
        0.0f,
        1.0f,
        {0.0f, -kHalfSqrtTwo, 0.0f, kHalfSqrtTwo});
    failures += Check(
        Near(headRightMovement.x, 1.0f) && Near(headRightMovement.y, 0.0f),
        "head-relative forward follows rightward head yaw");
    const input_math::Axis2 pitchedHeadMovement = input_math::ApplyHeadRelativeMovement(
        0.0f,
        1.0f,
        {kHalfSqrtTwo, 0.0f, 0.0f, kHalfSqrtTwo});
    failures += Check(
        Near(pitchedHeadMovement.x, 0.0f) && Near(pitchedHeadMovement.y, 1.0f),
        "head-relative movement ignores head pitch");

    input_math::ManipulationMotionState manipulationState;
    const input_math::ManipulationMouseDelta manipulationRight =
        input_math::ComputeManipulationMouseDelta(
            {}, {0.015625f, 0.0f, 0.0f}, {}, 1024.0f, 0.0005f, 80, 1.0f, -1.0f,
            manipulationState);
    failures += Check(
        manipulationRight.x == 16 && manipulationRight.y == 0
            && Near(manipulationRight.rightMeters, 0.015625f),
        "physical manipulation projects head-right hand motion");
    const input_math::ManipulationMouseDelta manipulationUp =
        input_math::ComputeManipulationMouseDelta(
            {}, {0.0f, 0.03125f, 0.0f}, {}, 1024.0f, 0.0005f, 80, 1.0f, -1.0f,
            manipulationState);
    failures += Check(
        manipulationUp.x == 0 && manipulationUp.y == -32
            && Near(manipulationUp.upMeters, 0.03125f),
        "physical manipulation maps hand-up to mouse-up");
    input_math::ManipulationMotionState cappedManipulationState;
    const input_math::ManipulationMouseDelta cappedManipulation =
        input_math::ComputeManipulationMouseDelta(
            {}, {1.0f, -1.0f, 0.0f}, {}, 1000.0f, 0.0005f, 80, 1.0f, -1.0f,
            cappedManipulationState);
    failures += Check(
        cappedManipulation.x == 80 && cappedManipulation.y == 80,
        "physical manipulation caps per-frame mouse deltas without backlog");
    const input_math::ManipulationMouseDelta deadzoneManipulation =
        input_math::ComputeManipulationMouseDelta(
            {}, {0.0001f, 0.0001f, 0.0f}, {}, 1000.0f, 0.0005f, 80, 1.0f, -1.0f,
            cappedManipulationState);
    failures += Check(
        deadzoneManipulation.x == 0 && deadzoneManipulation.y == 0,
        "physical manipulation ignores sub-deadzone jitter");

    crouch_math::PhysicalCrouchState crouchState;
    failures += Check(
        crouch_math::UpdatePhysicalCrouch(crouchState, 1.70f, true, 1, 0.35f, 0.25f)
            == crouch_math::PhysicalCrouchUpdate::Calibrated,
        "physical crouch calibrates standing height");
    failures += Check(
        crouch_math::UpdatePhysicalCrouch(crouchState, 1.34f, true, 1, 0.35f, 0.25f)
            == crouch_math::PhysicalCrouchUpdate::Enter,
        "physical crouch enters below threshold");
    failures += Check(
        crouch_math::UpdatePhysicalCrouch(crouchState, 1.40f, true, 1, 0.35f, 0.25f)
            == crouch_math::PhysicalCrouchUpdate::None,
        "physical crouch hysteresis holds state");
    failures += Check(
        crouch_math::UpdatePhysicalCrouch(crouchState, 1.46f, true, 1, 0.35f, 0.25f)
            == crouch_math::PhysicalCrouchUpdate::Exit,
        "physical crouch exits above release threshold");

    hud_math::HudQuadPose hudPose;
    failures += Check(
        hud_math::BuildHeadLockedQuadPose(
            {1.0f, 2.0f, 3.0f},
            {},
            1.5f,
            0.1f,
            1.6f,
            16.0f / 9.0f,
            hudPose),
        "head-locked HUD pose construction");
    failures += Check(
        Near(hudPose.position.x, 1.0f)
            && Near(hudPose.position.y, 2.1f)
            && Near(hudPose.position.z, 1.5f),
        "head-locked HUD local offset");
    failures += Check(
        Near(hudPose.widthMeters, 1.6f) && Near(hudPose.heightMeters, 0.9f),
        "head-locked HUD aspect ratio");
    failures += Check(
        !hud_math::BuildHeadLockedQuadPose({}, {}, 0.0f, 0.0f, 1.0f, 1.0f, hudPose),
        "head-locked HUD rejects invalid distance");

    hud_math::HudCylinderPose cylinderPose;
    failures += Check(
        hud_math::BuildHeadLockedCylinderPose(
            {1.0f, 2.0f, 3.0f}, {}, 1.5f, 0.1f, 1.6f, 16.0f / 9.0f,
            60.0f, cylinderPose),
        "head-locked curved HUD pose construction");
    failures += Check(
        Near(cylinderPose.radiusMeters, 1.527887f, 0.00001f)
            && Near(cylinderPose.centralAngleRadians, 1.047198f, 0.00001f)
            && Near(cylinderPose.aspectRatio, 16.0f / 9.0f),
        "curved HUD derives radius and angle from physical arc width");
    failures += Check(
        Near(cylinderPose.position.x, 1.0f)
            && Near(cylinderPose.position.y, 2.1f)
            && Near(cylinderPose.position.z, 3.027887f, 0.00001f)
            && Near(
                cylinderPose.radiusMeters * cylinderPose.centralAngleRadians
                    / cylinderPose.aspectRatio,
                0.9f),
        "curved HUD preserves center distance and texture aspect height");
    failures += Check(
        !hud_math::BuildHeadLockedCylinderPose(
            {}, {}, 1.5f, 0.0f, 1.6f, 1.0f, 360.0f, cylinderPose),
        "curved HUD rejects a full-circle layer");

    camera_math::Vector3 screenEffectPosition;
    failures += Check(
        screen_effect_math::ScaleCameraRelativePosition(
            {10.0f, 2.0f, -4.0f},
            {10.15f, 1.925f, -4.15f},
            10.0f,
            screenEffectPosition)
            && Near(screenEffectPosition.x, 11.5f)
            && Near(screenEffectPosition.y, 1.25f)
            && Near(screenEffectPosition.z, -5.5f),
        "screen material preserves angular placement at a comfortable distance");
    screen_effect_math::Size2 screenEffectSize;
    failures += Check(
        screen_effect_math::ScaleBillboardSize({0.3f, 0.15f}, 10.0f, screenEffectSize)
            && Near(screenEffectSize.width, 3.0f)
            && Near(screenEffectSize.height, 1.5f),
        "screen material preserves angular size when moved outward");
    failures += Check(
        !screen_effect_math::ScaleCameraRelativePosition({}, {}, 0.0f, screenEffectPosition)
            && !screen_effect_math::ScaleBillboardSize({0.0f, 1.0f}, 10.0f, screenEffectSize),
        "screen material scaling rejects invalid inputs");
    float reticleSizeMeters = 0.0f;
    failures += Check(
        hud_math::ComputeAngularQuadSize(2.0f, 1.0f, 0.005f, 0.1f, reticleSizeMeters)
            && Near(reticleSizeMeters, 0.034907f, 0.00001f),
        "interaction reticle angular size");
    failures += Check(
        hud_math::ComputeAngularQuadSize(0.1f, 0.1f, 0.008f, 0.08f, reticleSizeMeters)
            && Near(reticleSizeMeters, 0.008f),
        "interaction reticle minimum size clamp");
    failures += Check(
        hud_math::ComputeAngularQuadSize(100.0f, 5.0f, 0.008f, 0.08f, reticleSizeMeters)
            && Near(reticleSizeMeters, 0.08f),
        "interaction reticle maximum size clamp");

    std::array<float, 16> handMatrix{};
    const hands_math::HandRootCalibration handCalibration{
        {0.1f, -0.075f, 0.2f},
        {},
    };
    failures += Check(
        hands_math::BuildControllerHandMatrix(
            {1.0f, 2.0f, 3.0f},
            {0.0f, 0.0f, -1.0f},
            {0.0f, 1.0f, 0.0f},
            0.25f,
            handCalibration,
            handMatrix),
        "controller hand root builds from tracked basis");
    failures += Check(
        Near(handMatrix[0], -0.25f)
            && Near(handMatrix[5], 0.25f)
            && Near(handMatrix[10], -0.25f)
            && Near(handMatrix[15], 1.0f),
        "controller hand root preserves SOMA rotateY(pi) basis and scale");
    failures += Check(
        Near(handMatrix[3], 1.1f)
            && Near(handMatrix[7], 1.925f)
            && Near(handMatrix[11], 2.8f),
        "controller hand root applies controller-local position calibration");
    failures += Check(
        !hands_math::BuildControllerHandMatrix(
            {},
            {0.0f, 1.0f, 0.0f},
            {0.0f, 1.0f, 0.0f},
            0.25f,
            {},
            handMatrix),
        "controller hand root rejects collinear tracking basis");

    std::array<float, 16> hudObjectMatrix{};
    const hands_math::HudObjectCalibration hudObjectCalibration{
        {0.1f, -0.05f, 0.2f},
        {},
    };
    failures += Check(
        hands_math::BuildControllerHudObjectMatrix(
            {1.0f, 2.0f, 3.0f},
            {0.0f, 0.0f, -1.0f},
            {0.0f, 1.0f, 0.0f},
            2.0f,
            hudObjectCalibration,
            hudObjectMatrix),
        "controller HudObject root builds from tracked basis");
    failures += Check(
        Near(hudObjectMatrix[0], 2.0f)
            && Near(hudObjectMatrix[5], 2.0f)
            && Near(hudObjectMatrix[10], 2.0f)
            && Near(hudObjectMatrix[15], 1.0f),
        "controller HudObject preserves camera-style basis and native scale");
    failures += Check(
        Near(hudObjectMatrix[3], 1.1f)
            && Near(hudObjectMatrix[7], 1.95f)
            && Near(hudObjectMatrix[11], 2.8f),
        "controller HudObject applies grip-local position calibration");
    failures += Check(
        !hands_math::BuildControllerHudObjectMatrix(
            {},
            {0.0f, 1.0f, 0.0f},
            {0.0f, 1.0f, 0.0f},
            1.0f,
            {},
            hudObjectMatrix),
        "controller HudObject rejects collinear tracking basis");

    std::array<float, 16> flashlightMatrix{};
    const flashlight_math::FlashlightCalibration flashlightCalibration{
        {0.1f, -0.05f, 0.2f},
        {},
    };
    failures += Check(
        flashlight_math::BuildControllerFlashlightMatrix(
            {1.0f, 2.0f, 3.0f},
            {0.0f, 0.0f, -1.0f},
            {0.0f, 1.0f, 0.0f},
            flashlightCalibration,
            flashlightMatrix),
        "controller flashlight builds from tracked aim basis");
    failures += Check(
        Near(flashlightMatrix[0], 1.0f)
            && Near(flashlightMatrix[5], 1.0f)
            && Near(flashlightMatrix[10], 1.0f)
            && Near(flashlightMatrix[15], 1.0f),
        "controller flashlight maps OpenXR forward to HPL local negative Z");
    failures += Check(
        Near(flashlightMatrix[3], 1.1f)
            && Near(flashlightMatrix[7], 1.95f)
            && Near(flashlightMatrix[11], 2.8f),
        "controller flashlight applies aim-local position calibration");
    failures += Check(
        !flashlight_math::BuildControllerFlashlightMatrix(
            {},
            {0.0f, 1.0f, 0.0f},
            {0.0f, 1.0f, 0.0f},
            {},
            flashlightMatrix),
        "controller flashlight rejects collinear tracking basis");

    camera_math::Vector3 redirectedCone{};
    failures += Check(
        flashlight_math::RedirectConeDirection(
            {0.2f, 0.1f, -0.9746794f},
            {0.0f, 0.0f, -1.0f},
            {0.0f, 1.0f, 0.0f},
            {1.0f, 0.0f, 0.0f},
            {0.0f, 1.0f, 0.0f},
            redirectedCone)
            && Near(redirectedCone.x, 0.9746794f)
            && Near(redirectedCone.y, 0.1f)
            && Near(redirectedCone.z, 0.2f),
        "flashlight gameplay ray preserves randomized cone in controller basis");
    failures += Check(
        !flashlight_math::RedirectConeDirection(
            {0.0f, 0.0f, -1.0f},
            {0.0f, 1.0f, 0.0f},
            {0.0f, 1.0f, 0.0f},
            {0.0f, 0.0f, -1.0f},
            {0.0f, 1.0f, 0.0f},
            redirectedCone),
        "flashlight gameplay ray rejects malformed source basis");

    failures += Check(
        comfort_math::ShouldSuppressCameraRoll(1, false, true, true, true)
            && comfort_math::ShouldSuppressCameraRoll(2, false, true, true, true)
            && comfort_math::ShouldSuppressCameraRoll(3, false, true, true, true)
            && !comfort_math::ShouldSuppressCameraRoll(0, false, true, true, true)
            && !comfort_math::ShouldSuppressCameraRoll(99, true, true, true, true),
        "semantic camera roll policy preserves script and suppresses lean move climb");
    failures += Check(
        comfort_math::ShouldBlackoutPlayerStateTransition(0, 11)
            && comfort_math::ShouldBlackoutPlayerStateTransition(12, 0)
            && comfort_math::ShouldBlackoutPlayerStateTransition(14, 15)
            && comfort_math::ShouldBlackoutPlayerStateTransition(0, 17)
            && !comfort_math::ShouldBlackoutPlayerStateTransition(0, 1)
            && !comfort_math::ShouldBlackoutPlayerStateTransition(-1, 11),
        "authored high-motion state transitions request comfort blackout");
    failures += Check(
        Near(comfort_math::ResolveComfortOpticsTarget(
                 comfort_math::OpticsChannel::Fov, 0.8f, 1.2f),
            1.2f)
            && Near(comfort_math::ResolveComfortOpticsTarget(
                        comfort_math::OpticsChannel::FovMultiplier, 0.7f, 1.2f),
                1.0f)
            && Near(comfort_math::ResolveComfortOpticsTarget(
                        comfort_math::OpticsChannel::AspectMultiplier, 1.4f, 1.2f),
                1.0f)
            && Near(comfort_math::ResolveComfortOpticsTarget(
                        comfort_math::OpticsChannel::Fov, 0.8f, -1.0f),
                0.8f),
        "comfort optics returns native default FOV and neutral multipliers");

    using grab_math::ResolveAngularTargetVelocity;
    const camera_math::Vector3 noGrabRotation = ResolveAngularTargetVelocity({}, {}, 100.0f, 1.0f, 6.0f);
    failures += Check(
        Near(noGrabRotation.x, 0.0f) && Near(noGrabRotation.y, 0.0f) && Near(noGrabRotation.z, 0.0f),
        "grab rotation identity delta");
    const camera_math::Vector3 yawGrabRotation = ResolveAngularTargetVelocity(
        {},
        {0.0f, kHalfSqrtTwo, 0.0f, kHalfSqrtTwo},
        100.0f,
        1.0f,
        6.0f);
    failures += Check(
        Near(yawGrabRotation.x, 0.0f) && Near(yawGrabRotation.y, 6.0f) && Near(yawGrabRotation.z, 0.0f),
        "grab rotation follows shortest yaw arc and speed cap");
    const camera_math::Vector3 equivalentGrabRotation = ResolveAngularTargetVelocity(
        {},
        {0.0f, 0.0f, 0.0f, -1.0f},
        100.0f,
        1.0f,
        6.0f);
    failures += Check(
        Near(equivalentGrabRotation.x, 0.0f)
            && Near(equivalentGrabRotation.y, 0.0f)
            && Near(equivalentGrabRotation.z, 0.0f),
        "grab rotation treats negated quaternion as equivalent");

    menu_math::MenuPointerPosition menuPointer;
    failures += Check(
        menu_math::ProjectAimToMenu({}, {}, 70.0f, 50.0f, menuPointer)
            && Near(menuPointer.x, 0.5f)
            && Near(menuPointer.y, 0.5f),
        "head-relative menu aim projects to center");
    failures += Check(
        !menu_math::ProjectAimToMenu(
            {},
            {0.0f, 1.0f, 0.0f, 0.0f},
            70.0f,
            50.0f,
            menuPointer),
        "menu aim rejects controller pointing behind head");
    failures += Check(
        !menu_math::ProjectAimToMenu(
            {},
            {0.0f, 0.0f, 0.0f, 0.0f},
            70.0f,
            50.0f,
            menuPointer),
        "menu aim rejects malformed orientation");

    OpenXREyeView asymmetricEye;
    asymmetricEye.valid = true;
    asymmetricEye.angleLeft = -0.82f;
    asymmetricEye.angleRight = 0.66f;
    asymmetricEye.angleDown = -0.49f;
    asymmetricEye.angleUp = 0.73f;
    const float horizontalSpan = std::tan(asymmetricEye.angleRight) - std::tan(asymmetricEye.angleLeft);
    const float verticalSpan = std::tan(asymmetricEye.angleUp) - std::tan(asymmetricEye.angleDown);

    const OpenXREyeView centeredEye = camera_math::CenterProjectionFov(asymmetricEye);
    failures += Check(Near(centeredEye.angleLeft, -centeredEye.angleRight), "horizontal FOV centering");
    failures += Check(Near(centeredEye.angleDown, -centeredEye.angleUp), "vertical FOV centering");
    failures += Check(
        Near(std::tan(centeredEye.angleRight) - std::tan(centeredEye.angleLeft), horizontalSpan),
        "horizontal tangent span preservation");
    failures += Check(
        Near(std::tan(centeredEye.angleUp) - std::tan(centeredEye.angleDown), verticalSpan),
        "vertical tangent span preservation");

    std::array<float, 16> projection{};
    float verticalFov = 0.0f;
    float aspect = 0.0f;
    failures += Check(
        camera_math::BuildOpenXRProjection(centeredEye, 0.03f, 1000.0f, projection, verticalFov, aspect),
        "centered projection construction");
    failures += Check(Near(projection[2], 0.0f), "horizontal projection offset");
    failures += Check(Near(projection[6], 0.0f), "vertical projection offset");

    const camera_math::Quaternion identity;
    const camera_math::Vector3 inputVector{1.0f, 2.0f, 3.0f};
    const camera_math::Vector3 identityRotated = camera_math::RotateVector(identity, inputVector);
    failures += Check(
        Near(identityRotated.x, inputVector.x)
            && Near(identityRotated.y, inputVector.y)
            && Near(identityRotated.z, inputVector.z),
        "identity quaternion rotation");
    const camera_math::Vector3 yawRotated = camera_math::RotateVector(
        {0.0f, kHalfSqrtTwo, 0.0f, kHalfSqrtTwo},
        {0.0f, 0.0f, 1.0f});
    failures += Check(
        Near(yawRotated.x, 1.0f) && Near(yawRotated.y, 0.0f) && Near(yawRotated.z, 0.0f),
        "quarter-turn yaw rotation");

    const camera_math::Quaternion matrixTestRotation = camera_math::Normalize(
        {0.23f, -0.41f, 0.17f, 0.86f});
    const std::array<float, 16> rotationMatrix = camera_math::RotationMatrix(matrixTestRotation);
    const auto rowDot = [&rotationMatrix](size_t leftRow, size_t rightRow) {
        float value = 0.0f;
        for (size_t column = 0; column < 3; ++column) {
            value += rotationMatrix[leftRow * 4 + column]
                * rotationMatrix[rightRow * 4 + column];
        }
        return value;
    };
    failures += Check(
        Near(rowDot(0, 0), 1.0f) && Near(rowDot(1, 1), 1.0f) && Near(rowDot(2, 2), 1.0f),
        "rotation matrix unit basis");
    failures += Check(
        Near(rowDot(0, 1), 0.0f) && Near(rowDot(0, 2), 0.0f) && Near(rowDot(1, 2), 0.0f),
        "rotation matrix orthogonal basis");

    const camera_math::Vector3 matrixTestVector{0.31f, -0.27f, 0.73f};
    const camera_math::Vector3 quaternionResult = camera_math::RotateVector(
        matrixTestRotation,
        matrixTestVector);
    const camera_math::Vector3 matrixResult{
        rotationMatrix[0] * matrixTestVector.x + rotationMatrix[1] * matrixTestVector.y
            + rotationMatrix[2] * matrixTestVector.z,
        rotationMatrix[4] * matrixTestVector.x + rotationMatrix[5] * matrixTestVector.y
            + rotationMatrix[6] * matrixTestVector.z,
        rotationMatrix[8] * matrixTestVector.x + rotationMatrix[9] * matrixTestVector.y
            + rotationMatrix[10] * matrixTestVector.z,
    };
    failures += Check(
        Near(matrixResult.x, quaternionResult.x)
            && Near(matrixResult.y, quaternionResult.y)
            && Near(matrixResult.z, quaternionResult.z),
        "rotation matrix matches quaternion rotation");

    camera_math::PoseStabilityState poseLatch;
    float positionStep = 0.0f;
    float orientationStep = 0.0f;
    camera_math::PoseStabilityUpdate poseUpdate = camera_math::UpdatePoseStability(
        poseLatch,
        1,
        camera_math::Quaternion{},
        {0.0f, -1.2447f, 0.0f},
        8,
        0.25f,
        0.7853982f,
        positionStep,
        orientationStep);
    failures += Check(
        poseUpdate == camera_math::PoseStabilityUpdate::Started
            && poseLatch.consecutiveFrames == 1,
        "pose stability starts on first unique frame");
    poseUpdate = camera_math::UpdatePoseStability(
        poseLatch,
        1,
        camera_math::Quaternion{},
        {0.0f, -1.2447f, 0.0f},
        8,
        0.25f,
        0.7853982f,
        positionStep,
        orientationStep);
    failures += Check(
        poseUpdate == camera_math::PoseStabilityUpdate::DuplicateFrame
            && poseLatch.consecutiveFrames == 1,
        "pose stability ignores duplicate game-frame samples");

    const camera_math::Quaternion settledOrientation = camera_math::Normalize(
        {0.07f, 0.55f, 0.04f, -0.83f});
    const camera_math::Vector3 settledPosition{-0.45f, 0.55f, -0.33f};
    poseUpdate = camera_math::UpdatePoseStability(
        poseLatch,
        2,
        settledOrientation,
        settledPosition,
        8,
        0.25f,
        0.7853982f,
        positionStep,
        orientationStep);
    failures += Check(
        poseUpdate == camera_math::PoseStabilityUpdate::Reset
            && poseLatch.consecutiveFrames == 1
            && positionStep > 1.0f,
        "pose stability resets after reference-space jump");

    for (uint64_t frame = 3; frame <= 9; ++frame) {
        poseUpdate = camera_math::UpdatePoseStability(
            poseLatch,
            frame,
            settledOrientation,
            settledPosition,
            8,
            0.25f,
            0.7853982f,
            positionStep,
            orientationStep);
    }
    failures += Check(
        poseUpdate == camera_math::PoseStabilityUpdate::Ready
            && poseLatch.consecutiveFrames == 8,
        "pose stability requires consecutive settled frames");

    const camera_math::Quaternion equivalentOrientation{
        -settledOrientation.x,
        -settledOrientation.y,
        -settledOrientation.z,
        -settledOrientation.w,
    };
    poseUpdate = camera_math::UpdatePoseStability(
        poseLatch,
        10,
        equivalentOrientation,
        settledPosition,
        8,
        0.25f,
        0.7853982f,
        positionStep,
        orientationStep);
    failures += Check(
        poseUpdate == camera_math::PoseStabilityUpdate::Ready
            && Near(orientationStep, 0.0f),
        "pose stability accepts equivalent quaternion sign");

    const std::array<float, 16> identityMatrix = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
    const std::array<float, 16> translation = camera_math::TranslationMatrix({4.0f, 5.0f, 6.0f});
    failures += Check(
        camera_math::MatrixMultiply(identityMatrix, translation) == translation,
        "matrix identity multiplication");

    const camera_math::Vector3 roomscaleEye{1.0f, 2.0f, 3.0f};
    const camera_math::Vector3 roomscaleCenter{0.9f, 1.5f, 2.8f};
    const camera_math::Vector3 roomscaleNeutral{0.5f, 1.0f, 2.0f};
    const camera_math::Vector3 fullRoomscale = camera_math::ResolveTrackedEyeOffset(
        roomscaleEye,
        roomscaleCenter,
        roomscaleNeutral,
        camera_math::Quaternion{},
        true,
        true,
        2.0f,
        0.1f);
    failures += Check(
        Near(fullRoomscale.x, 1.0f)
            && Near(fullRoomscale.y, 2.2f)
            && Near(fullRoomscale.z, 2.0f),
        "roomscale and eye-height calibration");
    const camera_math::Vector3 horizontalRoomscale = camera_math::ResolveTrackedEyeOffset(
        roomscaleEye,
        roomscaleCenter,
        roomscaleNeutral,
        camera_math::Quaternion{},
        true,
        false,
        2.0f,
        0.1f);
    failures += Check(
        Near(horizontalRoomscale.x, 1.0f)
            && Near(horizontalRoomscale.y, 1.2f)
            && Near(horizontalRoomscale.z, 2.0f),
        "vertical roomscale suppression preserves stereo height");

    const GLfloat perspective[16] = {
        0.803333f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.428148f, 0.0f, 0.0f,
        0.0f, 0.0f, -1.000060f, -0.060002f,
        0.0f, 0.0f, -1.0f, 0.0f,
    };
    const gl_matrix::MatrixSummary summary = gl_matrix::SummarizeMatrix(perspective);
    failures += Check(summary.valid && summary.projectionLike, "OpenGL perspective classification");
    failures += Check(Near(summary.aspect, 16.0f / 9.0f, 0.0001f), "OpenGL matrix aspect");
    failures += Check(!gl_matrix::SummarizeMatrix(identityMatrix.data()).projectionLike, "identity is not projection");
    failures += Check(
        gl_matrix::MatrixValuesText(perspective).find(',') != std::string::npos,
        "matrix value formatting");

    hud_math::InteractionReticleColor reticleColor;
    failures += Check(
        !hud_math::ComputeInteractionReticleColor(0, reticleColor)
            && !hud_math::ComputeInteractionReticleColor(35, reticleColor),
        "semantic reticle rejects none and sentinel states");
    failures += Check(
        hud_math::ComputeInteractionReticleColor(14, reticleColor)
            && Near(reticleColor.red, 0.25f)
            && Near(reticleColor.green, 1.0f)
            && Near(reticleColor.blue, 0.55f),
        "semantic reticle classifies pickup state");
    failures += Check(
        hud_math::ComputeInteractionReticleColor(4, reticleColor)
            && Near(reticleColor.red, 1.0f)
            && Near(reticleColor.green, 0.72f)
            && Near(reticleColor.blue, 0.20f),
        "semantic reticle classifies manipulation state");
    failures += Check(
        hud_math::ComputeInteractionReticleColor(30, reticleColor)
            && Near(reticleColor.red, 1.0f)
            && Near(reticleColor.green, 0.30f)
            && Near(reticleColor.blue, 0.25f),
        "semantic reticle classifies unavailable state");
    float hapticAmplitudeScale = 0.0f;
    float hapticDurationScale = 0.0f;
    failures += Check(
        !hud_math::ComputeInteractionHapticProfile(1, hapticAmplitudeScale, hapticDurationScale),
        "semantic focus haptics ignore default cursor");
    failures += Check(
        hud_math::ComputeInteractionHapticProfile(14, hapticAmplitudeScale, hapticDurationScale)
            && Near(hapticAmplitudeScale, 0.75f)
            && Near(hapticDurationScale, 0.80f),
        "semantic focus haptics classify pickup state");
    failures += Check(
        hud_math::ComputeInteractionHapticProfile(4, hapticAmplitudeScale, hapticDurationScale)
            && Near(hapticAmplitudeScale, 1.15f)
            && Near(hapticDurationScale, 1.25f),
        "semantic focus haptics classify manipulation state");

    failures += Check(
        Near(comfort_vignette_math::ComputeMotionIntensity(
            0.8f, 0.0f, 0.0f, false, 0.3f, 0.6f), 5.0f / 7.0f),
        "comfort vignette normalizes movement after deadzone");
    failures += Check(
        Near(comfort_vignette_math::ComputeMotionIntensity(
            0.0f, 0.0f, -0.8f, true, 0.3f, 0.6f), 0.5f),
        "comfort vignette includes smooth turn intensity");
    failures += Check(
        Near(comfort_vignette_math::ComputeMotionIntensity(
            0.0f, 0.0f, -0.8f, false, 0.3f, 0.6f), 0.0f),
        "comfort vignette excludes snap-turn stick hold");
    failures += Check(
        Near(comfort_vignette_math::AdvanceEnvelope(0.0f, 1.0f, 0.05f, 200), 0.25f)
            && Near(comfort_vignette_math::AdvanceEnvelope(0.75f, 0.0f, 0.05f, 200), 0.5f),
        "comfort vignette attack and release envelope");
    std::vector<uint8_t> vignettePixels;
    failures += Check(
        comfort_vignette_math::Rasterize(64, 1.0f, 0.6f, 0.5f, vignettePixels)
            && vignettePixels.size() == 64u * 64u * 4u,
        "comfort vignette raster dimensions");
    const size_t vignetteCenter = (32u * 64u + 32u) * 4u;
    const size_t vignetteCorner = 3u;
    failures += Check(
        vignettePixels[vignetteCenter + 3] == 0
            && vignettePixels[vignetteCorner] >= 152
            && vignettePixels[vignetteCorner] <= 154,
        "comfort vignette preserves center and darkens periphery");
    failures += Check(
        !comfort_vignette_math::Rasterize(16, 1.0f, 0.6f, 0.5f, vignettePixels),
        "comfort vignette rejects undersized targets");

    status_panel_math::PanelModel panel;
    panel.visible = true;
    panel.selectedAction = 2;
    panel.trackingEnabled = true;
    panel.stereoEnabled = true;
    panel.roomscaleEnabled = true;
    panel.projectionCentered = true;
    panel.dualRenderReady = true;
    panel.continuousDualRender = true;
    panel.viewHistoryConfigured = true;
    panel.viewHistoryActive = true;
    panel.hudVisible = true;
    panel.hudCylinderAvailable = true;
    panel.hudCylinderActive = true;
    panel.reticleVisible = true;
    panel.inputAvailable = true;
    panel.controllerTracked = true;
    panel.playerState = 8;
    panel.gameFrame = 1234;
    std::vector<uint8_t> panelPixels;
    failures += Check(
        status_panel_math::RasterizePanel(panel, 1024, 512, panelPixels)
            && panelPixels.size() == 1024u * 512u * 4u,
        "VR status panel raster dimensions");
    failures += Check(
        std::any_of(panelPixels.begin() + 3, panelPixels.end(), [](uint8_t value) { return value != 0; }),
        "VR status panel raster is visible");
    failures += Check(
        !status_panel_math::RasterizePanel(panel, 128, 128, panelPixels),
        "VR status panel rejects undersized targets");
    failures += Check(status_panel_math::kActionCount == 9, "VR status panel action contract");

    if (failures == 0) {
        std::cout << "Render math tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
