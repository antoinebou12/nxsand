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

## Material Rule List

Rules are intentionally local and stable: each fragment pass reads a 2x2 Margolus block plus direct cardinal neighbors for reactions.

| Material | Movement | Interactions |
|----------|----------|--------------|
| Sand | Falls through empty/gas/liquids and slides diagonally at edges. | Lava fuses it into Stone; Acid can dissolve it. |
| Water | Fast downward liquid, slower sideways spread. | Strongly quenches Fire; Lava + Water creates Stone/Smoke; Ice can freeze it. |
| Fire | Rises as a hot gas, drifts lightly, then becomes Smoke/Empty. | Ignites Plant and Oil; Water/Ice/Acid extinguish it. |
| Smoke | Rises/drifts and fades out. | Ice can condense some smoke back into Water. |
| Wall | Static support. | Acid burns/corrodes it into Empty; Lava can slowly scorch it into Stone. |
| Acid | Corrosive liquid with restrained sideways spread. | Burns Sand, Plant, Ice, Stone, and Wall; Water dilutes some Acid into Smoke. |
| Plant | Static growth material near Water or supported Wall. | Burns near Fire/Lava and is eaten by Acid. |
| Lava | Heavy hot liquid, slower than Water. | Water/Ice quench it to Smoke/Stone; Sand becomes Stone; Oil can ignite nearby. |
| Stone | Heavy granular solid; falls and slides at edges. | Acid burns it away; Lava/Water can crack it into Smoke in small amounts. |
| Oil | Light liquid that floats on Water. | Fire/Lava ignite it; it spreads slower than Water. |
| Ice | Static cold solid. | Fire/Lava melt it to Water; Acid weakens it; freezes nearby Water. |

## Tests

Run `make test` for CPU unit tests covering materials, saves, settings, layout, grid policy, physics parameter serialization, brush-stroke command emission, menu scroll windows, and active-tile bounds. Runtime GPU behavior is validated through shader validation and Switch play testing.
