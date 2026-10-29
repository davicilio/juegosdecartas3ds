#!/usr/bin/env python3
"""Genera la voz que canta las bolas del bingo en romfs/voz/.

Se ejecuta en GitHub Actions al compilar (los ficheros no se guardan en el
repositorio). Usa Piper si se le pasa un modelo de voz y, si no, espeak-ng:

    python3 tools/gen_voice.py --piper es_ES-davefx-medium.onnx
    python3 tools/gen_voice.py            # con espeak-ng

Cada frase se guarda como PCM de 16 bits, mono y a 22050 Hz (el formato de
los efectos de sonido), recortando el silencio de los extremos.
"""
import argparse
import array
import os
import subprocess
import sys
import tempfile
import wave

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "romfs", "voz")
RATE = 22050

# Los nombres populares de algunas bolas (los mismos que salen en pantalla)
NICKNAMES = {
    1: "el primero",
    13: "la mala suerte",
    15: "la niña bonita",
    22: "los dos patitos",
    33: "la edad de Cristo",
    69: "del derecho y del revés",
    88: "las dos gordas",
    90: "el abuelo",
}

UNITS = ["cero", "uno", "dos", "tres", "cuatro", "cinco", "seis", "siete", "ocho", "nueve", "diez",
         "once", "doce", "trece", "catorce", "quince", "dieciséis", "diecisiete", "dieciocho",
         "diecinueve", "veinte", "veintiuno", "veintidós", "veintitrés", "veinticuatro",
         "veinticinco", "veintiséis", "veintisiete", "veintiocho", "veintinueve"]
TENS = {3: "treinta", 4: "cuarenta", 5: "cincuenta", 6: "sesenta", 7: "setenta", 8: "ochenta",
        9: "noventa"}


def words(n):
    """Número en letra: así no depende de cómo lea las cifras cada motor."""
    if n < 30:
        return UNITS[n]
    t, u = divmod(n, 10)
    return TENS[t] if u == 0 else "%s y %s" % (TENS[t], UNITS[u])


def phrases():
    """(fichero, texto) de todo lo que se dice"""
    out = []
    for n in range(1, 91):
        text = "El %s." % words(n)
        if n >= 10:
            # Como en los bingos: el número y luego sus cifras
            text = "El %s. %s, %s." % (words(n), words(n // 10), words(n % 10))
        if n in NICKNAMES:
            text = "El %s. ¡%s!" % (words(n), NICKNAMES[n].capitalize())
        out.append(("%02d" % n, text))
    out.append(("linea", "¡Línea!"))
    out.append(("bingo", "¡Bingo!"))
    out.append(("empieza", "Empieza el bingo. ¡Mucha suerte!"))
    return out


def read_wav(path):
    with wave.open(path, "rb") as w:
        ch, width, rate, n = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
        data = w.readframes(n)
    if width != 2:
        raise SystemExit("solo se admiten WAV de 16 bits (%s)" % path)
    s = array.array("h", data)
    if sys.byteorder == "big":
        s.byteswap()
    if ch > 1:
        s = array.array("h", (sum(s[i:i + ch]) // ch for i in range(0, len(s), ch)))
    return s, rate


def resample(s, rate):
    if rate == RATE or not s:
        return s
    n = int(len(s) * RATE / rate)
    out = array.array("h", [0] * n)
    for i in range(n):
        pos = i * rate / RATE
        j = int(pos)
        f = pos - j
        a = s[j]
        b = s[j + 1] if j + 1 < len(s) else a
        out[i] = int(a + (b - a) * f)
    return out


def trim_and_normalize(s, threshold=300, pad=RATE // 50):
    first = next((i for i, v in enumerate(s) if abs(v) > threshold), 0)
    last = next((i for i in range(len(s) - 1, -1, -1) if abs(s[i]) > threshold), len(s) - 1)
    s = s[max(0, first - pad):min(len(s), last + pad)]
    peak = max((abs(v) for v in s), default=0)
    if peak:
        k = 26000 / peak
        s = array.array("h", (max(-32768, min(32767, int(v * k))) for v in s))
    return s


def synth(text, wav, model):
    if model:
        subprocess.run(["piper", "--model", model, "--output_file", wav], input=text.encode("utf-8"), check=True,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    else:
        subprocess.run(["espeak-ng", "-v", "es", "-s", "150", "-w", wav, text], check=True)


def convert(wav, raw):
    s, rate = read_wav(wav)
    s = trim_and_normalize(resample(s, rate))
    if sys.byteorder == "big":
        s.byteswap()
    with open(raw, "wb") as f:
        f.write(s.tobytes())


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--piper", help="modelo .onnx de Piper; sin él se usa espeak-ng")
    args = ap.parse_args()
    os.makedirs(OUT, exist_ok=True)
    with tempfile.TemporaryDirectory() as tmp:
        for name, text in phrases():
            wav = os.path.join(tmp, name + ".wav")
            synth(text, wav, args.piper)
            convert(wav, os.path.join(OUT, name + ".raw"))
    print("Voz generada en", OUT, "con", "Piper" if args.piper else "espeak-ng")


if __name__ == "__main__":
    main()
