# Playbook handover - torso reference frames, private-FBO scissors, and arm deformation

**From:** SOMAVR (SOMA, HPL3, OpenGL + OpenXR, injected x64 DLL)  
**Date:** 2026-09-02  
**Build:** `0.95.5-torso-terminal-remap`  
**Audience:** VR Modding playbook main agent

This handover covers the latest headset diagnosis, HPL2 source comparison, FarCry2-VR rig comparison,
and the resulting SOMAVR implementation. It deliberately separates confirmed mechanisms from fixes
that are built and deterministic-tested but still await headset acceptance.

## 2026-09-03 correction: private-FBO remapping needs scale as well as origin

The next live run showed the terminal source viewport animating from
`520,615,256,145` through `0,893,2048,1155` while the private capture stayed
`0,0,1024,577`. Origin-only translation therefore produced clips as large as
`1843x924` in the smaller target and left email regions incomplete. The corrected
rule is: when an otherwise valid source clip misses a privately rebound target,
map both corners affinely from source viewport to capture viewport, using
floor(left/bottom) and ceil(right/top) so fractional conversion cannot discard
edge pixels. Restore the original scissor after the draw. Bypass remains only
the fail-soft path when validated mapping cannot intersect the target.

The same run exposed two reusable lifecycle lessons. First, a transient failure
in one optional live-patch lane must roll back that lane, not unrelated hooks;
otherwise one optics race can silently restore head bob, camera takeover, and
DoF together. Second, an absolute stereo replay that rejects a stale base is an
expected one-frame resync. It must not be promoted into the persistent
eye-sequence-fault latch; retain the staleness guard and retry the next pair.

The most important action is a **correction** to the existing SOMAVR scissor lesson in
`VR Modding/docs/10-graphics-apis.md`: bypassing every zero-intersection scissor is too broad. The
engine's clip can be valid in its cached source viewport and merely expressed in the wrong coordinate
space for a privately rebound capture FBO. Translate first; bypass only as a last-resort fallback.

---

## 1. A plausible yaw error can be a coordinate-frame error, not a tuning problem

**Suggested destinations:** `12-torso-calculations-and-ergonomics.md`, `failure-atlas.md`, and
`symptom-index.md`.

**Symptom.** The shoulder rig can begin nearly reversed, unwind slowly, tangle the arms, or appear to
have a bad follow threshold even though the HMD and native body headings are individually stable.

**Measured SOMAVR case.** At physical-body-follow activation, the value named
`camera.headWorldRotation` produced about `0.19 degrees` of yaw while SOMA's native camera/body heading
was about `115 degrees`. The old lane subtracted those values directly and started with a
`-114.73 degree` error.

The variable name was misleading: the HMD value was the **recentered tracking-space yaw delta**, not
an absolute world heading. The native camera value was in the game's world/body frame. Smoothing a
114-degree discrepancy only made the wrong answer move slowly.

**Correct composition.** Convert the relative HMD yaw into the native body frame before evaluating
the follow policy:

```text
worldHeadYaw = wrap(nativeBodyYaw + relativeHeadYaw)
yawError     = wrap(worldHeadYaw - torsoAnchorYaw)
```

SOMAVR now initializes the torso anchor from native yaw, composes through
`input_math::ComposeBodyFollowWorldYaw`, and logs all source values in one row:

```text
hpl_physical_body_follow anchor ...
  relativeHeadYawDegrees=...
  nativeYawDegrees=...
  worldHeadYawDegrees=...
  anchorYawDegrees=...
  policy=native_body_plus_tracking_space_head_yaw
```

**Cheap discriminator.** Before changing thresholds or filter constants, log both operands, their
declared coordinate spaces, and the first unfiltered residual. A near-zero tracking delta paired with
a large native heading is an immediate mixed-frame signature.

**General rule.** Names such as `worldRotation`, `cameraRotation`, or `absolutePose` are not evidence
of a coordinate frame. Establish the transform chain from behavior or construction. Filters cannot
repair subtraction between unlike spaces.

**Evidence:** root cause confirmed from a live headset log; corrected composition has deterministic
tests. **Headset acceptance of the corrected torso behavior is pending.**

---

## 2. A private GL FBO can disagree with the engine's cached viewport metadata

**Suggested destination:** replace/refine the stale-scissor paragraph in
`10-graphics-apis.md#own-state-in-a-global-machine-not-on-a-context-object`; add a symptom/failure-atlas
entry for partially flashing captured GUI content.

**Symptom.** A captured diegetic GUI has a stable opaque shell, but selected regions of its content
flash, tile, or remain black. The fault may change when the user looks toward the physical in-world
screen even though the OpenXR overlay itself remains solid.

**Cheap discriminator.** Give the overlay a known opaque background. If the background remains stable
while only source content changes, the composition layer did not disappear. Investigate the source
render/capture path before OpenXR alpha, layer ordering, or layer-budget policy.

**SOMAVR evidence.** The terminal capture target is `0,0,1024,577`, but HPL emitted many fully
offscreen scissors such as:

```text
(20,886,230,116)
(24,1797,446,224)
```

Released HPL2 source in:

- `AmnesiaAMachineForPigs/HPL2/core/sources/gui/GuiSet.cpp`
- `AmnesiaAMachineForPigs/HPL2/core/sources/impl/LowLevelGraphicsSDL.cpp`

shows that GUI clips are authored through the engine's current/cached render-target viewport. SOMAVR
binds its private FBO directly through GL, behind HPL's state cache. GL sees the new framebuffer and
viewport; HPL can continue emitting clips in the source framebuffer's coordinates.

The observed values prove a translation in at least one captured layout. For example, subtracting a
source viewport origin near `(256,768)` maps an offscreen clip beginning at `(532,886)` to the valid
capture-space position `(276,118)`.

**Correction to the existing playbook wording.** Do not immediately disable every scissor that has
zero intersection with the private target. That can recover pixels while destroying authored clipping,
which turns list panes, email regions, and dirty rectangles into tiles or flashes.

Use this order instead:

1. Save the original scissor and the engine/source viewport before rebinding the private target.
2. If the original scissor already intersects the capture viewport, leave it unchanged.
3. Otherwise translate its origin from source-viewport coordinates into capture coordinates:

```text
captureX = sourceScissorX - sourceViewportX + captureViewportX
captureY = sourceScissorY - sourceViewportY + captureViewportY
```

4. Apply the remapped clip only if the result intersects the capture target.
5. Restore the exact original scissor immediately after the draw.
6. Disable the scissor only as a logged fallback for a fully offscreen clip that cannot be mapped.

SOMAVR implements this in `terminal_math::RemapScissorToCaptureViewport` and emits bounded
`terminal_scissor_remap` telemetry with both viewports, the source clip, and the result. The fallback
still emits `terminal_scissor_bypass` so its use is visible.

**Scope caution.** The current evidence proves a translation, not a general scale transform. If source
and target extents differ, measure the engine's coordinate convention before adding scaling or axis
inversion.

**Evidence:** cause class is corroborated by HPL2 source and SOMA's clip values; implementation and
pure tests pass. **Headset acceptance of the remapped email panel is pending.**

---

## 3. Reaching the wrist target does not prove the arm mesh is solved

**Suggested destination:** extend the SOMAVR example in
`12-torso-calculations-and-ergonomics.md#efficient-two-bone-arm-ik`; cross-link the FarCry2-VR rig
mapping method from `02-viewmodels-and-hands.md`.

SOMA's released `hands_human.dae` and live named-node traversal identify a 34-node chain per side:

```text
Clavicle -> Shoulder -> Arm_1..5 -> Elbow_1..2 -> Arm_6..10 -> Wrist
          -> Thumb_1..3 and four finger chains of 1..4
```

The released skin weights show that the shirt is weighted through the clavicle, shoulder,
`Arm_1..7`, and both elbow nodes. The hand mesh overlaps around `Elbow_2`/`Arm_6..7` and continues
through `Arm_8..10`, wrist, and fingers.

The current prototype restores all 34 nodes but directly solves only three ownership points:

- `Arm_1` as the upper-arm/shoulder hinge;
- `Arm_6` as the elbow/forearm hinge;
- `Wrist` as the tracked endpoint.

That is enough for the wrist to reach the controller while the intermediate weighted nodes remain in
an unsuitable authored pose. The visible result is stretched, creased, or twisted skin despite a
numerically successful endpoint solve.

**FarCry2-VR supplies the transferable method, not portable indices.** Its player rig was mapped by
synthetic per-bone pose sweeps and showed that anatomical controls, twist bones, attachment helpers,
the camera bone, and the actually deforming range are different concepts. SOMA is easier in one way:
it exposes named hierarchical nodes and released skin weights. The FarCry2 indices and flat-array
composition rules do not transfer.

**Recommended rig-validation ladder:**

1. Enumerate the live hierarchy, names, parents, local transforms, and segment lengths.
2. Compare that hierarchy with released/source asset weights when available.
3. Perturb one candidate node at a time and observe pixels, not merely transform writes.
4. Identify whether the runtime consumes hierarchical locals, flat component-space matrices, or a
   later skinning palette.
5. Separate anatomical hinge ownership from distributed swing/twist and skin deformation.
6. Only then distribute the solved rotation across intermediate nodes, preserving authored rest bases
   and failing back to native animation when the chain does not match.

SOMAVR's optional `hpl_arm_hierarchy` census now records all 34 nodes per fresh seed, runtime parent
pointer/index, role, local/world position, and parent-segment length. It explicitly records
`releasedAssetWeightsKnown=1 runtimeSkinWeightsObserved=0` so static asset evidence is not mislabeled
as live skinning ownership.

**Terminology correction.** SOMAVR has mapped the complete **first-person arm and finger chain**. It
has not mapped a complete chest/spine/pelvis torso skeleton. Its current torso is an inferred body-yaw
and HMD-position shoulder model. Calling that a full-body or full-torso rig overstates the evidence.

**Evidence:** node names and asset weights are confirmed; runtime hierarchy telemetry is built.
Distributed twist ownership and visual acceptance remain open.

---

## 4. Use an OpenXR contract recorder as a discriminator, not a video recorder

**Suggested destination:** `06-debugging-methodology.md`, beside instrument-honesty and xr-tape scope.

`xr-tape` records the OpenXR contract: frame timing, located/submitted poses and projections, layer
sets, layer ordering, dimensions, and relevant extension structures. It does **not** record the pixels
inside a swapchain image or the game's skinned geometry.

For the terminal flash, xr-tape is still valuable as a negative discriminator:

- if the terminal layer remains present with stable pose, size, and order while recorded headset video
  shows content flashing, the fault is upstream in source rendering or capture;
- if the layer itself disappears or changes contract, investigate lifecycle, budget, or submission.

It cannot show which email rectangles flashed, whether a scissor deleted a draw, or how the arm mesh
deformed. Those require pixel/geometry evidence. SOMAVR's useful pairing is:

- headset video for what the user sees;
- `Ctrl+F10` for four sequential native/upscaled RGB/alpha terminal captures plus draw-state trace;
- the full SOMAVR log for scissor remap and hierarchy rows;
- xr-tape for the compositor-facing contract.

**General rule.** State an instrument's observation boundary in every test request. A green contract
trace can exonerate the compositor without proving the source image correct.

**Evidence:** confirmed from xr-tape's recorded schema and SOMAVR's capture architecture.

---

## Suggested symptom and failure-atlas entries

| Symptom | Cheapest separating test | Likely cause |
| --- | --- | --- |
| Shoulders begin reversed or unwind through a huge angle after VR activation | Log relative HMD yaw, native body yaw, composed yaw, and first residual | Tracking-space yaw delta subtracted directly from a world/body heading |
| Overlay shell is stable but selected GUI regions flash or tile | Force a stable opaque shell; log source viewport, target viewport, and each zero-intersection scissor | Engine emits cached source-target clips after the mod privately rebinds an FBO |
| Hands reach controllers but forearms stretch or crease | Pose-sweep intermediate bones and inspect released/live skin weights | Solver drives anatomical hinges but not the weighted twist/deform chain |
| xr-tape is green while a visual defect remains | Capture the source pixels and headset video | The fault is outside xr-tape's OpenXR contract boundary |

---

## Build and acceptance status

`0.95.5-torso-terminal-remap` passes:

- Release compile;
- all `9/9` CTest tests;
- xr-sim runtime negotiation, menu action, 600 layered frames, and 100% nonblack stereo;
- packaged doctor: `pass=9 warn=0 fail=0`.

Package: `D:\Dev Debug\SOMAVR\out\SOMAVR-latest`

```text
somavr.dll SHA-256
96F6D39513D7EC518B2D1B74F79952D1DABD90A0A813FD440EB27C5EB09AF409

archive SHA-256
7B4991AD82CF2F25B9F85A97B617369D98F49864DC2FC2C08A936FA494D6853D
```

The deterministic and simulated-runtime gates do not visually accept the two user-facing fixes.
Headset confirmation is still required for:

1. neutral shoulder initialization and physical-turn follow without reversal;
2. stable terminal email content while looking toward the physical laptop;
3. the hierarchy census matching the active hand-mesh variant before distributed twist work begins.

## SOMAVR source pointers

- Torso frame composition: `src/dll/HPLInputBridge.cpp`, `src/dll/HPLInputMath.{h,cpp}`
- Terminal clip mapping: `src/dll/HPLTerminalMath.{h,cpp}`, `src/dll/OpenGLHooks.{h,cpp}`,
  `src/dll/OpenXRGLBridge.cpp`
- Arm hierarchy census: `src/dll/HPLHandsBridge.cpp`
- Deterministic coverage: `tests/RenderMathTests.cpp`
- Current status and test protocol: `docs/CURRENT_STATE.md`, `docs/NEXT_LIVE_EVIDENCE.md`,
  `docs/TEST_CHECKLISTS.md`
