# SOMAVR Architecture

This document defines code ownership and growth rules for the injected DLL. It is
the guardrail against turning runtime hooks into a collection of unrelated feature
logic as the mod grows.

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

HPLStatusPanelBridge
  -> immutable player/camera/input snapshots
  -> guarded recenter, roomscale, projection, HUD, and reticle controls
  -> OpenXRRuntime status model publication

HPLGrabBridge
  -> signature-guarded vector PID output
  -> signature-guarded iPhysicsBody AddImpulse thunk
  -> HPLGrabMath shortest-arc angular target
  -> HPLPlayerState Grab ownership snapshot
  -> HPLCameraBridge world-pose conversion and camera origin
  -> OpenXRInput dominant grip pose/velocity snapshot

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

HPLHudMath
  -> tested head/aim-locked quad pose, aspect, angular-size, semantic color,
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
| `OpenXRRuntime` | Instance/system/session state, delayed loss recovery, frame pacing, view snapshots, projection/quad layer submission and bounded comfort-black frames | HPL camera transforms or gameplay input semantics |
| `OpenXRInput` | OpenXR action set, suggested bindings, action synchronization, grip/aim spaces, immutable input snapshots | SOMA movement, interaction, hand placement, or camera policy |
| `OpenXRHelpers` | OpenXR names, format strings, pose/view conversion | Handles, session lifetime, swapchain ownership |
| `OpenXRGLBridge` | OpenGL projection/HUD/reticle swapchain images, FBOs, invalidatable eye caches, transparent HUD capture, reticle drawing, image transfer, and state-preserving spectator backbuffer blit | OpenXR event/session, spectator selection policy, or HPL GUI/interaction identity policy |
| `OpenXRStatusPanelMath` | Pure fixed-glyph status/options rasterization into an OpenGL-oriented RGBA buffer | OpenXR/GL handles, input state, native pointers, or runtime policy |
| `OpenXRSpectatorMath` | Pure fit/fill/stretch source and destination rectangle calculation | GL state, eye-cache ownership, runtime policy, logging, or native window handles |
| `HPLCameraBridge` | Signature-guarded player-camera interception, VR mode state, and cached static/dynamic-filtered room-scale query orchestration | Generic quaternion/projection/collision-fraction algorithms |
| `HPLCameraMath` | Pure pose, matrix, FOV centering, projection construction, room-scale clearance/head-volume sampling, and tracked-component decomposition | HPL pointers, hotkeys, logging, OpenXR handles, or native collision calls |
| `HPLInputMath` | Pure radial stick deadzone and angle conversion used by native locomotion | Native pointers, action state, logging, or input injection |
| `HPLPhysicalCrouchMath` | Pure standing-height calibration and crouch hysteresis | Native input injection, OpenXR handles, player state, or logging |
| `HPLPlayerState` | Signature-guarded player/camera/body discovery, player/move IDs, camera ownership classification, immutable snapshots | Controller injection, camera transforms, OpenXR actions |
| `HPLInputBridge` | Reversible SOMA input-path controls plus authored-camera/hard-pause suppression and bounded high-motion player-state transition blackouts | Native player discovery, OpenXR action ownership, camera math, or authored pose replacement |
| `HPLStatusPanelBridge` | Exclusive panel input lifecycle and guarded user-facing VR option commands | Text rasterization, swapchains, native discovery, or world rendering |
| `HPLNativeLocomotion` | Guarded analog Move and exact-radian AddYaw calls only in unpaused normal player/move state; exposes the confirmed pause state to input policy | Player discovery, special-state input semantics, direct capsule transforms, or bypassing pause ownership |
| `HPLMenuBridge` | Paused-only head-relative controller aim to native client cursor routing | GUI rendering/capture, pause ownership, OpenXR actions, or gameplay clicks |
| `HPLMenuMath` | Pure HMD/controller orientation projection into normalized menu coordinates | HWND state, cursor mutation, native pointers, or logging |
| `HPLTerminalBridge` | Signature-guarded current 3D ImGui virtual-cursor substitution only during exact wall/handheld terminal states `8/9` | Terminal focus/camera placement, widget policy, HUD/menu capture, OpenXR actions, or non-terminal GUI ownership |
| `HPLInteractionBridge` | Signature-guarded native closest-entity query substitution, immutable finalized hit snapshot, and narrow reticle/focus-haptic publication | `CanInteract`, distance policy, focus callbacks, object physics, reticle rendering, or controller action ownership |
| `HPLGameplayHapticsBridge` | Exact-signature preservation/mirroring of SOMA's script-authored global rumble into bilateral OpenXR output | Raw collision synthesis, material classification, VR-hand inference from gamepad index, or script timing ownership |
| `HPLGameplayHapticsMath` | Pure rising-edge, strength-retrigger, refresh, bounded-segment, and falling-edge envelope policy | Native pointers, OpenXR handles, hook lifecycle, or logging |
| `HPLComfortBridge` | Transactional guarded ownership of semantic camera-add, Set/Fade camera-roll, authored FOV/multiplier, and world DoF boundaries during active VR | Lower-level camera transforms, player-state ownership, fades/tone mapping, or broad post chains |
| `HPLComfortMath` | Pure camera-add/roll enum classification, optics target policy, independent suppression policy, and high-motion player-state transition classification | Native pointers, hook lifecycle, tracking state, or logging |
| `HPLPresentationBridge` | Exact loading-screen query, AFR/XR/input transition policy, and probe-only native video stream lifecycle telemetry | Video playback replacement, menu/HUD capture, native loading ownership, or OpenXR session internals |
| `HPLGrabBridge` | Exact Grab force/torque PID identity, controller-relative target substitution, and one-shot AddImpulse throw redirection | PID tuning, object mass/collision/joints, persistent physics replacement, or script callbacks |
| `HPLGrabMath` | Pure shortest-arc quaternion delta to bounded angular target velocity | Native pointers, PID identity, hooks, tracking policy, or logging |
| `HPLHandsBridge` | Shared Lux-entity SetMatrix detour, exact `PlayerHands_*`/`Flashlight` identity routing, guarded grip-root/aim-light substitution, and narrow flashlight gameplay-ray redirection | Skeletal/tool animation, sockets, broad light mutation, general physics-ray policy, full-scale/custom/authored transforms, or untracked pose ownership |
| `HPLHandsMath` | Pure HPL basis reconstruction, scale preservation, and configurable root calibration | Native pointers, entity identity, tracking policy, or logging |
| `HPLFlashlightMath` | Pure OpenXR aim to HPL negative-Z spotlight basis, local calibration, and source-to-target cone-direction preservation | Native pointers, light identity/lifetime, tracking policy, ray classification, or logging |
| `HPLHudBridge` | Signature-guarded GameHudSet/GameHudImGui capture plus pause-gated exact-current-ImGui routing and per-set telemetry | OpenXR swapchain/session ownership, non-paused broad ImGui capture, or diegetic GUI policy |
| `HPLHudMath` | Pure quad pose, angular size, and aspect validation | GL state, OpenXR handles, native pointers, or logging |
| `HPLSubtitleBridge` | Signature-guarded scoped override/restore of native voice subtitle layout during active stereo | Subtitle content, localization, timing, enable state, font resources, or HUD swapchains |
| `HPLSubtitleMath` | Pure validated subtitle width/font/Y/shadow scaling | Native pointers, hooks, camera state, or logging |
| `HPLCompatibilityProbe` | Bounded render/audio/post-effect telemetry, left/right/mono CPU stage totals, and temporary probes; shared pose math comes from `HPLCameraMath` | Permanent GUI/HUD feature policy, GPU timing ownership, or unrelated gameplay systems |
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
- `HPLHudMath` owns testable VIEW-space quad placement and sizing while
  `OpenXRGLBridge` owns only GL/swapchain resources.
- `HPLHandsMath` owns testable controller-grip to HPL root reconstruction while
  `HPLHandsBridge` owns exact identity and native-state policy.
- `HPLMenuMath` owns head-relative aim projection and `HPLMenuBridge` owns only
  the paused native-window cursor lifecycle.
- `HPLSubtitleMath` owns validated native-layout scaling while
  `HPLSubtitleBridge` owns the one exact draw hook and immediate restoration.
- `HPLDualRenderDiagnostics` owns safe native-region snapshots and first/replay
  correlation, while `HPLTemporalMutationMath` owns tested byte hashing and
  bounded changed-range classification. `HPLCompatibilityProbe` only schedules
  and labels the render passes.

`somavr_render_math_tests` now protects symmetric tangent-span preservation,
zero projection offsets, projection construction, temporal mutation ranges,
pose/matrix basics, HUD quad
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
