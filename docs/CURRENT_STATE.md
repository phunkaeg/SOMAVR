# Current State

Date: 2026-09-10

## Objective

Bootstrap SOMAVR: a reverse-engineered VR mod for SOMA/HPL3, likely using DLL injection plus OpenXR.

## Initial Findings

- `D:\Dev Debug\SOMAVR` was empty at project start, so this folder is now treated as the mod workspace.
- SOMA is installed at `G:\SteamLibrary\steamapps\common\SOMA\`. The install includes `Soma_NoSteam.exe`, `Soma.exe`, `SDL2.dll`, `glew32.dll`, `_shadersource\`, and the game config folder.
- Ghidra confirms `Soma_NoSteam.exe` is a 64-bit Visual Studio 2010-era executable. The CRT entry calls `FUN_1401daec0`, which then calls the likely application entry `FUN_140125a20`.
- `FUN_140125a20` allocates an app/game object of size `0x8d0`, calls init at `FUN_14004b620`, then run/shutdown paths at `FUN_1400383e0` and `FUN_14003dd70`.
- `FUN_14004b620` logs `Version %d.%02d` with values `1, 0x6e`, matching a `1.110` style SOMA version string and making it a useful startup anchor.
- `config\game.cfg` sets the player camera defaults to `FOV="70"`, `FarClipPlane="1000"`, and `NearClipPlane="0.03"`.
- HPL2 source in `D:\Dev Debug\AmnesiaAMachineForPigs` and `D:\Dev Debug\AmnesiaTheDarkDescent` confirms the engine family uses SDL/OpenGL, `SDL_GL_SwapBuffers`, `glLoadMatrixf`, `glUniformMatrix4fv`, and fixed-function compatibility calls such as `glTexEnvfv`.
- First live log from `0.1.0-bootstrap` loaded successfully from `D:\Dev Debug\SOMAVR\build\Release\somavr.dll`, saw the NVIDIA OpenGL 4.6 context, and reached 2900+ frame summaries with no DLL errors.
- The first run proves the main scene projection is shader-uniform driven, not fixed-function: `fixedProjection={valid=0}`, while `uniformProjectionName="a_mtxModelViewProjection"` was projection-like with `fovYDeg=70.0000` and `aspect=2.3889`.
- Stable camera/config matches found so far: `config\game.cfg` has `FOV="70"`, `NearClipPlane="0.03"`, and `FarClipPlane="1000"`. Runtime matrices showed the 70 degree vertical FOV at the user's 3440x1440 viewport.

## Active Baseline

The rolling test package is now **`0.96.2-hands-bootstrap`**. It adds guarded,
campaign-owned creation before the vial through the hands module's native
PostUpdate script lifecycle. `HandBootstrap=1` is independent of the existing
retention/IK settings. The native receiver contract was corrected: module
updateable is a secondary base at `+0x110`, not the full script owner. No game
launch or headset acceptance has occurred for this build. See
`HANDS_BOOTSTRAP_RE.md` and the first test in `NEXT_LIVE_EVIDENCE.md`.

Previous build and retained interaction changes:

The previous headset candidate was **`0.96.1-interaction-follow`**. Built after the user
quit; not launched. It retains all 0.96.0 arm/depth work and profile tuning below.
Drawer gain now applies consistently to displacement and velocity; door leverage
uses the actual body-attached handle. Fast tracked trigger releases use a bounded,
exact-body native throw handoff instead of the speed-limited drop route. Slow
releases still place/drop. Per-grab motion and throw-outcome logs are included.
Release and all ten CTests pass; feel and player-recoil acceptance remain open.
See `INTERACTION_FOLLOW_REVIEW_2026-09-09.md` for evidence, limits and test protocol.

September 9 headset follow-up: the user reports both hands' fingers pointing up
with palm direction acceptable, and more consistent but too-small story objects.
The next-launch profile changes only existing settings: wrist pitch `45 -> -45`
(90 degrees downward, roll remains `-90`); Read distance scale `1.5 -> 1.2`
(20% closer); Read object scale `1 -> 2` (twice each authored linear dimension).
That profile-only edit left the then-running 0.96.0 session unchanged; the new
0.96.1 package retains it. Restart is required. This is
calibration tuning awaiting headset confirmation, not a new IK or Read hook.
The sampled live log already shows solved wrist goals with zero residual and
accepted, matching per-eye scene-depth copies. Full depth-image correspondence
and visual acceptance remain separate checks.

- Arm IK and wrist placement share one calibrated target and camera snapshot.
  The wrist consumes the successful solver's reachable endpoint instead of
  overriding its clamp. IK-off/read-failure fallback uses the calibrated target.
  Existing orientation calibration, two-hinge solver and frame guards remain.
  `hpl_wrist_goal` reports requested/selected/actual positions and residuals.
- A new `OpenXRSceneDepth.cpp` captures a validated single-sample depth
  renderbuffer at the exact player viewport's pre-post world-callback entry.
  No capture-local GL names are hardcoded. Default, incomplete, wrong-format,
  texture, multisample, viewport and depth-range mismatches fail closed.
- Color/depth pairing checks the render serial, pose frame and source viewport;
  each eye retains its own clip range. Desktop color copies no longer read depth.
- Ctrl+F10 includes per-eye float depth PFM and JSON metadata beside existing
  RGB captures. Readback is manual/bounded and restores PBO/pixel-pack state.
- Release defaults disable depth submission. The rolling test package explicitly
  enables only `DepthCompositionProbe=1` using `Package-Release.ps1 -SceneDepthTest`.
  Rendering to the headset remains color-only while the new source is evaluated.

Verification: Release compiled; all 10 CTests passed, including production GL
copy/readback in a hidden 64x64 context (no XR session), independent eye values,
source reuse, stale-serial rejection and state restoration. The subsequent live
run reached the player callback and captured D24S8 at matching serial/pose/viewport;
this alone is not full-image geometry/alignment or headset acceptance.
See `NEXT_LIVE_EVIDENCE.md` and the September 9 build-history entry.

### Evidence Behind This Build

September 9 evidence pass, before this implementation: existing apitrace replay
proves geometry depth in HPL's scene D24 attachment and matching sampled R16F
linear depth. The final default framebuffer is entirely far-plane depth in
both sampled frames, including ordinary gameplay. The 0.95.9 OpenXR copy
read that default buffer; format/blit success is not usable scene depth.
Live per-eye source identification/copy remains open. Full receipts and a
tested offline analyzer: `HPL_SCENE_DEPTH_PROOF_2026-09-09.md`.

PreyVR's latest arm IK is useful but still has headset-reported crosstalk and
reach/calibration issues. Its shared-goal pattern exposed a specific SOMA
conflict: the old arm solver used the raw controller point, then the wrist writer
added 6.4 cm of calibration offsets and ignored the solver's reach clamp.
The source-proven conflict is now corrected; visual acceptance remains open. Transfer priorities and
test contract: `PREYVR_ARM_IK_TRANSFER_2026-09-09.md`.

Project-only code Graphify refresh completed after 0.96.1 on September 9:
5054 nodes, 8537 links, preserving 925 document/concept nodes across 43 sources. The LAN semantic
runner remains unreachable, so September 9 document semantics are not yet
indexed, including the new interaction review; use the direct links here and in
`FEATURE_TRACEABILITY.md` for those.

The previous engineering and gameplay test build was
`0.95.9-player-space-recovery`. It responded to the September 7 test of 0.95.8:

- Corrects clockwise heading versus quaternion yaw signs for torso/snap turns;
  elbow poles now use the same virtual torso heading as shoulder placement.
- Adds an explicitly scene-world HMD orientation composed from native camera
  basis and tracking-relative rotation. Story placement and wrist position
  offsets use it; existing movement consumers retain their relative convention.
  Story distance remains latched once, but its direction follows the current
  player view, including turns after pickup. Settled orientation follows the
  view unless manipulated with grip.
- Applies the configured wrist pitch in the geometric calibration path as well
  as its fallback. This repairs an omitted calibration step, not a claim that
  all forearm twist/deformation is resolved.
- Remaps terminal source scissors even when they incidentally overlap the
  private capture target. Empty clips remain empty; disabling clipping is no
  longer a fallback. Full email rendering still requires headset acceptance.
- Gates replay on a live viewport world and no loading screen. The last log
  showed a world-less save-load pass disabling continuous stereo at frame 11248.
  Valid gameplay mismatches still fail closed; loading cannot disable the mode.
- Increases slide velocity/position gains to 1.25/18 and hinge velocity gain to
  1.5, retaining native joint axes, travel limits, and speed caps. Feel is untested.
- Extends Ctrl+F10 to two paired-eye RGB samples, 15 game frames apart, under
  `logs/eye-captures`. Pose-frame labels identify coherent versus AFR pairs.
  Existing terminal RGB/alpha captures remain. Readback can briefly stall.

The archived 0.95.8 log has 289 coherent sampled node/palette pairs, followed by
the loading-triggered stereo fallback. This narrows but does not close the
reported per-eye lag. No arm images were captured by the old terminal-only
hotkey. Evidence, limitations and next test: `TEST_REVIEW_2026-09-07.md` and
`NEXT_LIVE_EVIDENCE.md`. New build verification is offline only; SOMA was not
launched.

### Previous Evidence Build

The preceding engineering and gameplay test build was
`0.95.8-arm-palette-evidence`, layered on the
visually proven `0.9.0-calibration-haptics` OpenXR transport, native HPL camera
bridge, AFR stereo, full projection centering, one-key F10 activation, and
compatibility probes. It preserves every `0.95.6-regression-recovery` behavior
change and extends the bounded, read-only arm-render witness for the next
headset run:

- The visible-arm regression is now measured at the exact first-eye and replay-
  eye viewport boundary. For the shared arm root and both 15-node
  clavicle-to-wrist chains, the bridge records hashes before and after each
  render pass and compares the first pass with the replay pass. This separates
  three failure classes without changing a pose: different input matrices per
  eye, mutation during a render call, and mutation between passes. The sample
  policy covers the first eight eligible pairs and every thirtieth pair after
  that, so walking evidence is collected without turning every draw into a
  memory probe. Static HPL3 RE now confirms that `0x1401fe1a0` builds the final
  64-byte-per-bone palette at mesh `+0x340`, and `0x140338900` consumes it to
  CPU-skin each submesh into a dynamic VBO. Version 0.95.8 safely copies that
  validated palette at the same sampled boundaries. The next log can therefore
  distinguish node-input drift, render-time palette divergence, and a later
  CPU-skin/VBO-consumption fault without changing a pose or hooking a hot path.

Version `0.95.8` otherwise retains the focused recovery work derived from the
2026-09-03 headset log:

- One transient `thread_open` race while installing
  `FadeCameraAspectMultiplier` rolled back the entire comfort bridge in 0.95.5.
  That single failure restored head bob, terminal camera takeover, and Read
  depth-of-field blur. Vanished Toolhelp snapshot threads are now skipped, and
  depth-of-field plus optics patches have independent failure domains: an
  optional optics failure can no longer remove the already-installed camera-add
  suppression. Startup and summary rows report requested versus installed state
  for every lane.

- A 125 ms first-eye Read transition correctly rejected the cached pair base,
  but continuous same-frame stereo misclassified the expected abort as an eye
  sequence fault and disabled itself permanently. The replay owner now leaves
  the mode armed after `expected_pair_abort_retry_next_frame`; genuine
  unexplained eye/pose mismatches still invalidate caches and fail closed. This
  targets the observed left-eye locomotion lag and the later `stereoPoseGap=2`
  AFR fallback without weakening the 100 ms absolute-replay staleness guard.

- Native snap turn now commits the exact same yaw delta to the virtual torso in
  the turn tick and ignores that tick's pre-turn camera sample. Slow physical
  follow remains available for real-world body turns, but snap turn no longer
  leaves the shoulders behind or applies the delta twice.

- The laptop's live source viewport animates from `256x145` to `2048x1155`
  while the private capture remains `1024x577`. Origin-only scissor translation
  was therefore insufficient. Fully offscreen source clips now receive a
  conservative affine origin-and-scale mapping before draw and exact GL-state
  restoration afterward; unmappable clips retain the bounded disable fallback.

- The release/default story-object distance multiplier is now `1.5` instead of
  `2.0`, moving the stable Read presentation 25% closer while preserving the
  first-frame non-recursive placement and native scale.

- The 2026-09-02 headset log exposed a coordinate-frame error in physical body
  follow: the recentered HMD yaw was about `0.19 degrees`, but it was compared
  directly with SOMA's roughly `115 degree` world camera heading. That made the
  virtual torso begin sideways and slowly unwind, reversing shoulders and
  tangling the arm mesh. Version 0.95.5 composes native body yaw plus relative
  HMD yaw before evaluating the 45-degree follow threshold. New anchor rows log
  all three values so another reference-frame mismatch is immediately visible.

- The stable opaque terminal background disproves OpenXR layer dropout as the
  source of the remaining email flash. HPL2 source explains the captured clip
  pattern: `cGuiSet::Render` emits scissors in the engine's cached source
  framebuffer viewport, while SOMAVR has privately rebound a `1024x577` FBO.
  Version 0.95.5 translates fully offscreen clips by the saved source viewport
  origin and restores the original scissor after each draw. Translation is used
  only when the result intersects the capture target; the prior disable-scissor
  lane remains a logged fallback for unmappable clips.

- Both 34-node arm/finger hierarchies are named and parent-mapped, and the
  released `hands_human.dae` documents the shirt/hand skin-weight overlap. The
  current visual prototype still solves the anatomical hinges through
  `Arm_1` and `Arm_6`; intermediate `Arm_*` and `Elbow_*` nodes are restored but
  do not yet receive distributed swing/twist. With `HandTrackingProbe=1`, each
  fresh hand seed now emits `hpl_arm_hierarchy` rows for all 34 nodes, parent
  indices, segment lengths, and solver roles. This follows FarCry2-VR's useful
  distinction between a joint solve and the bones that actually deform skin.

- The 2026-09-01 headset run proved the retained shared arm root was first
  seeded after SOMA had already applied the medicine animation's exact
  `+0.6643875 m` local lift. Version 0.95.4 narrowly recognizes that authored
  translation and substitutes the established neutral root before retention;
  all other root poses stay native. `hpl_arm_root_pose_seed` reports the
  incoming and retained translations plus `authoredCorrection`, and the final
  summary counts corrections separately from drift repairs.

- Terminal overlay input now intersects a ray from the tracked controller
  origin with the exact configured VIEW-space HUD quad or cylinder. This adds
  the missing positional parallax and vertical offset that made the cursor
  disagree with the visible beam. The terminal capture alone receives opaque
  alpha after RGB capture, masking the native laptop that remained visible
  beneath mostly transparent GUI pixels and flashed as the HMD crossed it.
  Both visual changes require headset acceptance; non-terminal HUD surfaces
  retain their existing alpha behavior.

- Read/story objects now receive a stable view-forward presentation position
  on their first eligible frame, using the first native camera distance once.
  Native orientation continues through the bounded 45-frame settle window,
  after which grip rotation may take ownership. This separates comfortable
  placement from SOMA's below-view entrance animation and prevents recursive
  distance expansion.

- Version 0.95.3 removes the residual `iEntity3D::SetVisible` hook from the
  retained-hands subsystem. That function was typed for child `Entity3D`
  objects, while the bridge retained a `cMeshEntity`; the comparison could
  never suppress the intended hide and the wrong pointer type caused the F10
  activation crashes. Hand retention now has one lifecycle owner:
  signature-verified `iLuxEntity::SetActive`, with matrix-only synthetic pose
  updates. The correct `cMeshEntity::SetVisible` entry remains documented but
  intentionally unhooked because live `wake=1` evidence proves `SetActive`
  performs the required visibility work.

- A camera pair-base rejection during an active per-eye history transaction is
  now an expected abort, not an eye-sequence fault. The bridge snapshots the
  cumulative pair-rejection count around each first/replay render; an increase
  clears only that active transaction, increments `viewHistoryAborts`, and
  leaves the per-eye bank armed for the next valid pair. Unexplained eye/pose
  mismatches still fail closed. This directly addresses F-21's observed
  tracking-interruption latch without weakening the genuine mismatch guard.

- The test-only `tools/xrsim` port now has SOMAVR-owned edges end to end. The
  launcher proves runtime/session/frame progress from xr-sim's own `state.json`;
  the sequence runner uses SOMA PID-bound window input instead of the absent
  BioShock command channel; and its self-test covers the runner as well as
  runtime selection, WGL swapchains, actions, 600 frames, and stereo capture.
  This is protocol/lifecycle evidence only, not a substitute for real-runtime
  transfer timing or headset visual acceptance.

- Version 0.95.2 completes the applicable 2026-08-29 executable-playbook
  conformance pass without changing XR, stereo, IK, or interaction policy.
  Native wrist-basis failures are now classified as controller-input,
  non-finite/degenerate, non-orthogonal, improper-handedness, or quaternion
  failures in `hpl_arm_body_summary`. Deterministic tests prove the strong
  handedness guard catches a reflection that an absolute-determinant check
  accepts, pin the guard's acceptance/rejection margin, exercise extreme
  tracked rotations, and map all four edges of an asymmetric OpenXR frustum.
  The full delta audit is `docs/PLAYBOOK_AUDIT_2026-08-29.md`.

- Version 0.95.1 rejects reflected and materially sheared native transform
  bases before converting them to quaternions. Positively scaled proper bases
  still pass, while existing camera, wrist, hand, grab, and physics-body
  fallbacks keep ownership when validation fails. This is a narrow fleet-
  briefing hardening patch; XR pacing, stereo, projection, and IK behavior are
  unchanged.

- Version 0.95 replaces the arm solver's projected fixed-direction elbow pole
  with a cross-product construction. Cross magnitude continuously fades the
  one remaining lateral-arm singularity into torso-local history/native
  fallback before the existing swivel-rate cap. The tuned down/out/back bias
  remains, and deterministic tests cover ordinary, vertical, old-antipode,
  lateral-degeneracy, and continuity-sweep poses.

- OpenXR now times `xrWaitFrame`, `xrBeginFrame`, and `xrEndFrame`
  independently, including recovery End calls. Periodic `openxr_pacing` and
  `openxr_freshness` rows plus the final summary distinguish completed pairs,
  fresh stereo, coherent holds, black/fallback/retained projection, incomplete
  stereo, pair age, and cumulative failures. Rates print `unavailable` until
  they have a valid time/sample basis. No XR call moved threads in this build.

- Version 0.94 made an AFR pair one camera transaction. The first eye latches
  the complete native projection/view/frustum-parameter packet and the complete
  located OpenXR stereo snapshot. The second eye replays both absolutely; it
  never re-reads a dirty HPL frustum or consumes the next game tick's pose.
  Missing, mismatched, or more-than-100-ms-old pair state abandons the pair,
  invalidates incomplete caches, and restarts at eye zero. Summary counters
  expose every latch, replay, and rejection, while rendered pose-frame gap must
  remain zero.

- The OpenXR frame loop now performs one `xrWaitFrame`, one upcoming-render
  prediction, and exactly one `xrLocateViews` per running game tick. The former
  current-submission locate was removed; cached projection images continue to
  submit with the exact poses recorded when they rendered. F10-off now destroys
  the session and instance, clears runtime state, and allows a fresh F10
  bootstrap, so disabling VR cannot leave a focused runtime pacing the flat
  game. `xrWaitFrame` still executes on the `SwapBuffers` thread while VR is
  active: its direct cadence/backpressure and unbounded-runtime-wait risk remain
  open, measured questions rather than claimed fixes.

- Same-frame replay now accumulates total draw count and CPU duration and emits
  `hpl_dual_render_cost_summary`, including average microseconds per 1,000
  draws. Native-stereo promotion still requires sequential per-eye screen-space
  work: scene-color refraction, disocclusions, silhouettes, and other
  camera-dependent effects cannot be computed for one eye and copied to both.

- Version 0.93 applies three concrete cross-engine playbook lessons without
  changing rendering or gameplay ownership. The default-active occlusion-query
  and framebuffer-copy observers now use fixed-capacity, allocation-free
  identity tables. OpenXR records session/focus/profile/instance-loss and
  reference-space trigger opportunities. Startup and `--doctor` census API
  layers, while treating registration as evidence rather than an automatic
  incompatibility.

- Version 0.92 adds the evidence needed to evaluate the existing same-frame
  viewport replay and a future world-only native stereo path without changing
  either lane's ownership. While continuous replay is active, OpenGL hooks
  classify first-eye/replay-eye occlusion
  query reuse and `glCopyTexSubImage2D` scene-color scratch reuse. Both trackers
  are read-only, own-GL aware, bounded to 4096 identities, and fail closed.

- The vanilla apitrace baseline proves why those resources matter. HPL polls
  query availability/results and immediately recycles IDs `1..4` on the next
  frame; gameplay performs 6684 framebuffer copies, including per-object
  partial rectangles immediately before translucent refraction draws. The
  refractive shader consumes `aRefractionMap`, `aSceneDepth`, and six camera
  matrices through `cTranslucentTypeArguments`, so a `glUniform*`-only stereo
  lane cannot make that material family eye-correct.

- OpenXR GL handoff telemetry now includes nonblocking GPU timestamp pairs for
  cache capture and swapchain submission, with an eight-slot ring per eye and
  no result waits. CPU acquire/wait/copy/flush/release timing remains intact.
  XR frame-lock wait/hold and snapshot-accessor contention are also measured so
  the remaining single-mutex architecture can be split only if live evidence
  shows render-thread blocking.

- Native camera packet access now validates every page with `VirtualQuery` and
  protects each copy with SEH. Invalid or stale camera/frustum pointers disable
  the affected operation and increment bounded summary counters instead of
  faulting the process. Deterministic tests cover read/write protection,
  cross-page ranges, nulls, and pointer-offset overflow.

- Startup configuration is emitted as nine ownership-scoped rows instead of
  two oversized variadic records. The optional hands probe now also discovers
  the live player-hands module (`mlId=18`) and script object at the native
  update dispatcher. This is read-only: released scripts prove public
  `PlayerHands_SetVisible(true)` creates the campaign-selected model, but an
  active call remains gated on proving a script-owned invocation phase.

- Version 0.91 hardens the current AFR path before any native dual-render
  experiment is promoted. A transient per-eye projection-apply failure now
  costs one mono-orientation frame and retries; stereo ownership is suspended
  only after eight consecutive failures. A temporarily incomplete pair can
  re-submit the last complete stereo images with the exact poses used to render
  them for at most 12 frames, after which projection falls back to black rather
  than presenting an unbounded frozen world.

- All live native code patches now use the shared suspended-peer-thread,
  instruction-pointer and expected-byte transaction. Build-signature decode
  offsets have compile-time assertions, private GL operations preserve the full
  touched transaction state, logger output truncation is explicit, and release
  defaults no longer enable the hand-tracking diagnostic probe. Startup's
  `runtime_paths` and `config_applied` identity records remain visible at Warn
  log level and include config mtime and size.

- Native same-frame stereo is now an evidence-backed design, not a shipped
  feature. Static and source-backed RE identifies the second world-render call,
  culling traversal, occlusion-query lifecycle, temporal owner and presentation
  boundary, but the 0.91 binary still uses the established AFR/optional replay
  paths. Version 0.92 supplies the previously planned occlusion/refraction
  instrumentation; headset acceptance remains the promotion gate.

- Version 0.90 prices the native OpenGL OpenXR handoff before SOMAVR adopts the
  D3D11 interop architecture independently demonstrated by TheDarkModVR and
  OpenMW-VR. Per-eye acquire, wait,
  copy dispatch, flush, release and total CPU timings are source-tagged, while
  a combined projection timer reports average, maximum and display-budget
  pressure. Version 0.95.3 also logs the runtime's `XR_KHR_D3D11_enable` and
  the live SOMA context's `WGL_NV_DX_interop2` availability without changing
  backend selection. The comparison procedure and conditional backend shape
  live in `OPENXR_GL_TRANSFER_RE.md`.

- Version 0.89 makes the compositor and AFR contracts explicit. Every begun XR
  frame carries a projection layer; loading, tracking loss and recovery retain
  or black-fill projection content. Submission uses the image's recorded pose,
  while the next HPL render and OpenXR controller input use a one-period-ahead
  prediction. AFR rotation is latched across each pair and eye phase advances
  only after a successful cache fill. Private bridge GL work bypasses every HPL
  GL detour through a nested own-GL scope.
- The AddImpulse redirect now patches only after unique-signature, suspended-
  thread, instruction-pointer and expected-byte checks. Release scripts delete
  only a compiled allowlist; the install manifest remains an inventory record,
  never authority over unknown files.

- Version 0.88 is the first relocatable, package-owned baseline. Runtime files
  resolve beside the loaded DLL, the shipped INI comes from a tracked sanitized
  release profile, and the log proves config identity and parser acceptance.
  OpenXR submission keeps projection present through blackout/tracking/copy
  fallback, globally caps prioritized composition layers at 16, and bounds all
  swapchain waits. The injector and worker shutdown paths now fail closed.

- Version 0.87 closes the OpenXR unfocused-pacing hang class corroborated by
  BioShock VR. Before first focus, normal frame submission remains responsible
  for runtime bring-up. After the session has reached FOCUSED once, a later
  VISIBLE/unfocused period keeps polling events and capturing pending eye state
  but skips untimed `xrWaitFrame` until focus returns. Recovery invalidates
  stale stereo caches and resumes without another F10.

- Every build now carries generated version, flavor, configuration, exact Git
  describe/commit/dirty state, and UTC build time. Startup logs the DLL PE
  timestamp/image size plus the Windows, processor, and memory fingerprint.
  Logger initialization preserves the immediately preceding run as
  `logs/somavr.previous.log` before opening a new `somavr.log`.

- A bounded in-process x64 crash handler now complements the external hang
  dumper. It observes only always-fatal exceptions in the vectored lane, dumps
  ordinary access violations only after they are unhandled, chains and
  periodically re-arms around displaced top-level filters, records module/RVA,
  access type, registers and stack candidates, suppresses duplicate fault
  addresses, and caps capture at three attempts. Full memory requires the
  explicit `SOMAVR_FULLDUMP=1` environment variable.

- Exact HPL hand, flashlight, HUD/story, socketed, and Read identities now
  resolve once into cached behavior-neutral calibration snapshots. Clean
  shutdown persists discovered keys and baseline values to
  `somavr_entity_profiles.ini`; no profile value changes gameplay in 0.87.
  Promotion to active calibration remains family-by-family after live identity
  evidence.

- Version 0.86 keeps hands/IK active in native state 10 `Read`, waits 45 frames
  for the native story-object entrance transform to settle, and chooses one
  non-hand presentation owner per Read session. The requested per-wrist
  calibration is 4 cm down, 3 cm outward, 4 cm toward the view, plus 45 degrees
  of local pitch.

- If SOMA has already instantiated a hidden `PlayerHands_*` entity, the
  retained-hands lane now requests native active/visible state as soon as VR
  tracking is eligible. This addresses controller pickup timing and pre-fluid
  apartment saves without inventing a campaign hand model. True pre-creation
  remains gated on the future native `CreateHandModelIfNeeded` call.

- Locomotion can now continue while physical states Wheel through Tear or
  MovingButton own an interaction. The route calls the signature-guarded native
  character-body Move function and remains disabled for Read, terminal,
  authored-camera, paused, or non-normal move-state ownership.

- Delayed physical HMD turn now updates a virtual torso yaw used by the arm rig
  and never mutates SOMA's game camera. Stick/snap turns still update the torso
  reference. The terminal pointer intersects the actual configured HUD quad or
  cylinder, is 1.75x larger in the rolling profile, and the controller guides
  are soft 5%-idle/25%-targeted depth-terminated glows.

- Version 0.85 keeps the retained hands, arm IK, and wrist tracking active in
  native state 8 `Terminal`. The latest log proved terminal camera ownership
  and tracking remain normal; SOMAVR's state allowlist alone had suspended the
  rig. Terminal pointer/input and locomotion gates remain separate.

- Story-object placement now uses a native matrix and camera-relative offset
  latched once per Read session. This removes the observed recursive distance
  expansion where SOMAVR repeatedly treated its previous adjusted output as a
  fresh native pickup position.

- Terminal dirty-rect retention remains enabled when the offscreen-scissor
  repair lane is active. The previous eight-frame missing-clear fallback was
  firing despite a valid repaired sparse-render path and could expose partial
  email frames as occasional flashes.

- Version 0.84.1 fixes a live-proven transform precedence bug: body anchoring
  could replace a newly normalized full-scale hands root with its retained
  pre-F10 quarter-scale matrix. Body anchoring now preserves pose continuity
  while adopting the current accepted scale. Scale normalization also starts
  with active VR camera ownership and no longer waits for controller discovery.

- Version 0.84 introduced slow torso follow after physical HMD yaw stays beyond
  45 degrees for 250 ms. Version 0.86 confines that correction to the virtual
  arm-rig torso yaw, preventing the earlier unwanted camera rotation.

- Both wrists receive a configurable -90 degree controller-forward roll after
  deterministic palm calibration. The medicine bottle preserves its native
  `R_Hand` socket lifecycle but locks its first right-grip-relative transform to
  suppress authored rotation and jiggle.

- Story-object presentation now doubles camera-relative distance once and
  preserves each object's authored scale. HUD center clearing is disabled;
  semantic controller reticles remain independent OpenXR layers. Controller
  guides terminate against SOMA physics geometry, and terminal clicks no longer
  clear retained email pixels.

- `HandAlwaysVisible` remains native-seeded: after the campaign's
  `PlayerHandsHandler` creates the appropriate model, SOMAVR retains it through
  compatible states. Creating hands before the first authored animation still
  requires a guarded `CreateHandModelIfNeeded` handler call.

- Version 0.83 closes two live-proven transform gaps. The medicine animation
  writes `+0.6643875` local Y into the shared clavicle parent `j_Root`; SOMAVR
  now caches and restores that one common parent before the two 34-node arm
  chains. Wrist orientation now uses model palm geometry rather than whichever
  controller/native pose happened to be present at takeover.

- The terminal overlay now receives a VIEW-space pointer at the same normalized
  coordinates dispatched to HPL. It follows either HUD quad or cylinder shape
  and reuses the interaction-reticle composition layer, so layer count does not
  increase. Door/drawer sensitivity is intentionally unchanged pending one
  measured live tuning pass.

- Version 0.82 implements the deterministic torso-ergonomics rung from the
  cross-engine playbook. Controller wrists remain exact. Ordinary gestures do
  not move shoulders; only extension beyond 85% arm length adds a smoothed,
  side-specific clavicle contribution capped at 5 cm. Elbow preference is now
  body-local down/out/back rather than a raw shifted native point.

- The previous elbow direction is stored in torso coordinates and reprojected
  onto each new shoulder-to-wrist solution circle. Near vertical singularities
  history weight rises, while a 10-degree per-frame swivel cap prevents abrupt
  flips. Native body yaw therefore rotates the continuity state immediately;
  head-only yaw and roll still cannot own the shoulder bar.

- The 0.80.1 live run proved the HMD-position shoulder anchor, but disproved
  reconstructed body yaw and three-node pose restoration. Version 0.81 derives
  torso yaw from SOMA's native horizontal camera forward, shifts the shoulder
  centre 10 cm rearward, and restores the complete 34-node arm/finger hierarchy
  before IK. This prevents hidden intermediate nodes from carrying the
  medicine animation's two-foot lift into the retained rig. A 10 cm downward
  elbow pole bias produces a less lateral bend.

- Controller wrist ownership now remains active during state 13
  `MovingButton`, covering curtains and drawers without releasing or reseeding
  rotation calibration. The packaged full-pose freeze suppresses native finger
  animation while preserving SOMA's native socket attachment and scripted prop
  lifecycle.

- The native centred crosshair is now suppressed in the packaged profile so
  the selected controller hit owns the semantic interaction icon. Fresh
  current-ImGui draws classify the front-end as a controller-driven menu even
  when SOMA leaves a valid Normal player behind it.

- Terminal draw-state evidence found email scissors with no intersection with
  the capture viewport. Version 0.81 temporarily bypasses only those impossible
  clips during terminal draws, restores scissor state after each call, and logs
  bounded `terminal_scissor_bypass` evidence.

- The 0.79 live run accepts retained hand reacquisition and terminal exit, but
  disproves its arm restore assumption: lower-arm length grew from about
  `0.216 m` to `0.70 m`. HPL3's ApplyPost function returns immediately when
  `UsePostTransform=0`, so the old restore was a no-op. Version 0.80 resolves
  and signature-guards `cNode3D::SetMatrix` at `0x14023fee0`, caches authored
  shoulder/elbow/wrist locals, and explicitly restores them before each solve.

- The retained shared root now follows safety-clamped tracked HMD world
  position and player-body yaw. Its native/calibrated root-to-head vector is
  retained as the shoulder/neck offset, so room-scale leaning moves the torso
  instead of leaving shoulders on the capsule. Capsule camera position is a
  fail-closed fallback when tracked head position is unavailable.
  Body yaw is isolated as `headWorld * inverse(rawHmd)` and then yaw-filtered,
  so turning the player rotates shoulders around the camera while physical HMD
  pitch/roll/yaw remain independent. Later native hand submissions cannot
  replace the stable VR root after the first seed. Normal plus physical states
  `Grab` through `Tear` retain controller wrist/arm ownership.

- The 0.79 log proves both terminal look-away and controller A/B exit native
  state 8 in one frame. Main-menu routing now uses the focused SOMA window's
  visible native cursor because SOMA creates a valid Normal player/body behind
  its main menu. Pause/main-menu frames retain the native desktop backbuffer.
  Every fresh terminal session automatically records four frames of bounded GL
  draw-state evidence; `Ctrl+F10` remains available for image dumps.

- Recenter calibration is position plus yaw only. OpenXR pitch/roll remain
  referenced to the physical horizon even if the user recenters while tilted.
  `Ctrl+F10` adds a four-frame terminal draw-state trace to the existing image
  dump, recording FBO/program/scissor/texture/blend evidence for the unresolved
  email rectangles.

- Saved terminal frames prove SOMA emits sparse email rectangles. The 0.77 log
  also proves the current GUI route does not issue an intercepted nested
  `glClear`: every completed capture reported zero clear suppressions. Version
  0.77.1 therefore probes eight retained frames, then automatically returns to
  live-frame capture when that required evidence is absent. This prevents a
  permanently black email panel while the final compositor is developed.
  `Ctrl+F10` still dumps native and final upscaled surfaces.

- Wall-terminal entry now latches HMD orientation. Looking at least `65`
  degrees away for `8` frames sends the native cancel action and returns to
  scene navigation; handheld terminal state 9 is intentionally untouched.

- Grab state `1` now accepts the same native semantic Move dispatch as Normal.
  SOMA's released Grab script remains responsible for held-object mass and
  `InteractionMoveSpeedMul`, allowing locomotion while carrying general physics
  objects without bypassing authored collision or weight behavior.

- The visible `PlayerHands_0` entity is SOMA's shipped bilateral skinned mesh,
  not two independent roots. The former dominant-hand root override placed both
  quarter-scale hands at the right controller and is disabled in the stable
  profile. Both wrist bones and hand sockets are stable; the current build
  records local/parent/world bases in automatic state-transition bursts, and
  dry-runs and applies mathematically verified transient post transforms.

- The first 0.75 headset acceptance captured 73 samples per wrist with the
  correct `j_L_Arm_10` / `j_R_Arm_10` parent in every row. All 37 samples with
  live controller targets produced valid independent post candidates; worst
  reconstruction error was `0.0000014`. The remaining tiny close-up hands are
  fully explained by the untouched native root: uniform `0.25` scale and about
  `0.075` world units from the camera. This accepts a Normal-only prototype
  that normalizes shared-root scale but never its pose, then positions each
  wrist independently with lifecycle-safe restoration.

- Version 0.76 introduced that guarded prototype. While Normal/Normal and both
  grips are freshly tracked, the shared root keeps its authored pose but its
  supported quarter scale is normalized to `1.0`; each wrist is then moved to
  its matching grip with a transient position-only post-animation transform.
  The native post matrix and flag are restored and verified immediately after
  every application. `HandScaleNormalization` and `HandWristPosition` provide
  independent rollback.

- Version 0.77 promotes the confirmed shoulder/elbow chain. `j_*_Arm_1` and
  `j_*_Arm_6` receive transient two-bone corrections before the exact wrist
  target, preserving SOMA's animation and restoring post ownership after each
  application. Left and right eligibility are independent and reach is clamped
  to avoid locked elbows.

- Persistent hands are now an opt-in Normal-state layer. Version 0.77.2 removes
  all identity lookup from the activation hook: SOMA must first submit a native
  `PlayerHands_*` matrix and resolve its mesh before the exact pointer can arm
  active/visible retention, root updates, wrist positioning, and arm IK. This
  prevents a provisional construction-time name from poisoning the identity
  cache. Authored camera and non-Normal states fail closed to the original
  handler.

- The 0.77.2 headset test live-proved full-size hands, independent wrist
  positions, and the bilateral arm IK rig. Version 0.78 lowers the shared arm
  root by 30 cm and adds first-frame-anchored wrist rotation, preserving SOMA's
  initial palm alignment while following later controller yaw, pitch, and roll.
  The attached exit crash dump identified a stale cached mesh call through
  `cMeshEntity::SetVisible`; synthetic visibility calls are removed and a
  pause/menu transition now invalidates retained hand and orientation state.

- Version 0.78.0 exposed a startup lifetime gap in that new pause gate. Two
  independent dumps fail identically in `SOMA_GetGamePaused` because the
  startup game context exists before its `+0xc8` subsystem owner. Version
  0.78.1 reads the verified owner chain and paused byte directly, validates
  every pointer, and reports unavailable until initialization is complete.
  No hand, IK, or OpenXR frame had executed before either crash.

- A reusable authored-interaction event layer is built. The medicine profile
  measures cap/left-hand proximity and bottle-to-mouth tip pose, emits haptic
  `cap_remove_requested` and `drink_requested` events, and records tuning data.
  Gameplay script commit remains disabled for this evidence build.

- Live Frida tracing proved the native analog owner is the player-helper
  subobject at `playerRoot+0x110`, not the root pointer itself. Physical `W`
  calls `0x140154fb0` on that owner with analog type `1`, then reaches
  `iCharacterBody::Move(Forward,1.0)`. Normal-state controller locomotion now
  queues the controller-relative analog vector into that exact native semantic
  path. Synthetic keys are retained only for authored-state/failure fallback;
  the under-speed direct body accumulator remains opt-in diagnostics.

- The same Frida pass identified the curtain as MovingButton state `13`.
  `0x140154fb0` reads helper `+0xc8`, equivalent to root `+0x1d8`; the former
  root `+0xc8` readiness check was inspecting the wrong object. Controller Look
  now dispatches through the helper owner and preserves the shipped
  MovingButton script's direction averaging and state callbacks. Slide state
  `4` retains its guarded joint-follow path pending live acceptance.

- Terminal capture no longer enlarges the physical display's adaptive
  `256x145` to `596x337` atlas allocation. The focused state-8 flat GUI renders
  once directly into the `1920x1080` OpenXR HUD capture. The log already proved
  controller virtual coordinates moved across the `1024x577` GUI owner.

- Authored-state telemetry now distinguishes structural camera takeover,
  character-body camera detachment, and semantic authored states. A visible
  hands probe resolves the shipped wrists, hand sockets, and camera socket and
  records their world transforms. These additions are passive; no new camera
  or bone mutation has been promoted to the gameplay test package.

- Controller manipulation queues tracked Look deltas and delivers them at the
  start of SOMA's per-frame player helper `0x14015ba20`. Root player/state
  identity is validated separately; dispatcher `0x140154fb0` is called with
  the helper owner, and readiness is helper `+0xc8 -> script+0x10`. The 0.68.1
  log still prohibits arbitrary SOMAVR-phase calls.

- The 0.68.2 terminal route rendered once and copied the completed physical
  FBO's actual viewport. The 0.70 live-log analysis supersedes that route:
  adaptive atlas viewports were coherent but too small, so the focused set now
  renders directly into the full HUD target while preserving native
  pointer/widget ownership.

- Throw scaling never weakens native impulse and gains forward clearance from
  the body. Read translation and entrance timing are native again after live
  evidence proved per-update distance scaling was recursive; SOMAVR retains
  `2x` apparent object scale and controller orientation.

- The complete 0.66 log proved terminal dispatch reached SOMA, but physical
  laptop mesh misses repeatedly released the pointer. Version 0.67 introduced
  the exact focused state-8 overlay; 0.70 replaces its later atlas-copy path
  with one direct high-resolution draw. Widget dispatch, clicks, sounds, and
  callbacks remain native.

- Released Slide script source proves curtains and drawers accumulate semantic
  Look input through `mvMoveAdd`. The direct joint PID bypassed that route, while
  snap turn accidentally supplied it. Version 0.67 restores the existing native
  controller-to-Look bridge by default and keeps direct velocity opt-in.

- Loose-prop pull range was consumed by the initial hit-to-hand correction before
  any controller motion occurred. Version 0.67 bounds only post-acquisition hand
  travel, raises that allowance to `1.5` metres, and restores the native
  `6 rad/s` rotation response while keeping the stable bounded-error gain.

- `Soma.exe` is the Steamworks build and has a different native address layout
  from `Soma_NoSteam.exe`; the current signature doctor correctly rejects it.
  The packaged NoSteam developer launcher supports dev config and direct maps.

- The first 0.65.1 headset pass accepted the restored story-object route. Loose
  props still oscillated after pull-in, curtains lagged behind short tracked
  gestures, and terminal state `8` alternated its pointer gate without one
  applied virtual-position row. Version 0.66 bounds Grab torque error, gives
  Slide a controller/body positional catch-up term, and uses the exact native
  world-ImGui owner at manager `+0x170` for terminal dispatch and interception.

- The 0.65 live pass exposed two object-rotation regressions. Read presentation
  cached a transition matrix before SOMA completed its native approach, creating
  a slow distant arc. Grab added tracked angular velocity to SOMA's opposing
  camera-relative target, and both saturated near `6 rad/s` before cancelling.
  Version 0.65.1 restores native Read travel, directly applies unrestricted
  tracked orientation only at the object matrix, and drives Grab from absolute
  hand/body orientation plus measured body angular velocity.

- The 0.64.1 live pass restored independent left/right interaction, both visual
  guides, and native focus. Version 0.65 locks the initiating hand for every
  native physical interaction state, preventing crossed beams from changing a
  held object's owner until SOMA exits the state.

- Wall-terminal mesh projection was live, but the log contained no applied
  cursor rows because virtual positions only changed when native mouse motion
  happened. The terminal bridge now calls SOMA's original virtual-position
  dispatcher on every controller mesh hit.

- Door/Lever now combine controller translation around the native pivot with
  wrist angular velocity projected onto the native hinge pin. Grab begins with
  a bounded native-PID pull from the selected world hit toward the initiating
  grip, then retains the existing tracked translation, rotation, and release
  impulse paths.

- Read state holds right-click while right A/B is held and keeps the guarded
  apparent scale, but native pickup translation/timing are restored. Grip
  directly controls persistent full-axis object orientation without synthetic
  mouse input or camera drift. This path awaits headset acceptance.

- Both tracked controllers now feed SOMA's native closest-entity raycast. A
  deterministic pressed/hit/sticky/preferred policy selects one focus owner,
  while the outer native result is finalized exactly once. Both beams remain
  visible, the semantic context icon follows the selected beam depth, and the
  selected hand owns subsequent manipulation, terminal clicks, and Grab contact
  feedback. Static implementation and unit coverage are complete; headset
  acceptance is the current gate.

- The first 0.64 live pass proved both visual beams but exposed that the new
  inner-raycast signature omitted its leading `0x40` REX prefix. The bridge did
  not install, so native gaze semantics appeared while controller hit snapshots
  and world icons remained absent. Version 0.64.1 corrects the exact entry bytes
  and RIP-relative global offsets; 0.64.1 live evidence accepted the fix.

- Wall terminal state `8` now stays diegetic in VR: exact native hooks suppress
  only its body teleport, scripted camera rotation, and Terminal camera offset.
  SOMA's existing mesh projector converts the selected-controller world ray to
  native GUI UV/virtual coordinates, so pointing follows the physical screen
  rather than a head-relative cone. Static RE and implementation are complete;
  headset acceptance is the current gate. Handheld state `9` retains its native
  authored presentation.

- The 0.61 live pass retained the visually accepted stereo/tracking path and
  exposed two interaction/performance causes. Controller velocity was being
  normalized during the HPL reference transform, making hand speed ineffective,
  while Door/Lever still consumed camera-relative synthetic mouse input.
  Version 0.62 preserves velocity magnitude for Slide/grab/throw paths and maps
  controller world-space arcs directly onto native hinge angular velocity.

- Same-frame stereo remains healthy: 194 measured eye pairs averaged 2.61 ms,
  p95 4.36 ms, maximum 12.29 ms, and retained zero pose-frame gap. High-volume
  synchronous render probes, not scene GPU time, were the strongest stutter
  candidate. The active profile now returns to bounded operational telemetry and
  the logger batches ordinary rows without delaying warnings or errors.

- The 0.60 live pass proved same-frame stereo removes the perceived eye delay,
  controller-relative locomotion is stable, and Slide's mouse projection is the
  wrong abstraction for drawers and curtains. It also exposed an incorrect
  closest-entity output decode. The native ABI is now distance `+0x18`, body
  `+0x20`, entity `+0x28`.

- Slide state `4` now uses controller world velocity projected onto selected
  body joint 0's native pin through the existing vector PID hook. Read state
  `10` uses controller orientation, A/B native cancel, and no locomotion turn.
  A bounded Read-entity probe records the open prop needed for a later direct
  right-controller attachment.

- Visual native pitch is suppressed only while the active VR frustum is built,
  leaving HMD pitch as the horizon owner. The HUD capture now matches the live
  `1920x1080` game target. Quest requests `2688x2880` one-sample eye images, so
  the remaining softness is source-render resolution rather than swapchain
  allocation or an accidentally low OpenXR scale.

- The first broad 0.58 headset pass accepted the rigid world, tracking, eye
  height, same-frame shader compatibility, F1 panel, analog movement, input
  exclusion, and controller reconnect path. Same-frame stereo removed the only
  perceived inter-eye latency, so the packaged profile now enables it and logs
  exact left/right rendered pose-frame gaps. AFR remains a one-action rollback.

- Left-controller-relative movement, simultaneous left/right aim guides,
  dominant-secondary inspection cancel, grip-held readable rotation, and a
  dedicated 3x Slide-state scale are built for the next live pass. The packaged
  center-HUD clear is disabled because it was the exact source of the clipped
  interaction icon and missing square. Quest 3 vignette coverage is wider and
  explicitly logged; native font-scale subtitle tuning is restored.

- Native grabbed-object impacts now have an opt-in interaction-owner haptic path.
  Ghidra plus released HPL2 source confirm `cSurfaceData::OnImpact` at
  `0x14032f0e0` receives normal collision speed, contact position/count, and a
  physics body after Newton simulation. The exact-signature hook always runs
  SOMA first, then requires Grab state, a fresh tracked selected grip, and a
  contact point within a bounded radius before mapping speed to one OpenXR
  pulse. A 45 ms cooldown collapses duplicate material callbacks. Generated
  configs remain off; the active profile enables it for live evidence.

- Optional fixed foveation is now an OpenXR swapchain feature rather than a
  SOMA shader experiment. The runtime requires `XR_FB_swapchain_update_state`,
  `XR_FB_foveation`, and `XR_FB_foveation_configuration`, resolves all three
  extension functions, creates foveation-capable eye swapchains, and applies one
  level-controlled profile to both eyes. Profile/update failure applies a level-
  zero neutral profile and preserves normal VR submission. Generated configs
  remain off; the active profile requests medium fixed foveation so the next log
  directly reports Virtual Desktop capability and activation.

- Same-camera authored ownership changes now have an explicit VR handoff.
  Transitions into or out of matrix-controlled/script-owned camera motion keep
  F10 tracking and stereo active, invalidate the captured native frustum base,
  increment the calibration generation so all stereo histories reseed, and
  request the existing bounded transition blackout. Camera pointer replacement
  retains the stronger disable-and-rearm policy. Bounded ownership counters and
  transition logs make sit, ladder/climb, conversation, scripted animation, and
  hand-attached camera tests directly diagnosable.

- Temporal SSAO now has explicit per-eye GPU history. The exact native writer
  at `0x1403f2b50` reads and overwrites renderer texture `+0xe78` once per eye;
  the active profile banks two same-format GL copies around that function while
  preserving SOMA's shaders, framebuffer, and AO pipeline. Generated configs
  leave it off. Live headset acceptance must confirm stable AO during head
  translation/rotation, reset behavior, and no new GL errors or shutdown leak.

- The shipped shader/resource audit closes the generic previous-projection and
  velocity-history TODO. Temporal SSAO consumes the current eye projection from
  the active frustum and the already banked previous-view state; SOMA persists
  no separate previous projection or render-velocity history. ToneMapping,
  ImageTrail, temporal SSAO, and previous view are the observed temporal owners.

- ToneMapping now has explicit same-frame ownership for its authored exposure,
  white-cut, window fade, color-grading transition, and film-grain sampling
  packet. Ghidra confirms
  native update `0x1402842d0` runs inside each render. The first eye commits one
  update; the opposite eye replays the same baseline and leaves the first
  commit resident. AFR remains native. The active profile enables this guarded
  prototype; generated configs and `HPLToneMappingFrameControl=0` retain native
  behavior. Live tone/bloom/fade acceptance remains.

- ImageTrail is the first full post-effect temporal resource with explicit
  per-eye ownership. Confirmed native fields hold its framebuffer at `+0x50`,
  accumulation texture at `+0x58`, amount at `+0x98`, and clear flag at
  `+0xa0`. The active profile allocates a second native pair and banks the
  resource pointers plus clear state by actual eye/pose. Recenter/stale gaps
  clear both histories; exact destruction releases both pairs. Any signature,
  pointer, alias, or sequence failure immediately falls back to the proven
  render-only ImageTrail suppression. This awaits headset acceptance.

- A locomotion-gated comfort vignette now owns a dedicated VIEW-space OpenXR
  alpha layer. `HPLInputBridge` publishes motion intensity only after gameplay
  input survives loading, pause, panel, terminal, dead-state, and authored-
  camera policy. A tested 250 ms envelope fades a transparent-center radial
  mask in and out; stale samples fail to zero. Smooth turning can contribute,
  while snap turn keeps the existing black-frame path. The active profile and
  balanced/maximum presets enable it, F1 can toggle it, and generated configs
  remain off.

- The compositor HUD now supports both flat and curved presentation. When
  `XR_KHR_composition_layer_cylinder` is available, the active profile submits
  the captured alpha HUD on a 70-degree head-locked cylinder whose physical arc
  width, aspect-derived height, and center distance match the quad contract.
  The F1 panel switches shape live. Missing-extension and layer-validation paths
  retain or restore the quad without changing HUD capture or scene stereo.

- Inventory presentation now has an exact native activity authority. The
  signature-guarded `cLuxUserModule::OnAction` wrapper admits current ImGui to
  the HUD capture only for user-module ID `15`, open-inventory action `12`, and
  a pressed edge. Its five-second authorization covers the shipped three-second
  display plus `0.6/s` fade-out without broad GUI interception. Main menu is
  already owned by the exact pause gate; hints and credits use GameHudImGui,
  while crosshair, descriptions, infection, and flashes use GameHudSet.

- Shipped wake, game-over, and credits presentation ownership is now explicit.
  Exact `WakeHandler` global dispatch controls XR sleep blackout and a bounded
  wake current-ImGui capture window; exact dead state `17` authorizes the
  game-over current-ImGui set and a dominant-controller continue action.
  Credits already use GameHudImGui and therefore need no broader GUI capture.
  All additions fail closed behind `HPLScriptedPresentationControl` and await
  a live wake/death/credits pass.

- SOMA's script-authored gamepad rumble now crosses into OpenXR at the exact
  registered `SetRumble` wrapper. Damage, death, attacks, locked interactions,
  datamining/tool sequences, and authored environmental effects retain native
  strength/duration semantics as bilateral segmented VR haptics. The original
  physical-gamepad path is always called, repeated script updates are
  throttled, and explicit falling edges stop both OpenXR outputs.
- Grab-state collision feedback is independent of authored rumble. The native
  surface-impact function, sound creation, particles, physics, body callbacks,
  and physical-gamepad path remain untouched. Weak, distant, stale, untracked,
  authored-camera, and cooldown-duplicate events are rejected with bounded
  counters; `ContactHaptics=0` removes the hook completely.

- Sustained physical roomscale displacement can now advance the native player
  capsule through exact-signature guarded feet-position wrappers. The path is
  restricted to unpaused Normal/Normal ownership, requires the existing head
  safety result to be clear, samples the body volume before each 0.015 m step,
  and compensates the HMD neutral position to keep the rendered world fixed.
  It is enabled in the active test profile and has a one-line config rollback;
  headset/collision acceptance remains.

- Same-frame stereo is now available as an explicit opt-in sustained prototype.
  It replays only the exact player viewport, suppresses screen GUI on eye two,
  and leaves the engine viewport enumerator, update/script lifecycle, frame/stat
  reset, GUI, XR submission, and presentation once per game frame. Immediate
  cache failure or an eye/pose-sequence mismatch disables the mode and restores
  AFR. The active profile exposes the F1 panel control but starts it off.
  Automatic bounded samples and `Ctrl+F6` remain available for temporal mutation
  evidence around the stateful post-post phase at `0x1401f1480`.

- The first confirmed temporal resource is now isolated per eye. Ghidra and
  HPL2 source identify `*(renderer+0x438)+0x80` as a 64-byte previous-view
  matrix. During continuous exact-player rendering, SOMAVR restores the pending
  eye's bank before the viewport and captures SOMA's native update afterward.
  Renderer/history changes and eye/pose mismatches reset or fault closed to the
  original shared path. The active profile enables the control; the F1 panel
  exposes its live state.

- `0.47.0` broadens that exact packet bank to the normal F10 AFR path as well as
  same-frame stereo. This prevents an AFR eye from inheriting the other eye's
  immediately preceding previous-view matrix. Recenter generation changes and
  pose gaps over eight frames reseed from native state, preventing stale motion
  after calibration, loading, or tracking recovery.

- OpenXR controller coverage now includes the standard HTC Vive profile in
  addition to Simple, Touch, Index, and Microsoft Motion. Runtime interaction-
  profile change events resolve and log the exact active profile independently
  for each hand, making reconnect, one-hand fallback, and runtime/headset matrix
  results directly attributable.

- Configuration and startup diagnosis now have user-facing ownership. Comfort
  presets apply before explicit keys, preserving every tuned override. The
  injector doctor checks the complete launch prerequisites without starting the
  game; the local OpenXR build currently reports seven passes, one expected
  developer-layout warning, and zero failures. `USER_GUIDE.md` is included in
  the release checksum ledger.

- A dedicated head-locked OpenXR status/options panel is now available through
  `F1` or `Menu + Secondary`. It owns a separate alpha swapchain and reports
  tracking, stereo, input, player/authored-camera, roomscale, projection, HUD,
  and reticle state. Stick plus select/trigger controls recenter, same-frame
  stereo, and reversible runtime options while exclusive input ownership prevents
  actions leaking into SOMA. This is built and unit-tested but still needs headset
  acceptance.

- Exact wall/handheld terminal states `8/9` now route dominant-controller aim through
  SOMA's native spatial mesh projector and virtual ImGui cursor boundary. Current-ImGui identity,
  GameHud exclusion, 3D-set ownership, readable virtual layout, tracking, and
  config gates all fail closed to the original engine path. Trigger/select
  remains a native mouse click and releases on every terminal-state exit.

- The exact `HPL3_PostEffect_RenderOne` boundary now wraps each active effect in
  a read-only GL resource capture. Bound textures are identified by target, GL
  ID, level-zero dimensions, depth, and internal format; framebuffer writes and
  HPL input/output object identities are recorded alongside eye and pose frame.
  Ownership is classified only from resource-bearing left/right captures with
  the same nonzero pose frame. Startup and interval sampling are bounded, while
  `Ctrl+F6` forces both halves of the one-frame replay pair. This closes the
  evidence gap before per-eye tone/bloom/grading promotion or temporal-history
  duplication.

- Exact independent `HudObject` tools and Grab-state physics bodies now have an
  optional two-hand direction owner. The dominant grip remains the position
  anchor; a squeezed, fresh support grip inside configurable separation bounds
  supplies aim. Tools receive the composed basis at the existing exact matrix
  boundary. Physics bodies receive only a shortest-arc target through SOMA's
  confirmed torque PID. Both engagement and release re-anchor before applying
  torque, and every invalid/support-loss state immediately uses dominant-only
  or native behavior.

- `Ctrl+F6` and the automatically spaced samples retain the bounded diagnostic
  form of the exact-player replay. They force temporal mutation/resource capture
  around both eyes. `0.45.0` adds a separate sustained toggle through the F1
  panel; ordinary continuous frames use the same replay boundary without the
  expensive snapshots. Logs require opposite eyes from one tracked pose and
  expose the unavoidable duplicated post-post callback.

- Optional compositor depth is now fully wired. The OpenXR runtime negotiates
  D24/D32F or matching depth-stencil formats, builds matching per-eye depth
  caches and swapchains, and chains
  `XrCompositionLayerDepthInfoKHR` using the confirmed standard HPL/OpenGL depth
  convention. HPL near/far are converted to meters through `HPLWorldScale`.
  Unsupported formats, invalid clip data, missing caches, and copy failures all
  retain the proven color-only submission path.
- OpenXR frame resources now survive changing runtime contracts. A changed
  SOMA HDC/HGLRC triggers full delayed runtime recovery; changed recommended
  view dimensions or sample limits trigger a frame-resource rebuild. Stable
  view checks occur every 300 game frames without reallocating resources.

- Native subtitle presentation now has a narrow VR adapter. The exact voice
  subtitle draw worker at `0x1401c8dd0` reads layout from its
  `cLuxVoiceHandler` owner. During active stereo, `HPLSubtitleBridge` applies
  configured width/font/Y/shadow scaling only for that native call and restores
  the original values before returning. Localization, speaker names, timing,
  gradual reveal, font selection, and native enable settings remain untouched.
- The HUD layer now admits SOMA's exact current ImGui set while the confirmed
  pause getter reports `paused=1`. This gives the existing controller pointer a
  stable head-locked pause surface without broad ImGui interception. Main-menu,
  loading, game-over, terminal, and other non-paused owners still need explicit
  classification.

- F10/F11 and the diagnostic camera controls are now owned by the exact player
  camera whenever `HPLPlayerState` can identify it. Secondary viewport cameras
  retain native frusta and cannot consume activation edges or inherit headset
  pose. Bounded viewport identity rows expose camera/world/renderer/post/FBO,
  dimensions, flags, and player/secondary role for future reflection, terminal,
  save/load, and same-frame render work.
- The `0.32.0` per-eye depth evidence path remains available independently as a
  probe and provides bounded source/copy/sample telemetry around submission.
- The injector warns about common graphics/VR hook conflicts and blocks a
  duplicate SOMAVR injection. The release package now includes reversible,
  checksum-verified install/update/uninstall scripts that preserve user config.

- Exact script identity now separates three viewmodel owners at the shared
  Lux-entity matrix boundary. `PlayerHands_*` retains its existing controller
  root, exact `HudObject` can follow the dominant grip with its native scale,
  and `*_HudObject` inventory tools remain socket-owned so they are not
  transformed twice. Every tracking, state, authored-camera, scale, and math
  failure forwards SOMA's original matrix. The exact native DestroyEntity path
  evicts cached identities before queueing, preventing pointer reuse from
  applying an old tool policy to a newly created entity.
- The HUD capture now accumulates exact GameHudSet, exact
  `SOMA_GetGameHudImGui()->GetSet()`, and paused exact-current-ImGui draws into
  one transparent target per game frame. The pause getter is the hard ownership
  gate; diegetic and non-paused current ImGui sets stay native. The first exact
  draw clears, later exact draws append, and native GL state is restored after
  each set.
- Periodic `hpl_render_transaction` rows summarize the complete six-stage
  viewport transaction and identify a possible world-only replay scope without
  claiming callback safety. The new one-frame replay supplies that live proof
  without changing ordinary AFR frames.

- Script-created screen materials are now identified only by SOMA's exact
  `Screen Particle<decimal>` billboard name. While F10 VR is active, their
  shipped `0.15` camera-relative distance and native size are scaled together
  to a configurable `1.5 m`, preserving apparent coverage without near-field
  stereo convergence. The dedicated bridge restores native behavior outside VR
  and removes identities at the exact billboard destruction boundary.

- Exact registered FOV, FOV-multiplier, and aspect-multiplier leaf wrappers now
  have a reversible active-VR comfort policy. Scripted zoom/FOV requests resolve
  to the player's native default FOV and both multipliers resolve to `1.0` while
  F10 tracking is active; native requests and fade speeds return unchanged when
  VR is inactive or a channel is disabled.
- `HPLPresentationBridge` queries SOMA's exact loading-screen visibility once per
  game frame. Load entry invalidates both AFR caches, submits opaque-black
  projection content, and releases controller input; load exit invalidates
  again and adds a bounded two-frame guard before stereo repopulates. The
  desktop retains SOMA's native loading backbuffer.
- Signature-guarded `CreateVideo`/`DestroyVideo` hooks record stream identity,
  source name, active count, and peak concurrency without changing playback.
  This is deliberately a classifier for fullscreen versus diegetic video, not
  a video presentation override.

- Physical room-scale head translation now uses SOMA's confirmed world
  line-of-sight query to sweep a configurable center/radial/top/bottom head
  volume. One cached earliest-safe result is decomposed back into every
  eye/controller pose, preserving IPD, eye height, authored camera motion, and
  hand/flashlight coherence. The active profile includes moving geometry in the
  same sampled sweep; a static-only rollback remains configurable. Native
  capsule movement remains separate.
- The desktop mirror can now show a stable cached left or right eye with fit,
  fill, or stretch layout after XR submission. Native mode leaves the original
  backbuffer untouched. Existing render-stage hooks also accumulate left/right/
  mono CPU and nonblocking GPU timings for dual-render budgeting. A bounded
  timestamp-query pool drops saturated samples rather than stalling the game.
- OpenXR depth capability is now explicitly probed. Supported runtimes enable
  `XR_KHR_composition_layer_depth`, and one evidence row combines extension
  state, framebuffer depth bits/range, and HPL near/far projection data. Per-eye
  depth swapchains and submission remain gated on that live evidence.

- The exact scripted `Flashlight` light now follows the dominant controller's
  tracked aim pose through the existing guarded Lux-entity transform boundary.
  Independent local offset/rotation calibration is available. SOMA still owns
  light lifetime, fade/color, visibility, radius/FOV, particles, sensors, and
  callbacks; authored cameras, stale/lost tracking, and invalid state restore
  the original camera-mounted transform.
  Its three randomized agent-gobo gameplay rays now preserve their native cone
  while using the same cached light origin and controller-relative basis. Tool
  interaction and camera-animation grounding rays remain native.

- Wheel, Slide, SwingDoor, Lever, and Tear states now accept dominant-hand
  physical movement through SOMA's existing analog-look path. The adapter uses
  controller position relative to HMD position, projects onto head-right/up,
  bounds the relative mouse delta, and leaves all native constraints and scripts
  authoritative. The active profile enables it; generated configs default off.
- `HPLHudBridge` still correlates every rendered `cGuiSet` with current and
  gameplay-HUD `cImGui` ownership. It now promotes only the confirmed paused
  current owner into capture; telemetry remains the classifier for loading,
  wake, game-over, credits, inventory, and diegetic surfaces.

- A guarded native comfort bridge now intercepts SOMA's semantic camera-add
  setter. During active VR it zeros only Bob, Shake, and optional Sway while all
  authored movement/state channels remain native. It now also owns the exact
  Set/Fade camera-roll wrappers: Lean, Move, and Climb roll are disabled in the
  active profile while Script roll remains native. A reversible world DoF guard
  and exact VideoDistortion post-effect policy apply only during active tracking.
  Generated configs keep the native control boundaries disabled.
- Exact player-state IDs now drive transition telemetry and a two-frame comfort
  blackout around Ladder, ClimbLedge, InteractiveCameraAnimation, Sit, and Dead
  entry/exit. This hides abrupt authored pose handoffs without replacing state,
  constraints, animation, scripts, camera movement, or FOV.
- Controller closest-entity results publish validated entity/body pointers,
  native hit distance, and an HPL world hit point. The same exact aim pose and
  distance drive an application-space OpenXR reticle quad with compositor-correct
  binocular depth, while `HPLCrosshairBridge` observes SOMA's exact
  `eCrossHairState` decision through the registered global-script boundary.
- The reticle is age, tracking, distance, size, stereo, resource, and semantic
  guarded. It loads the 34 native icon files named by shipped `Player.hps`,
  preserves their aspect inside the quad, applies broad intent colors, and falls
  back to the procedural cross if loading fails. Both semantic gating and native
  artwork can be disabled independently in configuration.
- Optional focus-change haptics use native entity/body identity plus the same
  semantic state. Pickup, manipulation, traversal, social, unavailable, and
  simple-hint classes receive bounded profiles; default-cursor focus stays silent.

- Optional head-relative movement now rotates the movement stick by calibrated
  HMD yaw while rejecting pitch and roll. Optional physical crouch calibrates
  standing HMD height and drives SOMA's existing crouch toggle with hysteresis.
- A signature-guarded native grab path now augments only SOMA's exact Grab-state
  position PID error with dominant-controller translation. The first pickup
  sample and every unsafe state remain native, and SOMA still owns object mass,
  gravity, force limits, collision, joints, and callbacks.
- Grab rotation now adds the dominant grip's shortest-arc orientation target to
  SOMA's exact torque-PID error. Dominant primary still requests the native throw,
  but the exact AddImpulse wrapper can redirect that one authored impulse along
  tracked controller velocity or grip-forward aim with bounded velocity scaling.
- The compositor HUD can clear only a configurable center rectangle after the
  exact GameHudSet render, suppressing the native gaze crosshair without touching
  the eye images or other HUD regions.

- The working stereo transform now also resolves HMD and controller poses into
  HPL world coordinates. Periodic controller rows report dominant aim/grip
  positions and directions for live validation before native pick injection.
- Listener orientation and room-scale head translation are composed only for
  the native FMOD update and then restored, preserving authored camera state.
- `HPLHudBridge` now owns the exact gameplay HUD `cGuiSet` boundary at
  `0x140213970`, identified through the confirmed game-context getter at
  `0x1400cc9b0`. With `HudLayer=1`, only that 2D set is redirected into a
  transparent GL target and submitted as an alpha-blended VIEW-space OpenXR
  quad. Existing virtual/center-screen metrics remain in bounded telemetry.
- Paused current ImGui and native voice subtitles are now covered by narrow
  adapters. Main/loading/game-over menus, terminals, and every 3D/diegetic GUI
  remain native. Missing signatures/resources, non-visible XR state, or failed
  validation restore normal native rendering.
- The dominant controller's fully tracked world aim can replace only the
  start/direction passed to SOMA's native closest-entity wrapper at
  `0x1400cd750`. Strict query-type, native-origin, tracking, input, and
  authored-camera gates restore the original gaze query on any mismatch.
- SOMA still owns interaction ray length, LOS, `CanInteract`, distance policy,
  focus state, player-state transitions, physics, and map callbacks.
- The exact runtime `PlayerHands_*` entity is recognized through confirmed
  `cLuxProp` GetName and SetMatrix registrations. `HandControllerRoot=1`
  replaces only uniform quarter-scale Normal/Normal matrices with a dominant
  tracked-grip root reconstructed from SOMA's native `rotateY(pi)` convention.
  Position and model-space XYZ rotation are configurable. Full-scale, authored,
  non-normal, stale, lost-tracking, and malformed states remain native.
- Ordinary unpaused gameplay now receives radial-deadzone analog movement
  through the registered character-body Move wrapper and exact-degree snap or
  smooth body yaw through AddYaw. A signature-guarded game-pause getter plus
  Normal/Normal ownership gates prevent direct input in special states. A pause
  result now suppresses the semantic W/A/S/D/mouse fallback too, closing the
  previous possibility of controller input continuing behind menus.
- While paused, the dominant controller aim is projected relative to the HMD
  into SOMA's native client rectangle. Trigger/select uses the existing left
  mouse path, with a release latch preventing an accidental world interaction
  when the menu closes. Invalid pose/window/pause state fails closed.
- In two-controller play, the support-hand primary/secondary buttons now route
  through SOMA's existing flashlight and inventory actions. Dominant-hand role
  changes move those actions with the support hand; one-hand recenter is preserved.

- Invalid `xrLocateViews` output can no longer overwrite the last valid eye
  cache. Tracking samples have a configurable 30-frame usability bound and
  recover through a two-frame compositor blackout.
- Temporary pose expiry restores the native base view for that frame while
  preserving stereo intent, so valid tracking can resume automatically instead
  of requiring F10/F11 reactivation.
- Controller primary/secondary actions are available on either Touch/Index
  hand. Dominant hand and stick roles are configurable, with a bounded
  one-controller movement/action fallback when only one hand is active.

- `ReferenceSpace=local|stage` now supports seated/local and floor-aware standing
  calibration profiles with a logged fallback when STAGE is unavailable.
- OpenXR controller output haptics cover interaction, snap turn, menu, jump,
  crouch, recenter, semantic focus, and SOMA-authored gameplay rumble. Focus
  loss clears the entire input snapshot immediately.
- Native base and extended roll are now measured from confirmed camera fields;
  opt-in temporary suppression is available for authored-camera comfort testing.
- OpenXR session/instance loss now schedules an in-process runtime rebuild after
  `RecoveryDelayFrames` instead of permanently suspending submission.
- Player/camera replacement invalidates both AFR eye caches and automatically
  re-runs stable-pose calibration against the new native camera.
- Snap turn and recenter can omit projection layers for two comfort frames while
  OpenXR frame pacing remains alive.
- Named post-effect policy suppresses ImageTrail, ChromaticAberration, and
  RadialBlur only during active stereo rendering and restores native state after
  every compositor call.
- The final GUI path now reports individual GUI-set classification fields and
  draw footprints, providing the next HUD capture dataset.

- OpenXR left-stick movement now drives SOMA's own W/A/S/D input route with
  configurable press/release hysteresis.
- Right-stick turning supports configurable snap or smooth mouse-path input.
- Right trigger/select maps to native interaction, menu maps to Escape, and a
  held two-grip chord requests the existing stable F2 recenter pipeline.
- Left trigger holds run, right A jumps, and right B toggles crouch on confirmed
  Touch/Index profiles through SOMA's shipped default action keys.
- Every injected held input is released on VR disable, inactive controls, stale
  OpenXR samples, or DLL teardown.
- `HPLPlayerState` now owns the signature-guarded player/camera/body and state
  getters. It also reads confirmed camera rotate mode `+0x6c` and body camera
  update ownership `+0x1e8`, logging every authored-camera transition.
- Controller gameplay input releases automatically while a scripted sequence or
  hand-socket attachment owns the camera. Menu and recenter remain available.
- Render-stage logs now include per-stage GL draw/state deltas. Active post
  effects are inventoried by object/vtable/flags, and `Ctrl+F12` can isolate one
  active effect at a time without persisting mutations.
- This remains a guarded feature build: physical crouch toggle synchronization,
  grab translation/rotation stability, throw direction/scale, and center-clear
  HUD coverage require live acceptance before defaults can be enabled globally.

- `somavr_injector.exe`: launch-suspended or attach-by-PID/process-name DLL injector.
- `somavr.dll`: MinHook-based OpenGL/WGL telemetry DLL.
- `somavr_common`: logger and INI config shared by the injector/DLL.

The DLL is still a probe, not a correct stereo renderer. It hooks:

- `gdi32!SwapBuffers` for frame boundaries.
- `opengl32!wglMakeCurrent` and `opengl32!wglGetProcAddress` for context and extension discovery.
- fixed-function `glMatrixMode`, `glLoadMatrixf`, `glViewport`, `glDrawElements`, and `glDrawArrays`.
- extension functions returned by `wglGetProcAddress`, including `glUniformMatrix4fv`, `glGetUniformLocation`, `glUseProgram`, `glBindFramebuffer`, and `wglSwapIntervalEXT`.

The live `0.2.2-xrloaderpath` log proved the OpenXR loader/runtime discovery path:

- `openxr_loader_load ok` from `build-openxr\Release\openxr_loader.dll`.
- `openxr_extensions ... khrOpenGL=1`.
- runtime `VirtualDesktopXR 1.0.10`.
- system `Meta Quest 3`, orientation and position tracking available.
- OpenGL requirements accepted SOMA's context: `minGL=4.0.0 maxGL=5.0.0`.
- primary stereo views reported as `2688x2880` per eye, opaque blend mode only.
- `SessionProbe=0` skipped `xrCreateSession` as intended.

The same run still crashed later in `VirtualDesktop.LibOVRRT64_1.dll` with `0xc0000005`, so `0.2.3-xroneshot` changed the no-session path to one-shot discovery:

- `FrameSummaryInterval=120`.
- `MatrixSampleLimitPerFrame=32`.
- `UniformMatrixProjectionOnly=1`.
- `UniformMatrixLogLimit=256`.
- `somavr_build_flavor.txt` beside each DLL records `openxr=0` or `openxr=1`.
- the injector warns if `[OpenXR] Probe=1` is paired with a non-OpenXR DLL.
- the non-OpenXR DLL logs `build_without_openxr` as an error with the corrective path.
- the OpenXR DLL explicitly preloads `openxr_loader.dll` from beside `somavr.dll` before calling OpenXR.
- the default generated config stays at `SessionProbe=0` for conservative static probes.
- after requirements/view/blend discovery, SOMAVR now destroys the OpenXR instance and logs `openxr_instance released_after_static_probe`.
- summaries now include `openxrInstanceAlive=` and `openxrInstanceReleasedAfterProbe=`.

The latest live `0.2.3-xroneshot` log is better:

- it loaded `version=0.2.3-xroneshot buildOpenXR=1` from `build-openxr\Release`;
- it reached runtime `VirtualDesktopXR 1.0.10`, system `Meta Quest 3`, OpenGL requirements, stereo views, and blend modes;
- it skipped session creation, released the instance, and later summaries reported `openxrInstanceAlive=0`;
- SOMA continued logging frame summaries for over a minute after OpenXR release;
- no newer SOMA WER crash was found after that run.

The live `0.2.4-xrsessiononeshot` log extended that safer lifetime model to `xrCreateSession`:

- `xrCreateSession` succeeded on the early startup GL context;
- reference spaces were `VIEW`, `LOCAL`, and `STAGE`;
- swapchain formats included `GL_RGBA16F`, `GL_SRGB8_ALPHA8`, `GL_RGBA8`, and depth formats;
- the session reached `READY`;
- SOMAVR destroyed both session and instance and logged `openxr_runtime released_after_probe reason=session_probe_complete`;
- later summaries reached frame `2040` with `openxrSessionAlive=0`, `openxrInstanceAlive=0`, and `openxrSwapchainFormats=7`;
- no newer SOMA WER crash was found after that run.

The live `0.2.5-xrframeprobe` log confirmed the same session one-shot on SOMA's real frame context:

- `OnOpenGLContext` recorded early contexts but deferred OpenXR bootstrap;
- frame `120` still showed `openxrAttempted=0`, then the OpenXR runtime was loaded from the frame path;
- `openxr_bootstrap requirements_ok` and `openxr_session_probe ok` both used `hdc=0x420117aa hglrc=0x30000`;
- the session reached `READY`, reported `VIEW`, `LOCAL`, and `STAGE`, and returned seven GL swapchain formats;
- the process continued through frame `3240` after release with no newer SOMA WER crash found.

The live `0.2.6-xrhold` log confirmed short live-session lifetime:

- active config is `Probe=1`, `SessionProbe=1`, `ReleaseAfterProbe=1`, `BootstrapFrame=120`, and `HoldFrames=600`;
- after a successful frame-context session probe at frame `120`, SOMAVR kept the OpenXR session alive through frame `720`;
- it polled OpenXR events during the hold;
- it released the session and instance with `openxr_runtime released_after_probe reason=hold_complete`;
- later summaries showed `openxrSessionAlive=0` and `openxrInstanceAlive=0`;
- it does not call `xrBeginSession`, create swapchains, or submit frames yet.

`0.2.7-xrmanual` changes the next probe from frame-timed startup to manual start:

- active config is `Probe=1`, `SessionProbe=1`, `ReleaseAfterProbe=0`, `BootstrapFrame=120`, `HoldFrames=0`, and `ManualStart=1`;
- SOMAVR still injects at process launch so WGL/OpenGL hooks are present before GLEW setup;
- OpenXR bootstrap is deferred until F8 is pressed, so the user can reach a loaded save first;
- the trigger logs `openxr_manual_start triggered key=F8 frame=... hdc=... hglrc=...`;
- after the trigger, successful session probes stay alive until process shutdown.

The live `0.2.7-xrmanual` run confirmed the manual gate and in-game context:

- F8 triggered at game frame `4200` after the save was loaded;
- `VirtualDesktopXR 1.0.10` loaded successfully;
- `xrCreateSession` accepted SOMA's active `hglrc=0x30000`;
- the runtime transitioned through `IDLE` to `READY`;
- two views and seven swapchain formats were available;
- the live session remained healthy while SOMA continued rendering.

`0.3.0-xrframe` advances that confirmed session into the first real presentation path:

- OpenXR processing runs at every `SwapBuffers`, independently of the 120-frame summary interval;
- F8 is therefore sampled every rendered frame instead of once every 120 frames;
- `OpenXRRuntime` owns events, session state, frame timing, view location, and composition;
- `OpenXRGLBridge` owns per-eye OpenGL swapchains, images, FBO validation, and backbuffer copies;
- the session begins only after `XR_SESSION_STATE_READY`;
- each running frame uses `xrWaitFrame`, `xrBeginFrame`, `xrLocateViews`, swapchain acquire/wait/release, and `xrEndFrame`;
- the current layer duplicates SOMA's desktop backbuffer to both eyes, so it proves transport but not stereo rendering;
- repeated frame failures are bounded and submission is suspended after 60 consecutive failures.

The live `0.3.0-xrframe` run passed the complete transport test:

- F8 triggered at frame `2783` on the expected main context `hglrc=0x30000`;
- both eye swapchains were `2688x2880`, used `GL_SRGB8_ALPHA8`, and exposed three images;
- the session reached `FOCUSED` and remained there;
- the run submitted at least `938` consecutive two-view projection layers;
- eye positions changed over time, proving live pose updates;
- no `openxr_frame failure`, submission suspension, or other OpenXR error appeared.

`0.3.1-cameramap` keeps that path intact and adds a bounded F9 capture:

- 120 rendered frames by default;
- camera-related `glUniformMatrix4fv` calls only;
- module-relative call stacks for direct Ghidra navigation;
- full 4x4 samples for each relevant uniform;
- synchronized head pose, orientation, view-validity flags, and IPD telemetry.

The two live F9 captures both completed. Sequence 1 included accidental mouse input; sequence 2 used only HMD yaw/roll/pitch. In the clean sequence the OpenXR orientation changed while SOMA's sampled camera matrices remained independent, giving a clean before-bridge control.

Ghidra and the HPL2 source now map the native path from the matrix upload back to `cCamera::GetFrustum` at `0x140271b80` and `cFrustum::SetupPerspectiveProj` at `0x140270230`. `0.4.0-hplcamera` hooks the former and calls the latter after composing a calibrated orientation delta. This keeps view-projection and culling derivatives together.

The first bridge is deliberately bounded:

- exact function-prologue signatures must match before the hook is installed;
- F10 is an explicit enable/disable and neutral-pose calibration gate;
- only perspective frustums with camera-like near/far/FOV/aspect values qualify;
- only the camera selected by the F10 press is modified;
- SOMA's pristine base view is retained and restored on disable;
- position tracking, per-eye separation, OpenXR FOV replacement, and true stereo rendering remain future work.

F9 now distributes each uniform's four full-matrix samples across the 120-frame window instead of consuming them immediately. This makes the next capture suitable for measuring native camera response over the full head movement.

The live `0.4.0-hplcamera` test passed:

- F10 drove the native HPL camera for `1210` consecutive renders, not mouse input;
- rotation reached about `35.7` degrees and matrix captures changed across the full F9 window;
- disabling F10 restored the cached base view;
- OpenXR continued beyond `1800` submissions without failures;
- IPD was stable near `0.06852` meters.

`0.5.0-afrstereo` established the current experimental stereo layer. F11 alternates the HPL camera between runtime left/right eye position and FOV, while persistent GL caches retain the latest image for each eye. Both cached images are submitted with the exact OpenXR poses used to render them. This is not simultaneous stereo: each eye updates on alternating game frames.

`0.5.1-compatprobe` added exact-signature passive hooks at the six native viewport stages and the FMOD listener update without changing camera, AFR, or audio behavior.

The live `0.5.1` test passed and is analyzed in `docs\RUNTIME_ANALYSIS_0.5.1.md`. F11 delivered user-confirmed stereo for more than `1765` submitted frames without transport or cache failure. World rendering resolves into FBO `11`, post effects resolve into FBO `0`, and screen GUI remains on FBO `0`. Listener vectors remained authored while HMD orientation changed, confirming that audio needed an independent head delta.

The live `0.5.2-audiopost` test is analyzed in `docs\RUNTIME_ANALYSIS_0.5.2.md`. Audio appeared correct, although a stronger directional-source test remains. The run reached frame `10920` and `6350` stereo submissions without transport or hook failure. F12 mainly increased contrast and did not affect the dominant defect, proving it is upstream of post composition. The user identified realtime shadows as different between eyes and movement-dependent.

`0.5.3-shadowjitter` was the preceding build. It kept F8/F10/F11/F12 and audio behavior unchanged and added an F7 zero-radius experiment for `avShadowMapOffsetMul`.

The live `0.5.3` result showed that F7 itself worked but the proposed uniform control point did not: all toggles had `uploads=0 overrides=0`. Reflection artifacts are now also confirmed. SOMA's cube/environment path is eye-vector dependent, while world reflections sample a reflection texture generated from a mirrored current frustum. This makes shared per-frame resources under AFR the leading common hypothesis.

`0.5.4-renderdiag` captured three trouble spots successfully. Direct eye camera
matrices alternate correctly, but live deferred shadow programs `942/944` and
world reflection program `989` receive their inverse camera and screen
reconstruction values through uniform blocks. This is the first common mechanism
that directly fits the observed shadow and reflection displacement.

The live `0.5.5-reconstruct` run confirmed that F5 removes the left/right shadow
disagreement. Shadow UBO offset `96` changes from `-0.242513/+0.242513` to `0/0`,
so centered horizontal projection is now the active compatibility policy. The
remaining shadows and lighting are stereo-consistent but move with HMD position;
program `988` also identifies a view-depth reflection/refraction fade candidate
for the moving opaque window boundary.

The live `0.5.6-stability` run proved clean shutdown. The lifecycle hook installed,
released OpenXR before HPL Graphics teardown, and SOMA disappeared normally. F3
patched program `985` for `369` draws without visual change, rejecting reflection
distance fade. F4 showed little translation dependence; shadows instead correlate
strongly with HMD pitch and roll.

`0.5.7-fullcenter` established the current visual baseline. The prior policy left vertical projection offset
`-0.193187`; the new policy centers both projection axes while preserving tangent
span and synchronized submitted FOV. This directly tests the pitch/roll, diagonal
ceiling, and top-down window symptoms.

The user confirmed `0.5.7` fixes all observed shadow and reflection defects.
`0.5.8-onekey` promoted F10 to the normal usability path. F10 requests OpenXR and holds a
pending activation until valid pose/stereo views arrive, then enables tracking,
AFR stereo, and full centering together. A second F10 cancels or exits. F8/F11
remain diagnostic controls only.

The first architecture maintenance pass keeps the `0.5.8-onekey` behavior and
binary contract but moves deterministic responsibilities out of runtime hooks:
`HPLCameraMath` owns pose/projection math and the proven fully centered FOV policy,
`OpenGLMatrixAnalysis` owns matrix telemetry classification, and `OpenXRHelpers`
owns OpenXR names and view/pose conversion. `somavr_render_math_tests` runs in both
build flavors. Module boundaries and the next safe extractions are recorded in
`docs\ARCHITECTURE.md`.

The first live `0.5.8` test exposed severe view skew during HMD yaw and pitch.
The extracted quaternion-to-matrix function had an incorrect XY cross-term and
therefore generated a shearing, non-orthogonal camera rotation. `0.5.9-rotationfix`
corrects that term and adds orthonormality plus quaternion/matrix agreement tests.
That rigid-rotation fix remains in the active build; all one-key, full-center,
AFR, room-scale, audio, and shutdown policies are otherwise unchanged.

The live `0.5.9` test confirmed rigid camera rotation, then exposed a separate
one-key startup problem: OpenXR frame `2857` reported head `Y=-1.244683`, F10
captured it immediately, and frame `2858` settled roughly `1.79 m` higher. That
reference-space transition was incorrectly applied as room-scale head movement.
`0.5.10-poselatch` requires tracked position/orientation and eight consecutive
settled unique poses before neutral capture. Large startup jumps reset the latch;
projection, stereo, world scale, and normal physical head translation are unchanged.

The user confirmed `0.5.10` has no current graphical issues. `0.5.11-recenter`
keeps that path and adds F2 as an in-session neutral-pose recenter. It uses the
same tracked/stable latch as F10, leaves OpenXR and AFR stereo running, continues
rendering with the old neutral pose while waiting, then atomically replaces the
neutral orientation and position once eight stable tracked samples arrive.

`0.6.0-input-foundation` batches the first controller, calibration, tracking
diagnostic, and release-identity foundations without changing the proven default
camera transform. `OpenXRInput` owns a seven-action gameplay set, suggested
Simple, Touch, Index, and Motion Controller bindings, per-frame action synchronization, and
left/right grip and aim spaces. The public snapshot includes move/turn axes,
select, squeeze, menu, pose validity, and tracked bits. No snapshot value is fed
into SOMA yet, so keyboard/mouse behavior and native gameplay remain authoritative.

The camera bridge now supports `HPLRoomscaleVertical` and
`HPLEyeHeightOffsetMeters`. Their active defaults (`1` and `0.0`) are mathematically
identical to `0.5.11`; they provide a reversible route to seated/standing tuning.
Head snapshots now report sample age, and every build emits a SHA-256 manifest
beside the DLL.

The exit minidump disproved the earlier orphan-worker diagnosis for this run. It
contains only SOMA's main thread in OpenGL with Virtual Desktop runtime frames.
Ghidra names `HPL3_cSDLEngineSetup_Destructor` at `0x1403b16e0`. The `0.5.5`
lifecycle hook failed closed because its guard omitted the leading `0x40` byte;
`0.5.6` uses the exact installed sequence and again attempts OpenXR release before
HPL deletes Graphics and calls `SDL_Quit`. The dumper remains capture-only.

The design choices borrowed from UEVR and Praydog's analysis are recorded in `docs\UEVR_LEARNINGS.md`. Unreal-specific object assumptions were deliberately not imported.

A docs-only static RE pass now maps future locomotion, hands/tools, HUD, and full-screen effects in `docs\FUTURE_SYSTEMS_RE.md`. It identifies the main-loop and viewport order, semantic character-body movement route, camera-follow hand model, HUD/ImGui/world-GUI split, and the priority-sorted post-effect chain. No current `0.5.0-afrstereo` binaries or runtime configuration were changed by that pass.

A second compatibility pass in `docs\VR_COMPATIBILITY_RE.md` maps controller ownership onto SOMA's existing pick and PID physics, inventories authored camera states, identifies the FMOD listener commit, classifies loading/video presentation, and narrows the same-frame stereo boundary. `docs\FEATURE_TRACEABILITY.md` assigns stable `FEATURE.*` IDs and acceptance gates, and the local Graphify graph indexes these documents with the implementation. This remains documentation/tooling work only; the `0.5.0-afrstereo` binary is unchanged.

The shared `Soma_NoSteam.exe` Ghidra database was synchronized again on
2026-07-15. In addition to lifecycle evidence, the exact hand SetMatrix and
game-pause functions now carry the `0.16.0` controller-root and complete
pause/menu-input ownership contracts. The complete ledger is in
`docs\GHIDRA_SYNC.md`.

The OpenXR build now asks for:

- instance extension availability, especially `XR_KHR_opengl_enable`;
- runtime and HMD system properties;
- OpenGL graphics requirements for the live SOMA HDC/HGLRC;
- primary stereo view sizes and swapchain sample counts;
- environment blend modes;
- optional `xrCreateSession` using `XrGraphicsBindingOpenGLWin32KHR`;
- reference spaces, swapchain formats, and early session-state events when session creation succeeds.

## Next Step

The prioritized multi-feature live plan is maintained in
`docs/NEXT_LIVE_EVIDENCE.md`.

Launch the rolling `0.95.4-rig-terminal-read` package:

```powershell
& "D:\Dev Debug\SOMAVR\out\SOMAVR-latest\somavr_injector.exe" --launch "G:\SteamLibrary\steamapps\common\SOMA\Soma_NoSteam.exe" "D:\Dev Debug\SOMAVR\out\SOMAVR-latest\somavr.dll"
```

Run `somavr_injector --doctor` first and require zero failures. The next normal
headset pass should first validate the retained-hands crash cleanup and
automatic per-eye-history recovery after tracking loss, then collect the
matched-scene `openxr_gl_transfer`, `openxr_pacing`, freshness,
`hpl_occlusion_query_summary`, and `hpl_framebuffer_copy_summary` evidence. A
separate opt-in `HandTrackingProbe=1` pass can then prove the player-hands owner
timing. Exact steps, expected records, rollback rules, and promotion gates are
kept only in `docs/NEXT_LIVE_EVIDENCE.md` to avoid another stale duplicate
checklist here.

## apitrace Camera Confirmation (2026-08-10)

Captured a vanilla `Soma_NoSteam.exe` GL trace (no mod) with the apitrace MCP and confirmed the camera
end-to-end at the GL-driver layer:

- **View-projection**: `glUniformMatrix4fv(program=822, location=1)` — `track_camera` reported
  `camera_moves=true`; eye position and orientation tracked live gameplay movement (walk → stop → walk).
- **Projection**: `glUniformMatrix4fv(program=884, location=1)` — FOV_y 70° / FOV_x 102°, near 0.03,
  far 998.67, GL right-handed. Matches `game.cfg` and the native `frustum` packet.

Trace: `D:\Dev Debug\apitrace\traces\soma-gl-c3c7442f2aed` (3874 frames). This is independent
confirmation of the existing native camera RE, not a new lane. Detail in `docs/future-hook-map.md`
and `docs/HPL_OPENGL_NOTES.md`. GL program object names are trace-local evidence;
the uniform semantics and native frustum anchors, not numeric program IDs, are
the stable implementation authority.
