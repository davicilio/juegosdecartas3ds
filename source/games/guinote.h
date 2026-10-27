#ifndef GAMES_GUINOTE_H
#define GAMES_GUINOTE_H

#include <stdbool.h>

#include "core/cards.h"

/* Guiñote por parejas: el jugador 0 (humano) y el 2 contra el 1 y el 3. Se
 * juega en sentido 0 -> 1 -> 2 -> 3.
 *
 * Cada jugador recibe seis cartas y la siguiente se pone boca arriba: es la
 * muestra, que marca el triunfo y se roba la última. Orden de mayor a menor:
 * as, tres, rey, sota, caballo, 7, 6, 5, 4, 2. Puntos: as 11, tres 10, rey 4,
 * sota 3 y caballo 2 (120), más 10 "de últimas" para quien gana la última
 * baza.
 *
 * Mientras queda mazo no hay que asistir: se juega cualquier carta, y tras
 * cada baza roban todos empezando por el ganador, que sale en la siguiente.
 * La pareja que gana una baza puede cantar: rey y sota del mismo palo valen
 * 20 (las veinte) o 40 si son de triunfo (las cuarenta); cada palo se canta
 * una vez. También puede cambiar el 7 de triunfo por la muestra.
 *
 * Al acabarse el mazo empieza el arrastre: hay que asistir al palo y ganar la
 * baza si se puede; quien no tiene del palo debe fallar (con triunfo, y por
 * encima si ya se ha fallado). Si la baza la va ganando el compañero no hay
 * obligación de ganarla ni de fallar. En el arrastre ya no se canta.
 *
 * Gana la pareja que llega a 101 puntos al acabar una mano; si nadie llega,
 * se juega otra mano ("de vueltas") sumando los puntos. */

#define GN_PLAYERS    4
#define GN_HAND_SIZE  6
#define GN_GAME_POINTS 101
#define GN_LAST_TRICK 10

typedef enum {
	GN_PHASE_PLAYING,
	GN_PHASE_TRICK_DONE, /* las cuatro cartas están en la mesa: falta recogerlas */
	GN_PHASE_HAND_OVER,  /* se ha acabado una mano, pero no la partida */
	GN_PHASE_GAME_OVER
} GnPhase;

typedef struct {
	Deck deck; /* deck.cards[0] es la muestra mientras quede en el mazo */
	Card trump_card;
	int trump; /* Suit */
	Card hands[GN_PLAYERS][GN_HAND_SIZE];
	int hand_count[GN_PLAYERS];
	Card trick[GN_PLAYERS];
	bool trick_played[GN_PLAYERS];
	int played_in_trick;
	int leader;
	int current;
	int trick_winner;
	int trick_points;
	int last_trick_team; /* pareja que ganó la última baza recogida (-1 al empezar) */
	int points[2];       /* puntos de cada pareja en esta mano */
	int score[2];        /* acumulados de la partida (mano de vueltas) */
	int games_won[2];
	bool sung[NUM_SUITS]; /* palos ya cantados en esta mano */
	int last_cante_player; /* para la interfaz: quién cantó y qué */
	int last_cante_suit;
	int last_cante_points;
	bool swapped;          /* se acaba de cambiar el 7 (para la interfaz) */
	int hands_played;
	int first_leader;
	GnPhase phase;
	Rng rng;
} Guinote;

static inline int gn_team(int player)
{
	return player % 2;
}

int gn_card_points(Card c);
/* Fuerza dentro del palo: as 10, tres 9, rey 8, sota 7, caballo 6, 7 5 ... 2 1. */
int gn_card_strength(Card c);
/* true si challenger gana a best (que va ganando la baza) con el triunfo dado. */
bool gn_beats(Card challenger, Card best, int trump);
/* true en el arrastre (sin mazo): hay que asistir, montar y fallar. */
bool gn_arrastre(const Guinote* g);

void gn_init(Guinote* g, uint32_t seed);
/* Nueva partida (marcadores a 0). */
void gn_new_game(Guinote* g);
/* Siguiente mano de la partida (la de vueltas). */
void gn_new_hand(Guinote* g);

/* Jugador que va ganando la baza en curso (-1 si no hay cartas). */
int gn_trick_leader_now(const Guinote* g);
bool gn_can_play(const Guinote* g, int player, int hand_index);
bool gn_play(Guinote* g, int hand_index);
/* Recoge la baza: suma puntos, roban todos y sale el ganador. */
bool gn_collect(Guinote* g);

/* Palo que puede cantar el jugador ahora (-1 si ninguno): hace falta que su
 * pareja haya ganado la última baza, que quede mazo y tener rey y sota del
 * palo sin cantar. Se prefiere el de triunfo. */
int gn_cante_suit(const Guinote* g, int player);
bool gn_cante(Guinote* g, int player);
/* Índice del 7 de triunfo si el jugador puede cambiarlo por la muestra, o -1. */
int gn_swap_index(const Guinote* g, int player);
bool gn_swap(Guinote* g, int player);

int gn_ai_choose(const Guinote* g, int player);
/* Pareja ganadora de la partida (0 o 1) al acabar, o -1. */
int gn_game_winner(const Guinote* g);

#endif
