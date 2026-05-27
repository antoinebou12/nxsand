# Physics And Material Reactions

NXSand runs a 4-phase Margolus cellular automaton in `shaders/sim.frag` using GLES 3.0 fragment passes over a ping-pong `GL_R8UI` material grid. Tunables are uploaded through the `PhysicsBlock` UBO (`source/sim/physics_gpu.hpp`) and edited in **Element Settings** (`physics.json`). **Engine Settings** (`settings.json`) control resolution, palette, glow, flicker, grain, and AO only.

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
| Water | 0.96 | `water_flowRate` (default 0.38) | 3 | no |
| Acid | 0.88 | `acid_flowRate` (default 0.28) | 3 | no |
| Lava | `lava_flowRate+0.45` clamped | `lava_spreadRate` | 3 | no |
| Oil | 0.72 | `oil_floatRate` (default 0.19) | 2 | no |
| Fire / Smoke | `fire_speed` | `fire_spreadRate` / `smoke_driftRate` | 0 | no |
| Wall / Plant / Ice | — | — | — | yes |

Liquids spread horizontally into empty or same-or-lighter liquid cells (density layering keeps oil above water). Powders use `slideChance` in diagonal swaps so sand and stone fall off ledges without horizontal “flow.”

## Interaction matrix

Each row is the **cell being updated** when a cardinal neighbor of the listed type is present. Probabilities are per-frame unless noted.

| Cell | Neighbor | Outcome | Tunable / notes |
|------|----------|---------|-----------------|
| Empty | Plant + support | Plant | `plant_growthRate`, `plant_wallSupport` |
| Wall | Acid | Empty | `acid_wallCorrode` (default 0.06) |
| Wall | Lava | Stone | hardcoded 1.5% |
| Plant | Acid | Empty | 35% |
| Plant | Fire | Fire | `fire_ignitePlant` (default 0.10) |
| Plant | Lava | Fire | always |
| Oil | Fire | Fire | `max(oil_igniteRate, fire_igniteOil)` |
| Oil | Lava | Fire | `oil_igniteRate` |
| Water | Lava | Stone or 12% Smoke | quench feedback |
| Water | Fire | Water | extinguish (fire cell reacts separately) |
| Water | Acid | Smoke | 35% |
| Water | Ice | Ice | `ice_freezeRate` |
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
| Fire | Plant | Smoke | `fire_ignitePlant * 0.5` (fuel) |
| Fire | — | Smoke | `fire_smokeRate` |
| Smoke | Ice | Water | 40% |
| Smoke | — | Empty | `smoke_fadeRate` |

## Balance limits

- All reactions are **local** (4-neighbor) and **probabilistic**—no global floods in one frame.
- Acid corrosion is per-cell; pooling against walls increases contact rate via `acid_flowRate`, not instant deletion.
- Fire spreads along plant via `fire_ignitePlant`; water and acid extinguish fire cells.
- Lava + water: water often becomes stone; lava becomes smoke—stylized quench, not full thermodynamics.

## Tuning guide

| File | Contents |
|------|----------|
| `sdmc:/switch/nxsand/physics.json` (desktop: `./nxsand_save/physics.json`) | Element reaction rates |
| `settings.json` | Performance, palette, glow, flicker, grain, AO, controls, accessibility |

After editing Element Settings, params upload on the next sim frame. Engine settings apply via `App::applyRuntimeSettings()` on load, tab adjust, and shutdown flush.

## Diagrams

| Diagram | Source |
|---------|--------|
| Play frame pipeline | `docs/diagrams/sim-pipeline.mmd` |
| Material reaction graph | `docs/diagrams/material-reactions.mmd` |
| Lava/water quench | `docs/diagrams/lava-water-reaction.puml` |

Regenerate SVG per `docs/DIAGRAMS.md` when `sim.frag` rules change.

## Tests

Run `make test` for CPU unit tests (materials, saves, settings, layout scroll windows, physics JSON). GPU behavior is validated on device and through shader compile checks.
