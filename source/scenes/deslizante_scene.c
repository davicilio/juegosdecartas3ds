/* Puzle deslizante: las fichas en la pantalla táctil; arriba, cómo tiene
 * que quedar y los movimientos. */
#include <math.h>
#include <stdio.h>
#include <time.h>

#include "games/deslizante.h"
#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/gfx.h"
#include "ui/mii.h"
#include "ui/setup.h"
#include "ui/widgets.h"

static const char* const SIZES[] = { "3 x 3 (el 8)", "4 x 4 (el 15)", "5 x 5 (el 24)" };

static Deslizante s_game;
static bool s_started, s_in_setup, s_recorded;
static int s_size_idx = 1;
static Setup s_setup;
static float s_time, s_end_time;
static float s_off[DZ_MAX * DZ_MAX][2]; /* desplazamiento animado de cada ficha */
static Button s_btn_menu;

static float tile(void)
{
	return 200.0f / s_game.n;
}

static float grid_x(void)
{
	return (BOTTOM_W - s_game.n * tile()) / 2.0f;
}

#define GRID_Y 26.0f

static void dz_enter(void)
{
	if (!s_started) {
		s_started = true;
		s_in_setup = true;
		setup_init(&s_setup, "Puzle deslizante", "Desliza las fichas hacia el hueco hasta dejarlas en orden, de izquierda a derecha y de arriba abajo. Toca una ficha de la fila o columna del hueco y se mueven todas.");
		setup_add(&s_setup, "Tamaño", SIZES, 3, &s_size_idx);
	}
	button_set(&s_btn_menu, 262, 2, 54, 18, "Menú", C2D_Color32(120, 30, 35, 255), KEY_START);
}

static void begin(void)
{
	dz_init(&s_game, s_size_idx + 3, (uint32_t)time(NULL) ^ 0xD5);
	s_time = s_end_time = 0.0f;
	s_recorded = false;
	for (int i = 0; i < DZ_MAX * DZ_MAX; i++)
		s_off[i][0] = s_off[i][1] = 0.0f;
}

static void slide(int i)
{
	int before[DZ_MAX * DZ_MAX], n = s_game.n;
	for (int k = 0; k < n * n; k++)
		before[k] = s_game.cell[k];
	if (!dz_slide(&s_game, i)) {
		audio_play(SFX_ERROR);
		return;
	}
	/* Las fichas que se han movido salen de donde estaban */
	float t = tile();
	for (int k = 0; k < n * n; k++) {
		int v = s_game.cell[k];
		if (!v || before[k] == v)
			continue;
		for (int j = 0; j < n * n; j++)
			if (before[j] == v) {
				s_off[k][0] = (j % n - k % n) * t;
				s_off[k][1] = (j / n - k / n) * t;
			}
	}
	audio_play(SFX_CARD);
}

static SceneId dz_update(const Input* in, float dt)
{
	if (button_update(&s_btn_menu, in))
		return SCENE_MENU;
	if (s_in_setup) {
		if (setup_update(&s_setup, in)) {
			s_in_setup = false;
			begin();
		}
		return SCENE_DESLIZANTE;
	}
	Deslizante* g = &s_game;
	for (int k = 0; k < g->n * g->n; k++)
		for (int a = 0; a < 2; a++)
			s_off[k][a] = approach(s_off[k][a], 0.0f, 400.0f, dt);
	if (dz_solved(g)) {
		s_end_time += dt;
		if (!s_recorded) {
			s_recorded = true;
			profile_record_game(GAME_DESLIZANTE, true);
			audio_play(SFX_WIN);
		}
		if (s_end_time > 1.0f && ((in->down & KEY_A) || in->touch_down))
			begin();
		return SCENE_DESLIZANTE;
	}
	if (g->moves)
		s_time += dt;
	/* Cruceta: mueve hacia el hueco la ficha del lado contrario */
	int b = dz_blank(g), n = g->n;
	if ((in->repeat & KEY_LEFT) && b % n < n - 1)
		slide(b + 1);
	if ((in->repeat & KEY_RIGHT) && b % n > 0)
		slide(b - 1);
	if ((in->repeat & KEY_UP) && b / n < n - 1)
		slide(b + n);
	if ((in->repeat & KEY_DOWN) && b / n > 0)
		slide(b - n);
	if (in->touch_down) {
		float t = tile();
		int c = (int)floorf((in->tx - grid_x()) / t), r = (int)floorf((in->ty - GRID_Y) / t);
		if (c >= 0 && r >= 0 && c < n && r < n)
			slide(r * n + c);
	}
	return SCENE_DESLIZANTE;
}

static void draw_grid(float x0, float y0, float t, bool animated, float depth)
{
	const Deslizante* g = &s_game;
	int n = g->n;
	gfx_rrect(x0 - 5, y0 - 5, n * t + 10, n * t + 10, 8, depth, C2D_Color32(90, 55, 25, 255));
	gfx_rect(x0, y0, n * t, n * t, depth, C2D_Color32(50, 30, 15, 255));
	for (int k = 0; k < n * n; k++) {
		int v = animated ? g->cell[k] : (k < n * n - 1 ? k + 1 : 0);
		if (!v)
			continue;
		float x = x0 + k % n * t + (animated ? s_off[k][0] : 0), y = y0 + k / n * t + (animated ? s_off[k][1] : 0);
		bool right = animated && v == k + 1;
		gfx_rrect(x + 2, y + 3, t - 4, t - 4, 6, depth, C2D_Color32(0, 0, 0, 90));
		gfx_rrect(x + 1, y + 1, t - 4, t - 4, 6, depth, right ? C2D_Color32(80, 160, 80, 255) : C2D_Color32(200, 130, 60, 255));
		gfx_rrect(x + 3, y + 3, t - 8, t - 8, 5, depth, right ? C2D_Color32(110, 200, 110, 255) : C2D_Color32(240, 180, 100, 255));
		float sc = t / 70.0f;
		gfx_textf_plain(x + t / 2 - 1, y + t / 2 - 1 - 15.0f * sc, depth, sc, C2D_Color32(60, 30, 15, 255), C2D_AlignCenter, "%d", v);
	}
}

static void dz_draw_top(void)
{
	if (s_in_setup) {
		setup_draw_top(&s_setup);
		return;
	}
	const Deslizante* g = &s_game;
	gfx_background(BG_TABLE_TOP);
	gfx_text("Así tiene que quedar", 110, 14, DEPTH_UI, 0.5f, COLOR_GOLD, C2D_AlignCenter);
	draw_grid(30, 40, 160.0f / g->n, false, DEPTH_TABLE);
	gfx_panel(230, 50, 150, 120, DEPTH_UI, C2D_Color32(20, 30, 25, 220), C2D_Color32(150, 110, 40, 255));
	int t = (int)s_time;
	gfx_textf(305, 62, DEPTH_UI, 0.5f, COLOR_CREAM, C2D_AlignCenter, "Movimientos");
	gfx_textf(305, 82, DEPTH_UI, 0.8f, COLOR_GOLD, C2D_AlignCenter, "%d", g->moves);
	gfx_textf(305, 124, DEPTH_UI, 0.6f, COLOR_CREAM, C2D_AlignCenter, "%d:%02d", t / 60, t % 60);
	if (dz_solved(g) && banner_draw(s_end_time, 200, 120, 300, 60))
		gfx_textf(200, 104, DEPTH_BANNER, 0.6f, COLOR_GOLD, C2D_AlignCenter, "¡Resuelto en %d movimientos!", g->moves);
}

static void dz_draw_bottom(void)
{
	if (s_in_setup) {
		setup_draw_bottom(&s_setup);
		button_draw(&s_btn_menu, false);
		return;
	}
	gfx_background(BG_TABLE_BOTTOM);
	gfx_text(dz_solved(&s_game) ? "Toca para jugar otro" : "Toca una ficha o usa la cruceta", 6, 4, 0, 0.42f, COLOR_GOLD,
		C2D_AlignLeft);
	button_draw(&s_btn_menu, false);
	draw_grid(grid_x(), GRID_Y, tile(), true, 0);
}

/* Salir a medias desde la pausa: la próxima vez se empieza de cero */
static void scene_leave(void)
{
	s_started = false;
}

const Scene scene_deslizante = { dz_enter, dz_update, dz_draw_top, dz_draw_bottom, scene_leave };
