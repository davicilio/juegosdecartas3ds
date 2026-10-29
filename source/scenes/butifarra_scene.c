/* Butifarra por parejas: tu compañero se sienta enfrente (arriba). */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "games/butifarra.h"
#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/gfx.h"
#include "ui/table.h"
#include "ui/widgets.h"

static const float TRICK_X[BU_PLAYERS] = { 200, 154, 200, 246 };
static const float TRICK_Y[BU_PLAYERS] = { 148, 114, 80, 114 };
static const char* const SUIT_CAP[NUM_SUITS] = { "Oros", "Copas", "Espadas", "Bastos" };
static const char* const DOUBLE_WORD[3] = { "Contro", "Recontro", "Sant Vicenç" };

#define HAND_SC 0.5f

static Butifarra s_game;
static Table s_table;
static bool s_started;
static int s_cursor;
static float s_cpu_timer, s_trick_timer, s_end_time;
static BuPhase s_prev_phase;
static Button s_btn_main, s_btn_alt, s_btn_menu, s_btn_choice[6];

static const char* who(int p)
{
	return p == 0 ? "Tú" : table_name(&s_table, p);
}

static const char* trump_text(void)
{
	return s_game.butifarra ? "butifarra" : s_game.trump >= 0 ? suit_name((Suit)s_game.trump) : "-";
}

static void start_hand(void)
{
	s_cursor = 0;
	s_cpu_timer = s_trick_timer = s_end_time = 0.0f;
	s_prev_phase = s_game.phase;
	table_message(&s_table, "Reparte %s, que elige el triunfo", who(s_game.dealer));
}

static void bu_enter(void)
{
	if (!s_started) {
		table_init(&s_table, CPU_PEPE, CPU_LUCIA, CPU_RAMON);
		bu_init(&s_game, (uint32_t)time(NULL) ^ 0xB071);
		s_started = true;
		start_hand();
	}
	button_set(&s_btn_menu, 258, 6, 54, 20, "Menú", C2D_Color32(120, 30, 35, 255), KEY_START);
	for (int i = 0; i < NUM_SUITS; i++)
		button_set(&s_btn_choice[i], 12 + (i % 2) * 152, 120 + (i / 2) * 32, 144, 28, SUIT_CAP[i],
			C2D_Color32(60, 90, 160, 255), 0);
	button_set(&s_btn_choice[BU_BUTIFARRA], 12, 184, 144, 28, "¡Butifarra!", C2D_Color32(190, 120, 30, 255), 0);
	button_set(&s_btn_choice[BU_DELEGATE], 164, 184, 144, 28, "Delego", C2D_Color32(110, 110, 110, 255), 0);
}

static void after_choose(int p, int choice)
{
	if (choice == BU_DELEGATE) {
		table_bubble(&s_table, p, "Delego");
		table_message(&s_table, "%s delega: elige %s", who(p), s_game.chooser == 0 ? "tú" : who(s_game.chooser));
	} else if (choice == BU_BUTIFARRA) {
		table_bubble(&s_table, p, "¡Butifarra!");
		table_message(&s_table, "%s %s butifarra: sin triunfo y todo doble", who(p), p == 0 ? "cantas" : "canta");
	} else {
		table_bubble(&s_table, p, SUIT_CAP[choice]);
		table_message(&s_table, "Triunfo: %s", suit_name((Suit)choice));
	}
	audio_play(SFX_CHIP);
}

static void after_double(void)
{
	int p = s_game.last_double_player;
	if (s_game.last_double) {
		table_bubble(&s_table, p, DOUBLE_WORD[s_game.doubles - 1]);
		table_message(&s_table, "%s: ¡%s! Ahora vale x%d", who(p), DOUBLE_WORD[s_game.doubles - 1],
			bu_multiplier(&s_game));
		audio_play(SFX_CHIP);
	} else if (s_game.doubles == 0) {
		table_bubble(&s_table, p, "Bien");
	}
	if (s_game.phase == BU_PLAYING)
		table_message(&s_table, "Triunfo: %s (x%d). Sale %s", trump_text(), bu_multiplier(&s_game),
			s_game.current == 0 ? "tú" : who(s_game.current));
}

static SceneId bu_update(const Input* in, float dt)
{
	if (button_update(&s_btn_menu, in))
		return SCENE_MENU;
	const float y = 205, h = 28;
	int n = s_game.hand_count[0];
	bool mine = s_game.current == 0;
	switch (s_game.phase) {
	case BU_CHOOSE:
		if (mine) {
			s_btn_choice[BU_DELEGATE].enabled = bu_can_delegate(&s_game);
			for (int i = 0; i < 6; i++) {
				if (i == BU_DELEGATE && !s_btn_choice[i].enabled)
					continue;
				if (button_update(&s_btn_choice[i], in) && bu_choose(&s_game, i)) {
					after_choose(0, i);
					break;
				}
			}
		}
		break;
	case BU_DOUBLE: {
		static char label[32];
		snprintf(label, sizeof label, GLYPH_A " %s", DOUBLE_WORD[s_game.doubles]);
		button_set(&s_btn_main, 12, y, 144, h, label, C2D_Color32(190, 60, 50, 255), KEY_A);
		button_set(&s_btn_alt, 164, y, 144, h, GLYPH_B " Bien", C2D_Color32(40, 140, 60, 255), KEY_B);
		s_btn_main.enabled = s_btn_alt.enabled = mine;
		if (mine) {
			if (button_update(&s_btn_main, in)) {
				bu_double(&s_game, true);
				after_double();
			} else if (button_update(&s_btn_alt, in)) {
				bu_double(&s_game, false);
				after_double();
			}
		}
		break;
	}
	case BU_PLAYING:
		button_set(&s_btn_main, 12, y, 296, h, GLYPH_A " Echar", C2D_Color32(40, 140, 60, 255), KEY_A);
		s_btn_main.enabled = mine;
		if (mine) {
			if (in->repeat & KEY_LEFT)
				s_cursor = (s_cursor + n - 1) % n;
			if (in->repeat & KEY_RIGHT)
				s_cursor = (s_cursor + 1) % n;
			if (s_cursor >= n)
				s_cursor = n - 1;
			if (!bu_can_play(&s_game, 0, s_cursor))
				for (int i = 0; i < n; i++)
					if (bu_can_play(&s_game, 0, i)) {
						s_cursor = i;
						break;
					}
			int t = hand_touched(in, n, HAND_SC);
			int pick = t >= 0 ? t : button_update(&s_btn_main, in) ? s_cursor : -1;
			if (pick >= 0) {
				if (bu_play(&s_game, pick)) {
					audio_play(SFX_CARD);
					s_trick_timer = 0.0f;
				} else {
					audio_play(SFX_ERROR);
					table_message(&s_table, "Hay que asistir y matar a los rivales; si no tienes, fallar");
				}
			}
		}
		break;
	case BU_TRICK_DONE:
		s_trick_timer += dt;
		if (s_trick_timer > 1.0f) {
			int w = s_game.trick_winner;
			bu_collect(&s_game);
			if (w == 0)
				table_message(&s_table, "Te llevas la baza");
			else
				table_message(&s_table, "%s se lleva la baza", who(w));
		}
		break;
	case BU_HAND_OVER:
	case BU_GAME_OVER: {
		bool over = s_game.phase == BU_GAME_OVER;
		button_set(&s_btn_main, 12, y, 144, h, over ? GLYPH_A " Otra partida" : GLYPH_A " Siguiente mano",
			C2D_Color32(40, 140, 60, 255), KEY_A);
		button_set(&s_btn_alt, 164, y, 144, h, GLYPH_B " Menú", C2D_Color32(190, 60, 50, 255), KEY_B);
		s_end_time += dt;
		s_btn_main.enabled = s_btn_alt.enabled = s_end_time > 0.6f;
		if (button_update(&s_btn_main, in)) {
			if (over)
				bu_new_game(&s_game);
			else
				bu_new_hand(&s_game);
			start_hand();
		} else if (button_update(&s_btn_alt, in)) {
			return SCENE_MENU;
		}
		break;
	}
	}
	if (!mine && s_game.phase <= BU_PLAYING) {
		s_cpu_timer += dt;
		if (s_cpu_timer > 0.8f) {
			s_cpu_timer = 0.0f;
			int p = s_game.current;
			if (s_game.phase == BU_CHOOSE) {
				int c = bu_ai_choose(&s_game, p);
				bu_choose(&s_game, c);
				after_choose(p, c);
			} else if (s_game.phase == BU_DOUBLE) {
				bu_double(&s_game, bu_ai_double(&s_game, p));
				after_double();
			} else if (bu_play(&s_game, bu_ai_play(&s_game, p))) {
				audio_play(SFX_CARD);
				s_trick_timer = 0.0f;
			}
		}
	}
	if (s_game.phase != s_prev_phase) {
		if (s_game.phase >= BU_HAND_OVER) {
			s_end_time = 0.0f;
			int t = s_game.points[0] > 36 ? 0 : s_game.points[1] > 36 ? 1 : -1;
			for (int p = 0; p < BU_PLAYERS; p++)
				if (t >= 0)
					table_mood(&s_table, p, bu_team(p) == t ? EXPR_HAPPY : EXPR_SAD, 3.0f);
			if (s_game.phase == BU_GAME_OVER) {
				bool won = bu_winner(&s_game) == bu_team(0);
				profile_record_game(GAME_BUTIFARRA, won);
				audio_play(won ? SFX_WIN : SFX_LOSE);
			}
		}
		s_prev_phase = s_game.phase;
	}
	table_update(&s_table, dt);
	return SCENE_BUTIFARRA;
}

static void bu_draw_top(void)
{
	gfx_background(BG_TABLE_TOP);
	for (int p = 1; p < BU_PLAYERS; p++) {
		bool turn = s_game.current == p && s_game.phase <= BU_PLAYING;
		table_draw_seat(&s_table, p, s_game.hand_count[p] > 8 ? 8 : s_game.hand_count[p], turn,
			p == 2 ? "compañero" : p == s_game.dealer ? "reparte" : NULL);
	}
	/* Triunfo y marcador */
	gfx_panel(306, 150, 88, 82, DEPTH_UI, C2D_Color32(10, 50, 25, 210), C2D_Color32(20, 60, 30, 255));
	if (s_game.phase >= BU_DOUBLE) {
		if (s_game.butifarra)
			gfx_text("Butifarra", 350, 154, DEPTH_UI, 0.45f, COLOR_GOLD, C2D_AlignCenter);
		else {
			gfx_sprite(SPR_SUIT_FIRST + s_game.trump, 330, 163, 0.5f, DEPTH_UI);
			gfx_text(SUIT_CAP[s_game.trump], 362, 154, DEPTH_UI, 0.42f, COLOR_GOLD, C2D_AlignCenter);
		}
		gfx_textf(350, 172, DEPTH_UI, 0.42f, C2D_Color32(200, 230, 200, 255), C2D_AlignCenter, "x%d",
			bu_multiplier(&s_game));
	}
	gfx_textf(350, 192, DEPTH_UI, 0.42f, C2D_Color32(140, 200, 255, 255), C2D_AlignCenter, "Nos. %d (%d)",
		s_game.score[0], s_game.points[0]);
	gfx_textf(350, 210, DEPTH_UI, 0.42f, C2D_Color32(255, 150, 140, 255), C2D_AlignCenter, "Ellos %d (%d)",
		s_game.score[1], s_game.points[1]);
	if (s_game.phase == BU_PLAYING || s_game.phase == BU_TRICK_DONE)
		for (int k = 0; k < s_game.played_in_trick; k++) {
			int p = (s_game.leader + k) % BU_PLAYERS;
			gfx_card(s_game.trick[p], TRICK_X[p], TRICK_Y[p], 0.55f, 1.0f, (p - 1.5f) * 0.06f, DEPTH_CARDS + k * 0.1f,
				0.0f, true);
		}
	table_draw_me(&s_table, s_game.dealer == 0 ? "Repartes tú" : "");
	table_draw_bubbles(&s_table);

	if (s_game.phase >= BU_HAND_OVER && banner_draw(s_end_time, 200, 112, 340, 96)) {
		bool over = s_game.phase == BU_GAME_OVER;
		int w = over ? bu_winner(&s_game) : s_game.hand_gain[0] > 0 ? 0 : s_game.hand_gain[1] > 0 ? 1 : -1;
		const char* t = over ? (w == 0 ? "¡Ganáis la partida!" : "Ganan los rivales")
		                     : w == 0 ? "¡Ganáis la mano!" : w == 1 ? "Los rivales ganan la mano" : "Empate a 36";
		gfx_text(t, 200, 72, DEPTH_BANNER, 0.62f, w == 0 ? COLOR_GOLD : COLOR_RED, C2D_AlignCenter);
		gfx_textf(200, 98, DEPTH_BANNER, 0.47f, COLOR_CREAM, C2D_AlignCenter, "Nosotros %d - ellos %d de 72 (x%d)",
			s_game.points[0], s_game.points[1], bu_multiplier(&s_game));
		gfx_textf(200, 122, DEPTH_BANNER, 0.5f, COLOR_GOLD, C2D_AlignCenter, "Total: %d - %d (a %d)", s_game.score[0],
			s_game.score[1], BU_GAME);
	}
}

static void bu_draw_bottom(void)
{
	gfx_background(BG_TABLE_BOTTOM);
	table_draw_message(&s_table);
	button_draw(&s_btn_menu, false);
	int n = s_game.hand_count[0];
	bool mine = s_game.current == 0;
	bool playable[BU_HAND];
	for (int i = 0; i < n; i++)
		playable[i] = !(mine && s_game.phase == BU_PLAYING) || bu_can_play(&s_game, 0, i);
	if (mine && s_game.phase == BU_CHOOSE) {
		gfx_text(s_game.delegated ? "Te han delegado: elige el triunfo" : "Elige el triunfo, butifarra o delega",
			BOTTOM_W / 2, 28, 0, 0.47f, COLOR_GOLD, C2D_AlignCenter);
		hand_draw(s_game.hands[0], n, 0.45f, NULL, -1, NULL);
		for (int i = 0; i < 6; i++)
			if (i != BU_DELEGATE || s_btn_choice[i].enabled)
				button_draw(&s_btn_choice[i], true);
		for (int i = 0; i < NUM_SUITS; i++)
			gfx_sprite_ex(SPR_SUIT_FIRST + i, s_btn_choice[i].x + 18, s_btn_choice[i].y + 14, 0.45f, 0.45f, 0.0f, 0,
				NULL);
		return;
	}
	if (mine && s_game.phase == BU_DOUBLE)
		gfx_textf(BOTTOM_W / 2, 28, 0, 0.47f, COLOR_GOLD, C2D_AlignCenter, "Triunfo: %s. ¿%s?", trump_text(),
			DOUBLE_WORD[s_game.doubles]);
	else if (mine && s_game.phase == BU_PLAYING)
		gfx_text("¡Tu turno!", BOTTOM_W / 2, 28, 0, 0.5f, COLOR_GOLD, C2D_AlignCenter);
	hand_draw(s_game.hands[0], n, HAND_SC, NULL, mine && s_game.phase == BU_PLAYING ? s_cursor : -1, playable);
	gfx_text("Manilla (9) 5 · as 4 · rey 3 · caballo 2 · sota 1", BOTTOM_W / 2, 176, 0, 0.38f,
		C2D_Color32(200, 230, 200, 255), C2D_AlignCenter);
	if (s_game.phase == BU_DOUBLE || s_game.phase == BU_PLAYING || s_game.phase >= BU_HAND_OVER)
		button_draw(&s_btn_main, s_btn_main.enabled);
	if (s_game.phase == BU_DOUBLE || s_game.phase >= BU_HAND_OVER)
		button_draw(&s_btn_alt, s_btn_alt.enabled);
}

const Scene scene_butifarra = { bu_enter, bu_update, bu_draw_top, bu_draw_bottom };
