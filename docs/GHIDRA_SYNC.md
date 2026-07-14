# Ghidra Synchronization Ledger

## 2026-07-15 Native Comfort And Interaction Result Sync

| Address | Ghidra name | Evidence/use |
| --- | --- | --- |
| `0x140159360` | `HPL3_Script_cLuxPlayer_SetCameraPosAdd` | Registered setter with type in EDX and vector in R8. `0.19.0` zeros only semantic Bob/Shake/optional Sway while VR is active. |
| `0x140156f00` | `HPL3_Script_cLuxPlayer_SetCameraRoll` | Confirmed roll-current/goal array setter; documented as a future policy anchor and deliberately unhooked. |
| `0x1400cd750` | `SOMA_GetClosestEntity` | Plate comment records finalized output payload entity `+0x18`, body `+0x20`, distance `+0x28`, the world-hit snapshot, and its `0.20.0` OpenXR depth-reticle/focus-haptic consumers. |

The two player wrappers were renamed and tagged `SOMAVR`, `VR-comfort`, and
`AngelScript-wrapper`; the existing picker received interaction/result tags and
an updated control contract. The explicitly selected `Soma_NoSteam.exe` program
was saved after synchronization.

## 2026-07-15 AddImpulse And Physics Registration Sync

| Address | Ghidra name | Evidence/use |
| --- | --- | --- |
| `0x14049c720` | `HPL3_Script_iPhysicsBody_AddImpulse` | Created as a nine-byte function. It loads the body vtable and dispatches slot `+0x130`; `0.18.0` uses a reversible, signature-guarded 12-byte jump patch for controller-armed Grab throws. |
| `0x1404a0480` | `HPL3_Script_Register_iPhysicsBody` | Renamed registration owner for AddForce/AddTorque/AddImpulse and related physics-body script methods. |

Both anchors now have SOMAVR/physics/script tags and control-policy comments;
the AddImpulse thunk also has a patch-site bookmark with the exact signature.
The explicitly selected `Soma_NoSteam.exe` program was saved after synchronization.

## 2026-07-15 Native Grab PID And Camera Getter Sync

| Address | Ghidra name | Evidence/use |
| --- | --- | --- |
| `0x140238750` | `HPL3_PidControllerVec3_Output` | Exact vector PID output; `0.17.0` signature-guards this control hook and modifies only Grab force tuple `400/0/40`. |
| `0x140534eb0` | `HPL3_Script_Register_PidControllers` | Script-registration owner confirming vector PID surface and ownership. |
| `0x1400ab230` | `HPL3_Camera_GetPosition` | Returns camera world position at `+0x10`; grab/camera evidence anchor, not hooked. |
| `0x140237270` / `0x1402b8200` / `0x14000fda0` | `HPL3_Camera_GetPitch/Yaw/Roll` | Base angles at `+0x44/+0x48/+0x4c`; broad evidence getters, not control hooks. |

All six functions now carry focused SOMAVR/physics-hands/camera evidence tags
and plate comments describing the mutation or no-hook policy. The explicitly
selected `Soma_NoSteam.exe` program was saved after synchronization.

## 2026-07-15 Controller Hands And Menu Policy Sync

`SOMA_iLuxEntity_SetMatrix` at `0x1400bcd90` now records the `0.16.0`
controller-root contract, including the shipped hand script's
`cameraRotation * rotateY(pi) * scale` convention, exact `PlayerHands_*`
identity, quarter-scale/Normal-state gates, and original-matrix fallback.
It carries `Hands`, `Viewmodel`, `ControllerPose`, `ControlHook`, `Confirmed`,
and `SOMAVR` tags.

`SOMA_GetGamePaused` at `0x1400ccc90` now records complete gameplay-input
ownership: a true pause releases direct and synthetic gameplay routes and
permits only native menu cursor/click routing. It carries `Pause`, `Menu`,
`InputPolicy`, `ControlGate`, `Confirmed`, and `SOMAVR` tags. The explicitly
selected `Soma_NoSteam.exe` program was saved after both updates.

## 2026-07-15 Gameplay HUD Capture Sync

The `0.15.0` HUD transaction is now recorded on all three native anchors:

| Address | Ghidra name | Evidence/use |
| --- | --- | --- |
| `0x140213970` | `HPL3_GuiSet_Render` | Confirmed 2D branch ignores the render-target argument and draws into the current framebuffer; exact GameHudSet capture hook. |
| `0x1402981e0` | `HPL3_Scene_RenderScreenGui` | Confirmed non-3D set collection, priority sort, and final per-set render order after post effects. |
| `0x1400cc9b0` | `SOMA_GetGameHudSet` | Exact game-context `+0x50` identity gate; signature/context failure retains native HUD rendering. |

Their plate comments now describe the transparent FBO and VIEW-space OpenXR
quad ownership contract. HUD/GUI/OpenXR/capture/render-order identity tags were
applied as appropriate, and `Soma_NoSteam.exe` was saved after synchronization.

## 2026-07-15 Native Locomotion And Pause Policy Sync

The `0.14.0` normal-state native input contract is now explicit in
`Soma_NoSteam.exe`:

| Address | Ghidra name | Evidence/use |
| --- | --- | --- |
| `0x1402375f0` | `HPL3_Script_iCharacterBody_Move` | Registered `Move(eCharDir,float)` wrapper; typed object/direction/amount ABI, Forward `0`, Right `1`. |
| `0x140237460` | `HPL3_Script_iCharacterBody_AddYaw` | Renamed and typed registered `AddYaw(float)` wrapper; adds radians at body `+0xd4`. |
| `0x1400ccc90` | `SOMA_GetGamePaused` | Created and typed registered `GetGamePaused()` wrapper; reads subsystem `+0x2d4`. `0.16.0` now uses it as the hard gameplay-input/menu-pointer ownership gate. |

All three have control-policy comments and SOMAVR/locomotion evidence tags.
They are signature-guarded together so direct body input cannot run behind a
pause menu or outside normal player/move ownership. The program was saved after
the new function, prototypes, names, comments, and tags were applied.

## 2026-07-15 Hands Identity And Root Transform Sync

The `cLuxProp` registration chain used by `PlayerHandsHandler` is now explicit:

| Address | Ghidra name | Evidence/use |
| --- | --- | --- |
| `0x14000fb60` | `SOMA_iLuxEntity_GetName` | Created and typed compact accessor; returns entity `+0x120`, registered as inherited `const tString& GetName()` for `cLuxProp`. |
| `0x1400bcd90` | `SOMA_iLuxEntity_SetMatrix` | Confirmed `RDX` matrix ABI; `0.16.0` applies exact-name, quarter-scale, normal-state tracked-grip root substitution with native passthrough on every failed gate. |
| `0x14016ebe0` | `SOMA_Script_Register_cLuxProp` | Renamed registration owner from the exact `cLuxProp` class string and GetName/SetMatrix registrations. |

All three received PlayerHands/cLuxProp/identity/transform evidence comments and
tags where appropriate. The program was saved after the runtime probe anchors
were synchronized.

## 2026-07-15 Native Interaction And Hands Transform Sync

Two AngelScript registration targets were promoted, typed, commented, tagged,
and saved in `Soma_NoSteam.exe`:

| Address | Ghidra name | Evidence/use |
| --- | --- | --- |
| `0x1400cd750` | `SOMA_GetClosestEntity` | Registered global closest-entity query used by `Utility_PickBasics`; `0.12.0` substitutes controller start/direction only. |
| `0x1400bcd90` | `SOMA_iLuxEntity_SetMatrix` | Shared Lux entity SetMatrix target used by the `PlayerHands_*` script path; bounded controller-root mutation is built for exact normal quarter-scale states. |

The interaction prototype records start, direction, ray length, interaction type,
LOS, and output arguments. The transform prototype records object-first entity and
matrix pointers. Both functions carry evidence and SOMAVR subsystem tags.

## 2026-07-15 Spatial Ownership And HUD Getter Sync

Seven compact AngelScript HUD getters that were previously disassembled but not
defined as functions were created, named, typed as pointer-returning wrappers,
commented, and tagged `SOMAVR`, `HPL3`, `HUD`, `GUI`, and `Confirmed`:

| Address | Ghidra name | Context path |
| --- | --- | --- |
| `0x1400cc9b0` | `SOMA_GetGameHudSet` | `+0x50` |
| `0x1400cc9c0` | `SOMA_GetHudVirtualCenterSize` | `+0x58` |
| `0x1400cc9d0` | `SOMA_GetHudVirtualSize` | `+0x60` |
| `0x1400cc9f0` | `SOMA_GetHudVirtualStartPos` | `+0x70` |
| `0x1400cca00` | `SOMA_GetHudVirtualCenterScreenSize` | `+0x7c` |
| `0x1400cca10` | `SOMA_GetHudVirtualCenterScreenStartPos` | `+0x84` |
| `0x1400cca70` | `SOMA_GetCurrentImGui` | `+0xe8 -> +0x168` |

The names are backed by their registration strings, not inferred only from
layout. `0x1400cc9e0` remains deliberately unnamed until its adjacent
registration string is confirmed.

## 2026-07-15 Camera Rotation And Comfort Sync

The `cCamera` rotation surface registered by `HPL3_Script_Register_cCamera` at
`0x1404e1a80` is now explicit in Ghidra. Base setters/adders were promoted at
`0x140270ab0`, `0x140270b00`, `0x140270b50`, `0x140270b70`, `0x140270bc0`, and
`0x140270c10`; extended setters at `0x140270c40`, `0x140270c70`, and
`0x140270ca0`; and `HPL3_Camera_GetRoll` at `0x14000fda0`.

Decompilation confirms base roll at `cCamera+0x4c`, extended/authored roll at
`+0x68`, and the cache-dirty family at `+0x709/+0x70b/+0x70c/+0x70d`. These
anchors back `0.9.0` telemetry and configurable temporary roll suppression. The
functions received evidence comments and the program was saved.

## 2026-07-15 Runtime Resilience, GUI, And Effect Identity Sync

Seven virtual type-name getters were promoted and tagged `HPL3`, `PostEffect`,
and `SOMAVR`: ToneMapping `0x1402859b0`, FXAA `0x140386170`, ImageFadeFX
`0x140386b20`, VideoDistortion `0x1403878a0`, ChromaticAberration
`0x140388ee0`, RadialBlur `0x140389ed0`, and ImageTrail `0x14038ad00`.

Their vtables were named at `0x14069b038`, `0x1406ac3b8`, `0x1406ac4e8`,
`0x1406ac688`, `0x1406ac928`, `0x1406acb78`, and `0x1406acd48`. Plate comments
record how SOMAVR's named, temporary VR policy treats each effect.
`HPL3_GuiSet_Render` received the confirmed field ledger used by the new per-set
telemetry hook. The program was saved after synchronization.

`HPL3_Script_iCharacterBody_Move` at `0x1402375f0` was also confirmed, tagged,
and commented. Its direct body accumulator behavior explains why it remains an
RE anchor rather than the default locomotion injection point.

## 2026-07-15 Authored Camera, HUD, And Post-Effect Sync

The active `Soma_NoSteam.exe` database was updated and saved after the `0.7.2`
RE pass:

| Address | Ghidra name | Evidence/use |
| --- | --- | --- |
| `0x1404a94f0` | `HPL3_Camera_GetRotateMode` | Registered getter reads camera `+0x6c`; HPL2 confirms Euler `0`, matrix `1`. |
| `0x1404a9d70` | `HPL3_Script_Camera_GetRotateMode` | Script dispatch wrapper for the native getter. |
| `0x14049bdf0` | `HPL3_CharacterBody_SetCameraUpdateActive` | Created function and applied prototype; writes body `+0x1e8`. |
| `0x14049be00` | `HPL3_CharacterBody_GetCameraUpdateActive` | Created function and applied prototype; reads body `+0x1e8`. |
| `0x14022f8e0` | `HPL3_Viewport_CreateGuiSetIterator` | Iterates viewport `+0x90` GUI-set list for final screen GUI. |
| `0x140213970` | `HPL3_GuiSet_Render` | Configures and renders one flat/3D GUI set. |
| `0x1402d7a40` | `HPL3_PostEffect_RenderOne` | Executes one post effect via virtual `+0x68` and handles final copy. |
| `0x14033b950` / `0x14033bb00` | `HPL3_PostEffectComposite_BeginRender` / `EndRender` | Composite renderer setup and restoration around ordered effect iteration. |

The functions received authored-camera, character-body, camera, HUD/GUI,
post-effect, renderer, and SOMAVR tags plus evidence plate comments. The first
three native accessors received known prototypes. These anchors now back
`HPLPlayerState`, final-GUI state telemetry, and reversible per-effect isolation.

## 2026-07-14 Player And Input Sync

The active `Soma_NoSteam.exe` database now contains confirmed player-access
anchors needed by `FEATURE.LOCOMOTION` and `FEATURE.AUTHORED_CAMERA`:

| Address | Ghidra name | Evidence/use |
| --- | --- | --- |
| `0x1400cc860` | `SOMA_GetPlayer` | `GetPlayer()` registration plus a 15-byte getter returning game context `+0x140`; guarded by `HPLInputBridge`. |
| `0x140125ef0` | `SOMA_cLuxPlayer_GetCamera` | `cLuxPlayer` registration and direct player `+0x168` return. |
| `0x140155290` | `SOMA_cLuxPlayer_GetCharacterBody` | `cLuxPlayer` registration and direct player `+0x170` return. |
| `0x140155050` | `SOMA_cLuxPlayer_GetCurrentStateId` | Script registration and direct state-object/ID read. |
| `0x140155090` | `SOMA_cLuxPlayer_GetCurrentMoveStateId` | Script registration and direct move-state/ID read. |
| `0x140155c40` | `SOMA_cLuxPlayer_UpdateCameraDirection` | Smooths direction accumulators and applies camera/body orientation. |
| `0x14015ba20` | `SOMA_cLuxPlayerHelper_Update` | Per-frame helper owner and candidate future native action-injection phase. |

All seven functions received evidence plate comments and the database was saved.
The next native-input step is to map the state-aware move/turn entry before
replacing the reversible `0.7.0` SOMA input-path prototype.

## 2026-07-13 Lifecycle Sync

The SOMA program was open and the exit dump supplied a stable lifecycle lead.
`0x1403b16e0` was renamed `HPL3_cSDLEngineSetup_Destructor` and received a plate
comment recording its engine-subsystem deletion order. A `VR Lifecycle` bookmark
at `0x1403b1803` records the final `SDL_Quit` call and the requirement to release
OpenXR before Graphics teardown. The function was tagged `vr-lifecycle`,
`shutdown`, and `hpl3-confirmed`.

The CRT entry `0x140642bb4` received a `Runtime Hang` bookmark recording the dump
result: one main thread, RIP in OpenGL, Virtual Desktop runtime stack frames, and
no surviving SOMAVR worker. The database was saved after these changes.

Follow-up: the installed bytes and Ghidra disassembly confirm the destructor
starts `40 53 48 83 EC 20 48 8D 05`. The `0.5.5` signature omitted the leading
REX prefix and therefore failed closed. The plate comment and `VR Lifecycle`
bookmark now record the corrected `0.5.6` guard.

Runtime follow-up: `0.5.6-stability` installed the hook, logged pre-graphics
OpenXR shutdown begin/complete at this destructor, and SOMA exited normally. The
plate comment and lifecycle bookmark now record this as runtime-proven rather
than only dump/decompilation-backed.

## 2026-07-12 Shadow Follow-up

The `0.5.2` runtime result and installed HPL shader/HPL2 source map the next issue
to deferred shadow filtering. No new SOMA native address was promoted because the
`Soma_NoSteam.exe` program was not open during this pass; the connected Ghidra
instance exposed only `BioshockHD.exe`. Candidate cascade/light upload callers
remain pending until SOMA is active again. Runtime-backed findings are recorded
in `RUNTIME_ANALYSIS_0.5.2.md` and hypothesis S14.

The subsequent `0.5.3` pass adds no promoted address: F7 saw no target uniform
uploads, and the connected Ghidra instance still did not expose SOMA. Reflection
anchors remain source-backed pending an F6 live-program capture and a reopened
`Soma_NoSteam.exe` database.

Program: `Soma_NoSteam.exe`

This ledger records durable changes made to the shared Ghidra database. Markdown
remains the design and evidence history; Ghidra receives only names, prototypes,
comments, and tags whose confidence is high enough to improve decompilation.

## 2026-07-12 Sync

The first SOMAVR synchronization promoted 24 confirmed/high-confidence functions,
applied 14 known prototypes, added evidence plate comments, and attached searchable
tags. No speculative player-state, interaction-state, or temporal-resource owner
was renamed.

### Render And Frame Ownership

| Address | Ghidra name | Evidence/use |
| --- | --- | --- |
| `0x1402332b0` | `HPL3_Engine_RunMainLoop` | `OnDraw -> render viewports -> OnPostRender -> present`; once per game frame. |
| `0x1402328f0` | `HPL3_Script_DispatchLifecycle` | Script lifecycle ids and update/draw ownership. |
| `0x140298850` | `HPL3_Scene_RenderViewports` | Advances renderer frame state and enumerates visible viewports. |
| `0x140298630` | `HPL3_Scene_RenderViewport` | Mask bits `1` world, `2` screen GUI, `4` post effects; central stereo split map. |
| `0x14022f7d0` | `HPL3_Viewport_RunWorldDrawCallbacks` | Phase `0` pre-world and `1` post-world callbacks from viewport list `+0x60`. |
| `0x1401f9790` | `HPL3_Renderer_RenderWorld` | HPL2-matched world renderer and candidate per-eye scene work. |
| `0x140297670` | `HPL3_Scene_RenderWorldOverlays` | World/3D overlay pass; broad name retained pending live classification. |
| `0x14033b8f0` | `HPL3_PostEffectComposite_HasActiveEffects` | Determines whether world output enters post composition. |
| `0x14033bd80` | `HPL3_PostEffectComposite_Render` | Priority-sorted post-effect chain. |
| `0x1401f1480` | `HPL3_Renderer_RunPostPostEffectCallbacks` | Callback pass after post effects and before screen GUI. |
| `0x1402981e0` | `HPL3_Scene_RenderScreenGui` | Final flat GUI/HUD candidate boundary. |

### Camera, Audio, Input, And Presentation

| Address | Ghidra name | Evidence/use |
| --- | --- | --- |
| `0x140271b80` | `HPL3_Camera_GetFrustum` | Runtime-proven F10/F11 camera hook. |
| `0x140271870` | `HPL3_Camera_GetViewMatrix` | Returns cached matrix at camera `+0x74`; dirty flag `+0x709`. |
| `0x140270230` | `HPL3_Frustum_SetupPerspectiveProj` | Rebuilds projection and culling derivatives. |
| `0x140289340` | `HPL3_FMOD_UpdateListenerAndSystem` | Commits listener vectors then calls FMOD update. |
| `0x1402375f0` | `HPL3_Script_iCharacterBody_Move` | Semantic movement accumulator registered to AngelScript. |
| `0x1404a5030` | `HPL3_Script_Register_iCharacterBody` | Character-body script API registration. |
| `0x14015ca10` | `SOMA_Script_Register_cLuxPlayer` | Player camera/body API registration. |
| `0x14048b1a0` | `HPL3_Script_Register_SceneAPI` | Includes `SetCurrentListener(cViewport@)`. |
| `0x14048be80` | `HPL3_Script_Register_ResourceVideoAPI` | Includes `CreateVideo` and `DestroyVideo`. |
| `0x14038ae60` | `HPL3_PostEffect_ImageTrail_CreateResources` | Temporal image-trail texture/framebuffer ownership. |
| `0x1403896e0` | `HPL3_PostEffect_ChromaticAberration_Init` | VR comfort-sensitive optical effect. |
| `0x14038a5d0` | `HPL3_PostEffect_RadialBlur_Init` | VR comfort-sensitive blur effect. |
| `0x1403870a0` | `HPL3_PostEffect_ImageFade_Init` | Fade presentation path. |

### Instruction Comments

Exact EOL comments were added at:

- `0x140298692`: main viewport `GetFrustum` call and runtime return-address guard.
- `0x14061d188`: FMOD `set3DListenerAttributes` import/call target.
- `0x14048b2d4`: `SetCurrentListener` registration.
- `0x14048c89f` / `0x14048c8e4`: `CreateVideo` / `DestroyVideo` registrations.
- `0x1400edb2a`, `0x1400edbcf`, `0x1400edc74`, `0x1400edd19`: load-screen API registrations.

### Tags

The synchronized surface uses `SOMAVR-confirmed` plus subsystem tags including
`HPL3-render`, `HPL3-camera`, `HPL3-audio`, `HPL3-post-effect`, `HPL3-script-api`,
`VR-hook`, `VR-frame-ownership`, `VR-input`, `VR-comfort`, and `VR-temporal`.

## Promotion Policy

Promote a finding into Ghidra when at least one of these is true:

1. Runtime behavior and a stable executable address agree.
2. SOMA decompilation and matching released HPL2 source agree.
3. A unique registration/resource string establishes semantic ownership.

Keep a finding in Markdown only when the owner is still ambiguous, the name would
overstate behavior, or a live probe is explicitly designed to decide between
multiple candidates.

## Next Ghidra Targets

1. Classify the exact content inside `HPL3_Scene_RenderWorldOverlays`; the `0.5.1` log confirms it stays on world FBO `11`.
2. Name viewport render-target and GUI helpers after their live transitions agree.
3. Locate player-state transition/current-state accessors for authored-camera policy.
4. Locate native grab-target/PID ownership and interaction-state wrappers; the closest-entity query is now confirmed and hooked.
5. Map temporal previous-view/projection and image-history owners per eye.
6. Apply partial `cViewport`, sound-listener, camera, and frustum structures only after offsets survive runtime validation.
