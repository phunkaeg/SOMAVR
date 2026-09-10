# Campaign-Owned Hands Bootstrap

Date: 2026-09-10. Feature: `FEATURE.VISIBLE_HANDS`.
Build: `0.96.2-hands-bootstrap`. Status: **BUILT; static contract verified;
gameplay and headset acceptance pending**. No SOMA launch in this investigation.

## Question And Decision

Can SOMAVR request the campaign's own arm entity before the vial sequence,
without inventing an entity type, selecting a fixed asset, or calling script
from the renderer? Yes at the static-contract level. Use the native hands
module's completed PostUpdate boundary, then invoke its script methods
`SetVisible(true)` followed by `SetActive(true)`. Wait for the existing exact
`PlayerHands_*` SetMatrix bridge to confirm the native seed; dispatch is not
acceptance. Runtime execution of this new route remains untested.

The installed `script/modules/PlayerHandsHandler.hps` owns `msHandModel`, whose
default is `character/player/hands/hands_human.ent`. `SetVisible` calls
`CreateHandModelIfNeeded(currentMap)` before changing mesh visibility. The
creation helper leaves the entity inactive. `SetActive(true)` enables the
handler's native root-update path. `CreateWorldEntities` is empty: calling that
callback alone would not create hands. No game scripts or assets were changed.

## Receiver Correction

The 0.92 discovery assumptions were wrong, not a runtime proof of absent hands.
Constructor `0x1401b00b0` and base constructor `0x1401a9d40` establish:

| Receiver/field | Target evidence |
| --- | --- |
| Full `cLuxUserModule` primary vtable | `0x14068fcc8` |
| Secondary `iUpdateable` | Full object `+0x110`, vtable `0x14068fbb8` |
| Module ID | Full object `+0x158`, hands ID `18` |
| Script wrapper | The full object itself, NOT a pointer at `+0x90` |
| Context manager / group / last context | Full object `+0x10 / +0x20 / +0x28` |
| Low-level script object | Full object `+0xe8`, vtable `0x1406f2f48` |
| PostUpdate | Secondary vtable `+0x30` -> `0x1401ab3a0`; subtracts `0x110` before method calls |

`0x1401378e0` is **cLuxMapHandler's** OnAction script forwarder, not the module
implementation. Its sole vtable xref is `0x1406878c8`; table `0x1406877e8` has
COL `0x140718740`, type descriptor `0x140786d70` (`cLuxMapHandler`). A matching
script declaration in registration code did not establish native ownership.
The optional hands discovery probe now uses both exact module vtables and the
receiver adjustment. The old inventory observer at `0x1378e0` was not changed
behaviorally in this build; its claimed module-ID ownership needs a separate
review and must not be treated as proven by the old registry entry.

## Native Call Contract

Target: x64 `Soma_NoSteam.exe`, preferred base `0x140000000`, SHA-256
`395CD54830C8E66E22166E71AC6FE95FD3BB5433B898737836744A6D178A0A6A`.
All six production entry signatures match exactly once in executable sections.
The first 31-byte PostUpdate candidate matched FIVE functions; the final
57-byte signature includes its specific callback-bit/string reference and is
unique. Its leading `40` REX prefix is retained.

| Entry | ABI / concrete consumer |
| --- | --- |
| `0x1ab3a0` | `void(secondaryThis, float)`; original completes before bootstrap |
| `0x1dc170` | `bool(fullThis, NativeString32*, int)`; low object vtable `+0x38` -> `0x54b3a0` |
| `0x1dd830` | `bool(fullThis, NativeString32*, int)`; context vtable `+0x10` -> `0x484c50` |
| `0x1dc190` | `void(fullThis, int argIndex, bool)`; context `+0x18` -> `0x484510` |
| `0x1dd340` | `bool(fullThis)`; context `+0x70` -> `0x484860`; releases reserved byte `+0x30` |
| `0xcccb0` | Read-only current-map chain: `[[gameContext+0x148]+0x90]`; no new native getter call |

The context vtable is `0x1406c0e38`. Before invoking, the last context must
belong to this low script object (`context+0x80`), have a backend (`+0x38`),
and have neither reservation (`+0x30`) nor execution (`+0x48`) set. This is
checked after the native PostUpdate returns, on the calling game thread.

Declarations use explicit read-only 32-byte native string arguments: pointer
at 0, size at 16, capacity at 24. Both declarations exceed the 15-byte inline
limit. `0x481140` copies the name into the native cache. SOMAVR never gives the
native CRT ownership of its literal and never passes its own `std::string`.
Cache index `-1` uses names, not a hardcoded built-in callback index.

The engine Execute wrapper's boolean is not sufficient proof that an authored
operation completed; script exceptions may still return a nonnegative engine
status. Logs deliberately say `dispatched` and separately await native seed.
A native fault disables this lane for the process. No retries after partial
calls, no raw mesh `SetVisible`, no visibility-vtable guessing.

## Lifetime And Gates

`HandAlwaysVisible=1` retains existing hands. New independent `HandBootstrap=1`
requests initial creation. Both are enabled in the rolling test profile;
setting only `HandBootstrap=0` restores the vial-first creation behavior.

The policy waits for 30 distinct game frames AND 750 ms of continuously
eligible Normal/Normal gameplay, measured after loading and camera control
settle. It is not a timer starting at injection. Menus, loading, wake sequences,
authored cameras, stale poses, a mismatched active player/camera, or an
unavailable native context reset settling. An existing retained rig is left
alone. One attempt is allowed per map/script/loading-generation identity;
F10 toggles and pauses do not re-arm it. Loading generation handles reused
heap addresses. A missing native seed after five seconds is logged once.
The game can still create hands normally later, including through the vial.

## Diagnostics And Tests

`hpl_hands_bootstrap` logs signature counts, hook callbacks, exact-owner hits,
readiness hits, invocation, dispatch, native seed confirmation, and timeout.
`gate_mask=0` is eligible; blocked bits are:

| Bit | Meaning |
| --- | --- |
| `0x001` | No current map |
| `0x002` | Loading/wake presentation |
| `0x004` | Invalid player/camera-control snapshot |
| `0x008` | Not Normal player and movement states |
| `0x010` | Authored/detached camera |
| `0x020` | Tracking disabled or camera mismatch |
| `0x040` | Missing/stale head pose/basis |
| `0x080` | Paused or pause state unavailable |
| `0x100` | Current native player/body/state no longer matches |
| `0x200` | No matching completed native script context |

Offline reproduction: `scripts/Test-HandsBootstrapContract.ps1`. It reads the
PE, never loads or executes it. Raw decompiler/table receipts are in
`docs/evidence/hands-bootstrap-2026-09-10.json`; production-signature and five
concrete-callee checks are in `hands-bootstrap-contract-2026-09-10.json` in the
same directory. `somavr_hands_bootstrap` tests long loading, frame duplicates,
existing hands, pause interruption, pointer-reused reload, and bounded failure.

Next headset acceptance: load a save BEFORE the vial; press F10 and wait a
couple of seconds in controllable gameplay. Check full-size tracked arms,
then vial/cap/drinking, post-vial tracking, physics/terminal interactions,
pause/resume, reload, F10 off/on, and exit. Do not save over the only baseline
save until this new lifecycle path is accepted. Campaign model changes and
full-game compatibility remain untested, not release-certified.
