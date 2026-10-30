/* Cucú: una carta cada uno; quedársela o cambiarla con el siguiente. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "games/cucu.h"
#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/gfx.h"
#include "ui/table.h"
#include "ui/widgets.h"

static const float CARD_X[CUCU_PLAYERS] = { 200, 120, 200, 280 };
static const float CARD_Y[CUCU_PLAYERS] = { 170, 118, 76, 118 };

static Cucu s_game;
static Table s_table;
static bool s_started;
static float s_cpu_timer, s_end_time;
static CucuPhase s_prev_phase;
static Button s_btn_swap, s_btn_keep, s_btn_menu;

static const char* hearts(int lives)
{
	static const char* const h[] = { "Eliminado", "Vidas: 1", "Vidas: 2", "Vidas: 3" };
	return h[lives < 0 ? 0 : lives > 3 ? 3 : lives];
}

static void start_round(void)
{
	s_cpu_timer = s_end_time = 0.0f;
	s_prev_phase = s_game.phase;
	table_message(&s_table, "Reparte %s", s_game.dealer == 0 ? "tú" : table_name(&s_table, s_game.dealer));
}

static void cucu_enter(void)
{
	if (!s_started) {
		table_init(&s_table, CPU_PACO, CPU_MANOLO, CPU_LUCIA);
		cucu_init(&s_game, (uint32_t)time(NULL) ^ 0xC0C0);
		s_started = true;
		start_round();
	}
	button_set(&s_btn_menu, 258, 6, 54, 20, "Menú", C2D_Color32(120, 30, 35, 255), KEY_START);
}

static void decide(bool swap)
{
	int p = s_game.current;
	int q = cucu_partner(&s_game);
	cucu_decide(&s_game, swap);
	audio_play(swap ? SFX_FLIP : SFX_SELECT);
	const char* who = p == 0 ? "Tú" : table_name(&s_table, p);
	if (!swap) {
		table_bubble(&s_table, p, "Me quedo");
		table_message(&s_table, "%s se queda su carta", who);
	} else if (s_game.last_blocked) {
		table_bubble(&s_table, q, "¡Cucú!");
		table_message(&s_table, "%s tiene un rey: ¡cucú! No se cambia", q == 0 ? "Tú" : table_name(&s_table, q));
	} else if (q < 0) {
		table_bubble(&s_table, p, "Del mazo");
		table_message(&s_table, "%s cambia su carta por una del mazo", who);
	} else {
		table_bubble(&s_table, p, "Cambio");
		table_message(&s_table, "%s cambia con %s", who, q == 0 ? "contigo" : table_name(&s_table, q));
		if (q == 0)
			table_mood(&s_table, 0, EXPR_SURPRISED, 1.2f);
	}
}

static SceneId cucu_update(const Input* in, float dt)
{
	if (button_update(&s_btn_menu, in))
		return SCENE_MENU;
	const float y = 205, h = 28;
	if (s_game.phase == CUCU_PHASE_DECIDE) {
		bool dealer = s_game.current == s_game.dealer;
		button_set(&s_btn_swap, 12, y, 144, h, dealer ? GLYPH_A " Cambiar (mazo)" : GLYPH_A " Cambiar",
			C2D_Color32(200, 140, 30, 255), KEY_A);
		button_set(&s_btn_keep, 164, y, 144, h, GLYPH_B " Me quedo", C2D_Color32(40, 140, 60, 255), KEY_B);
		bool mine = s_game.current == 0;
		s_btn_swap.enabled = s_btn_keep.enabled = mine;
		if (mine) {
			if (button_update(&s_btn_swap, in))
				decide(true);
			else if (button_update(&s_btn_keep, in))
				decide(false);
		} else {
			s_cpu_timer += dt;
			if (s_cpu_timer > 0.9f) {
				s_cpu_timer = 0.0f;
				decide(cucu_ai_swap(&s_game));
			}
		}
	} else {
		bool over = s_game.phase == CUCU_PHASE_GAME_OVER;
		button_set(&s_btn_swap, 12, y, 144, h, over ? GLYPH_A " Otra partida" : GLYPH_A " Siguiente ronda",
			C2D_Color32(40, 140, 60, 255), KEY_A);
		button_set(&s_btn_keep, 164, y, 144, h, GLYPH_B " Menú", C2D_Color32(190, 60, 50, 255), KEY_B);
		s_btn_swap.enabled = s_btn_keep.enabled = s_end_time > 0.6f;
		s_end_time += dt;
		if (button_update(&s_btn_swap, in)) {
			if (over)
				cucu_new_game(&s_game);
			else
				cucu_new_round(&s_game);
			start_round();
		} else if (button_update(&s_btn_keep, in)) {
			return SCENE_MENU;
		}
	}
	if (s_game.phase != s_prev_phase) {
		s_prev_phase = s_game.phase;
		if (s_game.phase != CUCU_PHASE_DECIDE) {
			s_end_time = 0.0f;
			char losers[96] = "";
			for (int p = 0; p < CUCU_PLAYERS; p++)
				if (s_game.lost[p]) {
					table_mood(&s_table, p, EXPR_SAD, 2.5f);
					snprintf(losers + strlen(losers), sizeof losers - strlen(losers), "%s%s", losers[0] ? ", " : "",
						p == 0 ? "tú" : table_name(&s_table, p));
				}
			if (losers[0])
				table_message(&s_table, "Pierde%s una vida: %s", strchr(losers, ',') ? "n" : "", losers);
			else
				table_message(&s_table, "Empate: nadie pierde vida");
			audio_play(s_game.lost[0] ? SFX_LOSE : SFX_WIN);
			if (s_game.phase == CUCU_PHASE_GAME_OVER)
				profile_record_game(GAME_CUCU, cucu_winner(&s_game) == 0);
		}
	}
	table_update(&s_table, dt);
	return SCENE_CUCU;
}

static void cucu_draw_top(void)
{
	gfx_background(BG_TABLE_TOP);
	bool reveal = s_game.phase != CUCU_PHASE_DECIDE;
	for (int p = 1; p < CUCU_PLAYERS; p++)
		table_draw_seat(&s_table, p, 0, s_game.current == p && !reveal, hearts(s_game.lives[p]));
	for (int p = 0; p < CUCU_PLAYERS; p++) {
		if (!cucu_alive(&s_game, p) && !s_game.lost[p])
			continue;
		bool up = reveal;
		gfx_card(s_game.card[p], CARD_X[p], CARD_Y[p], 0.62f, up ? 1.0f : 0.0f, 0.0f, DEPTH_CARDS,
			reveal && s_game.lost[p] ? 0.45f : 0.0f, true);
	}
	/* El mazo del que reparte */
	gfx_card_back(330, 186, 0.5f, 0.5f, 0.1f, DEPTH_CARDS, NULL);
	gfx_text("Mazo", 330, 210, DEPTH_UI, 0.4f, COLOR_CREAM, C2D_AlignCenter);
	table_draw_me(&s_table, hearts(s_game.lives[0]));
	table_draw_bubbles(&s_table);

	if (s_game.phase == CUCU_PHASE_GAME_OVER && banner_draw(s_end_time, 200, 120, 300, 64)) {
		int w = cucu_winner(&s_game);
		if (w == 0)
			gfx_text("¡Ganas la partida!", 200, 104, DEPTH_BANNER, 0.75f, COLOR_GOLD, C2D_AlignCenter);
		else
			gfx_textf(200, 104, DEPTH_BANNER, 0.65f, COLOR_RED, C2D_AlignCenter, "Gana %s", table_name(&s_table, w));
	}
}

static void cucu_draw_bottom(void)
{
	gfx_background(BG_TABLE_BOTTOM);
	table_draw_message(&s_table);
	button_draw(&s_btn_menu, false);
	if (cucu_alive(&s_game, 0) || s_game.lost[0]) {
		gfx_card(s_game.card[0], BOTTOM_W / 2, 104, 1.1f, 1.0f, 0.0f, 0, 0.0f, true);
		if (s_game.phase == CUCU_PHASE_DECIDE && s_game.current == 0) {
			int q = cucu_partner(&s_game);
			if (s_game.card[0].rank == RANK_REY)
				gfx_text("¡Tienes un rey! Quédatelo", BOTTOM_W / 2, 36, 0, 0.5f, COLOR_GOLD, C2D_AlignCenter);
			else
				gfx_textf(BOTTOM_W / 2, 36, 0, 0.5f, COLOR_GOLD, C2D_AlignCenter, "¿Te la quedas o cambias con %s?",
					q < 0 ? "el mazo" : table_name(&s_table, q));
		}
	} else {
		gfx_text("Estás eliminado", BOTTOM_W / 2, 100, 0, 0.6f, COLOR_CREAM, C2D_AlignCenter);
	}
	gfx_text("As la más baja · rey la más alta", BOTTOM_W / 2, 176, 0, 0.42f, C2D_Color32(200, 230, 200, 255),
		C2D_AlignCenter);
	button_draw(&s_btn_swap, s_btn_swap.enabled);
	button_draw(&s_btn_keep, s_btn_keep.enabled);
}

/* Salir a medias desde la pausa: la próxima vez se empieza de cero */
static void scene_leave(void)
{
	s_started = false;
}

const Scene scene_cucu = { cucu_enter, cucu_update, cucu_draw_top, cucu_draw_bottom, scene_leave };
