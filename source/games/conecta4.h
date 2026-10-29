#ifndef GAMES_CONECTA4_H
#define GAMES_CONECTA4_H

#include <stdbool.h>
#include <stdint.h>

#include "core/rng.h"

/* Conecta 4: tablero de 7 columnas y 6 filas. Por turnos se deja caer una
 * ficha en una columna; gana quien junta cuatro en línea (horizontal,
 * vertical o diagonal). El jugador 1 es el humano (rojo) y el 2 la CPU. */

#define C4_COLS 7
#define C4_ROWS 6

typedef struct {
	uint8_t cell[C4_ROWS][C4_COLS]; /* fila 0 abajo; 0 vacío, 1 o 2 */
	int current;  /* 1 o 2 */
	int winner;   /* 0 nadie, 1, 2 o 3 empate */
	int moves;
	int last_row, last_col;
	uint8_t line[4][2]; /* fichas de la línea ganadora (fila, columna) */
	int level;    /* dificultad: profundidad de búsqueda de la CPU */
	int starter;
	Rng rng;
} Conecta4;

void c4_init(Conecta4* g, uint32_t seed);
void c4_new_game(Conecta4* g);
bool c4_can_drop(const Conecta4* g, int col);
/* Fila en la que caería la ficha (o -1) */
int c4_drop_row(const Conecta4* g, int col);
bool c4_drop(Conecta4* g, int col);
int c4_ai_choose(Conecta4* g);

#endif
