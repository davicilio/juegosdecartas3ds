#ifndef GAMES_MIERDA_H
#define GAMES_MIERDA_H

#include <stdbool.h>

#include "core/cards.h"

/* La mierda, para cuatro jugadores (el 0 es el humano).
 *
 * Cada uno recibe tres cartas boca abajo, tres boca arriba encima de ellas y
 * tres en la mano; las demás forman el mazo. Por turnos se echan al montón
 * una o varias cartas iguales que valgan al menos lo mismo que la de arriba
 * (orden: 3, 4, 5, 6, 7, caballo, rey, as). Mientras quede mazo se roba hasta
 * tener tres en la mano. Quien no puede echar se lleva el montón.
 *   - El 2 se echa sobre cualquier carta y el siguiente puede echar lo que
 *     quiera.
 *   - La sota se echa sobre cualquier carta y quema el montón (se retira);
 *     también se quema al juntarse cuatro cartas iguales seguidas. Tras quemar,
 *     vuelve a echar el mismo.
 * Sin cartas en la mano se juegan las de boca arriba y, después, las de boca
 * abajo a ciegas: si la que sale no vale, te llevas el montón con ella. El
 * último que se queda con cartas es la mierda. */

#define MI_PLAYERS 4
#define MI_MAX     DECK_SIZE

typedef enum { MI_FROM_HAND, MI_FROM_UP, MI_FROM_DOWN, MI_FROM_NONE } MiSource;

typedef enum { MI_PLAYING, MI_GAME_OVER } MiPhase;

typedef struct {
	Card hand[MI_PLAYERS][MI_MAX];
	int hand_n[MI_PLAYERS];
	Card up[MI_PLAYERS][3];
	bool up_has[MI_PLAYERS][3];
	Card down[MI_PLAYERS][3];
	bool down_has[MI_PLAYERS][3];
	Card deck[DECK_SIZE];
	int deck_n;
	Card pile[DECK_SIZE];
	int pile_n;
	int burned;
	int current;
	bool out[MI_PLAYERS];
	int out_order[MI_PLAYERS];
	int out_n;
	int loser;
	/* Lo último que ha pasado, para la interfaz */
	int last_player;
	Card last_card;
	int last_count;
	bool last_burn;
	bool last_took;      /* se llevó el montón */
	bool last_blind_fail; /* la de boca abajo no valía */
	MiPhase phase;
	Rng rng;
} Mierda;

int mi_value(Card c);
void mi_init(Mierda* g, uint32_t seed);
void mi_new_game(Mierda* g);
MiSource mi_source(const Mierda* g, int player);
bool mi_card_ok(const Mierda* g, Card c);
/* idx es la posición en la mano, en las de boca arriba o en las de boca abajo */
bool mi_can_play(const Mierda* g, int player, int idx);
/* Echa la carta idx y las iguales de la misma fuente */
bool mi_play(Mierda* g, int idx);
/* Llevarse el montón (solo si no se puede echar nada) */
bool mi_must_take(const Mierda* g, int player);
bool mi_take(Mierda* g);
/* IA: índice a echar, o -1 para llevarse el montón */
int mi_ai_choose(Mierda* g, int player);

#endif
