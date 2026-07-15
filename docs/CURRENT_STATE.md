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

The active build candidate is `0.55.0-ssao-frame-owner`, layered on the
visually proven `0.9.0-calibration-haptics` OpenXR transport, native HPL camera
bridge, AFR stereo, full projection centering, one-key F10 activation, and
compatibility probes:

- Temporal SSAO now has explicit per-eye GPU history. The exact native writer
  at `0x1403f2b50` reads and overwrites renderer texture `+0xe78` once per eye;
  the active profile banks two same-format GL copies around that function while
  preserving SOMA's shaders, framebuffer, and AO pipeline. Generated configs
  leave it off. Live headset acceptance must confirm stable AO during head
  translation/rotation, reset behavior, and no new GL errors or shutdown leak.

- ToneMapping now has explicit same-frame ownership for its authored exposure,
  white-cut, window fade, color-grading transition, and film-grain sampling
  packet. Ghidra confirms
  native update `0x1402842d0` runs inside each render. The first eye commits one
  update; the opposite eye replays the same baseline and leaves the first
  commit resident. AFR remains native. The active profile enables this guarded
  prototype; generated configs and `HPLToneMappingFrameControl=0` retain native
  behavior. Live tone/bloom/fade acceptance remains.

- ImageTrail is the first full post-effect temporal resource with explicit
  per-eye ownership. Confirmed native fields hold its framebuffer at `+0x50`,
  accumulation texture at `+0x58`, amount at `+0x98`, and clear flag at
  `+0xa0`. The active profile allocates a second native pair and banks the
  resource pointers plus clear state by actual eye/pose. Recenter/stale gaps
  clear both histories; exact destruction releases both pairs. Any signature,
  pointer, alias, or sequence failure immediately falls back to the proven
  render-only ImageTrail suppression. This awaits headset acceptance.

- A locomotion-gated comfort vignette now owns a dedicated VIEW-space OpenXR
  alpha layer. `HPLInputBridge` publishes motion intensity only after gameplay
  input survives loading, pause, panel, terminal, dead-state, and authored-
  camera policy. A tested 250 ms envelope fades a transparent-center radial
  mask in and out; stale samples fail to zero. Smooth turning can contribute,
  while snap turn keeps the existing black-frame path. The active profile and
  balanced/maximum presets enable it, F1 can toggle it, and generated configs
  remain off.

- The compositor HUD now supports both flat and curved presentation. When
  `XR_KHR_composition_layer_cylinder` is available, the active profile submits
  the captured alpha HUD on a 70-degree head-locked cylinder whose physical arc
  width, aspect-derived height, and center distance match the quad contract.
  The F1 panel switches shape live. Missing-extension and layer-validation paths
  retain or restore the quad without changing HUD capture or scene stereo.

- Inventory presentation now has an exact native activity authority. The
  signature-guarded `cLuxUserModule::OnAction` wrapper admits current ImGui to
  the HUD capture only for user-module ID `15`, open-inventory action `12`, and
  a pressed edge. Its five-second authorization covers the shipped three-second
  display plus `0.6/s` fade-out without broad GUI interception. Main menu is
  already owned by the exact pause gate; hints and credits use GameHudImGui,
  while crosshair, descriptions, infection, and flashes use GameHudSet.

- Shipped wake, game-over, and credits presentation ownership is now explicit.
  Exact `WakeHandler` global dispatch controls XR sleep blackout and a bounded
  wake current-ImGui capture window; exact dead state `17` authorizes the
  game-over current-ImGui set and a dominant-controller continue action.
  Credits already use GameHudImGui and therefore need no broader GUI capture.
  All additions fail closed behind `HPLScriptedPresentationControl` and await
  a live wake/death/credits pass.

- SOMA's script-authored gamepad rumble now crosses into OpenXR at the exact
  registered `SetRumble` wrapper. Damage, death, attacks, locked interactions,
  datamining/tool sequences, and authored environmental effects retain native
  strength/duration semantics as bilateral segmented VR haptics. The original
  physical-gamepad path is always called, repeated script updates are
  throttled, and explicit falling edges stop both OpenXR outputs.

- Sustained physical roomscale displacement can now advance the native player
  capsule through exact-signature guarded feet-position wrappers. The path is
  restricted to unpaused Normal/Normal ownership, requires the existing head
  safety result to be clear, samples the body volume before each 0.015 m step,
  and compensates the HMD neutral position to keep the rendered world fixed.
  It is enabled in the active test profile and has a one-line config rollback;
  headset/collision acceptance remains.

- Same-frame stereo is now available as an explicit opt-in sustained prototype.
  It replays only the exact player viewport, suppresses screen GUI on eye two,
  and leaves the engine viewport enumerator, update/script lifecycle, frame/stat
  reset, GUI, XR submission, and presentation once per game frame. Immediate
  cache failure or an eye/pose-sequence mismatch disables the mode and restores
  AFR. The active profile exposes the F1 panel control but starts it off.
  Automatic bounded samples and `Ctrl+F6` remain available for temporal mutation
  evidence around the stateful post-post phase at `0x1401f1480`.

- The first confirmed temporal resource is now isolated per eye. Ghidra and
  HPL2 source identify `*(renderer+0x438)+0x80` as a 64-byte previous-view
  matrix. During continuous exact-player rendering, SOMAVR restores the pending
  eye's bank before the viewport and captures SOMA's native update afterward.
  Renderer/history changes and eye/pose mismatches reset or fault closed to the
  original shared path. The active profile enables the control; the F1 panel
  exposes its live state.

- `0.47.0` broadens that exact packet bank to the normal F10 AFR path as well as
  same-frame stereo. This prevents an AFR eye from inheriting the other eye's
  immediately preceding previous-view matrix. Recenter generation changes and
  pose gaps over eight frames reseed from native state, preventing stale motion
  after calibration, loading, or tracking recovery.

- OpenXR controller coverage now includes the standard HTC Vive profile in
  addition to Simple, Touch, Index, and Microsoft Motion. Runtime interaction-
  profile change events resolve and log the exact active profile independently
  for each hand, making reconnect, one-hand fallback, and runtime/headset matrix
  results directly attributable.

- Configuration and startup diagnosis now have user-facing ownership. Comfort
  presets apply before explicit keys, preserving every tuned override. The
  injector doctor checks the complete launch prerequisites without starting the
  game; the local OpenXR build currently reports seven passes, one expected
  developer-layout warning, and zero failures. `USER_GUIDE.md` is included in
  the release checksum ledger.

- A dedicated head-locked OpenXR status/options panel is now available through
  `F1` or `Menu + Secondary`. It owns a separate alpha swapchain and reports
  tracking, stereo, input, player/authored-camera, roomscale, projection, HUD,
  and reticle state. Stick plus select/trigger controls recenter, same-frame
  stereo, and reversible runtime options while exclusive input ownership prevents
  actions leaking into SOMA. This is built and unit-tested but still needs headset
  acceptance.

- Exact wall/handheld terminal states `8/9` now route dominant-controller aim through
  SOMA's native virtual ImGui cursor boundary. Current-ImGui identity,
  GameHud exclusion, 3D-set ownership, readable virtual layout, tracking, and
  config gates all fail closed to the original engine path. Trigger/select
  remains a native mouse click and releases on every terminal-state exit.

- The exact `HPL3_PostEffect_RenderOne` boundary now wraps each active effect in
  a read-only GL resource capture. Bound textures are identified by target, GL
  ID, level-zero dimensions, depth, and internal format; framebuffer writes and
  HPL input/output object identities are recorded alongside eye and pose frame.
  Ownership is classified only from resource-bearing left/right captures with
  the same nonzero pose frame. Startup and interval sampling are bounded, while
  `Ctrl+F6` forces both halves of the one-frame replay pair. This closes the
  evidence gap before per-eye tone/bloom/grading promotion or temporal-history
  duplication.

- Exact independent `HudObject` tools and Grab-state physics bodies now have an
  optional two-hand direction owner. The dominant grip remains the position
  anchor; a squeezed, fresh support grip inside configurable separation bounds
  supplies aim. Tools receive the composed basis at the existing exact matrix
  boundary. Physics bodies receive only a shortest-arc target through SOMA's
  confirmed torque PID. Both engagement and release re-anchor before applying
  torque, and every invalid/support-loss state immediately uses dominant-only
  or native behavior.

- `Ctrl+F6` and the automatically spaced samples retain the bounded diagnostic
  form of the exact-player replay. They force temporal mutation/resource capture
  around both eyes. `0.45.0` adds a separate sustained toggle through the F1
  panel; ordinary continuous frames use the same replay boundary without the
  expensive snapshots. Logs require opposite eyes from one tracked pose and
  expose the unavoidable duplicated post-post callback.

- Optional compositor depth is now fully wired. The OpenXR runtime negotiates
  D24/D32F or matching depth-stencil formats, builds matching per-eye depth
  caches and swapchains, and chains
  `XrCompositionLayerDepthInfoKHR` using the confirmed standard HPL/OpenGL depth
  convention. HPL near/far are converted to meters through `HPLWorldScale`.
  Unsupported formats, invalid clip data, missing caches, and copy failures all
  retain the proven color-only submission path.
- OpenXR frame resources now survive changing runtime contracts. A changed
  SOMA HDC/HGLRC triggers full delayed runtime recovery; changed recommended
  view dimensions or sample limits trigger a frame-resource rebuild. Stable
  view checks occur every 300 game frames without reallocating resources.

- Native subtitle presentation now has a narrow VR adapter. The exact voice
  subtitle draw worker at `0x1401c8dd0` reads layout from its
  `cLuxVoiceHandler` owner. During active stereo, `HPLSubtitleBridge` applies
  configured width/font/Y/shadow scaling only for that native call and restores
  the original values before returning. Localization, speaker names, timing,
  gradual reveal, font selection, and native enable settings remain untouched.
- The HUD layer now admits SOMA's exact current ImGui set while the confirmed
  pause getter reports `paused=1`. This gives the existing controller pointer a
  stable head-locked pause surface without broad ImGui interception. Main-menu,
  loading, game-over, terminal, and other non-paused owners still need explicit
  classification.

- F10/F11 and the diagnostic camera controls are now owned by the exact player
  camera whenever `HPLPlayerState` can identify it. Secondary viewport cameras
  retain native frusta and cannot consume activation edges or inherit headset
  pose. Bounded viewport identity rows expose camera/world/renderer/post/FBO,
  dimensions, flags, and player/secondary role for future reflection, terminal,
  save/load, and same-frame render work.
- The `0.32.0` per-eye depth evidence path remains available independently as a
  probe and provides bounded source/copy/sample telemetry around submission.
- The injector warns about common graphics/VR hook conflicts and blocks a
  duplicate SOMAVR injection. The release package now includes reversible,
  checksum-verified install/update/uninstall scripts that preserve user config.

- Exact script identity now separates three viewmodel owners at the shared
  Lux-entity matrix boundary. `PlayerHands_*` retains its existing controller
  root, exact `HudObject` can follow the dominant grip with its native scale,
  and `*_HudObject` inventory tools remain socket-owned so they are not
  transformed twice. Every tracking, state, authored-camera, scale, and math
  failure forwards SOMA's original matrix. The exact native DestroyEntity path
  evicts cached identities before queueing, preventing pointer reuse from
  applying an old tool policy to a newly created entity.
- The HUD capture now accumulates exact GameHudSet, exact
  `SOMA_GetGameHudImGui()->GetSet()`, and paused exact-current-ImGui draws into
  one transparent target per game frame. The pause getter is the hard ownership
  gate; diegetic and non-paused current ImGui sets stay native. The first exact
  draw clears, later exact draws append, and native GL state is restored after
  each set.
- Periodic `hpl_render_transaction` rows summarize the complete six-stage
  viewport transaction and identify a possible world-only replay scope without
  claiming callback safety. The new one-frame replay supplies that live proof
  without changing ordinary AFR frames.

- Script-created screen materials are now identified only by SOMA's exact
  `Screen Particle<decimal>` billboard name. While F10 VR is active, their
  shipped `0.15` camera-relative distance and native size are scaled together
  to a configurable `1.5 m`, preserving apparent coverage without near-field
  stereo convergence. The dedicated bridge restores native behavior outside VR
  and removes identities at the exact billboard destruction boundary.

- Exact registered FOV, FOV-multiplier, and aspect-multiplier leaf wrappers now
  have a reversible active-VR comfort policy. Scripted zoom/FOV requests resolve
  to the player's native default FOV and both multipliers resolve to `1.0` while
  F10 tracking is active; native requests and fade speeds return unchanged when
  VR is inactive or a channel is disabled.
- `HPLPresentationBridge` queries SOMA's exact loading-screen visibility once per
  game frame. Load entry invalidates both AFR caches, submits zero XR layers,
  and releases controller input; load exit invalidates again and adds a bounded
  two-frame guard before stereo repopulates. The desktop retains SOMA's native
  loading backbuffer.
- Signature-guarded `CreateVideo`/`DestroyVideo` hooks record stream identity,
  source name, active count, and peak concurrency without changing playback.
  This is deliberately a classifier for fullscreen versus diegetic video, not
  a video presentation override.

- Physical room-scale head translation now uses SOMA's confirmed world
  line-of-sight query to sweep a configurable center/radial/top/bottom head
  volume. One cached earliest-safe result is decomposed back into every
  eye/controller pose, preserving IPD, eye height, authored camera motion, and
  hand/flashlight coherence. The active profile includes moving geometry in the
  same sampled sweep; a static-only rollback remains configurable. Native
  capsule movement remains separate.
- The desktop mirror can now show a stable cached left or right eye with fit,
  fill, or stretch layout after XR submission. Native mode leaves the original
  backbuffer untouched. Existing render-stage hooks also accumulate left/right/
  mono CPU and nonblocking GPU timings for dual-render budgeting. A bounded
  timestamp-query pool drops saturated samples rather than stalling the game.
- OpenXR depth capability is now explicitly probed. Supported runtimes enable
  `XR_KHR_composition_layer_depth`, and one evidence row combines extension
  state, framebuffer depth bits/range, and HPL near/far projection data. Per-eye
  depth swapchains and submission remain gated on that live evidence.

- The exact scripted `Flashlight` light now follows the dominant controller's
  tracked aim pose through the existing guarded Lux-entity transform boundary.
  Independent local offset/rotation calibration is available. SOMA still owns
  light lifetime, fade/color, visibility, radius/FOV, particles, sensors, and
  callbacks; authored cameras, stale/lost tracking, and invalid state restore
  the original camera-mounted transform.
  Its three randomized agent-gobo gameplay rays now preserve their native cone
  while using the same cached light origin and controller-relative basis. Tool
  interaction and camera-animation grounding rays remain native.

- Wheel, Slide, SwingDoor, Lever, and Tear states now accept dominant-hand
  physical movement through SOMA's existing analog-look path. The adapter uses
  controller position relative to HMD position, projects onto head-right/up,
  bounds the relative mouse delta, and leaves all native constraints and scripts
  authoritative. The active profile enables it; generated configs default off.
- `HPLHudBridge` still correlates every rendered `cGuiSet` with current and
  gameplay-HUD `cImGui` ownership. It now promotes only the confirmed paused
  current owner into capture; telemetry remains the classifier for loading,
  wake, game-over, credits, inventory, and diegetic surfaces.

- A guarded native comfort bridge now intercepts SOMA's semantic camera-add
  setter. During active VR it zeros only Bob, Shake, and optional Sway while all
  authored movement/state channels remain native. It now also owns the exact
  Set/Fade camera-roll wrappers: Lean, Move, and Climb roll are disabled in the
  active profile while Script roll remains native. A reversible world DoF guard
  and exact VideoDistortion post-effect policy apply only during active tracking.
  Generated configs keep the native control boundaries disabled.
- Exact player-state IDs now drive transition telemetry and a two-frame comfort
  blackout around Ladder, ClimbLedge, InteractiveCameraAnimation, Sit, and Dead
  entry/exit. This hides abrupt authored pose handoffs without replacing state,
  constraints, animation, scripts, camera movement, or FOV.
- Controller closest-entity results publish validated entity/body pointers,
  native hit distance, and an HPL world hit point. The same exact aim pose and
  distance drive an application-space OpenXR reticle quad with compositor-correct
  binocular depth, while `HPLCrosshairBridge` observes SOMA's exact
  `eCrossHairState` decision through the registered global-script boundary.
- The reticle is age, tracking, distance, size, stereo, resource, and semantic
  guarded. It loads the 34 native icon files named by shipped `Player.hps`,
  preserves their aspect inside the quad, applies broad intent colors, and falls
  back to the procedural cross if loading fails. Both semantic gating and native
  artwork can be disabled independently in configuration.
- Optional focus-change haptics use native entity/body identity plus the same
  semantic state. Pickup, manipulation, traversal, social, unavailable, and
  simple-hint classes receive bounded profiles; default-cursor focus stays silent.

- Optional head-relative movement now rotates the movement stick by calibrated
  HMD yaw while rejecting pitch and roll. Optional physical crouch calibrates
  standing HMD height and drives SOMA's existing crouch toggle with hysteresis.
- A signature-guarded native grab path now augments only SOMA's exact Grab-state
  position PID error with dominant-controller translation. The first pickup
  sample and every unsafe state remain native, and SOMA still owns object mass,
  gravity, force limits, collision, joints, and callbacks.
- Grab rotation now adds the dominant grip's shortest-arc orientation target to
  SOMA's exact torque-PID error. Dominant primary still requests the native throw,
  but the exact AddImpulse wrapper can redirect that one authored impulse along
  tracked controller velocity or grip-forward aim with bounded velocity scaling.
- The compositor HUD can clear only a configurable center rectangle after the
  exact GameHudSet render, suppressing the native gaze crosshair without touching
  the eye images or other HUD regions.

- The working stereo transform now also resolves HMD and controller poses into
  HPL world coordinates. Periodic controller rows report dominant aim/grip
  positions and directions for live validation before native pick injection.
- Listener orientation and room-scale head translation are composed only for
  the native FMOD update and then restored, preserving authored camera state.
- `HPLHudBridge` now owns the exact gameplay HUD `cGuiSet` boundary at
  `0x140213970`, identified through the confirmed game-context getter at
  `0x1400cc9b0`. With `HudLayer=1`, only that 2D set is redirected into a
  transparent GL target and submitted as an alpha-blended VIEW-space OpenXR
  quad. Existing virtual/center-screen metrics remain in bounded telemetry.
- Paused current ImGui and native voice subtitles are now covered by narrow
  adapters. Main/loading/game-over menus, terminals, and every 3D/diegetic GUI
  remain native. Missing signatures/resources, non-visible XR state, or failed
  validation restore normal native rendering.
- The dominant controller's fully tracked world aim can replace only the
  start/direction passed to SOMA's native closest-entity wrapper at
  `0x1400cd750`. Strict query-type, native-origin, tracking, input, and
  authored-camera gates restore the original gaze query on any mismatch.
- SOMA still owns interaction ray length, LOS, `CanInteract`, distance policy,
  focus state, player-state transitions, physics, and map callbacks.
- The exact runtime `PlayerHands_*` entity is recognized through confirmed
  `cLuxProp` GetName and SetMatrix registrations. `HandControllerRoot=1`
  replaces only uniform quarter-scale Normal/Normal matrices with a dominant
  tracked-grip root reconstructed from SOMA's native `rotateY(pi)` convention.
  Position and model-space XYZ rotation are configurable. Full-scale, authored,
  non-normal, stale, lost-tracking, and malformed states remain native.
- Ordinary unpaused gameplay now receives radial-deadzone analog movement
  through the registered character-body Move wrapper and exact-degree snap or
  smooth body yaw through AddYaw. A signature-guarded game-pause getter plus
  Normal/Normal ownership gates prevent direct input in special states. A pause
  result now suppresses the semantic W/A/S/D/mouse fallback too, closing the
  previous possibility of controller input continuing behind menus.
- While paused, the dominant controller aim is projected relative to the HMD
  into SOMA's native client rectangle. Trigger/select uses the existing left
  mouse path, with a release latch preventing an accidental world interaction
  when the menu closes. Invalid pose/window/pause state fails closed.
- In two-controller play, the support-hand primary/secondary buttons now route
  through SOMA's existing flashlight and inventory actions. Dominant-hand role
  changes move those actions with the support hand; one-hand recenter is preserved.

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
  crouch, recenter, semantic focus, and SOMA-authored gameplay rumble. Focus
  loss clears the entire input snapshot immediately.
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
- This remains a guarded feature build: physical crouch toggle synchronization,
  grab translation/rotation stability, throw direction/scale, and center-clear
  HUD coverage require live acceptance before defaults can be enabled globally.

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
2026-07-15. In addition to lifecycle evidence, the exact hand SetMatrix and
game-pause functions now carry the `0.16.0` controller-root and complete
pause/menu-input ownership contracts. The complete ledger is in
`docs\GHIDRA_SYNC.md`.

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
& "D:\Dev Debug\SOMAVR\build-openxr\Release\somavr_injector.exe" --launch "G:\SteamLibrary\steamapps\common\SOMA\Soma_NoSteam.exe"
```

2. Run `somavr_injector --doctor <Soma_NoSteam.exe>` and require zero failures,
   then confirm `version=0.52.0-per-eye-image-trail`,
   `hpl_per_eye_view_history initialized configured=1 packetBytes=0x40`, and no
   hook/signature failure. Load a save, face forward, and press F10 once.
3. Confirm the proven rigid world, eye height, centered projection, depth,
   shadows, reflections, controller input, HUD, and audio before changing mode.
   Open F1 and confirm `HUD SHAPE: CURVED`; toggle to `QUAD` and back while
   checking identical alpha, center distance, vertical placement, and content.
   An unsupported runtime must show `HUD SHAPE: QUAD ONLY` without XR failures.
4. Walk at partial and full stick, release, and confirm the comfort vignette
   fades only at the periphery. Open F1 and toggle it off/on; pause, terminal,
   loading, dead, authored-camera, and panel ownership must release it. Snap
   stick hold must not sustain it; smooth turn may drive it.
5. Before changing modes, `VIEW HISTORY` must already be `ACTIVE` in F10 AFR.
   Expect alternating eye `0/1` restores/captures. Recenter once and confirm a
   single generation reseed. Then enable `SAME FRAME STEREO` and confirm both
   eye transactions share each pose identity without a history fault.
6. Exercise quiet, reflective, shadowed, tone/bloom, fade, terminal, inventory,
   pause, authored-camera, and loading scenes while rotating and translating the
   HMD. Stop on cross-eye history, skew, changing shadow/reflection position,
   stale frames, duplicated GUI, or unacceptable pacing.
7. Toggle same-frame stereo off. AFR must return immediately and `VIEW HISTORY`
   must remain `ACTIVE`. Toggle on once more and confirm a clean transition.
8. Set `HPLPerEyeViewHistoryControl=0` for the direct rollback test; status must
   show `UNAVAILABLE` and native shared history must remain untouched.
9. Confirm the build manifest version/flavor/hash, exit normally, and attach the
   full log with final dual-render and per-eye-history summaries.
   Include the two `openxr_input interaction_profile` rows and verify they name
   the controller profile actually in use.
