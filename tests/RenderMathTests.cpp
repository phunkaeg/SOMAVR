#include "HPLCameraMath.h"
#include "CrashCapturePolicy.h"
#include "HPLComfortMath.h"
#include "HPLContactHapticsMath.h"
#include "HPLFlashlightMath.h"
#include "HPLHandsMath.h"
#include "HPLArmIKMath.h"
#include "HPLAuthoredInteractionMath.h"
#include "HPLGrabMath.h"
#include "HPLReadMath.h"
#include "HPLGameplayHapticsMath.h"
#include "HPLHudMath.h"
#include "HPLInteractionMath.h"
#include "HPLInputMath.h"
#include "HPLMenuMath.h"
#include "HPLPhysicalCrouchMath.h"
#include "HPLRoomscaleReconciliationMath.h"
#include "HPLScreenEffectMath.h"
#include "HPLSubtitleMath.h"
#include "HPLTerminalMath.h"
#include "HPLDualRenderMath.h"
#include "HPLPerEyeViewHistoryMath.h"
#include "HPLPerEyePostEffectMath.h"
#include "HPLToneMappingFrameMath.h"
#include "HPLSSAOTemporalMath.h"
#include "HPLTemporalMutationMath.h"
#include "HPLTwoHandMath.h"
#include "HPLPostEffectResourceMath.h"
#include "OpenGLMatrixAnalysis.h"
#include "OpenGLOwnership.h"
#include "OpenXRSpectatorMath.h"
#include "OpenXRDepthMath.h"
#include "OpenXRComfortVignetteMath.h"
#include "OpenXRFramePacingMath.h"
#include "OpenXRStatusPanelMath.h"
#include "PatchSafety.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
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
    failures += Check(
        openxr_frame_pacing_math::UpcomingRenderDisplayTime(1'000, 11) == 1'011
            && openxr_frame_pacing_math::UpcomingRenderDisplayTime(1'000, 0) == 1'000
            && openxr_frame_pacing_math::UpcomingRenderDisplayTime(
                (std::numeric_limits<int64_t>::max)() - 2, 11)
                == (std::numeric_limits<int64_t>::max)(),
        "OpenXR prediction advances one period without overflowing");
    failures += Check(
        openxr_frame_pacing_math::RequiresFallbackProjection(true, 0)
            && !openxr_frame_pacing_math::RequiresFallbackProjection(true, 1)
            && !openxr_frame_pacing_math::RequiresFallbackProjection(false, 0),
        "every begun OpenXR frame requires at least one projection layer");
    failures += Check(!IsOwnOpenGLWork(), "OpenGL ownership starts outside mod GL work");
    {
        ScopedOwnOpenGLWork outerOwnGl;
        failures += Check(IsOwnOpenGLWork(), "OpenGL ownership marks an active mod scope");
        {
            ScopedOwnOpenGLWork nestedOwnGl;
            failures += Check(IsOwnOpenGLWork(), "OpenGL ownership remains active when nested");
        }
        failures += Check(IsOwnOpenGLWork(), "nested OpenGL ownership restores the outer scope");
    }
    failures += Check(!IsOwnOpenGLWork(), "OpenGL ownership clears after scope exit");
    failures += Check(
        !patch_safety::InstructionPointerOverlapsPatch(0x0fff, 0x1000, 12)
            && patch_safety::InstructionPointerOverlapsPatch(0x1000, 0x1000, 12)
            && patch_safety::InstructionPointerOverlapsPatch(0x100b, 0x1000, 12)
            && !patch_safety::InstructionPointerOverlapsPatch(0x100c, 0x1000, 12),
        "native patch safety treats the byte window as a half-open interval");
    arm_ik_math::TwoBoneSolution armSolution;
    failures += Check(
        arm_ik_math::SolveTwoBone(
            {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f},
            {0.0f, -1.0f, 0.0f}, 0.75f, 0.75f, 0.985f, armSolution)
            && Near(armSolution.solvedDistance, 1.0f)
            && armSolution.elbow.y < 0.0f
            && !armSolution.reachClamped,
        "arm IK reaches a controller target on the native elbow side");
    failures += Check(
        arm_ik_math::SolveTwoBone(
            {0.0f, 0.0f, 0.0f}, {4.0f, 0.0f, 0.0f},
            {0.0f, 0.0f, 1.0f}, 0.5f, 0.5f, 0.98f, armSolution)
            && Near(armSolution.solvedDistance, 0.98f)
            && armSolution.reachClamped,
        "arm IK clamps unreachable targets before elbow lock");
    arm_ik_math::ShoulderReachSolution shoulderReach;
    failures += Check(
        arm_ik_math::ComputeReachShoulderTarget(
            {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f},
            {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f, 0.0f}, true,
            1.5f, 0.85f, 0.985f, 0.05f, 1.0f, nullptr, shoulderReach)
            && !shoulderReach.applied
            && Near(shoulderReach.blend, 0.0f),
        "shoulder reach compensation leaves ordinary hand motion anchored");
    failures += Check(
        arm_ik_math::ComputeReachShoulderTarget(
            {0.0f, 0.0f, 0.0f}, {0.0f, 0.2f, -1.48f},
            {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f, 0.0f}, false,
            1.5f, 0.85f, 0.985f, 0.05f, 1.0f, nullptr, shoulderReach)
            && shoulderReach.applied
            && shoulderReach.offsetWorld.y > 0.0f
            && shoulderReach.offsetWorld.z < 0.0f
            && Near(std::sqrt(
                shoulderReach.offsetWorld.x * shoulderReach.offsetWorld.x
                + shoulderReach.offsetWorld.y * shoulderReach.offsetWorld.y
                + shoulderReach.offsetWorld.z * shoulderReach.offsetWorld.z),
                0.05f, 1.0e-4f),
        "shoulder reach compensation contributes only a bounded forward-up offset");
    const camera_math::Vector3 previousShoulderLocal{0.0f, 0.0f, 0.05f};
    failures += Check(
        arm_ik_math::ComputeReachShoulderTarget(
            {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -0.5f},
            {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f, 0.0f}, true,
            1.5f, 0.85f, 0.985f, 0.05f, 0.5f,
            &previousShoulderLocal, shoulderReach)
            && shoulderReach.applied
            && Near(shoulderReach.offsetLocal.z, 0.025f),
        "latent shoulder contribution releases smoothly without delaying the wrist");

    arm_ik_math::ElbowPoleSolution elbowPole;
    failures += Check(
        arm_ik_math::ComputeErgonomicElbowPole(
            {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f},
            {-0.4f, 0.0f, -0.5f}, {0.0f, 0.0f, -1.0f},
            {0.0f, 1.0f, 0.0f}, true, 1.0f, 1.0f, 10.0f, nullptr, elbowPole)
            && elbowPole.directionWorld.x < 0.0f
            && elbowPole.directionWorld.y < 0.0f,
        "left ergonomic elbow favours a down-and-out torso-space pole");
    failures += Check(
        arm_ik_math::ComputeErgonomicElbowPole(
            {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f},
            {0.4f, 0.0f, -0.5f}, {0.0f, 0.0f, -1.0f},
            {0.0f, 1.0f, 0.0f}, false, 1.0f, 1.0f, 10.0f, nullptr, elbowPole)
            && elbowPole.directionWorld.x > 0.0f
            && elbowPole.directionWorld.y < 0.0f,
        "right ergonomic elbow remains independently down-and-out");
    const camera_math::Vector3 previousElbowLocal{1.0f, 0.0f, 0.0f};
    failures += Check(
        arm_ik_math::ComputeErgonomicElbowPole(
            {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.01f},
            {0.4f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f},
            {0.0f, 1.0f, 0.0f}, false, 1.0f, 1.0f, 5.0f,
            &previousElbowLocal, elbowPole)
            && elbowPole.historyUsed
            && elbowPole.singularityBlend > 0.9f
            && elbowPole.directionLocal.x > 0.9f,
        "elbow pole preserves torso-local history near vertical singularity");
    const camera_math::Vector3 oppositeElbowLocal{-1.0f, 0.0f, 0.0f};
    failures += Check(
        arm_ik_math::ComputeErgonomicElbowPole(
            {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f},
            {0.4f, 0.0f, -0.5f}, {0.0f, 0.0f, -1.0f},
            {0.0f, 1.0f, 0.0f}, false, 1.0f, 1.0f, 5.0f,
            &oppositeElbowLocal, elbowPole)
            && elbowPole.historyUsed
            && elbowPole.swivelLimited,
        "elbow pole caps a discontinuous per-frame swivel change");
    const std::array<float, 16> ikIdentityMatrix = {
        1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    std::array<float, 16> rotatedArm{};
    failures += Check(
        arm_ik_math::RotateWorldMatrixToward(
            ikIdentityMatrix, {1.0f, 0.0f, 0.0f},
            {0.0f, 1.0f, 0.0f}, 1.0f, rotatedArm)
            && Near(rotatedArm[0], 0.0f, 1.0e-4f)
            && Near(rotatedArm[4], 1.0f, 1.0e-4f),
        "arm IK rotates a node toward its solved child target");

    authored_interaction_math::MedicineSettings medicineSettings;
    medicineSettings.capLocalOffset = {0.0f, 0.1f, 0.0f};
    medicineSettings.capProximity = 0.12f;
    medicineSettings.mouthProximity = 0.15f;
    medicineSettings.drinkTipDegrees = 60.0f;
    medicineSettings.drinkHoldFrames = 2;
    authored_interaction_math::MedicineState medicineState;
    authored_interaction_math::MedicineFrame medicineFrame;
    medicineFrame.bottleValid = true;
    medicineFrame.bottleWorld = ikIdentityMatrix;
    medicineFrame.leftHandValid = true;
    medicineFrame.leftHandPosition = {0.0f, 0.1f, 0.0f};
    medicineFrame.leftActionPressed = true;
    medicineFrame.headValid = true;
    medicineFrame.headPosition = {0.0f, 0.1f, 0.0f};
    authored_interaction_math::MedicineResult medicineResult;
    failures += Check(
        authored_interaction_math::UpdateMedicine(
            medicineState, medicineSettings, medicineFrame, medicineResult)
            && medicineResult.capRemoved
            && medicineState.stage == authored_interaction_math::MedicineStage::AwaitDrink,
        "medicine profile removes the cap only on proximity plus action edge");
    medicineFrame.leftActionPressed = false;
    medicineFrame.bottleWorld = {
        1,0,0,0, 0,-1,0,0, 0,0,-1,0, 0,0,0,1};
    medicineFrame.headPosition = {0.0f, -0.1f, 0.0f};
    authored_interaction_math::UpdateMedicine(
        medicineState, medicineSettings, medicineFrame, medicineResult);
    failures += Check(
        authored_interaction_math::UpdateMedicine(
            medicineState, medicineSettings, medicineFrame, medicineResult)
            && medicineResult.drinkCompleted
            && medicineState.stage == authored_interaction_math::MedicineStage::Complete,
        "medicine profile requires a sustained tipped bottle at the mouth");
    failures += Check(
        interaction_math::SelectInteractionHand(
            {true, true, false, 2.0f}, {true, false, false, 0.0f}, 1, -1) == 0,
        "interaction selects the only hand with a native hit");
    failures += Check(
        interaction_math::SelectInteractionHand(
            {true, true, false, 2.0f}, {true, true, true, 1.0f}, 0, 0) == 1,
        "interaction press transfers ownership immediately");
    failures += Check(
        interaction_math::SelectInteractionHand(
            {true, true, false, 2.0f}, {true, true, false, 1.0f}, 1, 0) == 0,
        "interaction preserves the previous owner while both hands hit");
    failures += Check(
        interaction_math::SelectInteractionHand(
            {true, false, false, 0.0f}, {true, false, false, 0.0f}, 1, -1) == 1,
        "interaction falls back to the configured preferred hand");
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
    contact_haptics_math::Settings contactSettings;
    contactSettings.minSpeed = 0.5f;
    contactSettings.maxSpeed = 4.5f;
    contactSettings.maxDistance = 0.75f;
    contactSettings.minAmplitude = 0.1f;
    contactSettings.maxAmplitude = 0.6f;
    contactSettings.durationMs = 35;
    contactSettings.cooldownMs = 50;
    contact_haptics_math::State contactState;
    auto contactDecision = contact_haptics_math::Evaluate(
        contactState, true, 0.4f, 0.2f, 100, contactSettings);
    failures += Check(
        !contactDecision.pulse
            && contactDecision.reject == contact_haptics_math::RejectReason::BelowSpeed,
        "contact haptics reject weak native impacts");
    contactDecision = contact_haptics_math::Evaluate(
        contactState, true, 4.5f, 0.2f, 101, contactSettings);
    failures += Check(
        contactDecision.pulse && Near(contactDecision.amplitude, 0.6f)
            && contactDecision.durationMs == 35,
        "contact haptics map a strong nearby impact to the configured maximum");
    contactDecision = contact_haptics_math::Evaluate(
        contactState, true, 4.5f, 0.2f, 120, contactSettings);
    failures += Check(
        !contactDecision.pulse
            && contactDecision.reject == contact_haptics_math::RejectReason::Cooldown,
        "contact haptics suppress duplicate material callbacks");
    contactDecision = contact_haptics_math::Evaluate(
        contactState, true, 2.5f, 0.2f, 151, contactSettings);
    failures += Check(
        contactDecision.pulse && Near(contactDecision.amplitude, 0.35f),
        "contact haptics scale intermediate collision speed linearly");
    contactDecision = contact_haptics_math::Evaluate(
        contactState, true, 4.5f, 0.8f, 250, contactSettings);
    failures += Check(
        !contactDecision.pulse
            && contactDecision.reject == contact_haptics_math::RejectReason::TooFar,
        "contact haptics reject impacts outside the dominant-grip neighborhood");
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
    failures += Check(
        Near(input_math::QuaternionAngularDistanceDegrees(
            {}, {0.0f, kHalfSqrtTwo, 0.0f, kHalfSqrtTwo}), 90.0f, 0.0001f)
            && Near(input_math::QuaternionAngularDistanceDegrees(
                {}, {0.0f, -kHalfSqrtTwo, 0.0f, -kHalfSqrtTwo}), 90.0f, 0.0001f),
        "terminal look-away uses shortest quaternion angular distance");
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

    input_math::ManipulationMotionState rotationState;
    constexpr float kFiveDegrees = 0.0871557427f;
    constexpr float kCosFiveDegrees = 0.9961946981f;
    const input_math::ManipulationRotationDelta manipulationYaw =
        input_math::ComputeManipulationRotationDelta(
            {}, {0.0f, kFiveDegrees, 0.0f, kCosFiveDegrees},
            900.0f, 200, 1.0f, -1.0f, rotationState);
    failures += Check(
        manipulationYaw.x == 157 && manipulationYaw.y == 0
            && Near(manipulationYaw.yawRadians, input_math::DegreesToRadians(10.0f), 0.0001f),
        "inspection rotation maps controller yaw to native look pixels");
    input_math::ManipulationMotionState pitchRotationState;
    const input_math::ManipulationRotationDelta manipulationPitch =
        input_math::ComputeManipulationRotationDelta(
            {}, {kFiveDegrees, 0.0f, 0.0f, kCosFiveDegrees},
            900.0f, 200, 1.0f, -1.0f, pitchRotationState);
    failures += Check(
        manipulationPitch.x == 0 && manipulationPitch.y == -157
            && Near(manipulationPitch.pitchRadians, input_math::DegreesToRadians(10.0f), 0.0001f),
        "inspection rotation maps controller pitch with configured vertical sign");

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

    hud_math::HudQuadPose surfacePointer{};
    failures += Check(
        hud_math::BuildHudSurfacePointerPose(
            0.5f, 0.5f, 1.5f, 0.1f, 1.6f, 16.0f / 9.0f,
            false, 70.0f, 0.02f, surfacePointer)
            && Near(surfacePointer.position.x, 0.0f)
            && Near(surfacePointer.position.y, 0.1f)
            && Near(surfacePointer.position.z, -1.498f)
            && Near(surfacePointer.widthMeters, 0.02f),
        "HUD surface pointer maps normalized center to the quad center");
    failures += Check(
        hud_math::BuildHudSurfacePointerPose(
            1.0f, 0.0f, 1.5f, 0.0f, 1.6f, 16.0f / 9.0f,
            true, 70.0f, 0.02f, surfacePointer)
            && surfacePointer.position.x > 0.0f
            && surfacePointer.position.y > 0.0f
            && surfacePointer.orientation.y < 0.0f,
        "HUD surface pointer follows the cylinder arc and tangent");

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

    const std::array<float, 16> wristParentWorld = {
        0.25f, 0.0f, 0.0f, 10.0f,
        0.0f, 0.25f, 0.0f, 20.0f,
        0.0f, 0.0f, 0.25f, 30.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
    const std::array<float, 16> wristAnimatedLocal = {
        1.0f, 0.0f, 0.0f, 1.0f,
        0.0f, 1.0f, 0.0f, 2.0f,
        0.0f, 0.0f, 1.0f, 3.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
    std::array<float, 16> wristDesiredWorld = camera_math::MatrixMultiply(
        wristParentWorld, wristAnimatedLocal);
    wristDesiredWorld[3] = 11.5f;
    wristDesiredWorld[7] = 21.0f;
    wristDesiredWorld[11] = 29.5f;
    std::array<float, 16> wristPostTransform{};
    const bool wristPostValid = hands_math::BuildPostTransformForWorldTarget(
        wristParentWorld,
        wristAnimatedLocal,
        wristDesiredWorld,
        wristPostTransform);
    const std::array<float, 16> reconstructedWristWorld = camera_math::MatrixMultiply(
        camera_math::MatrixMultiply(wristParentWorld, wristPostTransform),
        wristAnimatedLocal);
    failures += Check(
        wristPostValid
            && Near(reconstructedWristWorld[3], wristDesiredWorld[3])
            && Near(reconstructedWristWorld[7], wristDesiredWorld[7])
            && Near(reconstructedWristWorld[11], wristDesiredWorld[11]),
        "wrist post-transform dry run reconstructs a world-space target");
    std::array<float, 16> singularWristParent{};
    failures += Check(
        !hands_math::BuildPostTransformForWorldTarget(
            singularWristParent,
            wristAnimatedLocal,
            wristDesiredWorld,
            wristPostTransform),
        "wrist post-transform dry run rejects a singular hierarchy");

    camera_math::Quaternion anchorControllerOrientation;
    camera_math::Quaternion anchorWristOrientation;
    const std::array<float, 16> anchoredWristWorld = {
        0.0f, 0.0f, 0.5f, 4.0f,
        0.0f, 0.5f, 0.0f, 5.0f,
        -0.5f, 0.0f, 0.0f, 6.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
    const bool wristAnchorsValid = camera_math::QuaternionFromForwardUp(
            {0.0f, 0.0f, -1.0f},
            {0.0f, 1.0f, 0.0f},
            anchorControllerOrientation)
        && camera_math::QuaternionFromRotationMatrix(
            anchoredWristWorld, anchorWristOrientation);
    std::array<float, 16> trackedWristWorld{};
    failures += Check(
        wristAnchorsValid
            && hands_math::BuildTrackedWristWorldMatrix(
                {7.0f, 8.0f, 9.0f},
                {0.0f, 0.0f, -1.0f},
                {0.0f, 1.0f, 0.0f},
                anchorControllerOrientation,
                anchorWristOrientation,
                anchoredWristWorld,
                trackedWristWorld)
            && Near(trackedWristWorld[0], anchoredWristWorld[0])
            && Near(trackedWristWorld[2], anchoredWristWorld[2])
            && Near(trackedWristWorld[5], anchoredWristWorld[5])
            && Near(trackedWristWorld[8], anchoredWristWorld[8])
            && Near(trackedWristWorld[3], 7.0f)
            && Near(trackedWristWorld[7], 8.0f)
            && Near(trackedWristWorld[11], 9.0f),
        "tracked wrist orientation preserves its native first-frame controller alignment");
    failures += Check(
        hands_math::BuildTrackedWristWorldMatrix(
            {7.0f, 8.0f, 9.0f},
            {-1.0f, 0.0f, 0.0f},
            {0.0f, 1.0f, 0.0f},
            anchorControllerOrientation,
            anchorWristOrientation,
            anchoredWristWorld,
            trackedWristWorld)
            && Near(std::sqrt(
                trackedWristWorld[0] * trackedWristWorld[0]
                    + trackedWristWorld[4] * trackedWristWorld[4]
                    + trackedWristWorld[8] * trackedWristWorld[8]), 0.5f)
            && !Near(trackedWristWorld[2], anchoredWristWorld[2]),
        "tracked wrist orientation follows controller rotation while preserving bone scale");

    const auto translatedIdentity = [](float x, float y, float z) {
        return std::array<float, 16>{
            1.0f, 0.0f, 0.0f, x,
            0.0f, 1.0f, 0.0f, y,
            0.0f, 0.0f, 1.0f, z,
            0.0f, 0.0f, 0.0f, 1.0f,
        };
    };
    const std::array<float, 16> palmIndex = translatedIdentity(4.12f, 5.0f, 5.80f);
    const std::array<float, 16> palmMiddle = translatedIdentity(4.04f, 5.0f, 5.76f);
    const std::array<float, 16> palmRing = translatedIdentity(3.96f, 5.0f, 5.76f);
    const std::array<float, 16> palmPinky = translatedIdentity(3.88f, 5.0f, 5.80f);
    camera_math::Quaternion palmToWrist{};
    std::array<float, 16> geometricWristWorld{};
    failures += Check(
        hands_math::BuildPalmToWristOrientation(
            anchoredWristWorld,
            palmIndex,
            palmMiddle,
            palmRing,
            palmPinky,
            palmToWrist)
            && hands_math::BuildTrackedWristWorldMatrixFromOffset(
                {7.0f, 8.0f, 9.0f},
                {0.0f, 0.0f, -1.0f},
                {0.0f, 1.0f, 0.0f},
                palmToWrist,
                anchoredWristWorld,
                geometricWristWorld)
            && Near(geometricWristWorld[0], anchoredWristWorld[0])
            && Near(geometricWristWorld[2], anchoredWristWorld[2])
            && Near(geometricWristWorld[5], anchoredWristWorld[5])
            && Near(geometricWristWorld[8], anchoredWristWorld[8]),
        "geometric palm calibration deterministically preserves the model wrist basis");
    const camera_math::Quaternion rolledPalmToWrist =
        hands_math::ApplyControllerForwardRoll({}, -90.0f);
    const camera_math::Vector3 rolledUp = camera_math::RotateVector(
        rolledPalmToWrist, {0.0f, 1.0f, 0.0f});
    failures += Check(
        Near(rolledUp.x, -1.0f) && Near(rolledUp.y, 0.0f) && Near(rolledUp.z, 0.0f),
        "wrist calibration applies configurable roll around controller forward");
    const camera_math::Quaternion pitchedWrist =
        hands_math::ApplyControllerLocalPitch({}, 45.0f);
    const camera_math::Vector3 pitchedForward = camera_math::RotateVector(
        pitchedWrist, {0.0f, 0.0f, -1.0f});
    failures += Check(
        Near(pitchedForward.y, std::sqrt(0.5f), 0.0001f)
            && Near(pitchedForward.z, -std::sqrt(0.5f), 0.0001f),
        "wrist calibration applies configurable pitch around controller right");
    const std::array<float, 16> degeneratePalm = translatedIdentity(4.0f, 5.0f, 6.0f);
    failures += Check(
        !hands_math::BuildPalmToWristOrientation(
            anchoredWristWorld,
            degeneratePalm,
            degeneratePalm,
            degeneratePalm,
            degeneratePalm,
            palmToWrist),
        "geometric palm calibration rejects a degenerate finger layout");

    const std::array<float, 16> retainedHandsRoot = {
        0.25f, 0.0f, 0.0f, 10.0f,
        0.0f, 0.25f, 0.0f, 1.6f,
        0.0f, 0.0f, 0.25f, -0.4f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
    const camera_math::Quaternion zeroYaw{};
    const float halfSqrt = std::sqrt(0.5f);
    const camera_math::Quaternion quarterTurnYaw{0.0f, halfSqrt, 0.0f, halfSqrt};
    const camera_math::Vector3 sourceCamera{10.0f, 1.7f, 0.0f};
    const camera_math::Vector3 targetCamera{12.0f, 1.8f, 3.0f};
    std::array<float, 16> bodyAnchoredRoot{};
    const camera_math::Vector3 expectedRelative = camera_math::RotateVector(
        quarterTurnYaw, {0.0f, -0.1f, -0.4f});
    failures += Check(
        hands_math::BuildBodyAnchoredRootMatrix(
            retainedHandsRoot,
            sourceCamera,
            zeroYaw,
            targetCamera,
            quarterTurnYaw,
            bodyAnchoredRoot)
            && Near(bodyAnchoredRoot[3], targetCamera.x + expectedRelative.x)
            && Near(bodyAnchoredRoot[7], targetCamera.y + expectedRelative.y)
            && Near(bodyAnchoredRoot[11], targetCamera.z + expectedRelative.z),
        "retained hand root follows tracked anchor translation and body yaw around the anchor pivot");
    failures += Check(
        Near(std::sqrt(
            bodyAnchoredRoot[0] * bodyAnchoredRoot[0]
                + bodyAnchoredRoot[4] * bodyAnchoredRoot[4]
                + bodyAnchoredRoot[8] * bodyAnchoredRoot[8]), 0.25f)
            && Near(std::sqrt(
                bodyAnchoredRoot[1] * bodyAnchoredRoot[1]
                    + bodyAnchoredRoot[5] * bodyAnchoredRoot[5]
                    + bodyAnchoredRoot[9] * bodyAnchoredRoot[9]), 0.25f),
        "retained hand body-yaw anchoring preserves native root scale");

    const std::array<float, 16> quarterScaleHands = {
        0.0f, 0.0f, 0.25f, 10.0f,
        0.0f, 0.25f, 0.0f, 20.0f,
        -0.25f, 0.0f, 0.0f, 30.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
    std::array<float, 16> fullScaleHands{};
    failures += Check(
        hands_math::NormalizeUniformScale(
            quarterScaleHands, 0.25f, 1.0f, 0.04f, fullScaleHands),
        "player hands quarter-scale root normalizes to authored full scale");
    failures += Check(
        Near(fullScaleHands[2], 1.0f)
            && Near(fullScaleHands[5], 1.0f)
            && Near(fullScaleHands[8], -1.0f)
            && Near(fullScaleHands[3], 10.0f)
            && Near(fullScaleHands[7], 20.0f)
            && Near(fullScaleHands[11], 30.0f),
        "player hands scale normalization preserves root pose");
    const std::array<float, 16> acceptedFullScaleHands = fullScaleHands;
    std::array<float, 16> nonUniformHands = quarterScaleHands;
    nonUniformHands[5] = 0.5f;
    failures += Check(
        !hands_math::NormalizeUniformScale(
            nonUniformHands, 0.25f, 1.0f, 0.04f, fullScaleHands),
        "player hands scale normalization rejects nonuniform authored roots");

    std::array<float, 16> staleBodyAnchoredRoot = bodyAnchoredRoot;
    std::array<float, 16> reconciledBodyAnchoredRoot{};
    failures += Check(
        hands_math::ApplyRootBasisScale(
            acceptedFullScaleHands, staleBodyAnchoredRoot, reconciledBodyAnchoredRoot)
            && Near(reconciledBodyAnchoredRoot[3], staleBodyAnchoredRoot[3])
            && Near(reconciledBodyAnchoredRoot[7], staleBodyAnchoredRoot[7])
            && Near(reconciledBodyAnchoredRoot[11], staleBodyAnchoredRoot[11])
            && Near(std::sqrt(
                reconciledBodyAnchoredRoot[0] * reconciledBodyAnchoredRoot[0]
                    + reconciledBodyAnchoredRoot[4] * reconciledBodyAnchoredRoot[4]
                    + reconciledBodyAnchoredRoot[8] * reconciledBodyAnchoredRoot[8]), 1.0f),
        "retained body anchoring adopts the current normalized hand-root scale");

    constexpr uint32_t kColorBit = 0x00004000;
    constexpr uint32_t kDepthBit = 0x00000100;
    constexpr uint32_t kStencilBit = 0x00000400;
    terminal_math::ClearPolicyResult terminalClear =
        terminal_math::ResolveRetainedSurfaceClear(
            kColorBit | kDepthBit | kStencilBit,
            kColorBit,
            true,
            true,
            true);
    failures += Check(
        terminalClear.colorSuppressed
            && terminalClear.forwardedMask == (kDepthBit | kStencilBit),
        "terminal retention suppresses only the nested color clear");
    terminalClear = terminal_math::ResolveRetainedSurfaceClear(
        kColorBit | kDepthBit,
        kColorBit,
        true,
        true,
        false);
    failures += Check(
        !terminalClear.colorSuppressed
            && terminalClear.forwardedMask == (kColorBit | kDepthBit),
        "terminal retention preserves clears for unrelated framebuffers");
    failures += Check(
        !terminal_math::ShouldFallbackToLiveTerminalFrames(7, 8, false)
            && terminal_math::ShouldFallbackToLiveTerminalFrames(8, 8, false)
            && !terminal_math::ShouldFallbackToLiveTerminalFrames(8, 8, true)
            && !terminal_math::ShouldFallbackToLiveTerminalFrames(8, 8, false, true),
        "terminal retention falls back only after bounded missing-clear evidence without a repaired draw path");
    terminal_math::HudPointerPosition terminalPointer{};
    const camera_math::Quaternion identityOrientation{0.0f, 0.0f, 0.0f, 1.0f};
    failures += Check(
        terminal_math::ProjectAimToHudSurface(
            identityOrientation,
            identityOrientation,
            true,
            70.0f,
            1.5f,
            1.6f,
            16.0f / 9.0f,
            terminalPointer)
            && Near(terminalPointer.x, 0.5f)
            && Near(terminalPointer.y, 0.5f),
        "terminal HUD projection aligns straight controller aim to overlay center");
    camera_math::Quaternion rightAim{};
    failures += Check(
        camera_math::QuaternionFromForwardUp(
            {0.5f, 0.0f, -0.8660254f},
            {0.0f, 1.0f, 0.0f},
            rightAim)
            && terminal_math::ProjectAimToHudSurface(
                identityOrientation,
                rightAim,
                true,
                70.0f,
                1.5f,
                1.6f,
                16.0f / 9.0f,
                terminalPointer)
            && terminalPointer.x > 0.90f
            && Near(terminalPointer.y, 0.5f),
        "terminal HUD projection follows the cylinder arc instead of a mismatched flat FOV");

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
        comfort_math::ShouldBlackoutAuthoredCameraTransition(false, true)
            && comfort_math::ShouldBlackoutAuthoredCameraTransition(true, false)
            && !comfort_math::ShouldBlackoutAuthoredCameraTransition(false, false)
            && !comfort_math::ShouldBlackoutAuthoredCameraTransition(true, true),
        "authored camera ownership changes request comfort blackout");
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
    const camera_math::Quaternion grabTarget =
        grab_math::ResolveRelativeOrientationTarget(
            {},
            {0.0f, kHalfSqrtTwo, 0.0f, kHalfSqrtTwo},
            {});
    const camera_math::Vector3 grabTargetVelocity =
        ResolveAngularTargetVelocity(
            {},
            grabTarget,
            100.0f,
            1.0f,
            6.0f);
    failures += Check(
        Near(grabTargetVelocity.x, 0.0f)
            && Near(grabTargetVelocity.y, 6.0f)
            && Near(grabTargetVelocity.z, 0.0f),
        "grab absolute orientation target follows unrestricted controller delta");
    const camera_math::Vector3 clampedGrabError =
        grab_math::ClampVectorMagnitude({9.0f, 12.0f, 0.0f}, 3.0f);
    failures += Check(
        Near(clampedGrabError.x, 1.8f)
            && Near(clampedGrabError.y, 2.4f)
            && Near(clampedGrabError.z, 0.0f),
        "grab angular PID error is magnitude bounded");
    const camera_math::Vector3 grabCorrection =
        grab_math::ResolveGrabPositionCorrection(
            {1.2f, 0.0f, 0.0f},
            {0.6f, 0.0f, 0.0f},
            1.0f,
            0.5f);
    failures += Check(
        Near(grabCorrection.x, 1.7f)
            && Near(grabCorrection.y, 0.0f)
            && Near(grabCorrection.z, 0.0f),
        "grab pull-in is preserved while controller travel alone is bounded");
    failures += Check(
        Near(grab_math::ResolveSlideTargetSpeed(
                 0.0f, 0.20f, 0.02f, 1.0f, 10.0f, 2.5f),
            1.8f)
            && Near(grab_math::ResolveSlideTargetSpeed(
                        1.0f, 0.40f, 0.0f, 1.0f, 10.0f, 2.5f),
                2.5f),
        "slide target catches body up to hand displacement and respects speed cap");

    using grab_math::ResolveHingeAngularVelocity;
    failures += Check(
        Near(ResolveHingeAngularVelocity(
                 {}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f},
                 {0.0f, 1.0f, 0.0f}, 1.0f, 4.0f),
            1.0f)
            && Near(ResolveHingeAngularVelocity(
                        {}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f},
                        {0.0f, 1.0f, 0.0f}, 1.0f, 4.0f),
                -1.0f),
        "hinge velocity follows signed controller arc around joint pin");
    failures += Check(
        Near(ResolveHingeAngularVelocity(
                 {}, {1.0f, 2.0f, 0.0f}, {0.0f, 3.0f, -10.0f},
                 {0.0f, 2.0f, 0.0f}, 1.0f, 4.0f),
            4.0f),
        "hinge velocity ignores axial offsets and clamps angular speed");
    failures += Check(
        Near(ResolveHingeAngularVelocity(
                 {}, {1.0f, 0.0f, 0.0f}, {2.0f, 3.0f, 0.0f},
                 {0.0f, 1.0f, 0.0f}, 1.0f, 4.0f),
            0.0f)
            && Near(ResolveHingeAngularVelocity(
                        {}, {}, {0.0f, 0.0f, -1.0f},
                        {0.0f, 1.0f, 0.0f}, 1.0f, 4.0f),
                0.0f),
        "hinge velocity rejects radial motion and degenerate radius");
    failures += Check(
        Near(grab_math::CombineHingeAngularVelocity(
                 0.5f,
                 {0.0f, 2.0f, 0.0f},
                 {0.0f, 1.0f, 0.0f},
                 1.0f,
                 4.0f),
            2.5f)
            && Near(grab_math::CombineHingeAngularVelocity(
                        3.0f,
                        {0.0f, 3.0f, 0.0f},
                        {0.0f, 2.0f, 0.0f},
                        1.0f,
                        4.0f),
                4.0f),
        "hinge wrist twist projects onto the native pin and shares the speed cap");
    failures += Check(
        Near(grab_math::ResolveThrowVelocityScale(0.5f, 0.25f, 2.0f, true), 1.0f)
            && Near(grab_math::ResolveThrowVelocityScale(3.0f, 0.25f, 2.0f, true), 1.5f)
            && Near(grab_math::ResolveThrowVelocityScale(8.0f, 0.25f, 2.0f, true), 2.0f),
        "controller throw scale preserves native strength and rewards faster throws");
    const camera_math::Vector3 safeRearwardThrow =
        grab_math::ResolveSafeThrowDirection(
            {0.0f, 0.0f, 1.0f},
            {0.0f, 0.0f, -1.0f},
            0.25f);
    const float safeRearwardDot = -safeRearwardThrow.z;
    const camera_math::Vector3 safeSideThrow =
        grab_math::ResolveSafeThrowDirection(
            {1.0f, 0.0f, 0.0f},
            {0.0f, 0.0f, -1.0f},
            0.25f);
    const float safeSideDot = -safeSideThrow.z;
    failures += Check(
        safeRearwardDot >= 0.249f
            && safeSideDot >= 0.249f
            && Near(std::sqrt(
                safeSideThrow.x * safeSideThrow.x
                + safeSideThrow.y * safeSideThrow.y
                + safeSideThrow.z * safeSideThrow.z), 1.0f),
        "controller throw direction keeps clearance from the player body");

    float bodyFollowYaw = 0.0f;
    const float bodyFollowStep = input_math::ComputeBodyFollowStepRadians(
        input_math::DegreesToRadians(60.0f), 10.0f, 20.0f, 500);
    failures += Check(
        input_math::ResolveHorizontalYaw(
            {0.0f, -0.5f, 0.0f, 0.8660254f}, bodyFollowYaw)
            && Near(bodyFollowYaw, input_math::DegreesToRadians(60.0f), 0.001f)
            && Near(bodyFollowStep, input_math::DegreesToRadians(10.0f), 0.001f)
            && Near(input_math::WrapRadians(input_math::DegreesToRadians(370.0f)),
                input_math::DegreesToRadians(10.0f), 0.001f),
        "physical body follow resolves horizontal HMD yaw and rate-limits toward release angle");

    std::array<float, 16> readNative{
        0.5f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.5f, -0.5f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
    std::array<float, 16> readPresented{};
    failures += Check(
        read_math::BuildReadPresentationMatrix(
            readNative, 2.0f, nullptr, readPresented)
            && Near(readPresented[0], 1.0f)
            && Near(readPresented[5], 2.0f)
            && Near(readPresented[10], 3.0f)
            && Near(readPresented[11], -0.5f),
        "read presentation multiplies authored per-axis scale while preserving pickup travel");
    const camera_math::Quaternion readYaw =
        read_math::ResolveRelativeOrientation(
            {},
            {0.0f, kHalfSqrtTwo, 0.0f, kHalfSqrtTwo},
            {});
    failures += Check(
        read_math::BuildReadPresentationMatrix(
            readNative, 2.0f, &readYaw, readPresented)
            && Near(readPresented[0], 0.0f)
            && Near(readPresented[2], 3.0f)
            && Near(readPresented[8], -1.0f)
            && Near(readPresented[11], -0.5f),
        "read presentation applies orientation while retaining authored scale proportions");
    camera_math::Vector3 readPresentationPosition{};
    failures += Check(
        read_math::ScaleCameraRelativePosition(
            {1.0f, 2.0f, 3.0f},
            {1.0f, 2.0f, 3.15f},
            2.0f,
            readPresentationPosition)
            && Near(readPresentationPosition.x, 1.0f)
            && Near(readPresentationPosition.y, 2.0f)
            && Near(readPresentationPosition.z, 3.3f),
        "read distance scales each current native position from the camera");
    failures += Check(
        read_math::ResolveLatchedCameraRelativePosition(
            {1.0f, 2.0f, 3.0f},
            {1.0f, 2.0f, 3.15f},
            {2.0f, 4.0f, 6.0f},
            2.0f,
            readPresentationPosition)
            && Near(readPresentationPosition.x, 2.0f)
            && Near(readPresentationPosition.y, 4.0f)
            && Near(readPresentationPosition.z, 6.3f),
        "read presentation follows camera translation from one latched native offset without recursive growth");
    camera_math::Quaternion extractedYaw{};
    failures += Check(
        camera_math::QuaternionFromRotationMatrix(
            camera_math::RotationMatrix(readYaw), extractedYaw)
            && Near(std::fabs(extractedYaw.y), kHalfSqrtTwo)
            && Near(std::fabs(extractedYaw.w), kHalfSqrtTwo),
        "rotation matrix round-trips through quaternion extraction");
    camera_math::Quaternion basisOrientation{};
    failures += Check(
        camera_math::QuaternionFromForwardUp(
            {0.0f, 0.0f, -1.0f},
            {0.0f, 1.0f, 0.0f},
            basisOrientation)
            && Near(basisOrientation.x, 0.0f)
            && Near(basisOrientation.y, 0.0f)
            && Near(basisOrientation.z, 0.0f)
            && Near(std::fabs(basisOrientation.w), 1.0f),
        "tracked forward/up basis resolves to world orientation");

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

    const float pitchHalfAngle = input_math::DegreesToRadians(15.0f);
    const float rollHalfAngle = input_math::DegreesToRadians(10.0f);
    const camera_math::Quaternion tiltedYaw = camera_math::Multiply(
        {0.0f, kHalfSqrtTwo, 0.0f, kHalfSqrtTwo},
        camera_math::Multiply(
            {std::sin(pitchHalfAngle), 0.0f, 0.0f, std::cos(pitchHalfAngle)},
            {0.0f, 0.0f, std::sin(rollHalfAngle), std::cos(rollHalfAngle)}));
    const camera_math::Quaternion yawOnly = camera_math::YawOnly(tiltedYaw);
    failures += Check(
        Near(yawOnly.x, 0.0f)
            && Near(yawOnly.z, 0.0f)
            && Near(std::fabs(yawOnly.y), kHalfSqrtTwo, 0.0001f)
            && Near(std::fabs(yawOnly.w), kHalfSqrtTwo, 0.0001f),
        "recenter calibration keeps yaw while discarding headset pitch and roll");

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

    using openxr_frame_pacing_math::Decision;
    failures += Check(
        openxr_frame_pacing_math::Decide(true, false, false) == Decision::Wait,
        "OpenXR pacing submits during initial focus acquisition");
    failures += Check(
        openxr_frame_pacing_math::Decide(true, true, true) == Decision::Wait,
        "OpenXR pacing submits while focused");
    failures += Check(
        openxr_frame_pacing_math::Decide(true, false, true) == Decision::SkipUntilFocused,
        "OpenXR pacing skips untimed waits after focus loss");
    failures += Check(
        openxr_frame_pacing_math::Decide(false, false, true) == Decision::Wait,
        "OpenXR pacing leaves stopped sessions to lifecycle ownership");

    crash_capture::Policy crashPolicy(3);
    failures += Check(
        crashPolicy.Begin(0xC0000005u, 0x1000u) == crash_capture::Decision::Capture,
        "crash policy accepts the first fault");
    failures += Check(
        crashPolicy.Begin(0xC0000005u, 0x2000u) == crash_capture::Decision::Busy,
        "crash policy rejects reentrant capture");
    crashPolicy.End();
    failures += Check(
        crashPolicy.Begin(0xC0000005u, 0x1000u) == crash_capture::Decision::Duplicate,
        "crash policy suppresses a repeated fault address");
    failures += Check(
        crashPolicy.Begin(0xC0000005u, 0x2000u) == crash_capture::Decision::Capture,
        "crash policy accepts a distinct second fault");
    crashPolicy.End();
    failures += Check(
        crashPolicy.Begin(0xC000001Du, 0x3000u) == crash_capture::Decision::Capture,
        "crash policy accepts a distinct third fault");
    crashPolicy.End();
    failures += Check(
        crashPolicy.Begin(0xC0000005u, 0x4000u) == crash_capture::Decision::LimitReached
            && crashPolicy.Attempts() == 3,
        "crash policy caps dump attempts per process");

    if (failures == 0) {
        std::cout << "Render math tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
