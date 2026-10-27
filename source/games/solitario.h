#ifndef GAMES_SOLITARIO_H
#define GAMES_SOLITARIO_H

#include <stdbool.h>

#include "core/cards.h"

/* Solitario tradicional con la baraja española de 40 cartas.
 *
 * Se reparten siete columnas (de 1 a 7 cartas, solo la última boca arriba) y
 * las 12 cartas que sobran forman el mazo. Hay que llevar las cuarenta cartas
 * a los cuatro montones de los palos, del as al rey (1, 2 ... 7, sota,
 * caballo, rey).
 *
 * En las columnas las cartas se colocan en orden descendente y alternando el
 * palo (sobre un caballo de oros, una sota de cualquier otro palo). Se puede
 * mover un grupo de cartas boca arriba si está bien ordenado. En una columna
 * vacía solo se puede poner un rey. Del mazo se saca una carta cada vez y,
 * cuando se acaba, se le da la vuelta a las descartadas. */

#define SOL_COLUMNS 7
#define SOL_STOCK   (DECK_SIZE - 28)

/* Montones: el mazo, los descartes, los cuatro palos y las siete columnas */
typedef enum {
	SOL_PILE_STOCK,
	SOL_PILE_WASTE,
	SOL_PILE_FOUNDATION, /* + palo */
	SOL_PILE_COLUMN = SOL_PILE_FOUNDATION + NUM_SUITS, /* + columna */
	SOL_PILES = SOL_PILE_COLUMN + SOL_COLUMNS
} SolPile;

typedef struct {
	Card cards[DECK_SIZE];
	int count;
	int hidden; /* las primeras hidden cartas están boca abajo */
} SolColumn;

typedef struct {
	Card stock[DECK_SIZE];
	int stock_count;
	Card waste[DECK_SIZE];
	int waste_count;
	int foundation[NUM_SUITS]; /* cartas colocadas de cada palo (0..10) */
	SolColumn col[SOL_COLUMNS];
	int moves;
	int passes; /* veces que se ha dado la vuelta al mazo */
	bool won;
	Rng rng;
} Solitario;

void sol_init(Solitario* g, uint32_t seed);
void sol_new_game(Solitario* g);

/* Saca una carta del mazo o, si está vacío, le da la vuelta a los descartes. */
bool sol_draw(Solitario* g);
/* Mueve la carta from_index de la columna (o la de arriba del montón from)
 * y las que tiene encima al montón to. */
bool sol_can_move(const Solitario* g, int from, int from_index, int to);
bool sol_move(Solitario* g, int from, int from_index, int to);
/* El mejor destino para esa carta (primero su palo, luego una columna), o -1. */
int sol_best_target(const Solitario* g, int from, int from_index);
/* Índice de la carta de arriba de un montón (-1 si está vacío). */
int sol_top_index(const Solitario* g, int pile);
/* Primera carta movible de una columna (la más profunda boca arriba). */
int sol_first_movable(const Solitario* g, int column);
/* Lleva a los palos una carta que se pueda; devuelve false si no hay ninguna. */
bool sol_auto_step(Solitario* g);
/* Ya no quedan cartas boca abajo ni en el mazo: se puede terminar solo. */
bool sol_can_finish(const Solitario* g);
/* Hay algún movimiento útil (o se puede sacar del mazo). */
bool sol_has_moves(const Solitario* g);

#endif
