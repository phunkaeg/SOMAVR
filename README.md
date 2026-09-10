# SOMAVR

An experimental **PC VR mod for SOMA**, bringing Frictional Games' HPL3/OpenGL
world into an OpenXR headset with stereoscopic rendering, tracked head movement,
motion-controller interaction, and the game's own hands and arms.

This is a reverse-engineered, injected mod, not an engine port or an official
Frictional Games release. SOMA itself is required and is not included.

**Current candidate: `0.96.2-hands-bootstrap` | Work in progress / testing build**

The core VR experience is working and has been tested in a headset. That does
**not** mean every feature, scene, device, or campaign sequence is finished.
Most hands-on testing has been in the opening apartment. Expect rough edges,
regressions, and configuration changes between builds. Keep a backup of your
saves and a known-good package. Do not overwrite your only baseline save while
testing new authored interactions.

## Features

| Area | Implementation and current state |
| --- | --- |
| Stereo and head tracking | Native game-camera integration, per-eye view/projection, positional and rotational tracking. This is not mouse-emulated head tracking. The current profile enables same-frame stereo, with alternate-frame rendering (AFR) as a fallback. |
| Locomotion | Analog movement, left-controller-relative direction by default, configurable head/body-relative movement, snap/smooth turning, and physical crouch support. Walking while carrying loose props and manipulating supported mechanisms is implemented. |
| Motion-controller interaction | Either hand can aim and interact. The initiating hand owns a held interaction; translucent aim guides shorten against scene collision geometry and brighten over interactables. |
| Physics objects and mechanisms | Tracked pickup, pull-to-hand assistance, rotation, drop/throw, drawers, doors, curtains, and other native interaction states. SOMA's physics and joint limits remain in charge; sensitivity and throw feel are still being tuned. |
| Native hands and arms | Uses SOMA's bilateral rig with wrist tracking, two-arm IK, estimated shoulders/torso, and compatible-state retention. Alignment, elbow behavior, and per-eye deformation still need wider testing. |
| Story-object inspection | Controller rotation plus adjustable camera-relative distance and scale. Current tuning brings objects closer and doubles their authored linear scale; different objects still need validation. |
| Terminals | Diegetic-camera preservation, a floating terminal panel, controller pointer/clicks, cancel, and look-away exit. Laptop email capture and beam/cursor alignment remain known problem areas. |
| HUD and menus | Spatialized HUD, flat/curved layer support with quad fallback, subtitles, interaction indicators, a desktop mirror, and controller menu input. Coverage varies by screen and state. |
| VR options and comfort | F1 in-headset panel, yaw/position recentering, bob/shake and selected camera/effect suppression, snap-turn black frames, comfort vignette, and collision-aware roomscale safeguards. These are comfort aids, not a guarantee of comfort. |
| Haptics and tools | Controller flashlight aiming and implementations for interaction, authored rumble, and contact feedback. Later-game tools and effects need further testing. |
| Diagnostics | Build/configuration logs, readiness doctor, crash dumps, paired-eye/terminal captures, scene-depth probes, and arm/palette witnesses for regressions. |

### New In This Candidate

`HandBootstrap=1` requests the **campaign-selected native hand model before
the first medicine/vial animation**. It waits for settled, controllable VR
gameplay and invokes the game's own hands handler after native PostUpdate.
It does not spawn a hardcoded replacement model or force an arbitrary mesh
visible. Existing rigs are left alone; attempts are bounded and logged.

This new creation path has passed static native-contract checks and automated
policy tests, but **has not yet been run in-game or accepted in a headset**.
The intended result is tracked arms without first picking up the vial, not a
claim that always-visible hands are already validated throughout the campaign.

## Requirements And Compatibility

- **64-bit Windows** and a legitimate PC installation of SOMA.
- The supported **`Soma_NoSteam.exe`** supplied with the tested installation.
  The Steam-integrated `Soma.exe` has a different native layout and is not
  interchangeable. The injector's doctor checks compatibility; do not rename
  executables or force another version through a failed signature check.
- A PC-connected VR headset and an active **64-bit OpenXR runtime supporting
  OpenGL (`XR_KHR_opengl_enable`)**. A bundled loader is not a headset runtime.
  A standalone Quest cannot run this mod locally.
- Working motion controllers. Bindings exist for Touch, Index, Windows Mixed
  Reality/Microsoft Motion, Vive, and Khronos Simple profiles; bindings are
  **not** a tested-device certification. Simple controllers cannot expose all
  stick/button workflows.
- A graphics driver supporting SOMA's OpenGL renderer and the selected
  runtime's OpenGL requirements, plus enough GPU headroom for stereo rendering.
  There is no established minimum GPU/performance specification yet.
- A keyboard and mouse for initial launch, loading saves, diagnostics, and
  fallback interaction when a menu or sequence is not fully controller-ready.

Recent development testing uses Quest 3 with VirtualDesktopXR and an NVIDIA
GPU. Other runtime/GPU/headset combinations are not comprehensively validated.
Avoid stacking ReShade, other graphics proxies, VR injectors, and capture
wrappers during the first test. The doctor reports known conflicts.

## Install

Use an OpenXR-enabled SOMAVR package containing `somavr.dll`,
`somavr_injector.exe`, `openxr_loader.dll`, `somavr.ini`, build metadata, and
`SHA256SUMS.txt`. Keep those files together. **Do not copy them over SOMA's
own DLLs or into the game installation.**

Extract the ZIP and open PowerShell in its `SOMAVR-latest` folder:

```powershell
& ".\Install-Or-Update-SOMAVR.ps1" -Destination "$env:LOCALAPPDATA\SOMAVR"
```

The installer verifies package checksums and uses a dedicated mod directory.
Updates preserve your existing `somavr.ini`; new package defaults are written
to `somavr.defaults.ini`. Compare those files when updating: a preserved older
configuration will not automatically enable new features such as `HandBootstrap`.
Unknown files are preserved rather than treated as installer-owned.

You can also run directly from an extracted package without installing it.
Replace the mod path in the commands below with that package directory.

## Run

1. Start your headset connection and the intended OpenXR runtime. Close any
   already-running SOMA instance.
2. Set the actual game executable path, then run the readiness check:

```powershell
$Soma = "G:\SteamLibrary\steamapps\common\SOMA\Soma_NoSteam.exe"
$Mod = "$env:LOCALAPPDATA\SOMAVR"
& "$Mod\somavr_injector.exe" --doctor $Soma "$Mod\somavr.dll"
```

3. Resolve failures before continuing. Launch through the injector:

```powershell
& "$Mod\somavr_injector.exe" --launch $Soma "$Mod\somavr.dll"
```

4. Load your save normally on the desktop. Once in gameplay, face your intended
   forward direction and press **F10**. One press requests the complete VR path;
   you do not need the old F8 > F10 > F11 sequence.
5. Use **F2** to recenter and **F1** for the VR options panel.

PowerShell needs the leading `&` when running an executable whose quoted path
contains spaces. Launching through the injector is recommended over attaching
late, because some hooks must observe OpenGL initialization.

### Existing Development Checkout

Double-click `Launch-SOMAVR.bat` in the repository root. It uses
`out\SOMAVR-latest`, refuses a duplicate SOMA process, and starts the **normal**
game. Its default game path is the `G:` path shown above; adjust `SOMA_EXE` in
the batch file for a different installation. `Launch-SOMAVR.bat --check` runs
the doctor without launching.

`Launch-SOMAVR-Dev.ps1` is a separate developer launcher. Do not use it for
ordinary save-game testing unless requested: developer settings can change
startup behavior, debug UI, and map selection.

## Essential Controls

Defaults below assume Touch-style controllers and the packaged profile.
Mappings are context-sensitive, especially when holding or inspecting objects.

| Control | Action |
| --- | --- |
| F10 | Enter/leave VR mode. |
| F2 | Recenter position and yaw; pitch/roll keep the level tracking horizon. |
| F1 | Open/close VR options; gameplay input is suppressed while the panel is open. |
| Left stick | Walk, relative to the left movement controller by default. |
| Right stick | Turn; snap turning is the default. Turning is gated in some interaction states. |
| Either trigger | Aim/select/interact; the initiating hand owns the interaction. |
| Holding hand's grip | Rotate compatible held/inspected objects. |
| Right A or B in inspection/terminal | Back/cancel. This is not a universal gameplay mapping. |
| Slow trigger release while holding a loose prop | Place/drop. |
| Fast trigger release, or holding hand's primary button | Native throw path for loose physics props; still under tuning. |
| Ctrl+F10 | Bounded diagnostic captures; expect a brief capture hitch. |

See [the user guide](docs/USER_GUIDE.md) for tuning and additional controls.

## Configuration And Rollback

The active `somavr.ini` lives **beside the DLL being injected**, not necessarily
in the source checkout. Startup logs name the loaded DLL and configuration.
Restart SOMA after editing the INI unless the setting has an explicit F1 control.

Useful independent switches include:

```ini
[Controller]
HandAlwaysVisible=1
HandBootstrap=1
MovementReference=controller
ReadObjectDistanceScale=1.2
ReadObjectScale=2
```

Set `HandBootstrap=0` to restore vial-first model creation while retaining the
existing hand/arm tracking system. `HandAlwaysVisible` controls retention, not
initial creation by itself. Do not enable every diagnostic or experimental
switch at once; start with the package profile and change one thing at a time.

## Known Limitations

- **Not a complete campaign conversion.** Later maps, special tools, ladders,
  cutscenes, death sequences, and changes of player model need more coverage.
  Some authored states deliberately suspend or limit VR overrides.
- **Arms are inferred from three tracked points.** There are no tracked elbows
  or shoulders. Wrist calibration, tangled poses, torso alignment, and
  locomotion-only per-eye arm lag have been reported and remain acceptance work.
- **Terminals are unfinished.** The laptop email region can flash or display
  fragmented/missing content, especially when looking directly at the physical
  screen. Pointer and beam alignment are also under investigation.
- **Interaction is not universally 1:1.** Native mass, collision and joint
  constraints remain. Doors/drawers and throwing still need feel validation;
  weak throws and player recoil have been reported. New follow-through/throw
  fixes should be tested rather than assumed to resolve every object.
- **Story inspection and UI need tuning.** Scale/distance vary with authored
  assets. Menu input, captions, cropping, and context-icon placement are not
  guaranteed correct in every state.
- **Stereo costs performance.** Same-frame mode renders the player viewport
  twice. AFR can reduce fresh updates per eye and introduce motion-dependent
  disagreement. Neither mode guarantees headset refresh rate on every system.
- **Swapchain resolution is not source detail.** The world currently derives
  from SOMA's render target/backbuffer. Increasing only the OpenXR target scale
  does not produce a higher-resolution native scene; aliasing remains possible.
- **Depth is diagnostic, not a shipping reprojection feature.** Scene-depth
  capture exists, but depth submission is off. AFW/spacewarp is not implemented.
- **Binary- and runtime-sensitive.** A different SOMA executable, graphics
  wrapper, driver, or runtime can break a previously working path. Hooks fail
  closed on mismatched contracts rather than supporting arbitrary builds.
- No standalone-headset build, full-body tracking, or established Linux/Proton
  support is claimed.

Stop testing if you feel uncomfortable. Recenter and diagnose stationary
before continuing movement tests; visual correctness and comfort require a
person in the headset, not just passing automated tests.

## Testing And Bug Reports

Start with [the next-test checklist](docs/NEXT_LIVE_EVIDENCE.md). For this build,
the key test is **a pre-vial save: F10, then arms appearing without the vial**,
followed by the medicine sequence, pause/resume, reload, and normal exit.

Attach `logs\somavr.log` from the directory beside the injected DLL. The prior
session is kept as `logs\somavr.previous.log`. Include the exact action/scene,
headset/runtime, affected eye(s), and whether the issue also appears on the
desktop. Keep the build-identity banner, including any `dirty` suffix.

Ctrl+F10 captures paired scene eyes and, when relevant, terminal surfaces under
`logs\eye-captures` and `logs\terminal-captures`. The paired scene images do
not include every final OpenXR overlay. Crash reports may also be written under
`logs\dumps`; share dumps privately because they can contain process memory.

Readiness, unit tests, xr-sim, and xr-tape can check specific contracts. They do
not prove the game rendered the right view or that a headset experience is good.

## Build From Source

Developers need Git, CMake 3.24+, and Visual Studio/MSVC with C++20 and Windows
SDK support. CMake fetches pinned MinHook and OpenXR SDK dependencies, so first
configuration needs network access (or populated dependency caches).

From a suitable Visual Studio developer shell:

```powershell
cmake -S . -B build-openxr -A x64 -DSOMAVR_ENABLE_OPENXR=ON
cmake --build build-openxr --config Release --parallel
ctest --test-dir build-openxr -C Release --output-on-failure
```

The default CMake option without `SOMAVR_ENABLE_OPENXR=ON` is a telemetry build,
not the headset build. Keep build flavor metadata with its matching DLL.

Create a package in a **dedicated disposable output directory**, never your
game directory, installed mod directory, or a folder containing captured logs:

```powershell
& ".\scripts\Package-Release.ps1" -BuildDirectory "build-openxr\Release" -OutputDirectory "dist" -IncludeDumper
```

The packager replaces generated staging and prunes older `SOMAVR-*` artifacts
inside that output directory. Treat it as build output, not archival storage.

Repository documentation: [current state](docs/CURRENT_STATE.md),
[feature ownership](docs/FEATURE_TRACEABILITY.md), [native address registry](docs/ADDRESS_REGISTRY.md),
[build history](docs/BUILD_HISTORY.md), and [hands bootstrap evidence](docs/HANDS_BOOTSTRAP_RE.md).
The complete RE documentation is in the repository; packaged documentation is
a smaller tester-focused subset. Graphify navigation is available locally via
`graphify-out/`; `Graphify-Update-CodeOnly.ps1` refreshes the deterministic code
graph while preserving existing document nodes.

## Attribution And Distribution

SOMA and its game assets belong to Frictional Games. This project is unofficial
and is not affiliated with or endorsed by Frictional Games. A licensed copy of
the game is required; do not redistribute the executable or game assets with
the mod.

This work draws on the in-house VR modding playbook and prior VR-mod research,
including SS2VR, BioShockVR, UEVR, and other source-available projects. Released
HPL2 source is GPL-licensed reference material; it has been used only as
reference for reverse engineering, and no HPL2 source is copied into or derived
within this project's code.

SOMAVR's own source and documentation are released under the MIT License --- see
[LICENSE](LICENSE). That license covers this mod only; it grants no rights in
SOMA or any Frictional Games asset. Third-party components linked into or
distributed with the built binaries (MinHook, BSD 2-Clause; the OpenXR SDK and
loader, Apache 2.0) carry their own terms, reproduced in
[THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md).
