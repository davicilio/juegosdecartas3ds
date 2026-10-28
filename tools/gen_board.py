#!/usr/bin/env python3
"""Genera los gráficos de los juegos de mesa que no usan la baraja española:
las cartas de colores del Uno y las fichas del Rummikub (gfx/board_*.png y el
atlas gfx/board.t3s). Son diseños propios, sin logotipos.

El orden del atlas define los índices BOARD_* de source/ui/gfx.h.

    python3 tools/gen_board.py
"""
import math
import os

from PIL import Image, ImageDraw

from gen_assets import CARD_H, CARD_W, GFX, SS, font

UNO_COLORS = [(220, 40, 45), (245, 196, 30), (60, 168, 70), (40, 100, 200)]
UNO_DARK = [(140, 20, 25), (170, 120, 10), (25, 100, 35), (20, 55, 130)]
WHITE = (255, 255, 255, 255)
INK = (30, 30, 35, 255)


def S(v):
    return int(round(v * SS))


def base_card(fill):
    w, h = S(CARD_W), S(CARD_H)
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.rounded_rectangle([0, 0, w - 1, h - 1], radius=S(7), fill=WHITE, outline=INK, width=S(1))
    d.rounded_rectangle([S(4), S(4), w - S(4), h - S(4)], radius=S(5), fill=fill)
    return img, d


def center_oval(img, color=WHITE):
    w, h = img.size
    o = Image.new("RGBA", img.size, (0, 0, 0, 0))
    ImageDraw.Draw(o).ellipse([w * 0.14, h * 0.22, w * 0.86, h * 0.78], fill=color)
    o = o.rotate(-28, center=(w / 2, h / 2), resample=Image.BICUBIC)
    img.alpha_composite(o)


def outlined_text(d, xy, text, size, fill, outline=INK, width=2, anchor="mm"):
    f = font(S(size))
    d.text(xy, text, font=f, fill=fill, anchor=anchor, stroke_width=S(width * 0.5), stroke_fill=outline)


def symbol_skip(d, cx, cy, r, color, width):
    d.ellipse([cx - r, cy - r, cx + r, cy + r], outline=color, width=width)
    a = math.radians(45)
    d.line([cx - r * math.cos(a), cy + r * math.sin(a), cx + r * math.cos(a), cy - r * math.sin(a)], fill=color,
           width=width)


def symbol_reverse(d, cx, cy, r, color, width):
    for sgn in (1, -1):
        pts = [(cx - sgn * r * 0.8, cy + sgn * r * 0.25), (cx + sgn * r * 0.45, cy + sgn * r * 0.25)]
        d.line(pts, fill=color, width=width)
        tip = (cx + sgn * r * 0.9, cy + sgn * r * 0.25)
        d.polygon([tip, (cx + sgn * r * 0.35, cy + sgn * r * 0.25 - r * 0.38),
                   (cx + sgn * r * 0.35, cy + sgn * r * 0.25 + r * 0.38)], fill=color)


def label(value):
    return {10: "", 11: "", 12: "+2", 13: "", 14: "+4"}.get(value, str(value))


def draw_symbol(d, value, cx, cy, r, color, big):
    width = S(3.2 if big else 1.6)
    if value == 10:
        symbol_skip(d, cx, cy, r, color, width)
    elif value == 11:
        symbol_reverse(d, cx, cy, r, color, width)
    else:
        outlined_text(d, (cx, cy), label(value), r * 2.2 / SS if big else r * 2.0 / SS, color,
                      width=2.4 if big else 1.2)


def make_uno(color, value):
    img, d = base_card(UNO_COLORS[color])
    center_oval(img)
    d = ImageDraw.Draw(img)
    w, h = img.size
    big = UNO_COLORS[color]
    draw_symbol(d, value, w / 2, h / 2, S(15), big + (255,), True)
    # índices en las esquinas
    for (x, y, rot) in [(S(12), S(14), 0), (w - S(12), h - S(14), 180)]:
        layer = Image.new("RGBA", img.size, (0, 0, 0, 0))
        ld = ImageDraw.Draw(layer)
        draw_symbol(ld, value, x, y, S(6), WHITE, False)
        if rot:
            layer = layer.rotate(180, center=(x, y), resample=Image.BICUBIC)
        img.alpha_composite(layer)
    return img.resize((CARD_W, CARD_H), Image.LANCZOS)


def four_color_oval(img):
    w, h = img.size
    o = Image.new("RGBA", img.size, (0, 0, 0, 0))
    od = ImageDraw.Draw(o)
    box = [w * 0.14, h * 0.22, w * 0.86, h * 0.78]
    for k, col in enumerate(UNO_COLORS):
        od.pieslice(box, k * 90 - 90, k * 90, fill=col + (255,))
    od.ellipse(box, outline=WHITE, width=S(1.5))
    o = o.rotate(-28, center=(w / 2, h / 2), resample=Image.BICUBIC)
    img.alpha_composite(o)


def make_wild(draw4):
    img, d = base_card((25, 25, 30))
    four_color_oval(img)
    d = ImageDraw.Draw(img)
    w, h = img.size
    if draw4:
        outlined_text(d, (w / 2, h / 2), "+4", 22, WHITE, width=3)
    for (x, y, rot) in [(S(13), S(14), 0), (w - S(13), h - S(14), 180)]:
        layer = Image.new("RGBA", img.size, (0, 0, 0, 0))
        ld = ImageDraw.Draw(layer)
        if draw4:
            outlined_text(ld, (x, y), "+4", 10, WHITE, width=1.2)
        else:
            for k, col in enumerate(UNO_COLORS):
                ld.pieslice([x - S(6), y - S(6), x + S(6), y + S(6)], k * 90 - 90, k * 90, fill=col + (255,))
        if rot:
            layer = layer.rotate(180, center=(x, y), resample=Image.BICUBIC)
        img.alpha_composite(layer)
    return img.resize((CARD_W, CARD_H), Image.LANCZOS)


def make_uno_back():
    """Dorso propio: cuatro rombos de colores sobre fondo oscuro."""
    img, d = base_card((25, 25, 30))
    w, h = img.size
    cx, cy = w / 2, h / 2
    r = S(11)
    for k, col in enumerate(UNO_COLORS):
        a = math.radians(-90 + k * 90)
        x, y = cx + math.cos(a) * r * 1.15, cy + math.sin(a) * r * 1.15
        d.polygon([(x, y - r), (x + r * 0.75, y), (x, y + r), (x - r * 0.75, y)], fill=col + (255,),
                  outline=WHITE, width=S(1))
    d.rounded_rectangle([S(8), S(8), w - S(8), h - S(8)], radius=S(4), outline=(80, 80, 90, 255), width=S(1))
    return img.resize((CARD_W, CARD_H), Image.LANCZOS)


# --- Fichas del Rummikub ------------------------------------------------------

TILE_W, TILE_H = 26, 34


def make_tile():
    w, h = S(TILE_W), S(TILE_H)
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.rounded_rectangle([0, S(1), w - 1, h - 1], radius=S(4), fill=(190, 170, 130, 255))
    d.rounded_rectangle([0, 0, w - 1, h - S(2)], radius=S(4), fill=(252, 246, 228, 255), outline=(150, 130, 100, 255),
                        width=S(1))
    d.ellipse([w / 2 - S(4), h - S(9), w / 2 + S(4), h - S(5)], outline=(200, 185, 150, 255), width=S(1))
    return img.resize((TILE_W, TILE_H), Image.LANCZOS)


def make_joker_face():
    """Carita del comodín, para dibujarla encima de una ficha."""
    w, h = S(TILE_W), S(TILE_H)
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    cx, cy, r = w / 2, h * 0.42, S(8)
    d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=(245, 190, 40, 255), outline=(200, 40, 40, 255), width=S(1.2))
    for sx in (-1, 1):
        d.ellipse([cx + sx * S(3) - S(1.2), cy - S(3), cx + sx * S(3) + S(1.2), cy - S(0.6)], fill=INK)
    d.arc([cx - S(4.5), cy - S(3), cx + S(4.5), cy + S(4.5)], 20, 160, fill=(200, 40, 40, 255), width=S(1.2))
    # gorro de bufón
    d.polygon([(cx - r, cy - S(5)), (cx - S(9), cy - S(13)), (cx, cy - r - S(1))], fill=(40, 100, 200, 255))
    d.polygon([(cx + r, cy - S(5)), (cx + S(9), cy - S(13)), (cx, cy - r - S(1))], fill=(220, 40, 45, 255))
    return img.resize((TILE_W, TILE_H), Image.LANCZOS)


def main():
    files = []
    for c in range(4):
        for v in range(13):
            fn = "board_uno_%d_%02d.png" % (c, v)
            make_uno(c, v).save(os.path.join(GFX, fn))
            files.append(fn)
    for fn, im in [("board_uno_wild.png", make_wild(False)), ("board_uno_draw4.png", make_wild(True)),
                   ("board_uno_back.png", make_uno_back()), ("board_tile.png", make_tile()),
                   ("board_joker.png", make_joker_face())]:
        im.save(os.path.join(GFX, fn))
        files.append(fn)
    with open(os.path.join(GFX, "board.t3s"), "w", newline="\n") as f:
        f.write("--atlas -f rgba8888 -z auto --border transparent\n")
        for fn in files:
            f.write(fn + "\n")
    print("Gráficos de juegos de mesa:", len(files))


if __name__ == "__main__":
    main()
