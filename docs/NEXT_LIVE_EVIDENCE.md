# Next Live Evidence

Date: 2026-08-24

Use `out\SOMAVR-latest`
(`0.93.0-playbook-hardening`) for the next run.

The first priority is a normal/default regression pass. Version 0.93 does not
enable native same-frame stereo or the hands-owner probe in the release profile;
ordinary visuals and controls must match 0.91.

1. Launch the rolling package, load the apartment save, press F10 once, and
   verify the established world, hands/IK, locomotion, interactions, terminal,
   HUD/menu and desktop-mirror behavior.
2. Stand, turn, and walk for at least 30 seconds, then use the laptop and pause
   menu. This collects XR mutex and GPU transfer evidence under ordinary frame
   pacing. Reload the save once to cover fallback-black/cache recovery.
3. Exit normally and attach the complete log. Preserve these rows when present:

```text
build_identity identity=0.93.0-playbook-hardening+...
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
proof_summary ... openxrFrameLockWaitMaxUs=... openxrFrameLockHoldMaxUs=...
proof_summary ... openxrSnapshotLockWaitMaxUs=... openxrSnapshotLockWaitOver100Us=...
proof_summary ... openxrGlLeftGpuCaptureAvgUs=... openxrGlLeftGpuSubmitAvgUs=...
proof_summary ... openxrGlRightGpuCaptureAvgUs=... openxrGlRightGpuSubmitAvgUs=...
proof_summary ... openxrSessionStateTransitions=... openxrFocusGainEvents=... openxrFocusLossEvents=...
proof_summary ... openxrInteractionProfileEvents=... openxrInstanceLossEvents=...
proof_summary ... openxrReferenceSpaceCreateAttempts=... openxrReferenceSpaceCreateSuccesses=... openxrReferenceSpaceCreateFailures=0
hpl_camera_bridge summary ... nativeMemoryReadFailures=0 nativeMemoryWriteFailures=0
proof_summary ... ownGlBypasses=...
```

Any native-memory failure, sustained snapshot lock wait above 100 us, busy GPU
query ring, zero-layer submit, persistent eye skew, or second-F10 recovery is a
specific follow-up target rather than a reason to guess at architecture.

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
