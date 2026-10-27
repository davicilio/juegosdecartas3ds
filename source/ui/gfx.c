#include "ui/gfx.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>

#define STEREO_PIXELS 2.2f /* desplazamiento máximo por unidad de profundidad */

static C3D_RenderTarget* s_top_left;
static C3D_RenderTarget* s_top_right;
static C3D_RenderTarget* s_bottom;
static C2D_SpriteSheet s_sprites;
static C2D_SpriteSheet s_backgrounds;
static C2D_SpriteSheet s_chars;
static C2D_TextBuf s_textbuf;

static float s_eye_sign; /* -1 izquierdo, +1 derecho, 0 pantalla inferior */
static float s_slider;
static float s_time;

bool gfx_init(void)
{
	romfsInit();
	gfxInitDefault();
	gfxSet3D(true);
	C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
	C2D_Init(C2D_DEFAULT_MAX_OBJECTS * 2);
	C2D_Prepare();

	s_top_left = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
	s_top_right = C2D_CreateScreenTarget(GFX_TOP, GFX_RIGHT);
	s_bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

	s_sprites = C2D_SpriteSheetLoad("romfs:/gfx/sprites.t3x");
	s_backgrounds = C2D_SpriteSheetLoad("romfs:/gfx/backgrounds.t3x");
	s_chars = C2D_SpriteSheetLoad("romfs:/gfx/chars.t3x");
	s_textbuf = C2D_TextBufNew(8192);
	return s_sprites && s_backgrounds && s_chars && s_textbuf;
}

void gfx_exit(void)
{
	if (s_textbuf)
		C2D_TextBufDelete(s_textbuf);
	if (s_sprites)
		C2D_SpriteSheetFree(s_sprites);
	if (s_backgrounds)
		C2D_SpriteSheetFree(s_backgrounds);
	if (s_chars)
		C2D_SpriteSheetFree(s_chars);
	C2D_Fini();
	C3D_Fini();
	gfxExit();
	romfsExit();
}

void gfx_frame_begin(void)
{
	s_time += 1.0f / 60.0f;
	s_slider = osGet3DSliderState();
	C2D_TextBufClear(s_textbuf);
	C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
}

void gfx_frame_end(void)
{
	C3D_FrameEnd(0);
}

bool gfx_begin_top(int eye)
{
	if (eye == 1 && s_slider <= 0.0f)
		return false;
	C3D_RenderTarget* target = eye == 0 ? s_top_left : s_top_right;
	C2D_TargetClear(target, COLOR_BLACK);
	C2D_SceneBegin(target);
	s_eye_sign = eye == 0 ? -1.0f : 1.0f;
	return true;
}

void gfx_begin_bottom(void)
{
	C2D_TargetClear(s_bottom, COLOR_BLACK);
	C2D_SceneBegin(s_bottom);
	s_eye_sign = 0.0f;
}

float gfx_dx(float depth)
{
	/* Los objetos cercanos se desplazan a la derecha en el ojo izquierdo y a
	 * la izquierda en el derecho (disparidad cruzada): parecen salir. */
	return -s_eye_sign * depth * s_slider * STEREO_PIXELS;
}

float gfx_time(void)
{
	return s_time;
}

void gfx_background(int bg)
{
	C2D_DrawImageAt(C2D_SpriteSheetGetImage(s_backgrounds, bg), 0.0f, 0.0f, 0.0f, NULL, 1.0f, 1.0f);
}

C2D_Image gfx_sprite_image(int id)
{
	return C2D_SpriteSheetGetImage(s_sprites, id);
}

C2D_Image gfx_char_image(int id)
{
	return C2D_SpriteSheetGetImage(s_chars, id);
}

void gfx_sprite_ex(int id, float cx, float cy, float sx, float sy, float angle, float depth, const C2D_ImageTint* tint)
{
	C2D_DrawImageAtRotated(gfx_sprite_image(id), cx + gfx_dx(depth), cy, 0.5f, angle, tint, sx, sy);
}

void gfx_sprite(int id, float cx, float cy, float scale, float depth)
{
	gfx_sprite_ex(id, cx, cy, scale, scale, 0.0f, depth, NULL);
}

void gfx_card(Card c, float cx, float cy, float scale, float flip, float angle, float depth, float dim, bool shadow)
{
	float width = fabsf(cosf(flip * (float)M_PI));
	bool face_up = flip > 0.5f;
	int id = face_up ? SPR_CARD_FIRST + card_index(c) : SPR_CARD_BACK;

	if (shadow) {
		/* La sombra se separa más cuanto más "levantada" está la carta. */
		float off = 2.0f + depth * 1.5f;
		gfx_sprite_ex(SPR_CARD_SHADOW, cx + off, cy + off, scale * width, scale, angle, depth * 0.3f, NULL);
	}

	C2D_ImageTint tint;
	if (dim > 0.0f)
		C2D_PlainImageTint(&tint, C2D_Color32(20, 20, 30, 255), dim);
	gfx_sprite_ex(id, cx, cy, scale * width, scale, angle, depth, dim > 0.0f ? &tint : NULL);
}

/* Por debajo de este tamaño la fuente del sistema se lee mal en la consola */
#define MIN_TEXT_SCALE 0.45f

static float text_scale(float scale)
{
	return scale < MIN_TEXT_SCALE ? MIN_TEXT_SCALE : scale;
}

void gfx_text(const char* str, float x, float y, float depth, float scale, u32 color, u32 align)
{
	if (!str || !*str)
		return;
	C2D_Text text;
	C2D_TextParse(&text, s_textbuf, str);
	C2D_TextOptimize(&text);
	scale = text_scale(scale);
	/* Posiciones en píxeles enteros: el texto sale más nítido */
	x = floorf(x + gfx_dx(depth) + 0.5f);
	y = floorf(y + 0.5f);
	/* Sombra solo hacia abajo y suave en el texto pequeño, para no emborronarlo */
	if (scale > 0.6f)
		C2D_DrawText(&text, C2D_WithColor | align, x + 2, y + 2, 0.5f, scale, scale, COLOR_SHADOW);
	else
		C2D_DrawText(&text, C2D_WithColor | align, x, y + 1, 0.5f, scale, scale, C2D_Color32(0, 0, 0, 110));
	C2D_DrawText(&text, C2D_WithColor | align, x, y, 0.5f, scale, scale, color);
}

void gfx_textf(float x, float y, float depth, float scale, u32 color, u32 align, const char* fmt, ...)
{
	char buf[256];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof buf, fmt, ap);
	va_end(ap);
	gfx_text(buf, x, y, depth, scale, color, align);
}

void gfx_text_wrap(const char* str, float x, float y, float depth, float scale, u32 color, float width)
{
	if (!str || !*str)
		return;
	C2D_Text text;
	C2D_TextParse(&text, s_textbuf, str);
	C2D_TextOptimize(&text);
	scale = text_scale(scale);
	x = floorf(x + gfx_dx(depth) + 0.5f);
	y = floorf(y + 0.5f);
	C2D_DrawText(&text, C2D_WithColor | C2D_WordWrap, x, y + 1, 0.5f, scale, scale, C2D_Color32(0, 0, 0, 110), width);
	C2D_DrawText(&text, C2D_WithColor | C2D_WordWrap, x, y, 0.5f, scale, scale, color, width);
}

float gfx_text_width(const char* str, float scale)
{
	C2D_Text text;
	float w = 0.0f, h = 0.0f;
	if (!str || !*str)
		return 0.0f;
	scale = text_scale(scale);
	C2D_TextParse(&text, s_textbuf, str);
	C2D_TextGetDimensions(&text, scale, scale, &w, &h);
	return w;
}

void gfx_rect(float x, float y, float w, float h, float depth, u32 color)
{
	C2D_DrawRectSolid(x + gfx_dx(depth), y, 0.5f, w, h, color);
}

void gfx_ellipse(float cx, float cy, float rx, float ry, float depth, u32 color)
{
	C2D_DrawEllipseSolid(cx - rx + gfx_dx(depth), cy - ry, 0.5f, rx * 2, ry * 2, color);
}

void gfx_line(float x0, float y0, float x1, float y1, float width, float depth, u32 color)
{
	float dx = gfx_dx(depth);
	C2D_DrawLine(x0 + dx, y0, color, x1 + dx, y1, color, width, 0.5f);
}

void gfx_triangle(float x0, float y0, float x1, float y1, float x2, float y2, float depth, u32 color)
{
	float dx = gfx_dx(depth);
	C2D_DrawTriangle(x0 + dx, y0, color, x1 + dx, y1, color, x2 + dx, y2, color, 0.5f);
}

u32 gfx_rgb(uint32_t rgb, u8 alpha)
{
	return C2D_Color32((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF, alpha);
}

static void corner(float cx, float cy, float r, float a0, float depth, u32 color)
{
	/* Cuarto de círculo como abanico de triángulos (sin solaparse con los
	 * rectángulos, así los colores translúcidos quedan uniformes). */
	const int steps = 5;
	float step = (float)M_PI / 2.0f / steps;
	for (int i = 0; i < steps; i++) {
		float t0 = a0 + i * step, t1 = t0 + step;
		C2D_DrawTriangle(cx, cy, color,
			cx + cosf(t0) * r, cy + sinf(t0) * r, color,
			cx + cosf(t1) * r, cy + sinf(t1) * r, color, 0.5f);
	}
}

void gfx_rrect(float x, float y, float w, float h, float r, float depth, u32 color)
{
	x += gfx_dx(depth);
	if (r * 2 > w)
		r = w / 2;
	if (r * 2 > h)
		r = h / 2;
	C2D_DrawRectSolid(x + r, y, 0.5f, w - 2 * r, h, color);
	C2D_DrawRectSolid(x, y + r, 0.5f, r, h - 2 * r, color);
	C2D_DrawRectSolid(x + w - r, y + r, 0.5f, r, h - 2 * r, color);
	corner(x + r, y + r, r, (float)M_PI, 0, color);
	corner(x + w - r, y + r, r, (float)M_PI * 1.5f, 0, color);
	corner(x + w - r, y + h - r, r, 0.0f, 0, color);
	corner(x + r, y + h - r, r, (float)M_PI * 0.5f, 0, color);
}

void gfx_panel(float x, float y, float w, float h, float depth, u32 fill, u32 border)
{
	gfx_rrect(x, y, w, h, 7, depth, border);
	gfx_rrect(x + 2, y + 2, w - 4, h - 4, 5, depth, fill);
	gfx_rrect(x + 3, y + 3, w - 6, (h - 6) * 0.45f, 4, depth, C2D_Color32(255, 255, 255, 28));
}

void gfx_fade(float alpha)
{
	if (alpha <= 0.0f)
		return;
	if (alpha > 1.0f)
		alpha = 1.0f;
	C2D_DrawRectSolid(0, 0, 0.5f, TOP_W, SCREEN_H, C2D_Color32(0, 0, 0, (u8)(alpha * 255)));
}

void gfx_chip_stack(int amount, float cx, float cy, float depth)
{
	/* Fichas de 25 (negras), 10 (verdes), 5 (azules) y 1 (rojas), en columnas. */
	static const int values[4] = { 25, 10, 5, 1 };
	static const int sprites[4] = { SPR_CHIP_BLACK, SPR_CHIP_GREEN, SPR_CHIP_BLUE, SPR_CHIP_RED };
	int columns[4] = { 0 };
	int ncols = 0;
	for (int i = 0; i < 4 && amount > 0; i++) {
		int n = amount / values[i];
		amount -= n * values[i];
		if (n > 6)
			n = 6;
		if (n > 0)
			columns[ncols++] = (n << 8) | sprites[i];
	}
	float x0 = cx - (ncols - 1) * 9.0f;
	for (int c = 0; c < ncols; c++) {
		int n = columns[c] >> 8;
		int spr = columns[c] & 0xFF;
		for (int k = 0; k < n; k++)
			gfx_sprite_ex(spr, x0 + c * 18.0f, cy - k * 3.0f, 0.75f, 0.75f, 0.0f, depth + k * 0.1f, NULL);
	}
}
