# Next Live Evidence

Date: 2026-09-10

Use `out\SOMAVR-latest`
(`0.96.2-hands-bootstrap`) for the next run. The prepared rolling test
package has `DepthCompositionProbe=1` and `DepthCompositionSubmit=0`; no manual
configuration change is needed. Do not enable depth submission for this test.

## First: Arms Before The Vial

1. Load a save BEFORE picking up the medicine. Enter normal gameplay, then F10.
   Within a couple of seconds, check that full-size arms appear and track without
   first touching the vial. Long splash/save loading is fine; it does not start
   the creation countdown.
2. Pick up the vial, remove cap/drink, then test a physics object and laptop.
   Check no duplicate hands, unwanted bottle, lost tracking, or shoulder jump.
3. Pause/resume, reload the pre-vial save, and toggle F10 off/on. Exit normally.
   Keep your original save; don't overwrite it for this lifecycle test yet.
4. Attach `out/SOMAVR-latest/logs/somavr.log`. Bootstrap counters and blocked-gate
   reasons are automatic. Ctrl+F10 is useful only for a visible rig problem.

Rollback: `HandBootstrap=0` in the package INI preserves IK/retention but lets
the game create the model through the vial as before. Details: `HANDS_BOOTSTRAP_RE.md`.

## Then: Joint Follow-Through And Throws

1. Same drawer: slowly open and close without stick movement; repeat with a mild
   diagonal pull. Pause halfway, then reverse. Check for late-travel slowdown or drift.
2. Same door: continuous open/close with a loose hand arc, then reverse halfway.
   Briefly recheck curtains. Native stops remain enforced.
3. Same light prop: gentle release should place/drop; release the trigger while
   throwing should now use native Throw. Compare with A, then the other hand.
   Report player kickback and whether either release route behaves differently.
4. Exit normally and attach the log. No hotkey is needed for these new diagnostics;
   they record hand/joint motion and whether a throw was actually applied or timed out.

Full evidence and decision rules: `INTERACTION_FOLLOW_REVIEW_2026-09-09.md`.

## Also: Neutral Hands And Inspection Size

The September 9 profile tuning is retained in 0.96.1. Relaunch to load it;
no live settings reload or DLL replacement is performed.

1. Hold each controller naturally in front of you. Fingers should point roughly
   forward instead of toward the ceiling; palm roll should look unchanged.
   Wrist pitch is now `-45` instead of `45`, a 90-degree downward correction.
2. Pick up the same photo/book. It should be twice the physical mesh size and
   20% closer than the previous run. Test grip rotation, A/back, and another
   object; report near clipping or any renewed rising animation.
3. Confirm both results after one snap turn and save reload. The arm/depth
   captures below remain enabled. This profile's appearance is not yet accepted.

## Next Test: Shared Arm Goals And Scene Depth

1. Load the medicine/hands save and press F10. Keep your head still; move one
   hand at a time at close, comfortable and maximum reach. Slowly roll each
   wrist. Report hand/controller separation, stretching or opposite-hand motion.
2. Repeat after a 90-degree snap turn and a physical body turn. Walk while
   moving both hands; press Ctrl+F10 and continue for two seconds. This retains
   the arm RGB pair/palette witness as well as any valid depth pair.
3. Face a nearby chair/table against a distant wall. Press Ctrl+F10, hold still
   for two seconds, then lean sideways and capture again. Include the entire
   `eye-captures` folder: new `_depth.pfm` and `_depth.json` files accompany RGB.
   Missing depth files plus a `skipped` log is useful evidence, not a reason to
   enable another mode. The callback/attachment may need a different boundary.
4. Reload the save, repeat one reach and one Ctrl+F10 capture, then briefly check
   a drawer, story object and laptop for regression. Exit normally.

Attach `out/SOMAVR-latest/logs/somavr.log`, `eye-captures` and any
`terminal-captures`. Ignore the synchronous capture hitch when judging normal
latency. Expected logs: `hpl_wrist_goal ... ikSolved=1` with small
`residualMeters`, `hpl_scene_depth_callback ... eligible=1`,
`openxr_scene_depth_hook ... eligible=1 captured=1`,
`openxr_scene_depth_capture ... result=accepted`, `openxr_scene_depth_pair ...
matched=1`, and `openxr_scene_depth_dump ... written=1 invalidPixels=0`.
Near/far pixel variation and image alignment are checked offline, not inferred
from `accepted`. `depth=0` at OpenXR submission is intentional.
The callback row also logs rejected viewport/loading/frustum/projection gates;
include those rows even when no depth image is produced.

Unreachable controllers may leave the modeled wrist at the arm's reach boundary;
the new log exposes this clamp rather than stretching the mesh afterward. No
distributed forearm-twist fix is claimed by this build.

## Previous Test: Player-Space Recovery (0.95.9)

1. Load the medicine/hands save and press F10. Inspect both wrists palms up and
   down, snap left twice/right twice, then turn physically. Shoulders and elbow
   poles must follow the body without reversing. Walk and sweep the hands across
   the chest; press Ctrl+F10 once while moving, and continue for two seconds.
2. Inspect the same story object facing two different directions, then snap
   while it is open. It must stay in front at the same distance, without a rising
   loop or blur. Grip rotation and A/back must still work.
3. Open an email. Capture once looking toward the real laptop and once looking
   away while the overlay is still active. Allow two seconds after each chord.
   Compare email body, list clipping and cursor; test look-away exit.
4. Open/close the drawer and door slowly, then at normal speed; try a slightly
   diagonal pull. Report overshoot, oscillation or too much resistance.
5. Reload the save while VR is active, then walk/snap again. Same-frame stereo
   must remain enabled automatically. Exit normally.

Attach `out/SOMAVR-latest/logs/somavr.log` and both `eye-captures` and
`terminal-captures` folders beneath it. Ctrl+F10 now captures two left/right
scene-image pairs as well as terminal evidence when active. It is synchronous:
ignore the capture hitch when judging normal latency. Eye images exclude XR
quad overlays, so the separate terminal captures remain necessary.

Expected new receipts: `stereo_eye_dump ... leftWritten=1 rightWritten=1`,
`samePose=1` for same-frame captures, `hpl_wrist_orientation_seed ...
pitchDegrees=45.00`, `terminal_scissor_remap ... originalIntersects=1`, and
continued `hpl_dual_render ... same_pose_opposite_eye` after reloading. Missing
`originalIntersects=1` means the new terminal overlap case was not exercised,
not that the content is fixed. The native/palette witness remains enabled.

## Retained Diagnostic Reference

The earlier test targeted the shared comfort installer and stereo-pair
causes from the 2026-09-03 log are closed. Version 0.95.6 also makes snap-turn
torso ownership immediate, scales terminal scissors, and moves Read objects 25%
closer. Version 0.95.8 adds the final deform palette at mesh `+0x340` to the
read-only first-eye/replay-eye witness over the shared arm root and both
clavicle-to-wrist chains. Hand scale, wrist calibration, two-bone geometry,
ordinary locomotion, and world-manipulation policy are otherwise unchanged.

1. Launch the rolling package, reach the opening medicine/hands state, and
   press F10 once. Walk for ten seconds: head bob must remain absent and both
   eyes must animate arms/world together. While walking, sweep both hands across
   the body and overhead for roughly ten seconds; this crosses several sampled
   arm pairs. Snap left twice and right twice; each
   snap must rotate the shoulder rig immediately. Then turn physically through
   the 45-degree threshold to confirm the separate gentle follow path.
2. Enter the laptop and aim at the center, corners, email list, and email body.
   The cursor must meet the active controller beam at each point. Look directly
   at the physical laptop, then 30-60 degrees away while keeping the overlay in
   view. The overlay should remain equally solid with no view-dependent content flash.
   Verify clicking and look-away exit still work.
3. Pick up one story/Read object. It should appear immediately without rising
   from below or enabling the full-screen blur. The 1.5x distance should be 25%
   closer than 0.95.5 while preserving native apparent scale. Let it settle,
   rotate it with grip, dismiss it, and repeat once.
4. Regress locomotion, a curtain or drawer, physics pickup, pause/menu, desktop
   mirror, and F10 stop/restart. Exit normally and attach the complete log.

```text
build_identity identity=0.95.9-player-space-recovery+...
runtime_paths ... source=module
config_applied ... mtime=... bytes=... parsedKeyHash=... accepted=... unknownKeys=0 unknownSections=0
openxr_api_layers available=... policy=report_only
openxr_api_layer name="..." ...
hook_config_diagnostics ...
hook_config_camera ...
hook_config_render ...
hook_config_openxr ...
controller_config_locomotion ...
controller_config_routing ...
controller_config_interaction ...
controller_config_hands ...
controller_config_presentation ...
openxr_gl_transfer ... phaseOrder=total/acquire/wait/copyCpu/flush/release gpuOrder=capture/submit ...
openxr_pacing ... waitUs=last/avg/max beginUs=last/avg/max endUs=last/avg/max ...
openxr_freshness ... pairCompletions=... freshStereo=... heldStereo=... blackProjection=... fallbackProjection=... incompleteStereo=... freshPairRateHz=... freshSubmitPercent=...
openxr_freshness_summary reason=shutdown ... focusSkips=... failures=... heldAgeFrames=latest/max ...
openxr_frame ok ... locateCalls=1 locateMaxPerFrame=1 ... stereoPoseGap=0 ...
hpl_arm_ik ... crossMagnitude=... singularityBlend=... crossFallback=... policy=...cross_product_elbow...
hpl_arm_render_pair ... input={...} palette={available=... count=.../... firstUpdate=... interPass=... replayUpdate=... outputMismatch=... maxOutputDelta=...} result=same_pose_same_arm_inputs_same_skin_palette|skin_palette_diverged_between_eyes|same_pose_same_arm_inputs_palette_unavailable|arm_inputs_changed_between_eyes|eye_or_pose_pair_mismatch
hpl_arm_render_summary ... captureAttempts=... captures=... captureMisses=... pairComparisons=... coherentPairs=... inputMismatches=... inRenderMutations=... interPassMutations=... eyePoseMismatches=... palettePairComparisons=... paletteOutputMismatches=... paletteFirstPassUpdates=... paletteReplayPassUpdates=... paletteInterPassMutations=... paletteUnavailable=...
hpl_physical_body_follow anchor ... relativeHeadYawDegrees=... nativeYawDegrees=... worldHeadYawDegrees=... anchorYawDegrees=... policy=native_body_plus_tracking_space_head_yaw
hpl_arm_hierarchy ... parentIndex=... role=... releasedAssetWeightsKnown=1 runtimeSkinWeightsObserved=0 ...
hpl_arm_root_pose_seed ... incomingTranslation=0.0000,0.6644,-0.0431 anchorTranslation=0.0000,0.0000,-0.0002 authoredCorrection=1
hpl_arm_body_summary ... sharedRootAuthoredSeedCorrections=1 ... wristRotation={... controllerBasisFallbacks=0 nativeBasisFallbacks={nonFiniteOrDegenerate=0 nonOrthogonal=0 improperHandedness=0 quaternion=0}} ...
hpl_terminal_pointer ... route=overlay_surface_ray... relative=... size=...
terminal_scissor_remap ... sourceViewport=... captureViewport=... scissor=... remapped=... policy=affine_source_viewport_clip_to_private_capture
hpl_comfort_bridge install_ok partial=0 cameraAdd={requested=1 installed=1 ...} ... depthOfField={requested=1 installed=1 ...}
hpl_dual_render ... result=expected_pair_abort_retry_next_frame ...
hpl_physical_body_follow snap_commit ... policy=immediate_native_snap_no_slow_follow
hpl_read_presentation ... settled=0 entryOverride=1 viewAnchor=... policy=latched_distance_current_scene_view_settling_native_orientation_non_recursive_controller_orientation
openxr_runtime suspended reason=f10_vr_mode_disabled ...
hpl_vr_mode disabled ... runtimeSuspended=1
proof_summary ... openxrFrameLockWaitMaxUs=... openxrFrameLockHoldMaxUs=...
proof_summary ... openxrWaitAvgUs=... openxrBeginAvgUs=... openxrEndAvgUs=... openxrFreshPairRateHz=... openxrFreshSubmitPercent=...
proof_summary ... openxrSnapshotLockWaitMaxUs=... openxrSnapshotLockWaitOver100Us=...
proof_summary ... openxrGlLeftGpuCaptureAvgUs=... openxrGlLeftGpuSubmitAvgUs=...
proof_summary ... openxrGlRightGpuCaptureAvgUs=... openxrGlRightGpuSubmitAvgUs=...
proof_summary ... openxrSessionStateTransitions=... openxrFocusGainEvents=... openxrFocusLossEvents=...
proof_summary ... openxrInteractionProfileEvents=... openxrInstanceLossEvents=...
proof_summary ... openxrReferenceSpaceCreateAttempts=... openxrReferenceSpaceCreateSuccesses=... openxrReferenceSpaceCreateFailures=0
hpl_camera_bridge summary ... pairBaseLatches=... pairBaseReplays=... pairBaseMissingRejects=0 pairBaseStaleRejects=0 pairBaseFrustumRejects=0 pairViewLatches=... pairViewReplays=... pairViewRejects=0 ... nativeMemoryReadFailures=0 nativeMemoryWriteFailures=0
hpl_dual_render_cost_summary ... averageDraws=... averageUs=... usPer1000Draws=...
proof_summary ... ownGlBypasses=...
```

The arm correction is expected exactly once for the live-proven medicine root;
repeated correction or correction of another translation is a failure. A
missing correction accompanied by a neutral incoming root is acceptable. Any
terminal pointer fallback, loss of clicks, transparent holes in other overlays,
recursive story-object distance growth, native-memory failure, zero-layer
submit, stereo pose gap, or failed second-F10 recovery is a specific follow-up
target rather than a reason to guess at architecture.

Interpret the arm witness before changing IK again. Repeated
`result=same_pose_same_arm_inputs_same_skin_palette` with opposite eyes and one
pose frame means both node inputs and the final deform palette are pair-coherent;
visible left/right disagreement then belongs downstream in HPL's submesh CPU
skin or dynamic-VBO consumption. `skin_palette_diverged_between_eyes` localises
the fault to render-time bone/palette update. `arm_inputs_changed_between_eyes`
localises a real CPU-side pass-boundary change. An eye/pose mismatch belongs to
stereo transaction ownership, while palette unavailability or capture misses
mean retained mesh/arm identity was incomplete at the sampled boundary.

The second pass is optional and explicitly diagnostic. In a package-local copy
of `somavr.ini`, change only `HandTrackingProbe=1`, leave the packaged
same-frame/dual-render values unchanged, load a save before and after the first
authored hands sequence, and preserve:

```text
hpl_user_module_bridge install_ok ... handsOwnerInstalled=1 ...
hpl_player_hands_module owner_acquired ... moduleId=18 scriptObject=... callback=4 policy=read_only_owner_discovery
hpl_user_module_bridge_summary ... handsIdCandidates=... handsVtableMatches=... handsOwnerChanges=... handsScriptObjectReads=...
```

No visual or hand behavior should change. Missing owner acquisition after a
loaded gameplay map means the next RE rung is dispatcher/lifetime attribution;
it does not authorize a direct script call.

The packaged continuous replay still activates the 0.92 read-only resource
observers. The ordinary pass should therefore end with:

```text
hpl_occlusion_query_summary ... sameFrameQueryReuse=... anomalies={targetConflicts=... unmatchedEnds=... stateOverflows=...}
hpl_framebuffer_copy_summary ... crossEyeTextureReuses=... textureReadFailures=... stateOverflows=...
```

Do not change the stereo mode until the normal visual regression is complete.
Afterward, one F1 same-frame off/on comparison is useful if convenient; record
whether query/copy pass counts disappear and return with the control.

The secondary priority remains the 0.90 OpenXR GL transfer audit in
`TEST_CHECKLISTS.md`. Run the fixed 60-second apartment route under the current
`VirtualDesktopXR` runtime and attach the complete log. If practical, repeat
with SteamVR active using identical refresh rate, resolution, depth and scene
content. Preserve these new rows:

```text
build_identity identity=0.90.0-gl-transfer-audit+...
openxr_extensions ... khrOpenGL=1 khrD3D11=...
openxr_gl_interop_capability currentDc=1 extensionQuery=1 wglNvDxInterop2=... policy=diagnostic_only
openxr_gl_transfer ... backend=OpenGL ... projectionAvgUs=... projectionMaxUs=...
openxr_gl_transfer ... leftUs=... rightUs=... phaseOrder=total/acquire/wait/copyCpu/flush/release gpuTiming=excluded
openxr_gl_transfer budget_pressure ... (only when transfer reaches 25% of the frame period)
proof_summary ... openxrGlProjectionTransferAvgUs=... openxrGlLeftTransferFailures=... openxrGlRightTransferFailures=...
```

This is an evidence gate, not a D3D11 build. Ordinary visuals and interaction
must remain identical to 0.89.

For the SteamVR/VirtualDesktopXR comparison, use the `copyCpu` phase as the
renderer-to-swapchain transfer cost. `projectionUs` includes runtime-controlled
acquire/wait time and cannot by itself justify a D3D11 interop backend.
The two capability bits only decide whether a later interop experiment is
possible on that runtime/GPU pair. They do not constitute performance evidence.

The first priority is the 0.89 frame-contract and native-safety pass in
`TEST_CHECKLISTS.md`; the 0.88 release-integrity and 0.87 focus-pacing checks
remain part of the regression baseline.
Establish ordinary VR with F10, leave the already-focused headset idle for at
least 60 seconds, then return without pressing F10. The desktop game must stay
responsive and stereo/input must recover automatically. Preserve these rows:

```text
build_identity identity=0.89.0-frame-contract+...
runtime_paths ... source=module
config_loaded ... parsedKeyHash=... accepted=... unknownKeys=0 unknownSections=0
openxr_frame ok ... layers=... predictedDisplayTime=... upcomingRenderDisplayTime=... predictionLeadNs=...
openxr_projection fallback ... content=retained_or_black ...
hpl_stereo fill_committed ... eye=... nextEye=... policy=advance_only_after_cache_fill
hpl_camera_bridge summary ... fillCommits=... fillRejects=... pairRotationLatches=... pairRotationReuses=...
proof_summary ... ownGlBypasses=...
openxr_layer_budget ... (only if the runtime cap drops decoration)
openxr_focus_pacing armed ...
openxr_focus_pacing skip_begin ...
openxr_focus_pacing resumed ... skippedFrames=...
hpl_entity_identity ... profile={valid=1 ...}
hpl_entity_profiles save=1 ...
```

After normal shutdown, check `somavr_entity_profiles.ini` and launch once more
to prove `logs\somavr.previous.log` preservation. The profiles are telemetry-only
in 0.89, so hands, flashlight, story objects, and medicine must behave exactly
as in 0.86. Do not intentionally crash SOMA; crash capture has an automated
child-process integration test.

The priority pass is the 0.86 checklist. It tests delayed Read capture and hand
continuity, the requested wrist calibration, native-seeded hand wake, movement
during physical interactions, exact terminal surface coordinates, virtual
torso follow without camera rotation, and the softer controller guides.

Highest-value markers are:

```text
hpl_hand_calibration ... wristPitchDegrees=45.00 ...
hpl_hands_visibility wake=... policy=reactivate_native_created_player_hands...
hpl_read_presentation ... settleAge=... settleFrames=45 ...
hpl_native_locomotion_summary ... interactionMoveFrames=... interactionMoveCalls=...
hpl_terminal_surface_config ... mapping=exact_quad_or_cylinder_surface_intersection
hpl_physical_body_follow step=... policy=...virtual_torso_follow_no_camera_turn
```

During Read state 10 and terminal state 8 there should be no corresponding hand
suspension. The first Read override should begin only after the native settle
window and should name the actual story object, not `*Arm*`/`*Hand*` helpers.
Physical body-follow steps must never coincide with an HMD/world yaw change;
the shoulder bar alone should follow. Interaction movement counters should rise
only while testing Wheel/Slide/Door/Lever/Tear/MovingButton states.

The primary pass is now the 0.84 checklist. It tests the requested -90 degree
wrist calibration, delayed physical-yaw capsule follow, exact medicine prop
stabilization, non-destructive overlays, scene-terminated guides, one-shot Read
distance scaling, and terminal click retention. Preserve the complete log even
when the visuals pass.

Highest-value markers are:

```text
hpl_physical_body_follow entry=...
hpl_physical_body_follow step=...
hpl_socketed_prop_anchor ... name=Tracer_Fluid_HudObject
hpl_socketed_prop_stabilized ...
hpl_read_presentation ... configuredDistanceScale=2.000 scaleMultiplier=1.000
hpl_socketed_prop_summary ... beamRayHits=...
```

No `HudSuppressCenterCrosshair` alpha clear should run. The native HUD and email
panel must have no square hole. A semantic icon at controller depth is expected;
report any remaining gaze-centred duplicate separately rather than treating it
as permission to re-enable pixel clearing.

Prioritize the isolated torso-ergonomics poses in the 0.82 checklist before the
longer UI pass. The log should show near-body `shoulderReach.blend` near zero,
increasing blend only near extension, `elbowErgonomics.valid=1`, history after
the first solved frame, and nonzero singularity/swivel counters only in difficult
poses. A shoulder that visibly chases ordinary hand motion, an elbow that remains
lateral, or a body turn that leaves elbow history behind is a failed result even
when wrist residual remains exact.

The highest-value evidence is one apartment pass covering the eight reported
regressions. Follow the 0.82 checklist in `TEST_CHECKLISTS.md`; no diagnostic
hotkey is required. Keep the hands visible before, during, and after drinking,
then test a curtain/drawer, controller reticle, main menu, and one laptop email.

Expected positive markers:

```text
hpl_arm_pose_seed ... nodes=34 freezePose=1
hpl_arm_ik ... elbowDownMeters=0.100
hpl_hands_visibility ... bodyAnchor=1 trackedHeadAnchor=1
hpl_controller_gameplay_policy ... mainMenu=1 currentImGui=1
terminal_scissor_bypass ...
```

State 13 manipulation should no longer emit hand-suspension/reseed rows. The
terminal bypass count may be zero on non-email pages; it should rise when the
formerly black or fragmented email content is drawn under an impossible
offscreen scissor.

The next run should prioritize body-anchored arms, physical-state tracking,
and menu ownership. Terminal draw diagnostics now arm automatically:

1. Load the apartment save, press F10, and trigger/drink the medicine. Keep both
   hands visible for 30 seconds. Shoulders must remain at the initial corrected
   height and arm lengths must remain stable instead of jumping upward.
2. Face one direction, then rotate the player 90-180 degrees with smooth/snap
   turn and locomotion while moving the HMD independently. Shoulders and elbows
   must rotate with the player body; forearms must not stretch back toward the
   old world heading.
3. Keep the capsule still and lean the HMD left, right, forward, backward, up,
   and down. The shoulder centre should retain a fixed neck offset from the
   physical head instead of remaining at the capsule origin. Head yaw, pitch,
   and roll alone must not rotate the torso.
4. Pick up and hold a general physics object. Move and rotate both controllers,
   walk to another part of the room, then release it. Hands should continue
   tracking throughout Grab state and remain aligned after release.
5. Recenter once while deliberately looking 20-30 degrees up or down and with a
   little head roll. Position and yaw should reset, but the world horizon must
   remain level and the physical pitch/roll view must remain truthful.
6. Open pause and return to the main menu. Both should accept the controller
   pointer, and both menus should be visible in the desktop window.
7. Enter the laptop. Press right-controller A once, re-enter, then look at least
   65 degrees away. Both actions should fully leave terminal state without a
   physical mouse click; looking back must not resurrect the overlay.
8. Re-enter and open an email that shows flashing rectangles. Leave it visible
   for four seconds; no diagnostic key is required. Press `Ctrl+F10` only if a
   fresh RGB/alpha image set is also desired. Exit normally and attach the log.
   Decisive rows are `terminal_draw_state auto_armed`, `terminal_draw_state`,
   `hpl_terminal_cancel`, `hpl_hands_visibility`, `hpl_arm_ik`, and
   `hpl_controller_gameplay_policy ... nativeCursor=1`.

The priority pass is persistent arms plus the medicine profile:

1. Press F10 and wait for both controllers. Trigger the medicine sequence and
   confirm both full-size hands and arms appear after SOMA first creates and
   seeds its native hand model. The decisive ordering is
   `hpl_entity_identity ... name=PlayerHands_0 playerHands=1`, then
   `hpl_hands_native_seed`, then scale/wrist/IK rows.
2. Move one hand at a time through low, high, crossed, close-to-chest, and
   near-full-extension poses. Look for shoulder stability, natural elbow bend,
   no locked elbow, no miniature duplicate, and no one-hand startup blocking.
3. Let the native hand animation end. Walk, turn, crouch slightly, and move
   both hands. Confirm the arms remain visible and follow the player. Then
   trigger any non-Normal/authored sequence available and confirm it remains
   native rather than fighting the camera.
4. During the medicine sequence, bring the left controller beside the bottle
   cap and press left trigger or grip once. Then bring the bottle neck to your
   mouth and tip it for roughly a quarter second. Haptics should mark both
   recognized gestures; native medicine progression is not replaced yet.
5. Exit normally and attach `somavr.log`. The decisive rows are `hpl_arm_ik`,
   `hpl_hands_visibility`, and `hpl_authored_interaction profile=medicine`.

Report which local bottle axis points toward the cap if visually obvious. The
log's `capDistance`, `mouthDistance`, and `tipDegrees` will otherwise let the
next build calibrate it. Immediate rollback switches are `HandArmIK=0`,
`HandAlwaysVisible=0`, or `MedicineInteraction=0` independently.

After the dedicated hand pass is accepted, leave one laptop message still for
five seconds. The expected current
route is `hpl_terminal_retention_fallback ... action=live_frame_capture` after
eight completed retained samples because the prior log saw no nested clear.
Confirm the panel no longer stays entirely black, then test pointer highlights,
clicks, and look-away exit. Press `Ctrl+F10` once while incomplete content is
visible; it captures native retained and final upscaled surfaces for the final
compositor design.

The 0.76 position prototype is live-proven. Scale normalization and bilateral
wrist takeover both succeeded with exact post-state restoration. The brief
miniature interval is now classified as controller-startup gating: the right
Touch profile appeared five seconds before the left, and the current code waits
for both grips before it normalizes even the shared root scale. The next hands
change should split scale eligibility from pose eligibility and resolve wrists
per hand. Controller orientation and arm IK remain out of scope for that
refinement.

The next acceptance should trigger the same hand animation and first check that
the root becomes life-sized as soon as VR activates, then that each wrist begins
following independently as its controller becomes available.
Move one controller at a time, cross them, separate them widely, walk and turn,
briefly interrupt tracking, then exit and retrigger the animation. Native wrist
rotation is expected in this build; do not score controller rotation yet.
Require paired `hpl_wrist_position` rows with `restored=1`, increasing
`wristFramesApplied`, no authored-post/read/math fallbacks, and no recurrence of
the bilateral right-controller attachment. Roll back scale alone with
`HandScaleNormalization=0`, wrist position alone with `HandWristPosition=0`,
or both for the accepted passive behavior.

SOMAVR has reached the point where the feature registry contains no unimplemented
VR system with enough static evidence for another responsible native mutation.
The remaining gates require headset, campaign-state, controller, runtime, or GPU
evidence. Run these passes in order; each pass is designed to settle several
project-phase rows from one log.

## Pass A: 0.56 Authored Camera Handoffs

Use the packaged `0.56.0-authored-camera-handoff` build, load a normal save, and
press F10 once. Exercise a sit sequence, ladder or climb, and one interactive
camera animation or hand-attached sequence.

Required evidence:

- rigid normal stereo and established eye height before each transition;
- `authored_camera_ownership_changed` preserving `tracking=1 stereo=1`;
- increasing `calibrationGeneration`, followed by fresh base/history captures;
- one `hpl_player_state_comfort` ownership transition and one blackout request;
- gameplay input released during authored ownership and restored afterward;
- no stale AO, ImageTrail, exposure, shadow, reflection, or FOV state.

This pass directly advances phases 8.1 through 8.4 and validates the generic
handoff used by sit, climb, conversation, animation, and hand attachment.

## Pass B: Temporal And Same-Frame Stereo

Enable same-frame stereo from F1. Visit one dark-to-bright route, one reflective
and shadowed room, one authored fade or grading transition, and any scene that
activates ImageTrail. Repeat once in AFR.

Required evidence:

- previous-view restore/capture pairs agree on eye, pose, and generation;
- ToneMapping replay and committed-restore counters rise together;
- temporal SSAO restore/commit and phase replay counters rise without faults;
- ImageTrail allocates distinct eye resources only when the effect activates;
- no binocular rivalry, stale-eye flash, doubled animation speed, or regression
  in the proven centered shadow/reflection path;
- representative per-eye CPU and GPU timing rows for AFR and same-frame modes.

This pass is the live gate for phase 2.13 and the promotion decision for
same-frame stereo. Static shader/resource RE is complete unless the log reveals
a new mutable owner.

## Pass C: Gameplay VR Systems

Exercise walking at partial/full stick, smooth and snap turn, physical crouch,
room-scale near a wall and moving door, interaction focus, one/two-hand grab,
wheel/door/lever manipulation, flashlight, terminal, inventory, pause, death,
and one tracked tool.

Required evidence:

- correct interaction profiles and two fresh controller pose streams;
- native analog locomotion only in Normal/Normal ownership, with semantic
  fallback elsewhere;
- no capsule tunnelling or body catch-up while authored camera ownership is set;
- controller ray hit depth/semantic icon agreement and stable reticle depth;
- native physics remains authoritative for grab, torque, throw, and mechanisms;
- light/hard grabbed-object impacts produce speed-scaled initiating-hand pulses,
  duplicate material callbacks are cooled down, and unrelated impacts stay
  silent; preserve native impact sound, particles, collision, and gamepad rumble;
- HUD/current-ImGui ownership is limited to its confirmed semantic surfaces;
- listener orientation follows the HMD during a directional near-field source.

This pass covers the remaining locomotion, hands, HUD, terminal, presentation,
audio, haptics, and accessibility acceptance rows.

## Pass D: Lifecycle And Hardware

Run doctor, launch, load another map/save, pause/unfocus, briefly interrupt HMD
tracking, change a graphics/window setting, and exit normally. When practical,
repeat on another OpenXR runtime or headset.

Required evidence:

- resource recreation or recovery without process restart;
- bounded tracking-loss retained/black projection path and one recovery blackout;
- no stale camera, renderer, history, GL context, or swapchain identity;
- clean lifecycle summaries and no lingering SOMA process;
- stable desktop spectator output and no graphics-proxy conflict;
- runtime/headset/GPU/Windows identifiers recorded with the full log.

This pass gates release regression, runtime recovery, graphics resize, hardware
matrix, and release-candidate phases. `0.57.0` now provides opt-in fixed
foveation plus exact capability/application telemetry. Compare levels 0 through
3 only after collecting baseline GPU timing; upscaling remains unimplemented
until a measured bottleneck and compatible OpenGL ownership path justify it.
