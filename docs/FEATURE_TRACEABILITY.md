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
| `FEATURE.VR_CONTROL_PANEL` | BUILT | `HPLStatusPanelBridge`, `OpenXRStatusPanelMath`, `OpenXRRuntime`, `OpenXRGLBridge` | F1, Menu+Secondary, VIEW-space alpha quad, guarded camera/runtime setters | `BUILD_HISTORY.md`, `ARCHITECTURE.md`, `TEST_CHECKLISTS.md` | Live-test layer orientation/alpha, exclusive input, every reversible action, and no stereo/world regression |
| `FEATURE.RECENTER` | BUILT | `HPLCameraBridge`, `HPLCameraMath`, `HPLInputBridge` | F2 or two-grip hold, stable neutral-pose latch | `BUILD_HISTORY.md`, `CURRENT_STATE.md`, `TEST_CHECKLISTS.md` | Live test confirms keyboard and controller recenter without stereo/session reset or height drift |
| `FEATURE.XR_INPUT` | BUILT | `OpenXRInput`, `OpenXRRuntime` | OpenXR action set, Simple/Touch/Index/Motion bindings, grip/aim action spaces | `BUILD_HISTORY.md`, `CURRENT_STATE.md`, `TEST_CHECKLISTS.md` | Live log confirms active bindings, both tracked controllers, and stable predicted poses |
| `FEATURE.XR_REFERENCE_SPACE` | BUILT | `OpenXRRuntime`, config | `XR_REFERENCE_SPACE_TYPE_LOCAL`, optional `STAGE`, runtime fallback | `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Compare seated/local and standing/stage calibration, eye height, recenter, and map transitions |
| `FEATURE.ROOMSCALE_SAFETY` | BUILT | `HPLCameraBridge`, `HPLCameraMath`, config | `0x1400cd710`, `0x140143650`, optional dynamic-inclusive center/radial/vertical sweep, shared physical-head decomposition | `FUTURE_SYSTEMS_RE.md`, `ADDRESS_REGISTRY.md`, `TEST_CHECKLISTS.md` | Live-test static walls plus moving doors/props and skipped authored-start probes; then recover shape cast or native capsule reconciliation |
| `FEATURE.ROOMSCALE_BODY_RECONCILIATION` | BUILT | `HPLNativeLocomotion`, `HPLCameraBridge`, `HPLRoomscaleReconciliationMath`, config | `0x140237920`, `0x140237970`, body size `+0x134`, sustained-displacement hysteresis, sampled capsule sweep, neutral-pose compensation | `FUTURE_SYSTEMS_RE.md`, `ADDRESS_REGISTRY.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Live-test capsule catch-up near walls, doors, stairs, crouch transitions, authored states, and repeated direction changes; retain config rollback until accepted |
| `FEATURE.TRACKING_RESILIENCE` | BUILT | `OpenXRRuntime`, `HPLCameraBridge` | pose-age bound, last-valid eye cache, zero-layer loss path, recovery blackout | `BUILD_HISTORY.md`, `CURRENT_STATE.md`, `TEST_CHECKLISTS.md` | Live-test brief and extended HMD tracking loss without stale-eye corruption, stereo teardown, or a visible recovery flash |
| `FEATURE.CONTROLLER_HAPTICS` | BUILT | `OpenXRInput`, `OpenXRRuntime`, `HPLInputBridge`, `HPLInteractionBridge`, `HPLHudMath` | vibration output action, per-hand output paths, focused-session guard, native focus identity plus semantic intent profiles | `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Confirm discrete and semantic focus pulses across active controller profiles without edge chatter or default-cursor buzz |
| `FEATURE.CONTROLLER_ACCESSIBILITY` | BUILT | `OpenXRInput`, `HPLInputBridge`, config | per-hand primary/secondary actions, dominant-hand roles, stick swap, one-hand fallback, support-hand flashlight/inventory | `BUILD_HISTORY.md`, `FUTURE_SYSTEMS_RE.md`, `TEST_CHECKLISTS.md` | Live-test role-aware jump/crouch/flashlight/inventory, swapped-stick, and each one-controller path on Touch/Index; define missing Simple/Motion bindings |
| `FEATURE.MENU_POINTER` | BUILT | `HPLMenuBridge`, `HPLMenuMath`, `HPLInputBridge`, `HPLNativeLocomotion` | `0x1400ccc90`, HMD/aim orientations, native SOMA client cursor and left-click path | `BUILD_HISTORY.md`, `FUTURE_SYSTEMS_RE.md`, `TEST_CHECKLISTS.md` | Live-test window modes, native cursor mapping, click-release latch, and non-pause ImGui surfaces; then couple pointer coordinates to future menu-layer presentation |
| `FEATURE.PAUSED_MENU_LAYER` | BUILT | `HPLHudBridge`, `HPLNativeLocomotion`, `OpenXRGLBridge`, `OpenXRRuntime` | `0x1400cca70`, `0x140071f20`, `0x1400ccc90`, exact current ImGui set plus confirmed pause ownership | `BUILD_HISTORY.md`, `FUTURE_SYSTEMS_RE.md`, `TEST_CHECKLISTS.md` | Live-test pause alpha/order, cursor alignment, resume behavior, window modes, and strict exclusion of non-paused/diegetic current ImGui |
| `FEATURE.DIEGETIC_GUI_POINTER` | BUILT | `HPLTerminalBridge`, `HPLInputBridge`, `HPLMenuMath`, `HPLPlayerState` | wall/handheld terminal states `8/9`, `0x1400cca70`, `0x1400cca90`, `0x140071f20`, `0x1402f0c90`, `cGuiSet +0x100..+0x10c/+0x139` | `ADDRESS_REGISTRY.md`, `FUTURE_SYSTEMS_RE.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Live-test both terminal classes, cursor alignment/clicks, unfocused handheld fallback, and rollback; refine to exact controller-ray/terminal-plane UV only if head-relative aim alignment is insufficient |
| `FEATURE.HEAD_TRACKING` | PROVEN | `HPLCameraBridge`, `HPLCameraMath` | `0x140271b80`, `0x140270230` | `CURRENT_STATE.md`, `VR_COMPATIBILITY_RE.md` | Remain correct through every authored camera state |
| `FEATURE.AFR_STEREO` | PROVEN | `HPLCameraBridge`, `OpenXRRuntime`, `OpenXRGLBridge` | F11, per-eye cache and submitted render pose | `BUILD_HISTORY.md`, `RUNTIME_ANALYSIS_0.5.1.md` | Preserve stability while shader/temporal compatibility is classified |
| `FEATURE.DUAL_RENDER` | EXPERIMENTAL | `HPLCompatibilityProbe`, `HPLDualRenderMath`, `HPLDualRenderDiagnostics`, `OpenXRRuntime` | `0x140298850`, `0x140298630`, immediate first-eye cache, manual/automatic bounded one-frame replay with GUI bit removed | `VR_COMPATIBILITY_RE.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Live automatic samples prove same-pose opposite eyes and classify whether duplicated post-post mutation ranges are equivalent or eye-specific before any persistent path is enabled |
| `FEATURE.DUAL_RENDER_TEMPORAL_PROBE` | BUILT | `HPLDualRenderDiagnostics`, `HPLTemporalMutationMath`, `HPLCompatibilityProbe` | `0x1401f1480`, renderer/current/history/settings snapshots, `*(renderer+0x20)+0x158 -> *(renderer+0x438)+0x80` | `ADDRESS_REGISTRY.md`, `VR_COMPATIBILITY_RE.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Three automatic samples produce readable first/replay region summaries and correlated mutation ranges without crashes or visible state regression |
| `FEATURE.VIEWPORT_OWNERSHIP` | BUILT | `HPLCompatibilityProbe`, `HPLCameraBridge`, `HPLPlayerState` | `0x140298630`, `0x140297f20`, `0x140297500`, viewport camera/world/renderer/post/FBO/settings fields | `ADDRESS_REGISTRY.md`, `VR_COMPATIBILITY_RE.md`, `TEST_CHECKLISTS.md` | Live logs classify reflection, terminal, loading, and save-transition viewports while only the exact player camera receives VR controls |
| `FEATURE.PER_EYE_CPU_TELEMETRY` | BUILT | `HPLCompatibilityProbe`, `HPLCameraBridge` | six guarded render-stage hooks, active AFR eye identity, QPC totals | `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md`, `VR_COMPATIBILITY_RE.md` | Capture representative left/right stage budgets alongside GPU timestamps |
| `FEATURE.PER_EYE_GPU_TELEMETRY` | BUILT | `HPLCompatibilityProbe`, `HPLCameraBridge` | six guarded render stages, nested `GL_TIMESTAMP` query pairs, bounded nonblocking pool | `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md`, `VR_COMPATIBILITY_RE.md` | Live capture shows nonzero left/right timings with no sustained dropped or invalid samples |
| `FEATURE.XR_DEPTH_CAPABILITY` | BUILT | `OpenXRRuntime`, `OpenXRGLBridge`, `OpenXRDepthMath`, `HPLCameraBridge` | `XR_KHR_composition_layer_depth`, D24/D32F/depth-stencil negotiation, same-format per-eye caches and swapchains, standard OpenGL depth range, HPL near/far divided by world units per meter | `FUTURE_SYSTEMS_RE.md`, `VR_COMPATIBILITY_RE.md`, `TEST_CHECKLISTS.md` | Live log confirms two depth swapchains, per-eye copies, and submitted depth frames without visual or runtime regression |
| `FEATURE.LOCOMOTION` | BUILT | `OpenXRInput`, `HPLInputBridge`, `HPLNativeLocomotion`, `HPLPhysicalCrouchMath`, `HPLPlayerState` | SOMA semantic fallback, `0x1402375f0`, `0x140237460`, `0x1400ccc90`, calibrated HMD yaw/height, player/move ownership | `FUTURE_SYSTEMS_RE.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Live-test head-relative direction, analog magnitude, physical-crouch toggle synchronization, pause safety, and special-state fallback |
| `FEATURE.AUTHORED_CAMERA` | EXPERIMENTAL | `HPLPlayerState`, `HPLInputBridge`, `HPLCameraBridge`, `HPLComfortBridge` | camera rotate mode `+0x6c`, body camera ownership `+0x1e8`, exact player states `0..20`, transition blackouts, semantic roll and optics wrappers | `VR_COMPATIBILITY_RE.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Live-test state IDs, native default-FOV restoration, and bounded guards across sit, ladder/climb, conversation, animation, and death; then add only the pose-composition policies evidence requires |
| `FEATURE.INTERACTION_RAY` | BUILT | `HPLInteractionBridge`, `HPLCameraBridge`, `HPLInputBridge` | `0x1400cd750`, `cLuxClosestEntityData +0x18/+0x20/+0x28`, world aim/hit snapshots, native `CanInteract` | `COMFORT_AND_FOCUS_RE.md`, `VR_COMPATIBILITY_RE.md`, `TEST_CHECKLISTS.md` | Live-test controller-directed focus and decoded distance/world hit across targets and states; verify no-hit/fallback clears validity and correlate native semantic states |
| `FEATURE.INTERACTION_RETICLE` | BUILT | `HPLInteractionBridge`, `HPLCrosshairBridge`, `HPLHudMath`, `OpenXRRuntime`, `OpenXRGLBridge` | native pick aim/distance, `0x140484ea0` script dispatch, exact 35-state enum, 34 shipped TGA icons, application-space alpha quad | `COMFORT_AND_FOCUS_RE.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Live-test native icon identity/aspect, semantic clearing, convergence, intent colors/haptics, then determine whether geometry occlusion is needed |
| `FEATURE.PHYSICS_HANDS` | BUILT | `HPLGrabBridge`, `HPLGrabMath`, `HPLTwoHandMath`, `HPLInputBridge`, `OpenXRInput` | PID output `0x140238750`, AddImpulse thunk `0x14049c720`, Grab state `1`, exact force/torque tuples, dominant/support grip pose and squeeze | `VR_COMPATIBILITY_RE.md`, `FUTURE_SYSTEMS_RE.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Live-test one/two-hand translation and shortest-arc rotation stability, mode re-anchoring, per-axis sign, one-shot velocity-directed throws, mass classes, and all native fallbacks |
| `FEATURE.PHYSICAL_MANIPULATION` | BUILT | `HPLInputBridge`, `HPLInputMath`, `OpenXRInput` | shipped states Wheel `3`, Slide `4`, SwingDoor `5`, Lever `6`, Tear `7`; `RotateBase::OnAnalogInput -> mvMoveAdd`; relative grip/head pose | `FUTURE_SYSTEMS_RE.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Live-test all five states, common-translation cancellation, per-axis sign/sensitivity, tracking reacquisition, and native constraints/callbacks |
| `FEATURE.VIEWMODEL` | BUILT | `HPLHandsBridge`, `HPLHandsMath`, `HPLTwoHandMath`, `HPLCameraBridge`, `HPLInputBridge` | world grip poses, support squeeze, `PlayerHandsHandler`, `0x14000fb60`, `0x1400bcd90`, `0x140127a70`, exact `HudObject`, socketed `*_HudObject`, destroy-time cache eviction | `FUTURE_SYSTEMS_RE.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Live-test one/two-hand independent interaction objects, separation fallback, destroy/recreate identity, socketed-tool non-duplication, calibration, and native fallback; then add evidence-driven per-tool profiles |
| `FEATURE.FLASHLIGHT_ALIGNMENT` | BUILT | `HPLHandsBridge`, `HPLFlashlightMath`, `HPLCameraBridge` | exact `Flashlight` identity, `Player.hps::UpdateFlashLightLOS`, `0x14000fb60`, `0x1400bcd90`, `0x1400cd7d0`, `0x140143a10`, dominant aim pose | `FUTURE_SYSTEMS_RE.md`, `ADDRESS_REGISTRY.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Live-test visual beam calibration, agent response/gobo aim, cone preservation, tracking fallback, and verify tool/grounding rays remain native |
| `FEATURE.HUD_LAYER` | BUILT | `HPLHudBridge`, `HPLHudMath`, `OpenXRGLBridge`, `OpenXRRuntime` | `0x1400cc9b0`, `0x1400cca90`, `0x140071f20`, `0x140213970`, same-frame additive transparent capture, VIEW-space quad | `FUTURE_SYSTEMS_RE.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Live-test GameHudSet plus GameHudImGui alpha/order, hints/inventory/subtitle ownership, center-clear coverage, and native fallback |
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
| `FEATURE.SMOKE_MATRIX` | BUILT | `docs/SMOKE_TEST_MATRIX.md`, bounded subsystem summaries | 12 representative startup/gameplay/UI/transition/recovery/shutdown scenarios | `SMOKE_TEST_MATRIX.md`, `TEST_CHECKLISTS.md` | Assign stable campaign saves/checkpoints and run the promotion subset on each feature build |
| `FEATURE.TELEMETRY` | PROVEN | `OpenGLHooks`, `OpenGLMatrixAnalysis`, `HPLCompatibilityProbe`, `HPLPlayerState`, `HPLInputBridge`, logger, config | Swap/FBO/matrix/runtime/stage/audio/player-state/post-effect bounded logs | all current docs | Correlate authored-camera transitions, stage-tagged draws, GUI state, and per-effect identities in a live run |

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
FEATURE.DESKTOP_SPECTATOR requires FEATURE.AFR_STEREO
FEATURE.DESKTOP_SPECTATOR requires FEATURE.XR_GL_SUBMISSION
FEATURE.XR_INPUT requires FEATURE.XR_BOOTSTRAP
FEATURE.XR_REFERENCE_SPACE requires FEATURE.XR_BOOTSTRAP
FEATURE.XR_DEPTH_CAPABILITY requires FEATURE.XR_GL_SUBMISSION
FEATURE.RELEASE_LIFECYCLE requires FEATURE.BUILD_IDENTITY
FEATURE.CONFLICT_DIAGNOSTICS requires FEATURE.INJECTION
FEATURE.TRACKING_RESILIENCE requires FEATURE.XR_GL_SUBMISSION
FEATURE.TRACKING_RESILIENCE constrains FEATURE.HEAD_TRACKING
FEATURE.TRACKING_RESILIENCE constrains FEATURE.AFR_STEREO
FEATURE.RECENTER requires FEATURE.XR_REFERENCE_SPACE
FEATURE.CONTROLLER_HAPTICS requires FEATURE.XR_INPUT
FEATURE.CONTROLLER_ACCESSIBILITY requires FEATURE.XR_INPUT
FEATURE.CONTROLLER_ACCESSIBILITY constrains FEATURE.LOCOMOTION
FEATURE.MENU_POINTER requires FEATURE.XR_INPUT
FEATURE.PAUSED_MENU_LAYER requires FEATURE.HUD_LAYER
FEATURE.PAUSED_MENU_LAYER requires FEATURE.MENU_POINTER
FEATURE.PAUSED_MENU_LAYER constrains FEATURE.TELEMETRY
FEATURE.LOCOMOTION requires FEATURE.XR_INPUT
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
  -> projection layers plus optional quad layer
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
