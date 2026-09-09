> Read `CLAUDE.md` in this folder once as well; the pair is one instruction set.
> Current user scope and the project-specific restrictions below apply throughout.

## Project knowledge graph

For architecture, ownership and design questions, query the existing project graph
before broad source browsing: `graphify explain "<concept>"`, `graphify query
"<engine noun phrase>"`, or `graphify path "<A>" "<B>"`. Use `rg` first for exact
symbols, byte patterns and known filenames; verify graph leads in their source.
Check age and coverage. Dirty graph files alone do not invalidate a query. If the
graph or tool is unavailable, state that limitation once and use scoped source/doc
searches. Do not turn graph setup into a prerequisite for an unrelated fix.
Refresh after substantial indexed changes using the project's update procedure;
avoid rebuilding third-party corpora for a small edit. Read a full graph report
only when a broad architecture review needs it.
The project graph is `graphify-out/graph.json`.
Start from `docs/FEATURE_TRACEABILITY.md` for feature ownership. Native addresses
are in `docs/ADDRESS_REGISTRY.md`; implementation maps are in
`docs/VR_COMPATIBILITY_RE.md` and `docs/FUTURE_SYSTEMS_RE.md`.

## Engine source for this target

**HPL2 source is on this machine:** `D:\Dev Debug\source code\AmnesiaTheDarkDescent` and `AmnesiaAMachineForPigs`, GPL v3, officially released by Frictional. HPL3's direct predecessor - free to read, and GPL terms apply if you copy. See playbook ch00 `#local-engine-sources`.

## RE workflow and tool discovery

Read [the shared RE workflow](<D:/Dev Debug/VR Modding/AGENT_RE_WORKFLOW.md>) once when choosing an RE approach.
Use `vr-re-workflow` for the method and `re-mcp-toolkit` for the selected tool's
preflight and caveats. Read their `SKILL.md` files directly if there is no skill invoker.
The session's discoverable tools and schemas determine what is callable; a configured
server, a responding host and the correct open program/process are separate checks.

Useful tool families: Ghidra for static contracts; ReGenny / `cheatengine` for live
layouts; Frida / x64dbg or x32dbg for execution; RenderDoc / apitrace for graphics
evidence. `local-llm` is optional bounded assistance: probe once, fall back if offline,
and verify its output against the original evidence. A second model is not proof.
Choose tools by the question, not by a fixed server count or a two-tool quota.

Respect current user scope and exclusive runtime ownership. A closed app or missing
launch script is not proof that automation is impossible: check the existing runbook,
launcher and supported CLI/API within that scope. Keep static work moving when live
work is unavailable. Static contracts can be proved from bytes; runtime acceptance
and headset quality require their own evidence.

## xr-sim and xr-tape

For an authorized live SOMA launch from Codex, run the injector outside the
filesystem sandbox using the normal approval mechanism. A sandboxed launch can
load the DLL successfully but stop at SOMA's "Could not write to Documents
folder" dialog before rendering. This was reproduced on 2026-09-09; retrying the
same binary outside the sandbox reached rendering. Do not change Documents ACLs
or use the developer launcher to work around it. Verify frame progress, not just
the injector's success line, and check for a leftover process before retrying.

`D:/Dev Debug/xr-sim` supplies a headset-free OpenXR runtime;
`D:/Dev Debug/xr-tape` records submitted poses, FOVs, layers and timing. Read their
current launcher scripts for process-scoped setup; this target needs the **x64**
loader/layer. Select runtime and layer per process, preserving shared machine state.
Use existing project launch/injection paths within the user's runtime authorization.
A trace checks the submission contract; it does not prove the game rendered those
views, accepted an input event, or looks correct in a headset. Correlate frames with
mod logs and graphics evidence.

Check `XrCompositionLayerDepthInfoKHR` separately: a reserved schema field does not prove the active layer records or validates depth. Use current `somavr_xrsim_smoke` receipts.

## Target tool constraints

`Soma.exe` / `Soma_NoSteam.exe` is x64 / HPL3 / OpenGL. The installed RenderDoc
path rejected SOMA's legacy GL context; use the recorded apitrace GL route for new
captures within authorized runtime scope. `docs/future-hook-map.md` owns the
2026-08-10 matrix receipts (program IDs are capture-local, not stable identifiers).
Inspect one frame for pipeline state; compare at least two controlled states for
camera movement. Manual CLI analysis with qapitrace/glretrace is a supported option.

## Fleet prior art

Before choosing a seam or repeating an experiment, route through
`D:/Dev Debug/VR Modding/docs/bottleneck-map.md` for this project's gate, or
`failure-atlas.md` / `symptom-index.md` for its symptom. Read only the relevant row
and linked section. The project's current receipts outrank a stale fleet summary.
State hypothesis, control, variable and decision rule before experimental code.
Record transferable results in the playbook when they change the route.

Use `D:/Dev Debug/VR Modding/cross-engine-graph/graphify-out/fleet-graph.json`
to find sibling documents: `graphify explain "Frustum and Culling Adjustment" --graph
"<absolute fleet-graph.json path>"` (one command). For queries use engine nouns, not
a long question; try near-duplicate concept names. Inspect graph age and coverage.
Cross-project links are `INFERRED` leads, not confirmation for this target. Open the
named source; its path may be relative to that project's `docs/`, and its label may
be a summary rather than literal text. A miss is not an exhaustive negative result:
fall through to `cross-project-index.md`, scoped `rg`, and sibling failure registries.
External prior art is indexed in the playbook's `sources.yml` / `docs/coverage.md`.

For scripts use `D:/Dev Debug/VR Modding/tools/graphio.py` (`load_graph`): node-link
relationships live under `links`; a defaulted lookup of `edges` can hide real data.
Check the interpreter before running helpers. Rebuild instructions live in
`cross-engine-graph/README.md`; do not rebuild the whole fleet for a routine lookup.


<!-- vr-research-integration:start -->
## Research receipts and playbook contributions

For a substantial VR research finding or failed approach, read
`D:/Dev Debug/VR Modding/docs/research-receipts.md` and use the
`vr-research-receipts` skill when available. `tools/research_receipt.py` in the
playbook creates/validates a project-owned receipt and queues a candidate for review.
Keep validity, fact verdict, baseline health, evidence grade and observation environment
separate. Raw evidence stays in this project; intake never promotes claims or clears
bottlenecks. Use the numerical camera-mapping recipe and `tools/research_checks.py`
when that is the cheapest relevant proof. Existing runtime/launch restrictions apply.
<!-- vr-research-integration:end -->
