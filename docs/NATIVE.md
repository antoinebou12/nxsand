# NXSand Native Switch + Desktop

NXSand is a C++ / SDL2 / OpenGL ES homebrew falling-sand game. Simulation uses `GL_R8UI` ping-pong textures with **`sim.frag`** (fragment) or **`sim.comp`** (compute) Margolus passes, plus `paint.frag` for brush stamps and `palette_lookup.frag` for presentation.

It mirrors the read-only reference project `E:\nxapplication` for save shape, material IDs, menu flow, and Joy-Con-first controls, while using a GPU-native runtime.

## Build

### Nintendo Switch

1. Install devkitPro.
2. Install portlibs:

```bash
(dkp-)pacman -S switch-dev switch-sdl2 switch-mesa switch-glm switch-freetype switch-harfbuzz
```

3. Build from the repo root:

```bash
make
```

Output: `build/NXSand.nro`.

Windows helper: `powershell -File scripts/build-native.ps1`.
FTP deploy helper: `scripts\serve-nro-ftp.ps1`, staging `dist/switch/NXSand.nro`.

### Desktop

Desktop is for faster iteration. It needs SDL2, FreeType, and GLESv2/Mesa or ANGLE.

```bash
make desktop
./build/NXSand
make test
```

Use `NXSAND_SHADER_DIR` to point at a shader folder if running outside the repo root. Legacy `NXENGINE_SHADER_DIR` still works.

Desktop uses the same handheld-first sim grid tiers as Switch (default **640×360** Balanced, not full window resolution). Override with `NXSAND_SIM_W` / `NXSAND_SIM_H` or Engine Settings.

### Desktop controls

| Action | Input |
|--------|--------|
| Menu | Esc |
| Material ring | H or Tab |
| Quick save | F5 |
| Move brush | WASD or arrow keys |
| Paint / erase | Space / Shift, or left / right mouse |
| Brush size | [ ] or mouse wheel |
| Menu navigation | WASD or arrows, Enter, Esc |

## Runtime Paths

| Item | Path |
|------|------|
| Switch app | `sdmc:/switch/NXSand.nro` |
| Switch saves | `sdmc:/switch/nxsand/` |
| Desktop saves | `./nxsand_save/` |

On first launch, legacy `nxengine` save folders are copied forward if the new folder is empty.

## Switch Graphics Stack

NXSand uses SDL2 with OpenGL ES 3.1 when the driver allows it (falls back to 3.0). Simulation rules are shared in **`shaders/sim_common.glsl`** and run as either four **fragment** passes (`sim.frag`) or four **compute** dispatches (`sim.comp`). Pick **Fragment** or **Compute** under **Engine Settings → Performance → Sim shader** (second row) on Switch and desktop.

`SimPipeline` keeps the same public API (`init`, `step`, `paintDisk`, `readTexture`, `readGridTo`, `uploadGridTopDown`, `clearAll`). **Fragment** uses active-tile row-run scissor, and **Compute** uses matching clipped row-run dispatch when active tiles are enabled.

## Play UI

| Action | Switch | Desktop |
|--------|--------|---------|
| Move brush | D-pad / stick | WASD, arrows, or mouse over sandbox |
| Paint | A or ZR | Space / left mouse |
| Erase | B or ZL | Shift / right mouse |
| Brush size | L / R | [ ] or mouse wheel |
| Material ring | X | H or Tab |
| Quick save | Y | F5 |
| Menu | + | Esc |

On Switch, orientation is landscape-only until a real rotated framebuffer transform is implemented.

## CI

GitHub Actions builds `build/NXSand.nro`, runs `make test`, and sanity-builds the Linux desktop binary under `build/NXSand`.
