/* Solitario Klondike: el mazo, el descarte, los montones y las siete
 * columnas en la pantalla táctil. */
#include <math.h>
#include <stdio.h>
#include <time.h>

#include "games/klondike.h"
#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/gfx.h"
#include "ui/mii.h"
#include "ui/setup.h"
#include "ui/widgets.h"

#define SC     0.56f
#define CW     (CARD_W * SC)
#define CH     (CARD_H * SC)
#define ROW_Y  (22.0f + CH / 2)
#define COL_Y  (ROW_Y + CH + 6)
#define BOTTOM 238.0f

static const char* const DRAW_NAMES[] = { "De una en una", "De tres en tres" };

static Klondike s_game;
static bool s_started, s_in_setup, s_recorded;
static int s_draw_idx;
static Setup s_setup;
static int s_sel = -1, s_sel_n; /* lugar elegido y cuántas cartas */
static int s_cursor;
static float s_end_time, s_finish_timer;
static Button s_btn_undo, s_btn_menu;

static int sprite(KlCard c)
{
	return c.up ? c.suit * 13 + (c.rank == 1 ? 12 : c.rank - 2) : FRENCH_BACK;
}

static float col_x(int c)
{
	return 4 + CW / 2 + c * ((BOTTOM_W - 8 - CW) / 6.0f);
}

static float found_x(int s)
{
	return col_x(3 + s);
}

static void spacing(int col, float* down, float* up)
{
	const KlBoard* b = &s_game.b;
	int n = b->col_n[col], hidden = 0;
	for (int i = 0; i < n; i++)
		hidden += !b->col[col][i].up;
	*down = 5.0f;
	*up = 14.0f;
	float need = hidden * *down + (n - hidden - 1) * *up, room = BOTTOM - CH / 2 - COL_Y;
	if (need > room && n > 1) {
		*down *= room / need;
		*up *= room / need;
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

static void kl_enter(void)
{
	if (!s_started) {
		s_started = true;
		s_in_setup = true;
		setup_init(&s_setup, "Solitario Klondike", "El solitario de siempre: siete columnas, el mazo y cuatro montones. Baja alternando colores y sube cada palo del as al rey.");
		setup_add(&s_setup, "Robar del mazo", DRAW_NAMES, 2, &s_draw_idx);
	}
	button_set(&s_btn_undo, 4, 2, 88, 18, GLYPH_Y " Deshacer", C2D_Color32(60, 90, 160, 255), KEY_Y);
	button_set(&s_btn_menu, 262, 2, 54, 18, "Menú", C2D_Color32(120, 30, 35, 255), KEY_START);
}

static void begin(void)
{
	kl_init(&s_game, s_draw_idx ? 3 : 1, (uint32_t)time(NULL) ^ 0xC10D);
	s_sel = -1;
	s_recorded = false;
	s_end_time = 0.0f;
}

/* Intenta mover lo elegido a to (con tantas cartas como quepan) */
static bool try_move(int to)
{
	for (int n = s_sel_n; n >= 1; n--)
		if (kl_move(&s_game, s_sel, n, to)) {
			audio_play(SFX_CARD);
			return true;
		}
	audio_play(SFX_ERROR);
	return false;
}

/* ¿Qué hay bajo el toque? Devuelve el lugar y, en una columna, la carta */
static int touched(const Input* in, int* card)
{
	*card = -1;
	if (in->ty < ROW_Y + CH / 2) {
		if (fabsf(in->tx - col_x(0)) < CW / 2)
			return 100; /* el mazo */
		if (in->tx > col_x(1) - CW / 2 && in->tx < col_x(2) + CW / 2)
			return KL_WASTE;
		for (int s = 0; s < 4; s++)
			if (fabsf(in->tx - found_x(s)) < CW / 2)
				return KL_FOUND0 + s;
		return -1;
	}
	for (int c = 0; c < KL_COLS; c++) {
		if (fabsf(in->tx - col_x(c)) > CW / 2 + 2)
			continue;
		int n = s_game.b.col_n[c];
		for (int i = 0; i < n; i++)
			if (in->ty >= card_y(c, i) - CH / 2)
				*card = i;
		return c;
	}
	return -1;
}

static SceneId kl_update(const Input* in, float dt)
{
	if (button_update(&s_btn_menu, in))
		return SCENE_MENU;
	if (s_in_setup) {
		if (setup_update(&s_setup, in)) {
			s_in_setup = false;
			begin();
		}
		return SCENE_KLONDIKE;
	}
	Klondike* g = &s_game;
	if (g->won) {
		s_end_time += dt;
		if (!s_recorded) {
			s_recorded = true;
			profile_record_game(GAME_KLONDIKE, true);
			audio_play(SFX_WIN);
		}
		if (s_end_time > 1.2f && ((in->down & KEY_A) || in->touch_down))
			begin();
		return SCENE_KLONDIKE;
	}
	/* Al tenerlo todo a la vista, las cartas suben solas */
	if (kl_can_finish(&g->b)) {
		s_finish_timer += dt;
		if (s_finish_timer > 0.12f) {
			s_finish_timer = 0;
			if (kl_finish_step(g) >= 0)
				audio_play(SFX_CARD);
		}
		return SCENE_KLONDIKE;
	}
	s_btn_undo.enabled = g->history_n > 0;
	if (button_update(&s_btn_undo, in) && kl_undo(g)) {
		s_sel = -1;
		audio_play(SFX_FLIP);
	}
	if (in->down & KEY_X) {
		kl_draw(g);
		s_sel = -1;
		audio_play(SFX_FLIP);
	}
	if (in->down & KEY_B)
		s_sel = -1;
	/* Cruceta: columnas; Ⓐ coge o suelta; arriba, el descarte */
	if (in->repeat & KEY_LEFT)
		s_cursor = (s_cursor + KL_COLS - 1) % KL_COLS;
	if (in->repeat & KEY_RIGHT)
		s_cursor = (s_cursor + 1) % KL_COLS;
	if ((in->down & KEY_UP) && g->b.waste_n) {
		s_sel = KL_WASTE;
		s_sel_n = 1;
	}
	if (in->down & KEY_A) {
		if (s_sel < 0) {
			int n = 0;
			while (n < g->b.col_n[s_cursor] && g->b.col[s_cursor][g->b.col_n[s_cursor] - 1 - n].up)
				n++;
			if (n) {
				s_sel = s_cursor;
				s_sel_n = n;
			}
		} else if (s_sel == s_cursor) {
			if (kl_auto_found(g, s_sel))
				audio_play(SFX_CARD);
			s_sel = -1;
		} else {
			try_move(s_cursor);
			s_sel = -1;
		}
	}
	if (in->touch_down && in->ty > 20) {
		int card, t = touched(in, &card);
		if (t == 100) {
			kl_draw(g);
			s_sel = -1;
			audio_play(SFX_FLIP);
		} else if (t >= 0) {
			if (t < KL_COLS)
				s_cursor = t;
			if (s_sel >= 0 && t == s_sel) {
				/* Segundo toque en lo mismo: al montón si se puede */
				if (s_sel_n == 1 && kl_auto_found(g, s_sel))
					audio_play(SFX_CARD);
				s_sel = -1;
			} else if (s_sel >= 0) {
				try_move(t);
				s_sel = -1;
			} else if (t == KL_WASTE && g->b.waste_n) {
				s_sel = KL_WASTE;
				s_sel_n = 1;
			} else if (t >= KL_FOUND0 && t < KL_WASTE && g->b.found[t - KL_FOUND0]) {
				s_sel = t;
				s_sel_n = 1;
			} else if (t < KL_COLS && card >= 0 && g->b.col[t][card].up) {
				s_sel = t;
				s_sel_n = g->b.col_n[t] - card;
				audio_play(SFX_SELECT);
			}
		}
	}
	return SCENE_KLONDIKE;
}

static void kl_draw_top(void)
{
	if (s_in_setup) {
		setup_draw_top(&s_setup);
		return;
	}
	const KlBoard* b = &s_game.b;
	gfx_background(BG_TABLE_TOP);
	/* Los cuatro montones, en grande */
	for (int s = 0; s < 4; s++) {
		float x = 80 + s * 80;
		if (b->found[s])
			gfx_french(s * 13 + (b->found[s] == 1 ? 12 : b->found[s] - 2), x, 90, 1.0f, 0.0f, DEPTH_CARDS, true);
		else
			gfx_rrect(x - 32, 90 - 48, 64, 96, 6, DEPTH_TABLE, C2D_Color32(0, 0, 0, 50));
	}
	int up = b->found[0] + b->found[1] + b->found[2] + b->found[3];
	gfx_textf(200, 160, DEPTH_UI, 0.55f, COLOR_GOLD, C2D_AlignCenter, "%d de 52 cartas subidas", up);
	gfx_textf(200, 186, DEPTH_UI, 0.45f, COLOR_CREAM, C2D_AlignCenter, "Movimientos: %d · robas de %d en %d", b->moves,
		s_game.draw, s_game.draw);
	gfx_text("Toca el mazo (o " GLYPH_X ") para robar.", 200, 204, DEPTH_UI, 0.42f, C2D_Color32(200, 230, 200, 255),
		C2D_AlignCenter);
	gfx_text("Toca dos veces una carta para subirla.", 200, 220, DEPTH_UI, 0.42f, C2D_Color32(200, 230, 200, 255),
		C2D_AlignCenter);
	if (s_game.won && banner_draw(s_end_time, 200, 120, 300, 60))
		gfx_textf(200, 104, DEPTH_BANNER, 0.62f, COLOR_GOLD, C2D_AlignCenter, "¡Ganada en %d movimientos!", b->moves);
}

static void kl_draw_bottom(void)
{
	if (s_in_setup) {
		setup_draw_bottom(&s_setup);
		button_draw(&s_btn_menu, false);
		return;
	}
	const KlBoard* b = &s_game.b;
	gfx_background(BG_TABLE_BOTTOM);
	button_draw(&s_btn_undo, s_btn_undo.enabled);
	button_draw(&s_btn_menu, false);
	/* Mazo y descarte */
	if (b->stock_n)
		gfx_french(FRENCH_BACK, col_x(0), ROW_Y, SC, 0.0f, 0, false);
	else
		gfx_rrect(col_x(0) - CW / 2, ROW_Y - CH / 2, CW, CH, 4, 0, C2D_Color32(0, 0, 0, 60));
	int show = b->waste_n < s_game.draw ? b->waste_n : s_game.draw;
	for (int k = 0; k < show; k++) {
		const KlCard* c = &b->waste[b->waste_n - show + k];
		float x = col_x(1) + k * 12;
		gfx_french(sprite(*c), x, ROW_Y, SC, 0.0f, 0, false);
		if (s_sel == KL_WASTE && k == show - 1)
			gfx_rect(x - CW / 2, ROW_Y - CH / 2, CW, 3, 0, COLOR_GOLD);
	}
	for (int s = 0; s < 4; s++) {
		float x = found_x(s);
		if (b->found[s])
			gfx_french(s * 13 + (b->found[s] == 1 ? 12 : b->found[s] - 2), x, ROW_Y, SC, 0.0f, 0, false);
		else
			gfx_rrect(x - CW / 2, ROW_Y - CH / 2, CW, CH, 4, 0, C2D_Color32(0, 0, 0, 60));
		if (s_sel == KL_FOUND0 + s)
			gfx_rect(x - CW / 2, ROW_Y - CH / 2, CW, 3, 0, COLOR_GOLD);
	}
	/* Columnas */
	for (int c = 0; c < KL_COLS; c++) {
		float x = col_x(c);
		gfx_rrect(x - CW / 2, COL_Y - CH / 2, CW, CH, 4, 0, C2D_Color32(0, 0, 0, 50));
		for (int i = 0; i < b->col_n[c]; i++) {
			float y = card_y(c, i);
			gfx_french(sprite(b->col[c][i]), x, y, SC, 0.0f, 0, false);
			if (s_sel == c && i >= b->col_n[c] - s_sel_n)
				gfx_rect(x - CW / 2, y - CH / 2, CW, 3, 0, COLOR_GOLD);
		}
		if (c == s_cursor)
			gfx_rect(x - CW / 2, COL_Y - CH / 2 - 4, CW, 2, 0, C2D_Color32(255, 215, 90, 200));
	}
}

/* Salir a medias desde la pausa: la próxima vez se empieza de cero */
static void scene_leave(void)
{
	s_started = false;
}

const Scene scene_klondike = { kl_enter, kl_update, kl_draw_top, kl_draw_bottom, scene_leave };
