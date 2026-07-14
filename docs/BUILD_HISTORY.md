# Build History

## 2026-07-15

### 0.26.0-gpu-depth-probe

- Added nonblocking per-eye GPU telemetry to the six existing guarded HPL
  render-stage hooks. Nested-safe `GL_TIMESTAMP` pairs use a bounded pool, are
  polled only after availability, and are attributed at stage end. Pool
  exhaustion drops samples instead of stalling SOMA.
- Added an opt-in OpenXR depth-capability probe. The runtime detects and
  conditionally enables `XR_KHR_composition_layer_depth`, then records default
  OpenGL depth precision/range with confirmed HPL projection near/far planes.
  This build deliberately does not submit depth layers yet.
- Missing timer-query entry points and unsupported depth extensions are isolated
  fallbacks. Generated configs keep both probes disabled; the active profile
  enables a 128-pair pool and depth evidence capture. Both build flavors pass.
  OpenXR SHA-256:
  `67C0A882F995EDD41F38A958951105377A347AC21EF138F9242D40CBE12C56F0`.
  Package SHA-256:
  `7658C575D49616064D829C56F08F89425C1AC5B92DD2826EE29FE941E57990E9`.

### 0.25.0-volume-spectator-telemetry

- Replaced the point-only room-scale safety sample with a configurable swept
  head-volume approximation: center, horizontal radial ring, and top/bottom
  static-world probes all share the confirmed `SOMA_CheckLineOfSight` boundary.
  The earliest valid obstruction controls one coherent eye/controller offset;
  probes whose authored start is already obstructed are skipped rather than
  trapping the player.
- Added desktop spectator controls sourced from the existing AFR eye caches.
  `DesktopMirrorEye=left|right` presents a stable eye only after XR submission;
  `native` is a no-op rollback. Fit, fill, and stretch policies use tested blit
  layouts, restore GL framebuffer/buffer/scissor/clear/color-mask state, and
  fail back to SOMA's native backbuffer.
- Added opt-in per-eye CPU telemetry to the six existing signature-guarded HPL
  render-stage hooks. Left, right, and mono call counts, average microseconds,
  and total milliseconds are emitted periodically and at shutdown without new
  executable detours. GPU timestamps remain future work.
- Added deterministic head-volume sample and spectator-layout tests. Generated
  configs keep all controls off/native; the active profile enables a six-point
  radial ring, two vertical probes, stable left-eye fit mirror, and per-eye CPU
  timing. Built and tested both default and OpenXR x64 Release flavors. OpenXR
  SHA-256:
  `CC9B34EDED6630653BD653C086735414F104457C6CE691D1A05E45FD6ECB72A9`.
  Package SHA-256:
  `D67FCFDFE38D50CF30C2440446116459D34D9CC519DB8ABA081C4716759228BF`.

### 0.24.0-roomscale-safety

- Added collision-aware room-scale head translation at the confirmed shipped
  `CheckLineOfSight` script wrapper. The active profile ray-tests the calibrated
  camera origin to the physical HMD offset against static world geometry, then
  uses a bounded binary search and configurable clearance to stop the head at
  the last safe point.
- The clamped physical-head component is shared by both eyes, HMD/controller
  world poses, interaction, hands, and flashlight placement. IPD, configured eye
  height, authored camera motion, controller-relative offsets, and native player
  capsule ownership remain unchanged.
- The control is signature-guarded and fail-closed at install. Invalid world
  state, malformed poses, unavailable native queries, and a rejected baseline
  retain the prior unmodified translation. Generated configs default off; the
  active test profile enables static-only safety with `0.12 m` clearance and six
  search iterations.
- Added deterministic clearance-factor and tracked-offset decomposition tests,
  explicit cache invalidation across activation/recenter/camera changes, bounded
  collision telemetry, and a static-wall smoke-test gate. Built and tested both
  default and OpenXR x64 Release flavors. OpenXR SHA-256:
  `1DDDCD231723EA857D377E3AE9478AA28188F72EC3D59B2B8527974813E6EA58`.
  Package SHA-256:
  `A0AB03ACF9CE0A92CF1A57906B318B8B47C2F0D6D22257BD036B9D5BD772A8A4`.

### 0.23.0-controller-flashlight

- Promoted the shipped flashlight transform to dominant-controller aim. Exact
  entity name `Flashlight` is recognized at the already signature-guarded
  `iLuxEntity.SetMatrix` boundary, so no additional executable detour or broad
  light-class mutation was introduced.
- Added `HPLFlashlightMath` to map tracked OpenXR forward/up into HPL's local
  negative-Z spotlight convention, with independent controller-local position
  and model-space rotation calibration. The active profile enables the feature;
  generated configs default off.
- Preserved SOMA's original light object, color/fade, visibility, radius, FOV,
  near plane, environment particles, frustum collision, light sensors, and
  script callbacks. Invalid player/tracking/pose-age/authored-camera/math states
  forward the original camera-mounted matrix.
- Added bounded exact-identity/pose/fallback telemetry and deterministic basis,
  offset, and malformed-pose tests. Built and tested default and OpenXR x64
  Release flavors. OpenXR SHA-256:
  `B07142C03DFCE1F0888DB38B6E4661DED334B82847206D5BBA783712A679FA3C`.
- Added a deterministic OpenXR release packager that validates build flavor,
  stages runtime/config/core-doc artifacts, writes per-file SHA-256s, and creates
  a versioned ZIP. Added a 12-scenario smoke-test matrix spanning startup,
  stereo, flashlight, interaction, manipulation, physics, UI, authored cameras,
  transitions, tracking loss, death/wake, and shutdown. Package SHA-256:
  `6FF9DA98FF9C29593B5FDBADB8A8AB7BB70768D1E2D82A3864507EB85D388D32`.

### 0.22.0-physical-manipulation

- Added dominant-grip physical manipulation for shipped player states Wheel `3`,
  Slide `4`, SwingDoor `5`, Lever `6`, and Tear `7`. Controller displacement
  relative to the HMD is projected onto current head-right/head-up and emitted as
  bounded relative mouse motion, preserving SOMA's native `mvMoveAdd`, joint,
  PID, physics, callback, and map-script ownership.
- Added state/tracking reacquisition anchors, subpixel accumulation, per-frame
  caps, sign and sensitivity controls, generated-config-off rollback, and bounded
  state/event/pixel telemetry. Room-scale body translation cancels before
  projection; Grab/Push retain their dedicated pose/throw paths.
- Added a passive ImGui identity probe to `HPLHudBridge`. Signature-guarded
  wrappers `GetCurrentImGui` `0x1400cca70`, `GetGameHudImGui` `0x1400cca90`, and
  `cImGui::GetSet` `0x140071f20` correlate rendered `cGuiSet` calls without
  changing presentation, creating the next log evidence for inventory, hints,
  menus, loading, death, wake, credits, and subtitle ownership.
- Added deterministic manipulation projection/deadzone/cap tests, documented and
  tagged the two newly recovered wrappers in Ghidra, and built/tested default and
  OpenXR x64 Release flavors. OpenXR SHA-256:
  `179AD16A2D2CF023C73CB2C8478152A36BD11B0863521F5B02BAB997AFEE1DB0`.

### 0.21.0-semantic-reticle

- Added `HPLCrosshairBridge`, a signature-guarded observer on registered global
  script dispatch `0x140484ea0`. It recognizes only
  `LuxPlayer::_Global_SetCrosshairState`, reads argument zero through confirmed
  `cScript_GetGlobalArgInt` wrapper `0x1404851d0`, then leaves SOMA's callback
  and interaction policy authoritative.
- Promoted the controller depth reticle from raw pick feedback to SOMA's exact
  35-state `eCrossHairState` vocabulary. The application-space layer now follows
  the native icon decision made after the shipped interaction/range checks and
  reports bounded semantic acceptance/rejection telemetry.
- Loaded and aspect-fitted all 34 crosshair assets named by shipped `Player.hps`
  directly from `graphics/hud`. Native artwork is intent-tinted and uploaded to
  the acquired OpenXR swapchain image; missing or malformed uncompressed TGA
  assets fall back to the prior procedural cross. `InteractionReticleNativeIcons`
  and `InteractionReticleSemantic` independently permit rollback.
- Added state-aware focus feedback. Pickup, manipulation, traversal, social,
  unavailable, and simple-hint states receive bounded amplitude/duration
  profiles; the ambiguous default cursor does not trigger haptics.
- Added deterministic semantic color and haptic-profile tests, validated all 36
  installed crosshair TGAs as uncompressed 24/32-bit assets, updated Ghidra with
  prototypes/comments/tags for the three script-global wrappers, and built/tested
  default plus OpenXR x64 Release flavors. OpenXR SHA-256:
  `0674E44B6C7C7151088627211F53104BC3A1AF4E3D53E22422CA50F350A8066C`.

### 0.20.0-depth-reticle

- Added an opt-in controller interaction reticle as a source-alpha OpenXR quad
  in application space. It consumes the exact dominant-hand aim pose and native
  closest-entity distance already published by `HPLInteractionBridge`, so each
  eye receives compositor-correct depth and convergence without another pick.
- Added angular-size, physical-size, distance, and frame-age bounds. Missing or
  stale native hits clear the layer; invalid tracking, missing stereo projection,
  comfort blackouts, and swapchain failures fail closed without a gaze fallback.
- Added a dedicated transparent OpenGL/OpenXR reticle swapchain with complete GL
  state restoration and bounded suspension after repeated transfer failures.
- Added optional low-amplitude focus-change haptics keyed to SOMA's native
  entity/body identity, with a configurable frame cooldown. The generic reticle
  and pulse deliberately do not claim unconfirmed crosshair icon semantics.
- Added deterministic angular-quad sizing tests and built/tested default and
  OpenXR x64 Release flavors. OpenXR SHA-256:
  `7E366518B111CBEEBFC792323CCFEB623221EC78005098271285A640556A0E71`.

### 0.19.0-comfort-focus

- Added a signature-guarded hook at the registered `cLuxPlayer::SetCameraPosAdd`
  wrapper `0x140159360`. While F10 VR tracking is active it semantically zeros
  only the shipped Bob, Shake, and optional Sway channels; crouch, climb,
  terminal, script, death, lean, crawl, and conversation remain native.
- Added independent generated-config-off controls and bounded per-channel
  telemetry. The active development profile enables Bob, Shake, and Sway for
  direct comfort acceptance.
- Promoted `cLuxClosestEntityData` from a hit boolean to a validated immutable
  controller-focus snapshot. Confirmed output offsets expose entity `+0x18`,
  body `+0x20`, and distance `+0x28`; the bridge now derives the exact world hit
  point for the future depth reticle without changing native pick policy.
- Named and documented `SetCameraPosAdd`, `SetCameraRoll`, and closest-entity
  result ownership in Ghidra. Added pure camera-add policy tests and built/tested
  default and OpenXR x64 Release flavors. OpenXR SHA-256:
  `F746AB81B0D1F69CC96AEA7D133D4EB762DA7FBB06CF44C472ED09FA4FF04105`.

### 0.18.0-interaction-polish

- Promoted the Grab torque probe into an opt-in controller rotation path. A
  shortest-arc grip quaternion delta augments only the exact `40/0/0.4|0.1`
  torque-PID error while SOMA retains angular velocity feedback, inertia
  transformation, the native speed/torque caps, collision, and callbacks.
- Added a signature-guarded patch at AngelScript `iPhysicsBody::AddImpulse`
  wrapper `0x14049c720`. A 350 ms one-shot intent armed by the existing native
  Grab throw action redirects the authored impulse along controller velocity,
  falling back to grip-forward aim; optional bounded velocity scaling preserves
  SOMA's mass-adjusted impulse as the baseline.
- Added opt-in center-crosshair removal to the compositor HUD capture. Only a
  configurable center rectangle is cleared to transparent; the GL transaction
  now restores framebuffer bindings, buffers, viewport, scissor box/enable,
  clear color, and color mask exactly.
- Added pure grab-rotation math and quaternion equivalence/cap tests. Built and
  tested default and OpenXR x64 Release flavors. OpenXR SHA-256:
  `EA470277AD71B0B29E11345E803C813AA51EE2E3A4AAB40B1E68E3310788B544`.

### 0.17.0-physics-input

- Added configurable head-relative locomotion using calibrated HMD yaw only;
  pitch and roll cannot tilt the movement plane. Body-relative movement remains
  the generated-config default and native/semantic routing is unchanged.
- Added physical crouch with standing-height calibration, recenter generation,
  and separate enter/exit thresholds. It owns SOMA's native crouch toggle only
  in Normal/Normal state and falls back to the existing button route when pose
  data is unusable.
- Added `HPLGrabBridge` at confirmed vector PID output `0x140238750`. In Grab
  state only, the dominant grip's camera-relative translation augments the
  exact `400/0/40` native position error while SOMA retains PID gains, mass,
  force caps, gravity, collision, joints, and callbacks. Pickup, invalid pose,
  stale input, authored camera, and all other PID calls pass through unchanged.
- Added native manipulation mappings: support squeeze plus turn-stick movement
  holds SOMA's existing InteractRotate action, while dominant primary requests
  SOMA's native Grab/Push throw/cancel action.
- Captured OpenXR grip linear and angular velocity at predicted display time.
  Release telemetry and exact `40/0/0.4|0.1` torque-PID probes now provide the
  next dataset for controller rotation and calibrated throw impulse; neither is
  substituted in this build.
- Added deterministic tests for head-relative yaw, pitch rejection, crouch
  calibration, hysteresis, and exit. Built and tested default and OpenXR x64
  Release flavors. OpenXR SHA-256:
  `4226008C6B5E844077A1C039628E5F6AD93BD9ED0AD2C3D0801D14346914276A`.

### 0.16.0-controller-hands

- Promoted the exact `PlayerHands_*` SetMatrix probe into a guarded controller
  root path. `HPLHandsMath` reconstructs SOMA's default
  `camera * rotateY(pi) * scale` basis from the dominant tracked grip while
  preserving native quarter scale, skeletal animation, sockets, and tools.
- Added configurable controller-local root position and model-space XYZ
  rotation calibration. The active test profile uses SOMA's native
  `-0.3 * 0.25 = -0.075` vertical root offset.
- Root substitution is restricted to exact identity, uniform quarter scale,
  Normal/Normal player ownership, active VR tracking, and a fresh fully tracked
  grip. Full-scale/custom/authored states and every failure forward the original
  matrix unchanged with bounded reason counters.
- Corrected hand telemetry to interpret HPL transform basis vectors as matrix
  columns, matching `cMath::MatrixUnitVectors` and the shipped hand script.
- Closed the paused-input fallback hole: a confirmed `cLux_GetGamePaused`
  result now suppresses both direct native locomotion and synthetic gameplay
  keys/buttons instead of allowing the semantic fallback behind menus.
- Added `HPLMenuBridge` and tested head-relative pointer projection. While
  paused, dominant aim moves SOMA's native client cursor and trigger/select
  clicks; held clicks are latched until release after returning to gameplay.
- Built and tested default and OpenXR x64 Release flavors. OpenXR SHA-256:
  `9FD03BD662E31A3CF8BF96297573933558DF41606231285CBF5F2EF52C9CCE96`.

### 0.15.0-hud-layer

- Extracted permanent GUI ownership from `HPLCompatibilityProbe` into a
  signature-guarded `HPLHudBridge`; the existing exact `GameHudSet` identity,
  per-set draw telemetry, and virtual HUD metrics remain available.
- The exact 2D gameplay HUD now renders into a transparent `1600x900` OpenGL
  target instead of both AFR eye images when the OpenXR session is visible.
  Menus, ImGui, subtitles outside this set, and 3D/diegetic GUI sets remain native.
- Added a dedicated OpenXR HUD swapchain and alpha-blended
  `XrCompositionLayerQuad` in VIEW reference space. Distance, physical width,
  vertical offset, pixel dimensions, and accepted capture age are configurable.
- Added fail-closed behavior: missing signatures/resources/session visibility
  keep the native HUD path; four consecutive copy failures suspend extraction
  so the next frame returns to the backbuffer path.
- Added tested `HPLHudMath` quad placement/aspect validation and expanded OpenXR
  frame/summary telemetry with HUD capture, submission, and fallback counters.
- Built and tested default and OpenXR x64 Release flavors. OpenXR SHA-256:
  `B7639F5EF7DDE23098B7913CAF10523F3D648179BA8F4B61F40EF3494CB2C299`.

### 0.14.0-native-locomotion

- Added `HPLNativeLocomotion`, signature-guarding the registered
  `iCharacterBody::Move`, `iCharacterBody::AddYaw`, and `cLux_GetGamePaused`
  wrappers before any direct native input is possible.
- Normal, unpaused player/move state now receives radial-deadzone analog
  forward/right magnitudes. Paused play, menus, ladders, grabs, terminals,
  authored cameras, non-normal move states, invalid bodies, and signature
  mismatches automatically retain the existing semantic W/A/S/D route.
- Snap turn now uses an exact configurable degree increment and smooth turn uses
  configurable degrees per second through the native body-yaw accumulator.
  The existing pixel/mouse path remains the automatic special-state fallback.
- Added bounded route and summary telemetry distinguishing `native_analog` /
  `native_radians` from `semantic_keys` / `semantic_mouse`.
- Added role-aware support-hand face actions: default left X toggles SOMA's
  flashlight through `F`, and left Y opens inventory through `Tab`. Left-dominant
  mode moves these actions to the right support hand; one-hand recenter remains unchanged.
- Built and tested default and OpenXR x64 Release flavors. OpenXR SHA-256:
  `7FCD71F56214E30028964CD3721F614C46BF429A56DC6CE4B852CDE65B27527A`.

### 0.13.0-hands-identity

- Added signature-guarded `HPLHandsBridge` ownership around the shared Lux
  entity SetMatrix wrapper and confirmed inherited GetName accessor.
- Added exact `PlayerHands_*` identification through the native bounded MSVC
  string layout at entity `+0x120`; unrelated entities receive no pose analysis.
- Added passive hand-root telemetry for HPL matrix translation, basis/scale,
  quarter/full-scale mode, native-camera distance, dominant grip pose/distance,
  and authored-camera/player-state ownership. Original matrices remain unchanged.
- Promoted the GetName accessor and `cLuxProp` registration owner in Ghidra and
  documented the exact measurements needed for a safe controller root override.
- Built and tested default and OpenXR x64 Release flavors. OpenXR SHA-256:
  `E180A16725978AB161A14C66901A2386D1554BF4E79FF6D681CF2426D49F8D05`.

### 0.12.0-native-interaction

- Added a signature-guarded `HPLInteractionBridge` at confirmed native wrapper
  `0x1400cd750`. It routes the dominant controller's tracked world-space aim into
  SOMA's closest-entity query while preserving native length, type, LOS,
  `CanInteract`, range, focus, player-state, and callback ownership.
- Added strict passthrough gates for query type, native camera-origin proximity,
  full controller tracking, active input, and authored-camera ownership, plus
  bounded substitution/hit/fallback telemetry.
- Added confirmed gameplay-HUD virtual center, virtual-size/start, and
  center-screen metric telemetry for resolution-independent HUD capture design.
- Confirmed and promoted the shared Lux entity `SetMatrix` wrapper used by the
  scripted `PlayerHands_*` path. Runtime identity remains the deliberate gate
  before controller-owned hand/viewmodel transforms.
- Built and tested default and OpenXR x64 Release flavors. OpenXR SHA-256:
  `488029E97B313289BBBBD74BE32419635998EC2EF8B9842C86F720955CCACB85`.

### 0.11.0-spatial-ownership

- Added one shared HPL world-pose bridge for the HMD and dominant controller.
  Head, aim, and grip positions use the exact origin and base-view basis already
  proven by the rigid stereo path.
- Added configurable positional audio correction. SOMAVR adds the tracked head
  world offset to the native listener only for the FMOD commit, then restores
  SOMA's fields immediately; orientation correction remains unchanged.
- Added bounded dominant-hand aim/grip telemetry with world position, forward,
  validity, and tracking flags. This is the implementation prerequisite for
  native interaction-ray and viewmodel ownership, without bypassing SOMA's
  `CanInteract`, range, focus, or physics policies.
- Confirmed and promoted seven HUD/ImGui getter wrappers in Ghidra. The GUI-set
  hook now identifies the exact gameplay HUD set through a signature-guarded
  game-context lookup and reports `gameHud=1` plus aggregate matches.
- Built and tested default and OpenXR x64 Release flavors. OpenXR SHA-256:
  `DD8C177B209B4859FBFE69DD7ACA2C4A0F8A31F101D5DBB679B475295CF947B3`.

### 0.10.0-tracking-accessibility

- Preserved OpenXR eye views in a last-known-good cache. `xrLocateViews` now
  writes into temporary storage and only publishes a sample after both stereo
  views and orientation/position validity bits pass validation.
- Added bounded tracking-loss behavior. Cached head/eye poses remain usable for
  `TrackingHoldFrames`, report untracked during the grace period, expire closed,
  and resume through a configurable recovery blackout without clearing stereo
  intent in the native HPL camera bridge.
- Added tracking degradation, loss, restoration, pose-age, fallback-frame, and
  recovery counters to bounded runtime and shutdown telemetry.
- Added configurable dominant-hand actions and left/right stick swap. Touch and
  Index primary/secondary face actions are now bound on both controllers.
- Added a one-controller fallback: the available controller owns movement,
  interaction, jump/crouch, haptics, and a held primary+secondary recenter chord.
  Turn and sprint are deliberately suppressed in this constrained mode.
- Built and tested default and OpenXR x64 Release flavors. OpenXR SHA-256:
  `153E59EDC6F48CA6DB7B10B93EDBC90EED2AA2970AADF878B73237FF7AF39936`.

### 0.9.0-calibration-haptics

- Added configurable OpenXR application spaces. `ReferenceSpace=local`
  preserves the proven seated/recentered path; `stage` selects the floor-aware
  standing space when advertised and falls back to `LOCAL` when unavailable.
- Added an OpenXR vibration-output action and per-hand haptic bindings for
  Simple, Touch, Index, and Microsoft Motion profiles. Interaction, snap turn,
  menu, jump, crouch, and successful recenter now provide discrete feedback.
- Hardened focus transitions. `XR_SESSION_NOT_FOCUSED` clears the complete input
  snapshot immediately and logs bounded loss/restoration transitions, ensuring
  synthetic held inputs release deterministically.
- Confirmed camera base roll at `cCamera+0x4c`, extended/authored roll at `+0x68`,
  and their dirty flags in Ghidra. The bridge now reports both and includes an
  opt-in `HPLNativeCameraRollSuppression=1` comfort policy; it remains off in the
  active config until tested in authored camera sequences.
- Built and tested default and OpenXR x64 Release flavors. OpenXR SHA-256:
  `B26C6911E859099A619ED955E49302B315A2A6D3BBCCF4B052842079DBDCA8E0`.

### 0.8.0-resilience-comfort

- Added automatic OpenXR recovery for session `EXITING`/`LOSS_PENDING` and
  instance-loss events. SOMAVR now destroys stale session/instance resources,
  waits a configurable frame delay, and reboots the runtime while preserving
  the user's stereo intent.
- Added explicit AFR cache invalidation and automatic stable-pose re-arming when
  SOMA replaces the active player or camera during save/load or map transition.
- Added bounded OpenXR comfort-black frames for snap turns and completed
  recenters. Frame pacing continues normally; only projection-layer submission
  is omitted for the configured number of frames.
- Promoted seven post-effect vtable identities from Ghidra. Active ImageTrail,
  ChromaticAberration, and RadialBlur effects are temporarily disabled only
  during active stereo VR compositor calls and restored immediately afterward.
  ToneMapping, FXAA, ImageFadeFX, and VideoDistortion remain enabled.
- Added priority lookup from the composite tree and named inventory/isolation
  logs. Ctrl+F12 diagnostics continue to override the normal comfort policy.
- Added a signature-guarded `HPL3_GuiSet_Render` hook with per-set 2D/3D flags,
  virtual dimensions, offsets, depth range, priority, framebuffer/program state,
  and draw-call deltas for HUD/diegetic classification.
- Built and tested default and OpenXR x64 Release flavors. OpenXR output remains
  `build-openxr-controller\Release`.

### 0.7.2-render-state-policy

- Extracted signature-guarded native player inspection from `HPLInputBridge`
  into reusable `HPLPlayerState` ownership for future locomotion, interaction,
  hands, comfort, and authored-camera adapters.
- Confirmed `cCamera::GetRotateMode` reads camera `+0x6c` and
  `iCharacterBody::Get/SetCameraUpdateActive` reads/writes body `+0x1e8`.
  Matching HPL2 source establishes Euler mode `0`; SOMA's shipped
  `PlayerHandsHandler` switches to matrix mode and disables body camera updates
  while attaching the camera to a hand bone.
- Added transition telemetry for camera mode/body ownership and a default-on
  controller policy that releases movement, turning, run, jump, crouch, and
  interaction during authored camera ownership. Menu and recenter remain live.
- Expanded all six HPL render-stage samples with draw, viewport, framebuffer,
  program, and clear deltas plus complete blend/depth/scissor/write-mask state.
  F6 `draws.csv` rows now identify their enclosing HPL render stage.
- Added a bounded active post-effect inventory using the confirmed composite
  vector at `+0x340/+0x348`, including object pointer, vtable RVA, active flags,
  input texture, render target, and transition detection.
- Added reversible selective post-effect diagnosis: `Ctrl+F12` cycles through
  currently active effects and renders only the selected effect for that call;
  `Shift+F12` restores the normal chain. Plain F12 retains the all-effect bypass.
- Promoted eight authored-camera, HUD, and post-effect helpers in Ghidra with
  names, prototypes where known, evidence comments, and subsystem tags.
- Built and tested OpenXR/default x64 Release flavors. OpenXR output:
  `build-openxr-controller\Release`.

## 2026-07-14

### 0.7.1-gameplay-actions

- Confirmed SOMA's shipped action defaults in `script/base/InputHandler.hps`:
  Space jump, Left Control toggle crouch, Left Shift hold run, left mouse
  interact, and right mouse interaction cancel.
- Added dedicated OpenXR jump and crouch actions. Oculus Touch and Valve Index
  bind right A to jump and right B to crouch; profiles without confirmed face
  buttons remain deliberately unbound.
- Moved Touch/Index `select` to trigger click, matching OpenXR semantics and
  avoiding the previous face-button/interact overlap.
- Added left-trigger hold-run, A/Space jump, and B/Left-Control crouch to the
  reversible SOMA input-path prototype. All held run state participates in the
  existing stale-input, F10-disable, and teardown release policy.
- Built and tested OpenXR/default x64 Release flavors. OpenXR output:
  `build-openxr-controller\Release`.

### 0.7.0-controller-prototype

- Added `HPLInputBridge` as a separate gameplay-input and native-player probe
  owner; camera and OpenXR runtime modules remain focused on their existing jobs.
- Added left-stick W/A/S/D locomotion with press/release hysteresis, configurable
  snap or smooth right-stick turn, right-trigger/select interaction, menu/Escape,
  and a two-grip hold that requests the proven recenter latch.
- Added fail-quiet input lifetime: all held keys/buttons release when VR mode is
  disabled, input is inactive/stale, or the bridge is removed.
- Resolved and signature-guarded `GetPlayer`, player state ID, and move-state ID
  getters. Bounded telemetry now correlates player, camera, body, active camera,
  authored state, move state, and OpenXR controls.
- Updated the shared Ghidra database with seven player/input names and evidence
  comments. Updated address, RE, traceability, state, test, and build docs.
- Built OpenXR x64 Release and passed the render-math suite. Output:
  `build-openxr-controller\Release`.

### 0.6.0-input-foundation

- Added `OpenXRInput` as a dedicated action/pose owner rather than expanding
  `OpenXRRuntime::Impl` with gameplay semantics.
- Added a runtime-neutral `somavr_gameplay` action set with move, turn, select,
  analog trigger, squeeze, menu, grip-pose, and aim-pose actions. Initial
  suggested bindings cover Khronos Simple, Oculus Touch, Valve Index, and
  Microsoft Motion Controller profiles.
- Attached the action set before session begin, created left/right grip and aim
  spaces, synchronized actions at predicted display time, and exposed a stable
  `OpenXRInputSnapshot` for future locomotion, hands, HUD pointer, and interaction
  bridges. This build observes input only and cannot move or interact with SOMA.
- Added bounded `openxr_input` state telemetry for sticks, buttons, squeeze, and
  pose validity, enabled in the active development config with
  `[OpenXR] InputEnabled=1`.
- Added pose-age telemetry to head and stereo snapshots, `openxr_view` summaries,
  and `hpl_stereo` rows to prepare tracking-loss and stale-pose policy work.
- Added configurable vertical room-scale policy and calibrated eye-height offset.
  Defaults preserve the proven camera path: `HPLRoomscaleVertical=1` and
  `HPLEyeHeightOffsetMeters=0.0`.
- Added an automatic `somavr_build_manifest.txt` beside each DLL with version,
  flavor, OpenXR bit, artifact name, and SHA-256.
- Built and tested default/OpenXR x64 Release flavors. OpenXR output:
  `build-openxr-input-foundation\Release`.

### 0.5.11-recenter

- Accepted the user-confirmed `0.5.10-poselatch` run as the current graphics
  baseline: one F10 enabled tracking, AFR stereo, full projection centering, and
  clean shutdown with no observed shadow/reflection regressions.
- Added an in-session F2 recenter control gated by `[Hooks] HPLRecenterControl`.
  Recenter does not restart OpenXR, disable stereo, or leave VR mode.
- Reused the same fully-tracked, eight-stable-frame neutral-pose latch as F10.
  While F2 is waiting for stable samples, SOMAVR continues rendering with the
  previous neutral pose rather than dropping back to the unmodified camera.
- F2 atomically replaces `neutralOrientation` and `neutralPosition`, resets AFR
  eye alternation to the left eye, and logs `hpl_recenter requested`,
  `calibration_wait`, `calibration_reset`, and `applied` rows.
- Built default and OpenXR x64 Release flavors. OpenXR output:
  `build-openxr-recenter\Release`.

### 0.5.10-poselatch

- Triaged the first `0.5.9-rotationfix` live run. The world remained rigid, but
  F10 calibrated against OpenXR frame `2857` at head `Y=-1.244683`; the next
  frame settled near `Y=+0.543`, producing an artificial upward offset of about
  `1.79` metres and placing the camera through the roof.
- Confirmed this is a one-key startup calibration regression rather than an HPL
  world-scale or projection fault. The older manual F8/F10 sequence naturally
  allowed the OpenXR reference space to settle before neutral capture.
- Exposed OpenXR orientation/position tracked flags to the camera bridge. F10
  calibration now requires both tracked bits rather than validity alone.
- Added a deterministic neutral-pose latch: eight consecutive unique poses must
  remain within `0.25 m` and `45 degrees` per frame. A startup reference-space
  discontinuity resets the latch instead of becoming permanent room-scale motion.
- Added bounded `calibration_wait` and `calibration_reset` telemetry plus unit
  tests for duplicate frames, large origin jumps, stable-frame accumulation, and
  equivalent quaternion signs.
- Kept the `0.5.9` rigid-rotation fix, full projection centering, AFR stereo,
  room-scale policy, audio correction, and clean shutdown path unchanged.
- Built default and OpenXR x64 Release flavors. OpenXR output:
  `build-openxr-poselatch\Release`.

## 2026-07-13

### 0.5.9-rotationfix

- Diagnosed the user-reported yaw/pitch skew in `0.5.8-onekey` as a camera-math
  regression introduced during the deterministic math extraction.
- Fixed the quaternion-to-matrix XY cross-term, which incorrectly used `2*y*y`
  where `2*x*y` was required. The resulting non-orthogonal matrix sheared the
  HPL view as the headset rotated.
- Added regression tests that require the generated rotation basis to remain
  unit length and mutually orthogonal, and require matrix rotation to agree with
  the independent quaternion-vector implementation.
- Kept F10 one-key activation, full projection centering, room-scale translation,
  AFR stereo, compatibility controls, and the OpenXR submission policy unchanged.
- Built default and OpenXR x64 Release flavors. OpenXR output:
  `build-openxr-rotationfix\Release`.

### 0.5.8-onekey

- Promoted F10 from a camera-only toggle to the normal VR-mode control.
- F10 now requests OpenXR manual startup through `OpenXRRuntime`, waits
  asynchronously for valid head and stereo views, calibrates the neutral pose,
  enables native head tracking and AFR stereo, and forces the proven fully
  centered projection policy.
- A second F10 press cancels a pending activation or exits tracking/stereo and
  restores SOMA's base camera. F8 and F11 remain available as low-level runtime
  and stereo diagnostics but are no longer required for normal activation.
- Added bounded `hpl_vr_mode requested/activated/cancelled/disabled` telemetry
  and API-attributed OpenXR manual-start logging.
- Built default and OpenXR x64 Release flavors. OpenXR output:
  `build-openxr-onekey\Release`.
- Established the first maintainability baseline without changing runtime
  behavior or the build version: extracted `HPLCameraMath`,
  `OpenGLMatrixAnalysis`, and `OpenXRHelpers`; added deterministic render-math
  tests and `ARCHITECTURE.md` ownership rules. Both Release flavors and tests pass.

### 0.5.7-fullcenter

- Triaged `0.5.6`: F3 patched program `985` for `369` draws with no visual
  effect, rejecting view-depth reflection fade as the window/oven artifact owner.
- F4 showed little translation dependence; the user isolated dynamic-shadow
  motion primarily to HMD pitch and roll.
- Confirmed the prior centered policy only removed horizontal asymmetry. The
  vertical projection center remained `-0.193187` in every stereo row.
- Extended centered projection to both axes while preserving each eye's original
  horizontal and vertical tangent span. OpenXR submission uses the same modified
  FOV as rendering.
- Accepted clean shutdown: the corrected lifecycle hook installed, logged
  pre-graphics OpenXR shutdown begin/complete, and SOMA exited normally.
- Built default and OpenXR x64 Release flavors. OpenXR output:
  `build-openxr-fullcenter\Release`.

### 0.5.6-stability

- Accepted F5 as a successful stereo-compatibility result. Shadow UBO offset
  `96` changes from opposite `-0.242513/+0.242513` eye projection centers to
  `0/0`, exactly matching the user-confirmed convergence.
- Made centered horizontal projection the active development default while
  retaining F5 as a reversible comparison.
- Added F4 room-scale isolation. Disabled mode removes tracked head-center
  translation but preserves eye separation and head orientation, testing whether
  camera-relative shadow/light state owns the remaining movement.
- Identified program `988` as a depth-driven translucent/refraction path with
  block-backed reflection size and fade parameters. Added F3 to bypass the
  view-depth reflection fade around affected draws and immediately restore the
  authored UBO values.
- Corrected the pre-graphics lifecycle signature to include the leading `0x40`
  byte confirmed in the installed executable and Ghidra. `0.5.5` correctly
  failed closed rather than installing against a mismatched guard.
- Updated Ghidra's lifecycle comment/bookmark and built default/OpenXR x64
  Release flavors. OpenXR output: `build-openxr-stability\Release`.

### 0.5.5-reconstruct

- Analyzed three successful F6 captures covering both AFR eyes. Direct temporal
  and inverse camera matrices alternate correctly, while the live deferred shadow
  programs `942/944` obtain their reconstruction camera packet from uniform
  blocks rather than direct uniforms.
- Confirmed the live world reflection/water program `989` uses the same
  uniform-block route for inverse projection/view and screen-space reflection
  parameters. This matches the observed shadow displacement and gives both
  defects a common reconstruction hypothesis.
- Extended F6 to snapshot bound uniform-buffer ranges per program and eye,
  including block/member metadata and raw 32-bit values.
- Added an F5 projection-center A/B. It preserves eye translation and vertical
  FOV but temporarily makes each eye's horizontal projection symmetric; the
  submitted OpenXR FOV is kept identical to the rendered FOV.
- Analyzed `Soma_NoSteam-10384.dmp`: only SOMA's main thread survived, stopped in
  OpenGL with Virtual Desktop runtime frames. No SOMAVR worker thread remained.
- Mapped and named `HPL3_cSDLEngineSetup_Destructor` at `0x1403b16e0` in Ghidra.
  A signature-guarded lifecycle hook now shuts OpenXR down before HPL deletes
  Graphics and calls `SDL_Quit` at `0x1403b1803`.
- Built x64 default and OpenXR Release flavors successfully. The OpenXR test
  output is `build-openxr-reconstruct\Release`.

## 2026-07-12

### 0.5.4-renderdiag

- Triaged `0.5.3-shadowjitter`: F7 toggled at frames `2072`, `3098`, and `3197`, but every row reported `uploads=0 overrides=0`. The correct conclusion is that the live shader never reached the targeted uniform, not that a zero radius failed visually.
- Added reflection RE. SOMA supports eye-vector cubemap reflections and screen-coordinate world reflections; HPL2 renders the latter from a mirrored current frustum into a reused reflection buffer.
- Added F6 four-frame render capture spanning both AFR eyes twice. It writes per-eye draw order/FBO/program data, all matrix uploads, active uniform inventories, and attached generated GLSL sources under `logs\render-captures`.
- Added shader-source classification for shadow, reflection, environment, temporal, and water paths.
- Added current AFR eye and render-pose frame to the camera bridge status so diagnostic rows have explicit eye ownership.
- Diagnosed the lingering process as an orphaned SOMAVR worker: the observed process had no window and exactly one thread. The worker now returns when it is the process's final thread, avoiding the prior stop-event/DLL-detach cycle.
- Added `somavr_dumper.exe`, which creates a thread-aware minidump by PID or executable name and supports optional `--full` memory capture.
- Built the x64 default Release flavor successfully. The OpenXR test output is `build-openxr-renderdiag\Release`.

### 0.5.3-shadowjitter

- Triaged the successful `0.5.2-audiopost` run. It reached game frame `10920`, `6350` stereo submissions, and `6571` total submissions without OpenXR failure, stereo suspension, signature mismatch, or hook failure.
- Accepted the user-confirmed audio result as provisionally correct; a stronger directional-source test remains.
- Rejected the all-post-effect chain as the owner of the principal stereo defect. F12 mainly changed contrast while realtime shadows remained different between eyes and moved with player motion.
- Mapped SOMA's deferred soft-shadow path. `deferred_light_frag.hpsl` selects jitter samples from screen pixel coordinates and scales them by `avShadowMapOffsetMul`; HPL2 confirms this value is uploaded through `glUniform2f`.
- Added targeted shadow/split uniform discovery that remains active after the general uniform-name log budget is exhausted.
- Added signature-independent OpenGL interception for `glUniform2f` and `glUniform2fv`. F7 toggles only `avShadowMapOffsetMul` between authored and zero values, preserving shadow maps, light matrices, and camera state.
- Added bounded `shadow_jitter_upload`, toggle, and shutdown counters. Suppression defaults off and is controlled by `[Hooks] HPLShadowJitterControl` and `HPLShadowJitterSuppressedDefault`.
- Built x64 Release default and OpenXR flavors successfully. The OpenXR output is `build-openxr-shadowjitter\Release`.

### 0.5.2-audiopost

- Triaged the successful `0.5.1-compatprobe` run. F8 started OpenXR at game frame `2296`, F10 enabled native HMD tracking at `2438`, and F11 produced user-confirmed full stereo at `2890`.
- The focused session reached at least `2383` total OpenXR submissions and `1765` stereo submissions with no OpenXR failure, eye-cache failure, hook mismatch, or stereo suspension.
- Confirmed the gameplay render split: world resolves FBO `0 -> 11`, active post effects resolve `11 -> 0`, and final screen GUI remains `0 -> 0`. Post effects averaged about `125 us` in sampled F11 gameplay frames.
- Confirmed hypothesis S11: listener forward/up remained fixed across large HMD quaternion changes and changed only with SOMA's authored camera/state.
- Added orientation-only FMOD correction while F10 is active. SOMA's authored forward/up are rotated by the current physical HMD delta, used only during the native listener commit, then immediately restored. Position and velocity remain authored.
- Added an audio quaternion self-test. Failure disables only listener correction while retaining camera, stereo, and telemetry.
- Added a signature-guarded `HPL3_PostEffectComposite_HasActiveEffects` hook. F12 reversibly bypasses all active post effects for shader-defect A/B testing; default is passthrough and HUD/screen GUI remain active.
- Added `[Hooks] HPLAudioListenerCorrection`, `HPLPostEffectControl`, and `HPLPostEffectBypassDefault`.
- Built x64 Release default and OpenXR flavors successfully. The OpenXR output is `build-openxr-audiopost\Release`.

## 2026-07-11

### 0.5.1-compatprobe

- Integrated the compatibility RE pass as passive runtime telemetry; F8/F10/F11 rendering behavior remains unchanged from `0.5.0-afrstereo`.
- Added `HPLCompatibilityProbe` with independent exact-signature guards for render viewport `0x140298630`, world render `0x1401f9790`, world/3D-GUI callbacks `0x140297670`, post effects `0x14033bd80`, `PostPostEffect` callbacks `0x1401f1480`, final screen GUI `0x1402981e0`, and FMOD listener update `0x140289340`.
- Render-stage samples record a pending render-frame number, nested sequence, viewport and render mask, before/after draw/read framebuffer, shader program, GL viewport, and CPU duration. Initial calls and up to eight calls per 120-frame sample are logged.
- Audio samples record listener position, velocity, forward/up vectors, center-head OpenXR pose, and current F10/F11 bridge state. The hook observes the engine listener commit but does not modify FMOD arguments.
- Added bounded shutdown totals for every stage, audio updates, samples, and successfully installed hooks.
- Added `[Hooks] HPLRenderStageProbe`, `HPLAudioListenerProbe`, and `HPLCompatibilityLogInterval`; both probes are enabled in the active development config.
- Built x64 Release default and OpenXR flavors successfully. The OpenXR test output is `build-openxr-compatprobe\Release`.

## 2026-07-10

### 0.5.0-afrstereo

- Triaged the successful live `0.4.0-hplcamera` run. F10 selected the confirmed render-viewport camera and applied native HMD orientation for `1210` consecutive renders before restoring the pristine view.
- Native rotation reached approximately `35.7` degrees, and F9's four spaced buckets showed changing temporal-view and inverse-view-projection matrices. This confirms HMD motion reached HPL3 camera state rather than mouse input.
- OpenXR remained healthy beyond `1800` submitted frames with no frame failure or suspension. Runtime IPD stayed near `0.06852` meters.
- Added F11-gated alternating-eye stereo while preserving F10 mono orientation as the fallback. F11 can be disabled independently and F10 also shuts stereo down before restoring the base view.
- Added per-eye OpenXR pose/FOV snapshots. Each eye view uses the calibrated head-relative position, including runtime IPD and positional head movement, mapped through configurable `HPLWorldScale`.
- Added OpenGL right-handed asymmetric projection generation from `XrFovf`. Eye axes remain parallel; convergence comes from eye translation and off-axis projection rather than camera toe-in.
- Added a projection self-test against SOMA's known 70-degree, 16:9, `0.03-1000` matrix. Failure disables only F11 stereo.
- Added persistent per-eye OpenGL cache textures/FBOs. The current backbuffer updates one cache each game frame; both cached eyes are copied into acquired OpenXR swapchain images and submitted with the exact poses/FOVs used to render them.
- Stereo waits until both eye caches have valid renders, then reports `stereo=1` and bounded capture/submission counters. Eight consecutive cache-capture failures suspend the stereo submission branch.
- Added `[Hooks] HPLStereoAFR` and `HPLWorldScale`. The active value is `1.0` SOMA units per OpenXR meter.
- Built x64 Release default and OpenXR flavors successfully. The OpenXR output is `build-openxr-afrstereo\Release`.

### 0.4.0-hplcamera

- Triaged two successful F9 captures from `0.3.1-cameramap`. Sequence 1 included mouse input; sequence 2 contained only deliberate HMD yaw/roll/pitch and is the clean control.
- In sequence 2 the OpenXR head quaternion changed from approximately `-0.00615,-0.58891,0.02084,0.80791` to `-0.07509,-0.59820,0.03994,0.79682`, while the sampled SOMA camera matrices did not follow it. This proves pose capture is live and the native game camera is still independent.
- Stable matrix-upload stacks mapped `+0x55ab0a` to the GLSL setter, `+0x435b33` to low-level `SetMatrix`, `+0x2ac5f0` to `iRenderFunctions::SetProjectionMatrix`, and `+0x2ad979` to normal frustum projection selection.
- Matched `0x140271b80` to `cCamera::GetFrustum` and `0x140270230` to `cFrustum::SetupPerspectiveProj` using Ghidra and the released HPL2 source. The latter updates view-projection, culling planes, sphere, vertices, and bounding volume, making it safer than late uniform mutation.
- Added `HPLCameraBridge`, guarded by exact prologue signatures for both target functions plus the confirmed main render-viewport return RVA `+0x298697`. A mismatch disables only this branch.
- F10 now toggles an orientation-only bridge. Enabling captures the current OpenXR orientation as neutral and locks the current perspective camera; disabling immediately restores SOMA's pristine cached view.
- The hook runs at `cCamera::GetFrustum` on every camera query, then calls native `SetupPerspectiveProj`, so headset-only movement can update the frustum even when SOMA did not mark its own camera dirty.
- Added base-matrix preservation keyed by SOMA's base/secondary frustum dirty flags to prevent HMD rotation from accumulating across frames. Orthographic frustums and non-camera perspective ranges are rejected.
- Added bounded native-camera candidate, calibration, application, pose-miss, and shutdown summary logging. F9 capture remains enabled for downstream matrix confirmation.
- Changed F9 full-matrix sampling from the first four matching uploads to four evenly spaced capture-window buckets, so the next log can correlate early, middle, and late HPL matrices with the arm/complete OpenXR poses.
- Built x64 Release default and OpenXR flavors successfully. The OpenXR test output is `build-openxr-hplcamera\Release`.

### 0.3.1-cameramap

- Triaged the successful `0.3.0-xrframe` live run. F8 triggered immediately at game frame `2783`; both `2688x2880` `GL_SRGB8_ALPHA8` eye swapchains were created with three complete FBO-backed images each; the session progressed through `READY`, `SYNCHRONIZED`, `VISIBLE`, and `FOCUSED`.
- The run completed at least `938` consecutive projection-layer submissions with changing eye poses, `openxrFrameSubmitFailed=0`, and no logged OpenXR errors or suspension. OpenXR transport and frame timing are now considered proven.
- Added an F9 camera-attribution window. It runs for 120 frames and captures only matrix uniforms whose names contain `View` or `Projection`.
- Added one module-relative stack trace per unique uniform callsite, suitable for direct RVA navigation in Ghidra. Plain absolute addresses are not used as the durable anchor.
- Added up to four full 4x4 samples per camera uniform, including program, location, transpose flag, and render-frame number.
- Added compact OpenXR pose telemetry with pose-validity flags, center-head position/orientation, pose frame, and measured IPD so matrix changes can be correlated with deliberate headset movement.
- Added `[Hooks] MatrixCapture`, `MatrixCaptureFrames`, `MatrixCaptureStackDepth`, `MatrixCaptureMaxSites`, and `MatrixCaptureSamplesPerUniform`.
- The current `0.3.0` DLL was locked by the still-running SOMA process, so the OpenXR flavor was built successfully to `build-openxr-cameramap\Release`. The default flavor was built normally to `build\Release`.

### 0.3.0-xrframe

- Triaged the successful `0.2.7-xrmanual` live run: F8 triggered at game frame `4200`, `VirtualDesktopXR 1.0.10` accepted SOMA's active `hglrc=0x30000`, the session reached `READY`, two views and seven swapchain formats were reported, and the session stayed alive through later gameplay frames.
- Fixed the F8 reliability issue discovered from that run. OpenXR had been updated only when a `frame_summary` was emitted, so a `FrameSummaryInterval=120` setting sampled F8 once every 120 frames. OpenXR now runs at every real `SwapBuffers`; telemetry remains independently throttled.
- Added `OpenXRGLBridge`, following UEVR's runtime/backend separation. It selects a runtime-supported color format, creates one OpenGL swapchain per eye at the runtime-recommended size, enumerates images, and validates an FBO for each image.
- Added session lifecycle handling: begin on `READY`, end on `STOPPING`, and stop submission on session loss.
- Added a complete frame path: `xrWaitFrame`, `xrBeginFrame`, `xrLocateViews`, swapchain acquire/wait/release, mirrored SOMA backbuffer blits, and `xrEndFrame` with a two-view projection layer.
- Added `[OpenXR] FrameSubmit`, `MirrorBackbuffer`, and `ResolutionScalePercent`. The active config enables all three at 100 percent resolution after manual F8 start.
- Added bounded frame-error logging and automatic suspension after 60 consecutive submission failures so the experimental presentation branch does not repeatedly hammer SOMA or the runtime.
- Reviewed local UEVR architecture and Praydog's reverse-engineering write-up. The applicable decisions are recorded in `docs\UEVR_LEARNINGS.md`.
- Built x64 Release default and OpenXR flavors. The OpenXR output is `build-openxr\Release\somavr.dll` with `version=0.3.0-xrframe`.

### 0.2.7-xrmanual

- Triaged live `0.2.6-xrhold`: OpenXR session creation succeeded on the main render context at frame `120`, stayed alive through the configured `HoldFrames=600` window, then released at frame `720` with `openxr_runtime released_after_probe reason=hold_complete`.
- Later summaries continued after release with `openxrSessionAlive=0` and `openxrInstanceAlive=0`, giving us a clean manual-start target for loading a save first.
- Added `[OpenXR] ManualStart=1` / `StartOnF8=1` support. When enabled, SOMAVR keeps the launch-time hooks installed but defers OpenXR bootstrap until F8 is pressed.
- Added `openxr_manual_start waiting` and `openxr_manual_start triggered` rows, plus summary fields `openxrManualStart=`, `openxrManualStartArmed=`, and `openxrManualStartFrame=`.
- Updated the active runtime config for the next pass: `Probe=1`, `SessionProbe=1`, `ReleaseAfterProbe=0`, `BootstrapFrame=120`, `HoldFrames=0`, and `ManualStart=1`.
- This build still does not call `xrBeginSession`, create swapchains, or submit frames. It is a stable "start XR after save load" gate before the first real OpenXR presentation work.
- Built x64 Release default and OpenXR probes. The flavor files now report `version=0.2.7-xrmanual`, with `openxr=0` in `build\Release` and `openxr=1` in `build-openxr\Release`.

## 2026-07-09

### 0.2.6-xrhold

- Triaged live `0.2.5-xrframeprobe`: OpenXR bootstrap deferred as intended. Frame `120` still showed `openxrAttempted=0`, then the OpenXR probe ran on the frame context `hdc=0x420117aa hglrc=0x30000`.
- `xrCreateSession` succeeded on the frame context, reached `READY`, reported the same `VIEW/LOCAL/STAGE` reference spaces and seven GL swapchain formats, and released cleanly.
- Later frame summaries continued through frame `3240` with `openxrSessionAlive=0`, `openxrInstanceAlive=0`, `openxrSessionReleasedAfterProbe=1`, and `openxrSwapchainFormats=7`.
- Windows Error Reporting still showed no newer `Soma_NoSteam.exe` crash after the frame-context one-shot.
- Added `[OpenXR] HoldFrames=600`. With `ReleaseAfterProbe=1`, a successful session probe now keeps the frame-context OpenXR session alive for the configured frame window, polls OpenXR events, then releases with `openxr_runtime released_after_probe reason=hold_complete`.
- This build intentionally does not call `xrBeginSession`, create swapchains, or submit frames. It only tests whether VirtualDesktop tolerates a live OpenXR session inside SOMA for a short controlled window.
- Built x64 Release default and OpenXR probes. The flavor files now report `version=0.2.6-xrhold`, with `openxr=0` in `build\Release` and `openxr=1` in `build-openxr\Release`.

### 0.2.5-xrframeprobe

- Triaged live `0.2.4-xrsessiononeshot`: `xrCreateSession` succeeded, the runtime reported reference spaces `VIEW`, `LOCAL`, and `STAGE`, and `xrEnumerateSwapchainFormats` returned seven GL formats: `GL_RGBA16F`, `GL_SRGB8_ALPHA8`, `GL_RGBA8`, `GL_DEPTH_COMPONENT32F`, `GL_DEPTH32F_STENCIL8`, `GL_DEPTH24_STENCIL8`, and `GL_DEPTH_COMPONENT16`.
- The session reached `READY`, then SOMAVR released both session and instance via `openxr_runtime released_after_probe reason=session_probe_complete releasedSession=1 releasedInstance=1`.
- Later frame summaries continued through frame `2040` with `openxrSessionCreated=1`, `openxrSessionAlive=0`, `openxrSessionReleasedAfterProbe=1`, `openxrInstanceAlive=0`, and `openxrSwapchainFormats=7`.
- Windows Error Reporting still showed no newer `Soma_NoSteam.exe` crash after the one-shot session probe.
- Important nuance: `0.2.4` created the OpenXR session on SOMA's early startup context (`hglrc=0x10000`), while later render frame summaries used the main frame context (`hglrc=0x30000`). `0.2.5` adds `[OpenXR] BootstrapFrame=120` so OpenXR bootstrap/session probing can be deferred to a real frame boundary and use the frame's current `HDC/HGLRC`.
- Updated the active runtime config for the next pass: `Probe=1`, `SessionProbe=1`, `ReleaseAfterProbe=1`, and `BootstrapFrame=120`.
- Built x64 Release default and OpenXR probes. The flavor files now report `version=0.2.5-xrframeprobe`, with `openxr=0` in `build\Release` and `openxr=1` in `build-openxr\Release`.

### 0.2.4-xrsessiononeshot

- Triaged live `0.2.3-xroneshot`: the OpenXR static probe completed, released the instance, and SOMA continued producing frame summaries for more than a minute afterward. The latest run reached frame `2280` with `openxrInstanceAlive=0` and `openxrInstanceReleasedAfterProbe=1`.
- Windows Error Reporting showed no new `Soma_NoSteam.exe` crash after the `0.2.3` run; the previous SOMA crash remained the older `0.2.2` VirtualDesktop runtime crash from before instance release.
- Added `[OpenXR] ReleaseAfterProbe=1` and extended the one-shot pattern to session probing. With `SessionProbe=1`, SOMAVR now calls `xrCreateSession`, logs reference spaces, swapchain formats, and bounded events if creation succeeds, then immediately destroys the OpenXR session and instance.
- Added summary fields `openxrReleaseAfterProbe=`, `openxrSessionAlive=`, and `openxrSessionReleasedAfterProbe=`. Expected safe session-probe summaries should end with `openxrInstanceAlive=0` and `openxrSessionAlive=0`.
- Updated the active runtime config for the next pass: `Probe=1`, `SessionProbe=1`, and `ReleaseAfterProbe=1`.
- Built x64 Release default and OpenXR probes. The flavor files now report `version=0.2.4-xrsessiononeshot`, with `openxr=0` in `build\Release` and `openxr=1` in `build-openxr\Release`.

### 0.2.3-xroneshot

- Triaged live `0.2.2-xrloaderpath`: the prior delay-load crash is fixed. The run loaded `openxr_loader.dll` from `build-openxr\Release`, enumerated `XR_KHR_opengl_enable`, created an OpenXR instance on `VirtualDesktopXR 1.0.10`, found `Meta Quest 3`, confirmed OpenGL requirements `minGL=4.0.0 maxGL=5.0.0`, and reported two recommended stereo views of `2688x2880`.
- `SessionProbe=0` worked as intended: no `xrCreateSession` was attempted, and frame summaries reported `openxrInitialized=1`, `openxrLoaderLoaded=1`, `openxrViews=2`, and `openxrSwapchainFormats=0`.
- The process still crashed later in `VirtualDesktop.LibOVRRT64_1.dll` with exception `0xc0000005`. Since no session was created, the strongest next suspect is lifetime/teardown of a live OpenXR instance/runtime inside SOMA.
- Changed the no-session path to one-shot discovery: when `SessionProbe=0`, SOMAVR now destroys the OpenXR instance immediately after requirements/view/blend discovery and logs `openxr_instance released_after_static_probe reason=session_probe_disabled`.
- Added summary fields `openxrInstanceAlive=` and `openxrInstanceReleasedAfterProbe=` so the next run can prove the instance was not kept alive while SOMA continues starting.

### 0.2.2-xrloaderpath

- Built x64 Release default probe: `build\Release\somavr.dll` and `build\Release\somavr_injector.exe`.
- Built x64 Release OpenXR probe: `build-openxr\Release\somavr.dll`, `build-openxr\Release\somavr_injector.exe`, and `build-openxr\Release\openxr_loader.dll`.
- Triaged the latest `0.2.1-xrpathguard` crash. The DLL was the correct OpenXR build (`buildOpenXR=1`), hooks installed, and the log reached `gl_context_info`, then stopped before `openxr_extensions`. Windows Error Reporting showed `Soma_NoSteam.exe` failing in `KERNELBASE.dll` with exception `0xc06d007e`, which matches a delay-load module-not-found failure.
- Likely cause: `somavr.dll` is injected from `build-openxr\Release`, but the first delayed `xr*` import searches from SOMA's process/search path and does not reliably find `openxr_loader.dll` beside the injected DLL.
- Added explicit `openxr_loader.dll` preload from the directory containing `somavr.dll` before any OpenXR API call. New rows are `openxr_loader_load attempt`, `openxr_loader_load ok`, or `openxr_loader_load failed ... lastError=...`.
- Added `openxrLoaderLoaded=` to OpenXR frame/proof summaries.
- Changed the active runtime config and default config to `SessionProbe=0` for the immediate retry. Expected next run should proceed from `gl_context_info` to `openxr_loader_load ok`, then `openxr_extensions`, `openxr_system`, `requirements_ok`, view/blend rows, and `openxr_session_probe skipped enabled=0`. If loader load fails, it should log the Windows error instead of crashing.

### 0.2.1-xrpathguard

- Built x64 Release default probe: `build\Release\somavr.dll` and `build\Release\somavr_injector.exe`.
- Built x64 Release OpenXR probe: `build-openxr\Release\somavr.dll`, `build-openxr\Release\somavr_injector.exe`, and `build-openxr\Release\openxr_loader.dll`.
- Triaged the latest `0.2.0-xrprobe` log. It was a clean OpenGL telemetry run, but it loaded `D:\Dev Debug\SOMAVR\build\Release\somavr.dll`, so `openxr_config buildOpenXR=0 enabled=1 sessionProbe=1` and no OpenXR runtime discovery was exercised.
- The same run confirmed the lower-noise logging budget worked: the log was about 100 KB, frame summaries appeared every 120 frames, and `a_mtxModelViewProjection` remained the active projection-like uniform at 70 degree vertical FOV.
- Added a generated `somavr_build_flavor.txt` beside each DLL. Default builds write `flavor=opengl/openxr=0`; OpenXR builds write `flavor=openxr/openxr=1`.
- Added injector-side mismatch detection. If `[OpenXR] Probe=1` and the selected DLL is not OpenXR-enabled, the injector prints a warning before injection with the OpenXR build path to use.
- Upgraded the non-OpenXR DLL's OpenXR-unavailable row to an error-level `build_without_openxr` message that explicitly says to use `build-openxr\Release\somavr.dll` or disable `[OpenXR] Probe`.
- Simplified the documented OpenXR launch command: run `build-openxr\Release\somavr_injector.exe --launch ...` and let that injector pick the DLL beside itself.

### 0.2.0-xrprobe

- Built x64 Release default probe: `build\Release\somavr.dll` and `build\Release\somavr_injector.exe`.
- Built x64 Release OpenXR probe: `build-openxr\Release\somavr.dll`, `build-openxr\Release\somavr_injector.exe`, and `build-openxr\Release\openxr_loader.dll`.
- Triaged the first live `0.1.0-bootstrap` log. It confirmed safe injection, NVIDIA OpenGL 4.6, a 3440x1440 viewport, and a shader-uniform projection path. Fixed-function projection remained invalid, while `a_mtxModelViewProjection` matched SOMA's configured 70 degree vertical FOV.
- Reduced default log volume: frame summaries now default to every 120 frames, matrix sampling remains broad enough to see the per-frame projection uniforms, and individual `uniform_matrix` rows are capped and projection-only by default.
- Expanded the OpenXR probe from graphics requirements only to runtime/system/view/session discovery. New expected rows include `openxr_extensions`, `openxr_system`, `openxr_view_configurations`, `openxr_view`, `openxr_blend_modes`, `openxr_session_probe`, `openxr_reference_spaces`, `openxr_swapchain_formats`, and `openxr_event`.
- Added `[OpenXR] SessionProbe=1` so `xrCreateSession` can be disabled independently if a runtime dislikes being probed from SOMA's active OpenGL context.
- Updated the active runtime `somavr.ini` for the next OpenXR run: `Probe=1`, `SessionProbe=1`, `FrameSummaryInterval=120`, `UniformMatrixProjectionOnly=1`, and `UniformMatrixLogLimit=256`.

## 0.1.0-bootstrap

Initial SOMAVR scaffold:

- x64 CMake project.
- launch/attach injector.
- OpenGL/WGL telemetry DLL.
- optional OpenXR build switch with an OpenGL requirements probe.
- first notes from Ghidra and HPL2 source comparison.
