#ifndef GAMES_BUSCAMINAS_H
#define GAMES_BUSCAMINAS_H

#include <stdbool.h>
#include <stdint.h>

#include "core/rng.h"

/* Buscaminas: destapa todas las casillas sin mina. Cada número dice cuántas
 * minas hay alrededor. La primera casilla que se destapa nunca tiene mina
 * (ni sus vecinas), y al tocar un número con tantas banderas alrededor como
 * su valor se destapan las demás vecinas. */

#define BM_MAX_W 16
#define BM_MAX_H 10

typedef enum { BM_EASY, BM_MEDIUM, BM_HARD, BM_LEVELS } BmLevel;
typedef enum { BM_PLAYING, BM_WON, BM_LOST } BmState;

typedef struct {
	int w, h, mines;
	BmLevel level;
	bool mine[BM_MAX_H][BM_MAX_W];
	bool open[BM_MAX_H][BM_MAX_W];
	bool flag[BM_MAX_H][BM_MAX_W];
	uint8_t near[BM_MAX_H][BM_MAX_W];
	bool placed;  /* las minas se ponen con el primer toque */
	int opened;
	int boom_x, boom_y;
	BmState state;
	Rng rng;
} Buscaminas;

void bm_level_size(BmLevel level, int* w, int* h, int* mines);
void bm_init(Buscaminas* g, BmLevel level, uint32_t seed);
/* Destapa (x, y). Devuelve false si no hace nada. */
bool bm_reveal(Buscaminas* g, int x, int y);
bool bm_toggle_flag(Buscaminas* g, int x, int y);
/* Destapa las vecinas de un número con sus banderas puestas */
bool bm_chord(Buscaminas* g, int x, int y);
int bm_flags(const Buscaminas* g);

#endif
