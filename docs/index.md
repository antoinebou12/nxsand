# NXSand

**NXSand** is a falling-sand sandbox for Nintendo Switch homebrew. Pour sand, spill water, light fires, and watch a **GPU Margolus cellular automaton** run at handheld-friendly resolution. Paint with Joy-Con or touch, pick materials from a ring, save three worlds, and tune performance when you need smoother play.

The primary artifact is **`NXSand.nro`**. Desktop builds (SDL2 + OpenGL ES 3.0 + ANGLE on Windows) exist for faster iteration; Switch hardware remains the performance target.

## Showcase

Gameplay on Nintendo Switch — materials, reactions, and save slots:

**[Watch gameplay video (MP4)](media/nxsand-showcase.mp4)**

<video src="media/nxsand-showcase.mp4" controls width="100%">
  <a href="media/nxsand-showcase.mp4">Download gameplay video</a>
</video>

## Quick links

| I want to… | Start here |
|------------|------------|
| Install on a Switch | [Install on Switch](INSTALL.md) |
| Learn controls | [Controls](CONTROLS.md) |
| Understand materials | [Physics & materials](PHYSICS.md) |
| Build from source | [Native build](NATIVE.md) |
| Read the runtime design | [Architecture](ARCHITECTURE.md) |
| Browse diagrams | [Diagram catalog](DIAGRAMS.md) |

## How it works (short)

![NXSand overview — Switch homebrew, GPU sim, materials](diagrams/home-overview.svg)

Each sim step runs **four Margolus phases** on ping-pong `GL_R8UI` textures. Rules live in `shaders/sim_common.glsl`. Presentation uses a custom OpenGL UI (quads + font atlas) — no ImGui and no `SDL_Renderer` for gameplay.

### Play frame pipeline

![Play frame pipeline](diagrams/sim-pipeline.svg)

### Margolus substep

![Margolus substep — four phases per tick](diagrams/sim-margolus-step.svg)

## Downloads

- **Releases** — [GitHub Releases](https://github.com/antoinebou12/nxsand/releases) (Switch, Linux, and Windows portable zips).
- **CI artifacts** — [native-nro workflow](https://github.com/antoinebou12/nxsand/actions/workflows/native-nro.yml) uploads `NXSand-switch.zip` with `switch/NXSand.nro` (and optional `NXSand.nsp` when `SWITCH_PROD_KEYS` is set).

![CI and release flow](diagrams/ci-release-flow.svg)

## Materials (overview)

Nineteen ring brush materials (IDs 1–17, 20, 22) plus spawn-only **Ember** (18) and **Flower** (19). ID 0 is empty. Legacy save byte **21** (former TNT) loads as empty. **Brick** (22) is a cohesive heavy solid with no chain explosions. Full tables and reaction notes: [Physics & materials](PHYSICS.md). Detailed reaction graph: `diagrams/material-reactions.mmd`.

![Material reactions (overview)](diagrams/material-reactions-overview.svg)

## License

MIT — see [LICENSE](https://github.com/antoinebou12/nxsand/blob/main/LICENSE).
