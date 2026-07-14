# SOMAVR

Reverse-engineering and OpenXR scaffold for a SOMA/HPL3 VR mod.

## Build

Default OpenGL telemetry probe:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release --parallel
```

OpenXR presentation build:

```powershell
cmake -S . -B build-openxr-onekey -A x64 -DSOMAVR_ENABLE_OPENXR=ON
cmake --build build-openxr-onekey --config Release --parallel
```

Run deterministic camera/projection and OpenGL matrix tests:

```powershell
ctest --test-dir build -C Release --output-on-failure
ctest --test-dir build-openxr-onekey -C Release --output-on-failure
```

## Run

Launch suspended and inject before OpenGL/GLEW initialization:

```powershell
& "D:\Dev Debug\SOMAVR\build\Release\somavr_injector.exe" --launch "G:\SteamLibrary\steamapps\common\SOMA\Soma_NoSteam.exe" "D:\Dev Debug\SOMAVR\build\Release\somavr.dll"
```

OpenXR launch:

```powershell
& "D:\Dev Debug\SOMAVR\build-openxr-onekey\Release\somavr_injector.exe" --launch "G:\SteamLibrary\steamapps\common\SOMA\Soma_NoSteam.exe"
```

Attach to an already-running process:

```powershell
& "D:\Dev Debug\SOMAVR\build\Release\somavr_injector.exe" Soma_NoSteam.exe "D:\Dev Debug\SOMAVR\build\Release\somavr.dll"
```

Logs are written to `logs\somavr.log`. On first DLL load, `somavr.ini` is created with probe settings.

Each build folder now carries `somavr_build_flavor.txt`. If `[OpenXR] Probe=1` and the injector is pointed at a non-OpenXR DLL, it prints a warning before injection.
The OpenXR DLL also preloads `openxr_loader.dll` from its own folder before the first OpenXR call; check `openxr_loader_load ok/failed` in the log.
After loading a save, F10 is the normal VR-mode toggle. One press requests OpenXR,
waits for valid views, calibrates the current head pose, enables native tracking
and AFR stereo, and applies the fully centered projection. Press F10 again to
leave VR camera/stereo mode. F8 and F11 remain diagnostic runtime/stereo controls;
F3-F6 retain the existing targeted diagnostics. Plain F12 toggles the entire
post chain, `Ctrl+F12` cycles reversible render-only isolation across currently
active effects, and `Shift+F12` restores the normal effect chain.

## Current Goal

This is not yet a simultaneous dual-eye renderer. OpenXR transport, native head tracking, and AFR stereo geometry are proven. The current goal is to classify shader/post compatibility and finish head-relative presentation before attempting two renders per game frame:

- signature-guarded native eye view/projection integration,
- persistent per-eye OpenGL cache transfer,
- preserve validated runtime IPD/world scale with the confirmed centered-FOV compatibility policy,
- bounded AFR fallback and telemetry,
- head-relative FMOD listener orientation,
- deferred reconstruction UBO attribution and eye-invariant shadow/reflection state,
- selective post-effect classification using active object/vtable inventories and reversible per-effect isolation,
- same-frame dual rendering after callback and temporal ownership are proven.

The active `somavr.ini` is currently set up for the OpenXR probe build:

```ini
[Hooks]
FrameSummaryInterval=120
UniformMatrixProjectionOnly=1
UniformMatrixLogLimit=256
MatrixCapture=1
MatrixCaptureFrames=120
MatrixCaptureStackDepth=8
MatrixCaptureMaxSites=64
MatrixCaptureSamplesPerUniform=4
RenderDiagnosticCapture=1
RenderDiagnosticFrames=4
RenderDiagnosticMaxPrograms=128
RenderDiagnosticMaxDraws=8192
HPLCameraBridge=1
HPLLifecycleShutdown=1
HPLProjectionCenterControl=1
HPLProjectionCenteredDefault=1
HPLRoomscaleControl=1
HPLRoomscaleEnabledDefault=1
HPLReflectionFadeControl=1
HPLCameraLogInterval=120
HPLStereoAFR=1
HPLWorldScale=1.0
HPLRenderStageProbe=1
HPLAudioListenerProbe=1
HPLAudioListenerCorrection=1
HPLAudioListenerTranslation=1
HPLPostEffectControl=1
HPLPostEffectBypassDefault=0
HPLPostEffectDisableImageTrail=1
HPLPostEffectDisableChromaticAberration=1
HPLPostEffectDisableRadialBlur=1
HPLShadowJitterControl=1
HPLShadowJitterSuppressedDefault=0
HPLCompatibilityLogInterval=120

[OpenXR]
Probe=1
SessionProbe=1
ReleaseAfterProbe=0
BootstrapFrame=120
HoldFrames=0
ManualStart=1
FrameSubmit=1
MirrorBackbuffer=1
ResolutionScalePercent=100
ReferenceSpace=local
RecoveryEnabled=1
RecoveryDelayFrames=120
TrackingHoldFrames=30
TrackingRecoveryBlackoutFrames=2

[Controller]
Enabled=1
Haptics=1
HapticAmplitude=0.35
HapticDurationMs=30
DominantHand=right
SwapSticks=0
OneHandFallback=1
SuppressDuringAuthoredCamera=1
ComfortBlackoutFrames=2
```

## Known Install

Current local install:

```text
G:\SteamLibrary\steamapps\common\SOMA\
```

Relevant binaries:

- `Soma_NoSteam.exe`
- `Soma.exe`
- `SDL2.dll`
- `glew32.dll`

## Documentation

- `docs\ARCHITECTURE.md`: DLL module ownership, dependency direction, growth rules, and planned structural splits.
- `docs\CURRENT_STATE.md`: active baseline, proven behavior, and next test.
- `docs\ADDRESS_REGISTRY.md`: Ghidra/runtime address ledger.
- `docs\HYPOTHESES.md`: testable claims and redirect criteria.
- `docs\future-hook-map.md`: evolving hook map and Graphify seed.
- `docs\FUTURE_SYSTEMS_RE.md`: locomotion, hands/tools, HUD, and full-screen-effect research roadmap.
- `docs\VR_COMPATIBILITY_RE.md`: interaction physics, authored cameras, audio, loading/video, and dual-render boundaries.
- `docs\FEATURE_TRACEABILITY.md`: stable `FEATURE.*` ownership, dependency, hook, and acceptance-gate registry.
- `docs\GHIDRA_SYNC.md`: Ghidra names, prototypes, comments, tags, and promotion policy.
- `docs\RUNTIME_ANALYSIS_0.5.1.md`: successful stereo run, render-stage/FBO evidence, and audio/shader conclusions.
- `docs\RUNTIME_ANALYSIS_0.5.2.md`: audio result, F12 redirect, and deferred-shadow jitter evidence.
- `docs\RUNTIME_ANALYSIS_0.5.3.md`: F7 attribution, reflection RE, render diagnostics, and process-lifetime diagnosis.
- `docs\RUNTIME_ANALYSIS_0.5.4.md`: first deferred reconstruction UBO and exit-dump analysis.
- `docs\RUNTIME_ANALYSIS_0.5.5.md`: confirmed F5 result and remaining shadow/reflection attribution.
- `docs\RUNTIME_ANALYSIS_0.5.6.md`: F3/F4 redirects, clean shutdown proof, and vertical-asymmetry finding.

For a process that remains after the game window closes, capture a thread-aware
minidump before ending it:

```powershell
& "D:\Dev Debug\SOMAVR\build-openxr-onekey\Release\somavr_dumper.exe" Soma_NoSteam.exe
```

The dumper only writes diagnostic state; it deliberately does not close the game.
Add `--full` only when full process memory is specifically needed, because it can
produce a much larger file.

Graphify is installed for repository navigation. Generated output is local in
`graphify-out\`; refresh it after architecture or code changes with:

```powershell
graphify update .
```

Use `graphify-out\graph.html` for the relationship view and
`graphify-out\GRAPH_TREE.html` for the file/symbol hierarchy. The source corpus is
kept focused by `.graphifyignore` so CMake and fetched dependency trees do not
obscure SOMAVR-owned code.
