# AGENTS.md - NXSand

Instructions for coding agents working in this repository. Codex and similar tools read this file for project-specific rules.

## Project identity

- **Repo:** `C:\Users\antoi\NXEngine` (or your clone path).
- **Product:** Nintendo Switch homebrew falling-sand sandbox: **SDL2 + OpenGL ES 3.0**, **fragment GPU simulation**, custom **OpenGL UI** (quads + font atlas), **no ImGui**, **no `SDL_Renderer`** for game or menus.
- **Artifact:** `build/NXSand.nro` (and `dist/switch/NXSand.nro` when copied for deploy).
- **Reference (read-only):** `E:\nxapplication` (nxsand / nx.js). Mirror behavior, UX, material IDs, and JSON+base64 save layout; **never edit** that tree or hardcode provenance paths into source.

## Non-negotiables

- Do not recreate todos from an attached plan; use the existing list and mark items `in_progress` while working.
- Do not edit attached plan files in `.cursor/plans/` when executing a user plan.
- Saves and config live under **`sdmc:/switch/nxsand/`**; desktop dev: **`./nxsand_save/`**. Migrate legacy `nxengine` saves/settings forward when present.
- Do not commit **`romfs/shaders/`** generated copies; `prepare_romfs` copies from `shaders/` at build time.
- Build outputs under **`build/`** and **`dist/`** only.
- Strip porting comment headers (paths into `E:/nxapplication`); keep code self-explanatory.
- Simulation runtime is **`shaders/sim.frag`** (fragment Margolus over `GL_R8UI`). Compute files may remain as disabled/reference material only and must not be copied into romfs.

## Architecture (current)

| Area | Location | Notes |
|------|-----------|--------|
| Entry / SDL bootstrap | `source/platform/main.cpp` | Romfs checks include `sim.frag`, `paint.frag` |
| Frame loop | `source/game/app.cpp` | Sim grid from `resolveSimGridSize` + `settings.json` performance; each frame calls `queryDrawableSize(..., settings.display.orientation)`; before world draw call `syncSimForSampling()` then `drawSimulation` |
| Drawable / orientation | `source/platform/screen_size.hpp` | `SDL_GL_GetDrawableSize` + `ScreenOrientation` (Auto / Landscape / Portrait). Prefer `queryDrawableSize` over raw `SDL_GL_GetDrawableSize` wherever UI or input maps window pixels |
| Fragment sim | `source/gpu/sim_pipeline.cpp`, `shaders/sim.frag` | Ping-pong `GL_R8UI`, Margolus 4 phases, **2x2 blocks rendered through FBOs** |
| GPU brush | `shaders/paint.frag` | Dirty-rect fragment stamp with ping-pong copy/swap |
| Render | `source/gpu/render_pipeline.cpp`, `shaders/palette_lookup.frag` | `uPaletteMode`: pretty / fast / debug; Switch default fast palette (`paletteMode=1`); auto-fast under sim stress |
| Input | `source/platform/input/` | Joy-Con-first; Switch face buttons via `switch_face.hpp` (A/B/X/Y, not positional SDL enums on switch-sdl2); pointer mapping uses `queryDrawableSize(..., settings.display.orientation)` |
| UI | `source/ui/` | GPU quads, not SDL renderer |
| Perf HUD | `source/ui/perf_overlay.cpp`, `source/gpu/perf_stats.hpp` | FPS, ms breakdown, grid, substeps, fragment passes, brush commands, dirty rect, active-tile fallback |
| Settings | `source/save/settings_io.cpp`, `source/game/game_settings.*` | Versioned `settings.json` with display/performance settings. Switch runtime is landscape-only until a real rotated framebuffer transform exists |
| Active tiles | `source/gpu/active_tiles.hpp` | CPU bitset on brush; fragment scissor optimization with full-grid fallback for stability |
| CPU reference | `source/sim/cpu_reference.cpp` | Tests / parity tooling only |
| Diagrams | `docs/diagrams/*.mmd`, `docs/DIAGRAMS.md` | Regenerate SVG when sim/render wiring changes |

Longer narrative: **`docs/ARCHITECTURE.md`**, **`docs/NATIVE.md`**, **`docs/PHYSICS.md`**.

## Performance priorities (when optimizing)

1. **Measure** on Switch (profiler HUD); desktop GLES is sanity only.
2. **Sim resolution** - default Balanced **640x360** handheld; Battery Saver **480x270**; env override `NXSAND_SIM_W` / `NXSAND_SIM_H`.
3. **Substeps** - `effectiveSubsteps` clamps to **1-2** (Balanced preset defaults to 2); fewer substeps = fewer passes (trade vs fluid feel).
4. **GPU passes** - keep FBO state restoration and texture sampling barriers boring and explicit.
5. **Shader** - prefer simple `texelFetch` and branch-light 2x2 rules over wide caches or driver-risky constructs.
6. **Brush** - keep dirty-rect GPU stamp path.
7. **Active tiles** - default stable/off on Switch; any uncertainty must fall back to full-grid sim rather than freezing motion.

Do not default **1280x720** sim on handheld OLED.

## Build commands

- **Switch (devkitPro shell):** `make` -> `build/NXSand.nro`. Requires `DEVKITPRO` set.
- **Windows helper:** `scripts\build-native.ps1`
- **Desktop:** `make desktop` - needs `g++` on PATH and pkg-config (or libs) for SDL2, GLESv2, FreeType. If `g++` is missing, add MSYS2 **MinGW64** `bin` to PATH or set `DESKTOP_CXX` to a full path to `g++.exe`.
- **Unit tests:** `make test` / `make golden` - CPU-only, no GPU.
- **FTP deploy:** `scripts/serve-nro-ftp.ps1` stages **`dist/switch/NXSand.nro`**.

## CI

- Workflow: `.github/workflows/native-nro.yml` - align artifact paths with README when they change.

## Documentation touch list

When behavior or paths change, update any of: `README.md`, `docs/INSTALL.md`, `docs/NATIVE.md`, `docs/PHYSICS.md`, `docs/DIAGRAMS.md`, `AGENTS.md`, verify scripts, CI notes.

## Git

Only commit when the user explicitly asks. Do not change `git config` or use destructive git commands unless requested.

## Learned preferences (concise)

- LAN FTP deploy path for NRO: `scripts/serve-nro-ftp.ps1` -> `dist/switch/NXSand.nro`.
- Match nxsand UX: main menu, three slots, Joy-Con-first HUD, material picker, settings; touch on desktop where applicable.
- Switch menus must use compact safe-area scroll lists; do not draw all rows when they exceed the visible panel.
- Material selection: ring wheel only (`PICKER_MATERIALS`, `material_wheel.hpp`); no grid selector; picker-first (no palette digit hotkeys).
- Switch controls: `switch_face.hpp` for confirm/back/ring; optional `NXSAND_SWITCH_SWAP_FACE_XY` env swaps ring face.
- Diagram sources: `docs/diagrams/*.mmd` / `*.puml`; regenerate SVG per `docs/DIAGRAMS.md` when `sim.frag` reactions or sim/render wiring change.
