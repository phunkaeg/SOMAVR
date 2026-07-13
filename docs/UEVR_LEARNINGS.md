# UEVR Learnings Applied to SOMAVR

Date: 2026-07-10

Sources:

- Local UEVR source: `D:\Dev Debug\UEVR`
- Praydog, [UEVR: An Exploration of Advanced Game Hacking Techniques](https://praydog.com/reverse-engineering/2023/07/03/uevr.html)

## Applicable Principles

### Separate Runtime State From Graphics Backend

UEVR keeps OpenXR lifecycle, frame synchronization, poses, and composition separate from its D3D11/D3D12 resource handling. SOMAVR now follows the same ownership split:

- `OpenXRRuntime` owns instance/session events, `xrWaitFrame`, `xrBeginFrame`, `xrLocateViews`, and `xrEndFrame`.
- `OpenXRGLBridge` owns OpenGL swapchains, runtime image enumeration, framebuffer objects, and backbuffer copies.

This gives a future Vulkan or translation backend a clear boundary and keeps HPL3/OpenGL state handling out of the OpenXR state machine.

### Anchor XR Work to a Proven Frame Boundary

UEVR tracks engine frames through reliable render hooks instead of coupling runtime work to diagnostic output. SOMAVR now calls its OpenXR boundary on every real `gdi32!SwapBuffers`, while `FrameSummaryInterval` controls logging only. This also fixes the previous F8 trigger being sampled once per 120 rendered frames.

### Treat the OpenXR Session as a State Machine

The `0.3.0-xrframe` build begins only after `XR_SESSION_STATE_READY`, ends on `STOPPING`, and suspends submission after repeated failures. Every begun frame is paired with `xrEndFrame`, including frames where no projection layer can be produced.

### Use Runtime-Recommended Resources, With Overrides

Per-eye swapchains use `xrEnumerateViewConfigurationViews` dimensions and a configurable `ResolutionScalePercent`. Format selection prefers `GL_SRGB8_ALPHA8`, then `GL_RGBA8`, then `GL_RGBA16F`, based on runtime-advertised formats.

### Validate Discovered State Before Depending on It

Swapchain image counts, OpenGL extension functions, framebuffer completeness, view counts, pose-validity flags, and session state are checked before submission. Failure in the presentation branch is isolated from SOMA's renderer and reported through bounded logs.

### Prefer Semantic Anchors for Future Engine Hooks

Praydog recommends string references, localized disassembly, call-stack anchors, and structural validation over one large brittle byte signature. For HPL3, the current semantic anchors are:

- shader names such as `a_mtxModelViewProjection`, `a_mtxTemporalProjection`, and `a_mtxInvViewProjection`;
- camera configuration values `FOV=70`, `NearClipPlane=0.03`, and `FarClipPlane=1000`;
- the proven `glUniformMatrix4fv` call path and its callers;
- HPL2 source camera/render functions that can seed HPL3 string and control-flow searches.

`0.3.1-cameramap` applies UEVR's return-address/call-stack anchor technique directly: F9 captures module-relative stack frames above HPL3's camera-related uniform uploads, bounded by site and sample limits so it remains suitable for live gameplay probing.

`0.4.0-hplcamera` carries the same validation pattern into the first native hook. It requires matching prologues for `cCamera::GetFrustum` and `cFrustum::SetupPerspectiveProj`, then accepts only calls returning to the confirmed render-viewport RVA. HPL camera ownership, OpenXR pose snapshots, and GL/OpenXR presentation remain separate modules.

`0.5.0-afrstereo` keeps that ownership split for the stereo experiment: the HPL bridge chooses and builds an eye view, `OpenXRRuntime` records the exact render pose/FOV, and `OpenXRGLBridge` retains and transfers eye images. The AFR toggle is isolated from session lifecycle and can fall back to the proven mono path without recreating OpenXR resources.

## Deliberately Not Ported

UEVR's Unreal-specific `GEngine`, `IStereoRendering`, `FSceneView`, UObject, CVar, and D3D texture discovery are not applicable to HPL3. SOMAVR borrows the analysis and lifecycle patterns, not Unreal object assumptions or code.

The first frame-submission build duplicates SOMA's desktop backbuffer into both eyes. It proves transport and timing, but it is not stereoscopic rendering. Correct 6DOF stereo still requires locating or safely overriding HPL3's camera/view/projection state before scene culling and temporal effects.
