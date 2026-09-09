# PureDark AFW: SOMAVR and Fleet Assessment

Date: 2026-09-07

Scope: source inspection, graph-led prior-art lookup, published release notes,
and existing SOMAVR receipts. No game launch, DLL change, or performance test.
Static implementation evidence, author claims, and SOMA runtime observations
are distinguished below. This document is suitable for fleet/playbook harvest.

## Decision

Useful architecture, not a drop-in SOMAVR feature. AFW renders one real eye per
engine frame and reconstructs the other, submitting both on that frame. The real
eye alternates on the next frame. This can reduce scene-render work compared
with two real eye renders, but reconstructed stereo is not equivalent to native
same-frame stereo and needs its own quality/performance acceptance.

Keep SOMAVR's current same-frame viewport replay as the reference and AFR as the
existing fallback. First prove scene-depth content and motion/history contracts;
only then price an optional reconstruction path. AFW cannot fix incorrect torso
coordinates, wrist calibration, GUI clipping, or differing arm animation inputs.

September 9 follow-up: `HPL_SCENE_DEPTH_PROOF_2026-09-09.md` closes scene-depth
existence/encoding in the existing baseline capture. Raw scene D24 and sampled
R16F linear depth agree across complete images, while the final default
framebuffer is all-far, including an ordinary gameplay frame. The current mod
copy still needs to move to a validated pre-post scene source and prove per-eye
ownership. This does not yet make AFW ready to implement or enable.

## Source Identity and Limits

- Local repository: `D:/Dev Debug/Other VR Mods/PureDark/UEVR`.
- Checked-out `master`: `74b76bc9428a906cbdc69de3ebc1905fd0e9cc57`.
  Searching this working tree alone misses AFW.
- Inspected `origin/AFW`: `fc8de1026622d42d0d1f76c3e9c8bb0d80b11035`,
  dated 2026-08-20. Used `git show` without changing the checkout.
- Published [beta.6 release](https://github.com/PureDark/UEVR/releases/tag/UEVR_AFW_v1.0-beta.6)
  is separate evidence. The local release DLL is not proven to match that later
  AFW source revision. The packaged nightly `PDAFWPlugin.dll` is x64;
  SHA-256 `5F3CFC38903AC438241A3C47CB4BC3AC329CB39B056E33A63C20045FE15336F9`.
- The AFW tree contains the plugin header and an eight-line dummy implementation,
  not the actual warp implementation. `InitDevice` returns null and
  `EvaluateFrameWarp` does nothing in the dummy. The functioning implementation
  is supplied in `PDAFWPlugin.dll`. Its exact reconstruction/hole-fill algorithm
  was not inspected here.
- The inspected repository `LICENSE` says "All rights reserved." Do not treat
  the fork or separate binary as an MIT code donor; establish applicable reuse
  and redistribution terms before copying or packaging it.

Immutable source anchors:

- [Plugin contract](https://github.com/PureDark/UEVR/blob/fc8de1026622d42d0d1f76c3e9c8bb0d80b11035/dependencies/pd-afwmod/include/PDAFWPlugin.h)
- [D3D12 eye capture, warp, and submission](https://github.com/PureDark/UEVR/blob/fc8de1026622d42d0d1f76c3e9c8bb0d80b11035/src/mods/vr/D3D12Component.cpp#L437)
- [NGX depth/motion extraction and correction](https://github.com/PureDark/UEVR/blob/fc8de1026622d42d0d1f76c3e9c8bb0d80b11035/src/mods/VR.cpp#L55)
- [Runtime eligibility](https://github.com/PureDark/UEVR/blob/fc8de1026622d42d0d1f76c3e9c8bb0d80b11035/src/mods/VR.hpp#L509)

## What the Integration Proves

`D3D12Component.cpp:517` packages current-eye colour, depth and motion vectors,
camera matrices, motion-vector convention, and warp mode into
`FrameWarpEvaluateParams`. Later copies place the real and reconstructed eyes
into their respective XR images. The end-frame condition includes AFW on every
frame, instead of waiting only for the second real AFR eye.

The plugin interface distinguishes alternate-eye, previous-frame, and combined
warping. It carries source/destination view and projection matrices and inverses,
plus separate same-eye/opposite-eye history for motion correction. Mode names
and inputs do not reveal the closed implementation's exact blending policy.

The working integration is D3D12-specific. `DeviceParams` also has D3D11 fields,
but its texture interface uses `ID3D12Resource` and UEVR eligibility checks DX12.
Those D3D11 fields do not establish a usable D3D11 or OpenGL backend. The release
instructions require DLSS/DLAA for the supported motion-data path; their AMD
workaround uses OptiScaler to expose that path. Author-reported gains are not
fleet measurements or a prediction for SOMA.

## Highest-Value Transferable Lessons

1. **Treat velocity as an explicit coordinate/time contract.** The NGX hook
   captures colour, depth, velocity, output dimensions, velocity scale, and
   jitter. It scales vectors for differing input/output resolutions. The API
   distinguishes current-to-previous same-eye, opposite-eye, and object-only
   vectors. A texture named "velocity" is not sufficient evidence.
2. **Separate camera motion from object motion.** The optional UE correction
   consumes current and previous depth/velocity and camera history. It applies
   an engine-specific object-motion factor of 2.0; that number is not a general
   AFR constant to copy into HPL3. The author's debug control is valuable:
   camera movement through a static scene must not classify the whole world as
   moving objects; an independently moving object is the positive control.
3. **Generate both target eyes from one current camera/pose basis.** Preserve
   the exact matrices that produced each retained image. Reconstructing with a
   newly sampled pose mislabeled as the source does not repair stale-eye shear.
4. **Keep UI out of world reconstruction.** The contract supports a separate
   UI input and hudless colour. The inspected UEVR call passes hudless colour
   and no UI texture. SOMAVR already has separate overlay paths; preserve that
   ownership instead of depth-warping text and reticles as world surfaces.
5. **Invalidate history on discontinuities.** AFW has warmup and resolution/
   switching cooldowns. Carry resource generation, frame/eye identity, and
   validity with colour/depth/velocity. A 90-frame delay is not itself proof of
   safe resource lifetime or correct history after a camera cut.
6. **Attachment cadence matters.** `UObjectHook.cpp` updates AFW attachments
   each render frame; its "Ultra Responsive" option disables attachment lerp.
   That is not a universal prediction improvement. SOMAVR must preserve one
   coherent arm solution across its two renders, not run IK twice because this
   fork ticks attachments more often than ordinary AFR.

Additional caution: `VR.cpp:146` replaces DLSS evaluation with a colour blit in
its RenderDoc mode and returns success. A capture made with that path is not an
unchanged DLSS/AFW performance baseline. Its D3D12 capture support also does not
resolve SOMA's documented legacy-OpenGL RenderDoc capture restriction.

## SOMAVR Prerequisite Found During This Audit

Current baseline is `0.95.9-player-space-recovery`, with same-frame replay and
AFR fallback, not an AFR-only implementation. Existing eye caches, pose/FOV
receipts, per-eye history and transfer timers are useful foundations.

However, depth format/copy success is not scene-depth content proof:

- Archived test log:
  `logs/receipts/2026-09-07-0.95.8/logs/somavr.log`.
- All **188 logged centre-depth samples** report `valid=1`, finite values,
  `centerMin=1.0000000`, and `centerMax=1.0000000`.
- `src/dll/OpenXRGLBridge.cpp:761` binds read framebuffer zero; the depth blit
  and subsequent 4x4 centre sample use that default framebuffer. Validity is
  based on depth bits and GL success, not evidence of visible geometry depth.
- This is a small centre sample, **not a census of the complete depth texture**.
  It does not prove the entire buffer is clear. It does leave usable scene
  depth unproven, consistent with sampling a postprocessed/cleared surface.
- HPL scene-depth consumers already exist as leads: the apitrace notes identify
  `aSceneDepth` at binding 7 in a translucent shader. Program/binding identities
  are capture-local. See `HPL_OPENGL_NOTES.md`, not a hardcoded program number.

Hypothesis S22 in `HYPOTHESES.md` established the projection/depth convention,
not this missing content proof. SOMA uses normal OpenGL depth: clip NDC -1..1,
stored depth 0..1. PureDark's reverse-Z conversion cannot be copied unchanged.

No object-motion-vector producer was verified in the reviewed HPL surface.
`a_mtxTemporalProjection` in the shader pack is temporal camera data, not proof
of a velocity texture. This is a bounded search result, not a claim that no
such producer exists anywhere in the engine.

## Fleet Applicability

| Target group | Useful next step | Why not a drop-in PDAFW integration |
| --- | --- | --- |
| SOMAVR, OpenGL x64 | Prove live scene depth; evaluate a GL reconstruction experiment only after that | No inspected GL plugin backend or NGX/object-velocity producer |
| D3D11 renderers, such as SS2VR and PreyVR | Price a source-visible D3D11 reprojection prototype against native stereo | D3D11 is not the inspected AFW D3D12 resource contract |
| BioShockVR and other x86 targets | Reuse math/diagnostic principles, evaluate existing paths first | Packaged PDAFW is x64; bitness and graphics ownership both matter |
| D3D9/D3D10 fleet renderers | Check existing interop and depth evidence before adding another graphics API | An XR D3D11 presentation bridge does not supply D3D12 resources or engine velocity |

Do not describe AFW as an undiscovered cure for BioShockVR: its current notes
already distinguish native per-eye rendering from a frozen centre-depth
reprojection experiment. Any revisit must address that experiment's rejection
criteria, not merely adopt a new name.

TheDarkModVR-style GL-to-D3D11 presentation sharing is a separate optimization
question. It does not by itself make SOMAVR compatible with PureDark AFW.

## Source-Visible Companion: SnowRunner-VR

`D:/Dev Debug/Other VR Mods/Snowrunner-VR`, inspected clean HEAD
`34454eb383c40c630422f9cd3dea6ad063f9da82`, supplies MIT-licensed D3D11 code.
Its `src/render/reproject.hpp` and `.cpp` expose actual depth reprojection and
compute shaders. The implementation reprojects the retained eye using its own
depth and rendered matrices, and can contribute that image to disocclusion
filling alongside fresh-eye depth-image-based reconstruction.

This is a better inspectable code reference for D3D11 fleet work, and a possible
algorithm reference for a separately written OpenGL path. It explicitly does
not solve genuinely moving objects without velocity. Its renderer-derived
rigid-object mask also improves on guessing object ownership from a near-depth
band. A vehicle/cockpit mask is not an automatic solution for articulated VR arms.

## Proposed Next Work, Not Implemented

1. **Depth content probe first.** Observe the actual HPL scene-depth producer
   before postprocessing, retain source FBO/attachment/size/format and frame/eye
   labels, and sample both source and copied cache over multiple image regions.
   Control: a stationary visible wall/object at a known approximate distance.
   Variable: camera translation/rotation. Pass only if linearized depths and
   geometry boundaries agree with the colour and the rendered camera.
2. **Static-world reconstruction offline.** Use a paired colour/depth/matrix
   capture from our two-real-eye reference. Synthesize one eye and compare with
   the held-out real eye. Measure disocclusion holes, silhouette error, and GPU
   time. Keep overlays separate. Do not start by replacing the shipping path.
3. **Dynamic acceptance separately.** Add independent moving props and tracked
   arms, then test history reset at snap turns, saves, menus and resize. Without
   object velocity, explicitly retain the moving-object limitation. Compare
   total frame pacing and native-render savings, not just warp dispatch cost.

The initial source search and archived-log audit need no game launch. New live
depth captures and GPU cost measurements require an authorized controlled run;
headset acceptance remains necessary for reconstruction artifacts. xr-tape can
verify submission timing/poses, not the correctness of synthesized image pixels.
