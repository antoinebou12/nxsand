# Switch OLED Performance Matrix

Run on real hardware after deploying `build/NXSand.nro`. Default handheld sim grid is 480x270 in Battery Saver, with higher presets available. Override with `NXSAND_SIM_W` and `NXSAND_SIM_H` when profiling.

| Scene | 480x270 | 640x360 | 720x405 | 960x540 | 1280x720 |
|-------|--------:|--------:|--------:|--------:|---------:|
| Empty world | | | | | |
| ~10% sand | | | | | |
| Sand + water | | | | | |
| Sand + water + fire | | | | | |
| Brush spam | | | | | |
| Full-screen stress | | | | | |

Acceptance:

- Minimum: 480x270 or 640x360 at stable 30 fps handheld.
- Preferred: 480x270 or 640x360 near 60 fps when scene load allows.
- 1280x720 sim is profiling-only, not the product default.

Use the in-game perf overlay: FPS, frame ms, sim ms, grid size, fragment passes, paint state, radius, material, and command count.
