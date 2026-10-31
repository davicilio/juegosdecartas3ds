/* Parejas con la baraja española: las cartas boca abajo en la pantalla táctil. */
#include <math.h>
#include <stdio.h>
#include <time.h>

#include "games/parejas.h"
#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/gfx.h"
#include "ui/mii.h"
#include "ui/widgets.h"

#define RIVAL CPU_LOLA

static const char* const LEVEL_NAME[3] = { "Despistada", "Normal", "Memoria de elefante" };
static const int LEVEL_MEMORY[3] = { 25, 55, 90 };

typedef enum { MODE_SETUP, MODE_PLAY } Mode;

static Parejas s_game;
static bool s_started, s_recorded;
static Mode s_mode;
static int s_players = 2, s_level = 1, s_pairs = 12;
static float s_flip[PJ_MAX];  /* 0 boca abajo, 1 boca arriba (animado) */
static float s_show_timer, s_cpu_timer, s_end_time;
static int s_cursor;
static Mood s_mood[2];
static Button s_btn_players, s_btn_level, s_btn_size, s_btn_play, s_btn_menu;

static int cols(void)
{
	return s_game.n > 16 ? 6 : 4;
}

static float card_scale(void)
{
	return s_game.n > 16 ? 0.5f : 0.52f;
}

static void grid_pos(int i, float* x, float* y)
{
	int c = cols(), rows = s_game.n / c;
	float cw = s_game.n > 16 ? 52.0f : 66.0f, ch = 50.0f;
	float x0 = (BOTTOM_W - c * cw) / 2.0f + cw / 2, y0 = 24 + (210 - rows * ch) / 2.0f + ch / 2;
	*x = x0 + (i % c) * cw;
	*y = y0 + (i / c) * ch;
}

static void pj_enter(void)
{
	if (!s_started) {
		s_started = true;
		s_mode = MODE_SETUP;
	}
	button_set(&s_btn_menu, 262, 2, 54, 20, "Menú", C2D_Color32(120, 30, 35, 255), KEY_START);
}

static void start(void)
{
	pj_init(&s_game, s_pairs, s_players, LEVEL_MEMORY[s_level], (uint32_t)time(NULL) ^ 0x9A4E);
	for (int i = 0; i < PJ_MAX; i++)
		s_flip[i] = 0.0f;
	s_show_timer = s_cpu_timer = s_end_time = 0.0f;
	s_recorded = false;
	s_cursor = 0;
	s_mode = MODE_PLAY;
}

static void flip(int i)
{
	if (pj_flip(&s_game, i)) {
		audio_play(SFX_FLIP);
		s_show_timer = 0.0f;
	}
}

static SceneId setup_update(const Input* in)
{
	static char players_label[32], level_label[40], size_label[24];
	snprintf(players_label, sizeof players_label, GLYPH_X " %s", s_players == 1 ? "Solo" : "Contra un rival");
	snprintf(level_label, sizeof level_label, GLYPH_Y " %s", LEVEL_NAME[s_level]);
	snprintf(size_label, sizeof size_label, "%d parejas " GLYPH_R, s_pairs);
	button_set(&s_btn_players, 40, 60, 240, 30, players_label, C2D_Color32(60, 90, 160, 255), KEY_X);
	button_set(&s_btn_level, 40, 100, 240, 30, level_label, C2D_Color32(200, 140, 30, 255), KEY_Y);
	button_set(&s_btn_size, 40, 140, 240, 30, size_label, C2D_Color32(40, 110, 120, 255), KEY_R);
	button_set(&s_btn_play, 60, 200, 200, 30, GLYPH_A " Jugar", C2D_Color32(40, 140, 60, 255), KEY_A);
	s_btn_level.enabled = s_players == 2;
	if (button_update(&s_btn_players, in))
		s_players = 3 - s_players;
	else if (button_update(&s_btn_level, in))
		s_level = (s_level + 1) % 3;
	else if (button_update(&s_btn_size, in))
		s_pairs = s_pairs == 12 ? 8 : 12;
	else if (button_update(&s_btn_play, in))
		start();
	return SCENE_PAREJAS;
}

static SceneId pj_update(const Input* in, float dt)
{
	if (button_update(&s_btn_menu, in))
		return SCENE_MENU;
	for (int p = 0; p < 2; p++)
		mood_update(&s_mood[p], dt);
	if (s_mode == MODE_SETUP)
		return setup_update(in);
	Parejas* g = &s_game;
	/* Animación de giro */
	for (int i = 0; i < g->n; i++) {
		bool up = g->taken[i] || i == g->first || i == g->second;
		s_flip[i] = approach(s_flip[i], up ? 1.0f : 0.0f, 4.0f, dt);
	}
	switch (g->phase) {
	case PJ_SHOW:
		s_show_timer += dt;
		if (s_show_timer > (g->last_match ? 0.8f : 1.3f)) {
			int who = g->current;
			bool match = g->last_match;
			pj_resolve(g);
			if (match) {
				audio_play(SFX_CHIP);
				mood_set(&s_mood[who], EXPR_HAPPY, 1.5f);
			}
		}
		break;
	case PJ_OVER:
		s_end_time += dt;
		if (!s_recorded) {
			s_recorded = true;
			bool won = pj_winner(g) == 0;
			profile_record_game(GAME_PAREJAS, won);
			audio_play(won ? SFX_WIN : SFX_LOSE);
			mood_set(&s_mood[0], won ? EXPR_HAPPY : EXPR_SAD, 30.0f);
			mood_set(&s_mood[1], won ? EXPR_SAD : EXPR_HAPPY, 30.0f);
		}
		if (s_end_time > 1.0f && ((in->down & KEY_A) || in->touch_down))
			s_mode = MODE_SETUP;
		break;
	default:
		if (g->current == 1) {
			s_cpu_timer += dt;
			if (s_cpu_timer > 0.8f) {
				s_cpu_timer = 0.0f;
				flip(pj_ai_pick(g));
			}
			break;
		}
		if (in->repeat & KEY_LEFT)
			s_cursor = (s_cursor + g->n - 1) % g->n;
		if (in->repeat & KEY_RIGHT)
			s_cursor = (s_cursor + 1) % g->n;
		if (in->repeat & KEY_UP)
			s_cursor = (s_cursor + g->n - cols()) % g->n;
		if (in->repeat & KEY_DOWN)
			s_cursor = (s_cursor + cols()) % g->n;
		if (in->down & KEY_A)
			flip(s_cursor);
		if (in->touch_down) {
			for (int i = 0; i < g->n; i++) {
				float x, y;
				grid_pos(i, &x, &y);
				if (fabsf(in->tx - x) < CARD_W * card_scale() / 2 && fabsf(in->ty - y) < CARD_H * card_scale() / 2) {
					s_cursor = i;
					flip(i);
				}
			}
		}
		break;
	}
	return SCENE_PAREJAS;
}

static void pj_draw_top(void)
{
	const Parejas* g = &s_game;
	gfx_background(BG_TABLE_TOP);
	if (s_mode == MODE_SETUP) {
		gfx_text("Parejas", 200, 30, DEPTH_UI, 0.9f, COLOR_GOLD, C2D_AlignCenter);
		static const Card SHOW[3] = { { SUIT_COPAS, 1 }, { SUIT_OROS, 12 }, { SUIT_COPAS, 1 } };
		for (int i = 0; i < 3; i++)
			gfx_card(SHOW[i], 140 + i * 60, 128, 0.9f, i == 1 ? 0.0f : 1.0f, (i - 1) * 0.1f, DEPTH_CARDS, 0.0f, true);
		gfx_text("Encuentra las dos cartas iguales", 200, 196, DEPTH_UI, 0.5f, COLOR_CREAM, C2D_AlignCenter);
		return;
	}
	/* Marcadores */
	for (int p = 0; p < g->players; p++) {
		float x = g->players == 1 ? 200 : 110 + p * 180;
		bool turn = g->current == p && g->phase != PJ_OVER;
		gfx_panel(x - 70, 10, 140, 64, DEPTH_UI, C2D_Color32(10, 50, 25, 220), turn ? COLOR_GOLD : C2D_Color32(20, 70, 35, 255));
		const Avatar* a = p == 0 ? &profile_get()->avatar : cpu_avatar(RIVAL);
		mii_draw(a, x - 44, 42, 18, DEPTH_UI, mood_expr(&s_mood[p]), false);
		gfx_text(p == 0 ? a->name : cpu_name(RIVAL), x + 20, 18, DEPTH_UI, 0.45f, COLOR_CREAM, C2D_AlignCenter);
		gfx_textf(x + 20, 36, DEPTH_UI, 0.7f, COLOR_GOLD, C2D_AlignCenter, "%d", g->score[p]);
	}
	gfx_textf(200, 82, DEPTH_UI, 0.45f, COLOR_CREAM, C2D_AlignCenter, "Intentos: %d", g->moves);
	/* Las dos cartas levantadas, en grande */
	if (g->first >= 0) {
		gfx_card(g->card[g->first], 160, 160, 0.95f, s_flip[g->first], -0.05f, DEPTH_CARDS, 0.0f, true);
		if (g->second >= 0)
			gfx_card(g->card[g->second], 240, 160, 0.95f, s_flip[g->second], 0.05f, DEPTH_CARDS, 0.0f, true);
	}
	if (g->phase == PJ_SHOW && s_show_timer > 0.3f)
		gfx_text(g->last_match ? "¡Pareja!" : "No son iguales", 200, 214, DEPTH_BANNER, 0.55f,
			g->last_match ? COLOR_GOLD : COLOR_CREAM, C2D_AlignCenter);
	if (g->phase == PJ_OVER && banner_draw(s_end_time, 200, 150, 320, 70)) {
		int w = pj_winner(g);
		if (g->players == 1)
			gfx_textf(200, 128, DEPTH_BANNER, 0.62f, COLOR_GOLD, C2D_AlignCenter, "¡Todas! En %d intentos", g->moves);
		else if (w < 0)
			gfx_text("¡Empate!", 200, 128, DEPTH_BANNER, 0.7f, COLOR_CREAM, C2D_AlignCenter);
		else
			gfx_textf(200, 128, DEPTH_BANNER, 0.62f, w == 0 ? COLOR_GOLD : COLOR_CREAM, C2D_AlignCenter, "%s",
				w == 0 ? "¡Ganas!" : "Gana tu rival");
		gfx_text("Toca para seguir", 200, 156, DEPTH_BANNER, 0.42f, COLOR_CREAM, C2D_AlignCenter);
	}
}

static void pj_draw_bottom(void)
{
	const Parejas* g = &s_game;
	gfx_background(BG_TABLE_BOTTOM);
	button_draw(&s_btn_menu, false);
	if (s_mode == MODE_SETUP) {
		gfx_text("Elige cómo jugar", BOTTOM_W / 2, 24, 0, 0.55f, COLOR_GOLD, C2D_AlignCenter);
		button_draw(&s_btn_players, false);
		if (s_players == 2)
			button_draw(&s_btn_level, false);
		button_draw(&s_btn_size, false);
		button_draw(&s_btn_play, true);
		return;
	}
	gfx_text(g->phase == PJ_OVER ? "Fin" : g->current == 0 ? "Te toca: levanta dos cartas" : "Juega tu rival", 8, 4,
		0, 0.45f, COLOR_GOLD, C2D_AlignLeft);
	float sc = card_scale();
	for (int i = 0; i < g->n; i++) {
		float x, y;
		grid_pos(i, &x, &y);
		if (g->taken[i] && i != g->first && i != g->second) {
			/* Hueco de la carta ya emparejada, con el color de quien se la llevó */
			gfx_rrect(x - CARD_W * sc / 2, y - CARD_H * sc / 2, CARD_W * sc, CARD_H * sc, 4, 0,
				g->owner[i] == 0 ? C2D_Color32(255, 215, 90, 60) : C2D_Color32(120, 160, 255, 60));
			continue;
		}
		gfx_card(g->card[i], x, y, sc, s_flip[i], 0.0f, 0, 0.0f, false);
		if (i == s_cursor && g->current == 0 && g->phase != PJ_OVER) {
			float pulse = 0.5f + 0.5f * sinf(gfx_time() * 6.0f);
			gfx_rect(x - CARD_W * sc / 2, y + CARD_H * sc / 2 + 1, CARD_W * sc, 2, 0,
				C2D_Color32(255, 215, 90, (u8)(150 + 100 * pulse)));
		}
	}
}

/* Salir a medias desde la pausa: la próxima vez se empieza de cero */
static void scene_leave(void)
{
	s_started = false;
}

const Scene scene_parejas = { pj_enter, pj_update, pj_draw_top, pj_draw_bottom, scene_leave };
