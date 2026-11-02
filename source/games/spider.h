#ifndef GAMES_SPIDER_H
#define GAMES_SPIDER_H

#include <stdbool.h>
#include <stdint.h>

#include "core/rng.h"

/* Solitario Araña con dos barajas francesas (104 cartas) en 10 columnas: 54
 * repartidas (las de arriba boca arriba) y 50 en el mazo, que se reparten de
 * diez en diez (una a cada columna, sin columnas vacías). Una carta va sobre
 * otra de un número más (de cualquier palo), pero solo se puede mover un
 * grupo si es una escalera del mismo palo. Al completar del rey al as del
 * mismo palo, la escalera sale del tablero. Se gana al sacar las ocho. Con
 * 1, 2 o 4 palos (de más fácil a más difícil). */

#define SP_COLS    10
#define SP_MAXCOL  104
#define SP_HISTORY 40

typedef struct {
	uint8_t rank;  /* 1..13 */
	uint8_t suit;  /* 0 picas, 1 corazones, 2 diamantes, 3 tréboles */
	bool up;
} SpCard;

typedef struct {
	SpCard col[SP_COLS][SP_MAXCOL];
	int col_n[SP_COLS];
	SpCard stock[50];
	int stock_n;
	int done;      /* escaleras completas sacadas */
	int done_suit[8];
	int moves;
} SpBoard;

typedef struct {
	SpBoard b;
	SpBoard history[SP_HISTORY];
	int history_n;
	int suits;
	bool won;
	Rng rng;
} Spider;

void sp_init(Spider* g, int suits, uint32_t seed);
/* Cartas de la escalera movible al final de la columna */
int sp_run(const SpBoard* b, int col);
/* ¿Se pueden mover las últimas n cartas de from a to? */
bool sp_can_move(const SpBoard* b, int from, int n, int to);
bool sp_move(Spider* g, int from, int n, int to);
bool sp_can_deal(const SpBoard* b);
bool sp_deal(Spider* g);
bool sp_undo(Spider* g);
/* Busca un movimiento útil (pista): devuelve true y lo deja en from/n/to */
bool sp_hint(const SpBoard* b, int* from, int* n, int* to);

#endif
