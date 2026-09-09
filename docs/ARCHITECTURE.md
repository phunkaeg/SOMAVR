# SOMAVR Architecture

This document defines code ownership and growth rules for the injected DLL. It is
the guardrail against turning runtime hooks into a collection of unrelated feature
logic as the mod grows.

## Camera Space Contract

`HPLCameraBridgeStatus::headWorldRotation*` is a legacy misnomer: it is the
recentered tracking-relative orientation, used by movement. New scene-space
consumers must use `headSceneOrientation` after checking its validity. The camera
bridge composes native camera basis before that tracking delta. Read presentation
and wrist world offsets use the scene value. Do not globally reinterpret the
legacy fields: that would rotate locomotion twice.

`HPLInputMath::ResolveHorizontalYaw` is clockwise from -Z. Its inverse is
`OrientationFromHorizontalYaw`, not a positive-Y angle quaternion. Shoulder root
placement and elbow poles share `GetHPLVirtualTorsoYaw`. See the September 7
receipt in `TEST_REVIEW_2026-09-07.md` for regression cases and evidence limits.

## Dependency Direction

```text
DllMain
  -> subsystem installation and shutdown only

OpenGLHooks
  -> OpenGLMatrixAnalysis
  -> OpenXRRuntime
  -> HPLCameraBridge status
  -> HPLPlayerState frame update

HPLCameraBridge
  -> HPLCameraMath
  -> OpenXRRuntime pose/view snapshots

HPLInputBridge
  -> OpenXRInput snapshots through OpenXRRuntime
  -> HPLPlayerState snapshot
  -> HPLCameraBridge recenter/status
  -> HPLNativeLocomotion guarded normal-state fast path
  -> HPLMenuBridge paused pointer and click policy
  -> HPLPhysicalCrouchMath tested height/hysteresis state machine
  -> HPLStatusPanelBridge exclusive user-interface input owner
  -> OpenXRComfortVignetteMath post-policy motion intensity

HPLStatusPanelBridge
  -> immutable player/camera/input snapshots
  -> guarded recenter, roomscale, projection, same-frame stereo, HUD visibility/shape, reticle, and comfort-vignette controls
  -> OpenXRRuntime status model publication

HPLDualRenderControl
  -> explicit configured/ready/enabled/fail-closed state
  -> HPLCompatibilityProbe exact-player viewport replay executor
  -> HPLStatusPanelBridge guarded runtime toggle

HPLArmRenderDiagnostics
  -> HPLCompatibilityProbe first-eye/replay-eye viewport boundaries
  -> read-only HPLHandsBridge shared-root and bilateral arm-chain snapshots
  -> bounded matrix-hash evidence for input coherence and in/inter-pass mutation
  -> no pose, stereo-state, or OpenGL mutation

HPLGrabBridge
  -> signature-guarded vector PID output
  -> signature-guarded iPhysicsBody AddImpulse thunk
  -> HPLGrabMath shortest-arc angular target
  -> HPLPlayerState Grab ownership snapshot
  -> HPLCameraBridge world-pose conversion and camera origin
  -> OpenXRInput interaction-owner grip pose/velocity snapshot

HPLNativeLocomotion
  -> signature-guarded iCharacterBody Move/AddYaw/GetFeetPosition/SetFeetPosition and game-pause getter
  -> HPLPlayerState immutable ownership snapshot
  -> HPLInputMath radial deadzone and angle conversion
  -> HPLRoomscaleReconciliationMath tested threshold/hysteresis/step policy
  -> HPLCameraBridge capsule sweep and neutral-pose compensation

HPLInteractionBridge
  -> OpenXRInput snapshots through OpenXRRuntime
  -> HPLPlayerState authored-camera snapshot
  -> HPLCameraBridge world-pose conversion and camera origin
  -> HPLInteractionMath deterministic pressed/hit/sticky/preferred hand selection
  -> native inner closest-entity probes for both hands and one outer finalizer
  -> HPLInputBridge/HPLGrabBridge selected-hand action and manipulation ownership
  -> immutable native entity/body/distance/world-hit snapshot
  -> narrow OpenXR interaction-reticle and focus-haptic updates

HPLCrosshairBridge
  -> signature-guarded global script dispatch and argument reader
  -> exact LuxPlayer crosshair callback identity
  -> HPLInteractionBridge semantic publication only after native callback succeeds

HPLComfortBridge
  -> signature-guarded cLuxPlayer SetCameraPosAdd wrapper
  -> HPLCameraBridge active-tracking ownership
  -> HPLComfortMath semantic channel policy

HPLHandsBridge
  -> signature-guarded cLuxProp identity and SetMatrix boundaries
  -> signature-guarded global closest-body flashlight gameplay-ray boundary
  -> HPLPlayerState authored-camera snapshot
  -> HPLCameraBridge controller world-pose conversion
  -> HPLHandsMath tested controller-root reconstruction
  -> HPLFlashlightMath tested cone-basis redirection

HPLMenuBridge
  -> HPLMenuMath tested head-relative aim projection
  -> native SOMA window cursor only while the pause getter is true

HPLHudBridge
  -> exact gameplay-HUD identity and cGuiSet render boundary
  -> OpenXRRuntime narrow begin/end capture API

HPLUserModuleBridge
  -> exact cLuxUserModule OnAction boundary and mlId identity
  -> HPLPresentationBridge bounded inventory activity publication

HPLHudMath
  -> tested head/aim-locked quad/cylinder pose, physical aspect, angular-size, semantic color,
     and semantic haptic-profile calculation

OpenXRRuntime
  -> OpenXRHelpers
  -> OpenXRInput
  -> OpenXRGLBridge

OpenXRGLBridge
  -> OpenXR swapchain/FBO ownership
  -> guarded uncompressed TGA decode/cache for SOMA's shipped crosshair artwork
  -> procedural reticle fallback when native artwork cannot be used
  -> status-panel texture upload and independent alpha swapchain
  -> comfort-vignette texture upload and independent alpha swapchain

HPLCompatibilityProbe / HPLLifecycle
  -> signature-guarded native HPL boundaries
```

Dependencies should point toward data, pure transforms, and narrow service
interfaces. A low-level math/helper module must not call hooks or own runtime
lifecycle.

## Module Ownership

| Module | Owns | Must not own |
| --- | --- | --- |
| `DllMain` | DLL attach worker, ordered subsystem install/shutdown | Feature logic, OpenGL/OpenXR calls, native camera policy |
| `OpenGLHooks` | Hook registration, GL/WGL interception, frame-boundary dispatch | New gameplay systems or OpenXR session policy |
| `OpenGLMatrixAnalysis` | Pure matrix classification and formatting | GL state, logging lifecycle, hooks |
| `OpenXRRuntime` | Instance/system/session state, delayed loss recovery, one-wait/one-locate upcoming-render frame pacing, independent Wait/Begin/End timing, fresh/held/fallback frame ledger, symmetric manual suspend/restart, extension negotiation, view snapshots, projection/quad/cylinder submission, comfort-vignette envelope/layer policy, and bounded comfort-black frames | HPL camera transforms, AFR pair ownership, or raw gameplay input semantics |
| `tools/xrsim` | Test-only x64 OpenXR runtime, OpenGL compositor, deterministic pose/action/lifecycle control, per-process launch, verified SOMA window input, and compositor captures | Production runtime policy, machine-wide runtime registration, real-driver performance claims, gameplay hooks, or headset acceptance |
| `OpenXRInput` | OpenXR action set, five standard suggested profiles, active per-hand interaction-profile diagnostics, action synchronization, grip/aim spaces, immutable input snapshots | SOMA movement, interaction, hand placement, or camera policy |
| `ConfigPreset` | Pure named comfort-profile parsing and application before ordinary INI overrides | File I/O, native hooks, runtime toggles, or experimental feature activation |
| Injector doctor | Non-invasive build/config/runtime/game/proxy readiness report with failing exit status for hard prerequisites | Launch, injection, runtime instance creation, or headset hardware acceptance |
| `OpenXRHelpers` | OpenXR names, format strings, pose/view conversion | Handles, session lifetime, swapchain ownership |
| `OpenXRGLBridge` | OpenGL projection/HUD/reticle/status/vignette swapchain images, foveation-capable eye create chains, FBOs, invalidatable eye caches, transparent HUD capture, pixel upload, image transfer, and state-preserving spectator backbuffer blit | OpenXR event/session, foveation profile policy, spectator selection policy, or HPL GUI/interaction identity policy |
| `OpenXRComfortVignetteMath` | Pure deadzone-normalized motion intensity, attack/release envelope, and transparent-center radial alpha raster | OpenXR/GL handles, native input policy, frame lifecycle, or logging |
| `OpenXRStatusPanelMath` | Pure fixed-glyph status/options rasterization into an OpenGL-oriented RGBA buffer | OpenXR/GL handles, input state, native pointers, or runtime policy |
| `OpenXRSpectatorMath` | Pure fit/fill/stretch source and destination rectangle calculation | GL state, eye-cache ownership, runtime policy, logging, or native window handles |
| `HPLCameraBridge` | Signature-guarded player-camera interception, VR mode state, immutable eye-one native-camera/OpenXR-view ownership across an AFR pair, and cached static/dynamic-filtered room-scale query orchestration | Generic quaternion/projection/collision-fraction algorithms or OpenXR frame pacing |
| `HPLCameraMath` | Pure pose, matrix, FOV centering, projection construction, AFR pair-base acceptance, room-scale clearance/head-volume sampling, and tracked-component decomposition | HPL pointers, hotkeys, logging, OpenXR handles, or native collision calls |
| `HPLArmIKMath` | Pure two-bone solve, reach-gated shoulder contribution, and cross-product elbow-pole policy with continuous lateral-singularity/history fallback | HPL node pointers, controller snapshots, visibility ownership, or logging |
| `HPLInputMath` | Pure radial stick deadzone and angle conversion used by native locomotion | Native pointers, action state, logging, or input injection |
| `HPLPhysicalCrouchMath` | Pure standing-height calibration and crouch hysteresis | Native input injection, OpenXR handles, player state, or logging |
| `HPLPlayerState` | Signature-guarded player/camera/body discovery, player/move IDs, camera ownership classification, immutable snapshots | Controller injection, camera transforms, OpenXR actions |
| `HPLInputBridge` | Reversible SOMA input-path controls plus authored-camera/hard-pause suppression and bounded high-motion player-state transition blackouts | Native player discovery, OpenXR action ownership, camera math, or authored pose replacement |
| `HPLStatusPanelBridge` | Exclusive panel input lifecycle and guarded user-facing VR option commands | Text rasterization, swapchains, native discovery, or world rendering |
| `HPLDualRenderControl` | Configured/ready/enabled state, explicit runtime changes, rejection counts, and fail-closed disable policy | Native viewport hooks, render-pass execution, GL resources, temporal state restoration, or XR submission |
| `HPLNativeLocomotion` | Guarded analog Move and exact-radian AddYaw calls only in unpaused normal player/move state; exposes the confirmed pause state to input policy | Player discovery, special-state input semantics, direct capsule transforms, or bypassing pause ownership |
| `HPLMenuBridge` | Paused-only head-relative controller aim to native client cursor routing | GUI rendering/capture, pause ownership, OpenXR actions, or gameplay clicks |
| `HPLMenuMath` | Pure HMD/controller orientation projection into normalized menu coordinates | HWND state, cursor mutation, native pointers, or logging |
| `HPLTerminalBridge` | Signature-guarded state-8 body/camera takeover suppression, exact world-ImGui input ownership, physical mesh projection, and head-locked-overlay pointer coordinates | Terminal focus, widget policy, GUI capture, non-terminal GUI ownership, or handheld state-9 presentation |
| `HPLInteractionBridge` | Signature-guarded dual-hand inner closest-entity probes, deterministic single-result selection, exactly one outer finalizer, immutable hit snapshot, and selected-hand ownership publication | `CanInteract`, distance policy, focus callbacks, object physics, or reticle rendering |
| `HPLGameplayHapticsBridge` | Exact-signature preservation/mirroring of SOMA's script-authored global rumble into bilateral OpenXR output | Raw collision synthesis, material classification, VR-hand inference from gamepad index, or script timing ownership |
| `HPLGameplayHapticsMath` | Pure rising-edge, strength-retrigger, refresh, bounded-segment, and falling-edge envelope policy | Native pointers, OpenXR handles, hook lifecycle, or logging |
| `HPLContactHapticsBridge` | Exact-signature observation of native surface impacts plus Grab-state/fresh interaction-owner-grip proximity gating and one-hand OpenXR pulse dispatch | Physics mutation, native sound/particle suppression, exact grabbed-body ownership, sustained scrape synthesis, or non-Grab collision feedback |
| `HPLContactHapticsMath` | Pure finite/speed/distance/cooldown validation and bounded linear amplitude mapping | Native pointers, controller poses, OpenXR handles, hook lifecycle, or logging |
| `HPLComfortBridge` | Transactional guarded ownership of semantic camera-add, Set/Fade camera-roll, authored FOV/multiplier, and world DoF boundaries during active VR | Lower-level camera transforms, player-state ownership, fades/tone mapping, or broad post chains |
| `HPLComfortMath` | Pure camera-add/roll enum classification, optics target policy, independent suppression policy, and high-motion player-state transition classification | Native pointers, hook lifecycle, tracking state, or logging |
| `HPLPresentationBridge` | Exact loading-screen query, wake sleep/timed-presentation state, bounded inventory activity state, shared blackout arbitration, AFR/XR/input transition policy, and probe-only native video lifecycle telemetry | Video replacement, GUI rendering/capture, broad user-module inference, or OpenXR session internals |
| `HPLUserModuleBridge` | Signature-guarded user-module action observation and exact module `15`/action `12` inventory publication after native dispatch | GUI capture, action mutation, generic module lifecycle inference, or OpenXR ownership |
| `HPLGrabBridge` | Exact Grab force/torque PID identity, controller-relative target substitution, and one-shot AddImpulse throw redirection | PID tuning, object mass/collision/joints, persistent physics replacement, or script callbacks |
| `HPLGrabMath` | Pure shortest-arc quaternion delta to bounded angular target velocity | Native pointers, PID identity, hooks, tracking policy, or logging |
| `HPLHandsBridge` | Shared Lux-entity SetMatrix detour, exact `PlayerHands_*`/`Flashlight` identity routing, guarded grip-root/aim-light substitution, and narrow flashlight gameplay-ray redirection | Skeletal/tool animation, sockets, broad light mutation, general physics-ray policy, full-scale/custom/authored transforms, or untracked pose ownership |
| `HPLHandsMath` | Pure HPL basis reconstruction, scale preservation, and configurable root calibration | Native pointers, entity identity, tracking policy, or logging |
| `HPLFlashlightMath` | Pure OpenXR aim to HPL negative-Z spotlight basis, local calibration, and source-to-target cone-direction preservation | Native pointers, light identity/lifetime, tracking policy, ray classification, or logging |
| `HPLHudBridge` | Signature-guarded GameHudSet/GameHudImGui capture plus exact pause, wake, dead-state, and inventory current-ImGui routing with per-set telemetry | OpenXR swapchain/session ownership, broad current-ImGui capture, or diegetic GUI policy |
| `HPLHudMath` | Pure quad/cylinder pose, physical arc, angular size, and aspect validation | GL state, OpenXR handles, native pointers, or logging |
| `HPLSubtitleBridge` | Signature-guarded scoped override/restore of native voice subtitle layout during active stereo | Subtitle content, localization, timing, enable state, font resources, or HUD swapchains |
| `HPLSubtitleMath` | Pure validated subtitle width/font/Y/shadow scaling | Native pointers, hooks, camera state, or logging |
| `HPLCompatibilityProbe` | Bounded render/audio/post-effect telemetry, left/right/mono CPU stage totals, normalized replay draw-cost telemetry, temporary probes, and the single exact-player viewport replay hook shared by bounded and continuous dual render; shared pose math comes from `HPLCameraMath` | Permanent dual-render user policy, GUI/HUD feature policy, GPU timing ownership, or unrelated gameplay systems |
| `HPLPerEyeViewHistory` | Guarded native access and per-eye transaction ownership for the confirmed renderer previous-view packet during continuous exact-player stereo | Other temporal resources, post-effect policy, camera scheduling, or viewport replay policy |
| `HPLPerEyeViewHistoryMath` | Pure two-eye packet banking, identity reseed, frame-regression reset, prepare, and commit rules | Native pointers, hooks, logging, or OpenXR state |
| `HPLToneMappingFrame` | Guarded native access and once-per-pose ownership for ToneMapping exposure, white-cut, window fade, grading-transition, and film-grain sampling state | Bloom scratch allocation, post-effect suppression, shader policy, camera scheduling, or XR submission |
| `HPLToneMappingFrameMath` | Pure first-eye/replay-eye scheduling across pose frames, calibration changes, eye-order changes, duplicates, and stereo release | Native pointers, packet offsets, hooks, logging, or rendering |
| `HPLLifecycle` | Pre-graphics OpenXR teardown boundary | General shutdown orchestration |

## Growth Rules

1. A hook callback should gather native state, delegate to one owner, restore any
   temporary state, and return. New multi-step behavior gets its own module.
2. Deterministic math, classification, and conversion code belongs in a library
   that can be tested without launching or injecting SOMA.
3. Native RVAs, signatures, and evidence stay synchronized with
   `ADDRESS_REGISTRY.md` and `GHIDRA_SYNC.md`; they are not duplicated casually.
4. Experimental probes remain bounded, configurable, and removable. A proven
   feature graduates from probe code into its permanent owner.
5. File length is a warning, not the design metric. Split when a translation unit
   has multiple unrelated reasons to change, owns more than one lifecycle, or
   contains independently testable logic.
6. New user-facing systems use stable `FEATURE.*` IDs and update Graphify after
   implementation or architecture changes.

## Current Refactor Baseline

The maintenance passes now include eleven focused extractions:

- `HPLCameraMath` owns quaternion/matrix operations, OpenXR projection creation,
  and the fully centered FOV policy proven by the `0.5.7` runtime result.
- `OpenGLMatrixAnalysis` owns matrix classification and telemetry formatting.
- `OpenXRHelpers` owns OpenXR enum/format names and view/pose conversions.
- `OpenXRInput` owns controller actions and predicted grip/aim acquisition while
  `OpenXRRuntime` remains the lifecycle and frame-submission facade.
- `HPLPlayerState` owns native player discovery and camera-ownership snapshots,
  allowing locomotion, hands, interaction, and comfort adapters to share one
  guarded source instead of duplicating executable offsets.
- `HPLHudBridge` owns the GUI-set hook and exact gameplay-HUD capture policy,
  removing permanent HUD behavior from `HPLCompatibilityProbe`.
- `HPLHudMath` owns testable VIEW-space quad/cylinder placement and sizing while
  `OpenXRGLBridge` owns only GL/swapchain resources.
- `HPLHandsMath` owns testable controller-grip to HPL root reconstruction while
  `HPLHandsBridge` owns exact identity and native-state policy.
- `HPLMenuMath` owns head-relative aim projection and `HPLMenuBridge` owns only
  the paused native-window cursor lifecycle.
- `HPLSubtitleMath` owns validated native-layout scaling while
  `HPLSubtitleBridge` owns the one exact draw hook and immediate restoration.
- `HPLDualRenderDiagnostics` owns safe native-region snapshots and first/replay
  correlation, while `HPLTemporalMutationMath` owns tested byte hashing and
  bounded changed-range classification. `HPLDualRenderControl` owns the explicit
  sustained-mode state; `HPLCompatibilityProbe` owns the one native viewport
  hook and executes either bounded diagnostic or continuous replay policy.

`somavr_render_math_tests` now protects symmetric tangent-span preservation,
zero projection offsets, projection construction, temporal mutation ranges,
pose/matrix basics, HUD quad/cylinder
placement/aspect validation, controller-hand basis/calibration, paused-menu aim
projection, radial stick scaling, turn-angle conversion, and OpenGL projection classification in
both build flavors.

## Next Structural Splits

These are ordered by value and runtime risk:

1. Extract F3/F6/F7/F9 shader, UBO, and matrix capture state from
   `OpenGLHooks.cpp` into `OpenGLDiagnostics` behind a narrow event API.
2. Extract MinHook/WGL extension registration into `OpenGLHookRegistry`, leaving
   `OpenGLHooks` as frame and callback dispatch.
3. Continue splitting `OpenXRRuntime::Impl` into bootstrap, session lifecycle,
   and frame-composition owners while preserving one public facade and one lock
   policy. Controller action ownership has already moved to `OpenXRInput`.
4. Grow the new `HPLPlayerState` snapshot with named state adapters only after
   each native state/ownership transition appears in a live log. Keep pose
   composition in `HPLCameraBridge` and input suppression in `HPLInputBridge`.

Each split should compile and test independently. Runtime-sensitive splits also
retain the previous DLL until a live log confirms equivalent behavior.
