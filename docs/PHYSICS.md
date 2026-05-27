# Physics And Material Reactions

NXSand runs a 4-phase Margolus cellular automaton in `shaders/sim.frag` using GLES 3.0 fragment passes over a ping-pong `GL_R8UI` material grid. The CPU reference in `E:\nxapplication\src\physics.ts` remains the feel target, not a bit-identical frame-order target.

OpenGL coordinates increase upward inside the sim texture. UI input, cursor drawing, touch, and paint commands are mapped through the shared `PlayRegion` before converting into grid coordinates.

## GPU Neighborhood

Each `simulateCell` fragment loads the neighboring cells it needs from `uSim` with `texelFetch`. The same material IDs are shared with `source/sim/materials.hpp`, and tunables are uploaded through the `PhysicsBlock` UBO defined in `source/sim/physics_gpu.hpp`.

## Material IDs

| ID | Name | Role |
|----|------|------|
| 0 | Empty | Gas space |
| 1 | Sand | Granular solid |
| 2 | Water | Fluid |
| 3 | Fire | Gas; spreads, becomes smoke |
| 4 | Smoke | Gas; fades and rises |
| 5 | Wall | Static solid |
| 6 | Acid | Corrosive fluid |
| 7 | Plant | Grows with water or wall support |
| 8 | Lava | Hot fluid |
| 9 | Stone | Heavy solid |
| 10 | Oil | Floats on water; ignites near heat |
| 11 | Ice | Solid; melts/freezes |
| 12-13 | Legacy | Reserved reference IDs; treated as empty |

## Lava And Water

When active lava sits on water, lava becomes smoke and the water cell becomes stone. This keeps the intended quench gameplay while using Stone instead of the reference browser build's Wall result.

## Tests

Run `make test` for CPU unit tests covering materials, saves, settings, layout, grid policy, physics parameter serialization, and brush-stroke command emission. Runtime GPU behavior is validated through shader validation and Switch play testing.
