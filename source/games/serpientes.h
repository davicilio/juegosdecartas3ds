#ifndef GAMES_SERPIENTES_H
#define GAMES_SERPIENTES_H

#include <stdbool.h>

#include "core/rng.h"

/* Serpientes y escaleras: de 2 a 4 jugadores en un tablero de 100 casillas.
 * Se tira un dado y se avanza; al pie de una escalera se sube y en la cabeza
 * de una serpiente se baja. Hay que llegar justo a la 100: lo que sobra se
 * cuenta hacia atrás. Con un 6 se vuelve a tirar. */

#define SE_PLAYERS 4
#define SE_GOAL    100
#define SE_LINKS   16

typedef struct {
	int from, to; /* escalera si to > from, serpiente si to < from */
} SeLink;

extern const SeLink SE_BOARD[SE_LINKS];

typedef struct {
	int players;
	int pos[SE_PLAYERS];   /* 0 = fuera del tablero, 1..100 */
	int current;
	int winner;            /* -1 */
	int last_roll, last_land, last_to;
	Rng rng;
} Serpientes;

void se_init(Serpientes* g, int players, uint32_t seed);
/* Tira el dado (die 0 = al azar) y mueve al que le toca */
int se_roll(Serpientes* g, int die);
/* Adónde lleva la casilla (ella misma si no hay escalera ni serpiente) */
int se_follow(int square);

#endif
