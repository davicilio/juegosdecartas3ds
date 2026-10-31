#include "ui/mjtile.h"

#include <math.h>

#include "games/mahjong.h"

static void pips(int n, float cx, float cy, float w, float h, float s, int kind, float depth)
{
	/* Posiciones de 1 a 9, como en un dado ampliado */
	static const float P[10][9][2] = {
		{ { 0 } },
		{ { 0, 0 } },
		{ { 0, -1 }, { 0, 1 } },
		{ { -1, -1 }, { 0, 0 }, { 1, 1 } },
		{ { -1, -1 }, { 1, -1 }, { -1, 1 }, { 1, 1 } },
		{ { -1, -1 }, { 1, -1 }, { 0, 0 }, { -1, 1 }, { 1, 1 } },
		{ { -1, -1 }, { 1, -1 }, { -1, 0 }, { 1, 0 }, { -1, 1 }, { 1, 1 } },
		{ { -1, -1 }, { 1, -1 }, { 0, -0.4f }, { -1, 0.3f }, { 1, 0.3f }, { -1, 1 }, { 1, 1 } },
		{ { -1, -1 }, { 0, -1 }, { 1, -1 }, { -1, 0 }, { 1, 0 }, { -1, 1 }, { 0, 1 }, { 1, 1 } },
		{ { -1, -1 }, { 0, -1 }, { 1, -1 }, { -1, 0 }, { 0, 0 }, { 1, 0 }, { -1, 1 }, { 0, 1 }, { 1, 1 } },
	};
	static const u32 dot_col[3] = { 0x2850B4, 0x2E8C3C, 0xC83232 };
	float ax = w * 0.3f, ay = h * 0.3f;
	for (int i = 0; i < n; i++) {
		float x = cx + P[n][i][0] * ax, y = cy + P[n][i][1] * ay;
		if (kind == 0) {
			float r = (n == 1 ? 4.5f : n <= 4 ? 2.7f : 2.0f) * s;
			gfx_ellipse(x, y, r, r, depth, gfx_rgb(dot_col[i % 3], 255));
		} else {
			float bw = 1.6f * s, bh = (n <= 4 ? 5.5f : 4.0f) * s;
			gfx_rect(x - bw / 2, y - bh / 2, bw, bh, depth, n == 1 ? C2D_Color32(200, 50, 50, 255) : C2D_Color32(30, 130, 60, 255));
		}
	}
}

void mjtile_face(int f, float cx, float cy, float w, float h, float s, float depth)
{
	u32 ink = C2D_Color32(40, 40, 60, 255);
	if (f < MJ_BAMBOO) {
		pips(f + 1, cx, cy, w, h, s, 0, depth);
	} else if (f < MJ_CHARS) {
		pips(f - MJ_BAMBOO + 1, cx, cy, w, h, s, 1, depth);
	} else if (f < MJ_WINDS) {
		gfx_textf_plain(cx, cy - 9 * s, depth, 0.55f * s, ink, C2D_AlignCenter, "%d", f - MJ_CHARS + 1);
		gfx_rect(cx - 5 * s, cy + 5 * s, 10 * s, 2.5f * s, depth, C2D_Color32(200, 40, 40, 255));
	} else if (f < MJ_DRAGONS) {
		static const char* const W[4] = { "N", "E", "S", "O" };
		gfx_text(W[f - MJ_WINDS], cx, cy - 9 * s, depth, 0.6f * s, C2D_Color32(30, 50, 110, 255), C2D_AlignCenter);
	} else if (f < MJ_FLOWERS) {
		int d = f - MJ_DRAGONS;
		if (d == 2) {
			gfx_rect(cx - w * 0.3f, cy - h * 0.3f, w * 0.6f, h * 0.6f, depth, C2D_Color32(40, 80, 180, 255));
			gfx_rect(cx - w * 0.3f + 1.5f * s, cy - h * 0.3f + 1.5f * s, w * 0.6f - 3 * s, h * 0.6f - 3 * s, depth,
				C2D_Color32(250, 245, 230, 255));
		} else {
			gfx_rect(cx - w * 0.28f, cy - h * 0.32f, w * 0.56f, h * 0.64f, depth,
				d == 0 ? C2D_Color32(200, 40, 40, 255) : C2D_Color32(30, 140, 60, 255));
			gfx_text(d == 0 ? "R" : "V", cx, cy - 8 * s, depth, 0.5f * s, COLOR_WHITE, C2D_AlignCenter);
		}
	} else if (f < MJ_SEASONS) {
		static const u32 petal[4] = { 0xE86EA0, 0xE8B428, 0x9A5AD2, 0xE8782A };
		int k = f - MJ_FLOWERS;
		for (int i = 0; i < 5; i++) {
			float a = i * 1.2566f;
			gfx_ellipse(cx + cosf(a) * 3.6f * s, cy + sinf(a) * 3.6f * s, 2.6f * s, 2.6f * s, depth,
				gfx_rgb(petal[k], 255));
		}
		gfx_ellipse(cx, cy, 1.8f * s, 1.8f * s, depth, C2D_Color32(250, 220, 60, 255));
	} else {
		static const u32 col[4] = { 0x3CA046, 0xE8C428, 0xD2782A, 0x5AAAE6 };
		static const char* const L[4] = { "P", "V", "O", "I" }; /* primavera, verano, otoño, invierno */
		int k = f - MJ_SEASONS;
		gfx_ellipse(cx, cy, 6.5f * s, 6.5f * s, depth, gfx_rgb(col[k], 255));
		gfx_text(L[k], cx, cy - 8 * s, depth, 0.5f * s, COLOR_WHITE, C2D_AlignCenter);
	}
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
	gfx_rect(x, y, w, h - edge, depth, C2D_Color32(60, 50, 40, 255));
	gfx_rect(x + 0.5f, y + 0.5f, w - 1, h - edge - 1, depth, C2D_Color32(250, 246, 232, 255));
	mjtile_face(solitaire_face(t), x + w / 2, y + (h - edge) / 2, w - 3, h - edge - 3, w / 22.0f, depth);
	if (dim)
		gfx_rect(x, y, w, h - edge, depth, C2D_Color32(0, 0, 0, 110));
}

void mjtile_riichi_sideways(int t, float x, float y, float w, float h, float depth)
{
	/* Sin rotar el dibujo: una ficha más ancha que alta, con la cara pequeña */
	gfx_rect(x, y, h, w, depth, C2D_Color32(60, 50, 40, 255));
	gfx_rect(x + 0.5f, y + 0.5f, h - 1, w - 1, depth, C2D_Color32(250, 246, 232, 255));
	if (t >= 0)
		mjtile_face(solitaire_face(t), x + h / 2, y + w / 2, w - 3, w - 3, w / 26.0f, depth);
}
