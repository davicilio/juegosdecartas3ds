/* Conecta 4: tú (rojo) contra la CPU (amarillo). */
#include <math.h>
#include <stdio.h>
#include <time.h>

#include "games/conecta4.h"
#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/gfx.h"
#include "ui/mii.h"
#include "ui/widgets.h"

#define TOP_CELL 30.0f
#define BOT_CELL 26.0f
#define BOT_X0   ((BOTTOM_W - C4_COLS * BOT_CELL) / 2.0f)
#define BOT_Y0   30.0f

static Conecta4 s_game;
static bool s_started;
static int s_cursor = 3;
static float s_cpu_timer, s_end_time, s_fall;
static Mood s_cpu_mood, s_my_mood;
static int s_prev_winner;
static int s_wins[3];
static Button s_btn_drop, s_btn_again, s_btn_menu, s_btn_level;
static const char* const LEVELS[] = { "Fácil", "Normal", "Difícil" };
static int s_level = 1;

static u32 disc_color(int who, int alpha)
{
	return who == 1 ? C2D_Color32(220, 40, 45, alpha) : C2D_Color32(245, 200, 30, alpha);
}

static void apply_level(void)
{
	static const int depth[3] = { 2, 4, 7 };
	s_game.level = depth[s_level];
}

static void c4_enter(void)
{
	if (!s_started) {
		c4_init(&s_game, (uint32_t)time(NULL) ^ 0xC4);
		apply_level();
		s_game.starter = 2;
		c4_new_game(&s_game);
		s_started = true;
	}
	s_prev_winner = s_game.winner;
	button_set(&s_btn_menu, 258, 4, 54, 20, "Menú", C2D_Color32(120, 30, 35, 255), KEY_START);
}

static void drop(int col)
{
	if (c4_drop(&s_game, col)) {
		audio_play(SFX_CHIP);
		s_fall = 0.0f;
	}
}

static SceneId c4_update(const Input* in, float dt)
{
	if (button_update(&s_btn_menu, in))
		return SCENE_MENU;
	static char level_label[24];
	snprintf(level_label, sizeof level_label, GLYPH_Y " %s", LEVELS[s_level]);
	button_set(&s_btn_level, 8, 4, 90, 20, level_label, C2D_Color32(60, 90, 160, 255), KEY_Y);
	if (button_update(&s_btn_level, in)) {
		s_level = (s_level + 1) % 3;
		apply_level();
	}
	s_fall += dt * 4.0f;
	if (!s_game.winner) {
		button_set(&s_btn_drop, 60, 205, 200, 28, GLYPH_A " Soltar ficha", C2D_Color32(40, 140, 60, 255), KEY_A);
		bool mine = s_game.current == 1;
		s_btn_drop.enabled = mine && c4_can_drop(&s_game, s_cursor);
		if (mine) {
			if (in->repeat & KEY_LEFT)
				s_cursor = (s_cursor + C4_COLS - 1) % C4_COLS;
			if (in->repeat & KEY_RIGHT)
				s_cursor = (s_cursor + 1) % C4_COLS;
			if (in->touch_down && in->ty >= BOT_Y0 - 20 && in->ty < BOT_Y0 + C4_ROWS * BOT_CELL &&
				in->tx >= BOT_X0 && in->tx < BOT_X0 + C4_COLS * BOT_CELL) {
				int col = (int)((in->tx - BOT_X0) / BOT_CELL);
				s_cursor = col;
				drop(col);
			} else if (button_update(&s_btn_drop, in)) {
				drop(s_cursor);
			}
		} else if (s_fall > 1.0f) {
			s_cpu_timer += dt;
			if (s_cpu_timer > 0.4f) {
				s_cpu_timer = 0.0f;
				drop(c4_ai_choose(&s_game));
			}
		}
	} else {
		button_set(&s_btn_again, 60, 205, 200, 28, GLYPH_A " Otra partida", C2D_Color32(40, 140, 60, 255), KEY_A);
		s_end_time += dt;
		if (s_end_time > 0.6f && button_update(&s_btn_again, in)) {
			c4_new_game(&s_game);
			s_end_time = 0.0f;
		}
	}
	if (s_game.winner != s_prev_winner) {
		s_prev_winner = s_game.winner;
		if (s_game.winner) {
			s_end_time = 0.0f;
			s_wins[s_game.winner == 3 ? 0 : s_game.winner]++;
			bool won = s_game.winner == 1;
			audio_play(won ? SFX_WIN : s_game.winner == 3 ? SFX_CHIP : SFX_LOSE);
			mood_set(&s_my_mood, won ? EXPR_HAPPY : EXPR_SAD, 4.0f);
			mood_set(&s_cpu_mood, s_game.winner == 2 ? EXPR_HAPPY : EXPR_SAD, 4.0f);
			profile_record_game(GAME_CONECTA4, won);
		}
	}
	mood_update(&s_cpu_mood, dt);
	mood_update(&s_my_mood, dt);
	return SCENE_CONECTA4;
}

/* Tablero con su esquina superior izquierda en (x0, y0) */
static void draw_board(float x0, float y0, float cell, float depth, bool top)
{
	float w = C4_COLS * cell, h = C4_ROWS * cell;
	gfx_rrect(x0 - 6, y0 - 6, w + 12, h + 12, 10, depth, C2D_Color32(30, 70, 170, 255));
	for (int r = 0; r < C4_ROWS; r++)
		for (int c = 0; c < C4_COLS; c++) {
			float cx = x0 + c * cell + cell / 2, cy = y0 + (C4_ROWS - 1 - r) * cell + cell / 2;
			int who = s_game.cell[r][c];
			float rad = cell * 0.4f;
			/* La última ficha cae desde arriba */
			if (who && r == s_game.last_row && c == s_game.last_col && s_fall < 1.0f) {
				float t = s_fall;
				cy = y0 - cell + (cy - (y0 - cell)) * t * t;
			}
			gfx_ellipse(x0 + c * cell + cell / 2, y0 + (C4_ROWS - 1 - r) * cell + cell / 2, rad, rad,
				depth, C2D_Color32(10, 25, 60, 255));
			if (who) {
				gfx_ellipse(cx, cy, rad, rad, depth + 0.3f, disc_color(who, 255));
				gfx_ellipse(cx - rad * 0.25f, cy - rad * 0.25f, rad * 0.35f, rad * 0.3f, depth + 0.3f,
					C2D_Color32(255, 255, 255, 70));
			}
		}
	/* Línea ganadora */
	if (s_game.winner == 1 || s_game.winner == 2) {
		float pulse = 0.5f + 0.5f * sinf(gfx_time() * 6.0f);
		for (int k = 0; k < 4; k++) {
			float cx = x0 + s_game.line[k][1] * cell + cell / 2;
			float cy = y0 + (C4_ROWS - 1 - s_game.line[k][0]) * cell + cell / 2;
			gfx_ellipse(cx, cy, cell * 0.16f, cell * 0.16f, depth + 0.5f,
				C2D_Color32(255, 255, 255, (u8)(150 + 100 * pulse)));
		}
	}
	(void)top;
}

static void c4_draw_top(void)
{
	gfx_background(BG_TABLE_TOP);
	float x0 = (TOP_W - C4_COLS * TOP_CELL) / 2.0f, y0 = 34.0f;
	draw_board(x0, y0, TOP_CELL, DEPTH_CARDS, true);
	mii_draw(&profile_get()->avatar, 40, 70, 20, DEPTH_UI, mood_expr(&s_my_mood), false);
	gfx_ellipse(40, 104, 9, 9, DEPTH_UI, disc_color(1, 255));
	gfx_textf(40, 116, DEPTH_UI, 0.45f, COLOR_CREAM, C2D_AlignCenter, "Tú: %d", s_wins[1]);
	mii_draw(cpu_avatar(CPU_PEPE), 360, 70, 20, DEPTH_UI, mood_expr(&s_cpu_mood), false);
	gfx_ellipse(360, 104, 9, 9, DEPTH_UI, disc_color(2, 255));
	gfx_textf(360, 116, DEPTH_UI, 0.45f, COLOR_CREAM, C2D_AlignCenter, "%s: %d", cpu_name(CPU_PEPE), s_wins[2]);
	if (s_game.winner && banner_draw(s_end_time, 200, 120, 300, 56)) {
		const char* t = s_game.winner == 1 ? "¡Cuatro en línea! Ganas" : s_game.winner == 2 ? "Gana la CPU" : "Empate";
		gfx_text(t, 200, 108, DEPTH_BANNER, 0.65f, s_game.winner == 1 ? COLOR_GOLD : COLOR_CREAM, C2D_AlignCenter);
	}
}

static void c4_draw_bottom(void)
{
	gfx_background(BG_TABLE_BOTTOM);
	button_draw(&s_btn_menu, false);
	button_draw(&s_btn_level, true);
	draw_board(BOT_X0, BOT_Y0, BOT_CELL, 0, false);
	if (!s_game.winner && s_game.current == 1) {
		float cx = BOT_X0 + s_cursor * BOT_CELL + BOT_CELL / 2;
		gfx_ellipse(cx, BOT_Y0 - 14, BOT_CELL * 0.35f, BOT_CELL * 0.35f, 0, disc_color(1, 220));
		gfx_text("Toca una columna", BOTTOM_W / 2, 190, 0, 0.42f, C2D_Color32(200, 230, 200, 255), C2D_AlignCenter);
	} else if (!s_game.winner) {
		gfx_text("Piensa la CPU...", BOTTOM_W / 2, 190, 0, 0.45f, COLOR_CREAM, C2D_AlignCenter);
	}
	if (!s_game.winner)
		button_draw(&s_btn_drop, s_btn_drop.enabled);
	else
		button_draw(&s_btn_again, s_end_time > 0.6f);
}

/* Salir a medias desde la pausa: la próxima vez se empieza de cero */
static void scene_leave(void)
{
	s_started = false;
}

const Scene scene_conecta4 = { c4_enter, c4_update, c4_draw_top, c4_draw_bottom, scene_leave };
