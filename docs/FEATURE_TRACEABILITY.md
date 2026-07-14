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
| `FEATURE.XR_BOOTSTRAP` | PROVEN | `OpenXRRuntime`, `OpenXRHelpers` | OpenXR loader, instance, system, session state | `CURRENT_STATE.md`, `UEVR_LEARNINGS.md` | Recover cleanly from runtime/session loss |
| `FEATURE.XR_GL_SUBMISSION` | PROVEN | `OpenXRRuntime`, `OpenXRGLBridge` | OpenGL swapchains, FBOs, `xrEndFrame` | `CURRENT_STATE.md` | Validate format/color-space and resize/recreation paths |
| `FEATURE.CLEAN_SHUTDOWN` | PROVEN | `HPLLifecycle`, `OpenXRRuntime` | `0x1403b16e0`, `0x1403b1803` | `RUNTIME_ANALYSIS_0.5.6.md`, `GHIDRA_SYNC.md` | Preserve clean exit across runtime/session-loss paths |
| `FEATURE.VR_MODE_CONTROL` | EXPERIMENTAL | `HPLCameraBridge`, `OpenXRRuntime` | F10 pending activation, F8/F11 diagnostics | `CURRENT_STATE.md`, `TEST_CHECKLISTS.md` | One F10 reaches tracking, stereo, and full centering from a loaded save |
| `FEATURE.RECENTER` | BUILT | `HPLCameraBridge`, `HPLCameraMath`, `HPLInputBridge` | F2 or two-grip hold, stable neutral-pose latch | `BUILD_HISTORY.md`, `CURRENT_STATE.md`, `TEST_CHECKLISTS.md` | Live test confirms keyboard and controller recenter without stereo/session reset or height drift |
| `FEATURE.XR_INPUT` | BUILT | `OpenXRInput`, `OpenXRRuntime` | OpenXR action set, Simple/Touch/Index/Motion bindings, grip/aim action spaces | `BUILD_HISTORY.md`, `CURRENT_STATE.md`, `TEST_CHECKLISTS.md` | Live log confirms active bindings, both tracked controllers, and stable predicted poses |
| `FEATURE.HEAD_TRACKING` | PROVEN | `HPLCameraBridge`, `HPLCameraMath` | `0x140271b80`, `0x140270230` | `CURRENT_STATE.md`, `VR_COMPATIBILITY_RE.md` | Remain correct through every authored camera state |
| `FEATURE.AFR_STEREO` | PROVEN | `HPLCameraBridge`, `OpenXRRuntime`, `OpenXRGLBridge` | F11, per-eye cache and submitted render pose | `BUILD_HISTORY.md`, `RUNTIME_ANALYSIS_0.5.1.md` | Preserve stability while shader/temporal compatibility is classified |
| `FEATURE.DUAL_RENDER` | RE_REQUIRED | `HPLCompatibilityProbe`, future native render bridge | `0x140298850`, `0x140298630`, `0x1401f9790` | `VR_COMPATIBILITY_RE.md` | Live stage/FBO telemetry proves a side-effect-safe per-eye boundary |
| `FEATURE.LOCOMOTION` | BUILT | `OpenXRInput`, `HPLInputBridge`, `HPLPlayerState` | SOMA input path, `0x1400cc860`, `0x140155050`, `0x140155090`, `0x140155290` | `FUTURE_SYSTEMS_RE.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Live-test movement/turn/interaction across normal and authored states, then replace emulation with mapped native actions |
| `FEATURE.AUTHORED_CAMERA` | EXPERIMENTAL | `HPLPlayerState`, `HPLInputBridge`, `HPLCameraBridge` | camera rotate mode `+0x6c`, body camera ownership `+0x1e8`, player/move state | `VR_COMPATIBILITY_RE.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Confirm transition detection/input suppression across sit, ladder, conversation, animation, and death states, then add pose-composition policy |
| `FEATURE.INTERACTION_RAY` | DESIGNED | future input/interaction bridge | `Utility_PickBasics`, native `CanInteract` | `VR_COMPATIBILITY_RE.md` | Controller ray selects same entities/ranges as native camera ray |
| `FEATURE.PHYSICS_HANDS` | DESIGNED | future pose bridge | native grab/rotate PID force and torque states | `VR_COMPATIBILITY_RE.md` | Stable grab, rotate, release, and throw with native collision |
| `FEATURE.VIEWMODEL` | RE_REQUIRED | future hands bridge | `PlayerHandsHandler`, `R_Hand`, tool `HudObject` | `FUTURE_SYSTEMS_RE.md` | Hands/tools follow controller poses without breaking animations |
| `FEATURE.HUD_LAYER` | RE_REQUIRED | `HPLCompatibilityProbe`, `OpenGLHooks`, future GUI capture bridge | `0x1402981e0`, `0x14022f8e0`, `0x140213970`, `0x1401297c0`, `XrCompositionLayerQuad` | `FUTURE_SYSTEMS_RE.md` | Use stage-tagged F6 and GUI GL-state telemetry to prove alpha/FBO behavior, then capture HUD/menu while world GUIs remain diegetic |
| `FEATURE.POST_EFFECT_POLICY` | EXPERIMENTAL | `HPLCompatibilityProbe`, `OpenGLHooks` | `0x14033b8f0`, `0x14033bd80`, effect vector `+0x340/+0x348`, history buffers | `FUTURE_SYSTEMS_RE.md`, `BUILD_HISTORY.md`, `RUNTIME_ANALYSIS_0.5.2.md` | Use Ctrl+F12 inventory/isolation to classify individual effects and define named comfort policy |
| `FEATURE.SHADOW_STABILITY` | PROVEN | `HPLCameraBridge`, `HPLCameraMath` | fully centered projection, programs `942/944` | `VR_COMPATIBILITY_RE.md`, `RUNTIME_ANALYSIS_0.5.6.md` | Regression-test additional levels and light types |
| `FEATURE.REFLECTION_STABILITY` | PROVEN | `HPLCameraBridge`, `HPLCameraMath` | fully centered projection, program `985` redirect | `VR_COMPATIBILITY_RE.md`, `RUNTIME_ANALYSIS_0.5.6.md` | Regression-test additional reflective materials and levels |
| `FEATURE.AUDIO_LISTENER` | EXPERIMENTAL | `HPLCompatibilityProbe`, `HPLCameraMath` | `0x140289340`, `0x14061d188`, `0x14048b2d4` | `VR_COMPATIBILITY_RE.md`, `RUNTIME_ANALYSIS_0.5.2.md` | Directional-source test confirms head-relative orientation without world-lock errors |
| `FEATURE.LOADING_VIDEO` | RE_REQUIRED | future presentation bridge | load-screen registrations, `CreateVideo`, `DestroyVideo` | `VR_COMPATIBILITY_RE.md` | Loading/menu/video remain stable while OpenXR pacing continues |
| `FEATURE.COMFORT_POLICY` | EXPERIMENTAL | `HPLCameraBridge`, state adapter, post policy, input | authored roll/bob/shake/FOV, vertical roomscale, eye-height offset, turn modes | both future RE documents | Live-test eye-height/vertical policy, then add standing/seated profiles and authored-motion controls |
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
FEATURE.HUD_LAYER requires FEATURE.DUAL_RENDER
FEATURE.AUTHORED_CAMERA requires FEATURE.HEAD_TRACKING
FEATURE.LOCOMOTION requires FEATURE.AUTHORED_CAMERA
FEATURE.INTERACTION_RAY requires FEATURE.AUTHORED_CAMERA
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
