#ifndef UI_GFX_H
#define UI_GFX_H

#include <citro2d.h>
#include <stdbool.h>

#include "core/cardback.h"
#include "core/cards.h"

#define TOP_W    400
#define BOTTOM_W 320
#define SCREEN_H 240

#define CARD_W 64
#define CARD_H 96

/* Índices del atlas gfx/sprites.t3s (el orden lo fija tools/gen_assets.py). */
enum {
	SPR_CARD_FIRST = 0, /* + card_index(c) */
	SPR_CARD_BACK = DECK_SIZE,
	SPR_CARD_GLOW,
	SPR_CARD_SHADOW,
	SPR_CHIP_RED,
	SPR_CHIP_BLUE,
	SPR_CHIP_GREEN,
	SPR_CHIP_BLACK,
	SPR_SUIT_FIRST, /* + Suit */
	SPR_LOGO = SPR_SUIT_FIRST + NUM_SUITS,
};

/* Índices del atlas gfx/backs.t3s (el orden lo fija tools/gen_backs.py). */
enum {
	BACK_EDGE,
	BACK_FILL,
	BACK_LINE,
	BACK_PATTERN_FIRST, /* + dibujo - 1 */
	BACK_EMBLEM_FIRST = BACK_PATTERN_FIRST + CB_PATTERNS - 1, /* + emblema - 1 */
};

/* Índices del atlas gfx/board.t3s (el orden lo fija tools/gen_board.py). */
enum {
	BOARD_UNO_FIRST = 0, /* + color * 13 + valor (0..9, salta, cambio de sentido, roba dos) */
	BOARD_UNO_WILD = 52,
	BOARD_UNO_DRAW4,
	BOARD_UNO_BACK,
	BOARD_TILE,  /* ficha del Rummikub (26x34) */
	BOARD_JOKER, /* carita del comodín, encima de una ficha */
};

/* Índices del atlas gfx/backgrounds.t3s. */
enum {
	BG_TITLE_TOP,
	BG_MENU_BOTTOM,
	BG_TABLE_TOP,
	BG_TABLE_BOTTOM,
};

/* Glifos de botones de la fuente del sistema de la 3DS. */
#define GLYPH_A     "\xEE\x80\x80"
#define GLYPH_B     "\xEE\x80\x81"
#define GLYPH_X     "\xEE\x80\x82"
#define GLYPH_Y     "\xEE\x80\x83"
#define GLYPH_L     "\xEE\x80\x84"
#define GLYPH_R     "\xEE\x80\x85"
#define GLYPH_DPAD  "\xEE\x80\x86"

#define COLOR_WHITE   C2D_Color32(255, 255, 255, 255)
#define COLOR_BLACK   C2D_Color32(0, 0, 0, 255)
#define COLOR_SHADOW  C2D_Color32(0, 0, 0, 150)
#define COLOR_GOLD    C2D_Color32(255, 210, 80, 255)
#define COLOR_CREAM   C2D_Color32(255, 246, 225, 255)
#define COLOR_RED     C2D_Color32(210, 45, 50, 255)
#define COLOR_GREEN   C2D_Color32(70, 190, 90, 255)
#define COLOR_PANEL   C2D_Color32(0, 0, 0, 120)

/* Profundidades estereoscópicas (unidades arbitrarias, positivo = hacia el
 * jugador). Solo afectan a la pantalla superior cuando el 3D está activado. */
#define DEPTH_TABLE   0.0f
#define DEPTH_CARDS   1.0f
#define DEPTH_LIFTED  2.5f
#define DEPTH_UI      2.0f
#define DEPTH_BANNER  3.5f

bool gfx_init(void);
void gfx_exit(void);

/* Inicio y fin de fotograma. */
void gfx_frame_begin(void);
void gfx_frame_end(void);
/* Prepara el dibujo de la pantalla superior para un ojo (0 = izquierdo,
 * 1 = derecho). Devuelve false si ese ojo no hace falta (3D apagado). */
bool gfx_begin_top(int eye);
void gfx_begin_bottom(void);
/* Desplazamiento horizontal para simular profundidad en el ojo actual. */
float gfx_dx(float depth);
/* Tiempo global en segundos, útil para animaciones. */
float gfx_time(void);

void gfx_background(int bg);
C2D_Image gfx_sprite_image(int id);
/* Sprite del atlas de los juegos de mesa, centrado en (cx, cy). */
void gfx_board(int id, float cx, float cy, float scale, float angle, float depth, const C2D_ImageTint* tint);
/* Pieza de los personajes (atlas gfx/chars.t3s, índices CHR_* de ui/chars_layout.h). */
C2D_Image gfx_char_image(int id);
/* Dibuja un sprite centrado en (cx, cy). */
void gfx_sprite(int id, float cx, float cy, float scale, float depth);
void gfx_sprite_ex(int id, float cx, float cy, float sx, float sy, float angle, float depth, const C2D_ImageTint* tint);

/* Carta centrada en (cx, cy). flip: 0 = boca abajo, 1 = boca arriba; los
 * valores intermedios producen la animación de giro. */
void gfx_card(Card c, float cx, float cy, float scale, float flip, float angle, float depth, float dim, bool shadow);

/* Dorso de las cartas: el diseño del editor (se vuelve a componer en el
 * siguiente fotograma) y el dibujo de un dorso suelto centrado en (cx, cy). */
void gfx_set_card_back(const CardBack* b);
const CardBack* gfx_card_back_design(void);
void gfx_card_back(float cx, float cy, float sx, float sy, float angle, float depth, const C2D_ImageTint* tint);

/* Búfer de texto del fotograma, para dibujar texto a mano con C2D_DrawText. */
C2D_TextBuf gfx_textbuf(void);
/* Texto con sombra. align: C2D_AlignLeft, C2D_AlignCenter o C2D_AlignRight. */
void gfx_text(const char* str, float x, float y, float depth, float scale, u32 color, u32 align);
void gfx_textf(float x, float y, float depth, float scale, u32 color, u32 align, const char* fmt, ...)
	__attribute__((format(printf, 7, 8)));
float gfx_text_width(const char* str, float scale);
/* Texto alineado a la izquierda que salta de línea al llegar a width. */
void gfx_text_wrap(const char* str, float x, float y, float depth, float scale, u32 color, float width);

void gfx_rect(float x, float y, float w, float h, float depth, u32 color);
void gfx_ellipse(float cx, float cy, float rx, float ry, float depth, u32 color);
void gfx_line(float x0, float y0, float x1, float y1, float width, float depth, u32 color);
void gfx_triangle(float x0, float y0, float x1, float y1, float x2, float y2, float depth, u32 color);
/* Color 0xRRGGBB a formato citro2d */
u32 gfx_rgb(uint32_t rgb, u8 alpha);
void gfx_rrect(float x, float y, float w, float h, float r, float depth, u32 color);
/* Panel redondeado con borde, al estilo de los menús de 3DS. */
void gfx_panel(float x, float y, float w, float h, float depth, u32 fill, u32 border);
/* Fundido de toda la pantalla actual con alpha 0..1. */
void gfx_fade(float alpha);

/* Pila de fichas decorativa (cantidad -> número de fichas dibujadas). */
void gfx_chip_stack(int amount, float cx, float cy, float depth);

#endif
