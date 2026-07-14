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

OpenXRRuntime
  -> OpenXRHelpers
  -> OpenXRInput
  -> OpenXRGLBridge

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
| `OpenXRRuntime` | Instance/system/session state, frame pacing, view snapshots, layer submission | HPL camera transforms or gameplay input semantics |
| `OpenXRInput` | OpenXR action set, suggested bindings, action synchronization, grip/aim spaces, immutable input snapshots | SOMA movement, interaction, hand placement, or camera policy |
| `OpenXRHelpers` | OpenXR names, format strings, pose/view conversion | Handles, session lifetime, swapchain ownership |
| `OpenXRGLBridge` | OpenGL swapchain images, FBOs, eye caches, backbuffer transfer | OpenXR event/session policy |
| `HPLCameraBridge` | Signature-guarded player-camera interception and VR mode state | Generic quaternion/projection algorithms |
| `HPLCameraMath` | Pure pose, matrix, FOV centering, projection construction | HPL pointers, hotkeys, logging, OpenXR handles |
| `HPLPlayerState` | Signature-guarded player/camera/body discovery, player/move IDs, camera ownership classification, immutable snapshots | Controller injection, camera transforms, OpenXR actions |
| `HPLInputBridge` | Reversible SOMA input-path controls and authored-camera suppression policy | Native player discovery, OpenXR action ownership, camera math |
| `HPLCompatibilityProbe` | Bounded render/audio/post-effect telemetry and temporary probes; shared pose math comes from `HPLCameraMath` | Permanent feature policy unrelated to a probe |
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

The maintenance passes now include five focused extractions:

- `HPLCameraMath` owns quaternion/matrix operations, OpenXR projection creation,
  and the fully centered FOV policy proven by the `0.5.7` runtime result.
- `OpenGLMatrixAnalysis` owns matrix classification and telemetry formatting.
- `OpenXRHelpers` owns OpenXR enum/format names and view/pose conversions.
- `OpenXRInput` owns controller actions and predicted grip/aim acquisition while
  `OpenXRRuntime` remains the lifecycle and frame-submission facade.
- `HPLPlayerState` owns native player discovery and camera-ownership snapshots,
  allowing locomotion, hands, interaction, and comfort adapters to share one
  guarded source instead of duplicating executable offsets.

`somavr_render_math_tests` now protects symmetric tangent-span preservation,
zero projection offsets, projection construction, pose/matrix basics, and OpenGL
projection classification in both build flavors.

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
