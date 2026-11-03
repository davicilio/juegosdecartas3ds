#include "ui/setup.h"

#include <math.h>

#include "ui/audio.h"
#include "ui/gfx.h"

#define ROW_Y(i) (54.0f + (i) * 44.0f)

void setup_init(Setup* s, const char* title, const char* text)
{
	s->title = title;
	s->text = text;
	s->n = 0;
	s->cursor = 0;
	button_set(&s->play, 60, 200, 200, 30, GLYPH_A " Jugar", C2D_Color32(40, 140, 60, 255), KEY_A);
}

void setup_add(Setup* s, const char* label, const char* const* names, int count, int* value)
{
	if (s->n >= SETUP_MAX)
		return;
	int i = s->n++;
	s->opt[i] = (SetupOption){ label, names, count, value };
	button_set(&s->prev[i], 40, ROW_Y(i) + 14, 34, 26, "<", C2D_Color32(60, 90, 160, 255), 0);
	button_set(&s->next[i], 246, ROW_Y(i) + 14, 34, 26, ">", C2D_Color32(60, 90, 160, 255), 0);
}

static void step(SetupOption* o, int d)
{
	*o->value = (*o->value + d + o->count) % o->count;
	audio_play(SFX_SELECT);
}

bool setup_update(Setup* s, const Input* in)
{
	if (s->n > 0) {
		if (in->repeat & KEY_UP)
			s->cursor = (s->cursor + s->n - 1) % s->n;
		if (in->repeat & KEY_DOWN)
			s->cursor = (s->cursor + 1) % s->n;
		if (in->repeat & KEY_LEFT)
			step(&s->opt[s->cursor], -1);
		if (in->repeat & KEY_RIGHT)
			step(&s->opt[s->cursor], 1);
	}
	for (int i = 0; i < s->n; i++) {
		if (button_update(&s->prev[i], in)) {
			s->cursor = i;
			step(&s->opt[i], -1);
		} else if (button_update(&s->next[i], in)) {
			s->cursor = i;
			step(&s->opt[i], 1);
		}
	}
	return button_update(&s->play, in);
}

void setup_draw_top(const Setup* s)
{
	gfx_background(BG_TABLE_TOP);
	gfx_panel(40, 30, 320, 180, DEPTH_UI, C2D_Color32(20, 30, 25, 220), C2D_Color32(150, 110, 40, 255));
	gfx_text(s->title, 200, 44, DEPTH_UI, 0.9f, COLOR_GOLD, C2D_AlignCenter);
	if (s->text)
		gfx_text_wrap(s->text, 60, 90, DEPTH_UI, 0.48f, COLOR_CREAM, 280);
}

void setup_draw_bottom(const Setup* s)
{
	gfx_background(BG_TABLE_BOTTOM);
	gfx_text("Antes de empezar", BOTTOM_W / 2, 20, 0, 0.55f, COLOR_GOLD, C2D_AlignCenter);
	float pulse = 0.5f + 0.5f * sinf(gfx_time() * 5.0f);
	for (int i = 0; i < s->n; i++) {
		const SetupOption* o = &s->opt[i];
		float y = ROW_Y(i);
		if (i == s->cursor)
			gfx_rrect(34, y - 2, 252, 46, 8, 0, C2D_Color32(255, 215, 90, (u8)(40 + 50 * pulse)));
		gfx_text(o->label, BOTTOM_W / 2, y, 0, 0.45f, COLOR_CREAM, C2D_AlignCenter);
		button_draw(&s->prev[i], false);
		button_draw(&s->next[i], false);
		gfx_text(o->names[*o->value], BOTTOM_W / 2, y + 17, 0, 0.6f, COLOR_GOLD, C2D_AlignCenter);
	}
	button_draw(&s->play, true);
}
