#ifndef GAMES_KENO_H
#define GAMES_KENO_H

#include <stdbool.h>

#include "core/rng.h"

/* Keno: eliges de 1 a 10 números del 1 al 80 y salen 20 bolas. Se cobra
 * según cuántos aciertos (tabla de KN_PAY, en décimas de la apuesta, contando
 * la propia apuesta). */

#define KN_NUMBERS 80
#define KN_DRAWN   20
#define KN_MAX_PICK 10

extern const int KN_PAY[KN_MAX_PICK + 1][KN_MAX_PICK + 1];

typedef struct {
	bool picked[KN_NUMBERS + 1];
	int picks;
	int drawn[KN_DRAWN];
	bool is_drawn[KN_NUMBERS + 1];
	int hits;
	Rng rng;
} Keno;

void kn_init(Keno* g, uint32_t seed);
/* Marca o desmarca un número; false si no se puede (ya hay 10) */
bool kn_toggle(Keno* g, int n);
void kn_clear(Keno* g);
/* Elige count números al azar */
void kn_quick_pick(Keno* g, int count);
/* Saca las 20 bolas y cuenta los aciertos; devuelve el multiplicador en décimas */
int kn_draw(Keno* g);
int kn_pay(int picks, int hits);
/* Lo que se cobra (apuesta más premio, redondeado hacia abajo) */
int kn_payout(int bet, int picks, int hits);

#endif
