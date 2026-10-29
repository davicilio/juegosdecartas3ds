#ifndef GAMES_BINGO_H
#define GAMES_BINGO_H

#include <stdbool.h>
#include <stdint.h>

#include "core/rng.h"

/* Bingo de 90 bolas con cartones como los de los bingos de España: 3 filas y
 * 9 columnas, 15 números (5 por fila). La primera columna lleva del 1 al 9,
 * la segunda del 10 al 19 ... y la última del 80 al 90; en cada columna los
 * números van de menor a mayor hacia abajo.
 *
 * Juegas tú (jugador 0, de 1 a BI_MAX_CARDS cartones) contra tres rivales.
 * Cada cartón cuesta BI_PRICE fichas y lo recaudado es el premio: la primera
 * línea (una fila completa) se lleva el BI_LINE_PCT % y el bingo (el cartón
 * entero), el resto. Hay que cantarlo: la máquina comprueba los números con
 * las bolas que han salido. */

#define BI_BALLS     90
#define BI_ROWS      3
#define BI_COLS      9
#define BI_PER_ROW   5
#define BI_PLAYERS   4
#define BI_MAX_CARDS 4
#define BI_PRICE     5
#define BI_START     100
#define BI_LINE_PCT  30

typedef struct {
	uint8_t num[BI_ROWS][BI_COLS]; /* 0 = casilla vacía */
	bool marked[BI_ROWS][BI_COLS];
} BiCard;

typedef enum { BI_SETUP, BI_PLAYING, BI_OVER } BiPhase;

typedef struct {
	BiCard cards[BI_PLAYERS][BI_MAX_CARDS];
	int card_n[BI_PLAYERS];
	bool drawn[BI_BALLS + 1];
	uint8_t order[BI_BALLS]; /* bolas en el orden en que salen */
	int drawn_n;
	int chips[BI_PLAYERS];
	int pot;
	int line_winner;  /* -1 si nadie ha cantado línea */
	int line_card;
	int line_row;
	int bingo_winner;
	int bingo_card;
	int line_prize, bingo_prize;
	BiPhase phase;
	Rng rng;
} Bingo;

/* Cartón al azar que cumple las reglas */
void bi_make_card(Rng* rng, BiCard* c);
/* Comprueba que un cartón es válido (para los tests) */
bool bi_card_valid(const BiCard* c);

void bi_init(Bingo* g, uint32_t seed);
/* Nueva partida con n cartones para ti (los rivales cogen 1 o 2) */
bool bi_start(Bingo* g, int n);
int bi_last(const Bingo* g);
/* Saca la siguiente bola (0 si no quedan) */
int bi_draw(Bingo* g);
/* Fila completa de un cartón (-1 si no hay) y si está entero, según las bolas */
int bi_card_line(const Bingo* g, const BiCard* c);
bool bi_card_full(const Bingo* g, const BiCard* c);
int bi_card_missing(const Bingo* g, const BiCard* c);
/* ¿Tiene el jugador línea o bingo que cantar? */
bool bi_has_line(const Bingo* g, int player);
bool bi_has_bingo(const Bingo* g, int player);
bool bi_claim_line(Bingo* g, int player);
bool bi_claim_bingo(Bingo* g, int player);
/* Marca (o desmarca) la casilla si su número ha salido */
bool bi_mark(Bingo* g, int card, int row, int col);
void bi_mark_all(Bingo* g, int player);
/* Lo que le falta al mejor cartón del jugador */
int bi_best_missing(const Bingo* g, int player);

#endif
