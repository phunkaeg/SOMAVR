# Runtime Analysis 0.5.4

Build: `0.5.4-renderdiag`

## Capture Summary

All three F6 captures completed and covered both AFR eyes twice.

| Capture | Render frames | Draws | Matrices | Programs |
| --- | --- | ---: | ---: | ---: |
| 1 | `3529-3532` | 1362 | 96 | 60 |
| 2 | `6332-6335` | 770 | 80 | 45 |
| 3 | `7096-7099` | 586 | 80 | 40 |

The direct eye matrices are coherent. Temporal projection alternates from a
horizontal offset near `-0.242513` for eye 0 to `+0.242513` for eye 1, temporal
view translation alternates near `-0.0695/+0.0695`, and inverse view-projection
also changes with the eye. Native eye selection is therefore reaching the normal
camera upload path.

## Deferred Reconstruction Finding

Programs `942` and `944` are live deferred shadow programs on world FBO `11` in
both eyes. Their texture samplers are direct uniforms, but projection, inverse
projection, view, inverse view, view-projection, near/far, inverse-screen-size,
screen-to-far-plane, and view-space-up values all report location `-1`. They are
members of an OpenGL uniform block and were invisible to the earlier direct
uniform hooks. This also explains why F7 observed no `avShadowMapOffsetMul`
uploads.

The user's visual observation is consistent with this split: the crosshair shows
the expected per-eye asymmetric projection offset, while shadows appear displaced
by the same amount. Geometry is using eye-local asymmetric projection, but the
deferred shader can reconstruct screen positions from a stale or mono-centered
camera packet.

Program `989`, a live world reflection/water program on FBO `11`, has the same
ownership pattern. Its inverse projection, inverse view, inverse screen size,
reflection fade, and reflection-size values are uniform-block members. The same
reconstruction mismatch can therefore explain the reflection artifact. Program
`853` also confirms a shared temporal SSAO/history route.

`0.5.5-reconstruct` adds F6 snapshots of every bound uniform block for the first
draw of each program and eye. The resulting `program_*_ubos.txt` files include
block bindings, ranges, member offsets/strides, and raw 32-bit values so eye 0 and
eye 1 camera packets can be compared directly.

## Exit Dump

`Soma_NoSteam-10384.dmp` contains exactly one thread. Its start address is
`Soma_NoSteam.exe+0x642bb4`, the CRT entry path, and its instruction pointer is
`opengl32.dll+0x10399`. The stack contains multiple Virtual Desktop OpenXR and
LibOVR runtime frames. No SOMAVR worker thread survives in this dump.

Ghidra maps `0x1403b16e0` to the `cSDLEngineSetup` destructor. It deletes Script,
Physics, Sound, Input, Resources, System, Graphics, Haptic, and Lipsync before
calling `SDL_Quit` at `0x1403b1803`. The new signature-guarded lifecycle hook
shuts OpenXR down at destructor entry, while the graphics context is still valid.

The dumper is capture-only by design. It writes a dump and leaves the target
process untouched; it is not expected to terminate SOMA.

## Next Test

- Use F5 briefly to switch between runtime asymmetric horizontal FOV and a
  symmetric horizontal projection. This tests whether the visible shadow and
  reflection displacement follows the projection-center offset.
- Take one F6 capture with F5 off and one with F5 on at the same trouble spot.
- Exit normally and verify `hpl_lifecycle pre_graphics_shutdown` completes and
  the process disappears.
