/* Solitario Araña: las diez columnas en la pantalla táctil; arriba, el mazo
 * que queda y las escaleras completas. */
#include <math.h>
#include <stdio.h>
#include <time.h>

#include "games/spider.h"
#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/gfx.h"
#include "ui/mii.h"
#include "ui/widgets.h"

#define SC      0.46f
#define CW      (CARD_W * SC)
#define CH      (CARD_H * SC)
#define COL_W   32.0f
#define COL_Y   (26.0f + CH / 2)
#define BOTTOM  236.0f

typedef enum { MODE_SETUP, MODE_PLAY } Mode;

static Spider s_game;
static bool s_started, s_recorded;
static Mode s_mode;
static int s_suits = 1;
static int s_sel_col = -1, s_sel_n; /* cartas elegidas (las últimas n de la columna) */
static int s_cursor;
static int s_hint_from = -1, s_hint_to = -1;
static float s_hint_time, s_end_time;
static char s_message[64];
static Button s_btn_undo, s_btn_deal, s_btn_hint, s_btn_suits, s_btn_play, s_btn_menu;

static int sprite(SpCard c)
{
	return c.up ? c.suit * 13 + (c.rank == 1 ? 12 : c.rank - 2) : FRENCH_BACK;
}

static void sp_enter(void)
{
	if (!s_started) {
		s_started = true;
		s_mode = MODE_SETUP;
	}
	button_set(&s_btn_menu, 262, 2, 54, 20, "Menú", C2D_Color32(120, 30, 35, 255), KEY_START);
}

static void start(void)
{
	sp_init(&s_game, s_suits, (uint32_t)time(NULL) ^ 0x5B1D);
	s_mode = MODE_PLAY;
	s_sel_col = -1;
	s_recorded = false;
	s_end_time = 0.0f;
	snprintf(s_message, sizeof s_message, "Ordena del rey al as del mismo palo");
}

/* Separación vertical de las cartas de una columna para que quepan */
static void spacing(int col, float* down, float* up)
{
	const SpBoard* b = &s_game.b;
	int n = b->col_n[col], hidden = 0;
	for (int i = 0; i < n; i++)
		hidden += !b->col[col][i].up;
	*down = 5.0f;
	*up = 12.0f;
	float need = hidden * *down + (n - hidden - 1) * *up;
	float room = BOTTOM - CH / 2 - COL_Y;
	if (need > room && n > 1) {
		float k = room / need;
		*down *= k;
		*up *= k;
	}
}

static float card_y(int col, int i)
{
	float down, up, y = COL_Y;
	spacing(col, &down, &up);
	for (int k = 0; k < i; k++)
		y += s_game.b.col[col][k].up ? up : down;
	return y;
}

static float col_x(int col)
{
	return 2 + col * COL_W + COL_W / 2;
}

static SceneId setup_update(const Input* in)
{
	static char label[32];
	snprintf(label, sizeof label, GLYPH_X " %d %s", s_suits, s_suits == 1 ? "palo (fácil)" : s_suits == 2 ? "palos (medio)" : "palos (difícil)");
	button_set(&s_btn_suits, 40, 140, 240, 30, label, C2D_Color32(60, 90, 160, 255), KEY_X);
	button_set(&s_btn_play, 60, 200, 200, 30, GLYPH_A " Jugar", C2D_Color32(40, 140, 60, 255), KEY_A);
	if (button_update(&s_btn_suits, in))
		s_suits = s_suits == 1 ? 2 : s_suits == 2 ? 4 : 1;
	else if (button_update(&s_btn_play, in))
		start();
	return SCENE_SPIDER;
}

static void try_move(int to)
{
	if (sp_move(&s_game, s_sel_col, s_sel_n, to)) {
		int done = s_game.b.done;
		audio_play(SFX_CARD);
		if (done && s_game.history_n > 0 && s_game.history[s_game.history_n - 1].done < done) {
			audio_play(SFX_WIN);
			snprintf(s_message, sizeof s_message, "¡Escalera completa! Llevas %d de 8", done);
		}
	} else {
		audio_play(SFX_ERROR);
	}
	s_sel_col = -1;
}

static SceneId sp_update(const Input* in, float dt)
{
	if (button_update(&s_btn_menu, in))
		return SCENE_MENU;
	if (s_mode == MODE_SETUP)
		return setup_update(in);
	Spider* g = &s_game;
	if (s_hint_time > 0)
		s_hint_time -= dt;
	if (g->won) {
		s_end_time += dt;
		if (!s_recorded) {
			s_recorded = true;
			profile_record_game(GAME_SPIDER, true);
			audio_play(SFX_WIN);
		}
		if (s_end_time > 1.2f && ((in->down & KEY_A) || in->touch_down))
			s_mode = MODE_SETUP;
		return SCENE_SPIDER;
	}
	button_set(&s_btn_undo, 2, 2, 82, 20, GLYPH_Y " Deshacer", C2D_Color32(60, 90, 160, 255), KEY_Y);
	static char deal_label[24];
	snprintf(deal_label, sizeof deal_label, GLYPH_X " Dar (%d)", g->b.stock_n / SP_COLS);
	button_set(&s_btn_deal, 88, 2, 82, 20, deal_label, C2D_Color32(40, 140, 60, 255), KEY_X);
	button_set(&s_btn_hint, 174, 2, 82, 20, GLYPH_R " Pista", C2D_Color32(200, 140, 30, 255), KEY_R);
	s_btn_undo.enabled = g->history_n > 0;
	s_btn_deal.enabled = g->b.stock_n > 0;
	if (button_update(&s_btn_undo, in) && sp_undo(g)) {
		s_sel_col = -1;
		audio_play(SFX_FLIP);
	}
	if (button_update(&s_btn_deal, in)) {
		if (sp_deal(g)) {
			audio_play(SFX_CARD);
			s_sel_col = -1;
		} else {
			audio_play(SFX_ERROR);
			snprintf(s_message, sizeof s_message, "Para dar no puede haber columnas vacías");
		}
	}
	if (button_update(&s_btn_hint, in)) {
		int f, n, t;
		if (sp_hint(&g->b, &f, &n, &t)) {
			s_hint_from = f;
			s_hint_to = t;
			s_hint_time = 1.5f;
		} else {
			snprintf(s_message, sizeof s_message, g->b.stock_n ? "No veo nada: da cartas del mazo" : "No veo nada útil");
		}
	}
	/* Cruceta: columna; Ⓐ: coger la escalera entera o soltar */
	if (in->repeat & KEY_LEFT)
		s_cursor = (s_cursor + SP_COLS - 1) % SP_COLS;
	if (in->repeat & KEY_RIGHT)
		s_cursor = (s_cursor + 1) % SP_COLS;
	if (in->down & KEY_A) {
		if (s_sel_col < 0) {
			int run = sp_run(&g->b, s_cursor);
			if (run) {
				s_sel_col = s_cursor;
				s_sel_n = run;
			}
		} else if (s_cursor == s_sel_col) {
			s_sel_col = -1;
		} else {
			try_move(s_cursor);
		}
	}
	if (in->down & KEY_B)
		s_sel_col = -1;
	if (in->touch_down && in->ty > 24) {
		int col = (int)((in->tx - 2) / COL_W);
		if (col >= 0 && col < SP_COLS) {
			s_cursor = col;
			if (s_sel_col >= 0 && col != s_sel_col) {
				try_move(col);
			} else {
				/* La carta tocada (la de más abajo que esté bajo el lápiz) */
				int n = g->b.col_n[col], pick = -1;
				for (int i = 0; i < n; i++)
					if (in->ty >= card_y(col, i) - CH / 2)
						pick = i;
				int run = sp_run(&g->b, col);
				if (pick >= 0 && pick >= n - run) {
					s_sel_col = col;
					s_sel_n = n - pick;
					audio_play(SFX_SELECT);
				} else {
					s_sel_col = -1;
				}
			}
		}
	}
	return SCENE_SPIDER;
}

static void sp_draw_top(void)
{
	const SpBoard* b = &s_game.b;
	gfx_background(BG_TABLE_TOP);
	if (s_mode == MODE_SETUP) {
		gfx_text("Solitario Araña", 200, 30, DEPTH_UI, 0.9f, COLOR_GOLD, C2D_AlignCenter);
		for (int i = 0; i < 6; i++)
			gfx_french(1 * 13 + 11 - i, 110 + i * 36, 120, 0.85f, 0.0f, DEPTH_CARDS + i * 0.1f, true);
		gfx_text_wrap("Diez columnas y dos barajas. Ordena las cartas del rey al as del mismo palo para sacarlas. "
		              "Con 1 palo es sencillo; con 4, todo un reto.",
			60, 176, DEPTH_UI, 0.45f, COLOR_CREAM, 280);
		return;
	}
	/* Mazo */
	for (int k = 0; k < b->stock_n / SP_COLS; k++)
		gfx_french(FRENCH_BACK, 40 + k * 8, 70, 0.8f, 0.0f, DEPTH_CARDS + k * 0.1f, k == 0);
	gfx_textf(60, 120, DEPTH_UI, 0.45f, COLOR_CREAM, C2D_AlignCenter, "Mazo: %d", b->stock_n);
	/* Escaleras completas: un rey de cada */
	for (int k = 0; k < 8; k++) {
		float x = 150 + k * 30;
		if (k < b->done)
			gfx_french(b->done_suit[k] * 13 + 11, x, 70, 0.8f, 0.0f, DEPTH_CARDS + k * 0.1f, true);
		else
			gfx_rrect(x - 25, 70 - 38, 50, 76, 5, DEPTH_TABLE, C2D_Color32(0, 0, 0, 50));
	}
	gfx_textf(255, 120, DEPTH_UI, 0.45f, COLOR_CREAM, C2D_AlignCenter, "Completas: %d de 8", b->done);
	gfx_textf(200, 160, DEPTH_UI, 0.45f, C2D_Color32(200, 230, 200, 255), C2D_AlignCenter, "Movimientos: %d · %d %s",
		b->moves, s_game.suits, s_game.suits == 1 ? "palo" : "palos");
	gfx_text(s_message, 200, 190, DEPTH_UI, 0.48f, COLOR_GOLD, C2D_AlignCenter);
	if (s_game.won && banner_draw(s_end_time, 200, 120, 300, 60))
		gfx_textf(200, 104, DEPTH_BANNER, 0.62f, COLOR_GOLD, C2D_AlignCenter, "¡Ganada en %d movimientos!", b->moves);
}

static void sp_draw_bottom(void)
{
	const SpBoard* b = &s_game.b;
	gfx_background(BG_TABLE_BOTTOM);
	button_draw(&s_btn_menu, false);
	if (s_mode == MODE_SETUP) {
		gfx_text("Elige con cuántos palos", BOTTOM_W / 2, 60, 0, 0.55f, COLOR_GOLD, C2D_AlignCenter);
		button_draw(&s_btn_suits, false);
		button_draw(&s_btn_play, true);
		return;
	}
	button_draw(&s_btn_undo, s_btn_undo.enabled);
	button_draw(&s_btn_deal, s_btn_deal.enabled);
	button_draw(&s_btn_hint, false);
	float pulse = 0.5f + 0.5f * sinf(gfx_time() * 8.0f);
	for (int c = 0; c < SP_COLS; c++) {
		float x = col_x(c);
		gfx_rrect(x - CW / 2, COL_Y - CH / 2, CW, CH, 3, 0, C2D_Color32(0, 0, 0, 50));
		bool hint = s_hint_time > 0 && (c == s_hint_from || c == s_hint_to);
		for (int i = 0; i < b->col_n[c]; i++) {
			float y = card_y(c, i);
			gfx_french(sprite(b->col[c][i]), x, y, SC, 0.0f, 0, false);
			bool sel = c == s_sel_col && i >= b->col_n[c] - s_sel_n;
			if (sel)
				gfx_rect(x - CW / 2, y - CH / 2, CW, 3, 0, COLOR_GOLD);
		}
		if (hint)
			gfx_rect(x - CW / 2, BOTTOM - 4, CW, 4, 0, C2D_Color32(120, 255, 120, (u8)(120 + 120 * pulse)));
		if (c == s_cursor)
			gfx_rect(x - CW / 2, 24, CW, 2, 0, COLOR_GOLD);
	}
}

/* Salir a medias desde la pausa: la próxima vez se empieza de cero */
static void scene_leave(void)
{
	s_started = false;
}

const Scene scene_spider = { sp_enter, sp_update, sp_draw_top, sp_draw_bottom, scene_leave };
