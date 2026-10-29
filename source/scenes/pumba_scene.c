/* Pumba: el "uno" de la baraja española, contra tres personajes. */
#include <math.h>
#include <stdio.h>
#include <time.h>

#include "games/pumba.h"
#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/gfx.h"
#include "ui/table.h"
#include "ui/widgets.h"

#define PILE_X   222.0f
#define PILE_Y   112.0f
#define DECK_X   166.0f
#define HAND_SC  0.6f

static Pumba s_game;
static Table s_table;
static bool s_started;
static int s_cursor;
static float s_cpu_timer, s_catch_timer, s_result_time;
static PuPhase s_prev_phase;
static Button s_btn_play, s_btn_draw, s_btn_say, s_btn_menu, s_btn_suit[NUM_SUITS];

static const char* const SUIT_CAP[NUM_SUITS] = { "Oros", "Copas", "Espadas", "Bastos" };

static void reset_round(void)
{
	s_cursor = 0;
	s_cpu_timer = s_catch_timer = s_result_time = 0.0f;
	s_prev_phase = s_game.phase;
	table_message(&s_table, "Empieza %s", s_game.current == 0 ? "tú" : table_name(&s_table, s_game.current));
}

static void pu_enter(void)
{
	if (!s_started) {
		table_init(&s_table, CPU_PACO, CPU_CARMEN, CPU_PEPE);
		pu_init(&s_game, (uint32_t)time(NULL) ^ 0x9B4);
		s_started = true;
		reset_round();
	}
	button_set(&s_btn_menu, 258, 6, 54, 20, "Menú", C2D_Color32(120, 30, 35, 255), KEY_START);
	for (int s = 0; s < NUM_SUITS; s++)
		button_set(&s_btn_suit[s], 20 + (s % 2) * 144, 60 + (s / 2) * 60, 136, 50, SUIT_CAP[s],
			C2D_Color32(60, 90, 160, 255), 0);
}

static void after_play(int p)
{
	if (s_game.phase >= PU_PHASE_ROUND_OVER)
		return;
	Card c = s_game.last_card;
	if (s_game.pending > 0) {
		table_message(&s_table, "¡Dos! %s tiene que robar %d o echar otro dos",
			s_game.current == 0 ? "Tú" : table_name(&s_table, s_game.current), s_game.pending);
		table_bubble(&s_table, p, s_game.pending > 2 ? "¡Y otros dos!" : "¡Roba dos!");
	} else if (c.rank == 1) {
		table_message(&s_table, "¡Salta!");
	} else if (c.rank == RANK_SOTA) {
		table_message(&s_table, "¡Cambio de sentido!");
	} else if (c.rank == RANK_CABALLO && s_game.phase == PU_PHASE_PLAYING) {
		table_message(&s_table, "Caballo: ahora se juega a %s", suit_name(s_game.suit));
	}
	if (s_game.hand_count[p] == 1 && s_game.said[p])
		table_bubble(&s_table, p, "¡Pumba!");
	s_catch_timer = 0.0f;
}

static void after_draw(int p)
{
	if (s_game.last_penalty_player == p && s_game.last_penalty > 0) {
		if (p == 0)
			table_message(&s_table, "Robas %d cartas", s_game.last_penalty);
		else
			table_message(&s_table, "%s roba %d cartas", table_name(&s_table, p), s_game.last_penalty);
		table_mood(&s_table, p, EXPR_SAD, 1.5f);
	}
}

static void human_play(int idx)
{
	if (!pu_can_play(&s_game, 0, idx)) {
		audio_play(SFX_ERROR);
		table_message(&s_table, s_game.pending > 0 ? "Solo puedes echar un dos o robar"
		                        : s_game.drew      ? "Solo puedes echar la carta que has robado"
		                                           : "Esa carta no vale");
		return;
	}
	pu_play(&s_game, idx);
	audio_play(SFX_CARD);
	after_play(0);
	if (s_cursor >= s_game.hand_count[0])
		s_cursor = s_game.hand_count[0] - 1;
	if (s_cursor < 0)
		s_cursor = 0;
}

static void cpu_turn(int p)
{
	if (s_game.phase == PU_PHASE_CHOOSE_SUIT) {
		pu_choose_suit(&s_game, pu_ai_suit(&s_game, p));
		after_play(p);
		return;
	}
	if (s_game.hand_count[p] == 2 && rng_range(&s_game.rng, 10) < 9)
		pu_say(&s_game, p);
	int i = pu_ai_choose(&s_game, p);
	if (i >= 0) {
		pu_play(&s_game, i);
		audio_play(SFX_CARD);
		after_play(p);
	} else if (!s_game.drew) {
		pu_draw(&s_game);
		audio_play(SFX_FLIP);
		after_draw(p);
	} else {
		pu_pass(&s_game);
	}
}

static int catchable_cpu(void)
{
	for (int p = 1; p < PU_PLAYERS; p++)
		if (pu_catchable(&s_game, p))
			return p;
	return -1;
}

static SceneId pu_update(const Input* in, float dt)
{
	if (button_update(&s_btn_menu, in))
		return SCENE_MENU;
	const float y = 205, h = 28;
	if (s_game.phase <= PU_PHASE_CHOOSE_SUIT) {
		bool mine = s_game.current == 0;
		button_set(&s_btn_play, 8, y, 96, h, GLYPH_A " Echar", C2D_Color32(40, 140, 60, 255), KEY_A);
		button_set(&s_btn_draw, 112, y, 96, h,
			s_game.pending > 0 && mine ? GLYPH_X " Robar" : s_game.drew && mine ? GLYPH_X " Pasar" : GLYPH_X " Robar",
			C2D_Color32(60, 90, 160, 255), KEY_X);
		int victim = catchable_cpu();
		button_set(&s_btn_say, 216, y, 96, h, victim >= 0 ? GLYPH_Y " ¡Pillado!" : GLYPH_Y " ¡Pumba!",
			C2D_Color32(220, 60, 50, 255), KEY_Y);
		bool my_say = s_game.hand_count[0] <= 2 && !s_game.said[0] && (s_game.hand_count[0] == 1 || mine);
		s_btn_say.enabled = victim >= 0 || my_say;
		s_btn_play.enabled = s_btn_draw.enabled = mine && s_game.phase == PU_PHASE_PLAYING;

		if (button_update(&s_btn_say, in)) {
			if (victim >= 0 && pu_catch(&s_game, victim)) {
				table_message(&s_table, "¡Pillas a %s! Roba dos", table_name(&s_table, victim));
				table_mood(&s_table, victim, EXPR_SURPRISED, 1.5f);
			} else if (pu_say(&s_game, 0)) {
				table_bubble(&s_table, 0, "¡Pumba!");
			}
		}
		if (mine && s_game.phase == PU_PHASE_CHOOSE_SUIT) {
			for (int s = 0; s < NUM_SUITS; s++)
				if (button_update(&s_btn_suit[s], in)) {
					pu_choose_suit(&s_game, s);
					after_play(0);
				}
		} else if (mine) {
			int n = s_game.hand_count[0];
			if (n > 0) {
				if (in->repeat & KEY_LEFT)
					s_cursor = (s_cursor + n - 1) % n;
				if (in->repeat & KEY_RIGHT)
					s_cursor = (s_cursor + 1) % n;
				if (s_game.drew && s_game.drawn_index >= 0)
					s_cursor = s_game.drawn_index;
				if (s_cursor >= n)
					s_cursor = n - 1;
			}
			int t = hand_touched(in, n, HAND_SC);
			if (t >= 0) {
				s_cursor = t;
				human_play(t);
			} else if (button_update(&s_btn_play, in)) {
				human_play(s_cursor);
			} else if (button_update(&s_btn_draw, in)) {
				if (s_game.drew) {
					pu_pass(&s_game);
				} else {
					bool penalty = s_game.pending > 0;
					pu_draw(&s_game);
					audio_play(SFX_FLIP);
					if (penalty)
						after_draw(0);
					else if (s_game.current == 0)
						table_message(&s_table, "Puedes echar la carta robada o pasar");
					else
						table_message(&s_table, "La carta robada no vale: pasas");
				}
			}
		} else {
			s_cpu_timer += dt;
			if (s_cpu_timer > 0.8f) {
				s_cpu_timer = 0.0f;
				cpu_turn(s_game.current);
			}
		}
		if (pu_catchable(&s_game, 0)) {
			s_catch_timer += dt;
			if (s_catch_timer > 2.2f) {
				pu_catch(&s_game, 0);
				table_message(&s_table, "¡Te han pillado sin decir pumba! Robas dos");
				table_mood(&s_table, 0, EXPR_SAD, 1.5f);
			}
		}
	} else {
		bool over = s_game.phase == PU_PHASE_GAME_OVER;
		button_set(&s_btn_play, 12, y, 144, h, over ? GLYPH_A " Otra partida" : GLYPH_A " Otra ronda",
			C2D_Color32(40, 140, 60, 255), KEY_A);
		button_set(&s_btn_draw, 164, y, 144, h, GLYPH_B " Menú", C2D_Color32(190, 60, 50, 255), KEY_B);
		s_btn_play.enabled = s_btn_draw.enabled = s_result_time > 0.6f;
		if (button_update(&s_btn_play, in)) {
			if (over)
				pu_new_game(&s_game);
			else
				pu_new_round(&s_game);
			reset_round();
		} else if (button_update(&s_btn_draw, in)) {
			return SCENE_MENU;
		}
	}
	if (s_game.phase != s_prev_phase) {
		s_prev_phase = s_game.phase;
		if (s_game.phase >= PU_PHASE_ROUND_OVER) {
			s_result_time = 0.0f;
			int w = s_game.round_winner;
			for (int p = 0; p < PU_PLAYERS; p++)
				table_mood(&s_table, p, p == w ? EXPR_HAPPY : EXPR_SAD, 4.0f);
			audio_play(w == 0 ? SFX_WIN : SFX_LOSE);
			if (s_game.phase == PU_PHASE_GAME_OVER)
				profile_record_game(GAME_PUMBA, pu_game_winner(&s_game) == 0);
		}
	}
	if (s_game.phase >= PU_PHASE_ROUND_OVER)
		s_result_time += dt;
	table_update(&s_table, dt);
	return SCENE_PUMBA;
}

static void pu_draw_top(void)
{
	gfx_background(BG_TABLE_TOP);
	for (int i = 0; i < 4; i++)
		gfx_card_back(DECK_X - i * 0.6f, PILE_Y - i * 0.8f, 0.6f, 0.6f, 0.0f, DEPTH_CARDS + i * 0.05f, NULL);
	gfx_textf(DECK_X, PILE_Y + 40, DEPTH_UI, 0.4f, COLOR_CREAM, C2D_AlignCenter, "%d", s_game.deck_count);
	if (s_game.discard_count > 1)
		gfx_card(s_game.discard[s_game.discard_count - 2], PILE_X - 3, PILE_Y + 2, 0.6f, 1.0f, -0.12f, DEPTH_CARDS, 0.0f,
			false);
	gfx_card(pu_top(&s_game), PILE_X, PILE_Y, 0.6f, 1.0f, 0.05f, DEPTH_CARDS + 0.3f, 0.0f, true);
	/* Palo que vale y doses acumulados */
	gfx_sprite_ex(SPR_SUIT_FIRST + s_game.suit, PILE_X + 52, PILE_Y - 14, 0.9f, 0.9f, 0.0f, DEPTH_UI, NULL);
	gfx_text(suit_name(s_game.suit), PILE_X + 52, PILE_Y + 4, DEPTH_UI, 0.42f, COLOR_CREAM, C2D_AlignCenter);
	if (s_game.pending > 0)
		gfx_textf(PILE_X, PILE_Y + 42, DEPTH_UI, 0.55f, COLOR_GOLD, C2D_AlignCenter, "+%d", s_game.pending);
	gfx_text(s_game.direction > 0 ? "Sentido: horario" : "Sentido: al revés", 330, 196, DEPTH_UI, 0.42f,
		C2D_Color32(200, 230, 200, 255), C2D_AlignCenter);
	for (int p = 1; p < PU_PLAYERS; p++) {
		char info[24];
		snprintf(info, sizeof info, "%d pts", s_game.score[p]);
		bool turn = s_game.current == p && s_game.phase <= PU_PHASE_CHOOSE_SUIT;
		table_draw_seat(&s_table, p, s_game.hand_count[p], turn, info);
	}
	char me[32];
	snprintf(me, sizeof me, "Tus puntos: %d", s_game.score[0]);
	table_draw_me(&s_table, me);
	table_draw_bubbles(&s_table);

	if (s_game.phase >= PU_PHASE_ROUND_OVER && banner_draw(s_result_time, 200, 112, 340, 96)) {
		bool over = s_game.phase == PU_PHASE_GAME_OVER;
		int w = over ? pu_game_winner(&s_game) : s_game.round_winner;
		char title[64];
		if (w == 0)
			snprintf(title, sizeof title, over ? "¡Ganas la partida!" : "¡Ganas la ronda!");
		else
			snprintf(title, sizeof title, over ? "Gana la partida %s" : "Gana la ronda %s", table_name(&s_table, w));
		gfx_text(title, 200, 72, DEPTH_BANNER, 0.65f, w == 0 ? COLOR_GOLD : COLOR_RED, C2D_AlignCenter);
		gfx_textf(200, 98, DEPTH_BANNER, 0.5f, COLOR_CREAM, C2D_AlignCenter, "%s %d puntos",
			s_game.round_winner == 0 ? "Te llevas" : table_name(&s_table, s_game.round_winner), s_game.round_points);
		gfx_textf(200, 122, DEPTH_BANNER, 0.45f, COLOR_GOLD, C2D_AlignCenter, "Tú %d · %d · %d · %d (a %d)",
			s_game.score[0], s_game.score[1], s_game.score[2], s_game.score[3], PU_GAME_POINTS);
	}
}

static void pu_draw_bottom(void)
{
	gfx_background(BG_TABLE_BOTTOM);
	table_draw_message(&s_table);
	button_draw(&s_btn_menu, false);
	bool mine = s_game.current == 0 && s_game.phase <= PU_PHASE_CHOOSE_SUIT;
	if (mine && s_game.phase == PU_PHASE_CHOOSE_SUIT) {
		gfx_text("Elige el palo", BOTTOM_W / 2, 30, 0, 0.6f, COLOR_GOLD, C2D_AlignCenter);
		for (int s = 0; s < NUM_SUITS; s++) {
			button_draw(&s_btn_suit[s], true);
			gfx_sprite_ex(SPR_SUIT_FIRST + s, s_btn_suit[s].x + 22, s_btn_suit[s].y + 25, 0.8f, 0.8f, 0.0f, 0, NULL);
		}
		return;
	}
	if (mine)
		gfx_text(s_game.pending > 0 ? "¡Echa un dos o roba!" : "¡Tu turno!", BOTTOM_W / 2, 30, 0, 0.55f, COLOR_GOLD,
			C2D_AlignCenter);
	int n = s_game.hand_count[0];
	bool playable[PU_MAX_HAND];
	for (int i = 0; i < n; i++)
		playable[i] = !mine || pu_can_play(&s_game, 0, i);
	hand_draw(s_game.hands[0], n, HAND_SC, NULL, mine ? s_cursor : -1, playable);
	if (s_game.said[0] && n == 1)
		gfx_text("Has dicho ¡Pumba!", BOTTOM_W / 2, 168, 0, 0.5f, COLOR_GOLD, C2D_AlignCenter);
	else
		gfx_text("As salta · 2 roba dos · sota cambia · caballo comodín", BOTTOM_W / 2, 172, 0, 0.38f,
			C2D_Color32(200, 230, 200, 255), C2D_AlignCenter);
	button_draw(&s_btn_play, s_btn_play.enabled);
	button_draw(&s_btn_draw, s_btn_draw.enabled);
	if (s_game.phase <= PU_PHASE_CHOOSE_SUIT)
		button_draw(&s_btn_say, s_btn_say.enabled);
}

const Scene scene_pumba = { pu_enter, pu_update, pu_draw_top, pu_draw_bottom };
