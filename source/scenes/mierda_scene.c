/* La mierda: no te quedes el último con cartas. */
#include <math.h>
#include <stdio.h>
#include <time.h>

#include "games/mierda.h"
#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/gfx.h"
#include "ui/table.h"
#include "ui/widgets.h"

#define HAND_SC  0.5f
#define TABLE_SC 0.5f
#define TABLE_Y  168.0f

static Mierda s_game;
static Table s_table;
static bool s_started;
static int s_cursor;
static float s_cpu_timer, s_end_time;
static MiPhase s_prev_phase;
static bool s_recorded_out;
static Button s_btn_play, s_btn_take, s_btn_menu;

static float table_x(int i)
{
	return BOTTOM_W / 2.0f + (i - 1) * 40.0f;
}

static void new_game(void)
{
	mi_new_game(&s_game);
	s_cursor = 0;
	s_cpu_timer = s_end_time = 0.0f;
	s_prev_phase = s_game.phase;
	s_recorded_out = false;
	table_message(&s_table, "Empieza %s", s_game.current == 0 ? "tú" : table_name(&s_table, s_game.current));
}

static void mi_enter(void)
{
	if (!s_started) {
		table_init(&s_table, CPU_PACO, CPU_LUCIA, CPU_PEPE);
		mi_init(&s_game, (uint32_t)time(NULL) ^ 0x3E7D);
		s_started = true;
		new_game();
	}
	button_set(&s_btn_menu, 258, 6, 54, 20, "Menú", C2D_Color32(120, 30, 35, 255), KEY_START);
}

static int source_count(void)
{
	MiSource s = mi_source(&s_game, 0);
	return s == MI_FROM_HAND ? s_game.hand_n[0] : s == MI_FROM_NONE ? 0 : 3;
}

static void after(int p)
{
	char name[32];
	card_describe(s_game.last_card, name, sizeof name);
	const char* who = p == 0 ? "Tú" : table_name(&s_table, p);
	if (s_game.last_blind_fail) {
		table_message(&s_table, "%s: sale %s a ciegas y no vale. ¡Al montón!", who, name);
		table_mood(&s_table, p, EXPR_SAD, 1.5f);
		audio_play(SFX_LOSE);
	} else if (s_game.last_took) {
		table_message(&s_table, "%s %s el montón", who, p == 0 ? "te llevas" : "se lleva");
		table_mood(&s_table, p, EXPR_SAD, 1.5f);
		audio_play(SFX_FLIP);
	} else if (s_game.last_burn) {
		table_message(&s_table, "¡%s quema el montón!", who);
		table_bubble(&s_table, p, "¡Quemado!");
		table_mood(&s_table, p, EXPR_HAPPY, 1.5f);
		audio_play(SFX_CHIP);
	} else {
		audio_play(SFX_CARD);
		if (s_game.last_count > 1)
			table_message(&s_table, "%s %d cartas iguales", p == 0 ? "Echas" : who, s_game.last_count);
		else if (s_game.last_card.rank == 2)
			table_message(&s_table, "%s un 2: vale cualquier carta", p == 0 ? "Echas" : who);
		else
			table_message(&s_table, "%s %s", p == 0 ? "Echas" : who, name);
	}
	if (s_game.out[p] && p != 0)
		table_bubble(&s_table, p, "¡Fuera!");
}

static SceneId mi_update(const Input* in, float dt)
{
	if (button_update(&s_btn_menu, in))
		return SCENE_MENU;
	if (s_game.phase == MI_PLAYING) {
		bool mine = s_game.current == 0;
		MiSource src = mi_source(&s_game, 0);
		button_set(&s_btn_play, 12, 205, 144, 28, src == MI_FROM_DOWN ? GLYPH_A " A ciegas" : GLYPH_A " Echar",
			C2D_Color32(40, 140, 60, 255), KEY_A);
		button_set(&s_btn_take, 164, 205, 144, 28, GLYPH_X " Me lo llevo", C2D_Color32(190, 60, 50, 255), KEY_X);
		s_btn_play.enabled = mine && !mi_must_take(&s_game, 0);
		s_btn_take.enabled = mine && mi_must_take(&s_game, 0);
		if (mine) {
			int n = source_count();
			if (n > 0) {
				if (in->repeat & KEY_LEFT)
					s_cursor = (s_cursor + n - 1) % n;
				if (in->repeat & KEY_RIGHT)
					s_cursor = (s_cursor + 1) % n;
				if (s_cursor >= n)
					s_cursor = n - 1;
				/* El cursor salta a algo que se pueda echar */
				if (!mi_can_play(&s_game, 0, s_cursor))
					for (int i = 0; i < n; i++)
						if (mi_can_play(&s_game, 0, i)) {
							s_cursor = i;
							break;
						}
			}
			int t = -1;
			if (src == MI_FROM_HAND)
				t = hand_touched(in, n, HAND_SC);
			else if (in->touch_down)
				for (int i = 0; i < 3; i++)
					if (fabsf(in->tx - table_x(i)) < CARD_W * TABLE_SC / 2 && fabsf(in->ty - TABLE_Y) < CARD_H * TABLE_SC / 2)
						t = i;
			int pick = t >= 0 ? t : button_update(&s_btn_play, in) ? s_cursor : -1;
			if (pick >= 0) {
				if (mi_play(&s_game, pick))
					after(0);
				else {
					audio_play(SFX_ERROR);
					table_message(&s_table, "Esa no vale: tiene que ser igual o más alta (o un 2 o una sota)");
				}
			} else if (button_update(&s_btn_take, in) && mi_take(&s_game)) {
				after(0);
			}
		} else {
			s_cpu_timer += dt;
			if (s_cpu_timer > 0.85f) {
				s_cpu_timer = 0.0f;
				int p = s_game.current;
				int i = mi_ai_choose(&s_game, p);
				if (i < 0 ? mi_take(&s_game) : mi_play(&s_game, i))
					after(p);
			}
		}
		if (s_game.out[0] && !s_recorded_out) {
			s_recorded_out = true;
			table_message(&s_table, "¡Te has librado! Puesto %d", s_game.out_n);
			audio_play(SFX_WIN);
		}
	} else {
		button_set(&s_btn_play, 12, 205, 296, 28, GLYPH_A " Otra partida", C2D_Color32(40, 140, 60, 255), KEY_A);
		s_end_time += dt;
		s_btn_play.enabled = s_end_time > 0.8f;
		if (button_update(&s_btn_play, in))
			new_game();
	}
	if (s_game.phase != s_prev_phase) {
		if (s_game.phase == MI_GAME_OVER) {
			s_end_time = 0.0f;
			int l = s_game.loser;
			for (int p = 0; p < MI_PLAYERS; p++)
				table_mood(&s_table, p, p == l ? EXPR_SAD : EXPR_HAPPY, 5.0f);
			profile_record_game(GAME_MIERDA, s_game.out_order[0] == 0);
			audio_play(l == 0 ? SFX_LOSE : SFX_WIN);
		}
		s_prev_phase = s_game.phase;
	}
	table_update(&s_table, dt);
	return SCENE_MIERDA;
}

static void mi_draw_top(void)
{
	gfx_background(BG_TABLE_TOP);
	for (int p = 1; p < MI_PLAYERS; p++) {
		char info[32];
		if (s_game.out[p])
			snprintf(info, sizeof info, "¡Fuera!");
		else
			snprintf(info, sizeof info, "%d en mano", s_game.hand_n[p]);
		bool turn = s_game.current == p && s_game.phase == MI_PLAYING;
		table_draw_seat(&s_table, p, s_game.hand_n[p] > 8 ? 8 : s_game.hand_n[p], turn, info);
		/* Sus cartas de la mesa */
		float x0 = table_seat_x(p) + (p == 2 ? 70 : 0), y0 = table_seat_y(p) + (p == 2 ? 10 : 70);
		for (int i = 0; i < 3; i++) {
			float x = x0 + (i - 1) * 18;
			if (s_game.down_has[p][i])
				gfx_card_back(x, y0, 0.28f, 0.28f, 0.0f, DEPTH_CARDS, NULL);
			if (s_game.up_has[p][i])
				gfx_card(s_game.up[p][i], x + 2, y0 + 4, 0.28f, 1.0f, 0.0f, DEPTH_CARDS + 0.1f, 0.0f, false);
		}
	}
	/* Mazo, montón y quemadas */
	if (s_game.deck_n > 0) {
		gfx_card_back(150, 116, 0.55f, 0.55f, 0.0f, DEPTH_CARDS, NULL);
		gfx_textf(150, 150, DEPTH_UI, 0.42f, COLOR_CREAM, C2D_AlignCenter, "Mazo %d", s_game.deck_n);
	}
	int n = s_game.pile_n;
	for (int k = n > 3 ? n - 3 : 0; k < n; k++)
		gfx_card(s_game.pile[k], 214 + (k - n + 1) * 8, 116, 0.6f, 1.0f, (k % 3 - 1) * 0.08f, DEPTH_CARDS + k * 0.01f, 0.0f,
			k == n - 1);
	gfx_textf(214, 152, DEPTH_UI, 0.42f, COLOR_CREAM, C2D_AlignCenter, n ? "Montón %d" : "Montón vacío", n);
	if (s_game.burned)
		gfx_textf(290, 180, DEPTH_UI, 0.4f, C2D_Color32(200, 230, 200, 255), C2D_AlignCenter, "Quemadas: %d",
			s_game.burned);
	table_draw_me(&s_table, s_game.out[0] ? "¡Fuera!" : "");
	table_draw_bubbles(&s_table);
	if (s_game.phase == MI_GAME_OVER && banner_draw(s_end_time, 200, 112, 320, 70)) {
		int l = s_game.loser;
		if (l == 0)
			gfx_text("¡Eres la mierda!", 200, 92, DEPTH_BANNER, 0.7f, COLOR_RED, C2D_AlignCenter);
		else
			gfx_textf(200, 92, DEPTH_BANNER, 0.62f, COLOR_GOLD, C2D_AlignCenter, "%s es la mierda", table_name(&s_table, l));
		int pos = 0;
		for (int i = 0; i < s_game.out_n; i++)
			if (s_game.out_order[i] == 0)
				pos = i + 1;
		if (pos)
			gfx_textf(200, 118, DEPTH_BANNER, 0.5f, COLOR_CREAM, C2D_AlignCenter, "Has acabado el %dº", pos);
	}
}

static void mi_draw_bottom(void)
{
	gfx_background(BG_TABLE_BOTTOM);
	table_draw_message(&s_table);
	button_draw(&s_btn_menu, false);
	bool mine = s_game.current == 0 && s_game.phase == MI_PLAYING;
	MiSource src = mi_source(&s_game, 0);
	if (mine)
		gfx_text(mi_must_take(&s_game, 0) ? "No puedes echar nada: te llevas el montón"
		         : src == MI_FROM_DOWN   ? "Elige una carta de boca abajo"
		         : src == MI_FROM_UP     ? "Juega con tus cartas de la mesa"
		                                 : "¡Tu turno!",
			BOTTOM_W / 2, 28, 0, 0.47f, COLOR_GOLD, C2D_AlignCenter);
	int n = s_game.hand_n[0];
	bool playable[MI_MAX];
	for (int i = 0; i < n; i++)
		playable[i] = !mine || mi_can_play(&s_game, 0, i);
	hand_draw(s_game.hand[0], n, HAND_SC, NULL, mine && src == MI_FROM_HAND ? s_cursor : -1, playable);
	/* Tus cartas de la mesa: boca abajo y, encima, boca arriba */
	float pulse = 0.75f + 0.25f * sinf(gfx_time() * 6.0f);
	for (int i = 0; i < 3; i++) {
		float x = table_x(i);
		bool sel = mine && src != MI_FROM_HAND && i == s_cursor;
		if (sel) {
			C2D_ImageTint tint;
			C2D_AlphaImageTint(&tint, pulse);
			gfx_sprite_ex(SPR_CARD_GLOW, x, TABLE_Y, TABLE_SC, TABLE_SC, 0.0f, 0, &tint);
		}
		if (s_game.down_has[0][i])
			gfx_card_back(x, TABLE_Y, TABLE_SC, TABLE_SC, 0.0f, 0, NULL);
		if (s_game.up_has[0][i])
			gfx_card(s_game.up[0][i], x + 3, TABLE_Y - 4, TABLE_SC, 1.0f, 0.0f, 0,
				mine && src == MI_FROM_UP && !mi_can_play(&s_game, 0, i) ? 0.45f : 0.0f, true);
	}
	gfx_text("El 2 vale siempre · la sota quema el montón", BOTTOM_W / 2, 132, 0, 0.38f, C2D_Color32(200, 230, 200, 255), C2D_AlignCenter);
	if (s_game.phase == MI_PLAYING) {
		button_draw(&s_btn_play, s_btn_play.enabled);
		button_draw(&s_btn_take, s_btn_take.enabled);
	} else {
		button_draw(&s_btn_play, s_btn_play.enabled);
	}
}

/* Salir a medias desde la pausa: la próxima vez se empieza de cero */
static void scene_leave(void)
{
	s_started = false;
}

const Scene scene_mierda = { mi_enter, mi_update, mi_draw_top, mi_draw_bottom, scene_leave };
