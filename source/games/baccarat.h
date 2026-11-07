#ifndef GAMES_BACCARAT_H
#define GAMES_BACCARAT_H

#include <stdbool.h>

#include "core/rng.h"
#include "games/poker.h" /* PkCard */

/* Punto y banca (baccarat), con un zapato de seis barajas. Se apuesta a la
 * mano del jugador (punto), a la de la banca o al empate. Cada mano recibe
 * dos cartas y, según las reglas fijas de la tercera carta, quizá una más.
 * Las cartas valen su número, las figuras y el 10 no valen nada y el as 1;
 * de la suma solo cuenta la última cifra. Gana la mano que se acerca más a
 * 9. Punto paga 1 a 1, banca 1 a 1 menos un 5 % de comisión y empate 8 a 1
 * (con empate, las apuestas a punto y banca se devuelven). */

#define BC_DECKS 6
#define BC_SHOE  (52 * BC_DECKS)

enum { BC_PLAYER, BC_BANKER, BC_TIE, BC_BETS };

typedef struct {
	PkCard shoe[BC_SHOE];
	int shoe_n;
	PkCard hand[2][3]; /* 0 punto, 1 banca */
	int n[2];
	int winner; /* BC_PLAYER, BC_BANKER o BC_TIE */
	Rng rng;
} Baccarat;

void bc_init(Baccarat* g, uint32_t seed);
int bc_card_value(PkCard c);
int bc_total(const PkCard* cards, int n);
/* Reparte una mano completa (las dos cartas de cada uno y las terceras) */
void bc_deal(Baccarat* g);
/* Igual, pero con las cartas dadas en el orden del reparto (para pruebas):
 * punto, banca, punto, banca y luego las terceras que hagan falta */
void bc_deal_cards(Baccarat* g, const PkCard* order);
/* Lo que se devuelve (apuesta más premio) */
int bc_payout(const Baccarat* g, const int bets[BC_BETS]);

#endif
