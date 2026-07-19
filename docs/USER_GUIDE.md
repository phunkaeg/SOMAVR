# SOMAVR User Guide

SOMAVR is an experimental OpenXR mod for the 64-bit `Soma_NoSteam.exe` build.
Keep a known-good package available when testing a new version.

## Requirements

- SOMA installed with `Soma_NoSteam.exe`.
- A working 64-bit OpenXR runtime selected by SteamVR, Meta, Virtual Desktop, or
  another runtime.
- A PC VR headset. Touch, Index, Microsoft Motion, Vive, and Khronos Simple
  controller profiles have explicit bindings.

## Install

Extract the release ZIP, open PowerShell in that folder, and install to a stable
location:

```powershell
& ".\Install-Or-Update-SOMAVR.ps1" -Destination "$env:LOCALAPPDATA\SOMAVR"
```

Updates preserve your `somavr.ini`; changed package defaults appear as
`somavr.defaults.ini`. The package and installer verify SHA-256 hashes.

## Check Readiness

Run this before launching a new package:

```powershell
& "$env:LOCALAPPDATA\SOMAVR\somavr_injector.exe" --doctor "G:\SteamLibrary\steamapps\common\SOMA\Soma_NoSteam.exe"
```

Proceed when the summary has `fail=0` and reports that SOMA's interaction hook
signatures match the supported build. Warnings name optional hook conflicts or
developer-layout fallbacks. Fix missing DLL, loader, config, runtime JSON, x64
game, or hook-signature failures before injection.

## Launch

```powershell
& "$env:LOCALAPPDATA\SOMAVR\somavr_injector.exe" --launch "G:\SteamLibrary\steamapps\common\SOMA\Soma_NoSteam.exe"
```

For repeated development tests, launch the developer configuration or jump
directly to a known map:

```powershell
& "$env:LOCALAPPDATA\SOMAVR\Launch-SOMAVR-Dev.ps1"
& "$env:LOCALAPPDATA\SOMAVR\Launch-SOMAVR-Dev.ps1" -Map "00_01_apartment.hpm" -MapFolder "maps/chapter00/"
```

Use `Soma_NoSteam.exe`. The install's `Soma.exe` imports Steam and has a
different native layout; the readiness doctor intentionally rejects its hook
signatures.

Load a save on the monitor, face forward, and press `F10` once. F10 enters or
leaves the complete VR camera/stereo path. `F2` recenters. `F1` opens the
head-locked status/options panel. Same-frame stereo remains an explicit panel
option; AFR remains the fallback. When supported, `HUD SHAPE` switches the
gameplay HUD between a flat quad and curved cylinder without restarting.

The packaged profile requests a 70-degree curve. To select it explicitly:

```ini
[OpenXR]
HudShape=cylinder
HudCylinderAngleDegrees=70
```

Use `HudShape=quad` for the original presentation. A runtime without
`XR_KHR_composition_layer_cylinder`, or one that rejects the layer, uses the
quad automatically.

The packaged test profile also enables a soft peripheral comfort vignette while
locomoting. Open F1 and select `COMFORT VIGNETTE` to compare it live. It fades
out automatically in menus, terminals, authored cameras, loading, dead state,
or when controller input becomes stale. Smooth turning contributes; snap turns
keep their short black-frame guard.

```ini
[OpenXR]
ComfortVignette=1
ComfortVignetteWidthMeters=1.2
ComfortVignetteStrength=0.75
ComfortVignetteInnerRadius=0.32
ComfortVignetteFadeMilliseconds=250
```

Set `ComfortVignette=0` for immediate hard rollback. Increase `InnerRadius` for
a wider clear center or reduce `Strength` for a lighter peripheral mask.

The packaged controller profile uses calibrated left-controller-relative
movement and simultaneous left/right aim guides:

```ini
[Controller]
MovementReference=controller
InteractionBothHands=1
AimGuide=1
AimGuideLengthMeters=1.2
ManipulationMotionPixelsPerMeter=900
ManipulationSlidePixelsPerMeter=2700
ManipulationReadPixelsPerRadian=900
GrabTranslation=1
GrabAttachToHand=1
SlideDirectVelocity=0
SlideVelocityScale=1
SlidePositionGain=12
SlideMaxVelocityMetersPerSecond=2.5
RotateDirectVelocity=1
RotateVelocityScale=1
RotateAngularVelocityScale=1
RotateMaxAngularSpeed=4
ReadPresentation=1
ReadObjectDistanceScale=1
ReadObjectScale=2
```

Use `MovementReference=head` for HMD-relative direction or `body` for SOMA's
native body-relative input. With `InteractionBothHands=1`, both guides are
visible and either trigger can claim SOMA's native interaction focus; its context
icon follows the selected guide to native hit depth. Set `InteractionBothHands=0`
for preferred-hand-only probing, or `AimGuide=0` to hide only the markers.
In Read views, hold the initiating hand's grip and rotate it to rotate the object
through full pitch, yaw, and roll; right-controller A or B exits.
`ReadPresentation=1` preserves SOMA's native pickup translation and timing while
applying `ReadObjectScale`. `ReadObjectDistanceScale` remains accepted for
configuration compatibility but translation scaling is disabled because live
evidence proved it fed back into SOMA's entrance animation; set
`ReadPresentation=0` for fully native presentation. Turn input is ignored while SOMA owns a physical
manipulation or Read state. `GrabAttachToHand=1` starts loose-prop Grab with a
selected-hit-to-grip pull through SOMA's native PID. The initial pull no longer
consumes the bounded controller-travel allowance. `GrabMaxOffsetMeters` limits
subsequent movement from the acquisition pose, not the distance from the object
to the hand. Slide uses SOMA's native controller-to-Look route so its shipped
script receives `mvMoveAdd`, speed factors, locked state, sounds, and callbacks.
Set `SlideDirectVelocity=1` only to compare the experimental joint-PID route.
Doors and levers combine hand translation around
their native pivot with wrist angular velocity projected onto the pin. Set
`RotateAngularVelocityScale=0` to disable only wrist twist, or
`RotateDirectVelocity=0` to restore the native camera-relative mouse route.
`SlidePositionGain` makes the joint catch up to controller displacement after a
short gesture; reduce it if a particular mechanism overshoots. Grab rotation
uses `GrabMaxAngularSpeed` as its bounded PID-error magnitude as well as its
target speed.

Wall terminals remain at their authored position instead of moving the player
and taking over the camera. The focused terminal display is duplicated into a
head-locked panel, and either controller can aim across that panel.
Trigger/select clicks and the existing cancel action exit through SOMA's native
input route. The relevant rollback controls are:

```ini
[Controller]
TerminalPointer=1
TerminalDiegetic=1
TerminalOverlay=1
TerminalRayPointer=1
TerminalRayLengthMeters=8
```

Set `TerminalOverlay=0` to keep only the physical display and restore native
mesh-ray pointer mapping. `TerminalRayPointer` then selects mesh-ray versus the
older head-relative pointer. Set `TerminalDiegetic=0` to restore the original
wall-terminal body/camera placement. Handheld terminals keep their authored
movement.

The active profile captures HUD content at the observed `1920x1080` SOMA target.
Quest currently requests `2688x2880` per eye, but world detail still originates
from SOMA's `1920x1080` backbuffer and is upscaled; `ResolutionScalePercent`
changes compositor target size, not native source detail.

Supporting runtimes can apply fixed foveation directly to the OpenXR eye
swapchains:

```ini
[OpenXR]
Foveation=1
FoveationLevel=2
FoveationDynamic=0
FoveationVerticalOffset=0.0
```

Levels run from `0` (none) to `3` (high). The feature requires all three FB
foveation extensions and automatically keeps native swapchains when unsupported.
Set `Foveation=0` for hard rollback. Compare GPU telemetry and peripheral image
quality before keeping it enabled.

Grabbed objects can produce initiating-hand feedback from SOMA's native surface
impacts:

```ini
[Controller]
ContactHaptics=1
ContactHapticMinSpeed=0.5
ContactHapticMaxSpeed=5.0
ContactHapticMaxDistanceMeters=0.75
ContactHapticMinAmplitude=0.08
ContactHapticMaxAmplitude=0.55
ContactHapticDurationMs=35
ContactHapticCooldownMs=45
```

The pulse requires Grab state and a fresh tracked interaction-owner grip near the native
contact point. It does not replace collision physics, impact sounds, particles,
or gamepad rumble. Increase the minimum speed or reduce the distance if weak or
nearby unrelated impacts feel noisy. Set `ContactHaptics=0` for hard rollback.

## Comfort Presets

Set one value under `[Comfort]` in `somavr.ini`:

```ini
[Comfort]
Preset=balanced
```

| Preset | Policy |
| --- | --- |
| `custom` | Uses the existing explicit settings unchanged. |
| `minimal` | Smooth turning, no routine black frames, and only ImageTrail suppression. |
| `balanced` | 30-degree snap turn, two-frame guards, a 60% comfort vignette, bob/shake and unsafe roll/optics suppression, and named post-effect policy. |
| `maximum` | Four-frame guards plus a stronger/narrower vignette, sway, and script-roll suppression. |

Every explicit INI key is parsed after the preset and therefore overrides it.
Presets never change world scale, eye height, hand calibration, movement
reference, stereo mode, or experimental feature controls.

## Diagnostics And Rollback

Logs are written to `logs\somavr.log`. Record the version, DLL hash, runtime,
headset, GPU, and relevant timestamps with every report.

- Set `HPLPerEyeViewHistoryControl=0` to restore native shared view history.
- Set `HPLPerEyeImageTrailControl=0` and
  `HPLPostEffectDisableImageTrail=1` to restore the proven ImageTrail
  suppression policy.
- Set `HPLPerEyeSSAOTemporalControl=0` to restore SOMA's native shared temporal
  SSAO history.
- Set `HPLSSAOFrameOwnerControl=0` to restore native once-per-eye SSAO jitter
  advancement while retaining per-eye GPU history.
- Disable `SAME FRAME STEREO` in F1 to return immediately to AFR.
- Press F10 to restore the native desktop camera/input path.
- Restore `Preset=custom` to use only explicit comfort settings.
- Set `ComfortVignette=0` or use its F1 action to remove dynamic tunneling.
- Set `Foveation=0` to restore ordinary color swapchains.
- Set `ContactHaptics=0` to remove native surface-impact observation and retain
  only focus/authored haptics.
- Keep `somavr.defaults.ini` for comparison; do not replace a tuned config
  blindly during updates.

Uninstall managed files while preserving the config:

```powershell
& "$env:LOCALAPPDATA\SOMAVR\Uninstall-SOMAVR.ps1" -Destination "$env:LOCALAPPDATA\SOMAVR"
```

Use `-RemoveConfig` only when the saved profile should also be deleted.
