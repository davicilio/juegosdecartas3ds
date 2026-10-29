/* Pocha: di cuántas bazas vas a hacer... y hazlas justas. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "games/pocha.h"
#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/gfx.h"
#include "ui/table.h"
#include "ui/widgets.h"

static const float TRICK_X[PO_PLAYERS] = { 200, 156, 200, 244 };
static const float TRICK_Y[PO_PLAYERS] = { 150, 116, 82, 116 };

#define HAND_SC 0.62f

static Pocha s_game;
static Table s_table;
static bool s_started;
static int s_cursor, s_bid;
static float s_cpu_timer, s_trick_timer, s_end_time;
static PoPhase s_prev_phase;
static Button s_btn_main, s_btn_alt, s_btn_menu;

static void start_round(void)
{
	s_cursor = 0;
	s_bid = 0;
	s_cpu_timer = s_trick_timer = s_end_time = 0.0f;
	s_prev_phase = s_game.phase;
	table_message(&s_table, "Mano %d de %d: %d carta%s. Triunfo: %s", s_game.round, PO_ROUNDS, s_game.round,
		s_game.round == 1 ? "" : "s", suit_name((Suit)s_game.trump));
}

static void po_enter(void)
{
	if (!s_started) {
		table_init(&s_table, CPU_CARMEN, CPU_RAMON, CPU_LOLA);
		po_init(&s_game, (uint32_t)time(NULL) ^ 0x90C4);
		s_started = true;
		start_round();
	}
	button_set(&s_btn_menu, 258, 6, 54, 20, "Menú", C2D_Color32(120, 30, 35, 255), KEY_START);
}

static void play(int i)
{
	po_play(&s_game, i);
	audio_play(SFX_CARD);
	if (s_game.phase == PO_TRICK_DONE)
		s_trick_timer = 0.0f;
}

static SceneId po_update(const Input* in, float dt)
{
	if (button_update(&s_btn_menu, in))
		return SCENE_MENU;
	const float y = 205, h = 28;
	int n = s_game.hand_count[0];
	switch (s_game.phase) {
	case PO_BIDDING: {
		static char label[32];
		snprintf(label, sizeof label, GLYPH_A " Digo %d", s_bid);
		button_set(&s_btn_main, 112, y, 96, h, label, C2D_Color32(40, 140, 60, 255), KEY_A);
		button_set(&s_btn_alt, 12, y, 92, h, GLYPH_L " Menos", C2D_Color32(60, 90, 160, 255), KEY_L);
		static Button more;
		button_set(&more, 216, y, 92, h, GLYPH_R " Más", C2D_Color32(60, 90, 160, 255), KEY_R);
		bool mine = s_game.current == 0;
		s_btn_main.enabled = mine && po_can_bid(&s_game, s_bid);
		s_btn_alt.enabled = mine;
		if (mine) {
			if (button_update(&s_btn_alt, in) || (in->repeat & KEY_LEFT))
				s_bid = s_bid > 0 ? s_bid - 1 : 0;
			if (button_update(&more, in) || (in->repeat & KEY_RIGHT))
				s_bid = s_bid < s_game.round ? s_bid + 1 : s_game.round;
			if (button_update(&s_btn_main, in)) {
				po_bid(&s_game, s_bid);
				table_bubble(&s_table, 0, s_bid == 1 ? "1 baza" : "Bazas dichas");
				table_message(&s_table, "Dices que harás %d", s_bid);
			}
		} else {
			s_cpu_timer += dt;
			if (s_cpu_timer > 0.8f) {
				s_cpu_timer = 0.0f;
				int p = s_game.current;
				int b = po_ai_bid(&s_game);
				po_bid(&s_game, b);
				char buf[24];
				snprintf(buf, sizeof buf, b == 1 ? "1 baza" : "%d bazas", b);
				table_bubble(&s_table, p, buf);
			}
		}
		break;
	}
	case PO_PLAYING:
		button_set(&s_btn_main, 12, y, 296, h, GLYPH_A " Echar", C2D_Color32(40, 140, 60, 255), KEY_A);
		s_btn_main.enabled = s_game.current == 0;
		if (s_game.current == 0 && n > 0) {
			if (in->repeat & KEY_LEFT)
				s_cursor = (s_cursor + n - 1) % n;
			if (in->repeat & KEY_RIGHT)
				s_cursor = (s_cursor + 1) % n;
			if (s_cursor >= n)
				s_cursor = n - 1;
			if (!po_can_play(&s_game, 0, s_cursor))
				for (int i = 0; i < n; i++)
					if (po_can_play(&s_game, 0, i)) {
						s_cursor = i;
						break;
					}
			int t = hand_touched(in, n, HAND_SC);
			int pick = t >= 0 ? t : button_update(&s_btn_main, in) ? s_cursor : -1;
			if (pick >= 0) {
				if (po_can_play(&s_game, 0, pick))
					play(pick);
				else {
					audio_play(SFX_ERROR);
					table_message(&s_table, "Hay que asistir, matar o fallar");
				}
			}
		} else if (s_game.current != 0) {
			s_cpu_timer += dt;
			if (s_cpu_timer > 0.7f) {
				s_cpu_timer = 0.0f;
				play(po_ai_choose(&s_game, s_game.current));
			}
		}
		break;
	case PO_TRICK_DONE:
		s_btn_main.enabled = false;
		s_trick_timer += dt;
		if (s_trick_timer > 1.0f) {
			int w = s_game.trick_winner;
			po_collect(&s_game);
			table_message(&s_table, "%s la baza", w == 0 ? "Te llevas" : table_name(&s_table, w));
		}
		break;
	case PO_ROUND_OVER:
	case PO_GAME_OVER: {
		bool over = s_game.phase == PO_GAME_OVER;
		button_set(&s_btn_main, 12, y, 144, h, over ? GLYPH_A " Otra partida" : GLYPH_A " Siguiente mano",
			C2D_Color32(40, 140, 60, 255), KEY_A);
		button_set(&s_btn_alt, 164, y, 144, h, GLYPH_B " Menú", C2D_Color32(190, 60, 50, 255), KEY_B);
		s_btn_main.enabled = s_btn_alt.enabled = s_end_time > 0.6f;
		s_end_time += dt;
		if (button_update(&s_btn_main, in)) {
			if (over)
				po_init(&s_game, (uint32_t)time(NULL));
			else
				po_new_round(&s_game);
			start_round();
		} else if (button_update(&s_btn_alt, in)) {
			return SCENE_MENU;
		}
		break;
	}
	}
	if (s_game.phase != s_prev_phase) {
		if (s_game.phase == PO_PLAYING)
			table_message(&s_table, "Sale %s", s_game.current == 0 ? "tú" : table_name(&s_table, s_game.current));
		if (s_game.phase == PO_ROUND_OVER || s_game.phase == PO_GAME_OVER) {
			s_end_time = 0.0f;
			for (int p = 0; p < PO_PLAYERS; p++)
				table_mood(&s_table, p, s_game.round_points[p] > 0 ? EXPR_HAPPY : EXPR_SAD, 2.5f);
			audio_play(s_game.round_points[0] > 0 ? SFX_WIN : SFX_LOSE);
			if (s_game.phase == PO_GAME_OVER)
				profile_record_game(GAME_POCHA, po_winner(&s_game) == 0);
		}
		s_prev_phase = s_game.phase;
	}
	table_update(&s_table, dt);
	return SCENE_POCHA;
}

static void po_draw_top(void)
{
	gfx_background(BG_TABLE_TOP);
	for (int p = 1; p < PO_PLAYERS; p++) {
		char info[32];
		if (s_game.bid[p] >= 0)
			snprintf(info, sizeof info, "%d pts · %d/%d", s_game.score[p], s_game.tricks_won[p], s_game.bid[p]);
		else
			snprintf(info, sizeof info, "%d puntos", s_game.score[p]);
		bool turn = s_game.current == p && (s_game.phase == PO_PLAYING || s_game.phase == PO_BIDDING);
		table_draw_seat(&s_table, p, s_game.hand_count[p], turn, info);
	}
	if (s_game.phase == PO_PLAYING || s_game.phase == PO_TRICK_DONE)
		for (int k = 0; k < s_game.played_in_trick; k++) {
			int p = (s_game.leader + k) % PO_PLAYERS;
			gfx_card(s_game.trick[p], TRICK_X[p], TRICK_Y[p], 0.6f, 1.0f, (p - 1.5f) * 0.05f, DEPTH_CARDS + k * 0.1f,
				0.0f, true);
		}
	/* Triunfo */
	gfx_card(s_game.trump_card, 340, 190, 0.5f, 1.0f, 0.2f, DEPTH_CARDS, 0.0f, true);
	gfx_textf(340, 222, DEPTH_UI, 0.42f, COLOR_GOLD, C2D_AlignCenter, "Triunfo: %s", suit_name((Suit)s_game.trump));
	char me[48];
	if (s_game.bid[0] >= 0)
		snprintf(me, sizeof me, "%d puntos · bazas %d de %d", s_game.score[0], s_game.tricks_won[0], s_game.bid[0]);
	else
		snprintf(me, sizeof me, "%d puntos", s_game.score[0]);
	table_draw_me(&s_table, me);
	table_draw_bubbles(&s_table);
	if ((s_game.phase == PO_ROUND_OVER || s_game.phase == PO_GAME_OVER) && banner_draw(s_end_time, 200, 112, 340, 96)) {
		if (s_game.phase == PO_GAME_OVER) {
			int w = po_winner(&s_game);
			if (w == 0)
				gfx_text("¡Ganas la partida!", 200, 70, DEPTH_BANNER, 0.65f, COLOR_GOLD, C2D_AlignCenter);
			else
				gfx_textf(200, 70, DEPTH_BANNER, 0.6f, COLOR_RED, C2D_AlignCenter, "Gana %s", table_name(&s_table, w));
		} else {
			gfx_textf(200, 70, DEPTH_BANNER, 0.6f, COLOR_GOLD, C2D_AlignCenter, "Fin de la mano %d", s_game.round);
		}
		gfx_textf(200, 100, DEPTH_BANNER, 0.45f, COLOR_CREAM, C2D_AlignCenter, "Esta mano: tú %+d · %+d · %+d · %+d",
			s_game.round_points[0], s_game.round_points[1], s_game.round_points[2], s_game.round_points[3]);
		gfx_textf(200, 122, DEPTH_BANNER, 0.45f, COLOR_GOLD, C2D_AlignCenter, "Total: tú %d · %d · %d · %d",
			s_game.score[0], s_game.score[1], s_game.score[2], s_game.score[3]);
	}
}

static void po_draw_bottom(void)
{
	gfx_background(BG_TABLE_BOTTOM);
	table_draw_message(&s_table);
	button_draw(&s_btn_menu, false);
	int n = s_game.hand_count[0];
	bool mine_play = s_game.phase == PO_PLAYING && s_game.current == 0;
	bool playable[PO_ROUNDS];
	for (int i = 0; i < n; i++)
		playable[i] = !mine_play || po_can_play(&s_game, 0, i);
	if (s_game.phase == PO_BIDDING && s_game.current == 0) {
		gfx_textf(BOTTOM_W / 2, 30, 0, 0.5f, COLOR_GOLD, C2D_AlignCenter, "¿Cuántas bazas vas a hacer? %d", s_bid);
		int f = po_forbidden_bid(&s_game);
		if (f >= 0)
			gfx_textf(BOTTOM_W / 2, 172, 0, 0.42f, C2D_Color32(255, 190, 180, 255), C2D_AlignCenter,
				"Repartes tú: no puedes decir %d", f);
	} else if (mine_play) {
		gfx_text("¡Tu turno!", BOTTOM_W / 2, 30, 0, 0.55f, COLOR_GOLD, C2D_AlignCenter);
	}
	hand_draw(s_game.hands[0], n, HAND_SC, NULL, mine_play ? s_cursor : -1, playable);
	if (s_game.phase != PO_BIDDING || s_game.current != 0)
		gfx_text("Acierta tus bazas: 10 + 5 por baza", BOTTOM_W / 2, 172, 0, 0.42f, C2D_Color32(200, 230, 200, 255),
			C2D_AlignCenter);
	button_draw(&s_btn_main, s_btn_main.enabled);
	if (s_game.phase == PO_BIDDING && s_game.current == 0) {
		button_draw(&s_btn_alt, true);
		Button more;
		button_set(&more, 216, 205, 92, 28, GLYPH_R " Más", C2D_Color32(60, 90, 160, 255), KEY_R);
		button_draw(&more, true);
	} else if (s_game.phase >= PO_ROUND_OVER) {
		button_draw(&s_btn_alt, s_btn_alt.enabled);
	}
}

const Scene scene_pocha = { po_enter, po_update, po_draw_top, po_draw_bottom };
