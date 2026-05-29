# Switch OLED Performance Matrix

Run on real hardware after deploying `build/NXSand.nro`. Default handheld sim grid is **480×270** in Battery Saver (`substeps=1`, Conservative active tiles), with higher presets available. Override with `NXSAND_SIM_W` and `NXSAND_SIM_H` when profiling.

Record from the in-game perf overlay (Full HUD): **FPS**, **frame ms**, **sim ms**, **grid**, **substeps**, **fragment passes**, **active tiles** (mode/count/fallback), **sleep** when idle, paint dirty rect.

| Scene | 480×270 | 640×360 | 720×405 | 960×540 | 1280×720 |
|-------|--------:|--------:|--------:|--------:|---------:|
| Empty world | | | | | |
| ~10% sand | | | | | |
| Sand + water | | | | | |
| Sand + water + fire | | | | | |
| Brush spam | | | | | |
| Full-screen stress | | | | | |

### Desktop sanity (GLES dev build)

Use `make desktop` and the same benchmark scenes (`Engine → Debug → benchmark scene`). Expect lower sim ms with Conservative tiles on sparse scenes and **sleep** after ~30 idle frames when active tile count reaches zero (settled matter allowed) or when the grid is empty with Active tiles Off. Fill a subset of rows above for regression notes; Switch numbers remain authoritative.

**Active tiles (Off / Stable / Fast):** partial row-runs should hug matter (Debug → show active tiles). Saves wake only occupied tiles; age-out re-wakes last bounds instead of the full grid. Compute and fragment use the same active row-runs. When **sim ms > 20** on the previous frame, desktop caps catch-up to one tick and may drop to **1 substep**; Switch keeps preset substeps.

**Example desktop smoke (fill after local run):**

| Scene | Grid | substeps | passes/frame | sim ms | frame ms | sleep |
|-------|------|----------|--------------|--------|----------|-------|
| Empty | 640×360 | 2 | 0 when sleeping | ~0 | — | yes |
| ~10% sand, Stable | 640×360 | 2 | 4–8 partial | &lt;15 target | &lt;33 | no |
| Settled lava/stone, Stable | 480×270 | 1 | 0 when sleeping | ~0 | — | yes after idle |
| Large bottom sand, Fast | 640×360 | 1–2 | partial or fallback | was ~48 before tile wake fix | — | no |

**Bloom Low** (Engine → Visuals): optional glow extract + 4 blur passes at sim resolution; leave **Off** for Battery Saver profiling. Note one row when testing fire/lava scenes with bloom on.

**Upscale filter** (Engine → Visuals): default **nearest** (single palette pass). Non-nearest filters add a sim-sized RGBA FBO plus `upscale.frag` into the play region — profile **tent** vs **lanczos3** at 480×270 and 640×360 when tuning presentation cost.

Acceptance:

- Minimum: 480×270 or 640×360 at stable **30 fps** handheld.
- Preferred: 480×270 or 640×360 near **60 fps** when scene load allows.
- **1280×720** sim is profiling-only, not the product default.

### TPT-informed physics (2026)

See also **Performance analogy (TPT → NXSand)** in [`TPT_IMPORT.md`](TPT_IMPORT.md). Presets set runtime `water_levelRate` to **0.18** via `applyPerfPresetPhysics` (does not auto-save `physics.json`).

Margolus rules in `shaders/sim_common.glsl` gained:

- **`water_levelRate`** — wide horizontal spread when two empty cells lie ahead (`rand < min(1, level × 3)`), plus pocket fill under ledges (`rand < min(1, level × 6)`; TPT `flood_water` inspiration; no CPU flood-fill).
- **Ice–water adjacency** — very slow ice thaw next to water; smoke+ice condensation tuned.

These add branch-light `cell()` reads only when `water_levelRate > 0` and on reaction checks. Active tiles default **Off**; enable Conservative in Engine settings when needed. Full-grid fallback when row-run area exceeds the safety threshold; **sleep** after 30 idle frames when active tile count is zero (or empty grid with Active tiles Off).

Re-profile sand+water scenes after shader updates; fill the table above if sim ms shifts.

**Post-change note (dev, not Switch):** `water_levelRate` adds at most two extra `cell()` reads per horizontal liquid swap attempt and only when the tunable is &gt; 0. Desktop smoke at 640×360 with ~10% sand+water should stay within the &lt;15 ms sim target on Conservative tiles; no active-tile threshold change was required. Switch OLED numbers still need a hardware fill-in.
