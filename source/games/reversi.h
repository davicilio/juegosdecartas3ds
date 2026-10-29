#ifndef GAMES_REVERSI_H
#define GAMES_REVERSI_H

#include <stdbool.h>
#include <stdint.h>

#include "core/rng.h"

/* Reversi en un tablero de 8x8: tú llevas las negras (1) y la CPU las blancas
 * (2). Empiezan las negras. Hay que poner ficha encerrando en línea recta
 * (horizontal, vertical o diagonal) fichas del rival entre la nueva y otra
 * tuya: esas fichas se dan la vuelta. Si no puedes poner, pasas. Cuando
 * nadie puede, gana quien tiene más fichas. */

#define RV_SIZE 8

typedef struct {
	uint8_t cell[RV_SIZE][RV_SIZE];
	int current;  /* 1 o 2 */
	int winner;   /* 0 se juega, 1, 2 o 3 empate */
	int count[3];
	int last_row, last_col; /* -1 si nada */
	bool last_passed;       /* el anterior no pudo poner */
	uint8_t flipped[RV_SIZE][RV_SIZE]; /* fichas que acaban de girar */
	int level;    /* profundidad de búsqueda de la CPU */
	Rng rng;
} Reversi;

void rv_init(Reversi* g, uint32_t seed);
void rv_new_game(Reversi* g);
/* Fichas que giraría (0 si no vale) */
int rv_flips(const Reversi* g, int player, int row, int col);
bool rv_can_move(const Reversi* g, int player);
bool rv_play(Reversi* g, int row, int col);
/* Movimiento de la CPU: devuelve fila*8+columna, o -1 */
int rv_ai_choose(const Reversi* g);

#endif
