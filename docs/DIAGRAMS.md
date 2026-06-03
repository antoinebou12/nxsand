# Architecture Diagrams

NXSand keeps editable diagram sources under `docs/diagrams/` beside committed SVG renders. Use **uml-mcp** (`generate_uml` / `generate_uml_batch`) or Kroki to regenerate; see [`diagrams/README.md`](diagrams/README.md) for the full catalog and rename rules.

## Quick index

| Diagram | Source | Use when |
|---------|--------|----------|
| CI / release artifacts | `diagrams/ci-release-flow.mmd` | GitHub Actions → portable zips |
| Launch & save sequence | `diagrams/nxsand-launch-flow.mmd` | Homebrew → app → `nxsand/` persistence |
| Play frame pipeline | `diagrams/sim-pipeline.mmd` | End-to-end CPU/GPU per frame |
| Source modules | `diagrams/source-modules.mmd` | Repo layout and dependencies |
| Scenes (Menu / Play) | `diagrams/game-scenes.mmd` | `App::frame`, menu transitions |
| Save / load | `diagrams/save-load-flow.puml` | `save.cpp` slot JSON + GPU upload |
| Core classes | `diagrams/core-runtime-classes.puml` | `App`, `SimPipeline`, `RenderPipeline` |
| Sim substep | `diagrams/sim-margolus-step.mmd` | Four Margolus phases, active tiles |
| Brush path | `diagrams/brush-input-flow.mmd` | Input → `paint.frag` → `step` |
| Docs home overview | `diagrams/home-overview.mmd` | Switch → GPU sim → materials |
| Material reactions (overview) | `diagrams/material-reactions-overview.mmd` | Grouped overview for docs home |
| Material reactions (full) | `diagrams/material-reactions.mmd` | All shader interaction edges |
| Lava/water quench | `diagrams/reaction-lava-water-quench.puml` | Specific quench branch in sim |

## Runtime path (summary)

GLES 3.0+ presentation is fragment-first:

1. `paint.frag` — dirty-rect brush stamp (ping-pong `GL_R8UI`).
2. `sim.frag` × 4 or `sim.comp` — Margolus phases (`PhysicsBlock` UBO).
3. `palette_lookup.frag` — material ID → color, grid, AO/flicker/grain.
4. Optional bloom: `bloom_bright.frag` → `bloom_blur.frag` → `bloom_composite.frag`.
5. `ui_quad` + font atlas — menus, HUD, perf overlay.

**Engine → Performance → Sim backend → Compute** selects `sim.comp` (GLES 3.1) instead of fullscreen `sim.frag` passes when supported.

## Regeneration checklist

Update sources first, then render SVGs and commit both when behavior changes:

- Sim/render wiring → `sim-pipeline.mmd`, `sim-margolus-step.mmd`
- `sim.frag` / `sim_common.glsl` reactions → `material-reactions.mmd`, `reaction-lava-water-quench.puml`
- Save format or paths → `save-load-flow.puml`, `nxsand-launch-flow.mmd`
- CI / release packaging → `ci-release-flow.mmd`
- New `source/` area or public type → `source-modules.mmd`, `core-runtime-classes.puml`

Do not commit generated copies under `romfs/` (build copies from `shaders/` via `prepare_romfs`).
