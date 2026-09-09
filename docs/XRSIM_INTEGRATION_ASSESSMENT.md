# xr-sim Integration Assessment

Date: 2026-09-03
Status: **assessed and measured; shared-runtime transport passes, shared action-edge regression identified.**
Subject: `D:\Dev Debug\Xr-sim` (the standalone, matured runtime) vs `SOMAVR\tools\xrsim\runtime`
(a vendored snapshot currently uncommitted in this repo).

## 2026-09-03 Headless Retest

Build `0.95.7-arm-pass-evidence` was checked without launching SOMA. The
standalone SOMAVR OpenGL smoke client ran for 600 frames through shared xr-sim
commit `9155410` and xr-tape commit `52d3fac`. The trace checker reported
`18 passed, 0 failed, 2 skipped`: exact Wait/Begin/End counts, monotonic display
time, no zero-layer submits, two submitted eyes, plausible 63 mm IPD, no
vertical disparity, parallel eyes, matching located/submitted pose and FOV,
distinct eye subimages, and one display time per pair. Layer-budget discovery
and depth consistency were not applicable to this generic one-projection-layer
client. The trace is under
`logs/headless-validation/0.95.7/traces/`.

The shared runtime does have one independently isolated action regression. Its
control channel acknowledged both `btn menu down/up`, but SOMAVR's smoke client
reported `menu-edge: no`. Shared `xrsim_actions.cpp` currently assigns
`XR_FALSE` to `XrActionStateBoolean::changedSinceLastSync` unconditionally.
SOMAVR's vendored simulator passed the identical 600-frame client with
`menu-edge: yes`, two nonblack stereo captures at 100%, and zero runtime errors.
Treat a failed menu-edge assertion against shared xr-sim commit `9155410` as a
simulator limitation, not a SOMAVR input regression, until that edge state is
implemented upstream.

## Short answer

Useful, and the value is specific rather than general: **SOMAVR submits depth composition layers,
and the vendored copy of xr-sim in this repo cannot see them.** That one gap is the case for
re-syncing.

What it does *not* currently buy is validation of the OpenXR fixes made in the 0.91 review
hardening pass — see [§3](#3-what-xr-sim-does-not-yet-validate), which is the more useful finding.

## 1. It works, measured not assumed

Ran the shared runtime's own catalog harness against the fleet:

```
.\tools\test-catalog.ps1 -External -CatalogRoot 'D:\Dev Debug'
```

```
PASS somavr: x64/opengl                     <- contract test
PASS somavr: external client probe          <- SOMAVR's own smoke exe, against shared xr-sim
Catalog validation passed for 7 profile(s).
runtime: xr-sim (version 1.0.0)
system: Meta Quest 3 (max layers 16, max swapchain 16384x16384)
XR_KHR_opengl_enable  v12
```

SOMAVR already has a catalog profile (`id: somavr`, `openxrBackend: opengl`,
`status: client-probe-verified`) pointing at `SOMAVR\build\Release\somavr_xrsim_smoke.exe --frames 90`,
and it passes against the shared runtime unmodified. So adoption is not blocked by anything.

Worth noting `max layers 16` — that is exactly the capacity F-05's `appendLayer` budget clamps to,
so the simulated system is representative for that path rather than permissive.

## 2. The vendored copy is a specialised subset (see §5 for why the line counts mislead)

`SOMAVR\tools\xrsim\runtime\` and `Xr-sim\src\` are the same lineage — identical file names, same
overall size — but have diverged:

| File | local | shared | changed lines |
| --- | ---: | ---: | ---: |
| `xrsim_compositor.cpp` | 597 | 966 | **1563** |
| `xrsim_instance.cpp` | 661 | 935 | 334 |
| `xrsim_actions.cpp` | 707 | 645 | 160 |
| `xrsim_session.cpp` | 745 | 694 | 137 |
| `xrsim_common.h` | 269 | 284 | 107 |
| `xrsim_graphics.cpp` | **absent** | 533 | — |
| others | — | — | 2-70 each |

Two files being *larger* locally looks like local work upstream lacks. It is not — see
[§5](#5-correction-the-divergence-is-a-refactor-not-lost-features). `xrsim_graphics.cpp` is the
multi-binding layer (D3D9/10/11/12, Vulkan, headless) plus the private `XR_XRSIM_d3d9_enable` /
`XR_XRSIM_d3d10_enable` extensions, and it is also where the GL code that the fork inlines into
`xrsim_session.cpp` now lives.

**The part that is not irrelevant is depth.** Counting depth references in the compositor:

| | local vendored | shared |
| --- | ---: | ---: |
| `depth` | **0** | 12 |
| `capture` | 15 | 21 |
| `subImage` | 4 | 7 |

SOMAVR submits `XrCompositionLayerDepthInfoKHR` — seven references in `OpenXRRuntime.cpp`, gated by
`DepthCompositionSubmit`, with its own `depthSubmittedFrameCount_` and `depthSubmissionFailures_`
counters. **The vendored runtime has no depth handling at all**, so it cannot observe, validate, or
fail that path. Any confidence drawn from it about depth submission is unfounded.

That is the concrete case for re-syncing, and it is narrow: not "the shared one is newer" but "the
shared one can see a layer type we actually submit."

## 3. What xr-sim does *not* yet validate

This is the more important finding, and it is easy to get wrong.

`somavr_xrsim_smoke` links `openxr_loader`, `opengl32`, `gdi32`, `user32` — **and none of SOMAVR's
own code.** It is a generic OpenGL OpenXR session probe. It proves a 64-bit GL session can be
created and pumped on this machine.

It does **not** exercise `OpenXRRuntime::SubmitFrameLocked`, which means none of the 0.91 hardening
is covered by it:

| Finding | Covered by the current probe? |
| --- | --- |
| F-02 never submit zero layers | no |
| F-05 layer budget and priority eviction | no |
| F-19 transient fault must not clear stereo mode | no |
| F-20 held pair must carry its own poses | no |

A green catalog run is an *environment* gate, not a regression gate. Reading it as the latter is
exactly the playbook-06 trap of announcing an outcome from something adjacent to it.

## 4. What would actually close the gap

xr-sim's control surface (`docs/CONTROL.md`) exposes the vocabulary those findings need:

| Control | Would exercise |
| --- | --- |
| `state`, `focus`, `idle` | the focus-pacing decision (`openxr_frame_pacing_math::Decide`) and F-06's skip-until-focused path |
| `instanceloss` | runtime recovery — and specifically whether `heldPair_` is correctly invalidated across it (F-20) |
| `compose`, `committed` | submitted layer counts and composition, i.e. F-02's never-zero invariant and F-05's eviction order |
| `hazard`, `hazards` | fault injection against the copy/acquire failure paths |
| `capture`, `shot` | per-eye image capture for the hold path |

The missing piece is a probe that **links SOMAVR's own submit path** rather than a bare loader
session. Two routes, in order of preference:

1. **Extract the decision logic into pure math and unit-test it**, following the pattern the project
   already uses. `openxr_frame_pacing_math` is precisely this, is already covered in
   `RenderMathTests.cpp`, and needed no runtime at all. The layer-budget `appendLayer` lambda and the
   `holdCandidate`/`holdActive` decision are both currently inline in `OpenXRRuntime.cpp` and are
   pure functions of their inputs. Lifting them into `somavr_render_math` would make F-02, F-05 and
   F-20 unit-testable **today, with no xr-sim dependency at all.** Cheapest and most durable.
2. **Then** a probe linking that logic against a live xr-sim session, for the parts that genuinely
   need a runtime — session-state transitions, instance loss, and depth-layer acceptance.

Route 1 is the higher-value half and does not depend on this integration decision.

## 5. Correction: the divergence is a refactor, not lost features

**An earlier revision of this document warned that the divergence ran "both ways" — that
`xrsim_actions.cpp` and `xrsim_session.cpp` being larger locally meant local work existed that
upstream lacked, and that a blind copy would "lose work in both directions." That was inferred from
line counts alone and is wrong.** Checked properly, the size inversion is an artifact of an upstream
refactor.

The local fork **inlines the OpenGL binding into the session layer**, with file-scope global state:

```cpp
HDC   g_deviceContext = nullptr;      // xrsim_session.cpp, local only
HGLRC g_glContext     = nullptr;
// ... XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR handled inline in xrCreateSession
compositor_init(g_deviceContext, g_glContext);
```

Upstream extracted exactly that code into `xrsim_graphics.cpp`, where the same binding is handled
**per session** rather than globally, alongside five others:

```cpp
// Xr-sim/src/xrsim_graphics.cpp:241
case XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR: {
    if (!b->hDC || !b->hGLRC) return XR_ERROR_GRAPHICS_DEVICE_INVALID;
    s.glDC = b->hDC;
    s.glRC = b->hGLRC;
```

So `xrsim_session.cpp` is *smaller* upstream because the GL code moved *out* of it, and
`xrsim_graphics.cpp` (533 lines, absent locally) is where it went. The local copy is a **specialised
subset**, not a superset. Per-session state is also strictly better than the fork's globals.

The same holds for the actions file: `control_bool`, `kTouch`, `actions_reset_session` and
`VC_TRIGGER_L` are all present upstream. Exactly one symbol is genuinely local-only —
`subaction_hand`, which resolves a subaction path to a hand index at query time. Upstream does not
need it because its binding table carries the hand index in the table itself (the third column of
each `BindingEntry`), resolving at binding time instead. A design difference, not a dropped feature.

**Net: there is nothing in the fork worth upstreaming, and nothing lost by adopting upstream.**

## 6. The remaining blocker is process, not technical

```
 M CMakeLists.txt        (+53 lines, adds somavr_xrsim64 and somavr_xrsim_smoke)
 M src/dll/HPLCameraBridge.cpp
 M AGENTS.md, CLAUDE.md
?? tools/                (entire directory untracked — includes tools/xrsim/)
```

All dated 2026-08-30 and still uncommitted as of 2026-08-31. This is another session's work in
flight. The technical risk of re-syncing is now known to be low, but the *process* risk is
unchanged: the files are uncommitted, so any change to them cannot be committed without dragging
that session's work into the commit. Commit or stash it first.

## Recommendation

1. **Adopt upstream and retire the fork.** The fork is a GL-specialised subset with global state
   where upstream has per-session state; it holds nothing worth preserving. SOMAVR's own smoke probe
   already passes against the shared runtime unmodified, so adoption is proven, not projected.
   Depth-layer support is the concrete gain.
2. **Prefer consuming `D:\Dev Debug\Xr-sim` over re-vendoring**, so this divergence cannot recur.
   That is a CMake change to a file another session currently has modified — commit or stash that
   work first.
3. **Do the `somavr_render_math` extraction regardless.** Lifting the layer-budget and hold
   decisions into pure math and unit-testing them closes the actual regression gap, needs no xr-sim
   at all, and is independent of every choice above.
