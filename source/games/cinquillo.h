#ifndef GAMES_CINQUILLO_H
#define GAMES_CINQUILLO_H

#include <stdbool.h>

#include "core/cards.h"

/* Cinquillo para 4 jugadores (el 0 es el humano, el resto la CPU).
 *
 * Se reparten las 40 cartas. Empieza quien tiene el cinco de oros, que está
 * obligado a jugarlo. Después, por turnos (en sentido antihorario), cada
 * jugador debe colocar una carta si puede: un cinco abre un palo nuevo y en
 * los palos abiertos se sigue la escalera hacia abajo (4, 3, 2, 1) o hacia
 * arriba (6, 7, sota, caballo, rey). Si no puede jugar, pasa y pone una ficha
 * en el bote. Gana la ronda el primero que se queda sin cartas y se lleva el
 * bote. Al empezar cada ronda todos ponen una ficha. La partida termina cuando
 * alguien se queda sin fichas o tras CQ_MAX_ROUNDS rondas. */

#define CQ_PLAYERS      4
#define CQ_HAND_SIZE    (DECK_SIZE / CQ_PLAYERS)
#define CQ_START_CHIPS  20
#define CQ_ANTE         1
#define CQ_PASS_COST    1
#define CQ_MAX_ROUNDS   10

typedef enum {
	CQ_PHASE_PLAYING,
	CQ_PHASE_ROUND_OVER,
	CQ_PHASE_GAME_OVER
} CqPhase;

typedef enum {
	CQ_ACTION_NONE,
	CQ_ACTION_PLAY,
	CQ_ACTION_PASS
} CqAction;

typedef struct {
	Card hands[CQ_PLAYERS][CQ_HAND_SIZE];
	int hand_count[CQ_PLAYERS];
	/* Escalera de cada palo en la mesa: posiciones (card_pos) mínima y
	 * máxima jugadas, o -1 si el palo aún no se ha abierto. */
	int low[NUM_SUITS];
	int high[NUM_SUITS];
	int chips[CQ_PLAYERS];
	int paid[CQ_PLAYERS]; /* puesto en el plato esta ronda */
	int pot;
	int current;      /* jugador al que le toca */
	int round_winner; /* -1 mientras se juega */
	int round;        /* ronda actual, empezando en 1 */
	int turns;        /* jugadas o pases en la ronda actual */
	/* Última acción, para que la interfaz pueda anunciarla. */
	CqAction last_action;
	int last_player;
	Card last_card;
	CqPhase phase;
	Rng rng;
} Cinquillo;

void cq_init(Cinquillo* g, uint32_t seed);
/* Baraja, reparte y cobra la entrada de una nueva ronda. */
void cq_start_round(Cinquillo* g);
bool cq_is_legal(const Cinquillo* g, Card c);
bool cq_can_play(const Cinquillo* g, int player);
/* Índices dentro de la mano de las cartas jugables; devuelve cuántas hay. */
int cq_legal_moves(const Cinquillo* g, int player, int* out);
/* Juega la carta hand_index del jugador actual. */
bool cq_play(Cinquillo* g, int hand_index);
/* Pasa el turno (solo si no tiene jugada). */
bool cq_pass(Cinquillo* g);
/* La IA elige una carta (índice en la mano) o -1 si tiene que pasar. */
int cq_ai_choose(const Cinquillo* g, int player);
/* Pasa a la siguiente ronda tras CQ_PHASE_ROUND_OVER. */
bool cq_next_round(Cinquillo* g);
/* Jugador con más fichas (el primero en caso de empate). */
int cq_leader(const Cinquillo* g);
void cq_sort_hand(Cinquillo* g, int player);

#endif
