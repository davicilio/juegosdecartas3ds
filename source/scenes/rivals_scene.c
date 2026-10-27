/* Tus rivales: qué Mii de la consola (o invitado) ocupa cada puesto. */
#include <math.h>
#include <stdio.h>

#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/gfx.h"
#include "ui/mii.h"
#include "ui/widgets.h"

#define ROW_X 10
#define ROW_Y 8
#define ROW_W 300
#define ROW_H 23
#define ROW_STEP 25

static int s_row;
static Mood s_mood[CPU_COUNT];
static Button s_btn_console, s_btn_back;

static void rivals_enter(void)
{
	s_row = 0;
	button_set(&s_btn_console, 12, 205, 168, 28, GLYPH_X " Miis de la consola", C2D_Color32(60, 90, 160, 255), KEY_X);
	button_set(&s_btn_back, 188, 205, 120, 28, GLYPH_B " Volver", C2D_Color32(40, 140, 60, 255), KEY_B);
	for (int i = 0; i < CPU_COUNT; i++) {
		s_mood[i].blink = i * 0.45f;
		mood_set(&s_mood[i], EXPR_HAPPY, 1.0f + i * 0.2f);
	}
}

static void pick(int row)
{
	audio_play(SFX_CONFIRM);
	if (rivals_pick((CpuId)row)) {
		mood_set(&s_mood[row], EXPR_HAPPY, 2.0f);
		audio_play(SFX_WIN);
	}
}

static SceneId rivals_update(const Input* in, float dt)
{
	for (int i = 0; i < CPU_COUNT; i++)
		mood_update(&s_mood[i], dt);
	if (in->repeat & KEY_DOWN) {
		s_row = (s_row + 1) % CPU_COUNT;
		audio_play(SFX_SELECT);
	}
	if (in->repeat & KEY_UP) {
		s_row = (s_row + CPU_COUNT - 1) % CPU_COUNT;
		audio_play(SFX_SELECT);
	}
	if (in->down & KEY_A)
		pick(s_row);
	for (int r = 0; r < CPU_COUNT; r++)
		if (input_touch_in(in, ROW_X, ROW_Y + r * ROW_STEP, ROW_W, ROW_H)) {
			s_row = r;
			pick(r);
		}
	if (button_update(&s_btn_console, in)) {
		rivals_reset();
		for (int i = 0; i < CPU_COUNT; i++)
			mood_set(&s_mood[i], EXPR_SURPRISED, 1.0f);
	}
	if (button_update(&s_btn_back, in) || (in->down & KEY_START))
		return SCENE_PROFILE;
	return SCENE_RIVALS;
}

static void rivals_draw_top(void)
{
	gfx_background(BG_TITLE_TOP);
	gfx_text("Tus rivales", 200, 6, DEPTH_UI, 0.7f, COLOR_GOLD, C2D_AlignCenter);
	int found = rivals_console_count();
	char status[96];
	if (found > 0)
		snprintf(status, sizeof status, "%d Mii%s en tu consola", found, found == 1 ? "" : "s");
	else if (found == 0)
		snprintf(status, sizeof status, "No hay Miis en tu consola: elige Miis invitados");
	else
		snprintf(status, sizeof status, "Elige a tus rivales con el selector de Miis");
	gfx_text(status, 200, 30, DEPTH_UI, 0.45f, COLOR_CREAM, C2D_AlignCenter);

	/* Los siete Miis de cuerpo entero: los pies en la línea de cada fila */
	static const float X[CPU_COUNT] = { 62, 154, 246, 338, 108, 200, 292 };
	static const float FEET[CPU_COUNT] = { 118, 118, 118, 118, 208, 208, 208 };
	for (int i = 0; i < CPU_COUNT; i++) {
		bool sel = i == s_row;
		float hop = sel ? fabsf(sinf(gfx_time() * 4.0f)) * 4.0f : 0.0f;
		gfx_ellipse(X[i], FEET[i], 20, 4, DEPTH_TABLE,
			sel ? C2D_Color32(255, 220, 90, 160) : C2D_Color32(0, 0, 0, 80));
		mii_draw_body(cpu_avatar((CpuId)i), X[i], FEET[i] - hop, 72, DEPTH_BANNER, mood_expr(&s_mood[i]));
		gfx_text(cpu_name((CpuId)i), X[i], FEET[i] + 4, DEPTH_UI, 0.45f, sel ? COLOR_GOLD : COLOR_WHITE, C2D_AlignCenter);
	}
}

static void rivals_draw_bottom(void)
{
	gfx_background(BG_MENU_BOTTOM);
	for (int r = 0; r < CPU_COUNT; r++) {
		CpuId id = (CpuId)r;
		float y = ROW_Y + r * ROW_STEP;
		bool sel = r == s_row;
		gfx_rrect(ROW_X, y, ROW_W, ROW_H, 6, 0, sel ? C2D_Color32(255, 236, 170, 255) : C2D_Color32(250, 244, 228, 240));
		mii_draw(cpu_avatar(id), ROW_X + 14, y + ROW_H / 2, 8, 0, EXPR_NORMAL, false);
		u32 ink = C2D_Color32(90, 15, 20, 255);
		gfx_text(cpu_name(id), ROW_X + 28, y + 1, 0, 0.45f, ink, C2D_AlignLeft);
		gfx_text(cpu_role(id), ROW_X + 28, y + 12, 0, 0.34f, C2D_Color32(110, 90, 80, 255), C2D_AlignLeft);
		const char* tag = cpu_is_manual(id) ? "Elegido" : cpu_is_fallback(id) ? "Reserva" : "Consola";
		u32 tag_col = cpu_is_manual(id) ? C2D_Color32(40, 140, 60, 255)
		            : cpu_is_fallback(id) ? C2D_Color32(140, 140, 140, 255)
		                                  : C2D_Color32(60, 90, 160, 255);
		gfx_rrect(ROW_X + ROW_W - 66, y + 4, 58, 15, 5, 0, tag_col);
		gfx_text(tag, ROW_X + ROW_W - 37, y + 5, 0, 0.38f, COLOR_WHITE, C2D_AlignCenter);
	}
	gfx_text(GLYPH_A " o toca un puesto para elegir su Mii (también invitados)", BOTTOM_W / 2, 186, 0, 0.36f,
		COLOR_CREAM, C2D_AlignCenter);
	button_draw(&s_btn_console, false);
	button_draw(&s_btn_back, true);
}

const Scene scene_rivals = { rivals_enter, rivals_update, rivals_draw_top, rivals_draw_bottom };
