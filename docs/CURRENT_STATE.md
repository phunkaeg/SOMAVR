# Current State

Date: 2026-07-15

## Objective

Bootstrap SOMAVR: a reverse-engineered VR mod for SOMA/HPL3, likely using DLL injection plus OpenXR.

## Initial Findings

- `D:\Dev Debug\SOMAVR` was empty at project start, so this folder is now treated as the mod workspace.
- SOMA is installed at `G:\SteamLibrary\steamapps\common\SOMA\`. The install includes `Soma_NoSteam.exe`, `Soma.exe`, `SDL2.dll`, `glew32.dll`, `_shadersource\`, and the game config folder.
- Ghidra confirms `Soma_NoSteam.exe` is a 64-bit Visual Studio 2010-era executable. The CRT entry calls `FUN_1401daec0`, which then calls the likely application entry `FUN_140125a20`.
- `FUN_140125a20` allocates an app/game object of size `0x8d0`, calls init at `FUN_14004b620`, then run/shutdown paths at `FUN_1400383e0` and `FUN_14003dd70`.
- `FUN_14004b620` logs `Version %d.%02d` with values `1, 0x6e`, matching a `1.110` style SOMA version string and making it a useful startup anchor.
- `config\game.cfg` sets the player camera defaults to `FOV="70"`, `FarClipPlane="1000"`, and `NearClipPlane="0.03"`.
- HPL2 source in `D:\Dev Debug\AmnesiaAMachineForPigs` and `D:\Dev Debug\AmnesiaTheDarkDescent` confirms the engine family uses SDL/OpenGL, `SDL_GL_SwapBuffers`, `glLoadMatrixf`, `glUniformMatrix4fv`, and fixed-function compatibility calls such as `glTexEnvfv`.
- First live log from `0.1.0-bootstrap` loaded successfully from `D:\Dev Debug\SOMAVR\build\Release\somavr.dll`, saw the NVIDIA OpenGL 4.6 context, and reached 2900+ frame summaries with no DLL errors.
- The first run proves the main scene projection is shader-uniform driven, not fixed-function: `fixedProjection={valid=0}`, while `uniformProjectionName="a_mtxModelViewProjection"` was projection-like with `fovYDeg=70.0000` and `aspect=2.3889`.
- Stable camera/config matches found so far: `config\game.cfg` has `FOV="70"`, `NearClipPlane="0.03"`, and `FarClipPlane="1000"`. Runtime matrices showed the 70 degree vertical FOV at the user's 3440x1440 viewport.

## Active Baseline

The active build candidate is `0.13.0-hands-identity`, layered on the
visually proven `0.9.0-calibration-haptics` OpenXR transport, native HPL camera
bridge, AFR stereo, full projection centering, one-key F10 activation, and
compatibility probes:

- The working stereo transform now also resolves HMD and controller poses into
  HPL world coordinates. Periodic controller rows report dominant aim/grip
  positions and directions for live validation before native pick injection.
- Listener orientation and room-scale head translation are composed only for
  the native FMOD update and then restored, preserving authored camera state.
- The final GUI hook can distinguish the exact gameplay HUD `cGuiSet` from
  menus, ImGui, subtitles, and diegetic sets via the confirmed game-context
  getter at `0x1400cc9b0`. It now reports the confirmed virtual-space and
  center-screen metrics needed to size a future alpha HUD target.
- The dominant controller's fully tracked world aim can replace only the
  start/direction passed to SOMA's native closest-entity wrapper at
  `0x1400cd750`. Strict query-type, native-origin, tracking, input, and
  authored-camera gates restore the original gaze query on any mismatch.
- SOMA still owns interaction ray length, LOS, `CanInteract`, distance policy,
  focus state, player-state transitions, physics, and map callbacks.
- The exact runtime `PlayerHands_*` entity is now recognized through confirmed
  `cLuxProp` GetName and SetMatrix registrations. Its original root matrix is
  left untouched while bounded logs correlate scale/basis/translation with the
  native camera, dominant grip pose, and authored player state.

- Invalid `xrLocateViews` output can no longer overwrite the last valid eye
  cache. Tracking samples have a configurable 30-frame usability bound and
  recover through a two-frame compositor blackout.
- Temporary pose expiry restores the native base view for that frame while
  preserving stereo intent, so valid tracking can resume automatically instead
  of requiring F10/F11 reactivation.
- Controller primary/secondary actions are available on either Touch/Index
  hand. Dominant hand and stick roles are configurable, with a bounded
  one-controller movement/action fallback when only one hand is active.

- `ReferenceSpace=local|stage` now supports seated/local and floor-aware standing
  calibration profiles with a logged fallback when STAGE is unavailable.
- OpenXR controller output haptics cover interaction, snap turn, menu, jump,
  crouch, and recenter. Focus loss clears the entire input snapshot immediately.
- Native base and extended roll are now measured from confirmed camera fields;
  opt-in temporary suppression is available for authored-camera comfort testing.
- OpenXR session/instance loss now schedules an in-process runtime rebuild after
  `RecoveryDelayFrames` instead of permanently suspending submission.
- Player/camera replacement invalidates both AFR eye caches and automatically
  re-runs stable-pose calibration against the new native camera.
- Snap turn and recenter can omit projection layers for two comfort frames while
  OpenXR frame pacing remains alive.
- Named post-effect policy suppresses ImageTrail, ChromaticAberration, and
  RadialBlur only during active stereo rendering and restores native state after
  every compositor call.
- The final GUI path now reports individual GUI-set classification fields and
  draw footprints, providing the next HUD capture dataset.

- OpenXR left-stick movement now drives SOMA's own W/A/S/D input route with
  configurable press/release hysteresis.
- Right-stick turning supports configurable snap or smooth mouse-path input.
- Right trigger/select maps to native interaction, menu maps to Escape, and a
  held two-grip chord requests the existing stable F2 recenter pipeline.
- Left trigger holds run, right A jumps, and right B toggles crouch on confirmed
  Touch/Index profiles through SOMA's shipped default action keys.
- Every injected held input is released on VR disable, inactive controls, stale
  OpenXR samples, or DLL teardown.
- `HPLPlayerState` now owns the signature-guarded player/camera/body and state
  getters. It also reads confirmed camera rotate mode `+0x6c` and body camera
  update ownership `+0x1e8`, logging every authored-camera transition.
- Controller gameplay input releases automatically while a scripted sequence or
  hand-socket attachment owns the camera. Menu and recenter remain available.
- Render-stage logs now include per-stage GL draw/state deltas. Active post
  effects are inventoried by object/vtable/flags, and `Ctrl+F12` can isolate one
  active effect at a time without persisting mutations.
- This is a fast testable bridge, not final analog locomotion. The live state log
  is intended to identify the safe native action boundary for its replacement.

- `somavr_injector.exe`: launch-suspended or attach-by-PID/process-name DLL injector.
- `somavr.dll`: MinHook-based OpenGL/WGL telemetry DLL.
- `somavr_common`: logger and INI config shared by the injector/DLL.

The DLL is still a probe, not a correct stereo renderer. It hooks:

- `gdi32!SwapBuffers` for frame boundaries.
- `opengl32!wglMakeCurrent` and `opengl32!wglGetProcAddress` for context and extension discovery.
- fixed-function `glMatrixMode`, `glLoadMatrixf`, `glViewport`, `glDrawElements`, and `glDrawArrays`.
- extension functions returned by `wglGetProcAddress`, including `glUniformMatrix4fv`, `glGetUniformLocation`, `glUseProgram`, `glBindFramebuffer`, and `wglSwapIntervalEXT`.

The live `0.2.2-xrloaderpath` log proved the OpenXR loader/runtime discovery path:

- `openxr_loader_load ok` from `build-openxr\Release\openxr_loader.dll`.
- `openxr_extensions ... khrOpenGL=1`.
- runtime `VirtualDesktopXR 1.0.10`.
- system `Meta Quest 3`, orientation and position tracking available.
- OpenGL requirements accepted SOMA's context: `minGL=4.0.0 maxGL=5.0.0`.
- primary stereo views reported as `2688x2880` per eye, opaque blend mode only.
- `SessionProbe=0` skipped `xrCreateSession` as intended.

The same run still crashed later in `VirtualDesktop.LibOVRRT64_1.dll` with `0xc0000005`, so `0.2.3-xroneshot` changed the no-session path to one-shot discovery:

- `FrameSummaryInterval=120`.
- `MatrixSampleLimitPerFrame=32`.
- `UniformMatrixProjectionOnly=1`.
- `UniformMatrixLogLimit=256`.
- `somavr_build_flavor.txt` beside each DLL records `openxr=0` or `openxr=1`.
- the injector warns if `[OpenXR] Probe=1` is paired with a non-OpenXR DLL.
- the non-OpenXR DLL logs `build_without_openxr` as an error with the corrective path.
- the OpenXR DLL explicitly preloads `openxr_loader.dll` from beside `somavr.dll` before calling OpenXR.
- the default generated config stays at `SessionProbe=0` for conservative static probes.
- after requirements/view/blend discovery, SOMAVR now destroys the OpenXR instance and logs `openxr_instance released_after_static_probe`.
- summaries now include `openxrInstanceAlive=` and `openxrInstanceReleasedAfterProbe=`.

The latest live `0.2.3-xroneshot` log is better:

- it loaded `version=0.2.3-xroneshot buildOpenXR=1` from `build-openxr\Release`;
- it reached runtime `VirtualDesktopXR 1.0.10`, system `Meta Quest 3`, OpenGL requirements, stereo views, and blend modes;
- it skipped session creation, released the instance, and later summaries reported `openxrInstanceAlive=0`;
- SOMA continued logging frame summaries for over a minute after OpenXR release;
- no newer SOMA WER crash was found after that run.

The live `0.2.4-xrsessiononeshot` log extended that safer lifetime model to `xrCreateSession`:

- `xrCreateSession` succeeded on the early startup GL context;
- reference spaces were `VIEW`, `LOCAL`, and `STAGE`;
- swapchain formats included `GL_RGBA16F`, `GL_SRGB8_ALPHA8`, `GL_RGBA8`, and depth formats;
- the session reached `READY`;
- SOMAVR destroyed both session and instance and logged `openxr_runtime released_after_probe reason=session_probe_complete`;
- later summaries reached frame `2040` with `openxrSessionAlive=0`, `openxrInstanceAlive=0`, and `openxrSwapchainFormats=7`;
- no newer SOMA WER crash was found after that run.

The live `0.2.5-xrframeprobe` log confirmed the same session one-shot on SOMA's real frame context:

- `OnOpenGLContext` recorded early contexts but deferred OpenXR bootstrap;
- frame `120` still showed `openxrAttempted=0`, then the OpenXR runtime was loaded from the frame path;
- `openxr_bootstrap requirements_ok` and `openxr_session_probe ok` both used `hdc=0x420117aa hglrc=0x30000`;
- the session reached `READY`, reported `VIEW`, `LOCAL`, and `STAGE`, and returned seven GL swapchain formats;
- the process continued through frame `3240` after release with no newer SOMA WER crash found.

The live `0.2.6-xrhold` log confirmed short live-session lifetime:

- active config is `Probe=1`, `SessionProbe=1`, `ReleaseAfterProbe=1`, `BootstrapFrame=120`, and `HoldFrames=600`;
- after a successful frame-context session probe at frame `120`, SOMAVR kept the OpenXR session alive through frame `720`;
- it polled OpenXR events during the hold;
- it released the session and instance with `openxr_runtime released_after_probe reason=hold_complete`;
- later summaries showed `openxrSessionAlive=0` and `openxrInstanceAlive=0`;
- it does not call `xrBeginSession`, create swapchains, or submit frames yet.

`0.2.7-xrmanual` changes the next probe from frame-timed startup to manual start:

- active config is `Probe=1`, `SessionProbe=1`, `ReleaseAfterProbe=0`, `BootstrapFrame=120`, `HoldFrames=0`, and `ManualStart=1`;
- SOMAVR still injects at process launch so WGL/OpenGL hooks are present before GLEW setup;
- OpenXR bootstrap is deferred until F8 is pressed, so the user can reach a loaded save first;
- the trigger logs `openxr_manual_start triggered key=F8 frame=... hdc=... hglrc=...`;
- after the trigger, successful session probes stay alive until process shutdown.

The live `0.2.7-xrmanual` run confirmed the manual gate and in-game context:

- F8 triggered at game frame `4200` after the save was loaded;
- `VirtualDesktopXR 1.0.10` loaded successfully;
- `xrCreateSession` accepted SOMA's active `hglrc=0x30000`;
- the runtime transitioned through `IDLE` to `READY`;
- two views and seven swapchain formats were available;
- the live session remained healthy while SOMA continued rendering.

`0.3.0-xrframe` advances that confirmed session into the first real presentation path:

- OpenXR processing runs at every `SwapBuffers`, independently of the 120-frame summary interval;
- F8 is therefore sampled every rendered frame instead of once every 120 frames;
- `OpenXRRuntime` owns events, session state, frame timing, view location, and composition;
- `OpenXRGLBridge` owns per-eye OpenGL swapchains, images, FBO validation, and backbuffer copies;
- the session begins only after `XR_SESSION_STATE_READY`;
- each running frame uses `xrWaitFrame`, `xrBeginFrame`, `xrLocateViews`, swapchain acquire/wait/release, and `xrEndFrame`;
- the current layer duplicates SOMA's desktop backbuffer to both eyes, so it proves transport but not stereo rendering;
- repeated frame failures are bounded and submission is suspended after 60 consecutive failures.

The live `0.3.0-xrframe` run passed the complete transport test:

- F8 triggered at frame `2783` on the expected main context `hglrc=0x30000`;
- both eye swapchains were `2688x2880`, used `GL_SRGB8_ALPHA8`, and exposed three images;
- the session reached `FOCUSED` and remained there;
- the run submitted at least `938` consecutive two-view projection layers;
- eye positions changed over time, proving live pose updates;
- no `openxr_frame failure`, submission suspension, or other OpenXR error appeared.

`0.3.1-cameramap` keeps that path intact and adds a bounded F9 capture:

- 120 rendered frames by default;
- camera-related `glUniformMatrix4fv` calls only;
- module-relative call stacks for direct Ghidra navigation;
- full 4x4 samples for each relevant uniform;
- synchronized head pose, orientation, view-validity flags, and IPD telemetry.

The two live F9 captures both completed. Sequence 1 included accidental mouse input; sequence 2 used only HMD yaw/roll/pitch. In the clean sequence the OpenXR orientation changed while SOMA's sampled camera matrices remained independent, giving a clean before-bridge control.

Ghidra and the HPL2 source now map the native path from the matrix upload back to `cCamera::GetFrustum` at `0x140271b80` and `cFrustum::SetupPerspectiveProj` at `0x140270230`. `0.4.0-hplcamera` hooks the former and calls the latter after composing a calibrated orientation delta. This keeps view-projection and culling derivatives together.

The first bridge is deliberately bounded:

- exact function-prologue signatures must match before the hook is installed;
- F10 is an explicit enable/disable and neutral-pose calibration gate;
- only perspective frustums with camera-like near/far/FOV/aspect values qualify;
- only the camera selected by the F10 press is modified;
- SOMA's pristine base view is retained and restored on disable;
- position tracking, per-eye separation, OpenXR FOV replacement, and true stereo rendering remain future work.

F9 now distributes each uniform's four full-matrix samples across the 120-frame window instead of consuming them immediately. This makes the next capture suitable for measuring native camera response over the full head movement.

The live `0.4.0-hplcamera` test passed:

- F10 drove the native HPL camera for `1210` consecutive renders, not mouse input;
- rotation reached about `35.7` degrees and matrix captures changed across the full F9 window;
- disabling F10 restored the cached base view;
- OpenXR continued beyond `1800` submissions without failures;
- IPD was stable near `0.06852` meters.

`0.5.0-afrstereo` established the current experimental stereo layer. F11 alternates the HPL camera between runtime left/right eye position and FOV, while persistent GL caches retain the latest image for each eye. Both cached images are submitted with the exact OpenXR poses used to render them. This is not simultaneous stereo: each eye updates on alternating game frames.

`0.5.1-compatprobe` added exact-signature passive hooks at the six native viewport stages and the FMOD listener update without changing camera, AFR, or audio behavior.

The live `0.5.1` test passed and is analyzed in `docs\RUNTIME_ANALYSIS_0.5.1.md`. F11 delivered user-confirmed stereo for more than `1765` submitted frames without transport or cache failure. World rendering resolves into FBO `11`, post effects resolve into FBO `0`, and screen GUI remains on FBO `0`. Listener vectors remained authored while HMD orientation changed, confirming that audio needed an independent head delta.

The live `0.5.2-audiopost` test is analyzed in `docs\RUNTIME_ANALYSIS_0.5.2.md`. Audio appeared correct, although a stronger directional-source test remains. The run reached frame `10920` and `6350` stereo submissions without transport or hook failure. F12 mainly increased contrast and did not affect the dominant defect, proving it is upstream of post composition. The user identified realtime shadows as different between eyes and movement-dependent.

`0.5.3-shadowjitter` was the preceding build. It kept F8/F10/F11/F12 and audio behavior unchanged and added an F7 zero-radius experiment for `avShadowMapOffsetMul`.

The live `0.5.3` result showed that F7 itself worked but the proposed uniform control point did not: all toggles had `uploads=0 overrides=0`. Reflection artifacts are now also confirmed. SOMA's cube/environment path is eye-vector dependent, while world reflections sample a reflection texture generated from a mirrored current frustum. This makes shared per-frame resources under AFR the leading common hypothesis.

`0.5.4-renderdiag` captured three trouble spots successfully. Direct eye camera
matrices alternate correctly, but live deferred shadow programs `942/944` and
world reflection program `989` receive their inverse camera and screen
reconstruction values through uniform blocks. This is the first common mechanism
that directly fits the observed shadow and reflection displacement.

The live `0.5.5-reconstruct` run confirmed that F5 removes the left/right shadow
disagreement. Shadow UBO offset `96` changes from `-0.242513/+0.242513` to `0/0`,
so centered horizontal projection is now the active compatibility policy. The
remaining shadows and lighting are stereo-consistent but move with HMD position;
program `988` also identifies a view-depth reflection/refraction fade candidate
for the moving opaque window boundary.

The live `0.5.6-stability` run proved clean shutdown. The lifecycle hook installed,
released OpenXR before HPL Graphics teardown, and SOMA disappeared normally. F3
patched program `985` for `369` draws without visual change, rejecting reflection
distance fade. F4 showed little translation dependence; shadows instead correlate
strongly with HMD pitch and roll.

`0.5.7-fullcenter` established the current visual baseline. The prior policy left vertical projection offset
`-0.193187`; the new policy centers both projection axes while preserving tangent
span and synchronized submitted FOV. This directly tests the pitch/roll, diagonal
ceiling, and top-down window symptoms.

The user confirmed `0.5.7` fixes all observed shadow and reflection defects.
`0.5.8-onekey` promoted F10 to the normal usability path. F10 requests OpenXR and holds a
pending activation until valid pose/stereo views arrive, then enables tracking,
AFR stereo, and full centering together. A second F10 cancels or exits. F8/F11
remain diagnostic controls only.

The first architecture maintenance pass keeps the `0.5.8-onekey` behavior and
binary contract but moves deterministic responsibilities out of runtime hooks:
`HPLCameraMath` owns pose/projection math and the proven fully centered FOV policy,
`OpenGLMatrixAnalysis` owns matrix telemetry classification, and `OpenXRHelpers`
owns OpenXR names and view/pose conversion. `somavr_render_math_tests` runs in both
build flavors. Module boundaries and the next safe extractions are recorded in
`docs\ARCHITECTURE.md`.

The first live `0.5.8` test exposed severe view skew during HMD yaw and pitch.
The extracted quaternion-to-matrix function had an incorrect XY cross-term and
therefore generated a shearing, non-orthogonal camera rotation. `0.5.9-rotationfix`
corrects that term and adds orthonormality plus quaternion/matrix agreement tests.
That rigid-rotation fix remains in the active build; all one-key, full-center,
AFR, room-scale, audio, and shutdown policies are otherwise unchanged.

The live `0.5.9` test confirmed rigid camera rotation, then exposed a separate
one-key startup problem: OpenXR frame `2857` reported head `Y=-1.244683`, F10
captured it immediately, and frame `2858` settled roughly `1.79 m` higher. That
reference-space transition was incorrectly applied as room-scale head movement.
`0.5.10-poselatch` requires tracked position/orientation and eight consecutive
settled unique poses before neutral capture. Large startup jumps reset the latch;
projection, stereo, world scale, and normal physical head translation are unchanged.

The user confirmed `0.5.10` has no current graphical issues. `0.5.11-recenter`
keeps that path and adds F2 as an in-session neutral-pose recenter. It uses the
same tracked/stable latch as F10, leaves OpenXR and AFR stereo running, continues
rendering with the old neutral pose while waiting, then atomically replaces the
neutral orientation and position once eight stable tracked samples arrive.

`0.6.0-input-foundation` batches the first controller, calibration, tracking
diagnostic, and release-identity foundations without changing the proven default
camera transform. `OpenXRInput` owns a seven-action gameplay set, suggested
Simple, Touch, Index, and Motion Controller bindings, per-frame action synchronization, and
left/right grip and aim spaces. The public snapshot includes move/turn axes,
select, squeeze, menu, pose validity, and tracked bits. No snapshot value is fed
into SOMA yet, so keyboard/mouse behavior and native gameplay remain authoritative.

The camera bridge now supports `HPLRoomscaleVertical` and
`HPLEyeHeightOffsetMeters`. Their active defaults (`1` and `0.0`) are mathematically
identical to `0.5.11`; they provide a reversible route to seated/standing tuning.
Head snapshots now report sample age, and every build emits a SHA-256 manifest
beside the DLL.

The exit minidump disproved the earlier orphan-worker diagnosis for this run. It
contains only SOMA's main thread in OpenGL with Virtual Desktop runtime frames.
Ghidra names `HPL3_cSDLEngineSetup_Destructor` at `0x1403b16e0`. The `0.5.5`
lifecycle hook failed closed because its guard omitted the leading `0x40` byte;
`0.5.6` uses the exact installed sequence and again attempts OpenXR release before
HPL deletes Graphics and calls `SDL_Quit`. The dumper remains capture-only.

The design choices borrowed from UEVR and Praydog's analysis are recorded in `docs\UEVR_LEARNINGS.md`. Unreal-specific object assumptions were deliberately not imported.

A docs-only static RE pass now maps future locomotion, hands/tools, HUD, and full-screen effects in `docs\FUTURE_SYSTEMS_RE.md`. It identifies the main-loop and viewport order, semantic character-body movement route, camera-follow hand model, HUD/ImGui/world-GUI split, and the priority-sorted post-effect chain. No current `0.5.0-afrstereo` binaries or runtime configuration were changed by that pass.

A second compatibility pass in `docs\VR_COMPATIBILITY_RE.md` maps controller ownership onto SOMA's existing pick and PID physics, inventories authored camera states, identifies the FMOD listener commit, classifies loading/video presentation, and narrows the same-frame stereo boundary. `docs\FEATURE_TRACEABILITY.md` assigns stable `FEATURE.*` IDs and acceptance gates, and the local Graphify graph indexes these documents with the implementation. This remains documentation/tooling work only; the `0.5.0-afrstereo` binary is unchanged.

The shared `Soma_NoSteam.exe` Ghidra database was synchronized again on
2026-07-13. The engine setup destructor was named and commented, and lifecycle
and runtime-hang bookmarks record the exact pre-SDL shutdown boundary and dump
evidence. The complete ledger is in `docs\GHIDRA_SYNC.md`.

The OpenXR build now asks for:

- instance extension availability, especially `XR_KHR_opengl_enable`;
- runtime and HMD system properties;
- OpenGL graphics requirements for the live SOMA HDC/HGLRC;
- primary stereo view sizes and swapchain sample counts;
- environment blend modes;
- optional `xrCreateSession` using `XrGraphicsBindingOpenGLWin32KHR`;
- reference spaces, swapchain formats, and early session-state events when session creation succeeds.

## Next Step

1. Launch the current OpenXR build:

```powershell
& "D:\Dev Debug\SOMAVR\build-openxr-controller\Release\somavr_injector.exe" --launch "G:\SteamLibrary\steamapps\common\SOMA\Soma_NoSteam.exe"
```

2. Confirm `version=0.13.0-hands-identity`, seven render-stage/GUI hooks,
   `hpl_interaction_bridge install_ok`, `hpl_hands_bridge install_ok`,
   `referenceSpace=local`, `recovery=1`, and controller haptics enabled.
3. Load a save game, face forward, and press F10 once.
4. Confirm `hpl_vr_mode requested`, API-attributed `openxr_manual_start triggered`,
   one or more `calibration_wait` rows, then `hpl_vr_mode activated` with
   `fullyTracked=1 stablePoseFrames=8` and tracking/stereo/centering enabled.
5. Confirm two `openxr_swapchain created` rows, `openxr_frame_resources ready`, and `openxr_session_begin ok`.
6. Confirm `openxr_frame ok ... layers=1 views=2` repeats and the headset receives SOMA's mirrored desktop image.
7. Confirm later summaries show `openxrFrameResourcesReady=1`, `openxrSessionRunning=1`, increasing `openxrSubmittedFrames`, and `openxrFrameSubmitFailed=0`.
8. Make small yaw and pitch movements first. The world must rotate rigidly with no
   shear, diagonal stretch, or scale change; press F10 immediately if it does not.
9. Confirm the first `hpl_stereo` eye offset is near IPD scale rather than metres,
   and that the native player eye height is retained.
10. Confirm alternating `hpl_stereo ... eye=0/1`, ready eye caches, and `openxr_frame ... stereo=1` without pressing F11.
11. Without pressing F5, test shadow motion using deliberate pitch and roll, then
    inspect the ceiling angle and window/oven boundary while moving normally.
12. Confirm `hpl_stereo` rows report `projectionOffset=0.000000,0.000000`.
13. Press F2 while facing a new comfortable forward direction. Expect
    `hpl_recenter requested`, then either bounded `calibration_wait` rows or
    `hpl_recenter applied ... stablePoseFrames=8`. Confirm tracking/stereo stay
    active and view height remains correct.
14. Load another save or cross a map transition. Expect `camera_replaced`,
    `openxr_stereo_cache invalidated`, calibration rows, and automatic stereo
    resumption without another F10 press or stale-eye flash.
15. Snap-turn and recenter. Confirm bounded `openxr_comfort_blackout` rows and
    no OpenXR frame failure or session restart.
16. Exercise visible damage/motion effects. Inventory rows should use effect
    names/priorities; the image trail, chromatic aberration, and radial blur
    counters may increase while tone mapping/fades remain visible.
17. Point the dominant controller away from screen center at several usable
    objects. Confirm focus follows controller aim and bounded
    `hpl_interaction_ray` rows report `applied=1`; then test authored-camera and
    tracking-loss fallbacks.
18. Capture gameplay, menu, subtitle, and terminal moments. Attach bounded
    `hpl_gui_set` rows including `hudMetrics` so HUD virtual-space calibration
    and diegetic GUI classification can be checked.
19. Confirm haptic pulses for discrete actions, then briefly remove runtime
    focus while holding movement and verify immediate release plus one loss and
    restoration transition in the log.
20. Exercise visible normal and authored hand/tool states. Attach
    `hpl_hands_identity` and `hpl_hands_pose` rows with quarter/full scale,
    camera distance, grip distance, and player-state transitions.
21. Confirm `somavr_build_manifest.txt` reports version
    `0.13.0-hands-identity`, flavor `openxr`, and a DLL SHA-256.
22. Exit normally. Confirm `hpl_lifecycle pre_graphics_shutdown begin` and
    `complete`, then check that `Soma_NoSteam.exe` disappears. If it remains,
    capture it with the dumper before manually terminating it.
