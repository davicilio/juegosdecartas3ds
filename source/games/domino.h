#ifndef GAMES_DOMINO_H
#define GAMES_DOMINO_H

#include <stdbool.h>
#include <stdint.h>

#include "core/rng.h"

/* Dominó por parejas, como se juega en los bares: el 0 (humano) y el 2
 * contra el 1 y el 3, que juegan en sentido 0 -> 1 -> 2 -> 3.
 *
 * 28 fichas del doble cero al doble seis, siete para cada uno. La primera
 * mano la empieza quien tiene el seis doble, con él; las siguientes, quien
 * ganó la anterior, con la ficha que quiera. Por turnos se pone una ficha que
 * case con uno de los dos extremos; quien no puede, pasa. La mano la gana la
 * pareja de quien se queda sin fichas ("dominó") y se apunta los puntos de
 * las fichas que les quedan a los rivales. Si nadie puede poner (la mano se
 * "cierra"), gana la pareja que tenga menos puntos y se apunta los de la
 * otra. Gana la partida la pareja que llega a 100. */

#define DO_PLAYERS 4
#define DO_TILES   28
#define DO_HAND    7
#define DO_GAME    100

typedef struct {
	uint8_t a, b;
} DoTile;

typedef enum {
	DO_PLAYING,
	DO_HAND_OVER,
	DO_GAME_OVER
} DoPhase;

typedef struct {
	DoTile hands[DO_PLAYERS][DO_HAND];
	int hand_count[DO_PLAYERS];
	/* La cadena de la mesa, de izquierda a derecha, con cada ficha orientada
	 * (a es su lado izquierdo y b el derecho) */
	DoTile chain[DO_TILES];
	int chain_count;
	int current;
	int passes;        /* pases seguidos */
	int score[2];
	int hand_winner;   /* pareja que ha ganado la mano (-1 sin acabar) */
	int hand_points;
	bool blocked;      /* la mano se ha cerrado */
	int domino_player; /* quien ha hecho dominó (-1 si se cerró) */
	int starter;       /* quien empieza la siguiente mano */
	bool first_hand;
	int last_player;   /* para la interfaz */
	bool last_passed;
	DoPhase phase;
	Rng rng;
} Domino;

int do_left(const Domino* g);
int do_right(const Domino* g);
void do_init(Domino* g, uint32_t seed);
void do_new_hand(Domino* g);
/* ¿Se puede poner la ficha idx del jugador actual en ese lado (0 izq, 1 der)? */
bool do_can_play(const Domino* g, int idx, int side);
bool do_has_move(const Domino* g, int player);
bool do_play(Domino* g, int idx, int side);
bool do_pass(Domino* g);
/* IA: ficha y lado, o false si pasa */
bool do_ai_choose(const Domino* g, int* idx, int* side);
int do_pips(const Domino* g, int player);

#endif
