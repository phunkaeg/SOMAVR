# Address Registry

Program: `Soma_NoSteam.exe` in Ghidra.

| Address | Status | Notes |
| --- | --- | --- |
| `entry` / `0x140642bb4` | Confirmed | CRT entry, calls `__security_init_cookie` and `__tmainCRTStartup`. The exit-hang dump's sole thread starts here. |
| `__tmainCRTStartup` | Confirmed | Visual Studio 2010 Release CRT startup. Calls `FUN_1401daec0`. |
| `0x1401daec0` | Confirmed | Command-line/startup bridge. Calls `SetProcessDPIAware`, converts command line, then calls `FUN_140125a20`. |
| `0x140125a20` | Confirmed | Likely real app entry. Allocates `0x8d0` app object, calls init/run/shutdown sequence. |
| `0x14004b620` | Candidate | Init path. Logs `Version %d.%02d` as `1.110`, then performs staged setup. |
| `0x1400383e0` | Candidate | Run path wrapper around virtual/engine run call. |
| `0x14003dd70` | Candidate | Shutdown/cleanup path. |
| `0x1403b16e0` | Confirmed | `cSDLEngineSetup` destructor. Deletes engine subsystems including Graphics; `0.5.5` hooks entry for pre-graphics OpenXR shutdown. Ghidra: `HPL3_cSDLEngineSetup_Destructor`. |
| `0x1403b1803` | Confirmed | `SDL_Quit` call inside the engine setup destructor, after Graphics deletion. Lifecycle shutdown must occur before this point. |
| `0x14055aa60` | Confirmed | GLSL matrix-uniform setter. The runtime callsite at `+0x55ab0a` reaches `__glewUniformMatrix4fv(location, 1, true, matrix)`. |
| `0x140435ab0` | Confirmed | Low-level OpenGL `SetMatrix`; caches projection/model-view, computes the combined MVP, and uploads it through the current program. |
| `0x1402ac580` | Confirmed by HPL2 match | `iRenderFunctions::SetProjectionMatrix`; includes the matching `Setting projection matrix` string and forwards matrix type 1. |
| `0x1402ad960` | Confirmed by HPL2 match | `iRenderFunctions::SetNormalFrustumProjection`; passes the current frustum projection at `frustum+0xd8`. |
| `0x1401f70e0` | High-confidence | Main renderer frustum path. It initializes render-function frustum state, sets normal frustum projection, and renders the scene. |
| `0x1402ab540` | Confirmed by behavior | Initializes render-function state from a frustum and copies its view matrix from `frustum+0x158`. |
| `0x140271b80` | Confirmed by HPL2 match | `cCamera::GetFrustum` variant used by the render viewport. Returns a cached base or secondary-rotation frustum and rebuilds it when dirty. Runtime hook RVA for `0.4.0`. |
| `0x1402719a0` | Confirmed by HPL2 match | Base `cCamera::GetFrustum` cache path, returning the embedded frustum at `camera+0x3f8`. |
| `0x140271250` | Confirmed by HPL2 match | `cCamera::GetViewMatrix` cache update. Builds the row-major world-to-camera matrix and its inverse. |
| `0x140270ab0` / `0x140270b00` / `0x140270b50` | Confirmed | `cCamera::SetPitch`, `SetYaw`, and `SetRoll`. Base angles live at `+0x44/+0x48/+0x4c`; the setters invalidate the native camera cache family. |
| `0x140270b70` / `0x140270bc0` / `0x140270c10` | Confirmed | `cCamera::AddPitch`, `AddYaw`, and `AddRoll`; update the same base fields and dirty flags. |
| `0x140270c40` / `0x140270c70` / `0x140270ca0` | Confirmed | Extended pitch/yaw/roll setters for `+0x60/+0x64/+0x68`. These authored offsets select the secondary frustum and invalidate `camera+0x70d`. |
| `0x14000fda0` | Confirmed | `cCamera::GetRoll`; returns base roll at `camera+0x4c`. Registered to AngelScript as `float GetRoll()`. |
| `0x1404e1a80` | Confirmed | Registers the `cCamera` pitch/yaw/roll and extended-rotation AngelScript surface. Ghidra: `HPL3_Script_Register_cCamera`. |
| `0x140270e10` | Confirmed by HPL2 match | `cCamera::GetProjectionMatrix`; caches perspective/orthographic projection at `camera+0xf4`. |
| `0x140270230` | Confirmed by HPL2 match | `cFrustum::SetupPerspectiveProj`; stores FOV/aspect/oblique state and calls the common setup that updates view-projection, planes, sphere, vertices, and BV. Direct-call RVA for `0.4.0`. |
| `0x1402702a0` | Confirmed by HPL2 match | `cFrustum::SetupOrthoProj`; orthographic sibling, deliberately not modified by the first bridge. |
| `0x140298630` | High-confidence | Render-viewport path; obtains the camera frustum through `0x140271b80` and passes it to the renderer. |
| `0x140298692` | Confirmed | Main render-viewport call to `cCamera::GetFrustum`; return RVA `0x298697` is an additional runtime selection guard in `0.4.0`. |
| `0x14022f7d0` | Confirmed by HPL2 match | Viewport pre/post-world callback dispatcher. Phase `0` invokes callback vtable `+0x08`; phase `1` invokes `+0x10`. Ghidra: `HPL3_Viewport_RunWorldDrawCallbacks`. |
| `0x1402332b0` | Confirmed | Main engine run loop. Dispatches script `OnDraw`, renders viewports through `0x140298850`, dispatches `OnPostRender`, then presents. |
| `0x1402328f0` | Confirmed | Script object/module lifecycle dispatcher. Callback id `2` is `_OnDraw`, `3` is post-render, `4/5/6` are update/post-update/variable-update. |
| `0x140154c40` | Confirmed | Dispatches a lifecycle id to the corresponding virtual method on an active player/module object. |
| `0x140298850` | Confirmed | Enumerates active viewports and calls `0x140298630` for each one. |
| `0x140298850` globals | Confirmed | Increments the renderer frame counter and resets render statistics once before active viewport enumeration; this boundary must not be duplicated per eye. |
| `0x1401f9790` | High-confidence | Main scene render invoked by `0x140298630` before post effects and GUI. |
| `0x140297670` | Confirmed by order, probe built | Viewport renderer callback pass after scene render and before active post-effect composition. Passive hook in `0.5.1` classifies calls/FBO state before dual rendering. |
| `0x14033bd80` | Confirmed | Active post-effect composite render. Iterates the priority-sorted effect tree and ping-pongs outputs. |
| `0x1401f1480` | Confirmed by log string and order | `PostPostEffect` renderer callback pass, after the post chain and before final GUI drawing. |
| `0x1402981e0` | Confirmed | Collects, priority-sorts, and renders viewport GUI sets after scene post effects. Parent iteration boundary for gameplay-HUD capture classification. |
| `0x1401297c0` | Confirmed | Invokes a script object's `OnGui(float)` callback when enabled. |
| `0x14022f8e0` | Confirmed | Creates an iterator over the viewport GUI-set list at viewport `+0x90`. Ghidra: `HPL3_Viewport_CreateGuiSetIterator`. |
| `0x140213970` | Confirmed, control hook built | Renders one `cGuiSet`; 2D sets draw into the current framebuffer. `0.15.0` redirects only exact GameHudSet draws to transparent capture. Ghidra: `HPL3_GuiSet_Render`. |
| `0x1400cc9b0` | Confirmed, identity gate built | `SOMA_GetGameHudSet`; returns game-context `+0x50`. The signature-derived context slot gates exact gameplay-HUD capture and fail-closed native fallback. |
| `0x1400cc9c0` / `0x1400cc9d0` | Confirmed | HUD virtual-center size at context `+0x58` and virtual size at `+0x60`. Ghidra: `SOMA_GetHudVirtualCenterSize`, `SOMA_GetHudVirtualSize`. |
| `0x1400cc9f0` | Confirmed | `SOMA_GetHudVirtualStartPos`; returns context `+0x70`. |
| `0x1400cca00` / `0x1400cca10` | Confirmed | Center-screen virtual size/start-position getters at context `+0x7c/+0x84`. |
| `0x1400cca70` | Confirmed | `SOMA_GetCurrentImGui`; follows game-context `+0xe8`, then `+0x168`. |
| `0x1400cd750` | Confirmed, control hook built | Registered global `GetClosestEntity` wrapper used by `Utility_PickBasics`. `0.12.0` can replace only start/direction with the tracked dominant-controller world ray while preserving native length, interaction type, LOS, output, `CanInteract`, distance, focus, and callbacks. Ghidra: `SOMA_GetClosestEntity`. |
| `0x1400bcd90` | Confirmed, control hook built | Shared AngelScript `iLuxEntity.SetMatrix` registration target used by derived Lux entity types. `0.16.0` replaces only exact `PlayerHands_*`, uniform quarter-scale, Normal/Normal matrices with a fresh tracked-grip root and otherwise forwards the original pointer. Ghidra: `SOMA_iLuxEntity_SetMatrix`. |
| `0x14000fb60` | Confirmed, identity gate built | Compact inherited `iLuxEntity.GetName` accessor registered for `cLuxProp`; returns native `tString` at entity `+0x120`. Exact `PlayerHands_*` prefix matching bounds all hand telemetry and control. Ghidra: `SOMA_iLuxEntity_GetName`. |
| `0x14016ebe0` | Confirmed | `cLuxProp` AngelScript registration owner. Registers inherited GetName through `0x14000fb60` and SetMatrix through `0x1400bcd90`. Ghidra: `SOMA_Script_Register_cLuxProp`. |
| `0x1404a5030` | Confirmed | Registers the AngelScript `iCharacterBody` API, including `Move`, `SetMoveSpeed`, `AddYaw`, and `SetYaw`. |
| `0x1402375f0` | Confirmed, control built | Native wrapper registered for `iCharacterBody::Move(eCharDir, float)`. `0.14.0` calls Forward `0` and Right `1` only while the unpaused normal player/move state owns the body. Ghidra: `HPL3_Script_iCharacterBody_Move`. |
| `0x140237460` | Confirmed, control built | Native wrapper registered for `iCharacterBody::AddYaw(float)`. Adds radians to body `+0xd4`; `0.14.0` uses it for exact-degree snap/smooth turning under the same normal-state gate. Ghidra: `HPL3_Script_iCharacterBody_AddYaw`. |
| `0x1400ccc90` | Confirmed, control built | Registered `cLux_GetGamePaused()` wrapper. Reads game subsystem `gameContext+0xc8`, paused byte `+0x2d4`; `0.16.0` uses it to suppress both direct body calls and every synthetic gameplay fallback while routing paused controller aim/clicks to the native menu. Ghidra: `SOMA_GetGamePaused`. |
| `0x14015ca10` | Confirmed | Registers the AngelScript `cLuxPlayer` API. Maps `GetCamera` to `0x140125ef0` and `GetCharacterBody` to `0x140155290`. |
| `0x1400cc860` | Confirmed by registration and decompilation | Global `GetPlayer()` wrapper. Returns the current `cLuxPlayer*` from game context `+0x140`. Signature-guarded probe anchor in `0.7.0`. Ghidra: `SOMA_GetPlayer`. |
| `0x140125ef0` | Confirmed by registration and decompilation | `cLuxPlayer::GetCamera()`. Returns player `+0x168`. Ghidra: `SOMA_cLuxPlayer_GetCamera`. |
| `0x140155290` | Confirmed by registration and decompilation | `cLuxPlayer::GetCharacterBody()`. Returns player `+0x170`. Ghidra: `SOMA_cLuxPlayer_GetCharacterBody`. |
| `0x140155050` | Confirmed by registration and decompilation | `cLuxPlayer::GetCurrentStateId()`. Reads state object at player `+0x1d8`, then ID `+0x160`, or returns `-1`. Runtime probe anchor in `0.7.0`. |
| `0x140155090` | Confirmed by registration and decompilation | `cLuxPlayer::GetCurrentMoveStateId()`. Reads move state at player `+0x200`, then ID `+0x158`, or returns `-1`. Runtime probe anchor in `0.7.0`. |
| `0x140155c40` | High-confidence by behavior and HPL2 comparison | Player camera-direction update. Smooths input accumulators at player `+0x368/+0x36c`, applies camera pitch/yaw, and synchronizes body/camera yaw ownership. Ghidra: `SOMA_cLuxPlayer_UpdateCameraDirection`. |
| `0x14015ba20` | High-confidence by behavior | Per-frame player helper update. Derives `cLuxPlayer` as `self-0x110` and runs collision, motion averaging, head, and camera helpers. Ghidra: `SOMA_cLuxPlayerHelper_Update`. |
| `0x1404a94f0` | Confirmed by registration and HPL2 match | `cCamera::GetRotateMode`; reads camera `+0x6c`. Euler mode is `0`, matrix mode is `1`. Ghidra: `HPL3_Camera_GetRotateMode`. |
| `0x14049bdf0` | Confirmed by registration | `iCharacterBody::SetCameraUpdateActive`; writes body `+0x1e8`. Ghidra: `HPL3_CharacterBody_SetCameraUpdateActive`. |
| `0x14049be00` | Confirmed by registration | `iCharacterBody::GetCameraUpdateActive`; reads body `+0x1e8`. Ghidra: `HPL3_CharacterBody_GetCameraUpdateActive`. |
| `0x14033c240` | Confirmed | Adds a post effect to the priority-sorted container and retained effect list. |
| `0x14033b8f0` | Confirmed, control hook built | Tests whether the composite has any active post effects. `0.5.2` uses an exact-signature F12 detour to return false for reversible post-chain isolation. |
| `0x1402d7a40` | Confirmed | Executes one active post effect through virtual `+0x68` and performs the final full-screen copy when appropriate. Ghidra: `HPL3_PostEffect_RenderOne`. |
| `0x14033b950` | Confirmed | Initializes post-composite frame state, target ratios, renderer state, and texture units. Ghidra: `HPL3_PostEffectComposite_BeginRender`. |
| `0x14033bb00` | Confirmed | Restores renderer state and publishes the post-composite result. Ghidra: `HPL3_PostEffectComposite_EndRender`. |
| `0x14038ae60` | Confirmed | Creates the image-trail history texture and framebuffer (`ImageTrailTexture`, `ImageTrailBuffer`). |
| `0x1403896e0` | Confirmed | Initializes `posteffect_chromatic_aberration_frag.hpsl` and its uniforms. |
| `0x14038a5d0` | Confirmed | Initializes `posteffect_radial_blur_frag.hpsl` and its uniforms. |
| `0x1403870a0` | Confirmed | Initializes `posteffect_image_fade_fx_frag.hpsl` and its uniforms. |
| `0x140271870` | Confirmed by HPL2 layout | `cCamera::GetViewMatrix` accessor. Rebuilds through `0x140271250` when dirty flag `camera+0x709` is set, then returns `camera+0x74`. |
| `0x140289340` | Confirmed, probe built | Live FMOD listener update. Reads position, velocity, forward, and up from the sound-system object, validates them, calls `set3DListenerAttributes`, then calls `EventSystem::update`. `0.5.1` observes its fields against the OpenXR head pose without mutation. |
| `0x14061d188` | Confirmed import | `FMOD::EventSystem::set3DListenerAttributes` import/call target. Candidate late audio-listener correction point. |
| `0x14048b2d4` | Confirmed by registration string | Registers `SetCurrentListener(cViewport@)` for AngelScript. |
| `0x14048c89f` | Confirmed by registration string | Registers `CreateVideo(const tString&)`, returning `iVideoStream`. |
| `0x14048c8e4` | Confirmed by registration string | Registers `DestroyVideo(iVideoStream@)`. |
| `0x1400edb2a`-`0x1400edd19` | Confirmed by registration strings | Registers load-screen force-background, small-icon, show-icon, and bar position/size controls. |

## HPL3 Camera Layout

Offsets confirmed from decompilation and the matching HPL2 source:

| Object | Offset | Meaning |
| --- | --- | --- |
| `cCamera` | `+0x10` | Position (`cVector3f`). |
| `cCamera` | `+0x44/+0x48/+0x4c` | Base pitch, yaw, and roll. `0.9.0` observes roll and can temporarily zero it only while the active VR frustum is evaluated. |
| `cCamera` | `+0x60/+0x64/+0x68` | Extended/authored pitch, yaw, and roll used by the secondary frustum. |
| `cCamera` | `+0x6c` | `eCameraRotateMode`: Euler angles `0`, matrix `1`. `HPLPlayerState` uses nonzero as one authored-camera signal. |
| `cCamera` | `+0x74` | Cached view matrix returned by `0x140271870`. |
| `cCamera` | `+0x709` | View-matrix dirty flag used by `0x140271870`. |
| `cCamera` | `+0x70b` | Projection/cache dirty byte set by the base angle setters. |
| `cCamera` | `+0x70c` | Base-frustum dirty flag. |
| `cCamera` | `+0x70d` | Secondary-rotation frustum dirty flag. |
| `cFrustum` | `+0x18/+0x1c` | Far/near planes. |
| `cFrustum` | `+0x20/+0x24` | Aspect/FOV. |
| `cFrustum` | `+0x30` | Infinite-far flag. |
| `cFrustum` | `+0x34` | Projection type (`0` perspective, `1` orthographic). |
| `cFrustum` | `+0x38` | Origin (`cVector3f`). |
| `cFrustum` | `+0xd8` | Projection matrix. |
| `cFrustum` | `+0x118` | View-projection matrix. |
| `cFrustum` | `+0x158` | View matrix. |

## SOMA Player Layout

Offsets confirmed by script registrations and the `0.7.0` state probe anchors:

| Offset | Meaning |
| --- | --- |
| `+0x168` | Active `cCamera*`. |
| `+0x170` | Active `iCharacterBody*`. |
| `+0x1d8` | Current player-state object; state ID is object `+0x160`. |
| `+0x200` | Current move-state object; move-state ID is object `+0x158`. |
| `+0x368/+0x36c` | Smoothed camera direction input accumulators used by `0x140155c40`. |

## HPL3 Character Body Layout

`0x1402375f0` is the confirmed AngelScript `iCharacterBody::Move` wrapper. It
adds the supplied amount to body `+0x94 + direction*4` and records the direction
at `+0xbc`. This is a low-level accumulator below SOMA's player-state input
dispatch, not a safe semantic locomotion boundary by itself.

| Object | Offset | Meaning |
| --- | --- | --- |
| `iCharacterBody` | `+0x1e8` | Camera-update ownership boolean exposed as `Get/SetCameraUpdateActive`. SOMA's hands script clears it during camera-to-bone attachment and restores it afterward. |

## SOMA Lux Entity Layout

| Object | Offset | Meaning |
| --- | --- | --- |
| `iLuxEntity` / `cLuxProp` | `+0x120` | Native MSVC `tString` name returned by registered `GetName`; `PlayerHands_*` is the exact runtime hand identity. |

The native x64 string layout used here is 16 bytes of inline storage or a heap
pointer, followed by length at `+0x10` and capacity at `+0x18`. The hands probe
bounds length/capacity and uses `ReadProcessMemory` before accepting an identity.
HPL `cMatrixf` translation is row-major elements `[3,7,11]`; basis/scale rows
are `[0..2]`, `[4..6]`, and `[8..10]`.

## HPL3 Post-Effect Composite Layout

| Object | Offset | Meaning |
| --- | --- | --- |
| composite | `+0x328` | Priority-sorted effect tree used for render order. |
| composite | `+0x340/+0x348` | Begin/end pointers for retained `iPostEffect*` vector. |
| effect | `+0x30` | Suppressed/disabled byte; an effect renders only when this is zero. |
| effect | `+0x31` | Active byte; an effect renders only when this is nonzero. |

Confirmed post-effect vtable RVAs used for runtime identity:

| Effect | Vtable RVA | GetTypeName |
| --- | --- | --- |
| ToneMapping | `0x69b038` | `0x1402859b0` |
| FXAA | `0x6ac3b8` | `0x140386170` |
| ImageFadeFX | `0x6ac4e8` | `0x140386b20` |
| VideoDistortion | `0x6ac688` | `0x1403878a0` |
| ChromaticAberration | `0x6ac928` | `0x140388ee0` |
| RadialBlur | `0x6acb78` | `0x140389ed0` |
| ImageTrail | `0x6acd48` | `0x14038ad00` |

The MSVC tree rooted through composite `+0x328` stores priority at node `+0x18`
and effect pointer at node `+0x20`; child links are `+0x0/+0x10` and `_Isnil`
is node `+0x29`.

## HPL3 GUI Set Layout

The shared HPL game-context pointer used by the compact HUD wrappers resolves to
image address `0x1407925e0` for this executable. Runtime code derives this slot
from the guarded RIP-relative getter instruction instead of hard-coding it.

Fields confirmed in `HPL3_GuiSet_Render` at `0x140213970`:

| Offset | Meaning |
| --- | --- |
| `+0x100/+0x104` | Virtual width and height. |
| `+0x108/+0x10c` | Virtual coordinate offsets. |
| `+0x110/+0x114` | GUI depth range. |
| `+0x138` | Depth/3D-layer behavior flag. |
| `+0x139` | 3D GUI flag. |
| `+0x188` | GUI-set render priority. |

## HPL3 Viewport Layout

Offsets confirmed by the typed `HPL3_Scene_RenderViewport` decompilation:

| Offset | Meaning |
| --- | --- |
| `+0x18` | Camera pointer. |
| `+0x20` | World pointer. |
| `+0x30` | Renderer pointer. |
| `+0x38` | Post-effect composite pointer. |
| `+0x40` | Auxiliary viewport/render object; exact type pending. |
| `+0x48` | Render-target packet/pointer. |
| `+0x50` / `+0x58` | Render-target position and size values. |
| `+0x60` | Viewport pre/post-world callback list. |
| `+0x78` | Renderer callback list/packet passed to world render. |
| `+0xa8` | Render settings pointer. |

## HPL3 Sound Listener Layout

Offsets confirmed at `HPL3_FMOD_UpdateListenerAndSystem`:

| Offset | Meaning |
| --- | --- |
| `+0x50` | Listener up vector. |
| `+0x5c` | Listener forward vector. |
| `+0x74` | Listener position. |
| `+0x80` | Listener velocity. |
| `+0xd8` | `FMOD::EventSystem*`. |

## Ghidra Database

Confirmed function names, prototypes, plate comments, instruction comments, and
tags were synchronized on 2026-07-12. See `docs\GHIDRA_SYNC.md` for the exact
promotion ledger and pending targets.

Keep this file limited to addresses that have been touched in Ghidra or validated at runtime.
