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
- Simulation runtime: **`shaders/sim.frag`** (fragment) or **`shaders/sim.comp`** (GLES 3.1 compute), selectable in Engine → Performance → Sim shader on Switch and desktop. Shared rules in **`shaders/sim_common.glsl`**. `prepare_romfs` copies `sim.comp` into `romfs/shaders/`.

## Architecture (current)

| Area | Location | Notes |
|------|-----------|--------|
| Entry / SDL bootstrap | `source/platform/main.cpp` | Romfs checks include `sim.frag`, `paint.frag` |
| Frame loop | `source/game/app.cpp` | Sim grid from `resolveSimGridSize` + `settings.json` performance; each frame calls `queryDrawableSize(..., settings.display.orientation)`; before world draw call `syncSimForSampling()` then `drawSimulation` |
| Drawable / orientation | `source/platform/screen_size.hpp` | `SDL_GL_GetDrawableSize` + `ScreenOrientation` (Auto / Landscape / Portrait). Prefer `queryDrawableSize` over raw `SDL_GL_GetDrawableSize` wherever UI or input maps window pixels |
| Fragment / compute sim | `source/gpu/sim_pipeline.cpp`, `shaders/sim.frag`, `shaders/sim.comp`, `shaders/sim_common.glsl` | Ping-pong `GL_R8UI`; fragment or compute (GLES 3.1 when supported); 4 Margolus phases; tunables in `physics.json` |
| GPU brush | `shaders/paint.frag` | Dirty-rect fragment stamp with ping-pong copy/swap |
| Render | `source/gpu/render_pipeline.cpp`, `shaders/palette_lookup.frag`, `shaders/upscale.frag`, `shaders/bloom_*.frag` | `uPaletteMode` 0 Pretty / 1 Fast / 2 Classic (green empty bg, palette tex); 3 material IDs when debug HUD on; blob halos (pretty mode); flicker/grain/AO from `settings.json` `visuals` (legacy `render` alias on load; flicker default off if key missing; AO Off = no neighbor shading; Flicker Off = no fire/lava/ember pulse or acid/plant shimmer); optional filtered upscale (`visuals.upscaleFilter`, default nearest); optional bloom (`VisualBloom::Low`: palette → `lookTex`, `bloom_bright` at sim/8, `bloom_blur` ×4 at sim/16, EP01-style `bloom_composite` into `postTex`, then upscale to play region) |
| Input | `source/platform/input/` | Joy-Con-first; Switch face buttons via `switch_face.hpp` (A/B/X/Y, not positional SDL enums on switch-sdl2); pointer mapping uses `queryDrawableSize(..., settings.display.orientation)`; menus: hold D-pad / arrows for repeat navigation and value adjust on Element Settings + Engine Settings slider rows (`menu_repeat`, toggle rows single-step) |
| UI | `source/ui/` | GPU quads, not SDL renderer |
| Perf HUD | `source/ui/perf_overlay.cpp`, `source/gpu/perf_stats.hpp` | FPS, ms breakdown, grid, substeps, fragment passes, brush commands, dirty rect, active-tile fallback, idle sleep |
| Settings | `source/save/settings_io.cpp`, `source/game/game_settings.*` | `settings.json` (engine) + `physics.json` (elements); sim resize/backend deferred one frame after tab exit (`schedulePendingHeavySettingsFlush`); disk flush queued on tab back / menu exit / shutdown (Switch same as desktop, one file per frame) |
| Active tiles | `source/gpu/active_tiles.hpp` | CPU bitset on brush; fragment scissor optimization with full-grid fallback for stability |
| CPU reference | `source/sim/cpu_reference.cpp` | Tests / parity tooling only |
| Materials | `source/sim/materials.hpp`, `shaders/sim_ids.glsl`, `shaders/sim_common.glsl` | 17 brush IDs (1–17) + spawn-only Ember (18); includes steam/glass/wood (12–14), metal/gunpowder (15–16), salt (17). Do not renumber existing IDs in saves. Regenerate `material-reactions.svg` when reactions change. |
| Diagrams | `docs/diagrams/*.mmd`, `docs/DIAGRAMS.md` | Include `material-reactions.mmd`; regenerate SVG when `sim_common.glsl` rules change |

Longer narrative: **`docs/ARCHITECTURE.md`**, **`docs/NATIVE.md`**, **`docs/PHYSICS.md`**.

## Performance priorities (when optimizing)

1. **Measure** on Switch (profiler HUD); desktop GLES is sanity only.
2. **Sim resolution** - Switch boots **Battery Saver 480x270**; Balanced **640x360** when dynamic resolution upgrades; env override `NXSAND_SIM_W` / `NXSAND_SIM_H`.
3. **Substeps** - `effectiveSubsteps` clamps to **1-2** (Battery Saver / Quality preset **1**, Balanced **2**); fewer substeps = fewer passes (trade vs fluid feel).
4. **GPU passes** - keep FBO state restoration and texture sampling barriers boring and explicit.
5. **Shader** - prefer simple `texelFetch` and branch-light 2x2 rules over wide caches or driver-risky constructs; micro-opts only after `docs/SWITCH_PERF_MATRIX.md` shows sim-bound frames.
6. **Brush** - keep dirty-rect GPU stamp path.
7. **Active tiles** - default **Off**; enable Conservative/Aggressive in Engine → Performance when needed. Full-grid fallback when >45% tiles active or too many runs; idle **sim.sleeping** after 30 frames with zero active tiles (populated static scenes OK). **Active tiles Off** sleeps only on an empty grid; compute and fragment skip dispatches while sleeping. **Dynamic resolution** defaults **Off** (optional Engine toggle on Switch/desktop).

Do not default **1280x720** sim on handheld OLED.

## Build commands

- **Switch (devkitPro shell):** `make` -> `build/NXSand.nro`. Requires `DEVKITPRO` set.
- **Windows helper:** `scripts\build-native.ps1`
- **Desktop:** `make desktop` - needs `g++` on PATH and pkg-config (or libs) for SDL2, GLESv2, FreeType. If `g++` is missing, add MSYS2 **MinGW64** `bin` to PATH or set `DESKTOP_CXX` to a full path to `g++.exe`.
- **Unit tests:** `make test` / `make golden` - CPU-only, no GPU. `make test-gpu` - SDL offscreen + GLES `SimPipeline` (upload/paint/step/readback).
- **FTP deploy:** `scripts/serve-nro-ftp.ps1` stages **`dist/switch/NXSand.nro`**.

## CI

- **Build:** `.github/workflows/native-nro.yml` — `make test`, then Switch `make` + `make dist`, artifact `NXSand-switch.zip` (`switch/NXSand.nro`).
- **Release:** `.github/workflows/release.yml` — `workflow_dispatch` with tag; same zip attached to GitHub Release.
- Align artifact paths with README / `docs/INSTALL.md` when they change.

## Documentation touch list

When behavior or paths change, update any of: `README.md`, `docs/INSTALL.md`, `docs/NATIVE.md`, `docs/PHYSICS.md`, `docs/DIAGRAMS.md`, `AGENTS.md`, verify scripts, CI notes.

## Git

Only commit when the user explicitly asks. Do not change `git config` or use destructive git commands unless requested.

## Learned User Preferences

- LAN FTP deploy path for NRO: `scripts/serve-nro-ftp.ps1` -> `dist/switch/NXSand.nro`.
- Match nxsand UX: main menu with no product title or version subtitle; live falling-sand backdrop at the bottom edge only (`menu_sim` through `menu_chrome`); no top header/title panel box behind the list; three slots, Joy-Con-first HUD, material picker, settings; touch on desktop where applicable.
- Engine and Element Settings: hold D-pad or arrow keys on slider rows for repeat adjust (`menu_repeat`); toggle rows stay single-step.
- Desktop play: mouse brush and WASD movement; menu/HUD hints use desktop copy (`ui_copy`), not Switch Joy-Con strings.
- Menu lists and in-play HUD: keep row labels and material hints aligned and vertically centered inside the sim viewport (`PlayRegion`); when fixing layout, verify Switch and desktop.
- Switch menus must use compact safe-area scroll lists; do not draw all rows when they exceed the visible panel.
- Material selection: ring wheel only (`PICKER_MATERIALS`, `material_wheel.hpp`); no grid selector; picker-first (no palette digit hotkeys).
- Switch controls: `switch_face.hpp` for confirm/back/ring; optional `NXSAND_SWITCH_SWAP_FACE_XY` env swaps ring face; menus fully navigable with joystick/D-pad (not touch-only).
- Diagram sources: `docs/diagrams/*.mmd` / `*.puml`; regenerate SVG per `docs/DIAGRAMS.md` when `sim.frag` reactions or sim/render wiring change.
- README: reference-versions table (Atmosphere, Hekate, libnx, SDL2); install/build links to devkitPro Getting Started; `make` as primary build path (no PowerShell helpers in README body); gameplay showcase video at `docs/media/nxsand-showcase.mp4`; no Reference blurbs to nx.js or `E:\nxapplication`.
- Settings saves: changing engine or element settings must not reset the active slot; brush radius changes flush to `settings.json` immediately (slot load uses global `controls.brushRadius`); Back from Engine Performance must not block on GPU sim reinit (defer one frame via `schedulePendingHeavySettingsFlush`); pre-overlay stall is usually GPU heavy apply, not disk; show desktop save-in-progress overlay during disk write.
- Physics feel: wall stays solid (fire/smoke never displace it); water fills trays and pools quickly; plant climbs walls and grows deeper underwater but not from map edges, with relatively slow spread (~0.07 default); relatively slow fire spread onto plant; longer smoke lifetime; lava vitrifies sand into glass; salt floats on water (density 2) and dissolves via `salt_dissolveRate`; packed gunpowder chains and detonates.

## Learned Workspace Facts

- Git remote: https://github.com/antoinebou12/nxsand (GitHub repo name `nxsand`; product/artifact NXSand).
- Git root is the NXEngine workspace folder (no nested `nxsand/` subfolder with its own `.git`).
- Switch: `SDL_GL_GetDrawableSize` can report portrait 720×1280 while the panel is landscape; use `nx::queryDrawableSize` from `screen_size.hpp` for UI, input, and render sizing.
- TPT stamp import: `source/save/tpt_stamp_import.*` with material map in `source/save/tpt_material_map.hpp`; limits and JSON format in `docs/TPT_IMPORT.md` (not full TPT particle physics).
- Desktop GLES compute default: set `NXSAND_ENABLE_COMPUTE=1` when running `make desktop` to build with `NXSAND_ENABLE_COMPUTE_DEFAULT` (Engine → Performance → Sim shader still selects fragment vs compute at runtime).
- Fresh settings default `activeTiles` Off and `dynamicResolution` Off (`game_settings.hpp`); keep off unless the user opts in.
- Water presets target fast tray/pool fill (`water_flowRate` 1.0, `water_levelRate` 0.18, boostedFlow wide ×3 / pocket ×6); see `docs/PHYSICS.md`.
- Wall cells are static: fire and smoke only spread into empty cells and must not displace or erode wall.
- Windows desktop: build/run via `scripts/build-desktop.ps1` and `scripts/run-desktop.ps1` (MSYS2 MinGW64 + ANGLE + GLAD); launch with repo root as cwd; plain `make desktop` from PowerShell or devkitPro MSYS usually fails.
- `settings.json` engine visuals load from `visuals` (legacy `render` alias); flicker defaults off when the key is omitted.
- CI/Linux `make test-gpu`: link `GPU_UNIT_GLAD_C` and `-Ithird_party/glad/include` like desktop; call `nx::gl::load_gl_functions()` after GL context in `tests/gpu_test_gl.cpp`; run from repo root (shaders/); apt `libegl1-mesa-dev`; env `SDL_VIDEODRIVER=offscreen` and `LIBGL_ALWAYS_SOFTWARE=1`; `readGridTo` is top-down (no extra row flip in tests).
