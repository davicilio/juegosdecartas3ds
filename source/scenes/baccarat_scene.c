/* Punto y banca: las manos arriba y las apuestas abajo. */
#include <math.h>
#include <stdio.h>
#include <time.h>

#include "games/baccarat.h"
#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/casino.h"
#include "ui/gfx.h"
#include "ui/mii.h"
#include "ui/wallet.h"
#include "ui/widgets.h"

#define ROAD    18
#define PICK_Y  178.0f
#define CARD_SC 0.72f

typedef enum { PH_BET, PH_DEAL, PH_RESULT } Phase;

static Baccarat s_game;
static bool s_started;
static Phase s_phase;
static int s_bets[BC_BETS], s_last[BC_BETS];
static int s_chip = 1, s_staked, s_back;
static int s_road[ROAD], s_road_n;
static int s_shown; /* cartas ya destapadas, en el orden del reparto */
static float s_t, s_result_time;
static char s_message[80];
static Button s_btn_deal, s_btn_clear, s_btn_repeat, s_btn_menu;

static const char* const NAMES[BC_BETS] = { "Punto", "Banca", "Empate" };
static const char* const PAYS[BC_BETS] = { "paga 1 a 1", "1 a 1 (comisión 5 %)", "paga 8 a 1" };
static const u32 COLORS[BC_BETS] = { 0x2860C0, 0xC02830, 0x30A050 };

static void area(int b, float* x, float* y, float* w, float* h)
{
	/* Punto a la izquierda, empate en medio, banca a la derecha */
	static const float X[BC_BETS] = { 6, 214, 112 };
	*x = X[b];
	*y = 28;
	*w = b == BC_TIE ? 96 : 100;
	*h = 120;
}

static void bc_enter(void)
{
	if (!s_started) {
		s_started = true;
		bc_init(&s_game, (uint32_t)time(NULL) ^ 0xBACC);
		for (int i = 0; i < BC_BETS; i++)
			s_bets[i] = s_last[i] = 0;
		s_phase = PH_BET;
		s_road_n = 0;
		snprintf(s_message, sizeof s_message, "Apuesta a punto, a banca o al empate");
	}
	button_set(&s_btn_menu, 262, 2, 54, 18, "Menú", C2D_Color32(120, 30, 35, 255), KEY_START);
}

/* Orden del reparto: punto, banca, punto, banca, tercera del punto, tercera de la banca */
static int total_cards(void)
{
	return s_game.n[0] + s_game.n[1];
}

static bool card_shown(int hand, int i)
{
	int order;
	if (i < 2)
		order = i * 2 + hand;
	else
		order = hand == 0 ? 4 : 4 + (s_game.n[0] == 3);
	return order < s_shown;
}

static SceneId bc_update(const Input* in, float dt)
{
	if (button_update(&s_btn_menu, in))
		return SCENE_MENU;
	bool betting = s_phase == PH_BET;
	int staked = casino_sum(s_bets, BC_BETS);
	button_set(&s_btn_clear, 4, 205, 96, 28, GLYPH_X " Quitar", C2D_Color32(150, 60, 50, 255), KEY_X);
	button_set(&s_btn_repeat, 104, 205, 100, 28, GLYPH_Y " Repetir", C2D_Color32(60, 90, 160, 255), KEY_Y);
	button_set(&s_btn_deal, 208, 205, 108, 28, GLYPH_A " Repartir", C2D_Color32(40, 140, 60, 255), KEY_A);
	s_btn_clear.enabled = betting && staked > 0;
	s_btn_repeat.enabled = betting && casino_sum(s_last, BC_BETS) > 0;
	s_btn_deal.enabled = betting && staked > 0;
	switch (s_phase) {
	case PH_BET:
		casino_pick_update(&s_chip, in, PICK_Y);
		if (in->touch_down)
			for (int b = 0; b < BC_BETS; b++) {
				float x, y, w, h;
				area(b, &x, &y, &w, &h);
				if (input_touch_in(in, x, y, w, h))
					casino_place(&s_bets[b], casino_chip_value(s_chip));
			}
		if (button_update(&s_btn_clear, in)) {
			casino_refund(s_bets, BC_BETS);
			audio_play(SFX_CHIP);
		}
		if (button_update(&s_btn_repeat, in))
			casino_repeat(s_bets, s_last, BC_BETS);
		if (button_update(&s_btn_deal, in)) {
			s_staked = staked;
			for (int i = 0; i < BC_BETS; i++)
				s_last[i] = s_bets[i];
			bc_deal(&s_game);
			s_shown = 0;
			s_t = 0.3f;
			s_phase = PH_DEAL;
			snprintf(s_message, sizeof s_message, "Cartas...");
		} else if (staked == 0 && wallet_chips() == 0 && (in->down & KEY_A))
			return SCENE_FICHAS;
		break;
	case PH_DEAL:
		s_t -= dt;
		if (s_t <= 0) {
			if (s_shown < total_cards()) {
				s_shown++;
				audio_play(SFX_CARD);
				s_t = s_shown == 4 ? 0.9f : 0.6f;
			} else {
				s_back = bc_payout(&s_game, s_bets);
				if (s_back)
					wallet_give(s_back);
				for (int i = ROAD - 1; i > 0; i--)
					s_road[i] = s_road[i - 1];
				s_road[0] = s_game.winner;
				if (s_road_n < ROAD)
					s_road_n++;
				profile_record_game(GAME_BACCARAT, s_back > s_staked);
				audio_play(s_back > s_staked ? SFX_WIN : s_back > 0 ? SFX_CHIP : SFX_LOSE);
				const char* who = s_game.winner == BC_TIE ? "Empate" : s_game.winner == BC_PLAYER ? "Gana el punto" : "Gana la banca";
				if (s_back)
					snprintf(s_message, sizeof s_message, "%s. Cobras %d", who, s_back);
				else
					snprintf(s_message, sizeof s_message, "%s. Pierdes %d", who, s_staked);
				s_phase = PH_RESULT;
				s_result_time = 0.0f;
			}
		}
		break;
	case PH_RESULT:
		s_result_time += dt;
		if (s_result_time > 1.0f && (in->touch_down || (in->down & (KEY_A | KEY_B)))) {
			for (int i = 0; i < BC_BETS; i++)
				s_bets[i] = 0;
			s_phase = PH_BET;
			snprintf(s_message, sizeof s_message, wallet_chips() > 0 ? "Hagan sus apuestas" : "Sin fichas: " GLYPH_A " para comprar");
		}
		break;
	}
	return SCENE_BACCARAT;
}

static int fr_id(PkCard c)
{
	return c.suit * 13 + c.rank - 2;
}

static void draw_hand(int hand, float cx)
{
	const char* title = hand == 0 ? "PUNTO" : "BANCA";
	gfx_text(title, cx, 12, DEPTH_UI, 0.6f, gfx_rgb(COLORS[hand], 255), C2D_AlignCenter);
	if (s_phase == PH_BET)
		return;
	int n = s_game.n[hand], visible = 0;
	PkCard shown[3];
	for (int i = 0; i < n; i++) {
		float x = cx - (n - 1) * 26 + i * 52, y = 100;
		if (i == 2) {
			/* La tercera va de lado */
			x = cx;
			y = 168;
		}
		if (card_shown(hand, i)) {
			gfx_french(fr_id(s_game.hand[hand][i]), x, y, CARD_SC, i == 2 ? 1.5708f : 0.0f, DEPTH_CARDS, true);
			shown[visible++] = s_game.hand[hand][i];
		}
	}
	if (visible) {
		int t = bc_total(shown, visible);
		gfx_rrect(cx - 18, 34, 36, 24, 6, DEPTH_UI, C2D_Color32(0, 0, 0, 160));
		gfx_textf(cx, 35, DEPTH_UI, 0.7f, COLOR_GOLD, C2D_AlignCenter, "%d", t);
	}
}

static void bc_draw_top(void)
{
	gfx_background(BG_TABLE_TOP);
	draw_hand(0, 110);
	draw_hand(1, 290);
	/* Marcador de las últimas manos */
	for (int i = 0; i < s_road_n; i++) {
		float x = 389 - (i % 9) * 11, y = 6 + (i / 9) * 11;
		gfx_ellipse(x, y + 4, 4.5f, 4.5f, DEPTH_UI, gfx_rgb(COLORS[s_road[i]], 255));
	}
	if (s_phase == PH_RESULT && banner_draw(s_result_time, 200, 120, 200, 50)) {
		const char* who = s_game.winner == BC_TIE ? "¡Empate!" : s_game.winner == BC_PLAYER ? "Gana el punto" : "Gana la banca";
		gfx_text(who, 200, 104, DEPTH_BANNER, 0.62f, COLOR_GOLD, C2D_AlignCenter);
	}
}

static void bc_draw_bottom(void)
{
	gfx_background(BG_TABLE_BOTTOM);
	gfx_text(s_message, 6, 4, 0, 0.45f, COLOR_GOLD, C2D_AlignLeft);
	button_draw(&s_btn_menu, false);
	for (int b = 0; b < BC_BETS; b++) {
		float x, y, w, h;
		area(b, &x, &y, &w, &h);
		bool win = s_phase == PH_RESULT && s_game.winner == b && fmodf(gfx_time(), 0.6f) < 0.35f;
		gfx_rrect(x, y, w, h, 10, 0, win ? COLOR_GOLD : gfx_rgb(COLORS[b], 255));
		gfx_rrect(x + 3, y + 3, w - 6, h - 6, 8, 0, C2D_Color32(20, 100, 50, 255));
		gfx_text(NAMES[b], x + w / 2, y + 8, 0, 0.6f, COLOR_CREAM, C2D_AlignCenter);
		gfx_text_wrap(PAYS[b], x + 8, y + 30, 0, 0.38f, C2D_Color32(220, 230, 200, 255), w - 16);
		casino_chip(s_bets[b], x + w / 2, y + 88, 1.3f, 0);
	}
	casino_pick_draw(s_chip, PICK_Y, casino_sum(s_bets, BC_BETS));
	button_draw(&s_btn_clear, false);
	button_draw(&s_btn_repeat, false);
	button_draw(&s_btn_deal, s_btn_deal.enabled);
}

static void scene_leave(void)
{
	if (s_phase != PH_RESULT)
		casino_refund(s_bets, BC_BETS);
	s_started = false;
}

const Scene scene_baccarat = { bc_enter, bc_update, bc_draw_top, bc_draw_bottom, scene_leave };
