# Installing NXSand on Nintendo Switch

NXSand ships as a homebrew `.nro`. You need a Switch setup that can run homebrew.

For controls, settings, and architecture, see the [full documentation site](https://antoinebou12.github.io/nxsand/).

## Build Output

After `make` in a devkitPro shell:

| Artifact | Path |
|----------|------|
| Switch NRO | `build/NXSand.nro` |
| Desktop binary | `build/NXSand` (`make desktop`) |

First Switch build: run `powershell -File scripts\gen_icon.ps1` if `romfs/icon.jpg` is missing.

Install Switch portlibs once:

```bash
(dkp-)pacman -S switch-dev switch-sdl2 switch-mesa switch-glm switch-freetype switch-harfbuzz
```

## Install Methods

### SD Card

1. Copy `build/NXSand.nro` to the microSD under `switch/`, for example `switch/NXSand.nro`.
2. Eject the card, boot the Switch, open Homebrew Menu, and launch **NXSand**.

### FTP From Your PC

1. Install Python 3 and `pip install pyftpdlib`.
2. Run `scripts\serve-nro-ftp.ps1`.
3. Upload `switch/NXSand.nro` to `sdmc:/switch/NXSand.nro`.

Override the FTP port with `$env:NXSAND_FTP_PORT = 2121`. The legacy `$env:NXENGINE_FTP_PORT` is still accepted.

### CI artifact

GitHub Actions uploads **NXSand-switch** (`NXSand-switch.zip`) with `switch/NXSand.nro` inside. Unzip at the SD card root so the file lands at `switch/NXSand.nro`, or copy that path manually to `sdmc:/switch/NXSand.nro`.

Tagged releases use **Actions → Release NXSand** (`workflow_dispatch`) and attach three portable zips:

| Zip | Contents |
|-----|----------|
| `NXSand-switch-v*.zip` | `switch/NXSand.nro` (unzip at SD root) |
| `NXSand-linux-v*.zip` | Flat folder: `NXSand`, `shaders/`, `romfs/fonts/`, `run.sh` |
| `NXSand-windows-v*.zip` | Flat folder: `NXSand.exe`, `NXSand-run.bat`, runtime DLLs, `shaders/`, `romfs/fonts/` |

## Saves

| Platform | Directory |
|----------|-----------|
| Switch | `sdmc:/switch/nxsand/` |
| Desktop | `./nxsand_save/` |

Legacy saves under `sdmc:/switch/nxengine/` or `./nxengine_save/` are migrated forward on first launch when possible. Save files use the nxsand JSON + base64 layout.

## Shaders

Runtime shaders live in `shaders/`. The Switch build copies fragment, vertex, shared GLSL, and compute shaders into `romfs/shaders/` via `make prepare_romfs`.

## Troubleshooting

- The app file must be `sdmc:/switch/NXSand.nro`, not inside the save folder.
- Rebuild with `make` after code or shader changes.
- If the app opens then exits, NXSand shows a fatal text screen and writes `sdmc:/switch/nxsand/launch.log` when possible.
- Saves belong in `sdmc:/switch/nxsand/`; the `.nro` belongs in `sdmc:/switch/`.
