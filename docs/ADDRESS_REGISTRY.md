# Address Registry

Program: `Soma_NoSteam.exe` in Ghidra.

| Address | Status | Notes |
| --- | --- | --- |
| `entry` / `0x140642bb4` | Confirmed | CRT entry, calls `__security_init_cookie` and `__tmainCRTStartup`. The exit-hang dump's sole thread starts here. |
| `__tmainCRTStartup` | Confirmed | Visual Studio 2010 Release CRT startup. Calls `FUN_1401daec0`. |
| `0x1401daec0` | Confirmed | Command-line/startup bridge. Calls `SetProcessDPIAware`, converts command line, then calls `FUN_140125a20`. |
| `0x140125a20` | Confirmed | Likely real app entry. Allocates `0x8d0` app object, calls init/run/shutdown sequence. |
| `0x14004b620` | Candidate | Init path. Logs `Version %d.%02d` as `1.110`, then performs staged setup. |
| `0x140109b30` | Confirmed, control hook built | Script-visible `cLuxInputHandler::SetRumble(int,float,float)`. `0.42.0` preserves native gamepad behavior and mirrors authored global strength/duration through a throttled bilateral OpenXR envelope. Ghidra: `SOMA_cLuxInputHandler_SetRumble`. |
| `0x14032f0e0` | Confirmed, control hook built | HPL3 `cSurfaceData::OnImpact(float,const cVector3f&,int,iPhysicsBody*)`. Native Newton update passes normal collision speed, contact position/count, and the selected material-side body. `0.58.0` calls the original unchanged, then emits an opt-in dominant-hand pulse only in Grab state when the tracked grip is fresh and near the contact. Ghidra: `HPL3_cSurfaceData_OnImpact`. |
| `0x14032f380` | Confirmed, documented | HPL3 `cSurfaceData::OnSlide(float,const cVector3f&,int,iPhysicsBody*,iPhysicsBody*)`. Owns continuous scrape sound start/update/stop from tangent speed and minimum contact count. Not hooked in `0.58.0`; retained as the exact future sustained-friction evidence point. Ghidra: `HPL3_cSurfaceData_OnSlide`. |
| `0x1405548b0` | Confirmed, documented | `cPhysicsWorldNewton::Update(float)`. Steps Newton, walks `0x60`-byte contact records, dispatches impact/slide by material priority, then calls both body collision callbacks. Confirms normal speed `+0x18`, tangent speed `+0x1c`, contact position `+0x2c`, body pointers `+0x38/+0x40`, material pointers `+0x48/+0x50`, and contact count `+0x58`. Ghidra: `HPL3_cPhysicsWorldNewton_Update`. |
| `0x1401158c0` | Confirmed registration | Registers `cLuxInputHandler` script methods and binds `SetRumble` directly to `0x140109b30`. Ghidra: `SOMA_Script_Register_cLuxInputHandler`. |
| `0x14015bf50` | Confirmed script wrapper | `cLuxPlayer::GiveDamage`; shipped player damage handling starts `Effect_Rumble_Start`, which converges on the hooked rumble boundary. Ghidra: `SOMA_cLuxPlayer_GiveDamage`. |
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
| `0x1400ab230` | Confirmed | `HPL3_Camera_GetPosition`; returns camera world position at `camera+0x10`. Confirmed while tracing native grab targets; broad evidence getter, deliberately not hooked. |
| `0x140237270` / `0x1402b8200` | Confirmed | `HPL3_Camera_GetPitch` and `HPL3_Camera_GetYaw`; return base angles at `camera+0x44/+0x48`. Grab/camera evidence getters, deliberately not hooked. |
| `0x1404e1a80` | Confirmed | Registers the `cCamera` pitch/yaw/roll and extended-rotation AngelScript surface. Ghidra: `HPL3_Script_Register_cCamera`. |
| `0x140270e10` | Confirmed by HPL2 match | `cCamera::GetProjectionMatrix`; caches perspective/orthographic projection at `camera+0xf4`. |
| `0x140270230` | Confirmed by HPL2 match | `cFrustum::SetupPerspectiveProj`; passes projection, view, far, near, FOV, and aspect to the common setup at `0x14026fcf0`. HPL2's matching finite projection maps near/far to standard OpenGL NDC `-1/+1`, hence normalized depth `0/1`. Direct-call RVA for the camera bridge. |
| `0x14026fcf0` | Confirmed by decompile and HPL2 match | Common frustum setup. Stores projection at `frustum+0xd8`, view at `+0x158`, far at `+0x18`, and near at `+0x1c`, then updates view-projection, planes, sphere, vertices, and bounding volume. `0.33.0` uses these confirmed clip semantics for `XrCompositionLayerDepthInfoKHR`; HPL world distances are divided by configured world units per meter. Ghidra: `HPL3_Frustum_SetupCommon`. |
| `0x1400cd710` | Confirmed, guarded control built | Registered AngelScript `CheckLineOfSight` wrapper. ABI is `bool(const float* start, const float* end, bool shadowOnly, bool staticOnly)`; supplies a null skip entity and calls `0x140143650`. `0.24.0` signature-guards it for static-world room-scale translation safety; `0.25.0` reuses it for center/radial/top/bottom head-volume probes. Ghidra: `SOMA_CheckLineOfSight`. |
| `0x140143650` | Confirmed, documented | Underlying world/physics line query. Resolves the active world, configures query filters, invokes the physics callback at vtable `+0x148`, and returns true only when no obstruction was recorded. Ghidra: `SOMA_PhysicsRay_CheckLineOfSight`. |
| `0x1400cd7d0` | Confirmed, guarded control hook built | Registered global `GetClosestBody` wrapper. ABI is `body(const float* start, const float* direction, float length, float* outDistance, float* outNormal)` and forwards to `0x140143a10`. `0.27.0` redirects only camera-origin rays in the recovered flashlight length range, preserving the randomized cone and every output. Ghidra: `SOMA_GetClosestBody`. |
| `0x140143a10` | Confirmed, documented | Underlying closest-body physics ray. Forms `end=start+direction*length`, invokes the active physics world callback at vtable `+0x148`, and returns the nearest body plus distance/normal. Ghidra: `SOMA_PhysicsRay_GetClosestBody`. |
| `0x1402702a0` | Confirmed by HPL2 match | `cFrustum::SetupOrthoProj`; orthographic sibling, deliberately not modified by the first bridge. |
| `0x140298630` | Confirmed, guarded continuous replay built | `HPL3_Scene_RenderViewport`. Exact fields: camera `+0x18`, world `+0x20`, active/visible/listener `+0x28/+0x29/+0x2a`, renderer `+0x30`, post composite `+0x38`, framebuffer `+0x48`, position `+0x50`, size `+0x58`, render settings `+0xa8`. Mask bit `1` runs world/callback/overlay work, bit `4` admits active post effects, bit `2` runs screen GUI, and post-post callbacks are unconditional. `0.45.0` can continuously replay only the exact player viewport with bit `2` removed after immediately caching eye one. The control is opt-in and disables itself on cache or eye-sequence failure; bounded manual/automatic diagnostic arms retain precedence. |
| `0x140298692` | Confirmed | Main render-viewport call to `cCamera::GetFrustum`; return RVA `0x298697` is an additional runtime selection guard in `0.4.0`. |
| `0x14022f7d0` | Confirmed by HPL2 match | Viewport pre/post-world callback dispatcher. Phase `0` invokes callback vtable `+0x08`; phase `1` invokes `+0x10`. Ghidra: `HPL3_Viewport_RunWorldDrawCallbacks`. |
| `0x1402332b0` | Confirmed | Main engine run loop. Dispatches script `OnDraw`, renders viewports through `0x140298850`, dispatches `OnPostRender`, then presents. |
| `0x1402328f0` | Confirmed | Script object/module lifecycle dispatcher. Callback id `2` is `_OnDraw`, `3` is post-render, `4/5/6` are update/post-update/variable-update. |
| `0x140154c40` | Confirmed | Dispatches a lifecycle id to the corresponding virtual method on an active player/module object. |
| `0x140298850` | Confirmed | Enumerates active viewports and calls `0x140298630` for each one. `0.45.0` deliberately remains below this boundary and never replays the enumerator. |
| `0x140298850` globals | Confirmed | Increments the renderer frame counter and resets render statistics once before active viewport enumeration; this boundary executes once per game frame in the continuous dual-render prototype. |
| `0x140297f20` | Confirmed | `HPL3_Scene_CreateViewport`; allocates `0xb0`, stores camera/world/renderer/post ownership and default `-1,-1` size, then inserts the viewport into the scene list. |
| `0x140297500` | Confirmed | `HPL3_Scene_DestroyViewport`; removes and destroys an owned viewport. |
| `0x140487ed0` / `0x140487f10` | Confirmed | Registered script wrappers for CreateViewport and DestroyViewport. Ghidra: `HPL3_Script_CreateViewport`, `HPL3_Script_DestroyViewport`. |
| `0x1401f9790` | High-confidence | Main scene render invoked by `0x140298630` before post effects and GUI. |
| `0x140297670` | Confirmed by order, probe built | Viewport renderer callback pass after scene render and before active post-effect composition. Passive hook in `0.5.1` classifies calls/FBO state before dual rendering. |
| `0x14033bd80` | Confirmed | Active post-effect composite render. Iterates the priority-sorted effect tree and ping-pongs outputs. |
| `0x1401f1480` | Confirmed by decompilation, HPL2 source, and order; guarded control built | `HPL3_Renderer_RenderPostPostEffects`, a full deferred/post-post renderer phase after the optional post chain and before final GUI drawing. It performs GPU work and callbacks, clears renderer `+0x69`, and copies the active frustum's `0x40`-byte view matrix from `*(renderer+0x20)+0x158` into previous-view history at `*(renderer+0x438)+0x80`; HPL2 identifies the destination field as `cMatrixf m_mtxPrevView`. `0.47.0` banks this exact packet per eye in AFR and same-frame stereo, restoring before the complete exact-player viewport and committing after the native copy only when predicted and actual eye/pose identities agree. Renderer/history replacement, recenter generation, and pose gaps over eight frames reseed from native state. All pointer access is guarded and faults closed to shared native history. Other temporal resources remain unclassified. |
| `0x1402981e0` | Confirmed | Collects, priority-sorts, and renders viewport GUI sets after scene post effects. Parent iteration boundary for gameplay-HUD capture classification. |
| `0x1401297c0` | Confirmed | `cLuxUserModule` script `OnGui(float)` dispatcher; `mlId` is at module `+0x158`. Shipped IDs include GameOver `10`, Wake `12`, Credits `19`. Callback traffic is not an activity signal because scripts may immediately return. Ghidra: `SOMA_cLuxUserModule_OnGui`. |
| `0x1401378e0` | Confirmed, observer hook built | Native `cLuxUserModule::OnAction(int,bool)` wrapper. Its exact 26-byte body loads the AngelScript object from module `+0x90` and forwards action/pressed to `0x140129a40`. `0.44.0` reads confirmed `mlId +0x158` and authorizes inventory HUD capture only for module `15`, action `12`, pressed edges. Ghidra: `SOMA_cLuxUserModule_OnAction`. |
| `0x140129a40` | Confirmed | AngelScript `OnAction(int,bool)` dispatcher reached by the native user-module wrapper. Resolves and invokes the script method when present. Ghidra: `SOMA_ScriptObject_OnAction`. |
| `0x1401ae870` | Confirmed registration | Registers `cLuxUserModule`, property `int mlId` at native offset `+0x158`, and the module action interface. Ghidra: `SOMA_Script_Register_cLuxUserModule`. |
| `0x14022f8e0` | Confirmed | Creates an iterator over the viewport GUI-set list at viewport `+0x90`. Ghidra: `HPL3_Viewport_CreateGuiSetIterator`. |
| `0x140213970` | Confirmed, control hook built | Renders one `cGuiSet`; 2D sets draw into the current framebuffer. `0.31.0` accumulates only exact GameHudSet and exact GameHudImGui-owned set draws into a clear-once-per-frame transparent capture; all other sets remain native. Ghidra: `HPL3_GuiSet_Render`. |
| `0x1400cc9b0` | Confirmed, identity gate built | `SOMA_GetGameHudSet`; returns game-context `+0x50`. The signature-derived context slot gates exact gameplay-HUD capture and fail-closed native fallback. |
| `0x1400cc9c0` / `0x1400cc9d0` | Confirmed | HUD virtual-center size at context `+0x58` and virtual size at `+0x60`. Ghidra: `SOMA_GetHudVirtualCenterSize`, `SOMA_GetHudVirtualSize`. |
| `0x1400cc9f0` | Confirmed | `SOMA_GetHudVirtualStartPos`; returns context `+0x70`. |
| `0x1400cca00` / `0x1400cca10` | Confirmed | Center-screen virtual size/start-position getters at context `+0x7c/+0x84`. |
| `0x1400cca70` | Confirmed | `SOMA_GetCurrentImGui`; follows game-context `+0xe8`, then `+0x168`. |
| `0x1400cca90` | Confirmed, identity/control gate built | `SOMA_GetGameHudImGui`; follows game-context `+0xe8`, then `+0x160`. Registered by exact `cImGui@ GetGameHudImGui()` string. `0.31.0` uses the signature-guarded identity for additive gameplay-HUD capture. |
| `0x140071f20` | Confirmed, identity/control gate built | `HPL3_ImGui_GetSet`; five-byte registered wrapper returns `cImGui +0x18` as the owned `cGuiSet`. `0.31.0` captures only the set reached from exact GameHudImGui; current/menu/diegetic owners remain native. |
| `0x1402f0b10` | Confirmed | `HPL3_ImGui_SendMousePosition`; registered physical-pixel cursor wrapper. It normalizes through low-level screen size, multiplies by owned `cGuiSet +0x100/+0x104`, subtracts `+0x108/+0x10c`, and updates cImGui mouse fields. |
| `0x1402f0c90` | Confirmed, control hook built | `HPL3_ImGui_SendMouseVirtualPosition`; registered virtual-coordinate cursor wrapper and exact world/3D GUI dispatch boundary. It stores position at cImGui `+0x4f3c/+0x4f40`, relative delta at `+0x4f4c/+0x4f50`, and clears first-update flag `+0x4f72`. `0.38.0` substitutes controller-derived coordinates only for exact wall/handheld terminal states `8/9`, current non-GameHud ImGui, and readable 3D cGuiSet ownership. |
| `0x1400f7f10` | Confirmed | `SOMA_ImGuiManager_UpdateInput`; selects the active ImGui owner. Its world-GUI branch projects native input and calls `0x1402f0c90`; the ordinary screen owner receives `0x1402f0b10`. |
| `0x1403132d0` | Confirmed, consumed | `HPL3_GuiSetEntity_ProjectRayToVirtual`; intersects a world ray segment with the spatial GUI mesh, barycentrically interpolates UV set `7`, and scales by cGuiSet virtual dimensions at `+0x100/+0x104`. `0.63.0` uses this exact native projector for controller-addressed terminals; mesh misses deactivate the pointer. |
| `0x1401c8dd0` | Confirmed, control built | `SOMA_VoiceSubtitle_Render`; exact native subtitle draw worker. Its render object points at `cLuxVoiceHandler` through `+0x10`; the worker draws localized speaker/text rows and reads owner `+0x174` width, `+0x178` active Y, `+0x17c` active font size, `+0x180` shadow offset, and `+0x350` font. `0.34.0` temporarily scales only the four layout floats during active stereo and restores them after the original call. |
| `0x1401d3ba0` | Confirmed | `SOMA_cLuxVoiceHandler_Constructor`; loads shipped Voice settings including gradual display, width, normal/large font sizes and Y positions, shadow offset, and font resource. Normal/large source values occupy `+0x188/+0x18c/+0x190/+0x194`; active draw values occupy `+0x174..+0x180`. |
| `0x1400cd750` | Confirmed, control/result hook built | Registered global `GetClosestEntity` wrapper used by `Utility_PickBasics`. Native output is distance `+0x18`, physics body `+0x20`, and Lux entity `+0x28`, followed by output vtable finalizer `+0x40`. `0.64.0` probes the inner raycast for both hands, copies only the selected candidate, and invokes this finalizer once; native length, LOS, `CanInteract`, focus, and callbacks remain authoritative. Ghidra: `SOMA_GetClosestEntity`. |
| `0x140484ea0` | Confirmed, observer hook built | Registered `cScript_RunGlobalFunc` thunk. The full 21-byte signature is guarded; exact `LuxPlayer::_Global_SetCrosshairState` and `WakeHandler::_Global_SetAsleep/_Global_StartWakeup` calls are observed while original dispatch remains authoritative. Ghidra: `HPL3_Script_RunGlobalFunc`. |
| `0x1404851d0` | Confirmed, read dependency built | Registered `cScript_GetGlobalArgInt(int)` wrapper. Returns parsed global argument through EAX despite the original decompiler's tail-jump ambiguity. `HPLCrosshairBridge` reads index zero immediately before the native callback. Ghidra: `HPL3_Script_GetGlobalArgInt`. |
| `0x140485200` | Confirmed, read dependency built | Registered `cScript_GetGlobalArgFloat(int)` wrapper. Returns argument zero through XMM0; `0.43.0` reads the shipped wake duration before native dispatch. Ghidra: `HPL3_Script_GetGlobalArgFloat`. |
| `0x140485720` | Confirmed, read dependency built | Registered `cScript_GetGlobalArgBool(int)` wrapper. `0.43.0` reads the shipped asleep flag before native dispatch. Ghidra: `HPL3_Script_GetGlobalArgBool`. |
| `0x140485810` | Confirmed, documented | Registered `cScript_SetGlobalArgInt(int,int)` wrapper. Converts the integer to a decimal string and stores the global argument; shipped `Player_SetCrossHairState` uses it before dispatch. Ghidra: `HPL3_Script_SetGlobalArgInt`. |
| `0x1400bcd90` | Confirmed, control hook built | Shared AngelScript `iLuxEntity.SetMatrix` registration target used by derived Lux entity types. `0.31.0` maps exact independent `HudObject` to a fresh dominant-grip camera-style basis while classifying socketed `*_HudObject` without overriding it. `0.36.0` optionally keeps that tool rooted at the dominant grip and aims it toward a squeezed, fresh support grip within bounded separation; failed support gates return to dominant-only control. Existing exact `PlayerHands_*` and `Flashlight` policies remain; every failed ownership gate forwards the original pointer. Ghidra: `SOMA_iLuxEntity_SetMatrix`. |
| `0x14000fb60` | Confirmed, identity gate built | Compact inherited `iLuxEntity.GetName` accessor registered for `cLuxProp`; returns native `tString` at entity `+0x120`. Exact `PlayerHands_*`, `HudObject`, `*_HudObject`, and `Flashlight` identities bound all transform telemetry/control. Ghidra: `SOMA_iLuxEntity_GetName`. |
| `0x140127a70` | Confirmed, lifecycle hook built | `cLuxMap::DestroyEntity`; marks entity `+0x6a8` pending destruction and queues it on map `+0x4f0`. `0.31.0` evicts the exact entity pointer from the bounded hands/tool identity cache before native queueing, preventing stale identity inheritance if SOMA reuses an address. Ghidra: `SOMA_cLuxMap_DestroyEntity`. |
| `0x14016ebe0` | Confirmed | `cLuxProp` AngelScript registration owner. Registers inherited GetName through `0x14000fb60` and SetMatrix through `0x1400bcd90`. Ghidra: `SOMA_Script_Register_cLuxProp`. |
| `0x1404a5030` | Confirmed | Registers the AngelScript `iCharacterBody` API, including `Move`, `SetMoveSpeed`, `AddYaw`, and `SetYaw`. |
| `0x1402375f0` | Confirmed, control built | Native wrapper registered for `iCharacterBody::Move(eCharDir, float)`. `0.14.0` calls Forward `0` and Right `1` only while the unpaused normal player/move state owns the body. Ghidra: `HPL3_Script_iCharacterBody_Move`. |
| `0x140237460` | Confirmed, control built | Native wrapper registered for `iCharacterBody::AddYaw(float)`. Adds radians to body `+0xd4`; `0.14.0` uses it for exact-degree snap/smooth turning under the same normal-state gate. Ghidra: `HPL3_Script_iCharacterBody_AddYaw`. |
| `0x140237920` | Confirmed, control hook built | Native `iCharacterBody::SetFeetPosition(const cVector3f&, bool)` wrapper. Converts feet Y to center Y with half of body size `+0x138`, then dispatches the ordinary position setter through vtable `+0x58`. `0.41.0` uses it for guarded roomscale catch-up; `0.63.0` suppresses only the wall-terminal state `8` setup teleport while VR tracking is active. Ghidra: `HPL3_CharacterBody_SetFeetPosition`. |
| `0x140237970` | Confirmed, control built | Native `iCharacterBody::GetFeetPosition()` wrapper with the Windows x64 hidden return buffer in `RDX`; returns center `+0x6c` minus half body height `+0x138`. `0.41.0` reads this immediately before each guarded roomscale catch-up step. Ghidra: `HPL3_CharacterBody_GetFeetPosition`. |
| `0x140238750` | Confirmed, control hook built | `HPL3_PidControllerVec3_Output`. Grab force (`400/0/40`) and torque (`40/0/0.4|0.1`) retain their controller targets. Slide state `4` force PID `6/0/0.1` receives controller velocity projected onto the native pin. `0.62.0` also recognizes SwingDoor/Lever states `5/6` only at torque PID `10/0/1` and adds controller arc velocity about the native pivot/pin. SOMA still owns PID integration, force/torque limits, mass, inertia, collision, joints, gravity, sounds, and callbacks; every unrelated tuple/state passes through. |
| `0x1401438c0` | Confirmed, control helper consumed | Native closest-entity raycast called by `0x1400cd750`; installed PE bytes begin `40 57 48 83 ec 60`. It writes distance through the first out pointer, physics body through the second, and owning Lux entity through the third without running the outer result finalizer. `0.64.1` safely obtains two local hand candidates here, then finalizes only the chosen outer result. The `48 8b 05` RIP load uses displacement `+9` and next instruction `+13` to resolve the game-context slot; ray owner is context `+0xc0`. Ghidra: `SOMA_Lux_GetClosestEntityRaycast`. |
| `0x140273510` | Confirmed, consumed | `iPhysicsBody::GetJoint(int)` leaf; reads pointer vector `body+0x168/+0x170`. `0.61.0` validates and reads joint 0 only for exact Slide PID ownership. Ghidra: `HPL3_PhysicsBody_GetJoint`. |
| `0x1401822f0` | Confirmed, consumed | Registered `iPhysicsJoint::GetPinDir()` leaf; returns `joint+0xe8`. `0.61.0` reads and normalizes the pin for controller-velocity projection. Ghidra: `HPL3_PhysicsJoint_GetPinDir`. |
| `0x1401822e0` | Confirmed, consumed | Registered `iPhysicsJoint::GetPivotPoint()` leaf; returns `joint+0xf4`. `0.62.0` combines this native hinge pivot with `GetPinDir()` for signed controller angular velocity in SwingDoor/Lever states. Ghidra: `HPL3_PhysicsJoint_GetPivotPoint`. |
| `0x14049c720` | Confirmed, control patch built | `HPL3_Script_iPhysicsBody_AddImpulse`. Nine-byte virtual thunk dispatching through body vtable `+0x130`; `0.18.0` signature-guards the thunk plus three INT3 bytes and redirects only a 350 ms controller-armed Grab throw before calling the concrete virtual method. |
| `0x1404a0480` | Confirmed registration owner | `HPL3_Script_Register_iPhysicsBody`; registers AddForce/AddTorque/AddImpulse and anchors the physics-body AngelScript ABI. |
| `0x140534eb0` | Confirmed | `HPL3_Script_Register_PidControllers`; registration owner used to confirm vector PID script ownership. |
| `0x1400ccc90` | Confirmed, control built | Registered `cLux_GetGamePaused()` wrapper. Reads game subsystem `gameContext+0xc8`, paused byte `+0x2d4`; `0.16.0` uses it to suppress both direct body calls and every synthetic gameplay fallback while routing paused controller aim/clicks to the native menu. Ghidra: `SOMA_GetGamePaused`. |
| `0x14015ca10` | Confirmed | Registers the AngelScript `cLuxPlayer` API. Maps `GetCamera` to `0x140125ef0` and `GetCharacterBody` to `0x140155290`. |
| `0x140159360` | Confirmed, control hook built | Registered `cLuxPlayer::SetCameraPosAdd(int,const cVector3f&)` wrapper. `0.19.0` zeroes configured Bob `1`, Shake `2`, and Sway `9` calls while F10 tracking is active. `0.63.0` also zeros Terminal type `4` only during live wall-terminal state `8`. Ghidra: `HPL3_Script_cLuxPlayer_SetCameraPosAdd`. |
| `0x1401562e0` | Confirmed, control hook built | Registered `cLuxPlayer::RotateCameraTowards(float,float,float,const cVector3f&,bool)` leaf. Stores active flag, acceleration/speed/maximum, target direction, and local flag at player `+0x364/+0x370..+0x388`. `0.63.0` suppresses only wall-terminal state `8` calls while tracking is active. Ghidra: `SOMA_cLuxPlayer_RotateCameraTowards`. |
| `0x140156d90` | Confirmed, control hook built | Registered `cLuxPlayer::FadeCameraRollTo(int,float,float,float)`. `0.28.0` may zero only configured Script `0`, Lean `1`, Move `2`, or Climb `3` targets during active VR while preserving speed multiplier and maximum speed. Ghidra: `HPL3_Script_cLuxPlayer_FadeCameraRollTo`. |
| `0x140156f00` | Confirmed, control hook built | Registered `cLuxPlayer::SetCameraRoll(int,float)`. Expands roll arrays rooted at player `+0x3a0/+0x3c0`; `0.28.0` applies the same independent semantic roll policy as the fade wrapper. Ghidra: `HPL3_Script_cLuxPlayer_SetCameraRoll`. |
| `0x140155210` | Confirmed, reversible control patch built | Exact registered `cLuxPlayer::FadeCameraFOVMulTo(float,float)` leaf. Writes target/speed to player `+0x38c/+0x390`; `0.29.0` can substitute target `1.0` during active VR. Ghidra: `HPL3_Script_cLuxPlayer_FadeCameraFOVMulTo`. |
| `0x140155230` | Confirmed, reversible control patch built | Exact registered `cLuxPlayer::FadeCameraAspectMulTo(float,float)` leaf. Writes target/speed to player `+0x394/+0x398`; `0.29.0` can substitute target `1.0` during active VR. Ghidra: `HPL3_Script_cLuxPlayer_FadeCameraAspectMulTo`. |
| `0x140155250` | Confirmed, reversible control patch built | Exact registered `cLuxPlayer::FadeCameraFOVTo(float,float)` leaf. Writes target/speed to player `+0x19c/+0x1a0`; `0.29.0` substitutes the validated native default at player `+0x194` during active VR. Ghidra: `HPL3_Script_cLuxPlayer_FadeCameraFOVTo`. |
| `0x1400cc860` | Confirmed by registration and decompilation | Global `GetPlayer()` wrapper. Returns the current `cLuxPlayer*` from game context `+0x140`. Signature-guarded probe anchor in `0.7.0`. Ghidra: `SOMA_GetPlayer`. |
| `0x140125ef0` | Confirmed by registration and decompilation | `cLuxPlayer::GetCamera()`. Returns player `+0x168`. Ghidra: `SOMA_cLuxPlayer_GetCamera`. |
| `0x140155290` | Confirmed by registration and decompilation | `cLuxPlayer::GetCharacterBody()`. Returns player `+0x170`. Ghidra: `SOMA_cLuxPlayer_GetCharacterBody`. |
| `0x140155050` | Confirmed by registration and decompilation | `cLuxPlayer::GetCurrentStateId()`. Reads state object at player `+0x1d8`, then ID `+0x160`, or returns `-1`. Runtime probe anchor in `0.7.0`. |
| `0x140155090` | Confirmed by registration and decompilation | `cLuxPlayer::GetCurrentMoveStateId()`. Reads move state at player `+0x200`, then ID `+0x158`, or returns `-1`. Runtime probe anchor in `0.7.0`. |
| `0x140155c40` | High-confidence by behavior and HPL2 comparison | Player camera-direction update. Smooths input accumulators at player `+0x368/+0x36c`, applies camera pitch/yaw, and synchronizes body/camera yaw ownership. Ghidra: `SOMA_cLuxPlayer_UpdateCameraDirection`. |
| `0x14015ba20` | High-confidence by behavior | Per-frame player helper update. Derives `cLuxPlayer` as `self-0x110` and runs collision, motion averaging, head, and camera helpers. Ghidra: `SOMA_cLuxPlayerHelper_Update`. |
| `0x1404a94f0` | Confirmed by registration and HPL2 match, ownership handoff built | `cCamera::GetRotateMode`; reads camera `+0x6c`. Euler mode is `0`, matrix mode is `1`. `0.56.0` treats same-camera mode ownership changes as native-base and temporal-history cut points while preserving VR. Ghidra: `HPL3_Camera_GetRotateMode`. |
| `0x14049bdf0` | Confirmed by registration | `iCharacterBody::SetCameraUpdateActive`; writes body `+0x1e8`. Ghidra: `HPL3_CharacterBody_SetCameraUpdateActive`. |
| `0x14049be00` | Confirmed by registration, ownership handoff built | `iCharacterBody::GetCameraUpdateActive`; reads body `+0x1e8`. `0.56.0` combines it with camera rotate mode to detect authored ownership entry/exit even when the camera pointer and player-state ID remain unchanged. Ghidra: `HPL3_CharacterBody_GetCameraUpdateActive`. |
| `0x140071f80` | Confirmed, control patch built | Registered `cWorld::SetDepthOfFieldActive(bool)` wrapper. Exact seven-byte body writes `world+0x264` and is followed by nine INT3 bytes; `0.28.0` uses the complete 16-byte region for reversible active-VR DoF suppression. Ghidra: `HPL3_World_SetDepthOfFieldActive`. |
| `0x14033c240` | Confirmed | Adds a post effect to the priority-sorted container and retained effect list. |
| `0x14033b8f0` | Confirmed, control hook built | Tests whether the composite has any active post effects. `0.5.2` uses an exact-signature F12 detour to return false for reversible post-chain isolation. |
| `0x1402d7a40` | Confirmed, resource probe built | Executes one active post effect as `(effect, composite, inputTexture, tempFramebuffer, isLastEffect)`, returns the output texture from virtual `+0x68`, and performs the final full-screen copy when appropriate. `0.37.0` signature-guards this exact boundary and records bounded GL texture target/ID/dimensions/format plus framebuffer writes, eye, and pose frame. It classifies ownership only for resource-bearing same-pose left/right pairs and never changes the call or resources. Ghidra: `HPL3_PostEffect_RenderOne`. |
| `0x14033b950` | Confirmed | Initializes post-composite frame state, target ratios, renderer state, and texture units. Ghidra: `HPL3_PostEffectComposite_BeginRender`. |
| `0x14033bb00` | Confirmed | Restores renderer state and publishes the post-composite result. Ghidra: `HPL3_PostEffectComposite_EndRender`. |
| `0x1402842d0` | Confirmed, frame owner built | Advances ToneMapping exposure/white-cut output `+0x8c/+0x94`, authored source/destination/window/transition floats `+0xf8..+0x120`, and color-grading transition pointers/state `+0xa0/+0xd8..+0xf4` using renderer frame time. Same-frame stereo called it once per eye. `0.53.0` replays the same baseline for eye two and preserves only eye one's committed update. Ghidra: `HPL3_PostEffect_ToneMapping_AdvanceFrameState`. |
| `0x1402845d0` | Confirmed, frame owner built | Advances film-grain current/next UV sample packets at `+0x138..+0x154`; RenderEffect advances quantized phase `+0x158`. `0.53.0` includes all nine floats in the shared once-per-pose packet so both eyes sample equivalent grain and only one phase update persists. Ghidra: `HPL3_PostEffect_ToneMapping_AdvanceFilmGrainOffsets`. |
| `0x140284fd0` | Confirmed, frame owner built | ToneMapping render virtual. Calls `0x1402842d0` before selecting bloom, grading, film-grain, and final shader variants. `0.53.0` owns the confirmed mutable packet at the surrounding exact `RenderOne` boundary. Ghidra: `HPL3_PostEffect_ToneMapping_RenderEffect`. |
| `0x140284d70` / `0x140283fd0` | Confirmed | Create/destroy six reduced-size ToneMapping bloom framebuffer/texture scratch pairs. The bright and blur passes fully rewrite them each invocation, so they are sequential scratch resources rather than temporal eye history. Ghidra: `HPL3_PostEffect_ToneMapping_CreateBloomResources` / `DestroyBloomResources`. |
| `0x140284e40` / `0x140284770` | Confirmed | ToneMapping bloom bright-pass and three-stage blur implementation. Released HPL2 bloom source independently supports the scratch-resource classification. Ghidra: `HPL3_PostEffect_ToneMapping_RenderBloomBrightPass` / `RenderBloomBlurPass`. |
| `0x1403f2b50` | Confirmed, per-eye GPU history and phase ownership built | Complete deferred SSAO pipeline. Samples previous temporal AO texture `renderer+0xe78` with program `+0xf40` and temporal view/projection uniforms, then overwrites it through framebuffer `+0xed8` every invocation. `0.54.0` restores/commits an eye-local GPU copy; `0.55.0` also frame-owns its shared temporal sample phase. The `0.56.0` shader/resource audit confirms projection is current-eye frustum state and previous view is the already banked `+0x80` matrix; no separate projection or velocity history exists. Ghidra: `HPL3_RendererDeferred_RenderSSAO`. |
| `0x1402aba30` | Confirmed, resource correlation hook built | Renderer texture-unit wrapper forwarding `(unit, iTexture*)` to low-level graphics virtual `+0x340`. `0.54.0` uses this exact boundary only during SSAO to correlate native `renderer+0xe78` with the actual OpenGL texture name. Ghidra: `HPL3_Renderer_SetTextureUnit`. |
| `0x1403f4530` / `0x1403f2880` | Confirmed | Create/destroy deferred SSAO textures `+0xe60/+0xe70/+0xe78/+0xe80`, framebuffers `+0xec0..+0xed8`, and nearby local-reflection resources. This proves `+0xe78/+0xed8` are persistent history resources rather than pass scratch. Ghidra: `HPL3_RendererDeferred_CreateSSAOAndReflectionResources` / `DestroySSAOAndReflectionResources`. |
| `0x1403f40d0` | Confirmed, sequential scratch | Local-reflection render path writes current reflection to `+0xef0/+0xea0`, then fully overwrites copy texture `+0xeb0` through framebuffer `+0xf00` from the current accumulation input before final composition. The copy prevents same-pass feedback and is not temporal history, so it remains native. Ghidra: `HPL3_RendererDeferred_RenderLocalReflection`. |
| `0x14079575c` | Confirmed, same-pose frame owner built | Global float temporal SSAO sample phase. `0x1403f2b50` advances it by renderer frame time and uploads the derived jitter value on every eye render. `0.55.0` replays the baseline for eye two and preserves one committed advancement. Ghidra: `g_flSSAOTemporalSamplePhase`. |
| `0x14038a8b0` | Confirmed, lifecycle hook built | Releases ImageTrail accumulation texture `effect+0x58` and framebuffer `+0x50`, then zeros both. `0.52.0` signature-hooks this virtual lifecycle boundary to release both per-eye pairs; pre-graphics shutdown releases the secondary and restores the primary for native destruction. Ghidra: `HPL3_PostEffect_ImageTrail_DestroyResources`. |
| `0x14038a8f0` | Confirmed | ImageTrail reset virtual; sets one-shot clear-history flag `effect+0xa0`. Ghidra: `HPL3_PostEffect_ImageTrail_Reset`. |
| `0x14038a930` | Confirmed | ImageTrail active-state callback; dispatches reset when becoming inactive. Ghidra: `HPL3_PostEffect_ImageTrail_OnSetActive`. |
| `0x14038a950` | Confirmed, per-eye control built | ImageTrail render virtual. Binds framebuffer `effect+0x50`, samples/returns accumulation texture `+0x58`, consumes amount `+0x98`, and clears once when `+0xa0` is set. `0.52.0` banks the two resource pointers plus clear flag by eye at the exact `RenderOne` boundary. Ghidra: `HPL3_PostEffect_ImageTrail_RenderEffect`. |
| `0x14038ae60` | Confirmed, per-eye allocation built | Creates the ImageTrail history texture at `effect+0x58` and framebuffer at `+0x50` (`ImageTrailTexture`, `ImageTrailBuffer`). `0.52.0` invokes this exact signature-guarded function lazily on the render thread to create the second eye pair. Ghidra: `HPL3_PostEffect_ImageTrail_CreateResources`. |
| `0x1403896e0` | Confirmed | Initializes `posteffect_chromatic_aberration_frag.hpsl` and its uniforms. |
| `0x14038a5d0` | Confirmed | Initializes `posteffect_radial_blur_frag.hpsl` and its uniforms. |
| `0x1403870a0` | Confirmed | Initializes `posteffect_image_fade_fx_frag.hpsl` and its uniforms. |
| `0x140271870` | Confirmed by HPL2 layout | `cCamera::GetViewMatrix` accessor. Rebuilds through `0x140271250` when dirty flag `camera+0x709` is set, then returns `camera+0x74`. |
| `0x140289340` | Confirmed, probe built | Live FMOD listener update. Reads position, velocity, forward, and up from the sound-system object, validates them, calls `set3DListenerAttributes`, then calls `EventSystem::update`. `0.5.1` observes its fields against the OpenXR head pose without mutation. |
| `0x14061d188` | Confirmed import | `FMOD::EventSystem::set3DListenerAttributes` import/call target. Candidate late audio-listener correction point. |
| `0x14048b2d4` | Confirmed by registration string | Registers `SetCurrentListener(cViewport@)` for AngelScript. |
| `0x14048c89f` | Confirmed by registration string | Registers `CreateVideo(const tString&)`, returning `iVideoStream`. |
| `0x14048c8e4` | Confirmed by registration string | Registers `DestroyVideo(iVideoStream@)`. |
| `0x140488fa0` | Confirmed, probe hook built | Exact `CreateVideo` script wrapper. `0.29.0` records the native string, returned stream pointer, and bounded active-stream count without changing playback. Ghidra: `HPL3_Script_CreateVideo`. |
| `0x140488fd0` | Confirmed, probe hook built | Exact `DestroyVideo` script wrapper. `0.29.0` records stream lifetime and calls the original wrapper unchanged. Ghidra: `HPL3_Script_DestroyVideo`. |
| `0x1400ccdb0` | Confirmed, read-only control built | Exact registered `IsLoadingScreenVisible()` wrapper. Resolves loading owners through game context slot `0x1407925e0`; `0.29.0` uses its result for XR blackout, AFR invalidation, and input release. Ghidra: `SOMA_IsLoadingScreenVisible`. |
| `0x14024a2f0` | Confirmed, control hook built | Exact `cWorld::CreateBillboard` native wrapper registered for AngelScript. `0.30.0` reads the native name and tracks only `Screen Particle<decimal>` identities. Ghidra: `HPL3_World_CreateBillboard`. |
| `0x140252700` | Confirmed, lifecycle hook built | Exact `cWorld::DestroyBillboard` wrapper. Removes tracked screen-material identity before native destruction. Ghidra: `HPL3_World_DestroyBillboard`. |
| `0x1402936c0` | Confirmed, control hook built | Shared `iEntity3D::SetPosition` leaf used by the registered billboard method. `0.30.0` scales only atomically matched screen-material pointers relative to the active camera origin. Ghidra: `HPL3_Entity3D_SetPosition`. |
| `0x140291700` | Confirmed, control hook built | Exact billboard `SetSize` leaf, writing `cBillboard +0x68/+0x6c`. Screen-material size follows the same distance ratio and returns to native outside F10 VR. Ghidra: `HPL3_Billboard_SetSize`. |
| `0x1400edb2a`-`0x1400edd19` | Confirmed by registration strings | Registers load-screen force-background, small-icon, show-icon, and bar position/size controls. |

## HPL3 Camera Layout

Offsets confirmed from decompilation and the matching HPL2 source:

| Object | Offset | Meaning |
| --- | --- | --- |
| `cCamera` | `+0x10` | Position (`cVector3f`). |
| `cCamera` | `+0x44/+0x48/+0x4c` | Base pitch, yaw, and roll. `0.61.0` can temporarily zero pitch only while the active VR frustum is evaluated so visual pitch remains HMD-owned; native state is restored immediately. Roll retains its separate opt-in policy. |
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
| `iCharacterBody` | `+0x6c` | Current center position; `GetFeetPosition` subtracts half the body height. |
| `iCharacterBody` | `+0x134` | Character body size vector `(x,y,z)`; `+0x138` is the height used by feet/center conversion. |
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
| ImageTrail | `+0x50` | Accumulation framebuffer pointer. |
| ImageTrail | `+0x58` | Accumulation texture pointer and render return value. |
| ImageTrail | `+0x98` | Authored trail amount used in exponential decay. |
| ImageTrail | `+0xa0` | One-shot clear-history flag set by Reset and consumed by RenderEffect. |
| ToneMapping | `+0x8c/+0x94` | Current exposure and white-cut values consumed by the final shader. |
| ToneMapping | `+0xa0/+0xd8/+0xe0` | Current, target, and queued color-grading texture pointers. |
| ToneMapping | `+0xe8..+0x120` | Grading speed/weight/active state plus exposure, white-cut, window, and authored transition source/destination/time packet. |
| ToneMapping | `+0x138..+0x158` | Film-grain current/next UV offsets and quantized time phase; shared once per same-pose stereo pair. |
| ToneMapping | `+0x160..+0x1b8` | Six bloom framebuffer/texture scratch pairs; regenerated per invocation, not temporal history. |

Confirmed post-effect vtable RVAs used for runtime identity:

| Effect | Vtable RVA | GetTypeName |
| --- | --- | --- |
| ToneMapping | `0x69b038` | `0x1402859b0` |
| FXAA | `0x6ac3b8` | `0x140386170` |
| ImageFadeFX | `0x6ac4e8` | `0x140386b20` |
| VideoDistortion | `0x6ac688` | `0x1403878a0` (`0.28.0` named VR policy) |
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
