# SOMAVR xr-sim

`somavr_xrsim64` is a test-only OpenXR runtime for desk-side SOMAVR checks. It
is selected per process with `XR_RUNTIME_JSON`; it never replaces the machine's
active OpenXR runtime.

The runtime and control model are adapted from the MIT-licensed
`bioshock-trilogy-vr` xr-sim. See
`LICENSE.bioshock-trilogy-vr.txt`. SOMAVR-specific work includes x64/OpenGL
swapchains, the compositor capture path, launch/inject integration, and SOMA
window input.

## Build And Self-Test

Configure with `SOMAVR_ENABLE_OPENXR=ON` and `SOMAVR_BUILD_XRSIM=ON`, then:

```powershell
& '.\tools\xrsim\scripts\xrsim-selftest.ps1' -BuildDir '.\build' -Configuration Release
```

The self-test proves runtime selection, a running stereo session, WGL
swapchains, action edges, the sequence runner, 600 submitted frames, and a
nonblack stereo compositor capture. It does not prove SOMA camera hooks,
gameplay, image quality, or real-runtime timing.

## Launch SOMA

```powershell
$run = & '.\tools\xrsim\scripts\xrsim-launch.ps1'
```

The returned object contains the SOMA process ID, runtime state directory, DLL
path, and log path. The launcher waits on xr-sim's own `state.json`, not on
borrowed game-log strings.

## Script A Sequence

```powershell
& '.\tools\xrsim\scripts\xrsim-run.ps1' -Dir $run.Dir -GameProcessId $run.Pid -Steps @(
    'head rot 20 0 0',
    '@frames 3',
    '@key escape',
    '@shot after_escape',
    '@assert runtime eq somavr-xrsim'
)
```

Ordinary lines control the simulated runtime. `@frames`, `@shot`, `@assert`,
and `@fps` are test directives. `@key` sends a key only after the target SOMA
window is found and verified as foreground. There is intentionally no borrowed
BioShock mod-command channel; SOMAVR currently exposes automatic F10 startup
through `SOMAVR_AUTOMATION` and verified window input.

## Evidence Boundary

xr-sim is appropriate for OpenXR lifecycle, layer, action, projection, pose,
and compositor-capture contracts. Real-runtime cadence, OpenGL transfer cost,
driver behavior, headset optics, comfort, and visual acceptance still require
the headset route.
