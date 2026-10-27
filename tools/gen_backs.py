#!/usr/bin/env python3
"""Genera las capas con las que el juego compone el dorso de las cartas que
se diseña en el editor (gfx/back_*.png y el atlas gfx/backs.t3s).

Las capas se dibujan al doble de resolución que las cartas (128x192) para
que el texto y los detalles se vean nítidos al ampliar la carta. Las capas
blancas (relleno, línea del marco y dibujos) se tiñen en el juego con el
color elegido; el canto de la carta y los emblemas llevan sus colores.

El orden de las capas define los índices BACK_* de source/ui/gfx.h.

    python3 tools/gen_backs.py
"""
import math
import os

from PIL import Image, ImageDraw

from gen_assets import CARD_H, CARD_W, CREAM, GFX, GOLD, GOLD_DARK, INK, SS, SUITS, draw_copa, draw_oro, draw_suit

K = 2 * SS  # píxeles del lienzo por píxel de carta
W, H = CARD_W * K, CARD_H * K
WHITE = (255, 255, 255, 255)
INSET = 3  # el canto de la carta mide 3 píxeles


def P(v):
    return v * K


def new():
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    return img, ImageDraw.Draw(img)


def inner_mask():
    m = Image.new("L", (W, H), 0)
    ImageDraw.Draw(m).rounded_rectangle([P(INSET), P(INSET), W - P(INSET), H - P(INSET)], radius=P(4), fill=255)
    return m


def clip_inner(img):
    out = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    out.paste(img, (0, 0), inner_mask())
    return out


def done(img):
    return img.resize((CARD_W * 2, CARD_H * 2), Image.LANCZOS)


# --- Base -------------------------------------------------------------------

def back_edge():
    """Canto de la carta (crema, con contorno) con el interior vacío."""
    img, d = new()
    d.rounded_rectangle([0, 0, W - 1, H - 1], radius=P(6), fill=CREAM, outline=INK, width=int(P(1)))
    hole = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    img.paste(hole, (0, 0), inner_mask())
    return done(img)


def back_fill():
    img, d = new()
    d.rounded_rectangle([P(INSET), P(INSET), W - P(INSET), H - P(INSET)], radius=P(4), fill=WHITE)
    return done(img)


def back_line():
    img, d = new()
    d.rounded_rectangle([P(6), P(6), W - P(6), H - P(6)], radius=P(3), outline=WHITE, width=int(P(1)))
    return done(img)


# --- Dibujos (blancos, se tiñen) -------------------------------------------

def pat_rombos():
    img, d = new()
    step = P(8)
    for i in range(-30, 30):
        d.line([(i * step, 0), (i * step + H, H)], fill=WHITE, width=int(P(1)))
        d.line([(i * step, H), (i * step + H, 0)], fill=WHITE, width=int(P(1)))
    return done(clip_inner(img))


def pat_rayas():
    img, d = new()
    step = P(9)
    for i in range(-30, 30):
        d.line([(i * step, 0), (i * step + H, H)], fill=WHITE, width=int(P(3.2)))
    return done(clip_inner(img))


def pat_lunares():
    img, d = new()
    step = P(9)
    r = P(2)
    for row in range(-1, CARD_H // 9 + 2):
        for col in range(-1, CARD_W // 9 + 2):
            x = col * step + (step / 2 if row % 2 else 0) + P(4)
            y = row * step + P(4)
            d.ellipse([x - r, y - r, x + r, y + r], fill=WHITE)
    return done(clip_inner(img))


def pat_cuadros():
    img, d = new()
    step = P(12)
    band = (255, 255, 255, 120)
    for i in range(0, CARD_W // 12 + 2):
        d.rectangle([i * step, 0, i * step + P(4), H], fill=band)
    over = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    od = ImageDraw.Draw(over)
    for j in range(0, CARD_H // 12 + 2):
        od.rectangle([0, j * step, W, j * step + P(4)], fill=band)
    img.alpha_composite(over)
    for i in range(0, CARD_W // 12 + 2):
        d.line([(i * step + P(8), 0), (i * step + P(8), H)], fill=WHITE, width=int(P(0.6)))
    for j in range(0, CARD_H // 12 + 2):
        d.line([(0, j * step + P(8)), (W, j * step + P(8))], fill=WHITE, width=int(P(0.6)))
    return done(clip_inner(img))


def pat_ondas():
    img, d = new()
    for j in range(-1, CARD_H // 7 + 2):
        y0 = j * P(7)
        pts = [(x, y0 + math.sin(x / P(8) * math.pi) * P(2)) for x in range(0, W + 1, SS)]
        d.line(pts, fill=WHITE, width=int(P(1.3)))
    return done(clip_inner(img))


def star(d, cx, cy, r, points=5, inner=0.45, fill=WHITE, outline=None, width=1):
    pts = []
    for i in range(points * 2):
        a = -math.pi / 2 + i * math.pi / points
        rr = r if i % 2 == 0 else r * inner
        pts.append((cx + math.cos(a) * rr, cy + math.sin(a) * rr))
    d.polygon(pts, fill=fill)
    if outline:
        d.line(pts + [pts[0]], fill=outline, width=width, joint="curve")


def pat_estrellas():
    img, d = new()
    step = P(12)
    for row in range(-1, CARD_H // 12 + 2):
        for col in range(-1, CARD_W // 12 + 2):
            x = col * step + (step / 2 if row % 2 else 0) + P(6)
            y = row * step + P(6)
            star(d, x, y, P(3.2), points=4, inner=0.35)
    return done(clip_inner(img))


PATTERNS = [pat_rombos, pat_rayas, pat_lunares, pat_cuadros, pat_ondas, pat_estrellas]


# --- Emblemas (con sus colores) --------------------------------------------

CX, CY = W / 2, H / 2


def ring(d, r, fill=(0, 0, 0, 70)):
    d.ellipse([CX - r, CY - r, CX + r, CY + r], fill=fill, outline=GOLD, width=int(P(1.5)))


def emb_palos():
    img, d = new()
    ring(d, P(15))
    for i, s in enumerate(SUITS):
        a = -math.pi / 2 + i * math.pi / 2
        draw_suit(d, s, CX + math.cos(a) * P(8.5), CY + math.sin(a) * P(8.5), int(P(4.5)))
    return done(img)


def emb_oro():
    img, d = new()
    draw_oro(d, CX, CY, int(P(15)))
    return done(img)


def emb_copa():
    img, d = new()
    ring(d, P(15))
    draw_copa(d, CX, CY + P(0.5), int(P(10)))
    return done(img)


def emb_corona():
    img, d = new()
    ring(d, P(15))
    base_y, top_y = CY + P(6), CY - P(7)
    pts = [(CX - P(10), base_y), (CX - P(11), top_y + P(3)), (CX - P(5), CY - P(1)), (CX, top_y - P(1)),
           (CX + P(5), CY - P(1)), (CX + P(11), top_y + P(3)), (CX + P(10), base_y)]
    d.polygon(pts, fill=GOLD)
    d.line(pts + [pts[0]], fill=GOLD_DARK, width=int(P(0.8)), joint="curve")
    d.rectangle([CX - P(10), base_y - P(1), CX + P(10), base_y + P(2.5)], fill=GOLD, outline=GOLD_DARK, width=int(P(0.6)))
    for x, y in [(CX - P(11), top_y + P(3)), (CX, top_y - P(1)), (CX + P(11), top_y + P(3))]:
        d.ellipse([x - P(1.6), y - P(1.6), x + P(1.6), y + P(1.6)], fill=(200, 40, 45, 255), outline=GOLD_DARK,
                  width=int(P(0.4)))
    for x in (CX - P(5), CX, CX + P(5)):
        d.ellipse([x - P(1.2), base_y - P(0.2), x + P(1.2), base_y + P(2.2)], fill=(60, 110, 200, 255))
    return done(img)


def emb_estrella():
    img, d = new()
    ring(d, P(15))
    star(d, CX, CY + P(0.5), P(11), fill=GOLD, outline=GOLD_DARK, width=int(P(0.8)))
    return done(img)


EMBLEMS = [emb_palos, emb_oro, emb_copa, emb_corona, emb_estrella]


def main():
    layers = [("back_edge.png", back_edge()), ("back_fill.png", back_fill()), ("back_line.png", back_line())]
    for fn in PATTERNS:
        layers.append(("back_" + fn.__name__ + ".png", fn()))
    for fn in EMBLEMS:
        layers.append(("back_" + fn.__name__ + ".png", fn()))
    for name, img in layers:
        img.save(os.path.join(GFX, name))
    with open(os.path.join(GFX, "backs.t3s"), "w", newline="\n") as f:
        f.write("--atlas -f rgba8888 -z auto --border transparent\n")
        for name, _ in layers:
            f.write(name + "\n")
    print("Capas del dorso generadas:", len(layers))


if __name__ == "__main__":
    main()
