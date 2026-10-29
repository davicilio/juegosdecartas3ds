/* Corazones con la baraja francesa: evita los corazones y la reina de picas. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "games/corazones.h"
#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/gfx.h"
#include "ui/table.h"
#include "ui/widgets.h"

static const float TRICK_X[HE_PLAYERS] = { 200, 156, 200, 244 };
static const float TRICK_Y[HE_PLAYERS] = { 150, 116, 82, 116 };

#define HAND_SC 0.56f

static Corazones s_game;
static Table s_table;
static bool s_started;
static bool s_sel[HE_HAND];
static int s_cursor;
static float s_cpu_timer, s_trick_timer, s_end_time;
static HePhase s_prev_phase;
static Button s_btn_main, s_btn_menu, s_btn_alt;

static int fr_id(PkCard c)
{
	return c.suit * 13 + c.rank - 2;
}

static const char* pass_text(void)
{
	switch (he_pass_direction(&s_game)) {
	case 1:
		return "Pasa 3 cartas a la izquierda";
	case -1:
		return "Pasa 3 cartas a la derecha";
	case 2:
		return "Pasa 3 cartas al de enfrente";
	default:
		return "Esta mano no se pasan cartas";
	}
}

static void start_hand(void)
{
	memset(s_sel, 0, sizeof s_sel);
	s_cursor = 0;
	s_cpu_timer = s_trick_timer = s_end_time = 0.0f;
	s_prev_phase = s_game.phase;
	if (s_game.phase == HE_PASSING)
		table_message(&s_table, "%s", pass_text());
	else
		table_message(&s_table, "Sale el 2 de tréboles");
}

static void he_enter(void)
{
	if (!s_started) {
		table_init(&s_table, CPU_LOLA, CPU_PEPE, CPU_PACO);
		he_init(&s_game, (uint32_t)time(NULL) ^ 0x4EA7);
		s_started = true;
		start_hand();
	}
	button_set(&s_btn_menu, 258, 6, 54, 20, "Menú", C2D_Color32(120, 30, 35, 255), KEY_START);
}

static void play(int i)
{
	int p = s_game.current;
	PkCard c = s_game.hands[p][i];
	if (!he_play(&s_game, i))
		return;
	audio_play(SFX_CARD);
	if (c.suit == 0 && c.rank == 12) {
		table_bubble(&s_table, p, "¡La reina!");
		table_message(&s_table, "%s echa la reina de picas", p == 0 ? "Tú:" : table_name(&s_table, p));
	}
	if (s_game.phase == HE_TRICK_DONE)
		s_trick_timer = 0.0f;
}

static SceneId he_update(const Input* in, float dt)
{
	if (button_update(&s_btn_menu, in))
		return SCENE_MENU;
	const float y = 205, h = 28;
	int n = s_game.hand_count[0];
	switch (s_game.phase) {
	case HE_PASSING: {
		button_set(&s_btn_main, 12, y, 296, h, GLYPH_A " Pasar las 3 cartas elegidas", C2D_Color32(40, 140, 60, 255),
			KEY_A);
		int count = 0, idx[3];
		for (int i = 0; i < n; i++)
			if (s_sel[i] && count < 3)
				idx[count++] = i;
		s_btn_main.enabled = count == 3;
		if (in->repeat & KEY_LEFT)
			s_cursor = (s_cursor + n - 1) % n;
		if (in->repeat & KEY_RIGHT)
			s_cursor = (s_cursor + 1) % n;
		int t = hand_touched(in, n, HAND_SC);
		if (t >= 0 || (in->down & KEY_B)) {
			int k = t >= 0 ? t : s_cursor;
			s_cursor = k;
			if (s_sel[k] || count < 3)
				s_sel[k] = !s_sel[k];
			audio_play(SFX_SELECT);
		} else if (button_update(&s_btn_main, in)) {
			he_pass(&s_game, idx);
			memset(s_sel, 0, sizeof s_sel);
			table_message(&s_table, "Cartas pasadas. Sale el 2 de tréboles");
			audio_play(SFX_CARD);
		}
		break;
	}
	case HE_PLAYING:
		button_set(&s_btn_main, 12, y, 296, h, GLYPH_A " Echar", C2D_Color32(40, 140, 60, 255), KEY_A);
		s_btn_main.enabled = s_game.current == 0;
		if (s_game.current == 0) {
			if (in->repeat & KEY_LEFT)
				s_cursor = (s_cursor + n - 1) % n;
			if (in->repeat & KEY_RIGHT)
				s_cursor = (s_cursor + 1) % n;
			if (s_cursor >= n)
				s_cursor = n - 1;
			if (!he_can_play(&s_game, 0, s_cursor))
				for (int i = 0; i < n; i++)
					if (he_can_play(&s_game, 0, i)) {
						s_cursor = i;
						break;
					}
			int t = hand_touched(in, n, HAND_SC);
			int pick = t >= 0 ? t : button_update(&s_btn_main, in) ? s_cursor : -1;
			if (pick >= 0) {
				if (he_can_play(&s_game, 0, pick))
					play(pick);
				else {
					audio_play(SFX_ERROR);
					table_message(&s_table, s_game.tricks == 0 && s_game.played_in_trick == 0
					                            ? "Hay que salir con el 2 de tréboles"
					                            : "Esa carta no se puede echar ahora");
				}
			}
		} else {
			s_cpu_timer += dt;
			if (s_cpu_timer > 0.7f) {
				s_cpu_timer = 0.0f;
				play(he_ai_choose(&s_game, s_game.current));
			}
		}
		break;
	case HE_TRICK_DONE:
		s_btn_main.enabled = false;
		s_trick_timer += dt;
		if (s_trick_timer > 1.0f) {
			int w = s_game.trick_winner;
			int pts = 0;
			for (int p = 0; p < HE_PLAYERS; p++)
				pts += he_card_points(s_game.trick[p]);
			he_collect(&s_game);
			if (pts > 0) {
				table_message(&s_table, "%s %s %d punto%s", w == 0 ? "Te llevas" : table_name(&s_table, w),
					w == 0 ? "" : "se lleva", pts, pts == 1 ? "" : "s");
				table_mood(&s_table, w, EXPR_SAD, 1.2f);
			}
		}
		break;
	case HE_HAND_OVER:
	case HE_GAME_OVER: {
		bool over = s_game.phase == HE_GAME_OVER;
		button_set(&s_btn_main, 12, y, 144, h, over ? GLYPH_A " Otra partida" : GLYPH_A " Siguiente mano",
			C2D_Color32(40, 140, 60, 255), KEY_A);
		button_set(&s_btn_alt, 164, y, 144, h, GLYPH_B " Menú", C2D_Color32(190, 60, 50, 255), KEY_B);
		s_btn_main.enabled = s_btn_alt.enabled = s_end_time > 0.6f;
		s_end_time += dt;
		if (button_update(&s_btn_main, in)) {
			if (over)
				he_init(&s_game, (uint32_t)time(NULL));
			else
				he_new_hand(&s_game);
			start_hand();
		} else if (button_update(&s_btn_alt, in)) {
			return SCENE_MENU;
		}
		break;
	}
	}
	if (s_game.phase != s_prev_phase) {
		if (s_game.phase == HE_HAND_OVER || s_game.phase == HE_GAME_OVER) {
			s_end_time = 0.0f;
			if (s_game.moon >= 0) {
				table_bubble(&s_table, s_game.moon, "¡A la luna!");
				table_message(&s_table, "¡%s dispara a la luna!", s_game.moon == 0 ? "Tú" : table_name(&s_table, s_game.moon));
			}
			if (s_game.phase == HE_GAME_OVER) {
				int w = he_winner(&s_game);
				profile_record_game(GAME_CORAZONES, w == 0);
				audio_play(w == 0 ? SFX_WIN : SFX_LOSE);
			}
		}
		s_prev_phase = s_game.phase;
	}
	table_update(&s_table, dt);
	return SCENE_CORAZONES;
}

static void he_draw_top(void)
{
	gfx_background(BG_TABLE_TOP);
	for (int p = 1; p < HE_PLAYERS; p++) {
		char info[24];
		snprintf(info, sizeof info, "%d (+%d)", s_game.score[p], s_game.points[p]);
		bool turn = s_game.current == p && s_game.phase == HE_PLAYING;
		table_draw_seat(&s_table, p, 0, turn, info);
	}
	if (s_game.phase == HE_PLAYING || s_game.phase == HE_TRICK_DONE)
		for (int k = 0; k < s_game.played_in_trick; k++) {
			int p = (s_game.leader + k) % HE_PLAYERS;
			gfx_french(fr_id(s_game.trick[p]), TRICK_X[p], TRICK_Y[p], 0.6f, (p - 1.5f) * 0.05f, DEPTH_CARDS + k * 0.1f,
				true);
		}
	if (!s_game.hearts_broken && s_game.phase == HE_PLAYING)
		gfx_text("Corazones sin romper", 330, 180, DEPTH_UI, 0.4f, C2D_Color32(200, 230, 200, 255), C2D_AlignCenter);
	char me[40];
	snprintf(me, sizeof me, "Tus puntos: %d (+%d)", s_game.score[0], s_game.points[0]);
	table_draw_me(&s_table, me);
	table_draw_bubbles(&s_table);
	if ((s_game.phase == HE_HAND_OVER || s_game.phase == HE_GAME_OVER) && banner_draw(s_end_time, 200, 112, 340, 96)) {
		if (s_game.phase == HE_GAME_OVER) {
			int w = he_winner(&s_game);
			gfx_text(w == 0 ? "¡Ganas la partida!" : "Fin de la partida", 200, 70, DEPTH_BANNER, 0.65f,
				w == 0 ? COLOR_GOLD : COLOR_RED, C2D_AlignCenter);
		} else {
			gfx_text("Fin de la mano", 200, 70, DEPTH_BANNER, 0.65f, COLOR_GOLD, C2D_AlignCenter);
		}
		gfx_textf(200, 100, DEPTH_BANNER, 0.45f, COLOR_CREAM, C2D_AlignCenter, "Esta mano: tú %d · %d · %d · %d",
			s_game.points[0], s_game.points[1], s_game.points[2], s_game.points[3]);
		gfx_textf(200, 122, DEPTH_BANNER, 0.45f, COLOR_GOLD, C2D_AlignCenter, "Total: tú %d · %d · %d · %d (a %d)",
			s_game.score[0], s_game.score[1], s_game.score[2], s_game.score[3], HE_END);
	}
}

static void he_draw_bottom(void)
{
	gfx_background(BG_TABLE_BOTTOM);
	table_draw_message(&s_table);
	button_draw(&s_btn_menu, false);
	int n = s_game.hand_count[0];
	bool playable[HE_HAND];
	bool mine = s_game.phase == HE_PLAYING && s_game.current == 0;
	for (int i = 0; i < n; i++)
		playable[i] = !mine || he_can_play(&s_game, 0, i);
	if (s_game.phase == HE_PASSING)
		gfx_text("Toca (o " GLYPH_B ") las tres cartas que quieres pasar", BOTTOM_W / 2, 30, 0, 0.45f, COLOR_GOLD,
			C2D_AlignCenter);
	else if (mine)
		gfx_text("¡Tu turno!", BOTTOM_W / 2, 30, 0, 0.55f, COLOR_GOLD, C2D_AlignCenter);
	hand_draw_french(s_game.hands[0], n, HAND_SC, s_game.phase == HE_PASSING ? s_sel : NULL,
		(mine || s_game.phase == HE_PASSING) ? s_cursor : -1, playable);
	gfx_text("Corazón 1 punto · reina de picas 13", BOTTOM_W / 2, 172, 0, 0.42f, C2D_Color32(200, 230, 200, 255),
		C2D_AlignCenter);
	button_draw(&s_btn_main, s_btn_main.enabled);
	if (s_game.phase == HE_HAND_OVER || s_game.phase == HE_GAME_OVER)
		button_draw(&s_btn_alt, s_btn_alt.enabled);
}

const Scene scene_corazones = { he_enter, he_update, he_draw_top, he_draw_bottom };
