/* Truco argentino mano a mano contra la CPU. */
#include <math.h>
#include <stdio.h>
#include <time.h>

#include "games/truco.h"
#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/gfx.h"
#include "ui/table.h"
#include "ui/widgets.h"

#define CPU_SEAT 2
#define HAND_SC  0.75f
#define BAZA_X(b) (130.0f + (b) * 64.0f)

static Truco s_game;
static Table s_table;
static bool s_started;
static int s_cursor;
static bool s_envido_menu;
static float s_cpu_timer, s_end_time;
static TrPhase s_prev_phase;
static Button s_btn[4], s_btn_env[3], s_btn_back, s_btn_menu;
static TrAction s_act[4];

static const char* truco_word(int level)
{
	return level <= 2 ? "Truco" : level == 3 ? "Retruco" : "Vale cuatro";
}

static const char* cpu(void)
{
	return table_name(&s_table, CPU_SEAT);
}

/* Asiento de la mesa de cada jugador */
static int seat(int p)
{
	return p == 0 ? 0 : CPU_SEAT;
}

static void start_hand(void)
{
	s_cursor = 0;
	s_envido_menu = false;
	s_cpu_timer = s_end_time = 0.0f;
	s_prev_phase = s_game.phase;
	table_message(&s_table, "%s", s_game.mano == 0 ? "Eres mano: sales tú" : "Es mano la CPU");
}

static void tr_enter(void)
{
	if (!s_started) {
		table_init(&s_table, CPU_PACO, CPU_MANOLO, CPU_LOLA);
		tr_init(&s_game, (uint32_t)time(NULL) ^ 0x7AC0);
		s_started = true;
		start_hand();
	}
	button_set(&s_btn_menu, 258, 6, 54, 20, "Menú", C2D_Color32(120, 30, 35, 255), KEY_START);
	static const char* const env[3] = { GLYPH_A " Envido", GLYPH_X " Real envido", GLYPH_Y " Falta envido" };
	static const u32 keys[3] = { KEY_A, KEY_X, KEY_Y };
	for (int i = 0; i < 3; i++)
		button_set(&s_btn_env[i], 12 + (i % 2) * 152, 140 + (i / 2) * 34, 144, 28, env[i], C2D_Color32(190, 120, 30, 255),
			keys[i]);
	button_set(&s_btn_back, 164, 174, 144, 28, GLYPH_B " Volver", C2D_Color32(110, 110, 110, 255), KEY_B);
}

/* Lo que se dice al hacer algo */
static void announce(int p, TrAction a)
{
	char buf[32];
	switch (a) {
	case TR_ACT_ENVIDO:
		snprintf(buf, sizeof buf, "¡Envido!");
		break;
	case TR_ACT_REAL:
		snprintf(buf, sizeof buf, "¡Real envido!");
		break;
	case TR_ACT_FALTA:
		snprintf(buf, sizeof buf, "¡Falta envido!");
		break;
	case TR_ACT_TRUCO:
		snprintf(buf, sizeof buf, s_game.truco_call > 2 ? "¡Quiero %s!" : "¡%s!", truco_word(s_game.truco_call));
		break;
	case TR_ACT_QUIERO:
		snprintf(buf, sizeof buf, "¡Quiero!");
		break;
	case TR_ACT_NOQUIERO:
		snprintf(buf, sizeof buf, "No quiero");
		break;
	case TR_ACT_MAZO:
		snprintf(buf, sizeof buf, "Me voy al mazo");
		break;
	default:
		return;
	}
	table_bubble(&s_table, seat(p), buf);
	audio_play(a == TR_ACT_NOQUIERO || a == TR_ACT_MAZO ? SFX_SELECT : SFX_CHIP);
}

static void act(int p, TrAction a)
{
	bool envido_pending = s_game.pending == TR_ACT_ENVIDO;
	if (!tr_act(&s_game, p, a))
		return;
	announce(p, a);
	s_envido_menu = false;
	const char* who = p == 0 ? "Tú" : cpu();
	if (envido_pending && (a == TR_ACT_QUIERO || a == TR_ACT_NOQUIERO)) {
		if (a == TR_ACT_QUIERO) {
			char b0[16], b1[16];
			snprintf(b0, sizeof b0, "Tengo %d", s_game.envido_pts[0]);
			snprintf(b1, sizeof b1, "Tengo %d", s_game.envido_pts[1]);
			table_bubble(&s_table, 0, b0);
			table_bubble(&s_table, CPU_SEAT, b1);
			table_message(&s_table, "Envido: tú %d, %s %d. %s %d", s_game.envido_pts[0], cpu(), s_game.envido_pts[1],
				s_game.envido_winner == 0 ? "Te llevas" : "Se lleva", s_game.envido_value);
			table_mood(&s_table, seat(s_game.envido_winner), EXPR_HAPPY, 2.0f);
			table_mood(&s_table, seat(1 - s_game.envido_winner), EXPR_SAD, 2.0f);
		} else {
			table_message(&s_table, "%s no quiere el envido: %d para %s", who, s_game.envido_value,
				s_game.envido_taker == 0 ? "ti" : cpu());
		}
		return;
	}
	switch (a) {
	case TR_ACT_ENVIDO:
	case TR_ACT_REAL:
	case TR_ACT_FALTA:
		table_message(&s_table, "%s %s", who, p == 0 ? "cantas el envido" : "canta el envido");
		break;
	case TR_ACT_TRUCO:
		table_message(&s_table, "%s %s %s", who, p == 0 ? "cantas" : "canta", truco_word(s_game.truco_call));
		break;
	case TR_ACT_QUIERO:
		table_message(&s_table, "La mano vale %d", s_game.truco_level);
		break;
	default:
		break;
	}
}

static int unused_index(int k)
{
	for (int i = 0, n = 0; i < 3; i++)
		if (!s_game.used[0][i] && n++ == k)
			return i;
	return -1;
}

static int unused_count(void)
{
	int n = 0;
	for (int i = 0; i < 3; i++)
		n += !s_game.used[0][i];
	return n;
}

static void play(int p, int idx)
{
	if (!tr_play(&s_game, p, idx))
		return;
	audio_play(SFX_CARD);
	s_cursor = 0;
}

/* Botones según lo que se puede hacer */
static void layout(void)
{
	for (int i = 0; i < 4; i++)
		s_act[i] = TR_ACT_COUNT;
	static char truco_label[32];
	int n = 0;
	bool mine = tr_actor(&s_game) == 0;
	if (!mine || s_game.phase >= TR_HAND_OVER)
		return;
	bool any_envido = tr_can(&s_game, 0, TR_ACT_ENVIDO) || tr_can(&s_game, 0, TR_ACT_REAL) ||
	                  tr_can(&s_game, 0, TR_ACT_FALTA);
	snprintf(truco_label, sizeof truco_label, GLYPH_X " %s", truco_word(tr_next_truco(&s_game)));
	if (s_game.phase == TR_RESPOND) {
		button_set(&s_btn[n], 0, 0, 0, 0, GLYPH_A " Quiero", C2D_Color32(40, 140, 60, 255), KEY_A);
		s_act[n++] = TR_ACT_QUIERO;
		button_set(&s_btn[n], 0, 0, 0, 0, GLYPH_B " No quiero", C2D_Color32(190, 60, 50, 255), KEY_B);
		s_act[n++] = TR_ACT_NOQUIERO;
	} else {
		button_set(&s_btn[n], 0, 0, 0, 0, GLYPH_A " Echar", C2D_Color32(40, 140, 60, 255), KEY_A);
		s_act[n++] = TR_ACT_NONE;
		button_set(&s_btn[n], 0, 0, 0, 0, "L Al mazo", C2D_Color32(110, 110, 110, 255), KEY_L);
		s_act[n++] = TR_ACT_MAZO;
	}
	if (tr_can(&s_game, 0, TR_ACT_TRUCO)) {
		button_set(&s_btn[n], 0, 0, 0, 0, truco_label, C2D_Color32(60, 90, 160, 255), KEY_X);
		s_act[n++] = TR_ACT_TRUCO;
	}
	if (any_envido) {
		button_set(&s_btn[n], 0, 0, 0, 0, GLYPH_Y " Envido...", C2D_Color32(190, 120, 30, 255), KEY_Y);
		s_act[n++] = TR_ACT_ENVIDO;
	}
	/* Dos botones abajo; si hay más, dos filas */
	for (int i = 0; i < n; i++) {
		s_btn[i].x = 12 + (i % 2) * 152;
		s_btn[i].y = (n <= 2 ? 205 : 170) + (i / 2) * 35;
		s_btn[i].w = 144;
		s_btn[i].h = 28;
		s_btn[i].enabled = true;
	}
}

static SceneId tr_update(const Input* in, float dt)
{
	if (button_update(&s_btn_menu, in))
		return SCENE_MENU;
	layout();
	int actor = tr_actor(&s_game);
	if (s_game.phase < TR_HAND_OVER && actor == 0) {
		if (s_envido_menu) {
			static const TrAction env[3] = { TR_ACT_ENVIDO, TR_ACT_REAL, TR_ACT_FALTA };
			for (int i = 0; i < 3; i++) {
				s_btn_env[i].enabled = tr_can(&s_game, 0, env[i]);
				if (button_update(&s_btn_env[i], in)) {
					act(0, env[i]);
					return SCENE_TRUCO;
				}
			}
			if (button_update(&s_btn_back, in))
				s_envido_menu = false;
			return SCENE_TRUCO;
		}
		int n = unused_count();
		if (s_game.phase == TR_PLAY && n > 0) {
			if (in->repeat & KEY_LEFT)
				s_cursor = (s_cursor + n - 1) % n;
			if (in->repeat & KEY_RIGHT)
				s_cursor = (s_cursor + 1) % n;
			if (s_cursor >= n)
				s_cursor = n - 1;
			int t = hand_touched(in, n, HAND_SC);
			if (t >= 0) {
				play(0, unused_index(t));
				return SCENE_TRUCO;
			}
		}
		for (int i = 0; i < 4; i++) {
			if (s_act[i] == TR_ACT_COUNT)
				continue;
			if (button_update(&s_btn[i], in)) {
				if (s_act[i] == TR_ACT_NONE)
					play(0, unused_index(s_cursor));
				else if (s_act[i] == TR_ACT_ENVIDO)
					s_envido_menu = true;
				else
					act(0, s_act[i]);
				break;
			}
		}
	} else if (s_game.phase < TR_HAND_OVER) {
		s_cpu_timer += dt;
		if (s_cpu_timer > 1.0f) {
			s_cpu_timer = 0.0f;
			int c;
			TrAction a = tr_ai(&s_game, 1, &c);
			if (a == TR_ACT_NONE)
				play(1, c);
			else
				act(1, a);
		}
	} else {
		bool over = s_game.phase == TR_GAME_OVER;
		button_set(&s_btn[0], 12, 205, 144, 28, over ? GLYPH_A " Otra partida" : GLYPH_A " Siguiente mano",
			C2D_Color32(40, 140, 60, 255), KEY_A);
		button_set(&s_btn[1], 164, 205, 144, 28, GLYPH_B " Menú", C2D_Color32(190, 60, 50, 255), KEY_B);
		s_end_time += dt;
		s_btn[0].enabled = s_btn[1].enabled = s_end_time > 0.6f;
		if (button_update(&s_btn[0], in)) {
			if (over)
				tr_new_game(&s_game);
			else
				tr_new_hand(&s_game);
			start_hand();
		} else if (button_update(&s_btn[1], in)) {
			return SCENE_MENU;
		}
	}
	if (s_game.phase != s_prev_phase) {
		if (s_game.phase >= TR_HAND_OVER) {
			s_end_time = 0.0f;
			int w = s_game.hand_winner;
			if (w >= 0) {
				table_mood(&s_table, seat(w), EXPR_HAPPY, 3.0f);
				table_mood(&s_table, seat(1 - w), EXPR_SAD, 3.0f);
			}
			if (s_game.phase == TR_GAME_OVER) {
				bool won = tr_winner(&s_game) == 0;
				profile_record_game(GAME_TRUCO, won);
				audio_play(won ? SFX_WIN : SFX_LOSE);
			}
		}
		s_prev_phase = s_game.phase;
	}
	table_update(&s_table, dt);
	return SCENE_TRUCO;
}

static void tr_draw_top(void)
{
	gfx_background(BG_TABLE_TOP);
	int left = 0;
	for (int i = 0; i < 3; i++)
		left += !s_game.used[1][i];
	char info[24];
	snprintf(info, sizeof info, "%d puntos", s_game.score[1]);
	table_draw_seat(&s_table, CPU_SEAT, left, tr_actor(&s_game) == 1 && s_game.phase < TR_HAND_OVER, info);
	/* Las tres bazas */
	for (int b = 0; b < 3; b++) {
		float x = BAZA_X(b);
		for (int p = 0; p < 2; p++)
			if (s_game.on_table[b][p]) {
				bool lost = s_game.baza_winner[b] >= 0 && s_game.baza_winner[b] != p &&
				            s_game.on_table[b][1 - p];
				gfx_card(s_game.table[b][p], x, p == 0 ? 170 : 114, 0.55f, 1.0f, (p ? -0.04f : 0.04f), DEPTH_CARDS,
					lost ? 0.45f : 0.0f, true);
			}
		if (s_game.on_table[b][0] && s_game.on_table[b][1] && s_game.baza_winner[b] < 0)
			gfx_text("Parda", x, 135, DEPTH_UI, 0.42f, COLOR_GOLD, C2D_AlignCenter);
	}
	/* Marcador */
	gfx_panel(306, 150, 88, 82, DEPTH_UI, C2D_Color32(10, 50, 25, 210), C2D_Color32(20, 60, 30, 255));
	gfx_text("A 15", 350, 154, DEPTH_UI, 0.42f, C2D_Color32(200, 230, 200, 255), C2D_AlignCenter);
	gfx_textf(350, 172, DEPTH_UI, 0.45f, COLOR_GOLD, C2D_AlignCenter, "Tú %d", s_game.score[0]);
	gfx_textf(350, 190, DEPTH_UI, 0.42f, COLOR_CREAM, C2D_AlignCenter, "%s %d", cpu(), s_game.score[1]);
	gfx_textf(350, 210, DEPTH_UI, 0.4f, C2D_Color32(200, 230, 200, 255), C2D_AlignCenter, "Vale %d",
		s_game.truco_level);
	char me[32];
	snprintf(me, sizeof me, "%s", s_game.mano == 0 ? "Eres mano" : "");
	table_draw_me(&s_table, me);
	table_draw_bubbles(&s_table);

	if (s_game.phase >= TR_HAND_OVER && banner_draw(s_end_time, 200, 118, 320, 80)) {
		bool over = s_game.phase == TR_GAME_OVER;
		int w = over ? tr_winner(&s_game) : s_game.hand_winner;
		char t[64];
		if (over)
			snprintf(t, sizeof t, w == 0 ? "¡Ganas la partida!" : "Gana %s", cpu());
		else
			snprintf(t, sizeof t, w == 0 ? "Ganas la mano (+%d)" : "La mano es de la CPU (+%d)", s_game.hand_points);
		gfx_text(t, 200, 92, DEPTH_BANNER, 0.6f, w == 0 ? COLOR_GOLD : COLOR_RED, C2D_AlignCenter);
		gfx_textf(200, 120, DEPTH_BANNER, 0.5f, COLOR_CREAM, C2D_AlignCenter, "Tú %d - %s %d", s_game.score[0], cpu(),
			s_game.score[1]);
	}
}

static void tr_draw_bottom(void)
{
	gfx_background(BG_TABLE_BOTTOM);
	table_draw_message(&s_table);
	button_draw(&s_btn_menu, false);
	int n = unused_count();
	Card cards[3];
	for (int k = 0; k < n; k++)
		cards[k] = s_game.hand[0][unused_index(k)];
	bool mine = tr_actor(&s_game) == 0 && s_game.phase < TR_HAND_OVER;
	if (s_envido_menu && mine) {
		gfx_textf(BOTTOM_W / 2, 30, 0, 0.5f, COLOR_GOLD, C2D_AlignCenter, "Tienes %d de envido. ¿Qué cantas?",
			tr_envido(s_game.dealt[0], 3));
		hand_draw(cards, n, HAND_SC * 0.8f, NULL, -1, NULL);
		for (int i = 0; i < 3; i++)
			button_draw(&s_btn_env[i], s_btn_env[i].enabled);
		button_draw(&s_btn_back, true);
		return;
	}
	if (mine) {
		const char* t = s_game.phase == TR_RESPOND
		                    ? (s_game.pending == TR_ACT_ENVIDO ? "Te cantan el envido: ¿quieres?" : "Te cantan el truco: ¿quieres?")
		                    : "¡Tu turno!";
		gfx_text(t, BOTTOM_W / 2, 28, 0, 0.5f, COLOR_GOLD, C2D_AlignCenter);
	}
	hand_draw(cards, n, HAND_SC, NULL, mine && s_game.phase == TR_PLAY ? s_cursor : -1, NULL);
	if (!s_game.envido_done && s_game.baza == 0)
		gfx_textf(BOTTOM_W / 2, 150, 0, 0.42f, C2D_Color32(200, 230, 200, 255), C2D_AlignCenter, "Tu envido: %d",
			tr_envido(s_game.dealt[0], 3));
	if (s_game.phase >= TR_HAND_OVER) {
		button_draw(&s_btn[0], s_btn[0].enabled);
		button_draw(&s_btn[1], s_btn[1].enabled);
		return;
	}
	for (int i = 0; i < 4; i++)
		if (s_act[i] != TR_ACT_COUNT)
			button_draw(&s_btn[i], true);
}

const Scene scene_truco = { tr_enter, tr_update, tr_draw_top, tr_draw_bottom };
