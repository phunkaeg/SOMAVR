# September 7 Test Review: Player-Space Recovery

## Evidence Identity

Tested: `0.95.8-arm-palette-evidence+35bd7f2-dirty/openxr/Release` on
VirtualDesktopXR, September 7, 13:27:23-13:32:14. The current module-root log was
`out/SOMAVR-latest/logs/somavr.log`, not the old July root `logs/somavr.log`.
The input log, config, manifest and terminal images are preserved under
`logs/receipts/2026-09-07-0.95.8` before replacing the rolling package.

## Findings and Changes

### Player and Tracking Coordinate Spaces

Static producer/consumer proof: `GetHPLCameraBridgeStatus` computes legacy
`headWorldRotation` as `conjugate(neutral) * currentHmd`. This is relative to the
native camera, despite the field name. `ResolveReadPresentationView` and wrist
offset placement treated it as a scene orientation. A neutral HMD therefore
placed books along world -Z regardless of native player heading.

0.95.9 adds `headSceneOrientation = nativeCameraOrientation * relativeHmd`.
Read and wrist world-offset consumers use this explicit field. Input movement
retains the tracking-relative fields, whose actual convention is now documented.
The Read anchor retains only the original distance for placement; every update
resolves a current-view offset. Settled orientation is transported with the view,
with grip-relative rotation preserved. No per-frame distance multiplication.

`ResolveHorizontalYaw` measures clockwise heading from -Z, whereas a +Y
quaternion turns -Z counterclockwise. The torso conversion had the wrong sign.
Native snap deltas also had the opposite sign to this heading convention.
Corrected both and used the virtual torso for elbow-pole orientation as well as
root placement. Tests cover both heading signs, nonzero native headings, pitch,
and half-turn story placement. No new player-capsule turn is introduced.

### Wrists

Both hands seeded deterministic geometric palm anchors in this log, including
after reload. That path applied the configured -90-degree roll but omitted the
configured 45-degree pitch; only the fallback applied both. One shared helper
now applies both once and the seed log prints the settings. This does not prove
all weighted forearm twist bones deform naturally. That remains visual acceptance.

### Terminal Capture

The native 1024x577 RGB image already has a black email pane with only small
fragments. The fault precedes upscale and XR composition. Original source
viewports include atlas tiles such as `8,871,256,145` and `264,615,256,145`.
The old repair only remapped clips with zero overlap against the capture target.
That is not a coordinate-space test: a source atlas rect can accidentally overlap
the capture viewport and still need conversion.

0.95.9 remaps source-coordinate clips for the owning capture target regardless
of accidental overlap, restores exactly after each draw, and preserves empty
clips. Removed the clipping-disable fallback. Regression tests cover scaled
atlas clips, overlap, and zero-area clips. This is a candidate rendering fix,
not headset-proven. `originalIntersects` identifies coverage of the new case.

Playbook chapter 10's `translate-dont-disable-scissor` is relevant prior art,
but its older SOMAVR-derived instruction to leave overlapping clips unchanged
is precisely the case now under test. Only update that fleet rule after live
source/capture evidence confirms it; geometry overlap alone never proves space.

### Per-Eye Arm Lag and Save Loading

Runtime: all 289 sampled pairs report matching arm inputs and final 79-bone
palettes. This rules out differences at those sampled boundaries, not all
frames, CPU skinning output, dynamic VBO consumption or headset timing.

At 13:31:08 frame 11247, viewport world becomes null and loading invalidates
caches. Frame 11248 replays only 11 draws with stale first-eye metadata and
reports `eye_sequence_mismatch`, permanently disabling continuous stereo.
Later frames use AFR. 0.95.9 checks live viewport world and loading state before
requesting replay, rechecks after the first render, and treats a world transition
during replay as an expected abort. Unexplained gameplay mismatches still disable
the unsafe path. Reload recovery must be tested; no automatic retry masks faults.

The old Ctrl+F10 path only captured terminals. There are no arm images in this
session. The new chord captures two recent left/right scene-cache pairs, 15 game
frames apart, with pose-frame identities. It runs after closing the prior XR
frame and before the next wait, under the existing own-GL/readback state scope.
It is bounded, synchronous and may hitch; it does not capture XR overlay layers.

### Joint Feel

The drawer lane runs and shows roughly 6 cm initial position lag during the
sampled pull. Slide gains increase from velocity=1/position=12 to 1.25/18;
hinge velocity gain increases from 1 to 1.5. Native joint axes and travel/speed
limits remain unchanged. These are provisional feel adjustments, not a new
axis-assistance algorithm. Compare slow/normal and diagonal pulls next session.

## Acceptance and Verification

OpenXR Release build and all nine CTest suites pass. SOMA was not launched.
The packaged injector doctor reports `pass=9 warn=0 fail=0`, including target
hook signatures and the VirtualDesktopXR runtime. Rolling package DLL SHA-256:
`65C40B77CD50913FC827B3FDAA906649FB454B56574F60F93D26CED308065341`.
The deterministic code graph was refreshed (4,954 nodes, 8,352 links); semantic
document re-extraction was not run. Existing BuildInfo.cpp macro parsing warning
persists in Graphify, but the compiler and tests pass.
Follow `NEXT_LIVE_EVIDENCE.md`: wrists/torso, walking eye captures, story pickup
at different headings, toward/away email captures, joint feel and save reload.
Remaining risks: visual wrist deformation, mixed GUI clipping conventions,
downstream skinning/VBO lag, and real-runtime capture cost.
