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
| 7 | Plant | Grows with water or wall support; blooms to flower in lush water |
| 8 | Lava | Hot fluid |
| 9 | Stone | Heavy solid |
| 10 | Oil | Light fluid (floats on water) |
| 11 | Ice | Cold solid |
| 12 | Steam | Rising gas; condenses on ice |
| 13 | Glass | Static solid; acid-etchable |
| 14 | Wood | Static solid; burns like plant |
| 15 | Metal | Static solid; acid-corrodible; sparks near fire/lava |
| 16 | Gunpowder | Powder; packed detonations chain through neighbors with blast puffs |
| 17 | Salt | Powder; dissolves in water; floats above water (density 2) |
| 18 | Ember | Spawn-only hot spark from fire/gunpowder; not in material ring |
| 19 | Flower | Spawn-only pink bloom from wet plant; not in material ring |
| 20 | Coal | Powder; slow burn to fire/smoke (no chain detonation) |
| 21 | TNT | Static explosive; contact or heat fuse; ~8-cell blast radius |
| 22 | Brick | Heavy cohesive powder; no ignition; low slide with neighbor cohesion |

## Movement (Margolus swaps)

| Material | fallChance | flowChance / slide | Density | Static |
|----------|------------|-------------------|---------|--------|
| Sand | 1.0 (×`sand_wetSlideScale` when water-adjacent) | powder slide ~0.55–0.70; wet sand uses `slideChanceAt` / `fallChanceAt` | 4 | no |
| Gunpowder | 1.0 | powder slide | 4 | no |
| Coal | 1.0 | powder slide | 4 | no |
| Stone | 1.0 | powder slide | 5 | no |
| Brick | 1.0 | `brick_slideScale` (default 0.30); ×`brick_cohesionScale` when 2+ brick neighbors | 6 | no |
| Water | 1.0 | `water_flowRate` (default 0.80), `water_levelRate` wide spread + pocket fill under ledges (default 0.12; triggers scale ×3 wide / ×6 pocket in `boostedFlow`) | 3 | no |
| Acid | 0.88 | `acid_flowRate` (default 0.28) | 3 | no |
| Lava | `lava_flowRate+0.45` clamped (default fall ~0.61) | `lava_spreadRate` (default 0.09) | 3 | no |
| Oil | 0.46 (×`oil_coldScale` when ice-adjacent) | `oil_floatRate` via `flowChanceAt` (default 0.11) | 2 | no |
| Fire / Smoke / Steam / Ember | `fire_speed` (steam ×0.9 rise) | `fire_spreadRate` / `smoke_driftRate` (ember ×1.2 drift); rise/drift only into **empty** cells | 0 | no |
| Wall / Plant / Flower / Ice / Glass / Wood / Metal / TNT | — | ice slow-thaw near water | — | yes |

Liquids spread horizontally into empty or same-or-lighter liquid cells (density layering keeps oil above water). Water also gets a **pocket** boost in `boostedFlow()` when one empty cell lies ahead with solid ground below it. Wide spread and pocket attempts roll against `min(1, water_levelRate × 3)` and `min(1, water_levelRate × 6)` respectively. Presets set `water_levelRate` to **0.12** via `applyPerfPresetPhysics` in `source/game/game_settings.cpp`. Powders use `slideChance` in diagonal swaps so sand and stone fall off ledges without horizontal “flow.”

## Interaction matrix

Each row is the **cell being updated** when a cardinal neighbor of the listed type is present. Probabilities are per-frame unless noted.

| Cell | Neighbor | Outcome | Tunable / notes |
|------|----------|---------|-----------------|
| Empty | Plant + water (surface) | Plant | `min(1, plant_growthRate×6)` (default base 0.07) |
| Empty | Plant + water below cell | Plant | `min(1, plant_growthRate×9)` (submerged / deeper) |
| Empty | Plant + in-grid wall | Plant | `min(1, plant_growthRate×3)` if `plant_wallSupport` on (max with water branch) |
| Empty | Plant + wall/plant below | Plant | `min(1, plant_growthRate×5)` wall climb (longer vertical columns) |
| Empty | Fire/lava + water | Steam | 20% |
| Wall | Acid | Empty | `acid_wallCorrode` (default 0.06) |
| Glass | Acid | Empty | `acid_wallCorrode × 0.4` |
| Wood | Acid | Empty | 25% |
| Wood | Fire (cardinal) | Fire | `min(1, fire_ignitePlant×2.0)`; instant if 2+ cardinal fire |
| Wood | Lava | Fire | always |
| Wood | Smoke (no fire/lava) | Coal | `wood_charRate` (default 0.04) |
| Wood | Fire (failed ignite roll) | Coal | `wood_charRate × 0.65` |
| Plant | Acid | Empty | 35% |
| Plant | Fire (4- or 8-neighbor) | Fire | cardinal roll `min(1, fire_ignitePlant×2.5)`; `×0.85` diagonal-only; instant if 2+ cardinal fire |
| Plant | Smoke | Plant | smoke does not ignite; fire/lava must touch plant to spread flame |
| Plant | Lush water (3+ wet neighbors) | Flower | `plant_bloomRate` (default 0.035); stronger on open tips |
| Plant | Lava | Fire | always |
| Flower | Acid / fire / lava | same as plant | smoke does not ignite |
| Oil | Fire (1 neighbor) | Fire | `max(oil_igniteRate, fire_igniteOil) × 3.0` |
| Oil | Fire (2+ cardinal) | Fire | ignite rate `×2.5` (cap 1.0) |
| Oil | Lava | Fire | `oil_igniteRate` |
| Water | Lava | Stone or 12% Smoke | quench feedback |
| Water | Fire | Water | extinguish (fire cell reacts separately) |
| Water | Acid | Smoke | 35% |
| Water | Ice | Ice | `ice_freezeRate` |
| Ice | Water | Water | slow adjacency (~0.2%/frame) |
| Lava | Water / Ice | Smoke | |
| Lava | Sand | Glass | vitrify on contact |
| Lava | Oil | Fire | `lava_igniteGas` |
| Sand | Lava | Glass | 55% |
| Sand | Acid | Empty | 12% |
| Sand | Water | Stone | `sand_lithifyRate` (default 0.005) |
| Stone | Acid | Empty | `acid_stoneCorrode` (default 0.045) |
| Stone | Lava + Water | Smoke | 4% |
| Ice | Fire | Water | `ice_meltRate` |
| Ice | Lava | Water | `max(ice_meltRate, 0.08)` |
| Ice | Acid | Water | 20% |
| Acid | Water | Smoke | 25% |
| Acid | Fire / Lava | Smoke | 18% |
| Acid | Wall / Stone | Smoke | 6% fizz |
| Fire | Water / Ice / Acid | Smoke, Steam, or Empty | 15% steam when water; else 35% smoke |
| Fire | Plant / Oil | Fire | fuel contact suppresses burnout while flame is touching fuel |
| Fire | — | Smoke | `fire_smokeRate` |
| Smoke | Ice | Water | 42% |
| Smoke | Wall | Smoke | linger: `smoke_fadeRate × 0.35`; horizontal drift damped beside wall |
| Smoke | Stone / Sand | Empty | `smoke_fadeRate + 0.05` |
| Smoke | — | Empty | `smoke_fadeRate` (default 0.010); drift `smoke_driftRate` (default 0.12) |
| Steam | Ice | Water | 35% condense |
| Steam | — | Empty | `smoke_fadeRate × 1.8` |
| Metal | Acid | Empty | `acid_wallCorrode × 0.25` |
| Metal | Water | Stone | `metal_rustRate` (default 0.003) |
| Metal | Fire + lava (2+ heat neighbors) | Fire | `metal_sparkRate` (default 0.10) |
| Gunpowder | Fire / Lava / chain | Fire or Smoke | ~78–92% ignite; chain through hot neighbors; +`gunpowder_packBoost` when packed; ×`gunpowder_wetIgniteScale` when wet (default 0.20) |
| Empty | Dense gunpowder + heat | Fire or Smoke | Blast ring beside detonating piles |
| Gunpowder | Acid | Empty | 18% |
| Coal | Fire / Lava / Ember | Fire or Smoke | `coal_burnRate` (default **0.025**); ×3.5 near lava; ×0.15 when wet; no chain |
| Coal | Acid | Empty | 12% |
| TNT | Contact (non-empty neighbor except wall/TNT/ember) or heat / chain | Fire, Smoke, or Ember | Contact = instant; blast radius **6**; `tnt_detonateRate` (default **0.92**) |
| TNT | Fire / Lava / Ember / 8-neighbor fire / chain | Fire or Smoke | Heat fuse; pack boosts; separate blast puff; instant when 3+ TNT in 8-neighborhood |
| TNT | Acid | Empty | 8% |
| Sand / water / powders / liquids / wood / glass | Fused TNT within blast radius | Fire, Ember, or Empty | Inner ring scorches; mid ring clears powders/liquids; wall/stone/brick/metal exempt |
| Empty | Fused TNT within blast radius **6** | Fire, Ember, or Smoke | Distance falloff; only evaluated near TNT/fire/ember |
| Brick | Acid | Empty | `acid_stoneCorrode × 0.5` |
| Brick | Lava + Water | Smoke | 4% (same as stone) |
| Acid | Brick | Smoke | 6% fizz (with wall/stone) |
| Smoke | Brick | Empty | `smoke_fadeRate + 0.05` (with stone/sand) |
| Salt | Water | Empty | `salt_dissolveRate` (default **0.035**); ×1.6 with 2 water neighbors, ×2.5 when surrounded |
| Salt | Acid | Empty | 15% |
| Salt | — | Water | Density **2** (lighter than water **3**) — grains float and rise through water |
| Empty | Fire (cardinal) | Ember | `ember_spawnRate` (default **0.004**) |
| Empty | Dense gunpowder + heat | Ember | 12% in blast ring |
| Ember | — | Empty | `ember_fadeRate` (default **0.28**) |
| Wood | Ember (cardinal) | Fire | `ember_igniteWood` (default **0.005**) |

## Balance limits

- Most reactions are **4-neighbor** and **probabilistic**—no global floods in one frame. Empty cells adjacent to fire or lava do **not** spawn smoke or fire directly (only `ember_spawnRate`, gunpowder blast puffs, or TNT blast when `tntBlastStrengthNearby > 0`). Plant ignition also checks **8-neighbors** (diagonal fire), but smoke alone does not ignite plant.
- Acid corrosion is per-cell; pooling against walls increases contact rate via `acid_flowRate`, not instant deletion.
- Fire spreads along plant via `fire_ignitePlant` (default **0.08**, cardinal `×2.5` per-frame roll, instant if two cardinal flame neighbors); smoke does not ignite plant. Oil pools use `fire_igniteOil` / `oil_igniteRate` (defaults **0.045** / **0.07**), with faster spread when two sides touch flame. Oil beside ice spreads and falls more slowly (`oil_coldScale`, default **0.40**). Fire touching plant or oil burns out more slowly (`fire_smokeRate × 0.35`) so contact persists. Fire and smoke **rise and drift only into empty cells**—they never swap into wall, stone, plant, ice, glass, wood, or metal. **Ember** (ID 18) spawns from fire-adjacent empty cells and gunpowder blast puffs; fades quickly (`ember_fadeRate`); rarely ignites wood (`ember_igniteWood`). Not paintable from the material ring. **Gunpowder** ignites from fire/lava or chain-heats through neighboring grains; packed piles add `gunpowder_packBoost` (default **0.12**) and can reach near-instant detonation, with blast smoke/fire in adjacent empty cells. Spark particles render on detonation (`source/ui/sim_fx.cpp`). Water neighbors damp ignition via `gunpowder_wetIgniteScale` (default **0.20**). **Sand** next to water falls and slides more slowly (`sand_wetSlideScale`, default **0.40**) and can lithify into stone (`sand_lithifyRate`, default **0.005**). **Wood** chars to **coal** in smoke without adjacent flame (`wood_charRate`, default **0.04**); slow pyrolysis beside fire can coal before full ignite. **Metal** rusts in water (`metal_rustRate`, default **0.003**); sparks to fire only with two or more heat neighbors (`metal_sparkRate`, default **0.10**). Ice exposed on top gets a brighter snow-cap tint in Pretty palette mode. Walls change only from acid (`acid_wallCorrode`); smoke lingers on wall faces. Water and acid extinguish fire cells.
- **Brick** (ID 22) falls like a heavy powder but uses low `brick_slideScale` and extra cohesion when touching other brick cells—painted stacks keep sharper edges and do not ignite from fire, lava, or chain heat. Tunables in Element Settings → Brick.
- Lava + water: water often becomes stone; lava becomes smoke—stylized quench, not full thermodynamics.

## Tuning guide

| File | Contents |
|------|----------|
| `sdmc:/switch/nxsand/physics.json` (desktop: `./nxsand_save/physics.json`) | Element reaction rates |
| `settings.json` | Performance, `visuals` (palette, bloom, flicker, grain, AO, upscale filter), controls, accessibility. Legacy top-level `render` is merged when `visuals` is absent. Flicker defaults **off** on load if omitted. |

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

## Reference: EP01_SandSim (study only)

Companion to [Game Engineering EP01](https://github.com/GameEngineering/EP01_SandSim) (CPU sequential scan, gunslinger post-process):

| EP01 | NXSand |
|------|--------|
| CPU bottom-up particle scan | GPU Margolus 4-phase `sim.frag` / `sim.comp` |
| Ember particle + velocity | ID **18** gas, probabilistic fade (spawn-only) |
| bright filter → blur → composite | `bloom_bright` → `bloom_blur` ×4 → `bloom_composite` |
| Post-process toggle `b` | Engine → Visuals → Bloom Low |

`sim.comp` uses GLES image load/store on a `GL_R32UI` compute grid and converts material IDs at upload/readback boundaries; no CPU atomics.
