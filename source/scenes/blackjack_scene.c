/* Blackjack contra la banca, con la baraja francesa. */
#include <math.h>
#include <stdio.h>
#include <time.h>

#include "games/blackjack.h"
#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/gfx.h"
#include "ui/mii.h"
#include "ui/wallet.h"
#include "ui/widgets.h"

#define DEALER_Y 74.0f
#define PLAYER_Y 168.0f

static Blackjack s_game;
static bool s_started;
static bool s_on_table; /* hay fichas del monedero en la mesa */
static float s_dealer_timer, s_result_time;
static BjPhase s_prev_phase;
static Mood s_dealer_mood, s_my_mood;
static char s_message[96];
static Button s_b1, s_b2, s_b3, s_b4, s_btn_menu;

static int fr_id(PkCard c)
{
	return c.suit * 13 + c.rank - 2;
}

/* Fichas del monedero a la mesa */
static void buy_in(void)
{
	s_game.chips = wallet_take(BJ_START);
	bj_set_bet(&s_game, s_game.bet);
	if (s_game.chips < BJ_MIN_BET)
		s_game.phase = BJ_BROKE;
	s_on_table = true;
}

/* Devuelve al monedero las fichas de la mesa, también las apostadas en una
 * mano sin acabar (que se anula) */
static void cash_out(void)
{
	if (!s_on_table)
		return;
	int n = s_game.chips;
	if (s_game.phase == BJ_PLAYING || s_game.phase == BJ_DEALER)
		for (int k = 0; k < s_game.hand_count; k++)
			n += s_game.hands[k].bet;
	wallet_give(n);
	s_game.chips = 0;
	s_on_table = false;
}

static void bj_enter(void)
{
	if (!s_started) {
		bj_init(&s_game, (uint32_t)time(NULL) ^ 0xB1AC);
		buy_in();
		s_started = true;
		snprintf(s_message, sizeof s_message, "Elige tu apuesta y reparte");
	}
	s_prev_phase = s_game.phase;
	button_set(&s_btn_menu, 258, 6, 54, 20, "Menú", C2D_Color32(120, 30, 35, 255), KEY_START);
}

static void announce_result(void)
{
	int net = s_game.net;
	bool bj = s_game.hand_count == 1 && bj_is_blackjack(&s_game.hands[0]);
	if (net > 0)
		snprintf(s_message, sizeof s_message, bj ? "¡Blackjack! Ganas %d" : "¡Ganas %d fichas!", net);
	else if (net < 0)
		snprintf(s_message, sizeof s_message, "Pierdes %d fichas", -net);
	else
		snprintf(s_message, sizeof s_message, "Empate: recuperas la apuesta");
	audio_play(net > 0 ? SFX_WIN : net < 0 ? SFX_LOSE : SFX_CHIP);
	mood_set(&s_my_mood, net > 0 ? EXPR_HAPPY : net < 0 ? EXPR_SAD : EXPR_NORMAL, 2.5f);
	mood_set(&s_dealer_mood, net > 0 ? EXPR_SAD : net < 0 ? EXPR_HAPPY : EXPR_NORMAL, 2.5f);
	profile_record_game(GAME_BLACKJACK, net > 0);
}

static SceneId bj_update(const Input* in, float dt)
{
	if (button_update(&s_btn_menu, in))
		return SCENE_MENU;
	const float y = 205, h = 28;
	static char deal_label[24];
	switch (s_game.phase) {
	case BJ_BETTING:
	case BJ_RESULT:
		snprintf(deal_label, sizeof deal_label, GLYPH_A " Repartir (%d)", s_game.bet);
		button_set(&s_b1, 8, y, 150, h, deal_label, C2D_Color32(40, 140, 60, 255), KEY_A);
		button_set(&s_b2, 164, y, 48, h, "-10", C2D_Color32(60, 90, 160, 255), KEY_L);
		button_set(&s_b3, 216, y, 48, h, "+10", C2D_Color32(60, 90, 160, 255), KEY_R);
		button_set(&s_b4, 268, y, 48, h, "+100", C2D_Color32(200, 140, 30, 255), KEY_X);
		s_b1.enabled = s_b2.enabled = s_b3.enabled = s_b4.enabled = true;
		if (button_update(&s_b2, in))
			bj_set_bet(&s_game, s_game.bet - 10);
		else if (button_update(&s_b3, in))
			bj_set_bet(&s_game, s_game.bet + 10);
		else if (button_update(&s_b4, in))
			bj_set_bet(&s_game, s_game.bet + 100);
		else if (button_update(&s_b1, in) && bj_deal(&s_game)) {
			audio_play(SFX_CARD);
			snprintf(s_message, sizeof s_message, "¿Pides carta o te plantas?");
		}
		break;
	case BJ_PLAYING:
		button_set(&s_b1, 4, y, 76, h, GLYPH_A " Pedir", C2D_Color32(40, 140, 60, 255), KEY_A);
		button_set(&s_b2, 82, y, 78, h, GLYPH_B " Planto", C2D_Color32(190, 60, 50, 255), KEY_B);
		button_set(&s_b3, 162, y, 76, h, GLYPH_X " Doblar", C2D_Color32(200, 140, 30, 255), KEY_X);
		button_set(&s_b4, 240, y, 76, h, GLYPH_Y " Separar", C2D_Color32(60, 90, 160, 255), KEY_Y);
		s_b1.enabled = s_b2.enabled = true;
		s_b3.enabled = bj_can_double(&s_game);
		s_b4.enabled = bj_can_split(&s_game);
		if (button_update(&s_b1, in)) {
			bj_hit(&s_game);
			audio_play(SFX_CARD);
		} else if (button_update(&s_b2, in)) {
			bj_stand(&s_game);
		} else if (button_update(&s_b3, in)) {
			bj_double(&s_game);
			audio_play(SFX_CHIP);
		} else if (button_update(&s_b4, in)) {
			bj_split(&s_game);
			audio_play(SFX_CHIP);
		}
		break;
	case BJ_DEALER:
		s_b1.enabled = s_b2.enabled = s_b3.enabled = s_b4.enabled = false;
		s_dealer_timer += dt;
		if (s_dealer_timer > 0.7f) {
			s_dealer_timer = 0.0f;
			if (bj_dealer_step(&s_game))
				audio_play(SFX_CARD);
		}
		break;
	case BJ_BROKE:
		/* Lo poco que quede (menos que la apuesta mínima) vuelve al monedero */
		cash_out();
		button_set(&s_b1, 12, y, 144, h,
			wallet_chips() >= BJ_MIN_BET ? GLYPH_A " Empezar de nuevo" : GLYPH_A " Comprar fichas",
			C2D_Color32(40, 140, 60, 255), KEY_A);
		button_set(&s_b2, 164, y, 144, h, GLYPH_B " Menú", C2D_Color32(190, 60, 50, 255), KEY_B);
		s_b1.enabled = s_b2.enabled = true;
		if (button_update(&s_b1, in)) {
			if (wallet_chips() < BJ_MIN_BET)
				return SCENE_FICHAS;
			bj_reset(&s_game);
			buy_in();
			snprintf(s_message, sizeof s_message, "Otras %d fichas. ¡Suerte!", s_game.chips);
		} else if (button_update(&s_b2, in)) {
			return SCENE_MENU;
		}
		break;
	}
	if (s_game.phase != s_prev_phase) {
		if ((s_game.phase == BJ_RESULT || s_game.phase == BJ_BROKE) && s_prev_phase != BJ_BETTING &&
			s_prev_phase != BJ_RESULT) {
			s_result_time = 0.0f;
			announce_result();
		} else if (s_game.phase == BJ_DEALER) {
			snprintf(s_message, sizeof s_message, "Juega la banca");
			s_dealer_timer = 0.0f;
		}
		s_prev_phase = s_game.phase;
	}
	s_result_time += dt;
	mood_update(&s_dealer_mood, dt);
	mood_update(&s_my_mood, dt);
	return SCENE_BLACKJACK;
}

static void draw_hand(const BjHand* h, float cx, float y, bool hide_second, bool active)
{
	float step = 26.0f;
	float x0 = cx - (h->n - 1) * step / 2.0f;
	for (int i = 0; i < h->n; i++)
		gfx_french(i == 1 && hide_second ? FRENCH_BACK : fr_id(h->cards[i]), x0 + i * step, y, 0.68f, 0.0f,
			DEPTH_CARDS + i * 0.08f + (active ? 0.4f : 0.0f), i == 0);
	if (!hide_second) {
		bool soft;
		int total = bj_total(h, &soft);
		gfx_textf(cx, y + 36, DEPTH_UI, 0.5f, total > 21 ? COLOR_RED : COLOR_GOLD, C2D_AlignCenter,
			total > 21 ? "%d: ¡te pasas!" : soft && total < 21 ? "%d (blanda)" : "%d", total);
	}
}

static void bj_draw_top(void)
{
	gfx_background(BG_TABLE_TOP);
	/* La banca */
	mii_draw(cpu_avatar(CPU_RAMON), 64, 44, 18, DEPTH_UI + 0.4f, mood_expr(&s_dealer_mood), false);
	gfx_panel(24, 66, 80, 20, DEPTH_UI, C2D_Color32(10, 50, 25, 220), C2D_Color32(190, 60, 50, 255));
	gfx_text("La banca", 64, 68, DEPTH_UI, 0.45f, COLOR_CREAM, C2D_AlignCenter);
	if (s_game.dealer.n > 0)
		draw_hand(&s_game.dealer, 220, DEALER_Y, s_game.phase == BJ_PLAYING, false);
	gfx_text("La banca pide hasta 17 · Blackjack paga 3 a 2", 200, 8, DEPTH_UI, 0.42f, C2D_Color32(200, 230, 200, 255),
		C2D_AlignCenter);
	/* Tus manos */
	for (int k = 0; k < s_game.hand_count; k++) {
		float cx = s_game.hand_count == 1 ? 220 : 160 + k * 150.0f;
		bool active = s_game.phase == BJ_PLAYING && k == s_game.active;
		if (active && s_game.hand_count > 1)
			gfx_rrect(cx - 70, PLAYER_Y - 40, 140, 96, 8, DEPTH_TABLE, C2D_Color32(255, 215, 90, 70));
		draw_hand(&s_game.hands[k], cx, PLAYER_Y, false, active);
		gfx_textf(cx, PLAYER_Y - 48, DEPTH_UI, 0.42f, COLOR_CREAM, C2D_AlignCenter, "Apuesta %d%s", s_game.hands[k].bet,
			s_game.hands[k].doubled ? " (doblada)" : "");
	}
	mii_draw(&profile_get()->avatar, 30, 212, 15, DEPTH_UI + 0.4f, mood_expr(&s_my_mood), false);
	gfx_textf(52, 204, DEPTH_UI, 0.5f, COLOR_GOLD, C2D_AlignLeft, "Fichas: %d", s_game.chips);
}

static void bj_draw_bottom(void)
{
	gfx_background(BG_TABLE_BOTTOM);
	gfx_text(s_message, 10, 10, 0, 0.5f, COLOR_GOLD, C2D_AlignLeft);
	button_draw(&s_btn_menu, false);
	gfx_textf(BOTTOM_W / 2, 50, 0, 0.6f, COLOR_CREAM, C2D_AlignCenter, "Fichas: %d", s_game.chips);
	if (s_game.phase == BJ_BETTING || s_game.phase == BJ_RESULT) {
		gfx_textf(BOTTOM_W / 2, 84, 0, 0.7f, COLOR_GOLD, C2D_AlignCenter, "Apuesta: %d", s_game.bet);
		gfx_chip_stack(s_game.bet, BOTTOM_W / 2, 150, 0);
	} else if (s_game.phase == BJ_PLAYING) {
		const BjHand* h = &s_game.hands[s_game.active];
		gfx_textf(BOTTOM_W / 2, 84, 0, 0.7f, COLOR_GOLD, C2D_AlignCenter, "Tienes %d", bj_total(h, NULL));
		char a = bj_advice(&s_game);
		const char* advice = a == 'H' ? "pedir carta" : a == 'S' ? "plantarte" : a == 'D' ? "doblar" : "separar";
		gfx_textf(BOTTOM_W / 2, 120, 0, 0.45f, C2D_Color32(200, 230, 200, 255), C2D_AlignCenter,
			"La estrategia básica dice: %s", advice);
		if (s_game.hand_count > 1)
			gfx_textf(BOTTOM_W / 2, 140, 0, 0.45f, COLOR_CREAM, C2D_AlignCenter, "Juegas la mano %d de 2",
				s_game.active + 1);
	}
	if (s_game.phase != BJ_DEALER) {
		button_draw(&s_b1, s_b1.enabled);
		button_draw(&s_b2, s_b2.enabled);
		if (s_game.phase != BJ_BROKE) {
			button_draw(&s_b3, s_b3.enabled);
			button_draw(&s_b4, s_b4.enabled);
		}
	}
}

/* Salir a medias desde la pausa: la próxima vez se empieza de cero */
static void scene_leave(void)
{
	cash_out();
	s_started = false;
}

const Scene scene_blackjack = { bj_enter, bj_update, bj_draw_top, bj_draw_bottom, scene_leave };
