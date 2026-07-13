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

HPLCameraBridge
  -> HPLCameraMath
  -> OpenXRRuntime pose/view snapshots

OpenXRRuntime
  -> OpenXRHelpers
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
| `OpenXRHelpers` | OpenXR names, format strings, pose/view conversion | Handles, session lifetime, swapchain ownership |
| `OpenXRGLBridge` | OpenGL swapchain images, FBOs, eye caches, backbuffer transfer | OpenXR event/session policy |
| `HPLCameraBridge` | Signature-guarded player-camera interception and VR mode state | Generic quaternion/projection algorithms |
| `HPLCameraMath` | Pure pose, matrix, FOV centering, projection construction | HPL pointers, hotkeys, logging, OpenXR handles |
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

The first maintenance pass made three behavior-preserving extractions:

- `HPLCameraMath` owns quaternion/matrix operations, OpenXR projection creation,
  and the fully centered FOV policy proven by the `0.5.7` runtime result.
- `OpenGLMatrixAnalysis` owns matrix classification and telemetry formatting.
- `OpenXRHelpers` owns OpenXR enum/format names and view/pose conversions.

`somavr_render_math_tests` now protects symmetric tangent-span preservation,
zero projection offsets, projection construction, pose/matrix basics, and OpenGL
projection classification in both build flavors.

## Next Structural Splits

These are ordered by value and runtime risk:

1. Extract F3/F6/F7/F9 shader, UBO, and matrix capture state from
   `OpenGLHooks.cpp` into `OpenGLDiagnostics` behind a narrow event API.
2. Extract MinHook/WGL extension registration into `OpenGLHookRegistry`, leaving
   `OpenGLHooks` as frame and callback dispatch.
3. After the `0.5.8-onekey` live test, split `OpenXRRuntime::Impl` into bootstrap,
   session lifecycle, and frame-composition owners while preserving one public
   facade and one lock policy.
4. Split authored-camera state policy from the HPL frustum adapter when the first
   state telemetry is live; avoid inventing that boundary before the native owner
   is observed.

Each split should compile and test independently. Runtime-sensitive splits also
retain the previous DLL until a live log confirms equivalent behavior.
