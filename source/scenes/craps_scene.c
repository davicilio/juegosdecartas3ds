/* Dados (craps): los dados arriba y el paño de apuestas abajo. */
#include <math.h>
#include <stdio.h>
#include <time.h>

#include "games/craps.h"
#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/casino.h"
#include "ui/gfx.h"
#include "ui/mii.h"
#include "ui/wallet.h"
#include "ui/widgets.h"

#define PICK_Y 178.0f
#define HISTORY 14

static Craps s_game;
static bool s_started, s_rolling;
static int s_chip = 1, s_face[2] = { 5, 2 };
static int s_history[HISTORY], s_history_n;
static float s_t, s_result_time;
static int s_staked;
static char s_message[90];
static Button s_btn_roll, s_btn_clear, s_btn_menu;

static const char* const NAMES[CR_BETS] = { "Pase", "No pase", "Campo", "Cualquier 7", "Craps" };
static const char* const PAYS[CR_BETS] = { "1 a 1 · solo en la salida", "1 a 1 · el 12 empata", "2-4 y 9-12 · 1 a 1\n(el 2 doble, el 12 triple)",
	"una tirada · 4 a 1", "2, 3 o 12 · 7 a 1" };

static void area(int b, float* x, float* y, float* w, float* h)
{
	switch (b) {
	case CR_PASS:
		*x = 6, *y = 116, *w = 308, *h = 40;
		break;
	case CR_DONTPASS:
		*x = 6, *y = 76, *w = 150, *h = 38;
		break;
	case CR_FIELD:
		*x = 6, *y = 26, *w = 150, *h = 48;
		break;
	case CR_ANY7:
		*x = 160, *y = 26, *w = 154, *h = 48;
		break;
	default:
		*x = 160, *y = 76, *w = 154, *h = 38;
		break;
	}
}

static void cr_enter(void)
{
	if (!s_started) {
		s_started = true;
		cr_init(&s_game, (uint32_t)time(NULL) ^ 0xC4A5);
		s_rolling = false;
		s_history_n = 0;
		snprintf(s_message, sizeof s_message, "Apuesta al pase antes de la salida");
	}
	button_set(&s_btn_menu, 262, 2, 54, 18, "Menú", C2D_Color32(120, 30, 35, 255), KEY_START);
}

static SceneId cr_update(const Input* in, float dt)
{
	if (button_update(&s_btn_menu, in))
		return SCENE_MENU;
	int staked = casino_sum(s_game.bets, CR_BETS);
	/* Solo se pueden retirar las que aún se pueden poner (el pase con punto, no) */
	int removable = 0;
	for (int b = 0; b < CR_BETS; b++)
		if (cr_can_bet(&s_game, b))
			removable += s_game.bets[b];
	button_set(&s_btn_clear, 4, 205, 120, 28, GLYPH_X " Quitar", C2D_Color32(150, 60, 50, 255), KEY_X);
	button_set(&s_btn_roll, 196, 205, 120, 28, GLYPH_A " Tirar", C2D_Color32(40, 140, 60, 255), KEY_A);
	s_btn_clear.enabled = !s_rolling && removable > 0;
	s_btn_roll.enabled = !s_rolling && staked > 0;
	s_result_time += dt;
	if (s_rolling) {
		s_t += dt;
		if ((int)(s_t * 14) != (int)((s_t - dt) * 14)) {
			s_face[0] = 1 + (int)(gfx_time() * 37) % 6;
			s_face[1] = 1 + (int)(gfx_time() * 53) % 6;
		}
		if (s_t > 1.0f) {
			s_rolling = false;
			int point_before = s_game.point;
			int back = cr_roll(&s_game, 0, 0);
			s_face[0] = s_game.die[0];
			s_face[1] = s_game.die[1];
			int sum = s_face[0] + s_face[1];
			if (back)
				wallet_give(back);
			for (int i = HISTORY - 1; i > 0; i--)
				s_history[i] = s_history[i - 1];
			s_history[0] = sum;
			if (s_history_n < HISTORY)
				s_history_n++;
			s_result_time = 0.0f;
			char what[48];
			if (point_before == 0 && s_game.point)
				snprintf(what, sizeof what, "Sale %d: es el punto", sum);
			else if (point_before && s_game.point == 0)
				snprintf(what, sizeof what, sum == 7 ? "Siete: se acaba la mano" : "¡%d! Sale el punto", sum);
			else
				snprintf(what, sizeof what, "Sale %d", sum);
			if (back || s_game.last_lost) {
				profile_record_game(GAME_CRAPS, back > s_game.last_lost);
				audio_play(back > s_game.last_lost ? SFX_WIN : back ? SFX_CHIP : SFX_LOSE);
				if (back && s_game.last_lost)
					snprintf(s_message, sizeof s_message, "%s. Cobras %d, pierdes %d", what, back, s_game.last_lost);
				else if (back)
					snprintf(s_message, sizeof s_message, "%s. Cobras %d", what, back);
				else
					snprintf(s_message, sizeof s_message, "%s. Pierdes %d", what, s_game.last_lost);
			} else {
				audio_play(SFX_CARD);
				snprintf(s_message, sizeof s_message, "%s", what);
			}
		}
		return SCENE_CRAPS;
	}
	casino_pick_update(&s_chip, in, PICK_Y);
	if (in->touch_down)
		for (int b = 0; b < CR_BETS; b++) {
			float x, y, w, h;
			area(b, &x, &y, &w, &h);
			if (input_touch_in(in, x, y, w, h)) {
				if (!cr_can_bet(&s_game, b)) {
					audio_play(SFX_ERROR);
					snprintf(s_message, sizeof s_message, "Al %s solo se apuesta antes de la salida", NAMES[b]);
				} else
					casino_place(&s_game.bets[b], casino_chip_value(s_chip));
			}
		}
	if (button_update(&s_btn_clear, in)) {
		for (int b = 0; b < CR_BETS; b++)
			if (cr_can_bet(&s_game, b) && s_game.bets[b]) {
				wallet_give(s_game.bets[b]);
				s_game.bets[b] = 0;
			}
		audio_play(SFX_CHIP);
	}
	if (button_update(&s_btn_roll, in)) {
		s_rolling = true;
		s_t = 0.0f;
		s_staked = staked;
		audio_play(SFX_FLIP);
	} else if (staked == 0 && wallet_chips() == 0 && (in->down & KEY_A))
		return SCENE_FICHAS;
	return SCENE_CRAPS;
}

static void cr_draw_top(void)
{
	gfx_background(BG_TABLE_TOP);
	/* Paño con los números del punto */
	gfx_rrect(20, 20, 360, 60, 10, DEPTH_TABLE, C2D_Color32(230, 220, 190, 255));
	static const int POINTS[6] = { 4, 5, 6, 8, 9, 10 };
	for (int i = 0; i < 6; i++) {
		float x = 50 + i * 60;
		gfx_rect(x - 28, 23, 56, 54, DEPTH_TABLE, C2D_Color32(20, 110, 55, 255));
		gfx_textf(x, 34, DEPTH_TABLE, 0.85f, COLOR_CREAM, C2D_AlignCenter, POINTS[i] == 6 ? "SEIS" : POINTS[i] == 9 ? "NUEVE" : "%d",
			POINTS[i]);
		if (s_game.point == POINTS[i]) {
			gfx_ellipse(x, 30, 14, 14, DEPTH_UI, COLOR_WHITE);
			gfx_text("ON", x, 22, DEPTH_UI, 0.5f, COLOR_BLACK, C2D_AlignCenter);
		}
	}
	if (s_game.point == 0) {
		gfx_ellipse(360, 98, 14, 14, DEPTH_UI, C2D_Color32(30, 30, 30, 255));
		gfx_text("OFF", 360, 91, DEPTH_UI, 0.4f, COLOR_WHITE, C2D_AlignCenter);
	}
	/* Dados */
	for (int k = 0; k < 2; k++) {
		float x = 165 + k * 70, y = 150;
		float a = s_rolling ? sinf(s_t * 20 + k) * 0.5f : 0.0f;
		if (s_rolling) {
			x += sinf(s_t * 9 + k * 2) * 30 * (1 - s_t);
			y += -fabsf(sinf(s_t * 12 + k)) * 40 * (1 - s_t);
		}
		(void)a;
		gfx_ellipse(x + 3, 182, 24, 6, DEPTH_TABLE, C2D_Color32(0, 0, 0, 80));
		gfx_piece(PIECE_DIE_FIRST + s_face[k] - 1, x, y, 1.4f, 1.4f, DEPTH_LIFTED);
	}
	if (!s_rolling && s_history_n)
		gfx_textf(200, 196, DEPTH_UI, 0.6f, COLOR_GOLD, C2D_AlignCenter, "%d", s_face[0] + s_face[1]);
	gfx_text(s_game.point ? "Hay punto: sigue tirando" : "Tirada de salida", 20, 90, DEPTH_UI, 0.45f, COLOR_CREAM, C2D_AlignLeft);
	for (int i = 0; i < s_history_n; i++)
		gfx_textf(388 - i * 26, 220, DEPTH_UI, 0.42f, i ? COLOR_CREAM : COLOR_GOLD, C2D_AlignRight, "%d", s_history[i]);
}

static void cr_draw_bottom(void)
{
	gfx_background(BG_TABLE_BOTTOM);
	gfx_text(s_message, 6, 4, 0, 0.42f, COLOR_GOLD, C2D_AlignLeft);
	button_draw(&s_btn_menu, false);
	for (int b = 0; b < CR_BETS; b++) {
		float x, y, w, h;
		area(b, &x, &y, &w, &h);
		bool open = cr_can_bet(&s_game, b);
		gfx_rrect(x, y, w, h, 6, 0, C2D_Color32(230, 220, 190, 255));
		gfx_rrect(x + 2, y + 2, w - 4, h - 4, 5, 0, open ? C2D_Color32(20, 110, 55, 255) : C2D_Color32(40, 80, 55, 255));
		gfx_text(NAMES[b], x + 8, y + 4, 0, 0.52f, open ? COLOR_CREAM : C2D_Color32(170, 180, 170, 255), C2D_AlignLeft);
		gfx_text(PAYS[b], x + 8, y + 19, 0, 0.36f, C2D_Color32(220, 230, 200, 255), C2D_AlignLeft);
		casino_chip(s_game.bets[b], x + w - 20, y + h / 2, 1.0f, 0);
	}
	casino_pick_draw(s_chip, PICK_Y, casino_sum(s_game.bets, CR_BETS));
	button_draw(&s_btn_clear, false);
	button_draw(&s_btn_roll, s_btn_roll.enabled);
}

static void scene_leave(void)
{
	/* Las apuestas que quedan en la mesa vuelven al monedero */
	casino_refund(s_game.bets, CR_BETS);
	s_rolling = false;
	s_started = false;
}

const Scene scene_craps = { cr_enter, cr_update, cr_draw_top, cr_draw_bottom, scene_leave };
