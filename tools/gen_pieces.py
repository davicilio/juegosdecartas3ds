#!/usr/bin/env python3
"""Genera los gráficos de los juegos de mesa (atlas gfx/pieces.t3s) y los
mapas de fondo de Conquista (atlas gfx/maps.t3s).

- Piezas de ajedrez (con las siluetas Staunton de la fuente DejaVu Sans).
- Dados del 1 al 6.
- Caras de las fichas de mahjong (en el orden del solitario: círculos,
  bambúes, caracteres, vientos norte/este/sur/oeste, dragones rojo/verde/
  blanco, flores y estaciones).
- Iconos de las casillas especiales de Negocio redondo.
- Mina y bandera del buscaminas.
- Bolas, soporte y cristal del bombo del bingo.
- Mapa de la península (mar, relieve y costa) con la misma proyección que
  usa source/scenes/conquista_scene.c.

Todo se dibuja a 4 aumentos y se reduce, para que los bordes salgan suaves.
El orden de los ficheros en pieces.t3s fija los índices de source/ui/gfx.h.
"""
import math
import os
import random

from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
GFX = os.path.join(ROOT, "gfx")
SS = 4  # superposición de muestras


def S(v):
    return int(round(v * SS))


def find_font(names):
    for n in names:
        for base in ("/usr/share/fonts/truetype/dejavu/", "/usr/share/fonts/TTF/", "C:/Windows/Fonts/"):
            p = os.path.join(base, n)
            if os.path.exists(p):
                return p
    return None


SANS = find_font(["DejaVuSans.ttf"])
SANS_BOLD = find_font(["DejaVuSans-Bold.ttf"])
SERIF_BOLD = find_font(["DejaVuSerif-Bold.ttf"])


def font(path, size):
    return ImageFont.truetype(path, size) if path else ImageFont.load_default()


def down(img, w, h):
    return img.resize((w, h), Image.LANCZOS)


def vgrad(w, h, top, bottom):
    g = Image.new("RGBA", (w, h))
    d = ImageDraw.Draw(g)
    for y in range(h):
        t = y / max(1, h - 1)
        d.line([(0, y), (w, y)], fill=tuple(int(top[i] + (bottom[i] - top[i]) * t) for i in range(4)))
    return g


def outline_mask(mask, radius):
    """Engorda una máscara (para contornos)."""
    return mask.filter(ImageFilter.MaxFilter(radius * 2 + 1))


# ---------------------------------------------------------------------------
# Ajedrez
# ---------------------------------------------------------------------------

# Siluetas rellenas (las "negras" de Unicode) para las dos colores
CHESS_GLYPH = {"P": "\u265F", "N": "\u265E", "B": "\u265D", "R": "\u265C", "Q": "\u265B", "K": "\u265A"}
# Los detalles interiores de las piezas blancas salen del glifo "blanco"
CHESS_OUTLINE = {"P": "\u2659", "N": "\u2658", "B": "\u2657", "R": "\u2656", "Q": "\u2655", "K": "\u2654"}


def glyph_mask(ch, size, n):
    f = font(SANS, S(size))
    m = Image.new("L", (S(n), S(n)), 0)
    d = ImageDraw.Draw(m)
    bb = d.textbbox((0, 0), ch, font=f)
    x = (S(n) - (bb[2] - bb[0])) / 2 - bb[0]
    y = (S(n) - (bb[3] - bb[1])) / 2 - bb[1] + S(0.5)
    d.text((x, y), ch, font=f, fill=255)
    return m


def make_chess(kind, white):
    n = 40
    sil = glyph_mask(CHESS_GLYPH[kind], 36, n)
    img = Image.new("RGBA", (S(n), S(n)), (0, 0, 0, 0))
    # Sombra suave
    shadow = sil.filter(ImageFilter.GaussianBlur(S(1.2)))
    sh = Image.new("RGBA", img.size, (0, 0, 0, 0))
    sh.putalpha(shadow.point(lambda v: v * 0.45))
    img.alpha_composite(sh, (S(1), S(1.5)))
    # Contorno
    edge = outline_mask(sil, S(0.9))
    col_edge = (40, 30, 25, 255) if white else (235, 225, 200, 255)
    layer = Image.new("RGBA", img.size, col_edge)
    img.paste(layer, (0, 0), edge)
    # Relleno con degradado (luz desde arriba a la izquierda)
    if white:
        fill = vgrad(S(n), S(n), (255, 252, 242, 255), (215, 200, 170, 255))
    else:
        fill = vgrad(S(n), S(n), (95, 85, 90, 255), (25, 20, 25, 255))
    img.paste(fill, (0, 0), sil)
    # Detalles: las líneas interiores del glifo blanco
    det = glyph_mask(CHESS_OUTLINE[kind], 36, n)
    inner = Image.eval(det, lambda v: v)  # trazos del dibujo
    lines = Image.new("RGBA", img.size, (60, 45, 35, 200) if white else (200, 185, 160, 170))
    # Solo los trazos que quedan dentro de la silueta y no son su borde
    eroded = sil.filter(ImageFilter.MinFilter(S(1.2) * 2 + 1))
    detail = Image.composite(inner, Image.new("L", img.size, 0), eroded)
    img.paste(lines, (0, 0), detail)
    # Brillo
    hl = Image.new("L", img.size, 0)
    ImageDraw.Draw(hl).ellipse([S(10), S(6), S(22), S(16)], fill=90 if white else 60)
    hl = Image.composite(hl.filter(ImageFilter.GaussianBlur(S(2))), Image.new("L", img.size, 0), eroded)
    img.paste(Image.new("RGBA", img.size, (255, 255, 255, 255)), (0, 0), hl)
    return down(img, n, n)


# ---------------------------------------------------------------------------
# Dados
# ---------------------------------------------------------------------------

PIPS = {1: [(0, 0)], 2: [(-1, -1), (1, 1)], 3: [(-1, -1), (0, 0), (1, 1)], 4: [(-1, -1), (1, -1), (-1, 1), (1, 1)],
        5: [(-1, -1), (1, -1), (0, 0), (-1, 1), (1, 1)], 6: [(-1, -1), (1, -1), (-1, 0), (1, 0), (-1, 1), (1, 1)]}


def make_die(v):
    n = 32
    img = Image.new("RGBA", (S(n), S(n)), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    # Sombra y canto
    sh = Image.new("L", img.size, 0)
    ImageDraw.Draw(sh).rounded_rectangle([S(3), S(4), S(31), S(31.5)], radius=S(6), fill=120)
    img.paste(Image.new("RGBA", img.size, (0, 0, 0, 255)), (0, 0), sh.filter(ImageFilter.GaussianBlur(S(1.2))))
    d.rounded_rectangle([S(1), S(1), S(29.5), S(29.5)], radius=S(6), fill=(200, 196, 188, 255))
    face = vgrad(S(n), S(n), (255, 255, 252, 255), (228, 224, 216, 255))
    m = Image.new("L", img.size, 0)
    ImageDraw.Draw(m).rounded_rectangle([S(1), S(1), S(28), S(27.5)], radius=S(5.5), fill=255)
    img.paste(face, (0, 0), m)
    d = ImageDraw.Draw(img)
    col = (200, 30, 40, 255) if v == 1 else (30, 30, 40, 255)
    r = 3.6 if v == 1 else 2.6
    for px, py in PIPS[v]:
        cx, cy = 14.5 + px * 7.2, 14.2 + py * 7.2
        d.ellipse([S(cx - r), S(cy - r + 0.5), S(cx + r), S(cy + r + 0.5)], fill=(255, 255, 255, 255))
        d.ellipse([S(cx - r), S(cy - r), S(cx + r), S(cy + r)], fill=col)
    return down(img, n, n)


# ---------------------------------------------------------------------------
# Mahjong
# ---------------------------------------------------------------------------

MJ_W, MJ_H = 24, 32
INK = (25, 30, 45, 255)
MJ_RED = (200, 30, 35, 255)
MJ_GREEN = (25, 125, 60, 255)
MJ_BLUE = (35, 70, 160, 255)


def tile_face():
    img = vgrad(S(MJ_W), S(MJ_H), (255, 253, 244, 255), (236, 228, 205, 255))
    d = ImageDraw.Draw(img)
    d.rounded_rectangle([S(0.5), S(0.5), S(MJ_W - 0.5), S(MJ_H - 0.5)], radius=S(2.5), outline=(180, 165, 130, 255),
                        width=S(0.6))
    return img, d


def circle_pip(d, cx, cy, r, color):
    d.ellipse([S(cx - r), S(cy - r), S(cx + r), S(cy + r)], fill=color)
    d.ellipse([S(cx - r * 0.72), S(cy - r * 0.72), S(cx + r * 0.72), S(cy + r * 0.72)], fill=(255, 250, 235, 255))
    d.ellipse([S(cx - r * 0.45), S(cy - r * 0.45), S(cx + r * 0.45), S(cy + r * 0.45)], fill=color)


PIN_POS = {
    1: [(12, 16)], 2: [(12, 9.5), (12, 22.5)], 3: [(6.5, 7.5), (12, 16), (17.5, 24.5)],
    4: [(7, 9.5), (17, 9.5), (7, 22.5), (17, 22.5)], 5: [(6.5, 7.5), (17.5, 7.5), (12, 16), (6.5, 24.5), (17.5, 24.5)],
    6: [(7.5, 6.5), (16.5, 6.5), (7.5, 16.5), (16.5, 16.5), (7.5, 25.5), (16.5, 25.5)],
    7: [(5.5, 5.5), (12, 8), (18.5, 10.5), (7.5, 18), (16.5, 18), (7.5, 26), (16.5, 26)],
    8: [(7.5, 5), (16.5, 5), (7.5, 12.3), (16.5, 12.3), (7.5, 19.6), (16.5, 19.6), (7.5, 27), (16.5, 27)],
    9: [(5, 6), (12, 6), (19, 6), (5, 16), (12, 16), (19, 16), (5, 26), (12, 26), (19, 26)],
}


def make_pin(n):
    img, d = tile_face()
    colors = [MJ_BLUE, MJ_GREEN, MJ_RED]
    r = {1: 8.5, 2: 5, 3: 4.2, 4: 4, 5: 3.8, 6: 3.5, 7: 3.1, 8: 3.0, 9: 2.8}[n]
    for k, (x, y) in enumerate(PIN_POS[n]):
        if n == 1:
            circle_pip(d, x, y, r, MJ_RED)
            d.ellipse([S(x - 2.2), S(y - 2.2), S(x + 2.2), S(y + 2.2)], fill=MJ_GREEN)
        else:
            col = MJ_RED if (n in (3, 5, 9) and k == len(PIN_POS[n]) // 2) or (n == 7 and k < 3) else colors[k % 2]
            circle_pip(d, x, y, r, col)
    return down(img, MJ_W, MJ_H)


def bamboo_stick(d, cx, cy, h, color):
    w = 2.6
    d.rounded_rectangle([S(cx - w / 2), S(cy - h / 2), S(cx + w / 2), S(cy + h / 2)], radius=S(1), fill=color)
    for t in (-0.5, 0.0, 0.5):
        y = cy + t * h * 0.9
        d.line([(S(cx - w / 2), S(y)), (S(cx + w / 2), S(y))], fill=(240, 240, 220, 255), width=S(0.5))


SOU_POS = {
    2: [(12, 9.5), (12, 22.5)], 3: [(12, 9.5), (7.5, 22.5), (16.5, 22.5)],
    4: [(7.5, 9.5), (16.5, 9.5), (7.5, 22.5), (16.5, 22.5)],
    5: [(6.5, 9.5), (17.5, 9.5), (12, 16), (6.5, 22.5), (17.5, 22.5)],
    6: [(5.5, 9.5), (12, 9.5), (18.5, 9.5), (5.5, 22.5), (12, 22.5), (18.5, 22.5)],
    7: [(12, 6), (5.5, 16), (12, 16), (18.5, 16), (5.5, 26), (12, 26), (18.5, 26)],
    8: [(5, 9.5), (9.7, 9.5), (14.3, 9.5), (19, 9.5), (5, 22.5), (9.7, 22.5), (14.3, 22.5), (19, 22.5)],
    9: [(5.5, 6), (12, 6), (18.5, 6), (5.5, 16), (12, 16), (18.5, 16), (5.5, 26), (12, 26), (18.5, 26)],
}


def make_sou(n):
    img, d = tile_face()
    if n == 1:
        # Un pájaro (el gorrión del 1 de bambú)
        d.ellipse([S(6), S(9), S(17), S(21)], fill=MJ_GREEN)
        d.ellipse([S(13), S(6), S(19), S(12)], fill=MJ_RED)
        d.polygon([(S(18.5), S(8.5)), (S(22), S(9.5)), (S(18.5), S(10.5))], fill=(220, 160, 30, 255))
        d.ellipse([S(15.5), S(7.8), S(16.8), S(9.1)], fill=(255, 255, 255, 255))
        d.polygon([(S(7), S(17)), (S(2), S(26)), (S(6), S(24)), (S(9), S(27)), (S(11), S(20))], fill=MJ_BLUE)
        d.line([(S(11), S(21)), (S(10), S(27))], fill=(150, 90, 30, 255), width=S(0.8))
        d.line([(S(13), S(21)), (S(14), S(27))], fill=(150, 90, 30, 255), width=S(0.8))
        return down(img, MJ_W, MJ_H)
    h = 10.5 if n in (2, 3, 4, 5, 6, 8) else 8
    for k, (x, y) in enumerate(SOU_POS[n]):
        red = (n == 5 and k == 2) or (n == 7 and k == 0) or (n == 9 and k in (1, 4, 7))
        bamboo_stick(d, x, y, h, MJ_RED if red else MJ_GREEN)
    return down(img, MJ_W, MJ_H)


def kanji_wan(d, cx, cy, s, color):
    """Dibuja el carácter 万 (diez mil) con trazos."""
    w = S(1.4 * s)
    d.line([(S(cx - 5 * s), S(cy - 4 * s)), (S(cx + 5 * s), S(cy - 4 * s))], fill=color, width=w)
    d.line([(S(cx - 1.5 * s), S(cy - 4 * s)), (S(cx - 2.5 * s), S(cy + 1 * s)), (S(cx - 5 * s), S(cy + 5 * s))],
           fill=color, width=w)
    d.line([(S(cx - 2 * s), S(cy - 0.5 * s)), (S(cx + 4 * s), S(cy - 0.5 * s)), (S(cx + 3.5 * s), S(cy + 4.5 * s)),
            (S(cx + 1.5 * s), S(cy + 4 * s))], fill=color, width=w)


CHINESE_NUM = {1: "一", 2: "二", 3: "三"}


def make_man(n):
    img, d = tile_face()
    # Número arriba: 1-3 con trazos horizontales y el resto en cifras
    if n <= 3:
        w = S(1.6)
        for k in range(n):
            y = 9 - (n - 1) * 1.8 + k * 3.6
            ln = 10 if k == n - 1 else 8 - k * 0.5
            d.line([(S(12 - ln / 2), S(y)), (S(12 + ln / 2), S(y))], fill=INK, width=w)
    else:
        f = font(SERIF_BOLD, S(12))
        txt = str(n)
        bb = d.textbbox((0, 0), txt, font=f)
        d.text((S(12) - (bb[2] - bb[0]) / 2 - bb[0], S(9) - (bb[3] - bb[1]) / 2 - bb[1]), txt, font=f, fill=INK)
    kanji_wan(d, 12, 22, 1.0, MJ_RED)
    return down(img, MJ_W, MJ_H)


def make_wind(letter):
    img, d = tile_face()
    f = font(SERIF_BOLD, S(17))
    bb = d.textbbox((0, 0), letter, font=f)
    x = S(12) - (bb[2] - bb[0]) / 2 - bb[0]
    y = S(16) - (bb[3] - bb[1]) / 2 - bb[1]
    d.text((x + S(0.6), y + S(0.6)), letter, font=f, fill=(150, 160, 190, 255))
    d.text((x, y), letter, font=f, fill=(20, 30, 70, 255))
    return down(img, MJ_W, MJ_H)


def make_dragon(which):
    img, d = tile_face()
    if which == "rojo":
        # 中: un rectángulo atravesado por un trazo vertical
        d.rectangle([S(5.5), S(10), S(18.5), S(20)], outline=MJ_RED, width=S(2))
        d.line([(S(12), S(5)), (S(12), S(27))], fill=MJ_RED, width=S(2.2))
    elif which == "verde":
        # Un brote verde estilizado (el dragón verde)
        for ang in (-50, -15, 15, 50):
            a = math.radians(ang - 90)
            x1, y1 = 12 + math.cos(a) * 9, 18 + math.sin(a) * 11
            d.line([(S(12), S(25)), (S(x1), S(y1))], fill=MJ_GREEN, width=S(2))
            d.ellipse([S(x1 - 2.2), S(y1 - 2.2), S(x1 + 2.2), S(y1 + 2.2)], fill=MJ_GREEN)
        d.line([(S(7), S(26)), (S(17), S(26))], fill=MJ_GREEN, width=S(2))
    else:
        # Blanco: marco azul doble
        d.rectangle([S(4.5), S(5), S(19.5), S(27)], outline=MJ_BLUE, width=S(1.6))
        d.rectangle([S(7.5), S(8), S(16.5), S(24)], outline=MJ_BLUE, width=S(0.8))
    return down(img, MJ_W, MJ_H)


def make_flower(k):
    img, d = tile_face()
    petal = [(232, 110, 160, 255), (232, 180, 40, 255), (154, 90, 210, 255), (232, 120, 42, 255)][k]
    d.line([(S(12), S(17)), (S(12), S(28))], fill=MJ_GREEN, width=S(1.4))
    d.ellipse([S(12), S(21), S(17), S(24)], fill=MJ_GREEN)
    for i in range(5):
        a = i * 2 * math.pi / 5 - math.pi / 2
        x, y = 12 + math.cos(a) * 4.2, 12 + math.sin(a) * 4.2
        d.ellipse([S(x - 3.2), S(y - 3.2), S(x + 3.2), S(y + 3.2)], fill=petal)
    d.ellipse([S(10), S(10), S(14), S(14)], fill=(250, 220, 60, 255))
    f = font(SANS_BOLD, S(6))
    d.text((S(2), S(1)), str(k + 1), font=f, fill=MJ_RED)
    return down(img, MJ_W, MJ_H)


def make_season(k):
    img, d = tile_face()
    col = [(60, 160, 70, 255), (232, 196, 40, 255), (210, 120, 42, 255), (90, 170, 230, 255)][k]
    if k == 0:  # primavera: brote
        d.ellipse([S(6), S(10), S(13), S(17)], fill=col)
        d.ellipse([S(11), S(8), S(18), S(15)], fill=col)
        d.line([(S(12), S(14)), (S(12), S(27))], fill=(110, 80, 40, 255), width=S(1.5))
    elif k == 1:  # verano: sol
        d.ellipse([S(7), S(10), S(17), S(20)], fill=col)
        for i in range(8):
            a = i * math.pi / 4
            d.line([(S(12 + math.cos(a) * 7), S(15 + math.sin(a) * 7)), (S(12 + math.cos(a) * 10), S(15 + math.sin(a) * 10))],
                   fill=col, width=S(1.3))
    elif k == 2:  # otoño: hoja
        d.polygon([(S(12), S(6)), (S(19), S(14)), (S(16), S(22)), (S(12), S(25)), (S(8), S(22)), (S(5), S(14))], fill=col)
        d.line([(S(12), S(8)), (S(12), S(28))], fill=(120, 60, 20, 255), width=S(1))
    else:  # invierno: copo
        for i in range(3):
            a = i * math.pi / 3
            d.line([(S(12 - math.cos(a) * 8), S(16 - math.sin(a) * 8)), (S(12 + math.cos(a) * 8), S(16 + math.sin(a) * 8))],
                   fill=col, width=S(1.6))
        d.ellipse([S(10), S(14), S(14), S(18)], fill=col)
    f = font(SANS_BOLD, S(6))
    d.text((S(2), S(1)), str(k + 1), font=f, fill=MJ_BLUE)
    return down(img, MJ_W, MJ_H)


def mahjong_faces():
    faces = []
    faces += [make_pin(n) for n in range(1, 10)]          # 0..8 círculos
    faces += [make_sou(n) for n in range(1, 10)]          # 9..17 bambúes
    faces += [make_man(n) for n in range(1, 10)]          # 18..26 caracteres
    faces += [make_wind(c) for c in ("N", "E", "S", "O")]  # 27..30 vientos
    faces += [make_dragon(w) for w in ("rojo", "verde", "blanco")]  # 31..33
    faces += [make_flower(k) for k in range(4)]           # 34..37
    faces += [make_season(k) for k in range(4)]           # 38..41
    return faces


# ---------------------------------------------------------------------------
# Negocio redondo
# ---------------------------------------------------------------------------

def icon_canvas():
    return Image.new("RGBA", (S(32), S(32)), (0, 0, 0, 0))


def make_icon_start():
    img = icon_canvas()
    d = ImageDraw.Draw(img)
    pts = [(4, 13), (18, 13), (18, 6), (29, 16), (18, 26), (18, 19), (4, 19)]
    d.polygon([(S(x + 0.8), S(y + 1)) for x, y in pts], fill=(0, 0, 0, 90))
    d.polygon([(S(x), S(y)) for x, y in pts], fill=(30, 150, 60, 255), outline=(15, 80, 30, 255))
    return down(img, 32, 32)


def make_icon_luck():
    img = icon_canvas()
    d = ImageDraw.Draw(img)
    d.rounded_rectangle([S(7), S(4), S(26), S(29)], radius=S(3), fill=(0, 0, 0, 80))
    d.rounded_rectangle([S(6), S(3), S(25), S(28)], radius=S(3), fill=(130, 70, 190, 255), outline=(70, 30, 110, 255),
                        width=S(1))
    d.rounded_rectangle([S(8.5), S(5.5), S(22.5), S(25.5)], radius=S(2), outline=(230, 200, 255, 255), width=S(0.8))
    f = font(SERIF_BOLD, S(17))
    bb = d.textbbox((0, 0), "?", font=f)
    d.text((S(15.5) - (bb[2] - bb[0]) / 2 - bb[0], S(15.5) - (bb[3] - bb[1]) / 2 - bb[1]), "?", font=f,
           fill=(255, 230, 90, 255))
    return down(img, 32, 32)


def make_icon_tax():
    img = icon_canvas()
    d = ImageDraw.Draw(img)
    # Un edificio con columnas
    d.polygon([(S(16), S(3)), (S(29), S(11)), (S(3), S(11))], fill=(150, 150, 160, 255), outline=(70, 70, 80, 255))
    for k in range(4):
        x = 6 + k * 6.3
        d.rectangle([S(x), S(12), S(x + 2.8), S(24)], fill=(225, 225, 230, 255), outline=(90, 90, 100, 255))
    d.rectangle([S(3), S(24), S(29), S(28)], fill=(150, 150, 160, 255), outline=(70, 70, 80, 255))
    # Moneda
    d.ellipse([S(19), S(17), S(30), S(28)], fill=(240, 190, 40, 255), outline=(150, 100, 10, 255), width=S(1))
    f = font(SANS_BOLD, S(8))
    d.text((S(22.2), S(17.6)), "€", font=f, fill=(150, 100, 10, 255))
    return down(img, 32, 32)


def make_icon_works():
    img = icon_canvas()
    d = ImageDraw.Draw(img)
    # Cono de obras
    d.polygon([(S(16), S(3)), (S(23), S(25)), (S(9), S(25))], fill=(240, 120, 20, 255))
    d.polygon([(S(13.6), S(10.5)), (S(18.4), S(10.5)), (S(19.5), S(14)), (S(12.5), S(14))], fill=(255, 255, 255, 255))
    d.polygon([(S(11.4), S(17.5)), (S(20.6), S(17.5)), (S(21.7), S(21)), (S(10.3), S(21))], fill=(255, 255, 255, 255))
    d.rectangle([S(5), S(25), S(27), S(28.5)], fill=(200, 90, 15, 255))
    return down(img, 32, 32)


def make_icon_rest():
    img = icon_canvas()
    d = ImageDraw.Draw(img)
    # Árbol y banco de la plaza
    d.rectangle([S(14.5), S(15), S(17.5), S(27)], fill=(120, 75, 35, 255))
    d.ellipse([S(6), S(3), S(26), S(19)], fill=(40, 140, 60, 255))
    d.ellipse([S(9), S(5), S(18), S(12)], fill=(90, 190, 90, 255))
    d.rectangle([S(3), S(24), S(29), S(26)], fill=(150, 95, 45, 255))
    d.rectangle([S(5), S(26), S(6.5), S(29)], fill=(80, 60, 50, 255))
    d.rectangle([S(25.5), S(26), S(27), S(29)], fill=(80, 60, 50, 255))
    return down(img, 32, 32)


# ---------------------------------------------------------------------------
# Buscaminas
# ---------------------------------------------------------------------------

def make_mine():
    img = Image.new("RGBA", (S(24), S(24)), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    for a in range(0, 360, 45):
        r = math.radians(a)
        d.line([(S(12 + math.cos(r) * 4), S(12 + math.sin(r) * 4)), (S(12 + math.cos(r) * 10.5), S(12 + math.sin(r) * 10.5))],
               fill=(25, 25, 30, 255), width=S(1.8))
    d.ellipse([S(5), S(5), S(19), S(19)], fill=(25, 25, 30, 255))
    d.ellipse([S(8), S(7.5), S(12), S(11.5)], fill=(255, 255, 255, 230))
    return down(img, 24, 24)


def make_flag():
    img = Image.new("RGBA", (S(24), S(24)), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.polygon([(S(6), S(21)), (S(18), S(21)), (S(16), S(18)), (S(8), S(18))], fill=(40, 40, 45, 255))
    d.rectangle([S(11.2), S(3), S(12.8), S(19)], fill=(40, 40, 45, 255))
    d.polygon([(S(12.8), S(3)), (S(12.8), S(12)), (S(3), S(7.5))], fill=(220, 30, 35, 255))
    d.polygon([(S(12.8), S(3)), (S(12.8), S(6)), (S(6), S(6.5))], fill=(250, 90, 90, 255))
    return down(img, 24, 24)


# ---------------------------------------------------------------------------
# Bingo: bolas, soporte del bombo y brillo del cristal
# ---------------------------------------------------------------------------

BALL_COLORS = [(216, 58, 58), (224, 122, 30), (224, 184, 30), (76, 168, 76), (46, 156, 156), (46, 108, 200),
               (106, 76, 192), (176, 76, 176), (112, 112, 112)]


def make_ball(rgb):
    n = 48
    img = Image.new("RGBA", (S(n), S(n)), (0, 0, 0, 0))
    c = S(n) / 2
    r = S(22)
    # Esfera con luz arriba a la izquierda (sombreado radial por píxel)
    px = img.load()
    lx, ly = c - r * 0.4, c - r * 0.45
    for y in range(S(n)):
        for x in range(S(n)):
            dx, dy = x - c, y - c
            if dx * dx + dy * dy > r * r:
                continue
            dl = math.hypot(x - lx, y - ly) / (r * 1.6)
            k = max(0.25, 1.15 - dl)
            px[x, y] = (min(255, int(rgb[0] * k + 30 * (1 - dl))), min(255, int(rgb[1] * k + 30 * (1 - dl))),
                        min(255, int(rgb[2] * k + 30 * (1 - dl))), 255)
    d = ImageDraw.Draw(img)
    # Círculo blanco del número, algo abombado
    w = r * 0.66
    disc = vgrad(S(n), S(n), (255, 255, 255, 255), (225, 225, 220, 255))
    m = Image.new("L", img.size, 0)
    ImageDraw.Draw(m).ellipse([c - w, c - w, c + w, c + w], fill=255)
    img.paste(disc, (0, 0), m)
    d.ellipse([c - w, c - w, c + w, c + w], outline=(0, 0, 0, 60), width=S(0.6))
    # Brillo
    hl = Image.new("L", img.size, 0)
    ImageDraw.Draw(hl).ellipse([c - r * 0.75, c - r * 0.85, c - r * 0.2, c - r * 0.5], fill=200)
    img.paste(Image.new("RGBA", img.size, (255, 255, 255, 255)), (0, 0), hl.filter(ImageFilter.GaussianBlur(S(1.5))))
    return down(img, n, n)


def make_drum_stand():
    """Soporte del bombo: dos patas desde los ejes (arriba, a 11 px de cada
    lado y 6 px de alto) hasta una base que queda por debajo de la esfera."""
    w, h = 160, 100
    img = Image.new("RGBA", (S(w), S(h)), (0, 0, 0, 0))
    gold = vgrad(S(w), S(h), (255, 222, 125, 255), (140, 88, 22, 255))
    m = Image.new("L", img.size, 0)
    md = ImageDraw.Draw(m)
    md.polygon([(S(6), S(6)), (S(16), S(6)), (S(52), S(84)), (S(38), S(84))], fill=255)
    md.polygon([(S(w - 6), S(6)), (S(w - 16), S(6)), (S(w - 52), S(84)), (S(w - 38), S(84))], fill=255)
    md.rounded_rectangle([S(22), S(82), S(w - 22), S(95)], radius=S(5), fill=255)
    md.ellipse([S(3), S(0), S(19), S(12)], fill=255)
    md.ellipse([S(w - 19), S(0), S(w - 3), S(12)], fill=255)
    sh = m.filter(ImageFilter.GaussianBlur(S(2)))
    img.paste(Image.new("RGBA", img.size, (0, 0, 0, 255)), (S(2), S(3)), sh.point(lambda v: v * 0.45))
    img.paste(gold, (0, 0), m)
    d = ImageDraw.Draw(img)
    d.rounded_rectangle([S(22), S(82), S(w - 22), S(95)], radius=S(5), outline=(110, 70, 20, 255), width=S(1))
    d.line([(S(27), S(85)), (S(w - 27), S(85))], fill=(255, 240, 190, 255), width=S(1))
    for cx in (11, w - 11):
        d.ellipse([S(cx - 4), S(2), S(cx + 4), S(10)], fill=(120, 80, 25, 255))
    # Manivela a la derecha
    d.line([(S(w - 11), S(6)), (S(w - 2), S(22))], fill=(120, 80, 25, 255), width=S(2.5))
    d.ellipse([S(w - 6), S(19), S(w), S(27)], fill=(200, 40, 40, 255))
    return down(img, w, h)


def make_drum_glass():
    n = 128
    img = Image.new("RGBA", (S(n), S(n)), (0, 0, 0, 0))
    c, r = S(n) / 2, S(n) / 2 - S(1)
    px = img.load()
    for y in range(S(n)):
        for x in range(S(n)):
            dx, dy = (x - c) / r, (y - c) / r
            dd = dx * dx + dy * dy
            if dd > 1:
                continue
            # Más opaco en el borde (efecto de cristal esférico) y un reflejo
            edge = dd ** 3
            spec = max(0.0, 1 - math.hypot(dx + 0.42, dy + 0.5) / 0.28)
            a = int(25 + 110 * edge + 170 * spec)
            px[x, y] = (230, 245, 255, min(255, a))
    return down(img, n, n)


# ---------------------------------------------------------------------------
# Mapa de la península para Conquista
# ---------------------------------------------------------------------------

COAST = [(-9.3, 43.0), (-8.9, 43.4), (-8.2, 43.7), (-7.0, 43.6), (-5.8, 43.6), (-4.5, 43.45), (-3.5, 43.5),
         (-2.5, 43.45), (-1.8, 43.4), (-1.4, 43.1), (-0.5, 42.8), (0.7, 42.85), (1.7, 42.5), (3.2, 42.45),
         (3.2, 41.9), (2.3, 41.4), (1.0, 41.05), (0.8, 40.7), (0.0, 40.0), (-0.3, 39.5), (0.2, 38.8), (-0.5, 38.3),
         (-0.8, 37.6), (-1.6, 37.3), (-2.1, 36.75), (-3.0, 36.75), (-4.4, 36.7), (-5.3, 36.1), (-6.0, 36.3),
         (-6.4, 36.8), (-7.4, 37.2), (-8.9, 37.0), (-8.8, 38.0), (-9.5, 38.8), (-9.0, 40.0), (-8.7, 41.0),
         (-8.9, 41.9), (-8.9, 42.5)]
# Frontera con Portugal (del Miño al Guadiana)
PORTUGAL = [(-8.9, 41.9), (-8.1, 41.85), (-7.2, 41.9), (-6.6, 41.95), (-6.2, 41.6), (-6.8, 41.0), (-6.9, 40.3),
            (-7.0, 39.7), (-7.3, 39.4), (-7.0, 38.8), (-7.2, 38.2), (-7.5, 37.6), (-7.4, 37.2)]
MALLORCA = [(2.35, 39.6), (2.7, 39.95), (3.2, 39.95), (3.45, 39.7), (3.25, 39.35), (2.8, 39.35), (2.55, 39.45)]
# Canarias en un recuadro, alrededor de sus fichas
CANARIAS_BOX = (-9.0, 36.1, -8.0, 36.0)


def project(k, ox, oy, lon, lat):
    return ox + (lon + 9.6) * k, oy + (43.9 - lat) * k * 1.12


def smooth(points, passes=2):
    """Suaviza un polígono cerrado (Chaikin)."""
    pts = points
    for _ in range(passes):
        out = []
        for i in range(len(pts)):
            a, b = pts[i], pts[(i + 1) % len(pts)]
            out.append((a[0] * 0.75 + b[0] * 0.25, a[1] * 0.75 + b[1] * 0.25))
            out.append((a[0] * 0.25 + b[0] * 0.75, a[1] * 0.25 + b[1] * 0.75))
        pts = out
    return pts


def make_map(w, h, k, ox, oy, bar=0):
    W, H = S(w), S(h)
    rnd = random.Random(7)
    # Mar con degradado y algo de oleaje
    img = vgrad(W, H, (70, 130, 190, 255), (40, 90, 150, 255))
    d = ImageDraw.Draw(img)
    for _ in range(int(w * h / 900)):
        x, y = rnd.uniform(0, W), rnd.uniform(0, H)
        d.arc([x, y, x + S(6), y + S(3)], 200, 340, fill=(140, 190, 230, 120), width=S(0.6))
    P = lambda lon, lat: tuple(v * SS for v in project(k, ox, oy, lon, lat))
    coast = smooth([P(*p) for p in COAST])
    # Aguas someras junto a la costa
    shallow = Image.new("L", (W, H), 0)
    ImageDraw.Draw(shallow).polygon(coast, fill=255)
    shallow = shallow.filter(ImageFilter.MaxFilter(S(4) * 2 + 1)).filter(ImageFilter.GaussianBlur(S(3)))
    img.paste(Image.new("RGBA", (W, H), (120, 180, 215, 255)), (0, 0), shallow.point(lambda v: v * 0.7))
    # Tierra con relieve (ruido suave) y sombra hacia el sureste
    land = Image.new("L", (W, H), 0)
    ImageDraw.Draw(land).polygon(coast, fill=255)
    for isle in (MALLORCA,):
        ImageDraw.Draw(land).polygon(smooth([P(*p) for p in isle]), fill=255)
    shadow = land.filter(ImageFilter.GaussianBlur(S(2)))
    img.paste(Image.new("RGBA", (W, H), (20, 40, 70, 255)), (S(1.5), S(2)), shadow.point(lambda v: v * 0.5))
    noise = Image.new("L", (w // 4 + 2, h // 4 + 2))
    noise.putdata([rnd.randint(0, 255) for _ in range((w // 4 + 2) * (h // 4 + 2))])
    noise = noise.resize((W, H), Image.BICUBIC).filter(ImageFilter.GaussianBlur(S(4)))
    # Más verde al norte y más ocre al sur, con un relieve suave
    _, ytop = P(0, 43.8)
    _, ybot = P(0, 36.0)
    base = Image.new("RGBA", (W, H))
    bd = ImageDraw.Draw(base)
    for y in range(H):
        t = min(1.0, max(0.0, (y - ytop) / (ybot - ytop)))
        col = (int(178 + 52 * t), int(196 + 14 * t), int(130 + 25 * t), 255)
        bd.line([(0, y), (W, y)], fill=col)
    hills = Image.new("RGBA", (W, H), (175, 160, 115, 255))
    terrain = Image.composite(hills, base, noise.point(lambda v: int(max(0, v - 150) * 1.2)))
    img.paste(terrain, (0, 0), land)
    # Portugal, más apagado
    pt = [P(*p) for p in PORTUGAL] + [P(-9.6, 37.0), P(-9.6, 42.0)]
    pmask = Image.new("L", (W, H), 0)
    ImageDraw.Draw(pmask).polygon(pt, fill=255)
    pmask = Image.composite(pmask, Image.new("L", (W, H), 0), land)
    img.paste(Image.new("RGBA", (W, H), (200, 196, 175, 255)), (0, 0), pmask)
    d = ImageDraw.Draw(img)
    d.line([P(*p) for p in PORTUGAL], fill=(150, 130, 100, 255), width=S(0.8), joint="curve")
    # Línea de costa
    d.line(coast + [coast[0]], fill=(90, 70, 45, 255), width=S(0.9), joint="curve")
    d.line(smooth([P(*p) for p in MALLORCA]) + [smooth([P(*p) for p in MALLORCA])[0]], fill=(90, 70, 45, 255),
           width=S(0.8))
    # Recuadro de Canarias
    x0, y0 = P(-9.0 - 0.75, 36.1 + 0.62)
    x1, y1 = P(-8.0 + 0.75, 36.0 - 0.7)
    d.rectangle([x0, y0, x1, y1], fill=(60, 120, 180, 255), outline=(230, 220, 190, 255), width=S(1))
    for lon, lat, rx, ry in ((-9.0, 36.1, 0.42, 0.3), (-8.0, 36.0, 0.35, 0.3)):
        cx, cy = P(lon, lat)
        d.ellipse([cx - rx * k * SS, cy - ry * k * SS, cx + rx * k * SS, cy + ry * k * SS], fill=(222, 208, 165, 255),
                  outline=(90, 70, 45, 255), width=S(0.7))
    f = font(SANS_BOLD, S(7))
    d.text((x0 + S(2), y0 + S(1)), "Canarias", font=f, fill=(235, 235, 220, 255))
    img = down(img, w, h)
    return img.convert("RGB")


# ---------------------------------------------------------------------------

def main():
    os.makedirs(GFX, exist_ok=True)
    files = []

    def add(name, im):
        im.save(os.path.join(GFX, name))
        files.append(name)

    for white in (True, False):
        for kind in "PNBRQK":
            add("piece_chess_%s%s.png" % ("w" if white else "b", kind.lower()), make_chess(kind, white))
    for v in range(1, 7):
        add("piece_die_%d.png" % v, make_die(v))
    for i, im in enumerate(mahjong_faces()):
        add("piece_mj_%02d.png" % i, im)
    add("piece_ng_start.png", make_icon_start())
    add("piece_ng_luck.png", make_icon_luck())
    add("piece_ng_tax.png", make_icon_tax())
    add("piece_ng_works.png", make_icon_works())
    add("piece_ng_rest.png", make_icon_rest())
    add("piece_mine.png", make_mine())
    add("piece_flag.png", make_flag())
    for i, col in enumerate(BALL_COLORS):
        add("piece_ball_%d.png" % i, make_ball(col))
    add("piece_drum_stand.png", make_drum_stand())
    add("piece_drum_glass.png", make_drum_glass())
    with open(os.path.join(GFX, "pieces.t3s"), "w", newline="\n") as f:
        f.write("--atlas -f rgba8888 -z auto --border transparent\n")
        for fn in files:
            f.write(fn + "\n")

    # Mapas de Conquista: misma proyección que conquista_scene.c
    make_map(400, 240, 25.0, 4.0, 0.0).save(os.path.join(GFX, "map_top.png"))
    make_map(320, 240, 22.5, 12.0, 34.0, bar=34).save(os.path.join(GFX, "map_bottom.png"))
    with open(os.path.join(GFX, "maps.t3s"), "w", newline="\n") as f:
        f.write("--atlas -f rgb565 -z auto\n")
        f.write("map_top.png\nmap_bottom.png\n")
    print("Piezas y mapas generados en", GFX, "(%d piezas)" % len(files))


if __name__ == "__main__":
    main()
