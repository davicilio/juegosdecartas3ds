/* Damas españolas: tú con las blancas contra la CPU. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "games/damas.h"
#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/gfx.h"
#include "ui/mii.h"
#include "ui/widgets.h"

#define BOT_CELL 26.0f
#define BOT_X0   ((BOTTOM_W - DA_SIZE * BOT_CELL) / 2.0f)
#define BOT_Y0   20.0f
#define TOP_CELL 27.0f

static Damas s_game;
static bool s_started;
static int s_cur_r = 5, s_cur_c = 0; /* cursor de la cruceta */
static int s_sel_r = -1, s_sel_c = -1; /* ficha elegida */
static DaMove s_moves[DA_MAX_MOVES];
static int s_move_count;
static float s_cpu_timer, s_end_time;
static int s_prev_winner;
static Mood s_cpu_mood, s_my_mood;
static Button s_btn_menu, s_btn_again, s_btn_level;
static int s_level = 1;
static const char* const LEVELS[] = { "Fácil", "Normal", "Difícil" };

static void refresh_moves(void)
{
	s_move_count = s_game.winner ? 0 : da_moves(&s_game, s_moves);
}

static void apply_level(void)
{
	static const int depth[3] = { 2, 4, 6 };
	s_game.level = depth[s_level];
}

static void da_enter(void)
{
	if (!s_started) {
		da_init(&s_game, (uint32_t)time(NULL) ^ 0xDA);
		apply_level();
		s_started = true;
	}
	refresh_moves();
	s_prev_winner = s_game.winner;
	button_set(&s_btn_menu, 258, 2, 54, 16, "Menú", C2D_Color32(120, 30, 35, 255), KEY_START);
}

static bool piece_can_move(int r, int c)
{
	for (int i = 0; i < s_move_count; i++)
		if (s_moves[i].fr == r && s_moves[i].fc == c)
			return true;
	return false;
}

/* Jugada de la ficha elegida que termina en (r, c), o -1 */
static int move_to(int r, int c)
{
	for (int i = 0; i < s_move_count; i++) {
		const DaMove* m = &s_moves[i];
		if (m->fr == s_sel_r && m->fc == s_sel_c && m->path[m->n_path - 1][0] == r && m->path[m->n_path - 1][1] == c)
			return i;
	}
	return -1;
}

static void click(int r, int c)
{
	if (s_sel_r >= 0) {
		int i = move_to(r, c);
		if (i >= 0) {
			da_apply(&s_game, &s_moves[i]);
			audio_play(s_moves[i].n_caps ? SFX_CHIP : SFX_CARD);
			if (s_moves[i].n_caps)
				mood_set(&s_cpu_mood, EXPR_SAD, 1.2f);
			s_sel_r = s_sel_c = -1;
			refresh_moves();
			return;
		}
	}
	if (da_owner(s_game.b[r][c]) == 1 && piece_can_move(r, c)) {
		s_sel_r = r;
		s_sel_c = c;
		audio_play(SFX_SELECT);
	} else if (da_owner(s_game.b[r][c]) == 1) {
		audio_play(SFX_ERROR); /* si hay que comer, solo valen las que comen */
	}
}

static SceneId da_update(const Input* in, float dt)
{
	if (button_update(&s_btn_menu, in))
		return SCENE_MENU;
	static char level_label[24];
	snprintf(level_label, sizeof level_label, GLYPH_Y " %s", LEVELS[s_level]);
	button_set(&s_btn_level, 8, 2, 84, 16, level_label, C2D_Color32(60, 90, 160, 255), KEY_Y);
	if (button_update(&s_btn_level, in)) {
		s_level = (s_level + 1) % 3;
		apply_level();
	}
	if (!s_game.winner) {
		if (s_game.current == 1) {
			if (in->repeat & KEY_LEFT)
				s_cur_c = (s_cur_c + 7) % 8;
			if (in->repeat & KEY_RIGHT)
				s_cur_c = (s_cur_c + 1) % 8;
			if (in->repeat & KEY_UP)
				s_cur_r = (s_cur_r + 7) % 8;
			if (in->repeat & KEY_DOWN)
				s_cur_r = (s_cur_r + 1) % 8;
			if (in->touch_down && in->tx >= BOT_X0 && in->tx < BOT_X0 + 8 * BOT_CELL && in->ty >= BOT_Y0 &&
				in->ty < BOT_Y0 + 8 * BOT_CELL) {
				s_cur_c = (int)((in->tx - BOT_X0) / BOT_CELL);
				s_cur_r = (int)((in->ty - BOT_Y0) / BOT_CELL);
				click(s_cur_r, s_cur_c);
			} else if (in->down & KEY_A) {
				click(s_cur_r, s_cur_c);
			} else if (in->down & KEY_B) {
				s_sel_r = s_sel_c = -1;
			}
		} else {
			s_cpu_timer += dt;
			if (s_cpu_timer > 0.5f) {
				s_cpu_timer = 0.0f;
				DaMove m;
				if (da_ai_choose(&s_game, &m)) {
					da_apply(&s_game, &m);
					audio_play(m.n_caps ? SFX_CHIP : SFX_CARD);
					if (m.n_caps)
						mood_set(&s_my_mood, EXPR_SAD, 1.2f);
				}
				refresh_moves();
			}
		}
	} else {
		button_set(&s_btn_again, 100, 2, 120, 16, GLYPH_A " Otra partida", C2D_Color32(40, 140, 60, 255), KEY_A);
		s_end_time += dt;
		if (s_end_time > 0.6f && button_update(&s_btn_again, in)) {
			da_new_game(&s_game);
			refresh_moves();
			s_end_time = 0.0f;
		}
	}
	if (s_game.winner != s_prev_winner) {
		s_prev_winner = s_game.winner;
		if (s_game.winner) {
			s_end_time = 0.0f;
			bool won = s_game.winner == 1;
			audio_play(won ? SFX_WIN : SFX_LOSE);
			mood_set(&s_my_mood, won ? EXPR_HAPPY : EXPR_SAD, 4.0f);
			mood_set(&s_cpu_mood, s_game.winner == 2 ? EXPR_HAPPY : EXPR_SAD, 4.0f);
			profile_record_game(GAME_DAMAS, won);
		}
	}
	mood_update(&s_cpu_mood, dt);
	mood_update(&s_my_mood, dt);
	return SCENE_DAMAS;
}

static void draw_piece(int piece, float cx, float cy, float cell, float depth)
{
	if (!piece)
		return;
	bool white = da_owner(piece) == 1;
	float r = cell * 0.4f;
	gfx_ellipse(cx + 1, cy + 2, r, r, depth, C2D_Color32(0, 0, 0, 90));
	gfx_ellipse(cx, cy, r, r, depth, white ? C2D_Color32(240, 232, 215, 255) : C2D_Color32(45, 35, 35, 255));
	gfx_ellipse(cx, cy, r * 0.7f, r * 0.7f, depth, white ? C2D_Color32(220, 208, 185, 255) : C2D_Color32(70, 55, 55, 255));
	if (piece == DA_WHITE_KING || piece == DA_BLACK_KING) {
		/* Corona */
		u32 gold = C2D_Color32(240, 190, 40, 255);
		gfx_triangle(cx - r * 0.5f, cy + r * 0.25f, cx - r * 0.3f, cy - r * 0.35f, cx - r * 0.1f,
			cy + r * 0.25f, depth, gold);
		gfx_triangle(cx - r * 0.2f, cy + r * 0.25f, cx, cy - r * 0.45f, cx + r * 0.2f, cy + r * 0.25f,
			depth, gold);
		gfx_triangle(cx + r * 0.1f, cy + r * 0.25f, cx + r * 0.3f, cy - r * 0.35f, cx + r * 0.5f,
			cy + r * 0.25f, depth, gold);
	}
}

static void draw_board(float x0, float y0, float cell, float depth, bool interactive)
{
	gfx_rrect(x0 - 4, y0 - 4, cell * 8 + 8, cell * 8 + 8, 4, depth, C2D_Color32(90, 55, 25, 255));
	for (int r = 0; r < 8; r++)
		for (int c = 0; c < 8; c++) {
			bool dark = (r + c) % 2 == 1;
			gfx_rect(x0 + c * cell, y0 + r * cell, cell, cell, depth,
				dark ? C2D_Color32(120, 80, 45, 255) : C2D_Color32(235, 215, 175, 255));
		}
	/* Última jugada y jugadas posibles */
	if (s_game.has_last) {
		const DaMove* m = &s_game.last;
		gfx_rect(x0 + m->fc * cell, y0 + m->fr * cell, cell, cell, depth, C2D_Color32(255, 230, 120, 70));
		int tr = m->path[m->n_path - 1][0], tc = m->path[m->n_path - 1][1];
		gfx_rect(x0 + tc * cell, y0 + tr * cell, cell, cell, depth, C2D_Color32(255, 230, 120, 90));
	}
	if (interactive && s_game.current == 1 && !s_game.winner) {
		for (int i = 0; i < s_move_count; i++) {
			const DaMove* m = &s_moves[i];
			if (s_sel_r < 0) {
				gfx_rect(x0 + m->fc * cell, y0 + m->fr * cell, cell, cell, depth, C2D_Color32(120, 220, 120, 70));
			} else if (m->fr == s_sel_r && m->fc == s_sel_c) {
				int tr = m->path[m->n_path - 1][0], tc = m->path[m->n_path - 1][1];
				gfx_ellipse(x0 + tc * cell + cell / 2, y0 + tr * cell + cell / 2, cell * 0.18f, cell * 0.18f, depth,
					C2D_Color32(120, 230, 120, 220));
			}
		}
		if (s_sel_r >= 0)
			gfx_rect(x0 + s_sel_c * cell, y0 + s_sel_r * cell, cell, cell, depth, C2D_Color32(120, 230, 120, 120));
	}
	for (int r = 0; r < 8; r++)
		for (int c = 0; c < 8; c++)
			draw_piece(s_game.b[r][c], x0 + c * cell + cell / 2, y0 + r * cell + cell / 2, cell, depth + 0.2f);
	if (interactive && s_game.current == 1 && !s_game.winner) {
		float pulse = 0.5f + 0.5f * sinf(gfx_time() * 6.0f);
		u32 col = C2D_Color32(255, 255, 255, (u8)(120 + 120 * pulse));
		float x = x0 + s_cur_c * cell, y = y0 + s_cur_r * cell;
		gfx_rect(x, y, cell, 2, 0, col);
		gfx_rect(x, y + cell - 2, cell, 2, 0, col);
		gfx_rect(x, y, 2, cell, 0, col);
		gfx_rect(x + cell - 2, y, 2, cell, 0, col);
	}
}

static void da_draw_top(void)
{
	gfx_background(BG_TABLE_TOP);
	draw_board((TOP_W - 8 * TOP_CELL) / 2.0f, 12, TOP_CELL, DEPTH_CARDS, false);
	mii_draw(cpu_avatar(CPU_MANOLO), 44, 50, 20, DEPTH_UI, mood_expr(&s_cpu_mood), false);
	gfx_textf(44, 76, DEPTH_UI, 0.42f, COLOR_CREAM, C2D_AlignCenter, "%s", cpu_name(CPU_MANOLO));
	gfx_textf(44, 92, DEPTH_UI, 0.42f, COLOR_GOLD, C2D_AlignCenter, "%d fichas", da_count(&s_game, 2));
	mii_draw(&profile_get()->avatar, 356, 170, 20, DEPTH_UI, mood_expr(&s_my_mood), false);
	gfx_text("Tú", 356, 196, DEPTH_UI, 0.42f, COLOR_CREAM, C2D_AlignCenter);
	gfx_textf(356, 212, DEPTH_UI, 0.42f, COLOR_GOLD, C2D_AlignCenter, "%d fichas", da_count(&s_game, 1));
	if (s_game.winner && banner_draw(s_end_time, 200, 120, 300, 56)) {
		const char* t = s_game.winner == 1 ? "¡Ganas la partida!" : s_game.winner == 2 ? "Gana la CPU" : "Tablas";
		gfx_text(t, 200, 108, DEPTH_BANNER, 0.65f, s_game.winner == 1 ? COLOR_GOLD : COLOR_CREAM, C2D_AlignCenter);
	}
}

static void da_draw_bottom(void)
{
	gfx_background(BG_TABLE_BOTTOM);
	draw_board(BOT_X0, BOT_Y0, BOT_CELL, 0, true);
	button_draw(&s_btn_menu, false);
	button_draw(&s_btn_level, true);
	if (s_game.winner)
		button_draw(&s_btn_again, s_end_time > 0.6f);
	else if (s_game.current == 2)
		gfx_text("Piensa...", 286, 120, 0, 0.42f, COLOR_CREAM, C2D_AlignCenter);
	else if (s_move_count > 0 && s_moves[0].n_caps > 0)
		gfx_text("¡Hay que comer!", 160, 2, 0, 0.42f, COLOR_GOLD, C2D_AlignCenter);
}

/* Salir a medias desde la pausa: la próxima vez se empieza de cero */
static void scene_leave(void)
{
	s_started = false;
}

const Scene scene_damas = { da_enter, da_update, da_draw_top, da_draw_bottom, scene_leave };
