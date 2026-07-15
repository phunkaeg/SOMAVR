# Native Comfort And Interaction Focus RE

Date: 2026-07-15
Build: findings through `0.52.0-per-eye-image-trail`

## Dynamic Peripheral Comfort

`0.51.0` adds a compositor-only comfort path on top of the confirmed controller
and authored-state ownership below. `HPLInputBridge` computes a normalized
motion target only after loading, stale input, F1-panel exclusivity, pause,
terminal, dead-state, and authored-camera gates. Movement magnitude uses the
configured radial deadzone. Turn magnitude contributes only when
`ComfortVignetteSmoothTurn=1` and snap turning is disabled; snap turns retain
their existing bounded black-frame request.

`OpenXRRuntime` advances the target through a display-period-based linear
attack/release envelope and submits a black, transparent-center radial texture
on a VIEW-space alpha quad. The quad is intentionally independent of world,
HUD, reticle, and projection transforms. `OpenXRGLBridge` owns its dedicated
swapchain and destroys it with every other session resource. Motion samples
expire after the same bounded age as controller input, preventing a stuck mask
if the bridge stops publishing.

The implementation borrows the proven 0.30 m distance and roughly 1.0 m square
coverage contract from SS2VR, but uses SOMAVR's OpenGL upload path and tested
pure raster/envelope math. Generated configs stay off; balanced and maximum
presets enable different strengths, explicit INI values override them, and F1
provides a live runtime toggle. No new SOMA RVA or Ghidra mutation is involved.

## Native Crosshair Semantic Boundary

Shipped `PlayerState_Normal.hps` runs `mPickBasics.UpdatePickCheck`, applies
`GetInteractionDisabled`, `CanInteract`, range, and icon policy, then calls
`Player_SetCrossHairState`. That helper writes global argument zero and dispatches
`LuxPlayer::_Global_SetCrosshairState`; `Player.hps` copies the integer into
`meCrossHairState` and selects one of 34 named `graphics/hud/crosshair_*.tga`
assets. This is the narrowest confirmed native semantic owner after the raw pick.

`HPLCrosshairBridge` signature-guards registered dispatch thunk `0x140484ea0`
and integer argument reader `0x1404851d0`. It safely decodes the three native
MSVC strings, observes only the exact callback above, calls the original first,
and publishes states `0..34` only when the callback succeeds. The bridge does not
replace script state, `CanInteract`, range, focus, callbacks, or GUI drawing.

The enum groups used only for VR feedback are:

| States | Intent | VR feedback |
| --- | --- | --- |
| `2,3,14` | Carry/pickup | Green native icon; light pulse |
| `4..13,32` | Push/pull/rotate/button | Amber native icon; firmer pulse |
| `15..18,22,28,29,33` | Tool/info/action | Blue native icon; neutral pulse |
| `19,23..25` | Traversal/transition | White native icon; longer pulse |
| `20,21` | Social/consume | Purple native icon; soft pulse |
| `30,31` | Unavailable/busy | Red native icon; reduced pulse |
| `1` | Ambiguous default cursor | Native/default reticle; no focus pulse |
| `34` | Simple/no-hints cursor | Native icon; short soft pulse |

The OpenXR renderer loads the exact enum-to-file table from shipped `Player.hps`,
decodes only bounded uncompressed 24/32-bit TGA data, aspect-fits it into the
reticle swapchain, and preserves the procedural cross as a fail-closed fallback.
`InteractionReticleSemantic` and `InteractionReticleNativeIcons` are separate
rollback controls.

## Semantic Camera Add Boundary

SOMA's shipped `script/player/Player_Types.hps` defines the camera-add slots:

| ID | Channel | `0.19.0` policy while VR is active |
| --- | --- | --- |
| `0` | Crouch | Preserve |
| `1` | Bob | Configurable semantic zero; enabled in the test profile |
| `2` | Shake | Configurable semantic zero; enabled in the test profile |
| `3` | Climb | Preserve |
| `4` | Terminal | Preserve |
| `5` | Script | Preserve |
| `6` | Dead | Preserve |
| `7` | Lean | Preserve |
| `8` | Crawl | Preserve |
| `9` | Sway | Configurable semantic zero; enabled in the test profile |
| `10` | Conversation | Preserve |

The registered signature string at `0x14068aa50` identifies
`void SetCameraPosAdd(int alType, const cVector3f&in avVector)`. Its registration
loads wrapper `0x140159360`. The wrapper preserves RCX/EDX for its call to
`0x1401587e0`, carries the vector in R8, and writes both current and goal values.
The first 18 bytes are unique and signature-guarded by `HPLComfortBridge`.

The hook calls the original wrapper with a zero vector for selected channels; it
does not skip the setter. That clears stale native goals while preserving the
engine's channel lifecycle. Suppression begins only after F10 tracking is active
and stops immediately when tracking is disabled. Generated configurations leave
the feature off.

`SetCameraRoll(int,float)` is separately confirmed at `0x140156f00`. It expands
four roll arrays and writes current/goal arrays rooted at player `+0x3a0/+0x3c0`.
It remains documented but unhooked because camera roll already has a lower-level
compatibility path and broad roll suppression previously needed careful visual
validation.

## Closest-Entity Result Layout

`SOMA_GetClosestEntity` at `0x1400cd750` forwards the output object fields directly
to the native picker:

| Output offset | Field | Use |
| --- | --- | --- |
| `+0x18` | `iLuxEntity* mpEntity` | Semantic target identity |
| `+0x20` | `iPhysicsBody* mpBody` | Native physics target identity |
| `+0x28` | `float mfDistance` | Distance along the supplied ray |

These offsets agree with the registered property strings at
`0x140682138/0x140682150/0x140682168` and the shipped
`Utility_PickBasics.hps` reads. After the native wrapper finalizes the output,
`HPLInteractionBridge` validates finite distance against native ray length,
normalizes the controller direction, and publishes an immutable snapshot with
frame, hand, distance, world hit point, entity, and body.

This snapshot is the engine-truth input for the `0.20.0` world-depth reticle.
The build copies the exact app-space controller aim pose used by the native query
and converts `mfDistance` through the configured HPL world scale. It does not
reconstruct depth from OpenGL or issue a second ray test. Invalid/no-hit queries
clear both snapshot and reticle validity so stale focus cannot be presented.

## OpenXR Depth Reticle Policy

`OpenXRRuntime` submits a small source-alpha `XrCompositionLayerQuad` in the
application reference space. Its center lies on controller aim at native hit
distance, its orientation follows the aim pose, and its physical width is
derived from a configurable angular size with explicit minimum/maximum clamps.
Submission requires a current native hit, fully tracked aim pose, an existing
stereo projection layer, and a healthy dedicated swapchain. Comfort blackouts
remove it with the other layers. Four consecutive draw failures suspend only the
reticle path.

This is deliberately a generic closest-entity marker. SOMA still owns query
length, LOS, range, `CanInteract`, focus callbacks, and actual interaction. The
remaining semantic work is to map its crosshair icon/availability owner and
either vary or suppress the marker accordingly. Native entity/body transitions
also drive an optional low-amplitude, cooldown-limited dominant-hand haptic.

## Acceptance Evidence

- `hpl_comfort_bridge install_ok` proves the guarded wrapper matched.
- `hpl_comfort_camera_add` rows identify each channel and original vector.
- `hpl_comfort_bridge_summary` separates Bob, Shake, Sway, and inactive-tracking
  calls.
- `hpl_interaction_ray ... hitSnapshot=1` reports distance, world point, entity,
  and body from the same native result used by SOMA.
- `openxr_frame ... reticle=1` and `reticleSubmitted` prove compositor delivery;
  summary counters expose updates, clears, expiry, submission, and failures.
- `hpl_interaction_bridge_summary` separates reticle updates and focus-haptic
  requests/applied pulses.
- A useful live run should exercise walking, sprinting, impacts, scripted camera
  motion, interaction targets at several distances, tracking loss, and F10 off/on.

## Remaining Inputs

1. Live acceptance that Bob/Shake/Sway suppression removes discomfort without
   harming ladders, crawl, death, terminals, scripted cameras, or conversations.
2. Crosshair semantic-state ownership so the depth reticle can preserve SOMA's icon
   and availability policy instead of displaying a generic unconditional dot.
3. Decide whether compositor-only binocular depth is sufficient or whether
   world geometry occlusion requires a later engine/depth-tested overlay path.
