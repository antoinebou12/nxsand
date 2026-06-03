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
make test         # CPU reference tests
```

=== "Desktop"

```bash
make desktop      # Linux/macOS with SDL2 + GLES + FreeType on PATH
make test-gpu     # Offscreen SDL + GLES (repo root cwd, shaders/)
```

**Windows:** use `scripts/build-desktop.ps1` and `scripts/run-desktop.ps1` (MSYS2 MinGW64 + ANGLE + GLAD). Plain `make desktop` from PowerShell usually fails.

Optional: `NXSAND_ENABLE_COMPUTE=1 make desktop` sets compute as the compile-time default; runtime still selects fragment vs compute in Engine → Performance.

## Environment variables

| Variable | Effect |
|----------|--------|
| `NXSAND_SIM_W` / `NXSAND_SIM_H` | Override sim grid size |
| `NXSAND_SHADER_DIR` | Shader search path (desktop) |
| `NXSAND_ENABLE_COMPUTE` | Build-time compute default (desktop make) |
| `SDL_VIDEODRIVER=offscreen` | GPU tests (CI/Linux) |

Legacy `NXENGINE_*` names are still read where noted in source.

## Tests

| Target | What it checks |
|--------|----------------|
| `make test` | CPU Margolus reference, no GPU |
| `make golden` | Golden grid parity |
| `make test-gpu` | `SimPipeline` upload, paint, step, readback |

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
