/* Ruleta francesa: el cilindro arriba y el paño de apuestas abajo. */
#include <math.h>
#include <stdio.h>
#include <time.h>

#include "games/ruleta.h"
#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/casino.h"
#include "ui/gfx.h"
#include "ui/mii.h"
#include "ui/wallet.h"
#include "ui/widgets.h"

#define PI       3.14159265f
#define WHEEL_X  200.0f
#define WHEEL_Y  120.0f
#define R_OUT    104.0f
#define R_NUM    92.0f
#define R_IN     72.0f
#define SPIN_T   5.5f
#define HISTORY  12

/* Paño */
#define T_Y     26.0f
#define CELL_W  22.0f
#define CELL_H  24.0f
#define NUM_X   26.0f
#define DOZ_Y   (T_Y + 3 * CELL_H + 2)
#define EVEN_Y  (DOZ_Y + 22)
#define PICK_Y  178.0f

typedef enum { PH_BET, PH_SPIN, PH_RESULT } Phase;

static Rng s_rng;
static bool s_started;
static Phase s_phase;
static int s_bets[RL_SPOTS], s_last[RL_SPOTS];
static int s_chip = 1;
static int s_result, s_history[HISTORY], s_history_n;
static int s_won, s_staked;
static float s_t, s_wheel0, s_wheel_speed, s_ball_final, s_result_time;
static char s_message[80];
static Button s_btn_spin, s_btn_clear, s_btn_repeat, s_btn_menu;

static void rects(int spot, float* x, float* y, float* w, float* h)
{
	if (spot == 0) {
		*x = 4;
		*y = T_Y;
		*w = NUM_X - 4;
		*h = 3 * CELL_H;
	} else if (spot < RL_NUMBERS) {
		int c = (spot - 1) / 3, r = 2 - (spot - 1) % 3;
		*x = NUM_X + c * CELL_W;
		*y = T_Y + r * CELL_H;
		*w = CELL_W;
		*h = CELL_H;
	} else if (spot < RL_COLUMN0) {
		*x = NUM_X + (spot - RL_DOZEN0) * 4 * CELL_W;
		*y = DOZ_Y;
		*w = 4 * CELL_W;
		*h = 20;
	} else if (spot < RL_LOW) {
		*x = NUM_X + 12 * CELL_W;
		*y = T_Y + (2 - (spot - RL_COLUMN0)) * CELL_H;
		*w = BOTTOM_W - 4 - *x;
		*h = CELL_H;
	} else {
		*x = NUM_X + (spot - RL_LOW) * 2 * CELL_W;
		*y = EVEN_Y;
		*w = 2 * CELL_W;
		*h = 20;
	}
}

static void ru_enter(void)
{
	if (!s_started) {
		s_started = true;
		rng_seed(&s_rng, (uint32_t)time(NULL) ^ 0x2017);
		for (int i = 0; i < RL_SPOTS; i++)
			s_bets[i] = s_last[i] = 0;
		s_phase = PH_BET;
		s_history_n = 0;
		s_wheel0 = 0.0f;
		s_won = -1;
		snprintf(s_message, sizeof s_message, "Toca el paño para apostar");
	}
	button_set(&s_btn_menu, 262, 2, 54, 18, "Menú", C2D_Color32(120, 30, 35, 255), KEY_START);
}

static float wheel_angle(float t)
{
	if (t >= SPIN_T)
		t = SPIN_T;
	return s_wheel0 + s_wheel_speed * (t - t * t / (2 * SPIN_T));
}

static int wheel_index(int n)
{
	for (int i = 0; i < RL_NUMBERS; i++)
		if (RL_WHEEL[i] == n)
			return i;
	return 0;
}

static float ball_angle(float t)
{
	float k = t >= SPIN_T ? 0.0f : (SPIN_T - t);
	return s_ball_final + 1.6f * k * k;
}

static float ball_radius(float t)
{
	float f = t / SPIN_T;
	if (f < 0.55f)
		return R_OUT - 4;
	float g = (f - 0.55f) / 0.45f;
	if (g > 1)
		g = 1;
	float bounce = fabsf(sinf(g * PI * 3)) * (1 - g) * 8;
	return R_OUT - 4 - g * (R_OUT - 4 - (R_NUM - 2)) + bounce;
}

static void start_spin(void)
{
	s_staked = casino_sum(s_bets, RL_SPOTS);
	for (int i = 0; i < RL_SPOTS; i++)
		s_last[i] = s_bets[i];
	s_result = rl_spin(&s_rng);
	s_wheel0 = fmodf(wheel_angle(SPIN_T), 2 * PI);
	s_wheel_speed = 4.0f + (rng_range(&s_rng, 100) / 100.0f);
	s_t = 0.0f;
	float final_wheel = wheel_angle(SPIN_T);
	s_ball_final = final_wheel + wheel_index(s_result) * 2 * PI / RL_NUMBERS;
	s_phase = PH_SPIN;
	snprintf(s_message, sizeof s_message, "No va más");
	audio_play(SFX_FLIP);
}

static SceneId ru_update(const Input* in, float dt)
{
	if (button_update(&s_btn_menu, in))
		return SCENE_MENU;
	bool betting = s_phase == PH_BET;
	button_set(&s_btn_clear, 4, 205, 96, 28, GLYPH_X " Quitar", C2D_Color32(150, 60, 50, 255), KEY_X);
	button_set(&s_btn_repeat, 104, 205, 100, 28, GLYPH_Y " Repetir", C2D_Color32(60, 90, 160, 255), KEY_Y);
	button_set(&s_btn_spin, 208, 205, 108, 28, GLYPH_A " Girar", C2D_Color32(40, 140, 60, 255), KEY_A);
	int staked = casino_sum(s_bets, RL_SPOTS);
	s_btn_clear.enabled = betting && staked > 0;
	s_btn_repeat.enabled = betting && casino_sum(s_last, RL_SPOTS) > 0;
	s_btn_spin.enabled = betting && staked > 0;
	switch (s_phase) {
	case PH_BET:
		casino_pick_update(&s_chip, in, PICK_Y);
		if (in->touch_down)
			for (int s = 0; s < RL_SPOTS; s++) {
				float x, y, w, h;
				rects(s, &x, &y, &w, &h);
				if (input_touch_in(in, x, y, w, h)) {
					casino_place(&s_bets[s], casino_chip_value(s_chip));
					break;
				}
			}
		if (button_update(&s_btn_clear, in)) {
			casino_refund(s_bets, RL_SPOTS);
			audio_play(SFX_CHIP);
		}
		if (button_update(&s_btn_repeat, in))
			casino_repeat(s_bets, s_last, RL_SPOTS);
		if (button_update(&s_btn_spin, in))
			start_spin();
		if (staked == 0 && wallet_chips() == 0 && (in->down & KEY_A))
			return SCENE_FICHAS;
		break;
	case PH_SPIN:
		s_t += dt;
		if (s_t >= SPIN_T + 0.4f) {
			int back = rl_payout(s_bets, s_result);
			if (back)
				wallet_give(back);
			s_won = back;
			for (int i = HISTORY - 1; i > 0; i--)
				s_history[i] = s_history[i - 1];
			s_history[0] = s_result;
			if (s_history_n < HISTORY)
				s_history_n++;
			profile_record_game(GAME_RULETA, back > s_staked);
			audio_play(back > s_staked ? SFX_WIN : back > 0 ? SFX_CHIP : SFX_LOSE);
			const char* col = s_result == 0 ? "verde" : rl_is_red(s_result) ? "rojo" : "negro";
			if (back > 0)
				snprintf(s_message, sizeof s_message, "%d %s. Cobras %d", s_result, col, back);
			else
				snprintf(s_message, sizeof s_message, "%d %s. Pierdes %d", s_result, col, s_staked);
			s_phase = PH_RESULT;
			s_result_time = 0.0f;
		}
		break;
	case PH_RESULT:
		s_result_time += dt;
		if (s_result_time > 1.2f && (in->touch_down || (in->down & (KEY_A | KEY_B)))) {
			for (int i = 0; i < RL_SPOTS; i++)
				s_bets[i] = 0;
			s_phase = PH_BET;
			snprintf(s_message, sizeof s_message, wallet_chips() > 0 ? "Hagan sus apuestas" : "Sin fichas: " GLYPH_A " para comprar");
		}
		break;
	}
	return SCENE_RULETA;
}

static u32 number_color(int n)
{
	return n == 0 ? C2D_Color32(20, 130, 60, 255) : rl_is_red(n) ? C2D_Color32(190, 25, 30, 255) : C2D_Color32(25, 25, 30, 255);
}

static void ru_draw_top(void)
{
	gfx_background(BG_TABLE_TOP);
	float t = s_phase == PH_BET ? 0.0f : s_t;
	float w = s_phase == PH_BET ? s_wheel0 + gfx_time() * 0.15f : wheel_angle(t);
	float d = DEPTH_CARDS;
	/* Madera exterior */
	gfx_ellipse(WHEEL_X + 3, WHEEL_Y + 5, R_OUT + 14, R_OUT + 14, DEPTH_TABLE, C2D_Color32(0, 0, 0, 90));
	gfx_ellipse(WHEEL_X, WHEEL_Y, R_OUT + 14, R_OUT + 14, DEPTH_TABLE, C2D_Color32(110, 60, 25, 255));
	gfx_ellipse(WHEEL_X, WHEEL_Y, R_OUT + 8, R_OUT + 8, DEPTH_TABLE, C2D_Color32(150, 90, 40, 255));
	gfx_ellipse(WHEEL_X, WHEEL_Y, R_OUT, R_OUT, DEPTH_TABLE, C2D_Color32(60, 35, 15, 255));
	/* Casillas */
	float step = 2 * PI / RL_NUMBERS;
	for (int i = 0; i < RL_NUMBERS; i++) {
		float a0 = w + (i - 0.5f) * step - PI / 2, a1 = a0 + step;
		float c0 = cosf(a0), s0 = sinf(a0), c1 = cosf(a1), s1 = sinf(a1);
		u32 col = number_color(RL_WHEEL[i]);
		float ro = R_OUT - 2, ri = R_IN;
		gfx_triangle(WHEEL_X + c0 * ro, WHEEL_Y + s0 * ro, WHEEL_X + c1 * ro, WHEEL_Y + s1 * ro, WHEEL_X + c0 * ri,
			WHEEL_Y + s0 * ri, d, col);
		gfx_triangle(WHEEL_X + c1 * ro, WHEEL_Y + s1 * ro, WHEEL_X + c1 * ri, WHEEL_Y + s1 * ri, WHEEL_X + c0 * ri,
			WHEEL_Y + s0 * ri, d, col);
		gfx_line(WHEEL_X + c0 * ri, WHEEL_Y + s0 * ri, WHEEL_X + c0 * ro, WHEEL_Y + s0 * ro, 1.0f, d,
			C2D_Color32(220, 190, 110, 255));
		float am = a0 + step / 2;
		gfx_textf_plain(WHEEL_X + cosf(am) * R_NUM, WHEEL_Y + sinf(am) * R_NUM - 5.5f, d, 0.36f, COLOR_WHITE,
			C2D_AlignCenter, "%d", RL_WHEEL[i]);
	}
	/* Cono central */
	gfx_ellipse(WHEEL_X, WHEEL_Y, R_IN, R_IN, d, C2D_Color32(120, 70, 30, 255));
	gfx_ellipse(WHEEL_X, WHEEL_Y, R_IN - 10, R_IN - 10, d, C2D_Color32(160, 100, 45, 255));
	gfx_ellipse(WHEEL_X, WHEEL_Y, 22, 22, d + 0.3f, C2D_Color32(200, 160, 70, 255));
	for (int k = 0; k < 4; k++) {
		float a = w + k * PI / 2;
		gfx_line(WHEEL_X, WHEEL_Y, WHEEL_X + cosf(a) * 34, WHEEL_Y + sinf(a) * 34, 4, d + 0.4f, C2D_Color32(230, 200, 110, 255));
		gfx_ellipse(WHEEL_X + cosf(a) * 34, WHEEL_Y + sinf(a) * 34, 4, 4, d + 0.4f, C2D_Color32(250, 225, 140, 255));
	}
	gfx_ellipse(WHEEL_X, WHEEL_Y, 9, 9, d + 0.5f, C2D_Color32(250, 225, 140, 255));
	/* Bola */
	if (s_phase != PH_BET) {
		float ba = ball_angle(t) - PI / 2, br = ball_radius(t);
		float bx = WHEEL_X + cosf(ba) * br, by = WHEEL_Y + sinf(ba) * br;
		gfx_ellipse(bx + 1, by + 1.5f, 5, 5, DEPTH_LIFTED, C2D_Color32(0, 0, 0, 110));
		gfx_ellipse(bx, by, 5, 5, DEPTH_LIFTED, C2D_Color32(245, 245, 245, 255));
		gfx_ellipse(bx - 1.5f, by - 1.5f, 1.8f, 1.8f, DEPTH_LIFTED, COLOR_WHITE);
	}
	/* Últimos números */
	gfx_text("Últimos", 372, 6, DEPTH_UI, 0.4f, COLOR_CREAM, C2D_AlignCenter);
	for (int i = 0; i < s_history_n; i++) {
		float y = 30 + i * 17;
		int n = s_history[i];
		gfx_rrect(356, y - 7, 32, 15, 4, DEPTH_UI, number_color(n));
		gfx_textf_plain(372, y - 6, DEPTH_UI, 0.4f, COLOR_WHITE, C2D_AlignCenter, "%d", n);
	}
	if (s_phase == PH_RESULT && banner_draw(s_result_time, WHEEL_X, WHEEL_Y, 120, 64)) {
		gfx_rrect(WHEEL_X - 26, WHEEL_Y - 24, 52, 30, 6, DEPTH_BANNER, number_color(s_result));
		gfx_textf(WHEEL_X, WHEEL_Y - 24, DEPTH_BANNER, 0.8f, COLOR_WHITE, C2D_AlignCenter, "%d", s_result);
		gfx_textf(WHEEL_X, WHEEL_Y + 8, DEPTH_BANNER, 0.45f, COLOR_GOLD, C2D_AlignCenter,
			s_won > 0 ? "Cobras %d" : "Sin premio", s_won);
	}
}

static void ru_draw_bottom(void)
{
	gfx_background(BG_TABLE_BOTTOM);
	gfx_text(s_message, 6, 4, 0, 0.45f, COLOR_GOLD, C2D_AlignLeft);
	button_draw(&s_btn_menu, false);
	u32 felt = C2D_Color32(20, 110, 55, 255), line = C2D_Color32(230, 220, 190, 255);
	gfx_rect(2, T_Y - 2, BOTTOM_W - 4, EVEN_Y + 22 - T_Y + 2, 0, line);
	static const char* const OUTSIDE[] = { "1-18", "Par", "", "", "Impar", "19-36" };
	static const char* const DOZEN[] = { "1ª docena", "2ª docena", "3ª docena" };
	for (int s = 0; s < RL_SPOTS; s++) {
		float x, y, w, h;
		rects(s, &x, &y, &w, &h);
		bool hit = s_phase == PH_RESULT && rl_wins(s, s_result);
		u32 fill = s < RL_NUMBERS ? number_color(s) : felt;
		if (hit && fmodf(gfx_time(), 0.6f) < 0.35f)
			fill = C2D_Color32(240, 200, 60, 255);
		gfx_rect(x + 1, y + 1, w - 2, h - 2, 0, fill);
		if (s < RL_NUMBERS)
			gfx_textf_plain(x + w / 2, y + h / 2 - 6.5f, 0, 0.42f, COLOR_WHITE, C2D_AlignCenter, "%d", s);
		else if (s < RL_COLUMN0)
			gfx_text(DOZEN[s - RL_DOZEN0], x + w / 2, y + 3, 0, 0.4f, COLOR_CREAM, C2D_AlignCenter);
		else if (s < RL_LOW)
			gfx_text("2:1", x + w / 2, y + 6, 0, 0.38f, COLOR_CREAM, C2D_AlignCenter);
		else if (s == RL_RED || s == RL_BLACK) {
			u32 c = s == RL_RED ? C2D_Color32(200, 30, 35, 255) : C2D_Color32(20, 20, 25, 255);
			gfx_triangle(x + w / 2 - 12, y + 10, x + w / 2, y + 3, x + w / 2 + 12, y + 10, 0, c);
			gfx_triangle(x + w / 2 - 12, y + 10, x + w / 2, y + 17, x + w / 2 + 12, y + 10, 0, c);
		} else
			gfx_text(OUTSIDE[s - RL_LOW], x + w / 2, y + 3, 0, 0.4f, COLOR_CREAM, C2D_AlignCenter);
	}
	for (int s = 0; s < RL_SPOTS; s++)
		if (s_bets[s]) {
			float x, y, w, h;
			rects(s, &x, &y, &w, &h);
			casino_chip(s_bets[s], x + w / 2, y + h / 2, 0.75f, 0);
		}
	casino_pick_draw(s_chip, PICK_Y, casino_sum(s_bets, RL_SPOTS));
	button_draw(&s_btn_clear, false);
	button_draw(&s_btn_repeat, false);
	button_draw(&s_btn_spin, s_btn_spin.enabled);
}

/* Salir a medias: las fichas de la mesa vuelven al monedero (si la bola
 * está girando, la tirada se anula) */
static void scene_leave(void)
{
	if (s_phase != PH_RESULT)
		casino_refund(s_bets, RL_SPOTS);
	s_started = false;
}

const Scene scene_ruleta = { ru_enter, ru_update, ru_draw_top, ru_draw_bottom, scene_leave };
