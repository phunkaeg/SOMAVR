# Hypotheses

## S22 - Native picker distance can drive a stable binocular controller reticle

Status: GUARDED BUILD READY (`0.20.0-depth-reticle`)

Hypothesis: an application-space OpenXR quad placed on the exact controller aim
pose at SOMA's finalized closest-entity distance provides stable binocular depth
without a second ray query or OpenGL depth reconstruction.

Evidence:

- `SOMA_GetClosestEntity` finalizes distance `+0x18`, body `+0x20`, and entity
  `+0x28` after applying native length, LOS, range, and `CanInteract` policy.
- `HPLInteractionBridge` already publishes that result with the exact app-space
  controller aim pose used to build the HPL query.
- The gameplay HUD proves source-alpha OpenXR quad swapchain creation and frame
  composition on SOMA's current OpenGL context.
- `0.20.0` adds a dedicated reticle swapchain, angular-size math, stale-hit and
  tracking guards, compositor submission counters, and focus-change haptics.

Confirms if the marker converges at target depth, stays rigid while head and
controller move, maintains comfortable apparent size, and clears immediately on
no hit, fallback, tracking loss, or F10 disable without shader regressions.

Redirects if convergence is correct but lack of world occlusion is distracting;
retain the native result contract and move drawing to a depth-tested engine pass.
If marker visibility disagrees with SOMA's icon/interaction state, map the native
crosshair semantic owner before changing picker policy.

## S21 - Semantic camera-add filtering removes VR motion without breaking authored states

Status: GUARDED BUILD READY (`0.19.0-comfort-focus`)

Hypothesis: zeroing only native camera-add Bob, Shake, and Sway at the registered
setter while VR tracking is active removes artificial head motion without
interfering with crouch, climb, terminals, scripts, death, lean, crawl, or
conversation camera ownership.

Evidence:

- The shipped `Player_Types.hps` gives stable IDs for all eleven channels.
- Registration string and decompilation confirm wrapper `0x140159360` with type
  in EDX and vector in R8; the build guards its exact first 18 bytes.
- The hook calls the original setter with zero, clearing current/goal state rather
  than bypassing native lifecycle.
- Suppression is gated by active F10 tracking and independently configurable per
  comfort channel.

Confirms if locomotion and impacts lose artificial camera motion while all
authored state transitions remain intact and logs show only IDs `1/2/9` changed.
Redirects to per-state policy if a selected channel is intentionally reused by a
special sequence.

## S13 - Native torque and impulse boundaries can support physical VR manipulation

Status: GUARDED BUILD READY (`0.18.0-interaction-polish`)

Hypothesis: controller orientation can steer SOMA's existing Grab torque target,
and a controller-armed native AddImpulse call can provide physical throw direction,
without replacing solver, inertia, collision, mass, or callback ownership.

Evidence:

- The shipped script uses torque PID `40/0/0.4|0.1`, `angle * 100`, speed cap `6`,
  body angular-velocity feedback, inertia transformation, and torque cap `1000`.
- `HPLGrabBridge` changes only that exact torque error in Grab state and retains
  the native pipeline after a shortest-arc controller quaternion target.
- AddImpulse wrapper `0x14049c720` is an exact virtual thunk to vtable `+0x130`.
  The reversible patch is one-shot, expires after 350 ms, and requires Grab state.
- Throw direction uses release velocity above a threshold and grip forward below;
  native impulse magnitude is preserved before optional `1.0..2.0` scaling,
  with a small camera-forward clearance component to avoid immediate body collision.

Confirms if objects settle to controller orientation without persistent spin and
each deliberate throw produces exactly one correctly directed impulse across light
and heavy bodies. Redirects to sign/axis calibration or earlier goal-matrix
ownership if rotation fights the native target; disable speed scaling if authored
object classes need fixed strength.

## S12 - Controller translation can steer SOMA's native Grab PID safely

Status: SUPERSEDED BY S13; TRANSLATION ACCEPTANCE STILL PENDING

Hypothesis: adding camera-relative dominant-grip displacement to the existing
Grab position error, while retaining SOMA's exact PID and physics response, will
produce physical controller-owned object translation without bypassing mass,
collision, gravity, joints, limits, or script callbacks.

Evidence:

- The shipped Grab state configures vector PID gains `400/0/40` and passes
  `wantedPosition - bodyPosition` to confirmed output `0x140238750`.
- `HPLGrabBridge` requires that tuple, player state Grab `1`, native camera
  ownership, fresh fully tracked grip, and a matching camera/player anchor.
- The pickup sample is unchanged; invalid or changing ownership resets the
  anchor and forwards the original error.
- The same output's torque tuple is now controlled under S13; live axis and
  stability acceptance remain pending.

Confirms if held objects follow controller translation without pickup jumps,
oscillation, runaway force, broken collision, or joint/callback regressions, and
tracking loss returns cleanly to native behavior.

Redirects if camera-relative displacement rotates incorrectly with body yaw or
native held-depth updates fight the controller target; use a world-space anchor
or intercept the earlier wanted-transform construction while retaining PID output.

## S1 - OpenXR can bind directly to SOMA's live OpenGL context

Status: CONFIRMED

Hypothesis: SOMAVR can create an OpenXR instance and session using `XR_KHR_opengl_enable` with SOMA's active `HDC` and `HGLRC`. If true, the first VR presentation proof can run inside the existing OpenGL render thread without a D3D translation layer.

Evidence:

- SOMA is an OpenGL title and the first live log captured a stable NVIDIA OpenGL 4.6 context.
- The OpenXR probe path passes the live `HDC/HGLRC` through `XrGraphicsBindingOpenGLWin32KHR`.
- The same build enumerates OpenXR stereo view sizes, blend modes, reference spaces, and swapchain formats when session creation works.
- The first `0.2.0` rerun did not test this hypothesis because it loaded the non-OpenXR DLL (`buildOpenXR=0`). `0.2.1-xrpathguard` adds build-flavor checks to prevent that from being mistaken for runtime evidence.
- The first correct OpenXR run (`0.2.1`) crashed before `openxr_extensions`; WER exception `0xc06d007e` pointed to delayed `openxr_loader.dll` resolution, not an OpenXR runtime rejection. `0.2.2-xrloaderpath` fixed this.
- Live `0.2.2` confirms the static OpenXR facts: `VirtualDesktopXR 1.0.10`, `Meta Quest 3`, `XR_KHR_opengl_enable=1`, GL requirement range `4.0.0` to `5.0.0`, two `2688x2880` recommended stereo views, opaque blend mode. It still crashed later in `VirtualDesktop.LibOVRRT64_1.dll` with `0xc0000005` while the instance remained alive and `SessionProbe=0`.
- `0.2.3-xroneshot` supports the live-instance lifetime theory: it released the instance after static discovery and SOMA kept rendering afterward with no newer SOMA WER crash found.
- `0.2.4-xrsessiononeshot` confirms that `xrCreateSession` accepts SOMA's early startup `HDC/HGLRC`, reaches `READY`, and reports reference spaces plus GL swapchain formats.
- `0.2.5-xrframeprobe` confirms that the same session one-shot works on the later render-frame `HDC/HGLRC`.
- `0.2.6-xrhold` confirmed that a live frame-context OpenXR session can remain alive inside SOMA for a short controlled window without beginning/submitting frames.
- `0.2.7-xrmanual` moves session creation behind F8 so OpenXR can be started after a save is loaded while keeping launch-time GL hooks.
- The live `0.2.7-xrmanual` run triggered at frame `4200`, loaded `VirtualDesktopXR 1.0.10`, created the session on `hglrc=0x30000`, reached `READY`, and remained alive while SOMA continued rendering.
- `0.3.0-xrframe` is the first build that uses the confirmed binding for swapchains and frame submission.
- The live `0.3.0-xrframe` run submitted at least `938` consecutive two-view layers while `FOCUSED`, with no frame failures. The direct OpenGL/OpenXR presentation path is now proven rather than provisional.

Confirms if:

- The log starts with `version=0.2.7-xrmanual buildOpenXR=1`.
- `openxr_manual_start waiting key=F8` appears before OpenXR runtime creation.
- After F8, `openxr_manual_start triggered key=F8 frame=...` appears on the active frame context.
- `openxr_loader_load ok` appears.
- `openxr_extensions ... khrOpenGL=1`.
- `openxr_bootstrap requirements_ok` reports a GL range that includes SOMA's OpenGL 4.6 context.
- `openxr_session_probe ok` appears, followed by `openxr_reference_spaces` and `openxr_swapchain_formats`.
- The session probe's `hglrc` matches the frame-summary render context.
- Later summaries show `openxrManualStartArmed=1`, `openxrInstanceAlive=1`, and `openxrSessionAlive=1` until shutdown.

Falsifies or redirects if:

- `khrOpenGL=0`, which means the selected OpenXR runtime cannot support this binding path.
- Requirements succeed but session creation fails from SOMA's first context. That would not kill the OpenXR plan; it would mean we should defer session creation to a later frame/context point or use a different bridge.
- Basic instance/system creation fails because no headset/runtime is active. Rerun after confirming the OpenXR runtime outside SOMA.
- `openxr_loader_load failed` redirects to dependency/search-path handling before any runtime conclusions.
- The later `VirtualDesktop.LibOVRRT64_1.dll` crash persists despite `openxrInstanceAlive=0`; then the issue is runtime/library load side effect rather than live instance lifetime, and the next path should defer OpenXR creation until later or use a short-lived external/helper probe.

## S2 - The main camera path is shader-uniform/native-camera, not fixed-function GL

Status: LIKELY

Hypothesis: SOMA/HPL3 computes the world camera/projection in engine code and uploads it through GLSL uniforms. The first useful stereo route should target native camera/frustum state or GLSL matrix mutation, not `glLoadMatrixf`.

Evidence:

- First live run showed `fixedProjection={valid=0}` across frame summaries.
- The same run repeatedly found projection-like `glUniformMatrix4fv` uploads.
- `a_mtxModelViewProjection` matched the configured SOMA 70 degree FOV at the user's 3440x1440 aspect.
- Related uniform names include temporal and inverse view/projection matrices, which suggests HPL3 maintains a coherent shader-side camera packet.

Confirms if:

- Further logs keep showing stable projection-like `a_mtxModelViewProjection` uploads.
- Static analysis finds HPL3 camera/FOV code feeding the same 70 degree FOV and near/far plane values from `config\game.cfg`.

Falsifies or redirects if:

- A different render path later produces fixed-function projection uploads for gameplay scenes.
- Uniform upload timing proves too late for culling and view-dependent effects, in which case the stereo route must move earlier to native camera ownership.

## S3 - Camera-uniform call stacks lead back to the native HPL3 frustum owner

Status: CONFIRMED

Hypothesis: stack frames above `glUniformMatrix4fv` for `a_mtxModelViewProjection`, temporal view/projection, and inverse view/projection uniforms will contain stable `Soma_NoSteam.exe` RVAs near HPL3's low-level matrix setter and the renderer/frustum callers that own the gameplay camera.

Evidence:

- HPL2 `iRenderFunctions::SetProjectionMatrix` forwards the frustum projection to `iLowLevelGraphics::SetMatrix`.
- `SetModelViewMatrix` and object `SetMatrix` similarly feed the low-level graphics layer.
- HPL2 camera code keeps projection, view, and frustum state separately, matching the family of HPL3 shader uniforms already observed.
- Praydog/UEVR uses return addresses and call stacks as anchor points when a late rendering hook is known but the owning engine function is not.
- Two F9 captures completed with stable executable-relative stacks. The clean second capture contained HMD-only movement.
- `+0x55ab0a -> +0x435b33 -> +0x2ac5f0 -> +0x2ad979` maps from the GLSL matrix setter through low-level `SetMatrix`, `iRenderFunctions::SetProjectionMatrix`, and normal frustum projection selection.
- Ghidra plus HPL2 source matching identified `0x140271b80` as `cCamera::GetFrustum` and `0x140270230` as `cFrustum::SetupPerspectiveProj`.

Confirms if:

- F9 capture logs one or more stable `Soma_NoSteam.exe+0x...` frames above camera uniform uploads.
- The same stack branch handles related projection/view uniforms or leads to functions with HPL camera/frustum structure patterns in Ghidra.

Redirects if:

- stacks stop entirely in the NVIDIA driver or generic GL wrapper without useful executable frames;
- all relevant uploads share only one opaque low-level setter, requiring caller-sensitive stack depth or a mid-hook at the setter;
- the native addresses or structure behavior fail to match the HPL2 source; this did not occur in the current executable.

## S4 - HMD orientation can be applied coherently at native frustum setup

Status: CONFIRMED

Hypothesis: composing the inverse calibrated OpenXR orientation delta before SOMA's pristine world-to-camera view, then calling native `cFrustum::SetupPerspectiveProj`, will update visible camera motion and HPL3 culling coherently without modifying the camera object's own matrix cache.

Evidence:

- HPL2 and HPL3 both compute `viewProjection = projection * view` inside common frustum setup, then update planes, sphere, vertices, and bounding volume.
- OpenXR and HPL's OpenGL camera convention are both right-handed with forward along negative Z.
- The bridge runs from `cCamera::GetFrustum`, which is queried by the render viewport even when SOMA's camera is not dirty.
- The clean capture proves HMD pose is valid and independent of mouse/game camera motion.
- Live `0.4.0` applied up to roughly `35.7` degrees of HMD rotation for `1210` renders, changed native camera matrices across the F9 window, restored cleanly, and did not disturb OpenXR submission.

Confirms if:

- F10 calibration succeeds and small yaw/pitch/roll produce corresponding game-camera motion.
- F9 matrix samples change during headset-only movement after F10.
- culling remains visually coherent and OpenXR submission continues without errors.

Redirects if:

- one or more axes are reversed or swapped, requiring an explicit OpenXR-to-HPL basis conversion;
- camera motion works but culling is wrong, indicating another cached frustum or render-view copy must be updated;
- the camera updates only intermittently, requiring a hook earlier in the render viewport or a late update closer to submission.

## S5 - Alternating cached eye renders can prove native stereo

Status: CONFIRMED (`0.5.1-compatprobe`)

Hypothesis: alternating the native HPL frustum between OpenXR left/right poses and asymmetric FOVs, while retaining the latest render for each eye, will produce correct stereo direction and scale without requiring the engine to render twice inside one game frame.

Evidence:

- `cCamera::GetFrustum` is reached once per observed game render, giving a stable left/right alternation point.
- The runtime reports stable eye separation near `0.0685` meters and valid per-eye FOVs.
- Native frustum setup recomputes projection, view-projection, planes, vertices, sphere, and BV from the supplied eye matrices.
- The OpenXR GL bridge already owns one swapchain per eye; persistent intermediate caches separate game-render cadence from swapchain image acquisition.

Confirms if:

- F11 produces alternating HPL eye logs and both eye caches become ready;
- OpenXR reports stereo submissions without cache or frame failures;
- scene depth has the correct direction and approximate world scale;
- disabling F11 returns immediately to the proven F10 mono path.

Redirects if:

- temporal mismatch is too uncomfortable even with correct geometry, requiring same-frame dual rendering;
- projection direction is reversed, requiring an off-axis sign correction;
- world scale is wrong, requiring `HPLWorldScale` calibration;
- effects or culling are eye-dependent in ways that require pass classification.

Live result: the user reported full stereo vision, and the log recorded at least
`1765` stereo submissions with no capture failure or suspension. Remaining visual
defects are now tracked under S9 rather than as failure of native stereo geometry.

## S6 - OpenXR locomotion should enter through semantic player movement

Status: GUARDED CONTROL BUILT, LIVE ACCEPTANCE PENDING

Hypothesis: feeding controller axes into SOMA's action/move-state path or the registered `iCharacterBody::Move(eCharDir, float)` wrapper will preserve speed modifiers, crouch/run/jump state, AI sound, breathing, and interaction restrictions better than keyboard synthesis or direct capsule movement.

Evidence:

- `script\base\InputHandler.hps` separates move, look, and gameplay actions.
- `MoveState_Normal.hps` applies the gameplay speed and state policy before native movement.
- `0x1404a5030` registers the native character-body API and maps `Move` to `0x1402375f0`.
- Released HPL2 source shows `Move` accumulates directional input for the physics update.
- `0.14.0` signature-guards `Move`, `AddYaw`, and `GetGamePaused`, then uses
  direct analog/radian calls only in unpaused Normal/Normal ownership. All other
  states retain semantic key/mouse input.

Confirms if analog magnitude, diagonal speed, run/crouch, collision, movement
audio, pause safety, and exact turn direction remain correct, while special
states automatically report and behave through the semantic fallback.

Redirects if SOMA's action dispatcher performs required work before `Move`; hook the higher semantic action path instead.

## S7 - SOMA's hands can become controller-owned without replacing the content

Status: GUARDED BUILD READY (`0.16.0-controller-hands`)

Hypothesis: the existing hands mesh, animations, `R_Hand` tool attachments, and callbacks can be retained while replacing the default camera-follow matrix with a dominant-controller pose.

Evidence:

- `PlayerHandsHandler.hps` constructs a world entity from `hands_human.ent` and updates one explicit matrix in `PostUpdate`.
- Tool/HudObject entities are attached to the `R_Hand` socket and updated separately.
- Custom position, custom rotation, full-scale, and camera-attachment states are already explicit script flags.
- `0x14000fb60` returns the registered Lux entity name at `+0x120`, while
  `0x1400bcd90` receives the shared script SetMatrix object and matrix pointers.
- The shipped script proves `cameraRotation * rotateY(pi) * scale`, and HPL2
  source proves basis columns plus translation `[3,7,11]`.
- `0.16.0` filters exact `PlayerHands_*` identity and substitutes a configurable
  tracked-grip root only for uniform quarter-scale Normal/Normal ownership.

Confirms if live output shows correct root calibration, native animation/socket
preservation, and automatic fallback for full-scale/authored/tracking-loss states.

Redirects if the scaled mesh or authored animations cannot tolerate physical scale; add per-tool pose/scale profiles or a dedicated viewmodel projection path.

## S8 - The final GUI-set pass is the right HUD extraction boundary

Status: GAMEPLAY HUD BUILD READY (`0.15.0-hud-layer`)

Hypothesis: exact GameHudSet identity during `0x1402981e0`, redirected at the
2D set renderer `0x140213970`, will capture gameplay HUD after scene post
effects without contaminating the per-eye scene targets.

Evidence:

- The main loop queues `OnDraw` before `0x140298850` renders viewports.
- `0x140298630` runs scene, post composite, `PostPostEffect`, then `0x1402981e0`.
- Crosshair, descriptions, flashes, and infection draw through `GameHudSet`;
  inventory, hints, menus, wake, and game-over use ImGui. Later RE established
  exact flat authorities: pause owns main menu, module `15` action `12` owns
  inventory, wake/dead have dedicated state, and hints/credits use GameHudImGui.

The `0.15.0` build implements that transaction and submits the result as a
VIEW-space alpha quad. It confirms if live output contains gameplay HUD pixels
with usable alpha while the eye scene remains complete and diegetic terminal GUI
remains in-world.

Redirects if alpha/scale is unusable or the exact set has hidden side effects;
restore native rendering. ImGui menus and subtitles known to use another owner
remain a separate capture/classification stage rather than an assumption.

## S9 - Temporal and optical screen effects require an explicit VR policy

Status: RUNTIME SUPPORTED, F12 ISOLATION BUILD READY (`0.5.2-audiopost`)

Hypothesis: image trail, lens distortion, chromatic aberration, radial blur, depth of field, shake, sway, and authored camera roll will be unstable or uncomfortable in stereo unless disabled, attenuated, or given per-eye resources.

Evidence:

- Image trail owns explicit history texture/framebuffer resources at `0x14038ae60`.
- The post chain is priority sorted and executes after the scene through `0x14033bd80`.
- Lens distortion would pre-distort an image that the OpenXR runtime must distort again.
- Shake and sway mutate additive camera position/roll independently of raw HMD tracking.

Confirms if an effect-activity capture correlates those effects with cross-eye mismatch or discomfort and the conservative policy removes it while preserving tone mapping, bloom, fog, and grading.

Redirects if an effect is already stereo-safe; retain it per eye, but keep temporal history isolated between eyes.

Live result: visible shader defects occurred under otherwise stable stereo, but no
GLSL compile/link error was logged. FBO telemetry proves active post effects resolve
`11 -> 0`; F12 now bypasses that chain for direct A/B classification.

## S10 - Controller interaction should reuse native physics ownership

Status: STATICALLY SUPPORTED

Hypothesis: replacing camera-derived pick and target poses with controller poses,
while retaining SOMA's native `CanInteract`, range, PID force/torque, impulse, and
state code, will provide physical VR interaction without duplicating map logic.

Confirms if controller selection matches native selection and grab, rotate,
release, throw, collision, and scripted callbacks remain stable.

Redirects if a state consumes required mouse/action side effects before its native
physics solver; inject at that higher semantic input boundary instead.

## S11 - Frustum-only tracking leaves the audio listener body-aligned

Status: CONFIRMED; CORRECTION BUILD READY (`0.5.2-audiopost`)

Hypothesis: because `HPLCameraBridge` modifies the returned render frustum rather
than the script-owned `cCamera`, `0x140289340` continues committing authored camera
orientation to FMOD instead of the HMD's center-head orientation.

Confirms if listener forward/up remain unchanged while the HMD moves under F10.
A late center-head correction should then rotate sound with no eye-dependent offset.

Redirects if another engine path already updates the listener from the modified
frustum; identify that owner and avoid a duplicate FMOD override.

Live result: listener forward/up remained fixed across large HMD quaternion changes
for multiple 120-frame intervals. `0.5.2` rotates only the vectors committed to FMOD
and immediately restores SOMA's authored listener fields.

## S12 - Safe dual rendering must remain below renderer frame ownership

Status: BUILT AS OPT-IN PROTOTYPE (`0.47.0-stereo-view-history`)

Hypothesis: `0x140298850` must execute once because it advances renderer frame state,
while selected scene/post work below `0x140298630` can execute once per eye after
viewport callbacks and temporal resources are classified.

Confirms if bounded stage probes show one game/render frame, two eye-local scene
and post executions, one flat GUI execution, and no duplicate script/audio/update
side effects.

Redirects if `0x1401f9790` or adjacent callbacks mutate once-per-frame state; split
the hook deeper or snapshot/restore only the proven render-local state.

Live result: one stable viewport executes world `0 -> 11`, overlays on `11`, post
`11 -> 0`, PostPostEffect on `0`, and GUI on `0`. `0.45.0` replays only the exact
player `HPL3_Scene_RenderViewport` with GUI bit `2` removed and never duplicates
the renderer frame owner at `0x140298850`. Pre/post-world callbacks and the
stateful post-post phase still repeat, so default promotion remains gated on live
  visual, temporal, and performance evidence.

`0.46.0` resolves the first proven temporal dependency: the exact previous-view
matrix copied by post-post is restored per eye before the complete viewport and
captured afterward. Other post-effect histories remain promotion gates.

`0.47.0` applies that same ownership to AFR and treats recenter or long pose gaps
as camera cuts, eliminating two additional stale/cross-eye history paths.

## S13 - Post-chain bypass can classify the reported shader defects

Status: REDIRECTED BY LIVE TEST (`0.5.2-audiopost`)

Hypothesis: returning false from `HPL3_PostEffectComposite_HasActiveEffects` while
leaving world render and final screen GUI active will remove defects owned by the
post chain without affecting native stereo geometry or OpenXR submission.

Confirms if F12 removes or materially changes the defect and the stage log stops
showing `post_effects` while world/HUD rendering continues.

Redirects if the defect is unchanged; inspect world-shader temporal/view uniforms,
reflection cameras, shadows, and eye-alternating history instead.

Live result: F12 mainly increased contrast and did not remove the eye mismatch or
movement-dependent instability. The user identified realtime shadows as the
dominant defect. Continue in the deferred shadow path, while retaining F12 for
later per-effect policy work.

## S14 - Screen-space soft-shadow jitter causes the stereo shadow shimmer

Status: CONTROL POINT NOT REACHED (`0.5.3-shadowjitter`)

Hypothesis: SOMA's soft-shadow jitter chooses different random kernels for the
same world point in each eye because `vScreenJitterCoord` is derived from
`px_vPosition.xy`. Zeroing only `avShadowMapOffsetMul` will make shadows harder
but remove most eye-to-eye difference and movement-dependent swimming.

Evidence:

- The installed HPL shader derives jitter lookup coordinates from screen pixels.
- `avShadowMapOffsetMul` scales every sampled scatter offset.
- HPL2 uploads the value through `glUniform2f`; its low-quality branch performs a
  single comparison without jitter smoothing.
- F12 proves the dominant issue is not the post composite.

Confirms if F7 produces increasing override counters and materially stabilizes
shadows in both eyes, with the expected tradeoff of harder edges.

Redirects if large shadow shapes or cascade boundaries still differ. In that
case, capture `a_mtxLightViewProj0..3`, `avSplitsNear`, `avSplitsFar`, and shadow
map FBO generation per eye before attempting matrix reuse.

Live result: F7 was detected, but all three toggles reported zero target uploads
and zero overrides. The test did not modify rendering. Use the F6 generated-program
inventory in `0.5.4-renderdiag` to identify the live shadow variant and setter.

## S15 - AFR shares view-dependent shadow/reflection resources between eyes

Status: PARTIALLY CONFIRMED

Hypothesis: shadows and reflections differ because alternating-eye camera state
feeds resources whose ownership is one-per-game-frame rather than one-per-eye.
The right and left cached images are also one game frame apart.

Evidence:

- Both reported defects are in world rendering and survived the F12 post bypass.
- Environment reflection depends directly on the current eye vector and inverse
  view matrix.
- World reflection uses a screen-sampled reflection map rendered from a mirrored
  current frustum; HPL2 reuses a shared reflection buffer without clearing it.
- SOMA's temporal SSAO path also uses one previous-frame texture and temporal view
  matrices, proving that shared frame history exists in this renderer family.

Live result: F6 found deferred shadow programs `942/944` and world reflection
program `989` on world FBO `11` in both eyes. Direct temporal and inverse camera
matrices alternate correctly, but the deferred reconstruction values are uniform
block members and were not visible to the original direct-uniform capture. Shared
program/FBO ownership is confirmed; exact eye-local uniform-buffer contents remain
the deciding evidence.

Redirects if resources are already eye-local. Then compare generated shader
variants, sampler bindings, clip planes, and exact eye render poses independently.

## S16 - Deferred reconstruction remains mono-centered under asymmetric eye FOV

Status: CONFIRMED

Hypothesis: eye geometry uses the correct asymmetric OpenXR projection, but the
uniform-block camera packet used by deferred shadows and screen-space reflections
still reconstructs from a mono-centered or stale projection. This introduces a
horizontal screen-space displacement equal to the per-eye projection-center
offset.

Evidence:

- The user reports that realtime shadows are offset by the same amount as the
  per-eye crosshair displacement.
- Direct eye matrices alternate with the expected opposite horizontal offsets.
- Shadow programs `942/944` and reflection program `989` read inverse projection,
  inverse view, and screen reconstruction parameters from uniform blocks.
- F7 saw no direct target upload because the relevant live path is block-backed.

Confirms if F5's symmetric horizontal projection materially reduces the shadow
and reflection displacement, or if F6 UBO snapshots show identical/mono-centered
projection data for eye 0 and eye 1 while direct matrices differ.

Redirects if the UBO packet is correctly eye-local and F5 has no effect. Then
compare shadow/reflection render-target generation, sampler bindings, temporal
history, and the one-frame AFR age difference.

Live result: F5 removed the left/right shadow disagreement. UBO offset `96`
changed from `-0.242513/+0.242513` to `0/0`, proving the visual displacement
tracks the asymmetric horizontal projection center. Centered horizontal
projection is the current compatibility policy.

## S17 - Room-scale head translation destabilizes camera-relative shadow state

Status: MOSTLY FALSIFIED

Hypothesis: after projection centering, the remaining shadow and angular lighting
motion is driven by tracked head-center translation entering camera-relative
light volumes, shadow-map selection, or shadow frustum setup.

Confirms if F4 disabled mode, which retains rotation and IPD, materially reduces
shadow movement while the player body remains stationary.

Redirects if shadows move identically with F4 off. Then isolate head orientation,
light-volume clipping, shadow-map FBO generation, and AFR resource age.

Live result: translational head movement had little effect. Pitch and roll are
the strong correlation, redirecting to vertical projection/reconstruction and
orientation-dependent light-volume behavior.

## S18 - View-depth reflection fade creates the moving opaque boundary

Status: FALSIFIED

Hypothesis: the window/oven sweep is produced by the authored translucent
reflection fade computed from view-space vertex depth, rather than an eye mismatch.

Evidence: live program `988` exposes `avReflectionFadeStartAndLength` and
`avReflectionSizeMul` in `cWaterMaterialArguments`; the shipped shader applies a
linear view-depth fade before reflection/refraction composition.

Confirms if F3 removes or freezes the moving boundary, even if the surface becomes
more uniformly reflective. Redirects to reflection-map bounds, mirrored-frustum
clipping, or refraction alpha if F3 patches the target but the sweep is unchanged.

Live result: F3 patched program `985` for `369` draws and produced no visible
change. Continue with reflection texture bounds, mirrored-frustum/clip ownership,
and material alpha.

## S19 - Remaining vertical projection asymmetry causes pitch/roll artifacts

Status: CONFIRMED

Hypothesis: horizontal centering fixed eye disagreement, but the retained vertical
projection offset `-0.193187` still disagrees with assumptions in deferred
screen-to-view reconstruction. Pitch exposes the vertical error directly; roll
rotates it into diagonal shadow/light and reflection boundaries.

Confirms if fully centering vertical FOV stabilizes dynamic shadows under pitch
and roll and improves the ceiling/window boundaries. Redirects to light-volume
varyings, shadow-map generation, reflection texture bounds, or mirrored clip
planes if the projection logs show `0,0` and artifacts remain unchanged.

Live result: fully centering both projection axes fixed all reported shadow,
ceiling-lighting, window, oven, and reflection artifacts in the tested level.

## S20 - The confirmed pause getter can own controller menu routing

Status: GUARDED BUILD READY (`0.16.0-controller-hands`)

Hypothesis: `SOMA_GetGamePaused` is a sufficiently narrow authority to stop
every gameplay injection route and reinterpret dominant controller aim/trigger
as native menu cursor/click input without changing SOMA's GUI callbacks.

Evidence:

- the wrapper directly returns the game subsystem's paused byte;
- native locomotion already uses it successfully as a fail-closed body-input gate;
- OpenXR exposes head and dominant aim orientations in one reference space;
- SOMA's existing menu remains responsible for hit testing and button actions.

The build confirms if windowed, borderless, and exclusive-fullscreen menus track
the native cursor, all gameplay inputs remain released while paused, and the
trigger-release latch prevents a closing click from leaking into world interaction.

Redirects if SOMA consumes a separate virtual cursor in any window mode. In that
case retain hard pause suppression, hook the native ImGui pointer setter, and
feed it the already-tested normalized `HPLMenuMath` coordinates.

## S21 - Global crosshair callback is the stable semantic interaction boundary

Status: GUARDED BUILD READY (`0.21.0-semantic-reticle`)

Hypothesis: observing `LuxPlayer::_Global_SetCrosshairState` after SOMA's native
pick, `CanInteract`, distance, and icon decisions provides a stable enum that can
drive controller-depth artwork and focus feedback without duplicating gameplay
interaction policy.

Evidence:

- shipped `PlayerState_Normal.hps` routes its resolved icon through
  `Player_SetCrossHairState`;
- shipped `Player.hps` consumes global argument zero as the exact 35-state enum
  and names all 34 non-null TGA assets;
- Ghidra confirms registered dispatch/get/set wrappers at `0x140484ea0`,
  `0x1404851d0`, and `0x140485810`;
- all 34 referenced assets exist as bounded uncompressed 32-bit TGAs in the
  installed game.

The build confirms if logged state names match the flat game's visible icon,
native artwork remains legible and aspect-correct at depth, and semantic focus
pulses occur only on usable state transitions. Redirect if the default state
frequently masks a usable target or callbacks occur before range validation; in
that case add a narrow observer around the script-visible `CanInteract` result
instead of weakening the existing guard.

## S22 - HPL scene depth can be submitted without reconstruction

Status: SCENE DEPTH PROVEN IN BASELINE REPLAY; GUARDED ACQUISITION BUILT IN 0.96.0; LIVE ACCEPTANCE OPEN

Hypothesis: SOMA's captured per-eye depth already follows the finite standard
OpenGL convention required by `XR_KHR_composition_layer_depth`, so a same-format
nearest blit plus HPL near/far in meters is sufficient.

Evidence: `0x140270230` supplies projection, view, far, and near to common setup
at `0x14026fcf0`; the matching HPL2 matrix maps near/far to NDC `-1/+1` and
normalized depth `0/1`. The build fails back to color-only on unsupported depth
formats, invalid clip data, absent caches, or copy failure.

2026-09-07 content-validation caveat: the archived 0.95.8 test contains 188
successful depth-probe rows, all with centre minimum/maximum 1.0000000. The
current probe reads a 4x4 centre patch of default framebuffer zero, not a
full-image scene-depth census. Format/convention and blit success therefore do
not establish that captured depth represents visible geometry. Scene-content
validation remains open; see `PUREDARK_AFW_ASSESSMENT_2026-09-07.md` for evidence
and the controlled next probe. No runtime or configuration change made by this
audit.

2026-09-09 discriminating proof: complete apitrace states show geometry in
HPL's shared D24 scene attachment and matching R16F linear depth. At both
separated scene samples all 2073600 pixels agree within 0.2% after linearization.
The default framebuffer has 2073600/2073600 depth pixels equal to 1.0 in each
paired final-frame sample, including ordinary gameplay at frame 3000. This
confirms a usable raw depth source exists but rejects the current default-FBO
copy as the proven geometry source. R16F is a color attachment encoding z/far,
not directly blit-compatible raw depth. Next gate: exact player viewport,
pre-post source identity and per-eye copy before shared-buffer overwrite.
See `HPL_SCENE_DEPTH_PROOF_2026-09-09.md`; that evidence pass changed no runtime behavior.

Implementation follow-up: 0.96.0 captures a validated renderbuffer at the exact
player pre-post callback, pairs depth/color by render serial and pose/viewport,
and records full PFM/JSON on Ctrl+F10. Production GL copy tests pass in an owned
hidden context, including eye independence and stale-source rejection. The next
decision gate is live callback/source identity and visible-geometry correlation,
not allocation success. Depth submission stays disabled in the test profile.

## S23 - Graphics-binding and view-contract changes can recover transactionally

Status: GUARDED BUILD READY (`0.33.0-depth-resources`)

Hypothesis: HDC/HGLRC identity plus periodic OpenXR view-configuration comparison
is sufficient to detect resource-invalidating display changes. Context changes
use full runtime recovery; recommendation changes rebuild only frame resources.

The build confirms this if fullscreen transitions, HMD sleep/wake, and runtime
resolution changes either remain stable or emit one bounded rebuild/recovery and
resume F10 VR without stale FBOs, swapchains, or a SOMA restart.
