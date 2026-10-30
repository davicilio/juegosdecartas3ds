#ifndef GAMES_POKER_H
#define GAMES_POKER_H

#include <stdbool.h>
#include <stdint.h>

#include "core/rng.h"

/* Póker Texas Hold'em para cuatro jugadores (el 0 es el humano), con la
 * baraja francesa de 52 cartas.
 *
 * Cada jugador recibe dos cartas tapadas y en la mesa salen, por fases, tres
 * cartas (flop), una (turn) y otra (river), con una ronda de apuestas antes y
 * después de cada una. Antes de repartir, los dos jugadores a la izquierda
 * del que reparte ponen la ciega pequeña y la grande. En cada ronda se puede
 * pasar (si nadie ha apostado), igualar, subir o retirarse. Gana el bote la
 * mejor jugada de cinco cartas entre las siete (las dos propias y las cinco
 * de la mesa), o el último que no se ha retirado. Con jugadores sin fichas
 * (all-in) se hacen botes secundarios.
 *
 * Jugadas, de menor a mayor: carta alta, pareja, doble pareja, trío,
 * escalera, color, full, póker y escalera de color.
 *
 * OMAHA: igual, pero con cuatro cartas propias, y la jugada se hace con
 * exactamente dos de ellas y tres de la mesa.
 *
 * DESCARTE (el póker clásico de cinco cartas): cinco cartas propias y nada en
 * la mesa. Tras una ronda de apuestas, cada uno cambia hasta tres cartas
 * (cuatro si se queda un as) y hay otra ronda antes de enseñar. */

#define PK_PLAYERS  4
#define PK_START    1000
#define PK_SMALL    10
#define PK_BIG      20
#define PK_BOARD    5
#define PK_MAX_HOLE 5

typedef enum { PK_HOLDEM, PK_OMAHA, PK_DRAW, PK_VARIANTS } PkVariant;

/* Carta: valor 2..14 (14 = as) y palo 0..3 (picas, corazones, diamantes, tréboles) */
typedef struct {
	uint8_t rank;
	uint8_t suit;
} PkCard;

typedef enum {
	PK_PREFLOP,
	PK_FLOP,
	PK_TURN,
	PK_RIVER,
	PK_DISCARD,   /* póker de descarte: cada uno cambia cartas */
	PK_SHOWDOWN,  /* se enseñan las cartas y se reparte el bote */
	PK_HAND_OVER, /* todos menos uno se han retirado */
	PK_GAME_OVER
} PkStage;

typedef enum {
	PK_FOLD,
	PK_CHECK_CALL,
	PK_RAISE,
	PK_ALL_IN
} PkAction;

typedef struct {
	PkCard deck[52];
	int deck_count;
	PkCard hole[PK_PLAYERS][PK_MAX_HOLE];
	int variant;               /* PkVariant */
	int hole_n;                /* cartas propias: 2, 4 o 5 */
	bool drew[PK_PLAYERS];     /* descarte: ya ha cambiado */
	int discards[PK_PLAYERS];  /* descarte: cuántas cambió */
	PkCard board[PK_BOARD];
	int board_count;
	int chips[PK_PLAYERS];
	int bet[PK_PLAYERS];       /* apostado en esta ronda */
	int invested[PK_PLAYERS];  /* apostado en toda la mano */
	bool folded[PK_PLAYERS];
	bool acted[PK_PLAYERS];    /* ha actuado desde la última subida */
	bool out[PK_PLAYERS];      /* eliminado (sin fichas) */
	int dealer;
	int current;
	int to_call;               /* apuesta más alta de la ronda */
	int min_raise;
	PkStage stage;
	/* Resultado de la mano */
	int won[PK_PLAYERS];       /* fichas ganadas en el reparto */
	uint32_t score[PK_PLAYERS];/* valor de la mejor jugada (en el showdown) */
	int last_action[PK_PLAYERS]; /* PkAction o -1, para la interfaz */
	int last_amount[PK_PLAYERS];
	int hands_played;
	Rng rng;
} Poker;

/* Partida acabada: te has quedado sin fichas o ya no quedan rivales */
static inline bool pk_game_over(const Poker* g)
{
	return g->stage == PK_GAME_OVER;
}

/* Valor de la mejor jugada de 5 entre n cartas (5..7): cuanto mayor, mejor. */
uint32_t pk_evaluate(const PkCard* cards, int n);
/* Categoría de una jugada (0 carta alta ... 8 escalera de color) y su nombre */
int pk_category(uint32_t score);
const char* pk_category_name(int category);

void pk_init(Poker* g, uint32_t seed);
void pk_init_variant(Poker* g, PkVariant variant, uint32_t seed);
/* Igual, pero tú te sientas con my_chips fichas (las que traes del monedero) */
void pk_init_stack(Poker* g, PkVariant variant, uint32_t seed, int my_chips);
/* Mejor jugada del jugador con lo que hay ahora en la mesa */
uint32_t pk_player_score(const Poker* g, int player);
/* Descarte: el jugador actual cambia las cartas marcadas */
bool pk_can_discard(const Poker* g, const bool* sel);
bool pk_discard(Poker* g, const bool* sel);
void pk_ai_discard(const Poker* g, int player, bool* sel);
void pk_new_hand(Poker* g);
int pk_pot(const Poker* g);
int pk_to_call(const Poker* g, int player);
bool pk_active(const Poker* g, int player); /* en la mano y sin retirarse */
/* Mínimo y máximo de la subida (lo que pone en total en la ronda) */
int pk_min_raise_to(const Poker* g);
int pk_max_raise_to(const Poker* g);
bool pk_can_check(const Poker* g);
/* El jugador actual actúa. Para subir, amount es el total al que sube en la ronda. */
bool pk_act(Poker* g, PkAction action, int amount);
/* Tras el showdown o el final de la mano */
bool pk_hand_finished(const Poker* g);
int pk_players_left(const Poker* g);

/* IA: decide la acción del jugador actual (y la cantidad si sube). */
PkAction pk_ai_decide(Poker* g, int* amount);
/* Probabilidad (0..100) estimada de ganar con las cartas del jugador. */
int pk_equity(Poker* g, int player, int samples);

#endif
