#ifndef GAMES_MENTIROSO_H
#define GAMES_MENTIROSO_H

#include <stdbool.h>

#include "core/cards.h"

/* Mentiroso, para cuatro jugadores (el 0 es el humano).
 *
 * Se reparten las 40 cartas. Quien empieza echa boca abajo de una a tres
 * cartas y dice qué son ("dos reyes"). Los siguientes tienen que echar
 * también de una a tres cartas diciendo que son de ese mismo número o figura
 * (sea verdad o no), o decir "¡mentiroso!" a quien acaba de echar. Entonces
 * se levantan sus cartas: si alguna no era lo que dijo, se lleva todo el
 * montón; si decía la verdad, se lo lleva quien le ha acusado. Quien gana la
 * acusación empieza de nuevo con el número que quiera. Gana el primero que se
 * queda sin cartas (si nadie le pilla mintiendo en la última jugada). */

#define MEN_PLAYERS 4
#define MEN_MAX_PLAY 3

typedef enum {
	MEN_PHASE_PLAYING,
	MEN_PHASE_REVEAL, /* se acaba de resolver una acusación */
	MEN_PHASE_GAME_OVER
} MenPhase;

typedef struct {
	Card hands[MEN_PLAYERS][DECK_SIZE];
	int hand_count[MEN_PLAYERS];
	Card pile[DECK_SIZE];
	int pile_count;
	int rank;          /* número o figura que se está diciendo (0 al empezar) */
	int declared;      /* cartas declaradas de ese número en esta tanda */
	Card last[MEN_MAX_PLAY]; /* la última jugada */
	int last_n;
	int last_player;   /* -1 si no hay jugada que acusar */
	int current;
	int winner;
	/* Resultado de la última acusación */
	int accuser, accused, taker;
	bool was_lie;
	MenPhase phase;
	Rng rng;
} Mentiroso;

void men_init(Mentiroso* g, uint32_t seed);
void men_new_game(Mentiroso* g);
/* Echa las cartas idx diciendo que son rank (solo cuenta si empieza la tanda). */
bool men_play(Mentiroso* g, const int* idx, int n, int rank);
bool men_can_call(const Mentiroso* g);
/* El jugador actual acusa al anterior */
bool men_call(Mentiroso* g);
/* Tras ver la acusación, se sigue jugando */
void men_continue(Mentiroso* g);
/* IA: true si acusa; si no, rellena la jugada */
bool men_ai_turn(Mentiroso* g, int* idx, int* n, int* rank);

#endif
