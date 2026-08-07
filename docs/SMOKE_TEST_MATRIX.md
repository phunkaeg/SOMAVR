# SOMAVR Smoke Test Matrix

Use this matrix for every feature build before broad campaign testing. Record the
save/checkpoint used for each row once a stable representative location is
chosen. A failure in a stop condition blocks promotion even if the new feature
appears to work.

| ID | Save/checkpoint | Exercise | Required evidence | Stop condition |
| --- | --- | --- | --- | --- |
| S01 | Startup and main menu | Run packaged doctor, launch through packaged injector, wait at menu, then load a save | Doctor `fail=0`, correct version/flavor, loader/runtime/config success, hook install rows, no signature failure | Doctor failure, crash, wrong DLL flavor, missing loader/runtime/config, or input behind menu |
| S02 | Quiet lit room | Press F10, inspect stereo and stable desktop eye, translate/rotate HMD, lean into static walls/corners/ceiling edges and a moving door, then inspect reflective and shadowed surfaces | VR activation, alternating headset eyes, stable selected spectator eye, zero projection offset, dynamic-inclusive head-volume rows, submitted frames | Skew, scale drift, eye mismatch, desktop alternation/corruption, wall/dynamic clipping or stuck clamp, moving shadows/reflections, or wrong eye height |
| S03 | Same room with light-sensitive target | Toggle and aim flashlight independently through yaw/pitch/roll; observe agent/gobo response; briefly lose hand tracking | Exact `Flashlight` identity, visual override plus cone-preserving gameplay-ray rows, native fallback then recovery | Visual beam or gameplay response remains camera-locked, stale, reversed, divergent, or moves world/camera |
| S04 | Mixed interactables | Sweep controller over pickup, button, door/lever, terminal, and unavailable target | Controller ray substitutions, native semantic reticle states, bounded focus haptics | Gaze still owns focus, wrong callback, stuck icon, or repeated haptic chatter |
| S05 | Door/wheel/slider/lever/tear | Hold interaction and move only the dominant hand, then move HMD and hand together | State `3..7`, bounded manipulation deltas, common-translation cancellation | Initial jump, wrong axis, object motion from shared room-scale translation |
| S06 | Physics pickup area with independent held tool | Grab, translate, and rotate light/heavy objects; hold support squeeze and move both grips to steer the object/tool; release support squeeze, then release and throw | Exact HudObject support-basis rows, bounded two-hand Grab engagement/substitution/release rows, force/torque PID matches, tracked targets, one-shot redirected impulse | Tool/object jumps on support engage/release, explosive force, persistent impulse patch, broken collision/joint, stuck grab, or two-hand control outside squeeze/separation gates |
| S07 | Subtitles, inventory, and pause | Trigger short/long subtitles, open inventory and pause, aim/click menu, toggle HUD quad/curved shape, close while trigger is held | Scoped subtitle layout rows with restoration, gameplay suppression, native menu pointer, pause-gated current-ImGui HUD capture, stable shape-toggle telemetry | Changed subtitle content/timing, invalid layout fallback, movement behind UI, accidental resume click, missing/distorted HUD, shape fallback failure, broad ImGui capture, or eye-local menu split |
| S08 | Authored camera event | Trigger sit/conversation/ladder/climb/camera animation/terminal zoom and move HMD throughout | Exact state-transition rows, bounded black guards, semantic roll/optics rows, controller suppression, live head tracking | Frozen head, persistent blackout, forced roll/FOV/aspect, double rotation, controller movement, or beam override |
| S09 | Map transition/loading/video | Load another save or cross a level boundary, exercise one video, and wait for gameplay | Exact load entry/exit, opaque-black projection with `layers>=1`, input release, eye-cache invalidation, automatic stereo resume, named video create/destroy rows | Any zero-layer submit, stale eye/input, black hang, manual F10/F11 required, loading shown at bad depth, or changed video playback |
| S10 | Damage/death/wake | Take damage, trigger a scripted screen material, death/game-over, reload, and complete wake transition | Exact screen-particle create/position/destroy rows, dead-state guard, camera add/roll, DoF and named post-effect policy rows, UI identity, tracking remains live | Near-field convergence, changed unrelated billboard, nauseating trail/distortion/blur/roll, invisible prompt, persistent blackout, frozen tracking, or stale effects |
| S11 | Tracking/runtime interruption | Remove HMD/controller tracking, change runtime focus, then restore | Pose-age hold/expiry, retained/black projection with `layers>=1`, upcoming-pose fallback counts, release of held inputs, recovery blackout, correct per-hand interaction-profile rows | Any zero-layer submit, stale pose/profile, stuck input, visible invalid frame, session restart loop, or unresolved active controller profile |
| S12 | Normal exit | Quit from gameplay and from menu on separate runs | Pre-graphics shutdown begin/complete, per-eye CPU rows, spectator counters, and final summaries | Lingering process, graphics teardown crash, missing timing/mirror evidence, or missing bounded summaries |
| S13 | Stereo-history and dual-render scenes | In F10 AFR, recenter and recover from a long tracking/load gap; then enable `SAME FRAME STEREO` in quiet, shadowed, reflective, tone/bloom, fade, HUD/menu, and post-heavy views | AFR and continuous per-eye previous-view restores/captures, one reseed per calibration/gap, same-pose opposite-eye replay, GUI bit removed, one upper lifecycle, clean native rollback | Crash, history fault/mismatch/reseed loop, extra upper lifecycle, simulation advance, duplicated GUI, persistent temporal artifact, unacceptable pacing, or broken AFR fallback |
| S14 | Locomotion comfort | Walk at partial/full stick, release, compare snap/smooth turn, then open F1/pause/terminal/loading/authored-camera/dead states | Dedicated vignette swapchain, smooth level telemetry, clear center, peripheral-only mask, suppression release, F1/preset control, clean recovery | Mask over center, menu-triggered mask, stuck opacity, snap-stick sustained vignette, eye-local mask, world/HUD motion, resource leak, or rollback failure |

## Build Record

For each tested artifact record:

```text
Version:
OpenXR DLL SHA-256:
Package ZIP SHA-256:
Runtime/headset:
GPU/driver:
Passed rows:
Failed rows and log timestamps:
```

Retain the corresponding `somavr.log`, build manifest, and any dump under the
same test-run label. Promote a build only after S01-S03, S07, S09, S11, and S12
pass; feature-specific rows must pass before that feature is marked proven.
