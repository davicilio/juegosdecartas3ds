/* As, 2, 3: cada uno echa su carta de arriba contando as, dos, tres... y si
 * coincide con lo que dice, se lleva el montón. */
#include <math.h>
#include <stdio.h>
#include <time.h>

#include "games/as23.h"
#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/gfx.h"
#include "ui/table.h"
#include "ui/widgets.h"

#define STACK_X     90.0f
#define STACK_Y     118.0f
#define STACK_SCALE 0.8f

/* Asiento de la mesa de cada jugador según cuántos sean */
static const int SEATS[3][AS23_MAX_PLAYERS] = { { 0, 2 }, { 0, 1, 3 }, { 0, 1, 2, 3 } };

static As23 s_game;
static Table s_table;
static bool s_started;
static int s_players = 4;
static float s_cpu_timer, s_end_time, s_flip;
static bool s_recorded;
static Button s_btn_play, s_btn_players, s_btn_menu;

static int seat(int p)
{
	return SEATS[s_players - 2][p];
}

static const char* say(int rank)
{
	static const char* const words[] = { "", "As", "Dos", "Tres", "Cuatro", "Cinco", "Seis", "Siete", "", "", "Sota",
		"Caballo", "Rey" };
	return words[rank];
}

static void new_game(void)
{
	as23_init(&s_game, s_players, (uint32_t)time(NULL) ^ 0xA523);
	s_cpu_timer = s_end_time = 0.0f;
	s_flip = 1.0f;
	s_recorded = false;
	table_message(&s_table, "Empieza %s. Se cuenta desde el as",
		s_game.current == 0 ? "tú" : table_name(&s_table, seat(s_game.current)));
}

static void as23_enter(void)
{
	if (!s_started) {
		table_init(&s_table, CPU_MANOLO, CPU_LUCIA, CPU_RAMON);
		s_started = true;
		new_game();
	}
	button_set(&s_btn_menu, 258, 6, 54, 20, "Menú", C2D_Color32(120, 30, 35, 255), KEY_START);
	button_set(&s_btn_play, 164, 205, 144, 28, GLYPH_A " Echar carta", C2D_Color32(40, 140, 60, 255), KEY_A);
}

static void play(void)
{
	int p = s_game.current, s = seat(p);
	if (!as23_play(&s_game))
		return;
	audio_play(SFX_CARD);
	s_flip = 0.0f;
	table_bubble(&s_table, s, say(s_game.last_said));
	char name[32];
	card_describe_article(s_game.last_card, name, sizeof name);
	if (s_game.last_match) {
		audio_play(p == 0 ? SFX_LOSE : SFX_CHIP);
		table_mood(&s_table, s, EXPR_SAD, 1.5f);
		if (p == 0)
			table_message(&s_table, "¡Coincide! Sale %s y te llevas %d cartas", name, s_game.last_taken);
		else
			table_message(&s_table, "¡Coincide! %s se lleva %d cartas", table_name(&s_table, s), s_game.last_taken);
		for (int q = 0; q < s_players; q++)
			if (q != p)
				table_mood(&s_table, seat(q), EXPR_HAPPY, 1.2f);
		s_cpu_timer = -0.8f; /* una pausa para verlo */
	} else if (!as23_over(&s_game)) {
		table_message(&s_table, "%s \"%s\" y sale %s", p == 0 ? "Dices" : table_name(&s_table, s), say(s_game.last_said),
			name);
	}
}

static SceneId as23_update(const Input* in, float dt)
{
	if (button_update(&s_btn_menu, in))
		return SCENE_MENU;
	static char players_label[24];
	snprintf(players_label, sizeof players_label, GLYPH_X " %d jugadores", s_players);
	button_set(&s_btn_players, 12, 205, 144, 28, players_label, C2D_Color32(60, 90, 160, 255), KEY_X);
	if (button_update(&s_btn_players, in)) {
		if (!s_recorded && !as23_over(&s_game) && s_game.plays > 0)
			profile_record_game(GAME_AS23, false);
		s_players = s_players == 4 ? 2 : s_players + 1;
		new_game();
		return SCENE_AS23;
	}
	s_flip = fminf(1.0f, s_flip + dt * 5.0f);
	if (!as23_over(&s_game)) {
		bool mine = s_game.current == 0;
		s_btn_play.enabled = mine && s_cpu_timer >= 0.0f;
		if (mine) {
			if (s_cpu_timer < 0.0f)
				s_cpu_timer += dt;
			bool touched = in->touch_down && fabsf(in->tx - STACK_X) < CARD_W * STACK_SCALE / 2 &&
			               fabsf(in->ty - STACK_Y) < CARD_H * STACK_SCALE / 2;
			if (s_btn_play.enabled && (touched || button_update(&s_btn_play, in)))
				play();
		} else {
			s_cpu_timer += dt;
			if (s_cpu_timer > 0.75f) {
				s_cpu_timer = 0.0f;
				play();
			}
		}
	} else {
		if (!s_recorded) {
			s_recorded = true;
			bool won = s_game.winner == 0;
			profile_record_game(GAME_AS23, won);
			audio_play(won ? SFX_WIN : SFX_LOSE);
			for (int p = 0; p < s_players; p++)
				table_mood(&s_table, seat(p), p == s_game.winner ? EXPR_HAPPY : EXPR_SAD, 5.0f);
			if (won)
				table_message(&s_table, "¡Te has quedado sin cartas!");
			else
				table_message(&s_table, "%s se queda sin cartas", table_name(&s_table, seat(s_game.winner)));
		}
		s_end_time += dt;
		button_set(&s_btn_play, 164, 205, 144, 28, GLYPH_A " Otra partida", C2D_Color32(40, 140, 60, 255), KEY_A);
		s_btn_play.enabled = s_end_time > 0.6f;
		if (button_update(&s_btn_play, in)) {
			new_game();
			button_set(&s_btn_play, 164, 205, 144, 28, GLYPH_A " Echar carta", C2D_Color32(40, 140, 60, 255), KEY_A);
		}
	}
	table_update(&s_table, dt);
	return SCENE_AS23;
}

static void as23_draw_top(void)
{
	gfx_background(BG_TABLE_TOP);
	for (int p = 1; p < s_players; p++) {
		char info[24];
		snprintf(info, sizeof info, "%d cartas", s_game.stack_count[p]);
		table_draw_seat(&s_table, seat(p), s_game.stack_count[p], s_game.current == p && !as23_over(&s_game), info);
	}
	/* El montón del centro: la de arriba boca arriba */
	int n = s_game.pile_count;
	for (int i = 0; i < n - 1 && i < 10; i++)
		gfx_card_back(200 + (i % 3 - 1) * 2.0f, 116 - i * 1.0f, 0.55f, 0.55f, (i % 5 - 2) * 0.08f,
			DEPTH_CARDS + i * 0.03f, NULL);
	if (n > 0)
		gfx_card(s_game.pile[n - 1], 200, 112 - fminf(n, 10) * 1.0f, 0.62f, s_flip, 0.05f, DEPTH_CARDS + 0.4f, 0.0f, true);
	else if (s_game.last_match && s_game.last_player >= 0)
		gfx_card(s_game.last_card, 200, 112, 0.62f, 1.0f, 0.0f, DEPTH_CARDS + 0.4f, 0.4f, true);
	gfx_textf(200, 160, DEPTH_UI, 0.45f, COLOR_CREAM, C2D_AlignCenter, "Montón: %d", n);
	char me[32];
	snprintf(me, sizeof me, "Tu mazo: %d", s_game.stack_count[0]);
	table_draw_me(&s_table, me);
	table_draw_bubbles(&s_table);

	if (as23_over(&s_game) && banner_draw(s_end_time, 200, 112, 320, 70)) {
		if (s_game.winner == 0)
			gfx_text("¡Ganas! Te has quedado sin cartas", 200, 100, DEPTH_BANNER, 0.6f, COLOR_GOLD, C2D_AlignCenter);
		else
			gfx_textf(200, 100, DEPTH_BANNER, 0.6f, COLOR_CREAM, C2D_AlignCenter, "Gana %s",
				table_name(&s_table, seat(s_game.winner)));
	}
}

static void as23_draw_bottom(void)
{
	gfx_background(BG_TABLE_BOTTOM);
	table_draw_message(&s_table);
	button_draw(&s_btn_menu, false);

	/* Tu mazo, boca abajo */
	int n = s_game.stack_count[0];
	bool mine = !as23_over(&s_game) && s_game.current == 0 && s_cpu_timer >= 0.0f;
	for (int i = 0; i < n && i < 12; i++) {
		float y = STACK_Y + 6 - i * 1.0f;
		if (i == (n < 12 ? n : 12) - 1 && mine) {
			C2D_ImageTint tint;
			C2D_AlphaImageTint(&tint, 0.75f + 0.25f * sinf(gfx_time() * 6.0f));
			gfx_sprite_ex(SPR_CARD_GLOW, STACK_X, y, STACK_SCALE, STACK_SCALE, 0.0f, 0, &tint);
		}
		gfx_card_back(STACK_X, y, STACK_SCALE, STACK_SCALE, 0.0f, 0, NULL);
	}
	gfx_textf(STACK_X, 176, 0, 0.45f, COLOR_CREAM, C2D_AlignCenter, "%d cartas", n);

	/* Lo que toca decir */
	if (!as23_over(&s_game)) {
		gfx_text(mine ? "Te toca decir" : "Ahora se dice", 228, 62, 0, 0.5f, C2D_Color32(200, 230, 200, 255),
			C2D_AlignCenter);
		const char* w = say(as23_called(&s_game));
		gfx_text(w, 228, 82, 0, 1.0f, mine ? COLOR_GOLD : COLOR_CREAM, C2D_AlignCenter);
		/* La cuenta, con lo que toca resaltado */
		static const char* const seq[RANKS_PER_SUIT] = { "As", "2", "3", "4", "5", "6", "7", "S", "C", "R" };
		for (int i = 0; i < RANKS_PER_SUIT; i++) {
			bool now = i == s_game.count;
			gfx_text(seq[i], 220 + (i - 4.5f) * 18.0f, now ? 124 : 128, 0, now ? 0.6f : 0.45f,
				now ? COLOR_GOLD : C2D_Color32(170, 200, 170, 255), C2D_AlignCenter);
		}
		if (mine)
			gfx_text("Toca tu mazo o pulsa " GLYPH_A, 228, 160, 0, 0.42f, C2D_Color32(200, 230, 200, 255),
				C2D_AlignCenter);
	}
	button_draw(&s_btn_players, true);
	button_draw(&s_btn_play, s_btn_play.enabled);
}

/* Salir a medias desde la pausa: la próxima vez se empieza de cero */
static void scene_leave(void)
{
	s_started = false;
}

const Scene scene_as23 = { as23_enter, as23_update, as23_draw_top, as23_draw_bottom, scene_leave };
