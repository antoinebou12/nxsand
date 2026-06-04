#!/usr/bin/env python3
"""Build an NSP forwarder for NXSand from build/NXSand.nro (requires nton + prod.keys)."""

from __future__ import annotations

import argparse
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

APP_NAME = "NXSand"
APP_AUTHOR = "antoi"
APP_VERSION_DEFAULT = "0.0.2"
SDMC_NRO_PATH = "/switch/NXSand.nro"
# Unofficial homebrew range; stable across rebuilds so forwarder updates replace in place.
TITLE_ID = "0100f2c0115b6000"


def repo_root() -> Path:
    return Path(__file__).resolve().parent.parent


def nton_output_dir() -> Path:
    return Path.home() / "Desktop" / "NTON"


def resolve_version(explicit: str) -> str:
    if explicit:
        return explicit.strip()
    makefile = repo_root() / "Makefile"
    if makefile.is_file():
        match = re.search(r"^APP_VERSION\s*:?=\s*(\S+)", makefile.read_text(encoding="utf-8"), re.M)
        if match:
            return match.group(1)
    return APP_VERSION_DEFAULT


def write_keys_from_env(env_var: str, dest: Path) -> bool:
    raw = os.environ.get(env_var, "").strip()
    if not raw:
        return False
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_text(raw if raw.endswith("\n") else raw + "\n", encoding="utf-8")
    return True


def resolve_keys_path(keys_file: Path | None, keys_env: str) -> Path | None:
    if keys_file and keys_file.is_file():
        return keys_file.resolve()

    cwd_keys = Path.cwd() / "prod.keys"
    if cwd_keys.is_file():
        return cwd_keys.resolve()

    home_keys = Path.home() / ".switch" / "prod.keys"
    if home_keys.is_file():
        return home_keys.resolve()

    temp_keys = repo_root() / "build" / ".prod.keys"
    if write_keys_from_env(keys_env, temp_keys):
        return temp_keys.resolve()

    return None


def ensure_nton(keys: Path) -> None:
    try:
        import nton  # noqa: F401
    except ImportError as exc:
        raise RuntimeError("nton is not installed (pip install nton)") from exc

    cwd_keys = Path.cwd() / "prod.keys"
    if not cwd_keys.is_file():
        shutil.copy2(keys, cwd_keys)


def nton_executable() -> str:
    found = shutil.which("nton")
    if found:
        return found
    candidate = Path(sys.executable).resolve().parent / "nton"
    if candidate.is_file():
        return str(candidate)
    if sys.platform == "win32" and candidate.with_suffix(".exe").is_file():
        return str(candidate.with_suffix(".exe"))
    raise RuntimeError("nton not found on PATH (pip install nton)")


def run_nton_build(nro: Path, version: str) -> Path:
    cmd = [
        nton_executable(),
        "build",
        str(nro.resolve()),
        "--sdmc",
        SDMC_NRO_PATH,
        "--name",
        APP_NAME,
        "--publisher",
        APP_AUTHOR,
        "--version",
        version,
        "--id",
        TITLE_ID,
    ]
    subprocess.run(cmd, check=True, cwd=repo_root())

    out_dir = nton_output_dir()
    if not out_dir.is_dir():
        raise RuntimeError(f"NTON output directory missing: {out_dir}")

    candidates = sorted(out_dir.glob("*.nsp"), key=lambda p: p.stat().st_mtime, reverse=True)
    if not candidates:
        raise RuntimeError(f"NTON did not write an NSP under {out_dir}")
    return candidates[0]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--nro",
        type=Path,
        default=repo_root() / "build" / "NXSand.nro",
        help="input NRO (default: build/NXSand.nro)",
    )
    parser.add_argument(
        "--out",
        type=Path,
        default=repo_root() / "dist" / "switch" / "NXSand.nsp",
        help="output NSP path (default: dist/switch/NXSand.nsp)",
    )
    parser.add_argument("--version", default="", help="forwarder display version (default: Makefile APP_VERSION)")
    parser.add_argument(
        "--keys",
        type=Path,
        default=None,
        help="prod.keys file (default: ./prod.keys, ~/.switch/prod.keys, or SWITCH_PROD_KEYS env)",
    )
    parser.add_argument(
        "--keys-env",
        default="SWITCH_PROD_KEYS",
        help="environment variable with prod.keys contents (default: SWITCH_PROD_KEYS)",
    )
    parser.add_argument(
        "--skip-if-no-keys",
        action="store_true",
        help="exit 0 when prod.keys are unavailable (CI without secret)",
    )
    args = parser.parse_args()

    nro = args.nro.resolve()
    if not nro.is_file():
        print(f"export-nsp: missing NRO: {nro}", file=sys.stderr)
        return 1

    keys = resolve_keys_path(args.keys, args.keys_env)
    if keys is None:
        msg = (
            "export-nsp: prod.keys not found. Place prod.keys in the repo root or ~/.switch/, "
            f"pass --keys, or set {args.keys_env} (GitHub Actions: repository secret SWITCH_PROD_KEYS)."
        )
        if args.skip_if_no_keys:
            print(msg)
            print("export-nsp: skipping NSP export")
            return 0
        print(msg, file=sys.stderr)
        return 1

    version = resolve_version(args.version)
    ensure_nton(keys)
    built = run_nton_build(nro, version)

    out = args.out.resolve()
    out.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(built, out)
    print(f"OK: {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
