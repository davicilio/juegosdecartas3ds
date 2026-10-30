#ifndef GAMES_FLOTA_H
#define GAMES_FLOTA_H

#include <stdbool.h>
#include <stdint.h>

#include "core/rng.h"

/* Hundir la flota en un tablero de 10x10 contra la CPU (el 0 eres tú).
 * Cada uno tiene un portaaviones (5), un acorazado (4), dos cruceros (3) y un
 * destructor (2), en línea recta y sin tocarse. Por turnos se dispara a una
 * casilla del rival: "agua", "tocado" o "hundido". Si tocas, vuelves a
 * disparar. Gana quien hunde antes toda la flota contraria. */

#define HF_SIZE  10
#define HF_SHIPS 5

enum { HF_NONE, HF_WATER, HF_HIT, HF_SUNK };

typedef struct {
	int row, col, len;
	bool horizontal;
	int hits;
} HfShip;

typedef struct {
	HfShip ships[2][HF_SHIPS];
	int8_t ship_at[2][HF_SIZE][HF_SIZE]; /* -1 agua, o el barco */
	uint8_t shot[2][HF_SIZE][HF_SIZE];   /* disparos recibidos por cada jugador */
	int sunk[2];
	int current;
	int winner;   /* -1 se juega */
	int last_result, last_ship, last_row, last_col, last_shooter;
	int shots[2];
	Rng rng;
} Flota;

extern const int HF_LENGTHS[HF_SHIPS];
const char* hf_ship_name(int ship);

void hf_init(Flota* g, uint32_t seed);
void hf_new_game(Flota* g);
/* Recoloca al azar los barcos de un jugador (antes de empezar) */
void hf_place_random(Flota* g, int player);
/* Dispara el jugador actual a (row, col) del rival. Devuelve HF_WATER... */
int hf_fire(Flota* g, int row, int col);
/* IA: casilla a la que dispara (row * HF_SIZE + col) */
int hf_ai_target(Flota* g);

#endif
