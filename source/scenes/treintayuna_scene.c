/* Treinta y una: cambia cartas con la mesa hasta sumar lo máximo de un palo. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "games/treintayuna.h"
#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/gfx.h"
#include "ui/table.h"
#include "ui/widgets.h"

static const float REVEAL_X[TYU_PLAYERS] = { 200, 70, 200, 330 };
static const float REVEAL_Y[TYU_PLAYERS] = { 196, 150, 70, 150 };

#define MINE_Y   82.0f
#define TABLE_Y  156.0f
#define MINE_SC  0.85f
#define TABLE_SC 0.62f

static TreintaYUna s_game;
static Table s_table;
static bool s_started;
static int s_cursor;   /* 0..2 tus cartas, 3..5 las de la mesa */
static int s_selected; /* tu carta elegida para cambiar, o -1 */
static float s_cpu_timer, s_end_time;
static TyuPhase s_prev_phase;
static Button s_btn_all, s_btn_knock, s_btn_menu;

static const char* lives_text(int lives)
{
	static const char* const t[] = { "Eliminado", "Vidas: 1", "Vidas: 2", "Vidas: 3" };
	return t[lives < 0 ? 0 : lives > 3 ? 3 : lives];
}

static float mine_x(int i)
{
	return BOTTOM_W / 2.0f + (i - 1) * 64.0f;
}

static float table_x(int i)
{
	return BOTTOM_W / 2.0f + (i - 1) * 50.0f;
}

static void start_round(void)
{
	s_cursor = 0;
	s_selected = -1;
	s_cpu_timer = s_end_time = 0.0f;
	s_prev_phase = s_game.phase;
	table_message(&s_table, "Empieza %s", s_game.current == 0 ? "tú" : table_name(&s_table, s_game.current));
}

static void tyu_enter(void)
{
	if (!s_started) {
		table_init(&s_table, CPU_RAMON, CPU_CARMEN, CPU_PEPE);
		tyu_init(&s_game, (uint32_t)time(NULL) ^ 0x31);
		s_started = true;
		start_round();
	}
	button_set(&s_btn_menu, 258, 6, 54, 20, "Menú", C2D_Color32(120, 30, 35, 255), KEY_START);
}

static void after_move(int p, const char* what)
{
	table_message(&s_table, "%s %s", p == 0 ? "Tú:" : table_name(&s_table, p), what);
	s_selected = -1;
}

static void swap_with(int t)
{
	if (s_selected < 0)
		return;
	tyu_swap(&s_game, s_selected, t);
	audio_play(SFX_CARD);
	after_move(0, "cambias una carta");
}

static SceneId tyu_update(const Input* in, float dt)
{
	if (button_update(&s_btn_menu, in))
		return SCENE_MENU;
	const float y = 205, h = 28;
	if (s_game.phase == TYU_PHASE_PLAYING) {
		bool mine = s_game.current == 0;
		button_set(&s_btn_all, 12, y, 144, h, GLYPH_X " Cambiar las tres", C2D_Color32(200, 140, 30, 255), KEY_X);
		button_set(&s_btn_knock, 164, y, 144, h, GLYPH_Y " Me planto", C2D_Color32(190, 60, 50, 255), KEY_Y);
		s_btn_all.enabled = mine;
		s_btn_knock.enabled = mine && tyu_can_knock(&s_game);
		if (mine) {
			if (in->repeat & KEY_LEFT)
				s_cursor = (s_cursor + 5) % 6;
			if (in->repeat & KEY_RIGHT)
				s_cursor = (s_cursor + 1) % 6;
			if (in->repeat & (KEY_UP | KEY_DOWN))
				s_cursor = (s_cursor + 3) % 6;
			int touched = -1;
			if (in->touch_down) {
				for (int i = 0; i < 3; i++) {
					if (fabsf(in->tx - mine_x(i)) <= CARD_W * MINE_SC / 2 && fabsf(in->ty - MINE_Y) <= CARD_H * MINE_SC / 2)
						touched = i;
					if (fabsf(in->tx - table_x(i)) <= CARD_W * TABLE_SC / 2 &&
						fabsf(in->ty - TABLE_Y) <= CARD_H * TABLE_SC / 2)
						touched = 3 + i;
				}
			}
			int pick = touched >= 0 ? touched : (in->down & KEY_A) ? s_cursor : -1;
			if (touched >= 0)
				s_cursor = touched;
			if (pick >= 0) {
				if (pick < 3) {
					s_selected = s_selected == pick ? -1 : pick;
					audio_play(SFX_SELECT);
				} else if (s_selected >= 0) {
					swap_with(pick - 3);
				} else {
					audio_play(SFX_ERROR);
					table_message(&s_table, "Primero elige qué carta tuya cambias");
				}
			} else if (button_update(&s_btn_all, in)) {
				tyu_swap_all(&s_game);
				audio_play(SFX_CARD);
				after_move(0, "cambias las tres");
			} else if (button_update(&s_btn_knock, in)) {
				tyu_knock(&s_game);
				table_bubble(&s_table, 0, "¡Me planto!");
				after_move(0, "te plantas: los demás juegan una vez más");
			}
		} else {
			s_cpu_timer += dt;
			if (s_cpu_timer > 1.0f) {
				s_cpu_timer = 0.0f;
				int p = s_game.current;
				int knocker = s_game.knocker;
				tyu_ai_turn(&s_game);
				audio_play(SFX_CARD);
				if (s_game.knocker != knocker) {
					table_bubble(&s_table, p, "¡Me planto!");
					after_move(p, "se planta");
				} else {
					after_move(p, "cambia");
				}
			}
		}
	} else {
		bool over = s_game.phase == TYU_PHASE_GAME_OVER;
		button_set(&s_btn_all, 12, y, 144, h, over ? GLYPH_A " Otra partida" : GLYPH_A " Siguiente ronda",
			C2D_Color32(40, 140, 60, 255), KEY_A);
		button_set(&s_btn_knock, 164, y, 144, h, GLYPH_B " Menú", C2D_Color32(190, 60, 50, 255), KEY_B);
		s_btn_all.enabled = s_btn_knock.enabled = s_end_time > 0.6f;
		s_end_time += dt;
		if (button_update(&s_btn_all, in)) {
			if (over)
				tyu_new_game(&s_game);
			else
				tyu_new_round(&s_game);
			start_round();
		} else if (button_update(&s_btn_knock, in)) {
			return SCENE_MENU;
		}
	}
	if (s_game.phase != s_prev_phase) {
		s_prev_phase = s_game.phase;
		if (s_game.phase != TYU_PHASE_PLAYING) {
			s_end_time = 0.0f;
			if (s_game.thirty_one >= 0) {
				table_bubble(&s_table, s_game.thirty_one, "¡Treinta y una!");
				table_message(&s_table, "¡%s tiene 31!", s_game.thirty_one == 0 ? "Tú" : table_name(&s_table, s_game.thirty_one));
			} else {
				table_message(&s_table, s_game.lost[0] ? "Pierdes una vida" : "Se enseñan las manos");
			}
			for (int p = 0; p < TYU_PLAYERS; p++)
				if (s_game.lost[p])
					table_mood(&s_table, p, EXPR_SAD, 2.5f);
			audio_play(s_game.lost[0] ? SFX_LOSE : SFX_WIN);
			if (s_game.phase == TYU_PHASE_GAME_OVER)
				profile_record_game(GAME_TREINTAYUNA, tyu_winner(&s_game) == 0);
		}
	}
	table_update(&s_table, dt);
	return SCENE_TREINTAYUNA;
}

static void tyu_draw_top(void)
{
	gfx_background(BG_TABLE_TOP);
	bool reveal = s_game.phase != TYU_PHASE_PLAYING;
	for (int p = 1; p < TYU_PLAYERS; p++) {
		char info[32];
		if (reveal && tyu_alive(&s_game, p) + s_game.lost[p])
			snprintf(info, sizeof info, "%d · %s", tyu_hand_value(s_game.hands[p]), lives_text(s_game.lives[p]));
		else
			snprintf(info, sizeof info, "%s%s", lives_text(s_game.lives[p]), s_game.knocker == p ? " ·plantado" : "");
		table_draw_seat(&s_table, p, reveal || !tyu_alive(&s_game, p) ? 0 : 3, s_game.current == p && !reveal, info);
	}
	if (reveal) {
		for (int p = 1; p < TYU_PLAYERS; p++)
			if (tyu_alive(&s_game, p) || s_game.lost[p])
				for (int i = 0; i < TYU_CARDS; i++)
					gfx_card(s_game.hands[p][i], REVEAL_X[p] + (i - 1) * 22.0f, REVEAL_Y[p], 0.45f, 1.0f, 0.0f,
						DEPTH_CARDS + i * 0.05f, s_game.lost[p] ? 0.4f : 0.0f, i == 0);
	} else {
		/* La mesa */
		for (int i = 0; i < TYU_CARDS; i++)
			gfx_card(s_game.table[i], 160 + i * 40.0f, 122, 0.62f, 1.0f, 0.0f, DEPTH_CARDS, 0.0f, true);
		gfx_text("Mesa", 200, 156, DEPTH_UI, 0.42f, COLOR_CREAM, C2D_AlignCenter);
	}
	char me[40];
	snprintf(me, sizeof me, "%s · tu mano: %d", lives_text(s_game.lives[0]), tyu_hand_value(s_game.hands[0]));
	table_draw_me(&s_table, me);
	table_draw_bubbles(&s_table);

	if (s_game.phase == TYU_PHASE_GAME_OVER && banner_draw(s_end_time, 200, 112, 300, 64)) {
		int w = tyu_winner(&s_game);
		if (w == 0)
			gfx_text("¡Ganas la partida!", 200, 96, DEPTH_BANNER, 0.75f, COLOR_GOLD, C2D_AlignCenter);
		else
			gfx_textf(200, 96, DEPTH_BANNER, 0.65f, COLOR_RED, C2D_AlignCenter, "Gana %s", table_name(&s_table, w));
	}
}

static void tyu_draw_bottom(void)
{
	gfx_background(BG_TABLE_BOTTOM);
	table_draw_message(&s_table);
	button_draw(&s_btn_menu, false);
	bool mine = s_game.current == 0 && s_game.phase == TYU_PHASE_PLAYING;
	if (!tyu_alive(&s_game, 0) && !s_game.lost[0]) {
		gfx_text("Estás eliminado", BOTTOM_W / 2, 90, 0, 0.6f, COLOR_CREAM, C2D_AlignCenter);
	} else {
		float pulse = 0.75f + 0.25f * sinf(gfx_time() * 6.0f);
		for (int i = 0; i < TYU_CARDS; i++) {
			float x = mine_x(i), y = MINE_Y - (s_selected == i ? 12.0f : 0.0f);
			if (mine && s_cursor == i) {
				C2D_ImageTint tint;
				C2D_AlphaImageTint(&tint, pulse);
				gfx_sprite_ex(SPR_CARD_GLOW, x, y, MINE_SC, MINE_SC, 0.0f, 0, &tint);
			}
			gfx_card(s_game.hands[0][i], x, y, MINE_SC, 1.0f, 0.0f, 0, 0.0f, true);
		}
		gfx_textf(BOTTOM_W / 2, 30, 0, 0.5f, COLOR_GOLD, C2D_AlignCenter, "Tu mano vale %d",
			tyu_hand_value(s_game.hands[0]));
	}
	if (s_game.phase == TYU_PHASE_PLAYING) {
		float pulse = 0.75f + 0.25f * sinf(gfx_time() * 6.0f);
		for (int i = 0; i < TYU_CARDS; i++) {
			if (mine && s_cursor == 3 + i) {
				C2D_ImageTint tint;
				C2D_AlphaImageTint(&tint, pulse);
				gfx_sprite_ex(SPR_CARD_GLOW, table_x(i), TABLE_Y, TABLE_SC, TABLE_SC, 0.0f, 0, &tint);
			}
			gfx_card(s_game.table[i], table_x(i), TABLE_Y, TABLE_SC, 1.0f, 0.0f, 0, 0.0f, true);
		}
		gfx_text("Mesa", 52, TABLE_Y - 8, 0, 0.45f, COLOR_CREAM, C2D_AlignCenter);
		if (mine)
			gfx_text(s_selected < 0 ? "Elige tu carta" : "y luego la de la mesa", 268, TABLE_Y - 8, 0, 0.42f,
				C2D_Color32(200, 230, 200, 255), C2D_AlignCenter);
	}
	button_draw(&s_btn_all, s_btn_all.enabled);
	button_draw(&s_btn_knock, s_btn_knock.enabled);
}

/* Salir a medias desde la pausa: la próxima vez se empieza de cero */
static void scene_leave(void)
{
	s_started = false;
}

const Scene scene_treintayuna = { tyu_enter, tyu_update, tyu_draw_top, tyu_draw_bottom, scene_leave };
