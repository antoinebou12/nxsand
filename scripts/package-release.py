#!/usr/bin/env python3
"""Stage portable NXSand release zips for Switch, Linux, and Windows."""

from __future__ import annotations

import argparse
import os
import re
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path

REQUIRED_SHADERS = (
    "sim.frag",
    "sim.comp",
    "paint.frag",
    "fullscreen.vert",
    "palette_lookup.frag",
    "ui_quad.vert",
    "ui_quad.frag",
    "bloom_bright.frag",
    "bloom_blur.frag",
    "bloom_composite.frag",
    "upscale.frag",
    "sim_common.glsl",
    "sim_ids.glsl",
)

README_LINUX = """NXSand {version} — Linux x86_64 (portable folder)

Run from this directory:
  ./run.sh
  or: ./NXSand

Requires Mesa OpenGL ES (typical desktop):
  sudo apt install libsdl2-2.0-0 libgles2 libegl1 libfreetype6

Saves: ./nxsand_save/
Shaders and font are bundled; launch with this folder as the working directory.
"""

README_WINDOWS = """NXSand {version} — Windows x64 (portable folder)

Run NXSand.exe from this folder (double-click or terminal).
ANGLE GLES DLLs are included; keep all .dll files beside the .exe.

Saves: .\\nxsand_save\\
Shaders and font are bundled; do not move NXSand.exe without the shaders folder.
"""

README_SWITCH = """NXSand {version} — Nintendo Switch

Unzip at the microSD root so the file is: switch/NXSand.nro
Copy to sdmc:/switch/NXSand.nro and launch from the homebrew menu.

Saves: sdmc:/switch/nxsand/
See docs/INSTALL.md in the repository for setup details.
"""


def normalize_version(tag: str) -> str:
    tag = tag.strip()
    return tag[1:] if tag.startswith("v") else tag


def repo_root() -> Path:
    return Path(__file__).resolve().parent.parent


def copy_shaders(src: Path, dst: Path) -> None:
    dst.mkdir(parents=True, exist_ok=True)
    for name in REQUIRED_SHADERS:
        path = src / name
        if not path.is_file():
            raise FileNotFoundError(f"missing shader: {path}")
        shutil.copy2(path, dst / name)


def copy_font(root: Path, staging: Path) -> None:
    font = root / "romfs" / "fonts" / "NotoSans-Regular.ttf"
    if not font.is_file():
        raise FileNotFoundError(f"missing font: {font}")
    out = staging / "romfs" / "fonts"
    out.mkdir(parents=True, exist_ok=True)
    shutil.copy2(font, out / font.name)


def write_readme(staging: Path, text: str) -> None:
    (staging / "README.txt").write_text(text, encoding="utf-8")


def write_run_sh(staging: Path) -> None:
    script = staging / "run.sh"
    script.write_text(
        "#!/bin/sh\n"
        'cd "$(dirname "$0")" || exit 1\n'
        'exec ./NXSand "$@"\n',
        encoding="utf-8",
    )
    script.chmod(0o755)


def _mingw_search_dirs(extra: Path) -> list[Path]:
    dirs: list[Path] = []
    prefix = os.environ.get("MINGW_PREFIX", "")
    if prefix:
        dirs.append(Path(prefix) / "bin")
    for candidate in (
        Path("/mingw64/bin"),
        Path("C:/msys64/mingw64/bin"),
        extra,
    ):
        if candidate.is_dir() and candidate not in dirs:
            dirs.append(candidate)
    return dirs


def _is_windows_system_dll(name: str) -> bool:
    lower = name.lower()
    if not lower.endswith(".dll"):
        return True
    system_prefixes = (
        "api-ms-",
        "ext-ms-",
        "ucrtbase",
        "vcruntime",
        "msvcp",
        "kernel",
        "ntdll",
        "user32",
        "gdi32",
        "shell32",
        "advapi32",
        "ole32",
        "combase",
        "rpcrt4",
        "ws2_32",
        "winmm",
        "imm32",
        "setupapi",
        "shlwapi",
        "crypt32",
        "sechost",
        "bcrypt",
        "cfgmgr32",
        "d3d",
        "dxgi",
        "dxguid",
        "opengl32",
        "wintypes",
        "windows.",
    )
    return any(lower.startswith(p) for p in system_prefixes)


def _resolve_dll_from_ldd(line: str, search_dirs: list[Path]) -> Path | None:
    dll_name = line.split("=>", 1)[0].strip()
    if not dll_name.lower().endswith(".dll") or _is_windows_system_dll(dll_name):
        return None
    match = re.search(r"=>\s+(/[^\s(]+)", line)
    if match:
        resolved = Path(match.group(1))
        if resolved.is_file():
            norm = str(resolved).replace("\\", "/").lower()
            if "/windows/" in norm or norm.startswith("/c/windows"):
                return None
            return resolved
    for directory in search_dirs:
        candidate = directory / dll_name
        if candidate.is_file():
            return candidate
    return None


def gather_windows_dlls(exe: Path, dest: Path) -> None:
    dest.mkdir(parents=True, exist_ok=True)
    for dll in exe.parent.glob("*.dll"):
        shutil.copy2(dll, dest / dll.name)

    search_dirs = _mingw_search_dirs(exe.parent)
    queue = [exe]
    seen: set[str] = set()

    while queue:
        target = queue.pop()
        try:
            proc = subprocess.run(
                ["ldd", str(target)],
                check=False,
                capture_output=True,
                text=True,
            )
        except FileNotFoundError:
            break
        if proc.returncode != 0 and not proc.stdout:
            continue
        for line in proc.stdout.splitlines():
            dll_path = _resolve_dll_from_ldd(line, search_dirs)
            if dll_path is None:
                continue
            dll_name = dll_path.name
            if dll_name in seen:
                continue
            shutil.copy2(dll_path, dest / dll_name)
            seen.add(dll_name)
            queue.append(dest / dll_name)


def zip_dir(folder: Path, zip_path: Path, arc_prefix: Path | None = None) -> None:
    zip_path.parent.mkdir(parents=True, exist_ok=True)
    if zip_path.is_file():
        zip_path.unlink()
    prefix = arc_prefix if arc_prefix is not None else folder.parent
    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as zf:
        for file in sorted(folder.rglob("*")):
            if file.is_file():
                zf.write(file, file.relative_to(prefix))


def verify_zip(zip_path: Path, required_members: list[str]) -> None:
    with zipfile.ZipFile(zip_path, "r") as zf:
        names = set(zf.namelist())
        missing = [m for m in required_members if m not in names]
        if missing:
            raise RuntimeError(f"{zip_path.name} missing entries: {missing}")


def package_switch(root: Path, version: str, out_dir: Path) -> Path:
    nro = root / "build" / "NXSand.nro"
    if not nro.is_file():
        raise FileNotFoundError(nro)
    staging = out_dir / "staging-switch"
    switch_dir = staging / "switch"
    if staging.exists():
        shutil.rmtree(staging)
    switch_dir.mkdir(parents=True)
    shutil.copy2(nro, switch_dir / "NXSand.nro")
    write_readme(staging, README_SWITCH.format(version=version))
    zip_path = out_dir / f"NXSand-switch-v{version}.zip"
    zip_dir(staging, zip_path, arc_prefix=staging)
    verify_zip(zip_path, ["switch/NXSand.nro", "README.txt"])
    print(f"OK: {zip_path}")
    return zip_path


def package_linux(root: Path, version: str, out_dir: Path) -> Path:
    exe = root / "build" / "NXSand"
    if not exe.is_file():
        raise FileNotFoundError(exe)
    folder_name = f"NXSand-linux-v{version}"
    staging = out_dir / folder_name
    if staging.exists():
        shutil.rmtree(staging)
    staging.mkdir(parents=True)
    shutil.copy2(exe, staging / "NXSand")
    (staging / "NXSand").chmod(0o755)
    copy_shaders(root / "shaders", staging / "shaders")
    copy_font(root, staging)
    write_readme(staging, README_LINUX.format(version=version))
    write_run_sh(staging)
    zip_path = out_dir / f"{folder_name}.zip"
    zip_dir(staging, zip_path)
    prefix = f"{folder_name}/"
    verify_zip(
        zip_path,
        [
            f"{prefix}NXSand",
            f"{prefix}run.sh",
            f"{prefix}README.txt",
            f"{prefix}shaders/sim.frag",
            f"{prefix}romfs/fonts/NotoSans-Regular.ttf",
        ],
    )
    print(f"OK: {zip_path}")
    return zip_path


def package_windows(root: Path, version: str, out_dir: Path) -> Path:
    exe = root / "build" / "NXSand.exe"
    if not exe.is_file():
        raise FileNotFoundError(exe)
    folder_name = f"NXSand-windows-v{version}"
    staging = out_dir / folder_name
    if staging.exists():
        shutil.rmtree(staging)
    staging.mkdir(parents=True)
    shutil.copy2(exe, staging / "NXSand.exe")
    gather_windows_dlls(exe, staging)
    copy_shaders(root / "shaders", staging / "shaders")
    copy_font(root, staging)
    write_readme(staging, README_WINDOWS.format(version=version))
    zip_path = out_dir / f"{folder_name}.zip"
    zip_dir(staging, zip_path)
    prefix = f"{folder_name}/"
    verify_zip(
        zip_path,
        [
            f"{prefix}NXSand.exe",
            f"{prefix}SDL2.dll",
            f"{prefix}README.txt",
            f"{prefix}shaders/sim.frag",
            f"{prefix}romfs/fonts/NotoSans-Regular.ttf",
            f"{prefix}libEGL.dll",
            f"{prefix}libGLESv2.dll",
        ],
    )
    print(f"OK: {zip_path}")
    return zip_path


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "platform",
        choices=("switch", "linux", "windows"),
        help="target platform bundle",
    )
    parser.add_argument(
        "--version",
        default="0.0.1",
        help="release version (without leading v)",
    )
    parser.add_argument(
        "--tag",
        default="",
        help="git tag (v0.0.1); overrides --version",
    )
    parser.add_argument(
        "--out-dir",
        default="dist/release",
        help="output directory for zips",
    )
    args = parser.parse_args()
    version = normalize_version(args.tag) if args.tag else normalize_version(args.version)
    root = repo_root()
    out_dir = root / args.out_dir
    out_dir.mkdir(parents=True, exist_ok=True)

    if args.platform == "switch":
        package_switch(root, version, out_dir)
    elif args.platform == "linux":
        package_linux(root, version, out_dir)
    else:
        package_windows(root, version, out_dir)
    return 0


if __name__ == "__main__":
    sys.exit(main())
