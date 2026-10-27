/* Menú principal: elección de juego. */
#include <math.h>

#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/gfx.h"
#include "ui/mii.h"
#include "ui/widgets.h"

typedef struct {
	const char* name;
	const char* description;
	SceneId scene; /* SCENE_COUNT si aún no está disponible */
	Suit icon;
	Card fan[3];
	int rivals[3]; /* Miis contra (o con) los que se juega; -1 = nadie */
} MenuEntry;

#define C(s, r) { SUIT_##s, r }

static const MenuEntry entries[] = {
	{ "Siete y media",
	  "Pide cartas y acércate a 7 y medio sin pasarte. Las figuras valen medio punto. "
	  "Apuesta tus fichas contra la banca: ¡la siete y media real paga doble!",
	  SCENE_SIETEYMEDIA, SUIT_OROS, { C(OROS, 7), C(COPAS, RANK_SOTA), C(ESPADAS, 3) }, { CPU_RAMON, -1, -1 } },
	{ "Cinquillo",
	  "Cuatro jugadores. Se sale con el cinco de oros y se van colocando las escaleras de cada palo. "
	  "El primero que se quede sin cartas se lleva el bote.",
	  SCENE_CINQUILLO, SUIT_COPAS, { C(BASTOS, 5), C(OROS, 5), C(COPAS, 5) }, { CPU_PACO, CPU_LOLA, CPU_MANOLO } },
	{ "Brisca",
	  "Juego de bazas contra un Mii. La muestra marca el triunfo; ases y treses valen más que el rey. "
	  "Suma más de 60 de los 120 puntos para ganar.",
	  SCENE_BRISCA, SUIT_ESPADAS, { C(OROS, 1), C(COPAS, 3), C(ESPADAS, RANK_REY) }, { CPU_CARMEN, -1, -1 } },
	{ "Escoba",
	  "Suma 15 con tu carta y las de la mesa para llevártelas. Si limpias la mesa, ¡escoba! "
	  "Cuentan cartas, oros, sietes y el siete de oros. Gana quien llegue a 21.",
	  SCENE_ESCOBA, SUIT_BASTOS, { C(OROS, 7), C(BASTOS, 5), C(COPAS, 3) }, { CPU_PEPE, -1, -1 } },
	{ "Mus",
	  "Por parejas, con tus Miis. Mus, descartes, grande, chica, pares y juego. "
	  "Hazle señas a tu compañero con SELECT... ¡y vigila las de los rivales! A 40 piedras.",
	  SCENE_MUS, SUIT_BASTOS, { C(OROS, RANK_REY), C(BASTOS, RANK_REY), C(COPAS, 3) }, { CPU_LOLA, CPU_PACO, CPU_MANOLO } },
	{ "Chinchón",
	  "Forma tríos y escaleras del mismo palo. Cierra con todo ligado (menos diez) o con una suelta "
	  "pequeña. Siete seguidas del mismo palo: ¡chinchón! Pierde quien pase de 100.",
	  SCENE_CHINCHON, SUIT_OROS, { C(ESPADAS, 4), C(ESPADAS, 5), C(ESPADAS, 6) }, { CPU_LUCIA, -1, -1 } },
	{ "Tute",
	  "Juego de bazas con triunfo. Canta las veinte y las cuarenta con el rey y el caballo del mismo palo, "
	  "¡o el tute con los cuatro reyes o los cuatro caballos!",
	  SCENE_COUNT, SUIT_COPAS, { C(COPAS, RANK_CABALLO), C(COPAS, RANK_REY), C(OROS, 1) }, { -1, -1, -1 } },
	{ "Culo",
	  "Quítate las cartas de encima antes que nadie. El primero que acaba manda en la siguiente mano "
	  "y el último... es el culo.",
	  SCENE_COUNT, SUIT_BASTOS, { C(BASTOS, 2), C(BASTOS, 2), C(OROS, 2) }, { -1, -1, -1 } },
	{ "Mentiroso",
	  "Echa cartas boca abajo diciendo lo que son... aunque sea mentira. Si te pillan, te llevas el montón.",
	  SCENE_COUNT, SUIT_ESPADAS, { C(ESPADAS, 4), C(OROS, 4), C(COPAS, 4) }, { -1, -1, -1 } },
	{ "Mierda",
	  "Otro clásico de la baraja española que llegará en una próxima versión.",
	  SCENE_COUNT, SUIT_BASTOS, { C(BASTOS, 7), C(COPAS, 1), C(ESPADAS, 6) }, { -1, -1, -1 } },
	{ "Burro",
	  "Juego de cartas para reírse en grupo. El que pierde va sumando letras hasta formar BURRO.",
	  SCENE_COUNT, SUIT_OROS, { C(OROS, 6), C(COPAS, 6), C(BASTOS, 6) }, { -1, -1, -1 } },
	{ "As, 2, 3",
	  "Otro juego de la baraja española que llegará en una próxima versión.",
	  SCENE_COUNT, SUIT_COPAS, { C(OROS, 1), C(COPAS, 2), C(ESPADAS, 3) }, { -1, -1, -1 } },
	/* Configuración */
	{ "Mi perfil",
	  "Tu Mii, tus estadísticas y tus rivales: los Miis de tu consola (o los Miis invitados) "
	  "que juegan contra ti en cada mesa.",
	  SCENE_PROFILE, SUIT_OROS, { C(OROS, 1), C(OROS, 1), C(OROS, 1) }, { -1, -1, -1 } },
	{ "Dorso de las cartas",
	  "Diseña la parte de atrás de las cartas: color, dibujo, marco, emblema y tu propio texto "
	  "(por ejemplo, el nombre de tu banco o de tu peña). Se usa en todos los juegos.",
	  SCENE_CARDBACK, SUIT_OROS, { C(OROS, 1), C(OROS, 1), C(OROS, 1) }, { -1, -1, -1 } },
};

#define NUM_ENTRIES ((int)(sizeof entries / sizeof entries[0]))
#define NUM_CONFIG 2
#define NUM_GAMES (NUM_ENTRIES - NUM_CONFIG)
#define GAME_ROWS ((NUM_GAMES + 1) / 2)

/* Juegos y próximos juegos: cuadrículas de dos columnas. Configuración: dos
 * botones pequeños debajo. */
#define NUM_READY   6 /* los primeros juegos ya se pueden jugar */
#define COL_X(c)    (8.0f + (c) * 156.0f)
#define HEAD_H      15.0f
#define GAME_Y      (2.0f + HEAD_H)
#define GAME_W      148.0f
#define GAME_H      22.0f
#define GAME_STEP   24.0f
#define SOON_HEAD   (GAME_Y + NUM_READY / 2 * GAME_STEP + 1.0f)
#define SOON_Y      (SOON_HEAD + HEAD_H)
#define CONFIG_HEAD (SOON_Y + (NUM_GAMES - NUM_READY + 1) / 2 * GAME_STEP + 1.0f)
#define CONFIG_Y    (CONFIG_HEAD + 14.0f)
#define CONFIG_H    18.0f

static int s_selected;
static Mood s_mood;
static float s_cursor_x, s_cursor_y;
static float s_shake; /* sacudida al elegir un juego no disponible */

static bool is_config(int i)
{
	return i >= NUM_GAMES;
}

static void item_rect(int i, float* x, float* y, float* w, float* h)
{
	if (is_config(i)) {
		*x = COL_X(i - NUM_GAMES);
		*y = CONFIG_Y;
		*h = CONFIG_H;
	} else {
		*x = COL_X(i % 2);
		*y = i < NUM_READY ? GAME_Y + (i / 2) * GAME_STEP : SOON_Y + ((i - NUM_READY) / 2) * GAME_STEP;
		*h = GAME_H;
	}
	*w = GAME_W;
}

static void menu_enter(void)
{
	float w, h;
	item_rect(s_selected, &s_cursor_x, &s_cursor_y, &w, &h);
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

/* Movimiento con la cruceta por la cuadrícula de juegos y la fila de configuración */
static int move(int i, int dx, int dy)
{
	int col = is_config(i) ? i - NUM_GAMES : i % 2;
	int row = is_config(i) ? GAME_ROWS : i / 2;
	col = (col + dx + 2) % 2;
	row = (row + dy + GAME_ROWS + 1) % (GAME_ROWS + 1);
	if (row == GAME_ROWS)
		return NUM_GAMES + col;
	int next = row * 2 + col;
	return next < NUM_GAMES ? next : NUM_GAMES - 1;
}

static SceneId menu_update(const Input* in, float dt)
{
	if (s_shake > 0.0f)
		s_shake -= dt;
	mood_update(&s_mood, dt);
	int dx = (in->repeat & KEY_RIGHT) ? 1 : (in->repeat & KEY_LEFT) ? -1 : 0;
	int dy = (in->repeat & KEY_DOWN) ? 1 : (in->repeat & KEY_UP) ? -1 : 0;
	if (dx || dy) {
		s_selected = move(s_selected, dx, dy);
		audio_play(SFX_SELECT);
	}
	float x, y, w, h;
	item_rect(s_selected, &x, &y, &w, &h);
	s_cursor_x = approach(s_cursor_x, x, 18.0f, dt);
	s_cursor_y = approach(s_cursor_y, y, 18.0f, dt);

	if (in->down & KEY_A)
		return launch(s_selected);
	if (in->down & KEY_START)
		return SCENE_QUIT;

	for (int i = 0; i < NUM_ENTRIES; i++) {
		item_rect(i, &x, &y, &w, &h);
		if (input_touch_in(in, x, y, w, h)) {
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

	const Avatar* me = &profile_get()->avatar;
	if (e->scene == SCENE_PROFILE) {
		/* En "Mi perfil" se ve tu Mii en lugar de las cartas */
		gfx_ellipse(78, 214, 40, 8, DEPTH_TABLE, C2D_Color32(0, 0, 0, 90));
		mii_draw_body(me, 78, 216, 118, DEPTH_CARDS + 1.0f, mood_expr(&s_mood));
	}
	/* Abanico de cartas del juego seleccionado */
	for (int i = 0; i < 3 && e->scene != SCENE_PROFILE; i++) {
		float a = (i - 1) * 0.32f + sinf(t * 1.3f + i) * 0.04f;
		float cx = 78 + (i - 1) * 26.0f;
		float cy = 160 + fabsf((float)(i - 1)) * 8.0f + sinf(t * 2.0f + i * 0.7f) * 2.0f;
		gfx_sprite_ex(SPR_CARD_SHADOW, cx + 4, cy + 5, 1.0f, 1.0f, a, DEPTH_CARDS, NULL);
		if (e->scene == SCENE_CARDBACK)
			gfx_card_back(cx, cy, 1.0f, 1.0f, a, DEPTH_CARDS + i * 0.4f, NULL);
		else
			gfx_sprite_ex(SPR_CARD_FIRST + card_index(e->fan[i]), cx, cy, 1.0f, 1.0f, a, DEPTH_CARDS + i * 0.4f, NULL);
	}

	/* Logotipo flotando por delante de la pantalla */
	gfx_sprite(SPR_LOGO, 200, 50 + sinf(t * 2.0f) * 3.0f, 1.0f, DEPTH_BANNER);

	/* Descripción del juego seleccionado */
	gfx_panel(156, 108, 232, 106, DEPTH_UI, C2D_Color32(40, 8, 12, 200), C2D_Color32(230, 170, 50, 255));
	if (e->scene == SCENE_PROFILE)
		mii_draw(me, 176, 126, 8, DEPTH_UI, EXPR_HAPPY, false);
	else
		gfx_sprite(SPR_SUIT_FIRST + e->icon, 176, 126, 0.8f, DEPTH_UI);
	gfx_text(e->scene == SCENE_PROFILE ? me->name : e->name, 192, 115, DEPTH_UI, 0.6f, COLOR_GOLD, C2D_AlignLeft);
	/* Caras de los Miis con los que se juega */
	for (int i = 0, k = 0; i < 3; i++) {
		if (e->rivals[i] < 0)
			continue;
		mii_draw(cpu_avatar((CpuId)e->rivals[i]), 374 - k * 22, 124, 8, DEPTH_UI, mood_expr(&s_mood), false);
		k++;
	}
	gfx_text_wrap(e->description, 166, 140, DEPTH_UI, 0.42f, COLOR_CREAM, 214);
	if (e->scene == SCENE_COUNT) {
		gfx_rrect(290, 112, 92, 20, 7, DEPTH_UI, C2D_Color32(120, 120, 120, 255));
		gfx_text("Próximamente", 336, 114, DEPTH_UI, 0.45f, COLOR_WHITE, C2D_AlignCenter);
	}
}

static void menu_draw_bottom(void)
{
	gfx_background(BG_MENU_BOTTOM);
	gfx_text("Juegos", BOTTOM_W / 2, 2, 0, 0.55f, COLOR_GOLD, C2D_AlignCenter);
	gfx_text("Próximamente", BOTTOM_W / 2, SOON_HEAD, 0, 0.45f, C2D_Color32(220, 200, 180, 255), C2D_AlignCenter);
	gfx_text("Configuración", BOTTOM_W / 2, CONFIG_HEAD, 0, 0.45f, COLOR_GOLD, C2D_AlignCenter);

	float shake = s_shake > 0.0f ? sinf(s_shake * 60.0f) * 4.0f : 0.0f;

	/* Cursor */
	float cx, cy, cw, ch;
	item_rect(s_selected, &cx, &cy, &cw, &ch);
	float pulse = 0.5f + 0.5f * sinf(gfx_time() * 5.0f);
	gfx_rrect(s_cursor_x - 3 + shake, s_cursor_y - 3, cw + 6, ch + 6, 10, 0,
		C2D_Color32(255, 215, 90, (u8)(150 + 100 * pulse)));

	for (int i = 0; i < NUM_ENTRIES; i++) {
		const MenuEntry* e = &entries[i];
		bool available = e->scene != SCENE_COUNT;
		bool small = is_config(i);
		float x, y, w, h;
		item_rect(i, &x, &y, &w, &h);
		if (i == s_selected)
			x += shake;
		u32 fill = available ? C2D_Color32(250, 244, 228, 255) : C2D_Color32(200, 190, 180, 255);
		gfx_panel(x, y, w, h, 0, fill, C2D_Color32(110, 20, 25, 255));
		float icon = small ? 0.6f : 0.75f;
		if (e->scene == SCENE_PROFILE)
			mii_draw(&profile_get()->avatar, x + 13, y + h / 2, small ? 6 : 8, 0, EXPR_NORMAL, false);
		else if (e->scene == SCENE_CARDBACK)
			gfx_card_back(x + 13, y + h / 2, 0.16f, 0.16f, 0.15f, 0, NULL);
		else
			gfx_sprite(SPR_SUIT_FIRST + e->icon, x + 13, y + h / 2, icon, 0);
		gfx_text(e->name, x + 26, y + (small ? 2 : 3), 0, small ? 0.45f : 0.52f,
			available ? C2D_Color32(90, 15, 20, 255) : C2D_Color32(110, 100, 95, 255), C2D_AlignLeft);
	}
	gfx_text(GLYPH_DPAD " Elegir   " GLYPH_A " Aceptar   START Salir", BOTTOM_W / 2, 223, 0, 0.45f, COLOR_CREAM,
		C2D_AlignCenter);
}

const Scene scene_menu = { menu_enter, menu_update, menu_draw_top, menu_draw_bottom };
