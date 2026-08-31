# xr-tape — first run against SOMAVR

Date: 2026-08-31
Result: **18 passed, 0 failed, 2 skipped** on `somavr_xrsim_smoke.exe`, x64 OpenGL, under xr-sim.
Trace: `%LOCALAPPDATA%\xr-tape\somavr_xrsim_smoke\trace-…-20260831-000055.ndjson`

## Setup note that cost a run

The first attempt failed with `xrCreateInstance failed: -4` and no trace. xr-tape is an **API
layer**, not a runtime — it needs a runtime underneath it. Set the runtime first, then invoke:

```powershell
$env:XR_RUNTIME_JSON = "$env:LOCALAPPDATA\xr-sim\runtime\xrsim-x64.json"
& 'D:\Dev Debug\xr-tape\tools\Invoke-XrTape.ps1' -Executable <exe> -Check
```

The guard behaved correctly — it threw rather than reporting a pass on an empty trace.

## The two SKIPs are the whole story

The 18 passes confirm the toolchain works on SOMAVR's binding. They do not advance coverage,
because they are all contract checks the smoke probe was already satisfying. **The two skipped
checks are precisely the two that matter to this project:**

| Skipped | Why it skipped | Why it matters here |
| --- | --- | --- |
| `layer_budget` | "system max layer count unknown" — the smoke probe never calls `xrGetSystemProperties` | This is **F-05** as an executable check. The layer *does* hook `xrGetSystemProperties`; the real mod calls it (`maxLayerCount_ = max(properties.graphicsProperties.maxLayerCount, 1u)`), so the real mod would populate it |
| `depth_submission_consistent` | "no depth layers submitted" | SOMAVR is the only fleet project submitting `XrCompositionLayerDepthInfoKHR`, gated by `DepthCompositionSubmit=1`. The smoke probe submits none |

So the same conclusion the xr-sim assessment reached still holds, one layer up: **a green run on
`somavr_xrsim_smoke` is an environment gate, not a regression gate.** The probe links none of
SOMAVR's code. What changes with xr-tape is that the regression gate now *exists* — it just needs
pointing at the real mod rather than the probe.

## Predicted false positive: `submitted_pose_matches_located` will fire on the hold path

Recorded before the run that produces it, so it is a prediction and not a rationalisation.

`check_submitted_pose_matches_located` compares each submitted view pose against
`frame.views["views"]` — **the views located in that same frame** — with `pose_tolerance` defaulting
to `1e-5`, i.e. effectively exact equality.

SOMAVR's F-20 fix deliberately violates that on hold frames. When a frame cannot refresh the eye
images, it re-submits the previous pair carrying **the poses those images were rendered from**,
because a compositor reprojects by the difference between the pose a layer declares and the pose at
display time. Declaring the *current* pose for a stale image zeroes that difference, suppresses
reprojection, and makes the world stick to the head — which is the bug F-20 fixed.

So on a hold frame the submitted pose is legitimately an older located pose, and the delta is
however far the head moved: orders of magnitude above `1e-5`.

The check is right for the ordinary case and its stated intent — *FAIL-XR-011, "the pose that
reached the matrix is not the one located"* — is about catching a **mangled** pose. A held pose is
not mangled, it is older. Suggested amendment that keeps the intent and removes the false positive:
match the submitted pose against the set of poses located in **this frame or any earlier frame**,
and fail only when it matches none — i.e. when the value was never located at all.

Note `eye_pair_shares_display_time` is *not* affected: it compares the locate and submit display
times within a frame, and SOMAVR passes `frameState.predictedDisplayTime` to both.

## Next run

Everything needed is present: `Soma_NoSteam.exe`, and `somavr.dll` + `somavr_injector.exe` in
`build-openxr\Release`. The chain works because the injector passes `nullptr` for `CreateProcessW`'s
environment, so the game inherits both `XR_RUNTIME_JSON` and the layer selection:

```powershell
$env:XR_RUNTIME_JSON = "$env:LOCALAPPDATA\xr-sim\runtime\xrsim-x64.json"
& 'D:\Dev Debug\xr-tape\tools\Invoke-XrTape.ps1' `
    -Executable 'D:\Dev Debug\SOMAVR\build-openxr\Release\somavr_injector.exe' `
    -ArgumentList '--launch','G:\SteamLibrary\steamapps\common\SOMA\Soma_NoSteam.exe' -Check
```

One config change first: the shipping config has `ManualStart=1`, so OpenXR never starts without the
F10/F8 hotkey and an unattended run records nothing. Stage a test config with `ManualStart=0`, or
drive activation through xr-sim's scripted input.

Expected from that run, none of which any existing test reaches: `layer_budget` populated (F-05),
`depth_submission_consistent` exercised, `never_submits_zero_layers` against the real submit path
(F-02), and — via xr-sim's `hazard` / `instanceloss` controls — F-19 and F-20 across a transient
fault.


---

# CORRECTION --- crashing runs DO produce traces

**An earlier revision of this document stated that a crashing run "produces no trace by
construction", because the layer flushes only on `xrDestroyInstance`. That is wrong, and the
recommendation to report it upstream as a limitation was based on a false premise.**

The layer flushes explicitly every 256 records (`if ((recorded % 256) == 0) g_trace.Flush();` in
`layer/xrtape_layer.cpp`), on top of a 64 KiB fully-buffered stdio stream. The periodic flush that
was recommended as a fix already existed.

Measured, after the claim had been repeated across three sessions:

| run | pid | outcome | trace |
| --- | --- | --- | ---: |
| 1 | 103996 | clean exit | 23.9 MB |
| 2 | 94740 | **crashed** | 2.4 MB |
| 3 | 17992 | **crashed** | 2.4 MB |
| 4 | 52752 | **crashed** | 2.2 MB |
| 5 | 11316 | stereo active, killed | 5.3 MB |

Both crashed runs whose traces were declared lost had written 2.4 MB each. What a crashed run
actually loses is the `footer` record and at most the records since the last flush.

The error was reading [xr-tape's SCHEMA.md](../../xr-tape/docs/SCHEMA.md), which correctly says the
*footer* is written from `xrDestroyInstance` and that "a process that was killed never gets there",
and generalising "it" from the footer to the whole trace. The directory was never listed. Upstream
has been amended to state the distinction explicitly.

**Rule: list the trace directory before reporting that a run produced nothing.**
