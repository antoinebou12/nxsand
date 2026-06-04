# Agent plans and Context7 MCP

How coding agents should run **Cursor plans** in this repo: use **Context7** for library docs instead of guessing APIs, then verify with `make test` / `scripts/verify-nro.ps1` as in `AGENTS.md`.

## Enable Context7 in Cursor

1. Get an API key from [context7.com/dashboard](https://context7.com/dashboard) (optional but improves rate limits).
2. Set a user or shell env var: `CONTEXT7_API_KEY=...`
3. Project MCP config: [`.cursor/mcp.json`](../.cursor/mcp.json) (remote server `https://mcp.context7.com/mcp`).
4. In Cursor: **Settings → MCP** — confirm `context7` is enabled for this workspace. Reload the window after changing `mcp.json`.

One-shot setup (global): `npx ctx7 setup` per [Context7 CLI docs](https://context7.com/docs/clients/cli).

## MCP tools (always read schemas first)

| Tool | Purpose |
|------|---------|
| `resolve-library-id` | Map a product name → Context7 library ID (e.g. `OpenGL ES` → `/khronosgroup/opengl-registry`) |
| `query-docs` | Ask a focused question against that library ID |

**Workflow:** `resolve-library-id` → pick best match (reputation + benchmark score) → `query-docs` with the plan step as the query. Cap: **3 calls per library** per task.

## NXSand plan → Context7 map

Use this when a plan touches the area below. Pass the **plan step text** as the `query` argument.

| Plan topic | `libraryName` | Preferred library ID | Example `query-docs` questions |
|------------|---------------|----------------------|--------------------------------|
| GLES sim / shaders / link stalls | `OpenGL ES` | `/khronosgroup/opengl-registry` | `KHR_parallel_shader_compile completion query`; `OES_get_program_binary ProgramBinaryOES`; `GLES 3.0 fragment shader image2D` |
| UI / window / GL context | `SDL2` | `/libsdl-org/sdl` | `SDL_GL_SetAttribute OpenGL ES 3`; `SDL_GL_CreateContext`; `SDL_GL_GetDrawableSize` |
| Shader authoring (concepts) | `Learn OpenGL` | `/websites/learnopengl` | `GLSL include files`; `uniform buffer object layout` |
| Cursor plans / MCP wiring | `Cursor` | `/websites/cursor` | `mcp.json workspaceFolder env`; `MCP server configuration` |

**Repo truth wins:** `AGENTS.md`, `docs/DEVELOPMENT.md`, and `shaders/sim_rules_body.glsl` override generic GLES advice (e.g. Switch does **not** use SD `shader_cache/`; unified `sim.frag` path).

## Plan execution checklist

1. Read the plan and `AGENTS.md`; do **not** edit files under `.cursor/plans/`.
2. For each step involving an external API, run **one** Context7 `query-docs` pass before coding.
3. Keep scope minimal; match existing patterns in `source/`.
4. **Verify:** `make test`; Switch changes → `scripts/build-native.ps1` + `scripts/verify-nro.ps1`; optional `scripts/validate-switch-launch-log.ps1` after device run.
5. Update `docs/`, `AGENTS.md`, and verify scripts if behavior or paths change.

## GLES notes (from Khronos registry via Context7)

Relevant to “unify sim shader” / “first link slow on Switch” plans:

- **`OES_get_program_binary` / `ARB_get_program_binary`:** lets the GL act as an offline compiler; `GetProgramBinary` + `ProgramBinary` avoid re-link on **drivers that support it**. Stock Switch Mesa in this project intentionally **does not** rely on this (see `shaderCacheEnabled()` on Switch).
- **`KHR_parallel_shader_compile`:** driver may compile shaders on worker threads; apps can poll completion instead of blocking one long `glLinkProgram`. NXSand uses compile progress callbacks and long Switch link timeouts (see `source/gpu/shader_program.cpp`, `docs/DEVELOPMENT.md`).
- **Expectation:** full `sim_rules_body.glsl` on Mesa can still take **many minutes** per session without program binaries — Context7 does not change that; only SPIR-V/precompiled shipping or a smaller rules body would.

## Cursor MCP reference

Project-level servers live in `.cursor/mcp.json`. Use `${env:VAR}` for secrets (see [Cursor MCP docs](https://cursor.com/docs/mcp)). CLI agents also read the same file ([Cursor CLI](https://cursor.com/docs/cli/using)).
