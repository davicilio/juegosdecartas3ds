#ifndef GAMES_CRAPS_H
#define GAMES_CRAPS_H

#include <stdbool.h>

#include "core/rng.h"

/* Dados (craps), con las apuestas más habituales:
 * - Pase: en la primera tirada gana con 7 u 11 y pierde con 2, 3 o 12. Con
 *   otro número, ese es el punto: se sigue tirando hasta que sale el punto
 *   (gana) o un 7 (pierde). Paga 1 a 1. Solo se apuesta antes de la salida.
 * - No pase: lo contrario, salvo que el 12 de salida empata. Paga 1 a 1.
 * - Campo (una tirada): gana con 2, 3, 4, 9, 10, 11 o 12; el 2 paga doble y
 *   el 12 triple; lo demás 1 a 1.
 * - Cualquier 7 (una tirada): paga 4 a 1.
 * - Cualquier craps (una tirada, 2, 3 o 12): paga 7 a 1. */

enum { CR_PASS, CR_DONTPASS, CR_FIELD, CR_ANY7, CR_CRAPS, CR_BETS };

typedef struct {
	int point;   /* 0 en la tirada de salida */
	int die[2];
	int bets[CR_BETS];
	int last_return; /* lo devuelto en la última tirada */
	int last_lost;   /* lo perdido en la última tirada */
	Rng rng;
} Craps;

void cr_init(Craps* g, uint32_t seed);
/* Se puede apostar a esa casilla ahora */
bool cr_can_bet(const Craps* g, int bet);
/* Tira (d1/d2 a 0 = al azar) y resuelve; devuelve lo que se cobra
 * (apuesta más premio) de las apuestas resueltas, que salen de bets */
int cr_roll(Craps* g, int d1, int d2);

#endif
