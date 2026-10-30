#ifndef GAMES_MAHJONG_H
#define GAMES_MAHJONG_H

#include <stdbool.h>
#include <stdint.h>

#include "core/rng.h"

/* Mahjong solitario con la tortuga clásica de 144 fichas en cinco pisos.
 *
 * Fichas: círculos, bambúes y caracteres del 1 al 9, los cuatro vientos y los
 * tres dragones (cuatro de cada), más cuatro flores y cuatro estaciones (cada
 * flor empareja con cualquier flor, y cada estación con cualquier estación).
 *
 * Se quitan parejas de fichas iguales que estén libres: sin nada encima y con
 * el lado izquierdo o el derecho despejado. Se gana al quitarlas todas. El
 * reparto siempre tiene solución (se construye quitando parejas al revés). */

#define MJ_TILES 144
#define MJ_KINDS 42 /* 34 normales + 4 flores + 4 estaciones */

enum {
	MJ_DOTS = 0,     /* 0..8 círculos */
	MJ_BAMBOO = 9,   /* 9..17 bambúes */
	MJ_CHARS = 18,   /* 18..26 caracteres */
	MJ_WINDS = 27,   /* 27..30 norte, este, sur, oeste */
	MJ_DRAGONS = 31, /* 31..33 rojo, verde, blanco */
	MJ_FLOWERS = 34, /* 34..37 */
	MJ_SEASONS = 38  /* 38..41 */
};

typedef struct {
	int8_t x, y, z; /* en medias fichas; z es el piso */
} MjPos;

typedef struct {
	uint8_t face[MJ_TILES];
	bool present[MJ_TILES];
	int left;
	int undo[MJ_TILES];  /* parejas quitadas, para deshacer */
	int undo_n;
	int shuffles;
	Rng rng;
} Mahjong;

extern const MjPos MJ_LAYOUT[MJ_TILES];

bool mj_match(int a, int b); /* ¿emparejan las caras a y b? */
void mj_init(Mahjong* g, uint32_t seed);
void mj_new_game(Mahjong* g);
bool mj_free(const Mahjong* g, int tile);
bool mj_remove(Mahjong* g, int a, int b);
bool mj_undo(Mahjong* g);
/* Pareja libre que se puede quitar (devuelve false si no hay) */
bool mj_hint(const Mahjong* g, int* a, int* b);
/* Vuelve a repartir las caras que quedan para que tenga solución */
bool mj_shuffle(Mahjong* g);

#endif
