#ifndef GAMES_BUTIFARRA_H
#define GAMES_BUTIFARRA_H

#include <stdbool.h>

#include "core/cards.h"

/* Butifarra, por parejas (0 y 2 contra 1 y 3), con la baraja de 48 cartas.
 * Se juega en sentido 0 -> 1 -> 2 -> 3 y el 0 es el humano.
 *
 * Doce cartas a cada uno. Quien reparte elige el triunfo (un palo) o
 * "butifarra" (sin triunfo, todo vale el doble), o lo delega en su
 * compañero, que ya tiene que elegir. Después los rivales pueden "contrar"
 * (x2), los que eligieron "recontrar" (x4) y los rivales cantar "Sant
 * Vicenç" (x8). Sale el de la derecha de quien reparte.
 *
 * Orden: nueve (la manilla), as, rey, caballo, sota, 8, 7 ... 2. Puntos:
 * manilla 5, as 4, rey 3, caballo 2, sota 1 y un punto por baza: 72 en
 * total. Hay que asistir al palo y, si la baza la van ganando los rivales,
 * matar si se puede; sin cartas del palo, fallar (y montar al fallo de los
 * rivales). Si la va ganando el compañero, no hace falta.
 *
 * La pareja que pasa de 36 se apunta lo que pasa, multiplicado. Gana la
 * primera pareja que llega a BU_GAME. */

#define BU_PLAYERS 4
#define BU_HAND    12
#define BU_DECK    48
#define BU_GAME    101

enum { BU_BUTIFARRA = 4, BU_DELEGATE = 5 };

typedef enum {
	BU_CHOOSE,  /* current elige triunfo, butifarra o delega */
	BU_DOUBLE,  /* current decide si contra, recontra o canta Sant Vicenç */
	BU_PLAYING,
	BU_TRICK_DONE,
	BU_HAND_OVER,
	BU_GAME_OVER
} BuPhase;

typedef struct {
	Card hands[BU_PLAYERS][BU_HAND];
	int hand_count[BU_PLAYERS];
	int dealer;
	int chooser;      /* quién ha elegido (quien reparte o su compañero) */
	bool delegated;
	int trump;        /* palo, o -1 en butifarra */
	bool butifarra;
	int doubles;      /* 0 nada, 1 contrada, 2 recontrada, 3 Sant Vicenç */
	int current;
	Card trick[BU_PLAYERS];
	bool trick_played[BU_PLAYERS];
	int played_in_trick;
	int leader;
	int trick_winner;
	int tricks_done;
	int points[2];    /* de cada pareja en esta mano */
	int score[2];
	int hand_gain[2]; /* lo que se ha apuntado cada pareja esta mano */
	int last_double_player;
	bool last_double;
	BuPhase phase;
	Rng rng;
} Butifarra;

int bu_team(int player);
int bu_card_points(Card c);
int bu_strength(Card c);
bool bu_beats(const Butifarra* g, Card challenger, Card best);
int bu_multiplier(const Butifarra* g);

void bu_init(Butifarra* g, uint32_t seed);
void bu_new_game(Butifarra* g);
void bu_new_hand(Butifarra* g);
bool bu_can_delegate(const Butifarra* g);
bool bu_choose(Butifarra* g, int choice);
bool bu_double(Butifarra* g, bool yes);
int bu_trick_leader_now(const Butifarra* g);
bool bu_can_play(const Butifarra* g, int player, int idx);
bool bu_play(Butifarra* g, int idx);
bool bu_collect(Butifarra* g);
int bu_winner(const Butifarra* g);

int bu_ai_choose(const Butifarra* g, int player);
bool bu_ai_double(const Butifarra* g, int player);
int bu_ai_play(const Butifarra* g, int player);

#endif
