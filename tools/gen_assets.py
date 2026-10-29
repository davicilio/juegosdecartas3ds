#!/usr/bin/env python3
"""Genera todos los gráficos del juego (cartas de la baraja española, fondos,
fichas, logotipo e icono) en la carpeta gfx/.

Los PNG generados se incluyen en el repositorio, así que solo hace falta
ejecutar este script si se quiere cambiar el aspecto de los gráficos:

    python3 tools/gen_assets.py

Requiere Pillow (pip install pillow).
"""
import math
import os
import random

from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GFX = os.path.join(ROOT, "gfx")

# Escala de supermuestreo: se dibuja a 4x y se reduce con filtro Lanczos para
# obtener bordes suaves a la resolución nativa de la 3DS.
SS = 4
CARD_W, CARD_H = 64, 96

SUITS = ["oros", "copas", "espadas", "bastos"]
RANKS = [1, 2, 3, 4, 5, 6, 7, 10, 11, 12]

CREAM = (251, 246, 233, 255)
INK = (40, 30, 30, 255)
GOLD = (232, 178, 30, 255)
GOLD_DARK = (150, 100, 10, 255)
RED = (200, 40, 45, 255)
RED_DARK = (120, 20, 25, 255)
BLUE = (43, 91, 184, 255)
BLUE_DARK = (20, 45, 110, 255)
GREEN = (62, 154, 58, 255)
GREEN_DARK = (30, 90, 30, 255)
WOOD = (138, 90, 43, 255)
WOOD_DARK = (85, 52, 22, 255)
SKIN = (245, 205, 160, 255)

SUIT_COLOR = {"oros": GOLD_DARK, "copas": RED, "espadas": BLUE, "bastos": GREEN_DARK}
# Número de cortes ("pintas") en el marco de la carta según el palo.
SUIT_GAPS = {"oros": 0, "copas": 1, "espadas": 2, "bastos": 3}


def find_font(bold=True):
    candidates = [
        "/usr/share/fonts/truetype/dejavu/DejaVuSerif-Bold.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSerif-Bold.ttf",
        "C:/Windows/Fonts/georgiab.ttf",
        "/Library/Fonts/Georgia Bold.ttf",
    ]
    for c in candidates:
        if os.path.exists(c):
            return c
    return None


FONT_PATH = find_font()


def font(size):
    if FONT_PATH:
        return ImageFont.truetype(FONT_PATH, size)
    return ImageFont.load_default()


def S(v):
    return int(round(v * SS))


# ---------------------------------------------------------------------------
# Símbolos de los palos. Todas las funciones reciben coordenadas ya escaladas.
# ---------------------------------------------------------------------------

def draw_oro(d, cx, cy, r):
    d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=GOLD, outline=GOLD_DARK, width=max(1, r // 7))
    r2 = r * 0.72
    d.ellipse([cx - r2, cy - r2, cx + r2, cy + r2], outline=GOLD_DARK, width=max(1, r // 10))
    # Sol central con rayos
    for i in range(8):
        a = i * math.pi / 4
        x1, y1 = cx + math.cos(a) * r * 0.2, cy + math.sin(a) * r * 0.2
        x2, y2 = cx + math.cos(a) * r * 0.55, cy + math.sin(a) * r * 0.55
        d.line([x1, y1, x2, y2], fill=GOLD_DARK, width=max(1, r // 9))
    r3 = r * 0.22
    d.ellipse([cx - r3, cy - r3, cx + r3, cy + r3], fill=RED, outline=GOLD_DARK, width=max(1, r // 14))


def draw_copa(d, cx, cy, r):
    w = max(1, r // 8)
    # Cáliz
    top = cy - r * 0.95
    bowl = [
        (cx - r * 0.8, top),
        (cx + r * 0.8, top),
        (cx + r * 0.7, cy - r * 0.35),
        (cx + r * 0.35, cy + r * 0.05),
        (cx - r * 0.35, cy + r * 0.05),
        (cx - r * 0.7, cy - r * 0.35),
    ]
    d.polygon(bowl, fill=RED, outline=RED_DARK)
    d.line(bowl + [bowl[0]], fill=RED_DARK, width=w)
    # Borde dorado
    d.rectangle([cx - r * 0.85, top - r * 0.12, cx + r * 0.85, top + r * 0.08], fill=GOLD, outline=GOLD_DARK)
    # Brillo
    d.line([cx - r * 0.45, top + r * 0.2, cx - r * 0.3, cy - r * 0.25], fill=(255, 150, 150, 255), width=w)
    # Tallo y nudo
    d.rectangle([cx - r * 0.12, cy, cx + r * 0.12, cy + r * 0.6], fill=GOLD, outline=GOLD_DARK)
    d.ellipse([cx - r * 0.25, cy + r * 0.15, cx + r * 0.25, cy + r * 0.4], fill=GOLD, outline=GOLD_DARK)
    # Pie
    d.polygon([(cx - r * 0.15, cy + r * 0.6), (cx + r * 0.15, cy + r * 0.6),
               (cx + r * 0.6, cy + r * 0.92), (cx - r * 0.6, cy + r * 0.92)], fill=RED, outline=RED_DARK)
    d.rectangle([cx - r * 0.65, cy + r * 0.88, cx + r * 0.65, cy + r], fill=GOLD, outline=GOLD_DARK)


def draw_espada(d, cx, cy, r, angle=0.0):
    """Espada vertical (angle en radianes, 0 = punta arriba)."""
    ca, sa = math.cos(angle), math.sin(angle)

    def P(x, y):
        return (cx + x * ca - y * sa, cy + x * sa + y * ca)

    blade = [P(0, -r), P(r * 0.14, -r * 0.8), P(r * 0.12, r * 0.35), P(-r * 0.12, r * 0.35), P(-r * 0.14, -r * 0.8)]
    d.polygon(blade, fill=(200, 215, 235, 255), outline=BLUE_DARK)
    d.line([P(0, -r * 0.85), P(0, r * 0.3)], fill=BLUE, width=max(1, r // 14))
    guard = [P(-r * 0.45, r * 0.32), P(r * 0.45, r * 0.32), P(r * 0.45, r * 0.45), P(-r * 0.45, r * 0.45)]
    d.polygon(guard, fill=GOLD, outline=GOLD_DARK)
    grip = [P(-r * 0.09, r * 0.45), P(r * 0.09, r * 0.45), P(r * 0.09, r * 0.82), P(-r * 0.09, r * 0.82)]
    d.polygon(grip, fill=BLUE, outline=BLUE_DARK)
    px, py = P(0, r * 0.9)
    pr = r * 0.12
    d.ellipse([px - pr, py - pr, px + pr, py + pr], fill=GOLD, outline=GOLD_DARK)


def draw_basto(d, cx, cy, r, angle=0.0):
    ca, sa = math.cos(angle), math.sin(angle)

    def P(x, y):
        return (cx + x * ca - y * sa, cy + x * sa + y * ca)

    club = [P(-r * 0.3, -r * 0.95), P(r * 0.3, -r * 0.95), P(r * 0.36, -r * 0.6),
            P(r * 0.14, r * 0.95), P(-r * 0.14, r * 0.95), P(-r * 0.36, -r * 0.6)]
    d.polygon(club, fill=WOOD, outline=WOOD_DARK)
    d.line(club + [club[0]], fill=WOOD_DARK, width=max(1, r // 10))
    # Nudos
    for (x, y) in [(-0.12, -0.55), (0.12, -0.2), (-0.06, 0.2), (0.05, 0.55)]:
        kx, ky = P(r * x, r * y)
        kr = r * 0.07
        d.ellipse([kx - kr, ky - kr, kx + kr, ky + kr], fill=WOOD_DARK)
    # Hojitas verdes
    for (x, y, s) in [(0.38, -0.4, 1), (-0.36, -0.05, -1), (0.28, 0.35, 1)]:
        lx, ly = P(r * x, r * y)
        lw, lh = r * 0.22, r * 0.12
        d.ellipse([lx - lw, ly - lh, lx + lw, ly + lh], fill=GREEN, outline=GREEN_DARK)


def draw_suit(d, suit, cx, cy, r, angle=0.0):
    if suit == "oros":
        draw_oro(d, cx, cy, r)
    elif suit == "copas":
        draw_copa(d, cx, cy, r)
    elif suit == "espadas":
        draw_espada(d, cx, cy, r, angle)
    else:
        draw_basto(d, cx, cy, r, angle)


# ---------------------------------------------------------------------------
# Cartas
# ---------------------------------------------------------------------------

PIP_LAYOUTS = {
    1: [(0.5, 0.5)],
    2: [(0.5, 0.22), (0.5, 0.78)],
    3: [(0.5, 0.2), (0.5, 0.5), (0.5, 0.8)],
    4: [(0.3, 0.24), (0.7, 0.24), (0.3, 0.76), (0.7, 0.76)],
    5: [(0.3, 0.22), (0.7, 0.22), (0.5, 0.5), (0.3, 0.78), (0.7, 0.78)],
    6: [(0.3, 0.2), (0.7, 0.2), (0.3, 0.5), (0.7, 0.5), (0.3, 0.8), (0.7, 0.8)],
    7: [(0.28, 0.14), (0.72, 0.14), (0.5, 0.32), (0.28, 0.5), (0.72, 0.5), (0.28, 0.86), (0.72, 0.86)],
    # Ochos y nueves: solo los usa la baraja de 48 cartas (butifarra)
    8: [(0.28, 0.12), (0.72, 0.12), (0.28, 0.37), (0.72, 0.37), (0.28, 0.63), (0.72, 0.63), (0.28, 0.88),
        (0.72, 0.88)],
    9: [(0.24, 0.14), (0.5, 0.14), (0.76, 0.14), (0.24, 0.5), (0.5, 0.5), (0.76, 0.5), (0.24, 0.86), (0.5, 0.86),
        (0.76, 0.86)],
}


def card_base(w, h):
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.rounded_rectangle([0, 0, w - 1, h - 1], radius=S(6), fill=CREAM, outline=INK, width=S(1))
    return img, d


def draw_frame(d, suit, w, h):
    """Marco interior con los cortes tradicionales que indican el palo."""
    col = SUIT_COLOR[suit]
    x0, y0, x1, y1 = S(4), S(4), w - S(4), h - S(4)
    lw = S(1.2)
    gaps = SUIT_GAPS[suit]
    # laterales completos
    d.line([x0, y0, x0, y1], fill=col, width=lw)
    d.line([x1, y0, x1, y1], fill=col, width=lw)
    for y in (y0, y1):
        if gaps == 0:
            d.line([x0, y, x1, y], fill=col, width=lw)
            continue
        # se divide el lado en gaps+1 trozos separados por huecos
        gap = S(3)
        seg = (x1 - x0 - gaps * gap) / (gaps + 1)
        x = x0
        for _ in range(gaps + 1):
            d.line([x, y, x + seg, y], fill=col, width=lw)
            x += seg + gap


def draw_index(d, rank, suit, w, h):
    f = font(S(13))
    txt = str(rank)
    col = SUIT_COLOR[suit]
    bbox = d.textbbox((0, 0), txt, font=f)
    tw = bbox[2] - bbox[0]
    th = bbox[3] - bbox[1]
    # esquina superior izquierda
    d.text((S(6) - bbox[0], S(6) - bbox[1]), txt, font=f, fill=col)
    # esquina inferior derecha
    d.text((w - S(6) - tw - bbox[0], h - S(6) - th - bbox[1]), txt, font=f, fill=col)


def draw_figure(d, rank, suit, w, h):
    """Figura estilizada: sota (10), caballo (11) o rey (12)."""
    col = SUIT_COLOR[suit]
    light = {"oros": (250, 220, 120, 255), "copas": (240, 120, 120, 255),
             "espadas": (130, 165, 230, 255), "bastos": (130, 200, 120, 255)}[suit]
    cx = w / 2
    # Panel de fondo de la figura
    px0, py0, px1, py1 = S(10), S(20), w - S(10), h - S(20)
    d.rounded_rectangle([px0, py0, px1, py1], radius=S(3), fill=(240, 232, 210, 255), outline=col, width=S(0.8))

    if rank == 11:
        # Caballo: cabeza de caballo detrás del jinete
        hx, hy = cx + S(9), S(40)
        horse = [(hx - S(6), hy + S(18)), (hx - S(7), hy), (hx - S(2), hy - S(10)), (hx + S(3), hy - S(13)),
                 (hx + S(5), hy - S(10)), (hx + S(14), hy - S(2)), (hx + S(13), hy + S(3)),
                 (hx + S(5), hy + S(1)), (hx + S(6), hy + S(18))]
        d.polygon(horse, fill=(160, 110, 70, 255), outline=WOOD_DARK)
        d.line(horse + [horse[0]], fill=WOOD_DARK, width=S(0.8))
        d.ellipse([hx + S(2), hy - S(7), hx + S(4), hy - S(5)], fill=INK)
        # crin
        d.line([(hx - S(2), hy - S(10)), (hx - S(8), hy + S(6))], fill=INK, width=S(1.5))
        body_cx = cx - S(5)
        head_y = S(33)
    else:
        body_cx = cx
        head_y = S(31)

    # Cuerpo (túnica)
    body = [(body_cx - S(14), h - S(22)), (body_cx - S(10), head_y + S(9)), (body_cx + S(10), head_y + S(9)),
            (body_cx + S(14), h - S(22))]
    d.polygon(body, fill=col, outline=INK)
    # Franja de la túnica
    d.polygon([(body_cx - S(4), head_y + S(9)), (body_cx + S(4), head_y + S(9)),
               (body_cx + S(6), h - S(22)), (body_cx - S(6), h - S(22))], fill=light)
    d.line([(body_cx - S(12), head_y + S(22)), (body_cx + S(12), head_y + S(22))], fill=GOLD, width=S(1.5))
    # Cabeza
    hr = S(6)
    d.ellipse([body_cx - hr, head_y - hr, body_cx + hr, head_y + hr], fill=SKIN, outline=INK, width=S(0.6))
    d.ellipse([body_cx - S(2.5), head_y - S(1), body_cx - S(1.3), head_y + S(0.2)], fill=INK)
    d.ellipse([body_cx + S(1.3), head_y - S(1), body_cx + S(2.5), head_y + S(0.2)], fill=INK)

    if rank == 12:
        # Rey: barba y corona
        d.chord([body_cx - hr, head_y - S(1), body_cx + hr, head_y + hr + S(3)], 0, 180, fill=(120, 80, 50, 255))
        crown = [(body_cx - S(7), head_y - S(4)), (body_cx - S(7), head_y - S(12)), (body_cx - S(3.5), head_y - S(7)),
                 (body_cx, head_y - S(13)), (body_cx + S(3.5), head_y - S(7)), (body_cx + S(7), head_y - S(12)),
                 (body_cx + S(7), head_y - S(4))]
        d.polygon(crown, fill=GOLD, outline=GOLD_DARK)
        d.ellipse([body_cx - S(1.2), head_y - S(9), body_cx + S(1.2), head_y - S(6.6)], fill=RED)
    elif rank == 10:
        # Sota: sombrero con pluma
        d.ellipse([body_cx - S(8), head_y - S(7), body_cx + S(8), head_y - S(3)], fill=INK)
        d.rectangle([body_cx - S(5), head_y - S(11), body_cx + S(5), head_y - S(5)], fill=INK)
        d.line([(body_cx + S(4), head_y - S(10)), (body_cx + S(11), head_y - S(16))], fill=RED, width=S(1.8))
    else:
        # Caballo: jinete con casco
        d.chord([body_cx - S(7), head_y - S(10), body_cx + S(7), head_y + S(2)], 180, 360, fill=(170, 170, 185, 255),
                outline=INK)

    # Cada figura sostiene su palo
    sx = body_cx + (S(-11) if rank != 11 else S(-8))
    draw_suit(d, suit, sx, h - S(32), S(7.5), angle=-0.4 if suit in ("espadas", "bastos") else 0)


def make_card(suit, rank):
    w, h = S(CARD_W), S(CARD_H)
    img, d = card_base(w, h)
    draw_frame(d, suit, w, h)
    draw_index(d, rank, suit, w, h)
    if rank <= 9:
        ax0, ay0 = S(10), S(18)
        aw, ah = w - S(20), h - S(36)
        r = 15 if rank == 1 else 8.5 if rank <= 3 else 7.5 if rank <= 5 else 6.5 if rank <= 7 else 5.6
        if suit in ("espadas", "bastos"):
            # los símbolos alargados se ven mejor algo más grandes
            r *= 1.25 if rank > 1 else 1.4
        r = S(r)
        for (fx, fy) in PIP_LAYOUTS[rank]:
            draw_suit(d, suit, ax0 + fx * aw, ay0 + fy * ah, r)
    else:
        draw_figure(d, rank, suit, w, h)
    return img.resize((CARD_W, CARD_H), Image.LANCZOS)


def make_back():
    w, h = S(CARD_W), S(CARD_H)
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.rounded_rectangle([0, 0, w - 1, h - 1], radius=S(6), fill=CREAM, outline=INK, width=S(1))
    d.rounded_rectangle([S(3), S(3), w - S(3), h - S(3)], radius=S(4), fill=(142, 27, 34, 255))
    # Celosía de rombos
    step = S(8)
    for i in range(-20, 30):
        d.line([(i * step, S(3)), (i * step + h, h)], fill=(175, 50, 55, 255), width=S(1))
        d.line([(i * step, h), (i * step + h, S(3))], fill=(175, 50, 55, 255), width=S(1))
    # Recorta lo que se sale del rectángulo interior
    mask = Image.new("L", (w, h), 0)
    ImageDraw.Draw(mask).rounded_rectangle([S(3), S(3), w - S(3), h - S(3)], radius=S(4), fill=255)
    base, _ = card_base(w, h)
    base.paste(img, (0, 0), mask)
    d = ImageDraw.Draw(base)
    d.rounded_rectangle([S(6), S(6), w - S(6), h - S(6)], radius=S(3), outline=GOLD, width=S(1))
    # Medallón central
    cx, cy = w / 2, h / 2
    d.ellipse([cx - S(15), cy - S(15), cx + S(15), cy + S(15)], fill=(110, 15, 22, 255), outline=GOLD, width=S(1.5))
    for i, s in enumerate(SUITS):
        a = -math.pi / 2 + i * math.pi / 2
        draw_suit(d, s, cx + math.cos(a) * S(8.5), cy + math.sin(a) * S(8.5), S(4.5))
    return base.resize((CARD_W, CARD_H), Image.LANCZOS)


def make_glow():
    w, h = CARD_W + 12, CARD_H + 12
    img = Image.new("RGBA", (w * SS, h * SS), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.rounded_rectangle([S(4), S(4), S(w - 4), S(h - 4)], radius=S(8), fill=(255, 230, 90, 255))
    img = img.filter(ImageFilter.GaussianBlur(S(2.5)))
    return img.resize((w, h), Image.LANCZOS)


def make_shadow():
    w, h = CARD_W + 8, CARD_H + 8
    img = Image.new("RGBA", (w * SS, h * SS), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.rounded_rectangle([S(4), S(4), S(w - 4), S(h - 4)], radius=S(7), fill=(0, 0, 0, 140))
    img = img.filter(ImageFilter.GaussianBlur(S(2)))
    return img.resize((w, h), Image.LANCZOS)


def make_chip(color, dark):
    n = 24
    img = Image.new("RGBA", (S(n), S(n)), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    c = S(n / 2)
    r = S(n / 2 - 1)
    d.ellipse([c - r, c - r, c + r, c + r], fill=color, outline=dark, width=S(1))
    for i in range(6):
        a = i * math.pi / 3
        x, y = c + math.cos(a) * r * 0.8, c + math.sin(a) * r * 0.8
        d.ellipse([x - S(2), y - S(2), x + S(2), y + S(2)], fill=(255, 255, 255, 255))
    r2 = r * 0.55
    d.ellipse([c - r2, c - r2, c + r2, c + r2], fill=color, outline=(255, 255, 255, 255), width=S(1))
    return img.resize((n, n), Image.LANCZOS)


def make_suit_icon(suit):
    n = 24
    img = Image.new("RGBA", (S(n), S(n)), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    draw_suit(d, suit, S(n / 2), S(n / 2), S(n / 2 - 1.5))
    return img.resize((n, n), Image.LANCZOS)


def make_logo():
    w, h = 320, 84
    img = Image.new("RGBA", (S(w), S(h)), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    f1 = font(S(34))
    f2 = font(S(15))
    t1 = "Baraja Española"
    t2 = "JUEGOS DE CARTAS"
    b1 = d.textbbox((0, 0), t1, font=f1)
    x1 = (S(w) - (b1[2] - b1[0])) / 2 - b1[0]
    y1 = S(6) - b1[1]
    # contorno grueso + sombra
    for dx in range(-S(3), S(3) + 1, S(1)):
        for dy in range(-S(3), S(3) + 1, S(1)):
            if dx * dx + dy * dy <= S(3) ** 2:
                d.text((x1 + dx, y1 + dy + S(2)), t1, font=f1, fill=(60, 10, 10, 255))
    # relleno con degradado dorado
    mask = Image.new("L", img.size, 0)
    ImageDraw.Draw(mask).text((x1, y1), t1, font=f1, fill=255)
    grad = Image.new("RGBA", img.size)
    gd = ImageDraw.Draw(grad)
    for y in range(S(h)):
        t = y / S(h * 0.55)
        t = min(1.0, max(0.0, t))
        col = (int(255 - 30 * t), int(236 - 90 * t), int(140 - 120 * t), 255)
        gd.line([(0, y), (S(w), y)], fill=col)
    img.paste(grad, (0, 0), mask)
    # subtítulo en una cinta roja
    b2 = d.textbbox((0, 0), t2, font=f2)
    tw = b2[2] - b2[0]
    rx0, ry0 = (S(w) - tw) / 2 - S(16), S(56)
    rx1, ry1 = (S(w) + tw) / 2 + S(16), S(78)
    d.polygon([(rx0 - S(10), ry0 + S(3)), (rx0, ry0 + S(3)), (rx0, ry1 + S(3)), (rx0 - S(10), ry1 + S(3)),
               (rx0 - S(5), (ry0 + ry1) / 2 + S(3))], fill=RED_DARK)
    d.polygon([(rx1 + S(10), ry0 + S(3)), (rx1, ry0 + S(3)), (rx1, ry1 + S(3)), (rx1 + S(10), ry1 + S(3)),
               (rx1 + S(5), (ry0 + ry1) / 2 + S(3))], fill=RED_DARK)
    d.rectangle([rx0, ry0, rx1, ry1], fill=RED, outline=GOLD, width=S(1))
    d.text(((S(w) - tw) / 2 - b2[0], (ry0 + ry1) / 2 - (b2[3] + b2[1]) / 2), t2, font=f2, fill=(255, 245, 220, 255))
    return img.resize((w, h), Image.LANCZOS)


# ---------------------------------------------------------------------------
# Fondos
# ---------------------------------------------------------------------------

def noise_layer(w, h, amount, seed):
    rnd = random.Random(seed)
    n = Image.new("L", (w, h))
    n.putdata([128 + rnd.randint(-amount, amount) for _ in range(w * h)])
    return n


def felt(w, h, base, seed, vignette=0.55):
    img = Image.new("RGB", (w, h), base)
    px = img.load()
    nz = noise_layer(w, h, 10, seed).filter(ImageFilter.GaussianBlur(0.6)).load()
    cx, cy = w / 2, h / 2
    maxd = math.hypot(cx, cy)
    for y in range(h):
        for x in range(w):
            dd = math.hypot(x - cx, (y - cy) * 1.3) / maxd
            v = 1.08 - vignette * dd * dd
            n = (nz[x, y] - 128) / 255.0
            r, g, b = base
            px[x, y] = (int(max(0, min(255, r * v + n * 40))),
                        int(max(0, min(255, g * v + n * 40))),
                        int(max(0, min(255, b * v + n * 40))))
    return img


def wood_strip(w, h, seed, horizontal=True):
    rnd = random.Random(seed)
    img = Image.new("RGB", (w, h))
    px = img.load()
    phase = [rnd.random() * 6 for _ in range(4)]
    for y in range(h):
        for x in range(w):
            u, v = (x, y) if horizontal else (y, x)
            grain = math.sin(v * 0.9 + math.sin(u * 0.05 + phase[0]) * 2.5 + phase[1]) * 0.5 + 0.5
            grain2 = math.sin(u * 0.013 + v * 0.3 + phase[2]) * 0.5 + 0.5
            t = 0.6 * grain + 0.4 * grain2
            px[x, y] = (int(110 + 50 * t), int(68 + 32 * t), int(30 + 18 * t))
    return img


def make_table_top():
    w, h = 400, 240
    img = felt(w, h, (28, 120, 62), 1)
    d = ImageDraw.Draw(img)
    # Línea decorativa del tapete
    d.rounded_rectangle([14, 14, w - 15, h - 15], radius=18, outline=(70, 160, 95), width=1)
    d.rounded_rectangle([17, 17, w - 18, h - 18], radius=16, outline=(20, 90, 45), width=1)
    # Borde de madera
    border = 7
    wood = wood_strip(w, h, 2)
    mask = Image.new("L", (w, h), 255)
    ImageDraw.Draw(mask).rounded_rectangle([border, border, w - 1 - border, h - 1 - border], radius=14, fill=0)
    img.paste(wood, (0, 0), mask)
    d.rounded_rectangle([border, border, w - 1 - border, h - 1 - border], radius=14, outline=(40, 22, 8), width=2)
    d.rounded_rectangle([1, 1, w - 2, h - 2], radius=4, outline=(200, 150, 90), width=1)
    return img


def make_table_bottom():
    w, h = 320, 240
    img = wood_strip(w, h, 7)
    d = ImageDraw.Draw(img)
    inner = felt(w - 12, h - 12 - 34, (22, 95, 50), 3, vignette=0.4)
    m = Image.new("L", inner.size, 0)
    ImageDraw.Draw(m).rounded_rectangle([0, 0, inner.size[0] - 1, inner.size[1] - 1], radius=10, fill=255)
    img.paste(inner, (6, 6), m)
    d.rounded_rectangle([6, 6, w - 7, h - 7 - 34], radius=10, outline=(40, 22, 8), width=2)
    # Bandeja inferior para botones
    d.rounded_rectangle([4, h - 38, w - 5, h - 4], radius=8, fill=(60, 36, 16), outline=(30, 16, 6), width=1)
    return img


def sunburst(w, h, c1, c2, rays=24, seed=0, center=None):
    img = Image.new("RGB", (w, h), c1)
    d = ImageDraw.Draw(img)
    cx, cy = center or (w / 2, h * 0.45)
    R = math.hypot(w, h)
    for i in range(rays):
        if i % 2:
            continue
        a0 = i * 2 * math.pi / rays
        a1 = (i + 1) * 2 * math.pi / rays
        d.polygon([(cx, cy), (cx + math.cos(a0) * R, cy + math.sin(a0) * R),
                   (cx + math.cos(a1) * R, cy + math.sin(a1) * R)], fill=c2)
    # Viñeta radial
    v = Image.new("L", (w, h))
    vp = v.load()
    maxd = math.hypot(max(cx, w - cx), max(cy, h - cy))
    for y in range(h):
        for x in range(w):
            dd = math.hypot(x - cx, y - cy) / maxd
            vp[x, y] = int(255 * min(1.0, dd * dd * 0.9))
    dark = Image.new("RGB", (w, h), (40, 5, 8))
    img = Image.composite(dark, img, v)
    return img


def make_title_top():
    img = sunburst(400, 240, (170, 28, 34), (192, 44, 48), rays=32)
    d = ImageDraw.Draw(img)
    # cenefa dorada inferior
    for i, y in enumerate(range(222, 240, 3)):
        d.line([(0, y), (400, y)], fill=(230, 170, 40) if i % 2 == 0 else (150, 90, 15))
    return img


def make_menu_bottom():
    img = sunburst(320, 240, (120, 18, 24), (135, 28, 34), rays=28, center=(160, -40))
    d = ImageDraw.Draw(img)
    for i, y in enumerate(range(0, 12, 3)):
        d.line([(0, y), (320, y)], fill=(230, 170, 40) if i % 2 == 0 else (150, 90, 15))
    return img


def make_icon():
    n = 48
    img = Image.new("RGBA", (S(n), S(n)), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.rounded_rectangle([0, 0, S(n) - 1, S(n) - 1], radius=S(8), fill=(28, 120, 62, 255), outline=WOOD_DARK, width=S(2))
    back = make_back().resize((S(22), S(33)), Image.LANCZOS).rotate(18, expand=True, resample=Image.BICUBIC)
    front = make_card("oros", 1).resize((S(22), S(33)), Image.LANCZOS).rotate(-12, expand=True, resample=Image.BICUBIC)
    img.alpha_composite(back, (S(4), S(5)))
    img.alpha_composite(front, (S(18), S(8)))
    return img.resize((n, n), Image.LANCZOS)


def write_t3s(name, files, fmt, border="none"):
    # Con -b transparent cada imagen lleva un borde transparente en el atlas, y
    # el filtrado lineal no "sangra" píxeles de las imágenes vecinas al escalar.
    with open(os.path.join(GFX, name), "w", newline="\n") as f:
        f.write("--atlas -f %s -z auto --border %s\n" % (fmt, border))
        for fn in files:
            f.write(fn + "\n")


def make_banner():
    """Imagen del banner del menú HOME para el .cia (256x128)."""
    w, h = 256, 128
    img = sunburst(w, h, (170, 28, 34), (192, 44, 48), rays=28, center=(w / 2, h * 0.55)).convert("RGBA")
    d = ImageDraw.Draw(img)
    for i, y in enumerate(range(h - 9, h, 3)):
        d.line([(0, y), (w, y)], fill=(230, 170, 40) if i % 2 == 0 else (150, 90, 15))
    # abanico de cartas a cada lado
    cards = [("oros", 1), ("copas", 12), ("espadas", 7), ("bastos", 10)]
    for k, (s, r) in enumerate(cards):
        c = make_card(s, r).resize((40, 60), Image.LANCZOS).rotate(18 - k * 12, expand=True, resample=Image.BICUBIC)
        x = 6 + k * 14 if k < 2 else w - 70 + (k - 2) * 14
        img.alpha_composite(c, (int(x), 56 - k % 2 * 4))
    logo = make_logo().resize((224, 59), Image.LANCZOS)
    img.alpha_composite(logo, ((w - 224) // 2, 6))
    return img


def main():
    os.makedirs(GFX, exist_ok=True)
    card_files = []
    for s in SUITS:
        for r in RANKS:
            fn = "card_%s_%02d.png" % (s, r)
            make_card(s, r).save(os.path.join(GFX, fn))
            card_files.append(fn)
    extras = [
        ("card_back.png", make_back()),
        ("card_glow.png", make_glow()),
        ("card_shadow.png", make_shadow()),
        ("chip_red.png", make_chip((200, 40, 45, 255), (110, 15, 20, 255))),
        ("chip_blue.png", make_chip((43, 91, 184, 255), (15, 35, 90, 255))),
        ("chip_green.png", make_chip((40, 150, 70, 255), (15, 70, 30, 255))),
        ("chip_black.png", make_chip((45, 45, 50, 255), (10, 10, 10, 255))),
    ]
    for s in SUITS:
        extras.append(("suit_%s.png" % s, make_suit_icon(s)))
    extras.append(("logo.png", make_logo()))
    # Ochos y nueves al final, para no mover los índices de las demás
    for s in SUITS:
        for r in (8, 9):
            extras.append(("card_%s_%02d.png" % (s, r), make_card(s, r)))
    for fn, im in extras:
        im.save(os.path.join(GFX, fn))
        card_files.append(fn)
    # El orden de este fichero define los índices usados en source/ui/assets.h
    write_t3s("sprites.t3s", card_files, "rgba8888", border="transparent")

    bgs = [
        ("bg_title_top.png", make_title_top()),
        ("bg_menu_bottom.png", make_menu_bottom()),
        ("bg_table_top.png", make_table_top()),
        ("bg_table_bottom.png", make_table_bottom()),
    ]
    for fn, im in bgs:
        im.save(os.path.join(GFX, fn))
    write_t3s("backgrounds.t3s", [fn for fn, _ in bgs], "rgb565")

    make_icon().save(os.path.join(ROOT, "icon.png"))
    os.makedirs(os.path.join(ROOT, "cia"), exist_ok=True)
    make_banner().save(os.path.join(ROOT, "cia", "banner.png"))
    print("Gráficos generados en", GFX)


if __name__ == "__main__":
    main()
