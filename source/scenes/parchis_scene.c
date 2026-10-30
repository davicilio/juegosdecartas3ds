/* Parchís: el tablero arriba y, abajo, el dado y tus cuatro fichas. */
#include <math.h>
#include <stdio.h>
#include <time.h>

#include "games/parchis.h"
#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/gfx.h"
#include "ui/mii.h"
#include "ui/widgets.h"

#define U   12.0f /* lado de una casilla */
#define BX0 ((TOP_W - 19 * U) / 2.0f)
#define BY0 6.0f

static const CpuId CPUS[PA_PLAYERS] = { CPU_COUNT, CPU_PACO, CPU_LOLA, CPU_PEPE };
static const u32 RGB[PA_PLAYERS] = { 0xE8C428, 0x2864D2, 0xD23232, 0x32A046 };
/* Centro de cada casa (en casillas) y dónde se sienta cada jugador */
static const float HOME_X[PA_PLAYERS] = { 15, 15, 4, 4 }, HOME_Y[PA_PLAYERS] = { 15, 4, 4, 15 };
static const float SEAT_X[PA_PLAYERS] = { 356, 356, 44, 44 }, SEAT_Y[PA_PLAYERS] = { 180, 50, 50, 180 };

static Parchis s_game;
static bool s_started;
static int s_sel;
static float s_cpu_timer, s_end_time, s_roll_anim;
static int s_face = 1;
static Mood s_mood[PA_PLAYERS];
static char s_message[96];
static bool s_recorded;
static Button s_btn_roll, s_btn_piece[PA_PIECES], s_btn_menu;

static const char* name(int p)
{
	return p == 0 ? profile_get()->avatar.name : cpu_name(CPUS[p]);
}

static const Avatar* avatar(int p)
{
	return p == 0 ? &profile_get()->avatar : cpu_avatar(CPUS[p]);
}

/* Casilla (columna, fila) de la posición i del circuito, en sentido contrario
 * a las agujas del reloj empezando por la columna izquierda del brazo de abajo */
static void ring_xy(int i, float* x, float* y)
{
	int c, r;
	if (i < 8) { c = 8; r = 11 + i; }
	else if (i == 8) { c = 9; r = 18; }
	else if (i < 17) { c = 10; r = 18 - (i - 9); }
	else if (i < 25) { c = 11 + (i - 17); r = 10; }
	else if (i == 25) { c = 18; r = 9; }
	else if (i < 34) { c = 18 - (i - 26); r = 8; }
	else if (i < 42) { c = 10; r = 7 - (i - 34); }
	else if (i == 42) { c = 9; r = 0; }
	else if (i < 51) { c = 8; r = i - 43; }
	else if (i < 59) { c = 7 - (i - 51); r = 8; }
	else if (i == 59) { c = 0; r = 9; }
	else { c = i - 60; r = 10; }
	*x = c;
	*y = r;
}

static void square_xy(int sq, float* x, float* y)
{
	ring_xy((sq + 8) % PA_RING, x, y);
}

/* Posición en pantalla del centro de una ficha */
static void piece_xy(int p, int i, float* x, float* y)
{
	int steps = s_game.pos[p][i];
	float cx, cy;
	if (steps == PA_HOME) {
		cx = HOME_X[p] + (i % 2 ? 1.2f : -1.2f);
		cy = HOME_Y[p] + (i / 2 ? 1.2f : -1.2f);
	} else if (steps == PA_GOAL) {
		static const float gx[PA_PLAYERS] = { 0, 0.6f, 0, -0.6f }, gy[PA_PLAYERS] = { 0.6f, 0, -0.6f, 0 };
		cx = 9 + gx[p] + (i - 1.5f) * 0.18f;
		cy = 9 + gy[p];
	} else if (steps >= PA_RING - 4) {
		int k = steps - (PA_RING - 4);
		static const int dx[PA_PLAYERS] = { 0, -1, 0, 1 }, dy[PA_PLAYERS] = { -1, 0, 1, 0 };
		static const int sx[PA_PLAYERS] = { 9, 17, 9, 1 }, sy[PA_PLAYERS] = { 17, 9, 1, 9 };
		cx = sx[p] + dx[p] * k;
		cy = sy[p] + dy[p] * k;
	} else {
		square_xy(pa_square(p, steps), &cx, &cy);
	}
	/* Si hay dos fichas juntas, se separan un poco */
	int same = 0, before = 0;
	for (int q = 0; q < PA_PLAYERS; q++)
		for (int j = 0; j < PA_PIECES; j++) {
			if (s_game.pos[q][j] == PA_HOME || s_game.pos[q][j] == PA_GOAL || steps == PA_HOME || steps == PA_GOAL)
				continue;
			bool here = steps < PA_RING - 4 ? pa_square(q, s_game.pos[q][j]) == pa_square(p, steps)
			                                  : q == p && s_game.pos[q][j] == steps;
			if (here) {
				same++;
				if (q < p || (q == p && j < i))
					before++;
			}
		}
	float off = same > 1 ? (before ? 0.22f : -0.22f) : 0.0f;
	*x = BX0 + (cx + 0.5f + off) * U;
	*y = BY0 + (cy + 0.5f + off) * U;
}

static void message(const char* m)
{
	snprintf(s_message, sizeof s_message, "%s", m);
}

static void new_game(void)
{
	pa_new_game(&s_game);
	s_sel = 0;
	s_recorded = false;
	s_end_time = 0.0f;
	message("Te toca: tira el dado. Con un 5 sacas ficha");
}

static void pa_enter(void)
{
	if (!s_started) {
		pa_init(&s_game, (uint32_t)time(NULL) ^ 0x9A3C);
		s_started = true;
		new_game();
	}
	button_set(&s_btn_menu, 258, 2, 54, 20, "Menú", C2D_Color32(120, 30, 35, 255), KEY_START);
}

static void after_roll(int p)
{
	char buf[96];
	if (s_game.last_three_sixes)
		snprintf(buf, sizeof buf, "¡Tres seises! %s la última ficha a casa", p == 0 ? "Vuelve" : "Le vuelve");
	else if (s_game.last_no_move)
		snprintf(buf, sizeof buf, "%s %d y no %s mover", p == 0 ? "Sacas un" : name(p), s_game.die, p == 0 ? "puedes" : "puede");
	else if (p == 0)
		snprintf(buf, sizeof buf, "Sacas un %d: elige ficha%s", s_game.die, s_game.move == 7 ? " (el 6 vale 7)" : "");
	else
		snprintf(buf, sizeof buf, "%s saca un %d", name(p), s_game.die);
	if (p == 0 || s_game.last_three_sixes || s_game.last_no_move)
		message(buf);
	if (s_game.last_three_sixes)
		mood_set(&s_mood[p], EXPR_SAD, 2.0f);
}

static void after_move(void)
{
	int p = s_game.last_player;
	char buf[96];
	audio_play(SFX_CHIP);
	if (s_game.last_captured_player >= 0) {
		int q = s_game.last_captured_player;
		if (p == 0)
			snprintf(buf, sizeof buf, "¡Te comes una de %s y cuentas 20!", name(q));
		else if (q == 0)
			snprintf(buf, sizeof buf, "¡%s se come una ficha tuya y cuenta 20!", name(p));
		else
			snprintf(buf, sizeof buf, "%s se come una de %s y cuenta 20", name(p), name(q));
		message(buf);
		mood_set(&s_mood[p], EXPR_HAPPY, 2.0f);
		mood_set(&s_mood[q], EXPR_SAD, 2.0f);
		audio_play(SFX_WIN);
	} else if (s_game.last_goal) {
		if (p == 0)
			snprintf(buf, sizeof buf, "Metes una ficha en la meta y cuentas 10");
		else
			snprintf(buf, sizeof buf, "%s mete una ficha en la meta y cuenta 10", name(p));
		message(buf);
		mood_set(&s_mood[p], EXPR_HAPPY, 2.0f);
	}
}

static SceneId pa_update(const Input* in, float dt)
{
	if (button_update(&s_btn_menu, in))
		return SCENE_MENU;
	s_roll_anim += dt;
	if (s_roll_anim < 0.4f)
		s_face = 1 + (int)(gfx_time() * 20.0f) % 6;
	bool mine = s_game.current == 0;
	bool over = s_game.phase == PA_OVER;
	button_set(&s_btn_roll, 12, 205, 120, 28, over ? GLYPH_A " Otra" : GLYPH_A " Tirar", C2D_Color32(40, 140, 60, 255),
		KEY_A);
	for (int i = 0; i < PA_PIECES; i++) {
		static char labels[PA_PIECES][32];
		int st = s_game.pos[0][i];
		if (st == PA_HOME)
			snprintf(labels[i], sizeof labels[i], "%d: en casa", i + 1);
		else if (st == PA_GOAL)
			snprintf(labels[i], sizeof labels[i], "%d: en la meta", i + 1);
		else if (st >= PA_RING - 4)
			snprintf(labels[i], sizeof labels[i], "%d: pasillo %d", i + 1, st - (PA_RING - 4) + 1);
		else
			snprintf(labels[i], sizeof labels[i], "%d: casilla %d", i + 1, pa_square(0, st));
		button_set(&s_btn_piece[i], 140 + (i % 2) * 90, 120 + (i / 2) * 36, 86, 30, labels[i],
			C2D_Color32(170, 140, 20, 255), 0);
		s_btn_piece[i].enabled = mine && s_game.phase == PA_MOVE && pa_can_move(&s_game, i);
	}
	if (over) {
		s_end_time += dt;
		if (s_end_time > 1.0f && button_update(&s_btn_roll, in))
			new_game();
	} else if (mine && s_roll_anim > 0.4f) {
		if (s_game.phase == PA_ROLL) {
			bool die_touched = in->touch_down && fabsf(in->tx - 66) < 40 && fabsf(in->ty - 110) < 40;
			if (die_touched || button_update(&s_btn_roll, in)) {
				s_face = pa_roll(&s_game, 0);
				s_roll_anim = 0.0f;
				audio_play(SFX_FLIP);
				after_roll(0);
				s_sel = 0;
			}
		} else {
			/* Elegir ficha: tocando su botón, o cruceta y Ⓐ */
			if (in->repeat & (KEY_LEFT | KEY_UP))
				s_sel = (s_sel + PA_PIECES - 1) % PA_PIECES;
			if (in->repeat & (KEY_RIGHT | KEY_DOWN))
				s_sel = (s_sel + 1) % PA_PIECES;
			if (!s_btn_piece[s_sel].enabled)
				for (int i = 0; i < PA_PIECES; i++)
					if (s_btn_piece[i].enabled) {
						s_sel = i;
						break;
					}
			int pick = -1;
			for (int i = 0; i < PA_PIECES; i++)
				if (s_btn_piece[i].enabled && button_update(&s_btn_piece[i], in))
					pick = i;
			if (pick < 0 && (in->down & KEY_A))
				pick = s_sel;
			if (pick >= 0) {
				if (pa_move(&s_game, pick))
					after_move();
				else {
					audio_play(SFX_ERROR);
					message(s_game.die == 5 ? "Con un 5 hay que sacar ficha" : "Esa ficha no se puede mover");
				}
			}
			if (s_game.current != 0)
				message("Juegan los demás...");
			else if (s_game.phase == PA_MOVE && s_game.move >= 10)
				message(s_game.move == 20 ? "Cuenta 20 con la ficha que quieras" : "Cuenta 10 con la ficha que quieras");
			else if (s_game.phase == PA_ROLL && pick >= 0)
				message("¡Has sacado un 6! Vuelve a tirar");
		}
	} else if (!mine) {
		s_cpu_timer += dt;
		if (s_cpu_timer > 0.7f) {
			s_cpu_timer = 0.0f;
			int p = s_game.current;
			if (s_game.phase == PA_ROLL) {
				s_face = pa_roll(&s_game, 0);
				s_roll_anim = 0.0f;
				audio_play(SFX_FLIP);
				after_roll(p);
			} else {
				int i = pa_ai_choose(&s_game);
				if (i >= 0 && pa_move(&s_game, i))
					after_move();
			}
			if (s_game.current == 0 && s_game.phase == PA_ROLL && !over)
				message("Te toca: tira el dado");
		}
	}
	if (s_game.phase == PA_OVER && !s_recorded) {
		s_recorded = true;
		s_end_time = 0.0f;
		int w = s_game.winner;
		for (int p = 0; p < PA_PLAYERS; p++)
			mood_set(&s_mood[p], p == w ? EXPR_HAPPY : EXPR_SAD, 5.0f);
		profile_record_game(GAME_PARCHIS, w == 0);
		audio_play(w == 0 ? SFX_WIN : SFX_LOSE);
		char buf[64];
		snprintf(buf, sizeof buf, w == 0 ? "¡Has metido las cuatro! Ganas" : "Gana %s", name(w));
		message(buf);
	}
	for (int p = 0; p < PA_PLAYERS; p++)
		mood_update(&s_mood[p], dt);
	return SCENE_PARCHIS;
}

static u32 rgb(int p, u8 a)
{
	return gfx_rgb(RGB[p], a);
}

static void cell_rect(float c, float r, u32 col, float d)
{
	gfx_rect(BX0 + c * U + 0.5f + gfx_dx(d), BY0 + r * U + 0.5f, U - 1, U - 1, d, col);
}

static void pa_draw_top(void)
{
	gfx_background(BG_TABLE_TOP);
	float d = DEPTH_TABLE;
	float dx = gfx_dx(d);
	gfx_rect(BX0 - 3 + dx, BY0 - 3, 19 * U + 6, 19 * U + 6, d, C2D_Color32(110, 70, 30, 255));
	gfx_rect(BX0 + dx, BY0, 19 * U, 19 * U, d, C2D_Color32(245, 238, 220, 255));
	/* Casas en las esquinas */
	for (int p = 0; p < PA_PLAYERS; p++) {
		float c0 = HOME_X[p] > 9 ? 11 : 0, r0 = HOME_Y[p] > 9 ? 11 : 0;
		gfx_rect(BX0 + c0 * U + dx + 2, BY0 + r0 * U + 2, 8 * U - 4, 8 * U - 4, d, rgb(p, 255));
		gfx_ellipse(BX0 + (HOME_X[p] + 0.5f) * U + dx, BY0 + (HOME_Y[p] + 0.5f) * U, 2.6f * U, 2.6f * U, d,
			C2D_Color32(255, 255, 255, 120));
	}
	/* Circuito */
	for (int sq = 1; sq <= PA_RING; sq++) {
		float c, r;
		square_xy(sq, &c, &r);
		u32 col = C2D_Color32(255, 252, 240, 255);
		for (int p = 0; p < PA_PLAYERS; p++)
			if (sq == pa_exit(p))
				col = rgb(p, 200);
		cell_rect(c, r, col, d);
		if (pa_safe(sq))
			gfx_ellipse(BX0 + (c + 0.5f) * U + dx, BY0 + (r + 0.5f) * U, U * 0.28f, U * 0.28f, d,
				C2D_Color32(150, 150, 160, 160));
	}
	/* Pasillos */
	for (int p = 0; p < PA_PLAYERS; p++)
		for (int k = 0; k < PA_LANE; k++) {
			static const int dxs[PA_PLAYERS] = { 0, -1, 0, 1 }, dys[PA_PLAYERS] = { -1, 0, 1, 0 };
			static const int sx[PA_PLAYERS] = { 9, 17, 9, 1 }, sy[PA_PLAYERS] = { 17, 9, 1, 9 };
			cell_rect(sx[p] + dxs[p] * k, sy[p] + dys[p] * k, rgb(p, 150), d);
		}
	/* Meta: cuatro triángulos */
	float cx = BX0 + 9.5f * U + dx, cy = BY0 + 9.5f * U, h = 1.5f * U;
	C2D_DrawTriangle(cx - h, cy + h, rgb(0, 255), cx + h, cy + h, rgb(0, 255), cx, cy, rgb(0, 255), 0.5f);
	C2D_DrawTriangle(cx + h, cy - h, rgb(1, 255), cx + h, cy + h, rgb(1, 255), cx, cy, rgb(1, 255), 0.5f);
	C2D_DrawTriangle(cx - h, cy - h, rgb(2, 255), cx + h, cy - h, rgb(2, 255), cx, cy, rgb(2, 255), 0.5f);
	C2D_DrawTriangle(cx - h, cy - h, rgb(3, 255), cx - h, cy + h, rgb(3, 255), cx, cy, rgb(3, 255), 0.5f);
	/* Fichas */
	float pulse = 0.5f + 0.5f * sinf(gfx_time() * 7.0f);
	for (int p = 0; p < PA_PLAYERS; p++)
		for (int i = 0; i < PA_PIECES; i++) {
			float x, y;
			piece_xy(p, i, &x, &y);
			float dd = DEPTH_CARDS;
			if (p == 0 && i == s_sel && s_game.current == 0 && s_game.phase == PA_MOVE && pa_can_move(&s_game, i))
				gfx_ellipse(x + gfx_dx(dd), y, 6.5f + pulse, 6.5f + pulse, dd, C2D_Color32(255, 255, 255, 230));
			gfx_ellipse(x + gfx_dx(dd) + 0.8f, y + 1.2f, 4.8f, 4.8f, dd, C2D_Color32(0, 0, 0, 100));
			gfx_ellipse(x + gfx_dx(dd), y, 4.8f, 4.8f, dd, C2D_Color32(40, 30, 30, 255));
			gfx_ellipse(x + gfx_dx(dd), y, 3.8f, 3.8f, dd, rgb(p, 255));
		}
	/* Jugadores */
	for (int p = 0; p < PA_PLAYERS; p++) {
		float x = SEAT_X[p], y = SEAT_Y[p];
		mii_draw(avatar(p), x, y, 16, DEPTH_UI, mood_expr(&s_mood[p]), false);
		bool turn = s_game.current == p && s_game.phase != PA_OVER;
		gfx_panel(x - 38, y + 18, 76, 30, DEPTH_UI, C2D_Color32(10, 50, 25, 220), turn ? COLOR_GOLD : rgb(p, 255));
		gfx_text(name(p), x, y + 20, DEPTH_UI, 0.42f, COLOR_CREAM, C2D_AlignCenter);
		gfx_textf(x, y + 33, DEPTH_UI, 0.4f, rgb(p, 255), C2D_AlignCenter, "Meta %d/4", pa_in_goal(&s_game, p));
	}
}

static void draw_die(float cx, float cy, int face)
{
	static const float pip[7][6][2] = {
		{ { 0 } },
		{ { 0, 0 } },
		{ { -1, -1 }, { 1, 1 } },
		{ { -1, -1 }, { 0, 0 }, { 1, 1 } },
		{ { -1, -1 }, { 1, -1 }, { -1, 1 }, { 1, 1 } },
		{ { -1, -1 }, { 1, -1 }, { 0, 0 }, { -1, 1 }, { 1, 1 } },
		{ { -1, -1 }, { 1, -1 }, { -1, 0 }, { 1, 0 }, { -1, 1 }, { 1, 1 } },
	};
	float s = 30;
	gfx_rrect(cx - s + 3, cy - s + 4, 2 * s, 2 * s, 10, 0, C2D_Color32(0, 0, 0, 90));
	gfx_rrect(cx - s, cy - s, 2 * s, 2 * s, 10, 0, C2D_Color32(250, 250, 245, 255));
	for (int i = 0; i < face; i++)
		gfx_ellipse(cx + pip[face][i][0] * 16, cy + pip[face][i][1] * 16, 5.5f, 5.5f, 0, C2D_Color32(30, 30, 40, 255));
}

static void pa_draw_bottom(void)
{
	gfx_background(BG_TABLE_BOTTOM);
	gfx_text_wrap(s_message, 8, 2, 0, 0.45f, COLOR_GOLD, 246);
	button_draw(&s_btn_menu, false);
	draw_die(66, 110, s_face);
	int p = s_game.current;
	gfx_textf(66, 150, 0, 0.45f, rgb(p, 255), C2D_AlignCenter, p == 0 ? "Te toca" : "Turno de %s", name(p));
	gfx_text("Tus fichas (amarillas)", 230, 96, 0, 0.45f, COLOR_CREAM, C2D_AlignCenter);
	for (int i = 0; i < PA_PIECES; i++) {
		button_draw(&s_btn_piece[i], s_btn_piece[i].enabled);
		if (s_btn_piece[i].enabled && i == s_sel) {
			float pulse = 0.6f + 0.4f * sinf(gfx_time() * 6.0f);
			gfx_rrect(s_btn_piece[i].x - 2, s_btn_piece[i].y - 2, s_btn_piece[i].w + 4, 3, 1, 0,
				C2D_Color32(255, 255, 255, (u8)(255 * pulse)));
		}
	}
	if (s_game.current == 0 && s_game.phase == PA_MOVE)
		gfx_textf(230, 196, 0, 0.45f, COLOR_GOLD, C2D_AlignCenter, "Mueves %d", s_game.move);
	if ((s_game.current == 0 && s_game.phase == PA_ROLL) || s_game.phase == PA_OVER)
		button_draw(&s_btn_roll, s_game.phase != PA_OVER || s_end_time > 1.0f);
	gfx_text("Seguros: casillas con círculo", 230, 216, 0, 0.4f, C2D_Color32(200, 230, 200, 255), C2D_AlignCenter);
}

const Scene scene_parchis = { pa_enter, pa_update, pa_draw_top, pa_draw_bottom };
