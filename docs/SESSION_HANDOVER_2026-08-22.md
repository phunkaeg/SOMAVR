# Session Handover — external review session, 2026-08-22

**To:** the main SOMAVR agent
**From:** a separate review session (not the main agent, no prior context on this project)
**Repo state at handover:** `main` @ `c8781a5`, version `0.91.0-review-hardening`
**Build state:** full Release build clean, `ctest -C Release` **7/7**, build manifest SHA-256 matches
the produced `somavr.dll`

You do not need to read the originating conversation. Everything actionable is here or in the two
documents this points at.

---

## 1. What this session was

A code review of the repo against `D:\dev debug\VR Modding\` (the VR playbook), which then turned
into acting on the findings one at a time as the user directed. It produced **20 numbered findings**
and 16 commits.

The full finding-by-finding detail lives in
**[docs/REVIEW_HANDOVER_2026-08-07.md](REVIEW_HANDOVER_2026-08-07.md)** — that is the reference
document, kept current. This file is the session-level summary: what changed, what to trust, what is
still open, and what to do next.

### Important: the repo moved underneath this session, repeatedly

Another session was committing to this repo throughout. Several findings were **already fixed by
that session** before this one reached them — F-01, F-02, F-03, F-05, F-07, F-09, F-15 and F-17 were
all found already-done and were *verified* rather than implemented. Two consequences:

- Commit messages here that say "verified" mean **read against the working tree**, not taken from
  another commit's message. Where verification failed, it is recorded as such.
- The baseline line in the review doc (`ed2e7d8` plus 57 modified files) describes a tree state that
  no longer exists. Treat the status table at the top of that document as authoritative, not the
  baseline header.

**Five files are still uncommitted and were deliberately never touched by this session** — they are
the other session's in-flight work: `AGENTS.md`, `CLAUDE.md`, `docs/CURRENT_STATE.md`,
`docs/HPL_OPENGL_NOTES.md`, `docs/future-hook-map.md`. Also untracked and left alone:
`AGENTS.md.bak-remcp-20260721-124446` (a dated backup; deleting it is the user's call).

---

## 2. Code that changed, and why it matters

Seven behaviour-relevant changes landed. All compile and pass the unit suite; **none is
headset-verified** (see §5).

| Commit | Change | Why it matters |
| --- | --- | --- |
| `5426687` | `runtime_paths` / `config_applied` emit at `Warn`, and `config_applied` carries the config file's mtime + size | `SetLevel(config.logLevel)` ran *before* the identity write, so `Level=warn` silently dropped the one line proving which config was read. A proof the subject can switch off is not a proof |
| `c18e4c8` | `DLL_PROCESS_DETACH` claims the stop event via `InterlockedExchangePointer` | The worker's exit path already claimed it atomically; detach did a plain read-then-`SetEvent`, so it could signal a handle the worker had just closed — and a closed handle value can be **recycled**, making that a signal to an unrelated kernel object |
| `22fc4c9` | `ScopedPeerThreadSuspension` lifted into `LiveCodePatch.h`; `HPLComfortBridge` now uses it | `HPLGrabBridge` suspended peer threads and checked every RIP against the patch range; `HPLComfortBridge` did a bare `memcpy` for **eight** 12-byte writes over live code |
| `4d66185` | Patch helpers take a named-field struct with `std::span` buffers | Eight positional args, five of them same-shaped pointers. Also makes the three buffer lengths one fact instead of three that must agree |
| `16c9e85` | A failed `ApplyStereoEye` costs one frame, not the session | One bad frame permanently dropped the player to mono, recoverable only by F11 — and the explicit F11 path performed *the same four assignments*, so nothing could tell a fault from a user decision |
| `0abb5e9` | The stereo hold submits the poses the held images were rendered from | The hold already existed; it submitted **stale imagery against the current head pose**, which zeroes the reprojection delta and makes the world stick to your face |
| `beb1f09` | F-04 last probe off, F-12 ownership scope, F-13 truncation detection, F-16 GL transaction guard, F-18 decode `static_assert`s | See the review doc |

### The two most likely to show up in a headset session

**`16c9e85` and `0abb5e9`** both touch the stereo path. If stereo behaves differently from the last
build you tested, they are the first suspects — in a good way, but verify rather than assume. New log
lines to watch: `hpl_stereo apply_failed consecutive=N` / `apply_recovered`, and
`openxr_stereo_hold active|released|exhausted` plus
`openxrStereoHoldEpisodes/Frames/Exhausted` in the summary.

---

## 3. Where the findings stand

Full table in the review doc. Summary:

- **Resolved and verified:** F-01, F-02, F-03, F-04, F-05, F-07, F-08, F-09, F-11, F-12, F-13
  (partly), F-15, F-16, F-17, F-18, F-19, F-20
- **Deliberately open**, each because it needs a measurement this machine cannot take:
  - **F-06** — the single `mutex_` over the XR frame loop. Bounded timeouts are done
    (`kSwapchainWaitTimeout`, 50 ms), so the whole-process hang is closed. Splitting the lock so the
    game thread cannot block behind `xrWaitFrame` touches every runtime accessor and changes lock
    ordering on the most timing-sensitive path in the project. Do not do this blind.
  - **F-10** — `ReadField`/`WriteField` in `HPLCameraBridge` still `memcpy` game memory unvalidated,
    unlike the `ReadProcessMemory` helpers used everywhere else. The fix has a real cost: per-frame,
    render thread, syscall per field. Choosing between `ReadProcessMemory`, an SEH POD helper, or a
    one-time `VirtualQuery` of the camera object needs a frametime number.
  - **F-14** — synchronous `glReadPixels` in the depth probe. Bounded, and now off in the shipping
    config. Kept for its diagnostic value; treat it as a covariate in any frametime measurement.
  - Remainder of **F-13** — the 2.4 KB single-line `hook_config` dump. Truncation now announces
    itself, so the trap is gone; splitting it is churn without a correctness argument.

---

## 4. Calibration: three claims in this session were wrong

Stated plainly because it affects how much weight to give the rest.

1. **The F-05 "drop order" residual was wrong.** I claimed `appendLayer` dropped layers by arrival
   order while the priority sort only reordered survivors. It does priority *eviction* — I read only
   the first branch and never looked at the `else`. Corrected in `bc03578`.
2. **The "trim the `0xcc` padding" recommendation was wrong and would have been damaging.** Those
   bytes are load-bearing: `sizeof(signature)` also sizes the patch save/restore buffer and is passed
   as `size` to `InstallAbsoluteJumpPatch`, which begins `if (… size < 12) return false`. Trimming
   would have failed the patch and taken the **entire comfort bridge** down on every machine — all
   comfort suppression, reporting `reason=depth_of_field_patch`. The `0xcc` bytes verify that the
   12-byte jump's spill region is alignment padding and safe to overwrite. `kAddImpulseThunkSignature`
   is the same convention (9-byte thunk + exactly 3 padding = 12). Reversed in `a05395f`.
3. **"SOMAVR clears both eyes to black on transient stereo loss" was wrong.** It already held the
   pair. The real defect was the pose it held it with. Corrected in place and in `0abb5e9`.

**All three came from the same mistake: concluding how a value is used after reading only one of its
uses.** The habit that catches it is cheap — `grep` every use of the identifier before asserting
anything about it. It is what turned up F-11 being half-fixed in a file I had not named, and what
stopped mistake 2 before it became a commit.

Corollary for you: the findings marked **fixed** were built and tested; the findings marked
**verified** were read. Neither category was run in a headset.

---

## 5. What needs a headset, and exactly what to read

Nothing in this session was validated in-game. The unit suite does not exercise injection, code
patching, or the XR frame loop.

Run order when you next have the hardware:

1. **Confirm build and config identity first.** `runtime_paths root= source=` and
   `config_applied path= mtime= bytes= parsedKeyHash= accepted= unknownKeys= unknownSections=` now
   survive any log level. If these disagree with what you think you deployed, stop there.
2. **Comfort bridge.** Success is silence; failure is
   `hpl_comfort_bridge patch_failed name=<which> reason=<why> thread=<id>` — the reason is now
   specific enough to act on. `restore_failed … action=leave_patch_owned` at shutdown is a *safe*
   outcome, not a crash.
3. **Stereo.** `hpl_stereo apply_failed consecutive=N` should be rare and self-recovering; if it
   reaches 8 the mode suspends as before. `openxr_stereo_hold` episodes should be short.
4. **Layer budget.** `openxr_layer_budget candidates= submitted= capacity= runtimeMax= dropped…` —
   if `runtimeMax` is below 13 on any runtime, the eviction path is live and worth reading.
5. **F-17's A/B**, which is the one measurement with a decision attached. Protocol is in
   `docs/OPENXR_GL_TRANSFER_RE.md`. **Compare `copyCpu`, not `projectionUs`** — the headline number
   brackets `xrAcquireSwapchainImage`/`xrWaitSwapchainImage`, so it mixes app-side transfer cost with
   compositor pacing, and a runtime that merely holds images longer will move it. Same caveat applies
   to the `budget_pressure` alarm, which thresholds on that wait-inclusive value and may cry wolf
   under a streaming runtime like VirtualDesktopXR.

---

## 6. The forward-looking result: native stereo is not blocked by RE

**[docs/NATIVE_STEREO_FEASIBILITY.md](NATIVE_STEREO_FEASIBILITY.md)** — written this session, and the
most consequential thing in it.

Playbook chapter 17's precondition — *is there an engine world-render call you can invoke with a pose
you supply?* — is **satisfied, and the seam is already hooked by this project**.

- `cScene::Render` passes the frustum to `iRenderer::Render` as an **explicit parameter**. That is
  what buys correct per-eye culling, LOD, sky and fog.
- `iRenderer::Render` is four calls and contains no simulation. `iRenderer::Update()`, which advances
  the renderer's time accumulator, is a **separate function Render does not call**. World updates and
  the audio listener are in `cScene::PostUpdate`.
- All five stages of that chain already exist as signature-verified HPL3 RVAs in
  `HPLCompatibilityProbe.cpp`.
- The clinching evidence: Ghidra names `0x33bd80` as
  `HPL3_PostEffectComposite_Render(composite, frameTime, frustum, inputTexture, renderTarget)`, and
  HPL2 calls `pPostEffectComposite->Render(afFrameTime, pFrustum, pInputTexture, pRenderTarget)` —
  `this` plus four arguments, same order, across an engine generation.

What remains is **not reverse-engineering**. It is a frame-budget question and a temporal-ownership
audit. The temporal machinery largely exists already (`HPLSSAOTemporalHistory`, `HPLToneMappingFrame`,
`HPLPerEyeViewHistory`, `HPLPerEyePostEffect`) because AFR needed the same thing. The unvalidated
interaction is **occlusion queries**: HPL2 keys them by source pointer and they span frames, so two
world renders against one query set is untested. Instrument that first.

Treat HPL2 source as an **oracle, not an artifact** — it tells you what to look for and what the
arguments mean; every offset must still be confirmed against `Soma_NoSteam.exe`.

---

## 7. Cross-project material now referenced

Three external projects were read this session and are cited in the docs. All were read for
architecture only; **no source was copied from any of them.**

| Project | Path | What it gave |
| --- | --- | --- |
| TheDarkModVR | `D:\Dev Debug\thedarkmodvr\` | The `WGL_NV_DX_interop2` route, and four in-source workaround comments proving runtimes clobber GL state around swapchain calls (F-16) |
| FEAR-VR (MIT) | `D:\Dev Debug\Other VR Mods\fear-vr\` | The one-bit-two-jobs stereo ownership bug, still unfixed upstream (F-19) |
| FarCry2-VR | `D:\Dev Debug\FarCry2-vr\reference\FC2VR_...\` | The corrective for it (NOP at RVA `0x00008B48`), last-pair-hold, and pose-space vs raw-matrix validation |

Two ideas from those projects are recorded but **not implemented**, both cheap and both worth taking:

- **Validate camera writes in pose space, not raw matrix space.** FarCry2-VR found a fixed-threshold
  16-coefficient View comparison gave *yaw-dependent* false rejects, because world translation
  amplifies rotation rounding at large map coordinates. SOMAVR does no camera readback verification
  at all today — chapter 06 says it should, and this is a precise warning about how to build it wrong.
- **Tag failure records with world position/heading, and give the skip counter a reason histogram.**
  Those two diagnostics are what let FarCry2-VR root-cause from a single run.

Also still open and unchanged: TheDarkModVR's D3D11 interop route. It is parked behind F-17's
measurement and **compounds with native stereo rather than competing** — if two world renders fit the
budget, submission cost matters more, not less.

---

## 8. Environment notes

- **Ghidra is running** with project `SS2_VR`. This session opened `/SOMA/Soma_NoSteam.exe`
  (version 75, image base `0x140000000`, 15,822 functions) **read-only with `auto_analyze=false`** —
  nothing was modified, but it is now open in the CodeBrowser alongside the Prey/Swat4/SS2 programs.
- **RenderDoc remains unusable on SOMA** — confirmed, not assumed: no `.rdc` files exist anywhere in
  the tree and `CLAUDE.md` records the GL-version incompatibility. A peer session suggested
  `pixel_history` for the reflection artifact; it cannot run here. apitrace is the documented
  substitute.
- No remote is configured on this repo, so nothing was pushed.
