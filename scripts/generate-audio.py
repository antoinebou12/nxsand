#!/usr/bin/env python3
"""Generate romfs/audio/*.wav for NXSand (48 kHz stereo S16LE). Stdlib only."""

from __future__ import annotations

import math
import random
import struct
import wave
from pathlib import Path

SAMPLE_RATE = 48000
CHANNELS = 2
SAMPLE_WIDTH = 2

ROOT = Path(__file__).resolve().parent.parent
OUT_DIR = ROOT / "romfs" / "audio"


def write_wav(path: Path, samples: list[float]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(path), "wb") as wf:
        wf.setnchannels(CHANNELS)
        wf.setsampwidth(SAMPLE_WIDTH)
        wf.setframerate(SAMPLE_RATE)
        frames = b"".join(
            struct.pack("<hh", int(max(-32767, min(32767, s * 32767.0))), int(max(-32767, min(32767, s * 32767.0))))
            for s in samples
        )
        wf.writeframes(frames)


def sine(freq: float, t: float, amp: float = 1.0) -> float:
    return amp * math.sin(2.0 * math.pi * freq * t)


def env_adsr(t: float, dur: float, attack: float, release: float) -> float:
    if t < 0.0 or t >= dur:
        return 0.0
    if t < attack:
        return t / max(attack, 1e-6)
    if t >= dur - release:
        return max(0.0, (dur - t) / max(release, 1e-6))
    return 1.0


def gen_tone(freq: float, duration_ms: int, amp: float = 0.35, attack_ms: float = 2.0, release_ms: float = 8.0) -> list[float]:
    dur = duration_ms / 1000.0
    attack = attack_ms / 1000.0
    release = release_ms / 1000.0
    n = int(SAMPLE_RATE * dur)
    out: list[float] = []
    for i in range(n):
        t = i / SAMPLE_RATE
        e = env_adsr(t, dur, attack, release)
        out.append(sine(freq, t, amp * e))
    return out


def gen_two_note(f1: float, f2: float, duration_ms: int, split_ms: int = 40) -> list[float]:
    a = gen_tone(f1, split_ms, amp=0.32)
    b = gen_tone(f2, duration_ms - split_ms, amp=0.34)
    return a + b


def gen_back_blip() -> list[float]:
    dur_ms = 50
    n = int(SAMPLE_RATE * dur_ms / 1000.0)
    out: list[float] = []
    for i in range(n):
        t = i / SAMPLE_RATE
        freq = 440.0 * (1.0 - 0.35 * (t / (dur_ms / 1000.0)))
        e = env_adsr(t, dur_ms / 1000.0, 0.003, 0.015)
        out.append(sine(freq, t, 0.28 * e))
    return out


def gen_explosion(heavy: bool) -> list[float]:
    dur_ms = 110 if heavy else 85
    dur = dur_ms / 1000.0
    n = int(SAMPLE_RATE * dur)
    rng = random.Random(42 if heavy else 17)
    out: list[float] = []
    for i in range(n):
        t = i / SAMPLE_RATE
        e = env_adsr(t, dur, 0.002, 0.04 if heavy else 0.03)
        bass = sine(180.0 if heavy else 210.0, t, 0.55 if heavy else 0.42)
        noise = (rng.random() * 2.0 - 1.0) * (0.35 if heavy else 0.28)
        mix = (bass + noise) * e
        lp = mix * (0.7 if heavy else 0.85)
        out.append(max(-1.0, min(1.0, lp)))
    return out


def gen_menu_theme() -> list[float]:
    """~36 s seamless loop: C major pentatonic pad + slow arpeggio."""
    loop_sec = 36.0
    n = int(SAMPLE_RATE * loop_sec)
    notes = [261.63, 329.63, 392.0, 523.25, 659.25]
    out: list[float] = []
    for i in range(n):
        t = i / SAMPLE_RATE
        phase = (t / loop_sec) * 2.0 * math.pi
        pad = sine(130.81, t, 0.08) + sine(196.0, t, 0.06)
        pad += sine(261.63, t, 0.05) * (0.85 + 0.15 * math.sin(phase * 0.5))
        arp_idx = int((t * 0.55) % len(notes))
        arp = sine(notes[arp_idx], t, 0.04) * (0.6 + 0.4 * math.sin(t * 3.1))
        shimmer = sine(880.0, t, 0.012) * (0.5 + 0.5 * math.sin(t * 0.9))
        s = pad + arp + shimmer
        out.append(max(-1.0, min(1.0, s * 0.55)))
    fade = 512
    for k in range(fade):
        w = k / fade
        out[k] *= w
        out[-(k + 1)] *= w
        out[k] += out[-(k + 1)] * (1.0 - w) * 0.5
    return out


def main() -> int:
    specs = {
        "menu_theme.wav": gen_menu_theme(),
        "ui_confirm.wav": gen_two_note(660.0, 880.0, 80),
        "ui_back.wav": gen_back_blip(),
        "ui_nav.wav": gen_tone(520.0, 30, amp=0.22, attack_ms=1.5, release_ms=6.0),
        "ui_material.wav": gen_tone(880.0, 45, amp=0.30),
        "explosion_light.wav": gen_explosion(False),
        "explosion_heavy.wav": gen_explosion(True),
    }
    for name, samples in specs.items():
        path = OUT_DIR / name
        write_wav(path, samples)
        print(f"OK: {path} ({len(samples) / SAMPLE_RATE:.2f}s)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
