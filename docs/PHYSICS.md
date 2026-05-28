# Physics And Material Reactions

NXSand runs a 4-phase Margolus cellular automaton over a ping-pong integer grid via `shaders/sim.frag` (fragment, `GL_R8UI`) or `shaders/sim.comp` (compute, `GL_R32UI`). Tunables are uploaded through the `PhysicsBlock` UBO (`source/sim/physics_gpu.hpp`) and edited in **Element Settings** (`physics.json`). **Engine Settings** (`settings.json`) control resolution, palette mode, bloom, flicker, grain, and AO only. Legacy `visuals.glowEnabled` in old saves maps to `bloom: 1` (Low) on load.

OpenGL coordinates increase upward inside the sim texture. UI input, cursor drawing, touch, and paint commands map through the shared `PlayRegion` before converting into grid coordinates.

## Material IDs

| ID | Name | Role |
|----|------|------|
| 0 | Empty | Gas space |
| 1 | Sand | Granular solid |
| 2 | Water | Fluid |
| 3 | Fire | Hot gas |
| 4 | Smoke | Gas |
| 5 | Wall | Static solid |
| 6 | Acid | Corrosive fluid |
| 7 | Plant | Grows with water or wall support |
| 8 | Lava | Hot fluid |
| 9 | Stone | Heavy solid |
| 10 | Oil | Light fluid (floats on water) |
| 11 | Ice | Cold solid |
| 12–13 | Legacy | Treated as empty |

## Movement (Margolus swaps)

| Material | fallChance | flowChance / slide | Density | Static |
|----------|------------|-------------------|---------|--------|
| Sand | 1.0 | powder slide ~0.55–0.70 via `slideChance` | 4 | no |
| Stone | 1.0 | powder slide | 5 | no |
| Water | 1.0 | `water_flowRate` (default 0.85), `water_levelRate` wide spread + pocket fill under ledges (default 0.10) | 3 | no |
| Acid | 0.88 | `acid_flowRate` (default 0.28) | 3 | no |
| Lava | `lava_flowRate+0.45` clamped (default fall ~0.61) | `lava_spreadRate` (default 0.09) | 3 | no |
| Oil | 0.55 | `oil_floatRate` (default 0.16) | 2 | no |
| Fire / Smoke | `fire_speed` | `fire_spreadRate` / `smoke_driftRate`; rise/drift only into **empty** cells (cannot displace wall, stone, plant, or ice) | 0 | no |
| Wall / Plant / Ice | — | ice slow-thaw near water | — | yes |

Liquids spread horizontally into empty or same-or-lighter liquid cells (density layering keeps oil above water). Water also gets a **pocket** boost in `boostedFlow()` when one empty cell lies ahead with solid ground below it. Presets set `water_levelRate` to **0.10** via `applyPerfPresetPhysics` in `source/game/game_settings.cpp`. Powders use `slideChance` in diagonal swaps so sand and stone fall off ledges without horizontal “flow.”

## Interaction matrix

Each row is the **cell being updated** when a cardinal neighbor of the listed type is present. Probabilities are per-frame unless noted.

| Cell | Neighbor | Outcome | Tunable / notes |
|------|----------|---------|-----------------|
| Empty | Plant + water (surface) | Plant | `min(1, plant_growthRate×9)` (default base 0.12) |
| Empty | Plant + water below cell | Plant | `min(1, plant_growthRate×14)` (submerged / deeper) |
| Empty | Plant + in-grid wall | Plant | `min(1, plant_growthRate×4)` if `plant_wallSupport` on (max with water branch) |
| Empty | Plant + wall/plant below | Plant | `min(1, plant_growthRate×7)` wall climb (longer vertical columns) |
| Wall | Acid | Empty | `acid_wallCorrode` (default 0.06) |
| Plant | Acid | Empty | 35% |
| Plant | Fire (4- or 8-neighbor) | Fire | cardinal roll `min(1, fire_ignitePlant×2.5)`; `×0.85` diagonal-only; instant if 2+ cardinal fire |
| Plant | Smoke | Fire | `fire_ignitePlant × 0.40` when plant ignition is enabled |
| Plant | Lava | Fire | always |
| Oil | Fire (1 neighbor) | Fire | `max(oil_igniteRate, fire_igniteOil) × 3.0` |
| Oil | Fire (2+ cardinal) | Fire | ignite rate `×2.5` (cap 1.0) |
| Oil | Lava | Fire | `oil_igniteRate` |
| Water | Lava | Stone or 12% Smoke | quench feedback |
| Water | Fire | Water | extinguish (fire cell reacts separately) |
| Water | Acid | Smoke | 35% |
| Water | Ice | Ice | `ice_freezeRate` |
| Ice | Water | Water | slow adjacency (~0.2%/frame) |
| Lava | Water / Ice | Smoke | |
| Lava | Sand | Stone | |
| Lava | Oil | Fire | `lava_igniteGas` |
| Sand | Lava | Stone | 55% |
| Sand | Acid | Empty | 12% |
| Stone | Acid | Empty | `acid_stoneCorrode` (default 0.045) |
| Stone | Lava + Water | Smoke | 4% |
| Ice | Fire | Water | `ice_meltRate` |
| Ice | Lava | Water | `max(ice_meltRate, 0.08)` |
| Ice | Acid | Water | 20% |
| Acid | Water | Smoke | 25% |
| Acid | Fire / Lava | Smoke | 18% |
| Acid | Wall / Stone | Smoke | 6% fizz |
| Fire | Water / Ice / Acid | Smoke or Empty | 35% smoke |
| Fire | Plant / Oil | Fire | fuel contact suppresses burnout while flame is touching fuel |
| Fire | — | Smoke | `fire_smokeRate` |
| Smoke | Ice | Water | 42% |
| Smoke | Wall | Smoke | linger: `smoke_fadeRate × 0.35`; horizontal drift damped beside wall |
| Smoke | Stone / Sand | Empty | `smoke_fadeRate + 0.05` |
| Smoke | — | Empty | `smoke_fadeRate` (default 0.010); drift `smoke_driftRate` (default 0.12) |

## Balance limits

- Most reactions are **4-neighbor** and **probabilistic**—no global floods in one frame. Plant ignition also checks **8-neighbors** (diagonal fire).
- Acid corrosion is per-cell; pooling against walls increases contact rate via `acid_flowRate`, not instant deletion.
- Fire spreads along plant via `fire_ignitePlant` (default **0.08**, cardinal `×2.5` per-frame roll, instant if two cardinal flame neighbors); oil pools use `fire_igniteOil` / `oil_igniteRate` (defaults **0.045** / **0.07**), with faster spread when two sides touch flame. Fire touching plant or oil burns out more slowly (`fire_smokeRate × 0.35`) so contact persists. Fire and smoke **rise and drift only into empty cells**—they never swap into wall, stone, plant, or ice. Fire has no wall-specific drift or reaction. Walls change only from acid (`acid_wallCorrode`); smoke lingers on wall faces. Water and acid extinguish fire cells.
- Lava + water: water often becomes stone; lava becomes smoke—stylized quench, not full thermodynamics.

## Tuning guide

| File | Contents |
|------|----------|
| `sdmc:/switch/nxsand/physics.json` (desktop: `./nxsand_save/physics.json`) | Element reaction rates |
| `settings.json` | Performance, palette, bloom, flicker, grain, AO, controls, accessibility |

After editing Element Settings, params upload on the next sim frame. Engine settings apply via `App::applyRuntimeSettings()` on load, tab adjust, and shutdown flush. If fire spread on plant or oil still feels slow, raise **Fire → Ignite plant / Ignite oil** or **Oil → Ignite** in Element Settings (existing `physics.json` values override new code defaults until you reset those sliders).

## Diagrams

| Diagram | Source |
|---------|--------|
| Play frame pipeline | `docs/diagrams/sim-pipeline.mmd` |
| Sim substep (4 phases) | `docs/diagrams/sim-margolus-step.mmd` |
| Material reaction graph | `docs/diagrams/material-reactions.mmd` |
| Lava/water quench | `docs/diagrams/reaction-lava-water-quench.puml` |

Full catalog: `docs/DIAGRAMS.md`. Regenerate SVG when `sim.frag` / `sim_common.glsl` rules change.

## TPT reference (import only)

Powder Toy uses a particle engine with pressure and temperature; NXSand stays a 1-byte-per-cell Margolus CA. Optional **stamp import** rasterizes a minimal TPT particle JSON into the grid (see `docs/TPT_IMPORT.md`). Full `.cps` / `GameSave` round-trip is not supported. For how TPT `SimulationConfig.h` performance ideas map to NXSand presets and `water_levelRate`, see **Performance analogy** in [`docs/TPT_IMPORT.md`](TPT_IMPORT.md).

## Tests

Run `make test` for CPU unit tests (materials, saves, settings, layout scroll windows, physics JSON, TPT stamp import). GPU behavior is validated on device and through shader compile checks.

## Reference: GPU-Sand-Sim-Unity (study only)

Companion to [NivMiz’s optimization video](https://www.youtube.com/watch?v=HrrJxkRlRfk): [NivMiz0/GPU-Sand-Sim-Unity](https://github.com/NivMiz0/GPU-Sand-Sim-Unity) (Unity 6, HLSL compute, ~1024×576 demo grid).

| Reference technique | NXSand |
|---------------------|--------|
| Per-pixel GPU parallel sand | Margolus 4-phase `sim.frag` or `sim.comp` (Engine → Performance → Sim backend) |
| Claims grid + atomics | Not used — 2×2 Margolus blocks avoid write races |
| Alternate L/R scan bias | Per-swap `flip` from `rng()` in horizontal rules |
| 2 simulation steps per frame | `effectiveSubsteps` (1–2); Battery Saver preset uses 1 |
| Full-grid dispatch | Active-tile row-run scissor/dispatch with full-grid fallback only for large or invalid active regions |

`sim.comp` uses GLES image load/store on a `GL_R32UI` compute grid and converts material IDs at upload/readback boundaries; no CPU atomics.
