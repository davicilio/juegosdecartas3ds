#ifndef GAMES_TRAGAPERRAS_H
#define GAMES_TRAGAPERRAS_H

#include "core/rng.h"

/* Tragaperras de tres rodillos y cinco líneas (las tres filas y las dos
 * diagonales). Se juega con 1, 3 o 5 líneas y una apuesta por línea; cada
 * línea con premio paga según la tabla (veces la apuesta de la línea). */

#define TP_REELS  3
#define TP_STRIP  22 /* símbolos de cada rodillo */
#define TP_LINES  5

typedef enum {
	TP_CHERRY,
	TP_LEMON,
	TP_ORANGE,
	TP_PLUM,
	TP_BELL,
	TP_BAR,
	TP_SEVEN,
	TP_SYMBOLS
} TpSymbol;

extern const int TP_REEL[TP_REELS][TP_STRIP];
/* Filas de cada línea: TP_LINE[l][rodillo] = fila 0 (arriba), 1 o 2 */
extern const int TP_LINE[TP_LINES][TP_REELS];

/* Premio (veces la apuesta) de tres símbolos en línea */
int tp_line_pay(const int sym[TP_REELS]);
/* Símbolo que se ve en la fila row (0..2) del rodillo r parado en stop */
int tp_symbol_at(int r, int stop, int row);
/* Premio total (en veces la apuesta por línea) jugando lines líneas;
 * win_mask recibe qué líneas han tenido premio (bit l) */
int tp_eval(const int stop[TP_REELS], int lines, int* win_mask);
void tp_spin(Rng* rng, int stop[TP_REELS]);
const char* tp_symbol_name(int s);

#endif
