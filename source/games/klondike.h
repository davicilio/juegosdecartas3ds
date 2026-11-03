#ifndef GAMES_KLONDIKE_H
#define GAMES_KLONDIKE_H

#include <stdbool.h>
#include <stdint.h>

#include "core/rng.h"

/* Solitario Klondike con la baraja francesa: siete columnas (la primera con
 * una carta, la séptima con siete, solo la última boca arriba), el mazo y
 * cuatro montones por palo del as al rey. En las columnas se baja de uno en
 * uno alternando colores, y en una columna vacía solo puede ir un rey. Del
 * mazo se roba de una en una o de tres en tres, y cuando se acaba se le da la
 * vuelta al descarte. */

#define KL_COLS    7
#define KL_HISTORY 48

typedef struct {
	uint8_t rank; /* 1..13 */
	uint8_t suit; /* 0 picas, 1 corazones, 2 diamantes, 3 tréboles */
	bool up;
} KlCard;

/* Lugares: 0..6 columnas, 7..10 montones, 11 descarte */
#define KL_FOUND0 KL_COLS
#define KL_WASTE  (KL_COLS + 4)

typedef struct {
	KlCard col[KL_COLS][20];
	int col_n[KL_COLS];
	KlCard stock[24], waste[24];
	int stock_n, waste_n;
	int found[4]; /* carta más alta de cada palo en su montón */
	int moves, passes;
} KlBoard;

typedef struct {
	KlBoard b;
	KlBoard history[KL_HISTORY];
	int history_n;
	int draw;     /* 1 o 3 */
	bool won;
	Rng rng;
} Klondike;

bool kl_red(KlCard c);
void kl_init(Klondike* g, int draw, uint32_t seed);
/* Roba del mazo (o da la vuelta al descarte) */
void kl_draw(Klondike* g);
/* ¿Se pueden mover n cartas desde from (columna o descarte, n = 1) a to? */
bool kl_can_move(const KlBoard* b, int from, int n, int to);
bool kl_move(Klondike* g, int from, int n, int to);
/* Sube al montón la carta de from si se puede */
bool kl_auto_found(Klondike* g, int from);
bool kl_undo(Klondike* g);
/* Sube solas todas las cartas que ya no hacen falta (cuando todo está a la vista) */
bool kl_can_finish(const KlBoard* b);
int kl_finish_step(Klondike* g);

#endif
