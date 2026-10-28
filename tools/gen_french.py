#!/usr/bin/env python3
"""Genera la baraja francesa de 52 cartas (para el póker) en gfx/fr_*.png y
el atlas gfx/french.t3s: índice = palo * 13 + (valor - 2), con el dorso al
final. Palos: 0 picas, 1 corazones, 2 diamantes, 3 tréboles.

    python3 tools/gen_french.py
"""
import os

from PIL import Image, ImageDraw, ImageFont

from gen_assets import CARD_H, CARD_W, GFX, SS

SUIT_GLYPH = ["♠", "♥", "♦", "♣"]
RED = (200, 30, 40, 255)
BLACK = (25, 25, 30, 255)
RANKS = {11: "J", 12: "Q", 13: "K", 14: "A"}


def S(v):
    return int(round(v * SS))


def find(name):
    for d in ("/usr/share/fonts/truetype/dejavu/", "/usr/share/fonts/dejavu/"):
        if os.path.exists(d + name):
            return d + name
    raise SystemExit("Falta la fuente " + name)


SANS = find("DejaVuSans.ttf")
SANS_BOLD = find("DejaVuSans-Bold.ttf")


def f(path, size):
    return ImageFont.truetype(path, S(size))


def card(suit, rank):
    w, h = S(CARD_W), S(CARD_H)
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.rounded_rectangle([0, 0, w - 1, h - 1], radius=S(6), fill=(253, 252, 248, 255), outline=(60, 60, 70, 255),
                        width=S(1))
    col = RED if suit in (1, 2) else BLACK
    label = RANKS.get(rank, str(rank))
    glyph = SUIT_GLYPH[suit]
    # esquinas
    for rot in (0, 180):
        layer = Image.new("RGBA", (w, h), (0, 0, 0, 0))
        ld = ImageDraw.Draw(layer)
        ld.text((S(8), S(9)), label, font=f(SANS_BOLD, 12 if label != "10" else 10.5), fill=col, anchor="mm")
        ld.text((S(8), S(20)), glyph, font=f(SANS, 10), fill=col, anchor="mm")
        if rot:
            layer = layer.rotate(180)
        img.alpha_composite(layer)
    d = ImageDraw.Draw(img)
    cx, cy = w / 2, h / 2
    if rank >= 11 and rank <= 13:
        # figuras: marco de color con la letra grande y el palo
        d.rounded_rectangle([S(14), S(16), w - S(14), h - S(16)], radius=S(4), fill=(250, 238, 200, 255),
                            outline=col, width=S(1.5))
        d.text((cx, cy - S(8)), label, font=f(SANS_BOLD, 30), fill=col, anchor="mm")
        d.text((cx, cy + S(18)), glyph, font=f(SANS, 18), fill=col, anchor="mm")
    elif rank == 14:
        d.text((cx, cy), glyph, font=f(SANS, 46), fill=col, anchor="mm")
    else:
        # números: el palo grande en el centro y el número debajo
        d.text((cx, cy - S(6)), glyph, font=f(SANS, 34), fill=col, anchor="mm")
        d.text((cx, cy + S(24)), label, font=f(SANS_BOLD, 15), fill=col, anchor="mm")
    return img.resize((CARD_W, CARD_H), Image.LANCZOS)


def back():
    w, h = S(CARD_W), S(CARD_H)
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.rounded_rectangle([0, 0, w - 1, h - 1], radius=S(6), fill=(253, 252, 248, 255), outline=(60, 60, 70, 255),
                        width=S(1))
    d.rounded_rectangle([S(4), S(4), w - S(4), h - S(4)], radius=S(4), fill=(30, 60, 140, 255))
    step = S(6)
    for i in range(-20, 30):
        d.line([(i * step, S(4)), (i * step + h, h)], fill=(60, 95, 180, 255), width=S(1))
        d.line([(i * step, h), (i * step + h, S(4))], fill=(60, 95, 180, 255), width=S(1))
    mask = Image.new("L", (w, h), 0)
    ImageDraw.Draw(mask).rounded_rectangle([S(4), S(4), w - S(4), h - S(4)], radius=S(4), fill=255)
    base = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    ImageDraw.Draw(base).rounded_rectangle([0, 0, w - 1, h - 1], radius=S(6), fill=(253, 252, 248, 255),
                                           outline=(60, 60, 70, 255), width=S(1))
    base.paste(img, (0, 0), mask)
    d = ImageDraw.Draw(base)
    d.rounded_rectangle([S(7), S(7), w - S(7), h - S(7)], radius=S(3), outline=(230, 230, 240, 255), width=S(1))
    return base.resize((CARD_W, CARD_H), Image.LANCZOS)


def main():
    files = []
    for s in range(4):
        for r in range(2, 15):
            fn = "fr_%d_%02d.png" % (s, r)
            card(s, r).save(os.path.join(GFX, fn))
            files.append(fn)
    back().save(os.path.join(GFX, "fr_back.png"))
    files.append("fr_back.png")
    with open(os.path.join(GFX, "french.t3s"), "w", newline="\n") as fh:
        fh.write("--atlas -f rgba8888 -z auto --border transparent\n")
        for fn in files:
            fh.write(fn + "\n")
    print("Baraja francesa:", len(files))


if __name__ == "__main__":
    main()
