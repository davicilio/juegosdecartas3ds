#ifndef GAMES_BURRO_H
#define GAMES_BURRO_H

#include <stdbool.h>

#include "core/cards.h"

/* El burro, para cuatro jugadores (el 0 es el humano).
 *
 * Se juega con cuatro números (las cuatro cartas de cada uno), es decir,
 * dieciséis cartas, y cada uno recibe cuatro. A la vez, todos pasan una carta
 * boca abajo al de su derecha y cogen la que les pasa el de su izquierda.
 * Quien junta las cuatro iguales pone la mano en la mesa y los demás tienen
 * que hacerlo en cuanto se den cuenta: el último se lleva una letra. Quien
 * completa B-U-R-R-O pierde la partida. Si alguien pone la mano sin tener
 * cuatro iguales (y nadie las tiene), se lleva él la letra. */

#define BUR_PLAYERS 4
#define BUR_HAND    4
#define BUR_WORD    5 /* B U R R O */

typedef enum {
	BUR_PASSING,   /* cada uno elige qué carta pasar */
	BUR_SLAP,      /* alguien ha puesto la mano: hay que reaccionar */
	BUR_ROUND_OVER,
	BUR_GAME_OVER
} BurPhase;

typedef struct {
	Card hands[BUR_PLAYERS][BUR_HAND];
	int letters[BUR_PLAYERS];
	bool slapped[BUR_PLAYERS];
	int slap_order[BUR_PLAYERS];
	int slaps;
	int round_loser;
	bool false_slap; /* la última ronda la perdió alguien por poner la mano sin tener */
	int passes;      /* intercambios en esta ronda */
	BurPhase phase;
	Rng rng;
} Burro;

void bur_init(Burro* g, uint32_t seed);
void bur_new_game(Burro* g);
void bur_new_round(Burro* g);
bool bur_has_four(const Burro* g, int player);
/* Alguien tiene cuatro iguales */
int bur_holder(const Burro* g);
/* Todos pasan a la vez: choice[p] es el índice de la carta que pasa p */
bool bur_pass(Burro* g, const int choice[BUR_PLAYERS]);
/* p pone la mano en la mesa */
bool bur_slap(Burro* g, int player);
int bur_loser(const Burro* g);
/* IA: carta a pasar (la del número que menos tiene) */
int bur_ai_pass(Burro* g, int player);

#endif
