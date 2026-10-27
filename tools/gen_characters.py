#!/usr/bin/env python3
"""Genera las piezas de los personajes (estilo "Animal Crossing": cabeza grande
y redonda, ojos sencillos y coloretes) en gfx/chr_*.png y el atlas
gfx/chars.t3s, además de source/ui/chars_layout.h con la posición de cada
pieza.

Todas las piezas comparten el mismo sistema de coordenadas: el origen es el
centro de la cabeza y la cabeza mide R píxeles de radio. Cada pieza se
recorta a su contenido y se guarda dónde queda su centro respecto al de la
cabeza, así que el juego solo tiene que dibujarlas una encima de otra.

Las piezas en gris (cabeza, pelo, camiseta, manos...) se tiñen en el juego
multiplicando por el color de piel, pelo o color favorito del Mii elegido.

    python3 tools/gen_characters.py
"""
import os

import numpy as np
from PIL import Image, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GFX = os.path.join(ROOT, "gfx")
HEADER = os.path.join(ROOT, "source", "ui", "chars_layout.h")

SS = 4          # supermuestreo
R = 40          # radio de la cabeza en el atlas (píxeles)
X0, X1 = -3.2, 3.2   # lienzo de dibujo, en radios de cabeza
Y0, Y1 = -3.0, 3.2

INK = (52, 36, 34, 255)
OUTLINE_GRAY = 168


def g(v, a=255):
    return (v, v, v, a)


class Layer:
    """Lienzo supermuestreado con coordenadas en radios de cabeza."""

    def __init__(self):
        self.w = int((X1 - X0) * R * SS)
        self.h = int((Y1 - Y0) * R * SS)
        self.img = Image.new("RGBA", (self.w, self.h), (0, 0, 0, 0))
        self.d = ImageDraw.Draw(self.img)

    def p(self, x, y):
        return ((x - X0) * R * SS, (y - Y0) * R * SS)

    def s(self, v):
        return v * R * SS

    def box(self, cx, cy, rx, ry):
        x0, y0 = self.p(cx - rx, cy - ry)
        x1, y1 = self.p(cx + rx, cy + ry)
        return [x0, y0, x1, y1]

    def ellipse(self, cx, cy, rx, ry, fill=None, outline=None, width=0.0):
        self.d.ellipse(self.box(cx, cy, rx, ry), fill=fill, outline=outline,
                       width=max(1, int(self.s(width))) if outline else 0)

    def poly(self, pts, fill=None, outline=None, width=0.0):
        pts = [self.p(x, y) for x, y in pts]
        self.d.polygon(pts, fill=fill)
        if outline:
            self.d.line(pts + [pts[0]], fill=outline, width=max(1, int(self.s(width))), joint="curve")

    def line(self, pts, fill, width):
        w = max(1, int(self.s(width)))
        pts = [self.p(x, y) for x, y in pts]
        self.d.line(pts, fill=fill, width=w, joint="curve")
        r = w / 2
        for x, y in (pts[0], pts[-1]):
            self.d.ellipse([x - r, y - r, x + r, y + r], fill=fill)

    def arc(self, cx, cy, rx, ry, a0, a1, fill, width, steps=24):
        pts = []
        for i in range(steps + 1):
            a = np.radians(a0 + (a1 - a0) * i / steps)
            pts.append((cx + rx * np.cos(a), cy + ry * np.sin(a)))
        self.line(pts, fill, width)

    def mask_with(self, other):
        """Deja solo lo que cae dentro del alfa de otro lienzo."""
        a = np.asarray(self.img).copy()
        m = np.asarray(other.img)[:, :, 3].astype(np.float32) / 255.0
        a[:, :, 3] = (a[:, :, 3] * m).astype(np.uint8)
        self.img = Image.fromarray(a)
        self.d = ImageDraw.Draw(self.img)

    def cut(self, other):
        """Quita lo que cae dentro del alfa de otro lienzo."""
        a = np.asarray(self.img).copy()
        m = np.asarray(other.img)[:, :, 3].astype(np.float32) / 255.0
        a[:, :, 3] = (a[:, :, 3] * (1.0 - m)).astype(np.uint8)
        self.img = Image.fromarray(a)
        self.d = ImageDraw.Draw(self.img)

    def over(self, other):
        self.img.alpha_composite(other.img)
        self.d = ImageDraw.Draw(self.img)

    def shade(self, cx, cy, rx, ry, light=1.0, dark=0.82, power=1.6):
        """Sombreado suave: más claro arriba a la izquierda y más oscuro en el
        borde inferior (solo cambia el brillo, el alfa se mantiene)."""
        a = np.asarray(self.img).astype(np.float32)
        ys, xs = np.mgrid[0:self.h, 0:self.w]
        px = (xs / SS / R + X0 - cx) / rx
        py = (ys / SS / R + Y0 - cy) / ry
        dist = np.clip(np.sqrt((px + 0.25) ** 2 + (py + 0.35) ** 2) / 1.45, 0, 1) ** power
        k = light + (dark - light) * dist
        a[:, :, :3] *= k[:, :, None]
        self.img = Image.fromarray(np.clip(a, 0, 255).astype(np.uint8))
        self.d = ImageDraw.Draw(self.img)

    def finish(self):
        """Reduce a 1x, recorta y devuelve (imagen, dx, dy) con el centro de
        la imagen respecto al centro de la cabeza."""
        small = self.img.resize((self.w // SS, self.h // SS), Image.LANCZOS)
        bbox = small.getchannel("A").point(lambda v: 255 if v > 3 else 0).getbbox()
        if bbox is None:
            raise ValueError("pieza vacía")
        x0, y0, x1, y1 = bbox
        x0, y0 = max(0, x0 - 1), max(0, y0 - 1)
        x1, y1 = min(small.width, x1 + 1), min(small.height, y1 + 1)
        crop = small.crop((x0, y0, x1, y1))
        cx = (x0 + x1) / 2.0 + X0 * R
        cy = (y0 + y1) / 2.0 + Y0 * R
        return crop, cx, cy


# ---------------------------------------------------------------------------
# Cabeza y cara
# ---------------------------------------------------------------------------

HEAD_RX, HEAD_RY = 1.0, 0.92
EYE_X, EYE_Y = -0.36, 0.04       # ojo izquierdo (el derecho es su reflejo)
MOUTH_Y = 0.42


def head_shape(L, fill):
    L.ellipse(0, 0, HEAD_RX, HEAD_RY, fill=fill)


def make_head():
    L = Layer()
    # orejas
    for sx in (-1, 1):
        L.ellipse(sx * 0.96, 0.1, 0.17, 0.2, fill=g(232), outline=g(OUTLINE_GRAY), width=0.03)
        L.ellipse(sx * 0.97, 0.1, 0.08, 0.11, fill=g(212))
    L.ellipse(0, 0, HEAD_RX, HEAD_RY, fill=g(250), outline=g(OUTLINE_GRAY), width=0.035)
    L.shade(0, 0, HEAD_RX, HEAD_RY, light=1.0, dark=0.86)
    return L


def make_nose():
    L = Layer()
    L.ellipse(0, 0.2, 0.09, 0.06, fill=g(205))
    L.ellipse(-0.025, 0.185, 0.03, 0.02, fill=g(240, 200))
    return L


def make_blush():
    L = Layer()
    for i in range(6):
        k = 1 - i / 6
        L.ellipse(-0.56, 0.3, 0.17 * k + 0.03, 0.1 * k + 0.02, fill=(255, 130, 140, 40))
    return L


def eye_open(lashes=False):
    L = Layer()
    L.ellipse(EYE_X, EYE_Y, 0.095, 0.15, fill=INK)
    L.ellipse(EYE_X + 0.03, EYE_Y - 0.06, 0.035, 0.04, fill=(255, 255, 255, 255))
    L.ellipse(EYE_X - 0.035, EYE_Y + 0.07, 0.016, 0.016, fill=(255, 255, 255, 200))
    if lashes:
        L.line([(EYE_X - 0.07, EYE_Y - 0.1), (EYE_X - 0.16, EYE_Y - 0.15)], INK, 0.035)
        L.line([(EYE_X - 0.09, EYE_Y - 0.03), (EYE_X - 0.18, EYE_Y - 0.05)], INK, 0.035)
    return L


def eye_wide():
    L = Layer()
    L.ellipse(EYE_X, EYE_Y, 0.13, 0.17, fill=(255, 255, 255, 255), outline=INK, width=0.035)
    L.ellipse(EYE_X, EYE_Y + 0.01, 0.055, 0.07, fill=INK)
    L.ellipse(EYE_X + 0.02, EYE_Y - 0.02, 0.018, 0.02, fill=(255, 255, 255, 255))
    return L


def eye_closed():
    """Ojo cerrado, relajado (arco hacia abajo)."""
    L = Layer()
    L.arc(EYE_X, EYE_Y - 0.02, 0.1, 0.07, 10, 170, INK, 0.045)
    return L


def eye_happy():
    """Ojo sonriente ("^")."""
    L = Layer()
    L.arc(EYE_X, EYE_Y + 0.06, 0.1, 0.1, 195, 345, INK, 0.05)
    return L


def make_brow():
    L = Layer()
    L.arc(EYE_X, -0.2, 0.12, 0.05, 200, 340, g(150), 0.06)
    return L


def mouth_smile():
    L = Layer()
    L.arc(0, MOUTH_Y - 0.06, 0.12, 0.08, 25, 155, INK, 0.04)
    return L


def mouth_happy():
    L = Layer()
    pts = [(-0.17, MOUTH_Y - 0.06), (0.17, MOUTH_Y - 0.06)]
    for i in range(1, 16):
        a = np.radians(i / 16 * 180)
        pts.append((0.17 * np.cos(a), MOUTH_Y - 0.06 + 0.17 * np.sin(a)))
    inside = Layer()
    inside.poly(pts, fill=(120, 30, 40, 255))
    tongue = Layer()
    tongue.ellipse(0, MOUTH_Y + 0.1, 0.11, 0.07, fill=(240, 110, 120, 255))
    tongue.mask_with(inside)
    inside.over(tongue)
    L.over(inside)
    L.poly(pts, outline=INK, width=0.035)
    return L


def mouth_sad():
    L = Layer()
    L.arc(0, MOUTH_Y + 0.06, 0.11, 0.07, 200, 340, INK, 0.04)
    return L


def mouth_o():
    L = Layer()
    L.ellipse(0, MOUTH_Y, 0.065, 0.085, fill=(120, 30, 40, 255), outline=INK, width=0.03)
    return L


def mouth_bite():
    """Morderse el labio inferior (seña de reyes): los dientes de arriba
    asoman sobre el labio de abajo."""
    L = Layer()
    L.ellipse(0, MOUTH_Y + 0.03, 0.15, 0.055, fill=(225, 105, 115, 255), outline=INK, width=0.03)
    L.poly([(-0.1, MOUTH_Y - 0.035), (0.1, MOUTH_Y - 0.035), (0.085, MOUTH_Y + 0.04), (-0.085, MOUTH_Y + 0.04)],
           fill=(255, 255, 255, 255), outline=INK, width=0.025)
    L.line([(0, MOUTH_Y - 0.03), (0, MOUTH_Y + 0.035)], (190, 190, 200, 255), 0.015)
    return L


def mouth_tongue():
    """Sacar la punta de la lengua (seña de ases)."""
    L = Layer()
    L.ellipse(0.02, MOUTH_Y + 0.06, 0.07, 0.085, fill=(240, 100, 115, 255), outline=INK, width=0.03)
    L.line([(0.02, MOUTH_Y + 0.02), (0.02, MOUTH_Y + 0.09)], (200, 70, 85, 255), 0.018)
    L.line([(-0.13, MOUTH_Y - 0.01), (0.15, MOUTH_Y - 0.01)], INK, 0.04)
    return L


def mouth_twist():
    """Torcer la boca hacia un lado (seña de medias)."""
    L = Layer()
    pts = []
    for i in range(17):
        t = i / 16
        x = -0.06 + 0.26 * t
        y = MOUTH_Y + 0.02 - 0.09 * t * t + 0.03 * np.sin(t * np.pi * 2)
        pts.append((x, y))
    L.line(pts, INK, 0.04)
    return L


# ---------------------------------------------------------------------------
# Pelo (gris, se tiñe con el color de pelo). Cada peinado tiene una capa de
# detrás (entre el cuerpo y la cabeza) y otra delante (sobre la frente).
# ---------------------------------------------------------------------------

HAIR = g(232)
HAIR_DARK = g(196)
HAIR_LIGHT = g(255)
HAIR_LINE = g(150)


def head_mask(grow=0.0):
    m = Layer()
    m.ellipse(0, 0, HEAD_RX + grow, HEAD_RY + grow, fill=(255, 255, 255, 255))
    return m


def cap(L, bottom=-0.28, side=0.15, grow=0.06):
    """Casquete de pelo que cubre la parte de arriba de la cabeza."""
    c = Layer()
    c.ellipse(0, -0.02, HEAD_RX + grow, HEAD_RY + grow, fill=HAIR)
    keep = Layer()
    keep.poly([(-1.3, -1.4), (1.3, -1.4), (1.3, side), (1.02, side), (0.75, bottom), (-0.75, bottom),
               (-1.02, side), (-1.3, side)], fill=(255, 255, 255, 255))
    c.mask_with(keep)
    L.over(c)


def shine(L, cx=-0.35, cy=-0.66, rx=0.22, ry=0.07, rot=-20):
    s = Layer()
    s.ellipse(cx, cy, rx, ry, fill=HAIR_LIGHT)
    s.img = s.img.rotate(rot, center=s.p(cx, cy), resample=Image.BICUBIC)
    s.d = ImageDraw.Draw(s.img)
    L.over(s)


def outline_of(L, width=0.035, color=HAIR_LINE):
    """Contorno alrededor de todo lo dibujado en la capa."""
    a = L.img.getchannel("A")
    from PIL import ImageFilter
    grown = a.filter(ImageFilter.MaxFilter(int(L.s(width)) * 2 + 1))
    o = Image.new("RGBA", L.img.size, color)
    o.putalpha(grown)
    o.alpha_composite(L.img)
    L.img = o
    L.d = ImageDraw.Draw(L.img)


def finish_hair(L, mask_head=None):
    outline_of(L)
    return L


def bangs(L, points, y_base=-0.42):
    """Mechones de flequillo: triángulos redondeados que bajan desde el casquete."""
    for x, y, w in points:
        L.poly([(x - w, y_base), (x + w, y_base), (x + 0.02, y)], fill=HAIR)


def hair_short():
    front = Layer()
    cap(front, bottom=-0.38, side=0.02)
    bangs(front, [(-0.55, -0.22, 0.2), (-0.2, -0.25, 0.22), (0.18, -0.3, 0.2), (0.52, -0.2, 0.2)], -0.45)
    for sx in (-1, 1):  # patillas
        front.poly([(sx * 0.98, -0.25), (sx * 0.86, -0.3), (sx * 0.86, 0.18), (sx * 0.96, 0.12)], fill=HAIR)
    shine(front)
    back = Layer()
    back.ellipse(0, -0.12, 1.06, 0.92, fill=HAIR)
    keep = Layer()
    keep.poly([(-1.3, -1.4), (1.3, -1.4), (1.3, 0.1), (-1.3, 0.1)], fill=(255, 255, 255, 255))
    back.mask_with(keep)
    return finish_hair(front), finish_hair(back)


def hair_long():
    front = Layer()
    cap(front, bottom=-0.3, side=0.1)
    # raya al medio con dos mechones que caen a los lados de la cara
    front.poly([(-0.02, -0.92), (-0.85, -0.4), (-0.95, 0.15), (-0.75, 0.0), (-0.55, -0.42)], fill=HAIR)
    front.poly([(0.02, -0.92), (0.85, -0.4), (0.95, 0.15), (0.75, 0.0), (0.55, -0.42)], fill=HAIR)
    front.line([(0, -0.95), (0, -0.55)], HAIR_DARK, 0.03)
    shine(front)
    back = Layer()
    back.ellipse(0, -0.05, 1.12, 1.0, fill=HAIR)
    back.poly([(-1.12, 0.0), (1.12, 0.0), (1.05, 1.15), (0.6, 1.25), (0, 1.15), (-0.6, 1.25), (-1.05, 1.15)], fill=HAIR)
    for x in (-0.6, 0.6):
        back.line([(x * 1.4, 0.3), (x * 1.5, 1.05)], HAIR_DARK, 0.03)
    return finish_hair(front), finish_hair(back)


def hair_bun():
    front = Layer()
    cap(front, bottom=-0.42, side=-0.05)
    # pelo peinado hacia atrás
    for x in (-0.45, -0.15, 0.15, 0.45):
        front.arc(x * 0.6, -0.1, 0.5, 0.75, 225 + x * 40, 260 + x * 40, HAIR_DARK, 0.025, steps=8)
    shine(front)
    back = Layer()
    back.ellipse(0, -1.02, 0.4, 0.34, fill=HAIR)
    back.arc(0, -1.02, 0.25, 0.18, 200, 330, HAIR_DARK, 0.03)
    back.ellipse(0, -0.1, 1.05, 0.92, fill=HAIR)
    keep = Layer()
    keep.poly([(-1.3, -1.6), (1.3, -1.6), (1.3, 0.25), (-1.3, 0.25)], fill=(255, 255, 255, 255))
    back.mask_with(keep)
    return finish_hair(front), finish_hair(back)


def hair_spiky():
    front = Layer()
    cap(front, bottom=-0.36, side=0.0)
    bangs(front, [(-0.5, -0.2, 0.17), (-0.18, -0.16, 0.17), (0.16, -0.2, 0.17), (0.48, -0.24, 0.17)], -0.45)
    shine(front)
    back = Layer()
    spikes = [(-1.05, -0.35), (-1.0, -0.85), (-0.65, -1.25), (-0.15, -1.38), (0.35, -1.32), (0.8, -1.08), (1.07, -0.6)]
    back.ellipse(0, -0.12, 1.0, 0.88, fill=HAIR)
    for i, (x, y) in enumerate(spikes):
        a = np.arctan2(y, x)
        bx0, by0 = 0.8 * np.cos(a - 0.3), 0.75 * np.sin(a - 0.3)
        bx1, by1 = 0.8 * np.cos(a + 0.3), 0.75 * np.sin(a + 0.3)
        back.poly([(bx0, by0), (x, y), (bx1, by1)], fill=HAIR)
    return finish_hair(front), finish_hair(back)


def hair_bald():
    front = Layer()
    for sx in (-1, 1):
        front.ellipse(sx * 0.93, -0.22, 0.09, 0.2, fill=HAIR)
    front.mask_with(head_mask(0.0))
    back = Layer()
    for sx in (-1, 1):
        back.ellipse(sx * 0.96, -0.2, 0.1, 0.22, fill=HAIR)
    return finish_hair(front), finish_hair(back)


def hair_bob():
    front = Layer()
    cap(front, bottom=-0.3, side=0.1)
    # flequillo recto
    front.poly([(-0.88, -0.55), (0.88, -0.55), (0.82, -0.28), (-0.82, -0.28)], fill=HAIR)
    for sx in (-1, 1):
        front.poly([(sx * 1.02, -0.3), (sx * 0.88, -0.3), (sx * 0.9, 0.38), (sx * 1.02, 0.42)], fill=HAIR)
    for x in (-0.45, 0.0, 0.45):
        front.line([(x, -0.5), (x + 0.03, -0.32)], HAIR_DARK, 0.025)
    shine(front)
    back = Layer()
    back.ellipse(0, -0.08, 1.1, 0.98, fill=HAIR)
    back.poly([(-1.1, -0.1), (1.1, -0.1), (1.06, 0.45), (-1.06, 0.45)], fill=HAIR)
    return finish_hair(front), finish_hair(back)


def hair_ponytail():
    front = Layer()
    cap(front, bottom=-0.32, side=0.05)
    bangs(front, [(-0.45, -0.22, 0.24), (-0.05, -0.28, 0.24), (0.38, -0.2, 0.22)], -0.48)
    shine(front)
    back = Layer()
    back.ellipse(0, -0.1, 1.06, 0.92, fill=HAIR)
    keep = Layer()
    keep.poly([(-1.3, -1.4), (1.3, -1.4), (1.3, 0.15), (-1.3, 0.15)], fill=(255, 255, 255, 255))
    back.mask_with(keep)
    tail = Layer()
    tail.poly([(0.75, -0.75), (1.0, -0.95), (1.35, -0.6), (1.42, 0.0), (1.3, 0.55), (1.12, 0.75),
               (1.18, 0.2), (1.05, -0.3)], fill=HAIR)
    tail.ellipse(0.88, -0.82, 0.14, 0.13, fill=HAIR_DARK)
    back.over(tail)
    return finish_hair(front), finish_hair(back)


def hair_curly():
    front = Layer()
    curls = [(-0.82, -0.3), (-0.72, -0.62), (-0.42, -0.85), (-0.05, -0.93), (0.32, -0.88), (0.65, -0.66),
             (0.84, -0.34), (-0.45, -0.48), (0.0, -0.52), (0.42, -0.48)]
    cap(front, bottom=-0.5, side=-0.2)
    for x, y in curls:
        front.ellipse(x, y, 0.22, 0.2, fill=HAIR)
        front.arc(x, y, 0.12, 0.1, 160, 330, HAIR_DARK, 0.025, steps=10)
    shine(front, cx=-0.3, cy=-0.8, rx=0.15, ry=0.05)
    back = Layer()
    for x, y in [(-1.02, -0.05), (-1.0, -0.5), (-0.75, -0.9), (-0.3, -1.08), (0.2, -1.08), (0.65, -0.95),
                 (0.98, -0.6), (1.04, -0.12), (-0.98, 0.32), (0.98, 0.32)]:
        back.ellipse(x, y, 0.3, 0.28, fill=HAIR)
    back.ellipse(0, -0.2, 1.0, 0.9, fill=HAIR)
    return finish_hair(front), finish_hair(back)


HAIR_STYLES = [("short", hair_short), ("long", hair_long), ("bun", hair_bun), ("spiky", hair_spiky),
               ("bald", hair_bald), ("bob", hair_bob), ("ponytail", hair_ponytail), ("curly", hair_curly)]


# ---------------------------------------------------------------------------
# Accesorios
# ---------------------------------------------------------------------------

def make_glasses():
    L = Layer()
    frame = (60, 45, 45, 255)
    for sx in (-1, 1):
        L.ellipse(sx * -EYE_X, EYE_Y, 0.22, 0.2, fill=(200, 230, 255, 60), outline=frame, width=0.045)
    L.arc(0, EYE_Y - 0.02, 0.15, 0.06, 200, 340, frame, 0.04, steps=10)
    for sx in (-1, 1):
        L.line([(sx * 0.57, EYE_Y - 0.04), (sx * 0.9, EYE_Y - 0.1)], frame, 0.04)
    return L


def make_beard():
    L = Layer()
    L.ellipse(0, 0.12, 1.0, 0.9, fill=HAIR)
    keep = head_mask(0.05)
    top = Layer()
    top.poly([(-1.3, 0.12), (-0.75, 0.12), (-0.45, 0.3), (0.45, 0.3), (0.75, 0.12), (1.3, 0.12), (1.3, 1.4),
              (-1.3, 1.4)], fill=(255, 255, 255, 255))
    keep.mask_with(top)
    L.mask_with(keep)
    hole = Layer()
    hole.ellipse(0, MOUTH_Y + 0.02, 0.22, 0.12, fill=(255, 255, 255, 255))
    hole.ellipse(0, 0.2, 0.6, 0.3, fill=(255, 255, 255, 255))
    L.cut(hole)
    outline_of(L, 0.03)
    return L


def make_mustache():
    L = Layer()
    for sx in (-1, 1):
        L.ellipse(sx * 0.12, 0.31, 0.15, 0.07, fill=HAIR)
        L.poly([(sx * 0.02, 0.28), (sx * 0.3, 0.3), (sx * 0.24, 0.37)], fill=HAIR)
    outline_of(L, 0.025)
    return L


# ---------------------------------------------------------------------------
# Cuerpo
# ---------------------------------------------------------------------------

BODY_TOP = 0.78


def make_torso():
    """Camiseta (gris, se tiñe con el color favorito)."""
    L = Layer()
    shirt = g(238)
    # mangas
    for sx in (-1, 1):
        L.poly([(sx * 0.4, BODY_TOP + 0.05), (sx * 0.78, BODY_TOP + 0.35), (sx * 0.85, BODY_TOP + 0.62),
                (sx * 0.62, BODY_TOP + 0.7), (sx * 0.42, BODY_TOP + 0.45)], fill=shirt)
    L.poly([(-0.45, BODY_TOP), (0.45, BODY_TOP), (0.6, BODY_TOP + 0.95), (-0.6, BODY_TOP + 0.95)], fill=shirt)
    L.ellipse(0, BODY_TOP + 0.95, 0.6, 0.12, fill=shirt)
    outline_of(L, 0.035, g(OUTLINE_GRAY))
    L.shade(0, BODY_TOP + 0.5, 0.7, 0.6, light=1.0, dark=0.8)
    # cuello de la camiseta
    L.arc(0, BODY_TOP + 0.02, 0.2, 0.1, 15, 165, g(200), 0.04)
    return L


def make_arms():
    """Brazos y manos (se tiñen con el color de piel)."""
    L = Layer()
    for sx in (-1, 1):
        L.line([(sx * 0.68, BODY_TOP + 0.62), (sx * 0.72, BODY_TOP + 0.85)], g(240), 0.15)
        L.ellipse(sx * 0.73, BODY_TOP + 0.92, 0.13, 0.12, fill=g(245))
    outline_of(L, 0.03, g(OUTLINE_GRAY))
    return L


def make_legs():
    """Pantalón y zapatos (con su color)."""
    L = Layer()
    pants = (64, 78, 128, 255)
    pants_dark = (46, 56, 98, 255)
    shoe = (110, 66, 40, 255)
    y = BODY_TOP + 0.95
    L.poly([(-0.56, y - 0.05), (0.56, y - 0.05), (0.5, y + 0.55), (0.06, y + 0.55), (0, y + 0.2), (-0.06, y + 0.55),
            (-0.5, y + 0.55)], fill=pants)
    L.line([(0, y + 0.18), (0, y + 0.5)], pants_dark, 0.03)
    for sx in (-1, 1):
        L.ellipse(sx * 0.3, y + 0.62, 0.25, 0.13, fill=shoe)
        L.ellipse(sx * 0.33, y + 0.57, 0.1, 0.035, fill=(160, 110, 80, 255))
    outline_of(L, 0.03, (50, 40, 40, 255))
    return L


def make_shins():
    """Piernas que asoman bajo la falda (se tiñen con el color de piel)."""
    L = Layer()
    y = BODY_TOP + 0.95
    for sx in (-1, 1):
        L.line([(sx * 0.25, y + 0.2), (sx * 0.27, y + 0.55)], g(240), 0.17)
    outline_of(L, 0.03, g(OUTLINE_GRAY))
    return L


def make_skirt():
    """Falda y zapatos (con su color), para los Miis femeninos."""
    L = Layer()
    skirt = (64, 78, 128, 255)
    shoe = (150, 50, 60, 255)
    y = BODY_TOP + 0.95
    L.poly([(-0.58, y - 0.05), (0.58, y - 0.05), (0.72, y + 0.35), (-0.72, y + 0.35)], fill=skirt)
    for x in (-0.35, 0.0, 0.35):
        L.line([(x * 0.9, y + 0.02), (x * 1.2, y + 0.32)], (46, 56, 98, 255), 0.025)
    for sx in (-1, 1):
        L.ellipse(sx * 0.3, y + 0.62, 0.22, 0.12, fill=shoe)
    outline_of(L, 0.03, (50, 40, 40, 255))
    return L


# ---------------------------------------------------------------------------

def pieces():
    out = [
        ("head", make_head()),
        ("nose", make_nose()),
        ("blush", make_blush()),
        ("eye_open", eye_open()),
        ("eye_lashes", eye_open(True)),
        ("eye_wide", eye_wide()),
        ("eye_closed", eye_closed()),
        ("eye_happy", eye_happy()),
        ("brow", make_brow()),
        ("mouth_smile", mouth_smile()),
        ("mouth_happy", mouth_happy()),
        ("mouth_sad", mouth_sad()),
        ("mouth_o", mouth_o()),
        ("mouth_bite", mouth_bite()),
        ("mouth_tongue", mouth_tongue()),
        ("mouth_twist", mouth_twist()),
        ("glasses", make_glasses()),
        ("beard", make_beard()),
        ("mustache", make_mustache()),
        ("torso", make_torso()),
        ("arms", make_arms()),
        ("legs", make_legs()),
        ("shins", make_shins()),
        ("skirt", make_skirt()),
    ]
    # Piezas del lado derecho de la cara: el reflejo de las del izquierdo (el
    # lienzo es simétrico respecto a x = 0)
    for name in ("blush", "eye_open", "eye_lashes", "eye_wide", "eye_closed", "eye_happy", "brow"):
        left = dict(out)[name]
        right = Layer()
        right.img = left.img.transpose(Image.FLIP_LEFT_RIGHT)
        out.append((name + "_r", right))
    for name, fn in HAIR_STYLES:
        front, back = fn()
        out.append(("hair_%s_front" % name, front))
        out.append(("hair_%s_back" % name, back))
    return out


def main():
    os.makedirs(GFX, exist_ok=True)
    files, layout = [], []
    for name, layer in pieces():
        img, cx, cy = layer.finish()
        fn = "chr_%s.png" % name
        img.save(os.path.join(GFX, fn))
        files.append(fn)
        layout.append((name, cx, cy))
    with open(os.path.join(GFX, "chars.t3s"), "w", newline="\n") as f:
        f.write("--atlas -f rgba8888 -z auto --border transparent\n")
        for fn in files:
            f.write(fn + "\n")
    with open(HEADER, "w", newline="\n") as f:
        f.write("/* Generado por tools/gen_characters.py: no editar a mano. */\n")
        f.write("#ifndef UI_CHARS_LAYOUT_H\n#define UI_CHARS_LAYOUT_H\n\n")
        f.write("/* Radio de la cabeza en el atlas gfx/chars.t3s (píxeles). */\n")
        f.write("#define CHR_HEAD_RADIUS %d.0f\n\n" % R)
        f.write("/* Índices del atlas, en el mismo orden que gfx/chars.t3s. */\nenum {\n")
        for name, _, _ in layout:
            f.write("\tCHR_%s,\n" % name.upper())
        f.write("\tCHR_COUNT\n};\n\n")
        f.write("/* Centro de cada pieza respecto al centro de la cabeza (píxeles del atlas). */\n")
        f.write("static const float CHR_OFFSET[CHR_COUNT][2] = {\n")
        for name, cx, cy in layout:
            f.write("\t{ %.1ff, %.1ff }, /* %s */\n" % (cx, cy, name))
        f.write("};\n\n#endif\n")
    print("Piezas de personajes generadas:", len(files))


if __name__ == "__main__":
    main()
