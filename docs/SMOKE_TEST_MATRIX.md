# SOMAVR Smoke Test Matrix

Use this matrix for every feature build before broad campaign testing. Record the
save/checkpoint used for each row once a stable representative location is
chosen. A failure in a stop condition blocks promotion even if the new feature
appears to work.

| ID | Save/checkpoint | Exercise | Required evidence | Stop condition |
| --- | --- | --- | --- | --- |
| S01 | Startup and main menu | Launch through packaged injector, wait at menu, then load a save | Correct version/flavor, loader success, hook install rows, no signature failure | Crash, wrong DLL flavor, missing loader, or input behind menu |
| S02 | Quiet lit room | Press F10, inspect stereo and stable desktop eye, translate/rotate HMD, lean into static walls/corners/ceiling edges, then inspect reflective and shadowed surfaces | VR activation, alternating headset eyes, stable selected spectator eye, zero projection offset, nine-probe room-scale rows, submitted frames | Skew, scale drift, eye mismatch, desktop alternation/corruption, wall clipping/stuck clamp, moving shadows/reflections, or wrong eye height |
| S03 | Same room | Toggle and aim flashlight independently through yaw/pitch/roll; briefly lose hand tracking | Exact `Flashlight` identity, override rows, native fallback then recovery | Beam remains camera-locked, stale, reversed, or moves world/camera |
| S04 | Mixed interactables | Sweep controller over pickup, button, door/lever, terminal, and unavailable target | Controller ray substitutions, native semantic reticle states, bounded focus haptics | Gaze still owns focus, wrong callback, stuck icon, or repeated haptic chatter |
| S05 | Door/wheel/slider/lever/tear | Hold interaction and move only the dominant hand, then move HMD and hand together | State `3..7`, bounded manipulation deltas, common-translation cancellation | Initial jump, wrong axis, object motion from shared room-scale translation |
| S06 | Physics pickup area | Grab, translate, rotate, release, and throw light/heavy objects | Force/torque PID matches, tracked targets, one-shot redirected impulse | Explosive force, persistent impulse patch, broken collision/joint, or stuck grab |
| S07 | Inventory and pause | Open inventory and pause, aim/click menu, close while trigger is held | Gameplay suppression, native menu pointer, HUD/ImGui identity rows | Movement behind UI, accidental resume click, missing HUD, or eye-local menu split |
| S08 | Authored camera event | Trigger sit/conversation/ladder/camera animation and move HMD throughout | Authored-camera transition rows, controller suppression, live head tracking | Frozen head, forced roll, double rotation, controller movement, or beam override |
| S09 | Map transition/loading/video | Load another save or cross a level boundary and wait for gameplay | Camera replacement, eye-cache invalidation, stable-pose recalibration, stereo resumes | Stale eye, black hang, manual F10/F11 required, or loading shown at bad depth |
| S10 | Damage/death/wake | Take damage, trigger death/game-over, reload, and complete wake transition | Post-effect identity/policy rows, UI identity, tracking remains live | Nauseating trail/blur/roll, invisible prompt, frozen tracking, or stale effects |
| S11 | Tracking/runtime interruption | Remove HMD/controller tracking, change runtime focus, then restore | Pose-age hold/expiry, zero-layer path, release of held inputs, recovery blackout | Stale pose, stuck input, visible invalid frame, session restart loop |
| S12 | Normal exit | Quit from gameplay and from menu on separate runs | Pre-graphics shutdown begin/complete, per-eye CPU rows, spectator counters, and final summaries | Lingering process, graphics teardown crash, missing timing/mirror evidence, or missing bounded summaries |

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
