#ifndef GAMES_ESCOBA_H
#define GAMES_ESCOBA_H

#include <stdbool.h>

#include "core/cards.h"

/* Escoba del 15 a dos jugadores (el 0 es el humano, el 1 la CPU).
 *
 * Las cartas valen su número, salvo sota 8, caballo 9 y rey 10. Se ponen cuatro
 * cartas en la mesa y se reparten tres a cada jugador. Por turnos se juega una
 * carta: si junto con una o varias cartas de la mesa suma 15, el jugador se
 * las lleva (es obligatorio cuando se puede); si no, la carta se queda en la
 * mesa. Llevarse todas las cartas de la mesa es una "escoba". Cuando se acaban
 * las manos se reparten otras tres, hasta agotar el mazo; las cartas que
 * queden en la mesa se las lleva el último que haya recogido.
 *
 * Puntos de cada ronda: 1 por escoba, 1 por tener más cartas, 1 por tener
 * más oros, 1 por el siete de oros y 1 por tener más sietes. Gana la partida
 * quien llegue antes a ES_GAME_POINTS. */

#define ES_PLAYERS     2
#define ES_HAND_SIZE   3
#define ES_SUM         15
#define ES_GAME_POINTS 21
#define ES_MAX_OPTIONS 64

typedef enum {
	ES_PHASE_PLAYING,
	ES_PHASE_ROUND_OVER,
	ES_PHASE_GAME_OVER
} EsPhase;

typedef struct {
	int escobas;
	int cards;
	int oros;
	int sietes;
	bool siete_oros;
	/* puntos obtenidos por cada concepto */
	int pt_cards, pt_oros, pt_siete_oros, pt_sietes;
	int total;
} EsRoundScore;

typedef struct {
	Deck deck;
	Card hands[ES_PLAYERS][ES_HAND_SIZE];
	int hand_count[ES_PLAYERS];
	Card table[DECK_SIZE];
	int table_count;
	Card won[ES_PLAYERS][DECK_SIZE];
	int won_count[ES_PLAYERS];
	int escobas[ES_PLAYERS];
	int score[ES_PLAYERS];  /* puntos de la partida */
	EsRoundScore round_score[ES_PLAYERS];
	int dealer;
	int current;
	int last_capturer;
	int round;
	int turns;
	/* Última jugada, para la interfaz */
	int last_player;
	Card last_card;
	Card last_captured[DECK_SIZE];
	int last_captured_count;
	bool last_escoba;
	int deal_escobas; /* escobas del repartidor al poner la mesa */
	int deals;        /* repartos de la ronda (para animarlos) */
	EsPhase phase;
	Rng rng;
} Escoba;

int es_card_value(Card c);
/* Combinaciones de cartas de la mesa (máscaras de bits sobre table[]) que con
 * la carta c suman 15. Devuelve cuántas hay (como mucho max). */
int es_find_captures(const Escoba* g, Card c, uint32_t* masks, int max);

void es_init(Escoba* g, uint32_t seed);
void es_start_round(Escoba* g);
/* Juega la carta hand_index del jugador actual llevándose las cartas de mask.
 * Si la carta puede capturar, mask debe ser una de las combinaciones válidas;
 * si no puede, mask debe ser 0. */
bool es_play(Escoba* g, int hand_index, uint32_t mask);
/* La IA elige carta y combinación. Devuelve el índice de la carta. */
int es_ai_choose(const Escoba* g, int player, uint32_t* mask_out);
bool es_next_round(Escoba* g);
/* Ganador de la partida (o -1 si aún no ha terminado). */
int es_winner(const Escoba* g);

#endif
