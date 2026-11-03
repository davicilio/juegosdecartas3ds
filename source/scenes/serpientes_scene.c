/* Serpientes y escaleras: el tablero de 100 casillas arriba y el dado abajo. */
#include <math.h>
#include <stdio.h>
#include <time.h>

#include "games/serpientes.h"
#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/gfx.h"
#include "ui/mii.h"
#include "ui/setup.h"
#include "ui/widgets.h"

#define CELL 23.0f
#define BX   ((TOP_W - 10 * CELL) / 2.0f)
#define BY   5.0f

static const CpuId CPUS[SE_PLAYERS] = { CPU_COUNT, CPU_PEPE, CPU_LOLA, CPU_PACO };
static const u32 TOKEN[SE_PLAYERS] = { 0xDC282D, 0x2864C8, 0x3CA846, 0xF0B41E };
static const char* const PLAYERS[] = { "2", "3", "4" };

typedef enum { ANIM_NONE, ANIM_ROLL, ANIM_WALK, ANIM_SLIDE } Anim;

static Serpientes s_game;
static bool s_started, s_in_setup, s_recorded;
static int s_players_idx = 2;
static Setup s_setup;
static Anim s_anim;
static float s_anim_t, s_cpu_timer, s_end_time;
static int s_shown[SE_PLAYERS], s_mover, s_steps, s_face = 1;
static float s_disp_x[SE_PLAYERS], s_disp_y[SE_PLAYERS];
static char s_message[80];
static Mood s_mood[SE_PLAYERS];
static Button s_btn_roll, s_btn_menu;

static const char* name(int p)
{
	return p == 0 ? profile_get()->avatar.name : cpu_name(CPUS[p]);
}

static const Avatar* avatar(int p)
{
	return p == 0 ? &profile_get()->avatar : cpu_avatar(CPUS[p]);
}

/* Centro de la casilla n (1..100; 0 = fuera, junto a la 1) */
static void square_xy(int n, float* x, float* y)
{
	if (n < 1) {
		*x = BX - CELL / 2;
		*y = BY + 9.5f * CELL;
		return;
	}
	int i = n - 1, row = i / 10, col = i % 10;
	if (row % 2)
		col = 9 - col;
	*x = BX + col * CELL + CELL / 2;
	*y = BY + (9 - row) * CELL + CELL / 2;
}

static void token_xy(int p, int n, float* x, float* y)
{
	static const float off[SE_PLAYERS][2] = { { -5, -5 }, { 5, -5 }, { -5, 5 }, { 5, 5 } };
	square_xy(n, x, y);
	*x += off[p][0];
	*y += off[p][1];
}

static void se_enter(void)
{
	if (!s_started) {
		s_started = true;
		s_in_setup = true;
		setup_init(&s_setup, "Serpientes y escaleras", "Tira el dado y avanza: las escaleras te suben y las serpientes te bajan. Gana quien llegue justo a la casilla 100. Con un 6, repites.");
		setup_add(&s_setup, "Jugadores", PLAYERS, 3, &s_players_idx);
	}
	button_set(&s_btn_menu, 262, 2, 54, 18, "Menú", C2D_Color32(120, 30, 35, 255), KEY_START);
}

static void begin(void)
{
	se_init(&s_game, s_players_idx + 2, (uint32_t)time(NULL) ^ 0x5E5E);
	for (int p = 0; p < SE_PLAYERS; p++) {
		s_shown[p] = 0;
		token_xy(p, 0, &s_disp_x[p], &s_disp_y[p]);
	}
	s_anim = ANIM_NONE;
	s_recorded = false;
	s_end_time = s_cpu_timer = 0.0f;
	snprintf(s_message, sizeof s_message, "Empiezas tú: tira el dado");
}

static void start_roll(void)
{
	s_mover = s_game.current;
	s_anim = ANIM_ROLL;
	s_anim_t = 0.0f;
	audio_play(SFX_FLIP);
}

static SceneId se_update(const Input* in, float dt)
{
	if (button_update(&s_btn_menu, in))
		return SCENE_MENU;
	for (int p = 0; p < SE_PLAYERS; p++)
		mood_update(&s_mood[p], dt);
	if (s_in_setup) {
		if (setup_update(&s_setup, in)) {
			s_in_setup = false;
			begin();
		}
		return SCENE_SERPIENTES;
	}
	Serpientes* g = &s_game;
	bool over = g->winner >= 0 && s_anim == ANIM_NONE;
	button_set(&s_btn_roll, 164, 205, 144, 28, over ? GLYPH_A " Otra partida" : GLYPH_A " Tirar el dado",
		C2D_Color32(40, 140, 60, 255), KEY_A);
	s_btn_roll.enabled = s_anim == ANIM_NONE && (over || g->current == 0);
	switch (s_anim) {
	case ANIM_NONE:
		if (over) {
			s_end_time += dt;
			if (!s_recorded) {
				s_recorded = true;
				profile_record_game(GAME_SERPIENTES, g->winner == 0);
				audio_play(g->winner == 0 ? SFX_WIN : SFX_LOSE);
				for (int p = 0; p < g->players; p++)
					mood_set(&s_mood[p], p == g->winner ? EXPR_HAPPY : EXPR_SAD, 5.0f);
			}
			if (s_end_time > 1.0f && button_update(&s_btn_roll, in))
				begin();
		} else if (g->current == 0) {
			if (button_update(&s_btn_roll, in))
				start_roll();
		} else {
			s_cpu_timer += dt;
			if (s_cpu_timer > 0.7f) {
				s_cpu_timer = 0.0f;
				start_roll();
			}
		}
		break;
	case ANIM_ROLL:
		s_anim_t += dt;
		s_face = 1 + (int)(gfx_time() * 20.0f) % 6;
		if (s_anim_t > 0.45f) {
			int from = g->pos[s_mover];
			se_roll(g, 0);
			s_face = g->last_roll;
			s_steps = g->last_roll;
			s_shown[s_mover] = from;
			s_anim = ANIM_WALK;
			s_anim_t = 0.0f;
		}
		break;
	case ANIM_WALK:
		s_anim_t += dt;
		if (s_anim_t > 0.15f) {
			s_anim_t = 0.0f;
			/* Se avanza y, si sobra, se cuenta hacia atrás desde la 100 */
			int n = s_shown[s_mover];
			bool back = n == SE_GOAL || (n > g->last_land && s_steps <= SE_GOAL - g->last_land);
			s_shown[s_mover] = back ? n - 1 : n + 1;
			if (--s_steps <= 0) {
				s_shown[s_mover] = g->last_land;
				int p = s_mover;
				if (g->last_to > g->last_land) {
					snprintf(s_message, sizeof s_message, "¡Escalera! %s sube a la %d", p == 0 ? "Subes" : name(p), g->last_to);
					mood_set(&s_mood[p], EXPR_HAPPY, 1.5f);
					audio_play(SFX_CHIP);
					s_anim = ANIM_SLIDE;
				} else if (g->last_to < g->last_land) {
					snprintf(s_message, sizeof s_message, "¡Serpiente! %s baja a la %d", p == 0 ? "Bajas" : name(p), g->last_to);
					mood_set(&s_mood[p], EXPR_SAD, 1.5f);
					audio_play(SFX_LOSE);
					s_anim = ANIM_SLIDE;
				} else {
					snprintf(s_message, sizeof s_message, "%s %d: casilla %d%s", p == 0 ? "Sacas un" : name(p),
						g->last_roll, g->last_land, g->last_roll == 6 && g->winner < 0 ? ". ¡Repite!" : "");
					s_anim = ANIM_NONE;
				}
				s_anim_t = -0.3f;
			}
			audio_play(SFX_SELECT);
		}
		break;
	case ANIM_SLIDE:
		s_anim_t += dt;
		if (s_anim_t > 0.0f) {
			s_shown[s_mover] = g->last_to;
			s_anim = ANIM_NONE;
		}
		break;
	}
	for (int p = 0; p < g->players; p++) {
		float tx, ty;
		token_xy(p, s_shown[p], &tx, &ty);
		float k = fminf(1.0f, dt * (s_anim == ANIM_WALK ? 14.0f : 5.0f));
		s_disp_x[p] += (tx - s_disp_x[p]) * k;
		s_disp_y[p] += (ty - s_disp_y[p]) * k;
	}
	return SCENE_SERPIENTES;
}

static void draw_ladder(int from, int to)
{
	float x0, y0, x1, y1;
	square_xy(from, &x0, &y0);
	square_xy(to, &x1, &y1);
	float dx = x1 - x0, dy = y1 - y0, len = sqrtf(dx * dx + dy * dy);
	float nx = -dy / len * 4.5f, ny = dx / len * 4.5f;
	u32 wood = C2D_Color32(150, 95, 40, 255);
	gfx_line(x0 + nx, y0 + ny, x1 + nx, y1 + ny, 2.2f, DEPTH_CARDS, wood);
	gfx_line(x0 - nx, y0 - ny, x1 - nx, y1 - ny, 2.2f, DEPTH_CARDS, wood);
	int rungs = (int)(len / 9);
	for (int k = 1; k < rungs; k++) {
		float t = (float)k / rungs, x = x0 + dx * t, y = y0 + dy * t;
		gfx_line(x + nx, y + ny, x - nx, y - ny, 1.6f, DEPTH_CARDS, C2D_Color32(190, 130, 60, 255));
	}
}

static void draw_snake(int from, int to, int k)
{
	float x0, y0, x1, y1;
	square_xy(from, &x0, &y0);
	square_xy(to, &x1, &y1);
	float dx = x1 - x0, dy = y1 - y0, len = sqrtf(dx * dx + dy * dy);
	float nx = -dy / len, ny = dx / len;
	static const u32 COL[3] = { 0x3CA046, 0x8C3CB4, 0xD27828 };
	u32 c = gfx_rgb(COL[k % 3], 255);
	const int SEG = 16;
	float px = x0, py = y0;
	for (int s = 1; s <= SEG; s++) {
		float t = (float)s / SEG, w = sinf(t * 3.14159f * 3) * 6 * (1 - t * 0.5f);
		float x = x0 + dx * t + nx * w, y = y0 + dy * t + ny * w;
		gfx_line(px, py, x, y, 4.0f - 2.5f * t, DEPTH_CARDS, c);
		px = x;
		py = y;
	}
	gfx_ellipse(x0, y0, 4.5f, 4.5f, DEPTH_CARDS, c);
	gfx_ellipse(x0 - 1.5f, y0 - 1.5f, 1, 1, DEPTH_CARDS, COLOR_WHITE);
	gfx_ellipse(x0 + 1.5f, y0 - 1.5f, 1, 1, DEPTH_CARDS, COLOR_WHITE);
}

static void se_draw_top(void)
{
	if (s_in_setup) {
		setup_draw_top(&s_setup);
		return;
	}
	const Serpientes* g = &s_game;
	gfx_background(BG_TABLE_TOP);
	gfx_rect(BX - 3, BY - 3, 10 * CELL + 6, 10 * CELL + 6, DEPTH_TABLE, C2D_Color32(110, 70, 30, 255));
	static const u32 TILE[4] = { 0xFAF0D7, 0xF5D7A0, 0xD7EBC8, 0xC8DCF0 };
	for (int n = 1; n <= SE_GOAL; n++) {
		float x, y;
		square_xy(n, &x, &y);
		gfx_rect(x - CELL / 2, y - CELL / 2, CELL, CELL, DEPTH_TABLE, gfx_rgb(TILE[(n + (n - 1) / 10) % 4], 255));
		gfx_textf_plain(x - CELL / 2 + 2, y - CELL / 2, DEPTH_TABLE, 0.45f, C2D_Color32(120, 90, 70, 255), C2D_AlignLeft, "%d", n);
	}
	for (int i = 0; i < SE_LINKS; i++)
		if (SE_BOARD[i].to > SE_BOARD[i].from)
			draw_ladder(SE_BOARD[i].from, SE_BOARD[i].to);
	for (int i = 0, k = 0; i < SE_LINKS; i++)
		if (SE_BOARD[i].to < SE_BOARD[i].from)
			draw_snake(SE_BOARD[i].from, SE_BOARD[i].to, k++);
	for (int p = 0; p < g->players; p++) {
		float x = s_disp_x[p], y = s_disp_y[p];
		gfx_ellipse(x + 1, y + 1.5f, 5.5f, 5.5f, DEPTH_BANNER, C2D_Color32(0, 0, 0, 100));
		gfx_ellipse(x, y, 5.5f, 5.5f, DEPTH_BANNER, gfx_rgb(TOKEN[p], 255));
		gfx_ellipse(x - 1.5f, y - 1.5f, 2, 1.5f, DEPTH_BANNER, C2D_Color32(255, 255, 255, 150));
	}
	if (g->winner >= 0 && s_anim == ANIM_NONE && banner_draw(s_end_time, 200, 120, 300, 60))
		gfx_textf(200, 104, DEPTH_BANNER, 0.62f, COLOR_GOLD, C2D_AlignCenter, g->winner == 0 ? "¡Llegas a la 100!" : "Gana %s",
			name(g->winner));
}

static void se_draw_bottom(void)
{
	if (s_in_setup) {
		setup_draw_bottom(&s_setup);
		button_draw(&s_btn_menu, false);
		return;
	}
	const Serpientes* g = &s_game;
	gfx_background(BG_TABLE_BOTTOM);
	gfx_text(s_message, 6, 4, 0, 0.42f, COLOR_GOLD, C2D_AlignLeft);
	button_draw(&s_btn_menu, false);
	for (int p = 0; p < g->players; p++) {
		float y = 44 + p * 38;
		bool turn = g->current == p && g->winner < 0;
		gfx_panel(8, y - 16, 168, 34, 0, C2D_Color32(10, 50, 25, 200), turn ? COLOR_GOLD : C2D_Color32(20, 60, 30, 255));
		mii_draw(avatar(p), 26, y + 1, 12, 0, mood_expr(&s_mood[p]), false);
		gfx_ellipse(48, y + 1, 6, 6, 0, gfx_rgb(TOKEN[p], 255));
		gfx_text(name(p), 60, y - 12, 0, 0.45f, COLOR_CREAM, C2D_AlignLeft);
		gfx_textf(60, y + 2, 0, 0.42f, C2D_Color32(200, 230, 200, 255), C2D_AlignLeft, "Casilla %d", s_shown[p]);
	}
	gfx_piece(PIECE_DIE_FIRST + s_face - 1, 250, 100, 1.85f, 1.85f, 0);
	if (g->winner < 0 && s_anim == ANIM_NONE && g->current == 0)
		gfx_text("¡Te toca!", 250, 140, 0, 0.5f, COLOR_GOLD, C2D_AlignCenter);
	button_draw(&s_btn_roll, s_btn_roll.enabled);
}

/* Salir a medias desde la pausa: la próxima vez se empieza de cero */
static void scene_leave(void)
{
	s_started = false;
}

const Scene scene_serpientes = { se_enter, se_update, se_draw_top, se_draw_bottom, scene_leave };
