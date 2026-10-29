/* El burro: pasa cartas hasta juntar cuatro iguales y, cuando alguien pone la
 * mano en la mesa, ¡no seas el último! */
#include <math.h>
#include <stdio.h>
#include <time.h>

#include "games/burro.h"
#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/gfx.h"
#include "ui/table.h"
#include "ui/widgets.h"

#define CARD_SC 0.9f
#define CARD_Y  96.0f
#define SLAP_X  (BOTTOM_W / 2.0f)
#define SLAP_Y  172.0f

static const char WORD[] = "BURRO";

static Burro s_game;
static Table s_table;
static bool s_started;
static int s_sel = -1, s_cursor;
static float s_react[BUR_PLAYERS]; /* cuándo pone la mano cada CPU (-1 nunca) */
static float s_clock, s_end_time;
static BurPhase s_prev_phase;
static Button s_btn_pass, s_btn_next, s_btn_menu;

static float card_x(int i)
{
	return BOTTOM_W / 2.0f + (i - 1.5f) * 64.0f;
}

static void start_round(void)
{
	s_sel = -1;
	s_end_time = 0.0f;
	s_clock = 0.0f;
	for (int p = 0; p < BUR_PLAYERS; p++)
		s_react[p] = -1.0f;
	s_prev_phase = s_game.phase;
	table_message(&s_table, "Elige una carta para pasar a tu derecha");
}

static void bur_enter(void)
{
	if (!s_started) {
		table_init(&s_table, CPU_LOLA, CPU_MANOLO, CPU_CARMEN);
		bur_init(&s_game, (uint32_t)time(NULL) ^ 0xB0B0);
		s_started = true;
		start_round();
	}
	button_set(&s_btn_menu, 258, 6, 54, 20, "Menú", C2D_Color32(120, 30, 35, 255), KEY_START);
}

static float rnd(float a, float b)
{
	return a + (b - a) * (float)rng_range(&s_game.rng, 1000) / 1000.0f;
}

static void slap(int p)
{
	bool first = s_game.phase == BUR_PASSING;
	if (!bur_slap(&s_game, p))
		return;
	audio_play(SFX_CHIP);
	table_bubble(&s_table, p, first ? "¡Burro!" : "¡Mano!");
	if (first && s_game.phase == BUR_SLAP) {
		/* Los demás se dan cuenta, unos antes que otros */
		for (int q = 1; q < BUR_PLAYERS; q++)
			if (!s_game.slapped[q])
				s_react[q] = s_clock + rnd(0.45f, 1.7f);
		table_message(&s_table, "¡%s pone la mano en la mesa! Pulsa " GLYPH_Y " ya", p == 0 ? "Tú" : table_name(&s_table, p));
	}
}

static void do_pass(void)
{
	int choice[BUR_PLAYERS];
	choice[0] = s_sel;
	for (int p = 1; p < BUR_PLAYERS; p++)
		choice[p] = bur_ai_pass(&s_game, p);
	bur_pass(&s_game, choice);
	audio_play(SFX_CARD);
	s_sel = -1;
	int h = bur_holder(&s_game);
	if (h > 0 && s_react[h] < 0)
		s_react[h] = s_clock + rnd(0.5f, 1.3f);
	table_message(&s_table, h == 0 ? "¡Tienes cuatro iguales! ¡Pon la mano!" : "Elige otra carta para pasar");
}

static SceneId bur_update(const Input* in, float dt)
{
	if (button_update(&s_btn_menu, in))
		return SCENE_MENU;
	s_clock += dt;
	bool slap_pressed = (in->down & KEY_Y) ||
	                    (in->touch_down && fabsf(in->tx - SLAP_X) < 70 && fabsf(in->ty - SLAP_Y) < 18);
	if (s_game.phase == BUR_PASSING) {
		button_set(&s_btn_pass, 12, 205, 296, 28, GLYPH_A " Pasar la carta", C2D_Color32(40, 140, 60, 255), KEY_A);
		s_btn_pass.enabled = true;
		if (in->repeat & KEY_LEFT)
			s_cursor = (s_cursor + BUR_HAND - 1) % BUR_HAND;
		if (in->repeat & KEY_RIGHT)
			s_cursor = (s_cursor + 1) % BUR_HAND;
		if (in->down & KEY_B) {
			s_sel = s_cursor;
			audio_play(SFX_SELECT);
		}
		if (in->touch_down)
			for (int i = 0; i < BUR_HAND; i++)
				if (fabsf(in->tx - card_x(i)) < CARD_W * CARD_SC / 2 && fabsf(in->ty - CARD_Y) < CARD_H * CARD_SC / 2) {
					s_sel = s_cursor = i;
					audio_play(SFX_SELECT);
				}
		if (slap_pressed)
			slap(0);
		else if (button_update(&s_btn_pass, in)) {
			if (s_sel < 0)
				s_sel = s_cursor;
			do_pass();
		}
		/* La CPU que junta cuatro pone la mano */
		for (int p = 1; p < BUR_PLAYERS && s_game.phase == BUR_PASSING; p++)
			if (s_react[p] >= 0 && s_clock >= s_react[p])
				slap(p);
	} else if (s_game.phase == BUR_SLAP) {
		if (slap_pressed)
			slap(0);
		for (int p = 1; p < BUR_PLAYERS && s_game.phase == BUR_SLAP; p++)
			if (!s_game.slapped[p] && s_react[p] >= 0 && s_clock >= s_react[p])
				slap(p);
	} else {
		bool over = s_game.phase == BUR_GAME_OVER;
		button_set(&s_btn_next, 12, 205, 296, 28, over ? GLYPH_A " Otra partida" : GLYPH_A " Otra ronda",
			C2D_Color32(40, 140, 60, 255), KEY_A);
		s_end_time += dt;
		s_btn_next.enabled = s_end_time > 0.8f;
		if (button_update(&s_btn_next, in)) {
			if (over)
				bur_new_game(&s_game);
			else
				bur_new_round(&s_game);
			start_round();
		}
	}
	if (s_game.phase != s_prev_phase) {
		if (s_game.phase >= BUR_ROUND_OVER) {
			int l = s_game.round_loser;
			s_end_time = 0.0f;
			table_mood(&s_table, l, EXPR_SAD, 3.0f);
			for (int p = 0; p < BUR_PLAYERS; p++)
				if (p != l)
					table_mood(&s_table, p, EXPR_HAPPY, 2.0f);
			char word[8];
			snprintf(word, sizeof word, "%.*s", s_game.letters[l], WORD);
			if (s_game.false_slap)
				table_message(&s_table, "%s la mano sin tener cuatro iguales: %s", l == 0 ? "Pones" : "Pone", word);
			else
				table_message(&s_table, "%s el último: %s", l == 0 ? "Eres" : table_name(&s_table, l), word);
			audio_play(l == 0 ? SFX_LOSE : SFX_WIN);
			if (s_game.phase == BUR_GAME_OVER)
				profile_record_game(GAME_BURRO, bur_loser(&s_game) != 0);
		}
		s_prev_phase = s_game.phase;
	}
	table_update(&s_table, dt);
	return SCENE_BURRO;
}

static void draw_letters(float x, float y, int n, float depth)
{
	for (int i = 0; i < BUR_WORD; i++) {
		char c[2] = { WORD[i], 0 };
		gfx_text(c, x + (i - 2) * 11, y, depth, 0.5f, i < n ? COLOR_RED : C2D_Color32(70, 110, 80, 255), C2D_AlignCenter);
	}
}

static void bur_draw_top(void)
{
	gfx_background(BG_TABLE_TOP);
	for (int p = 1; p < BUR_PLAYERS; p++) {
		bool slapped = s_game.phase != BUR_PASSING && s_game.slapped[p];
		table_draw_seat(&s_table, p, slapped ? 0 : BUR_HAND, false, " ");
		draw_letters(table_seat_x(p), table_seat_y(p) + 36, s_game.letters[p], DEPTH_UI + 0.1f);
		if (slapped)
			gfx_text("¡Mano!", table_seat_x(p), table_seat_y(p) + 54, DEPTH_UI, 0.45f, COLOR_GOLD, C2D_AlignCenter);
	}
	if (s_game.phase == BUR_SLAP) {
		float pulse = 0.8f + 0.2f * sinf(gfx_time() * 12.0f);
		gfx_text("¡Mano a la mesa!", 200, 110, DEPTH_BANNER, 0.9f * pulse, COLOR_GOLD, C2D_AlignCenter);
	}
	/* Al acabar la ronda se ven las cartas de quien juntó las cuatro */
	int h = bur_holder(&s_game);
	if (s_game.phase >= BUR_ROUND_OVER && h > 0)
		for (int i = 0; i < BUR_HAND; i++)
			gfx_card(s_game.hands[h][i], 152 + i * 32, 118, 0.5f, 1.0f, 0.0f, DEPTH_CARDS, 0.0f, true);
	table_draw_me(&s_table, "");
	draw_letters(110, 208, s_game.letters[0], DEPTH_UI);
	table_draw_bubbles(&s_table);
	if (s_game.phase == BUR_GAME_OVER && banner_draw(s_end_time, 200, 112, 300, 60)) {
		int l = bur_loser(&s_game);
		if (l == 0)
			gfx_text("¡Eres el burro!", 200, 98, DEPTH_BANNER, 0.7f, COLOR_RED, C2D_AlignCenter);
		else
			gfx_textf(200, 98, DEPTH_BANNER, 0.62f, COLOR_GOLD, C2D_AlignCenter, "%s es el burro", table_name(&s_table, l));
	}
}

static void bur_draw_bottom(void)
{
	gfx_background(BG_TABLE_BOTTOM);
	table_draw_message(&s_table);
	button_draw(&s_btn_menu, false);
	if (s_game.phase == BUR_PASSING)
		gfx_text("Elige la carta (toca o cruceta) y pásala con " GLYPH_A, BOTTOM_W / 2, 30, 0, 0.45f, C2D_Color32(200, 230, 200, 255),
			C2D_AlignCenter);
	float pulse = 0.75f + 0.25f * sinf(gfx_time() * 6.0f);
	for (int i = 0; i < BUR_HAND; i++) {
		float x = card_x(i), y = CARD_Y - (i == s_sel ? 14.0f : 0.0f);
		if (i == s_cursor && s_game.phase == BUR_PASSING) {
			C2D_ImageTint tint;
			C2D_AlphaImageTint(&tint, pulse);
			gfx_sprite_ex(SPR_CARD_GLOW, x, y, CARD_SC, CARD_SC, 0.0f, 0, &tint);
		}
		gfx_card(s_game.hands[0][i], x, y, CARD_SC, 1.0f, 0.0f, 0, 0.0f, true);
	}
	/* Botón de poner la mano */
	bool armed = s_game.phase <= BUR_SLAP && !(s_game.phase == BUR_SLAP && s_game.slapped[0]);
	gfx_rrect(SLAP_X - 70, SLAP_Y - 16, 140, 32, 10, 0,
		armed ? C2D_Color32(200, 40, 45, 255) : C2D_Color32(90, 90, 90, 255));
	gfx_text(GLYPH_Y " ¡Mano a la mesa!", SLAP_X, SLAP_Y - 9, 0, 0.5f, COLOR_CREAM, C2D_AlignCenter);
	if (s_game.phase == BUR_PASSING)
		button_draw(&s_btn_pass, s_btn_pass.enabled);
	else if (s_game.phase >= BUR_ROUND_OVER)
		button_draw(&s_btn_next, s_btn_next.enabled);
}

const Scene scene_burro = { bur_enter, bur_update, bur_draw_top, bur_draw_bottom };
