# Diagram Assets

Committed SVG renders plus editable source for regeneration.

| Source | SVG | Documents |
|--------|-----|-----------|
| `sim-pipeline.mmd` | `sim-pipeline.svg` | One play frame: input -> fragment GPU sim -> palette/glow -> drawable |
| `lava-water-reaction.puml` | `lava-water-reaction.svg` | Active lava on water -> smoke + stone (`shaders/sim.frag`) |

Regenerate after changing sim/render pass wiring or lava/water branches in `sim.frag`.
