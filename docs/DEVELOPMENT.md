# Development guide

Notes for contributors and agents working in the NXSand tree (`NXEngine` workspace).

## Repository layout

| Path | Role |
|------|------|
| `source/platform/` | Entry, romfs, fatal screen, input |
| `source/game/` | `App` loop, scenes, settings application |
| `source/gpu/` | `SimPipeline`, `RenderPipeline`, active tiles |
| `source/ui/` | GPU menus, HUD, material ring |
| `source/save/` | Slots, settings I/O, migration |
| `source/sim/` | Material IDs, `PhysicsParams`, CPU reference tests |
| `shaders/` | Sim, paint, palette, bloom, UI |
| `tests/` | CPU and GPU unit tests |
| `docs/` | Markdown sources for this site and GitHub |

Do **not** commit generated `romfs/shaders/` copies; `make prepare_romfs` copies from `shaders/` at build time.

## Build commands

=== "Switch (devkitPro)"

```bash
make              # build/NXSand.nro
make dist         # dist/switch/NXSand.nro
make nsp          # dist/switch/NXSand.nsp (pip install nton; prod.keys)
make test         # CPU reference tests
```

=== "Desktop"

```bash
make desktop      # Linux/macOS with SDL2 + GLES + FreeType on PATH
make test-gpu     # Offscreen SDL + GLES (repo root cwd, shaders/)
```

**Windows:** use `scripts/build-desktop.ps1` and `scripts/run-desktop.ps1` (MSYS2 MinGW64 + GLAD; native GLES first, ANGLE fallback when SDL cannot create a native GLES context). Plain `make desktop` from PowerShell usually fails. After shader edits, run `make test-gpu` from MSYS2 (repo root cwd) to validate compile + sim smoke tests.

Optional: `NXSAND_ENABLE_COMPUTE=1 make desktop` sets compute as the compile-time default; runtime still selects fragment vs compute in Engine → Performance.

## Environment variables

| Variable | Effect |
|----------|--------|
| `NXSAND_SIM_W` / `NXSAND_SIM_H` | Override sim grid size |
| `NXSAND_SHADER_DIR` | Shader search path (desktop) |
| `NXSAND_ENABLE_COMPUTE` | Build-time compute default (desktop make) |
| `NXSAND_BOOT_LOG` | Log desktop/Switch init stage timings to stderr |
| `NXSAND_SHADER_CACHE=0` | Disable GLES program-binary disk cache (`nxsand_save/shader_cache/`) |
| `NXSAND_WARM_SIM=1` | Enable desktop menu idle sim compile after 2 main-menu frames (default: off) |
| `NXSAND_DISABLE_AUDIO=1` | Skip SDL/audout tone audio (debug compile hangs) |
| `SDL_VIDEODRIVER=offscreen` | GPU tests (CI/Linux) |

**Startup:** the main menu shows after render + font + menu backdrop init. Full GPU simulation (`sim.frag` compile and grid FBOs) is deferred until **New**, **Load**, **Demo**, or returning to **Play** — see `App::ensureSimPipelineReady()` in `source/game/app.cpp`. On desktop, sim compile can also start on the main menu after two frames when `NXSAND_WARM_SIM=1` is set. Linked render programs are cached under **`nxsand_save/shader_cache/`** (Switch: **`sdmc:/switch/nxsand/shader_cache/`**); existing sim shader binaries are loaded if present, but sim cache misses link without requesting a new retrievable binary to avoid ANGLE/Switch driver stalls. Cache keys include resolved GLSL source and GPU fingerprint — edit `shaders/` and the next run recompiles automatically.

**First sim compile (desktop ANGLE):** a cache miss on `sim.frag` can take **30–90 seconds** while the driver translates the large rules shader. The boot overlay shows per-shader status and elapsed seconds during **compile** only; **link** runs with UI progress disabled and a clean GL context (`glFinish`, default FBO, no UI program) because repainting the boot screen immediately before `glLinkProgram` can stall ANGLE for minutes. `sim.frag` / `sim.comp` skip `GL_PROGRAM_BINARY_RETRIEVABLE_HINT` on first link; `paint.frag` and render shaders still use the disk cache. Binary cache writes run **after** the sim pipeline is ready. Use `NXSAND_BOOT_LOG=1` to see `[shader] glLinkProgram …` / `program link …ms ok`.

**If compile appears stuck (>2 minutes):** quit, delete `nxsand_save/shader_cache/`, retry with `NXSAND_SHADER_CACHE=0`, or set `NXSAND_DISABLE_AUDIO=1` to rule out SDL audio interfering with ANGLE compile. Check stderr for the last `[shader] fragment compile` / `program link` line. A **120s timeout** shows a toast with recovery hints. Tone audio opens **after** the sim pipeline is ready (not during boot compile). Second launch should log `[shader_cache] sim.frag hit` for paint/render shaders; `sim.frag` may still compile from source on ANGLE.

Legacy `NXENGINE_*` names are still read where noted in source.

## Tests

| Target | What it checks |
|--------|----------------|
| `make test` | CPU Margolus reference, no GPU |
| `make golden` | Golden grid parity |
| `make test-gpu` | `SimPipeline` upload, paint, step, readback (compile + run) |

Linux CI: `libegl1-mesa-dev`, `SDL_VIDEODRIVER=offscreen`, `LIBGL_ALWAYS_SOFTWARE=1`, run from repo root.

## Documentation site

This site uses **Material for MkDocs** (IPC Toolkit–style deep purple theme).

```bash
pip install -r requirements-docs.txt
mkdocs serve    # http://127.0.0.1:8000
mkdocs build    # ./site
```

Published to GitHub Pages on pushes to `main` that touch `docs/`, `mkdocs.yml`, or `requirements-docs.txt`. See [CI & releases](CI.md).

LLM-oriented index: [`llms.txt`](https://antoinebou12.github.io/nxsand/llms.txt) (also at `docs/llms.txt` in the repo; copied to the site root on build).

## Diagrams

Sources live under `docs/diagrams/` (`.mmd`, `.puml`). Regenerate SVGs with **uml-mcp** or Kroki when behavior changes; commit source + SVG together. Catalog: [Diagram catalog](DIAGRAMS.md).

## Agent rules

Project-specific agent instructions: [`AGENTS.md`](https://github.com/antoinebou12/nxsand/blob/main/AGENTS.md) in the repo root.
