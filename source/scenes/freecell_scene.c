/* Carta blanca (FreeCell): toca una carta y luego dónde la quieres poner. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "games/freecell.h"
#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/gfx.h"
#include "ui/mii.h"
#include "ui/widgets.h"

#define B_SC     0.55f
#define B_W      (CARD_W * B_SC)
#define B_H      (CARD_H * B_SC)
#define B_SLOT   40.0f
#define B_TOP_Y  32.0f /* centro de la fila de casillas y montones */
#define B_COL_Y  86.0f /* centro de la primera carta de las columnas */
#define B_BOTTOM 206.0f
#define T_SC     0.68f
#define T_SLOT   50.0f

static FreeCell s_game;
static bool s_started;
static int s_sel = -1;    /* lugar elegido (columna, casilla), o -1 */
static int s_cursor;      /* 0..15: fila de arriba y columnas */
static float s_win_time;
static bool s_recorded;
static Button s_btn_undo, s_btn_new, s_btn_menu;
static char s_message[64];

static int sprite(FcCard c)
{
	return c.suit * 13 + (c.rank == 1 ? 12 : c.rank - 2);
}

static void new_game(int deal)
{
	fc_new_game(&s_game, deal);
	fc_auto(&s_game);
	s_sel = -1;
	s_win_time = 0.0f;
	s_recorded = false;
	snprintf(s_message, sizeof s_message, "Partida nº %d", deal);
}

static void fc_enter(void)
{
	if (!s_started) {
		srand((unsigned)time(NULL));
		new_game(1 + rand() % 32000);
		s_started = true;
	}
	button_set(&s_btn_undo, 8, 214, 96, 24, GLYPH_Y " Deshacer", C2D_Color32(60, 90, 160, 255), KEY_Y);
	button_set(&s_btn_new, 112, 214, 96, 24, GLYPH_X " Nueva", C2D_Color32(40, 140, 60, 255), KEY_X);
	button_set(&s_btn_menu, 216, 214, 96, 24, "Menú", C2D_Color32(120, 30, 35, 255), KEY_START);
}

/* Separación entre cartas de una columna para que quepa */
static float spacing(int n, float max, float room)
{
	if (n <= 1)
		return max;
	float s = room / (n - 1);
	return s < max ? s : max;
}

/* Lugar de la pantalla táctil: 8..15 la fila de arriba (casillas y montones), 0..7 columnas */
static int place_at(float x, float y)
{
	int k = (int)(x / B_SLOT);
	if (k < 0 || k > 7)
		return -1;
	if (y < B_TOP_Y + B_H / 2 + 4)
		return FC_CELL0 + k; /* 8..11 casillas, 12..15 montones */
	if (y >= B_COL_Y - B_H / 2 && y < B_BOTTOM)
		return k;
	return -1;
}

static void tap(int place)
{
	if (place < 0)
		return;
	if (s_sel < 0) {
		if (place >= FC_FOUND0)
			return;
		bool has = place < FC_CELL0 ? s_game.b.col_n[place] > 0 : s_game.b.cell[place - FC_CELL0].rank > 0;
		if (has) {
			s_sel = place;
			audio_play(SFX_SELECT);
		}
		return;
	}
	int from = s_sel;
	s_sel = -1;
	if (place == from) {
		/* Otra vez la misma: al montón si se puede; si no, a una casilla libre */
		for (int f = FC_FOUND0; f < FC_PLACES; f++)
			if (fc_move(&s_game, from, f))
				goto moved;
		if (from < FC_CELL0)
			for (int c = FC_CELL0; c < FC_FOUND0; c++)
				if (fc_move(&s_game, from, c))
					goto moved;
		audio_play(SFX_ERROR);
		return;
	}
	if (place >= FC_FOUND0) {
		/* A cualquier montón: el de su palo */
		for (int f = FC_FOUND0; f < FC_PLACES; f++)
			if (fc_move(&s_game, from, f))
				goto moved;
	} else if (place >= FC_CELL0 && from >= FC_CELL0) {
		s_sel = place; /* de casilla a casilla: se cambia la elección */
		return;
	} else if (place >= FC_CELL0) {
		/* A la casilla tocada o, si está ocupada, a otra libre */
		if (fc_move(&s_game, from, place))
			goto moved;
		for (int c = FC_CELL0; c < FC_FOUND0; c++)
			if (fc_move(&s_game, from, c))
				goto moved;
	} else if (fc_move(&s_game, from, place)) {
		goto moved;
	}
	audio_play(SFX_ERROR);
	snprintf(s_message, sizeof s_message, "Ahí no se puede (como mucho %d de golpe)", fc_capacity(&s_game.b, place));
	return;
moved:
	audio_play(SFX_CARD);
	fc_auto(&s_game);
	snprintf(s_message, sizeof s_message, "Partida nº %d · %d movimientos", s_game.deal, s_game.b.moves);
}

static SceneId fc_update(const Input* in, float dt)
{
	if (button_update(&s_btn_menu, in))
		return SCENE_MENU;
	if (button_update(&s_btn_new, in)) {
		new_game(1 + rand() % 32000);
		return SCENE_FREECELL;
	}
	s_btn_undo.enabled = s_game.history_n > 0 && !s_game.won;
	if (button_update(&s_btn_undo, in) && fc_undo(&s_game)) {
		s_sel = -1;
		audio_play(SFX_FLIP);
	}
	if (!s_game.won) {
		int row = s_cursor / 8, col = s_cursor % 8;
		if (in->repeat & KEY_LEFT)
			col = (col + 7) % 8;
		if (in->repeat & KEY_RIGHT)
			col = (col + 1) % 8;
		if (in->repeat & (KEY_UP | KEY_DOWN))
			row = 1 - row;
		s_cursor = row * 8 + col;
		if (in->down & KEY_A)
			tap(row == 0 ? FC_CELL0 + col : col);
		if (in->down & KEY_B)
			s_sel = -1;
		if (in->touch_down)
			tap(place_at(in->tx, in->ty));
	} else {
		s_win_time += dt;
		if (!s_recorded) {
			s_recorded = true;
			profile_record_game(GAME_FREECELL, true);
			audio_play(SFX_WIN);
		}
	}
	return SCENE_FREECELL;
}

/* Fila de arriba: casillas libres y montones, en (x0 + k * slot, y) */
static void draw_top_row(float x0, float y, float slot, float sc, float depth, bool cursor)
{
	for (int k = 0; k < 8; k++) {
		float x = x0 + k * slot + slot / 2;
		gfx_rrect(x - CARD_W * sc / 2 + gfx_dx(depth), y - CARD_H * sc / 2, CARD_W * sc, CARD_H * sc, 4, depth,
			k < 4 ? C2D_Color32(20, 70, 35, 255) : C2D_Color32(30, 80, 45, 255));
		FcCard c = { 0, 0 };
		if (k < 4)
			c = s_game.b.cell[k];
		else if (s_game.b.found[k - 4])
			c = (FcCard){ (uint8_t)s_game.b.found[k - 4], (uint8_t)(k - 4) };
		if (c.rank)
			gfx_french(sprite(c), x, y, sc, 0.0f, depth, false);
		else if (k >= 4)
			gfx_text("A", x, y - 8, depth, 0.5f, C2D_Color32(90, 140, 100, 255), C2D_AlignCenter);
		if (cursor && s_sel == FC_CELL0 + k)
			gfx_rrect(x - CARD_W * sc / 2 - 2, y - CARD_H * sc / 2 - 2, CARD_W * sc + 4, 3, 1, depth, COLOR_GOLD);
	}
}

static void draw_columns(float x0, float y0, float slot, float sc, float bottom, float max_gap, float depth, bool cursor)
{
	for (int c = 0; c < FC_COLS; c++) {
		int n = s_game.b.col_n[c];
		float x = x0 + c * slot + slot / 2;
		float gap = spacing(n, max_gap, bottom - y0 - CARD_H * sc / 2);
		if (n == 0)
			gfx_rrect(x - CARD_W * sc / 2 + gfx_dx(depth), y0 - CARD_H * sc / 2, CARD_W * sc, CARD_H * sc, 4, depth,
				C2D_Color32(20, 70, 35, 255));
		int run = cursor && s_sel == c ? fc_run(&s_game.b, c) : 0;
		for (int i = 0; i < n; i++) {
			float y = y0 + i * gap;
			if (i >= n - run) {
				C2D_ImageTint tint;
				C2D_AlphaImageTint(&tint, 0.7f + 0.3f * sinf(gfx_time() * 6.0f));
				gfx_sprite_ex(SPR_CARD_GLOW, x, y, sc * 1.08f, sc * 1.05f, 0.0f, depth, &tint);
			}
			gfx_french(sprite(s_game.b.col[c][i]), x, y, sc, 0.0f, depth + i * 0.01f, i == 0);
		}
	}
}

static void fc_draw_top(void)
{
	gfx_background(BG_TABLE_TOP);
	draw_top_row(0, 40, T_SLOT, T_SC, DEPTH_CARDS, false);
	draw_columns(0, 116, T_SLOT, T_SC, 236, 16, DEPTH_CARDS, false);
	gfx_textf(200, 2, DEPTH_UI, 0.45f, COLOR_GOLD, C2D_AlignCenter, "Carta blanca · partida nº %d · %d/52 subidas",
		s_game.deal, fc_cards_home(&s_game.b));
	if (s_game.won && banner_draw(s_win_time, 200, 120, 300, 60)) {
		gfx_text("¡Partida resuelta!", 200, 100, DEPTH_BANNER, 0.7f, COLOR_GOLD, C2D_AlignCenter);
		gfx_textf(200, 126, DEPTH_BANNER, 0.47f, COLOR_CREAM, C2D_AlignCenter, "En %d movimientos", s_game.b.moves);
	}
}

static void fc_draw_bottom(void)
{
	gfx_background(BG_TABLE_BOTTOM);
	draw_top_row(0, B_TOP_Y, B_SLOT, B_SC, 0, true);
	gfx_text("Libres", 80, 1, 0, 0.4f, C2D_Color32(200, 230, 200, 255), C2D_AlignCenter);
	gfx_text("Montones", 240, 1, 0, 0.4f, C2D_Color32(200, 230, 200, 255), C2D_AlignCenter);
	draw_columns(0, B_COL_Y, B_SLOT, B_SC, B_BOTTOM, 13, 0, true);
	/* Cursor de la cruceta */
	float pulse = 0.5f + 0.5f * sinf(gfx_time() * 6.0f);
	int row = s_cursor / 8, col = s_cursor % 8;
	float x = col * B_SLOT + B_SLOT / 2, y = row == 0 ? B_TOP_Y + B_H / 2 + 3 : B_COL_Y - B_H / 2 - 5;
	gfx_ellipse(x, y, 3, 3, 0, C2D_Color32(255, 220, 60, (u8)(255 * pulse)));
	gfx_text(s_message, BOTTOM_W / 2, 198, 0, 0.4f, COLOR_CREAM, C2D_AlignCenter);
	button_draw(&s_btn_undo, s_btn_undo.enabled);
	button_draw(&s_btn_new, true);
	button_draw(&s_btn_menu, true);
}

const Scene scene_freecell = { fc_enter, fc_update, fc_draw_top, fc_draw_bottom };
