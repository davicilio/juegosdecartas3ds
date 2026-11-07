#ifndef GAMES_RULETA_H
#define GAMES_RULETA_H

#include <stdbool.h>

#include "core/rng.h"

/* Ruleta francesa (un solo cero), con las apuestas de la mesa:
 * - Pleno a un número (0..36): paga 35 a 1.
 * - Docenas (1-12, 13-24, 25-36) y columnas: pagan 2 a 1.
 * - Sencillas (rojo/negro, par/impar, falta 1-18/pasa 19-36): pagan 1 a 1.
 * Con el cero pierden todas menos el pleno al cero. */

#define RL_NUMBERS 37
enum {
	RL_DOZEN0 = 37, /* 37..39: primera, segunda y tercera docena */
	RL_COLUMN0 = 40, /* 40..42: columnas del 1, del 2 y del 3 */
	RL_LOW = 43,
	RL_EVEN,
	RL_RED,
	RL_BLACK,
	RL_ODD,
	RL_HIGH,
	RL_SPOTS
};

/* Orden de los números en el cilindro */
extern const int RL_WHEEL[RL_NUMBERS];

bool rl_is_red(int n);
/* La casilla de apuesta gana con el número n */
bool rl_wins(int spot, int n);
/* Cuánto paga por cada ficha (sin contar la apostada) */
int rl_pays(int spot);
/* Lo que se devuelve al jugador (apuesta más premio) con el número n */
int rl_payout(const int bets[RL_SPOTS], int n);
int rl_spin(Rng* rng);

#endif
