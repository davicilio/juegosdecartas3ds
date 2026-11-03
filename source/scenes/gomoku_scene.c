/* Cinco en raya: el tablero de 15x15 en la pantalla táctil y en grande arriba. */
#include <math.h>
#include <stdio.h>
#include <time.h>

#include "games/gomoku.h"
#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/gfx.h"
#include "ui/mii.h"
#include "ui/setup.h"
#include "ui/widgets.h"

#define RIVAL  CPU_LUCIA
#define B_CELL 14.0f
#define B_X    ((BOTTOM_W - GO_N * B_CELL) / 2.0f)
#define B_Y    24.0f
#define T_CELL 15.0f
#define T_X    16.0f
#define T_Y    7.0f

static const char* const LEVELS[] = { "Fácil", "Normal", "Difícil" };

static Gomoku s_game;
static bool s_started, s_in_setup, s_recorded;
static int s_level = 1, s_cursor = GO_N / 2 * GO_N + GO_N / 2;
static Setup s_setup;
static float s_cpu_timer, s_end_time;
static Mood s_mood;
static Button s_btn_menu;

static void go_enter(void)
{
	if (!s_started) {
		s_started = true;
		s_in_setup = true;
		setup_init(&s_setup, "Cinco en raya", "Pon tus piedras negras por turnos y junta cinco seguidas en horizontal, vertical o diagonal antes que la consola.");
		setup_add(&s_setup, "Dificultad", LEVELS, 3, &s_level);
	}
	button_set(&s_btn_menu, 262, 2, 54, 18, "Menú", C2D_Color32(120, 30, 35, 255), KEY_START);
}

static void begin(void)
{
	go_init(&s_game, s_level, (uint32_t)time(NULL) ^ 0x6060);
	s_recorded = false;
	s_cpu_timer = s_end_time = 0.0f;
}

static SceneId go_update(const Input* in, float dt)
{
	if (button_update(&s_btn_menu, in))
		return SCENE_MENU;
	mood_update(&s_mood, dt);
	if (s_in_setup) {
		if (setup_update(&s_setup, in)) {
			s_in_setup = false;
			begin();
		}
		return SCENE_GOMOKU;
	}
	Gomoku* g = &s_game;
	if (g->winner) {
		s_end_time += dt;
		if (!s_recorded) {
			s_recorded = true;
			profile_record_game(GAME_GOMOKU, g->winner == 1);
			audio_play(g->winner == 1 ? SFX_WIN : SFX_LOSE);
			mood_set(&s_mood, g->winner == 1 ? EXPR_SAD : EXPR_HAPPY, 30.0f);
		}
		if (s_end_time > 1.2f && ((in->down & KEY_A) || in->touch_down))
			begin();
		return SCENE_GOMOKU;
	}
	if (g->current == 2) {
		s_cpu_timer += dt;
		if (s_cpu_timer > 0.5f) {
			s_cpu_timer = 0.0f;
			int m = go_ai(g);
			go_play(g, m / GO_N, m % GO_N);
			audio_play(SFX_CHIP);
		}
		return SCENE_GOMOKU;
	}
	int r = s_cursor / GO_N, c = s_cursor % GO_N;
	if (in->repeat & KEY_LEFT)
		c = (c + GO_N - 1) % GO_N;
	if (in->repeat & KEY_RIGHT)
		c = (c + 1) % GO_N;
	if (in->repeat & KEY_UP)
		r = (r + GO_N - 1) % GO_N;
	if (in->repeat & KEY_DOWN)
		r = (r + 1) % GO_N;
	s_cursor = r * GO_N + c;
	bool place = in->down & KEY_A;
	if (in->touch_down) {
		int tc = (int)floorf((in->tx - B_X) / B_CELL), tr = (int)floorf((in->ty - B_Y) / B_CELL);
		if (tc >= 0 && tr >= 0 && tc < GO_N && tr < GO_N) {
			s_cursor = tr * GO_N + tc;
			place = true;
		}
	}
	if (place) {
		if (go_play(g, s_cursor / GO_N, s_cursor % GO_N))
			audio_play(SFX_CHIP);
		else
			audio_play(SFX_ERROR);
	}
	return SCENE_GOMOKU;
}

static void draw_board(float x0, float y0, float cell, float depth, bool cursor)
{
	const Gomoku* g = &s_game;
	float side = cell * GO_N;
	gfx_rect(x0 - 4, y0 - 4, side + 8, side + 8, depth, C2D_Color32(120, 80, 35, 255));
	gfx_rect(x0, y0, side, side, depth, C2D_Color32(220, 180, 110, 255));
	for (int k = 0; k < GO_N; k++) {
		float p = k * cell + cell / 2;
		gfx_rect(x0 + cell / 2, y0 + p, side - cell, 1, depth, C2D_Color32(90, 60, 30, 255));
		gfx_rect(x0 + p, y0 + cell / 2, 1, side - cell, depth, C2D_Color32(90, 60, 30, 255));
	}
	static const int STARS[5][2] = { { 3, 3 }, { 3, 11 }, { 7, 7 }, { 11, 3 }, { 11, 11 } };
	for (int k = 0; k < 5; k++)
		gfx_ellipse(x0 + STARS[k][1] * cell + cell / 2, y0 + STARS[k][0] * cell + cell / 2, 1.8f, 1.8f, depth,
			C2D_Color32(90, 60, 30, 255));
	float rad = cell * 0.44f;
	for (int r = 0; r < GO_N; r++)
		for (int c = 0; c < GO_N; c++) {
			int v = g->cell[r][c];
			if (!v)
				continue;
			float x = x0 + c * cell + cell / 2, y = y0 + r * cell + cell / 2;
			gfx_ellipse(x + 1, y + 1.2f, rad, rad, depth, C2D_Color32(0, 0, 0, 80));
			gfx_ellipse(x, y, rad, rad, depth, v == 1 ? C2D_Color32(25, 25, 30, 255) : C2D_Color32(245, 245, 240, 255));
			gfx_ellipse(x - rad * 0.3f, y - rad * 0.35f, rad * 0.35f, rad * 0.25f, depth,
				v == 1 ? C2D_Color32(120, 120, 130, 200) : C2D_Color32(255, 255, 255, 255));
			if (r == g->last_r && c == g->last_c)
				gfx_ellipse(x, y, rad * 0.25f, rad * 0.25f, depth, COLOR_RED);
		}
	if (g->winner == 1 || g->winner == 2)
		for (int k = 0; k < 5; k++)
			gfx_ellipse(x0 + g->win_c[k] * cell + cell / 2, y0 + g->win_r[k] * cell + cell / 2, rad * 0.4f, rad * 0.4f,
				depth, COLOR_GOLD);
	if (cursor && !g->winner) {
		float pulse = 0.5f + 0.5f * sinf(gfx_time() * 6.0f);
		float x = x0 + s_cursor % GO_N * cell, y = y0 + s_cursor / GO_N * cell;
		u32 k = C2D_Color32(220, 40, 40, (u8)(140 + 110 * pulse));
		gfx_rect(x, y, cell, 1.5f, depth, k);
		gfx_rect(x, y + cell - 1.5f, cell, 1.5f, depth, k);
		gfx_rect(x, y, 1.5f, cell, depth, k);
		gfx_rect(x + cell - 1.5f, y, 1.5f, cell, depth, k);
	}
}

static void go_draw_top(void)
{
	if (s_in_setup) {
		setup_draw_top(&s_setup);
		return;
	}
	const Gomoku* g = &s_game;
	gfx_background(BG_TABLE_TOP);
	draw_board(T_X, T_Y, T_CELL, DEPTH_TABLE, false);
	float px = 260;
	gfx_panel(px, 20, 130, 120, DEPTH_UI, C2D_Color32(20, 30, 25, 220), C2D_Color32(150, 110, 40, 255));
	mii_draw(cpu_avatar(RIVAL), px + 65, 56, 20, DEPTH_UI, mood_expr(&s_mood), false);
	gfx_text(cpu_name(RIVAL), px + 65, 84, DEPTH_UI, 0.45f, COLOR_CREAM, C2D_AlignCenter);
	gfx_text(LEVELS[g->level], px + 65, 102, DEPTH_UI, 0.42f, COLOR_GOLD, C2D_AlignCenter);
	gfx_text(g->current == 1 ? "Te toca (negras)" : "Piensa...", px + 65, 150, DEPTH_UI, 0.45f, COLOR_CREAM,
		C2D_AlignCenter);
	if (g->winner && banner_draw(s_end_time, 128, 120, 220, 60))
		gfx_text(g->winner == 1 ? "¡Cinco en raya!" : g->winner == 2 ? "Gana la consola" : "Tablas", 128, 104,
			DEPTH_BANNER, 0.6f, COLOR_GOLD, C2D_AlignCenter);
}

static void go_draw_bottom(void)
{
	if (s_in_setup) {
		setup_draw_bottom(&s_setup);
		button_draw(&s_btn_menu, false);
		return;
	}
	gfx_background(BG_TABLE_BOTTOM);
	gfx_text(s_game.winner ? "Toca para jugar otra" : "Toca una intersección para poner tu piedra", 6, 4, 0, 0.42f,
		COLOR_GOLD, C2D_AlignLeft);
	button_draw(&s_btn_menu, false);
	draw_board(B_X, B_Y, B_CELL, 0, true);
}

/* Salir a medias desde la pausa: la próxima vez se empieza de cero */
static void scene_leave(void)
{
	s_started = false;
}

const Scene scene_gomoku = { go_enter, go_update, go_draw_top, go_draw_bottom, scene_leave };
