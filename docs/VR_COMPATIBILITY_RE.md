# VR Compatibility Reverse-Engineering Map

## 0.33.0 Compositor Depth And Resource Recovery

The capture-only depth experiment is now a guarded submission path. When
`XR_KHR_composition_layer_depth` and a standard depth or depth-stencil format
are available, each eye receives a same-format depth
cache and OpenXR depth swapchain. The AFR depth copy uses nearest filtering and
the submitted projection views chain `XrCompositionLayerDepthInfoKHR` with
`minDepth=0`, `maxDepth=1`, and HPL near/far converted from world units to
meters. Any negotiation, cache, copy, or projection failure preserves the
proven color-only path for that frame. Negotiation prefers depth-stencil when
the live default framebuffer reports stencil bits, avoiding incompatible GL
depth blits while retaining depth-only fallbacks.

This convention is no longer inferred from framebuffer samples alone. Ghidra
at `0x140270230` and `0x14026fcf0`, plus the matching HPL2 projection source,
prove finite standard OpenGL depth: near/far map to normalized depth `0/1`.

The runtime also records the HDC/HGLRC used to create the OpenXR session. A
changed graphics binding enters the existing delayed full-runtime recovery.
Every 300 game frames it re-enumerates stereo view recommendations; changed
dimensions or sample limits transactionally rebuild spaces, color/depth
swapchains, caches, and layer resources. Stable checks do no GL allocation.

## 0.29.0 Loading, Video, And Optics Result

Authored script zoom no longer needs to distort the headset projection: exact
FOV, FOV-multiplier, and aspect-multiplier wrappers can retain native script
timing while selecting VR-neutral targets. Exact loading visibility now owns a
zero-layer XR interval with stereo-cache invalidation on both edges and input
release throughout. Video playback remains untouched while native stream names
and lifetimes are collected to separate fullscreen presentation from diegetic
screens.

## 0.28.0 Authored Comfort Evidence

The shipped player-state and camera-roll enums now have exact native policy
boundaries. Active VR may suppress only Lean/Move/Climb roll while preserving
Script roll, and may reject world DoF activation plus named VideoDistortion.
Short compositor-black guards cover entry/exit for states `11`, `12`, `14`,
`15`, and `17`; camera animation, constraints, FOV, and state ownership remain
native. The next log must prove each transition, roll route, DoF request, and
effect identity before any state-specific pose composition is promoted.

## 0.27.0 Gameplay Coherence Evidence

The controller flashlight's rendered matrix and low-frequency AI/gobo physics
rays now share one cached tracked basis. The bridge preserves each native random
cone sample and ray length, so controller yaw/pitch/roll changes gameplay aim
without collapsing SOMA's spread or altering tool and grounding ray callers.

Room-scale head-volume queries may now include dynamic bodies through the
existing native filter. This improves moving-door coherence but remains a
sampled volume whose jitter and authored-camera behavior need live validation.

## 0.32.0 Viewport Ownership And Depth Capture

Ghidra and the matching HPL2 layout confirm `cViewport` camera/world/render
ownership at `0x140298630`. `0.32.0` samples that packet at the render boundary
and correlates it with both `HPLPlayerState::camera` and the active VR camera.
The camera frustum hook uses the same invariant before reading any VR hotkey:
when the exact player camera is known, every different camera returns its native
frustum immediately. This closes the observed route by which a reflection,
terminal, water, or shadow camera could consume F10 or inherit headset pose.

The depth experiment now allocates one `GL_DEPTH_COMPONENT24` texture beside
each eye color cache and blits source depth with `GL_NEAREST`. Periodic
`openxr_depth_cache_probe` rows report source depth bits, GL errors, finite
center minimum/maximum, and eye identity. This was the capture-only evidence
gate subsequently promoted in `0.33.0`.

## 0.26.0 GPU And Depth Evidence

`HPLCompatibilityProbe` now issues nested-safe start/end `GL_TIMESTAMP` queries
for viewport, world, callbacks, post effects, post-post, and screen GUI. It never
blocks for results and preserves AFR eye attribution at stage completion. Read
`hpl_per_eye_cpu` and `hpl_per_eye_gpu` together to compare representative frame
budgets and identify stages whose side effects prevent same-frame repetition.

The same build conditionally enables `XR_KHR_composition_layer_depth` and logs
HPL near/far plus default framebuffer depth facts. This is capability discovery,
not depth submission; per-eye depth ownership and conversion remain required.

## 0.25.0 Dual-Render Budget Evidence

The six existing viewport/world/callback/post/post-post/GUI hooks now accumulate
QPC duration for every call and attribute it to the active AFR eye reported by
`HPLCameraBridge`. Periodic `hpl_per_eye_cpu` rows expose left/right/mono calls,
average microseconds, and total milliseconds without adding another executable
hook. These totals can reject an obviously unaffordable same-frame dual render,
but they do not include GPU completion time. A future nonblocking OpenGL
timestamp-query ring must measure GPU stage cost without forcing synchronization.

Created: 2026-07-11. This document covers systems that can silently break when
the renderer becomes stereoscopic even if the basic HMD camera path is correct.
It complements `FUTURE_SYSTEMS_RE.md`, which covers locomotion, hands, HUD, and
post effects in greater detail.

Confidence labels:

- **Confirmed**: observed in SOMA source/scripts, HPL2 source, Ghidra, or a live run.
- **High-confidence**: multiple independent pieces of evidence agree.
- **Candidate**: a concrete probe or hook exists, but runtime ownership is pending.

## Deferred Reconstruction Compatibility

The `0.5.4` F6 captures narrowed the leading shadow/reflection defect from broad
AFR resource sharing to a specific camera-packet boundary. Direct temporal and
inverse camera uniforms alternate correctly between eyes, while the live deferred
shadow programs `942/944` and world reflection program `989` obtain inverse
projection/view and screen reconstruction values from OpenGL uniform blocks.

This distinction matters because stereo geometry can be correct while deferred
lighting reconstructs world positions from a mono-centered projection. The
resulting horizontal error should track the asymmetric projection-center offset,
matching the user's crosshair/shadow observation. `0.5.5` confirmed this directly:
F5 removed the eye disagreement while UBO offset `96` changed from opposite
`-0.242513/+0.242513` centers to `0/0`. Centered horizontal projection is now the
active compatibility policy.

The remaining shadow motion is no longer a stereo-convergence defect. F4 in
`0.5.6` isolates tracked head-center translation while preserving rotation and
IPD, testing camera-relative shadow/light ownership. The same build's F3 control
targets program `988`'s block-backed view-depth reflection fade to distinguish
the moving opaque boundary from reflection-map or mirrored-frustum clipping.

Live `0.5.6` results redirect both controls: translation had little effect, and
F3 patched the live fade path for `369` draws without a visible change. Pitch and
roll are the strong shadow correlation. The horizontal-only policy still carried
vertical projection offset `-0.193187`; `0.5.7` centers both tangent spans to test
the remaining deferred reconstruction mismatch before moving into per-light and
reflection-frustum ownership.

The next implementation should update the native HPL camera packet at its owner,
not patch arbitrary shader bytes. UBO comparison is an attribution tool to locate
that owner and to distinguish camera-packet mismatch from shared shadow maps,
reflection buffers, or temporal history.

## Shutdown Ownership

The exit minidump contained one SOMA main thread in OpenGL/Virtual Desktop
teardown and no SOMAVR worker. Ghidra confirms `cSDLEngineSetup` destroys Graphics
before `SDL_Quit`. OpenXR must therefore be released at engine-destructor entry,
while the OpenGL context and runtime binding are still valid. `HPLLifecycle`
implements this as an exact-signature guarded hook at `0x1403b16e0`. The `0.5.5`
guard omitted the leading `0x40` byte and failed closed; `0.5.6` uses the exact
installed sequence `40 53 48 83 EC 20 48 8D 05`. The live `0.5.6` run logged
shutdown begin/complete and SOMA then exited normally.

## Core Pose Contract

VR camera state should be composed from separate owners:

```text
body yaw + authored camera base + HMD local pose + per-eye pose
```

Each term has a different lifetime:

| Term | Owner | Updated | May scripts overwrite it? |
| --- | --- | --- | --- |
| Body yaw / capsule position | SOMA character body | Simulation update | Yes |
| Authored camera base | SOMA player state, animation, or camera entity | Simulation and scripted camera update | Yes |
| HMD local pose | OpenXR | Predicted display frame | No |
| Per-eye pose and asymmetric FOV | OpenXR | Per eye render | No |

The current `HPLCameraBridge` modifies the returned frustum and therefore already
keeps the HMD orientation outside the script-owned `cCamera`. Future work should
preserve that separation. Writing raw HMD orientation into the game camera would
allow sit, ladder, animation, conversation, and hand-attachment states to erase
or accumulate tracking.

## Native Interaction And Physics Ownership

### Pick ray

`script/utilities/Utility_PickBasics.hps` constructs the native interaction query
from `camera position + offset` along `camera forward`. It samples a small
cross-pattern, chooses the closest entity, calls native `CanInteract`, and applies
the entity's native distance policy. This is a strong existing interaction layer.

VR should replace only the query pose:

```text
controller aim pose -> native closest-entity query -> CanInteract/range checks
                    -> native interaction state and callback
```

Do not replace `CanInteract`, distance handling, focus state, or map-script
callbacks with a parallel VR interaction database. A head-gaze fallback can feed
the same query when motion controllers are unavailable.

`0.12.0` closes the query boundary at registered global wrapper
`SOMA_GetClosestEntity` (`0x1400cd750`). `Utility_PickBasics.UpdatePickCheck`
passes camera position plus offset, camera forward, ray length, interaction type,
LOS policy, and an output object to this wrapper. `HPLInteractionBridge` replaces
only the first two arguments with the dominant controller's tracked HPL world
pose. It requires interaction type `0`, an incoming origin near the current
native frustum origin, full aim tracking, fresh active input, and no detected
authored-camera owner. Any failed gate calls the original query unchanged.

This leaves native ray length, LOS, `CanInteract`, entity distance policy,
focused entity/body IDs, player-state dispatch, and map callbacks authoritative.
Bounded `hpl_interaction_ray` telemetry reports substitutions, hits, controller
origin/direction, and each fallback class. Live focus behavior remains the gate
before the feature is considered proven.

`0.19.0` also decodes the finalized native result rather than inventing a second
focus test. The wrapper writes entity `+0x18`, body `+0x20`, and distance `+0x28`;
the bridge validates distance against native ray length and publishes the HPL
world hit point with frame and hand identity. `0.20.0` submits that exact aim and
distance as a generic application-space OpenXR reticle, with age/tracking/range
guards and optional focus-change haptics. No-hit and invalid results clear both
snapshot and layer validity.

`0.21.0` observes the post-policy crosshair callback through registered global
script dispatch `0x140484ea0` and argument reader `0x1404851d0`. The exact
`eCrossHairState` selects the 34 native TGA assets named by `Player.hps`, broad
intent color, and a bounded focus-haptic profile while SOMA keeps all interaction
authority. The default cursor remains semantically ambiguous and deliberately
does not pulse. World occlusion remains a live-test question rather than a new
geometry query in this build.

### Semantic camera comfort

SOMA's shipped player enum separates camera adds for crouch, Bob, Shake, climb,
terminal, script, death, lean, crawl, Sway, and conversation. `0.19.0` hooks the
registered setter at `0x140159360` and, only while VR tracking is active, can pass
zero for Bob `1`, Shake `2`, and Sway `9`. All state-bearing channels remain
native. This is narrower than suppressing a final camera matrix and can be
accepted independently for locomotion, impacts, and authored sequences.

### Grab and throw

The grab state already solves object movement using native mass-aware PID force
and angular-torque controllers. Its target transform is currently reconstructed
from camera position, camera rotation, local grab offset, and held depth. Throwing
uses camera forward for impulse direction.

The `0.18.0` controller implementation now:

1. Feeds camera-relative controller translation into exact force-PID error
   `0x140238750` only for Grab state `1` and gains `400/0/40`.
2. Preserves native PID gains, force caps, mass behavior, collision, joints, and state exit.
3. Adds a shortest-arc controller orientation target only to exact torque tuple
   `40/0/0.4|0.1`, preserving native angular feedback, inertia, and caps.
4. Signature-guards AddImpulse thunk `0x14049c720`; a 350 ms one-shot Grab intent
   redirects native impulse direction to release velocity or grip forward, with
   optional bounded speed scaling.
5. Falls back to the original camera/impulse path when pose, state, or intent is invalid.

This keeps SOMA responsible for physics and map behavior while VR owns only the
desired hand pose and release direction. Live acceptance must verify axis sign,
settling behavior, one-shot impulse ownership, and mass-class scaling.

### Rotate, doors, wheels, and levers

`PlayerState_Interact_RotateBase` translates look or move input into signed hinge
motion, then uses native angular velocity and PID torque with inertia and impulse
limits. It can also steer the body/camera when the cursor reaches a screen edge.

For VR, derive the signed interaction amount from controller displacement around
the hinge axis, preserve the native torque solver, and suppress screen-edge camera
steering while an HMD is active. This is the same ownership rule as grabbing:
replace the input signal, not the physical response.

### Push and constrained interaction states

Push, ladder, and related states apply native movement limits, camera limits,
crouch/run modifiers, and character-body motion. Their constraints must remain
authoritative. Controller input should enter through the semantic action or state
route rather than directly moving a rigid body or capsule.

## Authored Camera Compatibility

SOMA uses several states that detach, animate, constrain, or re-parent the camera:

| State | Source evidence | VR policy |
| --- | --- | --- |
| Normal movement | Player body owns camera base | Body yaw plus unrestricted local HMD pose |
| Sit | `PlayerState_Sit.hps` detaches camera, uses matrix rotation, interpolates pose, then restores body yaw/pitch | Keep authored translation/yaw as base; remove forced head roll; never freeze tracking |
| Interactive camera animation | `PlayerState_InteractiveCameraAnimation.hps` follows animation nodes and later moves the character to the camera | Preserve animation/world translation; layer HMD orientation and bounded local translation |
| Hand-bone camera attachment | `PlayerHandsHandler` can attach camera to a hand bone and disable normal body camera updates | Treat bone result as authored base; attenuate violent translation/roll; retain HMD orientation |
| Ladder/climb | Ladder state forces body positions, yaw, pitch/yaw limits, roll, and hand animation | Preserve locomotion constraints; suppress forced roll/head bob; keep independent HMD look |
| Conversation | Conversation code rotates toward a target and changes FOV/limits | Rotate the body/base toward target, not the physical head; use comfort-safe FOV policy |
| Dead/game-over | State and menu transitions may detach input and camera | Keep tracking live and present UI as a stable layer |

The bridge needs a small state adapter, not a new camera controller. Runtime
telemetry should capture player state, move state, camera rotation mode, camera
owner/parent, and whether character-body camera updates are enabled. Those values
will explain most camera incompatibilities without guessing from pixels.

`0.7.2` implements the first adapter boundary. Ghidra and the registered script
wrappers confirm camera rotate mode at `cCamera+0x6c` and body camera-update
ownership at `iCharacterBody+0x1e8`; matching HPL2 source confirms Euler mode is
zero. `HPLPlayerState` logs transitions and marks matrix mode or disabled body
updates as authored ownership. `HPLInputBridge` then releases injected movement,
turn, run, jump, crouch, and interaction while keeping menu/recenter live. This
does not yet alter pose composition: HMD tracking remains in the proven frustum
bridge while live tests classify each state.

## Audio Listener Pose

HPL2 `Scene.cpp::PostUpdate` updates the listener from the current viewport's
camera object. SOMA's HPL3 path reaches
`FMOD::EventSystem::set3DListenerAttributes` through `0x140289340`:

| Address | Evidence |
| --- | --- |
| `0x140289340` | Copies listener position, velocity, forward, and up fields; validates them; calls FMOD; then calls `EventSystem::update`. |
| `0x14061d188` | Imported `FMOD::EventSystem::set3DListenerAttributes` call target used by the live update path. |
| `0x14048b2d4` | Registers `SetCurrentListener(cViewport@)` for scripts. |

The current frustum-only HMD bridge does not change the `cCamera` object, so audio
is expected to remain aligned with SOMA's authored/mouse camera. That is a useful
side effect for script compatibility but incorrect for head-relative sound.

Runtime result and implementation:

1. `0.5.1` confirmed listener forward/up remain authored while HMD orientation changes.
2. `0.5.2` composes the physical HMD delta onto authored forward/up only during the signature-guarded engine commit.
3. SOMA's listener fields are restored immediately after FMOD copies them.
4. Left/right eye positions are never used for audio.
5. `0.11.0` adds the proven room-scale head world offset to listener position
   only for the FMOD commit and restores the native field immediately.
6. Listener velocity remains native pending Doppler validation.

## Loading, Menus, And Video

SOMA's normal load screen is an ImGui/menu composition. `MenuHandler.hps` draws a
backdrop, save screenshot, loading bar, logo, and press-to-start state. Script APIs
registered near `0x1400edb2a` through `0x1400edd19` control force-background,
small-icon, icon duration, and bar geometry.

The binary also contains HPL Theora stream classes and registers
`CreateVideo(const tString&)` and `DestroyVideo(iVideoStream@)` near
`0x14048c89f`/`0x14048c8e4`. The installed game has no standalone `.ogv`, `.mp4`,
or similar movie files; most named gameplay videos are texture/GUI compositions.

Presentation policy:

| Content | Default treatment |
| --- | --- |
| Load screen, main menu, game-over menu | Flat head-locked quad layer |
| True `iVideoStream` playback | Flat layer until a world-surface owner is identified |
| Terminals, datamining displays, train/omnitool sequences built in world GUI | Keep in stereo world space |
| Save screenshot used by load UI | Flat layer with the load UI |

OpenXR frame pacing must continue through loading if SOMA continues presenting.
If scene rendering pauses, submit the last valid world frame or a neutral clear
behind the UI without re-running game simulation.

`0.8.0-resilience-comfort` implements the first transition guard: when the
player-owned camera pointer changes after VR activation, SOMAVR suspends stereo
submission, clears both AFR eye caches, adopts the replacement camera, and runs
the stable-pose latch before resuming. OpenXR session and swapchain ownership are
kept alive. A separate delayed recovery path rebuilds OpenXR after session or
instance loss. Dedicated load-screen/video extraction remains future work.

## Same-Frame Dual Render Boundary

The proven native order is:

```text
main update / script OnDraw
  -> 0x140298850 enumerate active viewports
    -> 0x140298630 render one viewport
       -> 0x1401f9790 scene render
       -> 0x140297670 viewport renderer callbacks
       -> 0x14033bd80 active post-effect composite
       -> 0x1401f1480 deferred/PostPostEffects renderer phase
       -> 0x1402981e0 GUI sets
  -> script OnPostRender
  -> SwapBuffers / OpenXR submission
```

`0x140298850` also increments a renderer frame counter and resets global render
statistics before enumerating viewports. It must run once per game frame.
`0x140298630` is controllable through a render mask (`1` scene/post work, `2` GUI,
`4` post-effect enable), but calling it twice wholesale also repeats viewport
callbacks and the stateful `PostPostEffects` renderer phase.

### Proposed render transaction

```text
update once
capture authored camera base once
locate OpenXR views once for predicted display time

for left eye, then right eye:
  bind eye scene target
  apply eye view/projection
  select eye-local temporal history
  render scene + required world callbacks + post chain
  resolve eye image

restore authored camera/render state
draw flat GUI once into HUD target
submit two projection views + optional quad layer
present desktop mirror once
```

`0.5.1-compatprobe` adds the bounded call-count/FBO probe around
`0x1401f9790`, `0x140297670`, `0x14033bd80`, `0x1401f1480`, and `0x1402981e0`.
It records call order, viewport identity, mask bits, source/destination FBOs,
dimensions, and stage duration. This determines which
callbacks are render-only and which must remain once-per-game-frame.

`0.31.0` also emits one bounded `hpl_render_transaction` row for the complete
sampled frame. It collapses nested entries into ordered stage multiplicities and
per-stage draw, clear, and CPU totals. A canonical frame with draw-free callback
and post-post stages is reported only as `repeatCandidate=world_stage_only`; the
row deliberately retains `proof=callback_side_effects_still_require_controlled_replay`.
`0.35.0-dual-render-probe` implements that reversible experiment. `Ctrl+F6`
arms only the next exact player viewport. After its native first-eye render,
SOMAVR immediately preserves the pending eye in the OpenXR cache, clears only
mask bit `2`, and directly invokes `0x140298630` once more. This bypasses the
once-per-frame enumerator at `0x140298850`; update, script lifecycle, frame
counter/stat reset, GUI, submission, and presentation are not repeated.

The second call still repeats pre/post world callbacks, overlays, active post
effects, and the unconditional `PostPostEffects` renderer phase. Telemetry therefore
records the exact replay mask, CPU/draw/clear cost, first/second eye indices,
pose-frame identity, and explicitly labels duplicated post-post work. Any
eligibility or immediate cache-capture failure consumes the arm without replay
and leaves normal AFR in control.

Decompilation of `0x1401f1480` corrected the earlier callback-only assumption.
The phase performs substantial deferred GPU work, invokes callback lists, clears
renderer state at `+0x69`, and copies `0x40` bytes from current state
`*(renderer+0x20)+0x158` into history state `*(renderer+0x438)+0x80`. `0.40.0`
therefore adds three automatically spaced, one-frame samples after stable VR
activation. `HPLDualRenderDiagnostics` snapshots bounded readable regions for
the renderer (`0x800`), current state (`0x200`), history state (`0x100`), and
settings (`0x180`) before and after each first-eye/replay phase. It logs hashes,
changed-byte ranges, and whether both eyes mutate equivalent ranges. It does not
restore or alter any captured state.

`0.45.0-continuous-dual-render` promotes this exact boundary into an explicit
opt-in sustained prototype. `HPLDualRenderContinuousControl=1` exposes the F1
panel action while `HPLDualRenderContinuousDefault=0` keeps AFR as the startup
path. The exact player viewport is replayed every eligible frame, screen GUI is
still removed from eye two, and the once-per-frame enumerator and upper engine
lifecycle remain untouched. Ordinary continuous frames do not take mutation
snapshots; manual and automatic arms temporarily retain diagnostic precedence.
Any immediate cache failure or eye/pose-sequence mismatch disables continuous
mode and returns to AFR. Live evidence now gates promotion to a default, rather
than the existence of a sustained implementation.

`0.46.0-per-eye-view-history` implements the first eye-local temporal resource.
The exact `0x40` copy at `0x1401f1480` is the previous-view matrix: active
frustum view `+0x158` becomes history object `+0x80`. The camera bridge publishes
the pending eye/pose before each viewport, so SOMAVR restores that eye's bank
before scene/culling/post work and commits after the complete viewport only if
the rendered eye agrees. First use and renderer/history replacement seed both
banks from native state; failures immediately retain shared native history.

`0.47.0-stereo-view-history` applies the transaction to the proven AFR path too,
where native shared history otherwise alternates eye ownership each game frame.
It also treats recenter generation and pose gaps over eight frames as camera cuts,
reseeding both banks from live state before rendering resumes.

Temporal resources such as image trail, previous projection matrices, and
velocity history must either be isolated per eye or disabled. Exposure is now
classified differently: `0.53.0` owns it as shared authored frame state that
advances once per same-pose stereo pair. Sharing one camera-dependent history
between alternating eye transforms still produces cross-eye contamination.

## Additional High-Value RE

| Area | Why it matters | First probe |
| --- | --- | --- |
| Player state identity | Selects camera and interaction compatibility policy | Log state/move-state transitions and camera mode |
| Pause and focus behavior | OpenXR may keep running while SOMA time is stopped | Compare update, render, and swap cadence while paused/unfocused |
| Save/load transitions | Camera and world pointers are replaced | Log viewport/camera/frustum identity changes and reset VR caches |
| Culling and shadows | Per-eye frusta can omit objects or duplicate expensive work | Compare cull counts and shadow passes by eye |
| Water/reflection cameras | Secondary cameras must not receive player-eye pose | Classify every `cCamera::GetFrustum` caller/viewport |
| Dynamic resolution and resize | Eye targets must survive mode changes | Log viewport/FBO dimensions and resource recreation |
| Damage/death comfort | Screen shake, roll, fades, and chromatic effects intensify | Attribute effects to state and apply policy table |
| Controller haptics | Native interaction events already encode contact/action semantics | Observe grab, impact, damage, tool, and UI event boundaries |
| Recenter and height | Seated/standing users need stable body/head calibration | Persist origin mode, height offset, yaw recenter, world scale |
| Performance telemetry | Stereo roughly doubles expensive scene work | Per-eye CPU/GPU timing for scene, shadows, post, GUI, copy, submit |

## Deferred Shadow Stability

The `0.5.2` live test redirects the principal visual defect from post effects to
deferred lighting. Realtime shadows differ between eyes and swim with player
motion even when F12 bypasses all active post effects.

SOMA's installed `deferred_light_frag.hpsl` exposes two independent risk groups:

- soft filtering uses screen-coordinate jitter and `avShadowMapOffsetMul`;
- directional shadows select `a_mtxLightViewProj0..3` with near/far split vectors.

`0.5.3-shadowjitter` tests the first group without changing the second. F7 zeros
only the authored scatter radius. A successful test should make edges harder and
more binocularly stable. Remaining large-scale movement points to cascade fitting,
light matrices, or AFR-age differences and requires per-eye shadow-map/FBO
attribution before any matrix cache is attempted.

## Reflection Stability

SOMA has two materially different reflection routes. Environment maps calculate
the reflected direction from a camera-space eye vector and `a_mtxInvView`, so the
result must be evaluated with the correct eye matrix. Water/world reflections
sample a screen-space `aReflectionMap` rendered from a mirrored main frustum.

The HPL2 implementation uses a shared reflection framebuffer and deliberately
does not clear its reflection texture before each object pass. If HPL3 retains
that ownership, alternating eyes can consume a reflection generated for the
opposite eye or previous game frame. F6 captures the live generated shader,
matrix stream, draw/FBO order, and eye identity needed to prove or reject this.

## Recommended Implementation Order

1. Prove F11 eye ordering, scale, FOV, and cache submission.
2. Add render-stage/FBO telemetry without duplicating work.
3. Live-validate the opt-in `0.45.0` same-frame scene/post replay, especially the
   necessarily duplicated stateful post-post phase and performance budget.
4. Split flat HUD/menu from world GUI.
5. Add player-state telemetry and authored-camera adapters.
6. Add semantic OpenXR actions and locomotion.
7. Route controller poses into native pick/grab/rotate ownership.
8. Validate the `0.5.2` center-head audio listener correction with a directional source.
9. Use F7 to classify soft-shadow jitter, then trace cascade matrices if large-scale mismatch remains.
10. Re-enable compatible post effects with per-eye history; F12 has already excluded the chain as the primary shadow owner.
11. Add haptics, comfort presets, and performance tuning.

## Redirect Criteria

- If a candidate render hook repeats game/script updates, move deeper into the renderer.
- If a camera hook affects reflections, terminals, or shadow cameras, tighten viewport/caller selection.
- If native grab physics becomes unstable, restore its original target path before tuning PID gains.
- If listener correction changes scripted audio placement, hook later at FMOD commit and preserve authored position.
- If a GUI capture removes diegetic screens from the world, classify GUI sets by viewport/owner rather than globally.
