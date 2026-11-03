#ifndef GAMES_GOMOKU_H
#define GAMES_GOMOKU_H

#include <stdbool.h>
#include <stdint.h>

#include "core/rng.h"

/* Cinco en raya (gomoku) en un tablero de 15x15: se ponen piedras por turnos
 * y gana quien junte cinco seguidas en horizontal, vertical o diagonal. Tú
 * llevas las negras y empiezas; la consola, las blancas. */

#define GO_N 15

typedef struct {
	int8_t cell[GO_N][GO_N]; /* 0 vacía, 1 tú, 2 la consola */
	int current;             /* 1 o 2 */
	int winner;              /* 0 nadie, 1, 2 o 3 (tablero lleno) */
	int moves;
	int last_r, last_c;
	int win_r[5], win_c[5];
	int level;               /* 0 fácil, 1 normal, 2 difícil */
	Rng rng;
} Gomoku;

void go_init(Gomoku* g, int level, uint32_t seed);
bool go_play(Gomoku* g, int r, int c);
/* Jugada de la consola (fila * GO_N + columna) */
int go_ai(Gomoku* g);

#endif
