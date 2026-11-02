#include "ui/mjtile.h"

#include "games/mahjong.h"

/* Las caras son imágenes del atlas de piezas (tools/gen_pieces.py), con el
 * fondo de marfil incluido */
void mjtile_face(int f, float cx, float cy, float w, float h, float s, float depth)
{
	(void)s;
	gfx_piece_rect(PIECE_MJ_FIRST + f, cx - w / 2, cy - h / 2, w, h, depth);
}

/* Del índice del riichi (man, pin, sou, vientos E S O N, dragones blanco,
 * verde y rojo) al de las caras del solitario */
static int solitaire_face(int t)
{
	if (t < 9)
		return MJ_CHARS + t;
	if (t < 18)
		return t - 9;
	if (t < 27)
		return MJ_BAMBOO + t - 18;
	if (t < 31)
		return MJ_WINDS + (t - 27 + 1) % 4; /* el solitario empieza por el norte */
	return MJ_DRAGONS + 2 - (t - 31);
}

void mjtile_riichi(int t, float x, float y, float w, float h, float depth, bool dim)
{
	float edge = h * 0.12f;
	gfx_rect(x, y + edge, w, h - edge, depth, C2D_Color32(220, 160, 60, 255));
	if (t < 0) {
		gfx_rect(x, y, w, h - edge, depth, C2D_Color32(230, 170, 70, 255));
		gfx_rect(x + 1, y + 1, w - 2, h - edge - 2, depth, C2D_Color32(240, 190, 90, 255));
		return;
	}
	mjtile_face(solitaire_face(t), x + w / 2, y + (h - edge) / 2, w, h - edge, 1.0f, depth);
	if (dim)
		gfx_rect(x, y, w, h - edge, depth, C2D_Color32(0, 0, 0, 110));
}

void mjtile_riichi_sideways(int t, float x, float y, float w, float h, float depth)
{
	/* Sin rotar el dibujo: una ficha más ancha que alta, con la cara pequeña */
	gfx_rect(x, y, h, w, depth, C2D_Color32(60, 50, 40, 255));
	gfx_rect(x + 0.5f, y + 0.5f, h - 1, w - 1, depth, C2D_Color32(250, 246, 232, 255));
	if (t >= 0)
		mjtile_face(solitaire_face(t), x + h / 2, y + w / 2, w - 1, w - 1, 1.0f, depth);
}
