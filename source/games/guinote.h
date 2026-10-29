#ifndef GAMES_GUINOTE_H
#define GAMES_GUINOTE_H

#include <stdbool.h>

#include "core/cards.h"

/* Juegos de bazas con cantes para cuatro jugadores: el guiñote, el tute por
 * parejas y el tute cabrón. El jugador 0 es el humano y se juega en sentido
 * 0 -> 1 -> 2 -> 3. Por parejas, el 0 y el 2 contra el 1 y el 3.
 *
 * GUIÑOTE. Seis cartas cada uno y la siguiente boca arriba: la muestra, que
 * marca el triunfo y se roba la última. Orden: as, tres, rey, sota, caballo,
 * 7, 6, 5, 4, 2. Puntos: as 11, tres 10, rey 4, sota 3, caballo 2, más 10 de
 * últimas. Mientras queda mazo se juega cualquier carta y tras cada baza
 * roban todos empezando por el ganador. La pareja que gana una baza puede
 * cantar rey y sota del mismo palo (20, o 40 si son de triunfo) y cambiar el
 * 7 de triunfo por la muestra. Al acabarse el mazo empieza el arrastre (ver
 * abajo) y ya no se canta. Gana la pareja que llega a 101 al acabar una mano;
 * si nadie llega, se juega otra mano "de vueltas" sumando los puntos.
 *
 * TUTE (por parejas) y TUTE CABRÓN (cada uno para sí). Se reparten las 40
 * cartas, diez a cada uno, y la última (la de quien reparte) marca el
 * triunfo. Orden: as, tres, rey, caballo, sota, 7 ... 2. Puntos: as 11, tres
 * 10, rey 4, caballo 3, sota 2, más 10 de últimas. Se juega con las reglas
 * del arrastre desde la primera baza. Quien gana una baza (o su pareja)
 * puede cantar rey y caballo del mismo palo (20 o 40); con los cuatro reyes o
 * los cuatro caballos se canta el tute y se gana la mano. Cada mano la gana
 * quien suma más puntos; gana la partida quien gana tres manos.
 *
 * TUTE SUBASTADO. Como el tute por parejas, pero sin pinta: antes de jugar
 * se puja por los puntos (cartas, cantes y diez de últimas) que hará la
 * pareja, desde GN_MIN_BID y de diez en diez; quien pasa ya no vuelve a
 * pujar y, si pasan los tres primeros, el último tiene que pujar. Quien gana
 * la subasta elige el triunfo y sale. Si su pareja llega a lo pujado se lo
 * apunta; si no, se lo resta. Gana la pareja que llega a GN_SUB_POINTS.
 *
 * ARRASTRE: hay que asistir al palo y ganar la baza si se puede; quien no
 * tiene del palo debe fallar (con triunfo, y por encima si ya se ha fallado).
 * Por parejas, si la baza la va ganando el compañero no hay obligación de
 * ganarla ni de fallar. */

#define GN_PLAYERS     4
#define GN_MAX_HAND    10
#define GN_GAME_POINTS 101 /* guiñote */
#define GN_TUTE_HANDS  3   /* tute: manos para ganar la partida */
#define GN_LAST_TRICK  10
#define GN_MIN_BID     70  /* tute subastado */
#define GN_BID_STEP    10
#define GN_MAX_BID     230
#define GN_SUB_POINTS  300

typedef enum {
	GN_GUINOTE,
	GN_TUTE,
	GN_TUTE_CABRON,
	GN_TUTE_SUBASTADO,
	GN_VARIANTS
} GnVariant;

typedef enum {
	GN_PHASE_PLAYING,
	GN_PHASE_TRICK_DONE, /* las cuatro cartas están en la mesa: falta recogerlas */
	GN_PHASE_HAND_OVER,  /* se ha acabado una mano, pero no la partida */
	GN_PHASE_GAME_OVER,
	GN_PHASE_BIDDING,      /* tute subastado: puja el jugador current */
	GN_PHASE_CHOOSE_TRUMP  /* tute subastado: el que ganó la subasta elige el triunfo */
} GnPhase;

typedef struct {
	GnVariant variant;
	Deck deck; /* guiñote: deck.cards[0] es la muestra mientras quede en el mazo */
	Card trump_card;
	int trump; /* Suit */
	Card hands[GN_PLAYERS][GN_MAX_HAND];
	int hand_count[GN_PLAYERS];
	Card trick[GN_PLAYERS];
	bool trick_played[GN_PLAYERS];
	int played_in_trick;
	int leader;
	int current;
	int trick_winner;
	int trick_points;
	int last_trick_side; /* bando que ganó la última baza recogida (-1 al empezar) */
	/* Bandos: las parejas 0 y 1 o, en el tute cabrón, cada jugador */
	int points[GN_PLAYERS];    /* puntos de cada bando en esta mano */
	int score[GN_PLAYERS];     /* guiñote: puntos acumulados; tute: manos ganadas */
	int games_won[GN_PLAYERS];
	int hand_winner;           /* bando que ha ganado la última mano */
	bool tute;                 /* la mano se ha acabado por cantar el tute */
	bool sung[NUM_SUITS];      /* palos ya cantados en esta mano */
	int last_cante_player;     /* para la interfaz: quién cantó y qué */
	int last_cante_suit;       /* -1 si fue el tute */
	int last_cante_points;
	bool swapped; /* se acaba de cambiar el 7 (para la interfaz) */
	int hands_played;
	int first_leader;
	/* Tute subastado */
	int bid;          /* puja más alta (0 si nadie ha pujado) */
	int bidder;       /* quién la tiene, o -1 */
	bool passed[GN_PLAYERS];
	int last_bid_player;
	int last_bid;     /* 0 = pasó */
	bool bid_made;    /* al acabar la mano: la pareja llegó a lo pujado */
	GnPhase phase;
	Rng rng;
} Guinote;

/* Número de bandos (2 por parejas, 4 en el tute cabrón) y bando de un jugador */
int gn_sides(const Guinote* g);
int gn_side(const Guinote* g, int player);
/* Cartas en la mano al empezar */
int gn_hand_size(const Guinote* g);

int gn_card_points(const Guinote* g, Card c);
/* Fuerza dentro del palo (el as es la más alta). */
int gn_card_strength(const Guinote* g, Card c);
/* true si challenger gana a best (que va ganando la baza). */
bool gn_beats(const Guinote* g, Card challenger, Card best);
/* true si hay que asistir, montar y fallar. */
bool gn_arrastre(const Guinote* g);
/* Figura que acompaña al rey en los cantes: la sota (guiñote) o el caballo. */
int gn_cante_rank(const Guinote* g);

void gn_init(Guinote* g, GnVariant variant, uint32_t seed);
/* Nueva partida (marcadores a 0). */
void gn_new_game(Guinote* g);
/* Siguiente mano de la partida. */
void gn_new_hand(Guinote* g);

/* Jugador que va ganando la baza en curso (-1 si no hay cartas). */
int gn_trick_leader_now(const Guinote* g);
bool gn_can_play(const Guinote* g, int player, int hand_index);
bool gn_play(Guinote* g, int hand_index);
/* Recoge la baza: suma puntos, roban todos (guiñote) y sale el ganador. */
bool gn_collect(Guinote* g);

/* Palo que puede cantar el jugador ahora (-1 si ninguno): hace falta que su
 * bando haya ganado la última baza, que sea su turno y tener la pareja del
 * palo sin cantar. Se prefiere el de triunfo. */
int gn_cante_suit(const Guinote* g, int player);
bool gn_cante(Guinote* g, int player);
/* Tute: tiene los cuatro reyes o los cuatro caballos y puede cantarlo. */
bool gn_can_sing_tute(const Guinote* g, int player);
bool gn_sing_tute(Guinote* g, int player);
/* Guiñote: índice del 7 de triunfo si puede cambiarlo por la muestra, o -1. */
int gn_swap_index(const Guinote* g, int player);
bool gn_swap(Guinote* g, int player);

int gn_ai_choose(const Guinote* g, int player);

/* Tute subastado */
int gn_min_bid(const Guinote* g);
/* Quien tiene que pujar no puede pasar (han pasado los otros tres sin pujar) */
bool gn_must_bid(const Guinote* g);
/* Puja amount (0 para pasar) */
bool gn_bid(Guinote* g, int amount);
bool gn_choose_trump(Guinote* g, int suit);
int gn_ai_bid(const Guinote* g, int player);
int gn_ai_trump(const Guinote* g, int player);
/* Bando ganador de la partida al acabar, o -1. */
int gn_game_winner(const Guinote* g);

#endif
