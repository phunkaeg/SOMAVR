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

Proceed when the summary has `fail=0`. Warnings name optional hook conflicts or
developer-layout fallbacks. Fix missing DLL, loader, config, runtime JSON, or x64
game failures before injection.

## Launch

```powershell
& "$env:LOCALAPPDATA\SOMAVR\somavr_injector.exe" --launch "G:\SteamLibrary\steamapps\common\SOMA\Soma_NoSteam.exe"
```

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
ComfortVignetteStrength=0.60
ComfortVignetteInnerRadius=0.50
ComfortVignetteFadeMilliseconds=250
```

Set `ComfortVignette=0` for immediate hard rollback. Increase `InnerRadius` for
a wider clear center or reduce `Strength` for a lighter peripheral mask.

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
- Keep `somavr.defaults.ini` for comparison; do not replace a tuned config
  blindly during updates.

Uninstall managed files while preserving the config:

```powershell
& "$env:LOCALAPPDATA\SOMAVR\Uninstall-SOMAVR.ps1" -Destination "$env:LOCALAPPDATA\SOMAVR"
```

Use `-RemoveConfig` only when the saved profile should also be deleted.
