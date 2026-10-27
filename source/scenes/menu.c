/* Menú principal: elección de juego. */
#include <math.h>

#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/gfx.h"
#include "ui/widgets.h"

typedef struct {
	const char* name;
	const char* description;
	SceneId scene; /* SCENE_COUNT si aún no está disponible */
	Suit icon;
	Card fan[3];
} MenuEntry;

#define C(s, r) { SUIT_##s, r }

static const MenuEntry entries[] = {
	{ "Siete y media",
	  "Pide cartas y acércate a 7 y medio sin pasarte. Las figuras valen medio punto. "
	  "Apuesta tus fichas contra la banca: ¡la siete y media real paga doble!",
	  SCENE_SIETEYMEDIA, SUIT_OROS, { C(OROS, 7), C(COPAS, RANK_SOTA), C(ESPADAS, 3) } },
	{ "Cinquillo",
	  "Cuatro jugadores. Se sale con el cinco de oros y se van colocando las escaleras de cada palo. "
	  "El primero que se quede sin cartas se lleva el bote.",
	  SCENE_CINQUILLO, SUIT_COPAS, { C(BASTOS, 5), C(OROS, 5), C(COPAS, 5) } },
	{ "Brisca",
	  "Juego de bazas contra Carmen. La muestra marca el triunfo; ases y treses valen más que el rey. "
	  "Suma más de 60 de los 120 puntos para ganar.",
	  SCENE_BRISCA, SUIT_ESPADAS, { C(OROS, 1), C(COPAS, 3), C(ESPADAS, RANK_REY) } },
	{ "Escoba",
	  "Suma 15 con tu carta y las de la mesa para llevártelas. Si limpias la mesa, ¡escoba! "
	  "Cuentan cartas, oros, sietes y el siete de oros. Gana quien llegue a 21.",
	  SCENE_ESCOBA, SUIT_BASTOS, { C(OROS, 7), C(BASTOS, 5), C(COPAS, 3) } },
	{ "Mus",
	  "Por parejas: tú y Lola contra Paco y Manolo. Mus, descartes y los cuatro lances: grande, chica, "
	  "pares y juego. Envida, quiere... ¡o lánzate al órdago! A 40 piedras.",
	  SCENE_MUS, SUIT_BASTOS, { C(OROS, RANK_REY), C(BASTOS, RANK_REY), C(COPAS, 3) } },
	{ "Chinchón",
	  "Forma tríos y escaleras del mismo palo. Cierra con todo ligado (menos diez) o con una suelta "
	  "pequeña. Siete seguidas del mismo palo: ¡chinchón! Pierde quien pase de 100.",
	  SCENE_CHINCHON, SUIT_OROS, { C(ESPADAS, 4), C(ESPADAS, 5), C(ESPADAS, 6) } },
};

#define NUM_ENTRIES ((int)(sizeof entries / sizeof entries[0]))

#define ITEM_X 28
#define ITEM_Y 14
#define ITEM_W 264
#define ITEM_H 28
#define ITEM_STEP 33

static int s_selected;
static float s_cursor_y;
static float s_shake; /* sacudida al elegir un juego no disponible */

static void menu_enter(void)
{
	s_cursor_y = ITEM_Y + s_selected * ITEM_STEP;
}

static SceneId launch(int i)
{
	if (entries[i].scene == SCENE_COUNT) {
		s_shake = 0.3f;
		audio_play(SFX_ERROR);
		return SCENE_MENU;
	}
	audio_play(SFX_CONFIRM);
	return entries[i].scene;
}

static SceneId menu_update(const Input* in, float dt)
{
	if (s_shake > 0.0f)
		s_shake -= dt;
	if (in->repeat & KEY_DOWN) {
		s_selected = (s_selected + 1) % NUM_ENTRIES;
		audio_play(SFX_SELECT);
	}
	if (in->repeat & KEY_UP) {
		s_selected = (s_selected + NUM_ENTRIES - 1) % NUM_ENTRIES;
		audio_play(SFX_SELECT);
	}
	s_cursor_y = approach(s_cursor_y, ITEM_Y + s_selected * ITEM_STEP, 18.0f, dt);

	if (in->down & KEY_A)
		return launch(s_selected);
	if (in->down & KEY_START)
		return SCENE_QUIT;

	for (int i = 0; i < NUM_ENTRIES; i++) {
		if (input_touch_in(in, ITEM_X, ITEM_Y + i * ITEM_STEP, ITEM_W, ITEM_H)) {
			s_selected = i;
			return launch(i);
		}
	}
	return SCENE_MENU;
}

static void menu_draw_top(void)
{
	float t = gfx_time();
	const MenuEntry* e = &entries[s_selected];

	gfx_background(BG_TITLE_TOP);

	/* Abanico de cartas del juego seleccionado */
	for (int i = 0; i < 3; i++) {
		float a = (i - 1) * 0.32f + sinf(t * 1.3f + i) * 0.04f;
		float cx = 78 + (i - 1) * 26.0f;
		float cy = 160 + fabsf((float)(i - 1)) * 8.0f + sinf(t * 2.0f + i * 0.7f) * 2.0f;
		gfx_sprite_ex(SPR_CARD_SHADOW, cx + 4, cy + 5, 1.0f, 1.0f, a, DEPTH_CARDS, NULL);
		gfx_sprite_ex(SPR_CARD_FIRST + card_index(e->fan[i]), cx, cy, 1.0f, 1.0f, a, DEPTH_CARDS + i * 0.4f, NULL);
	}

	/* Logotipo flotando por delante de la pantalla */
	gfx_sprite(SPR_LOGO, 200, 50 + sinf(t * 2.0f) * 3.0f, 1.0f, DEPTH_BANNER);

	/* Descripción del juego seleccionado */
	gfx_panel(156, 108, 232, 106, DEPTH_UI, C2D_Color32(40, 8, 12, 200), C2D_Color32(230, 170, 50, 255));
	gfx_sprite(SPR_SUIT_FIRST + e->icon, 176, 126, 0.8f, DEPTH_UI);
	gfx_text(e->name, 192, 115, DEPTH_UI, 0.6f, COLOR_GOLD, C2D_AlignLeft);
	gfx_text_wrap(e->description, 166, 140, DEPTH_UI, 0.42f, COLOR_CREAM, 214);
}

static void menu_draw_bottom(void)
{
	gfx_background(BG_MENU_BOTTOM);

	float shake = s_shake > 0.0f ? sinf(s_shake * 60.0f) * 4.0f : 0.0f;

	/* Cursor */
	float pulse = 0.5f + 0.5f * sinf(gfx_time() * 5.0f);
	gfx_rrect(ITEM_X - 4 + shake, s_cursor_y - 4, ITEM_W + 8, ITEM_H + 8, 12, 0,
		C2D_Color32(255, 215, 90, (u8)(150 + 100 * pulse)));

	for (int i = 0; i < NUM_ENTRIES; i++) {
		const MenuEntry* e = &entries[i];
		bool available = e->scene != SCENE_COUNT;
		float x = ITEM_X + (i == s_selected ? shake : 0.0f);
		float y = ITEM_Y + i * ITEM_STEP;
		u32 fill = available ? C2D_Color32(250, 244, 228, 255) : C2D_Color32(190, 180, 170, 255);
		gfx_panel(x, y, ITEM_W, ITEM_H, 0, fill, C2D_Color32(110, 20, 25, 255));
		gfx_sprite(SPR_SUIT_FIRST + e->icon, x + 20, y + ITEM_H / 2, 0.85f, 0);
		gfx_text(e->name, x + 38, y + 5, 0, 0.6f,
			available ? C2D_Color32(90, 15, 20, 255) : C2D_Color32(110, 100, 95, 255), C2D_AlignLeft);
		if (!available) {
			gfx_rrect(x + ITEM_W - 96, y + 6, 88, 16, 6, 0, C2D_Color32(120, 120, 120, 255));
			gfx_text("Próximamente", x + ITEM_W - 52, y + 7, 0, 0.4f, COLOR_WHITE, C2D_AlignCenter);
		} else {
			gfx_text(GLYPH_A, x + ITEM_W - 20, y + 5, 0, 0.6f, C2D_Color32(90, 15, 20, 255), C2D_AlignCenter);
		}
	}
	gfx_text(GLYPH_DPAD " Elegir   " GLYPH_A " Jugar   START Salir", BOTTOM_W / 2, 216, 0, 0.45f, COLOR_CREAM,
		C2D_AlignCenter);
}

const Scene scene_menu = { menu_enter, menu_update, menu_draw_top, menu_draw_bottom };
