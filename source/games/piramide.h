#ifndef GAMES_PIRAMIDE_H
#define GAMES_PIRAMIDE_H

#include <stdbool.h>

#include "core/cards.h"

/* Solitario en pirámide.
 *
 * Se reparte una pirámide de siete filas (28 cartas). Las siete de la base
 * están boca arriba y las de encima boca abajo: no se sabe qué hay arriba.
 * Las 12 cartas que sobran forman tu mazo.
 *
 * Sacas cartas del mazo de una en una. Si en la base descubierta hay una
 * carta igual (del mismo número o figura: si sacas un caballo, un caballo),
 * puedes quitarla de la pirámide. También puedes quitar a la vez dos cartas
 * descubiertas iguales. Una carta de arriba se descubre cuando ya no tiene
 * ninguna de las dos cartas que la tapan.
 *
 * Las cartas que quitas de la pirámide pasan a tus descartes, así que en la
 * siguiente pasada vuelven a salir del mazo. El mazo se puede pasar cuatro
 * veces. Ganas si vacías la pirámide. */

#define PYR_ROWS   7
#define PYR_CARDS  (PYR_ROWS * (PYR_ROWS + 1) / 2)
#define PYR_STOCK  (DECK_SIZE - PYR_CARDS)
#define PYR_PASSES 4

typedef struct {
	Card card[PYR_CARDS];
	bool removed[PYR_CARDS];
	Card stock[DECK_SIZE];
	int stock_count;
	Card waste[DECK_SIZE];
	int waste_count; /* la de arriba es la carta actual */
	int pass;        /* pasada del mazo (1..PYR_PASSES) */
	int left;        /* cartas que quedan en la pirámide */
	int moves;
	Rng rng;
} Piramide;

/* Índice de la carta de la fila row (0 = cima) y posición pos (0..row). */
static inline int pyr_index(int row, int pos)
{
	return row * (row + 1) / 2 + pos;
}
int pyr_row(int index);

void pyr_init(Piramide* g, uint32_t seed);
void pyr_new_game(Piramide* g);

/* Boca arriba: no la tapa ninguna carta (y no se ha quitado). */
bool pyr_exposed(const Piramide* g, int index);
/* Carta actual del mazo (la de arriba de los descartes), o false si no hay. */
bool pyr_current(const Piramide* g, Card* out);
/* Sacar carta; si el mazo se ha acabado y quedan pasadas, se le da la vuelta. */
bool pyr_can_draw(const Piramide* g);
bool pyr_draw(Piramide* g);
/* Quitar una carta igual a la actual. */
bool pyr_can_take(const Piramide* g, int index);
bool pyr_take(Piramide* g, int index);
/* Quitar dos cartas descubiertas iguales. */
bool pyr_can_pair(const Piramide* g, int a, int b);
bool pyr_pair(Piramide* g, int a, int b);

bool pyr_won(const Piramide* g);
/* Ya no se puede hacer nada: ni sacar, ni quitar cartas. */
bool pyr_stuck(const Piramide* g);

#endif
