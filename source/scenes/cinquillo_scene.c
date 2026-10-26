/* Mesa del cinquillo: el jugador humano contra tres rivales de la CPU. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "games/cinquillo.h"
#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/gfx.h"
#include "ui/widgets.h"

#define TABLE_SCALE 0.46f
#define HAND_SCALE  0.62f
#define HAND_Y      108.0f
#define SLOT_STEP   21.0f
#define MAX_FLYING  48

static const char* const NAMES[CQ_PLAYERS] = { "Tú", "Paco", "Lola", "Manolo" };
/* Desde dónde salen volando las cartas de cada jugador (pantalla superior). */
static const float ORIGIN_X[CQ_PLAYERS] = { 200, 351, 49, 49 };
static const float ORIGIN_Y[CQ_PLAYERS] = { 290, 65, 65, 165 };
/* Posición de los marcadores de los rivales. */
static const float BADGE_X[CQ_PLAYERS] = { 0, 314, 12, 12 };
static const float BADGE_Y[CQ_PLAYERS] = { 0, 20, 20, 120 };

static Cinquillo s_game;
static bool s_started;

static VisCard s_table[NUM_SUITS][RANKS_PER_SUIT];
static VisCard s_hand[DECK_SIZE]; /* indexado por card_index */
static VisCard s_flying[MAX_FLYING]; /* cartas que salen de la mesa o de la mano */
static int s_seen_turns;
static int s_seen_round;
static int s_selected;
static float s_cpu_timer;
static float s_result_time;
static int s_round_prize;
static int s_pot_before;
static float s_pass_bubble[CQ_PLAYERS];
static char s_message[96];
static float s_message_flash;

static Button s_btn_play, s_btn_pass, s_btn_menu;

static float slot_x(int pos)
{
	return 200.0f + (pos - 4.5f) * SLOT_STEP;
}

static float row_y(int suit)
{
	return 46.0f + suit * 49.0f;
}

static void set_message(const char* msg)
{
	snprintf(s_message, sizeof s_message, "%s", msg);
	s_message_flash = 1.0f;
}

static void fly_away(VisCard* v, float tx, float ty, float delay)
{
	for (int i = 0; i < MAX_FLYING; i++) {
		if (!s_flying[i].active) {
			s_flying[i] = *v;
			s_flying[i].delay = delay;
			vis_target(&s_flying[i], tx, ty, s_flying[i].scale, s_flying[i].flip > 0.5f);
			break;
		}
	}
	v->active = false;
}

static void clear_table(void)
{
	int k = 0;
	audio_play(SFX_CARD);
	for (int s = 0; s < NUM_SUITS; s++)
		for (int p = 0; p < RANKS_PER_SUIT; p++)
			if (s_table[s][p].active)
				fly_away(&s_table[s][p], 460.0f, 120.0f, (k++) * 0.02f);
}

static void select_first_legal(void)
{
	int moves[CQ_HAND_SIZE];
	if (cq_legal_moves(&s_game, 0, moves) > 0)
		s_selected = moves[0];
}

static void reset_visuals(void)
{
	memset(s_table, 0, sizeof s_table);
	memset(s_hand, 0, sizeof s_hand);
	memset(s_flying, 0, sizeof s_flying);
	s_seen_turns = 0;
	s_seen_round = s_game.round;
	s_selected = 0;
	s_cpu_timer = 0.0f;
	s_result_time = 0.0f;
	set_message("Sale quien tenga el cinco de oros");
	select_first_legal();
}

static void cq_scene_enter(void)
{
	if (!s_started || s_game.phase == CQ_PHASE_GAME_OVER) {
		cq_init(&s_game, (uint32_t)time(NULL) ^ 0xC1C0);
		s_started = true;
		reset_visuals();
	}
	button_set(&s_btn_menu, 258, 10, 54, 20, "Menú", C2D_Color32(120, 30, 35, 255), KEY_START);
}

static bool table_busy(void)
{
	for (int s = 0; s < NUM_SUITS; s++)
		for (int p = 0; p < RANKS_PER_SUIT; p++)
			if (vis_moving(&s_table[s][p]))
				return true;
	return false;
}

/* Detecta la última jugada y lanza la animación correspondiente. */
static void on_action(void)
{
	int p = s_game.last_player;
	char buf[96], card[32];
	if (s_game.last_action == CQ_ACTION_PLAY) {
		Card c = s_game.last_card;
		VisCard* v = &s_table[c.suit][card_pos(c)];
		bool from_cpu = p != 0;
		vis_spawn(v, c, ORIGIN_X[p], ORIGIN_Y[p], 0.6f, !from_cpu, 0.0f);
		vis_target(v, slot_x(card_pos(c)), row_y(c.suit), TABLE_SCALE, true);
		v->depth = DEPTH_CARDS;
		card_describe(c, card, sizeof card);
		if (p == 0)
			snprintf(buf, sizeof buf, "Juegas el %s", card);
		else
			snprintf(buf, sizeof buf, "%s juega el %s", NAMES[p], card);
		set_message(buf);
	} else if (s_game.last_action == CQ_ACTION_PASS) {
		s_pass_bubble[p] = 1.2f;
		audio_play(SFX_CHIP);
		snprintf(buf, sizeof buf, "%s pasa%s", p == 0 ? "Tú" : NAMES[p], p == 0 ? "s" : "");
		set_message(buf);
	}
}

static void sync_hand(void)
{
	const int n = s_game.hand_count[0];
	bool present[DECK_SIZE] = { false };
	float spacing = n > 1 ? fminf(30.0f, 262.0f / (n - 1)) : 0.0f;
	float x0 = BOTTOM_W / 2.0f - spacing * (n - 1) / 2.0f;
	bool my_turn = s_game.phase == CQ_PHASE_PLAYING && s_game.current == 0;
	float delay = 0.0f;

	for (int i = 0; i < n; i++) {
		Card c = s_game.hands[0][i];
		int id = card_index(c);
		VisCard* v = &s_hand[id];
		present[id] = true;
		if (!v->active) {
			vis_spawn(v, c, BOTTOM_W / 2.0f, -60.0f, HAND_SCALE, false, delay);
			delay += 0.06f;
		}
		bool selected = my_turn && i == s_selected;
		vis_target(v, x0 + i * spacing, HAND_Y - (selected ? 14.0f : 0.0f), HAND_SCALE, true);
		v->dim = (my_turn && !cq_is_legal(&s_game, c)) ? 0.45f : 0.0f;
	}
	/* Las cartas que ya no están en la mano suben hacia la pantalla superior. */
	for (int id = 0; id < DECK_SIZE; id++)
		if (s_hand[id].active && !present[id])
			fly_away(&s_hand[id], s_hand[id].x, -70.0f, 0.0f);
}

static bool human_play(int idx)
{
	if (!cq_is_legal(&s_game, s_game.hands[0][idx])) {
		set_message("Esa carta no se puede jugar ahora");
		audio_play(SFX_ERROR);
		return false;
	}
	cq_play(&s_game, idx);
	if (s_selected >= s_game.hand_count[0])
		s_selected = s_game.hand_count[0] - 1;
	if (s_selected < 0)
		s_selected = 0;
	return true;
}

static int touched_card(const Input* in)
{
	if (!in->touch_down)
		return -1;
	/* De derecha a izquierda: las cartas de la derecha están encima. */
	for (int i = s_game.hand_count[0] - 1; i >= 0; i--) {
		const VisCard* v = &s_hand[card_index(s_game.hands[0][i])];
		float w = CARD_W * HAND_SCALE / 2.0f, h = CARD_H * HAND_SCALE / 2.0f;
		if (in->tx >= v->x - w && in->tx <= v->x + w && in->ty >= v->y - h && in->ty <= v->y + h)
			return i;
	}
	return -1;
}

static SceneId cq_scene_update(const Input* in, float dt)
{
	if (button_update(&s_btn_menu, in))
		return SCENE_MENU;

	bool busy = table_busy();
	const float y = 205, h = 28;

	s_pot_before = s_game.pot;
	switch (s_game.phase) {
	case CQ_PHASE_PLAYING:
		button_set(&s_btn_play, 12, y, 144, h, GLYPH_A " Jugar", C2D_Color32(40, 140, 60, 255), KEY_A);
		button_set(&s_btn_pass, 164, y, 144, h, GLYPH_X " Pasar", C2D_Color32(190, 60, 50, 255), KEY_X);
		if (s_game.current == 0) {
			bool can = cq_can_play(&s_game, 0);
			s_btn_play.enabled = can && !busy;
			s_btn_pass.enabled = !can && !busy;
			if (busy)
				break;
			int n = s_game.hand_count[0];
			if (in->repeat & KEY_LEFT) {
				s_selected = (s_selected + n - 1) % n;
				audio_play(SFX_SELECT);
			}
			if (in->repeat & KEY_RIGHT) {
				s_selected = (s_selected + 1) % n;
				audio_play(SFX_SELECT);
			}
			int t = touched_card(in);
			if (t >= 0) {
				s_selected = t;
				human_play(t);
			} else if (button_update(&s_btn_play, in)) {
				human_play(s_selected);
			} else if (button_update(&s_btn_pass, in)) {
				cq_pass(&s_game);
			}
		} else {
			s_btn_play.enabled = s_btn_pass.enabled = false;
			s_cpu_timer += dt;
			if (!busy && s_cpu_timer > 0.75f) {
				s_cpu_timer = 0.0f;
				int idx = cq_ai_choose(&s_game, s_game.current);
				if (idx >= 0)
					cq_play(&s_game, idx);
				else
					cq_pass(&s_game);
			}
		}
		break;
	case CQ_PHASE_ROUND_OVER:
		button_set(&s_btn_play, 60, y, 200, h, GLYPH_A " Siguiente ronda", C2D_Color32(40, 140, 60, 255), KEY_A);
		s_btn_play.enabled = !busy && s_result_time > 0.6f;
		if (button_update(&s_btn_play, in))
			cq_next_round(&s_game);
		break;
	case CQ_PHASE_GAME_OVER:
		button_set(&s_btn_play, 12, y, 144, h, GLYPH_A " Nueva partida", C2D_Color32(40, 140, 60, 255), KEY_A);
		button_set(&s_btn_pass, 164, y, 144, h, GLYPH_B " Menú", C2D_Color32(190, 60, 50, 255), KEY_B);
		s_btn_play.enabled = s_btn_pass.enabled = !busy && s_result_time > 0.6f;
		if (button_update(&s_btn_play, in)) {
			cq_init(&s_game, (uint32_t)time(NULL));
			clear_table();
			s_seen_turns = 0;
			s_seen_round = s_game.round;
			select_first_legal();
		} else if (button_update(&s_btn_pass, in)) {
			return SCENE_MENU;
		}
		break;
	}

	/* Cambios en el estado del juego -> animaciones */
	if (s_game.round != s_seen_round) {
		clear_table();
		s_seen_round = s_game.round;
		s_seen_turns = 0;
		s_result_time = 0.0f;
		set_message("Nueva ronda: sale el cinco de oros");
		select_first_legal();
	}
	if (s_game.turns != s_seen_turns) {
		s_seen_turns = s_game.turns;
		on_action();
		s_cpu_timer = 0.0f;
		if (s_game.phase != CQ_PHASE_PLAYING) {
			s_round_prize = s_pot_before;
			s_result_time = 0.0f;
		}
		if (s_game.phase == CQ_PHASE_PLAYING && s_game.current == 0)
			select_first_legal();
	}
	if (s_game.phase != CQ_PHASE_PLAYING && !busy) {
		if (s_result_time == 0.0f) {
			int winner = s_game.phase == CQ_PHASE_GAME_OVER ? cq_leader(&s_game) : s_game.round_winner;
			audio_play(winner == 0 ? SFX_WIN : SFX_LOSE);
		}
		s_result_time += dt;
	}
	if (s_game.phase == CQ_PHASE_PLAYING && s_game.current == 0 && s_selected >= s_game.hand_count[0])
		s_selected = s_game.hand_count[0] - 1;

	sync_hand();
	for (int s = 0; s < NUM_SUITS; s++)
		for (int p = 0; p < RANKS_PER_SUIT; p++)
			vis_update(&s_table[s][p], dt);
	for (int i = 0; i < DECK_SIZE; i++)
		vis_update(&s_hand[i], dt);
	for (int i = 0; i < MAX_FLYING; i++) {
		vis_update(&s_flying[i], dt);
		if (s_flying[i].active && !vis_moving(&s_flying[i]))
			s_flying[i].active = false;
	}
	for (int p = 0; p < CQ_PLAYERS; p++)
		if (s_pass_bubble[p] > 0.0f)
			s_pass_bubble[p] -= dt;
	if (s_message_flash > 0.0f)
		s_message_flash -= dt * 2.0f;
	return SCENE_CINQUILLO;
}

static void draw_badge(int p)
{
	float x = BADGE_X[p], y = BADGE_Y[p];
	bool turn = s_game.phase == CQ_PHASE_PLAYING && s_game.current == p;
	bool winner = s_game.phase != CQ_PHASE_PLAYING && s_game.round_winner == p;
	u32 border = (turn || winner) ? COLOR_GOLD : C2D_Color32(20, 60, 30, 255);
	float lift = turn ? 0.5f + 0.5f * sinf(gfx_time() * 5.0f) : 0.0f;

	if (turn)
		gfx_rrect(x - 3, y - 3, 80, 96, 10, DEPTH_UI, C2D_Color32(255, 220, 90, (u8)(80 + 100 * lift)));
	gfx_panel(x, y, 74, 90, DEPTH_UI, C2D_Color32(10, 50, 25, 210), border);
	gfx_text(NAMES[p], x + 37, y + 5, DEPTH_UI, 0.5f, turn ? COLOR_GOLD : COLOR_CREAM, C2D_AlignCenter);

	/* Cartas en la mano, en abanico */
	int n = s_game.hand_count[p];
	int shown = n > 5 ? 5 : n;
	for (int i = 0; i < shown; i++)
		gfx_sprite_ex(SPR_CARD_BACK, x + 30 + (i - (shown - 1) / 2.0f) * 5.0f, y + 44, 0.28f, 0.28f,
			(i - (shown - 1) / 2.0f) * 0.15f, DEPTH_UI + 0.2f, NULL);
	gfx_textf(x + 60, y + 36, DEPTH_UI, 0.5f, COLOR_WHITE, C2D_AlignCenter, "%d", n);

	gfx_sprite(SPR_CHIP_RED, x + 18, y + 76, 0.6f, DEPTH_UI);
	gfx_textf(x + 30, y + 68, DEPTH_UI, 0.5f, COLOR_WHITE, C2D_AlignLeft, "%d", s_game.chips[p]);

	if (s_pass_bubble[p] > 0.0f) {
		float pop = ease_out_back(fminf(1.0f, (1.2f - s_pass_bubble[p]) / 0.2f));
		float bw = 52 * pop, bh = 22 * pop;
		gfx_rrect(x + 37 - bw / 2, y + 40 - bh / 2, bw, bh, 8, DEPTH_BANNER, C2D_Color32(255, 250, 235, 240));
		if (pop > 0.8f)
			gfx_text("¡Paso!", x + 37, y + 32, DEPTH_BANNER, 0.5f, C2D_Color32(170, 30, 30, 255), C2D_AlignCenter);
	}
}

static void draw_banner(void)
{
	float t = fminf(1.0f, s_result_time / 0.35f);
	float s = ease_out_back(t);
	float h = 64 * s;
	gfx_rrect(50, 120 - h / 2, 300, h, 10, DEPTH_BANNER, C2D_Color32(0, 0, 0, 200));
	if (t < 0.6f)
		return;
	int w = s_game.round_winner;
	if (s_game.phase == CQ_PHASE_GAME_OVER) {
		int leader = cq_leader(&s_game);
		gfx_text("Fin de la partida", 200, 92, DEPTH_BANNER, 0.75f, COLOR_GOLD, C2D_AlignCenter);
		if (leader == 0)
			gfx_textf(200, 120, DEPTH_BANNER, 0.55f, COLOR_GREEN, C2D_AlignCenter,
				"¡Has ganado con %d fichas!", s_game.chips[0]);
		else
			gfx_textf(200, 120, DEPTH_BANNER, 0.55f, COLOR_CREAM, C2D_AlignCenter, "Gana %s con %d fichas",
				NAMES[leader], s_game.chips[leader]);
	} else {
		if (w == 0)
			gfx_text("¡Ganas la ronda!", 200, 92, DEPTH_BANNER, 0.8f, COLOR_GOLD, C2D_AlignCenter);
		else
			gfx_textf(200, 92, DEPTH_BANNER, 0.8f, COLOR_CREAM, C2D_AlignCenter, "%s gana la ronda", NAMES[w]);
		gfx_textf(200, 122, DEPTH_BANNER, 0.55f, COLOR_GOLD, C2D_AlignCenter, "Se lleva el bote: %d fichas",
			s_round_prize);
	}
}

static void cq_scene_draw_top(void)
{
	gfx_background(BG_TABLE_TOP);

	/* Huecos de cada palo; el del cinco lleva el símbolo del palo. */
	for (int s = 0; s < NUM_SUITS; s++) {
		for (int p = 0; p < RANKS_PER_SUIT; p++) {
			float w = CARD_W * TABLE_SCALE, h = CARD_H * TABLE_SCALE;
			u32 col = p == 4 ? C2D_Color32(0, 0, 0, 70) : C2D_Color32(0, 0, 0, 35);
			gfx_rrect(slot_x(p) - w / 2, row_y(s) - h / 2, w, h, 3, DEPTH_TABLE, col);
		}
		C2D_ImageTint tint;
		C2D_AlphaImageTint(&tint, 0.55f);
		gfx_sprite_ex(SPR_SUIT_FIRST + s, slot_x(4), row_y(s), 0.8f, 0.8f, 0.0f, DEPTH_TABLE, &tint);
	}

	for (int i = 0; i < MAX_FLYING; i++)
		if (s_flying[i].active && s_flying[i].ty > 0)
			vis_draw(&s_flying[i]);
	for (int s = 0; s < NUM_SUITS; s++)
		for (int p = 0; p < RANKS_PER_SUIT; p++)
			vis_draw(&s_table[s][p]);

	for (int p = 1; p < CQ_PLAYERS; p++)
		draw_badge(p);

	/* Bote y ronda */
	gfx_panel(314, 120, 74, 90, DEPTH_UI, C2D_Color32(60, 30, 10, 210), C2D_Color32(200, 150, 60, 255));
	gfx_text("Bote", 351, 125, DEPTH_UI, 0.5f, COLOR_CREAM, C2D_AlignCenter);
	gfx_chip_stack(s_game.pot, 351, 170, DEPTH_UI);
	gfx_textf(351, 174, DEPTH_UI, 0.55f, COLOR_GOLD, C2D_AlignCenter, "%d", s_game.pot);
	gfx_textf(351, 194, DEPTH_UI, 0.4f, COLOR_CREAM, C2D_AlignCenter, "Ronda %d/%d", s_game.round, CQ_MAX_ROUNDS);

	if (s_game.phase != CQ_PHASE_PLAYING && !table_busy())
		draw_banner();
}

static void cq_scene_draw_bottom(void)
{
	gfx_background(BG_TABLE_BOTTOM);

	u32 msg_col = s_message_flash > 0.0f ? COLOR_GOLD : COLOR_CREAM;
	gfx_text(s_message, 14, 12, 0, 0.5f, msg_col, C2D_AlignLeft);
	button_draw(&s_btn_menu, false);

	bool my_turn = s_game.phase == CQ_PHASE_PLAYING && s_game.current == 0;
	if (my_turn)
		gfx_text("¡Tu turno!", BOTTOM_W / 2, 34, 0, 0.55f, COLOR_GOLD, C2D_AlignCenter);
	else if (s_game.phase == CQ_PHASE_PLAYING)
		gfx_textf(BOTTOM_W / 2, 34, 0, 0.5f, COLOR_CREAM, C2D_AlignCenter, "Turno de %s...",
			NAMES[s_game.current]);

	/* Brillo bajo la carta seleccionada */
	if (my_turn && s_game.hand_count[0] > 0) {
		const VisCard* v = &s_hand[card_index(s_game.hands[0][s_selected])];
		if (v->active && cq_is_legal(&s_game, v->card)) {
			float pulse = 0.75f + 0.25f * sinf(gfx_time() * 6.0f);
			C2D_ImageTint tint;
			C2D_AlphaImageTint(&tint, pulse);
			gfx_sprite_ex(SPR_CARD_GLOW, v->x, v->y, HAND_SCALE, HAND_SCALE, 0.0f, 0, &tint);
		}
	}
	/* La mano, en el orden en que está (la de más a la derecha encima) */
	for (int i = 0; i < s_game.hand_count[0]; i++)
		vis_draw(&s_hand[card_index(s_game.hands[0][i])]);
	for (int i = 0; i < MAX_FLYING; i++)
		if (s_flying[i].active && s_flying[i].ty < 0)
			vis_draw(&s_flying[i]);

	gfx_textf(14, 160, 0, 0.5f, COLOR_CREAM, C2D_AlignLeft, "Tus fichas: %d", s_game.chips[0]);
	gfx_textf(306, 160, 0, 0.5f, COLOR_CREAM, C2D_AlignRight, "Bote: %d", s_game.pot);

	const char* hint = "";
	if (my_turn)
		hint = cq_can_play(&s_game, 0) ? "Toca una carta iluminada o usa " GLYPH_DPAD " y " GLYPH_A
		                               : "No tienes jugada: pasa y paga 1 ficha";
	gfx_text(hint, BOTTOM_W / 2, 180, 0, 0.42f, C2D_Color32(200, 230, 200, 255), C2D_AlignCenter);

	button_draw(&s_btn_play, s_btn_play.enabled);
	if (s_game.phase != CQ_PHASE_ROUND_OVER)
		button_draw(&s_btn_pass, s_btn_pass.enabled && my_turn);
}

const Scene scene_cinquillo = { cq_scene_enter, cq_scene_update, cq_scene_draw_top, cq_scene_draw_bottom };
