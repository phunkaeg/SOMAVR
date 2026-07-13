# Runtime Analysis: 0.5.2 Audio/Post Test

Date: 2026-07-12

## Result

The `0.5.2-audiopost` run validates the audio correction and rejects the post
effect chain as the owner of the principal stereo defect.

- F8 started OpenXR at game frame `4176`.
- F10 enabled native head tracking at pose frame `4328`.
- F11 enabled alternating-eye stereo at pose frame `4396`.
- The last complete summary was game frame `10920` with `6350` stereo OpenXR
  submissions and `6571` total submissions.
- No OpenXR frame failure, stereo suspension, native signature mismatch, or hook
  installation failure was logged.

## Audio

Listener correction remained active throughout the focused run. Logged committed
forward/up vectors followed the physical HMD delta while position and velocity
remained SOMA-authored. The user reported that sound appeared correct, although a
more directional sound source is still needed for a strong perceptual test.

This keeps `FEATURE.AUDIO_LISTENER` experimental rather than fully proven.

## F12 Result

F12 was switched on and off repeatedly between post-effect calls `4808` and
`6304`. The visible result was mainly increased contrast. It did not remove the
eye mismatch or movement-dependent instability.

The user identified the dominant defect more precisely:

- realtime shadows differ between the left and right eyes;
- shadow edges/patterns move or swim as the player moves.

This rejects hypothesis S13 for the principal defect. The issue is upstream of
the active post-effect composite and belongs to world/deferred-light rendering.

## Shadow Source Evidence

SOMA's installed `core/shaders/hpsl/deferred_light_frag.hpsl` uses screen pixel
coordinates to select its soft-shadow jitter kernel:

```text
vScreenJitterCoord = px_vPosition.xy * ShadowJitterLookupMul
```

The sampled offsets are scaled by the `avShadowMapOffsetMul` uniform. Because the
same world point projects to different pixels in the two eyes, the eyes can use
different random kernels even when their light-space projection is otherwise
correct. HPL2's `RendererDeferred.cpp` independently confirms that this uniform
is uploaded with `glUniform2f`, and that low shadow quality removes the jittered
smoothing path.

This mechanism directly predicts both eye-to-eye noise differences and
screen-locked swimming. It does not rule out a second problem in directional
cascade matrices (`a_mtxLightViewProj0..3` and split uniforms).

## Next Experiment

`0.5.3-shadowjitter` hooks only `avShadowMapOffsetMul`. F7 switches its submitted
value between SOMA's original radius and zero. Zero preserves shadow-map lookup
and lighting but collapses the random soft-shadow offsets onto the center sample.

Interpretation:

- If both eyes become stable with harder shadow edges, screen-space jitter is the
  primary defect and should be replaced with an eye-invariant filter.
- If only fine noise improves but large shadow shapes still differ or move, trace
  directional cascade splits and light view-projection generation next.
- If nothing changes and override counters remain zero, inspect uniform API/name
  attribution before changing renderer state.

The SOMA Ghidra program was not open during this pass, so no unverified binary
address was promoted. The evidence here is runtime and source-backed.
