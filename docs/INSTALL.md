# Installing NXSand on Nintendo Switch

NXSand ships as a homebrew `.nro`. You need a Switch setup that can run homebrew.

For controls, settings, and architecture, see the [full documentation site](https://antoinebou12.github.io/nxsand/).

## System firmware 22.1.0

Documented setup: **system firmware 22.1.0** with **Atmosphère 1.11.1** and **hekate 6.5.2** (see the [reference versions table](../README.md#reference-versions-june-2026) in the README).

When updating a homebrew Switch:

1. Copy the latest **Atmosphère** release to the microSD (overwrite `atmosphere/`, update **fusee.bin** / your boot payload).
2. Update **hekate** if your pack recommends a newer build for 22.1.0.
3. Run the official **22.1.0** system update only after Atmosphère lists support for that firmware.

Updating official firmware *before* Atmosphère supports it can break CFW until a matching Atmosphère build is installed.

## Build Output

After `make` in a devkitPro shell:

| Artifact | Path |
|----------|------|
| Switch NRO | `build/NXSand.nro` |
| Switch NSP forwarder | `dist/switch/NXSand.nsp` (`make nsp`; optional in CI) |
| Desktop binary | `build/NXSand` (`make desktop`) |

First Switch build: run `powershell -File scripts\gen_icon.ps1` if `romfs/icon.jpg` is missing (letterboxes `assets/nxsand-icon.png` to 256×256 without stretch). Regenerate menu theme and UI SFX with `python scripts/generate-audio.py` (writes `romfs/audio/*.wav`; `make regenerate-audio` is the same).

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

GitHub Actions uploads **NXSand-switch** (`NXSand-switch.zip`) with `switch/NXSand.nro` inside. When the repository secret **`SWITCH_PROD_KEYS`** is configured, the same zip also includes **`switch/NXSand.nsp`**. Unzip at the SD card root so the file lands at `switch/NXSand.nro`, or copy that path manually to `sdmc:/switch/NXSand.nro`.

Tagged releases use **Actions → Release NXSand** (`workflow_dispatch`) and attach three portable zips:

| Zip | Contents |
|-----|----------|
| `NXSand-switch-v*.zip` | `switch/NXSand.nro` (unzip at SD root); `switch/NXSand.nsp` when CI had `SWITCH_PROD_KEYS` |
| `NXSand-linux-v*.zip` | Flat folder: `NXSand`, `shaders/`, `romfs/fonts/`, `run.sh` |
| `NXSand-windows-v*.zip` | Flat folder: `NXSand.exe`, `NXSand-run.bat`, runtime DLLs, `shaders/`, `romfs/fonts/` |

## NSP forwarder (optional)

An NSP forwarder is a tiny installable title that launches the homebrew NRO at **`sdmc:/switch/NXSand.nro`**. You still need the `.nro` on the SD card; the forwarder only adds a Home-menu shortcut.

### Build locally

1. Install [NTON](https://github.com/rlaphoenix/nton): `pip install nton`
2. Place your Switch **`prod.keys`** at `~/.switch/prod.keys` or `./prod.keys` (from Lockpick on your own console).
3. After `make` and `make dist`:

```bash
make nsp
# or: python3 scripts/export-nsp.py
```

Output: **`dist/switch/NXSand.nsp`**. Title ID is fixed at `0100f2c0115b6000` so reinstalling updates the same forwarder.

### CI / releases

Add repository secret **`SWITCH_PROD_KEYS`** (full `prod.keys` file contents). Switch CI jobs run `scripts/export-nsp.py --skip-if-no-keys`; without the secret they still publish the NRO zip only.

### Install on Switch

1. Copy **`NXSand.nro`** to `sdmc:/switch/NXSand.nro` (required).
2. Install **`NXSand.nsp`** with your CFW tool (Tinfoil, DBI, Goldleaf, etc.).
3. Launch **NXSand** from the Home menu. If the forwarder fails, launch the `.nro` from the homebrew menu instead.

Forwarders need a CFW setup that allows unsigned NSP installs. Keep `prod.keys` private; never commit them.

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
- **Build and deploy:** `powershell -File scripts\build-native.ps1` → copy `dist/switch/NXSand.nro` to `sdmc:/switch/NXSand.nro`. Run `powershell -File scripts\verify-nro.ps1` before copying.
- **Confirm the deployed NRO:** delete `sdmc:/switch/nxsand/launch.log`, launch once, and check the latest block includes `build: switch-sim-log-v11`, `switch: sim shader=sim.frag`, `romfs: sim.frag=… sim_rules_body.glsl=…`, `sim shader path: shaders/sim.frag`, `switch: default sim backend=fragment`, and **no** `preferring compute sim`. Recopy from a fresh `build-native.ps1` build if you still see `sim_switch` paths.
- If the app opens then exits, NXSand shows a fatal text screen and writes `sdmc:/switch/nxsand/launch.log` when possible.
- **Why linking felt slow:** full **`sim_rules_body.glsl`** on Mesa can take **many minutes** on first play (same shader as desktop). Mesa has **no program-binary cache**, so every launch recompiles from source.
- **Boot (Switch):** menu loads without sim compile (`switch: boot sim deferred`). First **New Sandbox** / **Load** / **Demo** links **`sim.frag`**. **`compile wait` / `link wait` every ~15s** means progress — do not relaunch or force power off mid-link.
- **No `shader_cache/` on Switch:** the app does **not** create `sdmc:/switch/nxsand/shader_cache/` (GLES program binaries are unsupported on stock Mesa; compile is from source each session). `NXSAND_SHADER_CACHE` applies to desktop only.
- **Frozen sand** (materials render but do not move): Engine → Performance → Sim shader → **Fragment** (Compute may no-op on Mesa). Check `launch.log` for `sim step ok backend=Fragment`.
- **Compute:** toggle in Engine → Performance → Sim shader when `Compute sim: yes` in the log (no env var required). `NXSAND_ENABLE_COMPUTE=1` only forces Compute as default for QA.
- **Shader link progress:** `compile wait` / `link wait: sim.frag Ns` means Mesa is still working.
- **Verbose stderr:** `NXSAND_VERBOSE_LAUNCH_LOG=1` mirrors key lines while testing.
- **Validate a copied log:** `powershell -File scripts\validate-switch-launch-log.ps1 -Path path\to\launch.log`
- Device test checklist: (1) `build: switch-sim-log-v11`, (2) `switch: sim shader=sim.frag`, (3) `boot sim deferred` before menu, (4) first New Sandbox → `link ok: sim.frag` → sand moves, (5) optional Compute toggle in Engine settings.
- Empty or tiny menu text: rebuild with `fonts/NotoSans-Regular.ttf` present (copied into the NRO romfs) or check `launch.log` for `font ready` / `font restored` lines.
- Saves belong in `sdmc:/switch/nxsand/`; the `.nro` belongs in `sdmc:/switch/`.

### Atmosphere crashes after a full Switch reboot

If Atmosphere fails **after a cold boot** (not only when reopening NXSand), it is often SD or forwarder related, not the game binary alone:

1. Deploy the latest **`dist/switch/NXSand.nro`**. In `launch.log` you should see `switch: boot sim deferred` and **no** `boot sim prep: begin` before the menu.
2. Exit NXSand with **HOME** and check the last lines: `stage: shutdown end` then `stage: clean exit`. If those are missing, you may have powered off during shader link — wait for `link ok: sim.frag` on first play instead of force-powering off.
3. If you installed the **NSP forwarder**, remove `sdmc:/atmosphere/contents/` entries for NXSand temporarily and boot again.
4. On the SD card, delete `sdmc:/switch/nxsand/*.tmp` (and remove any legacy `shader_cache/` folder from older builds if present).
5. Reduce log wear: set `NXSAND_LAUNCH_LOG=0` to disable file logging, or let the app rotate `launch.log` when it exceeds 256 KB (`launch.log.old`).
6. If Atmosphere still crashes with NXSand removed from the SD card: check the microSD (Hekate **FATFS check**), update to **Atmosphère 1.11.1** + **hekate 6.5.2** for **firmware 22.1.0**, and capture `sdmc:/atmosphere/crash_reports/` if present.
