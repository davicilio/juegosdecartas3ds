/* Mahjong solitario: la tortuga arriba en grande y abajo para tocarla. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "games/mahjong.h"
#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/gfx.h"
#include "ui/mii.h"
#include "ui/mjtile.h"
#include "ui/widgets.h"

typedef struct {
	float tw, th, x0, y0, lx, ly; /* tamaño de ficha, origen y desplazamiento por piso */
} View;

static const View TOP_VIEW = { 25, 28, 12, 14, 2.6f, 2.6f };
static const View BOT_VIEW = { 20, 24, 10, 22, 2.0f, 2.2f };

static Mahjong s_game;
static bool s_started;
static int s_sel = -1, s_cursor = -1;
static int s_hint_a = -1, s_hint_b = -1;
static float s_hint_time, s_win_time;
static bool s_recorded;
static int s_order[MJ_TILES]; /* orden de dibujo */
static char s_message[80];
static Button s_btn_hint, s_btn_undo, s_btn_shuffle, s_btn_new, s_btn_menu;

static void message(const char* m)
{
	snprintf(s_message, sizeof s_message, "%s", m);
}

static void tile_rect(const View* v, int t, float* x, float* y)
{
	*x = v->x0 + MJ_LAYOUT[t].x * v->tw / 2 - MJ_LAYOUT[t].z * v->lx;
	*y = v->y0 + MJ_LAYOUT[t].y * v->th / 2 - MJ_LAYOUT[t].z * v->ly;
}

static int moves_left(void)
{
	int n = 0;
	for (int a = 0; a < MJ_TILES; a++) {
		if (!mj_free(&s_game, a))
			continue;
		for (int b = a + 1; b < MJ_TILES; b++)
			if (mj_free(&s_game, b) && mj_match(s_game.face[a], s_game.face[b]))
				n++;
	}
	return n;
}

static void new_game(void)
{
	mj_new_game(&s_game);
	s_sel = s_cursor = s_hint_a = s_hint_b = -1;
	s_win_time = 0.0f;
	s_recorded = false;
	message("Quita parejas de fichas iguales que estén libres");
}

static int cmp_order(const void* a, const void* b)
{
	int ta = *(const int*)a, tb = *(const int*)b;
	const MjPos *pa = &MJ_LAYOUT[ta], *pb = &MJ_LAYOUT[tb];
	if (pa->z != pb->z)
		return pa->z - pb->z;
	if (pa->x != pb->x)
		return pb->x - pa->x; /* de derecha a izquierda: se ve el canto de la izquierda */
	return pa->y - pb->y;
}

static void mj_enter(void)
{
	if (!s_started) {
		for (int t = 0; t < MJ_TILES; t++)
			s_order[t] = t;
		qsort(s_order, MJ_TILES, sizeof(int), cmp_order);
		mj_init(&s_game, (uint32_t)time(NULL) ^ 0x3A1);
		s_started = true;
		new_game();
	}
	button_set(&s_btn_hint, 4, 214, 60, 24, GLYPH_X " Pista", C2D_Color32(60, 90, 160, 255), KEY_X);
	button_set(&s_btn_undo, 68, 214, 60, 24, GLYPH_Y " Atrás", C2D_Color32(60, 90, 160, 255), KEY_Y);
	button_set(&s_btn_shuffle, 132, 214, 60, 24, "L Barajar", C2D_Color32(190, 120, 30, 255), KEY_L);
	button_set(&s_btn_new, 196, 214, 60, 24, "R Nueva", C2D_Color32(40, 140, 60, 255), KEY_R);
	button_set(&s_btn_menu, 260, 214, 56, 24, "Menú", C2D_Color32(120, 30, 35, 255), KEY_START);
}

static void pick(int t)
{
	if (t < 0)
		return;
	if (!mj_free(&s_game, t)) {
		audio_play(SFX_ERROR);
		message("Esa ficha no está libre: tiene algo encima o a los dos lados");
		return;
	}
	if (s_sel < 0 || s_sel == t) {
		s_sel = s_sel == t ? -1 : t;
		audio_play(SFX_SELECT);
		return;
	}
	if (mj_remove(&s_game, s_sel, t)) {
		audio_play(SFX_CARD);
		s_sel = -1;
		s_hint_a = s_hint_b = -1;
		int m = moves_left();
		char buf[80];
		if (s_game.left == 0)
			snprintf(buf, sizeof buf, "¡Tortuga despejada!");
		else if (m == 0)
			snprintf(buf, sizeof buf, "No quedan parejas libres: baraja (L) o vuelve atrás (Y)");
		else
			snprintf(buf, sizeof buf, "Quedan %d fichas · %d pareja%s posible%s", s_game.left, m, m == 1 ? "" : "s",
				m == 1 ? "" : "s");
		message(buf);
	} else {
		s_sel = t; /* no emparejan: se elige la nueva */
		audio_play(SFX_SELECT);
	}
}

/* Ficha bajo el punto de la pantalla táctil (la de más arriba) */
static int tile_at(float px, float py)
{
	for (int k = MJ_TILES - 1; k >= 0; k--) {
		int t = s_order[k];
		if (!s_game.present[t])
			continue;
		float x, y;
		tile_rect(&BOT_VIEW, t, &x, &y);
		if (px >= x && px < x + BOT_VIEW.tw && py >= y && py < y + BOT_VIEW.th)
			return t;
	}
	return -1;
}

/* Cruceta: salta a la ficha libre más cercana en esa dirección */
static void move_cursor(int dx, int dy)
{
	if (s_cursor < 0 || !s_game.present[s_cursor]) {
		for (int t = 0; t < MJ_TILES; t++)
			if (mj_free(&s_game, t)) {
				s_cursor = t;
				return;
			}
		return;
	}
	const MjPos* c = &MJ_LAYOUT[s_cursor];
	int best = -1;
	float bd = 1e9f;
	for (int t = 0; t < MJ_TILES; t++) {
		if (t == s_cursor || !mj_free(&s_game, t))
			continue;
		float ddx = MJ_LAYOUT[t].x - c->x, ddy = MJ_LAYOUT[t].y - c->y;
		float along = dx ? ddx * dx : ddy * dy;
		if (along <= 0)
			continue;
		float side = dx ? fabsf(ddy) : fabsf(ddx);
		float d = along + side * 3;
		if (d < bd) {
			bd = d;
			best = t;
		}
	}
	if (best >= 0)
		s_cursor = best;
}

static SceneId mj_update(const Input* in, float dt)
{
	if (button_update(&s_btn_menu, in))
		return SCENE_MENU;
	if (button_update(&s_btn_new, in)) {
		if (!s_recorded && s_game.left < MJ_TILES)
			profile_record_game(GAME_MAHJONG, false);
		new_game();
		return SCENE_MAHJONG;
	}
	s_hint_time -= dt;
	if (s_game.left == 0) {
		s_win_time += dt;
		if (!s_recorded) {
			s_recorded = true;
			profile_record_game(GAME_MAHJONG, true);
			audio_play(SFX_WIN);
		}
		return SCENE_MAHJONG;
	}
	s_btn_undo.enabled = s_game.undo_n > 0;
	if (button_update(&s_btn_hint, in)) {
		if (mj_hint(&s_game, &s_hint_a, &s_hint_b)) {
			s_hint_time = 2.5f;
			audio_play(SFX_CONFIRM);
		} else {
			message("No hay ninguna pareja libre: baraja (L)");
			audio_play(SFX_ERROR);
		}
	}
	if (button_update(&s_btn_undo, in) && mj_undo(&s_game)) {
		s_sel = -1;
		audio_play(SFX_FLIP);
		message("Pareja devuelta a su sitio");
	}
	if (button_update(&s_btn_shuffle, in)) {
		if (mj_shuffle(&s_game)) {
			s_sel = -1;
			audio_play(SFX_FLIP);
			message("Fichas barajadas: ahora seguro que tiene solución");
		} else {
			audio_play(SFX_ERROR);
			message("No se puede barajar: quedan fichas apiladas sin salida");
		}
	}
	if (in->repeat & KEY_LEFT)
		move_cursor(-1, 0);
	if (in->repeat & KEY_RIGHT)
		move_cursor(1, 0);
	if (in->repeat & KEY_UP)
		move_cursor(0, -1);
	if (in->repeat & KEY_DOWN)
		move_cursor(0, 1);
	if ((in->down & KEY_A) && s_cursor >= 0)
		pick(s_cursor);
	if (in->down & KEY_B)
		s_sel = -1;
	if (in->touch_down && in->ty < 210) {
		int t = tile_at(in->tx, in->ty);
		if (t >= 0)
			s_cursor = t;
		pick(t);
	}
	return SCENE_MAHJONG;
}

/* ---------------------------------------------------------------------------
 * Dibujo de las fichas
 * ------------------------------------------------------------------------- */

static void draw_board(const View* v, float depth, bool interactive)
{
	float pulse = 0.5f + 0.5f * sinf(gfx_time() * 7.0f);
	float s = v->tw / 25.0f;
	for (int k = 0; k < MJ_TILES; k++) {
		int t = s_order[k];
		if (!s_game.present[t])
			continue;
		float x, y;
		tile_rect(v, t, &x, &y);
		float d = depth + MJ_LAYOUT[t].z * 0.4f;
		float dx = gfx_dx(d);
		x += dx;
		/* Canto de la ficha (abajo e izquierda) y cara. Rectángulos simples: son
		 * 144 fichas y citro2d tiene un límite de figuras por fotograma */
		C2D_DrawRectSolid(x - v->lx, y + v->ly, 0.5f, v->tw, v->th, C2D_Color32(150, 110, 60, 255));
		C2D_DrawRectSolid(x, y, 0.5f, v->tw, v->th, C2D_Color32(60, 50, 40, 255));
		u32 face = C2D_Color32(250, 245, 230, 255);
		bool hint = s_hint_time > 0 && (t == s_hint_a || t == s_hint_b);
		if (t == s_sel)
			face = C2D_Color32(255, 225, 120, 255);
		else if (hint)
			face = C2D_Color32((u8)(200 + 55 * pulse), 255, (u8)(200 + 55 * pulse), 255);
		C2D_DrawRectSolid(x + 1, y + 1, 0.5f, v->tw - 2, v->th - 2, face);
		/* Los rectángulos de arriba son de citro2d directamente; lo que sigue
		 * usa gfx_*, que ya aplican el desplazamiento de la profundidad */
		x -= dx;
		mjtile_face(s_game.face[t], x + v->tw / 2, y + v->th / 2, v->tw - 4, v->th - 4, s, d);
		if (interactive && t == s_cursor) {
			u32 c = C2D_Color32(255, 200, 40, (u8)(160 + 95 * pulse));
			gfx_rect(x, y, v->tw, 2, d, c);
			gfx_rect(x, y + v->th - 2, v->tw, 2, d, c);
			gfx_rect(x, y, 2, v->th, d, c);
			gfx_rect(x + v->tw - 2, y, 2, v->th, d, c);
		}
	}
}

static void mj_draw_top(void)
{
	gfx_background(BG_TABLE_TOP);
	draw_board(&TOP_VIEW, DEPTH_TABLE, false);
	if (s_game.left == 0 && banner_draw(s_win_time, 200, 120, 300, 56))
		gfx_text("¡Tortuga despejada!", 200, 108, DEPTH_BANNER, 0.68f, COLOR_GOLD, C2D_AlignCenter);
}

static void mj_draw_bottom(void)
{
	gfx_background(BG_TABLE_BOTTOM);
	draw_board(&BOT_VIEW, 0, true);
	gfx_text(s_message, 4, 0, 0, 0.42f, COLOR_GOLD, C2D_AlignLeft);
	button_draw(&s_btn_hint, s_game.left > 0);
	button_draw(&s_btn_undo, s_btn_undo.enabled);
	button_draw(&s_btn_shuffle, s_game.left > 0);
	button_draw(&s_btn_new, true);
	button_draw(&s_btn_menu, true);
}

/* Salir a medias desde la pausa: la próxima vez se empieza de cero */
static void scene_leave(void)
{
	s_started = false;
}

const Scene scene_mahjong = { mj_enter, mj_update, mj_draw_top, mj_draw_bottom, scene_leave };
