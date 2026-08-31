# Crash: F10 activation crashes on the first dual-render replay

Date: 2026-09-01
Build: `0.95.0-freshness-elbow+3c7c1e9-dirty/openxr/Release` (`build-openxr\Release\somavr.dll`)
Runtime: xr-sim x64 + xr-tape recorder layer
Config: `config/somavr.release.ini` with `ManualStart=0` (everything else shipping defaults)
Artefacts: `…\xrtape-run\logs\dumps\Soma_NoSteam-crash-94740-20260831T213849-1.dmp`,
`somavr-crash.log`, `logs\somavr.log`

## Summary

Pressing **F10** crashes the game. The crash is **not** in the activation itself — activation
succeeds — but in **the first dual-render replay that activation makes eligible**. The shipping
config has `HPLDualRenderContinuousDefault=1`, so this is reachable in the default configuration.

The fault is an **execute access violation on a virtual call inside the engine's own
`iEntity3D::SetVisible`**, reached during the replay pass.

## Causal chain, from the log

All within one second, `07:38:49`:

| Line | Event |
| --- | --- |
| 466 | `hpl_vr_mode requested key=F10 … policy=openxr_tracking_stereo_fullcenter` |
| 469-474 | `hpl_dual_render skipped attempt=1..6 reason=eligibility tracking=0 stereo=0` — the 8-frame calibration wait |
| 467 | `hpl_vr_mode calibration_wait reason=initial_tracked_pose stable=1/8` |
| 475 | `hpl_vr_mode activated key=F10 … tracking=1 fullyTracked=1 stablePoseFrames=8 stereo=1 projectionCentered=1` |
| — | `hpl_stereo fill_committed eye=0 source=dual_render_first_eye poseFrame=2221` |
| — | `hpl_stereo applied=2 eye=1 poseFrame=2221` |
| — | 8 × `hpl_occlusion_query begin=… pass=replay_eye sameFrameReuse=1 sameFrameReuses=1..8 targetConflict=0` |
| — | `hpl_tone_mapping_frame replay=1 eye=1` then `mismatch=1 differenceMask=0x40` |
| — | `hpl_dual_render replay=1 attempt=7 source=continuous result=same_pose_opposite_eye firstEye=0 secondEye=1` |
| — | **crash** |

Attempts 1-6 were skipped for eligibility while tracking was still stabilising. **Attempt 7 is the
first replay that actually executed, and it crashed.** Before F10 there was one dual-render log line;
after F10 there were six, ending in the crash.

## The fault, exactly

```
code=0xc0000005  access operation=execute  target=0x0000000000006576
rip=0x0000000000006576   rax=0x00007FF736EB3858   rcx=rbx=0x00000000E4359458
fault module=unresolved
stack_candidate[0] Soma_NoSteam.exe rva=0x2cb5b5
stack_candidate[6] somavr.dll       rva=0x59e39
```

`rva=0x2cb5b5` is a return address inside `HPL3_Entity3D_SetVisible` (entry `0x1402cb5a0`), which
Ghidra disassembles as:

```
1402cb5a6: MOV  RAX, qword ptr [RCX]        ; RAX = entity->vtable
1402cb5ac: MOV  byte ptr [RCX + 0x38], DL   ; write the visible flag
1402cb5af: CALL qword ptr [RAX + 0xd0]      ; virtual call   <-- faulted
1402cb5b5: …                                 ; the recorded return address
```

So the object's vtable slot `0xd0` contained `0x6576`, which is not code. `RAX` held
`SOMA_base + 0x693858`, i.e. a pointer that *looks* like it is inside SOMA's image, so this reads as
**a non-Entity3D object being treated as one**, not as a wholly wild pointer.

Note the ordering: `MOV byte ptr [RCX+0x38], DL` executes **before** the faulting call, so the
visible flag was already written into whatever that object is. The corruption precedes the crash.

`somavr.dll rva=0x59e39` on the stack is consistent with `HookMeshEntitySetVisible`
([HPLHandsBridge.cpp:1208](src/dll/HPLHandsBridge.cpp:1208)) passing through to
`g_originalSetVisible` — i.e. the game called `SetVisible`, our hook forwarded it, and the engine's
own virtual call then faulted.

## What this is *not*

Checked and ruled out rather than assumed:

- **Not the retained-hands wake.** `HPLHandsBridge` does originate a `g_originalSetVisible(retained.mesh, true)`
  at [:3787](src/dll/HPLHandsBridge.cpp:3787), guarded only by a `valid` flag and a null check, and
  that lane is documented to fire "as soon as VR tracking becomes eligible" — which made it the
  obvious suspect. **It did not fire.** No `hpl_hands_visibility wake=` line appears anywhere in the
  log. The guard on that path is still thin and worth hardening, but it is not this crash.
- **Not the activation path itself.** `hpl_vr_mode activated` logged cleanly with `tracking=1
  stereo=1 projectionCentered=1` before anything went wrong.
- **Not a signature/RVA mismatch.** Every bridge logged `install_ok`, and `SetVisible`'s own
  signature check passed at install.

## Why it matters beyond the crash

This is the **first real execution of the second world render** on this project, and it did not
survive one frame. That is directly relevant to
[NATIVE_STEREO_FEASIBILITY.md](NATIVE_STEREO_FEASIBILITY.md): the replay lane is the prototype of the
native-stereo second render, and the failure lands squarely in the hazard class that study named —
a second render reaching engine state that does not expect re-entry.

Two corroborating signals in the same frame, both from the 0.92 observation instrumentation:

- **`sameFrameReuse=1` on all 8 replay-pass occlusion queries.** This is the hazard the feasibility
  study called "the single highest-value thing to instrument", now observed live on the replay pass.
  `targetConflict=0`, so the queries are not fighting over targets, but the pooled IDs are being
  re-entered within a frame exactly as the apitrace baseline predicted.
- **`hpl_tone_mapping_frame mismatch=1 differenceMask=0x40`** immediately before the crash — replay
  state diverging from the first eye.

Neither is proven to *cause* the crash. Both are the predicted symptoms of the predicted problem,
appearing in the frame that died.

## Immediate mitigation

`HPLDualRenderContinuousDefault=0` in the config lets F10 activate tracking and stereo without
arming the replay. That should restore a usable VR mode while the replay path is investigated. Worth
considering for the shipping config until this is understood, since the current default arms a path
that crashes on first execution.

## Next diagnostic steps

1. **Reproduce with the replay disabled** to confirm F10 alone is safe and isolate the replay as the
   cause rather than a correlate.
2. **Identify the object.** `RCX = 0xE4359458` is the `this` that was treated as an `iEntity3D`.
   `read_memory` at that address in a live session, or the minidump, will show whether its first
   qword is a plausible vtable and what the object actually is. `regenny_rtti_typename` on it would
   name it outright if RTTI is present.
3. **Bisect the replay.** The replay re-enters the world render; the crash is in an entity
   visibility path. Narrowing which stage of the replay issues the `SetVisible` — the render list
   build, culling, or an entity callback — localises it.
4. **Re-run under xr-tape once it is stable.** The trace for this run was lost: the layer buffers and
   flushes on `xrDestroyInstance`, and a crash means no clean teardown, so nothing was written. Any
   crashing run produces no trace by construction. Worth reporting upstream as a limitation — a
   periodic `Flush()` would preserve evidence up to the fault.
