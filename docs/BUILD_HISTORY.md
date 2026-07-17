# Build History

## 2026-07-17

### 0.65.1-object-rotation-fix

- Reverted the Read-object distance mutation after live evidence showed that
  caching the first transition frame forced SOMA's later entrance matrices onto
  an incorrect distant arc. Read objects now retain native pickup translation
  and timing while the configured apparent scale remains independent.
- Replaced Read's two-axis synthetic mouse rotation with direct tracked
  controller orientation at the guarded state-10 `SetMatrix` boundary. Grip
  rotation now supports unrestricted pitch, yaw, and roll, persists on release,
  and no longer feeds the script's `+/-pi/8` pitch clamp or shifts the camera.
- Replaced Grab's additive angular correction after the log proved SOMA and the
  mod saturated at opposite `+/-6 rad/s` targets. The new route anchors the
  selected body's `+0x50` world matrix to the initiating grip, reads actual
  angular velocity through virtual slot `+0x90`, and supplies absolute
  controller-relative target velocity minus body velocity to the existing
  `40/0/0.4|0.1` torque PID. One- and two-hand mode changes re-anchor before
  control, while invalid body, matrix, input, or ABI dependencies preserve the
  native error.
- Added quaternion basis extraction, relative object-orientation math, and
  deterministic coverage for matrix round trips, native Read travel, full-axis
  Read rotation, and absolute Grab targets. All four CTest suites pass; stable
  package doctor reports `pass=8 warn=0 fail=0`. OpenXR DLL SHA-256:
  `72CFF4F747732B9446B1192EB3E9485B3D2F5337A7C59489EBC1C985B6DA15F2`.
  Stable package SHA-256:
  `9B997A48150B48A813A7CA2B4676287488665820132BC5CB4E0C635F28BDFAE6`.
  Headset acceptance remains required for Read entrance/size/orientation and
  loose-prop one/two-hand stability.

### 0.65.0-physical-interaction-polish

- Fixed diegetic terminal cursor delivery. The previous build projected the
  controller ray correctly but waited for native mouse movement before SOMA's
  `cImGui` received the new virtual position. The bridge now dispatches each
  mesh-ray hit directly through the original `SendMouseVirtualPosition` path,
  preserving native widgets, clicks, sounds, and callbacks.
- Added interaction-owner locking for native Grab, Push, Wheel, Slide, Door,
  Lever, Tear, and Read states. Either beam can acquire an object, but the
  initiating hand remains the sole physical owner until SOMA leaves that state.
- Extended Door/Lever hinge control with controller angular velocity projected
  onto the native joint pin. Wrist twist and the existing hand-translation arc
  are combined before the native torque PID and speed limit.
- Added a bounded gravity-glove-style Grab start. The selected hit point is
  pulled toward the initiating grip through SOMA's existing PID, collision, and
  throw route; normal controller deltas continue from that hand after capture.
- Fixed Read cancel by holding native right mouse while right A or B is held,
  added grip hysteresis to Read rotation, and introduced a guarded Read-prop
  transform that doubles initial camera distance and apparent scale without
  moving the player camera.
- Added deterministic unit coverage for hinge wrist projection and Read
  presentation scale/distance. All four CTest suites pass. Stable package doctor
  reports `pass=8 warn=0 fail=0`. OpenXR DLL SHA-256:
  `DE19C43FF3A3117A2F03DF28049269EE9892D43CA6917FFB7EA1761D68340FBC`.
  Stable package SHA-256:
  `DCD90185D179B094A41D8FC7C13961E3382B85D6FB7E18D3EA818FAB5493CAB1`.
  Headset acceptance remains required for terminal pointer delivery, object
  presentation, hinge direction, pull strength, and ownership continuity.

### 0.64.1-dual-hand-interaction-fix

- Fixed the 0.64 startup regression that left both visual guides active while
  disabling controller interaction, native hit snapshots, and semantic icons.
  The latest log showed `hpl_interaction_bridge install_failed` followed by zero
  reticle updates and repeated semantic `no_hit_snapshot` rejects.
- Direct PE-byte verification against the installed `Soma_NoSteam.exe` found the
  inner raycast entry begins `40 57 48 83 ec 60`, not `57 48 83 ec 60`. The
  missing REX prefix also shifted the RIP displacement from `+8/+12` to the
  correct displacement/next-instruction offsets `+9/+13`.
- Split outer and inner signature diagnostics so future failures report the
  correct RVA plus actual and expected byte prefixes. Added compile-time checks
  tying the RIP offsets to the guarded `48 8b 05` instruction and startup config
  telemetry for `InteractionBothHands`.
- Moved both byte contracts into `SomaBuildSignatures.h`, shared by the DLL and
  injector. Doctor now maps the installed PE sections and blocks launch when
  either interaction RVA differs; its regression test explicitly rejects the
  missing-prefix 0.64 signature.
- All four CTest suites pass. Stable package doctor reports
  `pass=8 warn=0 fail=0`. OpenXR DLL SHA-256:
  `0CDA05F1FB17EFDE574AC8BF6BAFE68A4C5A158C1C2BC42A13435B245E5ECAEA`.
  Stable package SHA-256:
  `BDA977BD2C0639845F3CC00D9BE685E341F4BCD3D96FF9C34E8C0095FD1F407C`.

## 2026-07-16

### 0.64.0-dual-hand-interaction

- Added simultaneous left- and right-controller native interaction probes. The
  bridge calls `SOMA_Lux_GetClosestEntityRaycast` once per tracked hand, chooses
  one result deterministically, writes only that result to SOMA's outer closest-
  entity object, and invokes its native vtable `+0x40` finalizer exactly once.
  Native range, LOS, `CanInteract`, focus, callbacks, and player states remain
  authoritative.
- Added sticky focus arbitration: an exclusively pressed hand wins immediately,
  a unique native hit wins while pointing, overlapping hits retain the previous
  owner, and the configured dominant hand breaks an unresolved tie. The selected
  owner now follows native Slide, hinge, Grab, Read, terminal click, and contact-
  haptic routes instead of snapping back to the configured hand.
- Added simultaneous left/right application-space aim guides. The selected hit
  clips its guide to native hit depth, and SOMA's semantic context icon is placed
  at that selected beam endpoint. A dedicated guide swapchain allows both beams
  and the native context icon to coexist without overwriting one another.
- Added `InteractionBothHands=1` with a single-hand rollback, per-hand probe/hit/
  selection telemetry, deterministic selector tests, and dual-hand live checks.
  Ghidra comments for `0x1400cd750` and `0x1401438c0` were synchronized and the
  `Soma_NoSteam.exe` database was saved.
- The OpenXR Release tree passes all four CTest suites. Stable package doctor
  reports `pass=7 warn=0 fail=0`. OpenXR DLL SHA-256:
  `B91BF2243328BF7F5A7007FC9AF97E20B74DA0A94BDA6B32C3F5B36F25B5AAE4`.
  Stable package SHA-256:
  `962ED5068871DE2950BAE3486281725B2C60DFB7F0D7F9A60BB6382F7860D4A0`.

### 0.63.0-diegetic-terminals

- Added a diegetic wall-terminal mode for exact player state `8`. While F10 VR
  tracking is active, SOMAVR suppresses the terminal script's body teleport,
  `RotateCameraTowards` request, and Terminal camera-position add. The terminal
  remains at its authored world position and the player can physically lean in.
- Replaced guessed head-relative terminal coordinates with SOMA's own spatial
  GUI projector at `0x1403132d0`. A tracked controller world ray intersects the
  actual GUI mesh, uses native triangle UVs, and feeds exact virtual coordinates
  to the existing ImGui cursor path. A mesh miss does not click through; the old
  head-cone route is retained only when the native spatial owner is unavailable.
- Extended the visible controller guide to the terminal ray length while states
  `8/9` own input. Trigger/select, focus, sounds, widgets, callbacks, and native
  terminal exit remain engine-owned. Handheld terminal state `9` keeps its
  authored presentation and can share the exact spatial pointer.
- Added independent rollback keys: `TerminalDiegetic=0`,
  `TerminalRayPointer=0`, and `TerminalPointer=0`. New counters distinguish
  mesh hits/misses/unavailable owners and each suppressed takeover call.
- Changed release packaging to the stable `out\SOMAVR-latest` folder and ZIP.
  Successful default packaging prunes superseded SOMAVR package artifacts;
  `-Versioned` is now an explicit archival option.
- Both default and OpenXR Release trees pass all four CTest suites. Stable
  package doctor reports `pass=7 warn=0 fail=0`. OpenXR DLL SHA-256:
  `0FF9F583F4B3BD41F85B71BCF3425839410AF610195E1265C1AF5CF2354DAF13`.
  Stable package SHA-256:
  `8F487A256D9770A22CC6D333816CACD8D3344FA61D25690BB91488FD9304CCBC`.

## 2026-07-15

### 0.62.0-physical-hinges

- Corrected a controller-velocity transform bug exposed by the 0.61 live log.
  `ResolveHPLReferenceVectorWorld` normalized velocity as though it were an aim
  direction, so slow and fast hand motion both reached Slide as unit speed.
  Velocity and throw vectors now retain magnitude across the HPL transform.
- Added native SwingDoor/Lever hinge control. Exact player states `5/6` and
  torque PID `10/0/1` resolve selected-body joint 0, read its native pin at
  `+0xe8` and pivot at `+0xf4`, then convert the controller's world-space arc
  into signed angular velocity. SOMA retains PID integration, joint limits,
  physics, sounds, callbacks, and release lifecycle. The old camera-relative
  synthetic mouse route is excluded while direct hinge control owns the state.
- Reduced instrumentation-induced stutter. Normal log rows flush in bounded
  batches, while warnings/errors still flush immediately. The active test
  profile disables render-stage, replay, per-eye CPU/GPU, post-resource, and
  uniform probes after the required evidence was captured.
- Corrected stereo timing diagnostics to compare only same-frame eye pairs.
  The 0.61 log measured 194 valid pairs at 2.61 ms average, 4.36 ms p95, and
  12.29 ms maximum, with zero pose-frame gap. Prior-frame comparisons averaged
  57.76 ms and were expected scheduling differences, not same-frame stalls.
- Added signed hinge-arc, axial-motion rejection, degenerate-radius, and speed-
  clamp tests. Both default and OpenXR Release trees pass all four CTest suites.
  The packaged doctor reports `pass=7 warn=0 fail=0`. OpenXR DLL SHA-256:
  `C4E74A5E090FC1FBE3275BAFAED28D770F58B4E0F3DAAA633DB95E31817B035C`.
  Package SHA-256:
  `2BD270C82919A24870F5B5EF7EB82F1B1DC3C2AB414DCDDB1FE030A448091F8B`.

### 0.61.0-native-manipulation

- Corrected the closest-entity result ABI from live evidence and native
  decompilation: distance is `+0x18`, physics body `+0x20`, and Lux entity
  `+0x28`. Controller reticle depth, semantic focus, and selected-body identity
  now consume the actual fields instead of rejecting pointer bits as distance.
- Added native Slide joint control. The existing PID hook recognizes Slide's
  exact `6/0/0.1` force tuple, resolves selected body joint 0, reads its pin at
  `+0xe8`, and projects dominant-controller world velocity onto that pin. SOMA
  retains its PID, force limits, constraints, gravity, sounds, callbacks, and
  state lifecycle. This is the first camera-independent drawer/curtain route.
- Read rotation now uses incremental controller orientation rather than hand
  translation. Dominant A or B sends native inspection cancel, and snap/smooth
  turn is suppressed outside Normal state so it cannot rotate readables or
  mechanisms and cannot request an unrelated blackout.
- Added active-camera native pitch suppression at frustum evaluation while VR
  owns the camera. Native pitch state is restored immediately after evaluation;
  visual pitch comes from the HMD, while yaw/body ownership remains unchanged.
- Matched the HUD target to the observed `1920x1080` SOMA backbuffer and added
  bounded unique near-camera Read-entity matrix diagnostics for identifying the
  separate open-prop owner required by controller-attached inspection.
- Runtime evidence confirms Quest requests `2688x2880` per eye at one sample,
  while SOMA currently supplies a `1920x1080` eye render that is upscaled. A
  native/offscreen higher-resolution eye target remains the image-quality task;
  increasing only the OpenXR swapchain scale cannot recover source detail.
- The active SOMA profile reports `EdgeSmooth="false"`; the game exposes FXAA
  rather than a multisample source path. Source anti-aliasing was therefore off
  for this capture and should be tested independently from OpenXR sample count.
- Both default and OpenXR Release trees pass all four CTest suites. The packaged
  doctor reports `pass=7 warn=0 fail=0`. OpenXR DLL SHA-256:
  `40FDF5CE3038DFDD0217D6F4C8F15D576AB20BC70B09C754ED2ED71F491E5362`.
  Package SHA-256:
  `6BA58A782C6387E50168B8744A40C01DC95E6596639CC5E5A505DB20032F870B`.

### 0.60.0-evidence-capture

- Added behavior-neutral stereo timing evidence. OpenXR now records the
  microsecond delta between eye-cache captures, same-frame pair counts, samples
  over 20 ms, and each cached eye's age at XR submission alongside pose-frame
  gaps. This separates render sequencing from stale-pose/cache latency.
- Added bounded controller-relative locomotion records containing raw stick,
  transformed/deadzoned axes, movement-controller source, calibrated head and
  controller quaternions, native camera basis, and explicit reference fallback
  counters. Mouse/snap-yaw reports can now be diagnosed from one normal run.
- Interaction diagnostics now log every hit/no-hit/payload-validity transition,
  malformed closest-entity payload reasons, and native semantic-state changes
  with the correlated entity/body/distance/world-hit snapshot.
- Read and Zoom current-ImGui ownership transitions are now explicit in GUI-set
  telemetry without broadening HUD capture. Each physical manipulation session
  emits one exit summary with signed/absolute hand travel, peak frame motion,
  emitted mouse pixels, state, hand, and exit reason.
- No native addresses, rendering policy, controls, or gameplay behavior changed.
  This build exists to make the next headset pass unlock interaction, readable
  presentation, manipulation tuning, locomotion, and stereo follow-up work.
- Both default and OpenXR Release trees pass all four CTest suites. The packaged
  doctor reports `pass=7 warn=0 fail=0`. OpenXR DLL SHA-256:
  `C56C62D3702DADC9C25649E362B3B4F8E2659A506EF4E622A486DF49268671F0`.
  Package SHA-256:
  `AFC37537578FC7D1696636A0F8DB6FFD7DB04C32F8F9EC80A68CD13B7D201CC7`.

### 0.59.0-live-usability

- Promoted continuous same-frame stereo in the packaged test profile after the
  first full controller/HUD playtest found that it removed visible inter-eye
  latency without reintroducing shader, shadow, reflection, tracking, or eye-
  height regressions. OpenXR now measures rendered left/right pose-frame gaps
  directly and reports latest, maximum, nonzero, and sample counts.
- Added calibrated left-controller-relative locomotion. Raw controller aim is
  transformed through the same neutral frame used by HMD tracking before it
  reaches SOMA's native analog body movement, so pitch is ignored and virtual
  body yaw remains the sole world-space owner.
- Added a temporary three-point cyan OpenXR aim guide. It reuses the semantic
  reticle swapchain, respects the runtime's reported layer limit, disappears in
  menus/authored ownership, and yields to a valid native interaction reticle.
- Removed the packaged 96x96 center-HUD clear after live evidence proved it was
  clipping SOMA's interaction icon and producing the missing center square.
  The config rollback remains. Subtitle test tuning returns to native font scale
  with wider margins while exact voice-subtitle calls remain separately logged.
- Added a dedicated 2700 px/m Slide-state scale, dominant-grip Read rotation,
  and dominant-secondary native cancel for Read, Zoom, and terminal ownership.
  Other physical manipulation states retain the accepted 900 px/m scale.
- Increased test-profile vignette angular coverage to about 127 degrees,
  reduced the clear-center radius, increased strength, and added exact geometry
  telemetry. Tone-mapping mismatch logging now identifies field groups and is
  bounded to first/every-300 evidence instead of flooding successful runs.
- Both default and OpenXR Release trees pass all four CTest suites. The packaged
  doctor reports `pass=7 warn=0 fail=0`. OpenXR DLL SHA-256:
  `5C382B86E7582489186A6A57ADE6620D6101595323483DCC30431ECB2CAF4811`.
  Package SHA-256:
  `9279D414C948C7E7B89C8AB7A7433CDAC15C05F11793A3437212E526A7A5428B`.

### 0.58.0-grab-contact-haptics

- Confirmed HPL3's native physics-contact path from Newton update
  `0x1405548b0` to surface impact `0x14032f0e0` and slide `0x14032f380`.
  Released HPL2 source independently matches the function signatures, material-
  priority dispatch, contact record, and impact/scrape semantics.
- Added exact-signature `HPLContactHapticsBridge`. SOMA's impact handler always
  runs unchanged; Grab state, authored ownership, input age, dominant-grip
  tracking, and contact proximity gate one-hand OpenXR feedback afterward.
- Added bounded speed-to-amplitude mapping, configurable distance/speed/
  amplitude/duration controls, duplicate-material cooldown, per-reason summary
  counters, hard rollback, and deterministic math/config tests. Generated
  configs remain off while the active test profile enables the feature.
- Renamed, typed, tagged, documented, and saved all three native functions in
  the `Soma_NoSteam.exe` Ghidra database. Added a dedicated RE note and updated
  traceability, architecture, live-evidence, user-guide, and test ledgers.
- Both default and OpenXR Release trees pass all four CTest suites. The packaged
  doctor reports `pass=7 warn=0 fail=0`. OpenXR DLL SHA-256:
  `90487A3BCC49240EDCBC716B0EF2F17016E773BA301B2FC07E79CA3E8B7AD9BC`.
  Package SHA-256:
  `4A98C516B1C618EF4789EB8562811F8F3B0AEE533DA08AD1FB9A477D9326C7AA`.

### 0.57.0-fixed-foveation

- Added opt-in OpenXR fixed foveation with no SOMA render mutation. Instance
  creation requires the complete `XR_FB_swapchain_update_state`,
  `XR_FB_foveation`, and `XR_FB_foveation_configuration` extension set and
  resolves profile create/destroy plus swapchain-update entry points explicitly.
- Eye color swapchains advertise foveation capability. A shared configurable
  level/dynamic/vertical-offset profile is applied to both eyes after resource
  creation and reapplied after GL/view/session resource recreation.
- Added a pre-created level-zero profile as transactional rollback. Any profile
  creation or per-eye update failure retains ordinary frame submission and
  attempts to neutralize both eyes instead of failing OpenXR startup.
- Added generated-off configuration, active-profile capability probing, bounded
  extension/profile/application/failure telemetry, summary fields, and parsing
  tests for clamped level and vertical-offset controls.
- Both default and OpenXR Release trees pass all four CTest suites. The packaged
  doctor reports `pass=7 warn=0 fail=0`. OpenXR DLL SHA-256:
  `2C1F395913DC8F09220C6675454C760229C9E6A2512591FC8C8607C835E526E8`.
  Package SHA-256:
  `4069B5B1993D3750EC21CAF17EF81CD2AC52FE15FC3D9B4DE189FCC2F8D3F133`.

### 0.56.0-authored-camera-handoff

- Added an explicit same-camera ownership handoff for SOMA transitions that
  switch camera rotate mode or disable character-body camera updates. Tracking
  and stereo remain active while the native matrix baseline, eye schedule,
  room-scale safety cache, and all calibration-generation temporal histories
  are invalidated and reseeded on the next native frustum query.
- Extended the existing state-transition comfort policy to detect authored
  ownership changes even when the player state ID does not change. It requests
  one bounded blackout, suppresses duplicate requests when ownership and state
  change together, and records independent ownership-transition counters.
- Added deterministic ownership-transition policy tests and bounded camera/
  input telemetry. Camera pointer replacement deliberately retains the stronger
  cache invalidation and activation re-arm behavior.
- Completed a shipped shader and allocator audit: temporal SSAO uses the current
  eye projection plus the already isolated previous-view matrix. No independent
  previous-projection or render-velocity history exists in SOMA's render path;
  local-reflection and bloom targets are confirmed same-pass scratch.
- Both default and OpenXR Release trees pass all four CTest suites. The packaged
  doctor reports `pass=7 warn=0 fail=0`. OpenXR DLL SHA-256:
  `8D0022196A3A649712BC67B429B98C57174AF207FD00314BF58C9932AAC86CB2`.
  Package SHA-256:
  `326BD6533497387C633DAF5ED793E8A2DC8007D91CA3EBD53E0FDE1F2D876B9C`.

### 0.55.0-ssao-frame-owner

- Confirmed that `HPL3_RendererDeferred_RenderSSAO` advances global float
  `0x14079575c` by renderer frame time and derives temporal blur uniform `7`
  from it on every invocation. Same-frame stereo therefore gave eye two a
  different SSAO jitter phase even after the history texture was isolated.
- Added separately reversible `HPLSSAOFrameOwner`. For an eligible same-pose
  pair it captures the first-eye phase baseline and committed result, restores
  the baseline before eye two, then restores the first committed result after
  eye two. AFR and mono/native ownership remain unchanged; invalid memory or
  sequencing faults closed to native per-eye advancement with bounded summary
  and mismatch telemetry.
- Reclassified local-reflection texture `renderer+0xeb0` and framebuffer
  `+0xf00` as feedback-avoidance scratch: `0x1403f40d0` fully overwrites the
  copy from the current accumulation input before final composition. No
  per-eye allocation was added. Generated config remains off; the active test
  profile enables `HPLSSAOFrameOwnerControl=1`.
- Both default and OpenXR Release trees pass all four CTest suites. The packaged
  doctor reports `pass=7 warn=0 fail=0`. OpenXR DLL SHA-256:
  `617736BED59D1843C28E07331006C1ABEC1E510862832CCEA77ADB0C9DE27616`.

### 0.54.0-per-eye-ssao-history

- Confirmed SOMA's complete temporal SSAO ownership chain. Deferred renderer
  `0x1403f2b50` samples previous AO texture `renderer+0xe78` through temporal
  shader `+0xf40`, then overwrites the same texture through framebuffer
  `+0xed8` on every eye render. Allocator/destructor `0x1403f4530` /
  `0x1403f2880` prove it is a persistent owned history, while the other SSAO
  targets are pass scratch.
- Added signature-guarded `HPLSSAOTemporalHistory`. During the exact player
  stereo path it correlates native `+0xe78` with its GL texture at renderer
  texture-unit boundary `0x1402aba30`, allocates two matching GPU histories,
  restores the current eye before native SSAO, and commits the result after
  native SSAO with `glCopyImageSubData`. First observation seeds both eyes;
  calibration, resource, size, format, or context changes invalidate history.
- Generated configs remain off and the active test profile enables
  `HPLPerEyeSSAOTemporalControl=1`. Unsupported GL copy/storage entry points,
  non-2D targets, signature drift, or copy/allocation failures fault closed to
  native shared history. Added deterministic schedule/reset tests and bounded
  allocation/restore/commit/failure telemetry.
- Both default and OpenXR Release trees pass all four CTest suites. Ghidra
  confirms each hook signature has one exact match at its registered address,
  and the packaged doctor reports `pass=7 warn=0 fail=0`. OpenXR DLL SHA-256:
  `623A448B41719BD0B42689C0261383E7CF503668BB484B3CF195E910101D7CAA`.

### 0.53.0-tone-mapping-frame-owner

- Confirmed that `HPL3_PostEffect_ToneMapping_RenderEffect` at
  `0x140284fd0` calls `0x1402842d0` once per render to advance exposure,
  white-cut, window offsets, authored fade state, and color-grading transition
  state. Same-frame stereo therefore advanced this shared packet twice.
- Added `HPLToneMappingFrame` and tested `HPLToneMappingFrameMath`. At the
  existing exact `RenderOne` boundary, the first eye captures the pre-update
  packet and native committed result; the opposite eye replays the same
  baseline, then the first committed result is restored so only one update
  persists. AFR and non-player viewports remain native.
- Added guarded reads/writes for the confirmed `+0x8c/+0x94`, `+0xa0`, and
  `+0xd8..+0x120` packet. The same owner includes confirmed film-grain current/
  next sample offsets and phase at `+0x138..+0x158`, making grain sampling
  stereo-consistent and preventing double-speed same-frame advancement. Added
  deterministic eye-order/duplicate/release tests, bounded replay/mismatch
  telemetry, and a one-line config rollback. Generated configs remain off; the
  active test profile enables the control.
- Classified ToneMapping's six bloom framebuffer/texture pairs as sequential
  scratch resources that are fully rewritten by the bright/blur passes, not
  temporal histories requiring per-eye duplication. Both default and OpenXR
  Release trees pass all four CTest suites. The packaged doctor reports
  `pass=7 warn=0 fail=0`. OpenXR DLL SHA-256:
  `942523833EAA7F37ABCEC5EC6025ED2B1771413B9C30008023E0AA910F3E9EC7`.
  Package SHA-256:
  `5D02FFAF5003BA11392DD68E40E9EE6F99AB16DF3FBD09469B2A82A1B9675E62`.

### 0.52.0-per-eye-image-trail

- Promoted the first post-effect temporal resource from generic probing to a
  guarded per-eye implementation. Ghidra plus released HPL2 source confirm
  ImageTrail framebuffer `effect+0x50`, accumulation texture `+0x58`, amount
  `+0x98`, one-shot clear flag `+0xa0`, render virtual `0x14038a950`, resource
  creation `0x14038ae60`, and destruction `0x14038a8b0`.
- Added separate `HPLPerEyePostEffect` runtime and tested
  `HPLPerEyePostEffectMath` ownership modules. The runtime lazily allocates a
  second native texture/framebuffer pair on the exact render thread, switches
  pair plus clear state by actual eye/pose, and clears both histories after
  recenter, calibration changes, stale gaps, or non-stereo use.
- Added signature-guarded two-pair destruction and pre-graphics-shutdown
  cleanup. Every unknown signature, unreadable field, shared resource alias,
  or eye/pose mismatch faults closed to the existing named ImageTrail
  suppression. Generated configs remain conservative; the active test profile
  enables isolated history and disables only the old ImageTrail suppression.
- Added deterministic distinct-resource, eye-selection, clear-state,
  recalibration, and alias-rejection tests. Both default and OpenXR Release
  trees pass all four CTest suites. The packaged doctor reports
  `pass=7 warn=0 fail=0`. OpenXR DLL SHA-256:
  `E253CC071969AE20E983B9786A6190E8E1926B9CED79AA77A629892666D39773`.
  Package SHA-256:
  `241A83472E86F780903D076747FE30CD106638EA12E5EC8F43DC353C4061D4ED`.

### 0.51.0-comfort-vignette

- Added an optional locomotion comfort vignette as a dedicated head-locked
  OpenXR alpha layer. A tested radial mask keeps the center transparent and
  scales peripheral opacity from movement intensity through a configurable
  attack/release envelope.
- Motion ownership comes from `HPLInputBridge` after loading, pause, status
  panel, terminal, dead-state, and authored-camera policy. Stale input expires
  automatically. Smooth-turn input can contribute when smooth turning is in
  use; snap turning retains its existing bounded black-frame guard without a
  stick-held vignette.
- Added a dedicated swapchain with complete session/context teardown, generated
  config controls, balanced/maximum preset integration, runtime summary/frame
  telemetry, and `COMFORT VIGNETTE: ON/OFF` as the ninth F1 action. Generated
  configs remain off; the active test profile enables the reversible path.
- Added deterministic deadzone, smooth-turn, envelope, radial-alpha, invalid-
  target, config-precedence, preset, and panel action tests. Both default and
  OpenXR Release trees pass all four CTest suites. The packaged doctor reports
  `pass=7 warn=0 fail=0`. OpenXR DLL SHA-256:
  `D4951463AD5ECE90BA12BDD94088803AED75BB5845487EFBE911ED1B239149F6`.
  Package SHA-256:
  `4D3D4DECD6503BB48A13F14C131792A2608064E339241D481A4E976F67DFCBAB`.

### 0.50.0-curved-hud

- Added optional `XR_KHR_composition_layer_cylinder` HUD submission. The runtime
  enumerates and enables the instance extension when the configured HUD can use
  it, while unsupported runtimes retain the existing alpha quad automatically.
- Added tested physical geometry derived from the existing HUD width, texture
  aspect, configured center distance, and `HudCylinderAngleDegrees`. Radius is
  arc width divided by angle; the cylinder axis is offset by that radius so the
  center of the visible surface remains at the same distance as the flat HUD.
  This follows the proven UEVR overlay approach and the Khronos cylinder-layer
  height/aspect contract.
- Added `HUD SHAPE: CURVED/QUAD` to the F1 panel. Shape changes are immediate
  and do not rebuild the HUD swapchain. An unavailable extension reports
  `QUAD ONLY`; an `XR_ERROR_LAYER_INVALID` or validation rejection disables
  cylinder submission and returns to the quad on the following frame. The
  active test profile requests a 70-degree curve; generated configs remain on
  `quad`.
- Added configuration/parser, cylinder geometry, physical aspect, invalid-angle,
  and eight-action panel tests. Both default and OpenXR Release trees pass all
  four CTest suites. The packaged doctor reports `pass=7 warn=0 fail=0`.
  OpenXR DLL SHA-256:
  `3B123116D1E18BE4E1C5F3A0A1E59FDC3279305B8588AD98B4B915E2447D72D5`.
  Package SHA-256:
  `938BE2A691C5BA2ED57027809554D5C28EAEBDF39E1A9EA501903D9670381F4C`.

### 0.49.0-readiness-presets

- Added `[Comfort] Preset=custom|minimal|balanced|maximum`. The loader pre-scans
  only the preset, applies established comfort-owned defaults, rewinds, and then
  parses the complete INI normally. Explicit settings therefore always win.
  Presets cover snap/smooth turning, bounded transition/recovery black frames,
  semantic camera add/roll/optics policy, screen-effect distance, and named
  incompatible post effects without changing scale, height, hands, locomotion
  direction, stereo mode, or experimental feature controls.
- Added `somavr_injector --doctor [game] [dll]`. Without launching or injecting,
  it checks x64 PE identity, OpenXR build metadata, loader/config presence,
  probe/session/submission config, active 64-bit OpenXR runtime registration and
  JSON existence, optional SOMA executable identity, and known local proxies.
  Developer work-root config is an explicit warning; packaged missing config is
  a failure. The actual machine passes seven checks with one expected developer
  warning and resolves Virtual Desktop's runtime JSON.
- Added parser/precedence and directory-scan tests, bringing both builds to four
  deterministic CTest suites. Added the checksum-packaged `USER_GUIDE.md` for
  install, doctor, launch, controls, presets, rollback, and troubleshooting.
  The packaged doctor reports `pass=7 warn=0 fail=0`. OpenXR DLL SHA-256:
  `05580E757FF353E6B0C5EE9A664E93671832FDBB73706CE82205C0CA91DD5733`.
  Package SHA-256:
  `5F57C396D72E02504FB551CB09EE1D19F15FE1C465C35A41C4768858188BA8B9`.

### 0.48.0-controller-profile-diagnostics

- Added standard HTC Vive controller suggested bindings for trackpad movement
  and turn, trigger select/value, squeeze, menu, grip/aim poses, and bilateral
  haptics. Existing Simple, Touch, Index, and Microsoft Motion bindings remain,
  raising explicit profile coverage from four to five.
- Handles `XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED` and queries
  `xrGetCurrentInteractionProfile` for both top-level hand paths. Each event logs
  frame, source, hand, numeric path, resolved profile string, and result; final
  input summaries retain event count and last left/right profiles. Session
  teardown clears profile state so reconnect evidence cannot appear current
  when it is stale.
- This is passive diagnostics plus suggested bindings: it does not alter SOMA
  semantics or controller-role policy. Both Release flavors and all CTest suites
  pass; live Vive/WMR/Touch/Index/runtime-switch acceptance remains. OpenXR DLL
  SHA-256:
  `4A9DD72EA15E2A39D5A69FD92B850C8FA128C9DE2385FDDE3B103297F54D8156`.
  Package SHA-256:
  `68D48CD7A0F33F899CA1410D1190A8EA1C77496C1040074B75034F1C836B11EF`.

### 0.47.0-stereo-view-history

- Extended the guarded previous-view bank from continuous dual render to every
  exact-player stereo pass, including the proven AFR fallback. AFR otherwise
  feeds each eye the other eye's immediately preceding native history; the same
  pending-eye/actual-eye transaction now restores and captures the matching
  eye without changing renderer, simulation, or submission ownership.
- Added temporal discontinuity handling. Camera recenter/calibration generation
  changes and pose-frame gaps over eight render frames reseed both banks from
  live native state, covering recenter, loading, tracking interruption, and
  delayed viewport recovery. Renderer/history identity replacement retains the
  existing reset, and every path still faults closed behind
  `HPLPerEyeViewHistoryControl=0`.
- Added deterministic calibration and stale-gap tests and updated the in-headset
  status/test contract for ordinary F10 AFR as well as same-frame stereo. Both
  Release flavors and all CTest suites pass; live AFR/continuous visual and
  performance acceptance remains. OpenXR DLL SHA-256:
  `AC859876EDF7DC97969BB2455BDFA8C6AA1BE94C7E98EC6557989049FEBCF6FA`.
  Package SHA-256:
  `47510B3A41DC11706C5EE46E398F9202A920894A4224784984E020A198F77C4E`.

### 0.46.0-per-eye-view-history

- Confirmed that `HPL3_Renderer_RenderPostPostEffects` copies exactly `0x40`
  bytes from the active frustum view matrix at `*(renderer+0x20)+0x158` to the
  renderer history object at `*(renderer+0x438)+0x80`. HPL2 source independently
  identifies the packet as `cMatrixf m_mtxPrevView`.
- Added an opt-in left/right history bank for continuous exact-player stereo.
  The camera bridge publishes its pending eye and OpenXR pose frame before each
  viewport; SOMAVR restores that eye's packet before world rendering and commits
  SOMA's native post-post result only after the actual eye/pose identity agrees.
  First identity use seeds both banks from native state, while renderer/history
  replacement, frame regression, disable, and re-enable reset cleanly.
- `HPLPerEyeViewHistoryControl=0` is the hard rollback. Invalid pointers, packet
  access, or eye sequencing fault closed to shared native history until the
  same-frame mode is toggled. The F1 status panel reports standby, active, fault,
  or unavailable. Pure bank/reset/identity tests and all Release tests pass;
  live temporal and performance acceptance remains. OpenXR DLL SHA-256:
  `65A1DF5EE8E6BC70F29EBCF353670794A66B8D2D340A9F0BF06ACD37FC990C60`.
  Package SHA-256:
  `9C8DA33D959C757696D6AF76BDA7B1EE31FF684C1A27907096E51FA4A68FDA07`.

### 0.45.0-continuous-dual-render

- Promoted the proven exact-player one-frame replay into an explicit opt-in
  sustained same-frame stereo path. `HPLDualRenderControl` owns the configured,
  ready, enabled, rejection, and fail-closed state; the existing exact
  `HPL3_Scene_RenderViewport` hook remains the single native replay owner.
- Continuous mode immediately preserves eye one, replays only the exact player
  viewport with screen-GUI mask bit `2` removed, and lets the ordinary boundary
  preserve eye two. `HPL3_Scene_RenderViewports`, engine update/script lifecycle,
  renderer frame/stat reset, GUI, XR submission, and presentation still execute
  once per game frame. Cache failure or eye/pose-sequence mismatch disables the
  mode and returns to the proven AFR path.
- Added `SAME FRAME STEREO` to the in-headset F1 panel. The active profile exposes
  the control but starts it off; `HPLDualRenderContinuousControl=0` removes it
  completely. Manual and automatic bounded replay samples still collect temporal
  mutation evidence, while ordinary continuous frames avoid per-frame snapshots.
  Ghidra and the native address ledger now record the exact replay contract and
  the necessarily duplicated stateful post-post phase. Both Release flavors and
  all three CTest suites pass; live headset acceptance remains. OpenXR DLL
  SHA-256: `91DC8889E1531B118530CC5E022B78DF2141A77C93E9761B476B869F89523274`.
  Package SHA-256:
  `141B1E6AF7BC736C7459940BF523AEBA01096BE9FD5A94B477C19E5DAE6E45CB`.

### 0.44.0-inventory-presentation

- Traced the shipped inventory input path from
  `eAction_OpenInventory = 12` through `InventoryHandler::OnAction` to
  `AutoEnable(3)`. The inventory module is exact user-module ID `15`; its
  three-second hold and `0.6/s` fade fit inside a bounded five-second capture
  authorization.
- Added `HPLUserModuleBridge`, an exact-signature observer at `0x1401378e0`.
  It preserves native action dispatch and authorizes flat current-ImGui HUD
  capture only for module `15`, action `12`, pressed edges. Main menu remains
  covered by the already-proven native pause gate, while hints, credits,
  crosshair, descriptions, infection, and fullscreen flashes retain their
  exact GameHud paths.
- Added bounded inventory telemetry and a one-line rollback through
  `HPLInventoryPresentationControl=0`. Ghidra now records the native action
  wrapper, script dispatcher, registration owner, and confirmed `mlId +0x158`.
  Both Release flavors and all three CTest suites pass; live inventory
  acceptance remains. OpenXR DLL SHA-256:
  `F661A91F9B743247D1786A5F775101BDF4FD9212C561CFC80446E5AD96312D4A`.
  Package SHA-256:
  `54403D571E62DB55FF6C1F714E8F4FC9428157AB825574155DC888288C6926A2`.

### 0.43.0-scripted-presentation

- Traced shipped `WakeHandler`, `GameOverHandler`, and `CreditsHandler` scripts.
  Wake uses exact `cScript_RunGlobalFunc` calls, game over is owned by player
  state `17`, and credits draw through `GetGameHudImGui` already covered by the
  HUD layer. Module IDs are GameOver `10`, Wake `12`, and Credits `19`.
- Extended the guarded global-script dispatcher to decode exact bool/float wake
  arguments at `0x140485720/0x140485200`. Sleeping now owns the existing XR
  blackout; wake-up releases blackout and authorizes exact-current-ImGui HUD
  capture for the shipped eyelid duration. Loading and wake blackout ownership
  are arbitrated so one cannot prematurely clear the other.
- Added flat current-ImGui capture during exact dead state `17`, retained the
  unconditional GameHudImGui credits path, and routed dominant primary/select
  to SOMA's accepted Jump semantic while dead even if authored-camera input is
  otherwise suppressed. `HPLScriptedPresentationControl=0` is the single-line
  rollback. Both Release flavors and all three CTest suites pass; live
  acceptance remains. OpenXR DLL SHA-256:
  `E2D2D6C1A6AD6AC02C0604B789928D556D0C2DBBFF773709A0DA769274FD2B77`.
  Package SHA-256:
  `9ED22CCFCD1A53251661130064EA5D5B7586F441304BD82B93CFEA900B1BB745`.

### 0.42.0-native-gameplay-haptics

- Confirmed SOMA's script-visible `SetRumble` boundary at `0x140109b30` and
  traced shipped `Effect_Rumble_Start` use across player damage, death,
  attacks, locked interactions, datamining/tool sequences, and environmental
  effects. The native player damage wrapper at `0x14015bf50` is also named and
  documented in Ghidra.
- Added an exact-signature bridge that preserves SOMA's gamepad call, mirrors
  authored strength/duration bilaterally to OpenXR, and works without a
  connected physical gamepad. A tested rising-edge/strength/80 ms refresh
  envelope emits bounded 100 ms segments, suppresses per-frame chatter, and
  explicitly stops both hands on the authored falling edge.
- Added independent config controls and OpenXR `xrStopHapticFeedback` support.
  Raw unscripted physics contact/material haptics remain future work. Both
  Release flavors and all three CTest suites pass; live gameplay acceptance
  remains. OpenXR DLL SHA-256:
  `302C321D1E628AC38A59CE2B466964AB18CCCF5A9EB945A21A7F8E4FDFD693DB`.
  Package SHA-256:
  `DC0523D61253183A94094D230956E9836F8BADDD0AF8F01679605C7F1805962F`.

### 0.41.0-roomscale-body-reconciliation

- Confirmed and signature-guarded native character-body feet getters/setters at
  `0x140237970` and `0x140237920`, plus the size vector at body `+0x134`, using
  Ghidra and both released HPL2 implementations.
- Added optional sustained-displacement roomscale body catch-up: Normal/Normal
  and pause/loading/authored-state gates, 0.45/0.25 m hysteresis, 30-pose hold,
  0.015 m maximum steps, and a three-height center/radial capsule sweep through
  SOMA's confirmed collision query.
- Each accepted native feet step applies inverse tracking-neutral compensation
  to preserve the rendered camera position. Exact-signature failures, blocked
  safety, malformed bodies, and special states fail closed. Pure math coverage
  verifies activation, hysteresis, release, and step limits. Both Release
  flavors and all three CTest suites pass; live headset/collision acceptance
  remains. OpenXR DLL SHA-256:
  `6540881F013610FB020D52152D9BAA7F7621502A81F8419262B1CF7853A0BB12`.
  Package SHA-256:
  `B452032CAB22FBD43116F92066BAFC6FAA1BE694872930968E181B80BF6D9368`.

### 0.40.0-dual-render-temporal-probe

- Corrected `0x1401f1480` from a callback-only boundary to
  `HPL3_Renderer_RenderPostPostEffects`: a full deferred/post-post phase that
  performs GPU work and copies current renderer state into temporal history.
- Added three automatically spaced, exact-player one-frame replays after stable
  tracked stereo, while retaining manual `Ctrl+F6`. Every replay remains
  bounded, suppresses final screen GUI, captures the first eye immediately,
  and returns to the proven AFR path.
- Added `HPLDualRenderDiagnostics` and tested `HPLTemporalMutationMath` to safely
  snapshot renderer/current/history/settings regions and correlate changed-byte
  ranges across first and replay eyes. No captured native state is modified or
  restored. Both Release flavors and all three CTest suites pass; live temporal
  acceptance remains. OpenXR DLL SHA-256:
  `C077FB7C503648EC3E41008933CD1DE5C12DA1C69DC1E19E1D22BCC45F3A568B`.
  Package SHA-256:
  `6510A18F552F3588AAD2D21635C6FEAE29411100DDEF0181CAC18A254C36E938`.

### 0.39.0-vr-control-panel

- Added `FEATURE.VR_CONTROL_PANEL`: a dedicated head-locked OpenXR alpha quad
  with a tested CPU rasterizer and independent swapchain. It reports live
  tracking, stereo, controller, player-state, authored-camera, HUD, reticle,
  roomscale, and centered-projection state without modifying SOMA render data.
- `F1` or `Menu + Secondary` opens the panel. Movement-stick navigation and
  dominant select/trigger expose recenter, roomscale, centered projection, HUD
  layer, reticle, and close actions. Panel ownership is exclusive: gameplay,
  pause, terminal, locomotion, and manipulation input is released while open.
- Runtime HUD/reticle visibility and camera roomscale/projection setters use the
  existing guarded owners. Missing OpenXR resources remain fail-closed; the
  world projection/stereo path is untouched. Both Release flavors and all
  three CTest suites pass; live compositor/input acceptance remains. OpenXR
  DLL SHA-256:
  `ADA0F6D7574E46B4D17AABBB073764EA3CC933A0BD516E921D0B9DD18F68BF82`.
  Package SHA-256:
  `F45A87F2C6F9E472E702409D441272B279B7B413C9B79E7A6672AC99BFDDC827`.

### 0.38.0-terminal-pointer

- Added a signature-guarded detour at the exact 3D GUI input boundary,
  `cImGui::SendMouseVirtualPosition` (`0x1402f0c90`). During shipped wall and
  handheld terminal player states `8/9`, dominant-controller aim is projected relative to the HMD,
  smoothed, converted through the current `cGuiSet` virtual size/offset, and
  forwarded through SOMA's original ImGui update.
- Ownership is deliberately narrow: the target must be the exact current ImGui,
  must not be GameHudImGui, and must own a readable 3D `cGuiSet`. Pause, HUD,
  ordinary gameplay, malformed layout, tracking loss, and every
  signature failure retain the original native cursor path.
- Dominant select/trigger uses SOMA's existing left-click input route, with
  input release and pointer deactivation on every state/pause/tracking exit.
  New bounded logs report projection, native-hook application, GUI identity,
  virtual coordinates, and each fail-closed reason.
- Ghidra now names/types the physical and virtual cImGui cursor wrappers plus
  the manager update that proves world GUI dispatch. Both Release flavors and
  all three CTest suites pass; live terminal alignment/click testing remains.
  OpenXR DLL SHA-256:
  `397ED05762E30CE55D7258ED1A7A2D88B3C9C33C5503AB30A732AACAE61CA883`.
  Package SHA-256:
  `6E8831ACBDF838B17FF006E5A9A5CF7C3684D2A880CCD2288624D219763320A2`.

### 0.37.0-post-resource-probe

- Added an exact-signature hook at `HPL3_PostEffect_RenderOne` (`0x1402d7a40`)
  using the confirmed five-argument ABI and native output texture return.
- Added bounded per-effect GL capture for bound texture target/ID, level-zero
  dimensions/depth/internal format, framebuffer writes, HPL input/output object
  identity, eye, and pose frame. Captures run for startup samples, the normal
  diagnostic interval, and both halves of an armed `Ctrl+F6` replay.
- Added canonical resource-footprint hashing and conservative eye ownership
  classification. Shared/eye-distinct results require observed resources from
  both eyes at the same nonzero pose frame; ordinary AFR timing cannot satisfy
  the gate accidentally. No effect state, texture data, framebuffer attachment,
  or render order is mutated.
- Added generated-off/active-development-on configuration, overflow/call
  telemetry, summary counts, and pure tests for bind-order-independent hashes
  plus shared/distinct/unknown classification.
- Both Release flavors and all three CTest suites pass. OpenXR DLL SHA-256:
  `87B683886D9737C7A52CE6D9013D0DF068E355D794D8AB0D17D057E08FD54468`.
  Package SHA-256:
  `1EB2BB1CE6D03F8AB1262FB931EDCCA66E57F54EFDC9C6841E648436EFC991D0`.

### 0.36.0-two-hand-tools

- Added support-hand pose composition for exact independent `HudObject` tools.
  While the support controller is squeezed and both grips are fresh and within
  configured separation limits, the native uniform-scale object remains rooted
  at the dominant grip but aims toward the support grip. Releasing squeeze,
  losing either pose, unsafe separation, authored-camera ownership, or malformed
  native scale immediately restores the dominant-only or native path.
- Extended the exact Grab-state torque PID bridge with optional two-hand
  direction rotation. Engagement captures a direction anchor; subsequent
  support-hand motion becomes a bounded shortest-arc world angular target while
  SOMA retains its native torque PID, force caps, mass, inertia, collision,
  joints, gravity, and callbacks. Engagement and release each spend one native
  torque call re-anchoring before any correction, preventing mode-change spikes.
- Added independent tool/grab rollback switches, squeeze/separation/blend
  controls, bounded transition/fallback telemetry, a dedicated pure math module,
  and basis/shortest-arc tests. Generated defaults remain disabled; the active
  development profile enables both paths for live acceptance.
- Both Release flavors and all three CTest suites pass. OpenXR DLL SHA-256:
  `8F3D2A5685DE9D8AED98A05756124116F81F9B270E7CAC0519CE94B4BEFF4712`.
  Package SHA-256:
  `6BEC50FDC4C57E3CC4814C25D34B9846395F815CFD282D0BC60C9E6EAAC683DE`.

### 0.35.0-dual-render-probe

- Added the first executable `FEATURE.DUAL_RENDER` experiment. `Ctrl+F6` arms
  exactly one replay of the next exact player viewport; all secondary cameras
  and ordinary frames remain on the proven AFR path.
- The first native eye is copied into its OpenXR cache immediately. The same
  `HPL3_Scene_RenderViewport` is then invoked once with screen-GUI mask bit `2`
  cleared, preserving world and active-post bits. The normal frame boundary
  captures the second eye, while the once-per-frame viewport enumerator,
  renderer frame advance, script update, and presentation remain untouched.
- Added hard eligibility checks, same-pose/opposite-eye validation, immediate
  fallback on cache failure, replay CPU/draw/clear telemetry, and explicit
  accounting for the native post-post callback that the viewport function runs
  unconditionally. Generated configuration remains disabled; the active
  development profile enables the one-frame probe.
- Refactored pending-eye cache capture into a reusable OpenXR runtime operation
  and added pure replay-mask/eligibility/eye-sequence tests. Both Release flavors
  and all three CTest suites pass. OpenXR DLL SHA-256:
  `570F925B93B87CA29214D733940A1DC6DD52BB7DA06DB87094390733C58942B7`.
  Package SHA-256:
  `25B952922C7D6C23F28388FC1E7D657DF5BC0AA6FCDB2863F1392677D9D73C50`.

### 0.34.0-subtitles-menus

- Added a dedicated, signature-guarded native subtitle bridge at
  `0x1401c8dd0`. During active F10 stereo it temporarily scales SOMA's cached
  subtitle wrap width, active Y, font size, and shadow offset, invokes the
  original localized draw worker, and restores all four values immediately.
  Speaker names, timing, gradual reveal, language data, font ownership, and
  subtitle enable state remain native.
- Added exact paused-menu capture to the existing HUD transaction. Only the
  current ImGui set returned by `SOMA_GetCurrentImGui()->GetSet()` is admitted,
  and only while confirmed `SOMA_GetGamePaused()` reports pause ownership. The
  existing controller cursor/click route and all non-paused/diegetic ImGui sets
  remain unchanged.
- Added pure subtitle-layout tests, malformed-layout fallback counters, paused
  menu identity/capture counters, generated-config rollback switches, and an
  active development profile for live acceptance. Both Release flavors and all
  three test suites pass.
- Made release packaging compatible with Windows PowerShell 5.1 by replacing
  the unavailable `.NET Path.GetRelativePath` call with a validated staging-root
  prefix calculation; archive members still receive normalized relative hashes.
- Renamed and documented `SOMA_VoiceSubtitle_Render` at `0x1401c8dd0` and
  `SOMA_cLuxVoiceHandler_Constructor` at `0x1401d3ba0` in the shared Ghidra
  database. OpenXR DLL SHA-256:
  `9D5053CA3FA1F405221FCB37933D34D9D33DE5219012C582A21D1704444D928B`.
  Package SHA-256:
  `3ED658003BE5451A3FBDA14DAC837AF0B3F6D70F6CD066DE36F1889CF722BA5F`.

### 0.33.0-depth-resources

- Promoted the capture-only depth experiment into optional compositor depth.
  Supported runtimes negotiate D24/D32F or their depth-stencil counterparts,
  preferring the live default framebuffer's stencil topology, and receive one
  matching swapchain/cache per eye, and chain `XrCompositionLayerDepthInfoKHR`
  onto both projection views. Every unsupported or failed condition falls back
  to the proven color-only frame.
- Confirmed at `0x140270230` and `0x14026fcf0`, with matching HPL2 source, that
  SOMA uses finite standard OpenGL depth. Submitted min/max are `0/1`; HPL
  near/far are divided by world units per meter. Pure tests cover conversion and
  malformed clip contracts.
- Added graphics-binding and runtime-view resilience. A changed HDC/HGLRC enters
  existing full runtime recovery, while changed recommended dimensions or sample
  limits rebuild frame resources transactionally. Bounded summary counters expose
  checks, rebuilds, context changes, depth copies, and depth failures.
- Updated Ghidra, Graphify, traceability, RE maps, phase tracking, and the live
  checklist. Both Release flavors and all three test suites pass. OpenXR SHA-256:
  `DB626F12368FFE388527EA435EA5D4A88901EA200F35438156DBCA9AFCE1582E`.
  Package SHA-256:
  `7C7BE027CD51767374A03C312AFC824FE84C6515501FC37FAF3F0BE8CB3E5D4C`.

### 0.32.0-viewport-depth

- Hardened native camera ownership around the exact player camera exported by
  `HPLPlayerState`. Reflection, terminal, water, shadow, and other secondary
  viewport cameras now keep native frusta and cannot consume F4/F5/F10/F11
  edges or become the initial VR camera.
- Recovered the complete `cViewport` identity packet at `0x140298630`, including
  camera, world, renderer, post composite, framebuffer, position, size,
  active/visible/listener flags, and render settings. Bounded telemetry labels
  exact player, active-VR, secondary, and unresolved viewport calls.
- Extended the depth capability experiment with optional per-eye
  `GL_DEPTH_COMPONENT24` cache attachments. Each AFR capture copies source depth
  with `GL_NEAREST` and periodically samples center depth without yet creating
  OpenXR depth swapchains or submitting `XrCompositionLayerDepthInfoKHR`.
- Added pre-injection conflict diagnostics for duplicate SOMAVR injection,
  graphics proxies, ReShade, Special K, RTSS, OpenXR Toolkit/API layers,
  vrperfkit, OpenVR, and local `dinput8` loaders. Only duplicate SOMAVR blocks;
  other findings remain explicit warnings.
- Added checksum-verified install/update and manifest-driven uninstall scripts.
  Updates preserve `somavr.ini`, publish changed defaults separately, remove
  stale managed files, and never recursively delete an install root. A CTest
  lifecycle test covers fresh install, config preservation, update, stale-file
  cleanup, and uninstall under Windows PowerShell 5.1.
- Updated and saved the Ghidra viewport/create/destroy names and comments. Both
  Release flavors and all three test suites pass. OpenXR SHA-256:
  `08715E82D5C0C3457AD093279EE67ADBC79AFDD4ACCD56ED8CEDD83233508FCC`.
  Package SHA-256:
  `38A031665E0486FD93E68B850A21829C07FC1F50546AC5244FFB6719909BE550`.

### 0.31.0-tools-hud

- Extended the proven Lux entity identity hook to exact `HudObject` and
  `*_HudObject` script contracts. The independent camera-follow interaction
  object can now use a dominant-grip world transform with its native uniform
  scale; socketed inventory tools remain owned by the already controller-driven
  hand attachment and are classified without a second transform.
- Added strict native fallback for malformed/nonuniform matrices, missing player
  ownership, authored cameras, inactive F10 tracking, stale input, lost position
  or orientation, and failed transform math. Dedicated position/rotation
  calibration and bounded identity/pose/summary rows are enabled in the active
  development profile.
- Added an exact, signature-guarded `cLuxMap::DestroyEntity` lifecycle hook.
  It evicts destroyed entity pointers from the hands/tool identity cache before
  native queueing, so recreated script tools cannot inherit stale ownership if
  the allocator reuses an address. Hook installation remains transactional.
- Promoted exact `SOMA_GetGameHudImGui()->GetSet()` identity from passive
  telemetry into the existing HUD layer. The transparent target clears once per
  game frame, then accumulates exact GameHudSet and GameHudImGui draws into one
  VIEW-space OpenXR quad. Pause/menu and diegetic GUI sets remain native.
- Added one bounded `hpl_render_transaction` report per sample interval. It
  records the complete nested viewport/world/callback/post/GUI order, per-stage
  multiplicity, draws, clears, and CPU cost, and names only a world-stage replay
  candidate while explicitly retaining the callback-side-effect proof gate.
- Updated and saved the Ghidra names/comments for exact SetMatrix, DestroyEntity,
  GuiSet Render, and GameHudImGui functions. Pure tool-root tests, both Release flavors, and
  both test suites pass. OpenXR SHA-256:
  `D75E2F901C6BC2F9BCA55B98B02C36513233FF2CB635AF8DA44517A85C991F6D`.
  Package SHA-256:
  `CCC5FE427230E5F715863291F9E5E20D8FD61BCE7D798C0F1C586BE7B5089E4B`.

### 0.30.0-screen-effects

- Promoted SOMA's shipped `Effect_Screen.hps` screen-material path into a
  dedicated native bridge. Exact `Screen Particle<decimal>` billboard identity
  is captured at creation, retained through size/position updates, and removed
  at the exact world destruction boundary.
- During active F10 VR only, the authored `0.15` world-unit camera-relative
  placement is moved to a configurable `1.5 m` target. Billboard dimensions use
  the same ratio, preserving angular coverage while removing extreme binocular
  convergence. Leaving VR restores native position and size behavior.
- All four native boundaries are byte-signature guarded and installed
  transactionally. Generic position/size hooks use a fixed atomic identity set
  before touching the protected metadata map, keeping unrelated entities on a
  lock-free pass-through path. Pure scaling tests cover translation, angular
  size, and malformed inputs. Both Release flavors and tests pass.
- OpenXR SHA-256:
  `C4F6CCAC0C03103718D97DC6BFF47D7FC2D055FA3FF9CC9EDAAFD91B02B51E94`.
  Package SHA-256:
  `55182F897DACD22045D71C13105717462B31C17E8691C68D534EF268428A10A6`.

### 0.29.0-presentation-optics

- Added reversible active-VR control at the exact registered FOV,
  FOV-multiplier, and aspect-multiplier leaf wrappers. Script fade speed is
  preserved while targets resolve to native default FOV and neutral `1.0`
  multipliers; inactive VR remains native.
- Added `HPLPresentationBridge`: exact loading visibility now invalidates AFR
  caches on both edges, submits zero XR layers during loading, releases held
  controller input, and automatically rearms stereo through a bounded exit
  guard while leaving SOMA's native desktop loading presentation intact.
- Added signature-guarded, probe-only `CreateVideo`/`DestroyVideo` lifecycle
  telemetry. Native stream names, identity, pairing, active count, and peak
  concurrency are collected without replacing playback.
- Synchronized six native boundaries into Ghidra, rebuilt Graphify, and updated
  the project-phase Google Sheet. Both Release flavors and tests pass. OpenXR
  SHA-256: `D824022BBD3054B62895AA799A4C79D71D3769F7F00DB353B625FAEA6010D0B3`.
  Package SHA-256:
  `E8D9AC9483CA32D25E1906EDCE9C2DBAE4D99179C8366E6E6D0A94512C784440`.

### 0.28.0-authored-comfort

- Added exact semantic camera-roll control at the registered
  `FadeCameraRollTo` and `SetCameraRoll` wrappers. Lean, locomotion, and climb
  roll can be zeroed independently while scripted roll remains native by
  default; fade speed and maximum speed are preserved.
- Added a reversible world depth-of-field guard at
  `cWorld::SetDepthOfFieldActive` and extended named post-effect policy to
  `VideoDistortion`. Both act only while F10 VR tracking is active.
- Mapped the shipped player-state IDs and added bounded comfort black frames on
  entry to or exit from ladder, climb-ledge, interactive-camera, sit, and death
  states. Native state ownership, constraints, scripts, camera motion, and FOV
  remain untouched.
- All native boundaries are signature checked and installed transactionally;
  partial failure restores every prior byte/hook. Pure roll/state policy tests
  cover the shipped IDs. Both build flavors pass. OpenXR SHA-256:
  `35E70B5E065C4A2592070F7DFDC556732271056CCFF8351DE1566401C37D8A65`.
  Package SHA-256:
  `B1F52060998B37D99B2E273482B8919BD477DAA015F7A0EAB0809982F9562C1C`.

### 0.27.0-gameplay-coherence

- Extended controller flashlight ownership from the rendered spotlight to the
  three low-frequency agent-gobo gameplay rays in shipped
  `Player.hps::UpdateFlashLightLOS`. The exact registered `GetClosestBody`
  wrapper at `0x1400cd7d0` is signature guarded; only camera-origin rays in the
  flashlight's recovered length range are redirected.
- The redirected query starts at the exact cached visual flashlight matrix and
  rotates SOMA's randomized cone from the native camera basis into the tracked
  controller basis. Native ray length, distance/normal outputs, physics body,
  hit policy, scheduling, and all non-flashlight callers remain unchanged.
- Added an opt-in dynamic-inclusive mode to the existing sampled room-scale head
  volume. It reuses `SOMA_CheckLineOfSight` with `staticOnly=false`, retaining
  the same clearance, authored-start rejection, binary search, shared pose, and
  native capsule ownership. Generated configs default both additions off; the
  active profile enables them for live acceptance.
- Added deterministic off-axis cone-preservation tests and fail-closed rollback
  for partial native-hook installation. Both build flavors pass. OpenXR
  SHA-256:
  `14E69FD1BB8F0FD93AFE1AC0B6C625D00A51E006B55C38AB906B73EF890AFAF3`.
  Package SHA-256:
  `E3AA9FA33047BB82A48624A43C38701A4526C3213DB895D1F21BA7153A6B28C0`.

### 0.26.0-gpu-depth-probe

- Added nonblocking per-eye GPU telemetry to the six existing guarded HPL
  render-stage hooks. Nested-safe `GL_TIMESTAMP` pairs use a bounded pool, are
  polled only after availability, and are attributed at stage end. Pool
  exhaustion drops samples instead of stalling SOMA.
- Added an opt-in OpenXR depth-capability probe. The runtime detects and
  conditionally enables `XR_KHR_composition_layer_depth`, then records default
  OpenGL depth precision/range with confirmed HPL projection near/far planes.
  This build deliberately does not submit depth layers yet.
- Missing timer-query entry points and unsupported depth extensions are isolated
  fallbacks. Generated configs keep both probes disabled; the active profile
  enables a 128-pair pool and depth evidence capture. Both build flavors pass.
  OpenXR SHA-256:
  `67C0A882F995EDD41F38A958951105377A347AC21EF138F9242D40CBE12C56F0`.
  Package SHA-256:
  `7658C575D49616064D829C56F08F89425C1AC5B92DD2826EE29FE941E57990E9`.

### 0.25.0-volume-spectator-telemetry

- Replaced the point-only room-scale safety sample with a configurable swept
  head-volume approximation: center, horizontal radial ring, and top/bottom
  static-world probes all share the confirmed `SOMA_CheckLineOfSight` boundary.
  The earliest valid obstruction controls one coherent eye/controller offset;
  probes whose authored start is already obstructed are skipped rather than
  trapping the player.
- Added desktop spectator controls sourced from the existing AFR eye caches.
  `DesktopMirrorEye=left|right` presents a stable eye only after XR submission;
  `native` is a no-op rollback. Fit, fill, and stretch policies use tested blit
  layouts, restore GL framebuffer/buffer/scissor/clear/color-mask state, and
  fail back to SOMA's native backbuffer.
- Added opt-in per-eye CPU telemetry to the six existing signature-guarded HPL
  render-stage hooks. Left, right, and mono call counts, average microseconds,
  and total milliseconds are emitted periodically and at shutdown without new
  executable detours. GPU timestamps remain future work.
- Added deterministic head-volume sample and spectator-layout tests. Generated
  configs keep all controls off/native; the active profile enables a six-point
  radial ring, two vertical probes, stable left-eye fit mirror, and per-eye CPU
  timing. Built and tested both default and OpenXR x64 Release flavors. OpenXR
  SHA-256:
  `CC9B34EDED6630653BD653C086735414F104457C6CE691D1A05E45FD6ECB72A9`.
  Package SHA-256:
  `D67FCFDFE38D50CF30C2440446116459D34D9CC519DB8ABA081C4716759228BF`.

### 0.24.0-roomscale-safety

- Added collision-aware room-scale head translation at the confirmed shipped
  `CheckLineOfSight` script wrapper. The active profile ray-tests the calibrated
  camera origin to the physical HMD offset against static world geometry, then
  uses a bounded binary search and configurable clearance to stop the head at
  the last safe point.
- The clamped physical-head component is shared by both eyes, HMD/controller
  world poses, interaction, hands, and flashlight placement. IPD, configured eye
  height, authored camera motion, controller-relative offsets, and native player
  capsule ownership remain unchanged.
- The control is signature-guarded and fail-closed at install. Invalid world
  state, malformed poses, unavailable native queries, and a rejected baseline
  retain the prior unmodified translation. Generated configs default off; the
  active test profile enables static-only safety with `0.12 m` clearance and six
  search iterations.
- Added deterministic clearance-factor and tracked-offset decomposition tests,
  explicit cache invalidation across activation/recenter/camera changes, bounded
  collision telemetry, and a static-wall smoke-test gate. Built and tested both
  default and OpenXR x64 Release flavors. OpenXR SHA-256:
  `1DDDCD231723EA857D377E3AE9478AA28188F72EC3D59B2B8527974813E6EA58`.
  Package SHA-256:
  `A0AB03ACF9CE0A92CF1A57906B318B8B47C2F0D6D22257BD036B9D5BD772A8A4`.

### 0.23.0-controller-flashlight

- Promoted the shipped flashlight transform to dominant-controller aim. Exact
  entity name `Flashlight` is recognized at the already signature-guarded
  `iLuxEntity.SetMatrix` boundary, so no additional executable detour or broad
  light-class mutation was introduced.
- Added `HPLFlashlightMath` to map tracked OpenXR forward/up into HPL's local
  negative-Z spotlight convention, with independent controller-local position
  and model-space rotation calibration. The active profile enables the feature;
  generated configs default off.
- Preserved SOMA's original light object, color/fade, visibility, radius, FOV,
  near plane, environment particles, frustum collision, light sensors, and
  script callbacks. Invalid player/tracking/pose-age/authored-camera/math states
  forward the original camera-mounted matrix.
- Added bounded exact-identity/pose/fallback telemetry and deterministic basis,
  offset, and malformed-pose tests. Built and tested default and OpenXR x64
  Release flavors. OpenXR SHA-256:
  `B07142C03DFCE1F0888DB38B6E4661DED334B82847206D5BBA783712A679FA3C`.
- Added a deterministic OpenXR release packager that validates build flavor,
  stages runtime/config/core-doc artifacts, writes per-file SHA-256s, and creates
  a versioned ZIP. Added a 12-scenario smoke-test matrix spanning startup,
  stereo, flashlight, interaction, manipulation, physics, UI, authored cameras,
  transitions, tracking loss, death/wake, and shutdown. Package SHA-256:
  `6FF9DA98FF9C29593B5FDBADB8A8AB7BB70768D1E2D82A3864507EB85D388D32`.

### 0.22.0-physical-manipulation

- Added dominant-grip physical manipulation for shipped player states Wheel `3`,
  Slide `4`, SwingDoor `5`, Lever `6`, and Tear `7`. Controller displacement
  relative to the HMD is projected onto current head-right/head-up and emitted as
  bounded relative mouse motion, preserving SOMA's native `mvMoveAdd`, joint,
  PID, physics, callback, and map-script ownership.
- Added state/tracking reacquisition anchors, subpixel accumulation, per-frame
  caps, sign and sensitivity controls, generated-config-off rollback, and bounded
  state/event/pixel telemetry. Room-scale body translation cancels before
  projection; Grab/Push retain their dedicated pose/throw paths.
- Added a passive ImGui identity probe to `HPLHudBridge`. Signature-guarded
  wrappers `GetCurrentImGui` `0x1400cca70`, `GetGameHudImGui` `0x1400cca90`, and
  `cImGui::GetSet` `0x140071f20` correlate rendered `cGuiSet` calls without
  changing presentation, creating the next log evidence for inventory, hints,
  menus, loading, death, wake, credits, and subtitle ownership.
- Added deterministic manipulation projection/deadzone/cap tests, documented and
  tagged the two newly recovered wrappers in Ghidra, and built/tested default and
  OpenXR x64 Release flavors. OpenXR SHA-256:
  `179AD16A2D2CF023C73CB2C8478152A36BD11B0863521F5B02BAB997AFEE1DB0`.

### 0.21.0-semantic-reticle

- Added `HPLCrosshairBridge`, a signature-guarded observer on registered global
  script dispatch `0x140484ea0`. It recognizes only
  `LuxPlayer::_Global_SetCrosshairState`, reads argument zero through confirmed
  `cScript_GetGlobalArgInt` wrapper `0x1404851d0`, then leaves SOMA's callback
  and interaction policy authoritative.
- Promoted the controller depth reticle from raw pick feedback to SOMA's exact
  35-state `eCrossHairState` vocabulary. The application-space layer now follows
  the native icon decision made after the shipped interaction/range checks and
  reports bounded semantic acceptance/rejection telemetry.
- Loaded and aspect-fitted all 34 crosshair assets named by shipped `Player.hps`
  directly from `graphics/hud`. Native artwork is intent-tinted and uploaded to
  the acquired OpenXR swapchain image; missing or malformed uncompressed TGA
  assets fall back to the prior procedural cross. `InteractionReticleNativeIcons`
  and `InteractionReticleSemantic` independently permit rollback.
- Added state-aware focus feedback. Pickup, manipulation, traversal, social,
  unavailable, and simple-hint states receive bounded amplitude/duration
  profiles; the ambiguous default cursor does not trigger haptics.
- Added deterministic semantic color and haptic-profile tests, validated all 36
  installed crosshair TGAs as uncompressed 24/32-bit assets, updated Ghidra with
  prototypes/comments/tags for the three script-global wrappers, and built/tested
  default plus OpenXR x64 Release flavors. OpenXR SHA-256:
  `0674E44B6C7C7151088627211F53104BC3A1AF4E3D53E22422CA50F350A8066C`.

### 0.20.0-depth-reticle

- Added an opt-in controller interaction reticle as a source-alpha OpenXR quad
  in application space. It consumes the exact dominant-hand aim pose and native
  closest-entity distance already published by `HPLInteractionBridge`, so each
  eye receives compositor-correct depth and convergence without another pick.
- Added angular-size, physical-size, distance, and frame-age bounds. Missing or
  stale native hits clear the layer; invalid tracking, missing stereo projection,
  comfort blackouts, and swapchain failures fail closed without a gaze fallback.
- Added a dedicated transparent OpenGL/OpenXR reticle swapchain with complete GL
  state restoration and bounded suspension after repeated transfer failures.
- Added optional low-amplitude focus-change haptics keyed to SOMA's native
  entity/body identity, with a configurable frame cooldown. The generic reticle
  and pulse deliberately do not claim unconfirmed crosshair icon semantics.
- Added deterministic angular-quad sizing tests and built/tested default and
  OpenXR x64 Release flavors. OpenXR SHA-256:
  `7E366518B111CBEEBFC792323CCFEB623221EC78005098271285A640556A0E71`.

### 0.19.0-comfort-focus

- Added a signature-guarded hook at the registered `cLuxPlayer::SetCameraPosAdd`
  wrapper `0x140159360`. While F10 VR tracking is active it semantically zeros
  only the shipped Bob, Shake, and optional Sway channels; crouch, climb,
  terminal, script, death, lean, crawl, and conversation remain native.
- Added independent generated-config-off controls and bounded per-channel
  telemetry. The active development profile enables Bob, Shake, and Sway for
  direct comfort acceptance.
- Promoted `cLuxClosestEntityData` from a hit boolean to a validated immutable
  controller-focus snapshot. Confirmed output offsets expose entity `+0x18`,
  body `+0x20`, and distance `+0x28`; the bridge now derives the exact world hit
  point for the future depth reticle without changing native pick policy.
- Named and documented `SetCameraPosAdd`, `SetCameraRoll`, and closest-entity
  result ownership in Ghidra. Added pure camera-add policy tests and built/tested
  default and OpenXR x64 Release flavors. OpenXR SHA-256:
  `F746AB81B0D1F69CC96AEA7D133D4EB762DA7FBB06CF44C472ED09FA4FF04105`.

### 0.18.0-interaction-polish

- Promoted the Grab torque probe into an opt-in controller rotation path. A
  shortest-arc grip quaternion delta augments only the exact `40/0/0.4|0.1`
  torque-PID error while SOMA retains angular velocity feedback, inertia
  transformation, the native speed/torque caps, collision, and callbacks.
- Added a signature-guarded patch at AngelScript `iPhysicsBody::AddImpulse`
  wrapper `0x14049c720`. A 350 ms one-shot intent armed by the existing native
  Grab throw action redirects the authored impulse along controller velocity,
  falling back to grip-forward aim; optional bounded velocity scaling preserves
  SOMA's mass-adjusted impulse as the baseline.
- Added opt-in center-crosshair removal to the compositor HUD capture. Only a
  configurable center rectangle is cleared to transparent; the GL transaction
  now restores framebuffer bindings, buffers, viewport, scissor box/enable,
  clear color, and color mask exactly.
- Added pure grab-rotation math and quaternion equivalence/cap tests. Built and
  tested default and OpenXR x64 Release flavors. OpenXR SHA-256:
  `EA470277AD71B0B29E11345E803C813AA51EE2E3A4AAB40B1E68E3310788B544`.

### 0.17.0-physics-input

- Added configurable head-relative locomotion using calibrated HMD yaw only;
  pitch and roll cannot tilt the movement plane. Body-relative movement remains
  the generated-config default and native/semantic routing is unchanged.
- Added physical crouch with standing-height calibration, recenter generation,
  and separate enter/exit thresholds. It owns SOMA's native crouch toggle only
  in Normal/Normal state and falls back to the existing button route when pose
  data is unusable.
- Added `HPLGrabBridge` at confirmed vector PID output `0x140238750`. In Grab
  state only, the dominant grip's camera-relative translation augments the
  exact `400/0/40` native position error while SOMA retains PID gains, mass,
  force caps, gravity, collision, joints, and callbacks. Pickup, invalid pose,
  stale input, authored camera, and all other PID calls pass through unchanged.
- Added native manipulation mappings: support squeeze plus turn-stick movement
  holds SOMA's existing InteractRotate action, while dominant primary requests
  SOMA's native Grab/Push throw/cancel action.
- Captured OpenXR grip linear and angular velocity at predicted display time.
  Release telemetry and exact `40/0/0.4|0.1` torque-PID probes now provide the
  next dataset for controller rotation and calibrated throw impulse; neither is
  substituted in this build.
- Added deterministic tests for head-relative yaw, pitch rejection, crouch
  calibration, hysteresis, and exit. Built and tested default and OpenXR x64
  Release flavors. OpenXR SHA-256:
  `4226008C6B5E844077A1C039628E5F6AD93BD9ED0AD2C3D0801D14346914276A`.

### 0.16.0-controller-hands

- Promoted the exact `PlayerHands_*` SetMatrix probe into a guarded controller
  root path. `HPLHandsMath` reconstructs SOMA's default
  `camera * rotateY(pi) * scale` basis from the dominant tracked grip while
  preserving native quarter scale, skeletal animation, sockets, and tools.
- Added configurable controller-local root position and model-space XYZ
  rotation calibration. The active test profile uses SOMA's native
  `-0.3 * 0.25 = -0.075` vertical root offset.
- Root substitution is restricted to exact identity, uniform quarter scale,
  Normal/Normal player ownership, active VR tracking, and a fresh fully tracked
  grip. Full-scale/custom/authored states and every failure forward the original
  matrix unchanged with bounded reason counters.
- Corrected hand telemetry to interpret HPL transform basis vectors as matrix
  columns, matching `cMath::MatrixUnitVectors` and the shipped hand script.
- Closed the paused-input fallback hole: a confirmed `cLux_GetGamePaused`
  result now suppresses both direct native locomotion and synthetic gameplay
  keys/buttons instead of allowing the semantic fallback behind menus.
- Added `HPLMenuBridge` and tested head-relative pointer projection. While
  paused, dominant aim moves SOMA's native client cursor and trigger/select
  clicks; held clicks are latched until release after returning to gameplay.
- Built and tested default and OpenXR x64 Release flavors. OpenXR SHA-256:
  `9FD03BD662E31A3CF8BF96297573933558DF41606231285CBF5F2EF52C9CCE96`.

### 0.15.0-hud-layer

- Extracted permanent GUI ownership from `HPLCompatibilityProbe` into a
  signature-guarded `HPLHudBridge`; the existing exact `GameHudSet` identity,
  per-set draw telemetry, and virtual HUD metrics remain available.
- The exact 2D gameplay HUD now renders into a transparent `1600x900` OpenGL
  target instead of both AFR eye images when the OpenXR session is visible.
  Menus, ImGui, subtitles outside this set, and 3D/diegetic GUI sets remain native.
- Added a dedicated OpenXR HUD swapchain and alpha-blended
  `XrCompositionLayerQuad` in VIEW reference space. Distance, physical width,
  vertical offset, pixel dimensions, and accepted capture age are configurable.
- Added fail-closed behavior: missing signatures/resources/session visibility
  keep the native HUD path; four consecutive copy failures suspend extraction
  so the next frame returns to the backbuffer path.
- Added tested `HPLHudMath` quad placement/aspect validation and expanded OpenXR
  frame/summary telemetry with HUD capture, submission, and fallback counters.
- Built and tested default and OpenXR x64 Release flavors. OpenXR SHA-256:
  `B7639F5EF7DDE23098B7913CAF10523F3D648179BA8F4B61F40EF3494CB2C299`.

### 0.14.0-native-locomotion

- Added `HPLNativeLocomotion`, signature-guarding the registered
  `iCharacterBody::Move`, `iCharacterBody::AddYaw`, and `cLux_GetGamePaused`
  wrappers before any direct native input is possible.
- Normal, unpaused player/move state now receives radial-deadzone analog
  forward/right magnitudes. Paused play, menus, ladders, grabs, terminals,
  authored cameras, non-normal move states, invalid bodies, and signature
  mismatches automatically retain the existing semantic W/A/S/D route.
- Snap turn now uses an exact configurable degree increment and smooth turn uses
  configurable degrees per second through the native body-yaw accumulator.
  The existing pixel/mouse path remains the automatic special-state fallback.
- Added bounded route and summary telemetry distinguishing `native_analog` /
  `native_radians` from `semantic_keys` / `semantic_mouse`.
- Added role-aware support-hand face actions: default left X toggles SOMA's
  flashlight through `F`, and left Y opens inventory through `Tab`. Left-dominant
  mode moves these actions to the right support hand; one-hand recenter remains unchanged.
- Built and tested default and OpenXR x64 Release flavors. OpenXR SHA-256:
  `7FCD71F56214E30028964CD3721F614C46BF429A56DC6CE4B852CDE65B27527A`.

### 0.13.0-hands-identity

- Added signature-guarded `HPLHandsBridge` ownership around the shared Lux
  entity SetMatrix wrapper and confirmed inherited GetName accessor.
- Added exact `PlayerHands_*` identification through the native bounded MSVC
  string layout at entity `+0x120`; unrelated entities receive no pose analysis.
- Added passive hand-root telemetry for HPL matrix translation, basis/scale,
  quarter/full-scale mode, native-camera distance, dominant grip pose/distance,
  and authored-camera/player-state ownership. Original matrices remain unchanged.
- Promoted the GetName accessor and `cLuxProp` registration owner in Ghidra and
  documented the exact measurements needed for a safe controller root override.
- Built and tested default and OpenXR x64 Release flavors. OpenXR SHA-256:
  `E180A16725978AB161A14C66901A2386D1554BF4E79FF6D681CF2426D49F8D05`.

### 0.12.0-native-interaction

- Added a signature-guarded `HPLInteractionBridge` at confirmed native wrapper
  `0x1400cd750`. It routes the dominant controller's tracked world-space aim into
  SOMA's closest-entity query while preserving native length, type, LOS,
  `CanInteract`, range, focus, player-state, and callback ownership.
- Added strict passthrough gates for query type, native camera-origin proximity,
  full controller tracking, active input, and authored-camera ownership, plus
  bounded substitution/hit/fallback telemetry.
- Added confirmed gameplay-HUD virtual center, virtual-size/start, and
  center-screen metric telemetry for resolution-independent HUD capture design.
- Confirmed and promoted the shared Lux entity `SetMatrix` wrapper used by the
  scripted `PlayerHands_*` path. Runtime identity remains the deliberate gate
  before controller-owned hand/viewmodel transforms.
- Built and tested default and OpenXR x64 Release flavors. OpenXR SHA-256:
  `488029E97B313289BBBBD74BE32419635998EC2EF8B9842C86F720955CCACB85`.

### 0.11.0-spatial-ownership

- Added one shared HPL world-pose bridge for the HMD and dominant controller.
  Head, aim, and grip positions use the exact origin and base-view basis already
  proven by the rigid stereo path.
- Added configurable positional audio correction. SOMAVR adds the tracked head
  world offset to the native listener only for the FMOD commit, then restores
  SOMA's fields immediately; orientation correction remains unchanged.
- Added bounded dominant-hand aim/grip telemetry with world position, forward,
  validity, and tracking flags. This is the implementation prerequisite for
  native interaction-ray and viewmodel ownership, without bypassing SOMA's
  `CanInteract`, range, focus, or physics policies.
- Confirmed and promoted seven HUD/ImGui getter wrappers in Ghidra. The GUI-set
  hook now identifies the exact gameplay HUD set through a signature-guarded
  game-context lookup and reports `gameHud=1` plus aggregate matches.
- Built and tested default and OpenXR x64 Release flavors. OpenXR SHA-256:
  `DD8C177B209B4859FBFE69DD7ACA2C4A0F8A31F101D5DBB679B475295CF947B3`.

### 0.10.0-tracking-accessibility

- Preserved OpenXR eye views in a last-known-good cache. `xrLocateViews` now
  writes into temporary storage and only publishes a sample after both stereo
  views and orientation/position validity bits pass validation.
- Added bounded tracking-loss behavior. Cached head/eye poses remain usable for
  `TrackingHoldFrames`, report untracked during the grace period, expire closed,
  and resume through a configurable recovery blackout without clearing stereo
  intent in the native HPL camera bridge.
- Added tracking degradation, loss, restoration, pose-age, fallback-frame, and
  recovery counters to bounded runtime and shutdown telemetry.
- Added configurable dominant-hand actions and left/right stick swap. Touch and
  Index primary/secondary face actions are now bound on both controllers.
- Added a one-controller fallback: the available controller owns movement,
  interaction, jump/crouch, haptics, and a held primary+secondary recenter chord.
  Turn and sprint are deliberately suppressed in this constrained mode.
- Built and tested default and OpenXR x64 Release flavors. OpenXR SHA-256:
  `153E59EDC6F48CA6DB7B10B93EDBC90EED2AA2970AADF878B73237FF7AF39936`.

### 0.9.0-calibration-haptics

- Added configurable OpenXR application spaces. `ReferenceSpace=local`
  preserves the proven seated/recentered path; `stage` selects the floor-aware
  standing space when advertised and falls back to `LOCAL` when unavailable.
- Added an OpenXR vibration-output action and per-hand haptic bindings for
  Simple, Touch, Index, and Microsoft Motion profiles. Interaction, snap turn,
  menu, jump, crouch, and successful recenter now provide discrete feedback.
- Hardened focus transitions. `XR_SESSION_NOT_FOCUSED` clears the complete input
  snapshot immediately and logs bounded loss/restoration transitions, ensuring
  synthetic held inputs release deterministically.
- Confirmed camera base roll at `cCamera+0x4c`, extended/authored roll at `+0x68`,
  and their dirty flags in Ghidra. The bridge now reports both and includes an
  opt-in `HPLNativeCameraRollSuppression=1` comfort policy; it remains off in the
  active config until tested in authored camera sequences.
- Built and tested default and OpenXR x64 Release flavors. OpenXR SHA-256:
  `B26C6911E859099A619ED955E49302B315A2A6D3BBCCF4B052842079DBDCA8E0`.

### 0.8.0-resilience-comfort

- Added automatic OpenXR recovery for session `EXITING`/`LOSS_PENDING` and
  instance-loss events. SOMAVR now destroys stale session/instance resources,
  waits a configurable frame delay, and reboots the runtime while preserving
  the user's stereo intent.
- Added explicit AFR cache invalidation and automatic stable-pose re-arming when
  SOMA replaces the active player or camera during save/load or map transition.
- Added bounded OpenXR comfort-black frames for snap turns and completed
  recenters. Frame pacing continues normally; only projection-layer submission
  is omitted for the configured number of frames.
- Promoted seven post-effect vtable identities from Ghidra. Active ImageTrail,
  ChromaticAberration, and RadialBlur effects are temporarily disabled only
  during active stereo VR compositor calls and restored immediately afterward.
  ToneMapping, FXAA, ImageFadeFX, and VideoDistortion remain enabled.
- Added priority lookup from the composite tree and named inventory/isolation
  logs. Ctrl+F12 diagnostics continue to override the normal comfort policy.
- Added a signature-guarded `HPL3_GuiSet_Render` hook with per-set 2D/3D flags,
  virtual dimensions, offsets, depth range, priority, framebuffer/program state,
  and draw-call deltas for HUD/diegetic classification.
- Built and tested default and OpenXR x64 Release flavors. OpenXR output remains
  `build-openxr-controller\Release`.

### 0.7.2-render-state-policy

- Extracted signature-guarded native player inspection from `HPLInputBridge`
  into reusable `HPLPlayerState` ownership for future locomotion, interaction,
  hands, comfort, and authored-camera adapters.
- Confirmed `cCamera::GetRotateMode` reads camera `+0x6c` and
  `iCharacterBody::Get/SetCameraUpdateActive` reads/writes body `+0x1e8`.
  Matching HPL2 source establishes Euler mode `0`; SOMA's shipped
  `PlayerHandsHandler` switches to matrix mode and disables body camera updates
  while attaching the camera to a hand bone.
- Added transition telemetry for camera mode/body ownership and a default-on
  controller policy that releases movement, turning, run, jump, crouch, and
  interaction during authored camera ownership. Menu and recenter remain live.
- Expanded all six HPL render-stage samples with draw, viewport, framebuffer,
  program, and clear deltas plus complete blend/depth/scissor/write-mask state.
  F6 `draws.csv` rows now identify their enclosing HPL render stage.
- Added a bounded active post-effect inventory using the confirmed composite
  vector at `+0x340/+0x348`, including object pointer, vtable RVA, active flags,
  input texture, render target, and transition detection.
- Added reversible selective post-effect diagnosis: `Ctrl+F12` cycles through
  currently active effects and renders only the selected effect for that call;
  `Shift+F12` restores the normal chain. Plain F12 retains the all-effect bypass.
- Promoted eight authored-camera, HUD, and post-effect helpers in Ghidra with
  names, prototypes where known, evidence comments, and subsystem tags.
- Built and tested OpenXR/default x64 Release flavors. OpenXR output:
  `build-openxr-controller\Release`.

## 2026-07-14

### 0.7.1-gameplay-actions

- Confirmed SOMA's shipped action defaults in `script/base/InputHandler.hps`:
  Space jump, Left Control toggle crouch, Left Shift hold run, left mouse
  interact, and right mouse interaction cancel.
- Added dedicated OpenXR jump and crouch actions. Oculus Touch and Valve Index
  bind right A to jump and right B to crouch; profiles without confirmed face
  buttons remain deliberately unbound.
- Moved Touch/Index `select` to trigger click, matching OpenXR semantics and
  avoiding the previous face-button/interact overlap.
- Added left-trigger hold-run, A/Space jump, and B/Left-Control crouch to the
  reversible SOMA input-path prototype. All held run state participates in the
  existing stale-input, F10-disable, and teardown release policy.
- Built and tested OpenXR/default x64 Release flavors. OpenXR output:
  `build-openxr-controller\Release`.

### 0.7.0-controller-prototype

- Added `HPLInputBridge` as a separate gameplay-input and native-player probe
  owner; camera and OpenXR runtime modules remain focused on their existing jobs.
- Added left-stick W/A/S/D locomotion with press/release hysteresis, configurable
  snap or smooth right-stick turn, right-trigger/select interaction, menu/Escape,
  and a two-grip hold that requests the proven recenter latch.
- Added fail-quiet input lifetime: all held keys/buttons release when VR mode is
  disabled, input is inactive/stale, or the bridge is removed.
- Resolved and signature-guarded `GetPlayer`, player state ID, and move-state ID
  getters. Bounded telemetry now correlates player, camera, body, active camera,
  authored state, move state, and OpenXR controls.
- Updated the shared Ghidra database with seven player/input names and evidence
  comments. Updated address, RE, traceability, state, test, and build docs.
- Built OpenXR x64 Release and passed the render-math suite. Output:
  `build-openxr-controller\Release`.

### 0.6.0-input-foundation

- Added `OpenXRInput` as a dedicated action/pose owner rather than expanding
  `OpenXRRuntime::Impl` with gameplay semantics.
- Added a runtime-neutral `somavr_gameplay` action set with move, turn, select,
  analog trigger, squeeze, menu, grip-pose, and aim-pose actions. Initial
  suggested bindings cover Khronos Simple, Oculus Touch, Valve Index, and
  Microsoft Motion Controller profiles.
- Attached the action set before session begin, created left/right grip and aim
  spaces, synchronized actions at predicted display time, and exposed a stable
  `OpenXRInputSnapshot` for future locomotion, hands, HUD pointer, and interaction
  bridges. This build observes input only and cannot move or interact with SOMA.
- Added bounded `openxr_input` state telemetry for sticks, buttons, squeeze, and
  pose validity, enabled in the active development config with
  `[OpenXR] InputEnabled=1`.
- Added pose-age telemetry to head and stereo snapshots, `openxr_view` summaries,
  and `hpl_stereo` rows to prepare tracking-loss and stale-pose policy work.
- Added configurable vertical room-scale policy and calibrated eye-height offset.
  Defaults preserve the proven camera path: `HPLRoomscaleVertical=1` and
  `HPLEyeHeightOffsetMeters=0.0`.
- Added an automatic `somavr_build_manifest.txt` beside each DLL with version,
  flavor, OpenXR bit, artifact name, and SHA-256.
- Built and tested default/OpenXR x64 Release flavors. OpenXR output:
  `build-openxr-input-foundation\Release`.

### 0.5.11-recenter

- Accepted the user-confirmed `0.5.10-poselatch` run as the current graphics
  baseline: one F10 enabled tracking, AFR stereo, full projection centering, and
  clean shutdown with no observed shadow/reflection regressions.
- Added an in-session F2 recenter control gated by `[Hooks] HPLRecenterControl`.
  Recenter does not restart OpenXR, disable stereo, or leave VR mode.
- Reused the same fully-tracked, eight-stable-frame neutral-pose latch as F10.
  While F2 is waiting for stable samples, SOMAVR continues rendering with the
  previous neutral pose rather than dropping back to the unmodified camera.
- F2 atomically replaces `neutralOrientation` and `neutralPosition`, resets AFR
  eye alternation to the left eye, and logs `hpl_recenter requested`,
  `calibration_wait`, `calibration_reset`, and `applied` rows.
- Built default and OpenXR x64 Release flavors. OpenXR output:
  `build-openxr-recenter\Release`.

### 0.5.10-poselatch

- Triaged the first `0.5.9-rotationfix` live run. The world remained rigid, but
  F10 calibrated against OpenXR frame `2857` at head `Y=-1.244683`; the next
  frame settled near `Y=+0.543`, producing an artificial upward offset of about
  `1.79` metres and placing the camera through the roof.
- Confirmed this is a one-key startup calibration regression rather than an HPL
  world-scale or projection fault. The older manual F8/F10 sequence naturally
  allowed the OpenXR reference space to settle before neutral capture.
- Exposed OpenXR orientation/position tracked flags to the camera bridge. F10
  calibration now requires both tracked bits rather than validity alone.
- Added a deterministic neutral-pose latch: eight consecutive unique poses must
  remain within `0.25 m` and `45 degrees` per frame. A startup reference-space
  discontinuity resets the latch instead of becoming permanent room-scale motion.
- Added bounded `calibration_wait` and `calibration_reset` telemetry plus unit
  tests for duplicate frames, large origin jumps, stable-frame accumulation, and
  equivalent quaternion signs.
- Kept the `0.5.9` rigid-rotation fix, full projection centering, AFR stereo,
  room-scale policy, audio correction, and clean shutdown path unchanged.
- Built default and OpenXR x64 Release flavors. OpenXR output:
  `build-openxr-poselatch\Release`.

## 2026-07-13

### 0.5.9-rotationfix

- Diagnosed the user-reported yaw/pitch skew in `0.5.8-onekey` as a camera-math
  regression introduced during the deterministic math extraction.
- Fixed the quaternion-to-matrix XY cross-term, which incorrectly used `2*y*y`
  where `2*x*y` was required. The resulting non-orthogonal matrix sheared the
  HPL view as the headset rotated.
- Added regression tests that require the generated rotation basis to remain
  unit length and mutually orthogonal, and require matrix rotation to agree with
  the independent quaternion-vector implementation.
- Kept F10 one-key activation, full projection centering, room-scale translation,
  AFR stereo, compatibility controls, and the OpenXR submission policy unchanged.
- Built default and OpenXR x64 Release flavors. OpenXR output:
  `build-openxr-rotationfix\Release`.

### 0.5.8-onekey

- Promoted F10 from a camera-only toggle to the normal VR-mode control.
- F10 now requests OpenXR manual startup through `OpenXRRuntime`, waits
  asynchronously for valid head and stereo views, calibrates the neutral pose,
  enables native head tracking and AFR stereo, and forces the proven fully
  centered projection policy.
- A second F10 press cancels a pending activation or exits tracking/stereo and
  restores SOMA's base camera. F8 and F11 remain available as low-level runtime
  and stereo diagnostics but are no longer required for normal activation.
- Added bounded `hpl_vr_mode requested/activated/cancelled/disabled` telemetry
  and API-attributed OpenXR manual-start logging.
- Built default and OpenXR x64 Release flavors. OpenXR output:
  `build-openxr-onekey\Release`.
- Established the first maintainability baseline without changing runtime
  behavior or the build version: extracted `HPLCameraMath`,
  `OpenGLMatrixAnalysis`, and `OpenXRHelpers`; added deterministic render-math
  tests and `ARCHITECTURE.md` ownership rules. Both Release flavors and tests pass.

### 0.5.7-fullcenter

- Triaged `0.5.6`: F3 patched program `985` for `369` draws with no visual
  effect, rejecting view-depth reflection fade as the window/oven artifact owner.
- F4 showed little translation dependence; the user isolated dynamic-shadow
  motion primarily to HMD pitch and roll.
- Confirmed the prior centered policy only removed horizontal asymmetry. The
  vertical projection center remained `-0.193187` in every stereo row.
- Extended centered projection to both axes while preserving each eye's original
  horizontal and vertical tangent span. OpenXR submission uses the same modified
  FOV as rendering.
- Accepted clean shutdown: the corrected lifecycle hook installed, logged
  pre-graphics OpenXR shutdown begin/complete, and SOMA exited normally.
- Built default and OpenXR x64 Release flavors. OpenXR output:
  `build-openxr-fullcenter\Release`.

### 0.5.6-stability

- Accepted F5 as a successful stereo-compatibility result. Shadow UBO offset
  `96` changes from opposite `-0.242513/+0.242513` eye projection centers to
  `0/0`, exactly matching the user-confirmed convergence.
- Made centered horizontal projection the active development default while
  retaining F5 as a reversible comparison.
- Added F4 room-scale isolation. Disabled mode removes tracked head-center
  translation but preserves eye separation and head orientation, testing whether
  camera-relative shadow/light state owns the remaining movement.
- Identified program `988` as a depth-driven translucent/refraction path with
  block-backed reflection size and fade parameters. Added F3 to bypass the
  view-depth reflection fade around affected draws and immediately restore the
  authored UBO values.
- Corrected the pre-graphics lifecycle signature to include the leading `0x40`
  byte confirmed in the installed executable and Ghidra. `0.5.5` correctly
  failed closed rather than installing against a mismatched guard.
- Updated Ghidra's lifecycle comment/bookmark and built default/OpenXR x64
  Release flavors. OpenXR output: `build-openxr-stability\Release`.

### 0.5.5-reconstruct

- Analyzed three successful F6 captures covering both AFR eyes. Direct temporal
  and inverse camera matrices alternate correctly, while the live deferred shadow
  programs `942/944` obtain their reconstruction camera packet from uniform
  blocks rather than direct uniforms.
- Confirmed the live world reflection/water program `989` uses the same
  uniform-block route for inverse projection/view and screen-space reflection
  parameters. This matches the observed shadow displacement and gives both
  defects a common reconstruction hypothesis.
- Extended F6 to snapshot bound uniform-buffer ranges per program and eye,
  including block/member metadata and raw 32-bit values.
- Added an F5 projection-center A/B. It preserves eye translation and vertical
  FOV but temporarily makes each eye's horizontal projection symmetric; the
  submitted OpenXR FOV is kept identical to the rendered FOV.
- Analyzed `Soma_NoSteam-10384.dmp`: only SOMA's main thread survived, stopped in
  OpenGL with Virtual Desktop runtime frames. No SOMAVR worker thread remained.
- Mapped and named `HPL3_cSDLEngineSetup_Destructor` at `0x1403b16e0` in Ghidra.
  A signature-guarded lifecycle hook now shuts OpenXR down before HPL deletes
  Graphics and calls `SDL_Quit` at `0x1403b1803`.
- Built x64 default and OpenXR Release flavors successfully. The OpenXR test
  output is `build-openxr-reconstruct\Release`.

## 2026-07-12

### 0.5.4-renderdiag

- Triaged `0.5.3-shadowjitter`: F7 toggled at frames `2072`, `3098`, and `3197`, but every row reported `uploads=0 overrides=0`. The correct conclusion is that the live shader never reached the targeted uniform, not that a zero radius failed visually.
- Added reflection RE. SOMA supports eye-vector cubemap reflections and screen-coordinate world reflections; HPL2 renders the latter from a mirrored current frustum into a reused reflection buffer.
- Added F6 four-frame render capture spanning both AFR eyes twice. It writes per-eye draw order/FBO/program data, all matrix uploads, active uniform inventories, and attached generated GLSL sources under `logs\render-captures`.
- Added shader-source classification for shadow, reflection, environment, temporal, and water paths.
- Added current AFR eye and render-pose frame to the camera bridge status so diagnostic rows have explicit eye ownership.
- Diagnosed the lingering process as an orphaned SOMAVR worker: the observed process had no window and exactly one thread. The worker now returns when it is the process's final thread, avoiding the prior stop-event/DLL-detach cycle.
- Added `somavr_dumper.exe`, which creates a thread-aware minidump by PID or executable name and supports optional `--full` memory capture.
- Built the x64 default Release flavor successfully. The OpenXR test output is `build-openxr-renderdiag\Release`.

### 0.5.3-shadowjitter

- Triaged the successful `0.5.2-audiopost` run. It reached game frame `10920`, `6350` stereo submissions, and `6571` total submissions without OpenXR failure, stereo suspension, signature mismatch, or hook failure.
- Accepted the user-confirmed audio result as provisionally correct; a stronger directional-source test remains.
- Rejected the all-post-effect chain as the owner of the principal stereo defect. F12 mainly changed contrast while realtime shadows remained different between eyes and moved with player motion.
- Mapped SOMA's deferred soft-shadow path. `deferred_light_frag.hpsl` selects jitter samples from screen pixel coordinates and scales them by `avShadowMapOffsetMul`; HPL2 confirms this value is uploaded through `glUniform2f`.
- Added targeted shadow/split uniform discovery that remains active after the general uniform-name log budget is exhausted.
- Added signature-independent OpenGL interception for `glUniform2f` and `glUniform2fv`. F7 toggles only `avShadowMapOffsetMul` between authored and zero values, preserving shadow maps, light matrices, and camera state.
- Added bounded `shadow_jitter_upload`, toggle, and shutdown counters. Suppression defaults off and is controlled by `[Hooks] HPLShadowJitterControl` and `HPLShadowJitterSuppressedDefault`.
- Built x64 Release default and OpenXR flavors successfully. The OpenXR output is `build-openxr-shadowjitter\Release`.

### 0.5.2-audiopost

- Triaged the successful `0.5.1-compatprobe` run. F8 started OpenXR at game frame `2296`, F10 enabled native HMD tracking at `2438`, and F11 produced user-confirmed full stereo at `2890`.
- The focused session reached at least `2383` total OpenXR submissions and `1765` stereo submissions with no OpenXR failure, eye-cache failure, hook mismatch, or stereo suspension.
- Confirmed the gameplay render split: world resolves FBO `0 -> 11`, active post effects resolve `11 -> 0`, and final screen GUI remains `0 -> 0`. Post effects averaged about `125 us` in sampled F11 gameplay frames.
- Confirmed hypothesis S11: listener forward/up remained fixed across large HMD quaternion changes and changed only with SOMA's authored camera/state.
- Added orientation-only FMOD correction while F10 is active. SOMA's authored forward/up are rotated by the current physical HMD delta, used only during the native listener commit, then immediately restored. Position and velocity remain authored.
- Added an audio quaternion self-test. Failure disables only listener correction while retaining camera, stereo, and telemetry.
- Added a signature-guarded `HPL3_PostEffectComposite_HasActiveEffects` hook. F12 reversibly bypasses all active post effects for shader-defect A/B testing; default is passthrough and HUD/screen GUI remain active.
- Added `[Hooks] HPLAudioListenerCorrection`, `HPLPostEffectControl`, and `HPLPostEffectBypassDefault`.
- Built x64 Release default and OpenXR flavors successfully. The OpenXR output is `build-openxr-audiopost\Release`.

## 2026-07-11

### 0.5.1-compatprobe

- Integrated the compatibility RE pass as passive runtime telemetry; F8/F10/F11 rendering behavior remains unchanged from `0.5.0-afrstereo`.
- Added `HPLCompatibilityProbe` with independent exact-signature guards for render viewport `0x140298630`, world render `0x1401f9790`, world/3D-GUI callbacks `0x140297670`, post effects `0x14033bd80`, `PostPostEffect` callbacks `0x1401f1480`, final screen GUI `0x1402981e0`, and FMOD listener update `0x140289340`.
- Render-stage samples record a pending render-frame number, nested sequence, viewport and render mask, before/after draw/read framebuffer, shader program, GL viewport, and CPU duration. Initial calls and up to eight calls per 120-frame sample are logged.
- Audio samples record listener position, velocity, forward/up vectors, center-head OpenXR pose, and current F10/F11 bridge state. The hook observes the engine listener commit but does not modify FMOD arguments.
- Added bounded shutdown totals for every stage, audio updates, samples, and successfully installed hooks.
- Added `[Hooks] HPLRenderStageProbe`, `HPLAudioListenerProbe`, and `HPLCompatibilityLogInterval`; both probes are enabled in the active development config.
- Built x64 Release default and OpenXR flavors successfully. The OpenXR test output is `build-openxr-compatprobe\Release`.

## 2026-07-10

### 0.5.0-afrstereo

- Triaged the successful live `0.4.0-hplcamera` run. F10 selected the confirmed render-viewport camera and applied native HMD orientation for `1210` consecutive renders before restoring the pristine view.
- Native rotation reached approximately `35.7` degrees, and F9's four spaced buckets showed changing temporal-view and inverse-view-projection matrices. This confirms HMD motion reached HPL3 camera state rather than mouse input.
- OpenXR remained healthy beyond `1800` submitted frames with no frame failure or suspension. Runtime IPD stayed near `0.06852` meters.
- Added F11-gated alternating-eye stereo while preserving F10 mono orientation as the fallback. F11 can be disabled independently and F10 also shuts stereo down before restoring the base view.
- Added per-eye OpenXR pose/FOV snapshots. Each eye view uses the calibrated head-relative position, including runtime IPD and positional head movement, mapped through configurable `HPLWorldScale`.
- Added OpenGL right-handed asymmetric projection generation from `XrFovf`. Eye axes remain parallel; convergence comes from eye translation and off-axis projection rather than camera toe-in.
- Added a projection self-test against SOMA's known 70-degree, 16:9, `0.03-1000` matrix. Failure disables only F11 stereo.
- Added persistent per-eye OpenGL cache textures/FBOs. The current backbuffer updates one cache each game frame; both cached eyes are copied into acquired OpenXR swapchain images and submitted with the exact poses/FOVs used to render them.
- Stereo waits until both eye caches have valid renders, then reports `stereo=1` and bounded capture/submission counters. Eight consecutive cache-capture failures suspend the stereo submission branch.
- Added `[Hooks] HPLStereoAFR` and `HPLWorldScale`. The active value is `1.0` SOMA units per OpenXR meter.
- Built x64 Release default and OpenXR flavors successfully. The OpenXR output is `build-openxr-afrstereo\Release`.

### 0.4.0-hplcamera

- Triaged two successful F9 captures from `0.3.1-cameramap`. Sequence 1 included mouse input; sequence 2 contained only deliberate HMD yaw/roll/pitch and is the clean control.
- In sequence 2 the OpenXR head quaternion changed from approximately `-0.00615,-0.58891,0.02084,0.80791` to `-0.07509,-0.59820,0.03994,0.79682`, while the sampled SOMA camera matrices did not follow it. This proves pose capture is live and the native game camera is still independent.
- Stable matrix-upload stacks mapped `+0x55ab0a` to the GLSL setter, `+0x435b33` to low-level `SetMatrix`, `+0x2ac5f0` to `iRenderFunctions::SetProjectionMatrix`, and `+0x2ad979` to normal frustum projection selection.
- Matched `0x140271b80` to `cCamera::GetFrustum` and `0x140270230` to `cFrustum::SetupPerspectiveProj` using Ghidra and the released HPL2 source. The latter updates view-projection, culling planes, sphere, vertices, and bounding volume, making it safer than late uniform mutation.
- Added `HPLCameraBridge`, guarded by exact prologue signatures for both target functions plus the confirmed main render-viewport return RVA `+0x298697`. A mismatch disables only this branch.
- F10 now toggles an orientation-only bridge. Enabling captures the current OpenXR orientation as neutral and locks the current perspective camera; disabling immediately restores SOMA's pristine cached view.
- The hook runs at `cCamera::GetFrustum` on every camera query, then calls native `SetupPerspectiveProj`, so headset-only movement can update the frustum even when SOMA did not mark its own camera dirty.
- Added base-matrix preservation keyed by SOMA's base/secondary frustum dirty flags to prevent HMD rotation from accumulating across frames. Orthographic frustums and non-camera perspective ranges are rejected.
- Added bounded native-camera candidate, calibration, application, pose-miss, and shutdown summary logging. F9 capture remains enabled for downstream matrix confirmation.
- Changed F9 full-matrix sampling from the first four matching uploads to four evenly spaced capture-window buckets, so the next log can correlate early, middle, and late HPL matrices with the arm/complete OpenXR poses.
- Built x64 Release default and OpenXR flavors successfully. The OpenXR test output is `build-openxr-hplcamera\Release`.

### 0.3.1-cameramap

- Triaged the successful `0.3.0-xrframe` live run. F8 triggered immediately at game frame `2783`; both `2688x2880` `GL_SRGB8_ALPHA8` eye swapchains were created with three complete FBO-backed images each; the session progressed through `READY`, `SYNCHRONIZED`, `VISIBLE`, and `FOCUSED`.
- The run completed at least `938` consecutive projection-layer submissions with changing eye poses, `openxrFrameSubmitFailed=0`, and no logged OpenXR errors or suspension. OpenXR transport and frame timing are now considered proven.
- Added an F9 camera-attribution window. It runs for 120 frames and captures only matrix uniforms whose names contain `View` or `Projection`.
- Added one module-relative stack trace per unique uniform callsite, suitable for direct RVA navigation in Ghidra. Plain absolute addresses are not used as the durable anchor.
- Added up to four full 4x4 samples per camera uniform, including program, location, transpose flag, and render-frame number.
- Added compact OpenXR pose telemetry with pose-validity flags, center-head position/orientation, pose frame, and measured IPD so matrix changes can be correlated with deliberate headset movement.
- Added `[Hooks] MatrixCapture`, `MatrixCaptureFrames`, `MatrixCaptureStackDepth`, `MatrixCaptureMaxSites`, and `MatrixCaptureSamplesPerUniform`.
- The current `0.3.0` DLL was locked by the still-running SOMA process, so the OpenXR flavor was built successfully to `build-openxr-cameramap\Release`. The default flavor was built normally to `build\Release`.

### 0.3.0-xrframe

- Triaged the successful `0.2.7-xrmanual` live run: F8 triggered at game frame `4200`, `VirtualDesktopXR 1.0.10` accepted SOMA's active `hglrc=0x30000`, the session reached `READY`, two views and seven swapchain formats were reported, and the session stayed alive through later gameplay frames.
- Fixed the F8 reliability issue discovered from that run. OpenXR had been updated only when a `frame_summary` was emitted, so a `FrameSummaryInterval=120` setting sampled F8 once every 120 frames. OpenXR now runs at every real `SwapBuffers`; telemetry remains independently throttled.
- Added `OpenXRGLBridge`, following UEVR's runtime/backend separation. It selects a runtime-supported color format, creates one OpenGL swapchain per eye at the runtime-recommended size, enumerates images, and validates an FBO for each image.
- Added session lifecycle handling: begin on `READY`, end on `STOPPING`, and stop submission on session loss.
- Added a complete frame path: `xrWaitFrame`, `xrBeginFrame`, `xrLocateViews`, swapchain acquire/wait/release, mirrored SOMA backbuffer blits, and `xrEndFrame` with a two-view projection layer.
- Added `[OpenXR] FrameSubmit`, `MirrorBackbuffer`, and `ResolutionScalePercent`. The active config enables all three at 100 percent resolution after manual F8 start.
- Added bounded frame-error logging and automatic suspension after 60 consecutive submission failures so the experimental presentation branch does not repeatedly hammer SOMA or the runtime.
- Reviewed local UEVR architecture and Praydog's reverse-engineering write-up. The applicable decisions are recorded in `docs\UEVR_LEARNINGS.md`.
- Built x64 Release default and OpenXR flavors. The OpenXR output is `build-openxr\Release\somavr.dll` with `version=0.3.0-xrframe`.

### 0.2.7-xrmanual

- Triaged live `0.2.6-xrhold`: OpenXR session creation succeeded on the main render context at frame `120`, stayed alive through the configured `HoldFrames=600` window, then released at frame `720` with `openxr_runtime released_after_probe reason=hold_complete`.
- Later summaries continued after release with `openxrSessionAlive=0` and `openxrInstanceAlive=0`, giving us a clean manual-start target for loading a save first.
- Added `[OpenXR] ManualStart=1` / `StartOnF8=1` support. When enabled, SOMAVR keeps the launch-time hooks installed but defers OpenXR bootstrap until F8 is pressed.
- Added `openxr_manual_start waiting` and `openxr_manual_start triggered` rows, plus summary fields `openxrManualStart=`, `openxrManualStartArmed=`, and `openxrManualStartFrame=`.
- Updated the active runtime config for the next pass: `Probe=1`, `SessionProbe=1`, `ReleaseAfterProbe=0`, `BootstrapFrame=120`, `HoldFrames=0`, and `ManualStart=1`.
- This build still does not call `xrBeginSession`, create swapchains, or submit frames. It is a stable "start XR after save load" gate before the first real OpenXR presentation work.
- Built x64 Release default and OpenXR probes. The flavor files now report `version=0.2.7-xrmanual`, with `openxr=0` in `build\Release` and `openxr=1` in `build-openxr\Release`.

## 2026-07-09

### 0.2.6-xrhold

- Triaged live `0.2.5-xrframeprobe`: OpenXR bootstrap deferred as intended. Frame `120` still showed `openxrAttempted=0`, then the OpenXR probe ran on the frame context `hdc=0x420117aa hglrc=0x30000`.
- `xrCreateSession` succeeded on the frame context, reached `READY`, reported the same `VIEW/LOCAL/STAGE` reference spaces and seven GL swapchain formats, and released cleanly.
- Later frame summaries continued through frame `3240` with `openxrSessionAlive=0`, `openxrInstanceAlive=0`, `openxrSessionReleasedAfterProbe=1`, and `openxrSwapchainFormats=7`.
- Windows Error Reporting still showed no newer `Soma_NoSteam.exe` crash after the frame-context one-shot.
- Added `[OpenXR] HoldFrames=600`. With `ReleaseAfterProbe=1`, a successful session probe now keeps the frame-context OpenXR session alive for the configured frame window, polls OpenXR events, then releases with `openxr_runtime released_after_probe reason=hold_complete`.
- This build intentionally does not call `xrBeginSession`, create swapchains, or submit frames. It only tests whether VirtualDesktop tolerates a live OpenXR session inside SOMA for a short controlled window.
- Built x64 Release default and OpenXR probes. The flavor files now report `version=0.2.6-xrhold`, with `openxr=0` in `build\Release` and `openxr=1` in `build-openxr\Release`.

### 0.2.5-xrframeprobe

- Triaged live `0.2.4-xrsessiononeshot`: `xrCreateSession` succeeded, the runtime reported reference spaces `VIEW`, `LOCAL`, and `STAGE`, and `xrEnumerateSwapchainFormats` returned seven GL formats: `GL_RGBA16F`, `GL_SRGB8_ALPHA8`, `GL_RGBA8`, `GL_DEPTH_COMPONENT32F`, `GL_DEPTH32F_STENCIL8`, `GL_DEPTH24_STENCIL8`, and `GL_DEPTH_COMPONENT16`.
- The session reached `READY`, then SOMAVR released both session and instance via `openxr_runtime released_after_probe reason=session_probe_complete releasedSession=1 releasedInstance=1`.
- Later frame summaries continued through frame `2040` with `openxrSessionCreated=1`, `openxrSessionAlive=0`, `openxrSessionReleasedAfterProbe=1`, `openxrInstanceAlive=0`, and `openxrSwapchainFormats=7`.
- Windows Error Reporting still showed no newer `Soma_NoSteam.exe` crash after the one-shot session probe.
- Important nuance: `0.2.4` created the OpenXR session on SOMA's early startup context (`hglrc=0x10000`), while later render frame summaries used the main frame context (`hglrc=0x30000`). `0.2.5` adds `[OpenXR] BootstrapFrame=120` so OpenXR bootstrap/session probing can be deferred to a real frame boundary and use the frame's current `HDC/HGLRC`.
- Updated the active runtime config for the next pass: `Probe=1`, `SessionProbe=1`, `ReleaseAfterProbe=1`, and `BootstrapFrame=120`.
- Built x64 Release default and OpenXR probes. The flavor files now report `version=0.2.5-xrframeprobe`, with `openxr=0` in `build\Release` and `openxr=1` in `build-openxr\Release`.

### 0.2.4-xrsessiononeshot

- Triaged live `0.2.3-xroneshot`: the OpenXR static probe completed, released the instance, and SOMA continued producing frame summaries for more than a minute afterward. The latest run reached frame `2280` with `openxrInstanceAlive=0` and `openxrInstanceReleasedAfterProbe=1`.
- Windows Error Reporting showed no new `Soma_NoSteam.exe` crash after the `0.2.3` run; the previous SOMA crash remained the older `0.2.2` VirtualDesktop runtime crash from before instance release.
- Added `[OpenXR] ReleaseAfterProbe=1` and extended the one-shot pattern to session probing. With `SessionProbe=1`, SOMAVR now calls `xrCreateSession`, logs reference spaces, swapchain formats, and bounded events if creation succeeds, then immediately destroys the OpenXR session and instance.
- Added summary fields `openxrReleaseAfterProbe=`, `openxrSessionAlive=`, and `openxrSessionReleasedAfterProbe=`. Expected safe session-probe summaries should end with `openxrInstanceAlive=0` and `openxrSessionAlive=0`.
- Updated the active runtime config for the next pass: `Probe=1`, `SessionProbe=1`, and `ReleaseAfterProbe=1`.
- Built x64 Release default and OpenXR probes. The flavor files now report `version=0.2.4-xrsessiononeshot`, with `openxr=0` in `build\Release` and `openxr=1` in `build-openxr\Release`.

### 0.2.3-xroneshot

- Triaged live `0.2.2-xrloaderpath`: the prior delay-load crash is fixed. The run loaded `openxr_loader.dll` from `build-openxr\Release`, enumerated `XR_KHR_opengl_enable`, created an OpenXR instance on `VirtualDesktopXR 1.0.10`, found `Meta Quest 3`, confirmed OpenGL requirements `minGL=4.0.0 maxGL=5.0.0`, and reported two recommended stereo views of `2688x2880`.
- `SessionProbe=0` worked as intended: no `xrCreateSession` was attempted, and frame summaries reported `openxrInitialized=1`, `openxrLoaderLoaded=1`, `openxrViews=2`, and `openxrSwapchainFormats=0`.
- The process still crashed later in `VirtualDesktop.LibOVRRT64_1.dll` with exception `0xc0000005`. Since no session was created, the strongest next suspect is lifetime/teardown of a live OpenXR instance/runtime inside SOMA.
- Changed the no-session path to one-shot discovery: when `SessionProbe=0`, SOMAVR now destroys the OpenXR instance immediately after requirements/view/blend discovery and logs `openxr_instance released_after_static_probe reason=session_probe_disabled`.
- Added summary fields `openxrInstanceAlive=` and `openxrInstanceReleasedAfterProbe=` so the next run can prove the instance was not kept alive while SOMA continues starting.

### 0.2.2-xrloaderpath

- Built x64 Release default probe: `build\Release\somavr.dll` and `build\Release\somavr_injector.exe`.
- Built x64 Release OpenXR probe: `build-openxr\Release\somavr.dll`, `build-openxr\Release\somavr_injector.exe`, and `build-openxr\Release\openxr_loader.dll`.
- Triaged the latest `0.2.1-xrpathguard` crash. The DLL was the correct OpenXR build (`buildOpenXR=1`), hooks installed, and the log reached `gl_context_info`, then stopped before `openxr_extensions`. Windows Error Reporting showed `Soma_NoSteam.exe` failing in `KERNELBASE.dll` with exception `0xc06d007e`, which matches a delay-load module-not-found failure.
- Likely cause: `somavr.dll` is injected from `build-openxr\Release`, but the first delayed `xr*` import searches from SOMA's process/search path and does not reliably find `openxr_loader.dll` beside the injected DLL.
- Added explicit `openxr_loader.dll` preload from the directory containing `somavr.dll` before any OpenXR API call. New rows are `openxr_loader_load attempt`, `openxr_loader_load ok`, or `openxr_loader_load failed ... lastError=...`.
- Added `openxrLoaderLoaded=` to OpenXR frame/proof summaries.
- Changed the active runtime config and default config to `SessionProbe=0` for the immediate retry. Expected next run should proceed from `gl_context_info` to `openxr_loader_load ok`, then `openxr_extensions`, `openxr_system`, `requirements_ok`, view/blend rows, and `openxr_session_probe skipped enabled=0`. If loader load fails, it should log the Windows error instead of crashing.

### 0.2.1-xrpathguard

- Built x64 Release default probe: `build\Release\somavr.dll` and `build\Release\somavr_injector.exe`.
- Built x64 Release OpenXR probe: `build-openxr\Release\somavr.dll`, `build-openxr\Release\somavr_injector.exe`, and `build-openxr\Release\openxr_loader.dll`.
- Triaged the latest `0.2.0-xrprobe` log. It was a clean OpenGL telemetry run, but it loaded `D:\Dev Debug\SOMAVR\build\Release\somavr.dll`, so `openxr_config buildOpenXR=0 enabled=1 sessionProbe=1` and no OpenXR runtime discovery was exercised.
- The same run confirmed the lower-noise logging budget worked: the log was about 100 KB, frame summaries appeared every 120 frames, and `a_mtxModelViewProjection` remained the active projection-like uniform at 70 degree vertical FOV.
- Added a generated `somavr_build_flavor.txt` beside each DLL. Default builds write `flavor=opengl/openxr=0`; OpenXR builds write `flavor=openxr/openxr=1`.
- Added injector-side mismatch detection. If `[OpenXR] Probe=1` and the selected DLL is not OpenXR-enabled, the injector prints a warning before injection with the OpenXR build path to use.
- Upgraded the non-OpenXR DLL's OpenXR-unavailable row to an error-level `build_without_openxr` message that explicitly says to use `build-openxr\Release\somavr.dll` or disable `[OpenXR] Probe`.
- Simplified the documented OpenXR launch command: run `build-openxr\Release\somavr_injector.exe --launch ...` and let that injector pick the DLL beside itself.

### 0.2.0-xrprobe

- Built x64 Release default probe: `build\Release\somavr.dll` and `build\Release\somavr_injector.exe`.
- Built x64 Release OpenXR probe: `build-openxr\Release\somavr.dll`, `build-openxr\Release\somavr_injector.exe`, and `build-openxr\Release\openxr_loader.dll`.
- Triaged the first live `0.1.0-bootstrap` log. It confirmed safe injection, NVIDIA OpenGL 4.6, a 3440x1440 viewport, and a shader-uniform projection path. Fixed-function projection remained invalid, while `a_mtxModelViewProjection` matched SOMA's configured 70 degree vertical FOV.
- Reduced default log volume: frame summaries now default to every 120 frames, matrix sampling remains broad enough to see the per-frame projection uniforms, and individual `uniform_matrix` rows are capped and projection-only by default.
- Expanded the OpenXR probe from graphics requirements only to runtime/system/view/session discovery. New expected rows include `openxr_extensions`, `openxr_system`, `openxr_view_configurations`, `openxr_view`, `openxr_blend_modes`, `openxr_session_probe`, `openxr_reference_spaces`, `openxr_swapchain_formats`, and `openxr_event`.
- Added `[OpenXR] SessionProbe=1` so `xrCreateSession` can be disabled independently if a runtime dislikes being probed from SOMA's active OpenGL context.
- Updated the active runtime `somavr.ini` for the next OpenXR run: `Probe=1`, `SessionProbe=1`, `FrameSummaryInterval=120`, `UniformMatrixProjectionOnly=1`, and `UniformMatrixLogLimit=256`.

## 0.1.0-bootstrap

Initial SOMAVR scaffold:

- x64 CMake project.
- launch/attach injector.
- OpenGL/WGL telemetry DLL.
- optional OpenXR build switch with an OpenGL requirements probe.
- first notes from Ghidra and HPL2 source comparison.
