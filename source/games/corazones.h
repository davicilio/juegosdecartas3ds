#ifndef GAMES_CORAZONES_H
#define GAMES_CORAZONES_H

#include <stdbool.h>

#include "games/poker.h" /* PkCard */

/* Corazones (Hearts), para cuatro jugadores con la baraja francesa (el 0 es
 * el humano). Palos: 0 picas, 1 corazones, 2 diamantes, 3 tréboles.
 *
 * Trece cartas cada uno. Antes de cada mano se pasan tres cartas: a la
 * izquierda, a la derecha, al de enfrente y, la cuarta mano, no se pasan. Sale
 * el 2 de tréboles. Hay que asistir al palo; quien gana la baza sale en la
 * siguiente. Cada corazón vale 1 punto y la reina de picas, 13. No se puede
 * salir con corazones hasta que alguien haya echado uno (salvo que no quede
 * otra cosa), y en la primera baza no se pueden echar corazones ni la reina
 * de picas si se tiene otra carta. Quien se lleva todos los puntos de una
 * mano ("dispara a la luna") se queda a 0 y los demás suman 26. La partida
 * acaba cuando alguien llega a 100: gana quien tenga menos puntos. */

#define HE_PLAYERS 4
#define HE_HAND    13
#define HE_END     100

typedef enum {
	HE_PASSING,     /* eligiendo las tres cartas para pasar */
	HE_PLAYING,
	HE_TRICK_DONE,  /* baza completa: falta recogerla */
	HE_HAND_OVER,
	HE_GAME_OVER
} HePhase;

typedef struct {
	PkCard hands[HE_PLAYERS][HE_HAND];
	int hand_count[HE_PLAYERS];
	PkCard trick[HE_PLAYERS];
	bool trick_played[HE_PLAYERS];
	int played_in_trick;
	int leader, current, trick_winner;
	int tricks;
	bool hearts_broken;
	int points[HE_PLAYERS]; /* de esta mano */
	int score[HE_PLAYERS];  /* de la partida */
	int hand_number;
	int moon;               /* quien ha disparado a la luna (-1 nadie) */
	HePhase phase;
	Rng rng;
} Corazones;

int he_card_points(PkCard c);
/* Dirección del pase de esta mano: 1 izquierda, -1 derecha, 2 enfrente, 0 ninguno */
int he_pass_direction(const Corazones* g);

void he_init(Corazones* g, uint32_t seed);
void he_new_hand(Corazones* g);
/* Pasan todos: el humano las cartas idx (3), la IA las suyas */
bool he_pass(Corazones* g, const int* idx);
bool he_can_play(const Corazones* g, int player, int index);
bool he_play(Corazones* g, int index);
bool he_collect(Corazones* g);
int he_ai_choose(const Corazones* g, int player);
/* Las tres cartas que pasaría la IA */
void he_ai_pass(const Corazones* g, int player, int* idx);
int he_winner(const Corazones* g);

#endif
