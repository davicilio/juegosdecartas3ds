#ifndef GAMES_CULO_H
#define GAMES_CULO_H

#include <stdbool.h>

#include "core/cards.h"

/* Culo (o presidente), para cuatro jugadores (el 0 es el humano).
 *
 * Orden de las cartas, de menor a mayor: 3, 4, 5, 6, 7, sota, caballo, rey,
 * as y 2. Se reparten las 40 cartas. Quien sale echa de una a cuatro cartas
 * iguales; los siguientes tienen que echar el mismo número de cartas, pero
 * más altas, o pasar. Cuando todos pasan, quien echó la última jugada recoge
 * y sale con lo que quiera. El primero que se queda sin cartas es el
 * presidente; después, el vicepresidente, el vicecu... y el último, el culo.
 *
 * En la ronda siguiente el culo le da sus dos mejores cartas al presidente y
 * recibe las dos peores de este; el vicecu le da una al vicepresidente. */

#define CULO_PLAYERS 4
#define CULO_MAX_PLAY 4

typedef enum {
	CULO_PHASE_PLAYING,
	CULO_PHASE_ROUND_OVER
} CuloPhase;

typedef struct {
	Card hands[CULO_PLAYERS][DECK_SIZE];
	int hand_count[CULO_PLAYERS];
	Card pile[CULO_MAX_PLAY]; /* la jugada que hay que superar */
	int pile_n;               /* 0: la mesa está libre */
	int pile_owner;
	int current;
	int passes;               /* pases seguidos desde la última jugada */
	int finish[CULO_PLAYERS]; /* orden de llegada: finish[0] es el presidente */
	int finished;
	int role[CULO_PLAYERS];   /* puesto en la ronda anterior (0 presidente ... 3 culo), -1 al empezar */
	int rounds;
	int presidencies[CULO_PLAYERS];
	CuloPhase phase;
	Rng rng;
} Culo;

/* Valor de la carta en el culo: 3 -> 0 ... 2 -> 9 */
int culo_value(Card c);
void culo_init(Culo* g, uint32_t seed);
/* Nueva ronda (con el cambio de cartas si ya hubo una antes) */
void culo_new_round(Culo* g);
bool culo_done(const Culo* g, int player);
bool culo_can_play(const Culo* g, const int* idx, int n);
bool culo_play(Culo* g, const int* idx, int n);
bool culo_can_pass(const Culo* g);
bool culo_pass(Culo* g);
/* IA: rellena la jugada; devuelve false si pasa */
bool culo_ai_choose(Culo* g, int* idx, int* n);
const char* culo_role_name(int role);

#endif
