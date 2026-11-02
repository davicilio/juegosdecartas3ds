#ifndef GAMES_BACKGAMMON_H
#define GAMES_BACKGAMMON_H

#include <stdbool.h>
#include <stdint.h>

#include "core/rng.h"

/* Backgammon para dos: blancas (0, el humano) y negras (1). Puntas 0..23:
 * las blancas avanzan de la 23 a la 0 y sacan fichas por debajo; las negras,
 * de la 0 a la 23. Se tiran dos dados (dobles: cuatro movimientos). Una ficha
 * no puede caer en una punta con dos o más contrarias; si cae en una con una
 * sola, la manda a la barra, y desde la barra hay que volver a entrar antes
 * de mover nada más. Con todas las fichas en casa se pueden sacar. Hay que
 * usar todos los dados que se pueda y, si solo se puede uno, el mayor. Gana
 * quien saca antes sus 15 fichas: doble (gammon) si el otro no ha sacado
 * ninguna, y triple (backgammon) si además tiene alguna en la barra o en la
 * casa del ganador. */

#define BG_POINTS 24
#define BG_BAR    24 /* origen: la barra */
#define BG_OFF    25 /* destino: fuera del tablero */
#define BG_CHECKERS 15

typedef struct {
	int8_t pt[BG_POINTS]; /* + blancas, - negras */
	int8_t bar[2];
	int8_t off[2];
} BgBoard;

typedef struct {
	uint8_t from, to, die;
} BgStep;

typedef enum { BG_ROLL, BG_MOVE, BG_OVER } BgPhase;

typedef struct {
	BgBoard b;
	int current;
	int dice[2];
	int left[4];   /* dados que quedan por usar */
	int left_n;
	int winner;
	int points;    /* 1, 2 (gammon) o 3 (backgammon) */
	bool no_move;  /* la última tirada no se pudo mover */
	BgPhase phase;
	Rng rng;
} Backgammon;

void bg_init(Backgammon* g, uint32_t seed);
void bg_start_board(BgBoard* b);
int bg_pips(const BgBoard* b, int p);
/* Tira los dados (d1 o d2 a 0: al azar) */
void bg_roll(Backgammon* g, int d1, int d2);
/* Movimientos sueltos que se pueden hacer ahora respetando la regla de usar
 * el máximo de dados (devuelve cuántos) */
int bg_legal_steps(const Backgammon* g, BgStep* out, int max);
bool bg_step(Backgammon* g, int from, int to);
/* IA: hace la jugada entera del turno */
void bg_ai_play(Backgammon* g);

#endif
