# Runtime Analysis 0.5.6

Build: `0.5.6-stability`

## Lifecycle Confirmed

The corrected lifecycle hook installed at `Soma_NoSteam.exe+0x3b16e0`. Normal
exit logged both `hpl_lifecycle pre_graphics_shutdown begin` and `complete`, and
the process then disappeared normally. `FEATURE.CLEAN_SHUTDOWN` is proven for
this executable/runtime combination.

## F3 Reflection Fade Redirect

F3 was detected and the target was genuinely modified. Program `985` exposed
`avReflectionFadeStartAndLength` at block `1`, offset `32`; SOMAVR patched and
restored it for `369` draws. The authored values were `-0.0,-0.0`, and forcing a
full reflection factor caused no visible change. View-depth reflection fade is
therefore not the owner of the window/oven boundary.

The next reflection candidates are reflection/refraction texture bounds, the
mirrored reflection frustum, clip planes, and material alpha/composition.

## F4 Shadow Redirect

F4 toggled room-scale translation repeatedly. Positional HMD movement had little
effect, while pitch and roll strongly changed dynamic shadows. Room-scale
translation is not the primary owner.

The important remaining camera value is vertical projection center. `0.5.6`
centered only horizontal FOV; live stereo rows still report vertical projection
offset `-0.193187` for both eyes. HPL3 deferred lighting reconstructs view-space
position from screen coordinates, and this remaining asymmetric offset can move
the reconstructed position under pitch. Roll rotates the vertical mismatch into
a diagonal, matching the sharp ceiling boundary. It can also produce the observed
top-down sweep across a window.

`0.5.7-fullcenter` preserves each eye's horizontal and vertical tangent spans but
centers both axes. The submitted OpenXR FOV remains identical to the rendered FOV.
