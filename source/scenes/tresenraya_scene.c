/* Tres en raya: tú (X) contra la CPU (O). */
#include <math.h>
#include <stdio.h>
#include <time.h>

#include "games/tresenraya.h"
#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/gfx.h"
#include "ui/mii.h"
#include "ui/widgets.h"

#define TOP_CELL 62.0f
#define BOT_CELL 54.0f
#define BOT_X0   ((BOTTOM_W - 3 * BOT_CELL) / 2.0f)
#define BOT_Y0   30.0f

static TresEnRaya s_game;
static bool s_started;
static int s_cursor = 4;
static float s_cpu_timer, s_end_time, s_pop;
static Mood s_cpu_mood, s_my_mood;
static int s_prev_winner;
static int s_wins[3];
static Button s_btn_again, s_btn_menu, s_btn_level;
static const char* const LEVELS[] = { "Fácil", "Normal", "Difícil" };

static void tx_enter(void)
{
	if (!s_started) {
		tx_init(&s_game, (uint32_t)time(NULL) ^ 0x3E2);
		s_started = true;
	}
	s_prev_winner = s_game.winner;
	button_set(&s_btn_menu, 258, 4, 54, 20, "Menú", C2D_Color32(120, 30, 35, 255), KEY_START);
}

static void place(int cell)
{
	if (tx_play(&s_game, cell)) {
		audio_play(SFX_CHIP);
		s_pop = 0.0f;
	} else if (s_game.current == 1) {
		audio_play(SFX_ERROR);
	}
}

static SceneId tx_update(const Input* in, float dt)
{
	if (button_update(&s_btn_menu, in))
		return SCENE_MENU;
	static char level_label[24];
	snprintf(level_label, sizeof level_label, GLYPH_Y " %s", LEVELS[s_game.level]);
	button_set(&s_btn_level, 8, 4, 90, 20, level_label, C2D_Color32(60, 90, 160, 255), KEY_Y);
	if (button_update(&s_btn_level, in))
		s_game.level = (s_game.level + 1) % 3;
	s_pop += dt * 5.0f;
	if (!s_game.winner) {
		if (s_game.current == 1) {
			int r = s_cursor / 3, c = s_cursor % 3;
			if (in->repeat & KEY_LEFT)
				c = (c + 2) % 3;
			if (in->repeat & KEY_RIGHT)
				c = (c + 1) % 3;
			if (in->repeat & KEY_UP)
				r = (r + 2) % 3;
			if (in->repeat & KEY_DOWN)
				r = (r + 1) % 3;
			s_cursor = r * 3 + c;
			if (in->touch_down && in->tx >= BOT_X0 && in->tx < BOT_X0 + 3 * BOT_CELL && in->ty >= BOT_Y0 &&
				in->ty < BOT_Y0 + 3 * BOT_CELL) {
				s_cursor = (int)((in->ty - BOT_Y0) / BOT_CELL) * 3 + (int)((in->tx - BOT_X0) / BOT_CELL);
				place(s_cursor);
			} else if (in->down & KEY_A) {
				place(s_cursor);
			}
		} else if (s_pop > 1.0f) {
			s_cpu_timer += dt;
			if (s_cpu_timer > 0.5f) {
				s_cpu_timer = 0.0f;
				place(tx_ai_choose(&s_game));
			}
		}
	} else {
		button_set(&s_btn_again, 60, 205, 200, 28, GLYPH_A " Otra partida", C2D_Color32(40, 140, 60, 255), KEY_A);
		s_end_time += dt;
		if (s_end_time > 0.6f && button_update(&s_btn_again, in)) {
			tx_new_game(&s_game);
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
			profile_record_game(GAME_TRESENRAYA, won);
		}
	}
	mood_update(&s_cpu_mood, dt);
	mood_update(&s_my_mood, dt);
	return SCENE_TRESENRAYA;
}

static void draw_mark(int who, float cx, float cy, float size, float depth, float scale)
{
	float s = size * 0.32f * scale;
	if (who == 1) {
		u32 col = C2D_Color32(220, 50, 50, 255);
		gfx_line(cx - s, cy - s, cx + s, cy + s, size * 0.12f, depth, col);
		gfx_line(cx + s, cy - s, cx - s, cy + s, size * 0.12f, depth, col);
	} else if (who == 2) {
		gfx_ellipse(cx, cy, s * 1.1f, s * 1.1f, depth, C2D_Color32(40, 100, 210, 255));
		gfx_ellipse(cx, cy, s * 0.72f, s * 0.72f, depth, C2D_Color32(245, 240, 225, 255));
	}
}

static void draw_board(float x0, float y0, float cell, float depth, bool cursor)
{
	gfx_rrect(x0 - 6, y0 - 6, cell * 3 + 12, cell * 3 + 12, 10, depth, C2D_Color32(245, 240, 225, 255));
	u32 ink = C2D_Color32(60, 50, 50, 255);
	for (int k = 1; k < 3; k++) {
		gfx_rect(x0 + k * cell - 1.5f, y0 + 4, 3, cell * 3 - 8, depth, ink);
		gfx_rect(x0 + 4, y0 + k * cell - 1.5f, cell * 3 - 8, 3, depth, ink);
	}
	for (int i = 0; i < 9; i++) {
		float cx = x0 + (i % 3) * cell + cell / 2, cy = y0 + (i / 3) * cell + cell / 2;
		bool win = false;
		if (s_game.winner == 1 || s_game.winner == 2)
			for (int k = 0; k < 3; k++)
				win |= s_game.line[k] == i;
		if (win)
			gfx_rrect(cx - cell / 2 + 4, cy - cell / 2 + 4, cell - 8, cell - 8, 6, depth,
				C2D_Color32(255, 220, 90, (u8)(150 + 100 * sinf(gfx_time() * 6.0f))));
		float scale = 1.0f;
		draw_mark(s_game.cell[i], cx, cy, cell, depth + 0.3f, scale);
		if (cursor && i == s_cursor && !s_game.winner && s_game.current == 1 && !s_game.cell[i])
			draw_mark(1, cx, cy, cell * 0.7f, depth, 0.8f + 0.1f * sinf(gfx_time() * 6.0f));
	}
}

static void tx_draw_top(void)
{
	gfx_background(BG_TABLE_TOP);
	draw_board((TOP_W - 3 * TOP_CELL) / 2.0f, 26.0f, TOP_CELL, DEPTH_CARDS, false);
	mii_draw(&profile_get()->avatar, 46, 80, 22, DEPTH_UI, mood_expr(&s_my_mood), false);
	draw_mark(1, 46, 124, 40, DEPTH_UI, 1.0f);
	gfx_textf(46, 146, DEPTH_UI, 0.45f, COLOR_CREAM, C2D_AlignCenter, "Tú: %d", s_wins[1]);
	mii_draw(cpu_avatar(CPU_MANOLO), 354, 80, 22, DEPTH_UI, mood_expr(&s_cpu_mood), false);
	draw_mark(2, 354, 124, 40, DEPTH_UI, 1.0f);
	gfx_textf(354, 146, DEPTH_UI, 0.45f, COLOR_CREAM, C2D_AlignCenter, "%s: %d", cpu_name(CPU_MANOLO), s_wins[2]);
	gfx_textf(200, 222, DEPTH_UI, 0.42f, C2D_Color32(200, 230, 200, 255), C2D_AlignCenter, "Empates: %d", s_wins[0]);
	if (s_game.winner && banner_draw(s_end_time, 200, 120, 280, 56)) {
		const char* t = s_game.winner == 1 ? "¡Tres en raya! Ganas" : s_game.winner == 2 ? "Gana la CPU" : "Empate";
		gfx_text(t, 200, 108, DEPTH_BANNER, 0.65f, s_game.winner == 1 ? COLOR_GOLD : COLOR_CREAM, C2D_AlignCenter);
	}
}

static void tx_draw_bottom(void)
{
	gfx_background(BG_TABLE_BOTTOM);
	button_draw(&s_btn_menu, false);
	button_draw(&s_btn_level, true);
	draw_board(BOT_X0, BOT_Y0, BOT_CELL, 0, true);
	if (!s_game.winner)
		gfx_text(s_game.current == 1 ? "Tu turno: toca una casilla" : "Piensa la CPU...", BOTTOM_W / 2, 210, 0, 0.45f,
			COLOR_CREAM, C2D_AlignCenter);
	else
		button_draw(&s_btn_again, s_end_time > 0.6f);
}

/* Salir a medias desde la pausa: la próxima vez se empieza de cero */
static void scene_leave(void)
{
	s_started = false;
}

const Scene scene_tresenraya = { tx_enter, tx_update, tx_draw_top, tx_draw_bottom, scene_leave };
