/* Mancala: el tablero de madera con los hoyos; tú juegas la fila de abajo. */
#include <math.h>
#include <stdio.h>
#include <time.h>

#include "games/mancala.h"
#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/gfx.h"
#include "ui/mii.h"
#include "ui/setup.h"
#include "ui/widgets.h"

#define RIVAL CPU_CARMEN

static const char* const LEVELS[] = { "Fácil", "Normal", "Difícil" };

static Mancala s_game;
static bool s_started, s_in_setup, s_recorded;
static int s_level = 1, s_cursor;
static Setup s_setup;
static float s_cpu_timer, s_end_time;
static char s_message[80];
static Mood s_mood;
static Button s_btn_menu;

typedef struct {
	float x, y, w, h;
} Area;

static const Area TOP = { 20, 50, 360, 140 };
static const Area BOT = { 4, 60, 312, 130 };

static void mc_enter(void)
{
	if (!s_started) {
		s_started = true;
		s_in_setup = true;
		setup_init(&s_setup, "Mancala", "Siembra las semillas de tus hoyos una a una. Si la última cae en tu granero, repites; si cae en un hoyo tuyo vacío, te llevas las de enfrente.");
		setup_add(&s_setup, "Dificultad", LEVELS, 3, &s_level);
	}
	button_set(&s_btn_menu, 262, 2, 54, 18, "Menú", C2D_Color32(120, 30, 35, 255), KEY_START);
}

static void begin(void)
{
	mc_init(&s_game, s_level, (uint32_t)time(NULL) ^ 0x3A4C);
	s_recorded = false;
	s_cpu_timer = s_end_time = 0.0f;
	snprintf(s_message, sizeof s_message, "Te toca: toca uno de tus hoyos (abajo)");
}

/* Centro del hoyo k (0..13) en un área */
static void pit_pos(const Area* a, int k, float* x, float* y)
{
	float w = a->w / 8.0f;
	if (k == 6) {
		*x = a->x + 7.5f * w;
		*y = a->y + a->h / 2;
	} else if (k == 13) {
		*x = a->x + 0.5f * w;
		*y = a->y + a->h / 2;
	} else if (k < 6) {
		*x = a->x + (k + 1.5f) * w;
		*y = a->y + a->h * 0.75f;
	} else {
		*x = a->x + (12 - k + 1.5f) * w;
		*y = a->y + a->h * 0.25f;
	}
}

static void after_play(int p)
{
	const Mancala* g = &s_game;
	audio_play(g->captured ? SFX_CHIP : SFX_CARD);
	if (g->captured)
		snprintf(s_message, sizeof s_message, "%s", p == 0 ? "¡Capturas las semillas de enfrente!" : "Tu rival captura");
	else if (g->again)
		snprintf(s_message, sizeof s_message, "%s", p == 0 ? "¡Al granero! Vuelves a jugar" : "Tu rival repite");
	else
		snprintf(s_message, sizeof s_message, "%s", g->current == 0 ? "Te toca" : "Juega tu rival");
	if (g->captured || g->again)
		mood_set(&s_mood, p == 1 ? EXPR_HAPPY : EXPR_SAD, 1.2f);
}

static SceneId mc_update(const Input* in, float dt)
{
	if (button_update(&s_btn_menu, in))
		return SCENE_MENU;
	mood_update(&s_mood, dt);
	if (s_in_setup) {
		if (setup_update(&s_setup, in)) {
			s_in_setup = false;
			begin();
		}
		return SCENE_MANCALA;
	}
	Mancala* g = &s_game;
	if (g->over) {
		s_end_time += dt;
		if (!s_recorded) {
			s_recorded = true;
			bool won = g->pit[6] > g->pit[13];
			profile_record_game(GAME_MANCALA, won);
			audio_play(won ? SFX_WIN : SFX_LOSE);
		}
		if (s_end_time > 1.2f && ((in->down & KEY_A) || in->touch_down))
			begin();
		return SCENE_MANCALA;
	}
	if (g->current == 1) {
		s_cpu_timer += dt;
		if (s_cpu_timer > 0.8f) {
			s_cpu_timer = 0.0f;
			mc_play(g, mc_ai(g));
			after_play(1);
		}
		return SCENE_MANCALA;
	}
	if (in->repeat & KEY_LEFT)
		s_cursor = (s_cursor + 5) % 6;
	if (in->repeat & KEY_RIGHT)
		s_cursor = (s_cursor + 1) % 6;
	int pick = (in->down & KEY_A) ? s_cursor : -1;
	if (in->touch_down)
		for (int i = 0; i < 6; i++) {
			float x, y;
			pit_pos(&BOT, i, &x, &y);
			if (fabsf(in->tx - x) < 19 && fabsf(in->ty - y) < 28) {
				s_cursor = i;
				pick = i;
			}
		}
	if (pick >= 0) {
		if (mc_play(g, pick))
			after_play(0);
		else
			audio_play(SFX_ERROR);
	}
	return SCENE_MANCALA;
}

static void draw_board(const Area* a, float depth, bool cursor)
{
	const Mancala* g = &s_game;
	gfx_rrect(a->x, a->y, a->w, a->h, 18, depth, C2D_Color32(110, 65, 30, 255));
	gfx_rrect(a->x + 4, a->y + 4, a->w - 8, a->h - 8, 15, depth, C2D_Color32(150, 95, 45, 255));
	float w = a->w / 8.0f;
	static const u32 SEED[5] = { 0xE84A4A, 0x4A90E8, 0xE8C84A, 0x5AC85A, 0xC86AD8 };
	for (int k = 0; k < MC_PITS; k++) {
		float x, y;
		pit_pos(a, k, &x, &y);
		bool store = k == 6 || k == 13;
		float rx = w * 0.42f, ry = store ? a->h * 0.42f : a->h * 0.2f;
		gfx_ellipse(x, y, rx, ry, depth, C2D_Color32(80, 45, 20, 255));
		gfx_ellipse(x, y + 1, rx - 2, ry - 2, depth, C2D_Color32(95, 55, 25, 255));
		if (cursor && k == s_cursor && g->current == 0 && !g->over)
			gfx_ellipse(x, y, rx + 2, ry + 2, depth, C2D_Color32(255, 215, 90, 70));
		if (k == g->last)
			gfx_ellipse(x, y, rx - 2, ry - 2, depth, C2D_Color32(255, 230, 150, 50));
		/* Semillas: hasta 12 a la vista, en espiral */
		int n = g->pit[k], shown = n > 12 ? 12 : n;
		for (int s = 0; s < shown; s++) {
			float ang = s * 2.4f, rr = 2.5f + sqrtf((float)s) * 3.2f;
			float sx = x + cosf(ang) * rr * (rx / 20.0f), sy = y + sinf(ang) * rr * (ry / 20.0f);
			gfx_ellipse(sx, sy, 2.6f, 2.6f, depth, gfx_rgb(SEED[(s + k) % 5], 255));
			gfx_ellipse(sx - 0.8f, sy - 0.8f, 0.9f, 0.9f, depth, C2D_Color32(255, 255, 255, 160));
		}
		gfx_textf(x, store ? y + ry - 18 : (k < 6 ? y + ry - 2 : y - ry - 14), depth, 0.45f, COLOR_CREAM, C2D_AlignCenter,
			"%d", n);
	}
}

static void mc_draw_top(void)
{
	if (s_in_setup) {
		setup_draw_top(&s_setup);
		return;
	}
	const Mancala* g = &s_game;
	gfx_background(BG_TABLE_TOP);
	draw_board(&TOP, DEPTH_TABLE, false);
	mii_draw(cpu_avatar(RIVAL), 200, 26, 14, DEPTH_UI, mood_expr(&s_mood), false);
	gfx_textf(240, 16, DEPTH_UI, 0.45f, COLOR_CREAM, C2D_AlignLeft, "%s · %s", cpu_name(RIVAL), LEVELS[g->level]);
	gfx_textf(200, 204, DEPTH_UI, 0.5f, COLOR_GOLD, C2D_AlignCenter, "Tú %d · %d tu rival", g->pit[6], g->pit[13]);
	if (g->over && banner_draw(s_end_time, 200, 120, 280, 60)) {
		int a = g->pit[6], b = g->pit[13];
		gfx_textf(200, 104, DEPTH_BANNER, 0.6f, COLOR_GOLD, C2D_AlignCenter, "%s %d a %d",
			a > b ? "¡Ganas!" : a < b ? "Pierdes" : "Empate", a, b);
	}
}

static void mc_draw_bottom(void)
{
	if (s_in_setup) {
		setup_draw_bottom(&s_setup);
		button_draw(&s_btn_menu, false);
		return;
	}
	gfx_background(BG_TABLE_BOTTOM);
	gfx_text(s_message, 6, 4, 0, 0.42f, COLOR_GOLD, C2D_AlignLeft);
	button_draw(&s_btn_menu, false);
	draw_board(&BOT, 0, true);
	gfx_text("Tu granero", 300, 196, 0, 0.4f, COLOR_CREAM, C2D_AlignRight);
	gfx_text("El de tu rival", 10, 196, 0, 0.4f, COLOR_CREAM, C2D_AlignLeft);
}

/* Salir a medias desde la pausa: la próxima vez se empieza de cero */
static void scene_leave(void)
{
	s_started = false;
}

const Scene scene_mancala = { mc_enter, mc_update, mc_draw_top, mc_draw_bottom, scene_leave };
