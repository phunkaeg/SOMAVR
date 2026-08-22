# SOMAVR Future Hook Map

Created: 2026-07-09. Status: early reverse-engineering notes. Keep confirmed addresses in `docs\ADDRESS_REGISTRY.md`; keep this file as the evolving hook and graphify seed map.

## Current Proven Hooks

| Candidate | Confidence | Purpose | Evidence |
| --- | --- | --- | --- |
| `gdi32!SwapBuffers` | Runtime confirmed | Per-frame OpenXR pacing and independently throttled telemetry | `0.3.0-xrframe` decouples XR cadence from `FrameSummaryInterval`. |
| `opengl32!wglMakeCurrent` | Runtime confirmed | Capture SOMA's active HDC/HGLRC and GL context identity | Live log reported NVIDIA OpenGL 4.6 context. |
| `opengl32!wglGetProcAddress` | Runtime confirmed | Install hooks for GLEW/extension entry points after SOMA resolves them | Extension hooks captured GLSL uniform calls. |
| `glUniformMatrix4fv` | Runtime confirmed | Observe projection/view matrix uploads | Live run found `a_mtxModelViewProjection`, `a_mtxTemporalProjection`, `a_mtxTemporalView`, `a_mtxInvViewProjection`, and `a_mtxInvView`. |
| `glGetUniformLocation` | Runtime confirmed | Map program/location pairs back to shader uniform names | First run hit the configured name log cap with useful HPL3 names. |
| `glUseProgram` | Runtime confirmed | Attribute matrix uploads to shader programs | Frame summaries report active program `884` during main scene. |
| `glBindFramebuffer` | Runtime confirmed | Track default framebuffer versus offscreen render passes | First run saw repeated framebuffer bind cadence and final framebuffer 0. |
| `glViewport` | Runtime confirmed | Track native render extent and future eye target overrides | First run confirmed `3440x1440`. |
| `glMatrixMode` / `glLoadMatrixf` | Runtime negative for main scene | Detect fixed-function fallback if present | First run showed no valid fixed-function projection for the main scene. |

## OpenXR Bridge Candidates

| Candidate | Confidence | Purpose | Evidence |
| --- | --- | --- | --- |
| `openxr_loader.dll` preload | Runtime confirmed | Avoid injected-DLL delay-load search-path crash | `0.2.2-xrloaderpath` logged `openxr_loader_load ok`. |
| `XR_KHR_opengl_enable` instance extension | Runtime confirmed | Required for OpenXR/OpenGL interop | `0.2.2-xrloaderpath` logged `openxr_extensions ... khrOpenGL=1`. |
| `xrGetOpenGLGraphicsRequirementsKHR` | Runtime confirmed | Validate SOMA's GL version against the runtime | Live `0.2.2` logged `minGL=4.0.0 maxGL=5.0.0` for SOMA's OpenGL 4.6 context. |
| one-shot `xrDestroyInstance` after static probe | Runtime confirmed | Test whether VirtualDesktop crash is live-instance lifetime related | `0.2.3-xroneshot` logged `openxrInstanceAlive=0` and kept rendering afterward. |
| `xrCreateSession` with `XrGraphicsBindingOpenGLWin32KHR` | Frame context confirmed | Prove the runtime accepts SOMA's HDC/HGLRC | `0.2.5-xrframeprobe` succeeded on the frame context `hglrc=0x30000`. |
| live OpenXR session hold | Runtime confirmed | Test VirtualDesktop/SOMA tolerance before allocating swapchains | `0.2.6-xrhold` kept the frame-context session alive through frame `720`, then released cleanly. |
| F8 manual OpenXR start | Runtime confirmed | Launch hooks early but create OpenXR after a save is loaded | Live `0.2.7` triggered at frame `4200`; `0.3.0` fixes polling to every swap. |
| `xrEnumerateViewConfigurationViews` | Runtime confirmed | Capture per-eye recommended swapchain size | Live runtime reported two `2688x2880` views. |
| `xrEnumerateSwapchainFormats` | Runtime confirmed | Pick GL color/depth formats compatible with runtime | Live runtime reported seven formats including `GL_SRGB8_ALPHA8`. |
| `xrPollEvent` | Runtime confirmed | Drive the session lifecycle | Live run observed `IDLE` then `READY`. |
| OpenGL eye swapchains and FBOs | Runtime confirmed | Own runtime-recommended eye images | Live `0.3.0` created two `2688x2880` swapchains with three images each. |
| `xrWaitFrame` / `xrBeginFrame` / `xrEndFrame` | Runtime confirmed | Synchronize SOMA presentation with OpenXR | Live `0.3.0` completed at least `938` consecutive submissions. |
| `xrLocateViews` | Runtime confirmed | Capture per-eye pose and FOV at predicted display time | Live eye positions changed across the successful run. |
| F9 matrix call-stack capture | Runtime confirmed | Turn GLSL camera uploads into stable executable RVAs | Two captures completed; sequence 2 cleanly separated HMD motion from the unchanged SOMA camera. |
| `XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED` | Built in `0.48.0` | Attribute each hand to the runtime-selected controller profile | Queries and resolves both top-level hand profiles; session teardown clears stale identity. |

## Native HPL3 Camera Candidates

| Candidate | Confidence | Purpose | Evidence |
| --- | --- | --- | --- |
| `Soma_NoSteam.exe+0x271b80` | Runtime confirmed, guarded hook active | Intercept `cCamera::GetFrustum` on every render query | Ghidra behavior matches released HPL2 camera code; selection is restricted to the render-viewport return RVA `+0x298697`, and hot packet access is page-validated/SEH-guarded. |
| `Soma_NoSteam.exe+0x270230` | Runtime confirmed, guarded call active | Rebuild perspective frustum after applying HMD view orientation | HPL2 match updates view-projection and all culling derivatives; the active bridge calls it only for validated perspective frusta. |
| F10 orientation calibration | Runtime confirmed | Define a neutral pose and toggle the native bridge | Live `0.4.0` applied `1210` renders and restored the base view cleanly. |
| `frustum+0xd8/+0x118/+0x158` | Static confirmed | Projection/view-projection/view matrix packet | Read/write behavior in renderer and setup matches HPL2 layout. |
| F11 alternating-eye bridge | Build ready | First native IPD, position, and asymmetric-FOV stereo proof | Updates one HPL eye per game frame and retains both through GL caches. |
| Per-eye GL cache texture/FBO | Build ready | Decouple AFR game renders from OpenXR swapchain image rotation | Each acquired swapchain image receives the latest cached render for that eye. |

## apitrace Baseline Capture (2026-08-10)

Vanilla `Soma_NoSteam.exe` traced under apitrace 14.0 (win64, GL) via the apitrace MCP —
`traces\soma-gl-c3c7442f2aed`, 3874 frames, no mod loaded. Independent GL-driver-level confirmation
of the camera candidates above. `find_matrices` + `track_camera`:

| Slot (GL upload site) | Kind | Confirmation |
| --- | --- | --- |
| `glUniformMatrix4fv(program=822, location=1)` | view-projection | **`camera_moves=true`.** Eye position tracked WASD and `view_z_axis` tracked mouse-look across the capture (static → walk → stop → walk) — the real view matrix, i.e. the `a_mtxModelViewProjection`-class upload. Driver-level analogue of the native `frustum+0x118` (view-projection) packet. |
| `glUniformMatrix4fv(program=884, location=1)` | projection | Decoded confidence 1.0: **FOV_y 70.0°, FOV_x 102.4°, aspect 16:9, near 0.03, far 998.67, GL right-handed, normal-Z.** Matches `game.cfg` (`FOV=70`, `NearClipPlane=0.03`, `FarClipPlane~1000`) and the main-scene program `884` already logged above. Analogue of native `frustum+0xd8`. |

GL-layer VR-patch targets: rewrite the **view-projection** uniform (`program 822, loc 1`, and the
equivalent upload in every scene program) per eye with the IPD-offset view; adjust the **projection**
uniform (`program 884, loc 1`) for per-eye asymmetric FOV. These are the API-level counterparts of the
native `+0x271b80` / `+0x270230` / `frustum+*` candidates — use those RVAs to bind the patch in code.
Program object names are trace-local and must not become runtime signatures;
the decoded matrix role and validated native anchors are the authority.

### Native-Stereo Resource Confirmation (2026-08-23)

The baseline trace also proves two renderer-owned resources that must remain
eye coherent during any second world render:

| GL surface | Baseline evidence | 0.92 observation surface |
| --- | --- | --- |
| Occlusion queries | IDs `1..4` are polled for availability/result, then immediately reused by the next frame; `14,828` begin/end pairs total | `glBeginQuery`, `glEndQuery`, and query-result hooks tag first/replay passes and report same-frame ID reuse without altering results |
| Refraction scene color | `6,684` framebuffer copies; call `964252` copies a `942x888` object clip rectangle into texture 35 just before the refractive translucent draw | `glCopyTexSubImage2D` records destination texture, rectangle, pass, and cross-eye reuse; table capped at 4096 textures |
| Translucent camera packet | Refraction shader 368 samples `aRefractionMap`/`aSceneDepth`; shaders 368 and 578 consume view/projection/inverse matrices through `cTranslucentTypeArguments` | Native frustum and UBO ownership are required; trace-local program IDs are diagnostic labels only |

These are confirmation hooks, not a new stereo implementation. The current
continuous replay can now supply the evidence; any future world-only render
path must keep each eye's query, copied scene color, translucent draws, temporal
packet, and cache fill in one sequential transaction.

## Native Physics And Input Candidates

| Candidate | Confidence | Purpose | Evidence |
| --- | --- | --- | --- |
| `Soma_NoSteam.exe+0x238750` | Static confirmed, guarded control built | Add dominant-controller translation and rotation to native Grab target errors | `0.18.0` gates exact position `400/0/40` and torque `40/0/0.4|0.1` tuples and preserves native solver ownership. |
| `Soma_NoSteam.exe+0x49c720` | Static confirmed, guarded control patch built | Redirect one native Grab AddImpulse along controller release velocity | Exact virtual thunk uses body vtable `+0x130`; a short controller-armed window prevents unrelated impulses from being changed. |
| OpenXR `XrSpaceVelocity` on grip spaces | Build ready | Direct and optionally scale release impulse | Predicted-time velocity chooses throw direction above a threshold; scale preserves at least native impulse, is bounded `1.0..2.0`, and retains forward clearance from the player. |
| Native Middle/Right Mouse interaction actions | Build ready | Reuse SOMA rotate and throw/cancel state routes | Support squeeze holds InteractRotate; dominant primary invokes native Grab/Push right-click action under manipulation-state gates. |

## Likely Next Runtime Hooks

| Candidate | Confidence | Purpose | Notes |
| --- | --- | --- | --- |
| OpenGL texture/renderbuffer creation calls | Medium | Discover SOMA's scene color/depth targets for blit or eye target replacement | Add only after the OpenXR session probe is understood. |
| `glBindTexture`, `glFramebufferTexture*`, `glBlitFramebuffer` | Medium | Map post-processing and final resolve paths | The initial mirror bridge now uses FBO attachments and `glBlitFramebuffer`; tighter HPL3 pass attribution is still pending. |
| `glUniformMatrix4fv` mutation | Medium | First shader-side stereo proof if native camera hooks take longer | Requires knowing matrix convention and per-eye projection math. |
| Same-frame dual render hook | Built opt-in in `0.45.0`, all-stereo temporal bank in `0.47.0` | Replace AFR temporal mismatch with two eye renders per game frame | Replays only exact player `HPL3_Scene_RenderViewport`, removes screen-GUI bit `2`, keeps the upper enumerator/lifecycle once, and fails closed to AFR. The confirmed `m_mtxPrevView`-equivalent packet is banked per eye in AFR and continuous stereo with discontinuity reseeds; other post histories still need classification. |
| SDL swap/window paths | Low-medium | Fallback frame boundary/window sizing path | Only needed if GDI swap timing proves insufficient. |

## Gameplay And Presentation Candidates

Detailed evidence and staged implementation notes are in `docs\FUTURE_SYSTEMS_RE.md`
and `docs\VR_COMPATIBILITY_RE.md`. Stable graph nodes and acceptance gates are in
`docs\FEATURE_TRACEABILITY.md`.

| Candidate | Confidence | Purpose | Notes |
| --- | --- | --- | --- |
| `Soma_NoSteam.exe+0x2375f0` | Guarded control built | Native analog `iCharacterBody::Move` in unpaused Normal/Normal ownership | Forward `0` and Right `1`; special states automatically retain semantic key input. |
| `Soma_NoSteam.exe+0x237460` | Guarded control built | Exact-radian body `AddYaw` for snap/smooth turn | Normal-state only; semantic mouse path remains the special-state/signature fallback. |
| `Soma_NoSteam.exe+0x0ccc90` | Guarded policy built | Prevent direct body input while SOMA is paused or in menu ownership | Registered getter reads game subsystem paused byte `+0x2d4`. |
| `Soma_NoSteam.exe+0x155290` | Static confirmed | Resolve the active player's character body | Registered by the `cLuxPlayer` AngelScript API at `+0x15ca10`. |
| `Soma_NoSteam.exe+0x2328f0` | Static confirmed | Attribute script lifecycle callbacks | Callback id `2` queues `OnDraw`; id `3` is `OnPostRender`. |
| `Soma_NoSteam.exe+0x298850` | Static confirmed | Whole-frame viewport render boundary | Main loop calls this between script `OnDraw` and `OnPostRender`. |
| `Soma_NoSteam.exe+0x33bd80` | Runtime confirmed | Per-eye post-effect composite | F11 gameplay averaged about `125 us`; F12 bypass in `0.5.2` distinguishes post-chain defects from world shaders. |
| `Soma_NoSteam.exe+0x2981e0` / `+0x213970` | Guarded capture built | Final GUI iteration and exact 2D set draw boundary | GameHudSet, exact GameHudImGui, and pause-gated exact current ImGui accumulate into one alpha target and VIEW-space quad; non-paused/diegetic owners remain native. |
| `Soma_NoSteam.exe+0x1c8dd0` | Guarded control built | Exact localized voice-subtitle draw worker | `0.34.0` temporarily scales `cLuxVoiceHandler` width/Y/font/shadow layout during active stereo, calls the original worker, and restores native values immediately. |
| `Soma_NoSteam.exe+0x1d3ba0` | Confirmed constructor/config map | `cLuxVoiceHandler` active and normal/large subtitle layout ownership | Shipped Voice config proves width `860`, Y `700/690`, font `26/32`, shadow `1`; content/timing/font resource remain native. |
| `Soma_NoSteam.exe+0x0cd750` / `+0x1438c0` | Control and result bridge built | Dual-controller native interaction ray and single finalized focus result | Probe both tracked controller poses through the inner raycast, choose one owner, and finalize once; confirmed distance/body/entity/world hit drives the OpenXR semantic reticle while native policy and callbacks remain authoritative. |
| `Soma_NoSteam.exe+0x109b30` | Guarded control hook built | SOMA-authored global gameplay rumble | Preserve the original gamepad call and mirror strength/duration bilaterally through bounded OpenXR segments; covers scripted damage/tool/action effects without claiming raw physics contact. |
| `Soma_NoSteam.exe+0x0cd710` | World control call built | Registered `CheckLineOfSight` wrapper | `0.24.0` directly calls the guarded wrapper; `0.25.0` adds center/radial/top/bottom samples and `0.27.0` can include dynamic bodies. No detour or script ownership change. |
| `Soma_NoSteam.exe+0x143650` | Static confirmed | Underlying physics/world line query | Active-world resolution and physics callback are documented; sampled volume remains short of a native shape cast or capsule reconciliation. |
| `Soma_NoSteam.exe+0x0cd7d0` | Guarded control hook built | Registered global closest-body ray | `0.27.0` redirects only camera-origin `5..20`-unit calls to the cached visual flashlight origin/basis while preserving SOMA's randomized cone, length, outputs, and body policy. |
| `Soma_NoSteam.exe+0x143a10` | Static confirmed | Underlying closest-body physics ray | Forms the endpoint and invokes the active physics-world callback; tool `3` and camera-grounding `100` callers remain native. |
| `Soma_NoSteam.exe+0x0bcd90` | Guarded control hook built | Shared Lux entity SetMatrix boundary | `0.16.0` handles exact normal quarter-scale `PlayerHands_*` grip roots; `0.23.0` handles only exact `Flashlight` dominant-aim matrices. All authored/stale/special/failure states remain native. |
| `Soma_NoSteam.exe+0x00fb60` | Static confirmed, identity gate built | Inherited Lux entity GetName accessor | Returns native name at entity `+0x120`; exact `PlayerHands_*` and `Flashlight` gates avoid broad SetMatrix mutation. |
| `Soma_NoSteam.exe+0x0b3700` | Guarded control hook built | Native Lux entity `SetActive(bool)` | 0.77 suppresses player-hands deactivation only in tracked Normal/Normal ownership after exact identity resolution. |
| `Soma_NoSteam.exe+0x2cb5a0` | Guarded control hook built | Native entity/mesh `SetVisible(bool)` | 0.77 retains only the cached exact player-hands mesh; authored/non-Normal states pass through. |
| `Soma_NoSteam.exe+0x1297c0` | Static confirmed | Attribute module `OnGui` dispatch | Module `mlId` is `+0x158` (GameOver `10`, Wake `12`, Credits `19`), but callback presence is not active-state proof because shipped handlers can return immediately. |
| `Soma_NoSteam.exe+0x1378e0` | Guarded observer hook built | Exact user-module action activity | Preserves native dispatch; module `15`, action `12`, pressed edges authorize the shipped inventory hold/fade current-ImGui capture window. Registration at `+0x1ae870` proves `mlId +0x158`. |
| `Soma_NoSteam.exe+0x484ea0/+0x485200/+0x485720` | Guarded scripted-presentation observer built | Exact wake sleep/start activity and typed arguments | Sleep arbitrates XR blackout; authored wake duration authorizes flat current-ImGui capture. Native dispatch remains authoritative. |
| Player hands `PostUpdate` transform | Guarded controller root built | Replace camera-follow hands with controller pose | Tune configurable root calibration and per-tool profiles from live output while preserving mesh, animations, `R_Hand` attachments, tool callbacks, and camera attachments. |
| `Soma_NoSteam.exe+0x165270/+0x200980` | Passive skeleton probe built | Resolve the `PlayerHands_*` mesh and bilateral wrist/socket bones | `0.69.0` logs world transforms and pre/post flags without mutation. Stable evidence unlocks post-animation wrist tracking. |
| `Soma_NoSteam.exe+0x31c90/+0x4a9490/+0x4a94a0` | Static confirmed, control deferred | Enable and set per-bone post-animation correction | Begin with Normal state and exact wrists. Ladder/climb and camera-attached sequences stay native until separately accepted. |
| Character-body camera `+0x1b0` plus states `0..20` | Passive classifier built | Distinguish structural detachment from semantic authored states | State adapters must preserve native body progress/endpoints while composing HMD freedom at render time. |
| `Player.hps::UpdateFlashLightLOS` camera-ray sample | Guarded control built | Align randomized agent-gobo gameplay rays with the moved controller flashlight | The three low-frequency rays now share the visual light origin and controller-relative randomized cone; all general frustum/sensor behavior remains shipped script logic. |
| `Soma_NoSteam.exe+0x0ccc90` plus HMD/aim pose | Guarded menu control built | Hard pause suppression and native menu pointer | True pause releases every gameplay route; dominant aim projects to SOMA's client cursor and trigger/select remains native left click. |
| OpenXR action set | Design ready | Semantic movement, turn, interaction, and menu input | Snap turn first; body yaw and HMD-local pose remain separate. |
| `XrCompositionLayerQuad` HUD | Flat HUD routes built | Head-locked gameplay and confirmed flat-ImGui presentation | Exact GameHudSet/GameHudImGui plus pause, wake/dead, and inventory current-ImGui owners are captured; terminals stay in the stereo world. |
| `Soma_NoSteam.exe+0x159360` | Guarded comfort control built | Suppress semantic native camera Bob/Shake/Sway | Registered SetCameraPosAdd wrapper; preserve all other authored channels and activate only with VR tracking. |
| `Soma_NoSteam.exe+0x156d90/+0x156f00` | Guarded comfort control built | Semantic authored camera-roll policy | Fade and direct setters independently suppress Script/Lean/Move/Climb roll; active profile preserves Script and zeros Lean/Move/Climb only during VR. |
| `Soma_NoSteam.exe+0x071f80` | Guarded reversible patch built | Suppress world depth of field during VR | Exact seven-byte setter plus nine padding bytes; emulates the native `world+0x264` write and rejects only active requests while tracking. |
| `Soma_NoSteam.exe+0x155210/+0x155230/+0x155250` | Guarded reversible patches built | Neutralize authored FOV, FOV multiplier, and aspect multiplier during VR | Exact leaf wrappers preserve native speed and inactive-VR behavior; FOV resolves to player default and multipliers to `1.0`. |
| `Soma_NoSteam.exe+0x0ccdb0` | Read-only control built | Detect exact SOMA loading visibility | Drives opaque-black XR projection, cache invalidation, controller release, and bounded exit guard. |
| `Soma_NoSteam.exe+0x488fa0/+0x488fd0` | Guarded probe built | Classify video stream names and lifetime | Probe-only until live evidence distinguishes fullscreen transition video from diegetic screens. |
| Post-effect comfort policy | Source/static confirmed, control built | Disable or attenuate VR-hostile effects | ImageTrail, VideoDistortion, ChromaticAberration, and RadialBlur controls are built; fades and tone mapping remain native. |
| `Soma_NoSteam.exe+0x297670` | Runtime confirmed | World/3D overlay pass | Remains on FBO `11` after world render and is effectively free in sampled frames; exact content still needs classification. |
| `Soma_NoSteam.exe+0x289340` | Correction build ready | Commit center-head audio orientation | `0.5.1` confirmed authored vectors ignore HMD motion; `0.5.2` temporarily rotates forward/up during FMOD commit. |
| FMOD `set3DListenerAttributes` import | Static confirmed | Late listener-pose correction fallback | Use center-head pose, never per-eye positions; validate velocity before enabling Doppler. |
| Native pick/grab/rotate states | Source confirmed | Route controller pose into SOMA interaction ownership | Preserve `CanInteract`, distance policy, PID force/torque, collision, and map callbacks. |
| Player/camera state transition policy | Bounded guard built | Select authored-camera compatibility adapters | Exact IDs are logged; ladder, climb, animation, sit, and death entry/exit can request short compositor-black frames while native state/camera ownership remains. |
| Load/video presentation classifier | Source/static confirmed | Keep OpenXR alive and choose world versus quad-layer presentation | Load UI is ImGui; binary exposes Theora streams; gameplay screens are commonly diegetic GUI. |

## Graphify Seed

Initial node groups for a future graph view:

- Process: `Soma_NoSteam.exe`
- Injection: `somavr_injector.exe` -> `somavr.dll`
- GL context: `wglMakeCurrent` -> `HDC/HGLRC` -> `gl_context_info`
- Frame boundary: `SwapBuffers` -> `frame_summary`
- Shader matrices: `glGetUniformLocation` -> `glUniformMatrix4fv` -> `a_mtxModelViewProjection`
- Native camera: `cCamera::GetFrustum` -> `HPLCameraBridge` -> `cFrustum::SetupPerspectiveProj` -> `viewProjection/planes/BV`
- OpenXR runtime: `openxr_extensions` -> `xrCreateInstance` -> `xrGetSystem` -> `xrGetOpenGLGraphicsRequirementsKHR`
- OpenXR session: `XrGraphicsBindingOpenGLWin32KHR` -> `xrCreateSession` -> `xrEnumerateSwapchainFormats`
- OpenXR frame state: `XR_SESSION_STATE_READY` -> `xrBeginSession` -> `xrWaitFrame` -> `xrBeginFrame` -> `xrLocateViews` -> `xrEndFrame`
- OpenGL submission: `OpenXRGLBridge` -> `XrSwapchainImageOpenGLKHR` -> `GL framebuffer` -> `glBlitFramebuffer`
- Eye composition: `left/right XrView` -> `XrCompositionLayerProjectionView` -> `XrCompositionLayerProjection`
- Input bridge: `OpenXR actions` -> `SOMA action dispatch` -> `player/move state` -> `iCharacterBody`
- Pose ownership: `body yaw` + `authored camera base` + `HMD local pose` -> `cCamera::GetFrustum`
- Viewmodel: `PlayerHandsHandler` -> `hands entity` -> `R_Hand socket` -> `tool HudObject`
- Viewport order: `scene render` -> `post composite` -> stateful `PostPostEffects` phase -> `GUI sets`
- HUD layer: `GameHudSet/GameHudImGui` + exact flat current-ImGui authorities -> `HUD framebuffer` -> `XrCompositionLayerQuad`
- Interaction: `controller ray` -> `native pick` -> `crosshair semantic state` -> `native callback`

Useful edge labels:

- `resolved_by`
- `uploads`
- `owns_context`
- `validates`
- `requires`
- `feeds_next_build`
- `acquires_image`
- `submits_layer`
## 0.75 Terminal And Wrist Evidence

- Exact terminal-FBO, render-thread color-clear suppression preserves SOMA's
  dirty-rectangle GUI contract; depth/stencil and unrelated clears pass through.
- Wrist post-transform composition is now implemented as a tested dry run using
  local `+0x44`, world `+0x84`, parent `+0x180`, and post-left-multiply order.
  Live hierarchy/calibration evidence remains the gate for mutation.
