/* Dominó por parejas: tú y tu compañero (arriba) contra los de los lados. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "games/domino.h"
#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/gfx.h"
#include "ui/table.h"
#include "ui/widgets.h"

#define MY_TILE_W 30.0f
#define MY_Y      96.0f

static Domino s_game;
static Table s_table;
static bool s_started;
static int s_cursor;
static int s_choose = -1; /* ficha que vale por los dos lados: hay que elegir */
static float s_cpu_timer, s_end_time;
static DoPhase s_prev_phase;
static Button s_btn_left, s_btn_right, s_btn_pass, s_btn_menu;

/* Ficha con su centro en (cx, cy); w es el lado corto. Horizontal: a a la
 * izquierda; vertical: a arriba. */
static void draw_tile(DoTile t, float cx, float cy, float w, bool vertical, float depth, bool dim)
{
	float tw = vertical ? w : w * 2, th = vertical ? w * 2 : w;
	gfx_rrect(cx - tw / 2 + 1, cy - th / 2 + 2, tw, th, w * 0.18f, depth, C2D_Color32(0, 0, 0, 80));
	gfx_rrect(cx - tw / 2, cy - th / 2, tw, th, w * 0.18f, depth,
		dim ? C2D_Color32(170, 165, 150, 255) : C2D_Color32(250, 247, 235, 255));
	u32 ink = C2D_Color32(30, 30, 35, 255);
	if (vertical)
		gfx_rect(cx - w * 0.38f, cy - 0.5f, w * 0.76f, 1.2f, depth, C2D_Color32(120, 110, 100, 255));
	else
		gfx_rect(cx - 0.5f, cy - w * 0.38f, 1.2f, w * 0.76f, depth, C2D_Color32(120, 110, 100, 255));
	/* Puntos: rejilla de 3x3 en cada mitad */
	static const int8_t PIPS[7][6][2] = {
		{ { 9, 9 } },
		{ { 0, 0 } },
		{ { -1, -1 }, { 1, 1 } },
		{ { -1, -1 }, { 0, 0 }, { 1, 1 } },
		{ { -1, -1 }, { 1, -1 }, { -1, 1 }, { 1, 1 } },
		{ { -1, -1 }, { 1, -1 }, { 0, 0 }, { -1, 1 }, { 1, 1 } },
		{ { -1, -1 }, { 1, -1 }, { -1, 0 }, { 1, 0 }, { -1, 1 }, { 1, 1 } },
	};
	int vals[2] = { t.a, t.b };
	float pr = w * 0.09f, step = w * 0.27f;
	for (int half = 0; half < 2; half++) {
		float hx = vertical ? cx : cx + (half ? w / 2 : -w / 2);
		float hy = vertical ? cy + (half ? w / 2 : -w / 2) : cy;
		for (int k = 0; k < vals[half]; k++)
			gfx_ellipse(hx + PIPS[vals[half]][k][0] * step, hy + PIPS[vals[half]][k][1] * step, pr, pr, depth, ink);
	}
}

static void start_hand(void)
{
	s_cursor = 0;
	s_choose = -1;
	s_cpu_timer = s_end_time = 0.0f;
	s_prev_phase = s_game.phase;
	table_message(&s_table, s_game.first_hand ? "Sale quien tiene el seis doble: %s" : "Sale %s",
		s_game.current == 0 ? "tú" : table_name(&s_table, s_game.current));
}

static void do_enter(void)
{
	if (!s_started) {
		table_init(&s_table, CPU_PACO, CPU_LOLA, CPU_MANOLO);
		do_init(&s_game, (uint32_t)time(NULL) ^ 0xD0);
		s_started = true;
		start_hand();
	}
	button_set(&s_btn_menu, 258, 6, 54, 20, "Menú", C2D_Color32(120, 30, 35, 255), KEY_START);
}

static void play(int idx, int side)
{
	int p = s_game.current;
	do_play(&s_game, idx, side);
	audio_play(SFX_CHIP);
	s_choose = -1;
	if (s_game.phase != DO_PLAYING && s_game.domino_player >= 0) {
		table_bubble(&s_table, p, "¡Dominó!");
	}
}

static SceneId do_update(const Input* in, float dt)
{
	if (button_update(&s_btn_menu, in))
		return SCENE_MENU;
	const float y = 205, h = 28;
	if (s_game.phase == DO_PLAYING) {
		bool mine = s_game.current == 0;
		static char left_label[24], right_label[24];
		snprintf(left_label, sizeof left_label, GLYPH_L " Al %d", do_left(&s_game));
		snprintf(right_label, sizeof right_label, GLYPH_R " Al %d", do_right(&s_game));
		button_set(&s_btn_left, 8, y, 100, h, left_label, C2D_Color32(60, 90, 160, 255), KEY_L);
		button_set(&s_btn_right, 212, y, 100, h, right_label, C2D_Color32(60, 90, 160, 255), KEY_R);
		button_set(&s_btn_pass, 112, y, 96, h, GLYPH_X " Paso", C2D_Color32(190, 60, 50, 255), KEY_X);
		s_btn_left.enabled = mine && s_choose >= 0;
		s_btn_right.enabled = mine && s_choose >= 0;
		s_btn_pass.enabled = mine && !do_has_move(&s_game, 0);
		if (mine) {
			int n = s_game.hand_count[0];
			if (in->repeat & KEY_LEFT)
				s_cursor = (s_cursor + n - 1) % n;
			if (in->repeat & KEY_RIGHT)
				s_cursor = (s_cursor + 1) % n;
			if (s_cursor >= n)
				s_cursor = n - 1;
			int t = -1;
			if (in->touch_down)
				for (int i = 0; i < n; i++) {
					float x = BOTTOM_W / 2.0f + (i - (n - 1) / 2.0f) * (MY_TILE_W + 8);
					if (fabsf(in->tx - x) <= MY_TILE_W / 2 && fabsf(in->ty - MY_Y) <= MY_TILE_W)
						t = i;
				}
			int pick = t >= 0 ? t : (in->down & KEY_A) ? s_cursor : -1;
			if (pick >= 0) {
				s_cursor = pick;
				bool l = do_can_play(&s_game, pick, 0), r = do_can_play(&s_game, pick, 1);
				if (l && r && s_game.chain_count > 0 && do_left(&s_game) != do_right(&s_game)) {
					s_choose = pick;
					table_message(&s_table, "¿Por qué lado la pones?");
				} else if (l || r) {
					play(pick, l ? 0 : 1);
				} else {
					audio_play(SFX_ERROR);
					table_message(&s_table, "Esa ficha no casa con ningún extremo");
				}
			} else if (button_update(&s_btn_left, in)) {
				play(s_choose, 0);
			} else if (button_update(&s_btn_right, in)) {
				play(s_choose, 1);
			} else if (button_update(&s_btn_pass, in)) {
				do_pass(&s_game);
				table_bubble(&s_table, 0, "Paso");
			}
		} else {
			s_cpu_timer += dt;
			if (s_cpu_timer > 0.9f) {
				s_cpu_timer = 0.0f;
				int p = s_game.current, idx, side;
				if (do_ai_choose(&s_game, &idx, &side)) {
					play(idx, side);
				} else {
					do_pass(&s_game);
					table_bubble(&s_table, p, "Paso");
				}
			}
		}
	} else {
		bool over = s_game.phase == DO_GAME_OVER;
		button_set(&s_btn_left, 12, y, 144, h, over ? GLYPH_A " Otra partida" : GLYPH_A " Siguiente mano",
			C2D_Color32(40, 140, 60, 255), KEY_A);
		button_set(&s_btn_right, 164, y, 144, h, GLYPH_B " Menú", C2D_Color32(190, 60, 50, 255), KEY_B);
		s_btn_left.enabled = s_btn_right.enabled = s_end_time > 0.6f;
		s_end_time += dt;
		if (button_update(&s_btn_left, in)) {
			if (over)
				do_init(&s_game, (uint32_t)time(NULL));
			else
				do_new_hand(&s_game);
			start_hand();
		} else if (button_update(&s_btn_right, in)) {
			return SCENE_MENU;
		}
	}
	if (s_game.phase != s_prev_phase) {
		if (s_game.phase != DO_PLAYING) {
			s_end_time = 0.0f;
			int team = s_game.hand_winner;
			for (int p = 0; p < DO_PLAYERS; p++)
				table_mood(&s_table, p, p % 2 == team ? EXPR_HAPPY : EXPR_SAD, 3.0f);
			audio_play(team == 0 ? SFX_WIN : SFX_LOSE);
			if (s_game.phase == DO_GAME_OVER)
				profile_record_game(GAME_DOMINO, team == 0);
		}
		s_prev_phase = s_game.phase;
	}
	table_update(&s_table, dt);
	return SCENE_DOMINO;
}

/* La cadena en filas que van y vuelven, de arriba abajo */
static void draw_chain(void)
{
	const float w = 11.0f; /* lado corto */
	const int per_row = 13;
	int n = s_game.chain_count;
	int rows = (n + per_row - 1) / per_row;
	float y0 = 120 - (rows - 1) * 14.0f;
	for (int i = 0; i < n; i++) {
		int row = i / per_row, k = i % per_row;
		int in_row = row == rows - 1 ? n - row * per_row : per_row;
		bool reverse = row % 2 == 1;
		float x = 200 + ((reverse ? in_row - 1 - k : k) - (in_row - 1) / 2.0f) * (w * 2 + 2);
		DoTile t = s_game.chain[i];
		if (reverse) {
			uint8_t tmp = t.a;
			t.a = t.b;
			t.b = tmp;
		}
		draw_tile(t, x, y0 + row * 28.0f, w, false, DEPTH_CARDS, false);
	}
}

static void do_draw_top(void)
{
	gfx_background(BG_TABLE_TOP);
	for (int p = 1; p < DO_PLAYERS; p++) {
		char info[24];
		snprintf(info, sizeof info, "%d fichas", s_game.hand_count[p]);
		table_draw_seat(&s_table, p, 0, s_game.current == p && s_game.phase == DO_PLAYING, info);
	}
	draw_chain();
	gfx_panel(286, 180, 104, 50, DEPTH_UI, C2D_Color32(10, 50, 25, 210), C2D_Color32(20, 60, 30, 255));
	gfx_textf(338, 186, DEPTH_UI, 0.42f, C2D_Color32(140, 200, 255, 255), C2D_AlignCenter, "Nosotros %d",
		s_game.score[0]);
	gfx_textf(338, 204, DEPTH_UI, 0.42f, C2D_Color32(255, 150, 140, 255), C2D_AlignCenter, "Ellos %d (a %d)",
		s_game.score[1], DO_GAME);
	table_draw_me(&s_table, NULL);
	table_draw_bubbles(&s_table);
	if (s_game.phase != DO_PLAYING && banner_draw(s_end_time, 200, 112, 320, 76)) {
		int team = s_game.hand_winner;
		const char* how = s_game.blocked ? "Se cierra la mano" : "¡Dominó!";
		gfx_text(how, 200, 84, DEPTH_BANNER, 0.55f, COLOR_CREAM, C2D_AlignCenter);
		gfx_textf(200, 108, DEPTH_BANNER, 0.55f, team == 0 ? COLOR_GOLD : COLOR_RED, C2D_AlignCenter,
			s_game.phase == DO_GAME_OVER ? (team == 0 ? "¡Ganáis la partida! (+%d)" : "Ganan ellos la partida (+%d)")
			                             : (team == 0 ? "Ganáis la mano: +%d" : "Ganan ellos: +%d"),
			s_game.hand_points);
	}
}

static void do_draw_bottom(void)
{
	gfx_background(BG_TABLE_BOTTOM);
	table_draw_message(&s_table);
	button_draw(&s_btn_menu, false);
	bool mine = s_game.current == 0 && s_game.phase == DO_PLAYING;
	if (mine)
		gfx_textf(BOTTOM_W / 2, 30, 0, 0.5f, COLOR_GOLD, C2D_AlignCenter, s_game.chain_count ? "¡Tu turno! Extremos: %d y %d" : "¡Tu turno!",
			do_left(&s_game), do_right(&s_game));
	int n = s_game.hand_count[0];
	float pulse = 0.6f + 0.4f * sinf(gfx_time() * 6.0f);
	for (int i = 0; i < n; i++) {
		float x = BOTTOM_W / 2.0f + (i - (n - 1) / 2.0f) * (MY_TILE_W + 8);
		bool ok = !mine || do_can_play(&s_game, i, 0) || do_can_play(&s_game, i, 1);
		float y = MY_Y - ((mine && (i == s_cursor || i == s_choose)) ? 8.0f : 0.0f);
		if (mine && i == s_cursor)
			gfx_rrect(x - MY_TILE_W / 2 - 3, y - MY_TILE_W - 3, MY_TILE_W + 6, MY_TILE_W * 2 + 6, 6, 0,
				C2D_Color32(255, 215, 90, (u8)(255 * pulse)));
		draw_tile(s_game.hands[0][i], x, y, MY_TILE_W, true, 0, !ok);
	}
	if (s_game.phase == DO_PLAYING) {
		gfx_textf(BOTTOM_W / 2, 172, 0, 0.42f, C2D_Color32(200, 230, 200, 255), C2D_AlignCenter,
			"Puntos en tu mano: %d", do_pips(&s_game, 0));
		if (s_choose >= 0) {
			button_draw(&s_btn_left, true);
			button_draw(&s_btn_right, true);
		}
		button_draw(&s_btn_pass, s_btn_pass.enabled);
	} else {
		button_draw(&s_btn_left, s_btn_left.enabled);
		button_draw(&s_btn_right, s_btn_right.enabled);
	}
}

/* Salir a medias desde la pausa: la próxima vez se empieza de cero */
static void scene_leave(void)
{
	s_started = false;
}

const Scene scene_domino = { do_enter, do_update, do_draw_top, do_draw_bottom, scene_leave };
