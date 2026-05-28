# TPT stamp import

NXSand simulates a **Margolus cellular automaton** on a `GL_R8UI` grid. [The Powder Toy](https://powdertoy.co.uk/) (TPT) uses floating-point **particles**, pressure, temperature, and walls. This document describes the supported import path and limits.

## Supported input (v1)

Minimal JSON export (desktop dev / tests):

```json
{
  "width": 192,
  "height": 108,
  "particles": [
    { "type": 2, "x": 96.0, "y": 54.0 },
    { "type": 4, "x": 97.0, "y": 54.0 }
  ]
}
```

- `type` is a TPT element ID (`PT_*`).
- `x`, `y` are pixel coordinates in the stamp (top-left origin).
- Import scales the stamp into the target sim resolution and writes a top-down byte grid.

API: `nx::importTptStampJson` in [`source/save/tpt_stamp_import.cpp`](../source/save/tpt_stamp_import.cpp).

## Material mapping

| TPT ID | TPT name (typical) | NXSand |
|--------|-------------------|--------|
| 2, 64, 65 | WATR, DSTW, SLTW | Water |
| 1, 4 | DUST, SAND | Sand |
| 22, 23 | BRMT, STNE | Stone |
| 37 | LAVA | Lava |
| 3 | OIL | Oil |
| 6 | FIRE | Fire |
| 5 | SMKE | Smoke |
| 146 | ACID | Acid |
| 28 | BMTL | Wall |
| 17 | ICEI | Ice |
| 34 | PLNT | Plant |
| other | — | Empty (skipped) |

Mapping table: [`source/save/tpt_material_map.hpp`](../source/save/tpt_material_map.hpp).

## Canonical saves (unchanged)

Slot saves remain **JSON + base64** grid bytes under `sdmc:/switch/nxsand/slot-N.json` (desktop: `./nxsand_save/`). Import produces a grid you can load with `SimPipeline::uploadGridTopDown` or save via the normal slot format.

## Not supported

- Full TPT `.cps` / `GameSave` binary (pressure, velocity, ambient heat, gravity maps)
- Signs, stickmen, fighters, SOAP links, wires, photon channels
- Round-trip export back to TPT
- Per-particle velocity, temperature, or `tmp` fields

## Physics parity note

Shader rules in [`shaders/sim_common.glsl`](../shaders/sim_common.glsl) borrow **ideas** from TPT (liquid leveling, slow ice–water adjacency) but are not a port of `Simulation::UpdateParticles`. Tune via `physics.json` (`water_levelRate`, `ice_freezeRate`, etc.).

## Performance analogy (TPT → NXSand)

[The Powder Toy `SimulationConfig.h`](https://github.com/The-Powder-Toy/The-Powder-Toy/blob/master/src/SimulationConfig.h) optimizes a **particle engine** with separate air/pressure maps. NXSand is a **1-byte-per-cell GPU Margolus CA**—many TPT costs do not apply, but the tuning mindset maps as follows:

| TPT idea | NXSand lever | Where |
|----------|--------------|--------|
| `NPART` / `XRES×YRES` | Sim grid size | [`source/sim/sim_grid_policy.hpp`](../source/sim/sim_grid_policy.hpp), Engine → Performance, `NXSAND_SIM_W` / `NXSAND_SIM_H` |
| `CELL` (coarse aux maps) | Active tiles + idle sleep + brush dirty rect | [`source/gpu/active_tiles.hpp`](../source/gpu/active_tiles.hpp), [`source/game/app.cpp`](../source/game/app.cpp) |
| `flood_water` / wide horizontal spread | `water_levelRate` | [`shaders/sim_common.glsl`](../shaders/sim_common.glsl) `boostedFlow()` |
| Air / velocity integrators (`AIR_*`, `ISTP`, `CFDS`) | *Not implemented* | — |

### Preset cheat sheet (handheld)

| Engine → Performance | Typical sim | Substeps | `water_levelRate` (runtime) |
|----------------------|------------|----------|-------------------------------|
| Battery Saver | 480×270 | 1 | **0.010** (reduced wide/pocket fill) |
| Balanced | 640×360 | 2 | **0.028** (default) |
| Quality | 720×405 | 1 | **0.028** (default) |

Also prefer **Active tiles: Stable fallback**, **Bloom Off**, and **Upscale filter: nearest** when profiling on Switch. Fill measured FPS / sim ms in [`SWITCH_PERF_MATRIX.md`](SWITCH_PERF_MATRIX.md).

Preset physics overrides are **runtime only** (`applyPerfPresetPhysics` in [`source/game/game_settings.cpp`](../source/game/game_settings.cpp)); they do not rewrite `physics.json` until you save from Element Settings.
