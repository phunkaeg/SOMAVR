# Native Stereo on HPL3 — Precondition Study

Date: 2026-08-22
Status: **offline evidence only.** No binary was modified, nothing was run in-game.
Prompted by playbook chapter 17 (native stereo) and FEAR-VR's LithTech precedent.

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
  pointer, and those span frames. Two world renders per frame against one query set is an
  unvalidated interaction and should be the first thing instrumented.

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
| Occlusion queries survive two renders per frame | **unknown** — first thing to instrument |
| Frame budget allows two world renders | **unknown** — the question the per-eye GPU timestamp telemetry was built to answer, and it needs a headset |

## Bottom line

The reverse-engineering precondition that usually kills native stereo — *find a re-enterable world
render that takes a camera you control* — is **already satisfied and already hooked** on this
project. What remains is not RE work but a budget question and a temporal-ownership audit, and
neither can be settled from this machine.

The next concrete step is measurement, not more reading: put one experimental second `RenderWorld`
call behind a default-off config gate, assert the side-effect gate live (no doubled sound events,
particle ageing, AI ticks or input), and read the per-eye GPU timestamps that already exist.
