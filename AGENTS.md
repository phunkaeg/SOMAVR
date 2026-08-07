## graphify

This project has a knowledge graph at graphify-out/ with god nodes, community structure, and cross-file relationships.

When the user types `/graphify`, invoke the `skill` tool with `skill: "graphify"` before doing anything else.

Rules:
- Use Graphify as the first orientation pass for architecture, feature ownership, and code-archeology questions. Start with `docs/FEATURE_TRACEABILITY.md` nodes for planned VR work, then confirm exact addresses and literals with `rg`, Ghidra, logs, or a debugger.
- Treat `docs/ADDRESS_REGISTRY.md` as the confirmed native-address ledger and `docs/VR_COMPATIBILITY_RE.md` plus `docs/FUTURE_SYSTEMS_RE.md` as the future implementation maps.
- For codebase questions, first run `graphify query "<question>"` when graphify-out/graph.json exists. Use `graphify path "<A>" "<B>"` for relationships and `graphify explain "<concept>"` for focused concepts. These return a scoped subgraph, usually much smaller than GRAPH_REPORT.md or raw grep output.
- Dirty graphify-out/ files are expected after hooks or incremental updates; dirty graph files are not a reason to skip graphify. Only skip graphify if the task is about stale or incorrect graph output, or the user explicitly says not to use it.
- If graphify-out/wiki/index.md exists, use it for broad navigation instead of raw source browsing.
- Read graphify-out/GRAPH_REPORT.md only for broad architecture review or when query/path/explain do not surface enough context.
- After modifying code, run `graphify update .` to keep the graph current (AST-only, no API cost).

## Reverse-engineering MCPs

Ghidra, ReGenny, Frida, Cheat Engine, x64dbg/x32dbg and RenderDoc are all available as MCP tools.
**Load the `re-mcp-toolkit` skill before using any of them** — it carries the preflight calls,
per-tool caveats, and the pairing workflows (Ghidra static offsets -> ReGenny live layout, etc.).

1. **Never assume a tool's host app is running or attached.** The tools always appear in your tool
   list; that says nothing about whether the app behind them is live. Preflight first: Ghidra
   `list_instances`, ReGenny `regenny_status`, Cheat Engine `ping` (then `get_opened_process_id`),
   Frida `enumerate_processes`, x64dbg `get_debugger_status`.
2. **If a tool isn't live, ask the user to launch/attach it.** Don't guess, don't silently skip the
   step, and never fabricate a result you couldn't actually read. If the user has said this session
   that something is running, trust that until a call fails.
3. **Ghidra and ReGenny must be started before Claude Desktop.** Their tools still register when the
   apps are closed, but every call fails. Once running you can connect whenever. Ghidra keeps the
   relevant exes pre-loaded — but a program must be *opened* in the CodeBrowser before
   program-scoped tools work (`No context found for request` = nothing open).
4. **Cheat Engine and x64dbg/x32dbg are not running by default** — they must be requested. CE also
   has to be *attached to the running game*, which `ping` alone does not prove.
5. **RenderDoc only analyses existing `.rdc` captures — it cannot capture or inject.** Request a
   capture for a *named* use case (not a bare "take a capture"); captures live in `RenderDoc/` or
   `Captures/`.
6. Target is `Soma.exe` / `Soma_NoSteam.exe` (64-bit -> **x64dbg** if a debugger is needed).
7. **RenderDoc is unavailable for SOMA** — it runs an OpenGL version incompatible with the installed
   RenderDoc 1.41, so captures cannot be taken. Use Frida / Ghidra / ReGenny instead; do not plan a
   workflow around RenderDoc on this project.
