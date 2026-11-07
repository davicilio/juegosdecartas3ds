#ifndef GAMES_RUEDA_H
#define GAMES_RUEDA_H

#include "core/rng.h"

/* Rueda de la fortuna: una rueda de 54 casillas con los símbolos 1, 2, 5,
 * 10 y 20 y dos comodines. Se apuesta a uno o varios símbolos y, si la rueda
 * para en él, paga su número a 1 (los comodines, 40 a 1). */

#define RD_SLOTS 54
enum { RD_1, RD_2, RD_5, RD_10, RD_20, RD_JOKER, RD_STAR, RD_SYMBOLS };

extern const int RD_WHEEL[RD_SLOTS];

int rd_pays(int symbol);
int rd_spin(Rng* rng); /* casilla 0..RD_SLOTS-1 */
/* Lo que se devuelve (apuesta más premio) si sale la casilla slot */
int rd_payout(const int bets[RD_SYMBOLS], int slot);
const char* rd_label(int symbol);

#endif
