#ifndef GAMES_MANCALA_H
#define GAMES_MANCALA_H

#include <stdbool.h>

#include "core/rng.h"

/* Mancala (variante kalah): seis hoyos por jugador con cuatro semillas y un
 * granero a la derecha de cada uno. Coges todas las semillas de un hoyo tuyo
 * y las vas dejando de una en una en sentido contrario a las agujas del reloj
 * (en tu granero sí, en el del rival no). Si la última cae en tu granero,
 * vuelves a jugar; si cae en un hoyo tuyo vacío y enfrente hay semillas, te
 * llevas las dos cosas. Cuando un lado se queda vacío, cada uno guarda lo que
 * le queda y gana quien más semillas tenga en su granero.
 *
 * Hoyos 0..5 del jugador 0 (tú, abajo, de izquierda a derecha), 6 su granero,
 * 7..12 del jugador 1 y 13 el suyo. */

#define MC_PITS 14

typedef struct {
	int pit[MC_PITS];
	int current;
	bool over;
	int last;        /* hoyo donde cayó la última semilla */
	bool captured;   /* la última jugada capturó */
	bool again;      /* la última jugada da otro turno */
	int level;       /* 0 fácil, 1 normal, 2 difícil */
	Rng rng;
} Mancala;

void mc_init(Mancala* g, int level, uint32_t seed);
/* El hoyo i (0..5 del que juega, contado desde su izquierda) */
bool mc_can_play(const Mancala* g, int i);
bool mc_play(Mancala* g, int i);
int mc_store(int p);
int mc_ai(Mancala* g);

#endif
