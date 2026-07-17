# Feature Traceability Registry

This is the canonical, graph-friendly index for SOMAVR features. Stable
`FEATURE.*` IDs should appear in future design notes, commits, tests, and bounded
telemetry where useful. Graphify can then connect implementation, native anchors,
evidence, and acceptance gates without relying on filenames alone.

Status values: `PROVEN`, `EXPERIMENTAL`, `BUILT`, `DESIGNED`, `RE_REQUIRED`, `BLOCKED`.

## Registry

| Feature ID | Status | Code owner | Native/runtime anchors | Primary documentation | Next acceptance gate |
| --- | --- | --- | --- | --- | --- |
| `FEATURE.INJECTION` | PROVEN | `src/injector/main.cpp`, `src/dll/DllMain.cpp` | Remote `LoadLibraryW`, early DLL initialization | `CURRENT_STATE.md` | Launch and attach modes remain reliable across load/save cycles |
| `FEATURE.XR_BOOTSTRAP` | EXPERIMENTAL | `OpenXRRuntime`, `OpenXRHelpers` | OpenXR loader, instance, system, session state, delayed recovery | `CURRENT_STATE.md`, `UEVR_LEARNINGS.md` | Live-test session and instance loss recovery without restarting SOMA |
| `FEATURE.XR_GL_SUBMISSION` | PROVEN | `OpenXRRuntime`, `OpenXRGLBridge` | OpenGL swapchains, FBOs, `xrEndFrame` | `CURRENT_STATE.md` | Validate format and color-space behavior across the hardware matrix |
| `FEATURE.XR_RESOURCE_RECREATION` | BUILT | `OpenXRRuntime`, `OpenXRGLBridge` | bound HDC/HGLRC identity, periodic `xrEnumerateViewConfigurationViews`, transactional frame-resource rebuild | `BUILD_HISTORY.md`, `VR_COMPATIBILITY_RE.md`, `TEST_CHECKLISTS.md` | Live-test GL context replacement and runtime view-size/sample changes without stale resources or a process restart |
| `FEATURE.CLEAN_SHUTDOWN` | PROVEN | `HPLLifecycle`, `OpenXRRuntime` | `0x1403b16e0`, `0x1403b1803` | `RUNTIME_ANALYSIS_0.5.6.md`, `GHIDRA_SYNC.md` | Preserve clean exit across runtime/session-loss paths |
| `FEATURE.VR_MODE_CONTROL` | EXPERIMENTAL | `HPLCameraBridge`, `OpenXRRuntime` | F10 pending activation, F8/F11 diagnostics | `CURRENT_STATE.md`, `TEST_CHECKLISTS.md` | One F10 reaches tracking, stereo, and full centering from a loaded save |
| `FEATURE.VR_CONTROL_PANEL` | BUILT | `HPLStatusPanelBridge`, `OpenXRStatusPanelMath`, `OpenXRRuntime`, `OpenXRGLBridge` | F1, Menu+Secondary, VIEW-space alpha quad, guarded camera/runtime setters, same-frame stereo, HUD-shape, and comfort-vignette actions | `BUILD_HISTORY.md`, `ARCHITECTURE.md`, `TEST_CHECKLISTS.md` | Live-test layer orientation/alpha, exclusive input, all nine reversible actions including HUD shape and comfort vignette, and no stereo/world regression |
| `FEATURE.RECENTER` | BUILT | `HPLCameraBridge`, `HPLCameraMath`, `HPLInputBridge` | F2 or two-grip hold, stable neutral-pose latch | `BUILD_HISTORY.md`, `CURRENT_STATE.md`, `TEST_CHECKLISTS.md` | Live test confirms keyboard and controller recenter without stereo/session reset or height drift |
| `FEATURE.XR_INPUT` | BUILT | `OpenXRInput`, `OpenXRRuntime` | OpenXR action set, Simple/Touch/Index/Motion bindings, grip/aim action spaces | `BUILD_HISTORY.md`, `CURRENT_STATE.md`, `TEST_CHECKLISTS.md` | Live log confirms active bindings, both tracked controllers, and stable predicted poses |
| `FEATURE.XR_REFERENCE_SPACE` | BUILT | `OpenXRRuntime`, config | `XR_REFERENCE_SPACE_TYPE_LOCAL`, optional `STAGE`, runtime fallback | `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Compare seated/local and standing/stage calibration, eye height, recenter, and map transitions |
| `FEATURE.ROOMSCALE_SAFETY` | BUILT | `HPLCameraBridge`, `HPLCameraMath`, config | `0x1400cd710`, `0x140143650`, optional dynamic-inclusive center/radial/vertical sweep, shared physical-head decomposition | `FUTURE_SYSTEMS_RE.md`, `ADDRESS_REGISTRY.md`, `TEST_CHECKLISTS.md` | Live-test static walls plus moving doors/props and skipped authored-start probes; then recover shape cast or native capsule reconciliation |
| `FEATURE.ROOMSCALE_BODY_RECONCILIATION` | BUILT | `HPLNativeLocomotion`, `HPLCameraBridge`, `HPLRoomscaleReconciliationMath`, config | `0x140237920`, `0x140237970`, body size `+0x134`, sustained-displacement hysteresis, sampled capsule sweep, neutral-pose compensation | `FUTURE_SYSTEMS_RE.md`, `ADDRESS_REGISTRY.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Live-test capsule catch-up near walls, doors, stairs, crouch transitions, authored states, and repeated direction changes; retain config rollback until accepted |
| `FEATURE.TRACKING_RESILIENCE` | BUILT | `OpenXRRuntime`, `HPLCameraBridge` | pose-age bound, last-valid eye cache, zero-layer loss path, recovery blackout | `BUILD_HISTORY.md`, `CURRENT_STATE.md`, `TEST_CHECKLISTS.md` | Live-test brief and extended HMD tracking loss without stale-eye corruption, stereo teardown, or a visible recovery flash |
| `FEATURE.CONTROLLER_HAPTICS` | BUILT | `OpenXRInput`, `OpenXRRuntime`, `HPLInputBridge`, `HPLInteractionBridge`, `HPLGameplayHapticsBridge`, `HPLGameplayHapticsMath` | vibration output action, per-hand output paths, `0x140109b30` authored rumble, focused-session guard, segmented refresh/stop envelope | `FUTURE_SYSTEMS_RE.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Live-test damage, death, datamining, locked interaction, sustained effects, and physical-gamepad coexistence; native Grab impacts are owned separately by `FEATURE.GRAB_CONTACT_HAPTICS` |
| `FEATURE.GRAB_CONTACT_HAPTICS` | BUILT | `HPLContactHapticsBridge`, `HPLContactHapticsMath`, `HPLCameraBridge`, `OpenXRRuntime` | `0x14032f0e0` native surface impact, Grab state `1`, native normal speed/contact position/count/body, fresh dominant-grip world pose | `CONTACT_HAPTICS_RE.md`, `ADDRESS_REGISTRY.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Live-test weak/strong impacts, duplicate-material cooldown, hand ownership, false positives, native sound/physics preservation, and rollback; use `0x14032f380` slide telemetry only if sustained scrape feedback is justified |
| `FEATURE.CONTROLLER_ACCESSIBILITY` | BUILT | `OpenXRInput`, `HPLInputBridge`, config | per-hand primary/secondary actions, dominant-hand roles, stick swap, one-hand fallback, support-hand flashlight/inventory, five standard suggested profiles | `BUILD_HISTORY.md`, `FUTURE_SYSTEMS_RE.md`, `TEST_CHECKLISTS.md` | Live-test role-aware actions, swapped-stick, and each one-controller path on Touch/Index/WMR/Vive; Simple remains intentionally pose/select/menu limited |
| `FEATURE.XR_INTERACTION_PROFILE_DIAGNOSTICS` | BUILT | `OpenXRInput`, `OpenXRRuntime` | `XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED`, per-hand `xrGetCurrentInteractionProfile`, path resolution, session reset and summary | `BUILD_HISTORY.md`, `FUTURE_SYSTEMS_RE.md`, `TEST_CHECKLISTS.md` | Capture correct left/right profiles across Touch, Index, WMR, Vive, reconnect, one-hand loss, and runtime recovery |
| `FEATURE.MENU_POINTER` | BUILT | `HPLMenuBridge`, `HPLMenuMath`, `HPLInputBridge`, `HPLNativeLocomotion` | `0x1400ccc90`, HMD/aim orientations, native SOMA client cursor and left-click path | `BUILD_HISTORY.md`, `FUTURE_SYSTEMS_RE.md`, `TEST_CHECKLISTS.md` | Live-test window modes, native cursor mapping, click-release latch, and non-pause ImGui surfaces; then couple pointer coordinates to future menu-layer presentation |
| `FEATURE.PAUSED_MENU_LAYER` | BUILT | `HPLHudBridge`, `HPLNativeLocomotion`, `OpenXRGLBridge`, `OpenXRRuntime` | `0x1400cca70`, `0x140071f20`, `0x1400ccc90`, exact current ImGui set plus confirmed pause ownership | `BUILD_HISTORY.md`, `FUTURE_SYSTEMS_RE.md`, `TEST_CHECKLISTS.md` | Live-test pause alpha/order, cursor alignment, resume behavior, window modes, and strict exclusion of non-paused/diegetic current ImGui |
| `FEATURE.SCRIPTED_PRESENTATION` | BUILT | `HPLCrosshairBridge`, `HPLPresentationBridge`, `HPLHudBridge`, `HPLInputBridge` | `0x140484ea0`, `0x140485200`, `0x140485720`, dead state `17`, module IDs `10/12/19`, exact current/GameHud ImGui | `ADDRESS_REGISTRY.md`, `FUTURE_SYSTEMS_RE.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Live-test sleep blackout, wake eyelid timing/alpha, game-over text/continue, credits, loading overlap, and config rollback |
| `FEATURE.INVENTORY_PRESENTATION` | BUILT | `HPLUserModuleBridge`, `HPLPresentationBridge`, `HPLHudBridge` | `0x1401378e0`, `0x140129a40`, `0x1401ae870`, `mlId +0x158`, module `15`, action `12`, bounded current-ImGui capture | `ADDRESS_REGISTRY.md`, `FUTURE_SYSTEMS_RE.md`, `GHIDRA_SYNC.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Live-test inventory hold/fade, alpha/order, controller open/close, exact five-second expiry, non-inventory exclusion, and config rollback |
| `FEATURE.DIEGETIC_GUI_POINTER` | BUILT | `HPLTerminalBridge`, `HPLComfortBridge`, `HPLInputBridge`, `HPLPlayerState` | wall/handheld terminal states `8/9`, `0x1403132d0`, direct original `0x1402f0c90` dispatch, `0x140237920`, `0x1401562e0`, `0x140159360`, focused wrapper/entity `+0x180/+0x28` | `ADDRESS_REGISTRY.md`, `FUTURE_SYSTEMS_RE.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Live-test state 8 without takeover, physical lean, continuously moving mesh-ray cursor, miss behavior/click/exit, state 9 preservation, and all three rollback levels |
| `FEATURE.HEAD_TRACKING` | PROVEN | `HPLCameraBridge`, `HPLCameraMath` | `0x140271b80`, `0x140270230` | `CURRENT_STATE.md`, `VR_COMPATIBILITY_RE.md` | Remain correct through every authored camera state |
| `FEATURE.AFR_STEREO` | PROVEN | `HPLCameraBridge`, `OpenXRRuntime`, `OpenXRGLBridge` | F11, per-eye cache and submitted render pose | `BUILD_HISTORY.md`, `RUNTIME_ANALYSIS_0.5.1.md` | Preserve stability while shader/temporal compatibility is classified |
| `FEATURE.DUAL_RENDER` | BUILT | `HPLDualRenderControl`, `HPLCompatibilityProbe`, `HPLDualRenderMath`, `HPLDualRenderDiagnostics`, `HPLStatusPanelBridge`, `OpenXRRuntime` | `0x140298850`, `0x140298630`, immediate first-eye cache, opt-in continuous exact-player replay with GUI bit removed, bounded diagnostic precedence, fail-closed AFR rollback | `VR_COMPATIBILITY_RE.md`, `ADDRESS_REGISTRY.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Live-test frame pacing, post-post temporal behavior, shadows/reflections, HUD/presentation ownership, and panel on/off rollback before default promotion |
| `FEATURE.DUAL_RENDER_TEMPORAL_PROBE` | BUILT | `HPLDualRenderDiagnostics`, `HPLTemporalMutationMath`, `HPLCompatibilityProbe` | `0x1401f1480`, renderer/current/history/settings snapshots, `*(renderer+0x20)+0x158 -> *(renderer+0x438)+0x80` | `ADDRESS_REGISTRY.md`, `VR_COMPATIBILITY_RE.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Three automatic samples produce readable first/replay region summaries and correlated mutation ranges without crashes or visible state regression |
| `FEATURE.PER_EYE_VIEW_HISTORY` | BUILT | `HPLPerEyeViewHistory`, `HPLPerEyeViewHistoryMath`, `HPLCameraBridge`, `HPLCompatibilityProbe` | `0x1401f1480`, active frustum view `+0x158`, renderer history pointer `+0x438`, previous view `+0x80`, AFR/continuous pending/actual eye-pose transaction, calibration and stale-gap reseed | `ADDRESS_REGISTRY.md`, `VR_COMPATIBILITY_RE.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Live-test AFR and same-frame history status, authored-camera/recenter/loading/tracking reseed, alternating restore/capture, temporal image stability, performance, and hard rollback; shader/resource audit confirms no separate previous-projection or velocity history |
| `FEATURE.PER_EYE_IMAGE_TRAIL` | BUILT | `HPLPerEyePostEffect`, `HPLPerEyePostEffectMath`, `HPLCompatibilityProbe` | `0x1402d7a40`, `0x14038a8b0`, `0x14038a950`, `0x14038ae60`, ImageTrail `+0x50/+0x58/+0x98/+0xa0`, eye/pose/calibration bank, signature and lifecycle fallback | `ADDRESS_REGISTRY.md`, `FUTURE_SYSTEMS_RE.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Live-trigger ImageTrail; prove distinct left/right native resources, reset behavior, stereo-consistent trail, clean two-pair teardown, and one-line suppression rollback |
| `FEATURE.TONE_MAPPING_FRAME_OWNER` | BUILT | `HPLToneMappingFrame`, `HPLToneMappingFrameMath`, `HPLCompatibilityProbe` | `0x1402842d0`, `0x1402845d0`, `0x140284fd0`, ToneMapping `+0x8c/+0x94/+0xa0/+0xd8..+0x120/+0x138..+0x158`, same-pose baseline replay and single committed update | `ADDRESS_REGISTRY.md`, `FUTURE_SYSTEMS_RE.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Live-test dark/bright transitions, scripted fades, grading and film-grain changes, bloom, alternating eye order, AFR, same-frame stereo, mismatch telemetry, and config rollback |
| `FEATURE.PER_EYE_SSAO_HISTORY` | BUILT | `HPLSSAOTemporalHistory`, `HPLSSAOTemporalMath`, `HPLCompatibilityProbe`, `OpenGLHooks` | `0x1403f2b50`, `0x1402aba30`, renderer `+0xe78/+0xed8/+0xf40`, two GL history copies, eye/pose/calibration ownership | `ADDRESS_REGISTRY.md`, `FUTURE_SYSTEMS_RE.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Live-prove distinct histories, stable contact AO in both stereo modes, reset/reseed behavior, bounded allocations, clean shutdown, and config rollback |
| `FEATURE.SSAO_FRAME_OWNER` | BUILT | `HPLSSAOFrameOwner`, `HPLToneMappingFrameMath`, `HPLCompatibilityProbe` | `0x1403f2b50`, global phase `0x14079575c`, same-pose baseline replay and single committed update | `ADDRESS_REGISTRY.md`, `FUTURE_SYSTEMS_RE.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Live-test matching AO jitter in both eyes, AFR neutrality, same-frame replay/restore counters, mismatch telemetry, and independent config rollback |
| `FEATURE.VIEWPORT_OWNERSHIP` | BUILT | `HPLCompatibilityProbe`, `HPLCameraBridge`, `HPLPlayerState` | `0x140298630`, `0x140297f20`, `0x140297500`, viewport camera/world/renderer/post/FBO/settings fields | `ADDRESS_REGISTRY.md`, `VR_COMPATIBILITY_RE.md`, `TEST_CHECKLISTS.md` | Live logs classify reflection, terminal, loading, and save-transition viewports while only the exact player camera receives VR controls |
| `FEATURE.PER_EYE_CPU_TELEMETRY` | BUILT | `HPLCompatibilityProbe`, `HPLCameraBridge` | six guarded render-stage hooks, active AFR eye identity, QPC totals | `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md`, `VR_COMPATIBILITY_RE.md` | Capture representative left/right stage budgets alongside GPU timestamps |
| `FEATURE.PER_EYE_GPU_TELEMETRY` | BUILT | `HPLCompatibilityProbe`, `HPLCameraBridge` | six guarded render stages, nested `GL_TIMESTAMP` query pairs, bounded nonblocking pool | `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md`, `VR_COMPATIBILITY_RE.md` | Live capture shows nonzero left/right timings with no sustained dropped or invalid samples |
| `FEATURE.XR_DEPTH_CAPABILITY` | BUILT | `OpenXRRuntime`, `OpenXRGLBridge`, `OpenXRDepthMath`, `HPLCameraBridge` | `XR_KHR_composition_layer_depth`, D24/D32F/depth-stencil negotiation, same-format per-eye caches and swapchains, standard OpenGL depth range, HPL near/far divided by world units per meter | `FUTURE_SYSTEMS_RE.md`, `VR_COMPATIBILITY_RE.md`, `TEST_CHECKLISTS.md` | Live log confirms two depth swapchains, per-eye copies, and submitted depth frames without visual or runtime regression |
| `FEATURE.FIXED_FOVEATION` | BUILT | `OpenXRRuntime`, `OpenXRGLBridge`, config | `XR_FB_swapchain_update_state`, `XR_FB_foveation`, `XR_FB_foveation_configuration`, per-eye color swapchain create chains, shared configured and level-zero rollback profiles | `BUILD_HISTORY.md`, `USER_GUIDE.md`, `TEST_CHECKLISTS.md` | Live runtime reports all extensions/functions and `operational=1`; compare GPU time and peripheral image quality at levels 0..3, then retain only a measured useful default |
| `FEATURE.LOCOMOTION` | BUILT | `OpenXRInput`, `HPLInputBridge`, `HPLNativeLocomotion`, `HPLPhysicalCrouchMath`, `HPLPlayerState` | SOMA semantic fallback, `0x1402375f0`, `0x140237460`, `0x1400ccc90`, calibrated HMD or movement-controller yaw/height, player/move ownership | `FUTURE_SYSTEMS_RE.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Live-test left-controller-relative direction after mouse/snap yaw, analog magnitude, physical-crouch synchronization, pause safety, and special-state fallback |
| `FEATURE.COMFORT_VIGNETTE` | BUILT | `HPLInputBridge`, `OpenXRComfortVignetteMath`, `OpenXRRuntime`, `OpenXRGLBridge`, `HPLStatusPanelBridge` | post-policy controller motion, stale-frame expiry, VIEW-space alpha quad and dedicated swapchain | `BUILD_HISTORY.md`, `FUTURE_SYSTEMS_RE.md`, `TEST_CHECKLISTS.md` | Live-test mask coverage, center clarity, movement/smooth-turn attack and release, pause/panel/loading suppression, F1 toggle, comfort presets, and teardown/recovery |
| `FEATURE.AUTHORED_CAMERA` | EXPERIMENTAL | `HPLPlayerState`, `HPLInputBridge`, `HPLCameraBridge`, `HPLComfortBridge` | camera rotate mode `+0x6c`, body camera ownership `+0x1e8`, exact player states `0..20`, same-camera baseline/history reseed, transition blackouts, semantic roll and optics wrappers | `VR_COMPATIBILITY_RE.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Live-test preserved tracking/stereo and native default-FOV restoration across sit, ladder/climb, conversation, animation, hand attachment, and death; then add only pose-composition policies required by evidence |
| `FEATURE.INTERACTION_RAY` | BUILT | `HPLInteractionBridge`, `HPLInteractionMath`, `HPLCameraBridge`, `HPLInputBridge`, `HPLGrabBridge` | `0x1400cd750`, inner raycast `0x1401438c0`, distance `+0x18`, body `+0x20`, entity `+0x28`, two local hand candidates, one outer finalizer, sticky acquisition plus state-lifetime owner lock, native `CanInteract` | `ADDRESS_REGISTRY.md`, `VR_COMPATIBILITY_RE.md`, `TEST_CHECKLISTS.md` | Live-test independent focus/activation, exact one-callback behavior, crossed-beam stability, and owner release/reacquisition across Slide/hinge/Grab/Read/terminal |
| `FEATURE.INTERACTION_RETICLE` | BUILT | `HPLInteractionBridge`, `HPLCrosshairBridge`, `HPLHudMath`, `HPLInputBridge`, `OpenXRRuntime`, `OpenXRGLBridge` | selected native aim/distance, simultaneous per-hand guides, independent guide/semantic swapchains, `0x140484ea0` script dispatch, exact 35-state enum, 34 shipped TGA icons, application-space alpha quads | `COMFORT_AND_FOCUS_RE.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Live-test both guide identities, semantic icon hand switching/depth, overlap stability, layer limits, and terminal coexistence; then determine whether geometry occlusion is needed |
| `FEATURE.PHYSICS_HANDS` | BUILT | `HPLGrabBridge`, `HPLGrabMath`, `HPLTwoHandMath`, `HPLInputBridge`, `OpenXRInput` | PID output `0x140238750`, AddImpulse thunk `0x14049c720`, Grab state `1`, selected-hit-to-grip initial correction, state-lifetime owner lock, exact force/torque tuples, grip pose and squeeze | `VR_COMPATIBILITY_RE.md`, `FUTURE_SYSTEMS_RE.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Live-test bounded pull/catch distance and strength, one/two-hand hold stability, placement, velocity-directed throws, crossed-beam ownership, mass classes, and all native fallbacks |
| `FEATURE.PHYSICAL_MANIPULATION` | BUILT | `HPLGrabBridge`, `HPLGrabMath`, `HPLReadMath`, `HPLInputBridge`, `HPLInputMath`, `HPLInteractionBridge`, `OpenXRInput` | states Wheel `3`, Slide `4`, Door `5`, Lever `6`, Tear `7`, Read `10`; PID `0x140238750`, pin `+0xe8`, pivot `+0xf4`; translation plus projected grip angular velocity; held right-A/B cancel; guarded Read scale/distance presentation | `FUTURE_SYSTEMS_RE.md`, `ADDRESS_REGISTRY.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Live-test drawer/curtain depth, wrist-driven taps/levers, Read size/distance/rotation/cancel and camera stability; preserve native limits, sounds, callbacks, and rollback; Wheel/Tear still require a direct native route |
| `FEATURE.VIEWMODEL` | BUILT | `HPLHandsBridge`, `HPLHandsMath`, `HPLTwoHandMath`, `HPLCameraBridge`, `HPLInputBridge` | interaction-owner world grip pose, support squeeze, `PlayerHandsHandler`, `0x14000fb60`, `0x1400bcd90`, `0x140127a70`, exact `HudObject`, socketed `*_HudObject`, destroy-time cache eviction | `FUTURE_SYSTEMS_RE.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Live-test left/right initiating-hand interaction objects, two-hand separation fallback, destroy/recreate identity, socketed-tool non-duplication, calibration, and native fallback; then add evidence-driven per-tool profiles |
| `FEATURE.FLASHLIGHT_ALIGNMENT` | BUILT | `HPLHandsBridge`, `HPLFlashlightMath`, `HPLCameraBridge` | exact `Flashlight` identity, `Player.hps::UpdateFlashLightLOS`, `0x14000fb60`, `0x1400bcd90`, `0x1400cd7d0`, `0x140143a10`, dominant aim pose | `FUTURE_SYSTEMS_RE.md`, `ADDRESS_REGISTRY.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Live-test visual beam calibration, agent response/gobo aim, cone preservation, tracking fallback, and verify tool/grounding rays remain native |
| `FEATURE.HUD_LAYER` | BUILT | `HPLHudBridge`, `HPLHudMath`, `OpenXRGLBridge`, `OpenXRRuntime` | `0x1400cc9b0`, `0x1400cca90`, `0x140071f20`, `0x140213970`, same-frame additive transparent capture, VIEW-space quad or `XR_KHR_composition_layer_cylinder`; packaged center clear disabled after clipping evidence | `FUTURE_SYSTEMS_RE.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Recheck interaction icon, subtitles, readable descriptions, overlay center, curved/quad switching, and exact pause/wake/dead/inventory current-ImGui owners with full native center preserved |
| `FEATURE.SUBTITLE_PRESENTATION` | BUILT | `HPLSubtitleBridge`, `HPLSubtitleMath`, `HPLHudBridge` | `0x1401c8dd0`, `0x1401d3ba0`, `cLuxVoiceHandler +0x174/+0x178/+0x17c/+0x180`, native game HUD draw | `ADDRESS_REGISTRY.md`, `FUTURE_SYSTEMS_RE.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Live-test language, speaker names, gradual reveal, multiline wrapping, large-font mode, scale/Y calibration, immediate restoration, and HUD-layer placement |
| `FEATURE.DESKTOP_SPECTATOR` | BUILT | `OpenXRRuntime`, `OpenXRGLBridge`, `OpenXRSpectatorMath` | AFR eye caches, pre-SwapBuffers frame boundary, GL backbuffer blit | `BUILD_HISTORY.md`, `ARCHITECTURE.md`, `TEST_CHECKLISTS.md` | Live-test left/right eye identity, fit/fill/stretch, window modes, HUD expectations, and native rollback |
| `FEATURE.POST_EFFECT_POLICY` | BUILT | `HPLCompatibilityProbe`, `OpenGLHooks` | `0x14033b8f0`, `0x14033bd80`, priority tree `+0x328`, named vtables, active byte `+0x31`, exact VideoDistortion type | `FUTURE_SYSTEMS_RE.md`, `BUILD_HISTORY.md`, `RUNTIME_ANALYSIS_0.5.2.md` | Live-test suppression of ImageTrail, VideoDistortion, ChromaticAberration, and RadialBlur while fades/tone mapping remain intact |
| `FEATURE.POST_EFFECT_RESOURCES` | BUILT | `HPLCompatibilityProbe`, `OpenGLHooks`, `HPLPostEffectResourceMath` | `0x1402d7a40`, effect virtual `+0x68`, HPL input/output texture identity, GL texture target/ID/dimensions/format, framebuffer writes, same-pose eye signatures | `FUTURE_SYSTEMS_RE.md`, `ADDRESS_REGISTRY.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Run `Ctrl+F6` in representative tone/bloom/fade/video scenes; classify shared versus eye-distinct resources and use direct evidence to split histories or promote stateless effects |
| `FEATURE.SCREEN_MATERIAL_CONVERGENCE` | BUILT | `HPLScreenEffectBridge`, `HPLScreenEffectMath`, `HPLCameraBridge` | `0x14024a2f0`, `0x140252700`, `0x1402936c0`, `0x140291700`, exact `Screen Particle<decimal>` identity | `FUTURE_SYSTEMS_RE.md`, `ADDRESS_REGISTRY.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Live-test shipped screen effects at 1.5 m, F10 rollback, native timing/opacity, destruction pairing, and zero unrelated billboard changes |
| `FEATURE.SHADOW_STABILITY` | PROVEN | `HPLCameraBridge`, `HPLCameraMath` | fully centered projection, programs `942/944` | `VR_COMPATIBILITY_RE.md`, `RUNTIME_ANALYSIS_0.5.6.md` | Regression-test additional levels and light types |
| `FEATURE.REFLECTION_STABILITY` | PROVEN | `HPLCameraBridge`, `HPLCameraMath` | fully centered projection, program `985` redirect | `VR_COMPATIBILITY_RE.md`, `RUNTIME_ANALYSIS_0.5.6.md` | Regression-test additional reflective materials and levels |
| `FEATURE.AUDIO_LISTENER` | BUILT | `HPLCompatibilityProbe`, `HPLCameraBridge`, `HPLCameraMath` | `0x140289340`, world head offset, `0x14061d188`, `0x14048b2d4` | `VR_COMPATIBILITY_RE.md`, `BUILD_HISTORY.md` | Directional-source and near-field tests confirm orientation plus room-scale translation without world-lock or Doppler errors |
| `FEATURE.LOADING_VIDEO` | BUILT | `HPLPresentationBridge`, `OpenXRRuntime`, `HPLInputBridge`, `HPLCameraBridge` | `0x1400ccdb0`, game context `0x1407925e0`, `0x140488fa0`, `0x140488fd0`, AFR cache invalidation, zero-layer blackout | `VR_COMPATIBILITY_RE.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Live-test save/map load entry and automatic stereo/input recovery; use video lifecycle rows to classify fullscreen versus diegetic playback before adding any video override |
| `FEATURE.COMFORT_POLICY` | BUILT | `HPLComfortBridge`, `HPLComfortMath`, `HPLCameraBridge`, `OpenXRRuntime`, post policy, input | `0x140159360`, `0x140156d90`, `0x140156f00`, `0x140155210`, `0x140155230`, `0x140155250`, `0x140071f80`, semantic IDs and bounded black frames | `COMFORT_AND_FOCUS_RE.md`, both future RE documents | Live-test semantic add/roll/optics suppression, DoF/VideoDistortion policy, and transition guards across locomotion, zoom, terminals, climb, sit, camera animation, and death |
| `FEATURE.BUILD_IDENTITY` | BUILT | CMake build manifest, `scripts/Package-Release.ps1` | version, flavor, OpenXR bit, DLL/package SHA-256 | `BUILD_HISTORY.md`, `SMOKE_TEST_MATRIX.md` | Promote only OpenXR-validated versioned bundles; add CI/reproducibility checks before public release |
| `FEATURE.RELEASE_LIFECYCLE` | BUILT | install/update/uninstall PowerShell scripts, installer lifecycle CTest | package SHA-256 ledger, `.somavr-install.json`, preserved `somavr.ini` | `README.md`, `BUILD_HISTORY.md` | Test a real packaged update and uninstall from the user-selected deployment directory |
| `FEATURE.CONFLICT_DIAGNOSTICS` | BUILT | `src/injector/CompatibilityScan.cpp` | target modules, game-directory proxy DLLs, duplicate `somavr.dll` block | `README.md`, `TEST_CHECKLISTS.md` | Validate warning quality with ReShade/API-layer configurations and extend only from observed conflicts |
| `FEATURE.READINESS_DIAGNOSTICS` | BUILT | injector `--doctor`, `CompatibilityScan`, `ConfigManager` | x64 PE check, build flavor, loader/config, active runtime registry/JSON, SOMA path, directory proxy scan, machine-readable exit | `USER_GUIDE.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Run from packaged install on each supported runtime and preserve zero-failure evidence with release logs |
| `FEATURE.COMFORT_PRESETS` | BUILT | `ConfigPreset`, `ConfigManager`, INI | custom/minimal/balanced/maximum pre-scan, explicit-key precedence, deterministic tests | `USER_GUIDE.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Live compare all presets through turn, recenter, transitions, authored cameras, and named effects; tune only from evidence |
| `FEATURE.END_USER_GUIDE` | BUILT | `docs/USER_GUIDE.md`, release packager | install, doctor, launch, controls, presets, rollback, logs, troubleshooting | `README.md`, `BUILD_HISTORY.md` | Keep commands and defaults synchronized with every release package |
| `FEATURE.SMOKE_MATRIX` | BUILT | `docs/SMOKE_TEST_MATRIX.md`, `docs/NEXT_LIVE_EVIDENCE.md`, bounded subsystem summaries | 12 representative startup/gameplay/UI/transition/recovery/shutdown scenarios plus four multi-feature evidence passes | `SMOKE_TEST_MATRIX.md`, `TEST_CHECKLISTS.md`, `NEXT_LIVE_EVIDENCE.md` | Assign stable campaign saves/checkpoints and run the promotion subset on each feature build |
| `FEATURE.TELEMETRY` | PROVEN | `OpenGLHooks`, `OpenGLMatrixAnalysis`, `HPLCompatibilityProbe`, `HPLPlayerState`, `HPLInputBridge`, `HPLInteractionBridge`, `HPLHudBridge`, `OpenXRRuntime`, logger, config | Swap/FBO/matrix/runtime/stage/audio/player-state/post-effect bounded logs, eye-cache microtiming, controller transform, hit/semantic, GUI ownership, manipulation-session evidence | all current docs | Use the 0.60 live pass to correlate residual stereo latency, controller-relative direction, interaction payload/semantic ownership, Read/Zoom GUI ownership, and per-state manipulation scale |

## Dependency Edges

These relations are intentionally explicit Graphify seeds:

```text
FEATURE.INJECTION requires FEATURE.TELEMETRY
FEATURE.XR_BOOTSTRAP requires FEATURE.INJECTION
FEATURE.XR_GL_SUBMISSION requires FEATURE.XR_BOOTSTRAP
FEATURE.XR_RESOURCE_RECREATION requires FEATURE.XR_GL_SUBMISSION
FEATURE.CLEAN_SHUTDOWN requires FEATURE.XR_GL_SUBMISSION
FEATURE.VR_MODE_CONTROL requires FEATURE.XR_GL_SUBMISSION
FEATURE.VR_MODE_CONTROL requires FEATURE.AFR_STEREO
FEATURE.VR_CONTROL_PANEL requires FEATURE.VR_MODE_CONTROL
FEATURE.VR_CONTROL_PANEL requires FEATURE.XR_INPUT
FEATURE.VR_CONTROL_PANEL requires FEATURE.XR_GL_SUBMISSION
FEATURE.VR_CONTROL_PANEL controls FEATURE.RECENTER
FEATURE.VR_CONTROL_PANEL controls FEATURE.DUAL_RENDER
FEATURE.VR_CONTROL_PANEL controls FEATURE.HUD_LAYER
FEATURE.VR_CONTROL_PANEL controls FEATURE.INTERACTION_RETICLE
FEATURE.RECENTER requires FEATURE.VR_MODE_CONTROL
FEATURE.RECENTER requires FEATURE.HEAD_TRACKING
FEATURE.ROOMSCALE_SAFETY requires FEATURE.HEAD_TRACKING
FEATURE.ROOMSCALE_SAFETY requires FEATURE.XR_REFERENCE_SPACE
FEATURE.ROOMSCALE_BODY_RECONCILIATION requires FEATURE.ROOMSCALE_SAFETY
FEATURE.ROOMSCALE_BODY_RECONCILIATION requires FEATURE.LOCOMOTION
FEATURE.FLASHLIGHT_ALIGNMENT requires FEATURE.XR_INPUT
FEATURE.FLASHLIGHT_ALIGNMENT requires FEATURE.HEAD_TRACKING
FEATURE.INTERACTION_RAY shares FEATURE.ROOMSCALE_SAFETY native line query family
FEATURE.PER_EYE_CPU_TELEMETRY requires FEATURE.AFR_STEREO
FEATURE.DUAL_RENDER requires FEATURE.PER_EYE_CPU_TELEMETRY
FEATURE.DUAL_RENDER requires FEATURE.VIEWPORT_OWNERSHIP
FEATURE.DUAL_RENDER requires FEATURE.DUAL_RENDER_TEMPORAL_PROBE
FEATURE.DUAL_RENDER_TEMPORAL_PROBE requires FEATURE.VIEWPORT_OWNERSHIP
FEATURE.PER_EYE_VIEW_HISTORY requires FEATURE.DUAL_RENDER
FEATURE.PER_EYE_VIEW_HISTORY requires FEATURE.DUAL_RENDER_TEMPORAL_PROBE
FEATURE.PER_EYE_VIEW_HISTORY requires FEATURE.AFR_STEREO
FEATURE.PER_EYE_IMAGE_TRAIL requires FEATURE.DUAL_RENDER
FEATURE.PER_EYE_IMAGE_TRAIL requires FEATURE.POST_EFFECT_RESOURCES
FEATURE.PER_EYE_IMAGE_TRAIL constrains FEATURE.POST_EFFECT_POLICY
FEATURE.PER_EYE_SSAO_HISTORY requires FEATURE.DUAL_RENDER
FEATURE.PER_EYE_SSAO_HISTORY requires FEATURE.DUAL_RENDER_TEMPORAL_PROBE
FEATURE.SSAO_FRAME_OWNER requires FEATURE.PER_EYE_SSAO_HISTORY
FEATURE.DESKTOP_SPECTATOR requires FEATURE.AFR_STEREO
FEATURE.DESKTOP_SPECTATOR requires FEATURE.XR_GL_SUBMISSION
FEATURE.XR_INPUT requires FEATURE.XR_BOOTSTRAP
FEATURE.XR_REFERENCE_SPACE requires FEATURE.XR_BOOTSTRAP
FEATURE.XR_DEPTH_CAPABILITY requires FEATURE.XR_GL_SUBMISSION
FEATURE.RELEASE_LIFECYCLE requires FEATURE.BUILD_IDENTITY
FEATURE.CONFLICT_DIAGNOSTICS requires FEATURE.INJECTION
FEATURE.READINESS_DIAGNOSTICS requires FEATURE.CONFLICT_DIAGNOSTICS
FEATURE.COMFORT_PRESETS requires FEATURE.COMFORT_POLICY
FEATURE.END_USER_GUIDE requires FEATURE.RELEASE_LIFECYCLE
FEATURE.TRACKING_RESILIENCE requires FEATURE.XR_GL_SUBMISSION
FEATURE.TRACKING_RESILIENCE constrains FEATURE.HEAD_TRACKING
FEATURE.TRACKING_RESILIENCE constrains FEATURE.AFR_STEREO
FEATURE.RECENTER requires FEATURE.XR_REFERENCE_SPACE
FEATURE.CONTROLLER_HAPTICS requires FEATURE.XR_INPUT
FEATURE.GRAB_CONTACT_HAPTICS requires FEATURE.CONTROLLER_HAPTICS
FEATURE.GRAB_CONTACT_HAPTICS requires FEATURE.PHYSICS_HANDS
FEATURE.CONTROLLER_ACCESSIBILITY requires FEATURE.XR_INPUT
FEATURE.XR_INTERACTION_PROFILE_DIAGNOSTICS requires FEATURE.XR_INPUT
FEATURE.CONTROLLER_ACCESSIBILITY constrains FEATURE.LOCOMOTION
FEATURE.MENU_POINTER requires FEATURE.XR_INPUT
FEATURE.PAUSED_MENU_LAYER requires FEATURE.HUD_LAYER
FEATURE.PAUSED_MENU_LAYER requires FEATURE.MENU_POINTER
FEATURE.PAUSED_MENU_LAYER constrains FEATURE.TELEMETRY
FEATURE.SCRIPTED_PRESENTATION requires FEATURE.HUD_LAYER
FEATURE.SCRIPTED_PRESENTATION requires FEATURE.COMFORT_POLICY
FEATURE.SCRIPTED_PRESENTATION constrains FEATURE.AUTHORED_CAMERA
FEATURE.INVENTORY_PRESENTATION requires FEATURE.HUD_LAYER
FEATURE.INVENTORY_PRESENTATION requires FEATURE.CONTROLLER_ACCESSIBILITY
FEATURE.INVENTORY_PRESENTATION constrains FEATURE.DIEGETIC_GUI_POINTER
FEATURE.LOCOMOTION requires FEATURE.XR_INPUT
FEATURE.COMFORT_VIGNETTE requires FEATURE.LOCOMOTION
FEATURE.COMFORT_VIGNETTE requires FEATURE.XR_GL_SUBMISSION
FEATURE.COMFORT_VIGNETTE constrains FEATURE.COMFORT_POLICY
FEATURE.HEAD_TRACKING requires FEATURE.XR_BOOTSTRAP
FEATURE.AFR_STEREO requires FEATURE.HEAD_TRACKING
FEATURE.AFR_STEREO requires FEATURE.XR_GL_SUBMISSION
FEATURE.DUAL_RENDER requires FEATURE.AFR_STEREO
FEATURE.DUAL_RENDER requires FEATURE.TELEMETRY
FEATURE.POST_EFFECT_POLICY requires FEATURE.DUAL_RENDER
FEATURE.POST_EFFECT_RESOURCES requires FEATURE.DUAL_RENDER
FEATURE.POST_EFFECT_POLICY requires FEATURE.POST_EFFECT_RESOURCES
FEATURE.SHADOW_STABILITY requires FEATURE.AFR_STEREO
FEATURE.REFLECTION_STABILITY requires FEATURE.AFR_STEREO
FEATURE.DUAL_RENDER requires FEATURE.SHADOW_STABILITY
FEATURE.DUAL_RENDER requires FEATURE.REFLECTION_STABILITY
FEATURE.HUD_LAYER requires FEATURE.XR_GL_SUBMISSION
FEATURE.HUD_LAYER constrains FEATURE.AFR_STEREO
FEATURE.SUBTITLE_PRESENTATION requires FEATURE.HUD_LAYER
FEATURE.SUBTITLE_PRESENTATION requires FEATURE.HEAD_TRACKING
FEATURE.AUTHORED_CAMERA requires FEATURE.HEAD_TRACKING
FEATURE.LOCOMOTION requires FEATURE.AUTHORED_CAMERA
FEATURE.INTERACTION_RAY requires FEATURE.AUTHORED_CAMERA
FEATURE.INTERACTION_RAY requires FEATURE.XR_INPUT
FEATURE.INTERACTION_RETICLE requires FEATURE.INTERACTION_RAY
FEATURE.INTERACTION_RETICLE requires FEATURE.XR_GL_SUBMISSION
FEATURE.INTERACTION_RETICLE constrains FEATURE.HUD_LAYER
FEATURE.PHYSICS_HANDS requires FEATURE.INTERACTION_RAY
FEATURE.PHYSICS_HANDS requires FEATURE.VIEWMODEL
FEATURE.PHYSICAL_MANIPULATION requires FEATURE.XR_INPUT
FEATURE.PHYSICAL_MANIPULATION requires FEATURE.AUTHORED_CAMERA
FEATURE.PHYSICAL_MANIPULATION constrains FEATURE.PHYSICS_HANDS
FEATURE.AUDIO_LISTENER requires FEATURE.AUTHORED_CAMERA
FEATURE.LOADING_VIDEO requires FEATURE.HUD_LAYER
FEATURE.COMFORT_POLICY constrains FEATURE.AUTHORED_CAMERA
FEATURE.COMFORT_POLICY constrains FEATURE.POST_EFFECT_POLICY
FEATURE.SCREEN_MATERIAL_CONVERGENCE requires FEATURE.HEAD_TRACKING
FEATURE.SCREEN_MATERIAL_CONVERGENCE constrains FEATURE.POST_EFFECT_POLICY
FEATURE.COMFORT_POLICY constrains FEATURE.LOCOMOTION
HPLCameraBridge requires HPLCameraMath
HPLCompatibilityProbe requires HPLCameraMath
OpenGLHooks requires OpenGLMatrixAnalysis
OpenXRRuntime requires OpenXRHelpers
OpenXRRuntime requires OpenXRInput
HPLInputBridge requires OpenXRInput
HPLInputBridge requires HPLCameraBridge
HPLInputBridge requires HPLPlayerState
HPLCompatibilityProbe consumes OpenGLHooks telemetry
HPLDualRenderControl controls FEATURE.DUAL_RENDER
HPLCompatibilityProbe executes FEATURE.DUAL_RENDER exact viewport replay
HPLScreenEffectBridge requires HPLScreenEffectMath
HPLScreenEffectBridge requires HPLCameraBridge
HPLInputBridge requires HPLNativeLocomotion
HPLInputBridge requires HPLMenuBridge
HPLNativeLocomotion requires HPLPlayerState
HPLNativeLocomotion requires HPLRoomscaleReconciliationMath
HPLNativeLocomotion uses HPLCameraBridge roomscale collision and neutral compensation
HPLMenuBridge requires HPLMenuMath
HPLHandsBridge requires HPLHandsMath
HPLHudBridge requires OpenXRRuntime
HPLInputBridge feeds FEATURE.PHYSICAL_MANIPULATION
OpenXRRuntime requires HPLHudMath
HPLInteractionBridge feeds FEATURE.INTERACTION_RETICLE
HPLCrosshairBridge feeds FEATURE.INTERACTION_RETICLE
HPLCrosshairBridge requires HPLInteractionBridge
OpenXRGLBridge consumes SOMA native crosshair assets
```

## Runtime Flow

```text
somavr_injector.exe
  -> somavr.dll
  -> OpenGLHooks::HookSwapBuffers
  -> OpenXRRuntime::OnFrameBoundary
  -> HPLInputBridge::UpdateHPLInputBridge
  -> xrWaitFrame / xrBeginFrame / xrLocateViews
  -> HPLCameraBridge::HookCameraGetFrustum
  -> OpenXRGLBridge eye cache or swapchain copy
  -> xrEndFrame
```

Future same-frame flow:

```text
OpenXR predicted views
  -> FEATURE.DUAL_RENDER
  -> left/right scene and post targets
  -> FEATURE.HUD_LAYER
  -> projection layers plus optional quad/cylinder HUD layer
  -> FEATURE.XR_GL_SUBMISSION
```

Gameplay ownership flow:

```text
OpenXR actions
  -> FEATURE.LOCOMOTION -> native player/action state -> iCharacterBody
  -> FEATURE.INTERACTION_RAY -> native pick/CanInteract -> HPLCrosshairBridge semantic state -> native depth icon
  -> FEATURE.MENU_POINTER -> native paused menu cursor/click path
  -> controller pose -> FEATURE.PHYSICS_HANDS -> native PID force/torque
  -> relative grip/head motion -> FEATURE.PHYSICAL_MANIPULATION -> native mvMoveAdd/joints/PIDs
```

## Graphify Workflow

Generated graph files live in `graphify-out/` and are intentionally ignored.

```powershell
graphify update .
graphify query "How does an OpenXR pose reach SOMA's rendered view?"
graphify explain "Same-Frame Dual Render Boundary"
graphify affected "HPLCameraBridge"
graphify tree --graph graphify-out/graph.json --output graphify-out/GRAPH_TREE.html --root . --label SOMAVR
```

Run `graphify update .` after code or architecture-document changes. Use
`graphify path`, `query`, and `explain` before broad source searches, then use
`rg` for exact literals and final line-level confirmation.
