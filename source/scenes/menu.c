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
	int kind;      /* KIND_SPANISH, KIND_BOARD o KIND_FRENCH */
} MenuEntry;

#define C(s, r) { SUIT_##s, r }

enum { KIND_SPANISH, KIND_BOARD, KIND_FRENCH };

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
	{ "Guiñote",
	  "Por parejas con tu compañero. Canta las veinte y las cuarenta con rey y sota del mismo palo "
	  "y, en el arrastre, asiste y mata. Gana la pareja que llegue a 101.",
	  SCENE_GUINOTE, SUIT_OROS, { C(OROS, RANK_REY), C(OROS, RANK_CABALLO), C(ESPADAS, 1) }, { CPU_PEPE, CPU_LUCIA, CPU_CARMEN } },
	{ "Solitario tradicional",
	  "El solitario de toda la vida con la baraja española: baja las cartas alternando palos y "
	  "llévalas a sus montones, del as al rey.",
	  SCENE_SOLITARIO, SUIT_OROS, { C(OROS, 1), C(OROS, 2), C(OROS, 3) }, { -1, -1, -1 } },
	{ "Solitario en pirámide",
	  "La base de la pirámide está descubierta y lo de arriba, tapado. Saca cartas de tu mazo y "
	  "quita de la base las iguales: si sacas un caballo, busca un caballo.",
	  SCENE_PIRAMIDE, SUIT_COPAS, { C(COPAS, RANK_CABALLO), C(ESPADAS, RANK_CABALLO), C(BASTOS, 5) }, { -1, -1, -1 } },
	{ "Tute",
	  "Por parejas, con las 40 cartas repartidas. Asiste y mata, canta las veinte y las cuarenta con "
	  "rey y caballo... ¡y si juntas los cuatro reyes o caballos, canta el tute!",
	  SCENE_TUTE, SUIT_COPAS, { C(COPAS, RANK_CABALLO), C(COPAS, RANK_REY), C(OROS, 1) }, { CPU_LOLA, CPU_PACO, CPU_MANOLO } },
	{ "Culo",
	  "Echa cartas iguales más altas que las de la mesa o pasa. El primero en quedarse sin cartas es el "
	  "presidente; el último, el culo, que tendrá que darle sus mejores cartas.",
	  SCENE_CULO, SUIT_BASTOS, { C(BASTOS, 2), C(BASTOS, 2), C(OROS, 2) }, { -1, -1, -1 } },
	{ "Mentiroso",
	  "Echa cartas boca abajo diciendo lo que son... aunque sea mentira. Si te pillan, te llevas el montón.",
	  SCENE_MENTIROSO, SUIT_ESPADAS, { C(ESPADAS, 4), C(OROS, 4), C(COPAS, 4) }, { -1, -1, -1 } },
	{ "Mierda",
	  "Otro clásico de la baraja española que llegará en una próxima versión.",
	  SCENE_COUNT, SUIT_BASTOS, { C(BASTOS, 7), C(COPAS, 1), C(ESPADAS, 6) }, { -1, -1, -1 } },
	{ "Burro",
	  "Juego de cartas para reírse en grupo. El que pierde va sumando letras hasta formar BURRO.",
	  SCENE_COUNT, SUIT_OROS, { C(OROS, 6), C(COPAS, 6), C(BASTOS, 6) }, { -1, -1, -1 } },
	{ "As, 2, 3",
	  "Se reparten todas las cartas boca abajo y se echan por turnos contando as, dos, tres... "
	  "Si tu carta coincide con lo que dices, te llevas el montón. Gana quien se quede sin cartas.",
	  SCENE_AS23, SUIT_COPAS, { C(OROS, 1), C(COPAS, 2), C(ESPADAS, 3) }, { CPU_MANOLO, CPU_LUCIA, CPU_RAMON } },
	{ "Tute subastado",
	  "Tute por parejas en el que se puja por los puntos que se van a hacer. Quien gana la subasta "
	  "elige el triunfo.",
	  SCENE_COUNT, SUIT_COPAS, { C(COPAS, 1), C(COPAS, 3), C(COPAS, RANK_REY) }, { -1, -1, -1 } },
	{ "Tute cabrón",
	  "El tute de cada uno para sí: cuatro jugadores sin compañero. Gana la mano quien más puntos "
	  "sume; la partida, el primero que gane tres manos.",
	  SCENE_TUTE_CABRON, SUIT_ESPADAS, { C(ESPADAS, RANK_REY), C(BASTOS, RANK_REY), C(OROS, 3) }, { CPU_PEPE, CPU_LUCIA, CPU_CARMEN } },
	{ "Julepe",
	  "Juego de bazas con triunfo: hay que hacer al menos dos bazas o pagar el julepe.",
	  SCENE_COUNT, SUIT_BASTOS, { C(BASTOS, 1), C(BASTOS, 3), C(COPAS, 7) }, { -1, -1, -1 } },
	{ "Tresillo",
	  "Clásico de bazas a tres jugadores, antepasado del julepe, con su famosa espadilla y su basto.",
	  SCENE_COUNT, SUIT_ESPADAS, { C(ESPADAS, 1), C(BASTOS, 1), C(OROS, RANK_REY) }, { -1, -1, -1 } },
	{ "Cuatrillo",
	  "La versión del tresillo para cuatro jugadores.",
	  SCENE_COUNT, SUIT_OROS, { C(ESPADAS, 1), C(BASTOS, 1), C(COPAS, RANK_SOTA) }, { -1, -1, -1 } },
	{ "Pocha",
	  "Diez manos de una a diez cartas. Antes de jugar, di cuántas bazas vas a hacer: si aciertas, "
	  "10 puntos más 5 por baza; si fallas, pierdes 5 por cada baza de diferencia.",
	  SCENE_POCHA, SUIT_COPAS, { C(COPAS, 1), C(OROS, 3), C(BASTOS, RANK_REY) }, { CPU_CARMEN, CPU_RAMON, CPU_LOLA } },
	{ "Subasta",
	  "Juego de bazas por parejas en el que se puja por los puntos de la mano.",
	  SCENE_COUNT, SUIT_OROS, { C(OROS, 1), C(OROS, 3), C(ESPADAS, RANK_REY) }, { -1, -1, -1 } },
	{ "Truc",
	  "El juego de faroles de Valencia, Cataluña y Baleares: envites, retrucos y señas entre "
	  "compañeros.",
	  SCENE_COUNT, SUIT_ESPADAS, { C(ESPADAS, 1), C(BASTOS, 1), C(ESPADAS, 7) }, { -1, -1, -1 } },
	{ "Truco argentino",
	  "Envido, truco y flor: el juego de faroles más popular de Argentina y Uruguay.",
	  SCENE_COUNT, SUIT_BASTOS, { C(ESPADAS, 1), C(BASTOS, 1), C(OROS, 7) }, { -1, -1, -1 } },
	{ "Butifarra",
	  "El gran juego catalán por parejas, con triunfo elegido y contrada.",
	  SCENE_COUNT, SUIT_COPAS, { C(COPAS, 1), C(COPAS, 7), C(OROS, RANK_REY) }, { -1, -1, -1 } },
	{ "Treinta y una",
	  "Acércate a 31 sumando cartas del mismo palo, sin pasarte de la raya.",
	  SCENE_TREINTAYUNA, SUIT_OROS, { C(OROS, 1), C(OROS, RANK_REY), C(OROS, RANK_CABALLO) }, { -1, -1, -1 } },
	{ "La mona",
	  "Ve emparejando cartas. Pierde quien se quede al final con la carta sin pareja: la mona.",
	  SCENE_MONA, SUIT_COPAS, { C(COPAS, RANK_SOTA), C(OROS, RANK_SOTA), C(BASTOS, 4) }, { -1, -1, -1 } },
	{ "Cucú",
	  "Cada uno tiene una carta y puede cambiarla con el de al lado. Pierde quien acaba con la más "
	  "baja.",
	  SCENE_CUCU, SUIT_BASTOS, { C(BASTOS, 1), C(OROS, 2), C(COPAS, RANK_REY) }, { -1, -1, -1 } },
	{ "Pumba",
	  "Quédate sin cartas antes que nadie, con cartas que hacen robar o saltar turno. ¡Pumba!",
	  SCENE_COUNT, SUIT_ESPADAS, { C(ESPADAS, 2), C(OROS, 2), C(COPAS, RANK_SOTA) }, { -1, -1, -1 } },
	/* Baraja francesa */
	{ "Póker",
	  "Texas Hold'em contra tres personajes: dos cartas tuyas y cinco en la mesa. Pasa, iguala, sube o "
	  "retírate... ¡o vete con todo! Gana quien se quede con todas las fichas.",
	  SCENE_POKER, SUIT_OROS, { C(OROS, 1), C(OROS, 1), C(OROS, 1) }, { CPU_RAMON, CPU_LUCIA, CPU_MANOLO },
	  KIND_FRENCH },
	{ "Blackjack",
	  "Contra la banca: acércate a 21 sin pasarte. Pide, plántate, dobla o separa las parejas. "
	  "El blackjack paga 3 a 2. Abajo te dice qué haría la estrategia básica.",
	  SCENE_BLACKJACK, SUIT_OROS, { C(OROS, 1), C(OROS, 1), C(OROS, 1) }, { CPU_RAMON, -1, -1 }, KIND_FRENCH },
	{ "Corazones",
	  "Evita llevarte corazones (1 punto) y la reina de picas (13). Antes de cada mano se pasan tres "
	  "cartas. ¡O llévatelos todos y dispara a la luna! A 100 puntos gana quien menos tenga.",
	  SCENE_CORAZONES, SUIT_OROS, { C(OROS, 1), C(OROS, 1), C(OROS, 1) }, { CPU_LOLA, CPU_PEPE, CPU_PACO },
	  KIND_FRENCH },
	/* Juegos de mesa */
	{ "Conecta 4",
	  "Deja caer tus fichas rojas en las columnas y junta cuatro en línea antes que la CPU: en "
	  "horizontal, vertical o diagonal. Tres niveles de dificultad.",
	  SCENE_CONECTA4, SUIT_OROS, { C(OROS, 1), C(OROS, 1), C(OROS, 1) }, { CPU_PEPE, -1, -1 }, KIND_BOARD },
	{ "Damas",
	  "Damas españolas: comer es obligatorio (y la jugada que más come), las fichas comen hacia "
	  "delante y la dama vuela por las diagonales. Tres niveles de dificultad.",
	  SCENE_DAMAS, SUIT_OROS, { C(OROS, 1), C(OROS, 1), C(OROS, 1) }, { CPU_MANOLO, -1, -1 }, KIND_BOARD },
	{ "Dominó",
	  "Por parejas, como en el bar: casa tus fichas con los extremos de la mesa. Gana la mano quien "
	  "se queda sin fichas (o la pareja con menos puntos si se cierra). A 100 puntos.",
	  SCENE_DOMINO, SUIT_OROS, { C(OROS, 1), C(OROS, 1), C(OROS, 1) }, { CPU_LOLA, CPU_PACO, CPU_MANOLO },
	  KIND_BOARD },
	{ "Uno",
	  "Echa cartas del mismo color o número. Salta, cambio de sentido, roba dos y comodines... "
	  "¡y no te olvides de decir \"¡Uno!\" cuando te quede una carta!",
	  SCENE_UNO, SUIT_OROS, { C(OROS, 1), C(OROS, 1), C(OROS, 1) }, { CPU_PEPE, CPU_LOLA, CPU_CARMEN }, KIND_BOARD },
	{ "Rummikub",
	  "Fichas del 1 al 13 en cuatro colores. Baja escaleras y grupos (la primera vez, de al menos 30 "
	  "puntos) y añade fichas a la mesa. Gana quien se quede sin fichas.",
	  SCENE_RUMMIKUB, SUIT_OROS, { C(OROS, 1), C(OROS, 1), C(OROS, 1) }, { CPU_PACO, CPU_LUCIA, CPU_MANOLO }, KIND_BOARD },
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

/* Páginas (pestañas arriba): baraja española, baraja francesa y juegos de
 * mesa. Cada página tiene sus juegos y sus próximos juegos en dos columnas,
 * con desplazamiento. Configuración va aparte, fija abajo y en pequeño. */
enum { PAGE_SPANISH, PAGE_FRENCH, PAGE_BOARD, PAGES };
static const char* const PAGE_TITLE[PAGES] = { "Baraja española", "Baraja francesa", "De mesa" };
enum { SEC_PLAY, SEC_SOON, SEC_LIST, SEC_CONFIG = SEC_LIST, SEC_HIDDEN = -1 };
static const char* const SECTION_TITLE[SEC_LIST] = { "Juegos", "Próximamente" };

#define COL_X(c)    (8.0f + (c) * 156.0f)
#define TAB_H       22.0f
#define HEAD_H      15.0f
#define GAME_W      148.0f
#define GAME_H      22.0f
#define GAME_STEP   24.0f
#define VIEW_TOP    TAB_H
#define VIEW_BOTTOM 186.0f
#define CONFIG_HEAD (VIEW_BOTTOM + 2.0f)
#define CONFIG_Y    (CONFIG_HEAD + 13.0f)
#define CONFIG_H    18.0f
#define MAX_ROWS    64

static int s_page;

static int page_of(const MenuEntry* e)
{
	return e->kind == KIND_FRENCH ? PAGE_FRENCH : e->kind == KIND_BOARD ? PAGE_BOARD : PAGE_SPANISH;
}

static int section_of(int i)
{
	const MenuEntry* e = &entries[i];
	if (e->scene == SCENE_PROFILE || e->scene == SCENE_CARDBACK)
		return SEC_CONFIG;
	if (page_of(e) != s_page)
		return SEC_HIDDEN;
	return e->scene == SCENE_COUNT ? SEC_SOON : SEC_PLAY;
}

/* Posiciones de la página actual: y de cada elemento (en la lista), su
 * columna y su fila para la cruceta; y de cada título. */
static float s_item_y[NUM_ENTRIES];
static int s_item_col[NUM_ENTRIES], s_item_row[NUM_ENTRIES];
static int s_row_item[MAX_ROWS][2];
static int s_rows;
static float s_head_y[SEC_LIST];
static int s_section_count[SEC_LIST + 1];
static float s_content_h;
static int s_layout_page = -1;

static void layout(void)
{
	if (s_layout_page == s_page)
		return;
	s_layout_page = s_page;
	float y = TAB_H + 2.0f;
	s_rows = 0;
	for (int sec = 0; sec <= SEC_LIST; sec++) {
		int k = 0;
		if (sec < SEC_LIST) {
			s_head_y[sec] = y;
			y += HEAD_H;
		}
		for (int i = 0; i < NUM_ENTRIES; i++) {
			if (section_of(i) != sec)
				continue;
			if (k % 2 == 0) {
				s_row_item[s_rows][0] = s_row_item[s_rows][1] = i;
				s_rows++;
			}
			s_row_item[s_rows - 1][k % 2] = i;
			s_item_col[i] = k % 2;
			s_item_row[i] = s_rows - 1;
			s_item_y[i] = sec == SEC_CONFIG ? CONFIG_Y : y + (k / 2) * GAME_STEP;
			k++;
		}
		s_section_count[sec] = k;
		if (sec < SEC_LIST) {
			if (k == 0)
				y -= HEAD_H; /* apartado vacío: sin título */
			else
				y += (k + 1) / 2 * GAME_STEP + 1.0f;
		}
	}
	s_content_h = y + 2.0f;
}

static int s_selected;
static int s_page_selected[PAGES] = { -1, -1, -1 };
static Mood s_mood;
static float s_cursor_x, s_cursor_y;
static float s_scroll; /* desplazamiento de la lista de juegos */
static float s_shake; /* sacudida al elegir un juego no disponible */

static bool is_config(int i)
{
	return section_of(i) == SEC_CONFIG;
}

static void item_rect(int i, float* x, float* y, float* w, float* h)
{
	layout();
	*x = COL_X(s_item_col[i]);
	*y = s_item_y[i];
	*w = GAME_W;
	*h = is_config(i) ? CONFIG_H : GAME_H;
}

/* Posición en pantalla: los juegos se mueven con el desplazamiento */
static void screen_rect(int i, float* x, float* y, float* w, float* h)
{
	item_rect(i, x, y, w, h);
	if (!is_config(i))
		*y -= s_scroll;
}

/* Desplazamiento que deja a la vista el elemento elegido */
static float scroll_for(int i)
{
	float x, y, w, h;
	item_rect(i, &x, &y, &w, &h);
	if (is_config(i))
		return s_scroll;
	int sec = section_of(i);
	/* En la primera fila de un apartado se ve también su título */
	float top = y - 4.0f;
	if (y - s_head_y[sec] - HEAD_H < GAME_STEP)
		top = s_head_y[sec] - 2.0f;
	float target = s_scroll;
	if (top - VIEW_TOP < target)
		target = top - VIEW_TOP;
	if (y + h + 4.0f > target + VIEW_BOTTOM)
		target = y + h + 4.0f - VIEW_BOTTOM;
	float max = s_content_h - VIEW_BOTTOM;
	return target < 0.0f ? 0.0f : target > max ? max : target;
}

/* Primer elemento de la página (o configuración si no hay juegos) */
static int first_of_page(void)
{
	layout();
	return s_rows > 0 ? s_row_item[0][0] : 0;
}

static void set_page(int page)
{
	if (!is_config(s_selected))
		s_page_selected[s_page] = s_selected;
	s_page = (page + PAGES) % PAGES;
	layout();
	s_selected = s_page_selected[s_page] >= 0 ? s_page_selected[s_page] : first_of_page();
	s_scroll = 0.0f;
	s_scroll = scroll_for(s_selected);
	float x, y, w, h;
	screen_rect(s_selected, &x, &y, &w, &h);
	s_cursor_x = x;
	s_cursor_y = y;
	audio_play(SFX_SELECT);
}

static void menu_enter(void)
{
	float w, h;
	s_scroll = scroll_for(s_selected);
	screen_rect(s_selected, &s_cursor_x, &s_cursor_y, &w, &h);
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

/* Movimiento con la cruceta: las filas de todos los apartados seguidas */
static int move(int i, int dx, int dy)
{
	layout();
	int row = s_item_row[i], col = s_item_col[i];
	col = (col + dx + 2) % 2;
	row = (row + dy + s_rows) % s_rows;
	return s_row_item[row][col]; /* en una fila de un solo juego, ese */
}

static SceneId menu_update(const Input* in, float dt)
{
	if (s_shake > 0.0f)
		s_shake -= dt;
	mood_update(&s_mood, dt);
	/* Cambio de página: L/R o tocando la pestaña */
	if (in->down & KEY_L)
		set_page(s_page - 1);
	if (in->down & KEY_R)
		set_page(s_page + 1);
	for (int pg = 0; pg < PAGES; pg++)
		if (input_touch_in(in, pg * (BOTTOM_W / (float)PAGES), 0, BOTTOM_W / (float)PAGES, TAB_H) && pg != s_page)
			set_page(pg);
	int dx = (in->repeat & KEY_RIGHT) ? 1 : (in->repeat & KEY_LEFT) ? -1 : 0;
	int dy = (in->repeat & KEY_DOWN) ? 1 : (in->repeat & KEY_UP) ? -1 : 0;
	if ((dx || dy) && s_rows > 0) {
		s_selected = move(s_selected, dx, dy);
		audio_play(SFX_SELECT);
	}
	float x, y, w, h;
	float old_scroll = s_scroll;
	s_scroll = approach(s_scroll, scroll_for(s_selected), 14.0f, dt);
	s_cursor_y -= s_scroll - old_scroll; /* el cursor se mueve con la lista */
	screen_rect(s_selected, &x, &y, &w, &h);
	s_cursor_x = approach(s_cursor_x, x, 18.0f, dt);
	s_cursor_y = approach(s_cursor_y, y, 18.0f, dt);

	if (in->down & KEY_A)
		return launch(s_selected);
	if (in->down & KEY_START)
		return SCENE_QUIT;

	for (int i = 0; i < NUM_ENTRIES; i++) {
		if (section_of(i) == SEC_HIDDEN)
			continue;
		screen_rect(i, &x, &y, &w, &h);
		if (!is_config(i) && (y < VIEW_TOP || y + h > VIEW_BOTTOM))
			continue; /* tapado */
		if (input_touch_in(in, x, y, w, h)) {
			s_selected = i;
			return launch(i);
		}
	}
	return SCENE_MENU;
}

/* Icono de los juegos de mesa sin cartas (s = escala, 1 = unos 16 px) */
static void draw_board_icon(SceneId scene, float cx, float cy, float s, float depth)
{
	float dx = gfx_dx(depth);
	if (scene == SCENE_CONECTA4) {
		gfx_rrect(cx - 9 * s + dx, cy - 7 * s, 18 * s, 14 * s, 2 * s, depth, C2D_Color32(30, 70, 170, 255));
		for (int k = 0; k < 4; k++)
			gfx_ellipse(cx + (k % 2 ? 4.5f : -4.5f) * s + dx, cy + (k / 2 ? 3.2f : -3.2f) * s, 2.6f * s, 2.6f * s, depth,
				k == 3 ? C2D_Color32(245, 200, 30, 255) : C2D_Color32(220, 40, 45, 255));
	} else if (scene == SCENE_DAMAS) {
		for (int r = 0; r < 2; r++)
			for (int c = 0; c < 2; c++)
				gfx_rect(cx + (c - 1) * 8 * s + dx, cy + (r - 1) * 8 * s, 8 * s, 8 * s, depth,
					(r + c) % 2 ? C2D_Color32(120, 80, 45, 255) : C2D_Color32(235, 215, 175, 255));
		gfx_ellipse(cx - 4 * s + dx, cy + 4 * s, 3.2f * s, 3.2f * s, depth, C2D_Color32(240, 232, 215, 255));
		gfx_ellipse(cx + 4 * s + dx, cy - 4 * s, 3.2f * s, 3.2f * s, depth, C2D_Color32(45, 35, 35, 255));
	} else {
		gfx_rrect(cx - 5 * s + dx, cy - 9 * s, 10 * s, 18 * s, 2 * s, depth, C2D_Color32(250, 247, 235, 255));
		gfx_rect(cx - 4 * s + dx, cy - 0.3f * s, 8 * s, 0.6f * s + 0.5f, depth, C2D_Color32(120, 110, 100, 255));
		for (int k = 0; k < 2; k++)
			gfx_ellipse(cx + (k ? 2 : -2) * s + dx, cy - 4.5f * s + k * 0 + (k ? 2 : -2) * s * 0.5f, 1.2f * s, 1.2f * s,
				depth, C2D_Color32(30, 30, 35, 255));
		gfx_ellipse(cx + dx, cy + 4.5f * s, 1.2f * s, 1.2f * s, depth, C2D_Color32(30, 30, 35, 255));
	}
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
		if (e->scene != SCENE_RUMMIKUB && e->scene != SCENE_CONECTA4 && e->scene != SCENE_DAMAS &&
			e->scene != SCENE_DOMINO)
			gfx_sprite_ex(SPR_CARD_SHADOW, cx + 4, cy + 5, 1.0f, 1.0f, a, DEPTH_CARDS, NULL);
		if (e->scene == SCENE_CARDBACK)
			gfx_card_back(cx, cy, 1.0f, 1.0f, a, DEPTH_CARDS + i * 0.4f, NULL);
		else if (e->scene == SCENE_POKER || e->scene == SCENE_BLACKJACK || e->scene == SCENE_CORAZONES) {
			static const int poker[3] = { 0 * 13 + 12, 1 * 13 + 12, 2 * 13 + 11 };
			static const int bj[3] = { 3 * 13 + 12, 0 * 13 + 9, 2 * 13 + 6 };
			static const int hearts[3] = { 1 * 13 + 0, 1 * 13 + 8, 0 * 13 + 10 };
			const int* f = e->scene == SCENE_POKER ? poker : e->scene == SCENE_BLACKJACK ? bj : hearts;
			gfx_french(f[i], cx, cy, 1.0f, a, DEPTH_CARDS + i * 0.4f, false);
		} else if (e->scene == SCENE_CONECTA4 || e->scene == SCENE_DAMAS || e->scene == SCENE_DOMINO) {
			if (i == 1)
				draw_board_icon(e->scene, 78, 160, 4.0f, DEPTH_CARDS);
		} else if (e->scene == SCENE_UNO) {
			static const int uno[3] = { BOARD_UNO_FIRST + 0 * 13 + 7, BOARD_UNO_FIRST + 3 * 13 + 11, BOARD_UNO_WILD };
			gfx_board(uno[i], cx, cy, 1.0f, a, DEPTH_CARDS + i * 0.4f, NULL);
		} else if (e->scene == SCENE_RUMMIKUB) {
			/* Una escalera de fichas */
			gfx_board(BOARD_TILE, 52 + i * 30, 150, 1.4f, 0.0f, DEPTH_CARDS + i * 0.2f, NULL);
			static const u32 col[3] = { 0xD2282D, 0xD2282D, 0xD2282D };
			gfx_textf(52 + i * 30, 130, DEPTH_CARDS + i * 0.2f, 0.85f, gfx_rgb(col[i], 255), C2D_AlignCenter, "%d",
				7 + i);
		} else
			gfx_sprite_ex(SPR_CARD_FIRST + card_index(e->fan[i]), cx, cy, 1.0f, 1.0f, a, DEPTH_CARDS + i * 0.4f, NULL);
	}

	/* Logotipo flotando por delante de la pantalla */
	gfx_sprite(SPR_LOGO, 200, 50 + sinf(t * 2.0f) * 3.0f, 1.0f, DEPTH_BANNER);

	/* Descripción del juego seleccionado */
	gfx_panel(156, 108, 232, 106, DEPTH_UI, C2D_Color32(40, 8, 12, 200), C2D_Color32(230, 170, 50, 255));
	if (e->scene == SCENE_PROFILE)
		mii_draw(me, 176, 126, 8, DEPTH_UI, EXPR_HAPPY, false);
	else if (e->scene == SCENE_UNO)
		gfx_board(BOARD_UNO_WILD, 176, 126, 0.24f, 0.15f, DEPTH_UI, NULL);
	else if (e->scene == SCENE_POKER || e->scene == SCENE_BLACKJACK || e->scene == SCENE_CORAZONES)
		gfx_french(e->scene == SCENE_POKER ? 12 : e->scene == SCENE_BLACKJACK ? 3 * 13 + 12 : 1 * 13 + 12, 176, 126,
			0.24f, 0.15f, DEPTH_UI, false);
	else if (e->scene == SCENE_CONECTA4 || e->scene == SCENE_DAMAS || e->scene == SCENE_DOMINO)
		draw_board_icon(e->scene, 176, 126, 1.0f, DEPTH_UI);
	else if (e->scene == SCENE_RUMMIKUB)
		gfx_board(BOARD_JOKER, 176, 128, 0.8f, 0.0f, DEPTH_UI, NULL);
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
		gfx_rrect(290, 84, 92, 20, 7, DEPTH_UI, C2D_Color32(120, 120, 120, 255));
		gfx_text("Próximamente", 336, 86, DEPTH_UI, 0.45f, COLOR_WHITE, C2D_AlignCenter);
	}
}

static void menu_draw_bottom(void)
{
	gfx_background(BG_MENU_BOTTOM);
	layout();
	for (int sec = 0; sec < SEC_LIST; sec++) {
		if (s_section_count[sec] == 0)
			continue;
		bool soon = sec == SEC_SOON;
		gfx_textf(BOTTOM_W / 2, s_head_y[sec] - s_scroll, 0, soon ? 0.45f : 0.5f,
			soon ? C2D_Color32(220, 200, 180, 255) : COLOR_GOLD, C2D_AlignCenter, soon ? "%s (%d)" : "%s",
			SECTION_TITLE[sec], s_section_count[sec]);
	}
	if (s_section_count[SEC_PLAY] + s_section_count[SEC_SOON] == 0)
		gfx_text("Todavía no hay juegos aquí", BOTTOM_W / 2, 90, 0, 0.5f, COLOR_CREAM, C2D_AlignCenter);

	float shake = s_shake > 0.0f ? sinf(s_shake * 60.0f) * 4.0f : 0.0f;

	/* Cursor */
	float cx, cy, cw, ch;
	screen_rect(s_selected, &cx, &cy, &cw, &ch);
	float pulse = 0.5f + 0.5f * sinf(gfx_time() * 5.0f);
	if (!is_config(s_selected))
		gfx_rrect(s_cursor_x - 3 + shake, s_cursor_y - 3, cw + 6, ch + 6, 10, 0,
			C2D_Color32(255, 215, 90, (u8)(150 + 100 * pulse)));

	for (int i = 0; i < NUM_ENTRIES; i++) {
		const MenuEntry* e = &entries[i];
		bool available = e->scene != SCENE_COUNT;
		bool small = is_config(i);
		float x, y, w, h;
		if (section_of(i) == SEC_HIDDEN)
			continue;
		screen_rect(i, &x, &y, &w, &h);
		bool first_config = is_config(i) && (i == 0 || !is_config(i - 1));
		if (first_config) {
			/* Franja fija de configuración, que tapa la lista al desplazarse */
			gfx_rect(0, VIEW_BOTTOM, BOTTOM_W, SCREEN_H - VIEW_BOTTOM, 0, C2D_Color32(70, 10, 16, 255));
			gfx_rect(0, VIEW_BOTTOM, BOTTOM_W, 2, 0, C2D_Color32(230, 170, 50, 255));
			gfx_text("Configuración", BOTTOM_W / 2, CONFIG_HEAD, 0, 0.45f, COLOR_GOLD, C2D_AlignCenter);
			if (s_scroll < s_content_h - VIEW_BOTTOM - 1.0f)
				gfx_triangle(BOTTOM_W - 22, VIEW_BOTTOM - 10, BOTTOM_W - 10, VIEW_BOTTOM - 10, BOTTOM_W - 16,
					VIEW_BOTTOM - 3, 0, COLOR_GOLD);
			/* El cursor de configuración va encima de la franja */
			if (is_config(s_selected))
				gfx_rrect(s_cursor_x - 3 + shake, s_cursor_y - 3, cw + 6, ch + 6, 10, 0,
					C2D_Color32(255, 215, 90, (u8)(150 + 100 * pulse)));
		}
		if (!small && (y + h < VIEW_TOP - 8.0f || y > VIEW_BOTTOM))
			continue;
		if (i == s_selected)
			x += shake;
		u32 fill = available ? C2D_Color32(250, 244, 228, 255) : C2D_Color32(200, 190, 180, 255);
		gfx_panel(x, y, w, h, 0, fill, C2D_Color32(110, 20, 25, 255));
		float icon = small ? 0.6f : 0.75f;
		if (e->scene == SCENE_PROFILE)
			mii_draw(&profile_get()->avatar, x + 13, y + h / 2, small ? 6 : 8, 0, EXPR_NORMAL, false);
		else if (e->scene == SCENE_CARDBACK)
			gfx_card_back(x + 13, y + h / 2, 0.16f, 0.16f, 0.15f, 0, NULL);
		else if (e->scene == SCENE_UNO)
			gfx_board(BOARD_UNO_WILD, x + 13, y + h / 2, 0.2f, 0.15f, 0, NULL);
		else if (e->scene == SCENE_POKER || e->scene == SCENE_BLACKJACK || e->scene == SCENE_CORAZONES)
			gfx_french(e->scene == SCENE_POKER ? 12 : e->scene == SCENE_BLACKJACK ? 3 * 13 + 12 : 1 * 13 + 12, x + 13,
				y + h / 2, 0.2f, 0.15f, 0, false);
		else if (e->scene == SCENE_CONECTA4 || e->scene == SCENE_DAMAS || e->scene == SCENE_DOMINO)
			draw_board_icon(e->scene, x + 13, y + h / 2, 0.8f, 0);
		else if (e->scene == SCENE_RUMMIKUB)
			gfx_board(BOARD_JOKER, x + 13, y + h / 2 + 2, 0.75f, 0.0f, 0, NULL);
		else
			gfx_sprite(SPR_SUIT_FIRST + e->icon, x + 13, y + h / 2, icon, 0);
		float scale = small ? 0.45f : 0.52f;
		float tw = gfx_text_width(e->name, scale);
		if (tw > w - 30.0f)
			scale = fmaxf(0.45f, scale * (w - 30.0f) / tw);
		gfx_text(e->name, x + 26, y + (small ? 2 : 3), 0, scale,
			available ? C2D_Color32(90, 15, 20, 255) : C2D_Color32(110, 100, 95, 255), C2D_AlignLeft);
	}
	/* Pestañas de las páginas */
	gfx_rect(0, 0, BOTTOM_W, TAB_H, 0, C2D_Color32(70, 10, 16, 255));
	gfx_rect(0, TAB_H - 2, BOTTOM_W, 2, 0, C2D_Color32(230, 170, 50, 255));
	float tw = BOTTOM_W / (float)PAGES;
	for (int pg = 0; pg < PAGES; pg++) {
		bool on = pg == s_page;
		gfx_rrect(pg * tw + 3, 2, tw - 6, TAB_H - 4, 6, 0,
			on ? C2D_Color32(250, 220, 120, 255) : C2D_Color32(120, 30, 35, 255));
		gfx_text(PAGE_TITLE[pg], pg * tw + tw / 2, 4, 0, 0.45f, on ? C2D_Color32(90, 15, 20, 255) : COLOR_CREAM,
			C2D_AlignCenter);
	}
	gfx_text(GLYPH_L GLYPH_R " Página  " GLYPH_A " Aceptar  START Salir", BOTTOM_W / 2, 223, 0, 0.45f, COLOR_CREAM,
		C2D_AlignCenter);
}

const Scene scene_menu = { menu_enter, menu_update, menu_draw_top, menu_draw_bottom };
