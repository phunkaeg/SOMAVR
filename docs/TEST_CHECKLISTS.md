# Test Checklists

## 0.92.0 Native-Stereo Evidence

1. Launch `out\SOMAVR-latest`, load the apartment save, and press F10 once.
   Require `version=0.92.0-native-stereo-evidence`, package-local runtime/config
   paths, zero unknown config keys/sections, and nine ownership-scoped startup
   config rows with no truncation marker.
2. Run ordinary AFR for 30 seconds while standing, turning, and walking. Confirm
   the 0.91 world rigidity, stereo, hands/IK, locomotion, interactions, terminal,
   HUD/menu, reticles, guides, and desktop mirror have no visible regression.
3. Reload the save once, use the laptop, open/close pause, and exit normally.
   Require periodic `openxr_gl_transfer` rows with nonzero GPU timestamp samples
   when the GL API is available, zero invalid samples, and no sustained ring
   drops. Interpret GPU order as `capture/submit` independently of CPU phases.
4. Require `nativeMemoryReadFailures=0` and `nativeMemoryWriteFailures=0` in the
   camera summary. Any failure must disable only the affected operation and must
   not crash SOMA.
5. Record frame-lock wait/hold maximums and snapshot-lock waits. Repeated
   `openxrSnapshotLockWaitOver100Us` or material frame-lock contention is the
   gate for splitting XR state/submit locking; do not infer contention from a
   single startup outlier.
6. With release defaults, require bounded `hpl_occlusion_query_summary` and
   `hpl_framebuffer_copy_summary` rows after continuous replay. Record
   same-frame reuse counts; require zero target conflicts, unmatched ends,
   texture-read failures, and state overflows, with no visual behavior change.
7. Optional hands-owner pass: set only `HandTrackingProbe=1`, load gameplay,
   and require `owner_acquired ... moduleId=18 ... callback=4` plus matching
   summary counters. No script call, hand creation, or visibility change should
   occur.
8. Optional comparison after ordinary acceptance: toggle same-frame stereo off
   and on once through the F1 panel. Preserve query/copy summaries, CPU/GPU
   timings, and visual observations so replay-owned events can be separated
   from ordinary AFR traffic.

## 0.91.0 Review Hardening

1. Launch `out\SOMAVR-latest`, load the apartment save, and press F10 once.
   Require `version=0.91.0-review-hardening`, `runtime_paths ... source=module`,
   and `config_applied` with package-local path, mtime, byte size, nonzero hash,
   `unknownKeys=0`, and `unknownSections=0`.
2. Confirm the accepted world, stereo, hand/arm tracking, locomotion, physical
   interactions, story-object presentation, terminal emails, HUD/menu and
   desktop mirror have no visible regression.
3. Reload the save or cross one loading boundary. If practical, briefly remove
   and restore HMD tracking. A held pair must remain world-locked and recover
   without F10. `openxr_stereo_hold active` must use
   `policy=resubmit_last_pair_with_its_own_poses`, release promptly, and fall
   back to black after its 12-frame budget rather than hold forever.
4. Transient `hpl_stereo apply_failed` is allowed to affect one frame and must
   be followed by `apply_recovered`. Stereo may suspend only after eight
   consecutive failures; normal loading or one rejected projection must not
   permanently collapse the session to mono orientation.
5. Exercise laptop terminal, pause HUD, reticle, both guides, comfort vignette
   and a blackout/loading transition. Require no GL-state leak, email/HUD
   corruption, compositor freeze, zero-layer submit, or layer-limit stream.
6. Pick up and throw one ordinary physics prop. The shared live-patch lane must
   install/restore normally with no signature, expected-byte, instruction-
   pointer, peer-thread, or protection failure.
7. Exit through the pause menu and attach the complete log. Require a clean
   process exit and no crash dump. `proof_summary ... ownGlBypasses=` should be
   nonzero after the presentation exercise.
8. Treat `NATIVE_STEREO_FEASIBILITY.md` as the next experiment only. Native
   second-world-render stereo is not enabled by this package and is not part of
   the 0.91 acceptance result.

The GL transfer A/B below is still useful, but compare each eye's `copyCpu`
phase across identical runs. The wait-inclusive `projectionUs` total measures
runtime pacing as well as the copy and is not an interop decision metric by
itself.

## 0.90.0 OpenXR GL Transfer Audit

1. Launch the canonical package, load the apartment save and press F10. Confirm
   the 0.89 world, stereo, hands/IK, locomotion, interactions, terminal, HUD and
   desktop mirror have no visible regression.
2. Keep the headset refresh rate, resolution scale, depth setting and save
   fixed. Follow one repeatable 60-second route containing 20 seconds standing,
   20 seconds turning/walking, and 20 seconds using the laptop or pause HUD.
3. Exit normally and preserve the complete log. Require periodic
   `openxr_gl_transfer ... backend=OpenGL` rows, both eye attempt counts rising,
   success counts matching ordinary frames, and no repeating swapchain failure.
4. Read each `leftUs`/`rightUs` tuple using
   `total/acquire/wait/copyCpu/flush/release`. Note projection average/maximum
   and `budgetPressureFrames`; `gpuTiming=excluded` is intentional.
5. If SteamVR can be selected as the active OpenXR runtime, repeat the exact
   route and settings. Compare it with the current VirtualDesktopXR baseline.
   Do not infer a D3D11 win from different scene content or refresh rates.
6. Reload the save once so fallback-black traffic occurs. Its source should be
   tagged `black`, normal gameplay should return to `stereo_cache`, and there
   must be no compositor freeze or second F10.

The D3D11 interop prototype is warranted only if SteamVR repeatedly shows a
material transfer increase or budget pressure absent from VirtualDesktopXR.
See `OPENXR_GL_TRANSFER_RE.md` for phase interpretation and the fallback design.

## 0.89.0 Frame Contract And Native Safety

1. Launch the canonical package, load the apartment save, and press F10 once.
   Confirm world rigidity, hand/arm tracking, locomotion, interactions, terminal
   emails, pause/main-menu behavior and desktop mirror match the 0.88 baseline.
2. Stand still, then turn and pitch the HMD while walking. `openxr_frame ok`
   must report `upcomingRenderDisplayTime` one `predictionLeadNs` period after
   `predictedDisplayTime`. There must be no new inter-eye latency or skew.
3. Leave same-frame stereo off for at least 30 seconds. Require alternating
   `hpl_stereo fill_committed ... eye=0/1`, nonzero `pairRotationLatches` and
   `pairRotationReuses`, no repeating `fill_rejected`, and a rigid world while
   moving. Then enable same-frame stereo and verify the same fill sequence.
4. Reload the save or cross a loading boundary, then briefly occlude tracking.
   Every sampled `openxr_frame ok` must report `layers>=1`. Fallback rows may
   report retained or black projection, but no `xrEndFrame` zero-layer path,
   compositor freeze, stale eye, or second F10 may occur.
5. Exercise the laptop, pause HUD, reticle and both controller guides. At clean
   shutdown `proof_summary ... ownGlBypasses=` must be nonzero; terminal clear,
   reflection and post-effect counters must reflect game work only.
6. Pick up and throw one ordinary physics prop, then exit and relaunch once.
   Require `hpl_grab_bridge install_ok` with no `signature_not_unique`,
   `instruction_pointer_in_range`, `expected_bytes_changed`, or
   `impulse_restore_failed` rows. Throw behavior should match the prior build.
7. Exit through the pause menu and attach the complete `somavr.log`. Confirm no
   crash dump and a normal process exit.

## 0.88.0 Release Integrity And OpenXR Contract

1. Launch from `out/SOMAVR-latest` with the packaged injector and DLL. Confirm
   startup logs `runtime_paths ... source=module` and both the DLL and
   `config_loaded path=` resolve under that same package/install directory.
2. Confirm `config_loaded` reports `accepted` above 250 with
   `unknownKeys=0 unknownSections=0`, and records `lastWriteTicks` plus a
   nonzero `parsedKeyHash`.
3. Press F10, load the apartment save, and verify the established world,
   stereo, hands/IK, locomotion, interaction, terminal-email, HUD/menu and
   desktop-mirror behavior has no regression.
4. Cross a loading screen or reload the save. The HMD must show stable black
   rather than freeze. Sampled `openxr_frame` records during presentation
   blackout must report `layers>=1`; there must be no repeated
   `XR_ERROR_LAYER_LIMIT_EXCEEDED` or `openxr_swapchain *_wait_failed` stream.
5. Open the F1 panel while both aim guides, reticle, HUD and vignette can be
   active. At shutdown, capture the proof summary containing
   `openxrRuntimeMaxLayers`, `openxrMaxLayerCandidates`,
   `openxrMaxSubmittedLayers` and `openxrDroppedLayers`. Submitted maximum must
   be at most 16.
6. Briefly remove/occlude headset tracking, then restore it. The runtime should
   retain or black the projection and recover without a compositor freeze or a
   second F10 press.
7. Exit through the pause menu. Confirm clean process exit and no crash dump.

## 0.87.0 Reliability And Profile Evidence

1. Close SOMA and launch `out\SOMAVR-latest`. Load the normal apartment save,
   press F10 once, and confirm the 0.86 hands, arm IK, locomotion, story-object,
   terminal, pointer, and shader behavior has no visible regression. The first
   log block must contain `identity=0.87.0-reliability-profiles+...`, a full Git
   commit, dirty state, PE timestamp, and environment row.
2. Wait until the log/runtime has reached FOCUSED, then remove the headset or
   leave its proximity sensor idle long enough for the runtime to become
   VISIBLE. Leave it unfocused for at least 60 seconds. The desktop game must
   continue rendering/responding rather than gradually stalling.
3. Wear the headset again. Stereo, head tracking, hands, and controller input
   must resume without pressing F10. Repeat one shorter focus-loss cycle from
   the pause menu or laptop and confirm recovery again.
4. Exercise `PlayerHands_*`, the flashlight, one story/Read item, the medicine
   bottle, and the laptop if available. No calibration should change. Exit
   normally and preserve `hpl_entity_identity ... profile={...}` rows.
5. Confirm `somavr_entity_profiles.ini` exists after clean shutdown and contains
   only identities actually encountered. It is evidence data in this build;
   editing it must not alter gameplay yet.
6. Launch once more and exit normally. `logs\somavr.previous.log` must contain
   the prior complete run while `somavr.log` contains the new build identity.
   Do not deliberately crash the game; the real child-process minidump test is
   part of CTest.

Decisive focus rows are `openxr_focus_pacing armed`, one
`openxr_focus_pacing skip_begin`, and `openxr_focus_pacing resumed` with a
nonzero duration and skipped-frame count. `openxrWaitMaxUs` and
`openxrFocusPacingSkippedFrames` appear in the final OpenXR summary. A natural
crash should create one file under `logs\dumps` plus `somavr-crash.log`; full
memory is not expected unless `SOMAVR_FULLDUMP=1` was explicitly set.

## 0.86.0 Interaction Presence

1. Close SOMA and launch the rolling package. During splash/menu/save loading,
   leave the controllers untouched. Pick them up only after the apartment is
   playable, then press F10. Require `version=0.86.0-interaction-presence` and
   report whether full-size tracked hands wake before drinking the medicine.
2. Hold both controllers in a matching neutral pose. Check the requested
   calibration: each hand should be about 4 cm lower, 3 cm farther outward,
   4 cm nearer the camera, and pitched forward about 45 degrees relative to
   0.85. Move each controller through pitch/yaw/roll and check for symmetry.
3. Inspect the same story object twice. Allow its native rise animation to
   finish without moving the controller. It should settle in front of the view,
   not below it or facing the ceiling, and hands/IK must keep tracking in Read.
   Rotate with grip and exit with right A/B both times.
4. Grab and hold a curtain, drawer, and hinged door in turn. While still
   holding each mechanism, move with the left stick, then release it. Locomotion
   must continue without breaking the mechanism or moving during Read/terminal.
5. Open the laptop and aim at the center, all four edges, and several email
   controls. The larger visible pointer should coincide with the controller
   guide and the element that highlights/clicks. Browse emails for 20 seconds
   and verify both hands continue tracking.
6. Aim both guides at empty space and several interactable/non-interactable
   surfaces. They should look like faint translucent glows at rest, brighten
   over an interactable, and stop at scene depth rather than penetrate walls.
7. Face forward, physically look 30-40 degrees aside, then hold a 60-90 degree
   look for two seconds. Shoulders should gently follow only after the threshold
   while the world/HMD view remains perfectly still. Repeat with a snap turn to
   confirm the torso reference follows the explicit body turn.
8. Recheck curtains/drawers, normal movement, story-object exit, pause/main
   menu, email stability, save reload, and normal shutdown. Attach the complete
   log even if everything passes.

Decisive rows are `hpl_hand_calibration`, `hpl_hands_visibility wake`,
`hpl_read_presentation ... settleAge=... settleFrames=45`,
`hpl_native_locomotion_summary ... interactionMoveFrames=...`,
`hpl_terminal_surface_config`, and
`hpl_physical_body_follow ... policy=...no_camera_turn`. A missing wake row is
not itself failure if the hands were already natively active; use
`persistentHands.nativeSeeds/wakeRequests` in the summary to classify it.

## 0.85.0 Terminal Hands And Read Latch

1. Close SOMA, launch the rolling package, wait through the full splash/menu and
   save load, then press F10 once. Require
   `version=0.85.0-terminal-hands-read-latch` and confirm the hands remain full
   size with normal position and rotation tracking.
2. Enter the apartment laptop and keep both controllers moving while browsing
   several emails for at least 20 seconds. Both hands and arms must continue to
   track throughout terminal mode. Locomotion should remain disabled and the
   terminal pointer/click path should still work.
3. Leave one email open long enough to catch the previous intermittent flash.
   The panel should remain readable and stable. Look-away and controller exit
   must still detach normally.
4. Pick up `Notepad_open` or another story object. It should perform one native
   rise into view, settle at the configured farther distance, and remain there;
   it must not rise again every time HPL submits another matrix. Move/lean the
   HMD slightly and confirm the object follows as a stable camera-relative
   presentation rather than drifting away.
5. Rotate the story object with grip, put it down with the existing controller
   exit, then pick it up a second time. The second interaction must receive a
   fresh anchor and behave like the first.
6. Recheck one curtain or drawer, normal locomotion, pause menu, and shutdown,
   then attach the complete log.

Positive evidence is `hpl_read_session ... active=1`, one
`hpl_read_presentation ... anchorSeeded=1` per entity/session followed by stable
`sourceDistance` and `finalPos`, plus
`hpl_terminal_retention_stabilized ... offscreen_scissor_repair_observed`.
There should be no `hpl_hands_visibility suspended=1` on the transition to
`playerState=8`.

## 0.84.1 Hand Scale Retention Fix

1. Close SOMA, run doctor, launch the rolling package, wait through the main
   menu, load the apartment medicine save, and press F10 once. Require
   `version=0.84.1-hand-scale-retention-fix`.
2. Watch the native hands from the instant F10 is pressed. They may exist at
   SOMA's native miniature scale before VR activation, but must become full size
   as soon as VR camera tracking activates; controller discovery must not be a
   prerequisite.
3. Move and rotate both controllers. Hands, arms, shoulders, wrist roll, and
   medicine bottle behavior should otherwise match 0.84. Drink the medicine and
   verify the shoulder-height fix remains intact.
4. Exit normally and attach the full log. A stale pre-F10 seed should produce
   `hpl_hands_body_anchor_scale ... acceptedScale=1.0000
   staleRetainedScale=0.2500 finalScale=1.0000`. Subsequent `hpl_arm_ik` segment
   lengths must no longer remain at the failed `0.0699,0.0541` pair.

The independent rollback remains `HandScaleNormalization=0`, though this also
returns SOMA's intentionally miniature close-up rig and is diagnostic only.

## 0.84.0 Body Follow And Presentation

1. Close SOMA, run doctor, launch the rolling package, load the apartment save,
   and press F10 once. Require `version=0.84.0-body-follow-presentation`.
2. Hold both palms flat in a matching pose. They should now be rolled about 90
   degrees from 0.83 in the requested direction, with identical orientation
   after one save reload. Confirm fingers stay still.
3. Drink the medicine. Shoulders must remain at the accepted height and the
   bottle must follow the right hand without its previous autonomous rotation or
   jiggle. Preserve any `hpl_socketed_prop_anchor` row.
4. Face forward, then physically look 30-40 degrees left/right: the capsule and
   shoulders must not turn. Hold a 60-90 degree look for at least one second:
   they should begin following gently after a short pause. Pitch and roll must
   not trigger follow. Snap turn once and ensure follow does not immediately
   fight the turn.
5. Aim both guides at a wall, drawer, prop, and empty distance. Each guide should
   stop just in front of scene geometry rather than pass through it. The native
   semantic context icon should appear at the winning beam hit and be clearly
   readable. Record duplicate center icons separately; there must be no square
   cutout in any overlay.
6. Inspect at least two story objects of visibly different native sizes. Each
   should sit about twice the previous camera distance while retaining sensible
   relative scale. Rotate one with grip and exit with right A/B.
7. Open the laptop and click through several emails for 20 seconds. Require no
   center hole and no full-panel 2 Hz flash. The terminal pointer must remain
   aligned and look-away/A exit must still work.
8. Check curtains, one drawer, locomotion while holding a loose prop, pause/main
   menu, and normal shutdown for regressions. Attach the full log.

Decisive rows are `hpl_physical_body_follow`,
`hpl_socketed_prop_stabilized`, `hpl_read_presentation`, and the two subsystem
summary rows. Roll back independently with `PhysicalBodyFollow=0`,
`HandWristRollDegrees=0`, `HandSocketedPropStabilization=0`, or
`AimGuideSceneDepth=0`.

## 0.83.0 Shared Root, Palm Orientation, And Terminal Pointer

1. Run doctor and require `0.83.0-root-palm-terminal` with `fail=0`. Load the
   apartment save, press F10 once, and wait for both arms to track.
2. Before medicine, hold both controllers forward in a matching neutral pose.
   Check palm direction, thumb side, yaw, pitch, and roll. Reload the save once
   and repeat: the same physical controller orientation must produce the same
   visual hand orientation on both runs.
3. Drink the medicine. The shoulder bar must remain at its pre-medicine height
   while continuing to follow HMD translation and player-body turns. The log
   should contain `hpl_arm_root_pose_drift` with a Y delta near `0.66`, followed
   by stable `hpl_arm_ik` shoulder heights.
4. Rotate each controller independently through obvious yaw, pitch, and roll.
   Require `hpl_wrist_orientation_seed ... mode=geometric_palm` for both hands;
   `legacy_takeover_fallback` is diagnostic failure evidence to preserve.
5. Manipulate curtains and one drawer to ensure state-13 tracking and the proven
   interactions did not regress. Do not judge sensitivity in this pass beyond
   recording whether each feels too fast, too slow, laggy, or directionally
   wrong.
6. Enter the laptop. A blue terminal pointer should sit where the active motion
   controller aims on the overlay, including near all four edges. Click several
   email/UI targets and confirm the visible pointer and highlight/click agree.
   Repeat once with curved HUD and once with quad HUD if convenient.
7. Exit the terminal by look-away and controller A. The pointer must disappear
   immediately and the normal world interaction reticle must resume.
8. Exit normally and attach the complete log. Decisive summary fields are
   `sharedRootDriftCorrections`, `geometricSeeds`, `legacySeeds`,
   `openxrTerminalPointerUpdates`, `openxrTerminalPointerSubmittedFrames`, and
   `openxrTerminalPointerFailures`.

For the later manipulation calibration, keep SOMA running and perform one slow
10 cm drawer pull/push, one fast pull/push, and one slow door open/close while
the live debugger is attached. That isolates position gain, velocity scale, and
angular response without changing the now-working curtain path.

## 0.82.0 Torso Ergonomics

1. Run doctor and require `0.82.0-torso-ergonomics` with `fail=0`. Load the
   apartment save, press F10, and wait for both tracked arms.
2. Hold both hands still. Roll the HMD, then look 60-90 degrees left and right.
   The head must move immediately while the shoulder bar remains level and does
   not follow head-only yaw.
3. Turn the player using snap/smooth turn. Shoulders, elbows, and their remembered
   bend direction must rotate with the body without several frames of world-space
   lag.
4. Move each hand independently near the chest and waist. Wrists must remain
   exact and shoulders should stay effectively fixed. Then extend one arm fully
   forward/upward: only that shoulder may contribute subtly and smoothly, never
   more than about 5 cm. Retract and check for a smooth release without swimming.
5. Test relaxed hands, crossed hands, overhead hands, and one hand slightly
   behind its shoulder. Elbows should favour a lower natural bend, tuck when
   crossed, preserve continuity overhead, and never flip across the arm.
6. Repeat the medicine sequence. Require stable shoulder height, body-relative
   heading, controller wrist rotation, and no finger-animation takeover.
7. Manipulate the curtain and a drawer. State-13 tracking must remain intact and
   neither wrist nor elbow should reacquire a different offset after release.
8. Exit normally and attach the complete log. Decisive rows are `hpl_arm_ik`
   fields `shoulderReach`, `elbowErgonomics`, and the
   `hpl_arm_body_summary` ergonomic counters.

Rollback all new inference with `HandArmIKErgonomics=0`. Keep ergonomic elbows
but disable clavicle contribution with `HandShoulderReachCompensation=0`.
`HandShoulderReachStart`, `HandShoulderReachMaxMeters`,
`HandArmIKElbowDownMeters`, and `HandArmIKMaxSwivelDegreesPerFrame` tune the
remaining bounded behavior.

## 0.81.0 Stable Hands And UI Ownership

1. Run doctor and require `0.81.0-stable-hands-ui` with `fail=0`. Wait through
   the complete splash/main-menu period, load the apartment save, and press F10.
2. Before drinking, lean the HMD and rotate the player 90-180 degrees. The
   shoulder centre should follow HMD position, sit about 10 cm farther back,
   and rotate with capsule/body heading without following head-only yaw.
3. Drink the medicine. Shoulders and elbows must not jump upward. Fingers
   should keep their initial stable pose while the bottle remains attached to
   the correct native hand socket. Move both controllers through clear rotation
   and confirm the wrists continue tracking.
4. Check relaxed, crossed, chest-height, and extended hand poses. Elbows should
   favour a visibly lower pole instead of jutting horizontally outward.
5. Manipulate the curtain, a drawer, and a door. Hands must continue following
   both position and rotation throughout state 13 and must not acquire a new
   palm offset after release.
6. Aim either controller at several interactables. The semantic icon should
   appear at the selected beam hit; the old gaze-centred duplicate should be
   absent.
7. Test both the front-end main menu and pause menu. Aim and click with a motion
   controller and confirm the menu also remains visible on the desktop mirror.
8. Open the laptop and an email. The email panel should render continuously
   rather than remain black or flash sparse rectangles. Exit by look-away and
   right-controller A, then attach the full log.

Decisive rows are `hpl_arm_pose_seed ... nodes=34 freezePose=1`, `hpl_arm_ik`
with `elbowDownMeters=0.100`, `hpl_controller_gameplay_policy ... mainMenu=1`,
and `terminal_scissor_bypass`. Independent rollback controls are
`HandFreezePose=0`, `HandShoulderBackOffsetMeters=0`,
`HandArmIKElbowDownMeters=0`, and `HudSuppressCenterCrosshair=0`.

## 0.80.1 HMD Shoulder Rig, Physical Grab, Menu, And Terminal Probe

1. Run doctor and require `0.80.1-hmd-shoulder-rig` with `fail=0`. Wait at
   least 20 seconds at the main menu, load the apartment save, and press F10.
2. Trigger and drink the medicine. Hold both controllers still, move them, and
   rotate the player body. Shoulder height and upper/lower segment lengths must
   remain stable; shoulders must follow body heading rather than remain in the
   old world orientation.
3. Without locomotion, physically lean the headset in every direction. The
   shoulder centre should follow head position at a stable neck offset. Merely
   looking or tilting must not rotate the shoulder frame.
4. Rotate each controller through clear yaw, pitch, and roll. Both hands should
   retain the same initial palm alignment and follow independently after reload.
5. Pick up a general physics object and walk while holding it. Both tracked
   hands and arms must continue updating throughout Grab state and resume cleanly
   after release.
6. Test pause and the front-end main menu. Motion-controller aim/click must move
   the native cursor in both, and both must remain visible in the desktop mirror.
7. Enter a laptop email and wait four seconds. Require an automatic
   `terminal_draw_state auto_armed` row plus bounded draw-state samples without
   pressing a key. Confirm right-controller A and look-away still detach in one
   frame. `Ctrl+F10` is optional for fresh images.
8. Exit normally. Attach the full log. A passing arm run keeps logged
   `lengths=` near their first values instead of growing toward `0.6-0.7`.

Independent rollback remains `HandArmIK=0`, `HandAlwaysVisible=0`,
`HandWristRotation=0`, or `TerminalLookAwayExit=0`.

## 0.79.0 Hands, Terminal, Menu, And Recenter

1. Run doctor and require `0.79.0-hands-terminal-menu` with `fail=0`. Wait at
   least 20 seconds at the main menu, load the apartment save, and press F10.
2. Trigger the medicine hands. Confirm full-size bilateral position and
   rotation tracking, then pause/resume and interact with a room object. Hands
   must resume tracking instead of freezing in world space; shoulders must stay
   at the corrected height without progressive arm stretch.
3. Recenter while visibly pitched and rolled. Position and yaw reset, but the
   world horizon remains level and headset pitch/roll are not zeroed.
4. Use the motion-controller pointer in pause and main menu. Confirm each menu
   also remains visible in the desktop mirror.
5. Enter the laptop and press right-controller A. Re-enter and trigger the
   65-degree look-away exit. Both must leave native terminal state; the overlay
   and cursor must not return merely by looking back.
6. Open a broken email and press `Ctrl+F10` once. Keep it visible for four
   seconds, then attach the log and `logs\terminal-captures`. Require bounded
   `terminal_draw_state` rows containing FBO, program, scissor, texture, and
   blend state.
7. Exit normally. There must be no startup, pause, main-menu, or teardown crash.

Independent rollback remains `HandArmIK=0`, `HandAlwaysVisible=0`,
`HandWristRotation=0`, or `TerminalLookAwayExit=0`.

## 0.78.1 Startup Pause Gate

1. Run doctor and require `0.78.1-startup-pause-gate` with `fail=0`.
2. Launch twice. On each run, wait at least 20 seconds at the main menu before
   loading a save. The former deterministic crash occurred at 10 seconds.
3. Load the apartment medicine save and press F10. Confirm VR activation,
   locomotion, and the native hand sequence still work.
4. Continue with the 0.78.0 shoulder, wrist-rotation, tracking-reacquisition,
   menu resume, and normal quit checks below.
5. Attach the complete log if either run closes unexpectedly. A successful
   startup should include `pauseDataReady=1`; unavailable early ownership must
   fail closed without an access violation.

## 0.78.0 Arm Pose And Menu Safety

1. Run doctor and require `0.78.0-arm-pose-shutdown-safety` with `fail=0`.
   Load the apartment medicine save and press F10 once.
2. Trigger the medicine sequence and wait for the initially tiny native hands
   to become tracked. Both arms should appear as in 0.77.2, with no regression
   in scale, stereo, locomotion, or independent wrist position.
3. Compare the shoulders with the previous run. Their shared root should be
   about 30 cm lower while each wrist still meets its controller. Note whether
   either elbow now bends too low, crosses the torso, or reaches a hard limit.
4. Hold each hand in place and rotate that controller through clear yaw, pitch,
   and roll. The matching hand should rotate through the same relative change
   without an initial snap; the opposite hand should remain independent.
5. Briefly hide one controller from tracking, then reacquire it. That hand may
   return through one native-aligned anchor frame, but must not retain a stale
   angle, jump to the other hand, or disturb the other arm.
6. Open the pause/menu screen, close it, resume movement, and trigger the hands
   again if needed. Then enter the menu once more and quit normally. The game
   must not crash. Expect a bounded `hpl_hands_visibility ... invalidated`
   line at the ownership transition.
7. Attach the complete log. Useful acceptance counters are
   `shoulderOffsets`, `wristRotation.anchorSeeds/applications/fallbacks`, and
   `persistentHands.invalidations` in `hpl_arm_body_summary`.

Rollback rotation with `HandWristRotation=0`, shoulder calibration with
`HandShoulderVerticalOffsetMeters=0`, or persistence with
`HandAlwaysVisible=0`.

## 0.77.2 Hand Identity Recovery

1. Doctor reports `0.77.2-hand-identity-recovery` and `fail=0`. Load the same
   apartment medicine save and press F10.
2. Trigger the sequence that displays the tiny hands. Require a
   `hpl_entity_identity ... name=PlayerHands_0 playerHands=1` row followed by
   `hpl_hands_native_seed`. Their absence is the primary failure signal.
3. Once both controllers are tracked, the model should become life-sized and
   both wrists should follow independently. Move one controller at a time and
   require `hpl_hands_scale`, bilateral `hpl_wrist_position`, and `hpl_arm_ik`
   rows in the log.
4. Let the animation finish, then walk and turn. Record whether the hands remain
   visible and tracked; this separately tests the post-seed persistence layer.
5. Attach the complete log. Do not test the laptop in this pass.

Rollback persistent visibility with `HandAlwaysVisible=0`; scale, wrist, and IK
can be isolated with `HandScaleNormalization=0`, `HandWristPosition=0`, and
`HandArmIK=0`.

## 0.77.1 Hand Seed Recovery And Terminal Fallback

1. Doctor reports `0.77.1-hand-seed-terminal-fallback` and `fail=0`. Press F10
   after loading the apartment.
2. Trigger the medicine/hand sequence. A brief native setup phase is allowed,
   but the hands must then become life-sized and track the matching controllers
   with arm IK. Require `hpl_hands_native_seed`.
3. Let the native hand animation finish and continue walking/turning. Hands
   should remain visible in Normal gameplay. Enter and leave any available
   authored state and confirm native ownership still wins there.
4. Open the laptop and leave an email visible for at least five seconds. Expect
   `hpl_terminal_retention_fallback ... action=live_frame_capture` after eight
   retained samples unless a real color clear is observed. The email may still
   update in partial tiles, but the panel must not remain permanently black.
5. While the laptop is active, confirm pointer highlighting/clicks and look-away
   exit. If email tiles are incomplete, press `Ctrl+F10` once and attach the new
   four-frame native/HUD RGB and alpha sequence with the complete log.

Rollback hands independently with `HandAlwaysVisible=0` or `HandArmIK=0`.
`TerminalPreserveDirtyRects=0` forces the same live-frame terminal fallback
without the eight-sample probe.

## 0.77.0 Persistent Arms And Medicine Gestures

1. Doctor reports version `0.77.0-arm-ik-authored-interactions` and `fail=0`.
2. After F10 and native hand creation, both hands become full size independently.
3. Test each arm low/high/crossed/near chest/near reach; elbows bend on the
   native side and do not snap straight.
4. Wait for the native hand animation to finish, then walk and turn. Arms stay
   visible and controller tracked in Normal state.
5. Enter any authored/non-Normal state available. The native sequence must own
   hands and camera without a SOMAVR retention fight.
6. Put the left hand at the medicine cap and press trigger or grip once; expect
   one haptic and one `cap_remove_requested` row.
7. Bring the bottle neck to the HMD mouth position and tip it for at least 12
   frames; expect one haptic and one `drink_requested` row.
8. Preserve the log through normal exit. Roll back independently with
   `HandArmIK=0`, `HandAlwaysVisible=0`, or `MedicineInteraction=0`.

## 0.76.0 Full-Scale Independent Wrist Position

1. Run packaged doctor and require
   `version=0.76.0-fullscale-wrist-position`, OpenXR flavor, and `fail=0`.
   Press F10 once and briefly regress rigid stereo, normal locomotion, carry
   locomotion, interaction beams, curtains, and story-object inspection.
2. Trigger the same interaction that displays `PlayerHands_0`. The hands should
   become approximately life-sized rather than quarter-sized and should move
   from the face to their corresponding controller positions. Wrist rotation
   remains SOMA-authored in this build and is not an acceptance criterion.
3. Hold both controllers apart, then move only the left hand in a large circle
   while keeping the right still. Repeat for the right. Cross the controllers,
   separate them widely, move both vertically, walk, and snap-turn. Require
   independent positions with no bilateral attachment to the right controller.
4. Inspect the wrist/forearm transition for stretching, separation, inversion,
   or an unsuitable palm offset. Record whether each hand center lands above,
   below, ahead of, or behind the physical grip. This is calibration evidence,
   not an automatic failure of the position ownership path.
5. Briefly hide one controller from tracking, restore it, then put down/exit the
   hand animation and trigger it again. Native behavior must return while
   ineligible and both hands must recover without a stale offset or scale.
6. Exit normally and attach the complete `somavr.log`. Require paired
   `hpl_wrist_position` rows with `restored=1`, scale rows with
   `rootPosePreserved=1`, nonzero `wristFramesApplied`, and zero authored-post,
   hierarchy, read, and math fallbacks during the accepted interval.

Roll back size only with `HandScaleNormalization=0`, position only with
`HandWristPosition=0`, or both for the accepted 0.75 passive behavior. Keep
`HandControllerRoot=0`.

## 0.75.0 Terminal Dirty Rectangles And Wrist Candidate

1. Run package doctor and require
   `version=0.75.0-terminal-dirtyrect-wrist-candidate`, OpenXR flavor, and
   `fail=0`. Press F10 once and confirm rigid stereo, normal locomotion, carry
   locomotion, curtains, beams, and story-object inspection.
2. Enter the apartment laptop, open an email, and wait ten seconds without
   moving the pointer. The shell and complete email must accumulate and remain
   stable. Require `terminal_color_clear_suppressed` and later
   `hpl_terminal_capture ... clearSuppression={color=` with a nonzero color
   count, zero FBO/thread mismatches, and
   `policy=native_size_retained_surface_suppress_nested_color_clear_then_upscale`.
3. Move the controller pointer over several email controls and click between
   the inbox and two messages. Confirm highlights/clicks still work and old
   page text does not remain incorrectly. Look at least `65` degrees away and
   confirm native terminal exit. Each click should emit
   `hpl_terminal_retention_reset ... reason=controller_click`.
4. If any email content still flashes, smears, or remains stale, leave the bad
   state visible and press `Ctrl+F10` once. Attach the complete log plus all new
   `terminal_native_*` and `terminal_hud_*` files. There should be four RGB and
   four alpha files for each target.
5. Trigger the interaction that displays `PlayerHands_0`. Hold both controllers
   still apart for two seconds, then move only the left hand in a large circle,
   move only the right, and finally rotate both wrists through pitch/yaw/roll.
   Require left/right rows with `parentMatch=1`, `controllerTargetValid=1`,
   `postCandidate={valid=1`, and `worldError` close to zero. No miniature pair
   should follow the right controller because no bone/root mutation is active.
6. Put the hand-displaying object down, trigger it once more if practical, then
   exit normally and attach the entire `somavr.log`.

Rendering rollback is `TerminalPreserveDirtyRects=0`. Keep
`HandControllerRoot=0`; the wrist candidate is diagnostic and has no mutation
toggle in this build.

## 0.74.0 Native Terminal Surface And Wrist RE

1. Run package doctor and require
   `version=0.74.0-native-terminal-wrist-re`, OpenXR flavor, and `fail=0`.
   Press F10 and regress rigid stereo, tracking, locomotion, carry movement,
   curtains, beams, story objects, and throws.
2. Enter the apartment laptop and open an email. Require
   `openxr_terminal_capture_target created size=1024x577` and a terminal capture
   row with `viewport=0,0,1024,577` and
   `policy=native_size_retained_surface_then_upscale`. The complete email must
   remain stable without alternating rectangles. Pointer highlighting, clicks,
   and look-away exit must still work.
3. If email tiles still flash, leave the affected email visible and press
   `Ctrl+F10` once. Attach the four new RGB/alpha pairs and complete log. Do not
   add Frida to this pass.
4. Trigger the interaction that creates `PlayerHands_0`. The previous pair of
   miniature hands must no longer follow the right controller because
   `HandControllerRoot=0`. Native hands may remain in their authored pose.
   Move both controllers independently for several seconds and require
   `hpl_hands_bone` rows with matching `controllerHand=left/right`,
   `controllerTargetValid=1`, and changing target deltas.
5. Briefly confirm one Grab carry movement and curtain interaction, then exit
   normally and attach the complete log.

The terminal rollback remains `TerminalOverlay=0`. `HandControllerRoot=0` is
the accepted stable setting; do not enable the shared-root prototype.

## 0.73.0 Terminal Retention And Grab Movement

1. Run the stable package doctor and require
   `version=0.73.0-terminal-retention-grab-move`, OpenXR flavor, and `fail=0`.
   Press F10 once and briefly regress rigid stereo, tracking, ordinary
   locomotion, curtains, drawers, doors, beams, and story-object inspection.
2. Enter the laptop and open an email. Leave it visible for several seconds.
   The full email pane must accumulate and remain stable rather than showing
   flashing isolated tiles. Require `hpl_terminal_retention ... active=1` and
   later `hpl_terminal_capture ... retained=1`.
3. While still in the laptop, turn the HMD roughly `65` degrees away and hold
   for a moment. SOMA must leave terminal state and restore scene navigation.
   Require one `hpl_terminal_lookaway ... route=native_interact_cancel` row and
   a later retention transition to `active=0`. A quick glance should not exit.
4. Pick up a loose physics object, keep holding it, and walk forward, backward,
   and sideways with the left stick. Movement should remain controller-relative
   and retain SOMA's heavy-object slowdown. Require
   `hpl_movement_native_input ... state=grab(1)`; release and throw behavior
   should remain unchanged.
5. Exit normally and attach the complete log. A fresh `Ctrl+F10` capture is
   optional only if the email still fragments.

Rollback independently with `TerminalOverlay=0`,
`TerminalLookAwayExit=0`, or the existing semantic-input fallback settings.

## 0.72.0 Terminal Layer Dump

1. Run the stable package doctor and require
   `version=0.72.0-terminal-layer-dump`, OpenXR flavor, and `fail=0`. Press F10
   once and regress the accepted world, locomotion, curtain, drawer, and beams.
2. Enter the laptop and open the email view that fragments. Leave the broken
   content visible and press `Ctrl+F10` once. A short synchronous stall is
   expected while four RGB and four alpha BMPs are written.
3. Exit normally and attach the complete log plus the newest files from
   `logs\terminal-captures`. Require one `terminal_hud_dump armed` row, four
   successful `terminal_hud_dump` rows, and no `read_pixels` failure.
4. Do not use F6 or Frida in this first pass. The BMP sequence already separates
   a changing/stateful GUI render from an OpenXR compositor-only defect. A
   second `Ctrl+F10` capture on the laptop home screen is useful as a coherent
   control sample.

The diagnostic does not change terminal rendering. `TerminalOverlay=0` remains
the presentation rollback.

## 0.71.0 Native Semantic Input

1. Run the stable package doctor and require
   `version=0.71.0-native-semantic-input`, OpenXR flavor, and `fail=0`. Press
   F10 once and confirm the accepted rigid world, stereo, eye height, shaders,
   tracking, beams, loose props, and story-object behavior.
2. In Normal state, push the left stick fully forward/back/left/right and then
   diagonally. Walking must have ordinary SOMA speed and remain
   left-controller-relative after snap turns. Require
   `movementRoute=native_player_analog` and
   `hpl_movement_native_input ... analogOwner=... amount=... type_1`; synthetic
   key flags should remain `0000`.
3. Grab the apartment curtain with either interaction hand and move that hand
   left/right. Require `playerState=13`, `hpl_manipulation_motion`, then
   `hpl_manipulation_native_input` with non-null `stateScript` and `context`.
   The curtain must follow hand travel without snap turn.
4. Repeat on the desk drawer and bathroom tap, recording each state ID. State
   `13` mechanisms should use the semantic helper-owner route; Slide `4` may
   still use the direct joint route. Test both motion directions.
5. Regress door bidirectionality, laptop overlay/pointer, story-object
   distance/rotation/exit, loose-prop hold/throw, and clean shutdown. Attach the
   complete log.

Rollback independently with `ManipulationMotion=0`, `SlideDirectVelocity=0`,
`NativeLocomotion=1`, or `TerminalOverlay=0`.

## 0.70.0 Input And Terminal Recovery

1. Run the stable package doctor and require
   `version=0.70.0-input-terminal-recovery`, OpenXR flavor, and `fail=0`. Press
   F10 once and confirm the accepted stereo, tracking, eye height, shaders,
   beams, loose props, and story-object behavior.
2. Push the left stick fully in each direction. Walking must be back at ordinary
   SOMA speed and remain left-controller-relative after a snap turn. The log
   should report `movementRoute=semantic_keys` and visible `W/A/S/D` key states.
3. Grab a curtain and move laterally, then grab the desk drawer and move along
   its physical rail. Require `hpl_slide_anchor` followed by
   `hpl_slide_target`; movement must follow the owning hand without snap turn.
4. Open and close the bathroom door with hand travel around its hinge. Require
   `hpl_rotate_target ... state=5 ... wristTwist=0`; both directions must follow
   the hand. Test the tap separately and note its reported state. Lever/tap
   state `6` may report `wristTwist=1`.
5. Enter the laptop. Require a coherent sharp overlay and
   `policy=single_pass_direct_hud_target`, normally with a `1920x1080` viewport.
   Sweep either controller across the panel: the cursor must visibly follow,
   widgets must highlight, trigger must click, and A/B must exit.
6. Exit normally and attach the complete log. Also report whether the physical
   laptop screen freezing behind the owned overlay is visible or objectionable.

Rollback independently with `NativeLocomotion=1`, `SlideDirectVelocity=0`,
`RotateDirectVelocity=0`, or `TerminalOverlay=0`.

## 0.68.2 Native-Phase Recovery

1. Run stable-package doctor and require
   `version=0.68.2-native-phase-recovery`, OpenXR flavor, and `fail=0`. Press F10
   and confirm the accepted world, stereo, eye height, shaders, tracking,
   locomotion, beams, and loose-prop hold.
2. Hold one curtain and move only the owning controller left/right, then repeat
   on a drawer and bathroom tap. Require paired `hpl_manipulation_motion` and
   `hpl_manipulation_native_input` rows using
   `player_helper_update_0x15ba20_to_analog_0x154fb0`. SOMA must remain stable;
   report any reversed mechanism by name.
3. Inspect one story object. Its entrance must play once, settle at SOMA's
   native distance, appear `2x` size, rotate freely with grip, and exit with
   right A/B. It must not drift farther on later frames.
4. Enter the laptop. The head-locked image must be one coherent screen, not a
   4-by-2 tile. Sweep a controller and click a widget. Preserve
   `hpl_terminal_capture` and `hpl_terminal_pointer` rows even if cursor or
   highlighting is still wrong.
5. Exit normally and attach the complete log. A separate no-movement safety
   replay is unnecessary unless this build faults.

Rollback independently with `ManipulationMotion=0`, `TerminalOverlay=0`, or
`ReadPresentation=0`.

## 0.68.1 Input-Phase Safety

1. Run stable-package doctor and require
   `version=0.68.1-input-phase-safety`, OpenXR flavor, and `fail=0`. Press F10
   and confirm the established world, stereo, tracking, shaders, eye height,
   locomotion, beams, and loose-prop hold.
2. First perform a minimal crash-safety test: point at one curtain, press and
   hold trigger, wait two seconds, release, and repeat once without moving the
   controller. SOMA must remain running.
3. Hold trigger again and drag the owning controller left/right. Require a
   queued `hpl_manipulation_motion` row followed by
   `hpl_manipulation_native_input` with
   `route=native_input_phase_substitution_0x154fb0`. The curtain should move
   without snap turn.
4. Only after the curtain pass, test the bathroom tap and cupboards in both
   directions. Their state-13 rows must use the same phase-correct route.
5. Briefly regress terminal coherence/pointer, story-object distance/rotation,
   loose-object hold/throw, and clean shutdown. Attach the complete log whether
   curtain motion succeeds or not.

Rollback manipulation safely with `ManipulationMotion=0`.

## 0.68.0 Interaction Correction

1. Run stable-package doctor and require
   `version=0.68.0-interaction-correction`, OpenXR flavor, and `fail=0`. Press
   F10 and confirm the accepted stereo, rigid world, eye height, shaders,
   tracking, locomotion, beams, context icon, and loose-prop hold.
2. Hold trigger on a curtain and drag left/right with the owning hand. Repeat on
   the bathroom tap and cupboards in both directions. No snap turn should be
   required. Require `hpl_manipulation_motion` rows for Slide `4` and
   MovingButton `13` with
   `route=native_player_analog_dispatch_0x154fb0`.
3. Throw a cup gently and quickly away from the body. It must travel at least as
   far as the native-strength version and must not kick the player backward.
   Require `hpl_controller_throw` with `velocityScale>=1` and
   `forwardSafetyDot=0.250`.
4. Enter the laptop. The head-locked duplicate must be coherent and stable, with
   no fragmented or flashing text. Sweep either controller over it and click a
   native widget; the visible cursor must follow. HUD summary must show terminal
   captures while GUI draw counts remain one native render per call.
5. Inspect a story object. It should retain the accepted native entrance speed
   and orientation, settle at roughly twice the prior camera distance
   (approximately `0.30` instead of `0.15` world metres), rotate freely with
   grip, and exit with right A/B.
6. Exit normally and attach the complete log. Preserve manipulation, terminal
   pointer/HUD, throw, and Read-presentation rows.

Rollback independently with `ManipulationMotion=0`, `TerminalOverlay=0`,
`ThrowRedirect=0`, or `ReadPresentation=0`.

## 0.67.0 Native Manipulation And Terminal Overlay

1. Run stable-package doctor and require
   `version=0.67.0-native-manipulation-overlay`, OpenXR flavor, and `fail=0`.
   Press F10 and first confirm accepted stereo, rigid world, eye height, shaders,
   tracking, locomotion, both beams, context icon, and story objects.
2. Hold trigger on each starting curtain and drag the owning controller left and
   right. It must move without snap turn. Repeat with short gestures, reversed
   direction, either hand, and a drawer pull/push. Expect
   `hpl_manipulation_motion entered state=Slide(4)` and no default
   `hpl_slide_target` direct-PID rows.
3. Pull the mug and DSLR to the hand. Each should complete its approach instead
   of exhausting its movement limit at a distance, then follow at least the prior
   near/far range without chatter. Rotate around pitch, yaw, and roll; response
   should be useful but bounded. The translation log must report
   `unbounded_pull_in_plus_bounded_controller_travel`.
4. Enter the starting laptop. SOMAVR should leave the physical screen in place
   and show a readable head-locked duplicate in front of the player. Aim either
   beam across the duplicate: the cursor must follow continuously and trigger
   must activate native widgets without camera/body takeover.
5. Require terminal rows with `route=head_cone`,
   `owner=manager_world_input_0x170`, and applied dispatch counts. HUD summary
   must report nonzero terminal overlay matches/captures. Exit the terminal,
   verify the duplicate and click latch disappear, then re-enter once.
6. Exit normally and attach the complete log. For optional live Frida follow-up,
   leave SOMA running at the failing curtain, mug/DSLR, or laptop and report
   which object is selected.

Rollback independently with `TerminalOverlay=0`, `SlideDirectVelocity=1`,
`GrabAttachToHand=0`, or `GrabRotation=0`.

## 0.66.0 Interaction Stability

1. Run stable-package doctor and require
   `version=0.66.0-interaction-stability`, OpenXR flavor, and `fail=0`. Press F10
   and verify the accepted story-object presentation, stereo, shaders, tracking,
   locomotion, both beams, and context icon remain unchanged.
2. Pick up light and medium loose props. They should pull toward the owning hand,
   settle without visible angular chatter, follow hand orientation smoothly, and
   retain ordinary collision, placement, release, and throwing. The log should
   keep `modifiedError` magnitude at or below `maxSpeedAndError=3`.
3. Grab each starting-area curtain and make short then long left/right hand
   gestures. The curtain should continue catching up to hand displacement after
   velocity falls, move in both directions, and remain within its native joint
   limits. Repeat on a drawer with a pull/push depth gesture.
4. Enter the starting laptop terminal. Keep the player and camera in place, aim
   the beam across the physical display, and require the visible cursor to track
   it continuously. Trigger should activate a native widget. Require
   `hpl_terminal_pointer ... owner=manager_world_input_0x170` rows and no repeated
   `dispatch_failed`.
5. Exit normally and attach the complete log. Preserve
   `hpl_grab_rotation`, `hpl_slide_target` displacement/error fields,
   `hpl_terminal_pointer`, and all three bridge summaries.

Rollback independently with `GrabRotation=0`, `SlideDirectVelocity=0`, or
`TerminalRayPointer=0`.

## 0.65.1 Object Rotation Fix

1. Run stable-package doctor and require
   `version=0.65.1-object-rotation-fix`, OpenXR flavor, and `fail=0`. Press F10
   and confirm the accepted stereo, eye height, tracking, shaders, locomotion,
   both beams, and context icon remain unchanged.
2. Inspect the same storyline object used in the regression report. Its approach
   speed, path, and final distance should match native SOMA rather than slowly
   sliding up a distant arc. It should retain the configured doubled apparent
   scale and must not move or tilt the VR camera.
3. Hold the owning grip and rotate the story object around pitch, yaw, and roll.
   Require useful motion beyond the old pitch limit, no axis lock, and no snap
   back when grip is released and pressed again. Right A/B must still exit.
4. Pick up two or more loose physics objects. Rotate each continuously through
   all three axes, including beyond a half turn. Require no fixed angular stop,
   no SOMA-versus-controller vibration, plausible inertia, and normal collision,
   placement, release, and throw behavior.
5. Repeat one prop with two-hand support squeeze. Engagement and release should
   re-anchor without a pose jump. Cross beams while holding and confirm the
   initiating-hand ownership lock remains intact.
6. Attach the complete log. Useful markers are
   `hpl_read_presentation ... policy=native_pickup_travel_full_axis_controller_orientation`,
   `hpl_grab_anchor ... orientationTarget=1`, and
   `hpl_grab_rotation ... policy=absolute_controller_orientation_replaces_native_camera_goal`.

Rollback independently with `ReadPresentation=0`, `GrabRotation=0`, or
`TwoHandGrabRotation=0`.

## 0.65.0 Physical Interaction Polish

1. Run the stable package doctor and require
   `version=0.65.0-physical-interaction-polish`, OpenXR flavor, and `fail=0`.
   Press F10 once and confirm rigid stereo, eye height, tracking, shadows,
   reflections, locomotion, both beams, and semantic context icons are unchanged.
2. Enter the starting laptop/wall terminal without a camera or body takeover.
   Aim across the physical screen and require the cursor to follow the selected
   beam smoothly, including near the edges. Trigger must activate native controls.
   The log should contain `hpl_terminal_pointer ... direct_dispatch` rows.
3. Test a tap, toilet flush, lever, or hinged door. Rotate the owning wrist in
   both directions, then translate the hand around the hinge. Motion should be
   camera independent, smooth, and constrained by the native mechanism.
4. Inspect a storyline Read object. It should appear about twice as far from the
   eyes and twice the previous apparent size. Hold right grip and rotate through
   a useful range without camera drift. Press and briefly hold right A; the
   object and description must close through SOMA's normal cancel transition.
5. Pick up several loose physics props at different masses and distances. Each
   should pull toward the initiating grip, remain responsive while held, collide
   normally, place without a jump, and inherit a plausible release/throw impulse.
6. While holding a prop, cross both beams over it and press the other trigger.
   The held object must remain owned by the initiating hand until release. Then
   acquire it with the other hand. Require one lock and one unlock per session,
   without focus flicker or duplicate callbacks.
7. Exit normally and attach the complete log. Preserve terminal `applied` and
   `directDispatches`, hinge translation/wrist components, Grab attach correction,
   Read presentation/cancel rows, and interaction owner lock/unlock summaries.

Rollback one route at a time with `TerminalRayPointer=0`,
`RotateAngularVelocityScale=0`, `GrabAttachToHand=0`,
`ReadPresentation=0`, or `InteractionBothHands=0`.

## 0.64.1 Dual-Hand Interaction Startup Fix

1. Run doctor and require `SOMA interaction hook signatures match the supported
   build`, then require `version=0.64.1-dual-hand-interaction-fix`,
   `controller_config ... interactionBothHands=1`, and
   `hpl_interaction_bridge install_ok ... bothHands=1`. Stop immediately on any
   outer/inner signature or game-context-slot failure.
2. Press F10 and point either beam at the previously tested drawer or pickup.
   Require `hpl_interaction_ray ... applied=1`, a positive hit distance, a valid
   hit snapshot, and the semantic context icon at that beam endpoint.
3. Activate the drawer and pick up the object with each hand. Confirm native
   state transitions and initiating-hand motion are restored, then continue the
   full 0.64 dual-hand checklist below.

The 0.64.0 failure signature was two visible guides with zero interaction-
reticle updates, semantic `no_hit_snapshot` rejects, and bridge startup
`signature_mismatch`. This build must show none of those together.

## 0.64.0 Dual-Hand Interaction

1. Run the stable package doctor and require
   `version=0.64.0-dual-hand-interaction`, OpenXR flavor, and `fail=0`. Press F10
   once and confirm the established rigid world, eye height, stereo, shadows,
   reflections, locomotion, HUD, and terminal presentation remain unchanged.
2. Hold both controllers in view. Require a left and right cyan guide at the
   same time. Point only the left beam at a pickup, drawer, door, readable, or
   terminal control; SOMA's context icon must sit at the left beam endpoint and
   left trigger/select must activate it. Repeat independently with the right.
3. Aim both beams at the same target and move them across one another. Focus and
   the icon must remain stable rather than flicker each frame. Press the non-
   owning trigger: ownership and the icon must transfer immediately and exactly
   one native interaction must fire.
4. Start a Slide, hinge, Grab, and Read interaction with each hand where content
   permits. Motion, rotation, release, and contact haptics must stay attached to
   the initiating hand after its beam moves off the original hit surface.
5. Enter a wall or handheld terminal. Either beam may claim the pointer with its
   trigger; clicking, edge misses, native sounds, and cancel must remain correct.
   Both guides and a semantic world icon may coexist without either changing
   texture or disappearing unexpectedly.
6. Exit normally and attach the full log. Require per-hand nonzero probe/hit/
   selection counts and plausible switch counts, with no signature, finalizer,
   layer-limit, swapchain, or submission failure.

Rollback: set `InteractionBothHands=0` to restore preferred-hand-only native
probing and a single preferred-hand guide. `AimGuide=0` hides guides without
disabling dual-hand native interaction.

## 0.63.0 Diegetic Terminals

1. Run the stable package doctor and require
   `version=0.63.0-diegetic-terminals`, OpenXR flavor, and `fail=0`. Launch from
   `out\SOMAVR-latest`, press F10 once, and first confirm the accepted rigid
   world, eye height, same-frame stereo, shadows, reflections, and locomotion.
2. Activate a wall terminal from an ordinary standing distance. The player body
   must not teleport, the view must not rotate toward the display, and the
   camera must not be pulled into it. Physically lean closer and around the
   screen; head translation and rotation must remain 1:1.
3. Aim the dominant controller at the actual terminal surface. The extended
   guide and cursor should agree, including near screen edges. Aim just outside
   the mesh: the cursor must stop rather than click through a guessed rectangle.
   Trigger/select must activate native controls with normal sounds and feedback.
4. Exit with the existing controller cancel and re-enter several times. Focus,
   callbacks, movement suppression, and release must remain native, with no
   stuck click, beam, body offset, camera offset, or terminal state.
5. If available, test a handheld terminal. Its authored state-9 presentation
   must remain unchanged; evaluate spatial pointing separately from the wall
   terminal takeover. Also test pause/menu and one non-terminal world GUI.
6. Exit normally and attach the full log. Require nonzero
   `spatial={attempts=... hits=...}` and takeover suppression counts for a wall
   terminal. `headConeFallbacks` should stay zero when the focused spatial owner
   resolves normally.

Rollback: `TerminalRayPointer=0` keeps diegetic camera behavior but restores the
head-relative pointer. `TerminalDiegetic=0` restores the authored wall-terminal
camera/body takeover. `TerminalPointer=0` restores SOMA's complete pointer path.
Change one rollback at a time.

## 0.62.0 Physical Hinges And Stutter

1. Run packaged doctor and require `version=0.62.0-physical-hinges`, OpenXR
   flavor, and `fail=0`. Press F10 once and first confirm rigid stereo, eye
   height, tracking, shadows, reflections, HUD, locomotion, and interaction ray.
2. Open the same drawer or curtain with deliberately slow, medium, and fast hand
   motion along its physical axis. Motion must now reflect hand speed and remain
   independent of camera yaw. Require `hpl_slide_target` velocity magnitudes to
   vary materially rather than remaining approximately `1.0`.
3. Grab a hinged door and move the controller through the door's physical arc.
   Repeat with a lever. Require `hpl_rotate_anchor` and `hpl_rotate_target` rows,
   signed target speed, native limits/sounds, and no dependency on snap turn.
   Test both opening/closing directions and a hinge from each side if available.
4. While holding a door or lever, move the controller primarily along the hinge
   axis or directly toward the pivot. The mechanism should move little; a
   tangential arc should produce the strongest motion. Release must restore
   normal state without a jump or continuing torque.
5. Compare head rotation, walking, object grab, Slide, Door, and Lever smoothness
   with 0.61. The log should be much smaller and free of continuous
   `hpl_render_stage` and `hpl_per_eye_gpu/cpu` rows. Report whether periodic
   stutter is gone, reduced, unchanged, or tied to a specific interaction.
6. Open F1 and toggle Same Frame Stereo off/on once. Accepted mode should retain
   zero pose-frame gap. Same-frame capture deltas over 20 ms should be rare and
   must no longer include prior-frame comparisons.
7. Recheck Read rotation and A/B cancel plus ordinary grabbed-object translation,
   rotation, throw, and collision. Velocity-magnitude correction must not create
   excessive throws or unstable physics. Exit normally and attach the full log.

Rollback: set `RotateDirectVelocity=0` to restore native mouse-derived Door and
Lever input; set `SlideDirectVelocity=0` for the old Slide route. Keep only one
rollback changed per comparison.

## 0.61.0 Native Manipulation

1. Run packaged doctor and require `version=0.61.0-native-manipulation`, OpenXR
   flavor, and `fail=0`. Press F10 once. First confirm the accepted rigid world,
   same-frame stereo, eye height, shadows, reflections, and positional tracking.
2. Aim at several props before interacting. Require valid
   `hpl_interaction_ray ... hitSnapshot=1` rows with plausible positive distance,
   entity, and body fields; semantic reticle updates should no longer be held at
   zero by payload-distance rejects.
3. Grab a drawer handle and move the controller toward/away from the cabinet.
   Grab a curtain and move laterally. Require `hpl_slide_anchor` followed by
   `hpl_slide_target` rows. Physical motion should follow each joint axis without
   depending on camera angle or snap turn. Release must preserve native sounds,
   limits, gravity restoration, and state exit.
4. During Slide and Read, move the turn stick. It must not rotate the player,
   rotate the object, black out the display, or reset the view.
5. Inspect a readable. Hold dominant grip and rotate the controller through
   yaw/pitch; the object should rotate smoothly through a useful range. Press
   right-controller A to put it away; B remains an alternate cancel. Preserve
   the `hpl_manipulation_session` orientation totals.
6. While Read is active, preserve all `hpl_read_entity_candidate` rows and show
   the readable plus description background. These rows identify the open prop
   for controller attachment; report whether matching the HUD to `1920x1080`
   fixes the left-edge text crop.
7. Move the physical mouse vertically in Normal state. The VR horizon must stay
   HMD-owned and visually level; controller ray and HMD pitch must remain usable.
   Confirm native yaw/snap turn still works in Normal state.
8. Compare text and fine geometry with the prior build. The current source is
   still a `1920x1080`, one-sample SOMA render upscaled into `2688x2880` runtime
   eye images, so record softness and edge aliasing separately from HUD clipping.
9. Exercise ordinary Grab translation/rotation and release. It should remain
   controller-relative and native-physics constrained. Exit normally and attach
   the complete log, including bridge summaries.

## 0.60.0 Evidence Capture

1. Run packaged doctor and require `version=0.60.0-evidence-capture`, OpenXR
   flavor, and `fail=0`. Press F10 once in a loaded save and repeat the 0.59
   visual/interaction pass. Require no regression; this build intentionally
   changes telemetry only.
2. Keep Same Frame Stereo enabled, then toggle it off/on once in F1. Move near
   fine geometry in both modes. Preserve periodic `openxr_frame` rows and the
   shutdown fields `openxrStereoCaptureDeltaUs*`, `openxrStereoCacheAgeUs*`,
   and pose-frame gaps. Report any visible eye latency with the active mode.
3. Walk forward while aiming the left controller forward, left, right, and at
   steep pitch angles; repeat after mouse or snap yaw. Preserve bounded
   `hpl_controller_direction` rows. Direction must follow horizontal controller
   aim and `controllerReferenceFallbacks` should remain zero while tracked.
4. Aim at empty space, then several interactable props, doors, drawers, and a
   terminal until the native icon changes. Preserve `hpl_interaction_ray`,
   `hpl_interaction_payload`, and `hpl_interaction_semantic` transitions. These
   should reveal whether the remaining reticle gap is query, payload, or
   semantic ownership.
5. Enter Read and Zoom states and show any clipped description/background.
   Preserve state-transition `hpl_gui_set` rows with `read=1` or `zoom=1`, plus
   nearby GameHud rows. No additional current-ImGui surface should be captured.
6. Exercise Slide, Wheel, Door, Lever, Tear, and grip-held Read rotation with
   deliberate short and long hand motions. Each exit should emit exactly one
   `hpl_manipulation_session` summary; describe whether physical travel felt
   too weak, correct, or too strong for that state.
7. Exit normally and attach the complete log so subsystem summaries retain
   counts even when no transition row was triggered.

## 0.59.0 Live Usability

1. Run packaged doctor and require `version=0.59.0-live-usability`, OpenXR
   flavor, and `fail=0`. Press F10 once in a loaded save; continuous same-frame
   stereo should already be active. Require rigid tracking and no shader,
   shadow, reflection, eye-height, or positional regression.
2. Rotate and translate the HMD while inspecting fine nearby geometry. Require
   `stereoPoseGap=0` in periodic `openxr_frame` rows and summary maximum/nonzero
   counts of zero. Toggle Same Frame Stereo off/on in F1 to confirm the reported
   gap and perceived latency change together.
3. Point the left controller in several horizontal directions while moving the
   left stick forward, including after mouse and snap yaw. Direction must follow
   controller yaw, ignore controller pitch/roll, preserve analog magnitude, and
   report growing `controllerRelativeMovementFrames`.
4. Confirm three cyan aim markers follow the dominant controller ray, disappear
   while F1/menu/authored ownership is active, remain stereo-stable, and yield
   to any valid native semantic hit icon. Set `AimGuide=0` for hard rollback.
5. Recheck the native interaction icon, subtitles, readable-object description,
   and overlay center. The old square must be gone and no content may be clipped.
   If voice subtitles appear, retain `hpl_subtitle_layout` rows for calibration.
6. Open a drawer and compare hand travel with the prior build. Slide should be
   approximately 3x stronger while Wheel, Door, Lever, and Tear remain unchanged.
   Set `ManipulationSlidePixelsPerMeter=900` to reproduce the old mapping.
7. Enter a Read/inspection view. Hold dominant grip and move the controller to
   rotate the readable; press dominant Secondary to put it away. Repeat Secondary
   cancel in wall/handheld terminals and Zoom views. Normal-state Secondary must
   still crouch.
8. Walk and smooth-turn on Quest 3. Require a visible, centered peripheral mask,
   logged angular width near 127 degrees, clean fade-out on stop/panel/pause, and
   no opaque center. Toggle it in F1 and use `ComfortVignette=0` for rollback.
9. Exercise a bright/dark transition. Mismatch logs must be bounded to first
   eight/every 300 and include `differenceMask`; stereo tone/grading must remain
   visually matched. Attach the full shutdown summaries.

## 0.58.0 Native Grab-Contact Haptics

1. Run packaged doctor and require
   `version=0.58.0-grab-contact-haptics`, OpenXR flavor, and `fail=0`. Keep
   `ContactHaptics=1` and require `hpl_contact_haptics install_ok` at RVA
   `0x32f0e0` without a signature error.
2. Grab a movable object with the dominant controller. Tap it lightly, then
   strike a wall or floor more firmly. Require increasing native `speed` and
   bounded `amplitude` in `hpl_contact_haptics pulse` rows on the configured
   hand while native impact audio, particles, motion, and collision remain
   unchanged.
3. Repeat a single hard impact against a two-material surface. Require no rapid
   double pulse; `cooldown` rejects may rise. Hold an object still against a
   surface and require no continuous buzz from weak/resting contacts.
4. Cause unrelated impacts outside Grab state, beyond the configured grip
   radius, during tracking loss, and during an authored camera state. Require no
   pulse and increasing state/pose/stale/distance rejection evidence where
   applicable. Test a nearby unrelated impact while grabbing and report any
   false positive.
5. Release and re-grab, swap dominant hand, exercise authored gameplay rumble,
   and use a physical gamepad if available. Contact output must follow only the
   configured dominant hand; bilateral authored rumble and native gamepad
   behavior must remain independent.
6. Hard rollback: set `ContactHaptics=0`. Require the disabled row, no impact
   hook/pulse rows, and otherwise identical grab physics, sound, haptics, and
   shutdown. Attach the full contact summary and representative pulse rows.

## 0.57.0 Fixed Foveation

1. Run packaged doctor and require `version=0.57.0-fixed-foveation`, OpenXR
   flavor, and `fail=0`. Keep `Foveation=1`, `FoveationLevel=2`,
   `FoveationDynamic=0`, and `FoveationVerticalOffset=0.0` for discovery.
2. At OpenXR startup, inspect `openxr_extensions`. If any of the three required
   FB extensions is absent, require `openxr_foveation unavailable` and normal
   color swapchains with no startup/session regression.
3. On a supporting runtime require all three functions ready, two foveation-
   capable eye swapchains, `openxr_foveation active eyes=2`, and summary fields
   `ExtensionsEnabled=1`, `Operational=1`, `Applications>=1`, `Failures=0`.
4. Compare levels `0`, `1`, `2`, and `3` in the same save using per-eye GPU
   telemetry. Record world/post/submit timing, frame pacing, text readability,
   fine geometry, shadow edges, reflections, HUD, and peripheral artifacts.
5. Trigger recenter, same-frame/AFR switching, map load, graphics resize, and a
   runtime/session recovery. Every frame-resource rebuild must apply one fresh
   profile to both eyes without stale swapchains or growing failure counts.
6. Hard rollback: set `Foveation=0`. Extension enable/profile rows must disappear
   and established stereo, depth, HUD, temporal ownership, and shutdown behavior
   must remain identical.

## 0.56.0 Authored Camera Handoff

1. Run packaged doctor and require
   `version=0.56.0-authored-camera-handoff`, OpenXR flavor, and `fail=0`.
   Load a save and press F10; established rigid stereo and eye height must be
   unchanged during normal movement.
2. Exercise at least one sit, ladder/climb, interactive camera animation, or
   hand-attached camera event. Require an
   `authored_camera_ownership_changed` row that preserves `tracking=1` and
   `stereo=1`, increments `calibrationGeneration`, and is followed by a fresh
   native base-matrix capture without F10 reactivation.
3. Require one matching `hpl_player_state_comfort` ownership transition and one
   bounded blackout request. A simultaneous player-state plus ownership change
   must produce one blackout, not two.
4. During and after the authored motion, require independent HMD orientation,
   stereo-consistent shadows/reflections/AO, no stale ImageTrail or exposure
   flash, and restoration of gameplay input when native camera ownership ends.
5. Load another save or trigger a true camera pointer replacement. It must still
   use `camera_replaced ... policy=cache_invalidate_and_rearm`; F10 intent must
   return after the stable-pose gate with no old-camera matrices.
6. Attach the full log with camera/input/per-eye history summaries. Require
   `authoredCameraOwnershipChanges` and `authoredCameraTransitions` to agree for
   valid same-camera transitions and no persistent mismatch/fault stream.

## 0.55.0 SSAO Same-Pose Frame Owner

1. Run packaged doctor and require `version=0.55.0-ssao-frame-owner`, OpenXR
   flavor, and `fail=0`. Load a save, press F10, and require
   `ssaoFrameOwner=1` plus the existing `ssao_render` exact hook.
2. In same-frame stereo, inspect fine contact AO while translating and rotating
   the HMD. Require `firstPasses`, `replayPasses`, and `committedRestores` to
   rise together with `failures=0`; both eyes must show matching temporal noise.
3. Switch to AFR. Rendering must remain rigid and the owner must not manufacture
   replay restores across different pose frames. Switch back and recheck the
   same-frame counters without restart.
4. Exercise F2 recenter, calibration changes, loading, pause/inventory, tracking
   loss/recovery, and same-frame toggle. Require no stale phase, flashes, or
   persistent mismatch stream.
5. Inspect local reflective surfaces. They must remain unchanged; local
   reflection copy resources are deliberately native sequential scratch.
6. Independent rollback: set `HPLSSAOFrameOwnerControl=0` while leaving
   `HPLPerEyeSSAOTemporalControl=1`. Per-eye history must remain active and only
   native once-per-eye jitter advancement should return.

## 0.54.0 Per-Eye Temporal SSAO History

1. Run packaged doctor and require `version=0.54.0-per-eye-ssao-history`,
   OpenXR flavor, and `fail=0`. Load a save, press F10, and require
   `perEyeSSAOTemporal=1`, both `ssao_render` and `renderer_set_texture_unit`
   hooks installed, and no `hpl_ssao_temporal fault` row.
2. Require one allocation row with nonzero, distinct left/right GL history IDs
   and plausible AO dimensions/format. After warm-up, `restores` and `commits`
   must increase once per eligible eye render.
3. Inspect contact AO and moving character/object occlusion while translating,
   yawing, pitching, and rolling. Require stereo-consistent AO with no eye-to-eye
   rivalry, one-eye lag, alternating dark halos, persistence, or regression to
   the previously fixed shadow/reflection path.
4. Recenter, pause/resume, load a save, toggle F10, and lose/recover tracking.
   Require resets/reseeding without stale AO flashes, GL errors, crash, or
   increasing allocation count during steady-state play.
5. Test same-frame stereo and AFR. Both must remain rigid; non-player and mono
   viewports must stay native. Exit normally and require clean removal telemetry.
6. Hard rollback: set `HPLPerEyeSSAOTemporalControl=0`. All established visual
   behavior must remain available through native shared SSAO history.

## 0.53.0 ToneMapping Frame Owner

1. Run packaged doctor and require `version=0.53.0-tone-mapping-frame-owner`,
   OpenXR flavor, and `fail=0`. Launch, load a save, press F10, and require
   `toneMappingFrameControl=1` plus no `hpl_tone_mapping_frame fault` row.
2. In same-frame stereo, move between dark and bright spaces and trigger an
   authored fade or grading transition. Require paired `replay` rows with one
   `committedRestore` per opposite-eye pass. Both eyes must have identical
   exposure, fade timing, grading, bloom intensity, and film grain without
   pumping, binocular grain rivalry, or double-speed grain animation.
3. Require `mismatches=0`. A mismatch is useful evidence but blocks promotion;
   attach the full log and identify the scene/event if one occurs.
4. Toggle same-frame stereo off and continue in AFR. Tone mapping, bloom, film
   grain, and fades must remain native with no replay rows for single-eye pose
   frames. Re-enable and confirm pairing resumes regardless of first-eye order.
5. Recenter, pause, load, lose/recover tracking, and toggle F10 off/on. Require
   no stale grading pointer, exposure jump, persistent fade, crash, or fault.
6. Hard rollback: set `HPLToneMappingFrameControl=0`. All established geometry,
   ImageTrail isolation, shadows/reflections, HUD, and native ToneMapping must
   remain functional.

## 0.52.0 Per-Eye ImageTrail

1. Run the packaged doctor and confirm
   `version=0.52.0-per-eye-image-trail`, `fail=0`, and OpenXR flavor. Launch,
   load a save, press F10, and require `perEyeImageTrail=1`,
   `perEyeImageTrailAvailable=1`, and no `hpl_per_eye_post_effect fault` row.
2. Trigger a scene or scripted event that activates ImageTrail. Require one
   `allocated=1` row with four nonzero, pairwise eye-distinct resource pointers,
   followed by alternating same-pose left/right `restore` and `capture` rows.
3. Move and rotate the HMD through the effect. Both eyes must retain matching
   trail timing/intensity without cross-eye smear, flicker, stale-eye flashes,
   world skew, or changes to shadows/reflections. Confirm ordinary tone mapping,
   bloom, fades, HUD, and reticle presentation are unchanged.
4. Recenter with F2, pause/resume, load a save, toggle F10 off/on, and simulate
   brief tracking loss. The next eligible ImageTrail pass must log `reset=1`
   or start with `clear=1`; no old history may flash into either eye.
5. Exit normally after ImageTrail allocation. Require a tracked destroy row or
   pre-graphics removal row showing the secondary pair released, followed by
   normal process exit with no double-free, hang, or background SOMA process.
6. Hard rollback: set `HPLPerEyeImageTrailControl=0` and
   `HPLPostEffectDisableImageTrail=1`. ImageTrail must return to suppression and
   all established stereo behavior must match `0.51.0`.

## 0.51.0 Comfort Vignette

1. Run the packaged doctor and confirm `version=0.51.0-comfort-vignette` and
   `fail=0`. Launch, load a save, press F10, and require
   `openxr_comfort_vignette swapchain_created` plus `Ready=1` in summaries.
2. Walk from zero to full stick and release. The center must remain clear while
   peripheral darkness scales smoothly in and out over roughly 250 ms. Require
   increasing/decreasing `comfortVignetteLevel` and submitted frames only above
   zero; no world, HUD, reticle, eye, or projection movement is acceptable.
3. With snap turn enabled, hold the turn stick after a snap. The existing short
   black guard may occur, but the held stick must not sustain the vignette.
   Switch to smooth turn and confirm turn magnitude now drives the envelope.
4. While moving, open F1, pause, enter a wall/handheld terminal, load a save,
   trigger an authored camera, and reach dead state. Each must release the mask
   without leaking gameplay movement; normal locomotion must reacquire it.
5. Use F1 `COMFORT VIGNETTE: ON/OFF` repeatedly. Confirm the ninth row fits,
   input remains exclusive, disabling clears immediately, and re-enabling does
   not rebuild resources or alter HUD shape/stereo.
6. Compare `Preset=balanced` and `maximum`, then explicitly override strength,
   inner radius, fade time, and `ComfortVignette=0`. Confirm explicit keys win.
   Exercise runtime recovery and normal exit for clean swapchain teardown.

## 0.50.0 Curved HUD

1. Run the packaged doctor and confirm `version=0.50.0-curved-hud`, then launch,
   load a save, and press F10. Require `khrCompositionLayerCylinder=1`,
   `hudCylinderRequested=1`, and no instance/session/frame failure. If the
   extension is unavailable, require one `shape_fallback ... extension_unavailable`
   row and continue on the quad.
2. Open F1. Supported runtimes must show `HUD SHAPE: CURVED`; unsupported ones
   must show `HUD SHAPE: QUAD ONLY`. Confirm the added row does not overlap the
   footer or clip at the configured 1024x512 panel size.
3. Compare curved and quad modes in gameplay, pause, inventory, subtitles,
   wake eyelids, game over, and credits. Content, alpha, vertical placement,
   center distance, pointer alignment, capture age, and native fallback must be
   identical; only horizontal curvature may change.
4. Toggle shape repeatedly while turning and translating the HMD. Expect
   `openxr_hud shape_toggle effective=cylinder|quad`, continuous stereo, no
   swapchain rebuild, and `openxr_frame ... hudShape=` matching the selection.
5. Set `HudCylinderAngleDegrees` to 45 and 120 in separate runs. The visible arc
   width and aspect-derived height must remain stable while curvature changes.
   Restore 70 after comparison.
6. Set `HudShape=quad` and relaunch for the hard rollback. No cylinder extension
   is required for submission; the HUD must match the prior flat baseline.
   Any layer validation failure must log fallback and recover on the next frame.

## 0.49.0 Readiness And Presets

1. Run packaged `somavr_injector --doctor <Soma_NoSteam.exe>`. Require OpenXR
   DLL/flavor, loader, adjacent config, active runtime JSON, x64 game, and clean
   proxy scan passes with `fail=0`. A packaged config fallback warning is a bug.
2. Place a known proxy name in a temporary game-folder copy and confirm a warning;
   select a standard/non-OpenXR DLL or remove the runtime JSON path and confirm a
   nonzero doctor exit. Do not alter the real SOMA/runtime installation.
3. Set `Preset=minimal`, `balanced`, and `maximum` on separate launches. Confirm
   `hook_config comfortPreset=...` and the documented snap/blackout/camera/post
   policy. World scale, eye height, hands, and experimental stereo toggles must
   not change between presets.
4. Under `Preset=maximum`, explicitly set `HPLComfortSuppressScriptRoll=0` and
   `ComfortBlackoutFrames=7`. The log/runtime must use the explicit values.
5. Restore `Preset=custom` and the active profile. Run F10, F1, recenter,
   same-frame toggle, controller profile, loading, pause, and normal-exit smoke
   paths. Confirm all four CTest suites and packaged `USER_GUIDE.md`/hash ledger.

## 0.48.0 Controller Profiles

1. Confirm `version=0.48.0-controller-profile-diagnostics` and five successful
   `openxr_input bindings` rows: Simple `9`, Touch `19`, Index `18`, Microsoft
   Motion `11`, and HTC Vive `15`.
2. Start F10 VR with both controllers active. Expect one profile-change row per
   hand naming the actual profile, not `none`, `unresolved`, or `query_failed`.
3. Verify movement/turn, trigger select, squeeze, menu, grip/aim poses, and
   haptics. On Vive, use trackpads for movement/turn. Existing Touch/Index/WMR
   behavior must not change.
4. Power off or hide one controller, restore it, and exercise runtime focus or
   session recovery. Profile rows must update without stale hand identity,
   stuck input, or an action-set/session error.
5. At shutdown, confirm `openxrInteractionProfileEvents` is nonzero and the
   final left/right profile names match the connected devices.

## 0.47.0 All-Stereo View History

1. Confirm `version=0.47.0-stereo-view-history`, OpenXR flavor, configured
   history, `policy=active_stereo_exact_player_fail_closed`, and
   `maxPoseFrameGap=8`.
2. Enter F10 VR but leave same-frame stereo off. The panel must report
   `VIEW HISTORY: ACTIVE`; AFR eye `0/1` restores and captures must alternate
   without pointer, eye, pose, or repeated-reset faults.
3. Recenter with F2. Expect one calibration-generation reseed, normal eye height,
   and no visible temporal kick. Load a save or interrupt tracking for more than
   eight render frames; expect one stale-gap reseed on recovery.
4. Enable same-frame stereo. Each pose frame must receive matching left/right
   transactions while upper lifecycle ownership remains single-run. Inspect
   motion, shadows, reflections, tone/bloom, fades, HUD, and authored cameras.
5. Disable same-frame stereo: AFR history isolation must continue. Exit F10:
   status returns to standby and native state remains intact.
6. Relaunch with `HPLPerEyeViewHistoryControl=0`. Status must be unavailable and
   both AFR and same-frame paths must retain native shared-history behavior.

## 0.46.0 Per-Eye View History

1. Confirm `version=0.46.0-per-eye-view-history`, OpenXR flavor,
   `HPLPerEyeViewHistoryControl=1`, and
   `hpl_per_eye_view_history initialized configured=1 packetBytes=0x40`.
2. Enter F10 VR, open F1, and enable `SAME FRAME STEREO`. `VIEW HISTORY` must
   move from `STANDBY` to `ACTIVE`; first use should log one identity reset/seed.
3. Expect alternating restore/capture rows for eyes `0/1`, with both captures
   sharing each same-frame pose identity. No `render_eye_sequence_mismatch`,
   pointer-access fault, or repeated identity reseed is acceptable.
4. Rotate and translate through quiet, shadowed, reflective, bloom/tone, fade,
   terminal, inventory, authored-camera, and loading scenes. Check rigid geometry,
   stable eye height, no cross-eye trail, and no regression in the proven
   shadow/reflection correction.
5. Toggle same-frame stereo off and on. Status must return through `STANDBY`,
   reseed once, and resume without stale history. AFR must remain clean while off.
6. Set `HPLPerEyeViewHistoryControl=0` and relaunch. The panel must report
   `UNAVAILABLE`, no packet writes may occur, and continuous stereo must retain
   its prior shared-native-history behavior.
7. Attach the complete log and note GPU/frame-pacing behavior. Other temporal
   resources remain exploratory even if this matrix is accepted.

## 0.45.0 Continuous Dual Render

1. Confirm `version=0.45.0-continuous-dual-render`, OpenXR flavor,
   `dualRenderContinuousControl=1`, `dualRenderContinuousDefault=0`, and a ready
   continuous control after hook installation. Enter VR with F10 and confirm the
   session initially remains on the proven AFR path.
2. Open F1, navigate to `SAME FRAME STEREO: OFF`, and activate it. The panel must
   change to `ON` without recentering, rebuilding the XR session, leaking input,
   or changing eye height/world rigidity.
3. Expect the first five successful rows, then bounded interval rows, to report
   `source=continuous`, `result=same_pose_opposite_eye`, opposite eyes, one pose
   frame, replay mask bit `2` clear, and `temporalDiagnostics=0`. The viewport
   enumerator, renderer frame/stat reset, update/script lifecycle, GUI, XR
   submission, and presentation must remain once per game frame.
4. Inspect quiet, shadowed, reflective, tone/bloom, fade, inventory, pause,
   subtitle, terminal, flashlight, and authored-camera scenes. Stop for skew,
   eye mismatch, moving shadows/reflections, cross-eye history, duplicated HUD,
   broken menus, simulation-speed changes, or unacceptable frame pacing.
5. Toggle the panel action off. AFR must resume immediately without a session
   reset or stale eye. Toggle on again and verify the same clean transition.
6. Automatic samples and a manual `Ctrl+F6` arm must still report temporal
   mutation/resource diagnostics while continuous mode is enabled, then return
   to ordinary continuous rows. Any cache or eye-sequence failure must disable
   continuous mode and leave AFR operational.
7. Set `HPLDualRenderContinuousControl=0` for the hard rollback. The panel action
   must show unavailable and no continuous replay may occur.

## 0.44.0 Inventory Presentation

1. Confirm `version=0.44.0-inventory-presentation`,
   `hpl_user_module_bridge install_ok`, `moduleIdOffset=0x158`,
   `inventoryModuleId=15`, and `openInventoryAction=12`. Enter the known-good
   F10 path after loading a save.
2. Open inventory once. It must appear once, head locked and identical in both
   eyes, remain visible through the native three-second hold and fade, and not
   alter world geometry, shadows, reflections, eye height, or controls.
3. Confirm one `route=inventory_presentation` row and increasing
   `inventoryCurrentImGuiCaptures`. Wait at least five seconds; expect one
   `hpl_inventory_presentation event=complete` and no continued inventory
   capture while ordinary gameplay current-ImGui owners run.
4. Exercise hints, pause/main menu, terminals, credits, wake, death/game-over,
   subtitles, descriptions, infection, and white flashes. Each must retain its
   existing path; terminal/diegetic GUI must remain in the stereo world.
5. Repeat with `HPLInventoryPresentationControl=0`. Inventory must retain native
   behavior without current-ImGui HUD capture, and all `0.43.0` presentation
   behavior must remain unchanged. Attach the complete shutdown summaries.

## 0.43.0 Scripted Presentation

1. Confirm `version=0.43.0-scripted-presentation`,
   `scriptedPresentation=1`, and exact getter RVAs `0x485200/0x485720` in the
   install rows. Enter VR with F10 before a wake, death, or credits sequence.
2. Trigger a sleep/wake sequence. Sleep must be fully black without stale eye
   frames; wake must reveal the stereo world and place both eyelids identically
   in the HUD layer for the authored duration. Confirm `wakeSetAsleepEvents`,
   `wakeStartEvents`, and `wakeCurrentImGuiCaptures` increase.
3. Die and wait for the game-over screen. Text/background must be readable and
   stereo-consistent. Dominant primary or select must continue/reload exactly
   once; locomotion and turning must remain released. Confirm dead state `17`,
   `deadCurrentImGuiCaptures`, and `gameOverContinueActions`.
4. Run credits. Confirm they remain visible through GameHudImGui capture without
   dead/wake counters increasing. Exercise pause and a terminal afterward to
   verify their existing routing is unchanged.
5. Cross a loading transition while asleep or waking. No owner may clear a
   blackout still required by the other; stereo/input must recover normally.
   Repeat with `HPLScriptedPresentationControl=0` for exact `0.42.0` behavior.

## 0.42.0 Native Gameplay Haptics

1. Start the OpenXR build, load a save, press F10, and confirm
   `hpl_gameplay_haptics install_ok function=SetRumble rva=0x109b30`.
2. Trigger player damage, a locked lever/door, and a datamining or sustained
   scripted effect. Both controllers should follow SOMA's authored intensity;
   ordinary locomotion must not buzz.
3. Hold a sustained effect for several seconds. Confirm output remains smooth,
   stops promptly on release/end, and the log shows bounded `pulse=` rows rather
   than one OpenXR request per rendered frame.
4. Confirm the shutdown summary has native calls and nonzero pulses/applied
   hands. Repeat with `GameplayHaptics=0`: native gamepad rumble and all other VR
   input must remain unchanged.
5. When a physical gamepad is available, confirm its original rumble still
   works alongside VR output. Test pause, loading, tracking loss, and session
   focus transitions for stuck vibration.

## 0.41.0 Roomscale Body Reconciliation

1. Confirm `version=0.41.0-roomscale-body-reconciliation`, native feet get/set
   signatures are `1`, `bodyReconciliationReady=1`, and the configured
   threshold/target/step/hold values are `0.45/0.25/0.015/30` meters/frames.
2. Enter the known-good F10 path in open floor space. Lean or walk less than
   0.45 m from center: no body-reconciliation step should occur and the world,
   eye height, stereo, shadows, reflections, HUD, and reticle must stay rigid.
3. Remain beyond 0.45 m for about half a second. Expect one activation and
   bounded `hpl_roomscale_body_reconciliation` steps until the residual offset
   reaches about 0.25 m. The view must not jump, drag, rotate, or change height.
4. Repeat toward a solid wall, closed/open moving door, corner, stairs, and a
   movable prop. A blocked capsule sweep must report blocks without crossing
   geometry. Move back to clear space and confirm catch-up resumes.
5. Repeat while crouched, then enter pause, terminal, grab, ladder/climb, sit,
   conversation, and an authored camera where available. Only unpaused
   Normal/Normal ownership may step the body; all other states must reset.
6. Change direction repeatedly and physically return toward center. Confirm no
   oscillation or repeated activation inside the threshold. Set
   `HPLRoomscaleBodyReconciliation=0` for exact rollback. Stop on any camera
   discontinuity, floor-height change, collision penetration, state bypass, or
   signature failure, and attach the complete log summaries.

## 0.40.0 Dual-Render Temporal Probe

1. Confirm `version=0.40.0-dual-render-temporal-probe`,
   `dualRenderAutoProbe=1`, count `3`, delay `180`, interval `180`, and the
   guarded `post_post_effects` hook installs without signature failure.
2. Load a representative save and press F10 once. Do not press another probe
   key. Expect one `hpl_dual_render_auto scheduled=1` row, then exactly three
   automatic samples spaced about 180 rendered frames apart.
3. Each sample must report `same_pose_opposite_eye`, GUI suppression, bounded
   replay duration, and paired `hpl_temporal_mutation` rows for renderer,
   current state, history state, and settings. Attach every
   `hpl_temporal_pair` row; either equivalent or eye-specific ranges are useful.
4. Repeat once near animated lights, reflections, particles, and a fade/post
   effect. The world must remain rigid and retain the proven eye height,
   shadows, reflections, HUD, reticle, audio, input, and desktop mirror.
5. Press `Ctrl+F6` at one especially active scene and confirm a fourth sample
   reports `source=manual`. Disable `HPLDualRenderAutoProbe` for direct rollback;
   disable `HPLDualRenderReplayProbe` to restore the AFR-only baseline.
6. Stop on a crash, visible temporal jump, repeated GUI, stale eye, failed
   regions, eye-sequence mismatch, or more than three automatic replays. Exit
   normally and attach the final `hpl_compat_summary`.

## 0.39.0 VR Control Panel

1. Confirm `version=0.39.0-vr-control-panel`,
   `openxr_status_panel swapchain_created`, `statusPanelReady=1`, and
   `hpl_status_panel install enabled=1`. Enter the known-good F10 VR path.
2. Press `F1`. A stable head-locked panel should appear in both eyes without
   changing world scale, eye height, shadows, reflections, HUD convergence, or
   desktop mirror output. Press `F1` again and confirm immediate removal.
3. Open with `Menu + Secondary`. Move the movement stick once in each direction
   and confirm one-row-per-deflection navigation. Dominant select or trigger
   must activate exactly once per press; no movement, turn, pause, terminal
   click, grab, or manipulation action may leak through while the panel is open.
4. Exercise recenter and roomscale. Recenter must use the stable-pose latch;
   roomscale off/on must preserve IPD, orientation, calibrated eye height, and
   collision safety on re-enable.
5. Toggle centered projection only in a scene with known shadow/reflection
   anchors, then restore it to ON. Toggle HUD and reticle off/on and confirm only
   their compositor layers change. Close through the CLOSE row and controller
   Menu, then verify ordinary controls resume with no held inputs.
6. Stop on an upside-down panel, per-eye mismatch, opaque rectangle outside the
   panel, repeated actions, input leakage, compositor failure/suspension, or any
   world-render regression. Attach the log with `hpl_status_panel_summary` and
   `openxrStatusPanelSubmittedFrames`.

## 0.38.0 Diegetic Terminal Pointer

1. Confirm `version=0.38.0-terminal-pointer`,
   `hpl_terminal_bridge install_ok ... hooked=sendMouseVirtual`, and no signature
   failure. Keep `TerminalPointer=1` for this pass.
2. Enter F10 VR, focus a normal wall terminal, and confirm player state changes
   to `terminal(8)`. The policy row must show `terminalPointer=1`; paused-menu
   pointer and locomotion/turn routes must be inactive.
3. Aim the dominant controller around the terminal. Confirm the native cursor
   follows without moving the HMD and `hpl_terminal_pointer applied` reports the
   exact current non-HUD ImGui, a 3D set, sane virtual size/offset, and bounded
   relative deltas.
4. Select several widgets with dominant select/trigger. Each physical press and
   release must produce one native click transition and optional
   `terminal_click` haptic; entering the terminal while trigger is held must not
   create repeated clicks.
5. Exit, pause, open inventory, and use ordinary gameplay. The terminal pointer
   must disappear immediately and native menu/HUD/gameplay behavior must remain
   unchanged. Repeat after temporary controller tracking loss, then repeat the
   aim/click/exit checks with a handheld terminal in player state `9`.
6. Set `TerminalPointer=0` and repeat. SOMA's original projected mouse path must
   be restored completely. Stop on a HUD owner match, non-3D mutation, cursor
   drift while the controller is still, reversed axes, or a click held after
   terminal exit.

## 0.37.0 Post-Effect Resource Ownership

1. Confirm `version=0.37.0-post-resource-probe`,
   `postEffectResourceProbe=1`, `post_effect_render_one` hook success, and
   `textureQuery=1`. Stop on a signature failure or missing GL helper.
2. Enter F10 VR in a quiet lit scene and wait for both AFR eyes. Confirm named
   `hpl_post_effect_resources` rows contain nonzero texture dimensions and/or
   framebuffer bindings without continuous per-frame log spam.
3. Press `Ctrl+F6` once. The replay must still report
   `same_pose_opposite_eye`; matching effect rows must show the same nonzero
   `pairedPoseFrame`, `pairComparable=1`, and an explicit
   `eyeOwnership=shared_across_eyes` or `eye_distinct` result.
4. Repeat in a bright bloom/tone-mapping view, during an authored image fade,
   and where a named video/screen effect is active. Use `Ctrl+F12` when needed
   to isolate one effect, then `Shift+F12` to restore the chain.
5. Stop on changed visuals, missing effects, new stereo mismatch, texture/FBO
   overflow, zero-sized resources, a large sustained frame-time regression, or
   ownership classification without a same-pose pair.
6. Set `HPLPostEffectResourceProbe=0` to verify independent rollback; post
   policy, AFR, dual-render arming, HUD, depth, and OpenXR submission must remain
   unchanged.

## 0.36.0 Two-Hand Tools And Grab Rotation

1. Confirm `version=0.36.0-two-hand-tools`, `twoHandHudObject=1`, and
   `twoHandRotation=1` in the hands/grab install rows. Enter F10 VR with both
   grip poses tracked.
2. Trigger an interaction that creates exact `HudObject`. Move it with the
   dominant controller, squeeze the support grip, and move only the support
   controller. The object should remain rooted at the dominant hand and aim
   along the hand-to-hand line without changing native scale, animation, depth,
   collision, identity, or callbacks.
3. Release support squeeze, cross the minimum/maximum hand separation, and
   briefly lose support tracking. Each case must return immediately to the
   dominant-grip basis without a snap, stale direction, or unrelated entity
   override. Check `twoHandHudCandidates/Overrides/Fallbacks`.
4. Grab a movable physics body. Engage support squeeze, pause for the re-anchor
   call, then rotate the support hand around the dominant hand. Expect
   `hpl_grab_two_hand transition=engaged`, bounded
   `mode=two_hand_direction` torque rows, and native translation/physics.
5. Release and re-engage support squeeze while holding light and heavy bodies.
   Each transition must log `policy=reanchor_before_torque`; stop on a torque
   spike, object teleport, joint/collision break, oscillation, or persistent
   rotation after release.
6. Roll back independently with `TwoHandHudObject=0` or
   `TwoHandGrabRotation=0`. Dominant-only HudObject and Grab translation,
   rotation, throw, interaction, one-hand fallback, and authored-camera behavior
   must remain unchanged.

## 0.35.0 Same-Frame Dual-Render Probe

1. Confirm `version=0.35.0-dual-render-probe`,
   `dualRenderReplayProbe=1`, and `dualRenderKey=Ctrl+F6`. Load a save and enter
   the known-good F10 VR path before arming the experiment.
2. In a quiet scene, press `Ctrl+F6` once. Expect one `hpl_dual_render armed`
   row, one `openxr_stereo_cache ... source=dual_render_first_eye` row, and one
   `hpl_dual_render replay` row. No later frame may replay without a new press.
3. The replay result must be `same_pose_opposite_eye`: eye indices differ and
   both pose-frame values match. `replayMask` must equal `originalMask` with bit
   `2` removed, `screenGuiSuppressed=1`, and the next frame-boundary capture
   must preserve the second eye.
4. Inspect world rigidity, shadows, reflections, particles, animation, HUD,
   subtitles, audio, and interaction immediately before/after the press. Stop on
   a crash, simulation advance, visible one-frame GUI duplication, temporal
   contamination, wrong eye, or persistent visual state.
5. Press `Ctrl+F6` at least three more times in scenes with active post effects,
   dynamic shadows, and reflections. Retain replay duration/draw/clear totals
   and the final transaction/compatibility summaries. The duplicated
   post-post callback is expected telemetry, not yet accepted for sustained use.
6. Roll back with `HPLDualRenderReplayProbe=0`; AFR stereo must remain unchanged.
   Ordinary F6 render diagnostics and all F10/F12 controls must still work.

## 0.34.0 Subtitle Presentation And Paused Menu Layer

1. Confirm `version=0.34.0-subtitles-menus`,
   `hpl_subtitle_bridge install_ok`, and
   `hpl_hud_bridge installed ... layer=1 pausedMenu=1` with all signatures valid.
2. Load a save and press F10 once. Trigger short, long, multiline, named-speaker,
   and gradual-display subtitles if available. Text should be approximately 15%
   larger and wrap 10% narrower without changing content, reveal/audio timing,
   language, speaker identity, or native enable settings.
3. Confirm bounded `hpl_subtitle_layout` rows report the shipped baseline near
   `width=860 y=700 font=26 shadow=1`, scaled VR values, and `restored=1`.
   Large-font mode must produce its own valid native baseline rather than being
   forced back to normal-font values.
4. Pause gameplay. The exact current ImGui set should report
   `pause={enabled=1 valid=1 paused=1 capturedCurrent=1}` and render once on the
   stable HUD quad. Controller pointer/click alignment and gameplay suppression
   must remain correct through resume.
5. Open inventory, a terminal, and any non-paused ImGui surface. These must not
   be captured merely because they are current; diegetic UI remains world owned.
6. Roll back each feature independently: `HPLSubtitleControl=0` restores native
   subtitle layout, and `HudCapturePausedMenu=0` restores native pause rendering.
7. Regression-test HUD transparency/order, center-crosshair clearing, depth,
   shadows/reflections, eye height, room scale, loading/reload, tracking recovery,
   and clean shutdown. Final summaries should have zero invalid layout/renderer,
   pause-query, and capture fallbacks.

## 0.33.0 Depth Submission And XR Resource Recovery

1. Confirm `version=0.33.0-depth-resources`, `DepthCompositionSubmit=1`, and one
   `openxr_depth_capability` row with the extension enabled and
   `submissionImplemented=1`.
2. Press F10 after loading a save. Confirm `openxr_gl_bridge ready` reports two
   depth caches and two depth swapchains. Confirm `format_selected` matches the
   reported source stencil topology. If no supported standard depth format is
   exposed, confirm an explicit `no_supported_depth_format` warning and
   intact color-only stereo.
3. Inspect near geometry, distant geometry, transparent/reflection surfaces,
   HUD, shadows, and head translation. Confirm stereo remains rigid and the log
   accumulates `depthSubmitted` without `copy_failed` or frame failure rows.
4. Exercise pause, loading, save reload, fullscreen/window transitions, and HMD
   sleep/wake. `openxr_view_resources` should remain stable or log one complete
   rebuild; a changed HDC/HGLRC must enter runtime recovery instead of using stale
   swapchains. Confirm F10 VR returns without restarting SOMA.
5. Set `DepthCompositionSubmit=0` as the immediate rollback. The same run should
   retain color stereo and depth probes while submitting no depth chain.

## 0.32.0 Viewport Ownership, Depth, And Release Safety

1. Confirm `version=0.32.0-viewport-depth`, press F10 once after loading a save,
   and verify normal eye height, rigid head rotation, stereo, shadows, and
   reflections without extra F8/F11 presses.
2. Find `hpl_viewport_identity` rows. The gameplay viewport must become
   `role=player`; other camera pointers should be `secondary` or `unresolved`
   and retain `policy=player_camera_only_receives_vr_controls`.
3. Exercise a reflective room, terminal, loading transition, and save reload.
   No secondary view may consume F10/F11, replace the active VR camera, or gain
   headset yaw/pitch/roll. Retain the final camera and compatibility summaries.
4. Find `openxr_depth_cache_probe` for eye `0` and `1`. Record `depthBits`, all
   three GL errors, `valid`, `centerFinite`, and center min/max. Both eyes should
   become valid and depth should vary in mixed near/far scenes. No compositor
   depth behavior is expected yet.
5. Run the injector against an ordinary SOMA directory and expect a clean
   compatibility message. If graphics proxies or API layers are present, verify
   warnings name the file but do not block. A second injection must be refused.
6. From the ZIP, install to a temporary directory, edit `somavr.ini`, then run
   install/update again. The edit must survive and new defaults must appear as
   `somavr.defaults.ini`. Uninstall must remove managed files while preserving
   `somavr.ini` unless `-RemoveConfig` is supplied.
7. Regression-test HUD, reticle, hands/tools, flashlight, locomotion, physical
   interactions, menus, loading, audio, tracking recovery, and clean shutdown.
   Attach the complete log.

## 0.31.0 Tools, Gameplay ImGui, And Render Transaction

1. Confirm `version=0.31.0-tools-hud`, `controllerHudObject=1`, and
   `hpl_hands_bridge install_ok ... controllerHudObject=1` after launch.
2. Load a save, press F10 once, and use a prop/tool interaction that creates the
   exact script name `HudObject`. Expect `hpl_entity_identity ... hudObject=1`
   followed by `hpl_hud_object_pose ... requested=1 overridden=1`.
3. Move and rotate the dominant controller. The interaction object should follow
   the grip at stable world depth and scale. It must not remain camera locked,
   jump between eyes, or lose native interaction callbacks.
4. End that interaction and start it again. Expect
   `hpl_hands_entity_destroy ... cacheInvalidated=1 relevant=1 name=HudObject`,
   then a fresh identity row if SOMA reuses the pointer. The new object must not
   inherit any previous entity's policy.
5. Equip an inventory/Omnitool entity ending in `_HudObject`. Expect
   `socketedHudObject=1`; it should remain attached to the controller-driven hand
   animation, with no `hpl_hud_object_pose` override for that entity.
6. Trigger an authored camera or temporarily remove dominant-hand tracking. The
   independent object must immediately use its native matrix and later recover;
   summary fallback counters should explain the transition.
7. Exercise crosshair/descriptions, hints, inventory, subtitles, pause UI, and a
   terminal. Exact GameHudSet and `gameHudMatch=1` rows may capture into one HUD
   quad; pause/current ImGui and diegetic sets must remain native. Confirm
   `gameHudImGuiCaptures` becomes nonzero when dedicated gameplay ImGui draws.
8. Confirm the combined HUD retains transparency and that the center-crosshair
   clear does not erase unrelated central ImGui content. F10 off and XR focus
   loss must preserve native UI visibility.
9. Find periodic `hpl_render_transaction` rows. Record `order`, all six counts,
   per-stage draws/clears/durations, and `repeatCandidate`; no second render is
   attempted in this build.
10. Regression-test rigid world geometry, eye height, shadows/reflections, room
   scale, flashlight, interaction ray, menu pointer, loading, save transition,
   and clean shutdown. Attach the log.

## 0.30.0 Screen Materials

1. Confirm `version=0.30.0-screen-effects` and
   `hpl_screen_effect_bridge install_ok ... distanceScale=10.000` with no
   signature or partial-install failure.
2. Load a save and press F10. Trigger scripted damage, infection, distortion,
   flash, or other sequences that call `Effect_Screen_Start`. Each live effect
   should log one exact `Screen Particle<decimal>` creation.
3. Hold the head still, close either eye in turn, and inspect the material. It
   should cover the intended view without painful 15 cm convergence, eye-local
   displacement, changed aspect, or altered opacity/timing.
4. Rotate and translate the HMD. The material should remain camera-relative and
   stable while the world remains rigid. It must not affect ordinary particles,
   billboards, HUD, shadows, reflections, or controller-held models.
5. Toggle F10 off while an effect is live and confirm native size/placement
   resumes; toggle on and confirm the comfortable distance returns. Destroyed
   effects must emit matching destroy rows and never contaminate a later object
   reusing the same address.
6. Recheck loading, authored cameras, HUD/menu, stereo recovery, and shutdown.
   Retain the final bridge summary; `positionFallbacks` should be zero in stable
   gameplay.

Rollback with `HPLScreenEffectControl=0`; tune only convergence distance with
`HPLScreenEffectDistanceMeters`.

## 0.29.0 Presentation And Optics

1. Confirm `version=0.29.0-presentation-optics`, all three optics channels,
   loading control, two exit-black frames, and video lifecycle probing are on.
2. Press F10 and exercise a terminal/handheld terminal, conversation, and any
   scripted zoom. Headset FOV and aspect must remain stable while bounded
   `hpl_comfort_optics` rows show requested versus neutral targets. Disable F10
   and confirm native authored zoom returns.
3. Load a save and cross a map boundary. Confirm one load entry immediately
   invalidates stereo and submits zero XR layers, controller input is released,
   and one load exit invalidates again. Stereo must repopulate automatically
   after the bounded exit guard without F10/F11.
4. The desktop should retain SOMA's native loading presentation while the HMD is
   black. Stop for a stale eye, stuck key/button, black hang, or a loading frame
   shown as an uncomfortable world-depth projection.
5. Exercise available intro, terminal, or campaign video content. Record
   `hpl_video_lifecycle` names and pairing; playback must remain native and each
   created stream should be destroyed or explained at shutdown.
6. Recheck shadows, reflections, eye height, HUD, flashlight, locomotion,
   tracking recovery, and normal shutdown.

Rollback independently with `HPLComfortOpticsControl=0`,
`HPLLoadingScreenControl=0`, or `HPLVideoLifecycleProbe=0`.

## 0.28.0 Authored Comfort

1. Confirm `version=0.28.0-authored-comfort`, camera roll and DoF controls are
   enabled, `StateTransitionBlackoutFrames=2`, and VideoDistortion policy is on.
2. Press F10 in normal gameplay. Walk, run, lean, and crouch; the world must stay
   rigid and level while `hpl_comfort_camera_roll` classifies Move/Lean calls.
3. Exercise a ladder and ledge climb. Confirm state IDs `11/12`, one short black
   guard on entry/exit, native constraints, and suppressed Climb roll without a
   persistent black frame or lost tracking.
4. Trigger sit, interactive camera animation, conversation, and death/reload.
   Verify state IDs `15/14/16/17`, bounded transition rows, continuous head
   tracking, and native scripted camera/FOV behavior. Script roll should remain
   available unless explicitly enabled for suppression.
5. Visit a DoF-heavy or VideoDistortion sequence. Confirm suppression telemetry
   appears only while VR is active; fades, tone mapping, menus, and loading must
   remain visible.
6. Exit F10 VR and repeat one roll/DoF request. Native behavior must return.
   Verify shadows, reflections, eye height, HUD, controller input, save/load, and
   normal shutdown preserve the proven baseline.

Rollback independently with `HPLComfortCameraRollControl=0`,
`HPLComfortDepthOfFieldControl=0`, `HPLPostEffectDisableVideoDistortion=0`, or
`StateTransitionBlackoutFrames=0`.

## 0.27.0 Gameplay Coherence

1. Confirm `version=0.27.0-gameplay-coherence`,
   `flashlightGameplayRay=1`, and room-scale install telemetry reports
   `roomscaleSafetyDynamic=1` / `staticOnly=0`.
2. Press F10 in a loaded save. Aim the flashlight away from gaze at an agent or
   scripted light-sensitive target. Confirm visual illumination and gameplay
   response follow controller yaw/pitch/roll, with bounded
   `hpl_flashlight_gameplay_ray ... policy=preserve_random_cone` rows.
3. Move the controller through the native cone while holding the head still.
   Rays should retain subtle spread, start at the rendered light, and never snap
   to gaze. Lose tracking and enter an authored camera; both visual and gameplay
   paths must fall back together.
4. Exercise normal tool interaction and a camera-animation/grounding sequence.
   The summary should classify only flashlight-length camera-origin candidates;
   interaction at length `3` and grounding at `100` must remain native.
5. Lean near static walls, a moving door, and a movable prop. Dynamic-inclusive
   head-volume probes should clamp before clipping without pinning, chatter,
   eye/hand separation, or changing SOMA's player capsule.
6. Verify shadows, reflections, eye height, locomotion, HUD, stereo, tracking
   recovery, save/load, and normal shutdown retain the proven baseline.

Rollback either addition independently with `ControllerFlashlightGameplayRay=0`
or `HPLRoomscaleSafetyDynamic=0`. Stop for false ray classification, agent aim
still following gaze, dynamic-contact jitter, or any partial hook installation.

## 0.26.0 GPU And Depth Capability

1. Confirm `version=0.26.0-gpu-depth-probe`, `perEyeGpu=1`,
   `gpuQueryPairs=128`, and `hpl_per_eye_gpu ready ... nonBlocking=1` after F10.
2. Play through a representative lit room, post-effect-heavy area, menu, and
   interaction. Confirm `hpl_per_eye_gpu` has nonzero left/right timings;
   `dropped` should remain zero or bounded and `invalid=0`.
3. Confirm one `openxr_depth_capability` row reports extension available/enabled
   state, nonzero `glDepthBits`, depth range, projection type, and finite positive
   HPL near/far. An unavailable extension is a valid probe result.
4. Verify no hitching, world skew, stereo mismatch, shadow/reflection change,
   startup failure, or shutdown regression. Preserve the complete log.

Stop for sustained dropped samples, timer-query GL errors, or a visual/frame-
pacing regression. The two new probes can be disabled independently.

## 0.25.0 Head Volume, Spectator, And CPU Telemetry

1. Confirm `version=0.25.0-volume-spectator-telemetry`,
   `desktopMirrorEye=left desktopMirrorAspect=fit`,
   `roomscaleRadiusMeters=0.090 roomscaleVerticalRadiusMeters=0.120`, and
   `hpl_compat_probe ... perEyeCpu=1` in startup rows.
2. Load a save and press F10 once. Confirm stereo reaches warm state and one
   `openxr_desktop_mirror applied ... eye=left aspect=fit` row appears without an
   XR frame failure.
3. Inspect the desktop while rotating and translating. It must remain one stable
   left-eye view rather than alternating eyes; the headset image, IPD, projection,
   HUD layer, and frame submission must be unchanged. Black bars are expected
   when the window and eye-cache aspect ratios differ.
4. Set `DesktopMirrorEye=right` for one run and verify the eye changes. Exercise
   `fill` and `stretch`, then set `native` and verify SOMA's original alternating
   backbuffer returns. Any mirror failure must log `fallback=native_backbuffer`.
5. In open space, confirm room-scale rows report `probes=9 validProbes=9` and no
   clamp. Slowly approach flat walls, diagonal corners, ceiling edges, and low
   static geometry; at least one blocked probe should stop the shared head pose
   before clipping without eye, hand, flashlight, or reticle separation.
6. Recenter beside tight authored geometry and transition maps. Skipped probes
   may increase, but the view must not remain pinned after stepping into open
   space. Dynamic doors remain outside this static-only acceptance gate.
7. Let gameplay run for at least 240 frames. Confirm periodic
   `hpl_per_eye_cpu` rows for all six stages with increasing left/right calls and
   plausible nonzero averages; mono calls may cover startup and non-VR frames.
8. Exit normally and retain final CPU totals, spectator frame/failure counters,
   room-scale probe/query/skip counters, and the normal lifecycle summary.

Stop for headset changes caused by spectator mode, desktop corruption, GL-state
leakage, a persistent clamp in open space, stereo divergence, or stage timing
that attributes all active VR work to mono.

## 0.24.0 Room-Scale Safety

1. Launch the OpenXR Release build and confirm
   `version=0.24.0-roomscale-safety` plus
   `hpl_camera_bridge install_ok ... roomscaleSafety=1 ... staticOnly=1`.
2. Load a save, press F10 once, and wait for stable activation. Do not press F4;
   the active profile should report `roomscale=1`.
3. In open floor space, translate in all axes. The view, both hands, flashlight,
   reticle, and interaction ray must remain coherent; safety rows should normally
   report `queried=1 clamped=0 factor=1.00000`.
4. Slowly lean toward flat static walls, corners, ceiling geometry, and a low
   obstacle. The view must stop before clipping and emit bounded
   `hpl_roomscale_safety ... clamped=1` rows without stereo divergence or jitter.
5. Rotate the HMD while clamped and move the controllers independently. The world
   must remain rigid, IPD unchanged, and controller-relative hand/flashlight aim
   stable. Step back and confirm full translation resumes immediately.
6. Press F2 while clear, repeat a wall approach, then load another save. Recenter
   and camera replacement must invalidate the cached result; no stale wall plane
   may constrain the new origin.
7. Approach a moving door separately. Record behavior, but do not fail this build
   because `staticOnly=1` deliberately excludes dynamic geometry.
8. Press F4 to disable room scale and confirm physical translation ceases. Restore
   it and ensure safety resumes without a jump. Exit normally and preserve the
   final safety query/block/clamp/fallback counters.

Stop immediately for a crash, world skew, eye mismatch, height regression,
clamp persisting in open space, or hands/flashlight separating from the HMD.

## 0.23.0 Controller Flashlight

1. Launch `build-openxr\Release`, load a save, and press F10. Confirm
   `version=0.23.0-controller-flashlight`, `controller_config ... flashlightAim=1`,
   and `hpl_hands_bridge install_ok ... flashlightAim=1` without a signature or
   identity failure.
2. Toggle the flashlight with the support-hand action. Aim the dominant
   controller independently of the HMD through yaw, pitch, and roll. The beam
   and illuminated surfaces should follow the controller without moving the
   world, camera, HUD, or hand root.
3. Confirm one `hpl_entity_identity ... name=Flashlight flashlight=1` row and
   recurring `hpl_flashlight_pose ... requested=1 overridden=1` rows. Compare
   `aimPos`, `aimForward`, and `finalPos`; adjust only the flashlight offset or
   rotation calibration if the physical controller profile needs alignment.
4. Briefly lose dominant-hand tracking, pause, enter an authored camera, then
   recover. The beam must fall back to SOMA's camera-mounted transform without
   jumping, disappearing permanently, or remaining attached to stale tracking.
5. Check environment particles and an NPC/light-sensitive interaction where
   available. General spotlight frustum/sensor behavior should follow the beam;
   note any discrepancy in randomized agent-gobo detection, whose shipped
   helper still samples camera pitch/yaw.
6. Regress world rigidity, shadows/reflections, stereo, native reticle, HUD,
   physical manipulation, grab/throw, save/load, map change, tracking recovery,
   and shutdown. Preserve `hpl_hands_bridge_summary` and `openxr_summary`.

## 0.22.0 Physical Manipulation And ImGui Identity

1. Launch the OpenXR Release DLL, load a save, and press F10. Confirm
   `version=0.22.0-physical-manipulation`, `hpl_input_bridge install_ok` reports
   `manipulationMotion=1`, and `hpl_hud_bridge installed` lists all three ImGui
   RVAs without an identity/signature failure.
2. Use a wheel, slider/drawer, hinged door, lever, and tear interaction where
   available. Hold the normal dominant interaction control and move that hand
   left/right/up/down. Expect native object motion plus `entered state=...` and
   bounded `hpl_manipulation_motion event=...` rows for states `3..7`.
3. Walk or lean the HMD and controller together while holding an interaction.
   That common translation should not drive the object. Move only the hand and
   confirm it does. Tune axis signs and `ManipulationMotionPixelsPerMeter` if
   direction or sensitivity needs calibration.
4. Release/re-grab, change interaction type, briefly lose controller tracking,
   and open a pause menu. Every reacquisition must anchor without an initial
   jump; logs may report one tracking loss but no stale delta or stuck input.
   Grab/Push translation, rotation, and throw behavior must be unchanged.
5. During gameplay, inventory, hints, pause menu, load/save, death/game-over,
   wake, credits, and video transitions, preserve representative `hpl_gui_set`
   rows. Record `currentMatch`, `gameHudMatch`, pointer pairs, and render stage.
6. Regress world rigidity, shadows/reflections, native reticle, HUD alpha,
   hands, locomotion, authored cameras, save/load, tracking loss, and clean
   shutdown. Preserve `hpl_input_bridge_summary`, `hpl_hud_summary`, and
   `openxr_summary`.

## 0.21.0 Native Semantic Reticle And Focus Profiles

1. Launch the OpenXR Release DLL, load a save, and press F10. Confirm
   `version=0.21.0-semantic-reticle`, `hpl_crosshair_bridge install_ok`,
   `openxr_interaction_reticle native_assets_loaded count=34`, and no script,
   asset, reticle, or OpenXR signature/resource failure.
2. Aim at pickup, carry, push/pull/rotate, button, terminal, read/examine,
   traversal, conversation, recharge, unavailable, and no-hints targets where
   practical. The depth reticle should use SOMA's matching native artwork and
   change broad intent color without stretching the source aspect.
3. Compare each icon with the flat native HUD before and after F10. Logs should
   report the same `hpl_crosshair_semantic ... state=N name=...`; a missing or
   malformed icon must show the procedural cross rather than a blank layer.
4. Sweep across usable, busy/unavailable, ambiguous default, and noninteractive
   geometry. Semantic icons must clear within the configured age bound. Focus
   haptics must fire once per new confirmed target, remain silent for the default
   cursor, and feel subtly distinct for pickup versus manipulation/unavailable.
5. Set `InteractionReticleNativeIcons=0` and repeat one target to confirm the
   procedural colored cross fallback. Set `InteractionReticleSemantic=0` only
   for diagnosis to recover the `0.20.0` raw-pick behavior, then restore both.
6. Regress shadows/reflections, world rigidity, HUD alpha, hands, grab/throw,
   menu pointer, comfort blackouts, authored cameras, save/load, tracking loss,
   and clean shutdown. Preserve `hpl_crosshair_bridge_summary`,
   `hpl_interaction_bridge_summary`, and `openxr_summary` rows.

## 0.20.0 Controller Depth Reticle And Focus Haptics

1. Launch the OpenXR Release DLL, load a save, and press F10. Confirm
   `version=0.20.0-depth-reticle`, `openxr_interaction_reticle swapchain_created`,
   and no reticle, interaction-bridge, or OpenXR signature/resource failure.
2. Aim at native pick targets from roughly 0.2 to 8 meters. The cyan reticle
   should sit at the target depth with comfortable stereo convergence and nearly
   constant apparent size; it must follow controller aim rather than head gaze.
3. Move between targets and empty space. The reticle must clear within the
   configured age bound on no hit, tracking loss, authored-camera fallback, F10
   off, or a target outside the configured distance range. It must never freeze.
4. With `FocusHaptics=1`, entering a new native entity/body should produce one
   subtle pulse on the dominant hand. Holding focus must not buzz continuously;
   rapidly crossing edges must remain bounded by `FocusHapticCooldownFrames`.
5. Verify the fixed gaze crosshair remains suppressed but descriptions and other
   gameplay HUD survive. The generic reticle is allowed on any closest-entity
   result in this build; record cases where SOMA displays a different icon or
   rejects interaction so the semantic owner can be mapped next.
6. Regress shadows/reflections, world rigidity, HUD alpha, hands, grab/throw,
   menu pointer, comfort blackouts, authored cameras, save/load, tracking loss,
   and clean shutdown. Preserve `openxr_frame`, `openxr_summary`, and
   `hpl_interaction_bridge_summary` rows.

## 0.19.0 Native Comfort And Focus Snapshot

1. Launch the OpenXR Release DLL, load a save, and press F10. Confirm
   `version=0.19.0-comfort-focus`, `hpl_comfort_bridge install_ok` reports
   `bob=1 shake=1 sway=1`, and no signature mismatch is present.
2. Walk, sprint, stop, turn, crouch, and take a safe impact. HMD motion must stay
   rigid while native walking bob, camera shake, and sway are absent. Crouch
   height and body movement must still work normally.
3. Exercise a ladder, crawl space, terminal, scripted camera, conversation, and
   death/load transition where practical. Their authored camera offsets must be
   preserved. Logs should suppress only types `1`, `2`, and `9`.
4. Aim the dominant controller at several interactive and noninteractive targets
   at different distances. `hpl_interaction_ray` should report `hitSnapshot=1`,
   finite `hitDistance`, a stable `hitWorld`, and sensible non-null entity/body
   ownership where SOMA supplies it. No hit must report `hitSnapshot=0`.
5. Briefly lose tracking and toggle F10 off/on. Native camera adds must pass
   through while tracking is inactive, then suppression must resume without a
   stale offset or camera jump. Interaction snapshots must not survive no-hit or
   invalid-pose periods.
6. Regress shadows/reflections, stereo rigidity, HUD alpha/crosshair clear,
   controller hands, grab/rotate/throw, menu pointer, authored cameras, save/load,
   and clean shutdown. Preserve comfort and interaction summary rows.

## 0.18.0 Grab Rotation, Physical Throw, And Crosshair Filter

1. Launch the OpenXR Release DLL, load a save, and press F10. Confirm
   `version=0.18.0-interaction-polish`, `hpl_grab_bridge install_ok` reports
   `rotation=1 throwRedirect=1`, and the AddImpulse signature patch succeeds.
2. Pick up several light and heavy bodies. Rotate the dominant controller slowly
   through yaw, pitch, roll, and mixed axes. The object should settle to the same
   relative orientation without continuous spinning, axis swaps, pickup jumps,
   violent torque, or losing native collision. Expect increasing
   `rotationSubstitutions` and bounded `hpl_grab_rotation` rows.
3. Briefly lose controller tracking and leave/re-enter Grab. Native behavior must
   resume immediately; reacquisition must create a fresh anchor without a snap.
   If every axis is inverted together, set `GrabRotationSign=-1` and rerun.
4. Throw a held body with dominant primary at slow and fast controller speeds in
   several directions. Expect one `hpl_controller_throw applied=1` per Grab throw,
   `source=velocity` above threshold and `source=grip_forward` below it. Push-state
   cancel/throw and unrelated impulses must remain native.
5. Compare slow/fast distance with `ThrowVelocityScale=1`; scaling is clamped to
   `1.0..2.0` around `ThrowVelocityReference`, so it never weakens the authored
   impulse. Set it to `0` if direction is correct but authored object classes
   need their original fixed throw strength.
6. Trigger every crosshair icon and central interaction prompt. The gaze crosshair
   should be absent from the OpenXR HUD while descriptions, status effects, and
   noncentral HUD remain intact. If useful content is clipped, reduce
   `HudCrosshairClearRadiusPixels` or disable `HudSuppressCenterCrosshair`.
7. Regress HUD alpha, menus, hands, interaction ray, stereo rigidity,
   shadows/reflections, authored cameras, save/load, and clean shutdown. Preserve
   the final grab, HUD, OpenXR, and lifecycle summary rows.

## 0.17.0 Physics Input And Native Grab

1. Launch the OpenXR Release DLL and press F10 after loading a save. Confirm
   `version=0.17.0-physics-input`, `hpl_grab_bridge install_ok`, and no signature
   or hook failures.
2. Walk forward while facing several headings. With `MovementReference=head`,
   forward follows HMD yaw, but looking up/down or rolling the head does not
   skew, accelerate, or tilt movement. Verify run, collision, stairs, and the
   semantic fallback on ladders/terminals.
3. Stand naturally after F10/recenter, lower the HMD by at least the configured
   enter distance, then rise above the exit threshold. Confirm one crouch toggle
   each way, no threshold chatter, and sensible behavior after F2/F10 recenter.
4. Pick up light, heavy, jointed, and collision-constrained objects. The pickup
   must not jump. Moving the dominant controller should move the held target
   through SOMA's native physics; tracking loss or leaving Grab must return to
   the native camera target without a crash or runaway force.
5. During Grab/Push/Rotate interactions, hold support squeeze and use the turn
   stick to exercise native InteractRotate. Press dominant primary to test the
   native throw/cancel route. Confirm jump, sprint, crouch, and recenter do not
   fire accidentally in manipulation states.
6. Capture representative slow rotation and fast release motion. Preserve
   `hpl_grab_torque_probe`, `hpl_native_throw`, `openxr_input state`, and both
   bridge summary rows for controller-orientation and throw-scale correlation.
7. Regress hands, interaction ray, HUD/menu pointer, stereo geometry, shadows,
   reflections, authored camera sequences, save/load transitions, and shutdown.

## 0.16.0 Controller Hands, Pause Safety, And Menu Pointer

1. Launch the OpenXR Release build, load a normal gameplay save, and press F10
   once. Confirm `version=0.16.0-controller-hands`,
   `hpl_hands_bridge install_ok ... controllerRoot=1`,
   `hpl_menu_bridge install_ok enabled=1`, and `pauseSignature=1`.
2. Trigger a normal quarter-scale hand or equipped-tool animation. The visible
   hands/tool should follow dominant grip translation, yaw, pitch, and roll while
   native mesh animation and `R_Hand` attachments continue. Expect
   `rootRequested=1 rootOverridden=1`, finite `rootPos`, and increasing
   `rootOverrides`.
3. Check orientation and root placement with the default calibration. If needed,
   change only `HandRootOffsetX/Y/Z` or `HandRootPitch/Yaw/RollDegrees`, relaunch,
   and record the useful values. Do not compensate through world scale or IPD.
4. Exercise a ladder/crawl/special interaction, full-scale hand animation,
   authored camera, and brief controller tracking loss. Each must immediately
   restore SOMA's original hand matrix; fallback counters should identify scale,
   state, authored, pose, or stale ownership. Returning to Normal/Normal with a
   fresh grip should resume controller ownership automatically.
5. Open the pause menu while holding movement, turn, sprint, and trigger. The
   player must remain still and must not queue movement or interaction for menu
   exit. Periodic rows should report `paused=1 gameplaySuppressed=1` with no held
   W/A/S/D or sprint state.
6. While paused, point the dominant controller around the menu. SOMA's native
   cursor should follow the aim smoothly; trigger/select should click existing
   buttons and pulse once. Expect `hpl_menu_pointer applied` rows and nonzero
   `menuPointerFrames`.
7. Close the menu while still holding trigger. No world interaction may fire
   until trigger is released and pressed again. Set `MenuPointer=0` only if the
   native cursor path is incompatible with the current window mode.
8. Regression-test HUD alpha, rigid stereo, shadows/reflections, interaction ray,
   snap turn, recenter, save/load, and shutdown. Attach the log and final build
   manifest.

## 0.15.0 Gameplay HUD OpenXR Quad

1. Launch the OpenXR Release build, load a gameplay save, and press F10 once.
   Confirm `version=0.15.0-hud-layer`,
   `hpl_hud_bridge installed ... layer=1`, and
   `openxr_hud swapchain_created size=1600x900`.
2. Show the crosshair, interaction description, infection border, and a white
   flash if safely available. Matching rows must report
   `gameHud=1 hudCapture={enabled=1 started=1 completed=1}`; OpenXR frame rows
   should report `layers=2 ... hud=1` while current HUD content is captured.
3. Move and rotate the HMD quickly. The HUD must remain rigidly head locked,
   appear once rather than at different eye depths, preserve transparent areas,
   and show no dark rectangle, upside-down image, clipping, or alpha fringe.
4. Check text/crosshair scale and comfort at the default 1.5 m distance and
   1.6 m width. Adjust `HudDistanceMeters`, `HudWidthMeters`, and
   `HudVerticalOffsetMeters` only after recording the default result.
5. Open inventory, pause/main/load/game-over UI, show subtitles, and use a
   terminal. Only exact gameplay rows may report `gameHud=1`; all diegetic/3D
   GUI must remain in the stereo world. Record which flat surfaces still render
   natively so the next ImGui/menu classifier has evidence.
6. Before F10, after toggling VR off, and during runtime focus loss, the HUD must
   remain on SOMA's native backbuffer path. There must be no invisible UI.
7. Exercise recenter, snap turn, map/load transition, tracking loss/recovery,
   and the known rigid shadow/reflection scenes. Confirm no world, eye-height,
   input, post-effect, or shutdown regression.
8. Exit and attach the log. Expect nonzero `hpl_hud_summary` matches/captures
   and `openxrHudSubmittedFrames`, with zero submission failures and
   `openxrHudSuspended=0`.

## 0.14.0 Native Analog Locomotion And Exact-Angle Turn

1. Launch the OpenXR Release build, load a gameplay save, and press F10 once.
   Confirm `version=0.14.0-native-locomotion` and
   `hpl_native_locomotion install_complete ... moveSignature=1 ... addYawSignature=1 ... pauseSignature=1`.
2. In ordinary gameplay, slowly sweep the movement stick from center to full
   travel in cardinal and diagonal directions. Speed must vary continuously,
   diagonal motion must not receive a boost, and periodic controller rows must
   report `movementRoute=native_analog` with no held W/A/S/D keys.
3. Snap-turn in both directions. Each step should be exactly 30 degrees by
   default, preserve the rigid visual baseline, and report
   `turnRoute=native_radians`. If direction is reversed, set
   `NativeTurnSign=1`; do not change mouse sensitivity.
4. Set `SnapTurn=0`, relaunch, and verify smooth turning is frame-rate
   independent at `SmoothTurnDegreesPerSecond=120`. Restore snap turn after the
   test unless smooth turn is preferred.
5. Open the pause menu while holding movement and turn. The player must not
   accumulate motion or jump on resume. In `0.16.0+`, both native and semantic
   gameplay routes are explicitly suppressed and `pausedFrames` must increase.
6. Exercise at least one ladder, grab/push/door interaction, terminal/read
   state, and authored camera sequence. These states must retain native SOMA
   behavior through the semantic fallback; normal state must automatically
   return to native analog without a key press or reactivation.
7. Test walk/run, crouch/crawl, collision, stairs, and movement sounds. Attach
   the log; the final summary should show nonzero `moveFrames`, `moveCalls`, and
   `turnCalls`, with fallback counts corresponding to the exercised states.
8. In two-controller play, press support-hand primary and secondary. Default
   left X must toggle the flashlight and left Y must open inventory, with one
   haptic pulse each. Repeat with `DominantHand=left`; the actions must move to
   right A/B. One-hand primary+secondary must remain the recenter chord instead.

## 0.13.0 Player Hands Identity And Root-Pose Probe

1. Launch the OpenXR Release build, load a gameplay save, and press F10 once.
   Confirm `version=0.13.0-hands-identity` and
   `hpl_hands_bridge install_ok ... policy=identity_and_pose_probe_only`.
2. Trigger any normal hand/tool animation or state that makes SOMA's hands
   active. Expect one `hpl_entity_identity ... name=PlayerHands_* playerHands=1`
   row, followed by bounded `hpl_hands_pose` rows. There must be no crash and no
   visible change to hand placement or animation.
3. In normal tool idle/draw/holster states, capture rows with
   `scaleMode=quarter`, `gripValid=1`, finite basis vectors, and stable
   `cameraDistance`/`gripDistance`. Rotate and translate the controller enough to
   reveal handedness and the fixed model-to-grip orientation correction.
4. Exercise a full-scale hand animation, crawl/ladder/climb, custom hand
   placement, and a camera-socket animation if available. Capture transitions to
   `scaleMode=full`, `authoredCamera=1`, or distinct player/move states.
5. Briefly remove dominant-controller tracking and restore it. Probe output may
   report `gripValid=0`, but SOMA's original camera-follow hands must remain
   intact. Recenter/load another save and confirm a replacement `PlayerHands_*`
   identity is discovered.
6. Exit normally and attach the log. The summary should have nonzero
   `playerHandsIdentities`, `playerHandsCalls`, and preferably
   `trackedGripSamples`, with zero matrix read failures.

## 0.12.0 Native Controller Interaction And HUD Metrics

1. Launch the OpenXR Release build, load a normal gameplay save, and press F10
   once. Confirm `version=0.12.0-native-interaction` and
   `hpl_interaction_bridge install_ok ... policy=replace_start_direction_only`.
2. Keep your head still and point the dominant controller at a nearby usable
   object while moving the controller away from screen center. The crosshair or
   focus description should follow controller aim. Expect
   `hpl_interaction_ray ... applied=1` with finite controller direction and
   occasional `hit=1`.
3. Point your head at one usable object and the controller at another. Controller
   focus must win while the aim pose is fully tracked. Verify near/far native
   interaction limits and LOS still reject objects exactly as SOMA normally does.
4. Open a menu, enter an authored camera/hand animation if available, or remove
   controller tracking. The bridge must fall back without a crash; interaction
   summary counters should identify authored-camera, input, or tracking fallbacks.
5. Show the gameplay HUD, pause menu, subtitles, and a terminal. Attach bounded
   `hpl_gui_set` rows. `gameHud=1` rows should contain finite, stable
   `hudMetrics={...}` values across both eyes and display resolution changes.
6. Recenter, snap-turn, load another save, then repeat controller focus. Confirm
   no eye-height, rigid-world, shadow, reflection, or clean-shutdown regression.

## 0.11.0 Spatial Audio, Controller Poses, And HUD Identity

1. Launch the OpenXR Release build, load a save, and press F10 once. Confirm
   `version=0.11.0-spatial-ownership`, `hpl_game_hud_getter resolved=1`, and the
   known rigid/centered visual baseline.
2. Stand near a small directional sound. Rotate and translate your head without
   moving the player. Direction should follow head rotation and near-field
   balance should change naturally with head position. Expect periodic
   `hpl_audio_listener` rows with `correction=1 translation=1` and finite
   `committedPos`/`headWorldOffset` values.
3. Point the dominant controller forward, left, right, up, and down while moving
   it around the HMD. Periodic `hpl_controller` rows should report
   `worldAim={valid=1 tracked=11 ...}` and a smoothly changing unit `forward`.
   Grip positions should follow the physical controller without mirroring,
   scale jumps, or camera-yaw drift.
4. Show and hide the gameplay crosshair/interaction description, then open the
   pause menu and a terminal if available. Gameplay rows should report
   `gameHud=1`; menu/terminal/diegetic sets must remain `gameHud=0`.
5. Recenter, snap-turn, load a save, and cross a map boundary. Recheck all three
   systems after automatic camera replacement. Exit normally and attach the log.

## 0.10.0 Tracking Recovery And Controller Accessibility

1. Launch the OpenXR Release build, load a save, and press F10 once. Confirm
   `version=0.10.0-tracking-accessibility` and the rigid `0.9.0` visual baseline.
2. Briefly obstruct or disable HMD tracking, then restore it. Expect one
   `openxr_tracking degraded`, optional `lost`, then `restored` row. Stereo must
   resume automatically without F10/F11 and without a stale-eye flash.
3. Hold tracking unavailable beyond `TrackingHoldFrames=30`. The headset should
   fail closed through zero submitted layers while SOMA remains alive. Restored
   tracking should trigger the configured two-frame recovery blackout.
4. Test default roles, then `DominantHand=left`. Interaction, primary action,
   secondary action, and their haptics should follow the configured hand while
   left-stick movement and right-stick turn remain unchanged.
5. Set `SwapSticks=1`. Right stick should move and left stick should turn. Reset
   to `0` after testing.
6. With `OneHandFallback=1`, power off or remove one controller at a time. The
   available stick should move, its trigger/select should interact, and its
   primary/secondary buttons should jump/crouch. Turn and sprint should remain
   suppressed. Hold primary+secondary to recenter.
7. Restore both controllers. Expect `one_hand_fallback=0` and normal role
   mapping without stuck keyboard or mouse inputs. Exit and attach the log.

## 0.9.0 Reference Space, Focus, Haptics, And Native Roll

1. Launch `build-openxr-controller\Release\somavr_injector.exe`, load a save,
   and press F10. Confirm `version=0.9.0-calibration-haptics`,
   `requestedSpace=local selectedSpace=LOCAL`, and the known rigid stereo image.
2. Walk, interact, snap-turn, open the menu, jump, crouch, and complete a
   two-grip recenter. Confirm short pulses on the acting hand and increasing
   `openxrHapticRequests` without `openxrHapticFailures`.
3. Hold movement or interaction, then remove headset/runtime focus. SOMA must
   stop receiving the held action immediately. Expect one `focus_lost` row and
   one `focus_restored` row after focus returns, without restarting OpenXR.
4. Exit, set `ReferenceSpace=stage`, relaunch, and press F10 while standing.
   Expect `selectedSpace=STAGE` or an explicit fallback. Check eye height,
   room-scale translation, F2 recenter, and a save transition.
5. With `HPLNativeCameraRollSuppression=0`, trigger a sit, impact, sway, or
   scripted camera sequence and preserve the native roll rows. Repeat with `1`;
   translation/yaw/pitch must remain while forced roll is removed.
6. Revert roll suppression to `0` if any sequence loses intended framing. Exit
   normally and attach `logs\somavr.log`.

## 0.8.0 Runtime Resilience, Comfort, Effects, And GUI Sets

1. Launch `build-openxr-controller\Release\somavr_injector.exe`, load a save,
   and press F10 once. Confirm the known rigid, centered stereo baseline.
2. Confirm seven render-stage/GUI hooks install, including `gui_set_render`, and
   no native signature mismatch appears.
3. Snap-turn once and recenter once. Expect comfort-blackout request/completion
   rows with continuing successful OpenXR frames.
4. Load another save or cross a map. Expect camera replacement, eye-cache
   invalidation, stable calibration, and automatic stereo resumption without F10.
5. Visit gameplay HUD, subtitles, pause/menu, and a terminal. Preserve the
   `hpl_gui_set` rows, especially `is3d`, `priority`, virtual size, and draws.
6. Trigger damage, blur, or visual-distortion effects. Confirm named inventory
   rows and nonzero comfort suppression counters when the three targeted effects
   are active. Fades, tone mapping, and video distortion should remain present.
7. Ctrl+F12 should still isolate named effects; Shift+F12 restores policy.
8. Remove headset/runtime focus if practical. Confirm loss events either recover
   after the configured delay or fail closed without crashing SOMA.
9. Exit normally and attach `logs\somavr.log`.

## 0.7.2 Authored Camera And Render-State Policy

Purpose: validate the broader `0.7.2-render-state-policy` batch without changing
the proven F10 graphics path.

1. Launch the OpenXR build, load a save, and press F10 once.
2. Confirm `version=0.7.2-render-state-policy`, `hpl_player_state install_ok`,
   all six `hpl_compat_hook installed` rows, and no signature mismatch.
3. Walk, snap-turn, run, jump, crouch, interact, open Escape, and recenter with
   the two-grip chord. Baseline controls should remain unchanged.
4. Trigger at least one scripted or animation-owned camera sequence. Confirm
   `hpl_player_state ... authoredCamera=1` and
   `hpl_controller_authored_policy ... suppressed=1`; movement/turn/gameplay
   actions should stop while Escape and recenter remain responsive.
5. Confirm the sequence exits with `authoredCamera=0` and `suppressed=0`, then
   verify movement resumes without a stuck key or mouse button.
6. Inspect one periodic `hpl_render_stage ... stage=screen_gui` row. It should
   include nonnegative `calls={...}` deltas and complete blend, depth, scissor,
   color-write, and framebuffer state.
7. Press F6 once. `draws.csv` should contain the new `stage` column with values
   such as `world`, `post_effects`, `post_post_effect`, and `screen_gui`.
8. At a scene with visible full-screen effects, leave plain F12 bypass disabled
   and press `Ctrl+F12` repeatedly. Each press should log a selected object and
   vtable RVA and visibly isolate one active effect. Press `Shift+F12` to restore
   the normal chain.
9. Exit normally. Confirm no held input, clean OpenXR pre-graphics teardown, and
   `hpl_player_state_summary`, `hpl_compat_summary`, and
   `hpl_input_bridge_summary` rows.

## 0.7.1 Controller, Gameplay Actions, And Player-State Prototype

1. Close SOMA and launch:

```powershell
& "D:\Dev Debug\SOMAVR\build-openxr-controller\Release\somavr_injector.exe" --launch "G:\SteamLibrary\steamapps\common\SOMA\Soma_NoSteam.exe"
```

2. Confirm `version=0.7.1-gameplay-actions`, `controller_config enabled=1`,
   and `hpl_input_bridge install_ok`. A signature mismatch means this executable
   build is unsupported and controller injection will remain inactive.
3. Load a save and press F10 once. Confirm the known-good rigid stereo image,
   centered shadows/reflections, room scale, and eye height remain unchanged.
4. Confirm `hpl_player_probe` reports non-null player, camera, and body pointers;
   `cameraMatch=1`; and sensible player/move-state IDs.
5. Test left stick forward/back/strafe and diagonals. Movement should behave like
   W/A/S/D and stop immediately at stick release. Keyboard input must coexist.
6. Test one right-stick deflection and release. Default snap turn is
   pixel-calibrated, so record whether the step is too small/large and whether it
   is visually clean. Do not hold the stick through several tests without first
   returning it to center.
7. Point the normal gaze interaction at a door/object and press right trigger.
   Confirm it behaves like left click. Test the controller menu action as Escape.
8. On Touch or Index, test right A/Space jump, right B/Left-Control toggle
   crouch, and left-trigger/Left-Shift hold run. Each must preserve SOMA's normal
   movement noise, stamina/speed, animation, and state restrictions.
9. Hold both grips for about one second while still. Expect
   `hpl_recenter requested source=controller_grip_chord`, then the normal stable
   calibration and apply rows.
10. Briefly remove runtime focus or disable VR with F10 while holding movement.
   The player must stop; later summary should show no `sendFailures`.
11. Exercise a ladder, crawl/crouch area, scripted interaction, and pause/menu if
    convenient. Attach the log so state IDs can be mapped to each behavior.
12. Exit normally and confirm clean shutdown. Verify the DLL against the SHA-256
    in `somavr_build_manifest.txt` beside the final DLL.

## 0.6.0 Input, Pose, And Calibration Foundation

1. Close any running SOMA process and launch:

```powershell
& "D:\Dev Debug\SOMAVR\build-openxr-input-foundation\Release\somavr_injector.exe" --launch "G:\SteamLibrary\steamapps\common\SOMA\Soma_NoSteam.exe"
```

2. Confirm `version=0.6.0-input-foundation buildOpenXR=1`,
   `openxr_config ... input=1`, and `hpl_camera_bridge install_ok` reports
   `verticalRoomscale=1 eyeHeightOffsetMeters=0.0000`.
3. Load a save and press F10 once. The known-good rigid world, stereo, full
   projection centering, room-scale motion, shadows, and reflections must remain
   unchanged from `0.5.11`.
4. Expect `openxr_input initialized actions=8 profiles=4`, successful binding rows, and
   `openxr_input session_attached attached=1` before `openxr_session_begin ok`.
5. Move both controllers through a broad range. Periodic `openxr_input state`
   rows should report `gripValid=1,1 aimValid=1,1` while tracked.
6. Move the left and right sticks independently. Confirm `move=x,y` changes only
   for the left stick and `turn=x,y` only for the right stick.
7. Exercise triggers, face-button/select, grips/squeeze, and the left menu button. Confirm the
   matching telemetry changes. SOMA must not move or interact from these inputs;
   this build intentionally exposes snapshots without a native gameplay bridge.
8. Confirm `hpl_stereo` reports `poseAgeFrames` near zero during normal rendering.
   Brief runtime focus loss may increase it, but refocus must resume fresh samples.
9. Press F2 and confirm recenter still applies after eight stable tracked frames.
   F10 exit/re-entry and normal keyboard/mouse controls must still work.
10. Inspect `somavr_build_manifest.txt` beside the DLL and verify its SHA-256 with:

```powershell
Get-FileHash -Algorithm SHA256 "D:\Dev Debug\SOMAVR\build-openxr-input-foundation\Release\somavr.dll"
```

11. Exit normally and confirm the pre-graphics OpenXR shutdown rows and clean
    process exit.

## 0.5.11 In-Session Recenter

1. Close any running SOMA process and launch:

```powershell
& "D:\Dev Debug\SOMAVR\build-openxr-recenter\Release\somavr_injector.exe" --launch "G:\SteamLibrary\steamapps\common\SOMA\Soma_NoSteam.exe"
```

2. Confirm `version=0.5.11-recenter buildOpenXR=1` and the camera install row
   reports `recenterKey=F2 recenterControl=1`.
3. Load the same save, face forward, and press F10 once. Confirm the known-good
   `0.5.10` path: stereo, projection centering, room scale, and clean graphics.
4. Turn your body or chair to a deliberately offset facing direction, hold still,
   and press F2 once.
5. Expect `hpl_recenter requested key=F2`, then bounded `hpl_recenter
   calibration_wait` rows if tracking needs to settle, then
   `hpl_recenter applied ... stablePoseFrames=8`.
6. During the wait, the headset should continue showing the existing VR view
   rather than snapping back to the desktop camera.
7. After apply, the current HMD pose should become the new neutral orientation and
   room-scale origin. Stereo must remain active and no F8/F11 presses are needed.
8. Repeat F2 once while moving slightly. A `calibration_reset` row is acceptable;
   it should settle and apply only after motion stops.
9. Press F10 to exit VR mode, then F10 again to confirm one-key re-entry still
   uses the original activation latch. Exit SOMA normally and confirm clean
   lifecycle shutdown.

## 0.5.10 Neutral Pose Latch

1. Close any running SOMA process and launch:

```powershell
& "D:\Dev Debug\SOMAVR\build-openxr-poselatch\Release\somavr_injector.exe" --launch "G:\SteamLibrary\steamapps\common\SOMA\Soma_NoSteam.exe"
```

2. Confirm `version=0.5.10-poselatch buildOpenXR=1` and the camera install row
   reports `activationStableFrames=8`.
3. Load the same save, stand or sit normally, face forward, and press F10 once.
4. Expect OpenXR startup followed by `hpl_vr_mode calibration_wait`. A
   `calibration_reset reason=reference_space_jump` row is acceptable and proves
   a transient origin was rejected.
5. Expect activation shortly afterward with `fullyTracked=1 stablePoseFrames=8`.
   No F8 or F11 press is required.
6. Confirm the initial `neutralPosition` resembles subsequent `hmdPos` values.
   The first `hpl_stereo eyeOffset` should be centimetres, not the approximately
   `1.79 m` vertical offset seen in `0.5.9`.
7. Confirm native player eye height, rigid yaw/pitch/roll, room-scale translation,
   stereo depth, and the known-good shadow/reflection behavior.
8. Press F10 twice to test exit/re-entry, then exit SOMA normally and confirm
   clean lifecycle shutdown.

## 0.5.9 Camera Rotation Regression Fix

1. Close any running SOMA process and launch:

```powershell
& "D:\Dev Debug\SOMAVR\build-openxr-rotationfix\Release\somavr_injector.exe" --launch "G:\SteamLibrary\steamapps\common\SOMA\Soma_NoSteam.exe"
```

2. Confirm the log reports `version=0.5.9-rotationfix buildOpenXR=1`.
3. Load the same save used for the `0.5.8` test, face forward, and press F10 once.
4. Make small yaw movements, then small pitch movements. Geometry must remain
   rigid, with no shear, diagonal stretching, or view-dependent scale change.
5. Increase yaw and pitch gradually, then add roll and head translation. Confirm
   tracking, stereo depth, and room scale still match the `0.5.7` visual baseline.
6. Recheck one formerly broken shadow/reflection location. Full projection
   centering must remain active and those effects should stay stereo-consistent.
7. Press F10 twice to verify exit and re-entry, then exit SOMA normally.

Expected log markers remain `hpl_vr_mode activated ... stereo=1 projectionCentered=1 roomscale=1`,
`hpl_stereo ... projectionOffset=0.000000,0.000000`, and clean lifecycle shutdown.
No additional hotkeys are required.

## 0.5.8 One-Key VR Activation

1. Close any running SOMA process and launch:

```powershell
& "D:\Dev Debug\SOMAVR\build-openxr-onekey\Release\somavr_injector.exe" --launch "G:\SteamLibrary\steamapps\common\SOMA\Soma_NoSteam.exe"
```

2. Load a save normally. Do not press F8 or F11.
3. Face forward and press F10 once.
4. Expect this ordered sequence, allowing several frames for OpenXR startup:

```text
hpl_vr_mode requested key=F10
openxr_manual_start triggered source=api key=F10
openxr_session_begin ok
hpl_vr_mode activated key=F10 ... tracking=1 stereo=1 projectionCentered=1
```

5. Confirm the headset receives stereo, head tracking works, and an `hpl_stereo`
   row reports `projectionOffset=0.000000,0.000000`.
6. Recheck one previously broken shadow/reflection location to ensure the one-key
   path is visually identical to `0.5.7-fullcenter`.
7. Press F10 again. Expect `hpl_vr_mode disabled`, stereo submission to stop, and
   SOMA's base camera to be restored.
8. Press F10 once more and confirm the already-running OpenXR session re-enters
   tracking/stereo without F8 or F11.
9. Exit normally and reconfirm clean process shutdown.

F8 and F11 remain available only for separately testing OpenXR bootstrap and AFR
stereo. They are not part of the normal activation sequence.

## 0.5.7 Full Projection Center

1. Close any running SOMA process and launch:

```powershell
& "D:\Dev Debug\SOMAVR\build-openxr-fullcenter\Release\somavr_injector.exe" --launch "G:\SteamLibrary\steamapps\common\SOMA\Soma_NoSteam.exe"
```

2. Confirm `version=0.5.7-fullcenter`, lifecycle install success, and
   `projectionCenteredDefault=1`.
3. Load the save, press F8, F10, and F11. Do not press F5 initially.
4. Confirm an `hpl_stereo` row reports
   `projectionCentered=1 ... projectionOffset=0.000000,0.000000`.
5. At the known dynamic shadow, keep the body still and deliberately apply HMD
   pitch, then roll. Compare shadow position/cast against `0.5.6`.
6. Inspect the sharp ceiling lighting boundary under the same pitch/roll motion.
7. Back away from the window and inspect whether the top-down opaque/reflection
   sweep is reduced, stationary, or unchanged. Repeat at the oven.
8. Briefly press F5 to restore runtime asymmetric FOV. Confirm the log shows
   `enabled=0`, observe only long enough to compare, then press F5 again to return
   to fully centered mode.
9. Exit normally and reconfirm the lifecycle begin/complete rows and clean process
   exit.

Interpretation:

- Broad improvement across shadow, ceiling, and window strongly confirms the
  remaining vertical projection-center mismatch.
- Shadow improvement only redirects reflection work to mirrored texture/frustum
  bounds while keeping full centering as the lighting policy.
- No change with logged `0,0` offsets redirects dynamic lighting to per-light
  varyings/light volumes and reflection to texture/clip-plane ownership.

## 0.5.6 Shadow Motion And Reflection Fade

1. Close any running SOMA process and launch:

```powershell
& "D:\Dev Debug\SOMAVR\build-openxr-stability\Release\somavr_injector.exe" --launch "G:\SteamLibrary\steamapps\common\SOMA\Soma_NoSteam.exe"
```

2. Confirm `version=0.5.6-stability`, `hpl_lifecycle install_ok`,
   `projectionCenteredDefault=1`, and `roomscaleDefault=1`.
3. Load the save and press F8, F10, then F11. F5 should not be needed: stereo
   begins with centered horizontal projection.
4. At the realtime shadow and ceiling lighting edge, keep the player body still
   and move the HMD translationally. Record the baseline motion.
5. Press F4 once. Expect `hpl_roomscale enabled=0`. Repeat the same movement;
   rotation and stereo IPD remain active, but leaning no longer translates the
   render camera.
6. Classify both the shadow cast and sharp ceiling edge as improved, unchanged,
   or worse. Press F4 again to restore room-scale tracking.
7. At the window or oven, move forward/back until the opaque boundary sweeps
   across the surface. Press F3 once and repeat.
8. Expect `reflection_fade_bypass enabled=1`, a `reflection_fade_target` row, and
   nonzero `reflection_fade_override` rows. The surface may become uniformly more
   reflective; the important result is whether the moving boundary disappears.
9. Press F3 again to restore the authored fade. Optionally press F6 at the same
   view if the target or patches are absent.
10. Exit normally. Confirm lifecycle `begin` and `complete` rows and verify the
    SOMA process disappears.

Interpretation:

- F4 improvement confirms room-scale translation reaches camera-relative shadow
  ownership; unchanged behavior redirects to orientation/light-volume/AFR state.
- F3 removing the sweep confirms view-depth reflection fade. A nonzero patch with
  no visual change redirects to reflection-map bounds, mirrored-frustum clipping,
  or refraction opacity.
- `hpl_lifecycle signature_mismatch` means the corrected guard still needs direct
  runtime-byte comparison; do not treat process exit as a lifecycle-hook result.

## 0.5.5 Reconstruction And Shutdown Diagnostic

1. Close any running SOMA process and launch:

```powershell
& "D:\Dev Debug\SOMAVR\build-openxr-reconstruct\Release\somavr_injector.exe" --launch "G:\SteamLibrary\steamapps\common\SOMA\Soma_NoSteam.exe"
```

2. Load the same save, press F8, face forward, press F10, then press F11.
3. Return to one captured trouble spot containing a realtime shadow and, when
   possible, a reflective surface. Keep F5 off and press F6 once.
4. Wait for `render_diag complete`. Note the baseline eye-to-eye shadow and
   reflection displacement.
5. Press F5 once. Expect `hpl_projection_center enabled=1`. Briefly classify
   whether the displacement improves, worsens, or stays identical, and note any
   optical distortion or scale change.
6. Without moving the player, press F6 again and wait for completion. Press F5
   again to restore the runtime asymmetric FOV.
7. Confirm both capture folders include `program_*_ubos.txt` files and the log has
   `render_diag_ubo` rows for both eyes.
8. Exit normally. Expect `hpl_lifecycle pre_graphics_shutdown begin` followed by
   `complete`, and verify SOMA disappears from Task Manager.
9. If SOMA remains, use `somavr_dumper.exe Soma_NoSteam.exe`; remember that the
   dumper captures the process but does not terminate it.

Interpretation:

- F5 improvement strongly supports a mono-centered deferred reconstruction packet.
- Different eye UBO bytes at projection/inverse-projection offsets help locate the
  eye-local owner; identical bytes support a stale/shared packet.
- No F5 change with correct eye-local UBOs redirects to shared render targets,
  sampler ownership, temporal history, or AFR frame age.
- A clean exit after the lifecycle rows confirms OpenXR teardown ordering was the
  process-lifetime issue.

## 0.5.4 Shadow/Reflection Render Diagnostic

1. Close any running SOMA process and launch:

```powershell
& "D:\Dev Debug\SOMAVR\build-openxr-renderdiag\Release\somavr_injector.exe" --launch "G:\SteamLibrary\steamapps\common\SOMA\Soma_NoSteam.exe"
```

2. Load a save, press F8, face forward, press F10, then press F11.
3. Find one view containing a visibly unstable realtime shadow and reflective material.
4. Hold the camera mostly still and press F6 once.
5. Wait for `render_diag complete`; the capture lasts four game frames and spans both AFR eyes twice.
6. Attach the latest log and the new folder under `logs\render-captures`.
7. Exit normally. Confirm the process disappears from Task Manager within a few seconds.
8. If it remains, capture a minidump before ending it:

```powershell
& "D:\Dev Debug\SOMAVR\build-openxr-renderdiag\Release\somavr_dumper.exe" Soma_NoSteam.exe
```

Expected capture contents:

- `draws.csv` has both `eye=0` and `eye=1`, program/FBO transitions, and bounded draw order.
- `matrices.csv` contains eye-attributed view, projection, inverse-view, and light matrices.
- `program_*.txt` files contain active uniforms and generated shader sources; log rows classify reflection/shadow candidates.
- The log ends the capture with nonzero program/draw/matrix counts.

For a useful perceptual note, identify whether the reflective artifact is on
water/glass using a reflected world image, or on a metallic/glossy object using a
cubemap. The generated programs should then confirm the route.

## 0.5.3 Shadow Jitter A/B

This test keeps the validated OpenXR, camera, AFR stereo, audio, and post controls
unchanged. F7 affects only the soft-shadow offset radius and defaults off.

1. Close any running SOMA process.
2. Launch:

```powershell
& "D:\Dev Debug\SOMAVR\build-openxr-shadowjitter\Release\somavr_injector.exe" --launch "G:\SteamLibrary\steamapps\common\SOMA\Soma_NoSteam.exe"
```

3. Load a save, press F8, face forward, press F10, then press F11.
4. Confirm `version=0.5.3-shadowjitter`, `shadowJitterControl=1`, and hook installs for `glUniform2f` or `glUniform2fv`.
5. Find a high-contrast realtime shadow from a moving or nearby light. Study it in both eyes while still, then move the player slowly.
6. Press F7 once. Expect `shadow_jitter_suppression enabled=1`.
7. Repeat the same still/head/player movement and classify three things separately: fine edge grain, large shadow position/shape, and movement-dependent swimming.
8. Press F7 again. Expect `enabled=0` and immediate restoration of SOMA's authored soft-filter radius.
9. Attach the log and describe whether F7 made shadow edges harder, whether the eyes agreed better, and whether large shadow shapes still shifted.

Success markers:

- `uniform_location ... name="avShadowMapOffsetMul" shadowRelevant=1`.
- `shadow_jitter_upload ... overridden=0` before F7 and `overridden=1 submitted=0.000000000,0.000000000` afterward.
- Visual eye agreement improves while the expected hard-edge tradeoff appears.
- OpenXR submission remains stable with no stereo suspension.

Redirect markers:

- No target uniform or uploads: inspect which uniform setter/API SOMA uses before changing renderer state.
- Fine noise improves but large shapes still move: capture directional cascade matrices/splits and shadow-map generation by eye.
- No visual change despite nonzero overrides: the tested light may use the low-quality single-sample branch or another shadow implementation.

## 0.5.2 Audio And Post-Effect Test

This build preserves the successful F8/F10/F11 stereo path, adds head-relative listener orientation, and provides an F12 post-effect A/B toggle.

1. Close any running SOMA process and launch:

```powershell
& "D:\Dev Debug\SOMAVR\build-openxr-audiopost\Release\somavr_injector.exe" --launch "G:\SteamLibrary\steamapps\common\SOMA\Soma_NoSteam.exe"
```

2. Confirm `version=0.5.2-audiopost` and `hpl_compat_probe ... installed=8 requested=8` with `audioCorrection=1`, `postEffectControl=1`, and `postEffectBypass=0`.
3. Load the same save, press F8, then face forward and press F10.
4. Stand near a stable directional sound. Turn only your head and confirm the sound remains correctly positioned relative to the world instead of rotating with your body-facing camera.
5. Confirm sampled `hpl_audio_listener` rows show `correction=1`, changing `headDeltaQuat`, and `committedForward/Up` differing from the authored listener vectors.
6. Press F11 and confirm stereo still behaves like the successful `0.5.1` run.
7. Revisit one of the visible shader defects. Observe it normally, then press F12 once.
8. Confirm `hpl_post_effect_bypass enabled=1`. The image may become flatter or lose grading/bloom; HUD and menus should remain visible.
9. Decide whether the shader defect disappears, changes, or remains identical. Press F12 again and confirm `enabled=0` restores normal effects.
10. Quit normally and attach `logs\somavr.log`, noting the object/effect that looked wrong and the F12 result.

Safety and interpretation:

- F10 off means audio correction is off, even if configured.
- F12 defaults off and does not affect F11 eye pose or OpenXR submission.
- If F12 removes the defect, the next build will classify individual post effects.
- If F12 does not change it, the defect is likely in world shader camera/temporal state.
- Press F12 immediately if bypass causes a black frame or missing world; the toggle is reversible.
- `rotation_self_test_failed` disables only audio correction.
- Any `post_effect_has_active` signature mismatch disables only F12 control.

## 0.5.1 Compatibility Probe Test

This build is observational: it must look and play exactly like `0.5.0-afrstereo` while collecting the evidence needed for same-frame stereo and head-relative audio.

1. Close any running SOMA process and launch:

```powershell
& "D:\Dev Debug\SOMAVR\build-openxr-compatprobe\Release\somavr_injector.exe" --launch "G:\SteamLibrary\steamapps\common\SOMA\Soma_NoSteam.exe"
```

2. Confirm `version=0.5.1-compatprobe` and `hpl_compat_probe install_complete ... installed=7 requested=7`.
3. Load a save and wait at least 240 frames before starting OpenXR. The game must not crash or show any visual/audio change.
4. Confirm `hpl_render_stage` rows exist for `viewport`, `world`, `world_callbacks`, `post_effects` when active, `post_post_effect`, and `screen_gui`.
5. Press F8, then F10. Hold the in-game camera still and make deliberate HMD yaw, pitch, and roll movements for roughly five seconds.
6. Optionally press F11 briefly for the existing AFR test, then press it again to return to F10 mono.
7. Quit normally and attach `logs\somavr.log`.

Expected evidence:

- sampled stages share a frame and have ordered `sequence` values;
- `mask` identifies world/post/GUI ownership and `glBefore`/`glAfter` reveal target transitions;
- `durationUs` provides an early estimate of which work stereo will duplicate;
- `hpl_audio_listener` changes `hmdQuat` during headset-only movement;
- if listener forward/up remain fixed during that movement, hypothesis S11 is confirmed and the next audio build can apply a center-head correction;
- no `signature_mismatch`, `create_hook`, or `enable_hook` rows appear.

Failure markers:

- `installed` below `7`: inspect the named `hpl_compat_hook skipped` row; unaffected hooks remain usable.
- `glValid=0`: the stage was reached without SOMA's render GL context current.
- missing `world` with present `viewport`: the tested viewport may have no active renderer/world/frustum.
- missing `post_effects`: no post effect was active in the sampled scene; this is not automatically a failure.
- any visual, audio, or input change: disable both compatibility probes and retain the log for hook attribution.

## 0.5.0 Alternating-Eye Stereo Test

This is the first real stereo experiment, but it is AFR: one eye is rendered per game frame and the other eye uses its most recent cached frame. Test briefly while seated.

1. Close any running SOMA process and launch:

```powershell
& "D:\Dev Debug\SOMAVR\build-openxr-afrstereo\Release\somavr_injector.exe" --launch "G:\SteamLibrary\steamapps\common\SOMA\Soma_NoSteam.exe"
```

2. Load a save, press F8, and wait for repeated successful OpenXR frame submission.
3. Face forward and press F10. Confirm small head movements still feel like the proven native mono build.
4. Press F11 once, then remain fairly still for the two-eye cache warmup.
5. Expect:
   - `hpl_stereo enabled ... mode=alternating_eye`
   - `hpl_stereo applied=1 eye=0` and `applied=2 eye=1`
   - two `openxr_eye_cache created` rows from startup
   - `openxr_stereo_cache ... cachesReady=1`
   - later `openxr_frame ok ... stereo=1` with increasing stereo counters
6. Inspect a nearby doorway, console, or other object with clear depth. Stereo should no longer be the duplicated cross-eyed image; head translation should produce small positional parallax.
7. Press F11 immediately if stereo is reversed, excessively scaled, flickery, or uncomfortable. This returns to F10 mono orientation without unloading OpenXR.
8. Press F10 to restore SOMA's original camera before quitting.

Expected limitations:

- each eye updates at half the game-frame rate;
- moving objects and temporal effects can differ by one frame between eyes;
- the desktop mirror alternates eye views;
- HUD, post-processing, shadows, and effects have not yet been classified for stereo correctness.

Failure markers:

- `projection_self_test_failed`: F11 was disabled before native modification.
- `stereo_views_unavailable`: valid two-eye OpenXR data was not ready.
- `openxr_stereo_submission warming_up` without later `cachesReady=1`: one eye was not captured.
- `copyStereoCache` or `cache_*_failed`: inspect the named swapchain/cache operation.
- `openxr_stereo_submission suspended`: F11 fell back after repeated cache failures.

## 0.4.0 Native HPL Camera Test

This is an orientation bridge validation, not stereo gameplay. Keep the first test short and make small head movements.

1. Close any running SOMA process so no earlier DLL remains loaded.
2. Launch the versioned OpenXR build:

```powershell
& "D:\Dev Debug\SOMAVR\build-openxr-hplcamera\Release\somavr_injector.exe" --launch "G:\SteamLibrary\steamapps\common\SOMA\Soma_NoSteam.exe"
```

3. Load a save, press F8, and wait until the headset shows the mirrored game image.
4. Check for `version=0.4.0-hplcamera`, `hplCameraBridge=1`, `hpl_camera_bridge install_ok`, and at least one `hpl_camera candidate` row.
5. Face forward and press F10 once. Expect `hpl_head_tracking enabled ... mode=orientation` with a valid `poseFrame` and neutral quaternion.
6. Slowly turn a few degrees left/right, then pitch and roll slightly. The game camera should move with the headset while ordinary mouse look remains composited with it.
7. Stop immediately and press F10 if motion is reversed, axis-swapped, unstable, or uncomfortable. Expect `hpl_head_tracking disabled` and restoration of SOMA's base view.
8. If the direction is usable, press F9 once and move only the headset for the two-second capture. This should show SOMA view/MVP matrices changing with the OpenXR pose.
9. Press F10 again before quitting. Attach the resulting log.

Success markers:

- `hpl_head_tracking applied=1` followed by later rows with a changing `deltaAngleDeg` and `viewQuaternion`.
- `matrix_capture_sample` rows span sample buckets 1 through 4 and view/MVP values change during headset-only movement.
- OpenXR continues reporting successful two-view frame submission with no `openxr_frame failure`.

Failure markers:

- `signature_mismatch`: the executable differs from the mapped `Soma_NoSteam.exe`; the native bridge correctly stayed off.
- `enable_ignored ... openxr_pose_unavailable`: F10 was pressed before valid `xrLocateViews` data existed.
- No `hpl_camera candidate`: the mapped camera function was not reached in the loaded scene.
- Increasing `pose_unavailable`: OpenXR view location stopped producing valid pose data.

## 0.3.1 Camera Attribution Test

This build leaves the successful mirrored headset presentation unchanged. F9 starts a bounded camera-uniform and call-stack capture for the next 120 rendered frames.

1. Close the currently running SOMA process so it is not using the previous DLL.
2. Launch the versioned OpenXR build:

```powershell
& "D:\Dev Debug\SOMAVR\build-openxr-cameramap\Release\somavr_injector.exe" --launch "G:\SteamLibrary\steamapps\common\SOMA\Soma_NoSteam.exe"
```

3. Load into a save and press F8. Confirm the mirrored image appears and `openxr_frame ok ... layers=1 views=2` is logged.
4. Stand or sit still briefly, then press F9 once while the game is focused.
5. Slowly turn your head left and right, then slightly up and down for about two seconds. Avoid moving the player with mouse/controller during this short window.
6. Wait for `matrix_capture complete` before closing the game.
7. Confirm the log contains:
   - `matrix_capture armed key=F9 ... openxrPoseValid=1`
   - one or more `matrix_capture_site` rows with `Soma_NoSteam.exe+0x...` stack entries
   - `matrix_capture_sample` rows for names such as `a_mtxModelViewProjection`, `a_mtxTemporalProjection`, `a_mtxTemporalView`, or `a_mtxInvViewProjection`
   - `matrix_capture complete ... uploads=... sites=... samples=... openxrPoseValid=1`
8. Attach the resulting log. The module RVAs become the next Ghidra anchors for the HPL3 frustum/view owner.

## 0.3.0 OpenXR Mirrored Frame Test

This is an end-to-end presentation test, not stereo gameplay. Both eyes receive the same SOMA backbuffer while OpenXR supplies distinct eye poses/FOVs.

1. Start Virtual Desktop/OpenXR and make sure the headset is connected.
2. Confirm `somavr.ini` has `FrameSubmit=1`, `MirrorBackbuffer=1`, `ResolutionScalePercent=100`, and `ManualStart=1`.
3. Launch the OpenXR build:

```powershell
& "D:\Dev Debug\SOMAVR\build-openxr\Release\somavr_injector.exe" --launch "G:\SteamLibrary\steamapps\common\SOMA\Soma_NoSteam.exe"
```

4. Load into a save, then press F8 once. The key is now sampled every rendered frame.
5. Confirm the log reports `version=0.3.0-xrframe buildOpenXR=1` and `openxr_manual_start triggered`.
6. Confirm resource creation succeeds:
   - `openxr_gl_functions ready=1`
   - two `openxr_swapchain created eye=...` rows
   - `openxr_gl_bridge ready eyes=2`
   - `openxr_frame_resources ready space=LOCAL eyes=2`
7. Confirm `openxr_session_begin ok` follows the runtime's `READY` event.
8. Confirm the headset shows SOMA's desktop image in both eyes. Distortion, lack of real stereo, and incorrect camera response are expected in this transport proof.
9. Confirm repeated `openxr_frame ok` rows report `layers=1`, `views=2`, and increasing `xrFrame` values.
10. Confirm later summaries report `openxrFrameResourcesReady=1`, `openxrSessionRunning=1`, `openxrSubmittedFrames` increasing, and `openxrFrameSubmitFailed=0`.
11. Stop immediately if the headset image is uncomfortable. This build is intended only to validate presentation for a short period.

Failure markers:

- `openxr_gl_functions ready=0`: required FBO/blit entry points were not resolved.
- `openxr_swapchain create_failed` or `framebuffer_incomplete`: runtime image allocation or OpenGL interop failed.
- `openxr_session_begin failed`: session reached `READY` but could not transition to running.
- `openxr_frame failure`: inspect the named operation and result.
- `openxr_frame submission_suspended`: 60 consecutive frame failures disabled further submission for that run.

## First Injection Smoke Test

1. Build x64 Release.
2. Launch SOMA suspended through the injector.
3. Confirm `logs\somavr.log` starts and contains `somavr.dll loaded`.
4. Confirm a `hook_config` line appears with the intended INI values.
5. Confirm a `gl_context_info` line includes vendor, renderer, and OpenGL version.
6. Confirm `frame_summary` lines appear after the first 120 rendered frames, unless `FrameSummaryInterval` was lowered.
7. Confirm matrix telemetry appears:
   - `matrix_upload kind=fixed_projection`, or
   - `uniform_matrix ... projectionLike=1`.

## 0.2.7 OpenXR F8 Manual Start Test

1. Build with `-DSOMAVR_ENABLE_OPENXR=ON`.
2. Confirm `somavr.ini` has `[OpenXR] Probe=1`, `SessionProbe=1`, `ReleaseAfterProbe=0`, `BootstrapFrame=120`, `HoldFrames=0`, and `ManualStart=1`.
3. Launch with the OpenXR build:

```powershell
& "D:\Dev Debug\SOMAVR\build-openxr\Release\somavr_injector.exe" --launch "G:\SteamLibrary\steamapps\common\SOMA\Soma_NoSteam.exe"
```

4. Confirm the log starts with `version=0.2.7-xrmanual buildOpenXR=1`.
5. Confirm `openxr_config buildOpenXR=1 enabled=1 sessionProbe=1 releaseAfterProbe=0 bootstrapFrame=120 holdFrames=0 manualStart=1 key=F8`.
6. Confirm SOMAVR logs `openxr_manual_start waiting key=F8` and does not log OpenXR loader/runtime rows before F8 is pressed.
7. Load into an actual save game, then press F8 once.
8. Confirm `openxr_manual_start triggered key=F8 frame=... hdc=... hglrc=...`.
9. Confirm loader/runtime/session rows follow the trigger, including `openxr_loader_load ok`, `openxr_extensions ... khrOpenGL=1`, `openxr_bootstrap requirements_ok`, and `openxr_session_probe ok`.
10. Confirm later frame summaries show `openxrManualStartArmed=1`, `openxrSessionAlive=1`, `openxrInstanceAlive=1`, and `openxrReleaseAfterProbe=0`.
11. Since this build keeps the session alive, there should be no `openxr_runtime released_after_probe reason=hold_complete` unless the config is changed back to a hold/release probe.

## 0.2.6 OpenXR Session Hold Probe Test

1. Build with `-DSOMAVR_ENABLE_OPENXR=ON`.
2. Confirm `build-openxr\Release\openxr_loader.dll` exists beside `somavr.dll`.
3. Confirm `build-openxr\Release\somavr_build_flavor.txt` says `openxr=1`.
4. Confirm `somavr.ini` has `[OpenXR] Probe=1`, `SessionProbe=1`, `ReleaseAfterProbe=1`, `BootstrapFrame=120`, and `HoldFrames=600`.
5. Launch with the OpenXR build:

```powershell
& "D:\Dev Debug\SOMAVR\build-openxr\Release\somavr_injector.exe" --launch "G:\SteamLibrary\steamapps\common\SOMA\Soma_NoSteam.exe"
```

6. Confirm the log starts with `version=0.2.6-xrhold buildOpenXR=1`.
7. Confirm `openxr_config buildOpenXR=1 enabled=1 sessionProbe=1 releaseAfterProbe=1 bootstrapFrame=120 holdFrames=600`.
8. Confirm bootstrap deferral:
   - `openxr_bootstrap deferred_until_frame targetFrame=120`
   - no OpenXR loader/runtime rows before the deferral target is reached.
9. Confirm loader resolution:
   - `openxr_loader_load attempt ... openxr_loader.dll`
   - `openxr_loader_load ok ...`
10. Confirm extension/runtime rows:
   - `openxr_extensions ... khrOpenGL=1`
   - `openxr_instance runtime=...`
   - `openxr_system ... orientationTracking=1 positionTracking=1`
11. Confirm stereo view rows:
   - `openxr_view_configurations ... primaryStereo=1`
   - two `openxr_view index=...` rows, ideally with nonzero recommended sizes.
12. Confirm the session probe result:
   - success: `openxr_session_probe ok`, followed by `openxr_reference_spaces` and `openxr_swapchain_formats`;
   - redirect: `openxr_session_probe create_failed` with a concrete `XrResult`.
13. Confirm the `openxr_session_probe ok` `hglrc` matches the main frame-summary `hglrc`, expected from recent runs to be `0x30000`.
14. Confirm `openxr_runtime hold_after_probe frame=120 holdFrames=600 releaseFrame=720`.
15. Confirm frame summaries before release show `openxrSessionAlive=1`, `openxrInstanceAlive=1`, and `openxrSessionHeldAfterProbe=1`.
16. Confirm `openxr_runtime released_after_probe reason=hold_complete` appears around the release frame.
17. Confirm later frame summaries show `openxrInstanceAlive=0`, `openxrSessionAlive=0`, and `openxrSessionReleasedAfterProbe=1`.
18. If the game reaches the menu without a later `VirtualDesktop.LibOVRRT64_1.dll` crash, the next build can allocate swapchains without beginning/submitting frames yet.

## 0.2.5 OpenXR Frame-Context Session Probe Test

The `0.2.5-xrframeprobe` pass is considered successful when `openxr_bootstrap deferred_until_frame targetFrame=120`, `openxr_session_probe ok` uses the frame `hglrc`, `openxr_runtime released_after_probe reason=session_probe_complete`, and later summaries show `openxrInstanceAlive=0` and `openxrSessionAlive=0`.

## 0.2.4 OpenXR Startup-Context Session Probe Test

The `0.2.4-xrsessiononeshot` pass is considered successful when `openxr_session_probe ok`, `openxr_reference_spaces`, `openxr_swapchain_formats`, `openxr_runtime released_after_probe reason=session_probe_complete`, and later summaries show `openxrInstanceAlive=0` and `openxrSessionAlive=0`.

## 0.2.3 OpenXR Static One-Shot Probe Test

The `0.2.3-xroneshot` pass is considered successful when `SessionProbe=0`, `openxr_instance released_after_static_probe reason=session_probe_disabled`, and later frame summaries show `openxrInstanceAlive=0`.

## Failure Buckets

- No log file: DLL failed to load or dependency missing.
- Log file but no GL context: injected too late/early or wrong executable.
- Context but no frame summaries: `SwapBuffers` hook missed; add/verify SDL swap hook or GDI hook.
- Frame summaries but no matrices: rendering may be mostly shader uniforms with names hidden, or hooks installed after GLEW initialized.
- Matrices but no stable projection: move to Ghidra/static camera path or add capture-window logging around render passes.
- `buildOpenXR=0` while `Probe=1`: wrong DLL build was injected; rerun from `build-openxr\Release\somavr_injector.exe`.
- Last row is `gl_context_info` and Windows reports `0xc06d007e`: likely OpenXR delay-load dependency search failed; `0.2.2-xrloaderpath` should turn this into `openxr_loader_load ok/failed`.
- `openxr_loader_load failed`: fix the reported loader path or dependency error before testing runtime/session behavior.
- `openxr_extensions ... khrOpenGL=0`: selected OpenXR runtime cannot support the OpenGL binding path; switch runtime or use a non-OpenGL bridge.
- Loader/runtime/view rows succeed but later WER reports `VirtualDesktop.LibOVRRT64_1.dll` with `0xc0000005`: verify the latest summary has `openxrInstanceAlive=0`; if it does, the crash is likely a loader/runtime side effect rather than live-instance lifetime.
- Requirements succeed but `openxr_session_probe create_failed`: keep the runtime discovery path and retry session creation later in the render loop, or temporarily run with `SessionProbe=0`.
