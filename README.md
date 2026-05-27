# NXSand

Native Nintendo Switch homebrew falling-sand simulator. NXSand uses SDL2 + OpenGL ES 3.0 with a GPU Margolus cellular automaton: ping-pong `GL_R8UI` textures, four fragment passes, dirty-rect fragment painting, and custom OpenGL UI.

## Quick Start

### Desktop

Desktop is for iteration; Switch NRO is the primary target.

```bash
make desktop
./build/NXSand
make test
```

Windows helper: `powershell -File scripts/build-desktop.ps1`.
WSL helper: `bash scripts/build-desktop-wsl.sh`.

Saves go to `./nxsand_save/`. Legacy `./nxengine_save/` is migrated when possible.

### Switch NRO

1. Install devkitPro.
2. Install portlibs:

```bash
(dkp-)pacman -S switch-dev switch-sdl2 switch-mesa switch-glm switch-freetype switch-harfbuzz
```

3. Build:

```bash
make
```

Output: `build/NXSand.nro`. Copy it to `sdmc:/switch/NXSand.nro`.

Windows build helper: `scripts\build-native.ps1`.
LAN FTP deploy helper: `scripts\serve-nro-ftp.ps1`, staging `dist/switch/NXSand.nro`.

Switch saves go to `sdmc:/switch/nxsand/`. Legacy `sdmc:/switch/nxengine/` is migrated on first launch when possible.

## Controls

- `A` or `ZR`: paint
- `B` or `ZL`: erase
- `L` / `R`: brush radius
- `X`: material ring
- `+`: menu
- Desktop mouse/touchpad: left paints, right or Shift+left erases

## Architecture

| Path | Role |
|------|------|
| `source/platform/main.cpp` | Entry, romfs, fatal screen |
| `source/game/app.*` | Scene loop, sim tick, render orchestration |
| `source/gpu/sim_pipeline.*` | Ping-pong `GL_R8UI`, Margolus fragment passes, fragment brush |
| `source/gpu/render_pipeline.*` | Palette LUT, glow, UI passes |
| `source/gpu/font_atlas.*` | Switch shared font / FreeType R8 glyph atlas |
| `source/sim/materials.hpp` | Material IDs and palette |
| `shaders/sim.frag` | Margolus CA rules |
| `shaders/paint.frag` | Dirty-rect GPU brush stamp |
| `shaders/palette_lookup.frag` | Material ID to visible pixel |
| `source/save/save.cpp` | JSON save slots compatible with nxsand layout |

More detail: [docs/NATIVE.md](docs/NATIVE.md), [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md), and [docs/INSTALL.md](docs/INSTALL.md).

## Reference

This project is a native rewrite inspired by the read-only TypeScript / nx.js nxsand project at `E:\nxapplication`. The native port aims for play-feel parity, not bit-identical CPU frame order.
