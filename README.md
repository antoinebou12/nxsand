# NXSand

[![Build](https://github.com/antoinebou12/nxsand/actions/workflows/native-nro.yml/badge.svg)](https://github.com/antoinebou12/nxsand/actions/workflows/native-nro.yml)
[![Platform](https://img.shields.io/badge/platform-Nintendo%20Switch-E60012)](docs/INSTALL.md)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](https://github.com/antoinebou12/nxsand/blob/main/LICENSE)

**NXSand** is a falling-sand sandbox for Nintendo Switch homebrew. Pour sand, spill water, light fires, grow plants, melt ice, and watch materials react on a live pixel grid. Paint with the touchscreen or Joy-Con, pick materials from a ring, save up to three worlds, and tune performance when you want smoother play on handheld.

The simulation runs on the GPU (OpenGL ES 3.0): a Margolus cellular automaton on ping-pong `GL_R8UI` textures, four fragment passes per step, dirty-rect painting, and a custom OpenGL UI (no ImGui). Desktop builds exist for faster iteration; the Switch `.nro` is the primary target.

## Materials & play

| Material | Behavior (short) |
|----------|------------------|
| Sand | Falls and piles |
| Water | Flows, fills gaps |
| Fire / smoke | Burns and spreads smoke |
| Lava / acid | Hot or corrosive fluids |
| Plant | Grows near water |
| Ice / oil | Cold solid and slick liquid |
| Wall / stone | Static or structural solids |

Reactions follow the rules in `shaders/sim.frag` (for example, lava meeting water can form stone and smoke). Saves use the same JSON + base64 slot layout as the original nxsand web project so worlds stay portable.

## Controls

| Input | Action |
|-------|--------|
| **A** or **ZR** | Paint |
| **B** or **ZL** | Erase |
| **L** / **R** | Brush size |
| **X** | Material ring |
| **+** | Menu (slots, settings) |
| Mouse (desktop) | Left paint · right or Shift+left erase |

## Install on Switch (players)

You need a homebrew-ready Switch ([setup guide](https://switch.hacks.guide/)).

1. Get **`NXSand.nro`** — build it yourself (below) or download the latest **NXSand-nro** artifact from [GitHub Actions](https://github.com/antoinebou12/nxsand/actions/workflows/native-nro.yml) on the `main` branch.
2. Copy the file to the SD card: `sdmc:/switch/NXSand.nro` (folder `switch/` at the card root).
3. Launch from the Homebrew Menu.

Saves live in `sdmc:/switch/nxsand/`. Legacy `sdmc:/switch/nxengine/` data is migrated on first launch when possible. Full install notes: [docs/INSTALL.md](docs/INSTALL.md).

## Build from source

### 1. Install devkitPro

| OS | Install |
|----|---------|
| **Windows** | Download and run the installer from **[devkitPro installer releases](https://github.com/devkitPro/installer/releases)**. Use the **MSYS2** shortcut it adds (e.g. “devkitPro MSYS2”) so `make` and `dkp-pacman` are on your PATH. |
| **Linux / macOS** | Follow **[Getting Started](https://devkitpro.org/wiki/Getting_Started)** on devkitpro.org. |

Set `DEVKITPRO` if your environment does not (the installer usually does).

### 2. Install Switch libraries

In the devkitPro shell:

```bash
dkp-pacman -S switch-dev switch-sdl2 switch-mesa switch-glm switch-freetype switch-harfbuzz
```

### 3. Build the NRO

From the repository root:

```bash
make
```

Output: **`build/NXSand.nro`**. Copy to `sdmc:/switch/NXSand.nro`.

CI runs the same `make` on every push to `main`; see the workflow badge above for status.

### Desktop (optional)

For UI and logic work without a Switch:

```bash
make desktop
./build/NXSand    # or build\NXSand.exe on Windows
make test         # CPU reference tests, no GPU
```

Requires a C++20 toolchain plus SDL2, GLESv2, and FreeType (e.g. MSYS2 MinGW on Windows). Saves go to `./nxsand_save/`; legacy `./nxengine_save/` is migrated when possible.

## Project layout

| Path | Role |
|------|------|
| `source/platform/main.cpp` | Entry, romfs, fatal screen |
| `source/game/app.*` | Frame loop, sim tick, render |
| `source/gpu/sim_pipeline.*` | Ping-pong grid, Margolus passes, GPU brush |
| `source/gpu/render_pipeline.*` | Palette, glow, world draw |
| `shaders/sim.frag` | Cellular automaton rules |
| `shaders/paint.frag` | Dirty-rect brush stamp |
| `shaders/palette_lookup.frag` | Material ID → color |
| `source/save/save.cpp` | JSON save slots |

Deeper write-ups: [docs/NATIVE.md](docs/NATIVE.md) · [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) · [docs/PHYSICS.md](docs/PHYSICS.md) · [docs/INSTALL.md](docs/INSTALL.md)

## Reference

Native C++ / SDL2 port aimed at the same play feel as the nx.js **nxsand** falling-sand prototype (material IDs, menus, Joy-Con-first UX, save format). Behavior is tuned for GPU Margolus steps, not bit-identical CPU frame order.

## License

MIT — see [LICENSE](https://github.com/antoinebou12/nxsand/blob/main/LICENSE).
