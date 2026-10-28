#ifndef GAMES_MONA_H
#define GAMES_MONA_H

#include <stdbool.h>

#include "core/cards.h"

/* La mona, para cuatro jugadores (el 0 es el humano).
 *
 * Se quita de la baraja la sota de oros, así que una de las otras sotas se
 * queda sin pareja: es la mona. Se reparten todas las cartas y cada uno
 * echa a la mesa las parejas que tenga (dos cartas del mismo número o
 * figura). Por turnos, cada jugador roba una carta a ciegas al siguiente que
 * aún tenga cartas y, si hace pareja, la echa. Quien se queda sin cartas se
 * libra. Pierde el último, el que se queda con la mona. */

#define MONA_PLAYERS 4

typedef struct {
	Card hands[MONA_PLAYERS][DECK_SIZE];
	int hand_count[MONA_PLAYERS];
	int current;
	int pairs[MONA_PLAYERS]; /* parejas echadas */
	int out_order[MONA_PLAYERS]; /* quién se ha librado, por orden */
	int out_count;
	int loser; /* -1 mientras se juega */
	/* Última robada, para la interfaz */
	int last_from;
	Card last_card;
	bool last_paired;
	Rng rng;
} Mona;

void mona_init(Mona* g, uint32_t seed);
void mona_new_game(Mona* g);
bool mona_active(const Mona* g, int player);
/* A quién roba el jugador actual (el siguiente que tenga cartas) */
int mona_victim(const Mona* g);
/* Roba la carta index de la mano de la víctima */
bool mona_take(Mona* g, int index);
int mona_ai_pick(Mona* g);
bool mona_over(const Mona* g);

#endif
