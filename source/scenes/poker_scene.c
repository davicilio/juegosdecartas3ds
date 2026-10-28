/* Póker Texas Hold'em con la baraja francesa: tú contra tres personajes. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "games/poker.h"
#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/gfx.h"
#include "ui/table.h"
#include "ui/widgets.h"

static Poker s_game;
static Table s_table;
static bool s_started;
static int s_raise_to;
static int s_turn_seen = -1; /* para preparar la subida al empezar tu turno */
static float s_cpu_timer, s_end_time;
static PkStage s_prev_stage;
static bool s_recorded;
static Button s_btn_fold, s_btn_call, s_btn_raise, s_btn_all, s_btn_menu;

static int fr_id(PkCard c)
{
	return c.suit * 13 + c.rank - 2;
}

static void describe_action(int p)
{
	const char* who = p == 0 ? "Tú" : table_name(&s_table, p);
	char bubble[28];
	switch (s_game.last_action[p]) {
	case PK_FOLD:
		snprintf(bubble, sizeof bubble, "Me retiro");
		table_message(&s_table, "%s %s", who, p == 0 ? "te retiras" : "se retira");
		break;
	case PK_CHECK_CALL:
		if (s_game.last_amount[p] == 0) {
			snprintf(bubble, sizeof bubble, "Paso");
			table_message(&s_table, "%s %s", who, p == 0 ? "pasas" : "pasa");
		} else {
			snprintf(bubble, sizeof bubble, "Igualo");
			table_message(&s_table, "%s %s %d", who, p == 0 ? "igualas" : "iguala", s_game.last_amount[p]);
		}
		break;
	case PK_RAISE:
		snprintf(bubble, sizeof bubble, "Subo a %d", s_game.bet[p]);
		table_message(&s_table, "%s %s a %d", who, p == 0 ? "subes" : "sube", s_game.bet[p]);
		break;
	default:
		snprintf(bubble, sizeof bubble, "¡Con todo!");
		table_message(&s_table, "%s %s con todo (%d)", who, p == 0 ? "vas" : "va", s_game.bet[p]);
		break;
	}
	table_bubble(&s_table, p, bubble);
	audio_play(s_game.last_action[p] == PK_FOLD ? SFX_SELECT : SFX_CHIP);
}

static void new_hand(void)
{
	pk_new_hand(&s_game);
	s_cpu_timer = s_end_time = 0.0f;
	s_prev_stage = s_game.stage;
	s_turn_seen = -1;
	table_message(&s_table, "Mano %d: reparte %s", s_game.hands_played,
		s_game.dealer == 0 ? "tú" : table_name(&s_table, s_game.dealer));
	audio_play(SFX_CARD);
}

static void new_game(void)
{
	pk_init(&s_game, (uint32_t)time(NULL) ^ 0x9043);
	s_recorded = false;
	s_cpu_timer = s_end_time = 0.0f;
	s_prev_stage = s_game.stage;
	s_turn_seen = -1;
	table_message(&s_table, "Cada uno empieza con %d fichas. Ciegas %d/%d", PK_START, PK_SMALL, PK_BIG);
}

static void poker_enter(void)
{
	if (!s_started) {
		table_init(&s_table, CPU_RAMON, CPU_LUCIA, CPU_MANOLO);
		new_game();
		s_started = true;
	}
	button_set(&s_btn_menu, 258, 6, 54, 20, "Menú", C2D_Color32(120, 30, 35, 255), KEY_START);
}

static void act(PkAction a, int amount)
{
	int p = s_game.current;
	pk_act(&s_game, a, amount);
	describe_action(p);
}

static SceneId poker_update(const Input* in, float dt)
{
	if (button_update(&s_btn_menu, in))
		return SCENE_MENU;
	const float y = 205, h = 28;
	if (!pk_hand_finished(&s_game)) {
		bool mine = s_game.current == 0;
		if (mine && s_turn_seen != s_game.hands_played * 10 + s_game.stage) {
			s_turn_seen = s_game.hands_played * 10 + s_game.stage;
			s_raise_to = pk_min_raise_to(&s_game);
		}
		int call = pk_to_call(&s_game, 0);
		/* Los botones guardan el puntero al texto: tiene que seguir vivo */
		static char call_label[24], raise_label[24];
		if (pk_can_check(&s_game) || !mine)
			snprintf(call_label, sizeof call_label, GLYPH_A " Paso");
		else
			snprintf(call_label, sizeof call_label, GLYPH_A " Igualo %d", call);
		snprintf(raise_label, sizeof raise_label, GLYPH_X " Subo a %d", s_raise_to);
		button_set(&s_btn_fold, 4, y, 70, h, GLYPH_B " Fuera", C2D_Color32(120, 120, 120, 255), KEY_B);
		button_set(&s_btn_call, 78, y, 86, h, call_label, C2D_Color32(40, 140, 60, 255), KEY_A);
		button_set(&s_btn_raise, 168, y, 96, h, raise_label, C2D_Color32(200, 140, 30, 255), KEY_X);
		button_set(&s_btn_all, 268, y, 48, h, GLYPH_Y " Todo", C2D_Color32(190, 60, 50, 255), KEY_Y);
		bool can_raise = mine && pk_max_raise_to(&s_game) > s_game.to_call;
		s_btn_fold.enabled = s_btn_call.enabled = s_btn_all.enabled = mine;
		s_btn_raise.enabled = can_raise;
		if (mine) {
			int lo = pk_min_raise_to(&s_game), hi = pk_max_raise_to(&s_game);
			if (in->repeat & (KEY_R | KEY_UP | KEY_RIGHT))
				s_raise_to += PK_BIG;
			if (in->repeat & (KEY_L | KEY_DOWN | KEY_LEFT))
				s_raise_to -= PK_BIG;
			if (s_raise_to < lo)
				s_raise_to = lo;
			if (s_raise_to > hi)
				s_raise_to = hi;
			if (button_update(&s_btn_fold, in)) {
				if (pk_can_check(&s_game))
					act(PK_CHECK_CALL, 0); /* retirarse sin tener que pagar no tiene sentido: se pasa */
				else
					act(PK_FOLD, 0);
			} else if (button_update(&s_btn_call, in)) {
				act(PK_CHECK_CALL, 0);
			} else if (button_update(&s_btn_raise, in)) {
				act(s_raise_to >= hi ? PK_ALL_IN : PK_RAISE, s_raise_to);
			} else if (button_update(&s_btn_all, in)) {
				act(PK_ALL_IN, 0);
			}
		} else {
			s_cpu_timer += dt;
			if (s_cpu_timer > 0.9f) {
				s_cpu_timer = 0.0f;
				int amount = 0;
				PkAction a = pk_ai_decide(&s_game, &amount);
				act(a, amount);
			}
		}
	} else {
		bool over = pk_game_over(&s_game);
		button_set(&s_btn_call, 12, y, 144, h, over ? GLYPH_A " Otra partida" : GLYPH_A " Siguiente mano",
			C2D_Color32(40, 140, 60, 255), KEY_A);
		button_set(&s_btn_fold, 164, y, 144, h, GLYPH_B " Menú", C2D_Color32(190, 60, 50, 255), KEY_B);
		s_btn_call.enabled = s_btn_fold.enabled = s_end_time > 0.8f;
		s_end_time += dt;
		if (button_update(&s_btn_call, in)) {
			if (over)
				new_game();
			else
				new_hand();
		} else if (button_update(&s_btn_fold, in)) {
			return SCENE_MENU;
		}
	}
	if (s_game.stage != s_prev_stage) {
		if (s_game.stage >= PK_FLOP && s_game.stage <= PK_RIVER)
			audio_play(SFX_FLIP);
		if (pk_hand_finished(&s_game)) {
			s_end_time = 0.0f;
			for (int p = 0; p < PK_PLAYERS; p++)
				if (s_game.won[p] > 0) {
					table_mood(&s_table, p, EXPR_HAPPY, 3.0f);
				} else if (s_game.invested[p] > PK_BIG) {
					table_mood(&s_table, p, EXPR_SAD, 2.0f);
				}
			audio_play(s_game.won[0] > 0 ? SFX_WIN : SFX_LOSE);
			if (pk_game_over(&s_game) && !s_recorded) {
				s_recorded = true;
				profile_record_game(GAME_POKER, s_game.chips[0] > 0);
			}
		}
		s_prev_stage = s_game.stage;
	}
	table_update(&s_table, dt);
	return SCENE_POKER;
}

static void poker_draw_top(void)
{
	gfx_background(BG_TABLE_TOP);
	bool reveal = s_game.stage == PK_SHOWDOWN || (s_game.stage == PK_GAME_OVER && s_game.board_count == 5);
	for (int p = 1; p < PK_PLAYERS; p++) {
		char info[32];
		if (s_game.out[p])
			snprintf(info, sizeof info, "Sin fichas");
		else if (s_game.folded[p])
			snprintf(info, sizeof info, "%d · fuera", s_game.chips[p]);
		else if (s_game.chips[p] == 0)
			snprintf(info, sizeof info, "Con todo");
		else if (s_game.bet[p] > 0)
			snprintf(info, sizeof info, "%d (+%d)", s_game.chips[p], s_game.bet[p]);
		else
			snprintf(info, sizeof info, "%d fichas", s_game.chips[p]);
		table_draw_seat(&s_table, p, 0, s_game.current == p && !pk_hand_finished(&s_game), info);
		/* Sus dos cartas */
		if (!s_game.out[p] && !s_game.folded[p]) {
			float x = table_seat_x(p) + (p == 2 ? 50 : 0), y = table_seat_y(p) + (p == 2 ? 0 : 84);
			for (int k = 0; k < 2; k++)
				gfx_french(reveal ? fr_id(s_game.hole[p][k]) : FRENCH_BACK, x + (k - 0.5f) * 20.0f, y, 0.45f,
					(k - 0.5f) * 0.1f, DEPTH_CARDS + k * 0.05f, k == 0);
			if (reveal && s_game.score[p])
				gfx_text(pk_category_name(pk_category(s_game.score[p])), x, y + 24, DEPTH_UI, 0.42f, COLOR_GOLD,
					C2D_AlignCenter);
		}
		if (p == s_game.dealer)
			gfx_text("(D)", table_seat_x(p) + (p == 2 ? -84 : 34), table_seat_y(p) - 14, DEPTH_UI, 0.45f, COLOR_GOLD,
				C2D_AlignCenter);
	}
	/* Cartas de la mesa */
	for (int i = 0; i < PK_BOARD; i++) {
		float x = 200 + (i - 2) * 44.0f;
		if (i < s_game.board_count)
			gfx_french(fr_id(s_game.board[i]), x, 120, 0.62f, 0.0f, DEPTH_CARDS, true);
		else
			gfx_rrect(x - 20 + gfx_dx(DEPTH_TABLE), 120 - 30, 40, 60, 5, DEPTH_TABLE, C2D_Color32(0, 0, 0, 50));
	}
	gfx_textf(200, 156, DEPTH_UI, 0.55f, COLOR_GOLD, C2D_AlignCenter, "Bote: %d", pk_pot(&s_game));
	char me[40];
	snprintf(me, sizeof me, "Tus fichas: %d%s", s_game.chips[0], s_game.dealer == 0 ? " (D)" : "");
	table_draw_me(&s_table, me);
	table_draw_bubbles(&s_table);

	if (pk_hand_finished(&s_game) && banner_draw(s_end_time, 200, 116, 340, 80)) {
		if (pk_game_over(&s_game)) {
			if (s_game.chips[0] > 0)
				gfx_text("¡Te has llevado todas las fichas!", 200, 90, DEPTH_BANNER, 0.6f, COLOR_GOLD, C2D_AlignCenter);
			else
				gfx_text("Te has quedado sin fichas", 200, 90, DEPTH_BANNER, 0.6f, COLOR_RED, C2D_AlignCenter);
		}
		/* Ganadores de la mano */
		int line = 0;
		for (int p = 0; p < PK_PLAYERS && line < 2; p++) {
			if (s_game.won[p] <= 0)
				continue;
			const char* hand = s_game.stage != PK_HAND_OVER && s_game.score[p]
			                       ? pk_category_name(pk_category(s_game.score[p]))
			                       : "los demás se retiran";
			gfx_textf(200, (pk_game_over(&s_game) ? 112 : 96) + line * 20, DEPTH_BANNER, 0.5f,
				p == 0 ? COLOR_GOLD : COLOR_CREAM, C2D_AlignCenter, "%s %s %d (%s)", p == 0 ? "Ganas" : table_name(&s_table, p),
				p == 0 ? "" : "gana", s_game.won[p], hand);
			line++;
		}
	}
}

static void poker_draw_bottom(void)
{
	gfx_background(BG_TABLE_BOTTOM);
	table_draw_message(&s_table);
	button_draw(&s_btn_menu, false);
	if (!s_game.out[0]) {
		for (int k = 0; k < 2; k++)
			gfx_french(fr_id(s_game.hole[0][k]), 120 + k * 70.0f, 96, 1.05f, (k - 0.5f) * 0.08f, 0,
				true);
		/* Tu mejor jugada ahora mismo */
		PkCard seven[7] = { s_game.hole[0][0], s_game.hole[0][1] };
		for (int i = 0; i < s_game.board_count; i++)
			seven[2 + i] = s_game.board[i];
		uint32_t sc = pk_evaluate(seven, 2 + s_game.board_count);
		gfx_textf(272, 70, 0, 0.45f, COLOR_CREAM, C2D_AlignCenter, "Tienes:");
		gfx_text(pk_category_name(pk_category(sc)), 272, 86, 0, 0.5f, COLOR_GOLD, C2D_AlignCenter);
		if (s_game.folded[0])
			gfx_text("(retirado)", 272, 104, 0, 0.45f, C2D_Color32(200, 180, 170, 255), C2D_AlignCenter);
	}
	bool mine = s_game.current == 0 && !pk_hand_finished(&s_game);
	if (mine) {
		gfx_text("¡Tu turno!", BOTTOM_W / 2, 30, 0, 0.55f, COLOR_GOLD, C2D_AlignCenter);
		gfx_textf(BOTTOM_W / 2, 168, 0, 0.42f, C2D_Color32(200, 230, 200, 255), C2D_AlignCenter,
			"Para igualar: %d · Subida con " GLYPH_L "/" GLYPH_R " o la cruceta", pk_to_call(&s_game, 0));
	}
	gfx_textf(BOTTOM_W / 2, 184, 0, 0.45f, COLOR_CREAM, C2D_AlignCenter, "Bote: %d · Tus fichas: %d", pk_pot(&s_game),
		s_game.chips[0]);
	if (!pk_hand_finished(&s_game)) {
		button_draw(&s_btn_fold, s_btn_fold.enabled);
		button_draw(&s_btn_call, s_btn_call.enabled);
		button_draw(&s_btn_raise, s_btn_raise.enabled);
		button_draw(&s_btn_all, s_btn_all.enabled);
	} else {
		button_draw(&s_btn_call, s_btn_call.enabled);
		button_draw(&s_btn_fold, s_btn_fold.enabled);
	}
}

const Scene scene_poker = { poker_enter, poker_update, poker_draw_top, poker_draw_bottom };
