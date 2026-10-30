#ifndef GAMES_TRESENRAYA_H
#define GAMES_TRESENRAYA_H

#include <stdbool.h>
#include <stdint.h>

#include "core/rng.h"

/* Tres en raya: tú pones las X (1) y la CPU los círculos (2); empiezan por
 * turnos. Gana quien junta tres en línea. En "difícil" la CPU juega perfecto
 * (como mucho, empate); en "fácil" a veces se despista. */

typedef struct {
	uint8_t cell[9];  /* 0 vacío, 1 o 2 */
	int current;
	int winner;       /* 0 se juega, 1, 2 o 3 empate */
	int line[3];      /* casillas de la línea ganadora */
	int starter;
	int level;        /* 0 fácil, 1 normal, 2 difícil */
	Rng rng;
} TresEnRaya;

void tx_init(TresEnRaya* g, uint32_t seed);
void tx_new_game(TresEnRaya* g);
bool tx_play(TresEnRaya* g, int cell);
int tx_ai_choose(TresEnRaya* g);

#endif
