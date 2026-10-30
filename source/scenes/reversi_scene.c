/* Reversi: tú (negras) contra la CPU (blancas). */
#include <math.h>
#include <stdio.h>
#include <time.h>

#include "games/reversi.h"
#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/gfx.h"
#include "ui/mii.h"
#include "ui/widgets.h"

#define TOP_CELL 26.0f
#define BOT_CELL 22.0f
#define BOT_X0   ((BOTTOM_W - RV_SIZE * BOT_CELL) / 2.0f)
#define BOT_Y0   26.0f

static Reversi s_game;
static bool s_started;
static int s_cur_r = 2, s_cur_c = 3;
static float s_cpu_timer, s_end_time, s_flip = 1.0f;
static Mood s_cpu_mood, s_my_mood;
static int s_prev_winner;
static int s_wins[3];
static Button s_btn_again, s_btn_menu, s_btn_level;
static const char* const LEVELS[] = { "Fácil", "Normal", "Difícil" };
static int s_level = 1;
static char s_note[64];

static void apply_level(void)
{
	static const int depth[3] = { 1, 2, 4 };
	s_game.level = depth[s_level];
}

static void rv_enter(void)
{
	if (!s_started) {
		rv_init(&s_game, (uint32_t)time(NULL) ^ 0x2E7E);
		apply_level();
		s_started = true;
	}
	s_prev_winner = s_game.winner;
	button_set(&s_btn_menu, 258, 2, 54, 20, "Menú", C2D_Color32(120, 30, 35, 255), KEY_START);
}

static void place(int r, int c)
{
	int who = s_game.current;
	if (!rv_play(&s_game, r, c)) {
		if (who == 1)
			audio_play(SFX_ERROR);
		return;
	}
	audio_play(SFX_CHIP);
	s_flip = 0.0f;
	s_note[0] = 0;
	if (s_game.last_passed)
		snprintf(s_note, sizeof s_note, "%s no puede poner: pasa", who == 1 ? "La CPU" : "Tú");
}

static SceneId rv_update(const Input* in, float dt)
{
	if (button_update(&s_btn_menu, in))
		return SCENE_MENU;
	static char level_label[24];
	snprintf(level_label, sizeof level_label, GLYPH_Y " %s", LEVELS[s_level]);
	button_set(&s_btn_level, 8, 2, 90, 20, level_label, C2D_Color32(60, 90, 160, 255), KEY_Y);
	if (button_update(&s_btn_level, in)) {
		s_level = (s_level + 1) % 3;
		apply_level();
	}
	s_flip = fminf(1.0f, s_flip + dt * 3.0f);
	if (!s_game.winner) {
		if (s_game.current == 1) {
			if (in->repeat & KEY_LEFT)
				s_cur_c = (s_cur_c + RV_SIZE - 1) % RV_SIZE;
			if (in->repeat & KEY_RIGHT)
				s_cur_c = (s_cur_c + 1) % RV_SIZE;
			if (in->repeat & KEY_UP)
				s_cur_r = (s_cur_r + RV_SIZE - 1) % RV_SIZE;
			if (in->repeat & KEY_DOWN)
				s_cur_r = (s_cur_r + 1) % RV_SIZE;
			if (in->touch_down && in->tx >= BOT_X0 && in->tx < BOT_X0 + RV_SIZE * BOT_CELL && in->ty >= BOT_Y0 &&
				in->ty < BOT_Y0 + RV_SIZE * BOT_CELL) {
				s_cur_c = (int)((in->tx - BOT_X0) / BOT_CELL);
				s_cur_r = (int)((in->ty - BOT_Y0) / BOT_CELL);
				place(s_cur_r, s_cur_c);
			} else if (in->down & KEY_A) {
				place(s_cur_r, s_cur_c);
			}
		} else if (s_flip >= 1.0f) {
			s_cpu_timer += dt;
			if (s_cpu_timer > 0.5f) {
				s_cpu_timer = 0.0f;
				int m = rv_ai_choose(&s_game);
				if (m >= 0)
					place(m / RV_SIZE, m % RV_SIZE);
			}
		}
	} else {
		button_set(&s_btn_again, 60, 210, 200, 26, GLYPH_A " Otra partida", C2D_Color32(40, 140, 60, 255), KEY_A);
		s_end_time += dt;
		if (s_end_time > 0.6f && button_update(&s_btn_again, in)) {
			rv_new_game(&s_game);
			s_end_time = 0.0f;
			s_note[0] = 0;
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
			profile_record_game(GAME_REVERSI, won);
		}
	}
	mood_update(&s_cpu_mood, dt);
	mood_update(&s_my_mood, dt);
	return SCENE_REVERSI;
}

static void draw_disc(float cx, float cy, float rad, int who, float width, float depth)
{
	u32 col = who == 1 ? C2D_Color32(30, 30, 35, 255) : C2D_Color32(240, 240, 235, 255);
	gfx_ellipse(cx + 1 + gfx_dx(depth), cy + 1.5f, rad * width, rad, depth, C2D_Color32(0, 0, 0, 90));
	gfx_ellipse(cx + gfx_dx(depth), cy, rad * width, rad, depth, col);
	gfx_ellipse(cx - rad * 0.25f * width + gfx_dx(depth), cy - rad * 0.3f, rad * 0.35f * width, rad * 0.25f, depth,
		C2D_Color32(255, 255, 255, who == 1 ? 50 : 120));
}

static void draw_board(float x0, float y0, float cell, float depth, bool hints)
{
	float w = RV_SIZE * cell;
	gfx_rrect(x0 - 5 + gfx_dx(depth), y0 - 5, w + 10, w + 10, 6, depth, C2D_Color32(90, 55, 25, 255));
	for (int r = 0; r < RV_SIZE; r++)
		for (int c = 0; c < RV_SIZE; c++) {
			float x = x0 + c * cell, y = y0 + r * cell;
			gfx_rect(x + 0.5f + gfx_dx(depth), y + 0.5f, cell - 1, cell - 1, depth,
				(r + c) % 2 ? C2D_Color32(30, 120, 60, 255) : C2D_Color32(36, 132, 66, 255));
			int who = s_game.cell[r][c];
			float cx = x + cell / 2, cy = y + cell / 2;
			if (who) {
				float width = 1.0f;
				int shown = who;
				if (s_game.flipped[r][c] && s_flip < 1.0f) {
					width = fabsf(cosf(s_flip * (float)M_PI));
					if (s_flip < 0.5f)
						shown = 3 - who;
				}
				draw_disc(cx, cy, cell * 0.4f, shown, fmaxf(width, 0.08f), depth + 0.2f);
				if (r == s_game.last_row && c == s_game.last_col)
					gfx_ellipse(cx + gfx_dx(depth + 0.3f), cy, cell * 0.08f, cell * 0.08f, depth + 0.3f,
						C2D_Color32(220, 60, 50, 255));
			} else if (hints && !s_game.winner && s_game.current == 1 && rv_flips(&s_game, 1, r, c)) {
				gfx_ellipse(cx, cy, cell * 0.13f, cell * 0.13f, depth, C2D_Color32(20, 20, 25, 140));
			}
		}
}

static void rv_draw_top(void)
{
	gfx_background(BG_TABLE_TOP);
	float x0 = (TOP_W - RV_SIZE * TOP_CELL) / 2.0f, y0 = 16.0f;
	draw_board(x0, y0, TOP_CELL, DEPTH_CARDS, false);
	mii_draw(&profile_get()->avatar, 42, 76, 20, DEPTH_UI, mood_expr(&s_my_mood), false);
	draw_disc(42, 112, 9, 1, 1.0f, DEPTH_UI);
	gfx_textf(42, 126, DEPTH_UI, 0.5f, COLOR_GOLD, C2D_AlignCenter, "%d", s_game.count[1]);
	gfx_textf(42, 146, DEPTH_UI, 0.4f, COLOR_CREAM, C2D_AlignCenter, "Ganadas: %d", s_wins[1]);
	mii_draw(cpu_avatar(CPU_LUCIA), 358, 76, 20, DEPTH_UI, mood_expr(&s_cpu_mood), false);
	draw_disc(358, 112, 9, 2, 1.0f, DEPTH_UI);
	gfx_textf(358, 126, DEPTH_UI, 0.5f, COLOR_GOLD, C2D_AlignCenter, "%d", s_game.count[2]);
	gfx_textf(358, 146, DEPTH_UI, 0.4f, COLOR_CREAM, C2D_AlignCenter, "Ganadas: %d", s_wins[2]);
	if (s_game.winner && banner_draw(s_end_time, 200, 120, 300, 60)) {
		const char* t = s_game.winner == 1 ? "¡Ganas!" : s_game.winner == 2 ? "Gana la CPU" : "Empate";
		gfx_textf(200, 104, DEPTH_BANNER, 0.62f, s_game.winner == 1 ? COLOR_GOLD : COLOR_CREAM, C2D_AlignCenter,
			"%s  %d - %d", t, s_game.count[1], s_game.count[2]);
	}
}

static void rv_draw_bottom(void)
{
	gfx_background(BG_TABLE_BOTTOM);
	button_draw(&s_btn_menu, false);
	button_draw(&s_btn_level, true);
	draw_board(BOT_X0, BOT_Y0, BOT_CELL, 0, true);
	if (!s_game.winner && s_game.current == 1) {
		float pulse = 0.6f + 0.4f * sinf(gfx_time() * 6.0f);
		float x = BOT_X0 + s_cur_c * BOT_CELL, y = BOT_Y0 + s_cur_r * BOT_CELL;
		u32 col = C2D_Color32(255, 220, 60, (u8)(255 * pulse));
		gfx_rect(x, y, BOT_CELL, 2, 0, col);
		gfx_rect(x, y + BOT_CELL - 2, BOT_CELL, 2, 0, col);
		gfx_rect(x, y, 2, BOT_CELL, 0, col);
		gfx_rect(x + BOT_CELL - 2, y, 2, BOT_CELL, 0, col);
	}
	if (s_game.winner)
		button_draw(&s_btn_again, s_end_time > 0.6f);
	else
		gfx_text(s_note[0] ? s_note : s_game.current == 1 ? "Tu turno: toca una casilla marcada" : "Piensa la CPU...",
			BOTTOM_W / 2, 214, 0, 0.45f, s_note[0] ? COLOR_GOLD : COLOR_CREAM, C2D_AlignCenter);
}

/* Salir a medias desde la pausa: la próxima vez se empieza de cero */
static void scene_leave(void)
{
	s_started = false;
}

const Scene scene_reversi = { rv_enter, rv_update, rv_draw_top, rv_draw_bottom, scene_leave };
