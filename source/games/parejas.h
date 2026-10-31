#ifndef GAMES_PAREJAS_H
#define GAMES_PAREJAS_H

#include <stdbool.h>
#include <stdint.h>

#include "core/cards.h"
#include "core/rng.h"

/* Parejas: las cartas de dos barajas españolas, boca abajo. Se levantan dos;
 * si son iguales (mismo número y mismo palo) te las llevas y repites, y si
 * no, se vuelven a tapar. Se puede jugar solo (contando los intentos) o
 * contra un rival que recuerda mejor o peor las cartas que ha visto. */

#define PJ_MAX 24

typedef enum { PJ_PICK1, PJ_PICK2, PJ_SHOW, PJ_OVER } PjPhase;

typedef struct {
	Card card[PJ_MAX];
	bool taken[PJ_MAX];
	int owner[PJ_MAX];
	bool seen[PJ_MAX];  /* el rival se acuerda de esta carta */
	int n;              /* cartas en la mesa */
	int players;        /* 1 o 2 (el 1 es la CPU) */
	int current;
	int first, second;
	int score[2];
	int moves;          /* intentos (parejas levantadas) */
	int memory;         /* % de cartas que recuerda el rival */
	bool last_match;
	PjPhase phase;
	Rng rng;
} Parejas;

void pj_init(Parejas* g, int pairs, int players, int memory, uint32_t seed);
/* Levanta la carta i (en PJ_PICK1 o PJ_PICK2) */
bool pj_flip(Parejas* g, int i);
/* Tras enseñar las dos cartas (PJ_SHOW): se las lleva o se tapan */
void pj_resolve(Parejas* g);
bool pj_same(Card a, Card b);
int pj_ai_pick(Parejas* g);
int pj_winner(const Parejas* g); /* -1 empate */

#endif
