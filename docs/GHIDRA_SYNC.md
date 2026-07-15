# Ghidra Synchronization Ledger

## 2026-07-15 Temporal SSAO Sync

| Address | Ghidra name | Evidence/use |
| --- | --- | --- |
| `0x1403f2b50` | `HPL3_RendererDeferred_RenderSSAO` | Renamed/documented complete AO, blur, temporal reprojection, and final `+0xe78` overwrite path; exact `0.54.0` control hook. |
| `0x1402aba30` | `HPL3_Renderer_SetTextureUnit` | Renamed/documented native texture-unit bridge used to map `iTexture*` to GL texture ID. |
| `0x1403f4530` / `0x1403f2880` | `HPL3_RendererDeferred_CreateSSAOAndReflectionResources` / `DestroySSAOAndReflectionResources` | Renamed/documented lifecycle and exact renderer field map. |
| `0x1403f40d0` | `HPL3_RendererDeferred_RenderLocalReflection` | Renamed/documented same-pass feedback-avoidance scratch path; no persisted temporal owner. |
| `0x1403f9bb0` | `HPL3_RendererDeferred_RenderSSAOViewportOverlay` | Renamed/documented distinct viewport overlay/upscale consumer of `+0xe78`. |
| `0x14079575c` | `g_flSSAOTemporalSamplePhase` | Typed/documented shared float phase advanced once per SSAO invocation and frame-owned by `0.55.0`. |

`HPL3_RendererDeferred_RenderLocalReflection` is now classified as sequential
scratch rather than a temporal-resource candidate: `+0xeb0/+0xf00` is fully
overwritten from the current accumulation input before same-pass composition.

## 2026-07-15 ToneMapping Frame-Ownership Sync

| Address | Ghidra name | Evidence/use |
| --- | --- | --- |
| `0x1402842d0` | `HPL3_PostEffect_ToneMapping_AdvanceFrameState` | Renamed/documented as the exact renderer-time update of exposure, white-cut, window fade, authored transition, and grading-transition state now owned once per same-pose stereo pair. |
| `0x140284fd0` | `HPL3_PostEffect_ToneMapping_RenderEffect` | Renamed/documented as the virtual render that advances the packet before selecting final shader features. |
| `0x140284d70` / `0x140283fd0` | `HPL3_PostEffect_ToneMapping_CreateBloomResources` / `DestroyBloomResources` | Renamed/documented as six bloom scratch-pair lifecycle functions. |
| `0x140284e40` / `0x140284770` | `HPL3_PostEffect_ToneMapping_RenderBloomBrightPass` / `RenderBloomBlurPass` | Renamed/documented; full-overwrite behavior classifies bloom targets as sequential scratch, not temporal history. |
| `0x1402845d0` / `0x140284570` | `HPL3_PostEffect_ToneMapping_AdvanceFilmGrainOffsets` / `LoadDefaultGrading` | Renamed/documented; the former confirms current/next grain offsets `+0x138..+0x154` now included with RenderEffect phase `+0x158` in shared frame ownership, while grading resource setup remains separate. |

The explicitly selected `Soma_NoSteam.exe` database was saved after all eight
renames and plate comments.

## 2026-07-15 Per-Eye ImageTrail Sync

| Address | Ghidra name | Evidence/use |
| --- | --- | --- |
| `0x14038a8b0` | `HPL3_PostEffect_ImageTrail_DestroyResources` | Renamed and documented as the exact texture `+0x58` / framebuffer `+0x50` release-and-zero lifecycle boundary hooked by `0.52.0`. |
| `0x14038a8f0` | `HPL3_PostEffect_ImageTrail_Reset` | Renamed and documented as the writer of one-shot clear-history flag `+0xa0`. |
| `0x14038a930` | `HPL3_PostEffect_ImageTrail_OnSetActive` | Renamed and documented as the inactive-edge reset dispatcher. |
| `0x14038a950` | `HPL3_PostEffect_ImageTrail_RenderEffect` | Renamed and documented as the accumulation render: framebuffer `+0x50`, texture/return `+0x58`, amount `+0x98`, and clear flag `+0xa0`. |
| `0x14038ae60` | `HPL3_PostEffect_ImageTrail_CreateResources` | Existing name retained; decompilation reconfirmed exact native resource creation used lazily for the second eye pair. |

The explicitly selected `Soma_NoSteam.exe` database was saved after all four
renames and plate comments. Released HPL2 `PostEffect_ImageTrail` source
independently matches the accumulation texture/framebuffer and clear-on-reset
semantics.

## 2026-07-15 All-Stereo Previous-View Sync

| Address | Ghidra name | Evidence/use |
| --- | --- | --- |
| `0x1401f1480` | `HPL3_Renderer_RenderPostPostEffects` | Plate comment now records eye-local previous-view ownership for both AFR and same-frame stereo plus native reseed conditions. |
| `0x140298630` | `HPL3_Scene_RenderViewport` | Plate comment now records the guarded transaction on every exact-player stereo pass, not only continuous replay. |

No address or type inference changed. The explicitly selected
`Soma_NoSteam.exe` database was saved after both comments were revised.

## 2026-07-15 Per-Eye Previous-View Sync

| Address | Ghidra name | Evidence/use |
| --- | --- | --- |
| `0x1401f1480` | `HPL3_Renderer_RenderPostPostEffects` | Reconfirmed the exact `0x40` copy from active frustum view `+0x158` to renderer history `+0x80`; plate comment records the `0.46.0` per-eye bank and transaction timing. |
| `0x140298630` | `HPL3_Scene_RenderViewport` | Plate comment records that history selection must occur before this complete eye pass and native history capture after it returns. |

HPL2 `Camera.h` independently names the same-size packet `m_mtxPrevView`. The
explicitly selected `Soma_NoSteam.exe` database was saved after both plate
comments were updated.

## 2026-07-15 Continuous Dual Render Sync

| Address | Ghidra name | Evidence/use |
| --- | --- | --- |
| `0x140298630` | `HPL3_Scene_RenderViewport` | Plate comment now records the `0.45.0` exact-player continuous replay: immediate eye-one cache, screen-GUI bit `2` removed on eye two, same-pose guard, bounded logging, and fail-closed rollback. |
| `0x140298850` | `HPL3_Scene_RenderViewports` | Reconfirmed as the once-per-game-frame viewport owner. Its renderer frame increment/stat reset and enumeration are never duplicated. |
| `0x1401f1480` | `HPL3_Renderer_RenderPostPostEffects` | Plate comment records that continuous eye two necessarily repeats this stateful phase; bounded diagnostic arms still snapshot it, while ordinary continuous frames do not. |

The explicitly selected `Soma_NoSteam.exe` database was saved after all three
plate-comment updates and decompilation rechecks.

## 2026-07-15 Inventory Presentation Sync

| Address | Ghidra name | Evidence/use |
| --- | --- | --- |
| `0x1401378e0` | `SOMA_cLuxUserModule_OnAction` | Renamed and typed as the exact native `(module, action, pressed)` wrapper. Its 26-byte body forwards the script object at `+0x90`; `0.44.0` signature-guards this boundary. |
| `0x140129a40` | `SOMA_ScriptObject_OnAction` | Renamed and typed as the AngelScript `OnAction(int,bool)` dispatcher called by the wrapper. |
| `0x1401ae870` | `SOMA_Script_Register_cLuxUserModule` | Registration proves `int mlId` at native `+0x158` and binds the user-module action interface. |

The explicitly selected `Soma_NoSteam.exe` database was saved after all three
renames, both action prototypes, and plate comments. Shipped script evidence
then narrows inventory activity to module `15`, action `12`, pressed edges.

## 2026-07-15 Scripted Presentation Sync

| Address | Ghidra name | Evidence/use |
| --- | --- | --- |
| `0x140485200` | `HPL3_Script_GetGlobalArgFloat` | Renamed and typed as the exact float getter used for `WakeHandler::_Global_StartWakeup` duration. |
| `0x140485720` | `HPL3_Script_GetGlobalArgBool` | Renamed and typed as the exact bool getter used for `WakeHandler::_Global_SetAsleep`. |
| `0x1401297c0` | `SOMA_cLuxUserModule_OnGui` | Renamed and typed; plate comment records `mlId +0x158`, module IDs `10/12/19`, and why callback presence is not presentation activity. |

The explicitly selected `Soma_NoSteam.exe` database was saved after all three
renames, prototypes, and plate comments.

## 2026-07-15 Native Gameplay Haptics Sync

| Address | Ghidra name | Evidence/use |
| --- | --- | --- |
| `0x140109b30` | `SOMA_cLuxInputHandler_SetRumble` | Renamed, typed as the four-argument fastcall wrapper, and documented as the exact script-visible boundary mirrored by `0.42.0`. |
| `0x1401158c0` | `SOMA_Script_Register_cLuxInputHandler` | Registration strings bind `void SetRumble(int,float,float)` directly to the hooked wrapper. |
| `0x14015bf50` | `SOMA_cLuxPlayer_GiveDamage` | Shipped damage flow reaches `Effect_Rumble_Start` and therefore the shared rumble boundary. |

The explicitly selected `Soma_NoSteam.exe` database was saved after all three
renames, the rumble prototype, and plate comments.

## 2026-07-15 Post-Effect Resource Ownership Sync

| Address | Ghidra name | Evidence/use |
| --- | --- | --- |
| `0x1402d7a40` | `HPL3_PostEffect_RenderOne` | Existing name retained. The function now has the exact five-argument fastcall prototype `(effect, composite, inputTexture, tempFramebuffer, isLastEffect) -> outputTexture`. The plate comment records virtual `+0x68`, native final-copy ownership, and the read-only `0.37.0` same-pose GL resource-correlation contract. |

The explicitly selected `Soma_NoSteam.exe` database was saved after prototype
and plate-comment synchronization. No texture object layout or effect-specific
history field is claimed before live bound-resource evidence.

## 2026-07-15 Two-Hand Tool And Grab Sync

| Address | Ghidra name | Evidence/use |
| --- | --- | --- |
| `0x1400bcd90` | `SOMA_iLuxEntity_SetMatrix` | Existing exact `HudObject` boundary retained. The plate comment now records `0.36.0` support-grip aim composition: dominant position, squeezed fresh support direction, bounded separation, native uniform scale, and dominant-only/native fallback on every failed gate. |
| `0x140238750` | `HPL3_PidControllerVec3_Output` | Existing exact Grab force/torque PID boundary retained. The plate comment now records optional two-hand direction-delta torque, one-call engagement/release re-anchoring, and continued native ownership of PID integration and body physics. |

The explicitly selected `Soma_NoSteam.exe` database was saved after both plate
comment updates. No generic entity transform or generic physics-body ownership
is claimed by this feature.

## 2026-07-15 Subtitle Presentation Sync

| Address | Ghidra name | Evidence/use |
| --- | --- | --- |
| `0x1401c8dd0` | `SOMA_VoiceSubtitle_Render` | Renamed from the default symbol. The worker draws localized speaker and subtitle rows through the game GUI and repeatedly reads `cLuxVoiceHandler +0x174/+0x178/+0x17c/+0x180`; plate comment records the `0.34.0` scoped override/restore contract. |
| `0x1401d3ba0` | `SOMA_cLuxVoiceHandler_Constructor` | Renamed from the default symbol. Constructor string anchors and config loads establish gradual reveal, normal/large width/font/Y caches, shadow offset, and font resource ownership. |

The explicitly selected `Soma_NoSteam.exe` database was saved after both
renames and plate comments. No subtitle content, localization, timing, or font
resource function was renamed because the feature does not claim those owners.

## 2026-07-15 Frustum Depth-Convention Sync

| Address | Ghidra name | Evidence/use |
| --- | --- | --- |
| `0x14026fcf0` | `HPL3_Frustum_SetupCommon` | Renamed from the default symbol. Decompile proves projection `+0xd8`, view `+0x158`, far `+0x18`, and near `+0x1c`; the plate comment records `0.33.0` compositor-depth use and HPL-to-meter conversion. |
| `0x140270230` | `HPL3_Frustum_SetupPerspectiveProj` | Existing name retained. Plate comment now records finite standard OpenGL near/far mapping, matching HPL2 source, and the exact call into common setup. |

The explicitly selected `Soma_NoSteam.exe` database was saved after both
updates. No new executable hook was added; this pass closes the projection/depth
contract used by `XrCompositionLayerDepthInfoKHR`.

## 2026-07-15 Presentation And Optics Sync

| Address | Ghidra name | Evidence/use |
| --- | --- | --- |
| `0x140155210` | `HPL3_Script_cLuxPlayer_FadeCameraFOVMulTo` | Created, named, and typed exact two-float leaf; plate comment records player `+0x38c/+0x390` and reversible `0.29.0` neutral multiplier policy. |
| `0x140155230` | `HPL3_Script_cLuxPlayer_FadeCameraAspectMulTo` | Created, named, and typed exact two-float leaf; plate comment records player `+0x394/+0x398`. |
| `0x140155250` | `HPL3_Script_cLuxPlayer_FadeCameraFOVTo` | Created, named, and typed exact two-float leaf; plate comment records target/speed `+0x19c/+0x1a0` and native default FOV `+0x194`. |
| `0x1400ccdb0` | `SOMA_IsLoadingScreenVisible` | Created, named, and typed exact bool query; comment records game-context ownership and read-only per-frame use. |
| `0x140488fa0` | `HPL3_Script_CreateVideo` | Renamed and typed resource-video wrapper; comment records probe-only stream/name telemetry. |
| `0x140488fd0` | `HPL3_Script_DestroyVideo` | Renamed and typed destroy wrapper; comment records unchanged native dispatch after lifecycle accounting. |

All six anchors were tagged with existing SOMAVR subsystem tags and the
explicitly selected `Soma_NoSteam.exe` database was saved after synchronization.

## 2026-07-15 Authored Comfort Sync

| Address | Ghidra name | Evidence/use |
| --- | --- | --- |
| `0x140156d90` | `HPL3_Script_cLuxPlayer_FadeCameraRollTo` | Retyped to exact player/type/target/speed/max-speed ABI. Plate comment records semantic roll IDs and the `0.28.0` target-only suppression contract. |
| `0x140156f00` | `HPL3_Script_cLuxPlayer_SetCameraRoll` | Retyped direct setter; plate comment records the same independently configurable Script/Lean/Move/Climb policy. |
| `0x140071f80` | `HPL3_World_SetDepthOfFieldActive` | Created, named, and typed as the compact bool setter writing `world+0x264`; comment records the exact reversible function-plus-padding patch. |

All three anchors carry SOMAVR/Confirmed/VR-Comfort tags. The explicitly
selected `Soma_NoSteam.exe` database was saved after synchronization.

## 2026-07-15 Flashlight Gameplay-Ray Sync

| Address | Ghidra name | Evidence/use |
| --- | --- | --- |
| `0x1400cd7d0` | `SOMA_GetClosestBody` | Renamed and typed as the five-argument registered global wrapper. Plate comment records the `0.27.0` camera-origin/length gate and randomized-cone preservation contract. |
| `0x140143a10` | `SOMA_PhysicsRay_GetClosestBody` | Renamed and typed as the underlying closest-body ray query that forms its endpoint and returns body/distance/normal. |

Both functions were tagged `SOMAVR`, `Confirmed`, `Flashlight`, and
`PhysicsRay`. The explicitly selected `Soma_NoSteam.exe` database was saved
after synchronization.

## 2026-07-15 Volume/Spectator/Telemetry Reuse Audit

`0.26.0` adds no executable address. GPU timestamps use OpenGL entry points at
the existing six guarded render-stage boundaries. Depth probing reuses confirmed
HPL frustum fields already exposed by `HPLCameraBridge`; no new SOMA detour or
Ghidra label is required.

`0.25.0` adds no executable address. The head-volume sweep reuses the already
typed and tagged `SOMA_CheckLineOfSight` wrapper at `0x1400cd710`; spectator
output and CPU timing use existing GL/frame and six guarded render-stage hooks.
The selected `Soma_NoSteam.exe` database therefore required no new rename or
prototype, and the prior room-scale entries remain authoritative.

## 2026-07-15 Room-Scale Safety Sync

| Address | Ghidra name | Evidence/use |
| --- | --- | --- |
| `0x1400cd710` | `SOMA_CheckLineOfSight` | Renamed and typed as the registered four-argument fastcall wrapper; plate comment records null skip-entity forwarding and the SOMAVR static-world safety use. |
| `0x140143650` | `SOMA_PhysicsRay_CheckLineOfSight` | Renamed and typed as the six-argument underlying physics/world query; plate comment records active-world resolution, filter ownership, callback dispatch, and clear-segment return semantics. |

Both functions are tagged `SOMAVR`, `VR-Roomscale`, and `Collision`; the wrapper
also retains its AngelScript-wrapper identity. Exact wrapper bytes are enforced
by the DLL before the optional control is enabled.

## 2026-07-15 Diegetic Terminal Input Sync

| Address | Ghidra name | Evidence/use |
| --- | --- | --- |
| `0x1402f0b10` | `HPL3_ImGui_SendMousePosition` | Registered `SendMousePosition` wrapper; converts physical screen coordinates to the owned set's virtual size/offset. |
| `0x1402f0c90` | `HPL3_ImGui_SendMouseVirtualPosition` | Registered virtual cursor wrapper and exact `0.38.0` detour. Prototype, field behavior, ownership policy, plate comment, and SOMAVR bookmark added. |
| `0x1400f7f10` | `SOMA_ImGuiManager_UpdateInput` | Manager dispatch proves world GUI input calls the virtual wrapper directly while the ordinary screen owner receives the physical wrapper. |

The explicitly selected `Soma_NoSteam.exe` program was saved after all three
functions were renamed, typed, and documented.

## 2026-07-15 Controller Flashlight Sync

| Address | Ghidra name | Evidence/use |
| --- | --- | --- |
| `0x1400bcd90` | `SOMA_iLuxEntity_SetMatrix` | Plate comment now records shipped `Player.hps::UpdateFlashlightRotation`, exact `Flashlight` identity, dominant aim ownership, local negative-Z beam convention, and original-matrix fallback contract. |
| `0x14000fb60` | `SOMA_iLuxEntity_GetName` | Plate comment now records shared cLightSpot identity use and exact `Flashlight` matching in addition to `PlayerHands_*`. |

Both functions received `VR-Flashlight` plus pose/identity tags. The explicitly
selected `Soma_NoSteam.exe` program was saved after synchronization.

## 2026-07-15 ImGui Identity Sync

| Address | Ghidra name | Evidence/use |
| --- | --- | --- |
| `0x1400cca90` | `SOMA_GetGameHudImGui` | Registered `cImGui@ GetGameHudImGui()` getter; `gameContext +0xe8 -> +0x160`. Function, prototype, plate comment, and tags added for `0.22.0` passive render identity. |
| `0x140071f20` | `HPL3_ImGui_GetSet` | Registered `cGuiSet@ GetSet()` wrapper; exact five-byte `mov rax,[rcx+0x18]; ret`. Function, prototype, plate comment, and tags added for `0.22.0`. |

Both functions carry `SOMAVR`, `VR-HUD`, and `AngelScript-Wrapper` tags. The
explicitly selected `Soma_NoSteam.exe` program was saved after synchronization.

## 2026-07-15 Semantic Crosshair Dispatch Sync

| Address | Ghidra name | Evidence/use |
| --- | --- | --- |
| `0x140484ea0` | `HPL3_Script_RunGlobalFunc` | Registered global script dispatcher. `0.21.0` observes only exact `LuxPlayer::_Global_SetCrosshairState` native strings after guarding the complete 21-byte thunk. |
| `0x1404851d0` | `HPL3_Script_GetGlobalArgInt` | Registered integer argument reader. Prototype corrected to `int (int index)`; its tail jump returns the parsed value in EAX. |
| `0x140485810` | `HPL3_Script_SetGlobalArgInt` | Registered integer argument writer used by shipped `Player_SetCrossHairState` before dispatch. |

All three functions now have explicit prototypes and plate comments plus
`SOMAVR` and `VR-interaction` tags. The explicitly selected
`Soma_NoSteam.exe` program was saved after synchronization.

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

## 2026-07-15 Dual-Render Sync

`HPL3_Scene_RenderViewport` at `0x140298630` was re-decompiled before the first
same-frame experiment. Its plate comment now records the exact mask contract,
the unconditional post-post callback, and the `0.35.0` one-frame replay policy.
No speculative callback ownership was promoted.

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
| `0x1401f1480` | `HPL3_Renderer_RenderPostPostEffects` | Full deferred/post-post phase after post effects and before screen GUI. Decompilation confirms GPU work plus a `0x40`-byte current-to-history state copy through renderer `+0x20/+0x438`; renamed and plate-commented for the `0.40.0` temporal probe. |
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
5. Use `0.40.0` first/replay mutation ranges to type renderer current/history owners at `+0x20/+0x438`, then map the copied `+0x158 -> +0x80` matrix/state block per eye.
6. Apply partial `cViewport`, sound-listener, camera, and frustum structures only after offsets survive runtime validation.
