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
game, hook-signature failures, or an enabled API-layer registration whose JSON
manifest is missing before injection. API-layer registrations with present
manifests are reported for diagnosis and are not failures by themselves.

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
leaves the complete VR camera/stereo path. `F2` recenters position and yaw;
pitch and roll remain tied to OpenXR's level horizon. `F1` opens the
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
NativeLocomotion=0
LocomotionDuringInteractions=1
InteractionBothHands=1
AimGuide=1
AimGuideLengthMeters=1.2
AimGuideSceneDepth=1
AimGuideIdleAlpha=0.05
AimGuideInteractableAlpha=0.25
PhysicalBodyFollow=1
PhysicalBodyFollowThresholdDegrees=45
PhysicalBodyFollowReleaseDegrees=10
PhysicalBodyFollowDegreesPerSecond=20
PhysicalBodyFollowDelayMs=250
ManipulationMotionPixelsPerMeter=900
ManipulationSlidePixelsPerMeter=2700
ManipulationReadPixelsPerRadian=900
GrabTranslation=1
GrabAttachToHand=1
SlideDirectVelocity=1
SlideVelocityScale=1
SlidePositionGain=12
SlideMaxVelocityMetersPerSecond=2.5
RotateDirectVelocity=1
RotateVelocityScale=1
RotateAngularVelocityScale=1
RotateMaxAngularSpeed=4
ReadPresentation=1
ReadObjectDistanceScale=2
ReadObjectScale=1
ReadObjectSettleFrames=45
TerminalPointerScale=1.75
HandWristPitchDegrees=45
HandWristOutwardOffsetMeters=0.03
HandWristVerticalOffsetMeters=-0.04
HandWristViewForwardOffsetMeters=-0.04
```

Use `MovementReference=head` for HMD-relative direction or `body` for SOMA's
native body-relative input. `NativeLocomotion=0` now uses SOMA's native
player-helper semantic Move dispatcher, matching the route used by physical
W/A/S/D while retaining analog magnitude and controller-relative direction.
`NativeLocomotion=1` is the diagnostic direct-body route and is not recommended.
`PhysicalBodyFollow=1` leaves ordinary head look independent, then gently turns
the arm rig's virtual torso reference after HMD yaw remains beyond 45 degrees
for 250 ms. It never rotates the HMD/game camera, is yaw-only, stops at a
10-degree residual, and follows explicit stick turns. Set it to `0` for a
strictly capsule-anchored torso.
With `InteractionBothHands=1`, both guides are
visible and either trigger can claim SOMA's native interaction focus; its context
icon follows the selected guide to native hit depth. `AimGuideSceneDepth=1`
terminates each guide against SOMA's nearest physics body. The rolling profile
uses a soft glow at 5% idle opacity and 25% over an interactable. Set `InteractionBothHands=0`
for preferred-hand-only probing, or `AimGuide=0` to hide only the markers.
In Read views, hold the initiating hand's grip and rotate it to rotate the object
through full pitch, yaw, and roll; right-controller A or B exits.
`ReadPresentation=1` scales the current native camera-relative position once
with `ReadObjectDistanceScale` and multiplies, rather than replaces, the
object's authored scale with `ReadObjectScale`. `ReadObjectSettleFrames` lets
the native rise animation establish the correct pose before that source matrix
is latched; hands continue tracking throughout state 10. This avoids the old
floor-level and recursive entrance-animation captures; set
`ReadPresentation=0` for fully native presentation. Turn input is ignored while SOMA owns a physical
manipulation or Read state. `GrabAttachToHand=1` starts loose-prop Grab with a
selected-hit-to-grip pull through SOMA's native PID. The initial pull no longer
consumes the bounded controller-travel allowance. `GrabMaxOffsetMeters` limits
subsequent movement from the acquisition pose, not the distance from the object
to the hand. MovingButton mechanisms, including the apartment curtain, receive
controller motion through SOMA's shipped semantic Look script. True Slide state
`4` follows the selected body's native joint pin through SOMA's existing
velocity PID. Set `SlideDirectVelocity=0` only to compare its script Look route.
`LocomotionDuringInteractions=1` uses the guarded native character-body Move
route while Wheel, Slide, Door, Lever, Tear, or MovingButton owns the hand; it
does not enable locomotion during Read, terminal, pause, or authored cameras.
Doors follow hand translation around their native pivot. Levers additionally
accept wrist angular velocity
projected onto the pin. Set
`RotateAngularVelocityScale=0` to disable lever wrist twist, or
`RotateDirectVelocity=0` to restore the native camera-relative mouse route.
`SlidePositionGain` makes the joint catch up to controller displacement after a
short gesture; reduce it if a particular mechanism overshoots. Grab rotation
uses `GrabMaxAngularSpeed` as its bounded PID-error magnitude as well as its
target speed.

Wall terminals remain at their authored position instead of moving the player
and taking over the camera. The focused terminal display renders once, directly
into a high-resolution head-locked panel, and either controller can aim across
that panel. SOMA's sparse email rectangles remain under diagnosis; the capture
falls back to live frames when no retainable nested color clear is observed.
Trigger/select clicks and the existing cancel action exit through SOMA's native
input route. Right-controller A/B and looking away hold that cancel across
multiple input frames so SOMA cannot miss a same-frame click. The
relevant rollback controls are:

```ini
[Controller]
TerminalPointer=1
TerminalDiegetic=1
TerminalOverlay=1
TerminalPreserveDirtyRects=1
TerminalRayPointer=1
TerminalRayLengthMeters=8
TerminalPointerScale=1.75
TerminalLookAwayExit=1
TerminalLookAwayDegrees=65
TerminalLookAwayFrames=8
```

Set `TerminalOverlay=0` to keep only the physical display and restore native
mesh-ray pointer mapping. `TerminalRayPointer` then selects mesh-ray versus the
older head-relative pointer. Set `TerminalDiegetic=0` to restore the original
wall-terminal body/camera placement. Handheld terminals keep their authored
movement.

`TerminalPreserveDirtyRects=1` first attempts to preserve SOMA's incremental
email updates on the dedicated terminal target. If eight completed retained
captures contain no matching nested color clear, SOMAVR automatically returns
to live-frame capture so a black panel cannot be retained indefinitely. Set it
to `0` to select live-frame capture immediately without disabling the overlay.

Loose physics objects can remain held while walking. SOMA's own Grab-state mass
and movement-speed rules still apply, so heavier props may slow the player.

SOMA's occasional native hand model is a single two-hand rig. The shared-root
pose override stays disabled because it placed both miniature hands on the
right controller. The current prototype instead uses:

```ini
[Controller]
HandTrackingProbe=1
HandControllerRoot=0
HandScaleNormalization=1
HandWristPosition=1
HandWristRotation=1
HandWristRollDegrees=-90
HandSocketedPropStabilization=1
HandFreezePose=1
HandShoulderVerticalOffsetMeters=-0.30
HandShoulderBackOffsetMeters=0.10
HandArmIKElbowDownMeters=0.10
HandArmIKErgonomics=1
HandShoulderReachCompensation=1
HandShoulderReachStart=0.85
HandShoulderReachMaxMeters=0.05
HandArmIKMaxSwivelDegreesPerFrame=10
HandTargetScale=1.0
```

Scale normalization preserves the shared root's calibrated relationship to the
tracked head. The shoulder rig follows safety-clamped HMD world position while
its heading follows player-body yaw, so physical leaning moves the torso without
head-only look/tilt twisting it. Wrist position is independent per controller.
Wrist rotation uses the model's geometric palm basis and follows controller yaw,
pitch, and roll. The packaged -90 degree wrist roll is an explicit final
calibration. The packaged shoulder offset lowers the shared arm root by 30 cm, shifts
it 10 cm behind the native body-forward direction, and biases elbow poles 10 cm
down before IK. `HandFreezePose=1` restores the complete seeded arm and finger
pose before controller IK, suppressing scripted finger motion while retaining
native hand-socket prop attachments. Set `HandWristRotation=0` to retain native rotation,
`HandFreezePose=0` to retain native finger/intermediate-chain animation,
`HandShoulderVerticalOffsetMeters=0` to restore native shoulder height, or
disable scale/position independently for isolation. Keep
`HandControllerRoot=0`.

`HandArmIKErgonomics=1` keeps elbow continuity in body-local space and selects a
downward-biased bend with modest side/back contribution. `HandArmIKElbowDownMeters`
scales that downward preference around the packaged 0.10 m baseline.
`HandShoulderReachCompensation=1` allows only the matching clavicle to contribute
after `HandShoulderReachStart` arm extension, capped by
`HandShoulderReachMaxMeters`; tracked wrist poses are never smoothed. Disable
either lane independently before changing its tuning values.

For terminal rendering diagnosis, leave the problem view visible and press
`Ctrl+F10` once. Four native-size and four upscaled captures are written, each
with RGB and alpha variants, to `logs\terminal-captures`; a brief stall is
expected. The same chord records a bounded framebuffer/program/scissor/texture/
blend trace in `somavr.log`. These capture both sides of the terminal upscale,
not the projection eye images.

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

Logs are written to `logs\somavr.log`; the immediately preceding session is
preserved as `logs\somavr.previous.log`. Startup records exact Git/dirty build
identity, DLL PE identity, and the basic machine fingerprint, so attach the full
log rather than transcribing only the visible version.

Unhandled crashes automatically create a bounded rich dump and
`somavr-crash.log` under `logs\dumps`. The handler suppresses repeated fault
addresses and stops after three attempts per process. Set the environment
variable `SOMAVR_FULLDUMP=1` only when full process memory is specifically
requested; it can create a very large file. The external `somavr_dumper.exe`
remains the correct tool for a hung but still-running process.

`somavr_entity_profiles.ini` records exact live hand/tool/story identities and
their baseline calibration after a clean shutdown. In 0.87 it is telemetry-only:
editing it does not alter gameplay. Active per-entity calibration will be
promoted later, one proven family at a time.

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
## Persistent Arms And Authored Interactions

The development profile enables `HandArmIK`, `HandAlwaysVisible`, and the first
`MedicineInteraction` evidence profile. SOMAVR waits for SOMA's native hand
model, solves its shipped shoulder/elbow/wrist rig toward each controller, and
preserves its last stable root while pause/menu or authored ownership suspends
controller mutation. Each feature has an independent rollback switch. Real
player/body teardown invalidates the seed before save/load replacement.

The medicine profile currently recognizes left-hand cap removal and tipped
bottle-at-mouth gestures, logs their measurements, and provides haptic
confirmation. It deliberately does not advance SOMA's script yet; the first log
is used to calibrate the bottle-local cap axis before native commit is enabled.
