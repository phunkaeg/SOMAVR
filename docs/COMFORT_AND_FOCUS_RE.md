# Native Comfort And Interaction Focus RE

Date: 2026-07-15
Build: `0.19.0-comfort-focus`

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

This snapshot is the engine-truth input for a future world-depth reticle. The
build deliberately does not draw that reticle yet: icon/state ownership and the
least invasive world-render boundary still need confirmation. Invalid/no-hit
queries clear validity so a future consumer cannot present stale focus.

## Acceptance Evidence

- `hpl_comfort_bridge install_ok` proves the guarded wrapper matched.
- `hpl_comfort_camera_add` rows identify each channel and original vector.
- `hpl_comfort_bridge_summary` separates Bob, Shake, Sway, and inactive-tracking
  calls.
- `hpl_interaction_ray ... hitSnapshot=1` reports distance, world point, entity,
  and body from the same native result used by SOMA.
- A useful live run should exercise walking, sprinting, impacts, scripted camera
  motion, interaction targets at several distances, tracking loss, and F10 off/on.

## Remaining Inputs

1. Live acceptance that Bob/Shake/Sway suppression removes discomfort without
   harming ladders, crawl, death, terminals, scripted cameras, or conversations.
2. Crosshair semantic-state ownership so a depth reticle can preserve SOMA's icon
   and availability policy instead of displaying a generic unconditional dot.
3. A proven per-frame world overlay insertion point or an OpenXR world-space quad
   policy with depth/occlusion behavior.
