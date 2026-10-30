/* Póker con la baraja francesa: Texas Hold'em, Omaha y de descarte, tú contra
 * tres personajes. */
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

static Poker s_games[PK_VARIANTS];
static int s_v; /* PkVariant */
#define G (&s_games[s_v])
static Table s_tables[PK_VARIANTS];
#define s_table s_tables[s_v]
static bool s_started[PK_VARIANTS];
static bool s_sel[PK_MAX_HOLE]; /* descarte: cartas marcadas */
static const SceneId SCENE_OF[PK_VARIANTS] = { SCENE_POKER, SCENE_OMAHA, SCENE_DRAWPOKER };
static const GameId GAME_OF[PK_VARIANTS] = { GAME_POKER, GAME_OMAHA, GAME_DRAWPOKER };
static int s_raise_to;
static int s_turn_seen = -1; /* para preparar la subida al empezar tu turno */
static float s_cpu_timer, s_end_time;
static PkStage s_prev_stage;
static bool s_recorded[PK_VARIANTS];
static Button s_btn_discard;
static Button s_btn_fold, s_btn_call, s_btn_raise, s_btn_all, s_btn_menu;

static int fr_id(PkCard c)
{
	return c.suit * 13 + c.rank - 2;
}

static void describe_action(int p)
{
	const char* who = p == 0 ? "Tú" : table_name(&s_table, p);
	char bubble[28];
	switch (G->last_action[p]) {
	case PK_FOLD:
		snprintf(bubble, sizeof bubble, "Me retiro");
		table_message(&s_table, "%s %s", who, p == 0 ? "te retiras" : "se retira");
		break;
	case PK_CHECK_CALL:
		if (G->last_amount[p] == 0) {
			snprintf(bubble, sizeof bubble, "Paso");
			table_message(&s_table, "%s %s", who, p == 0 ? "pasas" : "pasa");
		} else {
			snprintf(bubble, sizeof bubble, "Igualo");
			table_message(&s_table, "%s %s %d", who, p == 0 ? "igualas" : "iguala", G->last_amount[p]);
		}
		break;
	case PK_RAISE:
		snprintf(bubble, sizeof bubble, "Subo a %d", G->bet[p]);
		table_message(&s_table, "%s %s a %d", who, p == 0 ? "subes" : "sube", G->bet[p]);
		break;
	default:
		snprintf(bubble, sizeof bubble, "¡Con todo!");
		table_message(&s_table, "%s %s con todo (%d)", who, p == 0 ? "vas" : "va", G->bet[p]);
		break;
	}
	table_bubble(&s_table, p, bubble);
	audio_play(G->last_action[p] == PK_FOLD ? SFX_SELECT : SFX_CHIP);
}

static void new_hand(void)
{
	pk_new_hand(G);
	s_cpu_timer = s_end_time = 0.0f;
	s_prev_stage = G->stage;
	s_turn_seen = -1;
	table_message(&s_table, "Mano %d: reparte %s", G->hands_played,
		G->dealer == 0 ? "tú" : table_name(&s_table, G->dealer));
	audio_play(SFX_CARD);
}

static void new_game(void)
{
	pk_init_variant(G, (PkVariant)s_v, (uint32_t)time(NULL) ^ (0x9043 + s_v * 77));
	s_recorded[s_v] = false;
	s_cpu_timer = s_end_time = 0.0f;
	s_prev_stage = G->stage;
	s_turn_seen = -1;
	table_message(&s_table, "Cada uno empieza con %d fichas. Ciegas %d/%d", PK_START, PK_SMALL, PK_BIG);
}

static void enter_variant(int v)
{
	s_v = v;
	if (!s_started[v]) {
		if (v == PK_OMAHA)
			table_init(&s_table, CPU_CARMEN, CPU_PEPE, CPU_PACO);
		else if (v == PK_DRAW)
			table_init(&s_table, CPU_LOLA, CPU_MANOLO, CPU_RAMON);
		else
			table_init(&s_table, CPU_RAMON, CPU_LUCIA, CPU_MANOLO);
		new_game();
		s_started[v] = true;
	}
	button_set(&s_btn_menu, 258, 6, 54, 20, "Menú", C2D_Color32(120, 30, 35, 255), KEY_START);
}

static void poker_enter(void)
{
	enter_variant(PK_HOLDEM);
}

static void omaha_enter(void)
{
	enter_variant(PK_OMAHA);
}

static void draw_enter(void)
{
	enter_variant(PK_DRAW);
}

/* Cartas propias en la pantalla de abajo */
static float my_card_scale(void)
{
	return G->hole_n == 2 ? 1.05f : G->hole_n == 4 ? 0.78f : 0.7f;
}

static float my_card_x(int k)
{
	float step = G->hole_n == 2 ? 70.0f : G->hole_n == 4 ? 52.0f : 48.0f;
	float x0 = G->hole_n == 5 ? 130.0f : 120.0f;
	return x0 + (k - (G->hole_n - 1) / 2.0f) * step;
}

static void after_discard(int p)
{
	char buf[24];
	snprintf(buf, sizeof buf, G->discards[p] ? "Cambio %d" : "Servido", G->discards[p]);
	table_bubble(&s_table, p, buf);
	audio_play(SFX_CARD);
}

static void act(PkAction a, int amount)
{
	int p = G->current;
	pk_act(G, a, amount);
	describe_action(p);
}

static SceneId poker_update(const Input* in, float dt)
{
	if (button_update(&s_btn_menu, in))
		return SCENE_MENU;
	const float y = 205, h = 28;
	if (G->stage == PK_DISCARD) {
		if (G->current == 0) {
			int n = 0;
			for (int i = 0; i < 5; i++)
				n += s_sel[i];
			static char label[32];
			snprintf(label, sizeof label, n ? GLYPH_A " Cambiar %d" : GLYPH_A " Servido", n);
			button_set(&s_btn_discard, 60, 205, 200, 28, label, C2D_Color32(40, 140, 60, 255), KEY_A);
			s_btn_discard.enabled = pk_can_discard(G, s_sel);
			if (in->touch_down)
				for (int k = 0; k < 5; k++)
					if (fabsf(in->tx - my_card_x(k)) < CARD_W * my_card_scale() / 2 &&
						fabsf(in->ty - 96) < CARD_H * my_card_scale() / 2) {
						s_sel[k] = !s_sel[k];
						audio_play(SFX_SELECT);
					}
			if (button_update(&s_btn_discard, in) && pk_discard(G, s_sel)) {
				after_discard(0);
				for (int i = 0; i < 5; i++)
					s_sel[i] = false;
			}
		} else {
			s_cpu_timer += dt;
			if (s_cpu_timer > 0.8f) {
				s_cpu_timer = 0.0f;
				int p = G->current;
				bool sel[5];
				pk_ai_discard(G, p, sel);
				pk_discard(G, sel);
				after_discard(p);
			}
		}
	} else if (!pk_hand_finished(G)) {
		bool mine = G->current == 0;
		if (mine && s_turn_seen != G->hands_played * 10 + G->stage) {
			s_turn_seen = G->hands_played * 10 + G->stage;
			s_raise_to = pk_min_raise_to(G);
		}
		int call = pk_to_call(G, 0);
		/* Los botones guardan el puntero al texto: tiene que seguir vivo */
		static char call_label[24], raise_label[24];
		if (pk_can_check(G) || !mine)
			snprintf(call_label, sizeof call_label, GLYPH_A " Paso");
		else
			snprintf(call_label, sizeof call_label, GLYPH_A " Igualo %d", call);
		snprintf(raise_label, sizeof raise_label, GLYPH_X " Subo a %d", s_raise_to);
		button_set(&s_btn_fold, 4, y, 70, h, GLYPH_B " Fuera", C2D_Color32(120, 120, 120, 255), KEY_B);
		button_set(&s_btn_call, 78, y, 86, h, call_label, C2D_Color32(40, 140, 60, 255), KEY_A);
		button_set(&s_btn_raise, 168, y, 96, h, raise_label, C2D_Color32(200, 140, 30, 255), KEY_X);
		button_set(&s_btn_all, 268, y, 48, h, GLYPH_Y " Todo", C2D_Color32(190, 60, 50, 255), KEY_Y);
		bool can_raise = mine && pk_max_raise_to(G) > G->to_call;
		s_btn_fold.enabled = s_btn_call.enabled = s_btn_all.enabled = mine;
		s_btn_raise.enabled = can_raise;
		if (mine) {
			int lo = pk_min_raise_to(G), hi = pk_max_raise_to(G);
			if (in->repeat & (KEY_R | KEY_UP | KEY_RIGHT))
				s_raise_to += PK_BIG;
			if (in->repeat & (KEY_L | KEY_DOWN | KEY_LEFT))
				s_raise_to -= PK_BIG;
			if (s_raise_to < lo)
				s_raise_to = lo;
			if (s_raise_to > hi)
				s_raise_to = hi;
			if (button_update(&s_btn_fold, in)) {
				if (pk_can_check(G))
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
				PkAction a = pk_ai_decide(G, &amount);
				act(a, amount);
			}
		}
	} else {
		bool over = pk_game_over(G);
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
	if (G->stage != s_prev_stage) {
		if (G->stage >= PK_FLOP && G->stage <= PK_RIVER)
			audio_play(SFX_FLIP);
		if (G->stage == PK_DISCARD)
			table_message(&s_table, "Ahora se cambian cartas: hasta tres (cuatro si guardas un as)");
		if (pk_hand_finished(G)) {
			s_end_time = 0.0f;
			for (int p = 0; p < PK_PLAYERS; p++)
				if (G->won[p] > 0) {
					table_mood(&s_table, p, EXPR_HAPPY, 3.0f);
				} else if (G->invested[p] > PK_BIG) {
					table_mood(&s_table, p, EXPR_SAD, 2.0f);
				}
			audio_play(G->won[0] > 0 ? SFX_WIN : SFX_LOSE);
			if (pk_game_over(G) && !s_recorded[s_v]) {
				s_recorded[s_v] = true;
				profile_record_game(GAME_OF[s_v], G->chips[0] > 0);
			}
		}
		s_prev_stage = G->stage;
	}
	table_update(&s_table, dt);
	return SCENE_OF[s_v];
}

static void poker_draw_top(void)
{
	gfx_background(BG_TABLE_TOP);
	bool reveal = G->stage == PK_SHOWDOWN ||
	              (G->stage == PK_GAME_OVER && (G->board_count == 5 || G->variant == PK_DRAW));
	for (int p = 1; p < PK_PLAYERS; p++) {
		char info[32];
		if (G->out[p])
			snprintf(info, sizeof info, "Sin fichas");
		else if (G->folded[p])
			snprintf(info, sizeof info, "%d · fuera", G->chips[p]);
		else if (G->chips[p] == 0)
			snprintf(info, sizeof info, "Con todo");
		else if (G->bet[p] > 0)
			snprintf(info, sizeof info, "%d (+%d)", G->chips[p], G->bet[p]);
		else
			snprintf(info, sizeof info, "%d fichas", G->chips[p]);
		table_draw_seat(&s_table, p, 0, G->current == p && !pk_hand_finished(G), info);
		/* Sus dos cartas */
		if (!G->out[p] && !G->folded[p]) {
			float x = table_seat_x(p) + (p == 2 ? 50 : 0), y = table_seat_y(p) + (p == 2 ? 0 : 84);
			int hn = G->hole_n;
			float step = hn == 2 ? 20.0f : hn == 4 ? 14.0f : 12.0f;
			for (int k = 0; k < hn; k++)
				gfx_french(reveal ? fr_id(G->hole[p][k]) : FRENCH_BACK, x + (k - (hn - 1) / 2.0f) * step, y, 0.45f,
					(k - (hn - 1) / 2.0f) * 0.08f, DEPTH_CARDS + k * 0.05f, k == 0);
			if (reveal && G->score[p])
				gfx_text(pk_category_name(pk_category(G->score[p])), x, y + 24, DEPTH_UI, 0.42f, COLOR_GOLD,
					C2D_AlignCenter);
		}
		if (p == G->dealer)
			gfx_text("(D)", table_seat_x(p) + (p == 2 ? -84 : 34), table_seat_y(p) - 14, DEPTH_UI, 0.45f, COLOR_GOLD,
				C2D_AlignCenter);
	}
	/* Cartas de la mesa (en el de descarte no hay) */
	for (int i = 0; i < PK_BOARD && G->variant != PK_DRAW; i++) {
		float x = 200 + (i - 2) * 44.0f;
		if (i < G->board_count)
			gfx_french(fr_id(G->board[i]), x, 120, 0.62f, 0.0f, DEPTH_CARDS, true);
		else
			gfx_rrect(x - 20 + gfx_dx(DEPTH_TABLE), 120 - 30, 40, 60, 5, DEPTH_TABLE, C2D_Color32(0, 0, 0, 50));
	}
	gfx_textf(200, 156, DEPTH_UI, 0.55f, COLOR_GOLD, C2D_AlignCenter, "Bote: %d", pk_pot(G));
	char me[40];
	snprintf(me, sizeof me, "Tus fichas: %d%s", G->chips[0], G->dealer == 0 ? " (D)" : "");
	table_draw_me(&s_table, me);
	table_draw_bubbles(&s_table);

	if (pk_hand_finished(G) && banner_draw(s_end_time, 200, 116, 340, 80)) {
		if (pk_game_over(G)) {
			if (G->chips[0] > 0)
				gfx_text("¡Te has llevado todas las fichas!", 200, 90, DEPTH_BANNER, 0.6f, COLOR_GOLD, C2D_AlignCenter);
			else
				gfx_text("Te has quedado sin fichas", 200, 90, DEPTH_BANNER, 0.6f, COLOR_RED, C2D_AlignCenter);
		}
		/* Ganadores de la mano */
		int line = 0;
		for (int p = 0; p < PK_PLAYERS && line < 2; p++) {
			if (G->won[p] <= 0)
				continue;
			const char* hand = G->stage != PK_HAND_OVER && G->score[p]
			                       ? pk_category_name(pk_category(G->score[p]))
			                       : "los demás se retiran";
			gfx_textf(200, (pk_game_over(G) ? 112 : 96) + line * 20, DEPTH_BANNER, 0.5f,
				p == 0 ? COLOR_GOLD : COLOR_CREAM, C2D_AlignCenter, "%s %s %d (%s)", p == 0 ? "Ganas" : table_name(&s_table, p),
				p == 0 ? "" : "gana", G->won[p], hand);
			line++;
		}
	}
}

static void poker_draw_bottom(void)
{
	gfx_background(BG_TABLE_BOTTOM);
	table_draw_message(&s_table);
	button_draw(&s_btn_menu, false);
	if (!G->out[0]) {
		bool discarding = G->stage == PK_DISCARD && G->current == 0;
		for (int k = 0; k < G->hole_n; k++) {
			float y = 96 - (discarding && s_sel[k] ? 14.0f : 0.0f);
			gfx_french(fr_id(G->hole[0][k]), my_card_x(k), y, my_card_scale(), (k - (G->hole_n - 1) / 2.0f) * 0.05f, 0,
				true);
			if (discarding && s_sel[k])
				gfx_text("Cambiar", my_card_x(k), y + 30, 0, 0.42f, COLOR_GOLD, C2D_AlignCenter);
		}
		/* Tu mejor jugada ahora mismo */
		uint32_t sc = pk_player_score(G, 0);
		gfx_textf(272, 70, 0, 0.45f, COLOR_CREAM, C2D_AlignCenter, "Tienes:");
		gfx_text(pk_category_name(pk_category(sc)), 272, 86, 0, 0.5f, COLOR_GOLD, C2D_AlignCenter);
		if (G->folded[0])
			gfx_text("(retirado)", 272, 104, 0, 0.45f, C2D_Color32(200, 180, 170, 255), C2D_AlignCenter);
	}
	if (G->stage == PK_DISCARD) {
		if (G->current == 0) {
			gfx_text("Toca las cartas que cambias", BOTTOM_W / 2, 30, 0, 0.5f, COLOR_GOLD, C2D_AlignCenter);
			button_draw(&s_btn_discard, s_btn_discard.enabled);
		}
		return;
	}
	bool mine = G->current == 0 && !pk_hand_finished(G);
	if (mine) {
		gfx_text("¡Tu turno!", BOTTOM_W / 2, 30, 0, 0.55f, COLOR_GOLD, C2D_AlignCenter);
		gfx_textf(BOTTOM_W / 2, 168, 0, 0.42f, C2D_Color32(200, 230, 200, 255), C2D_AlignCenter,
			"Para igualar: %d · Subida con " GLYPH_L "/" GLYPH_R " o la cruceta", pk_to_call(G, 0));
	}
	gfx_textf(BOTTOM_W / 2, 184, 0, 0.45f, COLOR_CREAM, C2D_AlignCenter, "Bote: %d · Tus fichas: %d", pk_pot(G),
		G->chips[0]);
	if (!pk_hand_finished(G)) {
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
const Scene scene_omaha = { omaha_enter, poker_update, poker_draw_top, poker_draw_bottom };
const Scene scene_drawpoker = { draw_enter, poker_update, poker_draw_top, poker_draw_bottom };
