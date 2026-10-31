/* Buscaminas: el campo en la pantalla táctil y el marcador arriba. */
#include <math.h>
#include <stdio.h>
#include <sys/stat.h>
#include <time.h>

#include "games/buscaminas.h"
#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/gfx.h"
#include "ui/mii.h"
#include "ui/widgets.h"

#define RECORDS_DIR  "sdmc:/3ds/JuegosDeCartas"
#define RECORDS_PATH RECORDS_DIR "/buscaminas.dat"

static const char* const LEVEL_NAME[BM_LEVELS] = { "Fácil", "Medio", "Difícil" };

static Buscaminas s_game;
static bool s_started, s_flag_mode, s_recorded;
static BmLevel s_level = BM_EASY;
static int s_cx, s_cy; /* cursor de la cruceta */
static float s_time, s_end_time;
static int s_best[BM_LEVELS]; /* mejores tiempos en segundos (0 = ninguno) */
static bool s_new_best;
static Mood s_mood;
static Button s_btn_level, s_btn_mode, s_btn_menu;

static void load_records(void)
{
	FILE* f = fopen(RECORDS_PATH, "rb");
	if (!f)
		return;
	if (fread(s_best, sizeof s_best, 1, f) != 1)
		for (int i = 0; i < BM_LEVELS; i++)
			s_best[i] = 0;
	fclose(f);
	for (int i = 0; i < BM_LEVELS; i++)
		if (s_best[i] < 0 || s_best[i] > 99999)
			s_best[i] = 0;
}

static void save_records(void)
{
	mkdir("sdmc:/3ds", 0777);
	mkdir(RECORDS_DIR, 0777);
	FILE* f = fopen(RECORDS_PATH, "wb");
	if (!f)
		return;
	fwrite(s_best, sizeof s_best, 1, f);
	fclose(f);
}

/* Tamaño de casilla y esquina del campo en la pantalla de abajo */
static float cell(void)
{
	return s_game.w > 13 ? 20.0f : 24.0f;
}

static float field_x(void)
{
	return (BOTTOM_W - s_game.w * cell()) / 2.0f;
}

static float field_y(void)
{
	return 24.0f + (216.0f - s_game.h * cell()) / 2.0f;
}

static void new_game(void)
{
	bm_init(&s_game, s_level, (uint32_t)time(NULL) ^ 0xB0B);
	s_time = s_end_time = 0.0f;
	s_recorded = s_new_best = false;
	s_cx = s_game.w / 2;
	s_cy = s_game.h / 2;
	mood_set(&s_mood, EXPR_NORMAL, 0.1f);
}

static void bm_enter(void)
{
	if (!s_started) {
		s_started = true;
		load_records();
		new_game();
	}
	button_set(&s_btn_menu, 262, 2, 54, 20, "Menú", C2D_Color32(120, 30, 35, 255), KEY_START);
}

static void act(int x, int y, bool flag)
{
	Buscaminas* g = &s_game;
	if (flag) {
		if (bm_toggle_flag(g, x, y))
			audio_play(SFX_SELECT);
	} else if (g->open[y][x]) {
		if (bm_chord(g, x, y))
			audio_play(SFX_FLIP);
	} else if (bm_reveal(g, x, y)) {
		audio_play(g->state == BM_LOST ? SFX_LOSE : SFX_FLIP);
		if (g->state == BM_PLAYING && g->near[y][x] == 0)
			mood_set(&s_mood, EXPR_HAPPY, 0.8f);
	}
}

static SceneId bm_update(const Input* in, float dt)
{
	Buscaminas* g = &s_game;
	if (button_update(&s_btn_menu, in))
		return SCENE_MENU;
	mood_update(&s_mood, dt);
	static char level_label[24], mode_label[24];
	snprintf(level_label, sizeof level_label, GLYPH_X " %s", LEVEL_NAME[s_level]);
	snprintf(mode_label, sizeof mode_label, s_flag_mode ? GLYPH_Y " Bandera" : GLYPH_Y " Destapar");
	button_set(&s_btn_level, 4, 2, 100, 20, level_label, C2D_Color32(60, 90, 160, 255), KEY_X);
	button_set(&s_btn_mode, 108, 2, 110, 20, mode_label,
		s_flag_mode ? C2D_Color32(200, 40, 45, 255) : C2D_Color32(40, 140, 60, 255), KEY_Y);
	if (button_update(&s_btn_level, in)) {
		s_level = (BmLevel)((s_level + 1) % BM_LEVELS);
		new_game();
		return SCENE_BUSCAMINAS;
	}
	if (button_update(&s_btn_mode, in))
		s_flag_mode = !s_flag_mode;

	if (g->state != BM_PLAYING) {
		s_end_time += dt;
		if (!s_recorded) {
			s_recorded = true;
			bool won = g->state == BM_WON;
			profile_record_game(GAME_BUSCAMINAS, won);
			mood_set(&s_mood, won ? EXPR_HAPPY : EXPR_SURPRISED, 30.0f);
			if (won) {
				audio_play(SFX_WIN);
				int t = (int)s_time + 1;
				if (s_best[s_level] == 0 || t < s_best[s_level]) {
					s_best[s_level] = t;
					s_new_best = true;
					save_records();
				}
			}
		}
		/* Cualquier toque o Ⓐ empieza otra */
		if (s_end_time > 0.8f && ((in->down & KEY_A) || (in->touch_down && in->ty > 24)))
			new_game();
		return SCENE_BUSCAMINAS;
	}
	if (g->placed)
		s_time += dt;

	if (in->repeat & KEY_LEFT)
		s_cx = (s_cx + g->w - 1) % g->w;
	if (in->repeat & KEY_RIGHT)
		s_cx = (s_cx + 1) % g->w;
	if (in->repeat & KEY_UP)
		s_cy = (s_cy + g->h - 1) % g->h;
	if (in->repeat & KEY_DOWN)
		s_cy = (s_cy + 1) % g->h;
	if (in->down & KEY_A)
		act(s_cx, s_cy, s_flag_mode);
	if (in->down & KEY_B)
		act(s_cx, s_cy, !s_flag_mode);
	if (in->touch_down) {
		float c = cell();
		int x = (int)floorf((in->tx - field_x()) / c), y = (int)floorf((in->ty - field_y()) / c);
		if (x >= 0 && y >= 0 && x < g->w && y < g->h) {
			s_cx = x;
			s_cy = y;
			/* Con L o R pulsado, el toque pone bandera */
			bool flag = s_flag_mode != ((in->held & (KEY_L | KEY_R)) != 0);
			act(x, y, flag);
		}
	}
	return SCENE_BUSCAMINAS;
}

static u32 number_color(int n)
{
	static const u32 C[9] = { 0, 0x1E50D2, 0x1E8C32, 0xD2281E, 0x14287D, 0x82141E, 0x148C8C, 0x202020, 0x707070 };
	return gfx_rgb(C[n], 255);
}

static void draw_mine(float cx, float cy, float r, float depth)
{
	gfx_line(cx - r * 1.3f, cy, cx + r * 1.3f, cy, 1.5f, depth, C2D_Color32(20, 20, 20, 255));
	gfx_line(cx, cy - r * 1.3f, cx, cy + r * 1.3f, 1.5f, depth, C2D_Color32(20, 20, 20, 255));
	gfx_ellipse(cx, cy, r, r, depth, C2D_Color32(20, 20, 20, 255));
	gfx_ellipse(cx - r * 0.35f, cy - r * 0.35f, r * 0.25f, r * 0.25f, depth, C2D_Color32(255, 255, 255, 220));
}

static void draw_flag(float cx, float cy, float s, float depth)
{
	gfx_rect(cx - 1, cy - 7 * s, 2, 13 * s, depth, C2D_Color32(40, 40, 40, 255));
	gfx_triangle(cx + 1, cy - 7 * s, cx + 1, cy - 1 * s, cx - 7 * s, cy - 4 * s, depth, C2D_Color32(220, 30, 30, 255));
	gfx_rect(cx - 5 * s, cy + 5 * s, 10 * s, 2, depth, C2D_Color32(40, 40, 40, 255));
}

static void bm_draw_top(void)
{
	const Buscaminas* g = &s_game;
	gfx_background(BG_TITLE_TOP);
	gfx_panel(40, 14, 320, 212, DEPTH_UI, C2D_Color32(40, 8, 12, 210), C2D_Color32(230, 170, 50, 255));
	gfx_text("Buscaminas", 200, 22, DEPTH_UI, 0.75f, COLOR_GOLD, C2D_AlignCenter);
	gfx_textf(200, 50, DEPTH_UI, 0.5f, COLOR_CREAM, C2D_AlignCenter, "%s · %d x %d · %d minas", LEVEL_NAME[s_level], g->w,
		g->h, g->mines);
	mii_draw(&profile_get()->avatar, 200, 112, 26, DEPTH_BANNER, mood_expr(&s_mood), false);
	/* Minas que quedan y tiempo */
	gfx_panel(64, 92, 80, 44, DEPTH_UI, C2D_Color32(10, 10, 10, 230), C2D_Color32(120, 120, 120, 255));
	draw_mine(80, 114, 6, DEPTH_UI);
	gfx_textf(124, 102, DEPTH_UI, 0.7f, COLOR_RED, C2D_AlignCenter, "%d", g->mines - bm_flags(g));
	gfx_panel(256, 92, 80, 44, DEPTH_UI, C2D_Color32(10, 10, 10, 230), C2D_Color32(120, 120, 120, 255));
	gfx_textf(296, 102, DEPTH_UI, 0.7f, COLOR_RED, C2D_AlignCenter, "%d", (int)s_time);
	if (s_best[s_level])
		gfx_textf(200, 156, DEPTH_UI, 0.45f, C2D_Color32(200, 230, 200, 255), C2D_AlignCenter, "Mejor tiempo: %d s",
			s_best[s_level]);
	gfx_text_wrap("Toca para destapar. Con L o R pulsado (o en modo bandera) pones banderas. Toca un número con "
	              "sus banderas puestas para destapar el resto.",
		56, 176, DEPTH_UI, 0.4f, C2D_Color32(230, 215, 190, 255), 290);
	if (g->state != BM_PLAYING && banner_draw(s_end_time, 200, 120, 300, 70)) {
		if (g->state == BM_WON) {
			gfx_text("¡Campo limpio!", 200, 98, DEPTH_BANNER, 0.7f, COLOR_GOLD, C2D_AlignCenter);
			gfx_textf(200, 126, DEPTH_BANNER, 0.48f, COLOR_CREAM, C2D_AlignCenter, "%d segundos%s", (int)s_time + 1,
				s_new_best ? " · ¡récord!" : "");
		} else {
			gfx_text("¡Bum!", 200, 98, DEPTH_BANNER, 0.8f, COLOR_RED, C2D_AlignCenter);
			gfx_text("Toca para jugar otra vez", 200, 128, DEPTH_BANNER, 0.45f, COLOR_CREAM, C2D_AlignCenter);
		}
	}
}

static void bm_draw_bottom(void)
{
	const Buscaminas* g = &s_game;
	gfx_rect(0, 0, BOTTOM_W, SCREEN_H, 0, C2D_Color32(60, 70, 80, 255));
	button_draw(&s_btn_level, false);
	button_draw(&s_btn_mode, s_flag_mode);
	button_draw(&s_btn_menu, false);
	float c = cell(), x0 = field_x(), y0 = field_y();
	for (int y = 0; y < g->h; y++)
		for (int x = 0; x < g->w; x++) {
			float px = x0 + x * c, py = y0 + y * c;
			bool lost = g->state == BM_LOST;
			if (g->open[y][x] || (lost && g->mine[y][x] && !g->flag[y][x])) {
				bool boom = x == g->boom_x && y == g->boom_y;
				gfx_rect(px, py, c, c, 0, boom ? C2D_Color32(230, 60, 50, 255) : C2D_Color32(205, 205, 200, 255));
				gfx_rect(px, py, c, 1, 0, C2D_Color32(150, 150, 150, 255));
				gfx_rect(px, py, 1, c, 0, C2D_Color32(150, 150, 150, 255));
				if (g->mine[y][x])
					draw_mine(px + c / 2, py + c / 2, c * 0.25f, 0);
				else if (g->near[y][x])
					gfx_textf_plain(px + c / 2, py + c / 2 - 15.0f * (c / 40.0f), 0, c / 40.0f, number_color(g->near[y][x]),
						C2D_AlignCenter, "%d", g->near[y][x]);
			} else {
				/* Casilla tapada con relieve */
				gfx_rect(px, py, c, c, 0, C2D_Color32(120, 125, 135, 255));
				gfx_rect(px, py, c - 2, c - 2, 0, C2D_Color32(245, 245, 250, 255));
				gfx_rect(px + 2, py + 2, c - 4, c - 4, 0, C2D_Color32(185, 190, 200, 255));
				if (g->flag[y][x]) {
					draw_flag(px + c / 2, py + c / 2, c / 24.0f, 0);
					if (lost && !g->mine[y][x])
						gfx_line(px + 3, py + 3, px + c - 3, py + c - 3, 2, 0, C2D_Color32(20, 20, 20, 255));
				}
			}
		}
	if (g->state == BM_PLAYING) {
		float px = x0 + s_cx * c, py = y0 + s_cy * c;
		float pulse = 0.5f + 0.5f * sinf(gfx_time() * 6.0f);
		u32 k = C2D_Color32(255, 200, 40, (u8)(150 + 100 * pulse));
		gfx_rect(px, py, c, 2, 0, k);
		gfx_rect(px, py + c - 2, c, 2, 0, k);
		gfx_rect(px, py, 2, c, 0, k);
		gfx_rect(px + c - 2, py, 2, c, 0, k);
	}
}

/* Salir a medias desde la pausa: la próxima vez se empieza de cero */
static void scene_leave(void)
{
	s_started = false;
}

const Scene scene_buscaminas = { bm_enter, bm_update, bm_draw_top, bm_draw_bottom, scene_leave };
