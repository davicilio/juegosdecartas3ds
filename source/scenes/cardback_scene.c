/* Editor del dorso de las cartas: color de fondo, dibujo, marco, emblema y un
 * texto propio. El diseño se guarda en la SD y se usa en todos los juegos. */
#include <math.h>
#include <stdio.h>
#include <sys/stat.h>

#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/gfx.h"
#include "ui/widgets.h"

#define CARDBACK_DIR  "sdmc:/3ds/JuegosDeCartas"
#define CARDBACK_PATH CARDBACK_DIR "/dorso.dat"

/* Filas del editor: las opciones y, entre ellas, la del texto */
enum { ROW_TEXT = CB_OPTIONS };
static const int ROWS[] = { CB_BACKGROUND, CB_PATTERN, CB_PATTERN_COLOR, CB_FRAME, CB_EMBLEM,
	ROW_TEXT, CB_TEXT_COLOR, CB_TEXT_POS };
#define NUM_ROWS ((int)(sizeof ROWS / sizeof ROWS[0]))

#define ROW_X     6
#define ROW_Y     26
#define ROW_W     308
#define ROW_H     19
#define ROW_STEP  21
#define ARROW_L_X 150
#define ARROW_R_X 290
#define ARROW_W   24

static CardBack s_edit;
static int s_cursor;
static float s_cursor_y;
static Button s_btn_default, s_btn_done;

void cardback_editor_init(void)
{
	CardBack b;
	if (!cardback_load(&b, CARDBACK_PATH))
		cardback_default(&b);
	gfx_set_card_back(&b);
}

static void apply(void)
{
	gfx_set_card_back(&s_edit);
}

static void cardback_enter(void)
{
	s_edit = *gfx_card_back_design();
	s_cursor = 0;
	s_cursor_y = ROW_Y;
	button_set(&s_btn_default, 10, 204, 140, 30, GLYPH_Y " Por defecto", C2D_Color32(200, 140, 30, 255), KEY_Y);
	button_set(&s_btn_done, 170, 204, 140, 30, GLYPH_B " Guardar", C2D_Color32(40, 140, 60, 255), KEY_B);
}

/* Teclado del sistema para escribir el texto del dorso */
static void edit_text(void)
{
	SwkbdState kbd;
	char buf[CARDBACK_TEXT_MAX];
	swkbdInit(&kbd, SWKBD_TYPE_NORMAL, 2, CARDBACK_TEXT_CHARS);
	swkbdSetHintText(&kbd, "Texto del dorso (por ejemplo, Caja Madrid)");
	swkbdSetInitialText(&kbd, s_edit.text);
	swkbdSetValidation(&kbd, SWKBD_ANYTHING, 0, 0);
	if (swkbdInputText(&kbd, buf, sizeof buf) == SWKBD_BUTTON_CONFIRM) {
		cardback_set_text(&s_edit, buf);
		apply();
	}
}

static void change(int row, int dir)
{
	if (ROWS[row] == ROW_TEXT) {
		edit_text();
		return;
	}
	cardback_step(&s_edit, (CardBackOption)ROWS[row], dir);
	audio_play(SFX_SELECT);
	apply();
}

static SceneId save_and_leave(void)
{
	mkdir("sdmc:/3ds", 0777);
	mkdir(CARDBACK_DIR, 0777);
	cardback_save(&s_edit, CARDBACK_PATH);
	return SCENE_MENU;
}

static SceneId cardback_update(const Input* in, float dt)
{
	if (in->repeat & KEY_DOWN) {
		s_cursor = (s_cursor + 1) % NUM_ROWS;
		audio_play(SFX_SELECT);
	}
	if (in->repeat & KEY_UP) {
		s_cursor = (s_cursor + NUM_ROWS - 1) % NUM_ROWS;
		audio_play(SFX_SELECT);
	}
	s_cursor_y = approach(s_cursor_y, ROW_Y + s_cursor * ROW_STEP, 18.0f, dt);

	if (ROWS[s_cursor] != ROW_TEXT && (in->repeat & (KEY_LEFT | KEY_RIGHT)))
		change(s_cursor, (in->repeat & KEY_LEFT) ? -1 : 1);
	else if (in->down & KEY_A)
		change(s_cursor, 1);

	for (int i = 0; i < NUM_ROWS; i++) {
		float y = ROW_Y + i * ROW_STEP;
		if (!input_touch_in(in, ROW_X, y, ROW_W, ROW_H))
			continue;
		s_cursor = i;
		if (ROWS[i] == ROW_TEXT)
			change(i, 1);
		else if (input_touch_in(in, ARROW_L_X - ARROW_W / 2, y, ARROW_W + 8, ROW_H))
			change(i, -1);
		else if (input_touch_in(in, ARROW_R_X - ARROW_W / 2 - 8, y, ARROW_W + 16, ROW_H))
			change(i, 1);
		else
			audio_play(SFX_SELECT);
	}

	if (button_update(&s_btn_default, in)) {
		cardback_default(&s_edit);
		apply();
	} else if (button_update(&s_btn_done, in) || (in->down & KEY_START)) {
		return save_and_leave();
	}
	return SCENE_CARDBACK;
}

static void cardback_draw_top(void)
{
	float t = gfx_time();
	gfx_background(BG_TITLE_TOP);

	/* Abanico de dorsos a la izquierda y una carta boca arriba a la derecha */
	for (int i = 0; i < 3; i++) {
		float a = (i - 1) * 0.3f;
		float cx = 62 + (i - 1) * 22.0f, cy = 150 + fabsf((float)(i - 1)) * 7.0f;
		gfx_sprite_ex(SPR_CARD_SHADOW, cx + 4, cy + 5, 0.9f, 0.9f, a, DEPTH_CARDS, NULL);
		gfx_card_back(cx, cy, 0.9f, 0.9f, a, DEPTH_CARDS + i * 0.3f, NULL);
	}
	Card ace = { SUIT_OROS, 1 };
	gfx_card(ace, 340, 150, 0.9f, 1.0f, 0.12f, DEPTH_CARDS, 0.0f, true);

	/* El dorso grande en el centro, balanceándose un poco */
	float a = sinf(t * 1.1f) * 0.04f;
	gfx_sprite_ex(SPR_CARD_SHADOW, 206, 126, 2.0f, 2.0f, a, DEPTH_CARDS, NULL);
	gfx_card_back(200, 120, 2.0f, 2.0f, a, DEPTH_BANNER, NULL);
}

static void draw_swatch(float x, float y, uint32_t rgb)
{
	gfx_rrect(x - 1, y - 1, 14, 14, 3, 0, C2D_Color32(60, 40, 30, 255));
	gfx_rrect(x, y, 12, 12, 3, 0, gfx_rgb(rgb, 255));
}

static void cardback_draw_bottom(void)
{
	gfx_background(BG_MENU_BOTTOM);
	gfx_text("Dorso de las cartas", BOTTOM_W / 2, 3, 0, 0.6f, COLOR_GOLD, C2D_AlignCenter);

	float pulse = 0.5f + 0.5f * sinf(gfx_time() * 5.0f);
	gfx_rrect(ROW_X - 3, s_cursor_y - 2, ROW_W + 6, ROW_H + 4, 8, 0, C2D_Color32(255, 215, 90, (u8)(150 + 100 * pulse)));

	const u32 ink = C2D_Color32(90, 15, 20, 255);
	for (int i = 0; i < NUM_ROWS; i++) {
		float y = ROW_Y + i * ROW_STEP;
		gfx_rrect(ROW_X, y, ROW_W, ROW_H, 6, 0, C2D_Color32(250, 244, 228, 255));
		if (ROWS[i] == ROW_TEXT) {
			gfx_text("Texto", ROW_X + 8, y + 2, 0, 0.5f, ink, C2D_AlignLeft);
			bool empty = s_edit.text[0] == '\0';
			gfx_text(empty ? "(sin texto)" : s_edit.text, (ARROW_L_X + ARROW_R_X) / 2, y + 2, 0, 0.5f,
				empty ? C2D_Color32(140, 120, 110, 255) : ink, C2D_AlignCenter);
			gfx_text(GLYPH_A, ARROW_R_X, y + 1, 0, 0.55f, ink, C2D_AlignCenter);
			continue;
		}
		CardBackOption o = (CardBackOption)ROWS[i];
		gfx_text(cardback_option_label(o), ROW_X + 8, y + 2, 0, 0.5f, ink, C2D_AlignLeft);
		const u32 arrow = C2D_Color32(170, 60, 50, 255);
		gfx_triangle(ARROW_L_X + 4, y + 4, ARROW_L_X + 4, y + ROW_H - 4, ARROW_L_X - 5, y + ROW_H / 2.0f, 0, arrow);
		gfx_triangle(ARROW_R_X - 4, y + 4, ARROW_R_X - 4, y + ROW_H - 4, ARROW_R_X + 5, y + ROW_H / 2.0f, 0, arrow);
		const char* name = cardback_value_name(o, s_edit.value[o]);
		float cx = (ARROW_L_X + ARROW_R_X) / 2.0f;
		bool swatch = o == CB_BACKGROUND || o == CB_TEXT_COLOR || (o == CB_PATTERN_COLOR) ||
		              (o == CB_FRAME && s_edit.value[o] > 0);
		if (swatch) {
			uint32_t rgb = o == CB_BACKGROUND     ? cardback_background_rgb(&s_edit)
			               : o == CB_TEXT_COLOR   ? cardback_text_rgb(&s_edit)
			               : o == CB_PATTERN_COLOR ? cardback_pattern_rgb(&s_edit)
			                                       : cardback_frame_rgb(&s_edit);
			float w = gfx_text_width(name, 0.5f);
			draw_swatch(cx - (w + 18) / 2, y + 3, rgb);
			cx += 9;
		}
		gfx_text(name, cx, y + 2, 0, 0.5f, ink, C2D_AlignCenter);
	}

	button_draw(&s_btn_default, false);
	button_draw(&s_btn_done, true);
}

const Scene scene_cardback = { cardback_enter, cardback_update, cardback_draw_top, cardback_draw_bottom };
