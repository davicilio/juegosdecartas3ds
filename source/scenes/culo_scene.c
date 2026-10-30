/* Culo: quítate las cartas de encima antes que nadie. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "games/culo.h"
#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/gfx.h"
#include "ui/table.h"
#include "ui/widgets.h"

#define HAND_SC 0.66f

static Culo s_game;
static Table s_table;
static bool s_started;
static bool s_sel[DECK_SIZE];
static int s_cursor;
static float s_cpu_timer, s_end_time;
static CuloPhase s_prev_phase;
static Button s_btn_play, s_btn_pass, s_btn_menu;

static void clear_sel(void)
{
	memset(s_sel, 0, sizeof s_sel);
}

static void start_round(void)
{
	clear_sel();
	s_cursor = 0;
	s_cpu_timer = s_end_time = 0.0f;
	s_prev_phase = s_game.phase;
	if (s_game.rounds > 0)
		table_message(&s_table, "Cambio de cartas hecho. Sale el culo: %s",
			s_game.current == 0 ? "tú" : table_name(&s_table, s_game.current));
	else
		table_message(&s_table, "Sale quien tiene el 3 de oros: %s",
			s_game.current == 0 ? "tú" : table_name(&s_table, s_game.current));
}

static void culo_enter(void)
{
	if (!s_started) {
		table_init(&s_table, CPU_LUCIA, CPU_RAMON, CPU_PEPE);
		culo_init(&s_game, (uint32_t)time(NULL) ^ 0xC010);
		s_started = true;
		start_round();
	}
	button_set(&s_btn_menu, 258, 6, 54, 20, "Menú", C2D_Color32(120, 30, 35, 255), KEY_START);
}

static void announce(int p, int n)
{
	char buf[40];
	if (n == 0) {
		table_bubble(&s_table, p, "Paso");
		return;
	}
	const char* name = rank_name(s_game.pile[0].rank);
	if (n == 1)
		snprintf(buf, sizeof buf, "%s", name);
	else
		snprintf(buf, sizeof buf, "%d de %s", n, name);
	table_bubble(&s_table, p, buf);
	audio_play(SFX_CARD);
	if (culo_done(&s_game, p)) {
		table_message(&s_table, "%s se queda sin cartas", p == 0 ? "Tú" : table_name(&s_table, p));
		table_mood(&s_table, p, EXPR_HAPPY, 2.0f);
	}
}

static int selected(int* idx)
{
	int n = 0;
	for (int i = 0; i < s_game.hand_count[0] && n < CULO_MAX_PLAY; i++)
		if (s_sel[i])
			idx[n++] = i;
	return n;
}

static SceneId culo_update(const Input* in, float dt)
{
	if (button_update(&s_btn_menu, in))
		return SCENE_MENU;
	const float y = 205, h = 28;
	if (s_game.phase == CULO_PHASE_PLAYING) {
		bool mine = s_game.current == 0;
		button_set(&s_btn_play, 12, y, 144, h, GLYPH_A " Echar", C2D_Color32(40, 140, 60, 255), KEY_A);
		button_set(&s_btn_pass, 164, y, 144, h, GLYPH_X " Paso", C2D_Color32(190, 60, 50, 255), KEY_X);
		s_btn_play.enabled = mine;
		s_btn_pass.enabled = mine && culo_can_pass(&s_game);
		if (mine) {
			int n = s_game.hand_count[0];
			if (n > 0) {
				if (in->repeat & KEY_LEFT)
					s_cursor = (s_cursor + n - 1) % n;
				if (in->repeat & KEY_RIGHT)
					s_cursor = (s_cursor + 1) % n;
				if (s_cursor >= n)
					s_cursor = n - 1;
			}
			int t = hand_touched(in, n, HAND_SC);
			if (t >= 0 || (in->down & KEY_B)) {
				int k = t >= 0 ? t : s_cursor;
				s_cursor = k;
				/* Solo cartas iguales: al elegir otra distinta se empieza de nuevo */
				int idx[CULO_MAX_PLAY];
				int count = selected(idx);
				if (!s_sel[k] && count > 0 && s_game.hands[0][idx[0]].rank != s_game.hands[0][k].rank)
					clear_sel();
				s_sel[k] = !s_sel[k];
				audio_play(SFX_SELECT);
			} else if (button_update(&s_btn_play, in)) {
				int idx[CULO_MAX_PLAY];
				int count = selected(idx);
				if (count == 0) {
					/* Sin elegir: tantas cartas iguales a la señalada como pida la mesa */
					int need = s_game.pile_n ? s_game.pile_n : 1;
					for (int i = 0; i < n && count < need; i++)
						if (s_game.hands[0][i].rank == s_game.hands[0][s_cursor].rank)
							idx[count++] = i;
				}
				if (culo_play(&s_game, idx, count)) {
					announce(0, count);
					clear_sel();
				} else {
					audio_play(SFX_ERROR);
					if (s_game.pile_n)
						table_message(&s_table, "Tienen que ser %d carta%s iguales y más altas", s_game.pile_n,
							s_game.pile_n == 1 ? "" : "s");
					else
						table_message(&s_table, "Elige cartas iguales");
				}
			} else if (button_update(&s_btn_pass, in)) {
				culo_pass(&s_game);
				announce(0, 0);
				clear_sel();
			}
		} else {
			s_cpu_timer += dt;
			if (s_cpu_timer > 0.8f) {
				s_cpu_timer = 0.0f;
				int p = s_game.current;
				int idx[CULO_MAX_PLAY], n;
				if (culo_ai_choose(&s_game, idx, &n) && culo_play(&s_game, idx, n)) {
					announce(p, n);
				} else {
					culo_pass(&s_game);
					announce(p, 0);
				}
			}
		}
	} else {
		button_set(&s_btn_play, 12, y, 144, h, GLYPH_A " Otra ronda", C2D_Color32(40, 140, 60, 255), KEY_A);
		button_set(&s_btn_pass, 164, y, 144, h, GLYPH_B " Menú", C2D_Color32(190, 60, 50, 255), KEY_B);
		s_btn_play.enabled = s_btn_pass.enabled = s_end_time > 0.6f;
		s_end_time += dt;
		if (button_update(&s_btn_play, in)) {
			culo_new_round(&s_game);
			start_round();
		} else if (button_update(&s_btn_pass, in)) {
			return SCENE_MENU;
		}
	}
	if (s_game.phase != s_prev_phase) {
		s_prev_phase = s_game.phase;
		if (s_game.phase == CULO_PHASE_ROUND_OVER) {
			s_end_time = 0.0f;
			profile_record_game(GAME_CULO, s_game.finish[0] == 0);
			audio_play(s_game.role[0] <= 1 ? SFX_WIN : SFX_LOSE);
			for (int p = 0; p < CULO_PLAYERS; p++)
				table_mood(&s_table, p, s_game.role[p] <= 1 ? EXPR_HAPPY : EXPR_SAD, 4.0f);
			table_message(&s_table, "Eres %s", culo_role_name(s_game.role[0]));
		}
	}
	table_update(&s_table, dt);
	return SCENE_CULO;
}

static void culo_draw_top(void)
{
	gfx_background(BG_TABLE_TOP);
	for (int p = 1; p < CULO_PLAYERS; p++) {
		const char* info = s_game.role[p] >= 0 ? culo_role_name(s_game.role[p]) : NULL;
		char buf[24];
		if (!info) {
			snprintf(buf, sizeof buf, "%d cartas", s_game.hand_count[p]);
			info = buf;
		}
		table_draw_seat(&s_table, p, s_game.hand_count[p],
			s_game.current == p && s_game.phase == CULO_PHASE_PLAYING, info);
	}
	/* La jugada que hay que superar */
	for (int i = 0; i < s_game.pile_n; i++)
		gfx_card(s_game.pile[i], 200 + (i - (s_game.pile_n - 1) / 2.0f) * 30.0f, 116, 0.7f, 1.0f, (i - 1) * 0.06f,
			DEPTH_CARDS + i * 0.1f, 0.0f, true);
	if (s_game.pile_n == 0 && s_game.phase == CULO_PHASE_PLAYING)
		gfx_text("Mesa libre", 200, 108, DEPTH_UI, 0.55f, C2D_Color32(200, 230, 200, 255), C2D_AlignCenter);
	gfx_text("3 4 5 6 7 S C R As 2", 200, 160, DEPTH_UI, 0.42f, COLOR_CREAM, C2D_AlignCenter);
	char me[40];
	if (s_game.role[0] >= 0)
		snprintf(me, sizeof me, "%s", culo_role_name(s_game.role[0]));
	else
		snprintf(me, sizeof me, "Tus cartas: %d", s_game.hand_count[0]);
	table_draw_me(&s_table, me);
	table_draw_bubbles(&s_table);

	if (s_game.phase == CULO_PHASE_ROUND_OVER && banner_draw(s_end_time, 200, 112, 320, 104)) {
		for (int k = 0; k < CULO_PLAYERS; k++) {
			int p = s_game.finish[k];
			gfx_textf(200, 70 + k * 20, DEPTH_BANNER, 0.5f, p == 0 ? COLOR_GOLD : COLOR_CREAM, C2D_AlignCenter,
				"%s: %s", culo_role_name(k), p == 0 ? "tú" : table_name(&s_table, p));
		}
	}
}

static void culo_draw_bottom(void)
{
	gfx_background(BG_TABLE_BOTTOM);
	table_draw_message(&s_table);
	button_draw(&s_btn_menu, false);
	bool mine = s_game.current == 0 && s_game.phase == CULO_PHASE_PLAYING;
	if (mine)
		gfx_text(s_game.pile_n ? "¡Tu turno! Supera la jugada o pasa" : "¡Tu turno! Sales tú", BOTTOM_W / 2, 30, 0,
			0.5f, COLOR_GOLD, C2D_AlignCenter);
	hand_draw(s_game.hands[0], s_game.hand_count[0], HAND_SC, s_sel, mine ? s_cursor : -1, NULL);
	if (s_game.hand_count[0] == 0 && s_game.phase == CULO_PHASE_PLAYING)
		gfx_text("Ya has acabado: espera a los demás", BOTTOM_W / 2, 100, 0, 0.5f, COLOR_CREAM, C2D_AlignCenter);
	gfx_text("Toca (o " GLYPH_B ") para elegir cartas iguales", BOTTOM_W / 2, 172, 0, 0.42f,
		C2D_Color32(200, 230, 200, 255), C2D_AlignCenter);
	button_draw(&s_btn_play, s_btn_play.enabled);
	button_draw(&s_btn_pass, s_btn_pass.enabled);
}

/* Salir a medias desde la pausa: la próxima vez se empieza de cero */
static void scene_leave(void)
{
	s_started = false;
}

const Scene scene_culo = { culo_enter, culo_update, culo_draw_top, culo_draw_bottom, scene_leave };
