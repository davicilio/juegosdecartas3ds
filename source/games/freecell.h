#ifndef GAMES_FREECELL_H
#define GAMES_FREECELL_H

#include <stdbool.h>
#include <stdint.h>

/* Carta blanca (FreeCell) con la baraja francesa.
 *
 * Las 52 cartas se reparten boca arriba en 8 columnas. Hay 4 casillas libres
 * (una carta en cada una) y 4 montones finales, uno por palo, del as al rey.
 * En las columnas se pone una carta sobre otra de un número más y de color
 * contrario. Se puede mover una escalera de golpe si cabe: tantas cartas como
 * (casillas libres + 1) x 2 elevado a las columnas vacías. Se gana al subir
 * las 52 cartas a los montones.
 *
 * Las partidas van numeradas como en el FreeCell de Windows (la nº 1 es la
 * misma), así que se pueden repetir y comparar. */

#define FC_COLS    8
#define FC_CELLS   4
#define FC_MAXCOL  (7 + 12)
#define FC_HISTORY 64

/* Lugares: 0..7 columnas, 8..11 casillas libres, 12..15 montones finales */
#define FC_CELL0  FC_COLS
#define FC_FOUND0 (FC_COLS + FC_CELLS)
#define FC_PLACES (FC_FOUND0 + 4)

typedef struct {
	uint8_t rank; /* 1 (as) .. 13 (rey); 0 = nada */
	uint8_t suit; /* 0 picas, 1 corazones, 2 diamantes, 3 tréboles */
} FcCard;

typedef struct {
	FcCard col[FC_COLS][FC_MAXCOL];
	int col_n[FC_COLS];
	FcCard cell[FC_CELLS];
	int found[4]; /* carta más alta subida de cada palo (0 = ninguna) */
	int moves;
} FcBoard;

typedef struct {
	FcBoard b;
	FcBoard history[FC_HISTORY];
	int history_n;
	int deal;
	bool won;
} FreeCell;

bool fc_red(FcCard c);
void fc_new_game(FreeCell* g, int deal);
/* Cuántas cartas se pueden mover de golpe hacia dest (columna vacía o no) */
int fc_capacity(const FcBoard* b, int dest);
/* Cartas de la escalera que hay al final de la columna */
int fc_run(const FcBoard* b, int col);
bool fc_can_move(const FcBoard* b, int from, int to);
bool fc_move(FreeCell* g, int from, int to);
/* Sube a los montones las cartas que ya no hacen falta; devuelve cuántas */
int fc_auto(FreeCell* g);
bool fc_undo(FreeCell* g);
int fc_cards_home(const FcBoard* b);

#endif
