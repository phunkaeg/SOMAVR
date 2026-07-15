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
cmake -S . -B build-openxr -A x64 -DSOMAVR_ENABLE_OPENXR=ON
cmake --build build-openxr --config Release --parallel
```

Run deterministic camera/projection and OpenGL matrix tests:

```powershell
ctest --test-dir build -C Release --output-on-failure
ctest --test-dir build-openxr -C Release --output-on-failure
```

Create a validated versioned OpenXR bundle and ZIP:

```powershell
& ".\scripts\Package-Release.ps1" -IncludeDumper
```

The packager rejects non-OpenXR build metadata, stages the injector, DLL,
OpenXR loader, active config, diagnostics, and core docs, then writes
`SHA256SUMS.txt` beside the runtime files. Output is under `out\`.

Install or update a packaged build into a dedicated directory:

```powershell
& ".\Install-Or-Update-SOMAVR.ps1" -Destination "$env:LOCALAPPDATA\SOMAVR"
```

The installer verifies every packaged SHA-256 before copying. Existing
`somavr.ini` is preserved and changed package defaults are written to
`somavr.defaults.ini`. Updates remove only stale files recorded in the previous
install manifest. Uninstall is equally bounded:

```powershell
& ".\Uninstall-SOMAVR.ps1" -Destination "$env:LOCALAPPDATA\SOMAVR"
```

This preserves `somavr.ini`; pass `-RemoveConfig` to remove it. The injector also
scans the target process and game directory for known graphics/VR hook conflicts.
Warnings are advisory, while an already loaded `somavr.dll` blocks duplicate
injection.

## Run

Launch suspended and inject before OpenGL/GLEW initialization:

```powershell
& "D:\Dev Debug\SOMAVR\build\Release\somavr_injector.exe" --launch "G:\SteamLibrary\steamapps\common\SOMA\Soma_NoSteam.exe" "D:\Dev Debug\SOMAVR\build\Release\somavr.dll"
```

OpenXR launch:

```powershell
& "D:\Dev Debug\SOMAVR\build-openxr\Release\somavr_injector.exe" --launch "G:\SteamLibrary\steamapps\common\SOMA\Soma_NoSteam.exe"
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
The development-only `HPLDualRenderReplayProbe=1` adds `Ctrl+F6`: one press
replays only the next exact player viewport once, suppresses screen GUI on the
second pass, and logs whether both eyes came from the same tracked pose. With
`HPLDualRenderAutoProbe=1`, three automatically spaced samples run after the
tracked player viewport becomes stable. Each sample also records the CPU regions
mutated by HPL3's deferred/post-post phase. These are bounded evidence probes,
not a persistent dual-render toggle.
With `HudLayer=1`, the exact gameplay HUD set is removed from the eye render and
submitted once as a transparent, compositor head-locked OpenXR quad. Menus,
ImGui, subtitles not owned by that set, and diegetic terminal GUIs remain on
their native paths until separately classified.
With `HandControllerRoot=1`, F10 also enables a guarded controller-owned root
for the exact `PlayerHands_*` entity. Only uniform quarter-scale hands in the
normal player/move state are replaced; full-scale/authored animations, stale or
lost tracking, and every identity/signature mismatch retain SOMA's matrix.
With `ControllerFlashlightAim=1`, the exact scripted `Flashlight` spotlight
follows the dominant controller aim pose. Independent local offset and rotation
calibration align different controller profiles; stale/lost tracking and
authored cameras automatically retain SOMA's camera-mounted transform.
`ControllerFlashlightGameplayRay=1` also redirects only the recovered
low-frequency flashlight agent/gobo ray pattern to that exact visual origin and
basis while preserving SOMA's randomized cone, length, hit outputs, and all
unrelated physics-ray callers.
With `HPLRoomscaleSafety=1`, physical HMD translation is checked against SOMA's
world before it is applied. `HPLRoomscaleSafetyDynamic=1` includes moving bodies;
set it to `0` for the prior static-only policy. Blocked movement is shortened by a bounded
search plus `HPLRoomscaleSafetyClearanceMeters`; both eyes, controller poses,
hands, interaction, and flashlight reuse the same result. A configurable
horizontal ring plus top/bottom probes approximate head volume; invalid probes
are skipped so a tight authored starting position cannot trap the view. Moving
away from the body for 30 tracked poses can also trigger optional native capsule
catch-up. `HPLRoomscaleBodyReconciliation=1` uses small collision-tested feet
steps and compensates the tracking neutral so the visible world should remain
stationary; set it to `0` for immediate rollback while live acceptance proceeds.
Native player-capsule reconciliation remains separate work.
`DesktopMirrorEye=left` or `right` replaces the alternating desktop image with a
stable cached eye after headset submission. `DesktopMirrorAspect` accepts `fit`,
`fill`, or `stretch`; `native` eye mode restores SOMA's untouched backbuffer.
Paused menus suppress all gameplay injection. The dominant controller aim moves
the native menu cursor and trigger/select clicks when `MenuPointer=1`.
With the opt-in `MovementReference=head`, movement follows calibrated HMD yaw
without inheriting pitch/roll. `PhysicalCrouch=1` drives SOMA's native crouch
toggle from tracked height with hysteresis. `GrabTranslation=1` augments only
the exact Grab-state force PID with dominant-controller displacement; SOMA keeps
  mass, collision, constraints, gravity, and callbacks. `GrabRotation=1` extends
  that contract through SOMA's torque PID, while `ThrowRedirect=1` redirects one
  native Grab impulse along tracked release velocity. `TwoHandHudObject=1`
  optionally points an independent held tool from the dominant grip toward a
  squeezed support grip. `TwoHandGrabRotation=1` applies the same bounded
  direction contract to Grab-state torque without replacing native physics.
  The compositor HUD can
  suppress the fixed gaze crosshair with `HudSuppressCenterCrosshair=1`.
  `HPLComfortCameraAddControl=1` removes semantic Bob, Shake, and optional Sway
  only while F10 tracking is active. `HPLComfortCameraRollControl=1` separately
  suppresses configured Script, Lean, Move, or Climb roll at the exact native
  setters; the active profile preserves Script roll. World depth of field and
  named VideoDistortion can also be disabled only during active VR, while short
  transition blackouts cover ladder, climb, camera animation, sit, and death.
  The interaction bridge publishes
  native hit depth/world position. `InteractionReticle=1` presents that exact
  controller hit as an application-space OpenXR quad. With
  `InteractionReticleSemantic=1`, SOMA's own crosshair callback chooses semantic
  state after native interaction policy; `InteractionReticleNativeIcons=1`
  aspect-fits the matching shipped artwork with a procedural fallback.
  `FocusHaptics=1` adds a bounded intent-scaled pulse when confirmed entity/body
  focus changes. `GameplayHaptics=1` also mirrors SOMA's authored global rumble
  for damage, scripted tools/actions, death, and environmental effects through
  bounded bilateral OpenXR segments. These prototypes remain opt-in live-acceptance features rather
  than generated-config defaults.

## Current Goal

This is not yet a sustained simultaneous dual-eye renderer. OpenXR transport,
native head tracking, and AFR stereo geometry are proven. `0.35.0` adds a
one-frame exact-player replay to validate the remaining callback, temporal, and
performance contracts before promotion. `0.36.0` also advances physical
presence with bounded two-hand independent-tool and carried-object control.
`0.37.0` maps each active post effect's bound GL textures, dimensions, formats,
and framebuffer writes, and classifies same-pose left/right resources as shared
or eye-distinct.

`0.39.0` adds a head-locked in-VR status and control panel. Press `F1` or
`Menu + Secondary`, navigate with the movement stick, and activate with dominant
select/trigger. It exposes recenter plus reversible roomscale, centered
projection, HUD-layer, and interaction-reticle controls while suppressing all
underlying gameplay input.

`0.38.0` adds controller-addressable diegetic wall and handheld terminals.
During exact terminal states `8/9`, dominant aim drives SOMA's native virtual ImGui cursor and
select/trigger uses the native click route. Exact current-owner, GameHud
exclusion, 3D-set, signature, tracking, and layout guards restore original
behavior on every unsupported path. Configure this under `[Controller]` with
`TerminalPointer` and its horizontal/vertical angle and smoothing controls.

- signature-guarded native eye view/projection integration,
- persistent per-eye OpenGL cache transfer,
- preserve validated runtime IPD/world scale with the confirmed centered-FOV compatibility policy,
- bounded AFR fallback and telemetry,
- head-relative FMOD listener orientation,
- deferred reconstruction UBO attribution and eye-invariant shadow/reflection state,
- selective post-effect classification using active object/vtable inventories and reversible per-effect isolation,
- gameplay HUD extraction into a configurable OpenXR quad layer,
- controller-owned native hands with calibration and authored-state fallback,
- paused-menu aim pointer and hard gameplay-input suppression,
- promote same-frame dual rendering only after repeated one-frame replay evidence.

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
HPLRoomscaleVertical=1
HPLRoomscaleSafety=1
HPLRoomscaleSafetyDynamic=1
HPLRoomscaleSafetyClearanceMeters=0.02
HPLRoomscaleSafetyIterations=6
HPLRoomscaleSafetyRadiusMeters=0.09
HPLRoomscaleSafetyVerticalRadiusMeters=0.12
HPLRoomscaleSafetyRadialSamples=6
HPLRoomscaleBodyReconciliation=1
HPLRoomscaleBodyReconciliationThresholdMeters=0.45
HPLRoomscaleBodyReconciliationTargetMeters=0.25
HPLRoomscaleBodyReconciliationMaxStepMeters=0.015
HPLRoomscaleBodyReconciliationHoldFrames=30
HPLReflectionFadeControl=1
HPLComfortCameraAddControl=1
HPLComfortSuppressHeadBob=1
HPLComfortSuppressCameraShake=1
HPLComfortSuppressSway=1
HPLComfortCameraRollControl=1
HPLComfortSuppressScriptRoll=0
HPLComfortSuppressLeanRoll=1
HPLComfortSuppressMoveRoll=1
HPLComfortSuppressClimbRoll=1
HPLComfortDepthOfFieldControl=1
HPLComfortOpticsControl=1
HPLComfortSuppressFov=1
HPLComfortSuppressFovMultiplier=1
HPLComfortSuppressAspectMultiplier=1
HPLLoadingScreenControl=1
HPLLoadingScreenExitBlackoutFrames=2
HPLScriptedPresentationControl=1
HPLVideoLifecycleProbe=1
HPLScreenEffectControl=1
HPLScreenEffectDistanceMeters=1.5
HPLComfortLogInterval=120
HPLCameraLogInterval=120
HPLStereoAFR=1
HPLWorldScale=1.0
HPLRenderStageProbe=1
HPLDualRenderReplayProbe=1
HPLDualRenderAutoProbe=1
HPLDualRenderAutoProbeCount=3
HPLDualRenderAutoProbeDelayFrames=180
HPLDualRenderAutoProbeIntervalFrames=180
HPLPerEyePerformanceTelemetry=1
HPLPerEyeGpuTelemetry=1
HPLGpuQueryPoolSize=128
HPLAudioListenerProbe=1
HPLAudioListenerCorrection=1
HPLAudioListenerTranslation=1
HPLPostEffectControl=1
HPLPostEffectResourceProbe=1
HPLPostEffectBypassDefault=0
HPLPostEffectDisableImageTrail=1
HPLPostEffectDisableVideoDistortion=1
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
DesktopMirrorEye=left
DesktopMirrorAspect=fit
DepthCompositionProbe=1
DepthCompositionSubmit=0
ResolutionScalePercent=100
ReferenceSpace=local
RecoveryEnabled=1
RecoveryDelayFrames=120
TrackingHoldFrames=30
TrackingRecoveryBlackoutFrames=2
HudLayer=1
HudWidthPixels=1600
HudHeightPixels=900
HudDistanceMeters=1.5
HudWidthMeters=1.6
HudVerticalOffsetMeters=0.0
HudMaxAgeFrames=2
InteractionReticle=1
InteractionReticleSemantic=1
InteractionReticleNativeIcons=1
InteractionReticleSizePixels=64
InteractionReticleAngularSizeDegrees=0.75
InteractionReticleMinSizeMeters=0.008
InteractionReticleMaxSizeMeters=0.08
InteractionReticleMinDistanceMeters=0.15
InteractionReticleMaxDistanceMeters=8.0
InteractionReticleMaxAgeFrames=2
StatusPanel=1
StatusPanelWidthPixels=1024
StatusPanelHeightPixels=512
StatusPanelDistanceMeters=1.25
StatusPanelWidthMeters=1.15
StatusPanelVerticalOffsetMeters=0.0

[Controller]
Enabled=1
NativeLocomotion=1
MovementReference=head
PhysicalCrouch=1
PhysicalCrouchEnterMeters=0.35
PhysicalCrouchExitMeters=0.25
NativeTurn=1
SnapTurnDegrees=30
SmoothTurnDegreesPerSecond=120
NativeTurnSign=-1
Flashlight=1
Inventory=1
MenuPointer=1
MenuPointerHorizontalDegrees=70
MenuPointerVerticalDegrees=50
MenuPointerSmoothing=0.35
Haptics=1
HapticAmplitude=0.35
HapticDurationMs=30
GameplayHaptics=1
GameplayHapticAmplitudeScale=0.75
GameplayHapticMinAmplitude=0.05
GameplayHapticRetriggerDelta=0.08
GameplayHapticRefreshMs=80
GameplayHapticSegmentMs=100
FocusHaptics=1
FocusHapticAmplitude=0.12
FocusHapticDurationMs=15
FocusHapticCooldownFrames=15
DominantHand=right
SwapSticks=0
OneHandFallback=1
SuppressDuringAuthoredCamera=1
InteractionRay=1
InteractionRayOriginTolerance=0.75
GrabTranslation=1
GrabTranslationScale=1.0
GrabMaxOffsetMeters=0.75
GrabRotation=1
GrabRotationGain=100.0
GrabRotationSign=1.0
GrabMaxAngularSpeed=6.0
TwoHandHudObject=1
TwoHandGrabRotation=1
TwoHandSqueezeThreshold=0.75
TwoHandMinSeparationMeters=0.08
TwoHandMaxSeparationMeters=1.2
TwoHandDirectionBlend=1.0
ThrowRedirect=1
ThrowVelocityScale=1
ThrowVelocityThreshold=0.35
ThrowVelocityReference=2.0
ManipulationMappings=1
HandTrackingProbe=1
HandControllerRoot=1
HandRootOffsetX=0.0
HandRootOffsetY=-0.075
HandRootOffsetZ=0.0
HandRootPitchDegrees=0.0
HandRootYawDegrees=0.0
HandRootRollDegrees=0.0
ControllerFlashlightAim=1
ControllerFlashlightGameplayRay=1
FlashlightOffsetX=0.0
FlashlightOffsetY=0.0
FlashlightOffsetZ=0.03
FlashlightPitchDegrees=0.0
FlashlightYawDegrees=0.0
FlashlightRollDegrees=0.0
ComfortBlackoutFrames=2
StateTransitionBlackoutFrames=2
```

`DepthCompositionSubmit` is opt-in in generated configurations until live
runtime and hardware-matrix acceptance is complete. The development
`somavr.ini` enables it; setting it to `0` immediately restores color-only
submission while retaining the independent depth evidence probe.

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
