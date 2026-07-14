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

## Native HPL3 Camera Candidates

| Candidate | Confidence | Purpose | Evidence |
| --- | --- | --- | --- |
| `Soma_NoSteam.exe+0x271b80` | Static confirmed, runtime hook pending | Intercept `cCamera::GetFrustum` on every render query | Ghidra behavior matches released HPL2 camera code; selection is restricted to the render-viewport return RVA `+0x298697`. |
| `Soma_NoSteam.exe+0x270230` | Static confirmed, direct call pending | Rebuild perspective frustum after applying HMD view orientation | HPL2 match updates view-projection and all culling derivatives. |
| F10 orientation calibration | Runtime confirmed | Define a neutral pose and toggle the native bridge | Live `0.4.0` applied `1210` renders and restored the base view cleanly. |
| `frustum+0xd8/+0x118/+0x158` | Static confirmed | Projection/view-projection/view matrix packet | Read/write behavior in renderer and setup matches HPL2 layout. |
| F11 alternating-eye bridge | Build ready | First native IPD, position, and asymmetric-FOV stereo proof | Updates one HPL eye per game frame and retains both through GL caches. |
| Per-eye GL cache texture/FBO | Build ready | Decouple AFR game renders from OpenXR swapchain image rotation | Each acquired swapchain image receives the latest cached render for that eye. |

## Native Physics And Input Candidates

| Candidate | Confidence | Purpose | Evidence |
| --- | --- | --- | --- |
| `Soma_NoSteam.exe+0x238750` | Static confirmed, guarded control built | Add dominant-controller translation and rotation to native Grab target errors | `0.18.0` gates exact position `400/0/40` and torque `40/0/0.4|0.1` tuples and preserves native solver ownership. |
| `Soma_NoSteam.exe+0x49c720` | Static confirmed, guarded control patch built | Redirect one native Grab AddImpulse along controller release velocity | Exact virtual thunk uses body vtable `+0x130`; a short controller-armed window prevents unrelated impulses from being changed. |
| OpenXR `XrSpaceVelocity` on grip spaces | Build ready | Direct and optionally scale release impulse | Predicted-time velocity chooses throw direction above a threshold; scale remains bounded `0.5..1.5` around a configurable reference speed. |
| Native Middle/Right Mouse interaction actions | Build ready | Reuse SOMA rotate and throw/cancel state routes | Support squeeze holds InteractRotate; dominant primary invokes native Grab/Push right-click action under manipulation-state gates. |

## Likely Next Runtime Hooks

| Candidate | Confidence | Purpose | Notes |
| --- | --- | --- | --- |
| OpenGL texture/renderbuffer creation calls | Medium | Discover SOMA's scene color/depth targets for blit or eye target replacement | Add only after the OpenXR session probe is understood. |
| `glBindTexture`, `glFramebufferTexture*`, `glBlitFramebuffer` | Medium | Map post-processing and final resolve paths | The initial mirror bridge now uses FBO attachments and `glBlitFramebuffer`; tighter HPL3 pass attribution is still pending. |
| `glUniformMatrix4fv` mutation | Medium | First shader-side stereo proof if native camera hooks take longer | Requires knowing matrix convention and per-eye projection math. |
| Same-frame dual render hook | High value, render split confirmed | Replace AFR temporal mismatch with two eye renders per game frame | Live split is world `0 -> 11`, post `11 -> 0`, GUI on `0`; pre/post-world callbacks still require a once-per-frame boundary. |
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
| `Soma_NoSteam.exe+0x2981e0` / `+0x213970` | Guarded capture built | Final GUI iteration and exact 2D set draw boundary | `0.15.0` captures only GameHudSet into alpha and submits a VIEW-space quad; ImGui/menu/subtitle classification remains. |
| `Soma_NoSteam.exe+0x0cd750` | Control and result bridge built | Dominant-controller native interaction ray and focus result | Replace only start/direction; confirmed entity/body/distance/world hit now drives the `0.20.0` OpenXR depth reticle and focus haptic while native policy and callbacks remain authoritative. |
| `Soma_NoSteam.exe+0x0bcd90` | Guarded control hook built | Shared Lux entity SetMatrix boundary | `0.16.0` replaces only exact normal quarter-scale `PlayerHands_*` roots with fresh grip poses; all authored/full-scale/special/failure states remain native. |
| `Soma_NoSteam.exe+0x00fb60` | Static confirmed, probe built | Inherited Lux entity GetName accessor | Returns native name at entity `+0x120`; signature-guarded exact hand identity avoids broad SetMatrix telemetry. |
| `Soma_NoSteam.exe+0x1297c0` | Static confirmed | Attribute module `OnGui` activity | Useful for inventory, hint, menu, wake, and game-over classification. |
| Player hands `PostUpdate` transform | Guarded controller root built | Replace camera-follow hands with controller pose | Tune configurable root calibration and per-tool profiles from live output while preserving mesh, animations, `R_Hand` attachments, tool callbacks, and camera attachments. |
| `Soma_NoSteam.exe+0x0ccc90` plus HMD/aim pose | Guarded menu control built | Hard pause suppression and native menu pointer | True pause releases every gameplay route; dominant aim projects to SOMA's client cursor and trigger/select remains native left click. |
| OpenXR action set | Design ready | Semantic movement, turn, interaction, and menu input | Snap turn first; body yaw and HMD-local pose remain separate. |
| `XrCompositionLayerQuad` HUD | Gameplay layer built | Head-locked gameplay HUD presentation | Exact GameHudSet capture is built; ImGui/menu/subtitle layers remain future work and terminals stay in the stereo world. |
| `Soma_NoSteam.exe+0x159360` | Guarded comfort control built | Suppress semantic native camera Bob/Shake/Sway | Registered SetCameraPosAdd wrapper; preserve all other authored channels and activate only with VR tracking. |
| `Soma_NoSteam.exe+0x156f00` | Static confirmed, documented | Future authored camera-roll policy | Current/goal roll arrays are mapped; leave unhooked until transition-specific acceptance requires it. |
| Post-effect comfort policy | Source/static confirmed | Disable or attenuate VR-hostile effects | Image trail, chromatic aberration, and radial blur controls are built; semantic Bob/Shake/Sway now have a separate native owner. Lens distortion and DoF remain. |
| `Soma_NoSteam.exe+0x297670` | Runtime confirmed | World/3D overlay pass | Remains on FBO `11` after world render and is effectively free in sampled frames; exact content still needs classification. |
| `Soma_NoSteam.exe+0x289340` | Correction build ready | Commit center-head audio orientation | `0.5.1` confirmed authored vectors ignore HMD motion; `0.5.2` temporarily rotates forward/up during FMOD commit. |
| FMOD `set3DListenerAttributes` import | Static confirmed | Late listener-pose correction fallback | Use center-head pose, never per-eye positions; validate velocity before enabling Doppler. |
| Native pick/grab/rotate states | Source confirmed | Route controller pose into SOMA interaction ownership | Preserve `CanInteract`, distance policy, PID force/torque, collision, and map callbacks. |
| Player/camera state transition probe | High value | Select authored-camera compatibility adapters | Log sit, ladder, animation, conversation, camera parent, and rotation mode. |
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
- Viewport order: `scene render` -> `post composite` -> `PostPostEffect` -> `GUI sets`
- HUD layer: `GameHudSet/cImGui` -> `HUD framebuffer` -> `XrCompositionLayerQuad`
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
