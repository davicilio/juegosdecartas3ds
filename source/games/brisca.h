#ifndef GAMES_BRISCA_H
#define GAMES_BRISCA_H

#include <stdbool.h>

#include "core/cards.h"

/* Brisca a dos jugadores (el 0 es el humano, el 1 la CPU).
 *
 * Cada jugador recibe tres cartas y la siguiente se pone boca arriba: es la
 * muestra, que marca el palo de triunfo y se queda debajo del mazo (será la
 * última carta en robarse). Por turnos se juega una carta cada uno; no hay
 * obligación de asistir. Gana la baza el triunfo más alto o, si no hay
 * triunfos, la carta más alta del palo de salida. Orden de mayor a menor: as,
 * tres, rey, caballo, sota, 7, 6, 5, 4, 2. Puntos: as 11, tres 10, rey 4,
 * caballo 3 y sota 2 (120 en total). Tras cada baza roba primero el ganador,
 * que sale en la siguiente. Gana quien pase de 60 puntos.
 *
 * Quien tenga el 7 de triunfo puede cambiarlo por la muestra si esta es un as,
 * un tres o una figura; si la muestra es más baja, se cambia con el 2. */

#define BR_PLAYERS   2
#define BR_HAND_SIZE 3
#define BR_TOTAL_POINTS 120

typedef enum {
	BR_PHASE_PLAYING,
	BR_PHASE_TRICK_DONE, /* las dos cartas están en la mesa: falta recogerlas */
	BR_PHASE_GAME_OVER
} BrPhase;

typedef struct {
	Deck deck; /* deck.cards[0] es la muestra mientras quede en el mazo */
	Card trump_card;
	int trump; /* Suit */
	Card hands[BR_PLAYERS][BR_HAND_SIZE];
	int hand_count[BR_PLAYERS];
	Card trick[BR_PLAYERS];
	bool trick_played[BR_PLAYERS];
	int leader;  /* quien ha salido en la baza actual */
	int current; /* a quien le toca jugar */
	int trick_winner;
	int trick_points;
	int points[BR_PLAYERS];
	int won_cards[BR_PLAYERS];
	int games_won[BR_PLAYERS];
	int tricks;
	int first_leader; /* quien salió en la primera baza; alterna cada partida */
	bool swapped;     /* se acaba de cambiar la muestra (para la interfaz) */
	BrPhase phase;
	Rng rng;
} Brisca;

int br_card_points(Card c);
/* Fuerza de la carta dentro de su palo: as 10 ... 2 1. */
int br_card_strength(Card c);
/* Devuelve true si la carta follow gana a lead con el triunfo dado. */
bool br_beats(Card follow, Card lead, int trump);

void br_init(Brisca* g, uint32_t seed);
void br_new_game(Brisca* g);
bool br_play(Brisca* g, int hand_index);
/* Recoge la baza terminada: suma puntos, roban ambos y sale el ganador. */
bool br_collect(Brisca* g);
/* Índice en la mano de la carta con la que se puede cambiar la muestra, o -1. */
int br_swap_index(const Brisca* g, int player);
bool br_swap(Brisca* g, int player);
int br_ai_choose(const Brisca* g, int player);
/* 0 o 1 si alguien ha ganado la partida, -1 en caso de empate a 60. */
int br_game_winner(const Brisca* g);

#endif
