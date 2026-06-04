#!/usr/bin/env python3
"""Crop letterboxed icon art to 256x256 JPEG for hbmenu / elf2nro --icon."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

import numpy as np
from PIL import Image


def _is_dark(rgb: np.ndarray, threshold: int = 40) -> np.ndarray:
    return (rgb[..., 0] <= threshold) & (rgb[..., 1] <= threshold) & (rgb[..., 2] <= threshold)


def _is_letterbox(rgb: np.ndarray) -> np.ndarray:
    r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
    return (r >= 210) & (g >= 175) & (b >= 115) & (np.abs(r.astype(np.int16) - g.astype(np.int16)) <= 40)


def _artwork_bbox(rgb: np.ndarray) -> tuple[int, int, int, int]:
    mask = ~_is_dark(rgb, 50)
    if mask.sum() < 16:
        h, w = rgb.shape[:2]
        return 0, 0, w - 1, h - 1
    ys, xs = np.where(mask)
    return int(xs.min()), int(ys.min()), int(xs.max()), int(ys.max())


def _square_crop(rgb: np.ndarray, x0: int, y0: int, x1: int, y1: int) -> np.ndarray:
    side = max(x1 - x0 + 1, y1 - y0 + 1)
    cx = (x0 + x1) // 2
    cy = (y0 + y1) // 2
    h, w = rgb.shape[:2]
    left = max(0, cx - side // 2)
    top = max(0, cy - side // 2)
    if left + side > w:
        left = max(0, w - side)
    if top + side > h:
        top = max(0, h - side)
    side = min(side, w - left, h - top)
    return rgb[top : top + side, left : left + side]


def _black_column_runs(dark_frac: np.ndarray, threshold: float = 0.9) -> list[tuple[int, int]]:
    black = dark_frac >= threshold
    runs: list[tuple[int, int]] = []
    start: int | None = None
    for i, is_black in enumerate(black):
        if is_black and start is None:
            start = i
        elif not is_black and start is not None:
            runs.append((start, i - 1))
            start = None
    if start is not None:
        runs.append((start, len(black) - 1))
    return runs


def _inner_icon_span(dark_frac: np.ndarray, length: int) -> tuple[int, int]:
    runs = _black_column_runs(dark_frac)
    inner = [(a, b) for a, b in runs if a > 4 and b < length - 5]
    if len(inner) >= 2:
        left = [r for r in inner if r[1] < length // 2]
        right = [r for r in inner if r[0] > length // 2]
        if left and right:
            return left[-1][1] + 1, right[0][0] - 1
    if len(runs) >= 2 and runs[0][0] <= length // 4 and runs[-1][1] >= (3 * length) // 4:
        return runs[0][1] + 1, runs[-1][0] - 1
    mask = dark_frac < 0.55
    if mask.sum() < 8:
        return 0, length - 1
    return int(np.where(mask)[0][0]), int(np.where(mask)[0][-1])


def crop_icon_array(rgb: np.ndarray) -> np.ndarray:
    letterbox = _is_letterbox(rgb)
    row_lb = letterbox.mean(axis=1) > 0.85
    if row_lb.any() and (~row_lb).any():
        y0 = int(np.where(~row_lb)[0][0])
        y1 = int(np.where(~row_lb)[0][-1])
        band = rgb[y0 : y1 + 1]
    else:
        band = rgb

    bh, bw = band.shape[:2]
    mid_rows = band[bh // 5 : 4 * bh // 5]
    mid_cols = band[:, bw // 5 : 4 * bw // 5]
    col_dark = _is_dark(mid_rows).mean(axis=0)
    row_dark = _is_dark(mid_cols).mean(axis=1)
    cx0, cx1 = _inner_icon_span(col_dark, bw)
    cy0, cy1 = _inner_icon_span(row_dark, bh)
    frame = band[cy0 : cy1 + 1, cx0 : cx1 + 1]

    ax0, ay0, ax1, ay1 = _artwork_bbox(frame)
    art = _square_crop(frame, ax0, ay0, ax1, ay1)

    side = max(art.shape[0], art.shape[1])
    ch, cw = art.shape[:2]
    pad = np.zeros((side, side, 3), dtype=np.uint8)
    py = (side - ch) // 2
    px = (side - cw) // 2
    pad[py : py + ch, px : px + cw] = art
    return np.array(Image.fromarray(pad).resize((256, 256), Image.Resampling.LANCZOS))


def crop_icon_file(src: Path, dst_jpg: Path, dst_png: Path | None = None) -> None:
    rgb = np.array(Image.open(src).convert("RGB"))
    out = crop_icon_array(rgb)
    img = Image.fromarray(out)
    dst_jpg.parent.mkdir(parents=True, exist_ok=True)
    img.save(dst_jpg, format="JPEG", quality=94)
    if dst_png is not None:
        dst_png.parent.mkdir(parents=True, exist_ok=True)
        img.save(dst_png, format="PNG")
    print(f"Wrote {dst_jpg}" + (f" and {dst_png}" if dst_png else ""))


def main(argv: list[str] | None = None) -> int:
    root = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "src",
        nargs="?",
        type=Path,
        default=root / "romfs" / "icon.jpg",
        help="letterboxed icon source (default: romfs/icon.jpg)",
    )
    parser.add_argument(
        "--out",
        type=Path,
        default=root / "romfs" / "icon.jpg",
        help="256x256 JPEG output (default: romfs/icon.jpg)",
    )
    parser.add_argument(
        "--png",
        type=Path,
        default=root / "data" / "icon-source.png",
        help="optional PNG copy (default: data/icon-source.png)",
    )
    args = parser.parse_args(argv)
    if not args.src.is_file():
        print(f"Missing icon source: {args.src}", file=sys.stderr)
        return 1
    crop_icon_file(args.src, args.out, args.png)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
