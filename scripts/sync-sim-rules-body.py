#!/usr/bin/env python3
"""Split sim_common.glsl into UBO + sim_rules_body.glsl (one body for sim.frag / sim.comp)."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SHADERS = ROOT / "shaders"
src = (SHADERS / "sim_common.glsl").read_text(encoding="utf-8")
lines = src.splitlines(keepends=True)

# Legacy monolithic file: UBO at lines 5-44 (1-based), body from line 46.
if "#include \"sim_rules_body.glsl\"" in src:
    print("sim_common already split; nothing to do")
    raise SystemExit(0)

ubo = "".join(lines[4:44])
body = "".join(lines[45:])

(SHADERS / "sim_rules_body.glsl").write_text(
    "// Shared CA rules body (sim.frag / sim.comp via sim_common.glsl).\n"
    "// Requires: sim_ids.glsl, uint cell(ivec2), uGridSize, uPhase, uFrame, PhysicsBlock.\n\n"
    + body,
    encoding="utf-8",
)

(SHADERS / "sim_common.glsl").write_text(
    "// Shared Margolus CA rules for sim.frag and sim.comp.\n"
    "// Requires: sim_ids.glsl, uGridSize, uPhase, uFrame, PhysicsBlock, uint cell(ivec2 c).\n\n"
    + ubo
    + '\n#include "sim_rules_body.glsl"\n',
    encoding="utf-8",
)

print(f"wrote sim_rules_body.glsl ({len(body.splitlines())} lines)")
