# BioShock VR Transfer Audit

Created: 2026-07-30

Source baseline: `D:\Dev Debug\Bioshock-vr` at commit `d8ae62f`
(`Merge pull request #15 from hedgehog90/pace-hang-fix`). The source project is
an MIT-licensed, native C++ OpenXR mod for the 32-bit D3D11 BioShock games.
SOMAVR is a 64-bit OpenGL/HPL3 mod, so this audit transfers proven ownership
patterns and failure handling, not engine addresses or rendering code.

## Executive Verdict

| Finding | SOMAVR position | Confidence | Scheduled action |
| --- | --- | --- | --- |
| Never enter untimed `xrWaitFrame` after a previously focused session becomes unfocused | Direct gap | High, field-proven in BioShock VR | Implement first, in an isolated build |
| Generate a trustworthy Git/build identity and preserve the previous session log | Partial | High | Add before broader crash diagnosis |
| Capture bounded, information-rich in-process crash dumps | Missing; only the external dumper exists | High | Add as a dedicated reliability build |
| Swap exact per-entity calibration profiles into existing hot-path values | Design opportunity | High | Build a behavior-neutral profile substrate, then promote named profiles |
| Revalidate skeletal owners and never accumulate solved output as the next reference pose | Substantially aligned | High | Retain as explicit rig invariants and tests |
| Defer dangerous control-plane mutations until outside native detours | Audit required | Medium | Inspect command paths after the reliability work |
| Add an always-on render watchdog or copy BioShock's D3D11 frame inspector | Not justified | Low | Do not port without a specific SOMA failure |

## 1. Urgent: OpenXR Focus Pacing

### BioShock evidence

`src/core/vr/openxr_runtime.cpp` contains the fix introduced by `22ed1b0`. Its
important observation is that `xrWaitFrame` has no timeout. A low-cadence
keepalive that continued calling it while the session was visible but not
focused wedged the game's Present thread. The desktop game appeared alive until
render backpressure stopped it, the headset froze every frame, and there was no
crash dump to explain the apparent hang.

The repaired policy is stateful:

1. Before the session has ever reached `XR_SESSION_STATE_FOCUSED`, continue
   normal pacing so the runtime can complete startup.
2. Latch that the session has been focused at least once.
3. After that latch, skip `xrWaitFrame` whenever the current state is not
   focused. Continue polling events every Present so recovery is event-driven.
4. Reset the latch when the session stops or a new session is created.
5. Record pacing skips and wait duration, and close any leaked open frame before
   beginning another.

### SOMAVR gap

`OpenXRRuntime::OnFrameBoundary` polls events, then calls
`SubmitFrameLocked` whenever the session is running and frame resources are
ready. `PollEventsLocked` keeps `sessionRunning_` true in
`XR_SESSION_STATE_VISIBLE`; `SubmitFrameLocked` then calls `xrWaitFrame`
without a focus-state guard. This is the same vulnerable ownership pattern.

SOMAVR does not normally carry an open XR frame across Present calls, so the
leaked-frame risk is lower. An explicit frame-open invariant is still worth
adding while this path is being touched.

### Implementation contract

Create `FEATURE.XR_FOCUS_PACING` in `OpenXRRuntime`:

- Add an `everFocused_` session latch and reset it on session start, STOPPING,
  EXITING, loss, and teardown.
- Keep polling OpenXR events regardless of focus.
- Capture a pending stereo eye before the pacing decision so an unfocused skip
  does not discard an already rendered eye.
- Once focus has previously been acquired, skip `xrWaitFrame`, `xrBeginFrame`,
  and `xrEndFrame` while not focused.
- Rate-limit one log line per unfocused episode, plus recovery and summary lines.
- Record the longest `xrWaitFrame` duration and skipped-frame count.
- Assert or recover if a new frame would begin while an old frame is still open.
- Keep this change isolated from hands, UI, interaction, and rendering policy.

### Acceptance gate

1. Enter VR and confirm the session reaches FOCUSED.
2. Remove the headset or otherwise make the runtime VISIBLE for at least 60
   seconds. The desktop game must continue, with no unbounded wait.
3. Wear the headset again. Stereo and input must resume without pressing F10.
4. Repeat from the pause menu, a terminal, and after loading a save.
5. Exit normally and inspect the pacing summary.

## 2. Build Identity And Crash Evidence

### Transferable BioShock design

`cmake/GenerateVersion.cmake` derives the runtime identity from the project
version plus `git describe --tags --always --dirty`. Startup also records the
DLL PE timestamp and a small environment fingerprint. This prevents a hand-edited
version string from making a tester's DLL or dump impossible to identify.

`src/core/util/crash.cpp` provides bounded in-process evidence:

- a top-level exception filter that can be re-armed if another module replaces
  it, while preserving and chaining the displaced handler;
- a first vectored handler that observes only always-fatal exceptions, avoiding
  ordinary access violations that may be deliberately guarded by probes;
- richer minidump flags, with an environment opt-in full dump;
- module plus RVA, access type, registers, and candidate return addresses;
- reentrancy protection, repeated-address suppression, and a three-dump cap.

The cap matters. BioShock VR's comments record a real unbounded failure that
created 2,083 dumps totalling 115 GB.

### SOMAVR plan

SOMAVR already writes a package version, flavor, and SHA-256 manifest and has a
manual external dumper for hangs. It should retain those and add:

- generated Git describe/dirty identity on every build;
- DLL PE timestamp and basic OS/CPU/RAM details in startup logs;
- rotation of the old `somavr.log` to `somavr.previous.log` before truncation;
- an x64-aware in-process crash handler with rich bounded minidumps;
- `SOMAVR_FULLDUMP=1` as the explicit full-memory opt-in;
- the same repeated-fault suppression and three-attempt session cap.

This should be a dedicated reliability build. A diagnostic-only deliberate
fault may validate it; production gameplay must never intentionally crash.

## 3. Exact Entity Calibration Profiles

### BioShock pattern

BioShock VR's `src/game/bioshock1r/aim.cpp` does not perform a map lookup in
every ray, laser, or viewmodel operation. It treats profiles as a swap layer
over the existing live atomic values:

1. Resolve the exact native equipped-object identity from its current owner.
2. On identity change, save the outgoing live values.
3. Load the known profile, or seed a new one from a valid generic baseline.
4. Leave the hot paths reading the same live values they already use.
5. Before saving, stash the active profile. After a bulk preset reload, reapply
   it so the generic baseline cannot overwrite an exact calibration.

The steady-state hot-path cost is effectively one identity comparison, and a
new profile cannot be accidentally seeded from zeroes before configuration is
ready.

### SOMAVR opportunity

This pattern fits several recurring SOMAVR problems better than more global
tuning values:

- different `PlayerHands_*` variants need stable wrist orientation/offsets;
- the flashlight and future tools need their own grip and aim bases;
- story `*_HudObject` meshes vary in authored scale, orientation, and read
  distance;
- the medicine bottle and later bespoke interactions need named behavior and
  calibration without turning the generic interaction path into a switch
  statement.

Create `FEATURE.ENTITY_CALIBRATION_PROFILES` as a reusable substrate. The key
must come from the exact live HPL entity owner/name/capability, never the last
interaction event or a stale cached pointer. Candidate fields are:

- dominant/support-hand role and two-hand mode;
- local hand position, orientation, and scale;
- story/read distance, orientation, and authored scale correction;
- controller aim/grip basis and native-animation blend policy;
- optional interaction thresholds where entity evidence proves they differ.

The first behavior-neutral build should only resolve keys, swap live snapshots,
persist profiles, and log transitions. Initial real profiles can then cover:

- generic `PlayerHands_*`;
- exact `Flashlight`;
- generic and exact `*_HudObject` story items;
- `Tracer_Fluid_HudObject` medicine handling;
- later socketed tool and authored-interaction families.

Tests must cover A/B/A identity switches, first-seen seeding, save/reload,
baseline reload followed by active-profile reapplication, owner destruction,
and absence of any map access in render/input hot paths.

## 4. Hand And Skeleton Invariants

BioShock VR's hand code provides useful corroboration, but no direct bone or
address transfer:

- revalidate cached actors before every write;
- exponentially back off discovery scans while the rig is absent;
- clear all dependent skeleton and attachment owners before native world
  teardown instead of trying to restore through stale pointers;
- solve from an engine-evaluated reference pose, never the previous solved
  output, or errors accumulate every frame;
- use local quaternion composition rather than adding Euler values after
  conversion;
- reapply only fresh cached bone output when a second stereo pass causes native
  skeleton re-evaluation.

SOMAVR already follows most of this through exact `PlayerHands_*` identity,
destroy-time eviction, restored native pose state, transient wrist/arm writes,
and explicit authored-state gates. These rules should become acceptance tests
for the entity-profile work rather than a new standalone feature build.

## 5. Smaller Patterns Worth Retaining

### Safe control plane

BioShock VR defers commands that can toggle hooks, scan memory, recreate XR
resources, or restore native state while execution is inside a hooked call.
SOMAVR should audit F1/hotkey/config command paths and queue only dangerous
mutations to a known frame boundary outside all detours. Pure atomic tuning can
remain immediate. This is an audit first, not a speculative rewrite.

### State-aware diagnostics

BioShock's render watchdog originally reported false hangs while loading, when
build/present counters were honestly idle. Any future SOMAVR watchdog must know
about loading, screen-only presentation, runtime focus, and detour depth. There
is no evidence that SOMAVR needs a generic watchdog today.

### Render ownership tracing

BioShock includes a D3D11 frame inspector and cheap constant-buffer fingerprint
watch. SOMAVR already has the OpenGL equivalents: bounded draw/UBO dumps, matrix
capture, call stacks, GL-state tracing, and terminal captures. The one useful
future extension is an exact UBO fingerprint watch that captures a writer stack
only when a named unresolved rendering owner justifies it.

### Native-first behavior

The source reinforces the current SOMAVR rule: identify the engine's native
owner and prove the failure before adding compensation. BioShock's core plus
per-game adapter architecture serves multiple executables; adding that
abstraction to a one-game HPL3 project would be unnecessary.

## 6. Already Aligned

No new implementation is needed for these BioShock patterns:

- one ordered F10 known-best VR preset with stereo enabled last;
- predicted-pose sampling for the upcoming render;
- pair-coherent stereo and per-eye temporal ownership;
- OpenXR layer counting, runtime maximum query, a 16-layer hard cap with array
  headroom, and priority-based truncation (implemented in 0.88 after the
  2026-08-07 review corrected this audit's earlier claim);
- controller interaction rays, HUD/menu layers, and desktop mirror;
- persistent-hand lifecycle ownership and body follow without mutating the HMD
  camera;
- bounded package install/update/uninstall and SHA-256 manifests;
- native FOV widening for culling and the current 120-degree SOMA baseline.

## 7. Deliberately Non-Portable

Do not transfer these items:

- the xinput proxy injection path;
- D3D11 immediate-context hooks and frame-inspector implementation;
- Unreal/Vengeance UObject, FName, ProcessEvent, bone, and address layouts;
- BioShock sequential-reentry and threaded-renderer repairs;
- BioShock foreground-FOV/viewmodel compensation;
- the square game-resolution recommendation, because SOMAVR sizes XR eye
  swapchains from runtime recommendations plus its resolution scale;
- any BioShock weapon class, bone index, attach matrix, or numerical tuning.

## 8. Implementation Schedule

Implementation status on 2026-07-30: stages 0, 1, and the behavior-neutral
portion of stage 2 are built in `0.87.0-reliability-profiles`. The focus-pacing
guard awaits headset acceptance. Generated build identity, previous-log
preservation, and bounded in-process crash capture pass automated tests,
including a real unhandled-exception child process that writes exactly one
dump. Exact entity profiles resolve, cache, and persist but do not yet affect
gameplay; stage 3 promotion remains pending.

| Stage | Feature | Scope | Promotion gate |
| --- | --- | --- | --- |
| 0 | `FEATURE.XR_FOCUS_PACING` | Ever-focused latch, unfocused pacing skip, wait timing, frame-open invariant | Headset idle/focus restore test passes without a flat-game stall |
| 1 | `FEATURE.BUILD_IDENTITY` | Generated Git identity, PE/environment fingerprint, previous-log preservation | Every log and DLL can be attributed to exact source state |
| 1 | `FEATURE.IN_PROCESS_CRASH_CAPTURE` | Bounded x64 rich dumps while retaining the external hang dumper | Diagnostic fault creates one readable dump; repeat/cap tests pass |
| 2 | `FEATURE.ENTITY_CALIBRATION_PROFILES` | Behavior-neutral exact-identity profile resolver, live-value swap, persistence, telemetry | Automated A/B/A and lifecycle tests plus live identity transitions pass |
| 3 | Entity profile promotion | Hands, flashlight, story items, medicine, then later tools | Each exact profile improves its target without changing generic entities |
| 4 | Safe control-plane audit | Classify immediate atomics versus deferred hook/XR/scanner mutations | No unsafe mutation can execute inside a native detour |
| 4 | Conditional UBO fingerprint watch | Add only for a named unresolved render owner | A concrete diagnosis needs it and the watch remains nearly free when idle |

Stages 0 and 1 are reliability work and should stay isolated from visible
gameplay changes. Stage 2 deliberately introduces the profile mechanism without
altering current tuning. Stage 3 is the point where live visual calibration
begins, one exact entity family at a time.

## Source Map

| Subject | BioShock VR source | Why it matters |
| --- | --- | --- |
| Focus pacing | `src/core/vr/openxr_runtime.cpp` | Field-proven unfocused `xrWaitFrame` hang and recovery policy |
| Crash capture | `src/core/util/crash.cpp` | Bounded dump ownership, diagnostics, filter coexistence |
| Build identity | `cmake/GenerateVersion.cmake` | Automatic Git/dirty attribution |
| Per-weapon profiles | `src/game/bioshock1r/aim.cpp` | Exact-owner profile swap without hot-path map lookups |
| Hand lifecycle | `src/game/bioshock1r/hands.cpp` | Cache validation, world teardown, absent-rig backoff |
| Bone application | `src/game/bioshock1r/bones.cpp` | Fresh reference pose and second-pass reapplication |
| Safe commands | `docs/ARCHITECTURE.md` | Deferral outside hooked-call depth |

## Licensing Note

The transfer proposed here is primarily architectural. If implementation later
copies or closely adapts MIT-licensed BioShock VR code, retain the applicable
copyright and license notice in SOMAVR's third-party attribution. Native
addresses, data layouts, and engine-specific constants are evidence for the
BioShock builds only and must never enter SOMAVR's address registry.
