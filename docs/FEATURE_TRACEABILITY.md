# Feature Traceability Registry

This is the canonical, graph-friendly index for SOMAVR features. Stable
`FEATURE.*` IDs should appear in future design notes, commits, tests, and bounded
telemetry where useful. Graphify can then connect implementation, native anchors,
evidence, and acceptance gates without relying on filenames alone.

Status values: `PROVEN`, `EXPERIMENTAL`, `BUILT`, `DESIGNED`, `RE_REQUIRED`, `BLOCKED`.

## Registry

September 10: `FEATURE.VISIBLE_HANDS` adds `HPLHandsBootstrap` and its tested
one-shot policy. Native PostUpdate `0x1ab3a0` uses secondary receiver `+0x110`;
the full module owns the script wrapper. Before-vial creation is BUILT, not
headset-proven. `HANDS_BOOTSTRAP_RE.md` supersedes old `0x154c40` probe ownership
claims and owns the new bootstrap acceptance test. Inventory's old `0x1378e0`
module identity claim is refuted (that function belongs to cLuxMapHandler);
its presentation observer needs a separately scoped audit.

September 9 interaction follow-through update: `FEATURE.PHYSICAL_MANIPULATION`
now includes consistent scaled slide targets, body-attached hinge leverage and
exact-body fast-release/native-throw handoff. Source/log evidence, tests and
remaining headset gates: `INTERACTION_FOLLOW_REVIEW_2026-09-09.md`.

| Feature ID | Status | Code owner | Native/runtime anchors | Primary documentation | Next acceptance gate |
| --- | --- | --- | --- | --- | --- |
| `FEATURE.PLAYER_SPACE_ORIENTATION` | BUILT | `HPLCameraBridge`, `HPLCameraMath`, `HPLInputMath`, `HPLHandsBridge` | native camera basis composed before tracking-relative HMD, clockwise heading inverse, shared torso/elbow yaw, current-view story offset at latched distance | `TEST_REVIEW_2026-09-07.md`, `ARCHITECTURE.md` | Story pickup and torso/wrist alignment at nonzero native headings, snap turns and physical turns |
| `FEATURE.STEREO_EYE_IMAGE_CAPTURE` | BUILT | `OpenXRRuntime`, `OpenXRGLBridge` | Ctrl+F10, two recent paired-eye cache RGB readbacks, per-eye pose frame, 15-frame separation, own-GL scope | `TEST_REVIEW_2026-09-07.md`, `NEXT_LIVE_EVIDENCE.md` | Capture locomotion-only arm lag with both eye images and CPU palette witness; exclude diagnostic hitches |
| `FEATURE.INJECTION` | PROVEN | `src/injector/main.cpp`, `src/dll/DllMain.cpp` | Remote `LoadLibraryW`, early DLL initialization | `CURRENT_STATE.md` | Launch and attach modes remain reliable across load/save cycles |
| `FEATURE.XR_BOOTSTRAP` | EXPERIMENTAL | `OpenXRRuntime`, `OpenXRHelpers` | OpenXR loader, instance, system, session state, delayed recovery | `CURRENT_STATE.md`, `UEVR_LEARNINGS.md` | Live-test session and instance loss recovery without restarting SOMA |
| `FEATURE.XR_SUBSTITUTE_RUNTIME` | BUILT | `tools/xrsim` | per-process `XR_RUNTIME_JSON`, x64 OpenGL swapchains, scripted pose/action/focus/lifecycle hazards, compositor capture, SOMA injector/window automation | `tools/xrsim/README.md`, `SMOKE_TEST_MATRIX.md`, `PRIOR_ART_SWEEP_2026-09-01.md` | Run the self-test and S00 on every XR contract change; keep real-runtime cadence, transfer cost, optics, and comfort as headset-only gates |
| `FEATURE.XR_GL_SUBMISSION` | PROVEN | `OpenXRRuntime`, `OpenXRGLBridge`, `OpenGLOwnership` | OpenGL swapchains/FBOs, full-state own-GL transaction and detour bypass, one prioritized projection on every begun `xrEndFrame`, per-eye acquire/wait/copy/flush/release timing, nonblocking cache-capture/submit GPU timestamp rings, and display-budget pressure | `CURRENT_STATE.md`, `BUILD_HISTORY.md`, `OPENXR_GL_TRANSFER_RE.md` | Run the controlled VirtualDesktopXR/SteamVR A/B; compare `copyCpu` with `gpuCapture/gpuSubmit`, retain native GL unless measured GPU/runtime handoff cost materially justifies D3D11 interop |
| `FEATURE.XR_RESOURCE_RECREATION` | BUILT | `OpenXRRuntime`, `OpenXRGLBridge` | bound HDC/HGLRC identity, periodic `xrEnumerateViewConfigurationViews`, transactional frame-resource rebuild | `BUILD_HISTORY.md`, `VR_COMPATIBILITY_RE.md`, `TEST_CHECKLISTS.md` | Live-test GL context replacement and runtime view-size/sample changes without stale resources or a process restart |
| `FEATURE.XR_FOCUS_PACING` | BUILT | `OpenXRRuntime`, `OpenXRFramePacingMath` | session-state events, ever-focused latch, independent Wait/Begin/End timing, frame-open invariant, stale-cache invalidation on focus recovery, symmetric F10 session/instance teardown | `BIOSHOCK_VR_TRANSFER_AUDIT.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Prove VISIBLE/unfocused recovery and F10 stop/restart; compare Wait/Begin/End, handoff, GL-transfer, lock, and fresh-frame evidence before any worker-thread claim |
| `FEATURE.CLEAN_SHUTDOWN` | PROVEN | `HPLLifecycle`, `OpenXRRuntime` | `0x1403b16e0`, `0x1403b1803` | `RUNTIME_ANALYSIS_0.5.6.md`, `GHIDRA_SYNC.md` | Preserve clean exit across runtime/session-loss paths |
| `FEATURE.VR_MODE_CONTROL` | EXPERIMENTAL | `HPLCameraBridge`, `OpenXRRuntime` | F10 pending activation, symmetric runtime suspend/rebootstrap, F8/F11 diagnostics | `CURRENT_STATE.md`, `TEST_CHECKLISTS.md` | One F10 reaches tracking/stereo; second F10 destroys runtime pacing; third F10 recreates the session and recovers without restarting SOMA |
| `FEATURE.VR_CONTROL_PANEL` | BUILT | `HPLStatusPanelBridge`, `OpenXRStatusPanelMath`, `OpenXRRuntime`, `OpenXRGLBridge` | F1, Menu+Secondary, VIEW-space alpha quad, guarded camera/runtime setters, same-frame stereo, HUD-shape, and comfort-vignette actions | `BUILD_HISTORY.md`, `ARCHITECTURE.md`, `TEST_CHECKLISTS.md` | Live-test layer orientation/alpha, exclusive input, all nine reversible actions including HUD shape and comfort vignette, and no stereo/world regression |
| `FEATURE.RECENTER` | BUILT | `HPLCameraBridge`, `HPLCameraMath`, `HPLInputBridge` | F2 or two-grip hold, stable position plus yaw-only neutral latch; pitch/roll remain level-space relative | `BUILD_HISTORY.md`, `CURRENT_STATE.md`, `TEST_CHECKLISTS.md` | Live test tilted recenter without horizon tilt, stereo/session reset, or height drift |
| `FEATURE.XR_INPUT` | BUILT | `OpenXRInput`, `OpenXRRuntime` | OpenXR action set, Simple/Touch/Index/Motion bindings, grip/aim action spaces | `BUILD_HISTORY.md`, `CURRENT_STATE.md`, `TEST_CHECKLISTS.md` | Live log confirms active bindings, both tracked controllers, and stable predicted poses |
| `FEATURE.XR_REFERENCE_SPACE` | BUILT | `OpenXRRuntime`, config | `XR_REFERENCE_SPACE_TYPE_LOCAL`, optional `STAGE`, runtime fallback | `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Compare seated/local and standing/stage calibration, eye height, recenter, and map transitions |
| `FEATURE.ROOMSCALE_SAFETY` | BUILT | `HPLCameraBridge`, `HPLCameraMath`, config | `0x1400cd710`, `0x140143650`, optional dynamic-inclusive center/radial/vertical sweep, shared physical-head decomposition | `FUTURE_SYSTEMS_RE.md`, `ADDRESS_REGISTRY.md`, `TEST_CHECKLISTS.md` | Live-test static walls plus moving doors/props and skipped authored-start probes; then recover shape cast or native capsule reconciliation |
| `FEATURE.ROOMSCALE_BODY_RECONCILIATION` | BUILT | `HPLNativeLocomotion`, `HPLCameraBridge`, `HPLRoomscaleReconciliationMath`, config | `0x140237920`, `0x140237970`, body size `+0x134`, sustained-displacement hysteresis, sampled capsule sweep, neutral-pose compensation | `FUTURE_SYSTEMS_RE.md`, `ADDRESS_REGISTRY.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Live-test capsule catch-up near walls, doors, stairs, crouch transitions, authored states, and repeated direction changes; retain config rollback until accepted |
| `FEATURE.TRACKING_RESILIENCE` | BUILT | `OpenXRRuntime`, `HPLCameraBridge` | one upcoming-render locate per tick, immutable AFR pair pose/base with 100 ms guard, retained/black projection, recovery blackout, fresh/held/black/fallback/incomplete frame ledger | `BUILD_HISTORY.md`, `CURRENT_STATE.md`, `TEST_CHECKLISTS.md` | Live-test brief and extended HMD tracking loss; require pair recovery, bounded held age, valid fresh-pair rate, no mixed-pair state, stale-eye corruption, stereo teardown, or zero-layer submit |
| `FEATURE.CONTROLLER_HAPTICS` | BUILT | `OpenXRInput`, `OpenXRRuntime`, `HPLInputBridge`, `HPLInteractionBridge`, `HPLGameplayHapticsBridge`, `HPLGameplayHapticsMath` | vibration output action, per-hand output paths, `0x140109b30` authored rumble, focused-session guard, segmented refresh/stop envelope | `FUTURE_SYSTEMS_RE.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Live-test damage, death, datamining, locked interaction, sustained effects, and physical-gamepad coexistence; native Grab impacts are owned separately by `FEATURE.GRAB_CONTACT_HAPTICS` |
| `FEATURE.GRAB_CONTACT_HAPTICS` | BUILT | `HPLContactHapticsBridge`, `HPLContactHapticsMath`, `HPLCameraBridge`, `OpenXRRuntime` | `0x14032f0e0` native surface impact, Grab state `1`, native normal speed/contact position/count/body, fresh dominant-grip world pose | `CONTACT_HAPTICS_RE.md`, `ADDRESS_REGISTRY.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Live-test weak/strong impacts, duplicate-material cooldown, hand ownership, false positives, native sound/physics preservation, and rollback; use `0x14032f380` slide telemetry only if sustained scrape feedback is justified |
| `FEATURE.CONTROLLER_ACCESSIBILITY` | BUILT | `OpenXRInput`, `HPLInputBridge`, config | per-hand primary/secondary actions, dominant-hand roles, stick swap, one-hand fallback, support-hand flashlight/inventory, five standard suggested profiles | `BUILD_HISTORY.md`, `FUTURE_SYSTEMS_RE.md`, `TEST_CHECKLISTS.md` | Live-test role-aware actions, swapped-stick, and each one-controller path on Touch/Index/WMR/Vive; Simple remains intentionally pose/select/menu limited |
| `FEATURE.XR_INTERACTION_PROFILE_DIAGNOSTICS` | BUILT | `OpenXRInput`, `OpenXRRuntime` | `XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED`, per-hand `xrGetCurrentInteractionProfile`, path resolution, session reset and summary | `BUILD_HISTORY.md`, `FUTURE_SYSTEMS_RE.md`, `TEST_CHECKLISTS.md` | Capture correct left/right profiles across Touch, Index, WMR, Vive, reconnect, one-hand loss, and runtime recovery |
| `FEATURE.MENU_POINTER` | BUILT | `HPLMenuBridge`, `HPLMenuMath`, `HPLInputBridge`, `HPLNativeLocomotion`, `OpenXRRuntime` | `0x1400ccc90`, HMD/aim orientations, native SOMA client cursor/click path, pause or no-camera-control main-menu ownership, native desktop backbuffer preservation | `BUILD_HISTORY.md`, `FUTURE_SYSTEMS_RE.md`, `TEST_CHECKLISTS.md` | Live-test pause/main-menu pointer and desktop mirror, window modes, cursor mapping, and click-release latch |
| `FEATURE.PAUSED_MENU_LAYER` | BUILT | `HPLHudBridge`, `HPLNativeLocomotion`, `OpenXRGLBridge`, `OpenXRRuntime` | `0x1400cca70`, `0x140071f20`, `0x1400ccc90`, exact current ImGui set plus confirmed pause ownership | `BUILD_HISTORY.md`, `FUTURE_SYSTEMS_RE.md`, `TEST_CHECKLISTS.md` | Live-test pause alpha/order, cursor alignment, resume behavior, window modes, and strict exclusion of non-paused/diegetic current ImGui |
| `FEATURE.SCRIPTED_PRESENTATION` | BUILT | `HPLCrosshairBridge`, `HPLPresentationBridge`, `HPLHudBridge`, `HPLInputBridge` | `0x140484ea0`, `0x140485200`, `0x140485720`, dead state `17`, module IDs `10/12/19`, exact current/GameHud ImGui | `ADDRESS_REGISTRY.md`, `FUTURE_SYSTEMS_RE.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Live-test sleep blackout, wake eyelid timing/alpha, game-over text/continue, credits, loading overlap, and config rollback |
| `FEATURE.INVENTORY_PRESENTATION` | BUILT | `HPLUserModuleBridge`, `HPLPresentationBridge`, `HPLHudBridge` | `0x1401378e0`, `0x140129a40`, `0x1401ae870`, `mlId +0x158`, module `15`, action `12`, bounded current-ImGui capture | `ADDRESS_REGISTRY.md`, `FUTURE_SYSTEMS_RE.md`, `GHIDRA_SYNC.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Live-test inventory hold/fade, alpha/order, controller open/close, exact five-second expiry, non-inventory exclusion, and config rollback |
| `FEATURE.DIEGETIC_GUI_POINTER` | BUILT | `HPLTerminalBridge`, `HPLComfortBridge`, `HPLInputBridge`, `HPLPlayerState` | wall/handheld terminal states `8/9`, `0x1403132d0`, direct original `0x1402f0c90` dispatch, `0x140237920`, `0x1401562e0`, `0x140159360`, focused wrapper/entity `+0x180/+0x28`, tracked controller-origin ray with orientation-only fail-soft fallback, multi-frame native cancel hold | `ADDRESS_REGISTRY.md`, `FUTURE_SYSTEMS_RE.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Live-test beam/cursor agreement at center and edges, right-A and look-away state exit latency, physical lean, click behavior, state 9 preservation, and all rollback levels |
| `FEATURE.TERMINAL_OVERLAY` | BUILT | `HPLHudBridge`, `HPLTerminalBridge`, `HPLTerminalMath`, `HPLInputBridge`, `OpenGLHooks`, `OpenXRGLBridge`, `OpenXRRuntime` | state `8`, manager world owner `+0x170`, focused set `+0x180/+0x18`, logical `1024x577`/`880x560`, one `0x140213970` render into retained native-size target, source-viewport scissor remap with guarded disable fallback, upscale to HUD, HMD look-away native cancel, controller-origin ray intersection against the configured VIEW-space quad/cylinder including vertical offset, terminal-only opaque-alpha closure, 1.75x pointer | `FUTURE_SYSTEMS_RE.md`, `ADDRESS_REGISTRY.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Headset-accept pointer/GUI coordinate agreement, direct-versus-away content stability, clicks, page changes, look-away exit, state-9 preservation, cleanup, and capture cost |
| `FEATURE.HEAD_TRACKING` | PROVEN | `HPLCameraBridge`, `HPLCameraMath` | `0x140271b80`, `0x140270230` | `CURRENT_STATE.md`, `VR_COMPATIBILITY_RE.md` | Remain correct through every authored camera state |
| `FEATURE.AFR_STEREO` | PROVEN | `HPLCameraBridge`, `OpenXRRuntime`, `OpenXRGLBridge` | F11, per-eye cache and submitted render pose, fill-committed phase, eye-zero native camera/base and full OpenXR stereo-snapshot latch, absolute eye-one replay, 100 ms/frustum guard, transient apply retry, 12-frame last-pair hold | `BUILD_HISTORY.md`, `RUNTIME_ANALYSIS_0.5.1.md`, `REVIEW_HANDOVER_2026-08-07.md` | Headset-prove zero rendered pose-frame gap and balanced pair base/view latch-replay counters through walking, HMD rotation, loading and tracking interruption, with no vertical disparity or motion shear |
| `FEATURE.DUAL_RENDER` | BUILT | `HPLDualRenderControl`, `HPLCompatibilityProbe`, `HPLDualRenderMath`, `HPLDualRenderDiagnostics`, `HPLStatusPanelBridge`, `OpenXRRuntime` | `0x140298850`, `0x140298630`, immediate first-eye cache, opt-in continuous exact-player replay with GUI bit removed, cumulative replay draws/time and microseconds per 1,000 draws, fail-closed AFR rollback | `VR_COMPATIBILITY_RE.md`, `ADDRESS_REGISTRY.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Live-test draw-correlated CPU/GPU cost, post-post temporal behavior, shadows/reflections, HUD/presentation ownership, and panel on/off rollback before default promotion |
| `FEATURE.NATIVE_STEREO` | DESIGNED | future `HPLNativeStereo`, `HPLDualRenderControl`, `HPLCompatibilityProbe`, `OpenGLHooks` | viewport `0x140298850`, render-list `0x1402975e0`, HPL3 coherent-culling owner pending exact confirmation, GL query interception at the API boundary, pooled GL query IDs, source-proven two-slot CPU visible-node history pending HPL3 layout confirmation, per-eye refraction/disocclusion/silhouette work, partial scene-color copies, translucent camera UBO, render-section boundary `0x140298630`, presentation `0x1402a0590` | `NATIVE_STEREO_FEASIBILITY.md`, `ADDRESS_REGISTRY.md`, `VR_COMPATIBILITY_RE.md`, `HPL_OPENGL_NOTES.md` | Price replay cost against draw count and prove per-eye CPU visibility history, GPU queries, screen-space resources, culling, shadows/reflections, temporal ownership, HUD correctness, and immediate AFR rollback before promotion |
| `FEATURE.DUAL_RENDER_TEMPORAL_PROBE` | BUILT | `HPLDualRenderDiagnostics`, `HPLTemporalMutationMath`, `HPLCompatibilityProbe` | `0x1401f1480`, renderer/current/history/settings snapshots, `*(renderer+0x20)+0x158 -> *(renderer+0x438)+0x80` | `ADDRESS_REGISTRY.md`, `VR_COMPATIBILITY_RE.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Three automatic samples produce readable first/replay region summaries and correlated mutation ranges without crashes or visible state regression |
| `FEATURE.PER_EYE_VIEW_HISTORY` | BUILT | `HPLPerEyeViewHistory`, `HPLPerEyeViewHistoryMath`, `HPLCameraBridge`, `HPLCompatibilityProbe` | `0x1401f1480`, active frustum view `+0x158`, renderer history pointer `+0x438`, previous view `+0x80`, AFR/continuous pending/actual eye-pose transaction, calibration and stale-gap reseed | `ADDRESS_REGISTRY.md`, `VR_COMPATIBILITY_RE.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Live-test AFR and same-frame history status, authored-camera/recenter/loading/tracking reseed, alternating restore/capture, temporal image stability, performance, and hard rollback; shader/resource audit confirms no separate previous-projection or velocity history |
| `FEATURE.PER_EYE_IMAGE_TRAIL` | BUILT | `HPLPerEyePostEffect`, `HPLPerEyePostEffectMath`, `HPLCompatibilityProbe` | `0x1402d7a40`, `0x14038a8b0`, `0x14038a950`, `0x14038ae60`, ImageTrail `+0x50/+0x58/+0x98/+0xa0`, eye/pose/calibration bank, signature and lifecycle fallback | `ADDRESS_REGISTRY.md`, `FUTURE_SYSTEMS_RE.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Live-trigger ImageTrail; prove distinct left/right native resources, reset behavior, stereo-consistent trail, clean two-pair teardown, and one-line suppression rollback |
| `FEATURE.TONE_MAPPING_FRAME_OWNER` | BUILT | `HPLToneMappingFrame`, `HPLToneMappingFrameMath`, `HPLCompatibilityProbe` | `0x1402842d0`, `0x1402845d0`, `0x140284fd0`, ToneMapping `+0x8c/+0x94/+0xa0/+0xd8..+0x120/+0x138..+0x158`, same-pose baseline replay and single committed update | `ADDRESS_REGISTRY.md`, `FUTURE_SYSTEMS_RE.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Live-test dark/bright transitions, scripted fades, grading and film-grain changes, bloom, alternating eye order, AFR, same-frame stereo, mismatch telemetry, and config rollback |
| `FEATURE.PER_EYE_SSAO_HISTORY` | BUILT | `HPLSSAOTemporalHistory`, `HPLSSAOTemporalMath`, `HPLCompatibilityProbe`, `OpenGLHooks` | `0x1403f2b50`, `0x1402aba30`, renderer `+0xe78/+0xed8/+0xf40`, two GL history copies, eye/pose/calibration ownership | `ADDRESS_REGISTRY.md`, `FUTURE_SYSTEMS_RE.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Live-prove distinct histories, stable contact AO in both stereo modes, reset/reseed behavior, bounded allocations, clean shutdown, and config rollback |
| `FEATURE.SSAO_FRAME_OWNER` | BUILT | `HPLSSAOFrameOwner`, `HPLToneMappingFrameMath`, `HPLCompatibilityProbe` | `0x1403f2b50`, global phase `0x14079575c`, same-pose baseline replay and single committed update | `ADDRESS_REGISTRY.md`, `FUTURE_SYSTEMS_RE.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Live-test matching AO jitter in both eyes, AFR neutrality, same-frame replay/restore counters, mismatch telemetry, and independent config rollback |
| `FEATURE.VIEWPORT_OWNERSHIP` | BUILT | `HPLCompatibilityProbe`, `HPLCameraBridge`, `HPLPlayerState` | `0x140298630`, `0x140297f20`, `0x140297500`, viewport camera/world/renderer/post/FBO/settings fields | `ADDRESS_REGISTRY.md`, `VR_COMPATIBILITY_RE.md`, `TEST_CHECKLISTS.md` | Live logs classify reflection, terminal, loading, and save-transition viewports while only the exact player camera receives VR controls |
| `FEATURE.PER_EYE_CPU_TELEMETRY` | BUILT | `HPLCompatibilityProbe`, `HPLCameraBridge` | six guarded render-stage hooks, active AFR eye identity, QPC totals | `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md`, `VR_COMPATIBILITY_RE.md` | Capture representative left/right stage budgets alongside GPU timestamps |
| `FEATURE.PER_EYE_GPU_TELEMETRY` | BUILT | `HPLCompatibilityProbe`, `HPLCameraBridge` | six guarded render stages, nested `GL_TIMESTAMP` query pairs, bounded nonblocking pool | `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md`, `VR_COMPATIBILITY_RE.md` | Live capture shows nonzero left/right timings with no sustained dropped or invalid samples |
| `FEATURE.XR_DEPTH_CAPABILITY` | EXPERIMENTAL | `OpenXRRuntime`, `OpenXRGLBridge`, `OpenXRSceneDepth`, `OpenXRDepthMath`, `HPLCompatibilityProbe` | 0.96.0 replaces default-FBO depth with guarded pre-post scene-renderbuffer capture; serial/pose/viewport pairing, per-eye clip ranges and Ctrl+F10 PFM/JSON built; real GL tests pass; submission defaults off | `HPL_SCENE_DEPTH_PROOF_2026-09-09.md`, `NEXT_LIVE_EVIDENCE.md`, `TEST_CHECKLISTS.md` | Prove the live player callback supplies the scene attachment and color/depth/pose/clip agreement; allocation, synthetic GL tests and XR submission alone do not pass |
| `FEATURE.ARM_GOAL_CONSISTENCY` | BUILT | `HPLHandsBridge`, `HPLArmIKMath`, `HPLHandsMath` | 0.96.0 shares one calibrated target/camera snapshot and successful reachable endpoint; body-yaw, mirrored-offset, reachable/clamped and invalid-input tests pass; bounded hpl_wrist_goal telemetry | `PREYVR_ARM_IK_TRANSFER_2026-09-09.md`, `NEXT_LIVE_EVIDENCE.md` | Headset-check reach, wrist/forearm continuity, snap turns, locomotion and reload; inspect clamp/residual telemetry; forearm twist and per-eye lag are not claimed solved |
| `FEATURE.FIXED_FOVEATION` | BUILT | `OpenXRRuntime`, `OpenXRGLBridge`, config | `XR_FB_swapchain_update_state`, `XR_FB_foveation`, `XR_FB_foveation_configuration`, per-eye color swapchain create chains, shared configured and level-zero rollback profiles | `BUILD_HISTORY.md`, `USER_GUIDE.md`, `TEST_CHECKLISTS.md` | Live runtime reports all extensions/functions and `operational=1`; compare GPU time and peripheral image quality at levels 0..3, then retain only a measured useful default |
| `FEATURE.LOCOMOTION` | BUILT | `OpenXRInput`, `HPLInputBridge`, `HPLInputMath`, `HPLNativeLocomotion`, `HPLPhysicalCrouchMath`, `HPLPlayerState` | live-proven helper owner root `+0x110`, semantic Move type `1` at `0x140154fb0`, guarded body Move `0x1402375f0` during physical states `3..7/13`, native turn `0x140237460`, calibrated movement-controller direction, immediate snap-turn torso commit with one-tick stale-sample guard, delayed yaw-only physical follow composed from native body plus relative HMD yaw without camera mutation | `FUTURE_SYSTEMS_RE.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Live-confirm normal and interaction-state walking speed, controller-relative direction, same-tick snap-turn shoulder continuity, and 45-degree delayed physical follow with a rigid HMD view and no initial world-heading mismatch |
| `FEATURE.COMFORT_VIGNETTE` | BUILT | `HPLInputBridge`, `OpenXRComfortVignetteMath`, `OpenXRRuntime`, `OpenXRGLBridge`, `HPLStatusPanelBridge` | post-policy controller motion, stale-frame expiry, VIEW-space alpha quad and dedicated swapchain | `BUILD_HISTORY.md`, `FUTURE_SYSTEMS_RE.md`, `TEST_CHECKLISTS.md` | Live-test mask coverage, center clarity, movement/smooth-turn attack and release, pause/panel/loading suppression, F1 toggle, comfort presets, and teardown/recovery |
| `FEATURE.AUTHORED_CAMERA` | EXPERIMENTAL | `HPLPlayerState`, `HPLInputBridge`, `HPLCameraBridge`, `HPLComfortBridge` | camera rotate mode `+0x6c`, body camera ownership `+0x1e8`, exact player states `0..20`, same-camera baseline/history reseed, transition blackouts, semantic roll and optics wrappers | `VR_COMPATIBILITY_RE.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Live-test preserved tracking/stereo and native default-FOV restoration across sit, ladder/climb, conversation, animation, hand attachment, and death; then add only pose-composition policies required by evidence |
| `FEATURE.AUTHORED_STATE_COMPATIBILITY` | EXPERIMENTAL | `HPLPlayerState`, future state-policy adapter | exact state IDs `0..20`, character-body camera `+0x1b0`, update flag `+0x1e8`, camera rotate mode `+0x6c`, semantic and structural ownership telemetry | `AUTHORED_STATES_AND_VISIBLE_HANDS_RE.md`, `ADDRESS_REGISTRY.md`, `GHIDRA_SYNC.md` | Capture Sit, animation, ladder/climb, conversation, death, zoom, Null, and CustomControls transitions; then compose HMD pose over the correct native base without mutating sequence endpoints |
| `FEATURE.INTERACTION_RAY` | BUILT | `HPLInteractionBridge`, `HPLInteractionMath`, `HPLCameraBridge`, `HPLInputBridge`, `HPLGrabBridge` | `0x1400cd750`, inner raycast `0x1401438c0`, distance `+0x18`, body `+0x20`, entity `+0x28`, two local hand candidates, one outer finalizer, sticky acquisition plus state-lifetime owner lock, native `CanInteract` | `ADDRESS_REGISTRY.md`, `VR_COMPATIBILITY_RE.md`, `TEST_CHECKLISTS.md` | Live-test independent focus/activation, exact one-callback behavior, crossed-beam stability, and owner release/reacquisition across Slide/hinge/Grab/Read/terminal |
| `FEATURE.INTERACTION_RETICLE` | BUILT | `HPLInteractionBridge`, `HPLCrosshairBridge`, `HPLHudMath`, `HPLInputBridge`, `HPLHandsBridge`, `OpenXRRuntime`, `OpenXRGLBridge` | selected native aim/distance, simultaneous per-hand guides terminated by guarded `0x1400cd7d0` body rays, 5% idle/25% targeted radial glow, independent guide/semantic swapchains, `0x140484ea0` script dispatch, exact 35-state enum, 34 shipped TGA icons, application-space alpha quads | `COMFORT_AND_FOCUS_RE.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Live-test visual quality, scene termination, semantic icon hand switching/depth, overlap stability, no HUD center holes, layer limits, and terminal coexistence |
| `FEATURE.PHYSICS_HANDS` | BUILT | `HPLGrabBridge`, `HPLGrabMath`, `HPLTwoHandMath`, `HPLInputBridge`, `OpenXRInput` | PID `0x140238750`, body matrix `+0x50`, angular velocity vtable `+0x90`, unique-signature and suspended-thread AddImpulse patch `0x14049c720`, native-strength throw floor and forward clearance, unbounded pull-in plus bounded hand travel | `VR_COMPATIBILITY_RE.md`, `FUTURE_SYSTEMS_RE.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Live-test patch installation/restoration plus accepted hold regression, gentle/fast throw distance, no player recoil, placement, two-hand rotation, ownership, mass classes, and fallbacks |
| `FEATURE.VISIBLE_HANDS` | BUILT | `HPLHandsBridge`, `HPLHandsMath`, `HPLArmIKMath`, `HPLArmRenderDiagnostics`, `HPLInputBridge`, `HPLUserModuleBridge` | exact `PlayerHands_*` identity, validated shared `j_Root` plus bilateral 34-node restore and runtime hierarchy census, released DAE shirt/hand weight overlap, current `Arm_1`/`Arm_6` two-hinge prototype, exact medicine-authored root-lift rejection to the neutral seed, HMD-position anchor, native-plus-relative virtual torso yaw, geometric palm calibration, reach-gated clavicles, cross-product elbow pole with continuous lateral-singularity fallback and bounded swivel, native-seeded wake/retention, bounded first-eye/replay-eye node and validated deform-palette coherence witness, confirmed palette owner `0x1401fe1a0` and CPU-skin/VBO owner `0x140338900`, read-only module-18/script-object owner discovery at `0x140154c40`, physical states `0..8`, Read `10`, and MovingButton `13` | `AUTHORED_STATES_AND_VISIBLE_HANDS_RE.md`, `FUTURE_SYSTEMS_RE.md`, `ADDRESS_REGISTRY.md`, cross-engine `12-torso-calculations-and-ergonomics.md` | Live-classify locomotion-only eye lag as node-input mutation, deform-palette divergence, AFR eye/pose mismatch, or downstream CPU-skin/VBO consumption before changing IK; then accept body-aligned shoulder heading, neutral medicine height, elbow continuity, and no false root correction, and separately prove module-owner acquisition plus a safe script-owned invocation phase before calling `PlayerHands_SetVisible(true)` on maps with no native seed |
| `FEATURE.ENTITY_CALIBRATION_PROFILES` | BUILT | `HPLEntityCalibrationProfiles`, `HPLHandsBridge`, future profile consumers | exact live HPL entity name/capability, baseline-seeded cached snapshot, `somavr_entity_profiles.ini`, destroy-time identity eviction | `BIOSHOCK_VR_TRANSFER_AUDIT.md`, `FUTURE_SYSTEMS_RE.md`, `BUILD_HISTORY.md` | Live log proves exact hands/flashlight/story/medicine keys and clean-shutdown persistence; then promote one family at a time from telemetry-only to active calibration |
| `FEATURE.AUTHORED_PHYSICAL_INTERACTIONS` | BUILT | `HPLAuthoredInteractionBridge`, `HPLAuthoredInteractionMath`, per-profile native commit adapters | medicine `Tracer_Fluid_HudObject`, native `R_Hand` socket ownership, left cap proximity/action edge, HMD mouth proximity, bottle tip/hold state machine, queued profile events and haptics | `AUTHORED_STATES_AND_VISIBLE_HANDS_RE.md`, `FUTURE_SYSTEMS_RE.md`, `BUILD_HISTORY.md` | Calibrate medicine cap local offset/up axis in headset, then connect cap/drink events to a guarded native script adapter; add later interactions as named profiles |
| `FEATURE.PHYSICAL_MANIPULATION` | BUILT | `HPLGrabBridge`, `HPLGrabMath`, `HPLReadMath`, `HPLInputBridge`, `HPLInputMath`, `HPLInteractionBridge`, `OpenXRInput` | Wheel `3`, direct Slide `4` PID `0x140238750`, point-only Door `5`, twist-capable Lever `6`, Tear `7`, Read `10`, MovingButton `13`; helper-owner semantic Look at `0x140154fb0`; guarded interaction-state locomotion through body Move `0x1402375f0`; one-owner Read object with immediate non-recursive view-forward position and 45-frame authored-orientation settle | `FUTURE_SYSTEMS_RE.md`, `ADDRESS_REGISTRY.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Live-prove locomotion while holding mechanisms, immediate comfortable Read distance without rise/pop, Read hand continuity and grip rotation, and retain direct Slide only where the mechanism is truly state `4` |
| `FEATURE.VIEWMODEL` | BUILT | `HPLHandsBridge`, `HPLHandsMath`, `HPLTwoHandMath`, `HPLCameraBridge`, `HPLInputBridge` | interaction-owner world grip pose, support squeeze, `PlayerHandsHandler`, `0x14000fb60`, `0x1400bcd90`, `0x140127a70`, exact `HudObject`, socketed `*_HudObject`, destroy-time cache eviction | `FUTURE_SYSTEMS_RE.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Live-test left/right initiating-hand interaction objects, two-hand separation fallback, destroy/recreate identity, socketed-tool non-duplication, calibration, and native fallback; then add evidence-driven per-tool profiles |
| `FEATURE.FLASHLIGHT_ALIGNMENT` | BUILT | `HPLHandsBridge`, `HPLFlashlightMath`, `HPLCameraBridge` | exact `Flashlight` identity, `Player.hps::UpdateFlashLightLOS`, `0x14000fb60`, `0x1400bcd90`, `0x1400cd7d0`, `0x140143a10`, dominant aim pose | `FUTURE_SYSTEMS_RE.md`, `ADDRESS_REGISTRY.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Live-test visual beam calibration, agent response/gobo aim, cone preservation, tracking fallback, and verify tool/grounding rays remain native |
| `FEATURE.HUD_LAYER` | BUILT | `HPLHudBridge`, `HPLHudMath`, `OpenXRGLBridge`, `OpenXRRuntime` | `0x1400cc9b0`, `0x1400cca90`, `0x140071f20`, `0x140213970`, same-frame additive transparent capture, VIEW-space quad or `XR_KHR_composition_layer_cylinder`; packaged center clear disabled after clipping evidence | `FUTURE_SYSTEMS_RE.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Recheck interaction icon, subtitles, readable descriptions, overlay center, curved/quad switching, and exact pause/wake/dead/inventory current-ImGui owners with full native center preserved |
| `FEATURE.TERMINAL_LAYER_DIAGNOSTICS` | BUILT | `HPLHudBridge`, `OpenXRRuntime`, `OpenXRGLBridge`, `OpenGLHooks` | focused state-8 GUI, `0x140213970`, pre-compositor HUD capture FBO, automatic GL state trace, affine source/capture scissor mapping including incidental overlap, preserved empty clips and exact restore, `Ctrl+F10` RGB/alpha readback | `FUTURE_SYSTEMS_RE.md`, `TEST_REVIEW_2026-09-07.md`, `TEST_CHECKLISTS.md` | Headset-test complete email rendering across the animated source viewport; require coverage of the overlapping-source case, then compare capture cost |
| `FEATURE.SUBTITLE_PRESENTATION` | BUILT | `HPLSubtitleBridge`, `HPLSubtitleMath`, `HPLHudBridge` | `0x1401c8dd0`, `0x1401d3ba0`, `cLuxVoiceHandler +0x174/+0x178/+0x17c/+0x180`, native game HUD draw | `ADDRESS_REGISTRY.md`, `FUTURE_SYSTEMS_RE.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Live-test language, speaker names, gradual reveal, multiline wrapping, large-font mode, scale/Y calibration, immediate restoration, and HUD-layer placement |
| `FEATURE.DESKTOP_SPECTATOR` | BUILT | `OpenXRRuntime`, `OpenXRGLBridge`, `OpenXRSpectatorMath` | AFR eye caches, pre-SwapBuffers frame boundary, GL backbuffer blit | `BUILD_HISTORY.md`, `ARCHITECTURE.md`, `TEST_CHECKLISTS.md` | Live-test left/right eye identity, fit/fill/stretch, window modes, HUD expectations, and native rollback |
| `FEATURE.POST_EFFECT_POLICY` | BUILT | `HPLCompatibilityProbe`, `OpenGLHooks` | `0x14033b8f0`, `0x14033bd80`, priority tree `+0x328`, named vtables, active byte `+0x31`, exact VideoDistortion type | `FUTURE_SYSTEMS_RE.md`, `BUILD_HISTORY.md`, `RUNTIME_ANALYSIS_0.5.2.md` | Live-test suppression of ImageTrail, VideoDistortion, ChromaticAberration, and RadialBlur while fades/tone mapping remain intact |
| `FEATURE.POST_EFFECT_RESOURCES` | BUILT | `HPLCompatibilityProbe`, `OpenGLHooks`, `HPLPostEffectResourceMath` | `0x1402d7a40`, effect virtual `+0x68`, HPL input/output texture identity, GL texture target/ID/dimensions/format, framebuffer writes, same-pose eye signatures | `FUTURE_SYSTEMS_RE.md`, `ADDRESS_REGISTRY.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Run `Ctrl+F6` in representative tone/bloom/fade/video scenes; classify shared versus eye-distinct resources and use direct evidence to split histories or promote stateless effects |
| `FEATURE.SCREEN_MATERIAL_CONVERGENCE` | BUILT | `HPLScreenEffectBridge`, `HPLScreenEffectMath`, `HPLCameraBridge` | `0x14024a2f0`, `0x140252700`, `0x1402936c0`, `0x140291700`, exact `Screen Particle<decimal>` identity | `FUTURE_SYSTEMS_RE.md`, `ADDRESS_REGISTRY.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Live-test shipped screen effects at 1.5 m, F10 rollback, native timing/opacity, destruction pairing, and zero unrelated billboard changes |
| `FEATURE.SHADOW_STABILITY` | PROVEN | `HPLCameraBridge`, `HPLCameraMath` | fully centered projection, programs `942/944` | `VR_COMPATIBILITY_RE.md`, `RUNTIME_ANALYSIS_0.5.6.md` | Regression-test additional levels and light types |
| `FEATURE.REFLECTION_STABILITY` | PROVEN | `HPLCameraBridge`, `HPLCameraMath` | fully centered projection, program `985` redirect | `VR_COMPATIBILITY_RE.md`, `RUNTIME_ANALYSIS_0.5.6.md` | Regression-test additional reflective materials and levels |
| `FEATURE.AUDIO_LISTENER` | BUILT | `HPLCompatibilityProbe`, `HPLCameraBridge`, `HPLCameraMath` | `0x140289340`, world head offset, `0x14061d188`, `0x14048b2d4` | `VR_COMPATIBILITY_RE.md`, `BUILD_HISTORY.md` | Directional-source and near-field tests confirm orientation plus room-scale translation without world-lock or Doppler errors |
| `FEATURE.LOADING_VIDEO` | BUILT | `HPLPresentationBridge`, `OpenXRRuntime`, `HPLInputBridge`, `HPLCameraBridge` | `0x1400ccdb0`, game context `0x1407925e0`, `0x140488fa0`, `0x140488fd0`, AFR cache invalidation, opaque-black projection | `VR_COMPATIBILITY_RE.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Live-test save/map load entry and automatic stereo/input recovery with projection present throughout; use video lifecycle rows to classify fullscreen versus diegetic playback before adding any video override |
| `FEATURE.COMFORT_POLICY` | BUILT | `HPLComfortBridge`, `HPLComfortMath`, `HPLCameraBridge`, `OpenXRRuntime`, post policy, input | `0x140159360`, `0x140156d90`, `0x140156f00`, `0x140155210`, `0x140155230`, `0x140155250`, `0x140071f80`, vanished-thread-safe code writes, independent DoF/optics patch failure domains, semantic IDs and bounded black frames | `COMFORT_AND_FOCUS_RE.md`, both future RE documents | Live-test installed-lane telemetry plus add/roll/optics suppression, DoF/VideoDistortion policy, and transition guards across locomotion, zoom, terminals, climb, sit, camera animation, and death |
| `FEATURE.BUILD_IDENTITY` | BUILT | `BuildInfo`, `cmake/GenerateBuildInfo.cmake`, CMake build manifest, `scripts/Package-Release.ps1` | generated version/flavor/configuration/Git describe/commit/dirty state, PE timestamp/image size, environment fingerprint, DLL/package SHA-256 | `BUILD_HISTORY.md`, `SMOKE_TEST_MATRIX.md`, `BIOSHOCK_VR_TRANSFER_AUDIT.md` | Confirm the headset log identifies the exact dirty/clean source state and packaged release on every field run |
| `FEATURE.IN_PROCESS_CRASH_CAPTURE` | BUILT | `CrashHandler`, `CrashCapturePolicy`, `Logger`, existing external dumper | x64 exception context, fatal-only VEH, chained/re-armed top-level filter, rich minidump, repeat suppression, three-attempt cap, `SOMAVR_FULLDUMP` opt-in | `BIOSHOCK_VR_TRANSFER_AUDIT.md`, `BUILD_HISTORY.md`, `USER_GUIDE.md` | Child-process integration test remains green; any natural field crash produces one attributable dump without suppressing SOMA/Steam crash ownership |
| `FEATURE.RELEASE_LIFECYCLE` | BUILT | install/update/uninstall PowerShell scripts, installer lifecycle CTest | package SHA-256 ledger, literal deletion allowlist, non-authoritative `.somavr-install.json`, preserved `somavr.ini`, explicit custom-destination opt-in | `README.md`, `BUILD_HISTORY.md` | Test a real packaged update/uninstall and prove unknown files survive manifest tampering |
| `FEATURE.CONFLICT_DIAGNOSTICS` | BUILT | `src/injector/CompatibilityScan.cpp` | target modules, game-directory proxy DLLs, duplicate `somavr.dll` block | `README.md`, `TEST_CHECKLISTS.md` | Validate warning quality with ReShade/API-layer configurations and extend only from observed conflicts |
| `FEATURE.READINESS_DIAGNOSTICS` | BUILT | injector `--doctor`, `CompatibilityScan`, `ConfigManager`, `OpenXRRuntime` | x64 PE check, build flavor, loader/config, active runtime registry/JSON, 64-bit implicit/explicit API-layer census, loader-visible layer properties, XR lifecycle trigger counters, SOMA path, directory proxy scan, machine-readable exit | `USER_GUIDE.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Run from packaged install on each supported runtime; preserve zero-failure doctor output and lifecycle counters, treating a zero trigger count as unexercised rather than passed |
| `FEATURE.COMFORT_PRESETS` | BUILT | `ConfigPreset`, `ConfigManager`, INI | custom/minimal/balanced/maximum pre-scan, explicit-key precedence, deterministic tests | `USER_GUIDE.md`, `BUILD_HISTORY.md`, `TEST_CHECKLISTS.md` | Live compare all presets through turn, recenter, transitions, authored cameras, and named effects; tune only from evidence |
| `FEATURE.END_USER_GUIDE` | BUILT | `docs/USER_GUIDE.md`, release packager | install, doctor, launch, controls, presets, rollback, logs, troubleshooting | `README.md`, `BUILD_HISTORY.md` | Keep commands and defaults synchronized with every release package |
| `FEATURE.SMOKE_MATRIX` | BUILT | `docs/SMOKE_TEST_MATRIX.md`, `docs/NEXT_LIVE_EVIDENCE.md`, bounded subsystem summaries | 12 representative startup/gameplay/UI/transition/recovery/shutdown scenarios plus four multi-feature evidence passes | `SMOKE_TEST_MATRIX.md`, `TEST_CHECKLISTS.md`, `NEXT_LIVE_EVIDENCE.md` | Assign stable campaign saves/checkpoints and run the promotion subset on each feature build |
| `FEATURE.TELEMETRY` | PROVEN | `OpenGLHooks`, `OpenGLMatrixAnalysis`, `HPLCompatibilityProbe`, `HPLPlayerState`, `HPLInputBridge`, `HPLInteractionBridge`, `HPLHudBridge`, `OpenXRRuntime`, logger, config | Swap/FBO/matrix/runtime/stage/audio/player-state/post-effect bounded logs, eye-cache microtiming, controller transform, hit/semantic, GUI ownership, manipulation-session evidence | all current docs | Use the 0.60 live pass to correlate residual stereo latency, controller-relative direction, interaction payload/semantic ownership, Read/Zoom GUI ownership, and per-state manipulation scale |

## Dependency Edges

These relations are intentionally explicit Graphify seeds:

```text
FEATURE.INJECTION requires FEATURE.TELEMETRY
FEATURE.XR_BOOTSTRAP requires FEATURE.INJECTION
FEATURE.XR_SUBSTITUTE_RUNTIME tests FEATURE.XR_BOOTSTRAP
FEATURE.XR_SUBSTITUTE_RUNTIME tests FEATURE.XR_GL_SUBMISSION
FEATURE.XR_GL_SUBMISSION requires FEATURE.XR_BOOTSTRAP
FEATURE.XR_RESOURCE_RECREATION requires FEATURE.XR_GL_SUBMISSION
FEATURE.XR_FOCUS_PACING requires FEATURE.XR_GL_SUBMISSION
FEATURE.XR_FOCUS_PACING constrains FEATURE.XR_BOOTSTRAP
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
FEATURE.IN_PROCESS_CRASH_CAPTURE requires FEATURE.INJECTION
FEATURE.IN_PROCESS_CRASH_CAPTURE requires FEATURE.BUILD_IDENTITY
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
FEATURE.NATIVE_STEREO requires FEATURE.AFR_STEREO
FEATURE.NATIVE_STEREO requires FEATURE.VIEWPORT_OWNERSHIP
FEATURE.NATIVE_STEREO requires FEATURE.PER_EYE_GPU_TELEMETRY
FEATURE.NATIVE_STEREO requires FEATURE.PER_EYE_VIEW_HISTORY
FEATURE.NATIVE_STEREO constrains FEATURE.DUAL_RENDER
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
FEATURE.ENTITY_CALIBRATION_PROFILES requires FEATURE.VISIBLE_HANDS
FEATURE.ENTITY_CALIBRATION_PROFILES requires FEATURE.VIEWMODEL
FEATURE.ENTITY_CALIBRATION_PROFILES supports FEATURE.AUTHORED_PHYSICAL_INTERACTIONS
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
