# Runtime Analysis: 0.5.3 Shadow/Reflection Test

Date: 2026-07-12

## F7 Result

F7 was detected three times:

- enabled at game frame `2072`;
- disabled at frame `3098`;
- enabled again at frame `3197`.

Every toggle reported `uploads=0 overrides=0`. Both `glUniform2f` and
`glUniform2fv` hooks installed correctly, but no active program exposed or
uploaded `avShadowMapOffsetMul`. The absence of a visual change is therefore
expected: `0.5.3` did not alter the shadow shader.

This does not disprove screen-coordinate shadow instability. It proves only that
the chosen runtime control point is absent from the live shader variants or uses
another upload path.

## Reflection Evidence

The user also reports reflective shaders differing between eyes. SOMA's installed
shader sources contain at least two relevant paths:

- environment/cubemap materials calculate a reflection vector from the current
  camera-space eye vector and transform it with `a_mtxInvView`;
- water/world reflections sample `aReflectionMap` using distorted screen
  coordinates.

HPL2's deferred renderer shows that world reflections build a mirrored frustum
from the current main frustum and render it into a shared reflection buffer. It
also explicitly marks that reflection texture as not cleared before the pass.
HPL3 may differ in detail, but the matching shader/resource names make shared or
stale eye ownership a high-priority hypothesis.

Both reflections and shadows are view-dependent world-render effects, while F12
already excluded the post chain. Their common failure strongly raises the
probability that AFR's alternating camera state and shared per-frame resources
are the root class, rather than two unrelated shader bugs.

## 0.5.4 Diagnostic Capture

F6 now records four game frames, enough to span both AFR eyes twice. It creates a
folder under `logs/render-captures` containing:

- `draws.csv`: eye, pose frame, program, framebuffer, viewport, and draw order;
- `matrices.csv`: every matrix upload attributed to eye/program/uniform;
- `program_<id>.txt`: active uniform inventory and attached generated GLSL source.

The generated source is classified for shadow, reflection, environment, temporal,
and water terms. This should reveal the actual live shadow variant and distinguish
cube reflection from mirrored world-reflection programs.

## Process Lifetime

After the visible game exited, `Soma_NoSteam.exe` had no window and exactly one
thread. This matches SOMAVR's worker waiting indefinitely on its stop event after
all SOMA threads have ended. The event was previously signalled only during DLL
detach, which cannot occur while that worker keeps the process alive.

`0.5.4` detects when its worker is the process's final thread and returns without
calling runtime teardown against an already-destroyed game/GL context. A bounded
minidump helper is also included for any remaining hang.
