# Test Checklists

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
