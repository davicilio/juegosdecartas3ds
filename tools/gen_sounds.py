#!/usr/bin/env python3
"""Sintetiza los efectos de sonido del juego en romfs/sfx/ como PCM crudo
(16 bits con signo, mono, 22050 Hz), el formato que reproduce ndsp sin
necesidad de decodificar nada en la consola.

    python3 tools/gen_sounds.py
"""
import math
import os
import random
import struct

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "romfs", "sfx")
RATE = 22050


def env(t, attack, decay):
    if t < attack:
        return t / attack
    return math.exp(-(t - attack) / decay)


def tone(freq, dur, vol=0.5, attack=0.005, decay=0.08, shape="sine"):
    out = []
    for i in range(int(dur * RATE)):
        t = i / RATE
        ph = 2 * math.pi * freq * t
        if shape == "square":
            s = 1.0 if math.sin(ph) >= 0 else -1.0
            s *= 0.5
        elif shape == "tri":
            s = 2 / math.pi * math.asin(math.sin(ph))
        else:
            s = math.sin(ph)
        out.append(s * vol * env(t, attack, decay))
    return out


def noise(dur, vol=0.5, attack=0.002, decay=0.03, lowpass=0.5, seed=1):
    rnd = random.Random(seed)
    out = []
    y = 0.0
    for i in range(int(dur * RATE)):
        t = i / RATE
        y += (rnd.uniform(-1, 1) - y) * lowpass
        out.append(y * vol * env(t, attack, decay))
    return out


def mix(*tracks):
    n = max(len(t) for t in tracks)
    out = [0.0] * n
    for tr in tracks:
        for i, s in enumerate(tr):
            out[i] += s
    return out


def concat(*parts):
    out = []
    for p in parts:
        out += p
    return out


def silence(dur):
    return [0.0] * int(dur * RATE)


def sweep_noise(dur, vol, seed):
    """Ruido con filtro que se abre: suena como una carta deslizándose."""
    rnd = random.Random(seed)
    out = []
    y = 0.0
    n = int(dur * RATE)
    for i in range(n):
        t = i / RATE
        lp = 0.05 + 0.6 * (i / n)
        y += (rnd.uniform(-1, 1) - y) * lp
        out.append(y * vol * env(t, dur * 0.6, dur * 0.25))
    return out


def save(name, samples):
    os.makedirs(OUT, exist_ok=True)
    with open(os.path.join(OUT, name + ".raw"), "wb") as f:
        for s in samples:
            v = int(max(-1.0, min(1.0, s)) * 32767)
            f.write(struct.pack("<h", v))


def save_wav(path, samples, rate=RATE):
    """WAV de 16 bits mono, para el sonido del banner del .cia."""
    import wave
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(b"".join(struct.pack("<h", int(max(-1.0, min(1.0, s)) * 32767)) for s in samples))


def main():
    save("card", mix(sweep_noise(0.09, 0.9, 3), noise(0.03, 0.4, decay=0.01, lowpass=0.9, seed=4)))
    save("flip", concat(noise(0.025, 0.6, decay=0.008, lowpass=0.7, seed=5), silence(0.03),
                        noise(0.03, 0.7, decay=0.01, lowpass=0.5, seed=6)))
    save("chip", mix(tone(2200, 0.12, 0.25, decay=0.03), tone(3300, 0.12, 0.18, decay=0.02),
                     noise(0.02, 0.4, decay=0.005, lowpass=0.95, seed=7)))
    save("select", tone(880, 0.07, 0.35, decay=0.03, shape="tri"))
    save("confirm", concat(tone(988, 0.06, 0.35, decay=0.05, shape="tri"),
                           tone(1319, 0.12, 0.35, decay=0.06, shape="tri")))
    save("error", concat(tone(220, 0.08, 0.35, decay=0.06, shape="square"),
                         tone(180, 0.12, 0.35, decay=0.08, shape="square")))
    notes = [523, 659, 784, 1047]
    save("win", concat(*[tone(f, 0.11, 0.35, decay=0.09, shape="tri") for f in notes[:-1]],
                       tone(notes[-1], 0.4, 0.4, decay=0.2, shape="tri")))
    save("lose", concat(tone(392, 0.15, 0.35, decay=0.12, shape="tri"), tone(330, 0.15, 0.35, decay=0.12, shape="tri"),
                        tone(262, 0.4, 0.35, decay=0.25, shape="tri")))
    # Melodía del banner del menú HOME (unos 2,5 segundos)
    melody = [(659, 0.18), (784, 0.18), (1047, 0.18), (988, 0.18), (784, 0.18), (880, 0.36), (1047, 0.8)]
    tune = concat(*[tone(f, d, 0.35, decay=d * 0.6, shape="tri") for f, d in melody])
    save_wav(os.path.join(ROOT, "cia", "banner.wav"), tune)
    print("Sonidos generados en", OUT)


if __name__ == "__main__":
    main()
