# HPL Scene Depth: Existing-Trace Proof

Date: 2026-09-09. Original evidence pass made no DLL/config changes or game launch.
Feature: `FEATURE.XR_DEPTH_CAPABILITY`.

## Implementation Follow-up: 0.96.0

The subsequent authorized build implements guarded scene-depth acquisition in
`OpenXRSceneDepth.cpp`, at the existing exact-player RenderWorldOverlays callback
entry before post-processing. It queries the live depth attachment rather than
hardcoding the object IDs below. Single-sample renderbuffers, complete FBOs,
matching formats, full-size viewport and standard depth range are required.
Color/depth are paired by render serial, pose frame and viewport; each eye carries
its own near/far. Missing or stale evidence retains color-only submission.

Ctrl+F10 now saves float PFM depth and JSON metadata alongside each matching RGB
eye capture. PFM is bottom-up, little endian; this differs from apitrace's
top-down state-image encoding used by the historical analyzer below. Readback
restores framebuffer, renderbuffer, scissor, PBO, pixel-pack and depth-transfer
state. No full-image readback is performed outside requested captures.

Release build and 10 CTests pass, including a real hidden-window GL test of the
production bridge with separate eye values, shared source overwrite, stale
serial rejection and hostile pack state. No SOMA launch or native-callback
acceptance yet. The test package enables the probe, not depth submission; follow
`NEXT_LIVE_EVIDENCE.md`. No AFW or reconstruction was added.

The rest of this document records the pre-implementation 0.95.9 findings.

## Decision

**HPL's scene render target contains useful geometry depth. The final desktop
framebuffer does not, in both sampled frames.** The current OpenXR copy reads
the latter. Successful allocation/blitting is not scene-content validation.

This closes the existence/encoding question for the recorded HPL GL pipeline,
not the current mod's per-eye acquisition or AFW feasibility. Do not enable a
warp based on the current `depthCacheValid` flag.

## Identity And Scope

- Existing trace: `D:/Dev Debug/apitrace/traces/soma-gl-c3c7442f2aed`, apitrace
  14, 519137611 bytes, 3874 frames. The file has no `.trace` extension, which
  caused the MCP inventory to omit it; direct file analysis works.
- Trace SHA256:
  `d127184baa13a1c9b74caea2a2c75b7c3b9641da43baf8b2b6a66ab01bcd02ea`.
- Trace metadata names `G:/SteamLibrary/steamapps/common/SOMA/Soma_NoSteam.exe`.
  Earlier documentation calls this the vanilla baseline. The trace does not
  establish today's executable hash or exclude every third-party hook.
- Static cross-check: Ghidra `/SOMA/Soma_NoSteam.exe`, x64 Visual Studio ABI,
  image base `0x140000000`.
- Current executable SHA256:
  `395cd54830c8e66e22166e71ac6fe95fd3bb5433b898737836744a6d178a0a6a`.
  This identifies the static target only, not the historical capture.
- Headless `glretrace.exe` replayed the existing file. A running Prey process
  was observed and left alone. No performance measurement or headset claim.

All numeric GL object/program names below are **capture-local**. Do not turn
FBO `11`, texture `33`, or a shader program number into runtime constants.

## Producer To Consumer

| Captured resource | Allocation / attachment evidence | Meaning |
|---|---|---|
| Renderbuffer 1 | call 5863: `GL_DEPTH24_STENCIL8`, 1920x1080 | Raw normalized scene depth and stencil |
| FBO 1 | calls 5878/5879 attach renderbuffer 1 to depth/stencil; G-buffer colors 25/27/29 | Opaque geometry producer |
| FBO 11 | calls 6011/6014/6015: color texture 31 (`GL_RGBA16F`) plus the same renderbuffer 1 | Lit scene / translucent accumulation with shared geometry depth |
| FBO 13 / texture 33 | calls 6053/6069: `GL_R16F`, 1920x1080, **color** attachment 0 | Sampled linear scene depth, not a raw depth attachment |

In frame 2113, FBO 1 is bound at 962641 and cleared at 962643/962644, then
receives world triangle/instanced draws. FBO 13 is bound at 963811; FBO 11 at
964065. At 964261, a refractive draw uses `aSceneDepth=7` with texture 33 and
compares sampled depth against `px_fLinearDepth`. `GL_DEPTH_TEST=TRUE`,
`GL_DEPTH_FUNC=LEQUAL`, but `GL_DEPTH_WRITEMASK=FALSE`: transparency consumes
opaque depth without necessarily adding its own depth.

Post-processing starts after depth testing is disabled at 964296. FBO 0 is
bound at 964496 for the final image / 2D work, then SwapBuffers at 964591.
The scene depth is not propagated to that final default framebuffer.

## Full-Image Controls

Each image contains **2073600 pixels**, all finite. Values are raw GL depth in
`[0,1]`. Complete states and previews are under
`logs/depth-proof-2026-09-09/`; scalar receipts are in each `analysis/*-summary.json`.

| Frame / state call | Surface | Min / max | Exact 1.0 pixels | Interpretation |
|---|---|---|---:|---|
| 2113 / 964261 | Scene FBO 11 | 0.550925314 / 0.989324093 | 0 | Apartment + medicine hand geometry |
| 2113 / 964590 | Desktop FBO 0 | 1 / 1 | 2073600 | Loading overlay visible; insufficient alone as gameplay control |
| 2114 / 966241 | Scene FBO 11 | 0.550918519 / 0.989324093 | 0 | Adjacent sample; same camera, not an independent movement control |
| 3000 / 2832882 | Scene FBO 11 | 0.941231668 / 1 | 3261 | Ordinary gameplay, desk/window view |
| 3000 / 2833206 | Desktop FBO 0 | 1 / 1 | 2073600 | Ordinary gameplay image with interaction icon; no loading overlay |

Frame ranges: 2113 = 961980..964591; 2114 = 964592..966569;
3000 = 2830709..2833207. State 2833206 is immediately before the final swap.

Between the two separated scene samples, inverse captured views give camera
positions `(-10.856877,1.894098,7.837855)` and
`(-9.995338,1.866910,1.234991)`: 6.658889 world units apart, with approximately
15.5524 degrees rotation difference. This is not merely two reads of one static
image. It is not an isolated WASD/yaw experiment either.

### Encoding Check

Captured shader uniforms give `near=0.03`, `far=1000`, normal GL depth. Projection
column arrays contain Z terms `-1.00006`, `-1`, `-0.0600018`; rounding the matrix
alone makes far-plane recovery less accurate than the explicit uniform.

For raw depth `d`, reconstruct positive camera distance:

```text
z = near * far / (far - (far - near) * d)
linear scene texture = z / far
```

Every pixel in both separated scene samples agrees with the independently
sampled R16F texture to **less than 0.2% relative error**. Maximum error is
0.099152% at 964261 and 0.107807% at 2832882. The latter has identical 3261
far-plane pixels in both representations. Centre reconstructed distances are
2.753179 and 2.699751 world units respectively.

Color/depth previews were visually checked for matching scene silhouettes.
The transparent medicine bottle body does not contribute matching surface
depth; the opaque cap and hand do. A single opaque depth layer cannot recover
transparent/reflected/disoccluded surfaces for AFW.

## What Changes The Implementation Route

`src/dll/OpenXRGLBridge.cpp:761` explicitly binds READ framebuffer 0 before the
color and depth copies. The depth branch at 779 validates bits and GL errors,
not visible geometry. This explains why the archived mod log had 188 successful
4x4 probes with min/max both 1.0. Replay corroborates the source-selection fault;
it does not prove a new source already works in modded stereo.

Ghidra `HPL3_Scene_RenderViewport` (`0x140298630`) orders world rendering,
stage-1 callbacks, `HPL3_Scene_RenderWorldOverlays` (`0x140297670`), optional
post-composite, then screen GUI. The existing `HookRenderWorldCallbacks` in
`HPLCompatibilityProbe.cpp:1803` wraps the world-overlay call. Observe this
existing pre-post boundary first; do not add another guessed native address.

Next implementation/proof contract:

1. At the exact player viewport, record context, draw/read FBO, attachment
   object/type/format/dimensions, viewport/scissor, clip data, eye and pose frame.
   Confirm the scene attachment at entry/exit; exclude reflection/GUI viewports.
2. Copy raw D24 depth into the matching eye's private cache **before** the next
   eye can overwrite the shared renderbuffer. Fail closed on missing attachment,
   changed context/size, unresolved samples, or eye/pose mismatch. Preserve GL state
   under the existing own-GL scope. Avoid synchronous full-frame CPU readback in
   the shipping hot path.
3. Capture one near object against a far wall, translate the HMD, and compare
   each eye's color, depth and exact projection. Repeat after save-load/context
   recreation. All-far is a valid possible view, not a universal runtime error;
   use a controlled geometry-positive scene for acceptance.
4. Only then accept depth submission or price AFW. xr-tape verifies the submitted
   metadata; paired GL images prove the pixels. AFW still needs current per-eye
   poses/history, disocclusion policy and a measured GPU budget.

Do not depth-blit texture 33 as if it were D24. It is a color texture encoding
`r=z/far`; a conversion would require
`d=far/(far-near)-(near*far)/((far-near)*(r*far))`, with zero/clear handling and
the exact eye's clip parameters. Prefer the proven raw depth attachment.

## Reproduce And Limitations

The MCP `dump_state` exceeded its 32 MB safety cap. This is an output-size limit,
not a missing capture. Supported CLI fallback, repeated for the five calls above:

```powershell
& 'D:\Dev Debug\apitrace\apitrace-14.0-win64\bin\glretrace.exe' `
  --headless --dump-state=2832882 --dump-format=json `
  'D:\Dev Debug\apitrace\traces\soma-gl-c3c7442f2aed'
```

Redirect stdout/stderr separately when repeating; stdout is a large JSON state,
not a screenshot. `tools/analyze_hpl_depth_state.py` reads complete files and
emits numerical summaries, raw arrays, previews and active GLSL. Requires NumPy
and Pillow. `tools/test_analyze_hpl_depth_state.py` has 8 passing offline controls,
including deliberately wrong sampled texture, empty default depth, malformed
payloads, nonfinite values and exporter row/byte order. Windows apitrace's float
image payload is native little endian/top-down despite its PFM-like header.

Replay exits were zero, but stderr reports unsupported early WGL DX-interoperability
calls, early `glCopyTexSubImage2D` format errors and shader/stream-buffer performance
warnings. Preserve these receipts. No claim of whole-trace pixel-perfect replay
or replay timing equivalence; the independently matching depth representations
and ordinary gameplay color image are the positive controls for this conclusion.

State SHA256 receipts:

```text
964261  9c95cce04ef4c8d98c5cfee50c93c541e245ee0af967452d4e611265a0bca60f
964590  b0ddc4a579ab03276d85284255915d2086bdc68b8d56d6c44eefd1e11188b7de
966241  6e6120615a50ad80e589bc2539fb830105b904d405ac331650c86a5abaeb1dcb
2832882 4cfeef014989a8de1f52e65a893ce8658b6eb7dc1c1f884e39e7632219f7baa0
2833206 babb0f89164bb733db23140cb78fe757edf515254b270f53ef479e091ec26125
```

Related: `PUREDARK_AFW_ASSESSMENT_2026-09-07.md`, S22 in `HYPOTHESES.md`,
`HPL_OPENGL_NOTES.md`, `PREYVR_ARM_IK_TRANSFER_2026-09-09.md`.
