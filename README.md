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

Create or replace the validated stable OpenXR bundle and ZIP:

```powershell
& ".\scripts\Package-Release.ps1" -IncludeDumper
```

The default output is `out\SOMAVR-latest` plus `out\SOMAVR-latest.zip`; older
generated SOMAVR package folders and ZIPs are removed after the new archive is
successfully created. Pass `-Versioned` only when intentionally preserving an
archival release. The packager rejects non-OpenXR build metadata, stages the injector, DLL,
OpenXR loader, active config, diagnostics, and core docs, then writes
`SHA256SUMS.txt` beside the runtime files. Output is under `out\`.
The concise player-facing instructions are in `docs\USER_GUIDE.md`.

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

Check the selected build, config, OpenXR runtime, game executable, architecture,
and game-directory hook conflicts without launching SOMA:

```powershell
& ".\somavr_injector.exe" --doctor "G:\SteamLibrary\steamapps\common\SOMA\Soma_NoSteam.exe"
```

Launch suspended and inject before OpenGL/GLEW initialization:

```powershell
& "D:\Dev Debug\SOMAVR\build\Release\somavr_injector.exe" --launch "G:\SteamLibrary\steamapps\common\SOMA\Soma_NoSteam.exe" "D:\Dev Debug\SOMAVR\build\Release\somavr.dll"
```

OpenXR launch:

```powershell
& "D:\Dev Debug\SOMAVR\out\SOMAVR-latest\somavr_injector.exe" --launch "G:\SteamLibrary\steamapps\common\SOMA\Soma_NoSteam.exe"
```

Attach to an already-running process:

```powershell
& "D:\Dev Debug\SOMAVR\build\Release\somavr_injector.exe" Soma_NoSteam.exe "D:\Dev Debug\SOMAVR\build\Release\somavr.dll"
```

Logs are written to `logs\somavr.log`. On first DLL load, `somavr.ini` is created with probe settings.
`[Comfort] Preset` accepts `custom`, `minimal`, `balanced`, or `maximum`.
The preset is applied first, then every explicit INI key overrides it, so existing
hand-tuned configs remain authoritative. The packaged development profile stays
on `custom`.

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
mutated by HPL3's deferred/post-post phase. These remain bounded evidence probes.
`HPLDualRenderContinuousControl=1` separately exposes `SAME FRAME STEREO` in the
F1 panel. It is off by default, reuses the exact-player replay without per-frame
diagnostic snapshots, and returns to AFR if eye-one caching or eye sequencing
fails.
`HPLPerEyeViewHistoryControl=1` isolates HPL3's confirmed 64-byte previous-view
matrix for each eye in both AFR and same-frame stereo. The feature restores
before each player-eye viewport, captures after SOMA's native update, resets on
recenter or stale-pose gaps, and falls back to shared native history on any
pointer or eye-sequence failure.
`HPLPerEyeImageTrailControl=1` adds a second native ImageTrail accumulation
texture/framebuffer and banks both pointers plus the effect's clear flag by
eye. It is signature- and lifecycle-guarded, resets on calibration/stale gaps,
and falls back to `HPLPostEffectDisableImageTrail=1` on any disagreement.
Generated configs leave this experimental path off. The active test profile
enables it and sets `HPLPostEffectDisableImageTrail=0`; restore those two values
to `0` and `1` respectively for the proven suppression baseline.
`HPLToneMappingFrameControl=1` makes shared ToneMapping exposure, white-cut,
window fade, color-grading transitions, and film-grain sampling advance once
per same-pose stereo pair. Eye two replays eye one's pre-update state and only
one native update is retained. It is guarded and opt-in; set it to `0` for
immediate native behavior.
`HPLPerEyeSSAOTemporalControl=1` isolates SOMA's temporal SSAO history per eye.
It signature-hooks the confirmed native SSAO writer and keeps two matching GPU
history copies while leaving the original AO shaders and render targets intact.
`HPLSSAOFrameOwnerControl=1` additionally makes the native temporal AO jitter
phase advance once per same-pose stereo pair. It can be disabled independently
without giving up per-eye GPU history.
It is opt-in and faults back to native shared history if GL copy support,
resource identity, or allocation disagrees; set it to `0` for immediate rollback.
With `HudLayer=1`, the exact gameplay HUD set is removed from the eye render and
submitted once as a transparent, compositor head-locked OpenXR layer.
`HudShape=cylinder` requests `XR_KHR_composition_layer_cylinder` and preserves
the configured center distance, physical width, and texture aspect; unsupported
or rejected cylinder layers fall back to the existing quad. The F1 panel can
switch `HUD SHAPE` live when the extension is available. Diegetic terminal GUIs
remain in the stereo world.
`ComfortVignette=1` adds a soft head-locked peripheral mask while accepted
gameplay locomotion is active. Its target comes from the resolved movement stick
and, in smooth-turn mode, the turn stick; loading, pause, terminals, dead state,
authored cameras, stale input, and the F1 panel all drive it back to zero. The
F1 panel can toggle it live. `ComfortVignetteStrength`, `InnerRadius`, and
`FadeMilliseconds` tune intensity, clear center, and attack/release; set
`ComfortVignette=0` for a hard rollback.
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
With `MovementReference=head`, movement follows calibrated HMD yaw. The
`controller` mode instead follows the calibrated left movement-controller yaw;
both ignore pitch/roll and preserve SOMA's native analog body movement.
`PhysicalCrouch=1` drives SOMA's native crouch
toggle from tracked height with hysteresis. `GrabTranslation=1` augments only
the exact Grab-state force PID with interaction-owner controller displacement; SOMA keeps
  mass, collision, constraints, gravity, and callbacks. `GrabRotation=1` extends
  that contract through SOMA's torque PID, while `ThrowRedirect=1` redirects one
  native Grab impulse along tracked release velocity. `TwoHandHudObject=1`
  optionally points an independent held tool from the interaction-owner grip toward a
  squeezed support grip. `TwoHandGrabRotation=1` applies the same bounded
  direction contract to Grab-state torque without replacing native physics.
  `InteractionBothHands=1` probes SOMA's native closest-entity ray with both
  tracked controllers and latches the initiating hand through physical
  interaction states. `AimGuide=1` displays both controller-ray guides while
  the selected native semantic icon follows the winning beam's hit depth. The
  compositor HUD can suppress the
  fixed gaze crosshair with `HudSuppressCenterCrosshair=1`, but this also clears
  native center content and is disabled in the packaged profile.
  `HPLComfortCameraAddControl=1` removes semantic Bob, Shake, and optional Sway
  only while F10 tracking is active. `HPLComfortCameraRollControl=1` separately
  suppresses configured Script, Lean, Move, or Climb roll at the exact native
  setters; the active profile preserves Script roll. World depth of field and
  named VideoDistortion can also be disabled only during active VR, while short
  transition blackouts cover ladder, climb, camera animation, sit, and death.
  Same-camera authored ownership changes preserve tracking/stereo while
  refreshing the native pose baseline and all per-eye temporal histories.
  The interaction bridge publishes
  native hit depth/world position. `InteractionReticle=1` presents that exact
  controller hit as an application-space OpenXR quad. With
  `InteractionReticleSemantic=1`, SOMA's own crosshair callback chooses semantic
  state after native interaction policy; `InteractionReticleNativeIcons=1`
  aspect-fits the matching shipped artwork with a procedural fallback.
  `FocusHaptics=1` adds a bounded intent-scaled pulse when confirmed entity/body
  focus changes. `GameplayHaptics=1` also mirrors SOMA's authored global rumble
  for damage, scripted tools/actions, death, and environmental effects through
  bounded bilateral OpenXR segments. `ContactHaptics=1` separately observes
  SOMA's native surface-impact speed and contact point, then pulses only the
  initiating hand while its freshly tracked Grab-state grip is near the collision.
  Native physics, impact audio/effects, and physical-gamepad output remain
  authoritative. Contact and focus feedback remain generated-off,
  live-acceptance features.
  Suggested bindings cover Khronos Simple, Oculus Touch, Valve Index,
  Microsoft Motion Controller, and HTC Vive profiles. Runtime profile-change
  events log the exact active profile for each hand, including reconnects and
  profile switches.

## Current Goal

The proven default remains OpenXR transport, native head tracking, and AFR stereo
geometry. `0.53.0` adds guarded once-per-frame ToneMapping exposure, fade, and
grading-transition ownership for same-frame stereo. `0.52.0` adds guarded per-eye ImageTrail resource and clear-state
ownership with exact native teardown and automatic suppression fallback.
`0.51.0` adds locomotion-gated compositor comfort tunneling with
tested fade/radial math, preset integration, and a live F1 toggle. `0.50.0`
adds an extension-negotiated curved HUD with live F1
quad/curved switching and automatic fallback. `0.49.0` adds deterministic comfort presets, a non-invasive readiness
doctor, and a packaged end-user guide. `0.48.0` adds HTC Vive controller bindings and exact per-hand active
interaction-profile diagnostics. `0.47.0` applies the first opt-in per-eye temporal resource to every
active stereo mode: HPL3's confirmed previous-view matrix is banked by eye in
both AFR fallback and same-frame rendering, with recenter and stale-gap resets.
`0.45.0` introduced the sustained
prototype below HPL3's
once-per-frame viewport owner. It is deliberately not the default until live
visual, temporal, performance, and rollback acceptance. `0.36.0` also advances physical
presence with bounded two-hand independent-tool and carried-object control.
`0.37.0` maps each active post effect's bound GL textures, dimensions, formats,
and framebuffer writes, and classifies same-pose left/right resources as shared
or eye-distinct.

`0.39.0` adds a head-locked in-VR status and control panel. Press `F1` or
`Menu + Secondary`, navigate with the movement stick, and activate with dominant
  select/trigger. It exposes recenter plus reversible roomscale, centered
  projection, same-frame stereo, HUD-layer, HUD-shape, interaction-reticle, and
  comfort-vignette controls
  while suppressing all underlying gameplay input.

`0.63.0` keeps wall terminals diegetic by suppressing only state `8` body
teleport, camera rotation, and terminal camera offset during active VR. Dominant
aim is projected through SOMA's native spatial GUI mesh/UV routine in states
`8/9`; select/trigger still uses the native click route. Handheld state `9`
retains its authored presentation. Configure `TerminalDiegetic`,
`TerminalRayPointer`, `TerminalRayLengthMeters`, and `TerminalPointer` under
`[Controller]`; each control has a fail-closed rollback path.

- signature-guarded native eye view/projection integration,
- persistent per-eye OpenGL cache transfer,
- preserve validated runtime IPD/world scale with the confirmed centered-FOV compatibility policy,
- bounded AFR fallback and telemetry,
- head-relative FMOD listener orientation,
- deferred reconstruction UBO attribution and eye-invariant shadow/reflection state,
- selective post-effect classification using active object/vtable inventories and reversible per-effect isolation,
- gameplay HUD extraction into configurable OpenXR quad/cylinder layers,
- controller-owned native hands with calibration and authored-state fallback,
- paused-menu aim pointer and hard gameplay-input suppression,
- live-validate and tune the opt-in same-frame renderer before default promotion.

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
HPLInventoryPresentationControl=1
HPLVideoLifecycleProbe=1
HPLScreenEffectControl=1
HPLScreenEffectDistanceMeters=1.5
HPLComfortLogInterval=120
HPLCameraLogInterval=120
HPLStereoAFR=1
HPLWorldScale=1.0
HPLRenderStageProbe=0
HPLDualRenderReplayProbe=0
HPLDualRenderAutoProbe=0
HPLDualRenderAutoProbeCount=3
HPLDualRenderAutoProbeDelayFrames=180
HPLDualRenderAutoProbeIntervalFrames=180
HPLDualRenderContinuousControl=1
HPLDualRenderContinuousDefault=1
HPLPerEyeViewHistoryControl=1
HPLPerEyeImageTrailControl=1
HPLToneMappingFrameControl=1
HPLPerEyePerformanceTelemetry=0
HPLPerEyeGpuTelemetry=0
HPLGpuQueryPoolSize=128
HPLAudioListenerProbe=1
HPLAudioListenerCorrection=1
HPLAudioListenerTranslation=1
HPLPostEffectControl=1
HPLPostEffectResourceProbe=0
HPLPostEffectBypassDefault=0
HPLPostEffectDisableImageTrail=0
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
Foveation=1
FoveationLevel=2
FoveationDynamic=0
FoveationVerticalOffset=0.0
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
MovementReference=controller
InteractionBothHands=1
AimGuide=1
AimGuideLengthMeters=1.2
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
ContactHaptics=1
ContactHapticMinSpeed=0.5
ContactHapticMaxSpeed=5.0
ContactHapticMaxDistanceMeters=0.75
ContactHapticMinAmplitude=0.08
ContactHapticMaxAmplitude=0.55
ContactHapticDurationMs=35
ContactHapticCooldownMs=45
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
ManipulationMotionPixelsPerMeter=900
ManipulationSlidePixelsPerMeter=2700
ManipulationReadPixelsPerRadian=900
SlideDirectVelocity=1
SlideVelocityScale=1
SlideMaxVelocityMetersPerSecond=2.5
RotateDirectVelocity=1
RotateVelocityScale=1
RotateMaxAngularSpeed=4
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

`Foveation` is also opt-in in generated configurations. It requires the complete
FB foveation extension family and otherwise falls back to ordinary eye
swapchains. Levels are `0` (none), `1` (low), `2` (medium), and `3` (high).
Set `Foveation=0` for hard rollback; use per-eye GPU telemetry before choosing a
quality level rather than assuming the runtime gains performance.

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
- `docs\NEXT_LIVE_EVIDENCE.md`: four prioritized headset passes that settle multiple project phases per log.
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
