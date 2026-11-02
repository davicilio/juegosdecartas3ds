#ifndef GAMES_SUDOKU_H
#define GAMES_SUDOKU_H

#include <stdbool.h>
#include <stdint.h>

#include "core/rng.h"

/* Sudoku: rellena la cuadrícula de 9x9 para que cada fila, columna y caja de
 * 3x3 tenga los números del 1 al 9. Los sudokus se generan al momento y
 * siempre tienen una única solución. */

typedef enum { SD_EASY, SD_MEDIUM, SD_HARD, SD_LEVELS } SdLevel;

typedef struct {
	uint8_t cell[81];      /* 0 = vacía */
	uint8_t solution[81];
	bool given[81];        /* números de partida (no se pueden cambiar) */
	uint16_t notes[81];    /* notas: bit n = número n */
	SdLevel level;
	int mistakes;
	Rng rng;
} Sudoku;

/* Cuántas soluciones tiene la cuadrícula (se para al llegar a limit) */
int sd_count_solutions(const uint8_t grid[81], int limit);
void sd_generate(Sudoku* g, SdLevel level, uint32_t seed);
/* Pone (o borra con 0) un número. Devuelve false si la casilla es fija. */
bool sd_set(Sudoku* g, int i, int v);
bool sd_toggle_note(Sudoku* g, int i, int v);
/* El número de la casilla choca con otro de su fila, columna o caja */
bool sd_conflict(const Sudoku* g, int i);
bool sd_solved(const Sudoku* g);
int sd_givens(const Sudoku* g);
/* Rellena una casilla con su solución (pista); devuelve la casilla o -1 */
int sd_hint(Sudoku* g, int prefer);

#endif
