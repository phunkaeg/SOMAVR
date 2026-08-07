# Authored-State Compatibility And Visible Hands RE

Date: 2026-07-22

Feature nodes: `FEATURE.AUTHORED_STATE_COMPATIBILITY`,
`FEATURE.VISIBLE_HANDS`, `FEATURE.AUTHORED_CAMERA`, and
`FEATURE.PHYSICAL_MANIPULATION`.

This is the first focused RE pass for preserving SOMA's authored player
sequences in VR and rendering tracked hands. Evidence comes from shipped HPL3
scripts/assets, released HPL2 source, the current SOMAVR bridge, and confirmed
`Soma_NoSteam.exe` functions.

No camera or bone mutation was enabled by this pass. Build
`0.69.0-authored-hands-probe` adds passive state-ownership and skeleton probes.

## 0.83 Shared Root And Palm-Basis Finding

Live Frida reads against the retained apartment hands entity resolve both
`j_L_Clavicle` and `j_R_Clavicle` to the same parent, `j_Root`; its parent is
`BoneStateRoot`. After medicine, `BoneStateRoot` remained at world Y about
`1.4916`, while `j_Root` local translation became
`(0.0, 0.6643875, -0.0431139)` and its world Y became about `2.1573`. The
existing 34-node side restores begin at each clavicle and therefore could not
remove this shared upstream offset.

Version 0.83 adds a single shared-root pose anchor. It validates the clavicle
parent pointer and post-transform state, seeds the first stable local matrix,
and calls confirmed `cNode3D::SetMatrix` once per game frame before restoring
either side. This preserves the independently body/HMD-anchored entity matrix;
only the authored local lift above `BoneStateRoot` is removed. Drift greater
than 5 cm emits a bounded proof row.

The 0.82 wrist mapping was also structurally nondeterministic: it captured
`inverse(controllerAtTakeover) * wristAtTakeover`, so load timing or an active
native hand animation changed the permanent controller offset. The replacement
derives palm forward from wrist to the mean of the four finger roots and palm up
from `cross(index-to-pinky, palm-forward)`. The invariant
`inverse(modelPalm) * modelWrist` is then applied to the current OpenXR grip
basis. Finger geometry failure is explicit and falls back to the old relative
anchor rather than applying an unverified rotation.

## 0.79 Transition And IK Accumulation Finding

The accepted 0.78.1 log supersedes the original invalidate-on-every-transition
policy. The same `PlayerHands_0` seed remained safe through thousands of Normal
frames, but entering Slide state invalidated it and no later native `SetMatrix`
reseed occurred. SOMAVR therefore stopped synthetic wrist/arm updates while the
already-visible mesh remained in its last world pose. Pause and post-medicine
object interaction exhibit the same ownership pattern.

Retention now has three states. `Active` applies tracked wrists and IK;
`Suspended` preserves the exact entity, mesh, stable root, and native
active/visible lifetime while pause, authored camera, non-Normal state, or
tracking transition owns presentation; `Invalid` clears all cached identity
only when player/camera-control/character-body ownership is genuinely gone.
Native matrices seen during a suspension may animate the current authored
sequence but do not overwrite the stable root used on return. Native entity
destroy remains the final pointer-level eviction.

The same log identifies an independent arm-solver feedback loop. Initial
segment lengths were approximately `0.2796/0.2163` world units; after retained
synthetic updates, the observed lower segment reached `0.5037`. Restoring the
post matrix and flag did not recompute world matrices, so the next frame
measured the previous frame's solved hierarchy. Version 0.79 invokes HPL3's
confirmed post-animation propagation on each shoulder with authored post state
before reading positions and applying the next transient solve. This makes the
animation locals, rather than SOMAVR's prior output, the solver input.

## 0.78 Live Acceptance And Lifetime Finding

The 0.77.2 headset run accepts the full hand chain: `PlayerHands_0` was seeded
natively, normalized from quarter scale, and then received continuous bilateral
wrist and `Arm_1 -> Arm_6 -> Wrist` IK applications from fresh grip poses. The
remaining visual calibration is a shared-root shoulder offset; 0.78 applies
`-0.30` meters on world Y before solving while leaving wrist endpoints at the
tracked controller positions.

Wrist orientation now uses a relative anchor rather than a raw basis takeover.
At the first eligible frame per hand, SOMAVR records both controller and native
wrist orientations. Later target orientation is
`controllerNow * inverse(controllerAnchor) * wristAnchor`. This preserves the
campaign animation's initial hand alignment and transfers subsequent controller
yaw, pitch, and roll. Anchors are discarded on tracking loss, entity change,
pause, authored state, or bridge removal.

`Soma_NoSteam.exe.48860.dmp` proves that cached renderer children cannot be
treated as independently owned. The crash attempted to execute `0x6576` from
the native `cMeshEntity::SetVisible` path while SOMAVR called the retained mesh
after the pause/menu transition. Retention now suppresses native hide/deactivate
only while the exact native seed is eligible; synthetic frames update the
parent entity matrix only. Pause/menu and non-Normal ownership invalidate the
seed before native teardown.

## 0.77 Implementation Promotion

The live arm chain and released mesh weights support a conservative analytic
solve using `j_*_Arm_1` as shoulder, `j_*_Arm_6` as elbow/forearm anchor, and
`j_*_Wrist` as endpoint. Version 0.77 applies transient shoulder and forearm
post rotations, then the proven wrist translation. Native elbow position is the
pole, reach clamps below full extension, authored post ownership fails closed,
and left/right tracking is independent.

The shipped handler creates the model lazily and selects campaign-specific hand
assets. SOMAVR therefore never fabricates a replacement. `HandAlwaysVisible`
starts only after an exact native `PlayerHands_*` seed and retains native
active/visible calls solely in Normal/Normal VR ownership. Ladder, ledge,
interactive-camera, conversation, death, custom, and null states remain native.

Authored physical actions now have a profile/event contract: named entity,
tracked participants, proximity volumes, pose thresholds, hold time, action
edges, haptic acknowledgement, and a separate native-commit adapter. Medicine
is the first profile. Its evidence build leaves commit disabled until bottle
cap axis and mouth/tip thresholds are confirmed in headset.

## Exact Player States

The canonical enum is shipped in `script/player/Player_Types.hps`:

| ID | State | VR classification |
| --- | --- | --- |
| `0` | Normal | Ordinary player ownership |
| `1` | Grab | Physical manipulation |
| `2` | Push | Physical manipulation |
| `3` | Wheel | Physical manipulation |
| `4` | Slide | Physical manipulation |
| `5` | SwingDoor | Physical manipulation |
| `6` | Lever | Physical manipulation |
| `7` | Tear | Physical manipulation |
| `8` | Terminal | Diegetic terminal |
| `9` | HandheldTerminal | Handheld terminal and prop |
| `10` | Read | Story-object presentation |
| `11` | Ladder | Semantic authored locomotion/camera |
| `12` | ClimbLedge | Semantic authored locomotion/camera |
| `13` | MovingButton | Physical manipulation |
| `14` | InteractiveCameraAnimation | Structural authored camera |
| `15` | Sit | Structural authored camera |
| `16` | Conversation | Semantic authored camera |
| `17` | Dead | Authored presentation |
| `18` | ZoomArea | Semantic authored camera and relocation |
| `19` | CustomControls | Map-defined; fail closed |
| `20` | Null | Scripted ownership; fail closed |

`HPLPlayerState` now reports the symbolic state, semantic authored-state class,
camera rotate mode, body camera-update flag, and the actual camera pointer held
by the character body at `+0x1b0`.

## Camera Ownership Model

The original `authoredCameraActive` signal is structural: the player camera is
not in Euler mode or the character body disables normal camera updating. This
catches strong takeovers but cannot identify every authored state.

Telemetry now keeps three signals separate:

- `structuralAuthoredCamera`: matrix mode or body camera update disabled.
- `bodyCameraDetached`: `characterBody+0x1b0` is not the normal player camera,
  including a deliberately null camera.
- `semanticAuthoredState`: a known authored sequence owns pose or presentation
  even if the camera remains Euler/body-attached.

Existing safety behavior still uses the proven structural signal. The semantic
signal is evidence for future per-state adapters.

## State Findings And Candidate Policies

### Sit, State 15

`PlayerState_Sit.hps` calls `CharacterBody.SetCamera(null)`, switches the camera
to Matrix mode, and directly writes rotation and position. Exit restores Euler,
body yaw, camera pitch, and body-camera ownership.

Preserve native seat placement/timing. Compose HMD orientation and bounded
roomscale after the authored seat base, never feed physical head motion into the
script endpoint, and reseed stereo/temporal histories on both transitions.

### Interactive Camera Animation, State 14

This uses the same null-body-camera and Matrix-mode pattern. On exit it moves the
character's feet to the camera position before restoring body ownership.

Preserve animation as the body/base transform. Never contaminate the native
camera position used by exit relocation with roomscale or per-eye offsets.
Compose HMD freedom at final render time.

### Ladder, State 11

Ladder remains structurally close to Euler/body-camera ownership, but forces
body position/yaw, camera pitch/yaw and limits, climb roll, full-scale hand
animations, and a custom hands world transform.

Preserve progress, constraints, and hand animation. Suppress climb roll/bob,
keep HMD look independent of native clamps, and prefer wrist post-animation
correction over replacing the authored full-scale root.

### Climb Ledge, State 12

Climb uses full-scale hands and a custom root. It advances player feet from the
hands mesh `Socket_Camera` bone and authors camera pitch, body yaw, roll, and
limits.

Preserve native animation and socket-driven body progress. Do not replace this
root. Any wrist correction must prove it does not alter camera-socket motion or
animation completion.

### Conversation, State 16

Conversation stays structurally normal while calling `RotateCameraTowards`,
changing FOV and pitch/yaw limits, moving the body toward a conversation point,
and adding conversation head bob.

Allow native body/base turn without forcing physical HMD orientation. Preserve
timing/relocation and continue neutralizing VR-hostile FOV and bob.

### Zoom Area, State 18

Zoom Area uses terminal camera-add, body-feet relocation,
`RotateCameraTowards`, and input suppression. It can therefore be authored
while structural camera signals look normal.

Preserve target and interaction ownership. Prefer physical lean or a stable
overlay to forced zoom, keep HMD orientation independent, and avoid involuntary
relocation unless interaction geometry requires it.

### Null And Custom Controls, States 20 And 19

These cannot share one global policy. Campaign scripts use Null for sequences
such as the WAU flower interaction: input is disabled, full-scale hands animate,
and camera ownership can move to `Socket_Camera`. CustomControls is map-defined.

Fail closed. Observe camera attachment, body-camera pointer, hand scale, and
animation identity before selecting an adapter.

### Dead, State 17

Dead is chiefly a presentation owner. Existing scripted-presentation/HUD work
handles game-over content. It remains semantic-authored so no future locomotion
or hands policy treats it as Normal.

## Shipped Hands Architecture

`PlayerHandsHandler.hps` creates `PlayerHands_N` from
`character/player/hands/hands_human.ent`, disables shadows/reflections, and
activates it as required.

Normal behavior:

- Root scale `0.25`.
- Y rotation of `pi`.
- Approximate offset `(0, -0.3, 0) * scale`.
- 20 percent of native head bob.
- Camera following unless a state supplies a custom transform.

Ladder, climb, and scripted sequences use scale `1.0` and custom transforms.
This validates the existing restriction of controller-root overrides to normal
quarter-scale hands.

Tools are map entities named `<picked-name>_HudObject`, attached to `R_Hand`.
The native handler owns draw, idle, holster, and custom animation callbacks.
Visible hands should retain these entities instead of creating a parallel tool
system.

## Skeleton And Art Pipeline

`hands_human.ent` is one skinned bilateral mesh. Its submeshes divide clothing
and skin, not left and right roots. The skeleton has:

- Left/right clavicle, arm, wrist, and finger chains.
- `Socket_L_Hand` and `Socket_R_Hand`.
- `Socket_Camera`.
- Entity sockets `L_Hand`, `R_Hand`, and `Camera`.

Campaign maps swap among `hands_human`, `hands_diving`, `hands_deepsea`, and
`hands_deepsea_mutilated`. These swaps convey suit/body state and must be
preserved. Poseable and chair-scan variants also exist.

The DAE is COLLADA 1.4.1, Y-up, centimeter units (`meter="0.01"`), exported by
an FBX COLLADA exporter. This is the 3ds Max baseline. Custom models can improve
geometry later, but the shipped skeleton is enough for a first prototype.

## Native Bone Control Surface

Released HPL2 source and HPL3 decompilation agree on animation composition:

```text
animated local = animated local * pre transform
animated local = post transform * animated local
```

Confirmed `cNode3D` layout:

| Offset | Meaning |
| --- | --- |
| `+0x44` | Local matrix |
| `+0x84` | World matrix |
| `+0xc4` | Use pre-transform |
| `+0xc5` | Use post-transform |
| `+0xc8` | Pre-transform matrix |
| `+0x108` | Post-transform matrix |

Post-transform is the leading controller-wrist path because it can adjust each
wrist after native animation. It is not enabled until live evidence proves:

- Bone identity and matrix convention.
- Update phase relative to animation and `PlayerHandsHandler.PostUpdate`.
- Whether correction needs elbow/clavicle distribution.
- Whether climb/camera attachment reads corrected `Socket_Camera`.
- Mesh and bone pointer replacement during model swaps.

Both arms share one root, so independent root transforms require split meshes.
Independent per-bone correction is the less invasive first route.

## Passive Runtime Probe

With `[HPLControllers] HandTrackingProbe=1`, `HPLHandsBridge` now:

1. Resolves the mesh from exact `PlayerHands_*` entities.
2. Resolves both wrists and all three key sockets by name.
3. Reads node world position and pre/post-use flags.
4. Logs on entity/mesh replacement and at the configured interval.

The first live result in `0.73.0` resolved the root question. `PlayerHands_0`
was uniformly `0.25` scale and contained both stable wrist chains. Moving its
root to the dominant right grip therefore produced two miniature hands on the
right controller. Root control is disabled in the stable profile. The `0.74.0`
probe pairs each wrist with its corresponding controller and logs target
position, delta, and distance without mutating the skeleton.

Version `0.75.0` adds twelve-frame bursts whenever the hands mesh or player
state changes. Each wrist row now records local matrix `+0x44`, parent pointer
`+0x180`, parent/world matrices `+0x84`, authored post matrix `+0x108`, and both
native/controller bases. The expected wrist parents are the shipped
`j_L_Arm_10` and `j_R_Arm_10` nodes across all six player-hand entity variants.

The probe also derives a position-only post-transform candidate and reconstructs
the desired world matrix to report numerical error. This is dry-run code:
`SetUsePostTransform`, `SetPostTransform`, and `ApplyPostAnimTransform` are not
called.

### 0.75 Live Wrist Acceptance

The 2026-07-21 headset run captured 73 samples for each wrist. Every one of the
146 wrist rows resolved the expected `Arm_10` parent. Once OpenXR controller
tracking became live, 16 left and 21 right samples produced valid independent
post-transform candidates. All 37 candidates reconstructed successfully; mean
world-position error was `0.000000835` and the worst was `0.0000014` world
units. Observed native-to-controller distances ranged from `0.24` to `0.58`
for the left wrist and `0.41` to `0.66` for the right.

The same run explains the remaining visual presentation. `PlayerHands_0`
stayed at uniform root scale `0.25`, inherited by each wrist and socket, and
its root remained about `0.075` world units from the camera. The tiny hands
floating immediately in front of the face are therefore the untouched native
close-up rig. They are not a failed controller attachment: root mutation was
disabled as intended.

This accepts the math and hierarchy gate for a guarded prototype. The next
mutation must keep shared-root position and orientation authored, normalize
only its uniform scale, then drive `j_L_Wrist` and `j_R_Wrist` independently
with position-only post-animation transforms. It must preserve and restore the
native post matrices/use flags on tracking loss, state exit, mesh replacement,
and entity destruction. Controller orientation remains diagnostic until a
separate wrist-basis calibration is proven.

### 0.76 Position Implementation

Version `0.76.0-fullscale-wrist-position` implements the accepted first stage.
The released `PlayerHandsHandler.hps` confirms `mbUseFullScaleModel` selects
exact root scales `0.25` and `1.0`, so scale normalization uses an authored
size rather than an inferred multiplier. SOMAVR changes only the three root
basis lengths and preserves the complete root position and rotation.

For each wrist, the bridge builds the previously accepted position-only post
matrix from the current parent world, current animated local, and matching grip
position. It then performs this bounded sequence once per game frame:

```text
save authored post matrix and use flag
SetPostTransform(candidate)
SetUsePostTransform(true)
ApplyPostAnimTransform(true)
SetPostTransform(authored)
SetUsePostTransform(authored)
verify exact restoration
```

The flag never remains enabled between frames, preventing old corrections from
being reapplied by the next animation update. Mutation requires Normal player
and move states, no authored camera, fresh two-hand tracking, exact wrist-parent
identity, and native `usePost=0`. `HandScaleNormalization=0` and
`HandWristPosition=0` isolate or fully roll back the prototype.

### 0.76 Live Takeover Timing

The 2026-07-21 `somavr - Copy.log` run explains the brief tiny-hand period
precisely. `PlayerHands_0` appeared at `15:56:16` with native scale `0.25`,
before VR was requested. F10 activated tracking at `15:56:25`. The runtime
reported a right Touch controller at `15:56:28`, but the left profile remained
`none` until `15:56:33`. The first shared-root normalization and both transient
wrist corrections occurred together at frame `2999`, also at `15:56:33`.

This is an eligibility delay, not a failed transform. `ResolveWristTrackingFrame`
currently rejects the whole frame until both grip poses are active, valid,
position-tracked, orientation-tracked, and fresh. Root scale normalization is
then gated by that bilateral result even though changing the shared root from
the authored `0.25` scale to authored `1.0` scale does not depend on either
controller pose.

The next low-risk refinement is to split the gates:

1. Normalize the shared root scale whenever the exact hands identity, Normal
   player/move states, active VR camera ownership, and native quarter-scale
   matrix are valid.
2. Resolve and apply each wrist independently when that hand's grip pose is
   fresh, leaving the other wrist fully native until its controller appears.
3. Keep the existing exact parent, native post-state, restoration, authored
   camera, lifecycle, and tracking-loss checks.

This should remove the visible miniature interval and make controller startup
or reconnect asymmetric without weakening authored-state compatibility.

### Live Medicine And Socket Validation

The same live process was inspected read-only through Frida, Cheat Engine,
ReGenny, and x64dbg. The active player was resolved from the guarded getter,
not from copied heap addresses. The current `PlayerHands_0` instance had stable
bilateral wrist parents, `Socket_L_Hand` parented to `j_L_Wrist`, and
`Socket_R_Hand` parented to `j_R_Wrist`. Both wrists had native `usePost=0`,
identity authored post matrices, and exact restoration after SOMAVR's transient
correction. No debugger breakpoints or memory writes were used.

The visible bottle is the authored `Tracer_Fluid_HudObject`. Released apartment
script passes that exact entity to `PlayerHands_PlayAnimation` for
`tool_tracer_fluid_drink`. `PlayerHandsHandler.hps` then calls
`AttachToSocket(pEntity, "R_Hand", true, true)` and invokes
`UpdateEntityAttachment()` after updating the hands root. The cap-removal timer
also addresses the same entity by name. Live RTTI resolves both `PlayerHands_0`
and `Tracer_Fluid_HudObject` as `cLuxProp`, matching the registered script
surface used by those calls. This confirms the desired ownership
boundary: preserve SOMA's hand animation, `R_Hand` socket orientation, prop
attachment, visibility callbacks, and scripted submesh changes; controller
work should continue at the wrist/arm pose layer rather than moving the bottle
as a separate VR object.

Ghidra confirms the native boundary at `0x1400be8f0`, now named
`SOMA_cLuxProp_AttachToSocket`. Its registered signature exposes rotation,
snap-to-parent, and lock policy. The implementation creates attachment state at
child `cLuxProp+0x740`, resolves the named socket through the parent's mesh, and
stores that socket at attachment `+0xa0`. These are documentation and future
lifecycle-probe anchors, not stable heap addresses or a reason to bypass the
native attachment path.

Live node memory also confirms a native node name at `+0x10`. That field is now
represented in `re/regenny/hands.genny`. Heap identities changed multiple times
during map streaming, so recorded entity, mesh, wrist, and socket addresses are
session evidence only and must never become static offsets.

### Elbow And Shoulder Chain

Live parent traversal extends each wrist through `Arm_10..6`, `Elbow_2/1`,
`Arm_5..1`, `Shoulder`, `Clavicle`, and the shared `j_Root`. Both chains are
symmetric and all sampled nodes retained native `usePre=0 usePost=0`. Released
mesh skinning confirms the numbered arm nodes distribute deformation and twist,
with shirt and hand geometry overlapping around `Elbow_2` and `Arm_6..7`.

This makes full controller-driven arms feasible, but not as independent shoulder
and elbow overwrites. The compatible design is post-animation two-segment IK
with distributed swing/twist, native clavicle/root anchoring, reach limits, an
elbow pole vector, and authored-state blending. The exact chain and staged
implementation are maintained in `FUTURE_SYSTEMS_RE.md`.

Markers:

```text
hpl_hands_skeleton ... meshChanged=...
hpl_hands_bone ... name=j_L_Wrist ...
hpl_hands_bone ... name=j_R_Wrist ...
hpl_hands_bone ... parentMatch=... controllerHand=left/right ...
hpl_hands_bone ... postCandidate={valid=... worldError=...}
hpl_hands_bone ... name=Socket_Camera ...
hpl_hands_bridge_summary ... skeleton={...}
hpl_player_state ... playerStateName=... bodyCameraDetached=...
    structuralAuthoredCamera=... semanticAuthoredState=...
```

## 2026-07-22 Ownership Correction

The 0.79 headset log resolves three prototype ambiguities:

- `ApplyPostAnimTransform` mutates the animated local matrix and returns when
  `UsePostTransform` is false. Clearing the flag does not undo the mutation.
  Repeated solves therefore grew the lower-arm segment from about `0.216 m` to
  as much as `0.70 m`. Version 0.80 caches the authored local matrices and
  restores shoulder, elbow, and wrist with `cNode3D::SetMatrix` before solving.
- The retained root followed camera translation only. Its shoulder basis stayed
  in the original world heading after body turns. The body frame is now
  reconstructed as `headWorld * inverse(rawHmd)`, reduced to yaw, and used to
  rotate the root-camera offset and root orientation around the camera pivot.
  This excludes physical HMD head turns from torso heading.
- The native camera origin is still capsule-owned and omits room-scale head
  translation. Version 0.80.1 replaces the positional pivot with the camera
  bridge's safety-clamped `headWorldPosition`, retaining the first root-to-head
  vector as a fixed shoulder/neck offset. Capsule position remains fallback.
- The mode gate suspended all non-Normal states, including `Grab`. States
  `Normal` through `Tear` (`0..7`) now retain wrist/arm tracking while pause,
  terminal, Read, and authored camera states remain suspension boundaries.

Wrist orientation anchors are now sampled after authored-local restoration and
before IK. This removes the controller-pose-dependent alignment that occurred
when the first anchor was captured from an already solved wrist.

## Implementation Sequence

1. Add state policies for `normal`, `body-base`, `animation-base`, `terminal`,
   `presentation`, and `native-fail-closed`.
2. Implement Normal-only position-only visible wrists behind a hard rollback,
   including root-scale normalization without shared-root pose takeover.
3. Capture passive/active transitions in Read, ladder/climb, terminal,
   conversation, Sit, death, and one Null/CustomControls sequence.
4. Add elbow/clavicle distribution if wrist-only correction stretches.
5. Add Read and handheld-tool policies.
6. Validate ladder/climb with native root pose and camera socket untouched.
7. Only then add controller orientation, model-specific calibration, or
   replacement 3ds Max assets.

## 2026-07-23 Full-Pose And Manipulation Evidence

The 0.80.1 headset log separates three independent ownership failures:

- The root-to-head translation was correct, but reconstructed
  `headWorld * inverse(rawHmd)` yaw did not follow observed player turns. The
  native camera's horizontal forward vector is stable body/capsule evidence and
  is now the primary retained-root yaw source.
- Shoulder, elbow, and wrist restoration was insufficient. Immediately after
  synthetic retention began, the solved elbow rose from roughly 1.38 to 1.78
  world metres even though the shared root moved down. Parent traversal and
  shipped mesh names show 34 relevant nodes per side from clavicle through the
  finger tips. Native animation can therefore lift or twist intermediate nodes
  while three visible anchors appear restored.
- Curtain and drawer sessions enter player state 13 `MovingButton`. The old
  eligibility set stopped at states `0..7`, explicitly restoring/releasing the
  rig and discarding wrist orientation anchors. State 13 is now retained; UI,
  terminal, Read, and authored-camera states remain fail-closed boundaries.

Version 0.81 caches and restores the complete local-pose chain before each IK
solve. With `HandFreezePose=1`, fingers are included, preventing the medicine
animation from closing or rotating them while the bottle remains attached by
SOMA's native `R_Hand` socket. This is deliberately pose ownership, not prop
ownership: the authored bottle, cap timer, visibility callbacks, and script
progression remain native.

Version 0.82 keeps elbow correction as a pole preference rather than a hard
joint translation. The solver selects a body-local down/out/back pole, blends
toward previous torso-local elbow history near vertical singularities, and
limits per-frame swivel. `HandArmIKElbowDownMeters` scales the downward
preference while the reach-clamped solve continues to preserve arm lengths.
Near full extension only, the matching clavicle may contribute a smoothed,
bounded shoulder offset; ordinary hand motion leaves the shoulder unchanged.

The cross-engine design contract is now documented in
[Torso Calculations & Ergonomics](../../VR%20Modding/docs/12-torso-calculations-and-ergonomics.md).
It treats head and hands as measured endpoints while body yaw, shoulder anchors,
and elbow swivel remain explicitly inferred variables. SOMAVR 0.82 implements
the deterministic baseline plus hand-conditioned shoulder reach, torso-space
swivel selection, singularity continuity, and bounded swivel rate. Headset
acceptance and per-user or per-avatar anthropometric calibration remain future
work rather than claimed current behavior.

## Confirmed Native Addresses

Canonical details live in `ADDRESS_REGISTRY.md`. Ghidra now names:

- `0x14023b110` `HPL3_CharacterBody_SetCamera`
- `0x140071f10` `HPL3_Viewport_SetCamera`
- `0x140270d70` `HPL3_Camera_SetRotateMode`
- `0x140270ee0` `HPL3_Camera_SetRotationMatrix`
- `0x140165270` `SOMA_iLuxEntity_GetMeshEntity`
- `0x1401fdc30` `HPL3_MeshEntity_GetBoneState`
- `0x140200980` `HPL3_MeshEntity_GetBoneStateFromName`
- `0x140200420` `HPL3_MeshEntity_GetSocket`
- `0x14023f600` / `0x14023f820` world matrix/position
- `0x14023f610` `HPL3_Node3D_GetParent`
- `0x14051a310` `HPL3_Node3D_GetLocalMatrix`
- `0x14023fee0` `HPL3_Node3D_SetMatrix`
- `0x1404a9470` / `0x1404a9480` pre/post getters
- `0x140031c90` / `0x1404a9490` pre/post enable setters
- `0x140031ca0` / `0x1404a94a0` pre/post matrix setters
- `0x1402401d0` / `0x140240290` pre/post apply functions
## 0.84 Persistent Visibility And Socketed Prop Boundary

The released `PlayerHandsHandler.hps` makes the remaining always-visible limit
explicit. `CreateWorldEntities` is empty. `CreateHandModelIfNeeded` creates
`PlayerHands_<n>` only when an authored hand animation first requests it, using
the current campaign-selected `msHandModel` (`hands_human`, `hands_diving`,
`hands_deepsea`, or mutilated variants). `HandAlwaysVisible` can safely reject
later inactive/hidden transitions and synthesize tracked updates only after this
native entity exists. It cannot create the correct model before that point.

The next safe implementation rung is therefore a guarded call into the live
player-hands handler's own `CreateHandModelIfNeeded(currentMap)`, followed by
native `SetActive(true)`. It must resolve the handler and current model from live
state, run only after map creation, preserve model swaps and map destruction,
and be disabled for missing/invalid ownership. Loading
`hands_human.ent` directly from the DLL would be wrong for later campaign states
and is not an acceptable shortcut.

For the apartment medicine sequence, the prop already has correct native
lifecycle ownership: `Tracer_Fluid_HudObject` is attached to `R_Hand`. Version
0.84 adds an exact-name transform adapter after the prop reaches that hand. It
caches `inverse(rightGripWorld) * nativePropWorld`, then submits
`rightGripWorld * cachedRelative` each update. This suppresses authored bottle
rotation/jiggle while retaining creation, attachment, cap script, visibility,
and destruction. Future authored objects must use separate named profiles; the
generic `*_HudObject` class remains untouched.

### 0.84.1 Retained Scale Precedence Finding

The 0.84 headset log proved a subtle false-positive in the original telemetry.
The SetMatrix hook normalized the current native root from `0.25` to `1.0`, but
the retained body-anchor stage ran later and rebuilt the outgoing matrix from a
quarter-scale seed captured before F10. Thus `hpl_hands_scale normalized=1` was
true about an intermediate matrix while HPL still received quarter scale. The
IK lengths of `0.0699 m` and `0.0541 m`, exactly one quarter of the established
arm dimensions, corroborate the final visual result.

The corrected ownership order is: native authored matrix, guarded root-scale
normalization, retained body translation/yaw, then arm and wrist corrections.
Retained body pose may not replace the accepted current-frame basis scale.
Scale eligibility is camera/state based and independent of controller poses;
per-hand pose availability continues to gate wrist and IK mutation separately.

## 0.85 Terminal Compatibility And Read Presentation Ownership

The 0.84.1 live log resolves the terminal hand-loss question without another
native hook. At terminal entry frame 4738, HPL reported player state 8 with
`moveState=0`, `structuralAuthoredCamera=0`, matching active/body camera,
`tracking=1`, and `paused=0`. SOMAVR suspended retained hands on that exact
transition because state 8 was absent from its compatibility allowlist. State 8
is therefore a compatible tracked-hands state, while terminal input ownership
and locomotion suppression remain responsibilities of their existing bridges.

Read-object placement exposed a separate transform-ownership error. The first
`Notepad_open` call was `0.4586` world units from the camera. After applying the
distance multiplier, later SetMatrix calls arrived at `0.8842` and `1.4697`:
HPL had retained SOMAVR's submitted transform, and the bridge scaled that output
again. Frame-age expiry was not a safe discriminator because legitimate native
updates were tens of frames apart.

The corrected contract is session scoped. Entering native state 10 `Read`
clears old anchors and increments a session id. The first eligible object call
latches its native matrix and the camera position at that instant. Every later
presentation is rebuilt from that immutable matrix and its original
camera-relative offset, translated with the current camera, then distance-scaled
once. Grip rotation may update the separate presentation orientation but never
the source matrix. Leaving Read clears all anchors, so the next pickup starts
from fresh native evidence.

## 0.86 Read Settle, Persistent Wake, And Virtual Torso Ownership

The 0.85 live log showed that the first Read candidate matrix is not yet a
presentation pose. It is roughly 0.457 m below the camera and oriented as part
of the native rise-in animation. Latching that first call faithfully preserves
the wrong moment. The same session also submits helper entities such as
`CellPhoneArm_open`, so treating every non-hand SetMatrix call as the story
object creates competing ownership.

Version 0.86 assigns one eligible non-special entity as the session owner,
updates its source matrix during a bounded 45-frame settle window, and only
then latches and applies camera-relative distance/rotation. Retained hands now
remain eligible during Read, because the log proved camera and tracking
ownership did not require suspension. The decisive evidence is
`hpl_read_presentation` with `settleAge>=settleFrames`, one stable entity, and
no `hpl_hands_visibility suspended=1` on entry to state 10.

The apartment log also proved a useful middle rung for persistent hands:
`PlayerHands_0` had already been created and retained before controller
tracking began. Calling the original native `SetActive(true)` and
`SetVisible(true)` once when tracking becomes eligible can wake that correct
campaign-owned entity. This is intentionally not a replacement for
`CreateHandModelIfNeeded`; the wake route fails closed when there is no retained
native seed.

Physical body follow must not share the native game-camera turn command. The
headset is the rendered view owner, while shoulders need only a slowly changing
yaw reference. Version 0.86 therefore publishes a yaw-only virtual torso
quaternion to the arm solver, follows native yaw deltas from explicit stick
turns, and never calls `cCharacterBody::AddYaw` for physical HMD rotation.
