# Native Stereo on HPL3 — Precondition Study

Date: 2026-08-23
Status: **observation build ready; existing viewport replay unchanged; future world-only native stereo unimplemented.**
Prompted by playbook chapter 17 (native stereo) and FEAR-VR's LithTech precedent.

## 0.92 Evidence Update

The original study correctly named occlusion queries as the first unknown, but
the vanilla apitrace baseline now makes the risk concrete. HPL polls query IDs
`1..4` for availability/result and immediately reuses the same IDs for new
work. Across the trace there are `14,828` begin/end pairs. Version 0.92 therefore
tags query begin/end/result traffic by first/replay eye and reports same-frame
ID reuse, target conflicts, unmatched ends, and bounded-state overflow. It does
not alter results or query ownership.

A second shared resource is now proven. Gameplay performs partial
`glCopyTexSubImage2D` rectangles immediately before refractive translucent
draws, matching released HPL2's per-object refraction texture copy. The shader
samples copied scene color and scene depth while receiving all camera and
inverse-camera matrices through a UBO. Version 0.92 observes destination texture
reuse across first/replay eyes. A valid native-stereo transaction must render,
copy, and consume refraction scratch sequentially per eye; duplicating opaque
world work alone is insufficient.

## Question

Chapter 17's precondition 2: *is there an engine camera-build / world-render call that can be
invoked with a pose the mod supplies?* FEAR-VR answered this for LithTech by reading the official
SDK and citing line numbers. SOMAVR can answer it the same way, because HPL2 — HPL3's direct
ancestor — is open source, and because this project has **already located and hooked the HPL3
counterparts by RVA**.

## Answer: yes, and the shape is unusually favourable

### HPL2, from source

`cScene::Render(float afFrameTime, tFlag alFlags)` (`HPL2/core/sources/scene/Scene.cpp:180`)
increments the render frame count once, then for each visible viewport runs, in order:

| Order | Call | Notes |
| --- | --- | --- |
| 1 | `RunViewportCallbackMessage(eViewportMessage_OnPreWorldDraw)` | |
| 2 | **`pRenderer->Render(afFrameTime, pFrustum, world, settings, renderTarget, bPostEffects, callbackList)`** | the world render |
| 3 | `RunViewportCallbackMessage(eViewportMessage_OnPostWorldDraw)` | |
| 4 | `Render3DGui(viewport, pFrustum, afFrameTime)` | |
| 5 | `pPostEffectComposite->Render(afFrameTime, pFrustum, inputTexture, renderTarget)` | |
| 6 | `RenderScreenGui(viewport, afFrameTime)` | |

Two properties matter.

**The frustum is an explicit parameter of the world render**, not read from a global or from the
camera at call time. Rendering the other eye is a matter of passing a different `cFrustum*` — which
is the whole point of native stereo, and the thing matrix patching can never buy, because culling,
LOD, sky and fog all derive from that frustum *inside* the call.

**The world render contains no simulation.** `iRenderer::Render`
(`HPL2/core/sources/graphics/Renderer.cpp:469`) is four calls and nothing else:

```cpp
BeginRendering(afFrameTime, apFrustum, apWorld, apSettings, apRenderTarget, ...);
SetupRenderList();
RenderObjects();
EndRendering();
```

`iRenderer::Update(afTimeStep)` — which advances `mfTimeCount`, the renderer's own time accumulator
— is a **separate function that Render does not call**. World updates and the audio listener live in
`cScene::PostUpdate`, also separate. So chapter 17's side-effect gate is answered structurally
rather than empirically: at HPL2's level of the tree, a second world render advances no clock, steps
no simulation and reads no input.

### HPL3, from SOMAVR's own address registry

The chain survives into HPL3, and SOMAVR has already hooked every stage of it:

| HPL2 | HPL3 RVA | SOMAVR constant |
| --- | --- | --- |
| per-viewport body of `cScene::Render` | `0x298630` | `kRenderViewportRva` |
| `iRenderer::Render` | `0x1f9790` | `kRenderWorldRva` |
| `RunViewportCallbackMessage` | `0x297670` | `kRenderWorldCallbacksRva` |
| `cPostEffectComposite::Render` | `0x33bd80` | `kRenderPostEffectsRva` |
| `cScene::RenderScreenGui` | `0x2981e0` | `kRenderScreenGuiRva` |

The strongest single piece of evidence is the post-effect composite. Ghidra names the HPL3 function
at `0x33bd80`:

```
HPL3_PostEffectComposite_Render(void* composite, float frameTime, void* frustum,
                                void* inputTexture, void* renderTarget)
```

HPL2 calls `pPostEffectComposite->Render(afFrameTime, pFrustum, pInputTexture, pRenderTarget)` —
`this` plus four arguments, in that order. **Argument for argument, across a decade and an engine
generation.** The call graph is not merely similar; it is the same code, evolved.

SOMAVR's existing `HookRenderViewport(scene, viewport, frameTime, renderMask)` signature likewise
matches HPL2's per-viewport loop body.

## What this makes possible

Chapter 17's definition — render the world twice, everything else exactly once — maps onto the
existing hook set without inventing anything:

```
HookRenderViewport
  |- save camera/frustum
  |- set LEFT frustum  -> call RenderWorld -> capture eye 0
  |- set RIGHT frustum -> call RenderWorld -> capture eye 1
  |- restore camera/frustum
  \- let stages 3-6 (callbacks, 3D GUI, post effects, screen GUI) run ONCE
```

The seam is already hooked. What is missing is the second invocation and the per-eye capture, not
the reverse-engineering.

## What source cannot answer, and what SOMAVR already has for it

Source proves the *structure* carries no simulation. It cannot prove the *shipping HPL3 binary* has
no per-frame state that a second world render would double-advance. Two known hazards, both of which
this project has already built machinery for — a striking convergence, because that machinery was
built for AFR and native stereo needs the same ownership:

- **Temporal passes.** SSAO history, image trail, tone mapping and view history all carry
  frame-to-frame state. `HPLSSAOTemporalHistory`, `HPLToneMappingFrame`, `HPLPerEyeViewHistory` and
  `HPLPerEyePostEffect` exist precisely to give those per-eye ownership. Playbook 14's hazard atlas
  is the checklist.
- **Occlusion queries.** HPL2's `iRenderer` assigns and retrieves occlusion samples keyed by source
  pointer, and those span frames. apitrace proves immediate pooled-ID reuse; 0.92 now provides the
  first/replay observation surface. Whether the existing second render corrupts visibility remains
  a live observation question on the existing replay path.
- **Refraction scratch.** HPL copies object clip rectangles into shared scene-color texture storage
  immediately before translucent draws. Both the copy and the camera UBO must be eye-local within
  the sequential render transaction.

## The discipline this must be held to

Chapter 11 states it for FEAR-VR and it applies identically here: **HPL2 source is an oracle, not an
artifact.** FEAR-VR compiled the official LithTech SDK successfully and then refused to ship it,
because retail imports `MSVCP71`/`MSVCR71` while a v141 rebuild imports `MSVCP140`/UCRT, and the
engine exchanges C++/CRT objects across the module boundary — reproducible `0xC0000005`.

SOMAVR is not exposed to that specific failure (it never links HPL2), but the epistemic rule is the
same: HPL2 tells you *what to look for and what the arguments mean*. Every offset, enum order and
struct layout must be confirmed against `Soma_NoSteam.exe` before anything calls it. The
correspondence table above is already built that way — each HPL3 RVA carries its own byte signature
and fails closed on mismatch.

## Status of the preconditions

| Precondition | Status |
| --- | --- |
| A world-render call that accepts a supplied pose | **yes** — frustum is an explicit parameter, confirmed in HPL2 source and by the surviving HPL3 call graph |
| The seam is locatable in the shipping binary | **yes** — already hooked, signature-verified, five stages mapped |
| The second render advances no simulation | **structurally yes**; needs a live side-effect assertion before it is a claim |
| Temporal passes can be given per-eye ownership | **partly built already** (SSAO, tone mapping, image trail, view history) |
| Occlusion queries survive two renders per frame | **instrumented, live result pending** — vanilla ID recycling is proven; first/replay reuse is now logged |
| Refraction scratch survives two renders per frame | **instrumented, live result pending** — partial-copy and shared destination texture ownership are now logged |
| Frame budget allows two world renders | **unknown** — the question the per-eye GPU timestamp telemetry was built to answer, and it needs a headset |

## Bottom line

The reverse-engineering precondition that usually kills native stereo — *find a re-enterable world
render that takes a camera you control* — is **already satisfied and already hooked** on this
project. What remains is not RE work but a budget question and a temporal-ownership audit, and
neither can be settled from this machine.

The next concrete step is measurement, not more reading: use the existing
bounded existing second-render lane, assert the side-effect gate live (no
doubled sound events, particle ageing, AI ticks or input), preserve the new
query/refraction summaries, and read both HPL-stage and OpenXR-transfer GPU
timestamps. Promotion is prohibited until that evidence is clean.


---

# Addendum, 2026-08-22 — answering chapter 17's revised questions

Chapter 17 gained a second axis after this study was written. All three follow-up questions are
answerable from HPL2 source. Same oracle-not-artifact caveat applies: every claim below is HPL2
structure and must be confirmed against `Soma_NoSteam.exe` before anything calls it.

## Q1 — how does the camera reach the renderer? **Parameter. The good column.**

```
cScene::Render
  -> iRenderer::Render(afFrameTime, cFrustum *apFrustum, cWorld*, cRenderSettings*, ...)
       -> BeginRendering(afFrameTime, apFrustum, ...)
            -> InitAndResetRenderFunctions(apFrustum, ...)
                 -> mpCurrentFrustum = apFrustum;     // RenderFunctions.cpp:56
```

`mpCurrentFrustum` is a member of `iRenderFunctions`, but it is **assigned from the argument at the
top of every render call**, and everything downstream reads that member — frustum projection
(`SetFrustumProjection(mpCurrentFrustum)`), cull-mode inversion, the model-view matrix. There is no
persistent global camera the engine consults. Supply a different `cFrustum*`, get a different eye.

**Consequence: FarCry2-VR's entire shipped failure family is designed out, not defended against.**
R1's exact post-rebuild camera restore, R2's PRIMARY baseline/projection freeze, R4's pose-space
validator and the last-pair hold all exist because that project mutates shared camera state and must
then police every observer of it. None of it is required where the camera is an argument.

**The sharper consequence is about this project, and it is new.** SOMAVR's *current AFR path* is in
the wrong column: `ApplyStereoEye(frustum, ...)` mutates the live camera's frustum in place. That is
why `RestoreBaseView`, base-matrix latching and `pairRotationValid` exist — and why findings F-19 (a
transient fault clearing persistent stereo mode) and F-20 (a held pair carrying the wrong pose)
happened this week. Both are the FC2VR family, hit independently, from the same root cause:
borrowing shared state.

Native stereo would replace that with *construct a second frustum and pass it*. It does not merely
add correct per-eye culling — **it retires a defect class this project has already paid for twice.**
That is a stronger argument for rung 1 than the rendering-quality one, and it was not visible before
the parameter/global distinction was drawn.

## Q2 — per-frame fixed arenas: two benign, one open

Audited against the SS2VR failure mode — an arena sized for one pass, exhausted partway through the
second, presenting as *geometry quietly missing in one eye* and reading as a culling bug:

| Resource | Reset scope | Verdict |
| --- | --- | --- |
| `mpCurrentRenderList` | `Clear()` inside `BeginRendering`, gated on `abAtStartOfRendering` (`Renderer.cpp:636`) | **benign** — resets per render call, so each eye builds a fresh list |
| `mpBatchBuffer`, deferred light batching | created once in init, destroyed at shutdown, sized `mlMaxBatchLights = 100` (`RendererDeferred.cpp:196, 664-675`) | **benign for stereo** — it is a persistent fixed arena, but per-pass scratch, filled and flushed inside one render. Two renders reuse it cleanly, and a >100-light scene starves *both* eyes equally, which is a pre-existing cap rather than a stereo asymmetry |
| Occlusion queries | `WaitAndRetrieveAllOcclusionQueries()` (`Renderer.cpp:523`); assigned and retrieved keyed by source pointer, spanning frames | **open** |

The audit comes back mostly clean, and both halves of the acceptance gate — *what advances twice* and
*what runs out* — converge on the same suspect. **Occlusion queries are the single unvalidated
interaction**, and a failure there would present exactly as the SS2VR symptom: missing geometry in
one eye that looks like a culling bug. Instrument that before anything else in a two-eye experiment.

## Q3 — does `cCamera` expose an override the engine already honours? **No.**

`include/scene/Camera.h` has no AnvilNext-style nullable `worldMatrixOverride`. What exists is
`mbProjectionUpdated`-style dirty flags and `SetProjectionMatrix(iLowLevelGraphics*)`, which *pushes*
the computed matrix into GL state rather than accepting a caller-supplied one. There is no escape
hatch to adopt.

The question is moot in the good case, though. An engine-honoured override is what you hunt for when
the camera is a global you would otherwise have to fight; where the frustum is already a parameter,
passing a different one **is** the override.

## Revised precondition table

| Precondition | Status |
| --- | --- |
| Camera reaches the renderer as a parameter, not a mutated global | **yes** — `mpCurrentFrustum = apFrustum` per call; the borrow/restore/freeze family is not needed |
| Per-frame arenas survive two passes | **yes** for the render list and the light batch buffer |
| Occlusion queries survive two renders per frame | **instrumented, headset result pending** — vanilla immediate reuse is proven and 0.92 tags first/replay traffic |
| Refraction scratch survives two renders per frame | **instrumented, headset result pending** — partial copied rectangles and cross-eye destination texture reuse are logged |
| Frame budget allows two world renders | still the headset question |

## Note

None of this argues for removing AFR. It argues that native stereo, *if the budget allows it*,
retires a category of defect rather than trading one for another — and that the defects it retires
are ones this project has already been bitten by. Worth weighing when the budget number arrives.


## How to argue for this when it gets scheduled

Playbook `08-project-process.md` now carries a completeness ladder, T0 (flat in a headset) to T4
(the game's own systems adapted for VR), and places SOMAVR at **T3** — AFR stereo, native hands and
interaction, comfort options as first-class settings. It also separates three orthogonal axes:
**mode** (how you build it), **stereo rung** (how the second eye is produced), and **completeness
tier** (how much of the game becomes VR).

**Native stereo is entirely rung movement. It does not raise the tier at all.** Nothing in this
document makes more of the game into VR; hands, interaction, comfort and HUD are unchanged by it.
That is worth stating plainly, because the obvious pitch — "native stereo is more VR" — is false,
and a proposal resting on it deserves to lose to work that actually moves the tier.

The real case is the one the parameter/global finding exposed, and it is a **maintenance** argument:

- SOMAVR's AFR path borrows and mutates shared camera state, which is the column the whole
  FarCry2-VR defect family lives in.
- This project has already paid for that twice in one week — **F-19** (a transient fault clearing
  persistent stereo mode) and **F-20** (a held pair carrying the wrong pose) are both that family,
  found independently, same root cause.
- Because HPL passes the frustum as a parameter, rung 1 does not *defend against* that class, it
  *removes* it: construct a second frustum, pass it, own nothing shared.

So the honest framing when this is scheduled: it buys **correct per-eye culling, LOD, sky and fog**,
and it **retires a defect class already paid for twice** — at the cost of a second world render whose
budget is still unmeasured, and three shared-resource hazards (occlusion queries, refraction scratch,
temporal passes) of which only the third already has machinery. It is not a features item and should
not be scheduled as one.
