# xr-sim Integration Assessment

Date: 2026-08-31
Status: **assessed and measured, not integrated.** Nothing in the SOMAVR tree was modified.
Subject: `D:\Dev Debug\Xr-sim` (the standalone, matured runtime) vs `SOMAVR\tools\xrsim\runtime`
(a vendored snapshot currently uncommitted in this repo).

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

## 2. The vendored copy is a stale fork, and the divergence matters in one place

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

Most of that is backend breadth SOMAVR does not need: `xrsim_graphics.cpp` is the multi-binding
layer (D3D9/10/11/12, Vulkan, headless) and the private `XR_XRSIM_d3d9_enable` /
`XR_XRSIM_d3d10_enable` extensions. Irrelevant here — SOMAVR is x64 OpenGL only.

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

## 5. Blocker before any re-sync — read this first

**The vendored runtime is uncommitted work belonging to another session.** At time of writing:

```
 M CMakeLists.txt        (+53 lines, adds somavr_xrsim64 and somavr_xrsim_smoke)
 M src/dll/HPLCameraBridge.cpp
 M AGENTS.md, CLAUDE.md
?? tools/                (entire directory untracked — includes tools/xrsim/)
```

All dated 2026-08-30, uncommitted as of 2026-08-31. Overwriting `tools/xrsim/runtime/` with a sync
from `Xr-sim\src\` would silently discard whatever that session changed in it, and the divergence
table above shows the local copy is not simply an older snapshot — `xrsim_actions.cpp` and
`xrsim_session.cpp` are *larger* locally, so there is local work in there that does not exist
upstream.

**Reconcile before re-syncing.** The right sequence is: commit or stash the in-flight work, diff
`tools/xrsim/runtime` against `Xr-sim/src` properly, decide whether the local-only additions should
go upstream into the shared project, and only then adopt. A blind copy loses work in both
directions.

## Recommendation

1. **Do route 1 regardless** — lift the layer-budget and hold decisions into `somavr_render_math`
   and unit-test them. No dependency on this decision, closes the real regression gap.
2. **Re-sync the vendored runtime only after reconciling** the uncommitted `tools/` work, and treat
   depth-layer support as the reason rather than freshness for its own sake.
3. **Prefer consuming the shared project over vendoring** long-term, so this divergence does not
   recur — but that is a CMake change to a file another session currently has modified, so it is not
   a change to make unilaterally.
