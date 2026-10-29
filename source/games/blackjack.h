#ifndef GAMES_BLACKJACK_H
#define GAMES_BLACKJACK_H

#include <stdbool.h>

#include "games/poker.h" /* PkCard: valor 2..14 y palo */

/* Blackjack contra la banca, con un zapato de cuatro barajas francesas.
 *
 * Apuestas fichas y recibes dos cartas; la banca, una boca arriba y otra
 * tapada. Las cartas valen su número, las figuras 10 y el as 1 u 11. Pides
 * cartas para acercarte a 21 sin pasarte, o te plantas. También puedes
 * doblar (doblas la apuesta y recibes una sola carta) y separar dos cartas
 * iguales en dos manos. La banca pide hasta tener 17 o más. Blackjack (as y
 * carta de 10 de entrada) paga 3 a 2; ganar, 1 a 1; empatar devuelve la
 * apuesta. */

#define BJ_DECKS     4
#define BJ_SHOE      (52 * BJ_DECKS)
#define BJ_MAX_CARDS 12
#define BJ_START     1000
#define BJ_MIN_BET   10

typedef enum {
	BJ_BETTING,  /* eligiendo la apuesta */
	BJ_PLAYING,  /* el jugador juega su mano (o manos) */
	BJ_DEALER,   /* juega la banca */
	BJ_RESULT,   /* resultado de la mano */
	BJ_BROKE     /* sin fichas */
} BjPhase;

typedef struct {
	PkCard cards[BJ_MAX_CARDS];
	int n;
	int bet;
	bool doubled;
	bool done;
	int result; /* fichas ganadas o perdidas (para la interfaz) */
} BjHand;

typedef struct {
	PkCard shoe[BJ_SHOE];
	int shoe_count;
	BjHand hands[2]; /* la segunda solo al separar */
	int hand_count;
	int active;      /* mano que se está jugando */
	BjHand dealer;
	int chips;
	int bet;         /* apuesta elegida para la siguiente mano */
	int net;         /* resultado de la última mano */
	int hands_won, hands_played;
	BjPhase phase;
	Rng rng;
} Blackjack;

int bj_card_value(PkCard c);
/* Valor de una mano (el as cuenta 11 si no se pasa); soft = hay un as como 11 */
int bj_total(const BjHand* h, bool* soft);
bool bj_is_blackjack(const BjHand* h);

void bj_init(Blackjack* g, uint32_t seed);
void bj_set_bet(Blackjack* g, int bet);
/* Reparte con la apuesta elegida */
bool bj_deal(Blackjack* g);
bool bj_hit(Blackjack* g);
bool bj_stand(Blackjack* g);
bool bj_can_double(const Blackjack* g);
bool bj_double(Blackjack* g);
bool bj_can_split(const Blackjack* g);
bool bj_split(Blackjack* g);
/* La banca pide una carta (para animarla); devuelve false cuando acaba */
bool bj_dealer_step(Blackjack* g);
/* Nueva partida con las fichas iniciales */
void bj_reset(Blackjack* g);
/* Consejo de la estrategia básica: 'H' pedir, 'S' plantarse, 'D' doblar, 'P' separar */
char bj_advice(const Blackjack* g);

#endif
