#ifndef GAMES_PARCHIS_H
#define GAMES_PARCHIS_H

#include <stdbool.h>

#include "core/rng.h"

/* Parchís para cuatro jugadores: amarillo (0, el humano), azul, rojo y verde,
 * en ese orden. El circuito tiene 68 casillas; cada color sale de la suya
 * (5, 22, 39, 56) y, al llegar a la de su entrada (68, 17, 34, 51), sube por
 * su pasillo de 7 casillas hasta la meta.
 *
 * - Se tira un dado. Para sacar una ficha de casa hace falta un 5, y si se
 *   puede sacar, es obligatorio.
 * - Con un 6 se vuelve a tirar; si no queda ninguna ficha en casa, el 6 vale
 *   7. Al tercer 6 seguido, la última ficha movida vuelve a casa (salvo que
 *   ya esté en el pasillo).
 * - En una casilla caben dos fichas. Dos del mismo color forman una barrera
 *   que nadie puede pasar.
 * - Si caes en una ficha de otro color fuera de un seguro, te la comes: vuelve
 *   a su casa y tú cuentas 20 con la ficha que quieras. Al sacar ficha
 *   también se come a quien esté en tu salida.
 * - Para entrar en la meta hay que sacar lo justo; al entrar se cuentan 10.
 * - Gana quien mete antes sus cuatro fichas. */

#define PA_PLAYERS 4
#define PA_PIECES  4
#define PA_RING    68
#define PA_LANE    7
#define PA_HOME    (-1)
#define PA_GOAL    (PA_RING - 4 + PA_LANE) /* pasos desde la salida hasta la meta */

typedef enum { PA_ROLL, PA_MOVE, PA_OVER } PaPhase;

typedef struct {
	/* Pasos de cada ficha desde su salida: PA_HOME en casa, 0..63 en el
	 * circuito, 64..70 en el pasillo y PA_GOAL en la meta */
	int pos[PA_PLAYERS][PA_PIECES];
	int current;
	int die;
	int move;        /* lo que hay que mover ahora (dado, 20 o 10) */
	int bonus[4];    /* premios pendientes (20 por comer, 10 por meta) */
	int bonus_n;
	int sixes;
	int last_piece;  /* última ficha movida por el jugador actual */
	int winner;
	PaPhase phase;
	/* Para la interfaz */
	int last_player, last_from, last_to, last_moved;
	int last_captured_player, last_captured_piece;
	bool last_goal, last_three_sixes, last_no_move;
	Rng rng;
} Parchis;

/* Casilla del circuito (1..68) de un color a tantos pasos, o 0 si no está en él */
int pa_square(int player, int steps);
bool pa_safe(int square);
int pa_exit(int player);

void pa_init(Parchis* g, uint32_t seed);
void pa_new_game(Parchis* g);
/* Tira el dado (die 0 = al azar). Pasa a PA_MOVE o al siguiente turno */
int pa_roll(Parchis* g, int die);
bool pa_can_move(const Parchis* g, int piece);
/* Pasos a los que llegaría la ficha con el movimiento actual (-2 si no puede) */
int pa_target(const Parchis* g, int piece);
bool pa_move(Parchis* g, int piece);
/* Cuenta los fichas de un jugador en la meta */
int pa_in_goal(const Parchis* g, int player);
int pa_ai_choose(const Parchis* g);

#endif
