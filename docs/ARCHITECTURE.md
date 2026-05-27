# NXSand Source Layout

NXSand is GPU-first: SDL/libnx owns window and input, while menus, HUD, text, the simulation, and presentation are custom OpenGL.

| Directory | Role |
|-----------|------|
| `source/platform/` | Switch bootstrap, romfs checks, fatal screen |
| `source/platform/input/` | SDL/libnx input to `InputState` |
| `source/game/` | Scene loop, settings, sim tick, render orchestration |
| `source/gpu/` | GL helpers, `SimPipeline`, `RenderPipeline`, font atlas, perf timers |
| `source/sim/` | Materials, physics params, CPU reference tests, brush helpers |
| `source/ui/` | Menu/HUD layout and GPU draw lists |
| `source/save/` | JSON slots, settings, `physics.json`, migration paths |
| `shaders/` | Runtime GLES shaders |

## GPU Frame Graph

1. `paint.frag`: dirty-rect brush stamp into the next `GL_R8UI` texture.
2. `sim.frag` x 4: Margolus phases through fullscreen triangle passes.
3. `palette_lookup.frag`: material ID to visible color/grid/background.
4. `glow_extract.frag` / `glow_blur.frag`: optional glow.
5. `ui_quad.*`: HUD, menus, perf overlay, text atlas quads.

The shipped runtime does not depend on `glDispatchCompute`, `glBindImageTexture`, or `GL_R32UI`.

Shaders are copied into the Switch NRO via `make prepare_romfs` under `romfs/shaders/`.
