# NXSand Native Switch + Desktop

NXSand is a C++ / SDL2 / OpenGL ES homebrew falling-sand game. The shipped Switch runtime uses a GLES 3.0 fragment/vertex pipeline: `GL_R8UI` ping-pong textures, `shaders/sim.frag` for four Margolus phases, `shaders/paint.frag` for dirty-rect brush stamps, and `palette_lookup.frag` for presentation.

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

## Runtime Paths

| Item | Path |
|------|------|
| Switch app | `sdmc:/switch/NXSand.nro` |
| Switch saves | `sdmc:/switch/nxsand/` |
| Desktop saves | `./nxsand_save/` |

On first launch, legacy `nxengine` save folders are copied forward if the new folder is empty.

## Switch Graphics Stack

NXSand still uses SDL2 on Switch to create the window and GL context through switch-mesa/EGL. The game requests OpenGL ES 3.0 because gameplay no longer depends on compute shaders, image load/store, or `GL_R32UI`.

`SimPipeline` keeps the same public API (`init`, `step`, `paintDisk`, `readTexture`, `readGridTo`, `uploadGridTopDown`, `clearAll`) but internally renders fullscreen triangle passes into `GL_R8UI` FBOs.

## Play UI

- `A` or `ZR`: paint
- `B` or `ZL`: erase
- `L` / `R`: brush radius
- `X`: material ring
- `+`: menu

On Switch, orientation is landscape-only until a real rotated framebuffer transform is implemented.

## CI

GitHub Actions builds `build/NXSand.nro`, runs `make test`, and sanity-builds the Linux desktop binary under `build/NXSand`.
