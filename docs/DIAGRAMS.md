# Architecture Diagrams

NXSand keeps editable diagram sources under `docs/diagrams/` beside committed SVG renders. Regenerate diagrams when simulation pass wiring, lava/water rules, or ownership boundaries change.

Preferred sources:

| Diagram | Source |
|---------|--------|
| Play frame pipeline | `docs/diagrams/sim-pipeline.mmd` |
| Lava/water quench | `docs/diagrams/lava-water-reaction.puml` |

The current runtime path is GLES 3.0 fragment based: `paint.frag`, four `sim.frag` Margolus passes, `palette_lookup.frag`, optional glow, then UI quads.

Do not commit generated copies under `romfs/`.
