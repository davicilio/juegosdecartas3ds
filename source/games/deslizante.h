#ifndef GAMES_DESLIZANTE_H
#define GAMES_DESLIZANTE_H

#include <stdbool.h>

#include "core/rng.h"

/* Puzle deslizante (el "15"): fichas numeradas en una cuadrícula de n x n
 * con un hueco. Se desliza a él una ficha vecina (o una fila o columna
 * entera de golpe) hasta dejarlas en orden. Siempre tiene solución: se
 * desordena con movimientos de verdad. */

#define DZ_MAX 5

typedef struct {
	int n;
	int cell[DZ_MAX * DZ_MAX]; /* 0 = hueco */
	int moves;
	Rng rng;
} Deslizante;

void dz_init(Deslizante* g, int n, uint32_t seed);
/* Mueve hacia el hueco la ficha i y las que haya entre las dos (misma fila o columna) */
bool dz_slide(Deslizante* g, int i);
bool dz_solved(const Deslizante* g);
int dz_blank(const Deslizante* g);

#endif
