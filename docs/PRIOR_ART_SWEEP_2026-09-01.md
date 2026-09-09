# Prior-Art Sweep - 2026-09-01

## Scope

This pass started from SOMAVR's feature registry and fleet bottleneck row, then
queried the local and reconciled Graphify graphs, searched the live VR Modding
playbook, inspected the in-house projects, and opened only source projects with
an exact matching signal. The focused external sources were
`bioshock-trilogy-vr` for frame/pair pacing and xr-sim, plus TheDarkModVR and
OpenMW-VR for the OpenGL-to-D3D11 OpenXR boundary.

## Current Bottleneck

The fleet ledger identifies SOMAVR's active gate as `BN-PERF-001`, with
`BN-RND-001` alongside it: determine the matched-headset cost and correctness of
the OpenGL transfer, occlusion queries, refraction copies, and remaining
screen-space/post owners. Camera discovery, OpenXR transport, selected AFR
architecture, input, locomotion, and packaging have already cleared their
earlier gates.

That ordering matters. No source-only evidence justifies replacing native
OpenGL submission, moving `xrWaitFrame`, or promoting native same-frame stereo
before the existing telemetry is exercised on the real runtime and GPU.

## Findings Applied

### xr-sim port identity

The playbook's ported-tool review named a real local defect. The runtime and
launcher had already been adapted to SOMA, but `xrsim-run.ps1` still offered
`bs1|bs2|bsi` and called a nonexistent BioShock `game-cmd.ps1`. This route could
only fail when a sequence reached `@mod`.

The sequence runner now uses `@key` through `xrsim-window-input.ps1`, requiring
the exact SOMA PID and verified foreground ownership. BioShock-only parameters,
paths, and output contracts are removed. The runtime's source attribution and
MIT license are retained explicitly, and the self-test now executes the
sequence runner rather than merely parsing it.

### Evidence semantics

The newer `PERF-007` request-ID pattern is important when a transport queues,
stages, imports, or hands frames between threads/processes. SOMAVR's current GL
path is synchronous at the frame boundary and has no output queue. Its AFR
ledger already carries the rendered game-frame/pose identity, per-eye capture
QPC, pair completion serial, capture delta, and cache age at submission, while
submitting each cached image with its recorded render pose.

Adding a second request identity now would duplicate evidence without changing
the scheduling decision. It becomes mandatory if a wait-ahead worker, D3D11
interop queue, asynchronous copy ring, or cross-process transport is built.
At that point duplicate request IDs must be counted and stale output discarded,
not queued.

### Interop prerequisite evidence

The OpenXR extension inventory now records `khrD3D11`, and the live game WGL
context records `wglNvDxInterop2`. These are diagnostic-only capability bits.
They establish whether an interop experiment is possible on the tested runtime
and GPU; they do not select or initialize a D3D11 backend.

## Confirmed Existing Alignment

- AFR eye two replays eye one's complete cached native camera base and OpenXR
  stereo snapshot, with a 100 ms/frustum guard.
- Torso follow already uses separate engage/release thresholds, delay, and a
  bounded turn rate: 45 degrees, 10 degrees, 250 ms, and 20 degrees/second by
  default. The newer two-threshold ergonomics guidance is already represented.
- Per-eye GL handoff telemetry separates acquire, wait, CPU copy, flush,
  release, total projection time, and nonblocking GPU capture/submit spans.
- Layer priority/capping, nonzero projection fallback, focus pacing, F10
  teardown/rebootstrap, and frame/pair freshness are already explicit.

## External Source Disposition

- OpenMW-VR added the intermediary-texture constraint to the conditional D3D11
  design and justified the two new capability fields.
- Doom 3 BFG VR uses ordinary GL framebuffer resolve/mirror blits. It does not
  supply a different desktop OpenXR handoff or an AFR-pair solution.
- Black Mesa/L4D2VR renders separate GLB hands from OpenVR skeletal actions and
  hides selected native viewmodel arm meshes. That is useful for a replacement
  hand renderer, but it does not solve lifetime-safe creation of SOMA's native
  campaign hand entity.
- BFVR's shoulder anchors, bounded elbow-pole continuity, singularity fallback,
  and endpoint checks corroborate SOMAVR's existing torso/arm math. The engine
  hook and native solver ownership are different, so no code transfer is
  warranted.

## Deferred With Reason

### TheDarkModVR D3D11 interop

TheDarkModVR proves the architecture: register a D3D11 texture with
`WGL_NV_DX_interop2`, render from GL into it, unlock, flip-blit into the D3D11 XR
swapchain image, and leave the engine renderer untouched. It does not prove
that SOMAVR needs that copy. Its stated pressure is SteamVR's native GL path;
SOMAVR's VirtualDesktopXR and SteamVR costs must be compared using the existing
`copyCpu`, `gpuCapture`, `gpuSubmit`, Wait, and End spans first.

OpenMW-VR independently corroborates the same D3D11-session/WGL-sharing shape.
Its implementation also records a constraint missing from the simpler sketch:
some runtime-owned D3D11 swapchain images cannot be registered directly with
WGL. Its direct registration is disabled and it renders through a separate
GL-shareable D3D11 texture, then performs `CopyResource` into the acquired XR
image. A future SOMAVR spike must therefore budget and identify the
intermediary copy; direct registration can only be an optional proven fast
path, not the assumed design.

### Persistent hands before a native seed

No sibling mod supplies a transferable answer. SOMA's released scripts prove
that `PlayerHands_SetVisible(true)` reaches module 18 and creates the current
campaign hand model, but safe native invocation still requires a script-owned
phase and owner lifetime. The read-only owner probe is the correct next rung;
calling it from an arbitrary render hook is not licensed by prior art.

### Native same-frame stereo

External projects reinforce that second-pass geometry cost follows draw count
and that screen-space effects must be produced per eye. SOMAVR already measures
replay draws/time, query reuse, scene-color copy reuse, and temporal ownership.
Promotion remains a headset/GPU decision, not an autonomous code task.

## Next Evidence

The next headset run should follow `NEXT_LIVE_EVIDENCE.md`. Before that run,
desk-side verification is:

```powershell
& '.\tools\xrsim\scripts\xrsim-selftest.ps1' -BuildDir '.\build' -Configuration Release
```

Pass means the simulated OpenXR protocol, action edge, sequence runner, stereo
layer, and compositor capture paths work. It says nothing about real headset
timing, transfer cost, hand comfort, or scene correctness.
