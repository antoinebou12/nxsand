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

Do **not** commit generated `romfs/shaders/` copies; `make prepare_romfs` copies from `shaders/` at build time. **`romfs/audio/`** WAVs (menu theme + UI/explosion SFX) are checked in; regenerate with `python scripts/generate-audio.py` or `make regenerate-audio`.

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

Optional: `NXSAND_ENABLE_COMPUTE=1 make desktop` sets compute as the compile-time default; runtime still selects fragment vs compute in Engine → Performance. **Switch and desktop** link the same **`sim.frag`** / **`sim_rules_body.glsl`** (full rules; first Switch link on Mesa can take many minutes). Compute opt-in uses **`sim.comp`** (`NXSAND_ENABLE_COMPUTE=1`; `imageStore` may still be a no-op on stock GLES). Persisted **`performance.simBackend`: Compute** is migrated to Fragment on launch unless compute is forced. `NXSAND_FORCE_FRAGMENT=1` is redundant on Switch but still honored.

## Environment variables

| Variable | Effect |
|----------|--------|
| `NXSAND_SIM_W` / `NXSAND_SIM_H` | Override sim grid size |
| `NXSAND_SHADER_DIR` | Shader search path (desktop) |
| `NXSAND_ENABLE_COMPUTE` | Build-time compute default (desktop make) |
| `NXSAND_BOOT_LOG` | Log desktop/Switch init stage timings to stderr |
| `NXSAND_SHADER_CACHE=0` | Desktop only: disable GLES program-binary disk cache (`nxsand_save/shader_cache/`). Ignored on Switch (cache never used; folder not created). |
| `NXSAND_WARM_SIM=1` | Desktop only: menu idle sim compile after two main-menu frames |
| `NXSAND_DISABLE_AUDIO=1` | Skip SDL/audout tone audio (debug compile hangs) |
| `SDL_VIDEODRIVER=offscreen` | GPU tests (CI/Linux) |

**Startup:** the main menu shows after render + font + menu backdrop init. Full GPU simulation compile and grid FBOs are deferred until **New**, **Load**, **Demo**, or returning to **Play** — see `App::ensureSimPipelineReady()` in `source/game/app.cpp`. On desktop, set `NXSAND_WARM_SIM=1` to opt into menu idle sim compile after two main-menu frames. **Switch** always defers sim compile to first play (`switch: boot sim deferred` in `launch.log`); **`shader_cache/` is never created** on Switch (`shaderCacheEnabled()` is false). First play links **`sim.frag`** under the progress overlay. After sim link, **`RenderPipeline::warmupWorldShaders()`** links palette + upscale under the same overlay. Desktop caches linked programs under **`nxsand_save/shader_cache/`** when the driver supports program binaries.

**Switch first sim compile:** does **not** destroy/recreate the GL context. Path is **`sim.frag`** (same rules as desktop, FBO ping-pong). Optional **`sim.comp`** with `NXSAND_ENABLE_COMPUTE=1` (compute `imageStore` may still no-op on Mesa). Progress in **`launch.log`**: `initSimPipeline: trying fragment`, `link ok: sim.frag`, `sim compile ok`, `sim step ok backend=Fragment`. Heartbeat: `link wait: sim.frag Ns`.

**Switch hardware smoke test:** deploy `dist/switch/NXSand.nro`. Expect `build: switch-sim-log-v11`, `switch: sim shader=sim.frag`, `sim shader path: shaders/sim.frag`, `boot sim deferred` before menu, then first New Sandbox → `link ok: sim.frag` → sand falls. Engine → Performance → Sim shader toggles Fragment/Compute when supported.

**First sim compile (desktop ANGLE):** a cache miss on `sim.frag` can take **30–90 seconds** while the driver translates the large rules shader. Desktop may reset the GL context before sim link; **link** runs with UI progress disabled during link because repainting immediately before `glLinkProgram` can stall ANGLE for minutes. Desktop does **not** persist `sim.frag` / `sim.comp` binaries (paint and render shaders still use the disk cache). Use `NXSAND_BOOT_LOG=1` to see `[shader] glLinkProgram …` / `program link …ms ok`.

**If compile appears stuck:** check **`launch.log`** for `link wait: sim.frag Ns` (still linking). Sand visible but frozen on Switch with `initSimPipeline: trying compute` and no init failure: compute `imageStore` no-op — use default Fragment or delete `settings.json` so migration runs. Set **`NXSAND_FORCE_FRAGMENT=1`** only when overriding a desktop compute default. Desktop: stderr last `[shader]` line, or set `NXSAND_DISABLE_AUDIO=1` to rule out SDL audio vs ANGLE; delete **`nxsand_save/shader_cache/`** after shader updates if needed. Compile/link **timeout**: desktop **120s**, Switch **300s** when `KHR_parallel_shader_compile` is available; on Switch, a failed Compute attempt reports the error instead of automatically falling into Fragment unless Fragment was explicitly forced.

### Romfs audio

| File | Use |
|------|-----|
| `romfs/audio/menu_theme.wav` | Main-menu ambient loop (~36 s, seamless) |
| `romfs/audio/ui_*.wav` | Menu confirm/back/nav/material one-shots |
| `romfs/audio/explosion_*.wav` | Gunpowder detonation cues |

All WAVs are 48 kHz stereo 16-bit PCM. Desktop streams via **SDL2 `SDL_QueueAudio`**; Switch uses libnx **audout** with a 3-buffer ring. Menu theme plays on the main menu only; **Play** stops it. Engine → **Audio** tab: **Volume** Off/Low/Medium/High and **Menu music** On/Off (saved in `settings.json` under `audio`). Replace WAVs without code changes after regenerating or editing sources in `scripts/generate-audio.py`.

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

## Agent rules and plans

Project-specific agent instructions: [`AGENTS.md`](https://github.com/antoinebou12/nxsand/blob/main/AGENTS.md) in the repo root.

**Cursor plans + Context7 MCP:** when executing a plan, use the Context7 MCP server ([`.cursor/mcp.json`](../.cursor/mcp.json), env `CONTEXT7_API_KEY`) to resolve library IDs and query current docs before changing GLES/SDL code. Workflow, library map, and verification checklist: [Agent plans and Context7 MCP](AGENT_PLANS.md).
