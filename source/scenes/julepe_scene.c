/* Julepe: decide si juegas, cambia cartas y haz al menos dos bazas. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "games/julepe.h"
#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/gfx.h"
#include "ui/table.h"
#include "ui/widgets.h"
#include "ui/wallet.h"

static const float TRICK_X[JU_PLAYERS] = { 200, 150, 200, 250 };
static const float TRICK_Y[JU_PLAYERS] = { 146, 112, 80, 112 };

#define HAND_SC 0.75f

static Julepe s_game;
static Table s_table;
static bool s_started;
static bool s_on_table; /* hay fichas del monedero en la mesa */
static bool s_sel[JU_HAND];
static int s_cursor;
static float s_cpu_timer, s_trick_timer, s_end_time;
static JuPhase s_prev_phase;
static Button s_btn_main, s_btn_alt, s_btn_menu;

static const char* who(int p)
{
	return p == 0 ? "Tú" : table_name(&s_table, p);
}

static void start_hand(void)
{
	memset(s_sel, 0, sizeof s_sel);
	s_cursor = 0;
	s_cpu_timer = s_trick_timer = s_end_time = 0.0f;
	s_prev_phase = s_game.phase;
	char trump[32];
	card_describe(s_game.trump_card, trump, sizeof trump);
	table_message(&s_table, "Reparte %s. Pinta: %s. Plato: %d", who(s_game.dealer), trump, s_game.pot);
}

/* Te sientas con fichas del monedero (en vez de las JU_START de la partida) */
static void buy_in(void)
{
	s_game.chips[0] += wallet_take(JU_START) - JU_START;
	s_on_table = true;
}

/* Tus fichas vuelven al monedero (y si debes, se paga de él). Si la mano no
 * ha acabado, se anula y recuperas también lo que has puesto en el plato. */
static void cash_out(void)
{
	if (!s_on_table)
		return;
	int n = s_game.chips[0];
	if (s_game.phase < JU_HAND_OVER && s_game.dealer == 0)
		n += JU_ANTE;
	if (n > 0)
		wallet_give(n);
	else
		wallet_take(-n);
	s_on_table = false;
}

static void ju_enter(void)
{
	if (!s_started) {
		table_init(&s_table, CPU_RAMON, CPU_LOLA, CPU_PEPE);
		ju_init(&s_game, (uint32_t)time(NULL) ^ 0x1E3E);
		buy_in();
		s_started = true;
		start_hand();
	}
	button_set(&s_btn_menu, 258, 6, 54, 20, "Menú", C2D_Color32(120, 30, 35, 255), KEY_START);
}

static void decide(bool play)
{
	int p = s_game.current;
	if (!ju_decide(&s_game, play))
		return;
	table_bubble(&s_table, p, play ? "¡Juego!" : "Paso");
	audio_play(SFX_SELECT);
	if (s_game.phase == JU_DISCARDING)
		table_message(&s_table, "Juegan %d. Ahora se cambian cartas", ju_entered(&s_game));
}

static void discard(const bool* sel)
{
	int p = s_game.current;
	if (!ju_discard(&s_game, sel))
		return;
	char buf[24];
	snprintf(buf, sizeof buf, s_game.discarded[p] ? "Cambio %d" : "Servido", s_game.discarded[p]);
	table_bubble(&s_table, p, buf);
	audio_play(SFX_CARD);
	if (s_game.phase == JU_PLAYING)
		table_message(&s_table, "Sale %s", s_game.current == 0 ? "tú" : table_name(&s_table, s_game.current));
}

static void play(int i)
{
	if (!ju_play(&s_game, i))
		return;
	audio_play(SFX_CARD);
	if (s_game.phase == JU_TRICK_DONE)
		s_trick_timer = 0.0f;
}

static SceneId ju_update(const Input* in, float dt)
{
	if (button_update(&s_btn_menu, in))
		return SCENE_MENU;
	const float y = 205, h = 28;
	int n = s_game.hand_count[0];
	bool mine = s_game.current == 0;
	switch (s_game.phase) {
	case JU_DECIDING:
		button_set(&s_btn_main, 12, y, 144, h, GLYPH_A " Juego", C2D_Color32(40, 140, 60, 255), KEY_A);
		button_set(&s_btn_alt, 164, y, 144, h, GLYPH_B " Paso", C2D_Color32(190, 60, 50, 255), KEY_B);
		s_btn_main.enabled = mine;
		s_btn_alt.enabled = mine && !ju_must_play(&s_game, 0);
		if (mine) {
			if (button_update(&s_btn_main, in))
				decide(true);
			else if (button_update(&s_btn_alt, in))
				decide(false);
		}
		break;
	case JU_DISCARDING: {
		int count = 0;
		for (int i = 0; i < n; i++)
			count += s_sel[i];
		static char label[32];
		snprintf(label, sizeof label, count ? GLYPH_A " Cambiar %d" : GLYPH_A " Servido", count);
		button_set(&s_btn_main, 12, y, 296, h, label, C2D_Color32(40, 140, 60, 255), KEY_A);
		s_btn_main.enabled = mine;
		if (mine) {
			if (in->repeat & KEY_LEFT)
				s_cursor = (s_cursor + n - 1) % n;
			if (in->repeat & KEY_RIGHT)
				s_cursor = (s_cursor + 1) % n;
			int t = hand_touched(in, n, HAND_SC);
			if (t >= 0 || (in->down & KEY_B)) {
				int k = t >= 0 ? t : s_cursor;
				s_cursor = k;
				s_sel[k] = !s_sel[k];
				audio_play(SFX_SELECT);
			} else if (button_update(&s_btn_main, in)) {
				discard(s_sel);
				memset(s_sel, 0, sizeof s_sel);
			}
		}
		break;
	}
	case JU_PLAYING:
		button_set(&s_btn_main, 12, y, 296, h, GLYPH_A " Echar", C2D_Color32(40, 140, 60, 255), KEY_A);
		s_btn_main.enabled = mine;
		if (mine) {
			if (in->repeat & KEY_LEFT)
				s_cursor = (s_cursor + n - 1) % n;
			if (in->repeat & KEY_RIGHT)
				s_cursor = (s_cursor + 1) % n;
			if (s_cursor >= n)
				s_cursor = n - 1;
			if (!ju_can_play(&s_game, 0, s_cursor))
				for (int i = 0; i < n; i++)
					if (ju_can_play(&s_game, 0, i)) {
						s_cursor = i;
						break;
					}
			int t = hand_touched(in, n, HAND_SC);
			int pick = t >= 0 ? t : button_update(&s_btn_main, in) ? s_cursor : -1;
			if (pick >= 0) {
				if (ju_can_play(&s_game, 0, pick))
					play(pick);
				else {
					audio_play(SFX_ERROR);
					table_message(&s_table, "Hay que asistir y montar; si no tienes del palo, fallar");
				}
			}
		}
		break;
	case JU_TRICK_DONE:
		s_btn_main.enabled = false;
		s_trick_timer += dt;
		if (s_trick_timer > 1.0f) {
			int w = s_game.trick_winner;
			ju_collect(&s_game);
			if (w == 0)
				table_message(&s_table, "Te llevas la baza");
			else
				table_message(&s_table, "%s se lleva la baza", table_name(&s_table, w));
		}
		break;
	case JU_HAND_OVER:
	case JU_GAME_OVER: {
		bool over = s_game.phase == JU_GAME_OVER;
		bool broke = over && wallet_chips() == 0;
		button_set(&s_btn_main, 12, y, 144, h,
			broke ? GLYPH_A " Comprar fichas" : over ? GLYPH_A " Otra partida" : GLYPH_A " Siguiente mano",
			C2D_Color32(40, 140, 60, 255), KEY_A);
		button_set(&s_btn_alt, 164, y, 144, h, GLYPH_B " Menú", C2D_Color32(190, 60, 50, 255), KEY_B);
		s_btn_main.enabled = s_btn_alt.enabled = s_end_time > 0.6f;
		s_end_time += dt;
		if (button_update(&s_btn_main, in)) {
			if (broke)
				return SCENE_FICHAS;
			if (over) {
				ju_new_game(&s_game);
				buy_in();
			} else
				ju_new_hand(&s_game);
			start_hand();
		} else if (button_update(&s_btn_alt, in)) {
			return SCENE_MENU;
		}
		break;
	}
	}
	/* Turnos de la CPU */
	if (!mine && s_game.phase <= JU_PLAYING) {
		s_cpu_timer += dt;
		if (s_cpu_timer > 0.8f) {
			s_cpu_timer = 0.0f;
			int p = s_game.current;
			if (s_game.phase == JU_DECIDING) {
				decide(ju_must_play(&s_game, p) || ju_ai_enter(&s_game, p));
			} else if (s_game.phase == JU_DISCARDING) {
				bool sel[JU_HAND];
				ju_ai_discard(&s_game, p, sel);
				discard(sel);
			} else {
				play(ju_ai_choose(&s_game, p));
			}
		}
	}
	if (s_game.phase != s_prev_phase) {
		if (s_game.phase == JU_HAND_OVER || s_game.phase == JU_GAME_OVER) {
			s_end_time = 0.0f;
			for (int p = 0; p < JU_PLAYERS; p++) {
				if (s_game.julepe[p]) {
					table_bubble(&s_table, p, "¡Julepe!");
					table_mood(&s_table, p, EXPR_SAD, 3.0f);
				} else if (s_game.change[p] > 0) {
					table_mood(&s_table, p, EXPR_HAPPY, 3.0f);
				}
			}
			if (s_game.void_hand)
				table_message(&s_table, "No juegan dos: el plato se queda para la siguiente");
			else if (s_game.julepe[0])
				table_message(&s_table, "¡Te han dado julepe! Pagas %d", -s_game.change[0]);
			if (s_game.phase == JU_GAME_OVER) {
				bool won = ju_winner(&s_game) == 0;
				profile_record_game(GAME_JULEPE, won);
				cash_out(); /* acabada: tus fichas, al monedero */
				audio_play(won ? SFX_WIN : SFX_LOSE);
			}
		}
		s_prev_phase = s_game.phase;
	}
	table_update(&s_table, dt);
	return SCENE_JULEPE;
}

static void ju_draw_top(void)
{
	gfx_background(BG_TABLE_TOP);
	for (int p = 1; p < JU_PLAYERS; p++) {
		char info[32];
		if (s_game.phase >= JU_DISCARDING && s_game.phase <= JU_TRICK_DONE && !s_game.playing[p])
			snprintf(info, sizeof info, "%d · pasa", s_game.chips[p]);
		else if (s_game.phase >= JU_PLAYING && s_game.phase <= JU_TRICK_DONE)
			snprintf(info, sizeof info, "%d · %d bazas", s_game.chips[p], s_game.tricks[p]);
		else
			snprintf(info, sizeof info, "%d fichas", s_game.chips[p]);
		bool turn = s_game.current == p && s_game.phase <= JU_PLAYING;
		int cards = s_game.phase >= JU_DISCARDING && !s_game.playing[p] ? 0 : s_game.hand_count[p];
		table_draw_seat(&s_table, p, cards, turn, info);
	}
	/* Triunfo y plato */
	gfx_card(s_game.trump_card, 330, 166, 0.5f, 1.0f, 1.57f, DEPTH_CARDS, 0.0f, true);
	gfx_text("Pinta", 330, 186, DEPTH_UI, 0.4f, C2D_Color32(200, 230, 200, 255), C2D_AlignCenter);
	gfx_textf(330, 124, DEPTH_UI, 0.5f, COLOR_GOLD, C2D_AlignCenter, "Plato: %d", s_game.pot);
	if (s_game.phase == JU_PLAYING || s_game.phase == JU_TRICK_DONE)
		for (int p = 0; p < JU_PLAYERS; p++)
			if (s_game.trick_played[p])
				gfx_card(s_game.trick[p], TRICK_X[p], TRICK_Y[p], 0.55f, 1.0f, (p - 1.5f) * 0.06f,
					DEPTH_CARDS + (p == s_game.trick_winner && s_game.phase == JU_TRICK_DONE ? 0.3f : 0.0f), 0.0f, true);
	char me[40];
	if (s_game.phase >= JU_PLAYING && s_game.phase <= JU_TRICK_DONE && s_game.playing[0])
		snprintf(me, sizeof me, "Fichas: %d · bazas: %d", s_game.chips[0], s_game.tricks[0]);
	else
		snprintf(me, sizeof me, "Tus fichas: %d", s_game.chips[0]);
	table_draw_me(&s_table, me);
	table_draw_bubbles(&s_table);

	if ((s_game.phase == JU_HAND_OVER || s_game.phase == JU_GAME_OVER) && banner_draw(s_end_time, 200, 112, 340, 100)) {
		if (s_game.phase == JU_GAME_OVER) {
			int w = ju_winner(&s_game);
			gfx_text(w == 0 ? "¡Ganas la partida!" : "Fin de la partida", 200, 68, DEPTH_BANNER, 0.65f,
				w == 0 ? COLOR_GOLD : COLOR_RED, C2D_AlignCenter);
		} else {
			gfx_text(s_game.void_hand ? "Mano sin jugar" : "Fin de la mano", 200, 68, DEPTH_BANNER, 0.65f, COLOR_GOLD,
				C2D_AlignCenter);
		}
		for (int p = 0; p < JU_PLAYERS; p++) {
			const char* tag = !s_game.playing[p] ? "pasa" : s_game.julepe[p] ? "julepe" : "";
			gfx_textf(200, 94 + p * 15, DEPTH_BANNER, 0.42f, p == 0 ? COLOR_GOLD : COLOR_CREAM, C2D_AlignCenter,
				"%s: %d bazas  %+d  → %d fichas %s", who(p), s_game.tricks[p], s_game.change[p], s_game.chips[p], tag);
		}
	}
}

static void ju_draw_bottom(void)
{
	gfx_background(BG_TABLE_BOTTOM);
	table_draw_message(&s_table);
	button_draw(&s_btn_menu, false);
	int n = s_game.hand_count[0];
	bool mine = s_game.current == 0;
	bool playable[JU_HAND];
	for (int i = 0; i < n; i++)
		playable[i] = !(mine && s_game.phase == JU_PLAYING) || ju_can_play(&s_game, 0, i);
	const char* hint = NULL;
	if (mine && s_game.phase == JU_DECIDING)
		hint = ju_must_play(&s_game, 0) ? "Repartes y solo ha entrado uno: tienes que jugar"
		                                : "¿Juegas? Tendrás que hacer al menos dos bazas";
	else if (mine && s_game.phase == JU_DISCARDING)
		hint = "Toca (o " GLYPH_B ") las cartas que quieres cambiar";
	else if (mine && s_game.phase == JU_PLAYING)
		hint = "¡Tu turno!";
	if (hint)
		gfx_text(hint, BOTTOM_W / 2, 30, 0, 0.45f, COLOR_GOLD, C2D_AlignCenter);
	bool show = !(s_game.phase >= JU_DISCARDING && s_game.phase <= JU_TRICK_DONE && !s_game.playing[0]);
	if (show)
		hand_draw(s_game.hands[0], n, HAND_SC, s_game.phase == JU_DISCARDING ? s_sel : NULL,
			mine && s_game.phase <= JU_PLAYING ? s_cursor : -1, playable);
	else
		gfx_text("Pasas esta mano", BOTTOM_W / 2, 100, 0, 0.6f, COLOR_CREAM, C2D_AlignCenter);
	gfx_text("As, 3, rey, caballo, sota, 7... 2 · cada baza, 1/5 del plato", BOTTOM_W / 2, 176, 0, 0.38f,
		C2D_Color32(200, 230, 200, 255), C2D_AlignCenter);
	if (s_game.phase != JU_TRICK_DONE)
		button_draw(&s_btn_main, s_btn_main.enabled);
	if (s_game.phase == JU_DECIDING || s_game.phase >= JU_HAND_OVER)
		button_draw(&s_btn_alt, s_btn_alt.enabled);
}

/* Salir a medias desde la pausa: la próxima vez se empieza de cero */
static void scene_leave(void)
{
	cash_out();
	s_started = false;
}

const Scene scene_julepe = { ju_enter, ju_update, ju_draw_top, ju_draw_bottom, scene_leave };
