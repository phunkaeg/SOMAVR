# SOMAVR Code Review — Handover

Date: 2026-08-07
Reviewer: external read-only review (no code changed, nothing run in-game)
Baseline reviewed: working tree at `ed2e7d8` **plus 57 modified + 16 untracked source files**
Reference standard: `D:\dev debug\VR Modding\docs\` (the VR playbook), chapters 06, 07, 08, 10
Cross-project corroboration: TheDarkModVR (`D:\Dev Debug\thedarkmodvr\`) — see F-16, F-17

---

## How to read this

Every finding has a **location**, the **evidence** it is real (not a hunch), and a **fix**. Where
the playbook already names the failure mode, the chapter is cited — those are not style opinions,
they are rules this project's own reference document says cost real time at least once.

Findings are ordered by consequence, not by effort. **P0 = ships broken or can hang/crash a user.**

Do not "fix" anything in [What is already right](#what-is-already-right) — several of those are
load-bearing and correct.

---

## Verdict

The engineering core is strong: signature-verified fail-closed hooks, a well-bounded x64 crash
handler, pure math split into a testable static library, careful GL state save/restore, and
genuinely excellent documentation. The problems are almost entirely at the **edges** — packaging,
configuration delivery, the OpenXR submit contract, and repo state — and two of them mean the
mod as packaged today does not do what its config says on any machine but this one.

18 findings:

- 1 makes every shipped config setting silently inert (**P0**)
- 1 fails a rule the playbook states explicitly, and is confirmed firing in the project's own log (**P0**)
- 1 means a clean clone cannot build (**P0**)
- 5 are latent hangs / freezes / crashes (**P1**)
- The rest are traps that will cost a debugging session each, including one open architectural
  question (F-17) that the current instrumentation cannot answer either way

---

## P0 — must fix before anything is shipped or tested off this machine

### F-01 · The DLL reads config, writes logs, and writes crash dumps to the *build machine's source directory*

**Location:** [CMakeLists.txt:77](CMakeLists.txt:77) → [src/common/Logger.cpp:194](src/common/Logger.cpp:194)

```cmake
target_compile_definitions(somavr_common PUBLIC SOMAVR_WORK_ROOT="${CMAKE_CURRENT_SOURCE_DIR}")
```

```cpp
std::filesystem::path WorkRoot()
{
#ifdef SOMAVR_WORK_ROOT
    return std::filesystem::path(SOMAVR_WORK_ROOT);   // baked at compile time
```

`WorkRoot()` is the sole root for **all four** runtime paths — there is no runtime override:

| Consumer | Location |
| --- | --- |
| `somavr.ini` | [Logger.cpp:208](src/common/Logger.cpp:208) |
| `logs/somavr.log` | [Logger.cpp:203](src/common/Logger.cpp:203) |
| `logs/dumps/` | [DllMain.cpp:72](src/dll/DllMain.cpp:72) |
| `somavr_entity_profiles.ini` | [HPLHandsBridge.cpp:3325](src/dll/HPLHandsBridge.cpp:3325) |

**Why this is P0.** [Package-Release.ps1:107](scripts/Package-Release.ps1:107) copies `somavr.ini`
into the package, and `Install-Or-Update-SOMAVR.ps1` installs it to `%LOCALAPPDATA%\SOMAVR\`. The
DLL never reads that file. On a user's machine it looks for `D:\Dev Debug\SOMAVR\somavr.ini`;
`InitializeAtPath` then calls `create_directories` and `WriteDefaultConfig()`
([Config.cpp:152-155](src/common/Config.cpp:152)), so if that machine has a `D:` drive it will
*silently create a fresh default config in a stranger's directory tree* and run from it. Every
setting the user edits does nothing. There is no error — this is precisely playbook 06's
"settings lie" plus 07's config-identity trap, in its worst form.

**Evidence it is already biting you:** your own most recent log, line 2 vs line 3:

```
somavr.dll loaded ... path=D:\Dev Debug\SOMAVR\out\SOMAVR-latest\somavr.dll
config_loaded path=D:/Dev Debug/SOMAVR\somavr.ini
```

The packaged DLL under `out\` was already reading the source-tree config. Any test run from a
package has been reading a config that was not in the package.

**Fix.** Resolve the work root at runtime from the loaded module's own directory
(`GetModuleFileNameW(g_module)` → `parent_path()`), with an ordered search: module dir → then
`%LOCALAPPDATA%\SOMAVR` → then, only in a Debug/dev build, the compiled-in `SOMAVR_WORK_ROOT`.
Keep `SOMAVR_WORK_ROOT` as an opt-in dev override behind a CMake option, never as the default.

**Then close the loop the playbook asks for** (07, "Log the config's identity"): the
`config_loaded` line must carry **path + last-write time + a hash of the parsed keys**, not just
the path. A path alone cannot distinguish "read the right file" from "read a stale one".

---

### F-02 · `xrEndFrame` is called with `layerCount = 0` on the comfort/presentation blackout path

**Location:** [OpenXRRuntime.cpp:3474-3495](src/dll/OpenXRRuntime.cpp:3474)

```cpp
if (comfortBlackout || presentationBlackout) {
    layerCount = 0;                                    // <-- submit nothing
    ...
}
...
endInfo.layerCount = layerCount;
endInfo.layers = layerCount > 0 ? layers : nullptr;
result = xrEndFrame(session_, &endInfo);
```

Playbook 07 names this exactly:

> **The floor is a hazard too: never submit *zero* layers.** … BioshockVR reproduced the same
> fail-fast in two independent crash dumps under a streaming runtime — an execute-access violation
> on a freed marker `0xDEDEDEDE` — correlated specifically with `layerCount=0` submissions. …
> **Keep the projection layer present every frame and clear its eye images to black.**

**Evidence this is live, not theoretical.** From `logs/somavr.log`:

```
[warn] openxr_presentation_blackout active=1 reason=loading_screen frame=380 ...
[info] openxr_presentation_blackout active=0 reason=loading_screen_complete frame=464 ...
```

That is **84 consecutive game frames** of zero-layer submission on one loading screen — roughly a
second of it, every level load. The sampled `openxr_frame ok` lines (1-in-300) already caught one
`layers=0`.

**There are three more paths to the same state**, all reached without any blackout:

1. `xrLocateViews` fails or returns invalid flags → `copied` stays false → the projection layer at
   [:2901-2907](src/dll/OpenXRRuntime.cpp:2901) is never appended → `layerCount == 0`. Any tracking
   dropout hits this.
2. `CopyCacheToEye` / `CopyBackbufferToEye` failure at [:2843](src/dll/OpenXRRuntime.cpp:2843) →
   same.
3. `frameState.shouldRender == XR_FALSE` → same. (This one is spec-legal; the others are not what
   you want.)

**Fix.** Make the projection layer unconditional once the session is running: keep it in `layers[0]`
every frame and implement blackout by **clearing the eye swapchain images to black** rather than by
dropping the layer. On copy/locate failure, submit the previous frame's eye images (or black) with
the last-known-good pose. The rule is *"submit black", never "submit nothing"*.

---

### F-03 · A clean clone cannot build — required build script and 16 source files are untracked

**Location:** working tree

[CMakeLists.txt:46](CMakeLists.txt:46) invokes `cmake/GenerateBuildInfo.cmake`, which is **not in
git**. Sixteen source files listed in `add_library(somavr SHARED ...)` are also untracked, including
whole subsystems:

```
cmake/GenerateBuildInfo.cmake        <- build fails at configure without this
src/common/BuildInfo.cpp/.h
src/dll/CrashHandler.cpp/.h          <- the entire crash handler
src/dll/CrashCapturePolicy.h
src/dll/HPLArmIKMath.cpp/.h
src/dll/HPLAuthoredInteractionBridge.cpp/.h
src/dll/HPLAuthoredInteractionMath.cpp/.h
src/dll/HPLEntityCalibrationProfiles.cpp/.h
src/dll/HPLTerminalMath.cpp/.h
src/dll/OpenXRFramePacingMath.h
```

Plus **57 modified tracked files** uncommitted. The last commit is `ed2e7d8`, but the binary you
have been testing (`0.85.0` / `0.87.0`) cannot be reconstructed from any commit.

**Compounding this:** `somavr.ini` is in `.gitignore`, yet
[Package-Release.ps1:107](scripts/Package-Release.ps1:107) copies it into every release. **The
shipped default configuration is an untracked, machine-local file.** On a clean clone the packaging
script fails outright; on a dirty tree it ships whatever dev config happens to be sitting there —
which is exactly how F-04 below got into the release package.

**Fix.**
1. Commit the untracked sources and `cmake/GenerateBuildInfo.cmake`.
2. Add a tracked `config/somavr.release.ini` holding the *shipping* defaults, and have
   `Package-Release.ps1` copy **that**, not the working config. Keep the root `somavr.ini` ignored
   as the dev scratch config.
3. Add a CI/manual step that configures and builds from a fresh `git clone` — that single check
   catches this class permanently.

---

## P1 — latent hangs, freezes, and crashes

### F-04 · The shipped `somavr.ini` is the developer probe config, and one probe costs a `GetAsyncKeyState` per draw call

**Location:** [somavr.ini](somavr.ini) (packaged verbatim), consumed at
[OpenGLHooks.cpp:1932](src/dll/OpenGLHooks.cpp:1932) and [:1947](src/dll/OpenGLHooks.cpp:1947)

The packaged config ships with diagnostics enabled: `MatrixCapture=1`, `RenderDiagnosticCapture=1`,
`HPLReflectionFadeControl=1`, `HPLVideoLifecycleProbe=1`, `HPLAudioListenerProbe=1`,
`HandTrackingProbe=1`, `DepthCompositionProbe=1`. `README.md:316` even says so out loud: *"The
active `somavr.ini` is currently set up for the OpenXR probe build."* The C++ defaults in
`Config.h` are all correctly `false` — it is only the shipped file that turns them on.

The sharpest one is `HPLReflectionFadeControl=1`, because of this:

```cpp
void APIENTRY HookGlDrawElements(GLenum mode, GLsizei count, GLenum type, const void* indices)
{
    ...
    PollReflectionFadeControl();     // <-- every single draw call
```

```cpp
void PollReflectionFadeControl()
{
    if (!g_config.hplReflectionFadeControl) return;
    const bool keyDown = (GetAsyncKeyState(VK_F3) & 0x8000) != 0;   // Win32 call, per draw
```

That is a user32 call on the render thread once per `glDrawElements` **and** once per
`glDrawArrays`, for the entire session. Playbook 06: *"never emit from the render thread at
per-draw rates"* — a syscall is worse than a log line.

Worse, once F3 is toggled, [`ApplyReflectionFadeBypass`](src/dll/OpenGLHooks.cpp:858) does a
`glGetNamedBufferSubData` (a **synchronous GPU readback**) plus two `glNamedBufferSubData` writes
**around every draw**. That will not just cost frametime, it will serialise the pipeline.

**Fix.**
1. Ship a release config with every `*Probe*`, `*Capture*`, and `*Diagnostic*` key at `0` (see F-03,
   fix 2).
2. Move the F3 poll out of the draw hooks and onto the frame boundary in `LogFrameSummary`, beside
   `UpdateShadowJitterHotkey` / `UpdateRenderDiagnosticHotkey` where the other hotkeys already live.
   Hotkey edge detection at 90 Hz is plenty; at draw rate it is a self-inflicted profile.
3. Cache the reflection-fade UBO offset/binding per program (it already is) and hoist the
   read-back out of the per-draw path — capture `authored` **once** when the bypass is armed, not
   per draw. As written, if the restore is ever skipped the next read latches the mod's own
   `1.0e20f` as "authored", which is playbook 07's *"never read your own output back as fresh
   input"* — the failure mode this project reportedly hit three times already.

---

### F-05 · The OpenXR layer budget is only enforced for the aim-guide layers, and you are already running at the array ceiling

**Location:** [OpenXRRuntime.cpp:2727](src/dll/OpenXRRuntime.cpp:2727),
[:3210-3217](src/dll/OpenXRRuntime.cpp:3210)

```cpp
const XrCompositionLayerBaseHeader* layers[12] = {};
```

`layerCapacity = std::min(maxLayerCount_, std::size(layers))` is computed **once**, inside the
controller-aim-guide block. Every other append is unchecked:

| Layer | Append site | Bounds-checked? |
| --- | --- | --- |
| Projection | [:2905](src/dll/OpenXRRuntime.cpp:2905) | no |
| HUD quad/cylinder | [:2990](src/dll/OpenXRRuntime.cpp:2990) | no |
| Terminal pointer | [:3092](src/dll/OpenXRRuntime.cpp:3092) | no |
| Interaction reticle | [:3174](src/dll/OpenXRRuntime.cpp:3174) | no |
| Aim guide segments | [:3309](src/dll/OpenXRRuntime.cpp:3309) | **yes** |
| Status panel | [:3387](src/dll/OpenXRRuntime.cpp:3387) | no |
| Comfort vignette | [:3455](src/dll/OpenXRRuntime.cpp:3455) | no |

Today the arithmetic happens to work out: the guide block reserves 1 for the status panel and 1 for
the vignette, so the total lands at exactly 12. Your own log confirms you are sitting on the
ceiling — `layers=12` appears in the sampled frames, alongside `layers=10` and `layers=11`.

Two real consequences:

1. **A runtime reporting a low `maxLayerCount` freezes the HMD.** If `maxLayerCount_ < 5`, the four
   fixed layers plus the vignette exceed it, `xrEndFrame` returns `XR_ERROR_LAYER_LIMIT_EXCEEDED`
   **every frame**, and per playbook 07 that is a frozen headset with the flat game still running —
   not a crash, so easy to misread.
2. **The next layer type added overflows a stack array.** Any new always-on layer that appends
   without updating `reservedGuideLayers` writes past `layers[12]`. Playbook 07: *"size the layer
   array with headroom (a too-small array is a stack overflow, not a clamp)."*

**Also flag:** [docs/BIOSHOCK_VR_TRANSFER_AUDIT.md:234](docs/BIOSHOCK_VR_TRANSFER_AUDIT.md:234)
lists *"OpenXR layer counting, runtime maximum query, fixed array capacity, and priority-based
truncation"* under **"Already Aligned"**. The runtime maximum is queried but not enforced, and
there is no priority-based truncation at all — layers append in fixed source order. The audit
should be corrected or the code brought up to it; right now the doc will stop someone from finding
this.

**Fix.** Replace every raw `layers[layerCount++]` with a single `AppendLayer(header)` helper that
refuses past `std::min(maxLayerCount_, std::size(layers))` and counts drops. Size the array to 16
(the spec floor) for headroom. Append **most-important-first** — projection, HUD, reticle, then
guides/panel/vignette — so the cap drops decoration rather than the world.

---

### F-06 · `xrWaitSwapchainImage` uses `XR_INFINITE_DURATION` while holding the lock the game thread needs

**Location:** [OpenXRGLBridge.cpp:654](src/dll/OpenXRGLBridge.cpp:654) and
[:705](src/dll/OpenXRGLBridge.cpp:705); lock at [OpenXRRuntime.cpp:396](src/dll/OpenXRRuntime.cpp:396)

`OnFrameBoundary` takes `mutex_` and holds it across the **entire** `SubmitFrameLocked`, which
contains `xrWaitFrame` (blocks until the compositor's next frame) and two
`xrWaitSwapchainImage` calls with an infinite timeout.

Meanwhile the game/update thread reaches into the same mutex constantly — `HPLGrabBridge` and
`HPLInteractionBridge` hooks call `GetLatestInput()`
([OpenXRRuntime.cpp:795](src/dll/OpenXRRuntime.cpp:795)), `GetLatestHeadPose()`, and
`RequestHapticPulse()` ([:1028](src/dll/OpenXRRuntime.cpp:1028), which issues
`xrApplyHapticFeedback` *inside* the lock) from `HPLInputBridge.cpp:948`, `1018`, `1137`, `1610`,
`HPLGrabBridge.cpp:463`, `519`, `673`, `922`, `HPLInteractionBridge.cpp:350`, `811`.

Two consequences: routine per-frame contention where the game thread waits on the compositor, and —
if a runtime ever stalls a swapchain wait — a **whole-process hang** rather than a dropped frame,
with no diagnostic distinguishing it from a game freeze.

**Fix.** Give the swapchain waits a bounded timeout (a frame period or two) and treat expiry as
"skip this eye this frame, count it" — never infinite. Separately, split the lock: a small
`stateMutex_` for the published snapshots the game thread reads/writes, and a `submitMutex_` for
the XR frame loop. The game thread should never be able to block on `xrWaitFrame`.

---

### F-07 · Injector treats a timed-out remote thread as success, then frees memory that thread is still reading

**Location:** [src/injector/main.cpp:470-484](src/injector/main.cpp:470)

```cpp
WaitForSingleObject(thread, 10000);          // return value discarded

DWORD remoteModule = 0;
GetExitCodeThread(thread, &remoteModule);    // STILL_ACTIVE (259) on timeout -> "success"
CloseHandle(thread);
VirtualFreeEx(process, remotePath, 0, MEM_RELEASE);   // frees the path the thread may be reading
...
if (remoteModule == 0) { ... return false; }
std::wcout << L"Injected " << fullPath << ...;
```

Playbook 07 states the requirement almost verbatim:

> *bound the remote thread's wait with an explicit failure (a stall otherwise masquerades as
> success)*, and *verify the module is actually present afterwards*. **On timeout, deliberately do
> not free memory the still-running remote thread might still read.**

All three are missed. A stalled `LoadLibraryW` reports "Injected …", and the `VirtualFreeEx` can
pull the path string out from under it — a crash inside the game attributable to nothing.

Secondary: on x64 `GetExitCodeThread` truncates `LoadLibraryW`'s `HMODULE` to 32 bits, so it is not
a reliable success signal even on the happy path.

**Fix.**
```cpp
const DWORD wait = WaitForSingleObject(thread, 10000);
if (wait != WAIT_OBJECT_0) {
    std::wcerr << L"Remote LoadLibraryW did not complete within 10s; leaking remote page deliberately\n";
    CloseHandle(thread);
    CloseHandle(process);
    return false;                 // note: no VirtualFreeEx
}
```
Then verify presence for real: re-run `ScanCompatibility(pid)` (or a `Module32*` walk) after the
wait and require `somavr.dll` to be listed. That is the check that makes injection fail closed.

---

### F-08 · `DllMain` detach closes the event the worker thread is waiting on

**Location:** [src/dll/DllMain.cpp:466-478](src/dll/DllMain.cpp:466) and
[:554-560](src/dll/DllMain.cpp:554)

```cpp
} else if (reason == DLL_PROCESS_DETACH) {
    if (g_stopEvent != nullptr) {
        SetEvent(g_stopEvent);
        CloseHandle(g_stopEvent);      // worker may be inside WaitForSingleObject right now
        g_stopEvent = nullptr;         // and may read this next iteration
    }
}
```

The worker loop is:

```cpp
for (;;) {
    if (WaitForSingleObject(g_stopEvent, 500) == WAIT_OBJECT_0) break;
    if (IsOnlyCurrentThreadRemaining()) { ...; break; }
    somavr::MaintainCrashHandler();
}
```

Two defects. The handle is closed immediately after `SetEvent`, racing a concurrent wait (handle
recycling). And once `g_stopEvent` is nulled, `WaitForSingleObject(nullptr, 500)` returns
`WAIT_FAILED` — which is not `WAIT_OBJECT_0`, so **the loop does not break; it spins hot**. On
process exit `IsOnlyCurrentThreadRemaining()` saves you (that is the good orphan-worker guard from
playbook 07, correctly implemented). On an explicit `FreeLibrary` unload it does not, and you get a
100%-CPU spin in a module that is being unmapped.

**Fix.** Signal only; never close from `DllMain`. Let the worker close the handle on its own way
out, and have the loop treat any non-`WAIT_TIMEOUT` result as a reason to exit.

---

## P2 — traps that will cost a session each

### F-09 · Unknown config keys and unknown sections are silently ignored

**Location:** [src/common/Config.cpp:533-1008](src/common/Config.cpp:533)

The parser is a long `if/else if` chain per section, terminating in
`else if (ParseLateControllerConfig(config_, key, value)) {}` — **with no final `else`**, and no
handling for an unrecognised `[Section]` either. A typo'd key, a key that moved sections, or a key
added to the ini but not to the parser produces zero output.

Playbook 07 makes this hard rule #1 for a reason:

> Miss one and the key is silently rejected — and that rejection often looks *identical* to running
> a stale build. … After adding a key, **confirm from logs** that the running build accepts it (no
> "unknown key" warning) before asking anyone to test it.

Adding a key currently requires four edits in lockstep (`Config.h` field, parser branch,
`WriteDefaultConfig`, and the `hook_config` log line in `DllMain.cpp`). Nothing enforces that.

**Fix.** Add `else { LogUnknownKey(section, key); ++unknownKeys_; }` and the same for unknown
sections, then emit one summary line: `config_keys accepted=N unknown=M`. That single counter is
the difference between "the fix didn't work" and "the fix never loaded".

---

### F-10 · Three raw, unguarded reads/writes of game memory in the hot camera path

**Location:** [HPLCameraBridge.cpp:450-471](src/dll/HPLCameraBridge.cpp:450),
[HPLGrabBridge.cpp:163](src/dll/HPLGrabBridge.cpp:163),
[HPLCompatibilityProbe.cpp:1492](src/dll/HPLCompatibilityProbe.cpp:1492)

Most of the codebase does this correctly via `ReadProcessMemory(GetCurrentProcess(), ...)` —
`HPLInputBridge`, `HPLHandsBridge`, `HPLTerminalBridge`, `HPLInteractionBridge`,
`HPLNativeLocomotion`, `HPLPresentationBridge`, `HPLScreenEffectBridge`, `HPLCrosshairBridge`,
`HPLUserModuleBridge` all do. Three do not:

```cpp
// HPLCameraBridge.cpp — no null check, no guard, and this includes WRITES
template <typename T> T ReadField(const void* object, size_t offset)
{ T value{}; std::memcpy(&value, static_cast<const std::byte*>(object) + offset, sizeof(value)); return value; }

template <typename T> void WriteField(void* object, size_t offset, const T& value)
{ std::memcpy(static_cast<std::byte*>(object) + offset, &value, sizeof(value)); }

void MarkCameraRotationDirty(void* camera) { WriteField(camera, kCameraViewDirtyOffset, dirty); /* ... */ }
```

This is the per-frame render-thread path — the highest-consequence place in the mod for a stale
pointer. Playbook 07: *"Guard every raw memory read/write … and treat non-finite / null as
'unavailable,' not 'zero.' … fail closed."*

`HPLGrabBridge::ReadFloat` is the same shape, used by `IsGrabForcePid`/`IsGrabTorquePid` for PID
classification — the caller null-checks, but nothing validates the address.
`HPLCompatibilityProbe::ReadVector`/`WriteVector` are the mildest of the three (they do null-check
inline) but still dereference an unvalidated game pointer.

**Fix.** Route all three through the same `ReadProcessMemory` helper the rest of the codebase
already uses, returning `bool`. Keep the SEH discipline in mind if you ever prefer `__try`: playbook
07 requires the guarded region be **POD-only** — no `std::string`, no containers, no destructors —
because MSVC rejects `__try` in a function needing C++ unwinding (C2712). `CrashHandler.cpp:196`
already gets this right and is the model to copy.

---

### F-11 · The `AddImpulse` patch rewrites 12 bytes of live code with no thread suspension

**Location:** [HPLGrabBridge.cpp:1355-1398](src/dll/HPLGrabBridge.cpp:1355)

The ceremony is otherwise textbook — `IsInsideImage` range check, exact signature verify, original
bytes saved, `VirtualProtect` round-trip, `FlushInstructionCache`, restore on removal. The gap is
that `WriteCodeBytes` is a bare `memcpy` over a thunk the game may be executing on another thread at
that instant. Twelve bytes are not written atomically; a thread mid-thunk can execute a
half-rewritten instruction stream.

Every other hook in the project goes through MinHook, which suspends threads for exactly this
reason. `RestoreAddImpulsePatch()` at shutdown has the same exposure, arguably worse — the game is
definitely live then.

Note also that playbook 07 records this site as *"a guarded INT3-plus-absolute-jump applied only for
a one-shot window instead of a permanent detour"*; the code now installs it permanently at load
whenever `ThrowRedirect=1`. Worth reconciling the doc with the implementation either way.

**Fix.** Suspend other threads (and check none has an RIP inside `[target, target+12)`) around both
the patch and the restore — or, simpler, write the 12 bytes as a hot-patch-safe sequence: write
bytes 2..11 first, then the leading two bytes last with a single aligned atomic store.

---

### F-12 · Every GL detour intercepts the mod's own GL calls — no "this call is mine" scope

**Location:** whole-file — `OpenGLHooks.cpp` detours vs `OpenXRGLBridge.cpp` GL work

There is no thread-local self-call guard anywhere in the DLL (`grep thread_local` finds pass-state
tracking only, no reentrancy scope). `OpenXRGLBridge::ResolveGlProc` resolves `glBindFramebuffer` via
`wglGetProcAddress`/`GetProcAddress` — but MinHook patches the *code at that address*, so the
bridge's own calls run through the mod's detours.

Concretely, `HookGlBindFramebuffer` ([OpenGLHooks.cpp:1433](src/dll/OpenGLHooks.cpp:1433)) updates
`g_currentFramebuffer`. Every XR eye blit, HUD capture, reticle raster and desktop mirror therefore
overwrites the mod's own idea of "the framebuffer the game has bound" — and that value is read by
`HookGlClear` ([:1963](src/dll/OpenGLHooks.cpp:1963)) to decide terminal clear suppression, and by
the post-effect resource capture arrays. Draw/bind counters in `frame_summary` are likewise
inflated by the mod's own work.

Playbook 07 is unusually emphatic here — *BioshockVR spent six build cycles* on this exact class,
chasing what looked like a runtime bug.

**Fix.** Add `thread_local int g_inOwnGlWork;` with a scoped RAII incrementer. Wrap every
`OpenXRGLBridge` operation and every private raster in it, and have each detour early-out to the
original when it is non-zero. Cheap, and it makes all the telemetry mean what it says.

---

### F-13 · `hook_config` is one 90-argument `printf` into a 4096-byte truncating buffer

**Location:** [DllMain.cpp:91](src/dll/DllMain.cpp:91) (format string is 2,414 chars) and
[:176](src/dll/DllMain.cpp:176); buffer at [Logger.cpp:132](src/common/Logger.cpp:132)

```cpp
char message[4096] = {};
vsnprintf_s(message, sizeof(message), _TRUNCATE, fmt, args);
```

The rendered line in your current log is **2,409 characters** — 59% of the buffer, growing with
every key you add. When it crosses 4096 it will truncate **silently**, and the thing that gets
truncated is the tail of the config dump: the OpenXR block. That is the single most
test-validity-critical line the mod emits.

Separately, a 90-argument variadic call has no compile-time checking; one inserted field in the
wrong position is undefined behaviour with no diagnostic. Playbook 07 flags the general form
("wide positional argument lists transpose silently").

**Fix.** Split the dump into one line per section (`hook_config_hooks`, `hook_config_roomscale`,
`hook_config_openxr`, …), and have `WriteV` detect truncation (`vsnprintf` returns the needed
length) and emit a `log_truncated` warning rather than dropping the tail quietly.

---

### F-14 · Synchronous `glReadPixels` on the eye-capture path, enabled in the shipped config

**Location:** [OpenXRGLBridge.cpp:582](src/dll/OpenXRGLBridge.cpp:582)

The depth-cache probe does a blocking `glReadPixels(..., GL_FLOAT, ...)` plus several `glGetError()`
calls inside the per-eye capture. It is bounded (samples 1-4, then every 120th), and
`DepthCompositionProbe=1` is on in the packaged ini. Playbook 06: *"GPU texture readback is
synchronous unless carefully staged … If a visual glitch happens only while dumping, classify it as
capture pressure before treating it as a renderer regression."*

**Fix.** Low urgency given the throttle, but it belongs in the release-config sweep from F-04, and
any frametime measurement taken with it on should record that fact as a covariate.

---

### F-15 · `Logger::Path()` returns a reference with no lock

**Location:** [Logger.cpp:164](src/common/Logger.cpp:164)

`logPath_` is guarded by `mutex_` in `Initialize`, but `Path()` hands out a bare reference to it.
`DllMain` calls `g_config->Path()` (a different object) so this is not currently exercised across
threads — file it as a small correctness cleanup: return by value.

---

### F-16 · The runtime's GL-context clobber window is outside SOMAVR's save/restore bracket

**Location:** [OpenXRGLBridge.cpp:634-681](src/dll/OpenXRGLBridge.cpp:634) (`CopyCacheToEye`) and
[:1043](src/dll/OpenXRGLBridge.cpp:1043) (`CopyCacheToImage`)

Corroborated externally. TheDarkModVR (idTech4, OpenGL, shipped) carries **four** separate
workarounds in `renderer/vr/OpenXRBackend.cpp` for a runtime mutating GL state behind the
application's back:

```
:157, :842, :1045  // hack: current SteamVR OpenXR implementation is broken and does not
                   // properly reset the GL context after certain calls
:986               // hack: the SteamVR OpenXR runtime does not properly set viewport and
                   // scissor before copying render textures :(
```

SOMAVR's ordering leaves it exposed to exactly that:

```
xrAcquireSwapchainImage      <- runtime issues GL calls here
xrWaitSwapchainImage         <- runtime issues GL calls here
  CopyCacheToImage()           save state -> blit -> restore state
glFlush
xrReleaseSwapchainImage      <- runtime issues GL calls here, AFTER the restore
```

The save/restore window is **nested inside** the acquire/release bracket. Two consequences:

1. State captured at the top of `CopyCacheToImage` may already be the *runtime's*, not the game's —
   so "restore" writes back the wrong values.
2. Anything the runtime clobbers during `xrReleaseSwapchainImage` is never restored at all. It
   leaks into the game's next draw after `SwapBuffers` returns.

Note the second TDM comment is *viewport and scissor* — the same pair as SOMAVR's terminal-tile
stale-scissor bug already recorded in playbook 10. Worth holding as an alternate root cause if that
one ever recurs.

**Fix.** Move the state capture to before `xrAcquireSwapchainImage` and the restore to after
`xrReleaseSwapchainImage`, so the guard brackets every runtime call, not just your own blit. Same
change in `CopyDepthCacheToEye`. This is cheap and correct regardless of how F-17 is decided.

---

### F-17 · There is no instrumentation that could price a GL-vs-D3D11 submission path

**Location:** [OpenXRRuntime.cpp:69](src/dll/OpenXRRuntime.cpp:69),
[:2669-2684](src/dll/OpenXRRuntime.cpp:2669)

TheDarkModVR ships `xr_preferD3D11` defaulting to **"1"** — D3D11 is their *default* XR path and GL
is the fallback:

```cpp
idCVar xr_preferD3D11( "xr_preferD3D11", "1", ...,
    "Use D3D11 for OpenXR session to work around a performance issue with SteamVR's OpenXR implementation" );
```

Their mechanism: render into a GL texture as usual, share it via `WGL_NV_DX_interop2`, one small
D3D11 flip-blit into the real XR swapchain image. The GL renderer is untouched. Cost is one extra
device plus one extra copy per eye per frame.

**Whether this applies to SOMAVR is currently unanswerable, and that is the finding.** Three gaps:

- **Different runtime.** The only session on file ran `runtime="VirtualDesktopXR" version=1.0.10`
  ([somavr.log](logs/somavr.log)). TDM's cvar names SteamVR specifically. SOMAVR has never been
  measured on SteamVR's OpenXR runtime, so the symptom has neither been observed nor excluded.
- **The one wait metric has a useless threshold.** `kLongXrWaitThresholdUs = 100000` — 100 ms. At
  90 Hz a healthy `xrWaitFrame` is ~11 ms, so this only fires on a near-hang. It fired **0 times**
  in that session, which is not evidence of good performance; it is evidence the threshold cannot
  detect a performance problem. `xrWaitFrame` is also the wrong thing to watch — it is the pacing
  wait, and TDM's cost is in *submission*.
- **Submission itself is untimed.** `stereoCaptureDeltaUs` measures the gap *between* eye captures,
  not the cost of acquire → wait → blit → release. The `GL_TIMESTAMP` query ring exists
  ([HPLCompatibilityProbe.cpp:468](src/dll/HPLCompatibilityProbe.cpp:468)) but is attributed to AFR
  eye *render* work, and `HPLPerEyePerformanceTelemetry=0` in the shipped config anyway.

**Do not adopt the D3D11 interop path on this evidence.** It is a second graphics device, a second
resource-lifetime domain, and a new failure surface — the exact shape playbook 07 warns about when
a feature's resource lifetime tangles with the compositor's. Measure first.

**Fix (measurement, ~half a day).**
1. Drop `kLongXrWaitThresholdUs` to something diagnostic (~2× the display period) so the existing
   counter can actually see a regression.
2. Add a CPU timer around the acquire→release bracket in `CopyCacheToEye`, reported per eye in the
   existing `openxr_frame ok` summary as `eyeSubmitUs=`. Two `QpcNow()` calls; the plumbing is
   already there next to `xrWaitLastUs_`.
3. Run one A/B: same scene, same route, SteamVR OpenXR vs VirtualDesktopXR, `eyeSubmitUs` compared.
   If SteamVR shows a large unexplained submit cost, the interop path is priced and justified. If
   it does not, this is a TDM/SteamVR-specific issue and SOMAVR should stay GL-native.

Reference implementation if it is ever justified: `renderer/vr/OpenXRSwapchainDX.cpp`,
`OpenXRSwapchainGL.cpp`, `D3D11Helper.cpp` in `D:\Dev Debug\thedarkmodvr\`.

---

### F-18 · The REX-prefix fix was applied at one site, not adopted as a class

**Location:** [SomaBuildSignatures.h:49-60](src/common/SomaBuildSignatures.h:49) (guarded) vs
[HPLNativeLocomotion.cpp:214-221](src/dll/HPLNativeLocomotion.cpp:214) and
[HPLHudBridge.cpp:169-172](src/dll/HPLHudBridge.cpp:169) (unguarded)

Prompted by PreyVR's audit of the same class. **Result of the scan: no true interior anchor found**
— the Prey tell (a signature where somebody abandoned the prologue for an interior offset) does not
appear in SOMAVR's registry. But the scan surfaced a different gap.

Three sites extract a RIP-relative `disp32` at hardcoded `+3` and compute the next instruction at
`+7`. Only one carries the coupling guard:

| Site | Prefix verified? | Suffix verified? | `static_assert` on the offsets? |
| --- | --- | --- | --- |
| `SomaBuildSignatures.h` (raycast) | yes | — | **yes, ×3** |
| `HPLHudBridge.cpp:169` | yes (`48 8b 05`) | yes (`48 8b 40 50 c3` at +7) | no |
| `HPLNativeLocomotion.cpp:214` | via full-signature `memcmp` only | no | **no** |

**This is not a live bug.** `kGetGamePausedSignature` begins `48 8b 05 …` and the full-signature
`memcmp` gates the extraction, so `+3`/`+7` are correct on the current build. The hazard is that
nothing *enforces* the coupling. Whoever updates that signature for a future SOMA build — the exact
moment the raycast incident happened — gets no compile error if the new prologue uses a different
instruction form. The offsets silently become wrong and resolve `g_gameContextSlot` to garbage;
`IsReadable` would catch most of it, but not all.

`HPLHudBridge` is the better pattern of the two: verifying both the `48 8b 05` prefix **and** the
suffix at `+7` pins the instruction length at runtime, independent of any assert.

**Fix (~20 minutes).** Move the two remaining `+3`/`+7` pairs into `SomaBuildSignatures.h` as named
constants beside `kRaycastGameContextDisplacementOffset`, and give them the same three
`static_assert`s: displacement offset + `sizeof(int32_t)` == next-instruction offset, next-instruction
offset ≤ `sizeof(signature)`, and the three bytes before the displacement are `48 8b 05`. That last
assert is what turns the class into a compile error instead of a session.

**Two follow-ups for whoever has Ghidra open** (I could not confirm these — per `CLAUDE.md` Ghidra
must be launched before the client, and it was not):

- [HPLCompatibilityProbe.cpp:2147](src/dll/HPLCompatibilityProbe.cpp:2147)
  `kRenderPostEffectsSignature` begins `4d 85 c9 0f 84 74 01 00 00` — `test r9,r9; je rel32` **before**
  any register save. That ordering is unusual for a prologue. It is plausibly a function that tests
  an argument before frame setup, but it is the one signature in the set that would look the same
  either way. Worth confirming it sits at the function entry.
- [HPLComfortBridge.cpp:56](src/dll/HPLComfortBridge.cpp:56) extends a 7-byte setter signature with
  nine `0xcc` alignment bytes to reach uniqueness. Padding is a linker artifact, not code — it moves
  when the *next* function's alignment changes, not when this function does. It fails closed, so it
  is safe, but it will produce a confusing "signature mismatch" on a build where nothing about the
  target function changed.

Separately, several signatures bake build-specific relative displacements into the pattern itself
(`HPLCrosshairBridge.cpp:30` `e9 5b 73 e1 ff`; `HPLHudBridge.cpp:178/183` and
`HPLNativeLocomotion.cpp` `48 8b 05 <disp32>`). That makes them build fingerprints rather than
patterns. Fine — arguably desirable here, since fail-closed on a patched SOMA is the wanted
behaviour — but it should be a stated intent in `ADDRESS_REGISTRY.md` rather than an accident, so
nobody "fixes" them into wildcards later.

---

## What is already right

Do not regress these.

- **Signature-verified, fail-closed hook installs.** `HPLCameraBridge:1419`, `HPLComfortBridge:510`,
  `HPLGrabBridge:1373`, `HPLHandsBridge:3368`, `HPLContactHapticsBridge:210`,
  `HPLGameplayHapticsBridge:137`, `HPLCrosshairBridge:274` all range-check with `IsInsideImage` and
  byte-compare the prologue before touching anything, and log a specific `reason=` on refusal.
  Sub-features degrade individually (`hpl_roomscale_safety disabled reason=line_of_sight_signature_mismatch`)
  rather than taking the build down. This is playbook 07's core discipline, done properly.
- **The crash handler.** `CrashHandler.cpp` hits every requirement in playbook 06: reentrancy guard,
  duplicate-fault-address suppression, a hard 3-capture session cap, vectored lane restricted to
  always-fatal codes, chained + periodically re-armed top-level filter, module/RVA + access type +
  registers + stack candidates, full memory behind an explicit env var. The `__try` block at
  `:196` is correctly POD-only.
- **The orphaned-worker exit condition** (`IsOnlyCurrentThreadRemaining`, `DllMain.cpp:43`) — the
  race-free "am I the last thread" check playbook 07 recommends over a stop handshake.
- **AFR eye-phase toggle gated on a real fill.** `HPLCameraBridge.cpp:1310` only flips
  `nextEyeIndex` when `ApplyStereoEye` succeeded, and resets it on recenter/calibration change. That
  is exactly the desync guard playbook 10 asks for.
- **GL state save/restore around private work,** including the scissor enable/box that playbook 10
  singles out (`OpenXRGLBridge.cpp:537-540`, `626-628`, `769-772`, `810-812`).
- **Companion-DLL preload** from the module's own directory before the first delay-loaded `xr*` call
  (`OpenXRRuntime.cpp:1656-1681`) — the `0xc06d007e` trap, already closed.
- **Level filter taken before the logger mutex** (`Logger.cpp:128`) — playbook 06's render-thread
  serialisation trap, already closed.
- **Pure math isolated into `somavr_render_math`** with 2,000 lines of tests, plus config-preset,
  injector-compatibility, logger, crash-handler and installer-lifecycle tests. This is the right
  architecture and it is why the math is trustworthy.
- **Packaging integrity:** SHA-256 manifest, flavor/version cross-checks, refusal to package a
  non-OpenXR build, staging-path escape checks, checksum verification on install.
- **Documentation.** `BUILD_HISTORY`, `CURRENT_STATE`, `HYPOTHESES`, `ADDRESS_REGISTRY`,
  `TEST_CHECKLISTS` are unusually good. The one correction needed is the layer-budget claim in
  `BIOSHOCK_VR_TRANSFER_AUDIT.md:234` (see F-05).

---

## Suggested order of work

1. **F-03** — commit everything and add a tracked release ini. Nothing below is verifiable until
   the tree is reproducible.
2. **F-01** — runtime work-root resolution + config identity logging. Every subsequent test is
   invalid until the DLL reads the config the tester edited.
3. **F-09** — unknown-key warning. Two lines, and it makes F-01's fix self-verifying.
4. **F-02** — projection layer every frame, blackout by clearing to black.
5. **F-05** — `AppendLayer` helper, array to 16, priority order. Correct the transfer audit.
6. **F-04** — release-config sweep + move the F3 poll to the frame boundary.
7. **F-07, F-08** — injector timeout/verify, DllMain event lifetime. Small and self-contained.
8. **F-06** — bounded swapchain waits first (small), lock split second (larger).
9. **F-16** — move the GL state guard outside the acquire/release bracket. Small, and independent
   of everything else.
10. **F-18** — hoist the two remaining `+3`/`+7` displacement offsets into `SomaBuildSignatures.h`
    with the same `static_assert`s the raycast site already has. ~20 minutes, and it converts a
    future debugging session into a compile error.
11. **F-10, F-11, F-12** — memory-safety and reentrancy hardening.
12. **F-13, F-14, F-15** — logging and cleanup.
13. **F-17** — add submit-path timing, then run the SteamVR-vs-VirtualDesktopXR A/B. This is a
    measurement task, not an implementation task; do not let it turn into a D3D11 interop port
    until the numbers ask for one.

Per playbook 07's HaloVR lesson, do these as **one independently verified path per headset build** —
a broad cleanup touching many paths at once is how you get a clean launch and a fatal error at the
first level transition.

---

## Not verified in this review

Stated plainly so nobody reads silence as a pass:

- **Nothing was run.** No build, no test execution, no in-game session. All findings are from source
  reading plus the committed `logs/somavr.log` from the 2026-07-27 session.
- **RVA correctness** for the 82 hardcoded offsets was not checked against the binary. Ghidra/ReGenny
  were not attached (and per `CLAUDE.md` must be launched before the client, which was not done).
  The signature checks make a wrong RVA fail closed, which is the important property.
- **Whether the AFR stereo path currently works.** The log shows
  `openxr_stereo_submission enabled=0` after ~7 minutes and repeated
  `warming_up leftReady=0 rightReady=0`. That may be expected for that session or may be a real
  regression — it needs a headset run to tell apart, and it is the most likely candidate for the
  next investigation after the P0s land.
- **Whether `maxLayerCount` is ever below 12 on real runtimes.** F-05's freeze scenario is derived
  from the code, not observed. The array-headroom half of it stands regardless.
- Per-file review depth was uneven: `DllMain`, `Logger`, `Config`, `OpenGLHooks`, `OpenXRRuntime`,
  `OpenXRGLBridge`, `CrashHandler`, `HPLCameraBridge`, `HPLGrabBridge`, the injector and the
  packaging scripts were read closely. The remaining ~30 `HPL*Bridge`/`HPL*Math` files were surveyed
  by pattern search (raw reads, signature checks, hot-path work) rather than read line by line.
