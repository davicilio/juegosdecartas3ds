#ifndef GAMES_OCA_H
#define GAMES_OCA_H

#include <stdbool.h>

#include "core/rng.h"

/* El juego de la oca, de 2 a 4 jugadores (el 0 es el humano), con un dado.
 *
 * Se sale de la casilla 1 y se gana llegando justo a la 63 (si sobra, se
 * rebota hacia atrás). Casillas especiales:
 *   - Oca (5, 9, 14, 18, 23, 27, 32, 36, 41, 45, 50, 54 y 59): "de oca a oca
 *     y tiro porque me toca", saltas a la siguiente y vuelves a tirar.
 *   - Puente (6 y 12): saltas al otro puente y vuelves a tirar.
 *   - Dados (26 y 53): saltas al otro dado y vuelves a tirar.
 *   - Posada (19): pierdes un turno.
 *   - Pozo (31): te quedas hasta que otro caiga y te saque.
 *   - Laberinto (42): vuelves a la 30.
 *   - Cárcel (56): pierdes dos turnos.
 *   - Muerte (58): vuelves a empezar. */

#define OCA_MAX_PLAYERS 4
#define OCA_GOAL        63

typedef enum {
	OCA_NORMAL,
	OCA_GOOSE,
	OCA_BRIDGE,
	OCA_DICE,
	OCA_INN,
	OCA_WELL,
	OCA_MAZE,
	OCA_JAIL,
	OCA_DEATH,
	OCA_GARDEN
} OcaSquare;

typedef struct {
	int players;
	int pos[OCA_MAX_PLAYERS];
	int skip[OCA_MAX_PLAYERS];
	bool in_well[OCA_MAX_PLAYERS];
	int current;
	int winner; /* -1 mientras se juega */
	/* Última tirada, para la interfaz */
	int last_player;
	int last_roll;
	int last_land;   /* casilla a la que llegó con el dado */
	int last_to;     /* casilla final tras el efecto */
	int last_square; /* OcaSquare de la casilla en la que cayó */
	bool last_again;
	int last_freed;  /* a quién sacó del pozo, o -1 */
	Rng rng;
} Oca;

OcaSquare oca_square(int n);
void oca_init(Oca* g, int players, uint32_t seed);
void oca_new_game(Oca* g);
/* Tira el dado el jugador actual (roll 0 = al azar) */
int oca_roll(Oca* g, int roll);

#endif
