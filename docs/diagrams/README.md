# Diagram Assets

Editable sources and committed SVG renders for NXSand architecture and game logic. Regenerate when simulation, save format, scene flow, or module boundaries change.

## Regenerate (uml-mcp / Kroki)

From the repo root, use the Cursor **uml-mcp** `generate_uml` tool (or Kroki / Mermaid Live / PlantUML locally). Pass `output_dir` as `docs/diagrams` and rename outputs to match the **SVG** column below. Mermaid sources use `diagram_type: mermaid`; PlantUML sources use `diagram_type: plantuml`.

## Catalog

| Topic | Source | SVG |
|-------|--------|-----|
| Play frame GPU path | `sim-pipeline.mmd` | `sim-pipeline.svg` |
| Source module map | `source-modules.mmd` | `source-modules.svg` |
| Menu / Play scenes | `game-scenes.mmd` | `game-scenes.svg` |
| Save / load slots | `save-load-flow.puml` | `save-load-flow.svg` |
| Core C++ types | `core-runtime-classes.puml` | `core-runtime-classes.svg` |
| One sim substep (4 phases) | `sim-margolus-step.mmd` | `sim-margolus-step.svg` |
| Brush input → GPU stamp | `brush-input-flow.mmd` | `brush-input-flow.svg` |
| Material reaction graph (16 mats) | `material-reactions.mmd` | `material-reactions.svg` |
| Lava + water quench | `reaction-lava-water-quench.puml` | `reaction-lava-water-quench.svg` |

Narrative docs: [`../ARCHITECTURE.md`](../ARCHITECTURE.md), [`../DIAGRAMS.md`](../DIAGRAMS.md), [`../PHYSICS.md`](../PHYSICS.md).

## When to update

- **`sim-pipeline.mmd`**, **`sim-margolus-step.mmd`**: sim/render pass wiring, compute backend, active-tile scissor rules.
- **`material-reactions.mmd`**, **`reaction-lava-water-quench.puml`**: `shaders/sim.frag` / `sim_common.glsl` reaction branches.
- **`save-load-flow.puml`**: `source/save/save.cpp` JSON fields or paths.
- **`game-scenes.mmd`**, **`brush-input-flow.mmd`**: menu flow, input mapping, `PlayRegion`.
- **`core-runtime-classes.puml`**, **`source-modules.mmd`**: new top-level modules or ownership changes.
