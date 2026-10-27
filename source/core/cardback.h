#ifndef CORE_CARDBACK_H
#define CORE_CARDBACK_H

#include <stdbool.h>
#include <stdint.h>

/* Diseño del dorso de las cartas, que el jugador cambia en el editor: color
 * de fondo, dibujo, marco, emblema y un texto propio (por ejemplo, el nombre
 * de un banco o de una peña). */

#define CARDBACK_MAGIC    0x444F5231u /* "DOR1" */
#define CARDBACK_VERSION  1
#define CARDBACK_TEXT_MAX 48 /* UTF-8, hasta CARDBACK_TEXT_CHARS caracteres */
#define CARDBACK_TEXT_CHARS 16

typedef enum {
	CB_BACKGROUND,    /* color de fondo */
	CB_PATTERN,       /* dibujo */
	CB_PATTERN_COLOR, /* color del dibujo (0 = automático) */
	CB_FRAME,         /* color del marco (0 = sin marco) */
	CB_EMBLEM,        /* emblema del centro (0 = ninguno) */
	CB_TEXT_COLOR,
	CB_TEXT_POS,
	CB_OPTIONS
} CardBackOption;

typedef enum { CB_TEXT_TOP, CB_TEXT_MIDDLE, CB_TEXT_BOTTOM, CB_TEXT_POSITIONS } CardBackTextPos;

#define CB_COLORS   14
#define CB_PATTERNS 7 /* liso + 6 dibujos */
#define CB_FRAMES   5 /* sin marco + 4 colores */
#define CB_EMBLEMS  6 /* ninguno + 5 emblemas */

typedef struct {
	uint32_t magic;
	uint16_t version;
	uint8_t value[CB_OPTIONS];
	char text[CARDBACK_TEXT_MAX];
} CardBack;

/* El dorso de siempre: granate con rombos, marco dorado y los cuatro palos. */
void cardback_default(CardBack* b);
/* Recorta cada valor a su rango y termina el texto. */
void cardback_sanitize(CardBack* b);
bool cardback_load(CardBack* b, const char* path);
bool cardback_save(const CardBack* b, const char* path);

/* Número de valores de una opción, y nombre de cada valor y de la opción. */
int cardback_option_count(CardBackOption o);
const char* cardback_option_label(CardBackOption o);
const char* cardback_value_name(CardBackOption o, int value);
/* Pasa al valor siguiente (dir = 1) o anterior (dir = -1), dando la vuelta. */
void cardback_step(CardBack* b, CardBackOption o, int dir);

/* Colores en formato 0xRRGGBB */
uint32_t cardback_color_rgb(int color);
uint32_t cardback_background_rgb(const CardBack* b);
uint32_t cardback_pattern_rgb(const CardBack* b);
uint32_t cardback_frame_rgb(const CardBack* b);
uint32_t cardback_text_rgb(const CardBack* b);

/* Copia texto UTF-8 recortándolo a CARDBACK_TEXT_CHARS caracteres sin partir
 * ninguno por la mitad. */
void cardback_set_text(CardBack* b, const char* text);

#endif
