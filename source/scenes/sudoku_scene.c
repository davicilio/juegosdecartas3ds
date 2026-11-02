/* Sudoku: la cuadrícula táctil y el teclado de números abajo; arriba, la
 * cuadrícula grande con el tiempo y los fallos. */
#include <math.h>
#include <stdio.h>
#include <time.h>

#include "games/sudoku.h"
#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/gfx.h"
#include "ui/mii.h"
#include "ui/widgets.h"

#define B_CELL 22.0f
#define B_X    4.0f
#define B_Y    22.0f
#define T_CELL 25.0f
#define T_X    20.0f
#define T_Y    7.0f

static const char* const LEVEL_NAME[SD_LEVELS] = { "Fácil", "Medio", "Difícil" };

typedef enum { MODE_SETUP, MODE_PLAY } Mode;

static Sudoku s_game;
static bool s_started, s_recorded, s_notes;
static Mode s_mode;
static int s_level;
static int s_sel = 40, s_num = 1;
static float s_time, s_end_time;
static Button s_btn_num[9], s_btn_erase, s_btn_notes, s_btn_hint, s_btn_level[SD_LEVELS], s_btn_play, s_btn_menu;

static void sd_enter(void)
{
	if (!s_started) {
		s_started = true;
		s_mode = MODE_SETUP;
	}
	button_set(&s_btn_menu, 262, 2, 54, 18, "Menú", C2D_Color32(120, 30, 35, 255), KEY_START);
}

static void start(void)
{
	sd_generate(&s_game, (SdLevel)s_level, (uint32_t)time(NULL) ^ 0x5D0);
	s_mode = MODE_PLAY;
	s_sel = 40;
	s_time = s_end_time = 0.0f;
	s_recorded = s_notes = false;
}

static void place(int v)
{
	Sudoku* g = &s_game;
	if (g->given[s_sel]) {
		audio_play(SFX_ERROR);
		return;
	}
	if (s_notes && v) {
		sd_toggle_note(g, s_sel, v);
		audio_play(SFX_SELECT);
		return;
	}
	int before = g->mistakes;
	sd_set(g, s_sel, g->cell[s_sel] == v ? 0 : v);
	audio_play(g->mistakes > before ? SFX_ERROR : SFX_CARD);
}

static SceneId setup_update(const Input* in)
{
	for (int i = 0; i < SD_LEVELS; i++)
		button_set(&s_btn_level[i], 40, 50 + i * 40, 240, 32, LEVEL_NAME[i],
			i == s_level ? C2D_Color32(220, 150, 30, 255) : C2D_Color32(60, 90, 160, 255), 0);
	button_set(&s_btn_play, 60, 200, 200, 30, GLYPH_A " Jugar", C2D_Color32(40, 140, 60, 255), KEY_A);
	if (in->down & KEY_UP)
		s_level = (s_level + SD_LEVELS - 1) % SD_LEVELS;
	if (in->down & KEY_DOWN)
		s_level = (s_level + 1) % SD_LEVELS;
	for (int i = 0; i < SD_LEVELS; i++)
		if (button_update(&s_btn_level[i], in))
			s_level = i;
	if (button_update(&s_btn_play, in))
		start();
	return SCENE_SUDOKU;
}

static SceneId sd_update(const Input* in, float dt)
{
	if (button_update(&s_btn_menu, in))
		return SCENE_MENU;
	if (s_mode == MODE_SETUP)
		return setup_update(in);
	Sudoku* g = &s_game;
	if (sd_solved(g)) {
		s_end_time += dt;
		if (!s_recorded) {
			s_recorded = true;
			profile_record_game(GAME_SUDOKU, true);
			audio_play(SFX_WIN);
		}
		if (s_end_time > 1.0f && ((in->down & KEY_A) || in->touch_down))
			s_mode = MODE_SETUP;
		return SCENE_SUDOKU;
	}
	s_time += dt;
	/* Teclado de números a la derecha */
	static char labels[9][2];
	for (int v = 1; v <= 9; v++) {
		labels[v - 1][0] = (char)('0' + v);
		button_set(&s_btn_num[v - 1], 210 + (v - 1) % 3 * 36, 26 + (v - 1) / 3 * 36, 34, 34, labels[v - 1],
			v == s_num ? C2D_Color32(220, 150, 30, 255) : C2D_Color32(60, 90, 160, 255), 0);
	}
	button_set(&s_btn_erase, 210, 138, 106, 24, GLYPH_B " Borrar", C2D_Color32(120, 120, 120, 255), KEY_B);
	button_set(&s_btn_notes, 210, 166, 106, 24, s_notes ? GLYPH_X " Notas: sí" : GLYPH_X " Notas: no",
		s_notes ? C2D_Color32(200, 140, 30, 255) : C2D_Color32(40, 110, 120, 255), KEY_X);
	button_set(&s_btn_hint, 210, 194, 106, 24, GLYPH_Y " Pista", C2D_Color32(40, 140, 60, 255), KEY_Y);
	for (int v = 1; v <= 9; v++)
		if (button_update(&s_btn_num[v - 1], in)) {
			s_num = v;
			place(v);
		}
	if (button_update(&s_btn_erase, in) && !g->given[s_sel]) {
		sd_set(g, s_sel, 0);
		g->notes[s_sel] = 0;
	}
	if (button_update(&s_btn_notes, in))
		s_notes = !s_notes;
	if (button_update(&s_btn_hint, in)) {
		int i = sd_hint(g, s_sel);
		if (i >= 0) {
			s_sel = i;
			s_time += 30.0f; /* las pistas cuestan medio minuto */
			audio_play(SFX_CONFIRM);
		}
	}
	/* Cruceta: moverse; L/R: número; Ⓐ: ponerlo */
	int r = s_sel / 9, c = s_sel % 9;
	if (in->repeat & KEY_LEFT)
		c = (c + 8) % 9;
	if (in->repeat & KEY_RIGHT)
		c = (c + 1) % 9;
	if (in->repeat & KEY_UP)
		r = (r + 8) % 9;
	if (in->repeat & KEY_DOWN)
		r = (r + 1) % 9;
	s_sel = r * 9 + c;
	if (in->repeat & KEY_R)
		s_num = s_num % 9 + 1;
	if (in->repeat & KEY_L)
		s_num = (s_num + 7) % 9 + 1;
	if (in->down & KEY_A)
		place(s_num);
	if (in->touch_down) {
		int tc = (int)floorf((in->tx - B_X) / B_CELL), tr = (int)floorf((in->ty - B_Y) / B_CELL);
		if (tc >= 0 && tc < 9 && tr >= 0 && tr < 9) {
			s_sel = tr * 9 + tc;
			audio_play(SFX_SELECT);
		}
	}
	return SCENE_SUDOKU;
}

/* Cuadrícula en (x0, y0) con casillas de lado cell */
static void draw_grid(float x0, float y0, float cell, bool small)
{
	const Sudoku* g = &s_game;
	float side = cell * 9;
	gfx_rect(x0 - 2, y0 - 2, side + 4, side + 4, 0, C2D_Color32(40, 40, 60, 255));
	int sel_v = g->cell[s_sel];
	for (int i = 0; i < 81; i++) {
		int r = i / 9, c = i % 9;
		float x = x0 + c * cell, y = y0 + r * cell;
		bool same_house = r == s_sel / 9 || c == s_sel % 9 || (r / 3 == s_sel / 27 && c / 3 == s_sel % 9 / 3);
		u32 bg = C2D_Color32(252, 250, 244, 255);
		if (s_mode == MODE_PLAY) {
			if (i == s_sel)
				bg = C2D_Color32(255, 214, 120, 255);
			else if (sel_v && g->cell[i] == sel_v)
				bg = C2D_Color32(250, 232, 170, 255);
			else if (same_house)
				bg = C2D_Color32(228, 236, 250, 255);
		}
		gfx_rect(x + 0.5f, y + 0.5f, cell - 1, cell - 1, 0, bg);
		if (g->cell[i]) {
			u32 ink = g->given[i] ? C2D_Color32(30, 30, 45, 255) : C2D_Color32(40, 90, 200, 255);
			if (!g->given[i] && sd_conflict(g, i))
				ink = C2D_Color32(210, 40, 40, 255);
			float sc = cell / 36.0f;
			gfx_textf_plain(x + cell / 2, y + cell / 2 - 15.0f * sc, 0, sc, ink, C2D_AlignCenter, "%d", g->cell[i]);
		} else if (g->notes[i] && !small) {
			for (int v = 1; v <= 9; v++)
				if (g->notes[i] & (1 << v))
					gfx_ellipse(x + 5 + (v - 1) % 3 * (cell - 10) / 2, y + 5 + (v - 1) / 3 * (cell - 10) / 2, 1.8f, 1.8f, 0,
						C2D_Color32(90, 110, 160, 255));
		} else if (g->notes[i]) {
			for (int v = 1; v <= 9; v++)
				if (g->notes[i] & (1 << v))
					gfx_rect(x + 3 + (v - 1) % 3 * 6, y + 3 + (v - 1) / 3 * 6, 3, 3, 0, C2D_Color32(90, 110, 160, 255));
		}
	}
	/* Líneas gruesas de las cajas */
	for (int k = 0; k <= 3; k++) {
		gfx_rect(x0 + k * 3 * cell - 1, y0 - 1, 2, side + 2, 0, C2D_Color32(40, 40, 60, 255));
		gfx_rect(x0 - 1, y0 + k * 3 * cell - 1, side + 2, 2, 0, C2D_Color32(40, 40, 60, 255));
	}
}

static void sd_draw_top(void)
{
	gfx_background(BG_TABLE_TOP);
	if (s_mode == MODE_SETUP) {
		gfx_text("Sudoku", 200, 40, DEPTH_UI, 1.0f, COLOR_GOLD, C2D_AlignCenter);
		gfx_text_wrap("Rellena la cuadrícula para que cada fila, cada columna y cada caja de 3x3 tenga los números "
		              "del 1 al 9 sin repetir. Cada sudoku es nuevo y tiene una sola solución.",
			60, 100, DEPTH_UI, 0.5f, COLOR_CREAM, 280);
		return;
	}
	draw_grid(T_X, T_Y, T_CELL, false);
	float px = 260;
	gfx_panel(px, 10, 130, 120, DEPTH_UI, C2D_Color32(20, 30, 25, 220), C2D_Color32(150, 110, 40, 255));
	gfx_text(LEVEL_NAME[s_game.level], px + 65, 16, DEPTH_UI, 0.6f, COLOR_GOLD, C2D_AlignCenter);
	int t = (int)s_time;
	gfx_textf(px + 65, 44, DEPTH_UI, 0.7f, COLOR_CREAM, C2D_AlignCenter, "%d:%02d", t / 60, t % 60);
	gfx_textf(px + 65, 76, DEPTH_UI, 0.45f, s_game.mistakes ? C2D_Color32(255, 160, 150, 255) : COLOR_CREAM,
		C2D_AlignCenter, "Fallos: %d", s_game.mistakes);
	gfx_textf(px + 65, 98, DEPTH_UI, 0.42f, C2D_Color32(200, 230, 200, 255), C2D_AlignCenter, "Número: %d (L/R)", s_num);
	if (sd_solved(&s_game) && banner_draw(s_end_time, 200, 120, 300, 70)) {
		gfx_text("¡Sudoku resuelto!", 200, 98, DEPTH_BANNER, 0.7f, COLOR_GOLD, C2D_AlignCenter);
		gfx_textf(200, 126, DEPTH_BANNER, 0.45f, COLOR_CREAM, C2D_AlignCenter, "En %d:%02d con %d fallos", t / 60, t % 60,
			s_game.mistakes);
	}
}

static void sd_draw_bottom(void)
{
	gfx_background(BG_TABLE_BOTTOM);
	button_draw(&s_btn_menu, false);
	if (s_mode == MODE_SETUP) {
		gfx_text("Elige la dificultad", BOTTOM_W / 2, 20, 0, 0.55f, COLOR_GOLD, C2D_AlignCenter);
		for (int i = 0; i < SD_LEVELS; i++)
			button_draw(&s_btn_level[i], i == s_level);
		button_draw(&s_btn_play, true);
		return;
	}
	gfx_text(s_notes ? "Modo notas: toca números para apuntarlos" : "Toca una casilla y un número", 6, 3, 0, 0.42f,
		COLOR_GOLD, C2D_AlignLeft);
	draw_grid(B_X, B_Y, B_CELL, true);
	if (sd_solved(&s_game))
		return;
	for (int v = 0; v < 9; v++)
		button_draw(&s_btn_num[v], v + 1 == s_num);
	button_draw(&s_btn_erase, false);
	button_draw(&s_btn_notes, s_notes);
	button_draw(&s_btn_hint, false);
}

/* Salir a medias desde la pausa: la próxima vez se empieza de cero */
static void scene_leave(void)
{
	s_started = false;
}

const Scene scene_sudoku = { sd_enter, sd_update, sd_draw_top, sd_draw_bottom, scene_leave };
