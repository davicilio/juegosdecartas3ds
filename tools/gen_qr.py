#!/usr/bin/env python3
"""Genera un código QR (PNG) con un texto, sin dependencias aparte de Pillow.

Se usa para el QR de instalación del .cia con FBI ("Remote Install -> Scan QR
Code"):

    python3 tools/gen_qr.py URL salida.png

Implementa el modo byte del estándar QR (ISO/IEC 18004) con corrección de
errores M, versiones 1 a 10 y elección automática de la mejor máscara.
"""
import sys

from PIL import Image, ImageDraw

# Por versión (1-10) y nivel M: (codewords de corrección por bloque,
# [(número de bloques, codewords de datos por bloque), ...])
ECC_M = {
    1: (10, [(1, 16)]),
    2: (16, [(1, 28)]),
    3: (26, [(1, 44)]),
    4: (18, [(2, 32)]),
    5: (24, [(2, 43)]),
    6: (16, [(4, 27)]),
    7: (18, [(4, 31)]),
    8: (22, [(2, 38), (2, 39)]),
    9: (22, [(3, 36), (2, 37)]),
    10: (26, [(4, 43), (1, 44)]),
}
ALIGN = {1: [], 2: [6, 18], 3: [6, 22], 4: [6, 26], 5: [6, 30], 6: [6, 34], 7: [6, 22, 38],
         8: [6, 24, 42], 9: [6, 26, 46], 10: [6, 28, 50]}
ECL_BITS_M = 0  # bits de nivel de corrección en la información de formato


# --- Reed-Solomon sobre GF(256) con el polinomio 0x11D -------------------------

def gf_mul(x, y):
    z = 0
    for i in range(7, -1, -1):
        z = (z << 1) ^ ((z >> 7) * 0x11D)
        z ^= ((y >> i) & 1) * x
    return z & 0xFF


def rs_divisor(degree):
    result = [0] * (degree - 1) + [1]
    root = 1
    for _ in range(degree):
        for j in range(degree):
            result[j] = gf_mul(result[j], root)
            if j + 1 < degree:
                result[j] ^= result[j + 1]
        root = gf_mul(root, 0x02)
    return result


def rs_remainder(data, divisor):
    result = [0] * len(divisor)
    for b in data:
        factor = b ^ result.pop(0)
        result.append(0)
        for i, coef in enumerate(divisor):
            result[i] ^= gf_mul(coef, factor)
    return result


# --- Codificación -------------------------------------------------------------

def data_capacity(version):
    _, blocks = ECC_M[version]
    return sum(n * d for n, d in blocks)


def encode_data(text, version):
    data = text.encode("utf-8")
    bits = []

    def put(value, n):
        bits.extend((value >> i) & 1 for i in range(n - 1, -1, -1))

    put(0b0100, 4)  # modo byte
    put(len(data), 8 if version < 10 else 16)
    for b in data:
        put(b, 8)
    cap = data_capacity(version) * 8
    put(0, min(4, cap - len(bits)))
    put(0, (8 - len(bits) % 8) % 8)
    pad = 0xEC
    while len(bits) < cap:
        put(pad, 8)
        pad ^= 0xEC ^ 0x11
    return [int("".join(map(str, bits[i:i + 8])), 2) for i in range(0, len(bits), 8)]


def add_ecc_and_interleave(codewords, version):
    ecc_len, groups = ECC_M[version]
    divisor = rs_divisor(ecc_len)
    blocks, k = [], 0
    for n, dlen in groups:
        for _ in range(n):
            block = codewords[k:k + dlen]
            k += dlen
            blocks.append((block, rs_remainder(block, divisor)))
    result = []
    for i in range(max(len(b[0]) for b in blocks)):
        for data, _ in blocks:
            if i < len(data):
                result.append(data[i])
    for i in range(ecc_len):
        for _, ecc in blocks:
            result.append(ecc[i])
    return result


# --- Matriz -------------------------------------------------------------------

class Matrix:
    def __init__(self, version):
        self.version = version
        self.size = version * 4 + 17
        self.mod = [[False] * self.size for _ in range(self.size)]
        self.fixed = [[False] * self.size for _ in range(self.size)]

    def set(self, x, y, dark, fixed=True):
        self.mod[y][x] = dark
        if fixed:
            self.fixed[y][x] = True

    def draw_function_patterns(self):
        n = self.size
        for i in range(n):
            self.set(6, i, i % 2 == 0)
            self.set(i, 6, i % 2 == 0)
        for cx, cy in ((3, 3), (n - 4, 3), (3, n - 4)):
            for dy in range(-4, 5):
                for dx in range(-4, 5):
                    x, y = cx + dx, cy + dy
                    if 0 <= x < n and 0 <= y < n:
                        d = max(abs(dx), abs(dy))
                        self.set(x, y, d not in (2, 4))
        pos = ALIGN[self.version]
        for i, ax in enumerate(pos):
            for j, ay in enumerate(pos):
                if (i == 0 and j == 0) or (i == 0 and j == len(pos) - 1) or (i == len(pos) - 1 and j == 0):
                    continue
                for dy in range(-2, 3):
                    for dx in range(-2, 3):
                        self.set(ax + dx, ay + dy, max(abs(dx), abs(dy)) != 1)
        self.draw_format(0)
        self.draw_version()

    def draw_format(self, mask):
        data = (ECL_BITS_M << 3) | mask
        rem = data
        for _ in range(10):
            rem = (rem << 1) ^ ((rem >> 9) * 0x537)
        bits = ((data << 10) | rem) ^ 0x5412
        n = self.size
        bit = lambda i: ((bits >> i) & 1) != 0
        for i in range(6):
            self.set(8, i, bit(i))
        self.set(8, 7, bit(6))
        self.set(8, 8, bit(7))
        self.set(7, 8, bit(8))
        for i in range(9, 15):
            self.set(14 - i, 8, bit(i))
        for i in range(8):
            self.set(n - 1 - i, 8, bit(i))
        for i in range(8, 15):
            self.set(8, n - 15 + i, bit(i))
        self.set(8, n - 8, True)  # módulo oscuro fijo

    def draw_version(self):
        if self.version < 7:
            return
        rem = self.version
        for _ in range(12):
            rem = (rem << 1) ^ ((rem >> 11) * 0x1F25)
        bits = (self.version << 12) | rem
        n = self.size
        for i in range(18):
            dark = ((bits >> i) & 1) != 0
            a, b = n - 11 + i % 3, i // 3
            self.set(a, b, dark)
            self.set(b, a, dark)

    def draw_codewords(self, data):
        n = self.size
        i = 0
        right = n - 1
        while right >= 1:
            if right == 6:
                right = 5
            for vert in range(n):
                for j in range(2):
                    x = right - j
                    upward = ((right + 1) & 2) == 0
                    y = n - 1 - vert if upward else vert
                    if not self.fixed[y][x] and i < len(data) * 8:
                        self.mod[y][x] = ((data[i >> 3] >> (7 - (i & 7))) & 1) != 0
                        i += 1
            right -= 2

    def apply_mask(self, mask):
        for y in range(self.size):
            for x in range(self.size):
                if self.fixed[y][x]:
                    continue
                inv = [
                    (x + y) % 2 == 0,
                    y % 2 == 0,
                    x % 3 == 0,
                    (x + y) % 3 == 0,
                    (x // 3 + y // 2) % 2 == 0,
                    x * y % 2 + x * y % 3 == 0,
                    (x * y % 2 + x * y % 3) % 2 == 0,
                    ((x + y) % 2 + x * y % 3) % 2 == 0,
                ][mask]
                self.mod[y][x] ^= inv

    def penalty(self):
        n, m = self.size, self.mod
        score = 0
        for lines in (m, [list(col) for col in zip(*m)]):
            for row in lines:
                run, prev = 0, None
                for v in row:
                    if v == prev:
                        run += 1
                    else:
                        if run >= 5:
                            score += run - 2
                        run, prev = 1, v
                if run >= 5:
                    score += run - 2
                s = "".join("1" if v else "0" for v in row)
                score += 40 * (s.count("10111010000") + s.count("00001011101"))
        for y in range(n - 1):
            for x in range(n - 1):
                if m[y][x] == m[y][x + 1] == m[y + 1][x] == m[y + 1][x + 1]:
                    score += 3
        dark = sum(sum(row) for row in m)
        score += abs(dark * 20 - n * n * 10) // (n * n) * 10
        return score


def make_qr(text):
    for version in range(1, 11):
        if len(text.encode("utf-8")) + 3 <= data_capacity(version):
            break
    else:
        raise ValueError("Texto demasiado largo para este generador")
    codewords = add_ecc_and_interleave(encode_data(text, version), version)
    best, best_score = None, None
    for mask in range(8):
        q = Matrix(version)
        q.draw_function_patterns()
        q.draw_codewords(codewords)
        q.apply_mask(mask)
        q.draw_format(mask)
        s = q.penalty()
        if best_score is None or s < best_score:
            best, best_score = q, s
    return best


def save_png(q, path, scale=8, border=4):
    n = q.size + border * 2
    img = Image.new("RGB", (n * scale, n * scale), "white")
    d = ImageDraw.Draw(img)
    for y in range(q.size):
        for x in range(q.size):
            if q.mod[y][x]:
                px, py = (x + border) * scale, (y + border) * scale
                d.rectangle([px, py, px + scale - 1, py + scale - 1], fill="black")
    img.save(path)


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print(__doc__)
        sys.exit(1)
    qr = make_qr(sys.argv[1])
    save_png(qr, sys.argv[2])
    print("QR versión %d guardado en %s" % (qr.version, sys.argv[2]))
