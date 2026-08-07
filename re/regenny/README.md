# SOMAVR ReGenny Workspace

This directory is the version-controlled live-memory schema for the supported
64-bit `Soma_NoSteam.exe`. Open `SOMAVR.genny`; it imports the domain files in
the required order.

## Safety Contract

- The workspace is read-only by default. Do not use ReGenny writes until a
  mutation has a separately documented rollback and signature/state gate.
- `*Partial` means only named fields are known. Generated padding is unknown,
  and the declared end size is only the smallest useful inspection extent.
- Add fields only after they are recorded in `docs/ADDRESS_REGISTRY.md` or have
  equivalent Ghidra and runtime evidence.
- Ghidra remains authoritative for functions and control flow. These schemas
  are authoritative for consolidated live object layouts.
- Generated headers are not currently part of the shipped DLL build.

## Initial Schemas

| File | Live objects |
| --- | --- |
| `render.genny` | camera, frustum, viewport, listener, renderer, post effect |
| `player.genny` | player, state objects, character body, world, Lux entity |
| `hands.genny` | transform nodes, wrist hierarchy, authored post transforms |
| `gui.genny` | game context, ImGui ownership, wrappers, terminal GUI set |
| `physics.genny` | closest-entity result, body, joint, contact packet |

## First Attach

1. Start SOMA and reach a stable loaded save.
2. Attach ReGenny to `Soma_NoSteam.exe`.
3. Open `SOMAVR.genny` and confirm all imported types parse.
4. Select `soma.GameContextPartial` at
   `<Soma_NoSteam.exe>+0x7925e0->0x0`.
5. Follow typed pointers to GUI ownership. Resolve player, camera, hand, and
   physics instances from the existing guarded probes before selecting them.

The static game-context slot is supported-build-specific. Re-run the injector
doctor and signature checks before trusting it after any executable update.

## Validation

On 2026-07-21 the installed ReGenny MCP opened `SOMAVR.genny`, resolved all six
imports, and loaded 25 structures/classes. Representative `get_type` checks
returned the expected offsets for `CameraPartial`, `PlayerPartial`,
`Node3DPartial`, `GameContextPartial`, and `PhysicsJointPartial`. ReGenny was
left detached with no live process writes.

The first live attach later that day resolved the active player through
`Soma_NoSteam.exe+0x0cc860` and validated `Node3DPartial` against both wrist
chains. Node names use `NativeStringPartial` at `+0x10`, local/world matrices
remain at `+0x44/+0x84`, post-use remains clear at `+0xc5`, the restored post
matrix at `+0x108` is identity, and the parent pointer is at `+0x180`.

Heap addresses in logs are observations, not reusable pointers. A load or map
stream can destroy and recreate the player-hands entity, mesh, wrists, sockets,
and attached HudObject while the process and player object remain alive. Always
resolve the current player from the guarded getter and take current hand-node
addresses from the latest identity/skeleton rows before selecting a type.
