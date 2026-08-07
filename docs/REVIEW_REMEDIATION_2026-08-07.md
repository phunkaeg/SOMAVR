# Review Remediation - 2026-08-07

Source review: `REVIEW_HANDOVER_2026-08-07.md`.

## Closed In 0.88

| Finding | Resolution |
| --- | --- |
| F-01 | Runtime root resolves from `somavr.dll`; `SOMAVR_ROOT` is the explicit dev override. Config identity logs path, timestamp, parsed-key hash and parser counts. |
| F-02 | Renderable XR frames prioritize a projection layer. Blackout clears both eye images to opaque black; locate/copy failures use valid retained content and last-known/fallback views. Open-frame recovery also attempts one projection layer. |
| F-03 | Required generated-build, crash, IK, interaction, profile, terminal and pacing sources plus the release config are tracked by the 0.88 baseline commit. Packaging consumes `config/somavr.release.ini`, never the ignored scratch INI. |
| F-05 | All layer admission goes through one runtime-aware collector. Submit is hard-capped at 16, storage has 24 slots, and priority replacement plus max/drop telemetry is active. |
| F-07 | Remote-thread timeout/failure is explicit, uncertain remote memory is retained, and loaded-module presence is required for success. |
| F-08 | DllMain signals only. The worker owns event close and exits for success or failure rather than spinning on an invalid handle. |
| F-09 | Unknown sections/keys warn and are counted; release-config parser coverage is an automated test. |
| F-15 | `Logger::Path()` returns a mutex-protected value. |

## Closed In 0.89

| Finding | Resolution |
| --- | --- |
| F-02 follow-up | Every begun frame now has a hard one-projection minimum, including `shouldRender=false` and emergency close. Context recovery closes before GL resource release. |
| F-11 | AddImpulse installation/restoration requires one executable signature, suspended peer threads, no RIP in the patch window, and exact expected bytes. |
| F-12 | A DLL-wide nested own-GL scope brackets complete bridge operations; all GL detours directly forward mod-owned calls. |
| Packaging safety | Install/update/uninstall use a literal allowlist, require explicit custom-destination opt-in, preserve unknown files, and never recursively remove the install tree. |

## Partially Closed

| Finding | Resolution and remaining work |
| --- | --- |
| F-04 | Release captures/probes are disabled and F3 polling is once per frame. The explicitly enabled reflection bypass still performs per-draw UBO save/restore; redesign that experimental lane before promoting it. |
| F-06 | Every swapchain wait is bounded to 50 ms and preserves acquired-image state across timeout. Splitting XR submission from published input/pose locking remains a dedicated architecture build. |
| F-14 | Depth readback is disabled in release defaults. Replacing the diagnostic readback with staged PBO/fence capture remains optional diagnostic work. |

## Deliberately Deferred

| Finding | Next isolated build |
| --- | --- |
| F-10 | Convert the three remaining raw camera/grab/probe accesses to checked process-memory helpers, with focused stale-pointer tests. |
| F-13 | Split the large positional config log into section records and make logger formatting length-aware. |

The lock split, raw-memory conversion, code-patch synchronization and own-GL
scope are intentionally not combined with 0.88. Each can affect a different
live thread or renderer ownership boundary and should be independently verified
at a loading-screen and level-transition boundary.
