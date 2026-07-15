# Test Checklists

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
   `0.5..1.5` around `ThrowVelocityReference`. Set it to `0` if direction is correct
   but authored object classes need their original fixed throw strength.
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
