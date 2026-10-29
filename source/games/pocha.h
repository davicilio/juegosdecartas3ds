#ifndef GAMES_POCHA_H
#define GAMES_POCHA_H

#include <stdbool.h>

#include "core/cards.h"

/* Pocha, para cuatro jugadores (el 0 es el humano), cada uno para sí.
 *
 * Se juegan diez manos: en la primera se reparte una carta a cada uno, en la
 * segunda dos... y en la décima, diez. La carta siguiente al reparto (o, si no
 * quedan, la última repartida) marca el triunfo. Antes de jugar, cada uno
 * dice cuántas bazas va a hacer; el que reparte, el último en hablar, no
 * puede decir un número que haga que la suma sea igual a las cartas de la
 * mano (alguien tiene que fallar). Hay que asistir al palo y matar si se
 * puede; quien no tiene del palo tiene que fallar con triunfo (y por encima,
 * si ya han fallado). Orden: as, tres, rey, caballo, sota, 7, 6, 5, 4, 2.
 *
 * Quien acierta sus bazas suma 10 más 5 por baza; quien falla resta 5 por
 * cada baza de diferencia. Gana quien más puntos tiene al final. */

#define PO_PLAYERS 4
#define PO_ROUNDS  10

typedef enum {
	PO_BIDDING,
	PO_PLAYING,
	PO_TRICK_DONE,
	PO_ROUND_OVER,
	PO_GAME_OVER
} PoPhase;

typedef struct {
	Card hands[PO_PLAYERS][PO_ROUNDS];
	int hand_count[PO_PLAYERS];
	Card trump_card;
	int trump;
	int round;  /* 1..PO_ROUNDS: cartas de esta mano */
	int dealer;
	int bid[PO_PLAYERS]; /* -1 sin decir */
	int bids_made;
	int tricks_won[PO_PLAYERS];
	Card trick[PO_PLAYERS];
	int played_in_trick;
	int leader, current, trick_winner;
	int score[PO_PLAYERS];
	int round_points[PO_PLAYERS];
	PoPhase phase;
	Rng rng;
} Pocha;

int po_strength(Card c);
bool po_beats(const Pocha* g, Card challenger, Card best);

void po_init(Pocha* g, uint32_t seed);
void po_new_round(Pocha* g);
/* La apuesta prohibida para el que reparte (-1 si no hay) */
int po_forbidden_bid(const Pocha* g);
bool po_can_bid(const Pocha* g, int bid);
bool po_bid(Pocha* g, int bid);
bool po_can_play(const Pocha* g, int player, int index);
bool po_play(Pocha* g, int index);
bool po_collect(Pocha* g);
int po_ai_bid(const Pocha* g);
int po_ai_choose(const Pocha* g, int player);
int po_winner(const Pocha* g);

#endif
