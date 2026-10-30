/* Toma 6: elige carta, se colocan de menor a mayor y la sexta de una fila
 * se la lleva entera. */
#include <math.h>
#include <stdio.h>
#include <time.h>

#include "games/toma6.h"
#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/gfx.h"
#include "ui/table.h"
#include "ui/widgets.h"

#define CARD_TW 30.0f
#define CARD_TH 42.0f
#define ROW_X0  92.0f
#define ROW_Y0  26.0f
#define ROW_DY  48.0f

static Toma6 s_game;
static Table s_table;
static bool s_started;
static int s_cursor;
static float s_timer, s_end_time;
static TmPhase s_prev_phase;
static Button s_btn_main, s_btn_rows[TM_ROWS], s_btn_menu;

static const char* who(int p)
{
	return p == 0 ? "Tú" : table_name(&s_table, p);
}

static u32 card_color(int c)
{
	switch (tm_heads(c)) {
	case 7:
		return C2D_Color32(170, 90, 200, 255);
	case 5:
		return C2D_Color32(230, 90, 90, 255);
	case 3:
		return C2D_Color32(245, 205, 70, 255);
	case 2:
		return C2D_Color32(120, 180, 235, 255);
	default:
		return C2D_Color32(250, 248, 240, 255);
	}
}

/* Carta dibujada: número grande y sus cabezas de buey arriba */
static void draw_card(int c, float x, float y, float sc, float depth, bool dim)
{
	float w = CARD_TW * sc, h = CARD_TH * sc;
	gfx_rrect(x - w / 2 + 1.5f, y - h / 2 + 2, w, h, 4 * sc, depth, C2D_Color32(0, 0, 0, 90));
	gfx_rrect(x - w / 2, y - h / 2, w, h, 4 * sc, depth, C2D_Color32(60, 40, 60, 255));
	gfx_rrect(x - w / 2 + 1, y - h / 2 + 1, w - 2, h - 2, 3 * sc, depth, card_color(c));
	int n = tm_heads(c);
	for (int i = 0; i < n; i++) {
		float hx = x + (i - (n - 1) / 2.0f) * 4.2f * sc, hy = y - h / 2 + 6 * sc;
		gfx_ellipse(hx, hy, 1.9f * sc, 1.7f * sc, depth, C2D_Color32(110, 40, 120, 255));
	}
	gfx_textf_plain(x, y - 6 * sc, depth, 0.62f * sc, C2D_Color32(60, 30, 60, 255), C2D_AlignCenter, "%d", c);
	if (dim)
		gfx_rrect(x - w / 2, y - h / 2, w, h, 4 * sc, depth, C2D_Color32(20, 20, 30, 110));
}

static void start_round(void)
{
	s_cursor = 0;
	s_timer = s_end_time = 0.0f;
	s_prev_phase = s_game.phase;
	table_message(&s_table, "Elige una carta. La sexta de una fila se la lleva");
}

static void tm_enter(void)
{
	if (!s_started) {
		table_init(&s_table, CPU_LUCIA, CPU_RAMON, CPU_CARMEN);
		tm_init(&s_game, (uint32_t)time(NULL) ^ 0x7066);
		s_started = true;
		start_round();
	}
	button_set(&s_btn_menu, 258, 6, 54, 20, "Menú", C2D_Color32(120, 30, 35, 255), KEY_START);
	static char labels[TM_ROWS][24];
	for (int r = 0; r < TM_ROWS; r++) {
		snprintf(labels[r], sizeof labels[r], "Fila %d", r + 1);
		button_set(&s_btn_rows[r], 12 + (r % 2) * 152, 140 + (r / 2) * 34, 144, 28, labels[r],
			C2D_Color32(190, 60, 50, 255), 0);
	}
}

static void choose(int idx)
{
	if (!tm_choose(&s_game, 0, idx))
		return;
	audio_play(SFX_CARD);
	for (int p = 1; p < TM_PLAYERS; p++)
		tm_choose(&s_game, p, tm_ai_card(&s_game, p));
	s_timer = -0.6f; /* un momento para ver las cartas */
	table_message(&s_table, "Se colocan de la más baja a la más alta");
}

static void after_place(void)
{
	int p = s_game.last_player;
	audio_play(SFX_CARD);
	if (s_game.last_took) {
		table_message(&s_table, "%s %s la fila %d: %d cabezas", who(p), p == 0 ? "te llevas" : "se lleva",
			s_game.last_row + 1, s_game.last_taken);
		table_mood(&s_table, p, EXPR_SAD, 2.0f);
		audio_play(p == 0 ? SFX_LOSE : SFX_CHIP);
		if (s_game.last_taken >= 5)
			table_bubble(&s_table, p, "¡Vaya toro!");
	}
}

static SceneId tm_update(const Input* in, float dt)
{
	if (button_update(&s_btn_menu, in))
		return SCENE_MENU;
	s_timer += dt;
	int n = s_game.hand_n[0];
	switch (s_game.phase) {
	case TM_CHOOSE:
		button_set(&s_btn_main, 60, 205, 200, 28, GLYPH_A " Jugar esta", C2D_Color32(40, 140, 60, 255), KEY_A);
		s_btn_main.enabled = !s_game.played[0];
		if (!s_game.played[0] && n > 0) {
			if (in->repeat & KEY_LEFT)
				s_cursor = (s_cursor + n - 1) % n;
			if (in->repeat & KEY_RIGHT)
				s_cursor = (s_cursor + 1) % n;
			if (s_cursor >= n)
				s_cursor = n - 1;
			int t = -1;
			if (in->touch_down)
				for (int i = n - 1; i >= 0 && t < 0; i--) {
					float x = BOTTOM_W / 2.0f + (i - (n - 1) / 2.0f) * 31.0f;
					if (fabsf(in->tx - x) < 15 && fabsf(in->ty - 100) < 24)
						t = i;
				}
			if (t >= 0) {
				s_cursor = t;
				choose(t);
			} else if (button_update(&s_btn_main, in)) {
				choose(s_cursor);
			}
		}
		break;
	case TM_PLACE:
		if (s_timer > 0.8f) {
			s_timer = 0.0f;
			tm_step(&s_game);
			if (s_game.phase != TM_TAKE_ROW)
				after_place();
		}
		break;
	case TM_TAKE_ROW:
		if (s_game.chooser == 0) {
			for (int r = 0; r < TM_ROWS; r++) {
				static char labels[TM_ROWS][32];
				snprintf(labels[r], sizeof labels[r], "Fila %d: %d cabezas", r + 1, tm_row_heads(&s_game, r));
				s_btn_rows[r].label = labels[r];
				if (button_update(&s_btn_rows[r], in)) {
					tm_take_row(&s_game, r);
					after_place();
					s_timer = 0.0f;
					break;
				}
			}
		} else if (s_timer > 0.8f) {
			s_timer = 0.0f;
			tm_take_row(&s_game, tm_ai_row(&s_game));
			after_place();
		}
		break;
	case TM_ROUND_OVER:
	case TM_GAME_OVER: {
		bool over = s_game.phase == TM_GAME_OVER;
		button_set(&s_btn_main, 60, 205, 200, 28, over ? GLYPH_A " Otra partida" : GLYPH_A " Otra ronda",
			C2D_Color32(40, 140, 60, 255), KEY_A);
		s_end_time += dt;
		s_btn_main.enabled = s_end_time > 0.8f;
		if (button_update(&s_btn_main, in)) {
			if (over)
				tm_new_game(&s_game);
			else
				tm_new_round(&s_game);
			start_round();
		}
		break;
	}
	}
	if (s_game.phase != s_prev_phase) {
		if (s_game.phase == TM_TAKE_ROW)
			table_message(&s_table, s_game.chooser == 0 ? "Tu carta es la más baja: elige qué fila te llevas"
			                                            : "%s tiene la más baja y elige fila", who(s_game.chooser));
		if (s_game.phase >= TM_ROUND_OVER) {
			s_end_time = 0.0f;
			if (s_game.phase == TM_GAME_OVER) {
				int w = tm_winner(&s_game);
				profile_record_game(GAME_TOMA6, w == 0);
				audio_play(w == 0 ? SFX_WIN : SFX_LOSE);
				for (int p = 0; p < TM_PLAYERS; p++)
					table_mood(&s_table, p, p == w ? EXPR_HAPPY : EXPR_SAD, 5.0f);
			}
		}
		s_prev_phase = s_game.phase;
	}
	table_update(&s_table, dt);
	return SCENE_TOMA6;
}

static void tm_draw_top(void)
{
	gfx_background(BG_TABLE_TOP);
	/* Las cuatro filas */
	for (int r = 0; r < TM_ROWS; r++) {
		float y = ROW_Y0 + r * ROW_DY + CARD_TH / 2;
		gfx_rrect(ROW_X0 - 22, y - CARD_TH / 2 - 2, 6 * (CARD_TW + 3) + 26, CARD_TH + 4, 6,
			DEPTH_TABLE, C2D_Color32(10, 50, 25, 140));
		gfx_textf(ROW_X0 - 12, y - 8, DEPTH_UI, 0.45f, COLOR_CREAM, C2D_AlignCenter, "%d", r + 1);
		/* El hueco de la sexta, en rojo */
		float x6 = ROW_X0 + 5 * (CARD_TW + 3) + CARD_TW / 2;
		gfx_rrect(x6 - CARD_TW / 2, y - CARD_TH / 2, CARD_TW, CARD_TH, 4, DEPTH_TABLE,
			C2D_Color32(200, 50, 50, 90));
		for (int i = 0; i < s_game.row_n[r]; i++)
			draw_card(s_game.row[r][i], ROW_X0 + i * (CARD_TW + 3) + CARD_TW / 2, y, 1.0f, DEPTH_CARDS, false);
		gfx_textf(x6 + CARD_TW / 2 + 14, y - 8, DEPTH_UI, 0.42f, C2D_Color32(200, 230, 200, 255), C2D_AlignCenter,
			"%d", tm_row_heads(&s_game, r));
	}
	/* Los jugadores y sus cartas del turno */
	static const float PX[TM_PLAYERS] = { 36, 36, 364, 364 }, PY[TM_PLAYERS] = { 200, 40, 40, 200 };
	for (int p = 0; p < TM_PLAYERS; p++) {
		mii_draw(table_avatar(&s_table, p), PX[p], PY[p], 15, DEPTH_UI, mood_expr(&s_table.mood[p]), false);
		gfx_textf(PX[p], PY[p] + 16, DEPTH_UI, 0.42f, COLOR_CREAM, C2D_AlignCenter, "%s", p == 0 ? "Tú" : who(p));
		gfx_textf(PX[p], PY[p] + 29, DEPTH_UI, 0.42f, COLOR_GOLD, C2D_AlignCenter, "%d cabezas", s_game.score[p]);
		if (s_game.played[p] && s_game.phase != TM_CHOOSE)
			draw_card(s_game.played[p], PX[p] + (p < 2 ? 42 : -42), PY[p], 0.85f, DEPTH_CARDS + 0.5f, false);
		else if (s_game.played[p])
			gfx_rrect(PX[p] + (p < 2 ? 30 : -54), PY[p] - 18, 24, 34, 4, DEPTH_CARDS,
				C2D_Color32(110, 40, 120, 255));
	}
	table_draw_bubbles(&s_table);
	if (s_game.phase >= TM_ROUND_OVER && banner_draw(s_end_time, 200, 112, 320, 90)) {
		bool over = s_game.phase == TM_GAME_OVER;
		int w = tm_winner(&s_game);
		gfx_text(over ? (w == 0 ? "¡Ganas la partida!" : "Fin de la partida") : "Fin de la ronda", 200, 76, DEPTH_BANNER,
			0.62f, over && w != 0 ? COLOR_RED : COLOR_GOLD, C2D_AlignCenter);
		gfx_textf(200, 102, DEPTH_BANNER, 0.45f, COLOR_CREAM, C2D_AlignCenter, "Esta ronda: tú %d · %d · %d · %d",
			s_game.round_heads[0], s_game.round_heads[1], s_game.round_heads[2], s_game.round_heads[3]);
		gfx_textf(200, 122, DEPTH_BANNER, 0.47f, COLOR_GOLD, C2D_AlignCenter, "Total: tú %d · %d · %d · %d (a %d)",
			s_game.score[0], s_game.score[1], s_game.score[2], s_game.score[3], TM_END);
	}
}

static void tm_draw_bottom(void)
{
	gfx_background(BG_TABLE_BOTTOM);
	table_draw_message(&s_table);
	button_draw(&s_btn_menu, false);
	if (s_game.phase == TM_TAKE_ROW && s_game.chooser == 0) {
		gfx_textf(BOTTOM_W / 2, 40, 0, 0.5f, COLOR_GOLD, C2D_AlignCenter, "Tu %d es más baja que todas las filas",
			s_game.played[0]);
		draw_card(s_game.played[0], BOTTOM_W / 2, 92, 1.4f, 0, false);
		for (int r = 0; r < TM_ROWS; r++)
			button_draw(&s_btn_rows[r], true);
		return;
	}
	int n = s_game.hand_n[0];
	bool choosing = s_game.phase == TM_CHOOSE && !s_game.played[0];
	for (int i = 0; i < n; i++) {
		float x = BOTTOM_W / 2.0f + (i - (n - 1) / 2.0f) * 31.0f;
		float y = 100 - (choosing && i == s_cursor ? 10 : 0);
		draw_card(s_game.hand[0][i], x, y, 1.0f, 0, false);
	}
	if (choosing)
		gfx_text("Toca la carta que juegas", BOTTOM_W / 2, 30, 0, 0.47f, COLOR_GOLD, C2D_AlignCenter);
	gfx_text("Cabezas de buey: casi todas 1, acabadas en 5: 2,", BOTTOM_W / 2, 146, 0, 0.42f,
		C2D_Color32(200, 230, 200, 255), C2D_AlignCenter);
	gfx_text("de 10: 3, dobles (11, 22...): 5 y el 55: 7", BOTTOM_W / 2, 162, 0, 0.42f, C2D_Color32(200, 230, 200, 255),
		C2D_AlignCenter);
	if (s_game.phase == TM_CHOOSE || s_game.phase >= TM_ROUND_OVER)
		button_draw(&s_btn_main, s_btn_main.enabled);
}

/* Salir a medias desde la pausa: la próxima vez se empieza de cero */
static void scene_leave(void)
{
	s_started = false;
}

const Scene scene_toma6 = { tm_enter, tm_update, tm_draw_top, tm_draw_bottom, scene_leave };
