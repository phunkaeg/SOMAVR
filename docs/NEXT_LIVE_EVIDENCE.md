# Next Live Evidence

Date: 2026-07-18

Use `out\SOMAVR-latest` (`0.67.0-native-manipulation-overlay`) for the next run.
The story-object route remains accepted. Concentrate on the mug/DSLR, curtains,
and starting laptop: distant props must complete their pull-in and then follow
the hand through the new bounded travel range; curtains must respond to held-
trigger hand drag without snap turn; and the laptop must appear as a head-locked
duplicate whose cursor follows either controller continuously.

Required markers are `hpl_terminal_pointer ... route=head_cone`,
`hpl_hud_gui ... terminal={match=1 captured=1}`,
`hpl_grab_anchor ... attachToHand=1 orientationTarget=1`,
`hpl_grab_rotation ... absolute_controller_orientation_bounded_error`,
`hpl_grab_translation ... unbounded_pull_in_plus_bounded_controller_travel`,
`hpl_manipulation_motion entered state=Slide(4)`,
`hpl_terminal_pointer ... owner=manager_world_input_0x170`,
`hpl_read_presentation ... native_pickup_travel_full_axis_controller_orientation`,
`inspection_exit`, and paired
`hpl_interaction_owner_lock active=1/0`. Confirm the established stereo,
tracking, eye height, shadows, reflections, context icon, and locomotion first.
Any poor direction or strength should be reported with the object/mechanism and
hand used; all new mappings have independent INI rollback keys.

SOMAVR has reached the point where the feature registry contains no unimplemented
VR system with enough static evidence for another responsible native mutation.
The remaining gates require headset, campaign-state, controller, runtime, or GPU
evidence. Run these passes in order; each pass is designed to settle several
project-phase rows from one log.

## Pass A: 0.56 Authored Camera Handoffs

Use the packaged `0.56.0-authored-camera-handoff` build, load a normal save, and
press F10 once. Exercise a sit sequence, ladder or climb, and one interactive
camera animation or hand-attached sequence.

Required evidence:

- rigid normal stereo and established eye height before each transition;
- `authored_camera_ownership_changed` preserving `tracking=1 stereo=1`;
- increasing `calibrationGeneration`, followed by fresh base/history captures;
- one `hpl_player_state_comfort` ownership transition and one blackout request;
- gameplay input released during authored ownership and restored afterward;
- no stale AO, ImageTrail, exposure, shadow, reflection, or FOV state.

This pass directly advances phases 8.1 through 8.4 and validates the generic
handoff used by sit, climb, conversation, animation, and hand attachment.

## Pass B: Temporal And Same-Frame Stereo

Enable same-frame stereo from F1. Visit one dark-to-bright route, one reflective
and shadowed room, one authored fade or grading transition, and any scene that
activates ImageTrail. Repeat once in AFR.

Required evidence:

- previous-view restore/capture pairs agree on eye, pose, and generation;
- ToneMapping replay and committed-restore counters rise together;
- temporal SSAO restore/commit and phase replay counters rise without faults;
- ImageTrail allocates distinct eye resources only when the effect activates;
- no binocular rivalry, stale-eye flash, doubled animation speed, or regression
  in the proven centered shadow/reflection path;
- representative per-eye CPU and GPU timing rows for AFR and same-frame modes.

This pass is the live gate for phase 2.13 and the promotion decision for
same-frame stereo. Static shader/resource RE is complete unless the log reveals
a new mutable owner.

## Pass C: Gameplay VR Systems

Exercise walking at partial/full stick, smooth and snap turn, physical crouch,
room-scale near a wall and moving door, interaction focus, one/two-hand grab,
wheel/door/lever manipulation, flashlight, terminal, inventory, pause, death,
and one tracked tool.

Required evidence:

- correct interaction profiles and two fresh controller pose streams;
- native analog locomotion only in Normal/Normal ownership, with semantic
  fallback elsewhere;
- no capsule tunnelling or body catch-up while authored camera ownership is set;
- controller ray hit depth/semantic icon agreement and stable reticle depth;
- native physics remains authoritative for grab, torque, throw, and mechanisms;
- light/hard grabbed-object impacts produce speed-scaled initiating-hand pulses,
  duplicate material callbacks are cooled down, and unrelated impacts stay
  silent; preserve native impact sound, particles, collision, and gamepad rumble;
- HUD/current-ImGui ownership is limited to its confirmed semantic surfaces;
- listener orientation follows the HMD during a directional near-field source.

This pass covers the remaining locomotion, hands, HUD, terminal, presentation,
audio, haptics, and accessibility acceptance rows.

## Pass D: Lifecycle And Hardware

Run doctor, launch, load another map/save, pause/unfocus, briefly interrupt HMD
tracking, change a graphics/window setting, and exit normally. When practical,
repeat on another OpenXR runtime or headset.

Required evidence:

- resource recreation or recovery without process restart;
- bounded tracking-loss zero-layer path and one recovery blackout;
- no stale camera, renderer, history, GL context, or swapchain identity;
- clean lifecycle summaries and no lingering SOMA process;
- stable desktop spectator output and no graphics-proxy conflict;
- runtime/headset/GPU/Windows identifiers recorded with the full log.

This pass gates release regression, runtime recovery, graphics resize, hardware
matrix, and release-candidate phases. `0.57.0` now provides opt-in fixed
foveation plus exact capability/application telemetry. Compare levels 0 through
3 only after collecting baseline GPU timing; upscaling remains unimplemented
until a measured bottleneck and compatible OpenGL ownership path justify it.
