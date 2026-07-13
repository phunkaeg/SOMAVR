# Runtime Analysis 0.5.5

Build: `0.5.5-reconstruct`

## Confirmed Projection-Center Cause

F5 converged the left/right realtime-shadow differences. The UBO captures match
the visual result exactly:

- F5 off: `cLightArguments.a_mtxProjection` contains horizontal center
  `-0.242513` for eye 0 and `+0.242513` for eye 1 at block offset `96`.
- F5 on: both eyes contain `0.0` at offset `96`.
- The shadow programs continue to use the correct eye-local view and inverse
  matrices, while the large left/right displacement disappears visually.

This confirms S16: HPL3's deferred screen reconstruction is not compatible with
the runtime asymmetric horizontal projection used by the AFR bridge. A centered
horizontal projection is now the development default, while F5 remains available
to restore the original runtime asymmetry for comparison.

## Remaining Shadow And Lighting Motion

With centered projection, shadows are stereo-consistent but still change as the
HMD moves through tracked space. Programs `942/944` consume the full eye-local
view packet and reconstruct positions from the depth buffer before sampling the
shadow map. The remaining candidates are room-scale translation moving a
camera-relative light/shadow frustum; view-dependent shadow-map selection,
resolution, or culling; and state changing on alternate AFR frames.

The sharp ceiling lighting boundary can be another visible edge of the same
light-volume or shadow-frustum behavior. `0.5.6` adds F4 to remove only the
tracked head-center translation while preserving head rotation and eye IPD.

## Reflection Sweep

Capture 3 contains program `988`, a water/translucent refraction path with
`avReflectionSizeMul`, `avReflectionFadeStartAndLength`, inverse projection/view,
depth, normal, diffuse, and refraction inputs. The shipped shader computes
reflection fade directly from view-space vertex depth:

```text
1 - clamp((viewZ - fadeStart) / fadeLength, 0, 1)
```

That authored desktop-camera transition can appear as a top-to-bottom sweep
across a large window as the HMD backs away. `0.5.6` adds F3 to force this fade
factor fully on only around affected draws, restoring the original UBO bytes
immediately afterward. This distinguishes a fade-plane problem from reflection
texture bounds, mirrored-frustum clipping, or refraction opacity.

## Lifecycle Result

The lifecycle hook failed closed with `signature_mismatch`. Static image bytes
and Ghidra show the function begins `40 53 48 83 EC 20 48 8D 05`; `0.5.5`
omitted the leading `0x40` REX prefix from its guard. `0.5.6` corrects the guard.
