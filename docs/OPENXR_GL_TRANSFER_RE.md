# OpenXR OpenGL Transfer Audit

Date: 2026-08-24

## Decision

SOMAVR keeps its native `XR_KHR_opengl_enable` path until runtime evidence
shows that the OpenXR swapchain handoff consumes a material part of the frame
budget. A D3D11 swapchain plus `WGL_NV_DX_interop2` is a viable fallback
architecture, not the default implementation.

TheDarkModVR is the shipped cross-engine precedent. Its renderer remains
OpenGL, but its optional Windows path registers one D3D11 texture as a GL
texture, renders through the GL framebuffer, unlocks the interop object, and
draws one flipped D3D11 triangle into the acquired XR swapchain image. Its
source attributes the workaround to SteamVR OpenXR context-switch overhead in
combination with bindless textures. SOMA has not yet demonstrated that same
condition.

TheDarkModVR is GPL software. This document records architecture and observed
behavior only; SOMAVR does not copy its implementation source.

OpenMW-VR provides an independent implementation of the same boundary. It
selects `XR_KHR_D3D11_enable` only when `WGL_NV_DX_interop2` is present, exposes
a D3D11 texture to GL, and locks/unlocks it around rendering. Its attempted
direct registration of runtime-owned XR images is disabled because some
runtimes return textures that cannot be shared this way; the working path uses
an intermediary D3D11 texture and `CopyResource` into the acquired image. Any
SOMAVR prototype must include that extra copy in both its design and telemetry.

## SOMAVR Transfer Boundary

Normal AFR submission is:

1. acquire the OpenGL color swapchain image;
2. wait with the existing 50 ms bound;
3. blit the completed per-eye cache into the runtime-owned framebuffer;
4. call `glFlush`;
5. release the image;
6. optionally repeat the equivalent path for depth.

`0.90.0-gl-transfer-audit` measures this boundary without `glFinish` or any
other forced GPU synchronization. Each eye accumulates attempts, successes,
failures, latest/total/maximum CPU microseconds, and phase times for
acquire/wait/copy/flush/release. The source is tagged `stereo_cache`,
`backbuffer`, or `black` so loading and recovery fills remain distinguishable.

The runtime separately measures the complete two-eye projection-transfer loop,
including optional depth. It emits `openxr_gl_transfer` every 300 completed XR
frames and bounded `budget_pressure` warnings when transfer consumes at least
25 percent of `predictedDisplayPeriod`.

The `copyCpu` value covers GL state queries/restoration and command dispatch;
it is not GPU execution time. Version `0.92.0-native-stereo-evidence` adds the
missing GPU measurement without synchronizing the pipeline: each eye owns an
eight-slot ring of GL timestamp pairs. Capture brackets game framebuffer to eye
cache; submit brackets eye cache/backbuffer/black into the acquired XR image.
Availability is polled on later transfers and unavailable results are skipped,
never waited on. The log reports latest/average/maximum microseconds, samples,
dropped busy slots, invalid timestamp pairs, and API availability.

The GPU ring uses GL query IDs allocated by SOMAVR inside the own-GL scope, so
the HPL occlusion-query observer ignores them. It is telemetry only and does not
alter acquire, copy, flush, release, or swapchain ownership.

Version `0.93.0-playbook-hardening` also removes dynamic container growth from
the default-active query and framebuffer-copy observers. Both use fixed 4096-
identity tables and report saturation through their existing overflow counters;
no identity insertion can allocate from a GL hook.

## Runtime A/B Gate

Use the same save, viewpoint, headset refresh rate, SOMAVR resolution scale,
depth setting, HUD state, and 60-second movement route for both runs:

1. Run through `VirtualDesktopXR`, the currently observed baseline runtime.
2. Make SteamVR the active OpenXR runtime and repeat the route.
3. Preserve every `openxr_gl_transfer` and final OpenXR summary row.
4. Compare projection average/maximum, budget-pressure count, per-eye failure
   count, the CPU phase that owns any increase, and per-eye `gpuCapture` /
   `gpuSubmit` time and query-drop rate.

Build the D3D11 interop prototype only if SteamVR repeatedly shows material
transfer cost or budget pressure that is absent under VirtualDesktopXR. A high
`wait` or `release` with low `gpuSubmit` supports a runtime-pacing/context
hypothesis. A high `copyCpu` with low GPU time points first to SOMAVR's own
state/copy dispatch. A high `gpuSubmit` is direct evidence that the GL copy is
expensive. Comparable low numbers do not justify another graphics device,
extension path, swapchain format negotiation branch, recovery path, and
packaging dependency.

## Conditional D3D11 Shape

If promoted, the smallest compatible design is:

- require both `XR_KHR_D3D11_enable` and `WGL_NV_DX_interop2`;
- log both prerequisites on the native GL baseline before enabling a prototype;
- create the D3D11 device on the runtime-required adapter LUID;
- assume an intermediary GL-shareable D3D11 texture plus a measured copy into
  the runtime image; treat direct runtime-image registration as optional;
- retain all HPL rendering, AFR caches, HUD capture, and diagnostics in GL;
- register one shared intermediate texture per source shape, not each HPL
  render target;
- lock only while GL writes the shared texture, then unlock before D3D access;
- issue one D3D11 flip/copy into each acquired XR color image;
- preserve native GL as the fail-closed fallback;
- keep depth on native GL until independently measured and designed.

The A/B backend must emit the same telemetry schema with `backend=D3D11Interop`
so the optimization proves itself on identical content.
