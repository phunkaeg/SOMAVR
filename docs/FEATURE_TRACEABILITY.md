# Feature Traceability Registry

This is the canonical, graph-friendly index for SOMAVR features. Stable
`FEATURE.*` IDs should appear in future design notes, commits, tests, and bounded
telemetry where useful. Graphify can then connect implementation, native anchors,
evidence, and acceptance gates without relying on filenames alone.

Status values: `PROVEN`, `EXPERIMENTAL`, `BUILT`, `DESIGNED`, `RE_REQUIRED`, `BLOCKED`.

## Registry

| Feature ID | Status | Code owner | Native/runtime anchors | Primary documentation | Next acceptance gate |
| --- | --- | --- | --- | --- | --- |
| `FEATURE.INJECTION` | PROVEN | `src/injector/main.cpp`, `src/dll/DllMain.cpp` | Remote `LoadLibraryW`, early DLL initialization | `CURRENT_STATE.md` | Launch and attach modes remain reliable across load/save cycles |
| `FEATURE.XR_BOOTSTRAP` | EXPERIMENTAL | `OpenXRRuntime`, `OpenXRHelpers` | OpenXR loader, instance, system, session state, delayed recovery | `CURRENT_STATE.md`, `UEVR_LEARNINGS.md` | Live-test session and instance loss recovery without restarting SOMA |
| `FEATURE.XR_GL_SUBMISSION` | PROVEN | `OpenXRRuntime`, `OpenXRGLBridge` | OpenGL swapchains, FBOs, `xrEndFrame` | `CURRENT_STATE.md` | Validate format/color-space and resize/recreation paths |
| `FEATURE.CLEAN_SHUTDOWN` | PROVEN | `HPLLifecycle`, `OpenXRRuntime` | `0x1403b16e0`, `0x1403b1803` | `RUNTIME_ANALYSIS_0.5.6.md`, `GHIDRA_SYNC.md` | Preserve clean exit across runtime/session-loss paths |
| `FEATURE.VR_MODE_CONTROL` | EXPERIMENTAL | `HPLCameraBridge`, `OpenXRRuntime` | F10 pending activation, F8/F11 diagnostics | `CURRENT_STATE.md`, `TEST_CHECKLISTS.md` | One F10 reaches tracking, stereo, and full centering from a loaded save |
| `FEATURE.RECENTER` | BUILT | `HPLCameraBridge`, `HPLCameraMath`, `HPLInputBridge` | F2 or two-grip hold, stable neutral-pose latch | `BUILD_HISTORY.md`, `CURRENT_STATE.md`, `TEST_CHECKLISTS.md` | Live test confirms keyboard and controller recenter without stereo/session reset or height drift |
| `FEATURE.XR_INPUT` | BUILT | `OpenXRInput`, `OpenXRRuntime` | OpenXR action set, Simple/Touch/Index/Motion bindings, grip/aim action spaces | `BUILD_HISTORY.md`, `CURRENT_STATE.md`, `TEST_CHECKLISTS.md` | Live log confirms active bindings, both tracked controllers, and stable predicted poses |
| `FEATURE.XR_REFERENCE_SPACE` | BUILT | `OpenXRRuntime`, config | `XR_REFERENCE_SPACE_TYPE_LOCAL`, optional `STAGE`, runtime fallback | `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Compare seated/local and standing/stage calibration, eye height, recenter, and map transitions |
| `FEATURE.TRACKING_RESILIENCE` | BUILT | `OpenXRRuntime`, `HPLCameraBridge` | pose-age bound, last-valid eye cache, zero-layer loss path, recovery blackout | `BUILD_HISTORY.md`, `CURRENT_STATE.md`, `TEST_CHECKLISTS.md` | Live-test brief and extended HMD tracking loss without stale-eye corruption, stereo teardown, or a visible recovery flash |
| `FEATURE.CONTROLLER_HAPTICS` | BUILT | `OpenXRInput`, `OpenXRRuntime`, `HPLInputBridge`, `HPLInteractionBridge` | vibration output action, per-hand output paths, focused-session guard, native focus identity transitions | `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Confirm discrete and focus-change pulses across active controller profiles without edge chatter |
| `FEATURE.CONTROLLER_ACCESSIBILITY` | BUILT | `OpenXRInput`, `HPLInputBridge`, config | per-hand primary/secondary actions, dominant-hand roles, stick swap, one-hand fallback, support-hand flashlight/inventory | `BUILD_HISTORY.md`, `FUTURE_SYSTEMS_RE.md`, `TEST_CHECKLISTS.md` | Live-test role-aware jump/crouch/flashlight/inventory, swapped-stick, and each one-controller path on Touch/Index; define missing Simple/Motion bindings |
| `FEATURE.MENU_POINTER` | BUILT | `HPLMenuBridge`, `HPLMenuMath`, `HPLInputBridge`, `HPLNativeLocomotion` | `0x1400ccc90`, HMD/aim orientations, native SOMA client cursor and left-click path | `BUILD_HISTORY.md`, `FUTURE_SYSTEMS_RE.md`, `TEST_CHECKLISTS.md` | Live-test window modes, native cursor mapping, click-release latch, and non-pause ImGui surfaces; then couple pointer coordinates to future menu-layer presentation |
| `FEATURE.HEAD_TRACKING` | PROVEN | `HPLCameraBridge`, `HPLCameraMath` | `0x140271b80`, `0x140270230` | `CURRENT_STATE.md`, `VR_COMPATIBILITY_RE.md` | Remain correct through every authored camera state |
| `FEATURE.AFR_STEREO` | PROVEN | `HPLCameraBridge`, `OpenXRRuntime`, `OpenXRGLBridge` | F11, per-eye cache and submitted render pose | `BUILD_HISTORY.md`, `RUNTIME_ANALYSIS_0.5.1.md` | Preserve stability while shader/temporal compatibility is classified |
| `FEATURE.DUAL_RENDER` | RE_REQUIRED | `HPLCompatibilityProbe`, future native render bridge | `0x140298850`, `0x140298630`, `0x1401f9790` | `VR_COMPATIBILITY_RE.md` | Live stage/FBO telemetry proves a side-effect-safe per-eye boundary |
| `FEATURE.LOCOMOTION` | BUILT | `OpenXRInput`, `HPLInputBridge`, `HPLNativeLocomotion`, `HPLPhysicalCrouchMath`, `HPLPlayerState` | SOMA semantic fallback, `0x1402375f0`, `0x140237460`, `0x1400ccc90`, calibrated HMD yaw/height, player/move ownership | `FUTURE_SYSTEMS_RE.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Live-test head-relative direction, analog magnitude, physical-crouch toggle synchronization, pause safety, and special-state fallback |
| `FEATURE.AUTHORED_CAMERA` | EXPERIMENTAL | `HPLPlayerState`, `HPLInputBridge`, `HPLCameraBridge` | camera rotate mode `+0x6c`, body camera ownership `+0x1e8`, player/move state | `VR_COMPATIBILITY_RE.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Confirm transition detection/input suppression across sit, ladder, conversation, animation, and death states, then add pose-composition policy |
| `FEATURE.INTERACTION_RAY` | BUILT | `HPLInteractionBridge`, `HPLCameraBridge`, `HPLInputBridge` | `0x1400cd750`, `cLuxClosestEntityData +0x18/+0x20/+0x28`, world aim/hit snapshots, native `CanInteract` | `COMFORT_AND_FOCUS_RE.md`, `VR_COMPATIBILITY_RE.md`, `TEST_CHECKLISTS.md` | Live-test controller-directed focus and decoded distance/world hit across targets and states; verify no-hit/fallback clears validity and correlate native semantic states |
| `FEATURE.INTERACTION_RETICLE` | BUILT | `HPLInteractionBridge`, `HPLHudMath`, `OpenXRRuntime`, `OpenXRGLBridge` | native pick aim/distance, application-space alpha quad, angular size and age bounds | `COMFORT_AND_FOCUS_RE.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Live-test convergence, apparent size, clearing, haptic cadence, and mismatch cases before adding semantic icon and occlusion policy |
| `FEATURE.PHYSICS_HANDS` | BUILT | `HPLGrabBridge`, `HPLGrabMath`, `HPLInputBridge`, `OpenXRInput` | PID output `0x140238750`, AddImpulse thunk `0x14049c720`, Grab state `1`, exact force/torque tuples, controller pose/velocity | `VR_COMPATIBILITY_RE.md`, `FUTURE_SYSTEMS_RE.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Live-test translation and shortest-arc rotation stability, per-axis sign, one-shot velocity-directed throws, velocity scaling across object masses, and all native fallbacks |
| `FEATURE.VIEWMODEL` | BUILT | `HPLHandsBridge`, `HPLHandsMath`, `HPLCameraBridge`, `HPLInputBridge` | world grip pose, `PlayerHandsHandler`, `0x14000fb60`, `0x1400bcd90`, `R_Hand`, tool `HudObject` | `FUTURE_SYSTEMS_RE.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Live-test root placement/orientation calibration, tool sockets, and automatic native fallback across full-scale, custom, authored, and tracking-loss states; then add per-tool profiles |
| `FEATURE.HUD_LAYER` | BUILT | `HPLHudBridge`, `HPLHudMath`, `OpenXRGLBridge`, `OpenXRRuntime` | `0x1400cc9b0`, `0x140213970`, transparent GL capture FBO, center crosshair clear, VIEW-space quad | `FUTURE_SYSTEMS_RE.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Live-test alpha/scale, center-clear coverage, text placement, and native fallback; then add subtitle policy |
| `FEATURE.POST_EFFECT_POLICY` | BUILT | `HPLCompatibilityProbe`, `OpenGLHooks` | `0x14033b8f0`, `0x14033bd80`, priority tree `+0x328`, named vtables, active byte `+0x31` | `FUTURE_SYSTEMS_RE.md`, `BUILD_HISTORY.md`, `RUNTIME_ANALYSIS_0.5.2.md` | Live-test default suppression of ImageTrail, ChromaticAberration, and RadialBlur while fades/tone mapping remain intact |
| `FEATURE.SHADOW_STABILITY` | PROVEN | `HPLCameraBridge`, `HPLCameraMath` | fully centered projection, programs `942/944` | `VR_COMPATIBILITY_RE.md`, `RUNTIME_ANALYSIS_0.5.6.md` | Regression-test additional levels and light types |
| `FEATURE.REFLECTION_STABILITY` | PROVEN | `HPLCameraBridge`, `HPLCameraMath` | fully centered projection, program `985` redirect | `VR_COMPATIBILITY_RE.md`, `RUNTIME_ANALYSIS_0.5.6.md` | Regression-test additional reflective materials and levels |
| `FEATURE.AUDIO_LISTENER` | BUILT | `HPLCompatibilityProbe`, `HPLCameraBridge`, `HPLCameraMath` | `0x140289340`, world head offset, `0x14061d188`, `0x14048b2d4` | `VR_COMPATIBILITY_RE.md`, `BUILD_HISTORY.md` | Directional-source and near-field tests confirm orientation plus room-scale translation without world-lock or Doppler errors |
| `FEATURE.LOADING_VIDEO` | EXPERIMENTAL | `OpenXRRuntime`, `HPLCameraBridge`, future presentation bridge | camera replacement, AFR cache invalidation, load-screen registrations, video lifecycle | `VR_COMPATIBILITY_RE.md` | Save/load and map transitions automatically re-arm VR; classify dedicated loading/video presentation next |
| `FEATURE.COMFORT_POLICY` | BUILT | `HPLComfortBridge`, `HPLComfortMath`, `HPLCameraBridge`, `OpenXRRuntime`, post policy, input | `0x140159360`, semantic Bob/Shake/Sway IDs `1/2/9`, comfort-black frames, native roll fields | `COMFORT_AND_FOCUS_RE.md`, both future RE documents | Live-test semantic Bob/Shake/Sway suppression across locomotion, impacts, and authored states; retain the roll hook as a separate opt-in policy |
| `FEATURE.BUILD_IDENTITY` | BUILT | CMake build manifest | version, flavor, OpenXR bit, DLL SHA-256 | `BUILD_HISTORY.md` | Verify packaged builds reproduce identity and integrity metadata |
| `FEATURE.TELEMETRY` | PROVEN | `OpenGLHooks`, `OpenGLMatrixAnalysis`, `HPLCompatibilityProbe`, `HPLPlayerState`, `HPLInputBridge`, logger, config | Swap/FBO/matrix/runtime/stage/audio/player-state/post-effect bounded logs | all current docs | Correlate authored-camera transitions, stage-tagged draws, GUI state, and per-effect identities in a live run |

## Dependency Edges

These relations are intentionally explicit Graphify seeds:

```text
FEATURE.INJECTION requires FEATURE.TELEMETRY
FEATURE.XR_BOOTSTRAP requires FEATURE.INJECTION
FEATURE.XR_GL_SUBMISSION requires FEATURE.XR_BOOTSTRAP
FEATURE.CLEAN_SHUTDOWN requires FEATURE.XR_GL_SUBMISSION
FEATURE.VR_MODE_CONTROL requires FEATURE.XR_GL_SUBMISSION
FEATURE.VR_MODE_CONTROL requires FEATURE.AFR_STEREO
FEATURE.RECENTER requires FEATURE.VR_MODE_CONTROL
FEATURE.RECENTER requires FEATURE.HEAD_TRACKING
FEATURE.XR_INPUT requires FEATURE.XR_BOOTSTRAP
FEATURE.XR_REFERENCE_SPACE requires FEATURE.XR_BOOTSTRAP
FEATURE.TRACKING_RESILIENCE requires FEATURE.XR_GL_SUBMISSION
FEATURE.TRACKING_RESILIENCE constrains FEATURE.HEAD_TRACKING
FEATURE.TRACKING_RESILIENCE constrains FEATURE.AFR_STEREO
FEATURE.RECENTER requires FEATURE.XR_REFERENCE_SPACE
FEATURE.CONTROLLER_HAPTICS requires FEATURE.XR_INPUT
FEATURE.CONTROLLER_ACCESSIBILITY requires FEATURE.XR_INPUT
FEATURE.CONTROLLER_ACCESSIBILITY constrains FEATURE.LOCOMOTION
FEATURE.MENU_POINTER requires FEATURE.XR_INPUT
FEATURE.LOCOMOTION requires FEATURE.XR_INPUT
FEATURE.HEAD_TRACKING requires FEATURE.XR_BOOTSTRAP
FEATURE.AFR_STEREO requires FEATURE.HEAD_TRACKING
FEATURE.AFR_STEREO requires FEATURE.XR_GL_SUBMISSION
FEATURE.DUAL_RENDER requires FEATURE.AFR_STEREO
FEATURE.DUAL_RENDER requires FEATURE.TELEMETRY
FEATURE.POST_EFFECT_POLICY requires FEATURE.DUAL_RENDER
FEATURE.SHADOW_STABILITY requires FEATURE.AFR_STEREO
FEATURE.REFLECTION_STABILITY requires FEATURE.AFR_STEREO
FEATURE.DUAL_RENDER requires FEATURE.SHADOW_STABILITY
FEATURE.DUAL_RENDER requires FEATURE.REFLECTION_STABILITY
FEATURE.HUD_LAYER requires FEATURE.XR_GL_SUBMISSION
FEATURE.HUD_LAYER constrains FEATURE.AFR_STEREO
FEATURE.AUTHORED_CAMERA requires FEATURE.HEAD_TRACKING
FEATURE.LOCOMOTION requires FEATURE.AUTHORED_CAMERA
FEATURE.INTERACTION_RAY requires FEATURE.AUTHORED_CAMERA
FEATURE.INTERACTION_RAY requires FEATURE.XR_INPUT
FEATURE.INTERACTION_RETICLE requires FEATURE.INTERACTION_RAY
FEATURE.INTERACTION_RETICLE requires FEATURE.XR_GL_SUBMISSION
FEATURE.INTERACTION_RETICLE constrains FEATURE.HUD_LAYER
FEATURE.PHYSICS_HANDS requires FEATURE.INTERACTION_RAY
FEATURE.PHYSICS_HANDS requires FEATURE.VIEWMODEL
FEATURE.AUDIO_LISTENER requires FEATURE.AUTHORED_CAMERA
FEATURE.LOADING_VIDEO requires FEATURE.HUD_LAYER
FEATURE.COMFORT_POLICY constrains FEATURE.AUTHORED_CAMERA
FEATURE.COMFORT_POLICY constrains FEATURE.POST_EFFECT_POLICY
FEATURE.COMFORT_POLICY constrains FEATURE.LOCOMOTION
HPLCameraBridge requires HPLCameraMath
HPLCompatibilityProbe requires HPLCameraMath
OpenGLHooks requires OpenGLMatrixAnalysis
OpenXRRuntime requires OpenXRHelpers
OpenXRRuntime requires OpenXRInput
HPLInputBridge requires OpenXRInput
HPLInputBridge requires HPLCameraBridge
HPLInputBridge requires HPLPlayerState
HPLCompatibilityProbe consumes OpenGLHooks telemetry
HPLInputBridge requires HPLNativeLocomotion
HPLInputBridge requires HPLMenuBridge
HPLNativeLocomotion requires HPLPlayerState
HPLMenuBridge requires HPLMenuMath
HPLHandsBridge requires HPLHandsMath
HPLHudBridge requires OpenXRRuntime
OpenXRRuntime requires HPLHudMath
HPLInteractionBridge feeds FEATURE.INTERACTION_RETICLE
```

## Runtime Flow

```text
somavr_injector.exe
  -> somavr.dll
  -> OpenGLHooks::HookSwapBuffers
  -> OpenXRRuntime::OnFrameBoundary
  -> HPLInputBridge::UpdateHPLInputBridge
  -> xrWaitFrame / xrBeginFrame / xrLocateViews
  -> HPLCameraBridge::HookCameraGetFrustum
  -> OpenXRGLBridge eye cache or swapchain copy
  -> xrEndFrame
```

Future same-frame flow:

```text
OpenXR predicted views
  -> FEATURE.DUAL_RENDER
  -> left/right scene and post targets
  -> FEATURE.HUD_LAYER
  -> projection layers plus optional quad layer
  -> FEATURE.XR_GL_SUBMISSION
```

Gameplay ownership flow:

```text
OpenXR actions
  -> FEATURE.LOCOMOTION -> native player/action state -> iCharacterBody
  -> FEATURE.INTERACTION_RAY -> native pick/CanInteract -> interaction state
  -> FEATURE.MENU_POINTER -> native paused menu cursor/click path
  -> controller pose -> FEATURE.PHYSICS_HANDS -> native PID force/torque
```

## Graphify Workflow

Generated graph files live in `graphify-out/` and are intentionally ignored.

```powershell
graphify update .
graphify query "How does an OpenXR pose reach SOMA's rendered view?"
graphify explain "Same-Frame Dual Render Boundary"
graphify affected "HPLCameraBridge"
graphify tree --graph graphify-out/graph.json --output graphify-out/GRAPH_TREE.html --root . --label SOMAVR
```

Run `graphify update .` after code or architecture-document changes. Use
`graphify path`, `query`, and `explain` before broad source searches, then use
`rg` for exact literals and final line-level confirmation.
