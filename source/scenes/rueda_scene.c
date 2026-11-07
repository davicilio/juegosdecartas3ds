/* Rueda de la fortuna: la rueda arriba y las apuestas abajo. */
#include <math.h>
#include <stdio.h>
#include <time.h>

#include "games/rueda.h"
#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/casino.h"
#include "ui/gfx.h"
#include "ui/mii.h"
#include "ui/wallet.h"
#include "ui/widgets.h"

#define PI      3.14159265f
#define CX      200.0f
#define CY      128.0f
#define RADIUS  108.0f
#define SPIN_T  5.0f
#define PICK_Y  150.0f

typedef enum { PH_BET, PH_SPIN, PH_RESULT } Phase;

static Rng s_rng;
static bool s_started;
static Phase s_phase;
static int s_bets[RD_SYMBOLS], s_last[RD_SYMBOLS];
static int s_chip = 1, s_slot, s_back, s_staked;
static float s_t, s_angle0, s_angle_total, s_angle, s_result_time;
static int s_last_tick;
static char s_message[80];
static Button s_btn_spin, s_btn_clear, s_btn_repeat, s_btn_menu;

static const u32 SYM_COL[RD_SYMBOLS] = { 0xE8D040, 0x3C8CDC, 0xD24646, 0x46B45A, 0x9A50C8, 0x202020, 0xF08C1E };

static void area(int s, float* x, float* y, float* w, float* h)
{
	if (s < 4) {
		*x = 6 + s * 78;
		*y = 26;
	} else {
		*x = 6 + (s - 4) * 104;
		*y = 76;
	}
	*w = s < 4 ? 74 : 100;
	*h = 46;
}

static void rd_enter(void)
{
	if (!s_started) {
		s_started = true;
		rng_seed(&s_rng, (uint32_t)time(NULL) ^ 0xF0F0);
		for (int i = 0; i < RD_SYMBOLS; i++)
			s_bets[i] = s_last[i] = 0;
		s_phase = PH_BET;
		s_angle = 0.0f;
		snprintf(s_message, sizeof s_message, "Apuesta a uno o varios símbolos");
	}
	button_set(&s_btn_menu, 262, 2, 54, 18, "Menú", C2D_Color32(120, 30, 35, 255), KEY_START);
}

/* La casilla i está centrada en el ángulo angle + i * paso; la flecha está arriba */
static float slot_step(void)
{
	return 2 * PI / RD_SLOTS;
}

static SceneId rd_update(const Input* in, float dt)
{
	if (button_update(&s_btn_menu, in))
		return SCENE_MENU;
	bool betting = s_phase == PH_BET;
	int staked = casino_sum(s_bets, RD_SYMBOLS);
	button_set(&s_btn_clear, 4, 205, 96, 28, GLYPH_X " Quitar", C2D_Color32(150, 60, 50, 255), KEY_X);
	button_set(&s_btn_repeat, 104, 205, 100, 28, GLYPH_Y " Repetir", C2D_Color32(60, 90, 160, 255), KEY_Y);
	button_set(&s_btn_spin, 208, 205, 108, 28, GLYPH_A " Girar", C2D_Color32(40, 140, 60, 255), KEY_A);
	s_btn_clear.enabled = betting && staked > 0;
	s_btn_repeat.enabled = betting && casino_sum(s_last, RD_SYMBOLS) > 0;
	s_btn_spin.enabled = betting && staked > 0;
	switch (s_phase) {
	case PH_BET:
		s_angle += dt * 0.1f;
		casino_pick_update(&s_chip, in, PICK_Y);
		if (in->touch_down)
			for (int s = 0; s < RD_SYMBOLS; s++) {
				float x, y, w, h;
				area(s, &x, &y, &w, &h);
				if (input_touch_in(in, x, y, w, h))
					casino_place(&s_bets[s], casino_chip_value(s_chip));
			}
		if (button_update(&s_btn_clear, in)) {
			casino_refund(s_bets, RD_SYMBOLS);
			audio_play(SFX_CHIP);
		}
		if (button_update(&s_btn_repeat, in))
			casino_repeat(s_bets, s_last, RD_SYMBOLS);
		if (button_update(&s_btn_spin, in)) {
			s_staked = staked;
			for (int i = 0; i < RD_SYMBOLS; i++)
				s_last[i] = s_bets[i];
			s_slot = rd_spin(&s_rng);
			/* Para que la casilla quede arriba: angle + slot*paso = -k*2pi */
			s_angle0 = fmodf(s_angle, 2 * PI);
			float target = -s_slot * slot_step();
			float delta = fmodf(target - s_angle0, 2 * PI);
			if (delta < 0)
				delta += 2 * PI;
			/* Medio paso al azar dentro de la casilla para que no pare siempre en el centro */
			float jitter = ((int)rng_range(&s_rng, 61) - 30) / 100.0f * slot_step();
			s_angle_total = delta + 2 * PI * 4 + jitter;
			s_t = 0.0f;
			s_last_tick = -1;
			s_phase = PH_SPIN;
			snprintf(s_message, sizeof s_message, "¡Gira la rueda!");
			audio_play(SFX_FLIP);
		} else if (staked == 0 && wallet_chips() == 0 && (in->down & KEY_A))
			return SCENE_FICHAS;
		break;
	case PH_SPIN: {
		s_t += dt;
		float f = fminf(1.0f, s_t / SPIN_T);
		float e = 1 - (1 - f) * (1 - f) * (1 - f);
		s_angle = s_angle0 + s_angle_total * e;
		int tick = (int)floorf(s_angle / slot_step() + 0.5f);
		if (tick != s_last_tick) {
			s_last_tick = tick;
			audio_play(SFX_SELECT);
		}
		if (s_t >= SPIN_T + 0.3f) {
			s_back = rd_payout(s_bets, s_slot);
			if (s_back)
				wallet_give(s_back);
			int sym = RD_WHEEL[s_slot];
			profile_record_game(GAME_RUEDA, s_back > s_staked);
			audio_play(s_back > s_staked ? SFX_WIN : s_back ? SFX_CHIP : SFX_LOSE);
			if (s_back)
				snprintf(s_message, sizeof s_message, "Sale %s. Cobras %d", rd_label(sym), s_back);
			else
				snprintf(s_message, sizeof s_message, "Sale %s. Pierdes %d", rd_label(sym), s_staked);
			s_phase = PH_RESULT;
			s_result_time = 0.0f;
		}
		break;
	}
	case PH_RESULT:
		s_result_time += dt;
		if (s_result_time > 1.0f && (in->touch_down || (in->down & (KEY_A | KEY_B)))) {
			for (int i = 0; i < RD_SYMBOLS; i++)
				s_bets[i] = 0;
			s_phase = PH_BET;
			snprintf(s_message, sizeof s_message, wallet_chips() > 0 ? "Hagan sus apuestas" : "Sin fichas: " GLYPH_A " para comprar");
		}
		break;
	}
	return SCENE_RUEDA;
}

static void rd_draw_top(void)
{
	gfx_background(BG_TABLE_TOP);
	float d = DEPTH_CARDS, step = slot_step();
	gfx_ellipse(CX + 3, CY + 5, RADIUS + 10, RADIUS + 10, DEPTH_TABLE, C2D_Color32(0, 0, 0, 100));
	gfx_ellipse(CX, CY, RADIUS + 10, RADIUS + 10, DEPTH_TABLE, C2D_Color32(200, 150, 40, 255));
	gfx_ellipse(CX, CY, RADIUS + 4, RADIUS + 4, DEPTH_TABLE, C2D_Color32(110, 60, 25, 255));
	for (int i = 0; i < RD_SLOTS; i++) {
		float a0 = s_angle + (i - 0.5f) * step - PI / 2, a1 = a0 + step;
		u32 c = gfx_rgb(SYM_COL[RD_WHEEL[i]], 255);
		gfx_triangle(CX, CY, CX + cosf(a0) * RADIUS, CY + sinf(a0) * RADIUS, CX + cosf(a1) * RADIUS, CY + sinf(a1) * RADIUS, d, c);
		gfx_line(CX + cosf(a0) * 30, CY + sinf(a0) * 30, CX + cosf(a0) * RADIUS, CY + sinf(a0) * RADIUS, 1.0f, d,
			C2D_Color32(250, 240, 210, 255));
		float am = a0 + step / 2;
		int sym = RD_WHEEL[i];
		const char* lab = sym == RD_JOKER ? "J" : sym == RD_STAR ? "*" : rd_label(sym);
		gfx_text(lab, CX + cosf(am) * (RADIUS - 12), CY + sinf(am) * (RADIUS - 12) - 6, d, 0.38f,
			sym == RD_1 ? C2D_Color32(60, 40, 10, 255) : COLOR_WHITE, C2D_AlignCenter);
		/* Clavos del borde */
		gfx_ellipse(CX + cosf(a0) * (RADIUS + 1), CY + sinf(a0) * (RADIUS + 1), 1.8f, 1.8f, d, C2D_Color32(240, 220, 150, 255));
	}
	gfx_ellipse(CX, CY, 30, 30, d + 0.3f, C2D_Color32(200, 150, 40, 255));
	gfx_ellipse(CX, CY, 24, 24, d + 0.3f, C2D_Color32(140, 20, 30, 255));
	gfx_text("Suerte", CX, CY - 7, d + 0.3f, 0.45f, COLOR_GOLD, C2D_AlignCenter);
	/* Flecha */
	gfx_triangle(CX - 10, CY - RADIUS - 16, CX + 10, CY - RADIUS - 16, CX, CY - RADIUS + 8, DEPTH_UI, C2D_Color32(240, 240, 240, 255));
	gfx_triangle(CX - 6, CY - RADIUS - 14, CX + 6, CY - RADIUS - 14, CX, CY - RADIUS + 3, DEPTH_UI, C2D_Color32(200, 30, 40, 255));
	if (s_phase == PH_RESULT && banner_draw(s_result_time, CX, CY, 150, 50))
		gfx_textf(CX, CY - 14, DEPTH_BANNER, 0.6f, COLOR_GOLD, C2D_AlignCenter, s_back ? "Cobras %d" : "Sin premio", s_back);
}

static void rd_draw_bottom(void)
{
	gfx_background(BG_TABLE_BOTTOM);
	gfx_text(s_message, 6, 4, 0, 0.45f, COLOR_GOLD, C2D_AlignLeft);
	button_draw(&s_btn_menu, false);
	for (int s = 0; s < RD_SYMBOLS; s++) {
		float x, y, w, h;
		area(s, &x, &y, &w, &h);
		bool win = s_phase == PH_RESULT && RD_WHEEL[s_slot] == s && fmodf(gfx_time(), 0.6f) < 0.35f;
		gfx_rrect(x, y, w, h, 8, 0, win ? COLOR_GOLD : C2D_Color32(230, 220, 190, 255));
		gfx_rrect(x + 2, y + 2, w - 4, h - 4, 7, 0, gfx_rgb(SYM_COL[s], 255));
		gfx_text(rd_label(s), x + 8, y + 4, 0, 0.52f, s == RD_1 ? C2D_Color32(60, 40, 10, 255) : COLOR_WHITE, C2D_AlignLeft);
		gfx_textf(x + 8, y + 26, 0, 0.36f, s == RD_1 ? C2D_Color32(60, 40, 10, 255) : COLOR_CREAM, C2D_AlignLeft, "%d a 1",
			rd_pays(s));
		casino_chip(s_bets[s], x + w - 18, y + h / 2, 0.95f, 0);
	}
	casino_pick_draw(s_chip, PICK_Y, casino_sum(s_bets, RD_SYMBOLS));
	button_draw(&s_btn_clear, false);
	button_draw(&s_btn_repeat, false);
	button_draw(&s_btn_spin, s_btn_spin.enabled);
}

static void scene_leave(void)
{
	if (s_phase != PH_RESULT)
		casino_refund(s_bets, RD_SYMBOLS);
	s_started = false;
}

const Scene scene_rueda = { rd_enter, rd_update, rd_draw_top, rd_draw_bottom, scene_leave };
