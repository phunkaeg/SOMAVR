# SOMAVR — Claude guide

> Also read `AGENTS.md` in this folder — its graphify rules apply here too (kept there to avoid
> duplicating the same content in two files that then drift).

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
8. **For frame capture use apitrace (has an MCP now), not RenderDoc.** apitrace captures legacy GL,
   which RenderDoc rejects. **Proven on SOMA (2026-08-10)** via the apitrace MCP: view-projection at
   `glUniformMatrix4fv(program=822, location=1)` (`camera_moves=true`) and projection at `program=884,
   location=1` (FOV 70°/102°, near 0.03, far 998.67) — see `docs/future-hook-map.md`. Flow:
   `trace_launch(api="gl")` the **vanilla** game (no mod) → move around → `trace_stop` → `find_matrices`
   (scope to one frame) + `track_camera`; `decode_matrix` to check candidates. Manual fallback:
   `apitrace trace` / `qapitrace` / `glretrace --dump-state`. See **`vr-re-workflow`**.
