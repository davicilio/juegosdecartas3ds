#ifndef GAMES_DAMAS_H
#define GAMES_DAMAS_H

#include <stdbool.h>
#include <stdint.h>

#include "core/rng.h"

/* Damas con las reglas españolas: tablero de 8x8, doce fichas por bando en
 * las casillas oscuras. Las fichas avanzan en diagonal una casilla y comen
 * saltando hacia delante. La ficha que llega a la última fila corona y se
 * convierte en dama, que se mueve y come en diagonal a cualquier distancia
 * ("dama voladora"). Comer es obligatorio y, si hay varias formas, hay que
 * elegir la que come más fichas. Las fichas comidas se quitan al terminar
 * la jugada. Gana quien deja al rival sin fichas o sin jugadas; 80 jugadas
 * seguidas sin comer ni mover fichas sin coronar son tablas.
 *
 * Bando 1: blancas (el humano, abajo, empiezan). Bando 2: negras (CPU). */

#define DA_SIZE     8
#define DA_MAX_PATH 12
#define DA_MAX_MOVES 128

enum { DA_EMPTY, DA_WHITE, DA_BLACK, DA_WHITE_KING, DA_BLACK_KING };

typedef struct {
	int8_t fr, fc;                   /* casilla de salida */
	int8_t path[DA_MAX_PATH][2];     /* casillas por las que pasa (la última es la de llegada) */
	int8_t caps[DA_MAX_PATH][2];     /* fichas comidas */
	int8_t n_path, n_caps;
} DaMove;

typedef struct {
	uint8_t b[DA_SIZE][DA_SIZE];
	int current;   /* 1 o 2 */
	int winner;    /* 0, 1, 2 o 3 tablas */
	int quiet;     /* jugadas seguidas sin comer ni mover fichas */
	int level;     /* profundidad de la CPU */
	DaMove last;
	bool has_last;
	Rng rng;
} Damas;

static inline int da_owner(int piece)
{
	return piece == DA_WHITE || piece == DA_WHITE_KING ? 1 : piece ? 2 : 0;
}

void da_init(Damas* g, uint32_t seed);
void da_new_game(Damas* g);
/* Jugadas legales del bando que mueve */
int da_moves(const Damas* g, DaMove* out);
bool da_apply(Damas* g, const DaMove* m);
int da_ai_choose(Damas* g, DaMove* out);
int da_count(const Damas* g, int side);

#endif
