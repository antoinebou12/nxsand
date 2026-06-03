#!/usr/bin/env python3
"""Regenerate docs/diagrams/*.svg from *.mmd via Kroki (same backend as uml-mcp)."""
from __future__ import annotations

import json
import sys
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DIAGRAMS = ROOT / "docs" / "diagrams"

PAIRS = [
    ("home-overview.mmd", "home-overview.svg"),
    ("sim-pipeline.mmd", "sim-pipeline.svg"),
    ("sim-margolus-step.mmd", "sim-margolus-step.svg"),
    ("ci-release-flow.mmd", "ci-release-flow.svg"),
    ("material-reactions-overview.mmd", "material-reactions-overview.svg"),
]


def render_mermaid(code: str) -> bytes:
    body = json.dumps({"diagram_source": code, "diagram_type": "mermaid", "output_format": "svg"}).encode()
    req = urllib.request.Request(
        "https://kroki.io/mermaid/svg",
        data=body,
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    with urllib.request.urlopen(req, timeout=120) as resp:
        return resp.read()


def main() -> int:
    for mmd_name, svg_name in PAIRS:
        mmd_path = DIAGRAMS / mmd_name
        svg_path = DIAGRAMS / svg_name
        code = mmd_path.read_text(encoding="utf-8")
        print(f"Rendering {mmd_name} -> {svg_name} ...", flush=True)
        try:
            svg = render_mermaid(code)
        except Exception as exc:
            print(f"  FAILED: {exc}", file=sys.stderr)
            return 1
        svg_path.write_bytes(svg)
        print(f"  OK ({len(svg)} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
