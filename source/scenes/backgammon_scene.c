/* Backgammon: el tablero táctil abajo y el grande arriba. Tus fichas son las
 * blancas y van hacia tu casa, abajo a la derecha. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "games/backgammon.h"
#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/gfx.h"
#include "ui/mii.h"
#include "ui/widgets.h"

#define RIVAL CPU_RAMON
#define BAR_W  16.0f
#define TRAY_W 22.0f

typedef struct {
	float x, y, w, h;
} Area;

static const Area TOP_AREA = { 8, 6, 384, 228 };
static const Area BOTTOM_AREA = { 2, 24, 316, 214 };

static Backgammon s_game;
static bool s_started, s_recorded;
static int s_from = -1;
static BgStep s_steps[64];
static int s_steps_n;
static bool s_dirty = true;
static float s_cpu_timer, s_end_time;
static char s_message[96];
static Mood s_mood;
static Button s_btn_roll, s_btn_menu;

static void refresh(void)
{
	s_steps_n = bg_legal_steps(&s_game, s_steps, 64);
	s_dirty = false;
}

static float point_w(const Area* a)
{
	return (a->w - BAR_W - TRAY_W) / 12.0f;
}

/* Columna en pantalla de la punta i (0..11 de izquierda a derecha) y si está abajo */
static void point_col(int i, int* col, bool* bottom)
{
	*bottom = i < 12;
	*col = *bottom ? 11 - i : i - 12;
}

static float col_x(const Area* a, int col)
{
	return a->x + col * point_w(a) + (col >= 6 ? BAR_W : 0);
}

static void new_game(void)
{
	bg_init(&s_game, (uint32_t)time(NULL) ^ 0xBACC);
	s_from = -1;
	s_dirty = true;
	s_recorded = false;
	s_cpu_timer = s_end_time = 0.0f;
	snprintf(s_message, sizeof s_message, s_game.current == 0 ? "Empiezas tú: tira los dados" : "Empieza %s",
		cpu_name(RIVAL));
}

static void bk_enter(void)
{
	if (!s_started) {
		s_started = true;
		new_game();
	}
	button_set(&s_btn_menu, 262, 2, 54, 18, "Menú", C2D_Color32(120, 30, 35, 255), KEY_START);
}

static bool is_source(int from)
{
	for (int i = 0; i < s_steps_n; i++)
		if (s_steps[i].from == from)
			return true;
	return false;
}

static bool is_target(int from, int to)
{
	for (int i = 0; i < s_steps_n; i++)
		if (s_steps[i].from == from && s_steps[i].to == to)
			return true;
	return false;
}

/* Qué hay bajo el toque: punta, barra o bandeja de fichas sacadas */
static int touched(const Area* a, int tx, int ty)
{
	float pw = point_w(a);
	if (tx >= a->x + a->w - TRAY_W)
		return BG_OFF;
	float bx = a->x + 6 * pw;
	if (tx >= bx && tx < bx + BAR_W)
		return BG_BAR;
	float rx = tx - a->x;
	int col = (int)(rx >= 6 * pw + BAR_W ? (rx - BAR_W) / pw : rx / pw);
	if (col < 0 || col > 11)
		return -1;
	bool bottom = ty > a->y + a->h / 2;
	return bottom ? 11 - col : 12 + col;
}

static void dice_text(char* buf, int size)
{
	if (s_game.dice[0] == s_game.dice[1])
		snprintf(buf, (size_t)size, "dobles de %d", s_game.dice[0]);
	else
		snprintf(buf, (size_t)size, "%d y %d", s_game.dice[0], s_game.dice[1]);
}

static SceneId bk_update(const Input* in, float dt)
{
	if (button_update(&s_btn_menu, in))
		return SCENE_MENU;
	mood_update(&s_mood, dt);
	Backgammon* g = &s_game;
	if (g->phase == BG_OVER) {
		s_end_time += dt;
		if (!s_recorded) {
			s_recorded = true;
			bool won = g->winner == 0;
			profile_record_game(GAME_BACKGAMMON, won);
			audio_play(won ? SFX_WIN : SFX_LOSE);
			mood_set(&s_mood, won ? EXPR_SAD : EXPR_HAPPY, 30.0f);
		}
		if (s_end_time > 1.2f && ((in->down & KEY_A) || in->touch_down))
			new_game();
		return SCENE_BACKGAMMON;
	}
	if (s_dirty)
		refresh();
	if (g->current == 0) {
		button_set(&s_btn_roll, 110, 115, 100, 30, GLYPH_A " Tirar", C2D_Color32(200, 140, 30, 255), KEY_A);
		s_btn_roll.enabled = g->phase == BG_ROLL;
		if (g->phase == BG_ROLL) {
			if (button_update(&s_btn_roll, in)) {
				bg_roll(g, 0, 0);
				s_dirty = true;
				s_from = -1;
				audio_play(SFX_FLIP);
				char d[24];
				dice_text(d, sizeof d);
				if (g->no_move)
					snprintf(s_message, sizeof s_message, "Sacas %s y no puedes mover", d);
				else
					snprintf(s_message, sizeof s_message, "Sacas %s: toca una ficha y su destino", d);
			}
			return SCENE_BACKGAMMON;
		}
		/* Con fichas en la barra, se empieza por ellas */
		if (g->b.bar[0] && s_from != BG_BAR)
			s_from = BG_BAR;
		if (in->touch_down) {
			int t = touched(&BOTTOM_AREA, in->tx, in->ty);
			if (t >= 0) {
				if (s_from >= 0 && is_target(s_from, t)) {
					bool hit = t < BG_POINTS && g->b.pt[t] == -1;
					bg_step(g, s_from, t);
					s_dirty = true;
					s_from = -1;
					audio_play(hit ? SFX_CHIP : SFX_CARD);
					if (hit)
						mood_set(&s_mood, EXPR_SAD, 1.5f);
					if (g->current != 0 && g->phase == BG_ROLL)
						snprintf(s_message, sizeof s_message, "Juega %s", cpu_name(RIVAL));
				} else if (is_source(t)) {
					s_from = t == s_from ? -1 : t;
					audio_play(SFX_SELECT);
				} else {
					audio_play(SFX_ERROR);
				}
			}
		}
		if (in->down & KEY_B)
			s_from = -1;
	} else {
		s_cpu_timer += dt;
		if (s_cpu_timer > 0.8f) {
			s_cpu_timer = 0.0f;
			if (g->phase == BG_ROLL) {
				bg_roll(g, 0, 0);
				char d[24];
				dice_text(d, sizeof d);
				snprintf(s_message, sizeof s_message, "%s saca %s%s", cpu_name(RIVAL), d, g->no_move ? " y no puede mover" : "");
				audio_play(SFX_FLIP);
			} else {
				int bar_before = g->b.bar[0];
				bg_ai_play(g);
				if (g->b.bar[0] > bar_before) {
					snprintf(s_message, sizeof s_message, "¡%s te come una ficha!", cpu_name(RIVAL));
					mood_set(&s_mood, EXPR_HAPPY, 1.5f);
					audio_play(SFX_CHIP);
				} else {
					audio_play(SFX_CARD);
				}
				if (g->phase == BG_ROLL && g->current == 0)
					snprintf(s_message + strlen(s_message), sizeof s_message - strlen(s_message), "%s",
						strlen(s_message) ? ". Te toca" : "Te toca");
			}
			s_dirty = true;
		}
	}
	return SCENE_BACKGAMMON;
}

static void draw_checker(float cx, float cy, float r, int p, float depth)
{
	gfx_ellipse(cx + 1, cy + 1.5f, r, r, depth, C2D_Color32(0, 0, 0, 90));
	if (p == 0) {
		gfx_ellipse(cx, cy, r, r, depth, C2D_Color32(150, 140, 120, 255));
		gfx_ellipse(cx, cy, r - 1, r - 1, depth, C2D_Color32(245, 238, 220, 255));
		gfx_ellipse(cx, cy, r * 0.6f, r * 0.6f, depth, C2D_Color32(225, 215, 190, 255));
	} else {
		gfx_ellipse(cx, cy, r, r, depth, C2D_Color32(20, 15, 15, 255));
		gfx_ellipse(cx, cy, r - 1, r - 1, depth, C2D_Color32(150, 30, 35, 255));
		gfx_ellipse(cx, cy, r * 0.6f, r * 0.6f, depth, C2D_Color32(120, 20, 25, 255));
	}
	gfx_ellipse(cx - r * 0.3f, cy - r * 0.35f, r * 0.3f, r * 0.2f, depth, C2D_Color32(255, 255, 255, 80));
}

static void draw_stack(float cx, float base, float r, int n, int p, bool down, float depth)
{
	int shown = n > 5 ? 5 : n;
	for (int k = 0; k < shown; k++)
		draw_checker(cx, base + (down ? 1 : -1) * (r + k * 2 * r), r, p, depth);
	if (n > 5)
		gfx_textf_plain(cx, base + (down ? 1 : -1) * (r + 4 * 2 * r) - 8, depth, 0.45f,
			p == 0 ? C2D_Color32(60, 40, 30, 255) : COLOR_WHITE, C2D_AlignCenter, "%d", n);
}

static void draw_board(const Area* a, float depth, bool interactive)
{
	const Backgammon* g = &s_game;
	float pw = point_w(a), r = pw / 2 - 1;
	if (r > 11)
		r = 11;
	/* Marco y fieltro */
	gfx_rect(a->x - 4, a->y - 4, a->w + 8, a->h + 8, depth, C2D_Color32(90, 55, 25, 255));
	gfx_rect(a->x, a->y, a->w - TRAY_W, a->h, depth, C2D_Color32(30, 100, 60, 255));
	gfx_rect(a->x + 6 * pw, a->y, BAR_W, a->h, depth, C2D_Color32(90, 55, 25, 255));
	gfx_rect(a->x + a->w - TRAY_W, a->y, TRAY_W, a->h, depth, C2D_Color32(70, 40, 20, 255));
	float pulse = 0.5f + 0.5f * sinf(gfx_time() * 6.0f);
	for (int i = 0; i < BG_POINTS; i++) {
		int col;
		bool bottom;
		point_col(i, &col, &bottom);
		float x = col_x(a, col), tip = a->h * 0.42f;
		u32 c = (i % 2) ? C2D_Color32(230, 210, 170, 255) : C2D_Color32(160, 50, 45, 255);
		bool hl = interactive && ((s_from >= 0 && is_target(s_from, i)) || (s_from < 0 && is_source(i)) || s_from == i);
		if (bottom)
			gfx_triangle(x, a->y + a->h, x + pw, a->y + a->h, x + pw / 2, a->y + a->h - tip, depth, c);
		else
			gfx_triangle(x, a->y, x + pw, a->y, x + pw / 2, a->y + tip, depth, c);
		if (hl) {
			u32 k = s_from == i ? COLOR_GOLD : C2D_Color32(255, 230, 80, (u8)(110 + 120 * pulse));
			gfx_rect(x + 2, bottom ? a->y + a->h - 4 : a->y + 1, pw - 4, 3, depth, k);
		}
		int n = g->b.pt[i];
		if (n)
			draw_stack(x + pw / 2, bottom ? a->y + a->h : a->y, r, n > 0 ? n : -n, n > 0 ? 0 : 1, !bottom, depth);
	}
	/* Barra */
	float bx = a->x + 6 * pw + BAR_W / 2;
	if (g->b.bar[0])
		draw_stack(bx, a->y + a->h / 2 + 4, r * 0.8f, g->b.bar[0], 0, true, depth);
	if (g->b.bar[1])
		draw_stack(bx, a->y + a->h / 2 - 4, r * 0.8f, g->b.bar[1], 1, false, depth);
	if (interactive && s_from == BG_BAR)
		gfx_rect(a->x + 6 * pw, a->y + a->h / 2 - 2, BAR_W, 4, depth, COLOR_GOLD);
	/* Fichas sacadas, como canto de ficha */
	float tx = a->x + a->w - TRAY_W + 3;
	for (int k = 0; k < g->b.off[0]; k++)
		gfx_rect(tx, a->y + a->h - 5 - k * 5, TRAY_W - 6, 4, depth, C2D_Color32(245, 238, 220, 255));
	for (int k = 0; k < g->b.off[1]; k++)
		gfx_rect(tx, a->y + 2 + k * 5, TRAY_W - 6, 4, depth, C2D_Color32(150, 30, 35, 255));
	if (interactive && s_from >= 0 && is_target(s_from, BG_OFF))
		gfx_rect(a->x + a->w - TRAY_W, a->y + a->h / 2, TRAY_W, a->h / 2, depth,
			C2D_Color32(255, 230, 80, (u8)(60 + 80 * pulse)));
	/* Dados en la mitad derecha */
	if (g->phase == BG_MOVE || (g->phase == BG_ROLL && g->dice[0])) {
		float dx = a->x + 6 * pw + BAR_W + 3 * pw, dy = a->y + a->h / 2;
		for (int k = 0; k < 2; k++) {
			bool used = g->phase == BG_ROLL;
			gfx_piece(PIECE_DIE_FIRST + g->dice[k] - 1, dx - 16 + k * 32, dy, 0.85f, 0.85f, depth);
			if (used)
				gfx_ellipse(dx - 16 + k * 32, dy, 13, 13, depth, C2D_Color32(0, 0, 0, 80));
		}
	}
}

static void bk_draw_top(void)
{
	const Backgammon* g = &s_game;
	gfx_background(BG_TABLE_TOP);
	draw_board(&TOP_AREA, DEPTH_TABLE, false);
	/* Rival y cuenta de puntos (pips) */
	float pw = point_w(&TOP_AREA);
	float px = TOP_AREA.x + 3 * pw;
	gfx_panel(px - 52, 100, 104, 40, DEPTH_UI, C2D_Color32(20, 30, 25, 220),
		g->current == 1 && g->phase != BG_OVER ? COLOR_GOLD : C2D_Color32(150, 110, 40, 255));
	mii_draw(cpu_avatar(RIVAL), px - 34, 120, 11, DEPTH_UI, mood_expr(&s_mood), false);
	gfx_textf(px + 14, 103, DEPTH_UI, 0.42f, COLOR_CREAM, C2D_AlignCenter, "Tú %d", bg_pips(&g->b, 0));
	gfx_textf(px + 14, 119, DEPTH_UI, 0.42f, C2D_Color32(255, 170, 160, 255), C2D_AlignCenter, "Él %d",
		bg_pips(&g->b, 1));
	if (g->phase == BG_OVER && banner_draw(s_end_time, 200, 120, 300, 64)) {
		static const char* const KIND[4] = { "", "", " (gammon: doble)", " (backgammon: triple)" };
		if (g->winner == 0)
			gfx_textf(200, 100, DEPTH_BANNER, 0.62f, COLOR_GOLD, C2D_AlignCenter, "¡Has ganado!%s", KIND[g->points]);
		else
			gfx_textf(200, 100, DEPTH_BANNER, 0.55f, COLOR_CREAM, C2D_AlignCenter, "Gana %s%s", cpu_name(RIVAL),
				KIND[g->points]);
		gfx_text("Toca para jugar otra", 200, 126, DEPTH_BANNER, 0.42f, COLOR_CREAM, C2D_AlignCenter);
	}
}

static void bk_draw_bottom(void)
{
	const Backgammon* g = &s_game;
	gfx_background(BG_TABLE_BOTTOM);
	gfx_text(s_message, 4, 3, 0, 0.42f, COLOR_GOLD, C2D_AlignLeft);
	button_draw(&s_btn_menu, false);
	draw_board(&BOTTOM_AREA, 0, g->current == 0 && g->phase == BG_MOVE);
	if (g->current == 0 && g->phase == BG_ROLL)
		button_draw(&s_btn_roll, true);
}

/* Salir a medias desde la pausa: la próxima vez se empieza de cero */
static void scene_leave(void)
{
	s_started = false;
}

const Scene scene_backgammon = { bk_enter, bk_update, bk_draw_top, bk_draw_bottom, scene_leave };
