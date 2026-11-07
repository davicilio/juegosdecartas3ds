#ifndef UI_CASINO_H
#define UI_CASINO_H

#include <stdbool.h>

#include "ui/input.h"

/* Cosas comunes a los juegos de casino: la ficha con la que se apuesta (de
 * 1, 5, 25, 100 o 500), las fichas puestas sobre la mesa y cómo se sacan del
 * monedero y se devuelven. */

#define CASINO_CHIP_KINDS 5

int casino_chip_value(int idx);
/* Cambia de ficha con L/R o tocando una de la fila dibujada en y */
void casino_pick_update(int* idx, const Input* in, float y);
/* Fila de fichas para elegir y, a la derecha, las fichas que te quedan */
void casino_pick_draw(int idx, float y, int staked);

/* Una ficha con su cantidad encima, centrada en (cx, cy) */
void casino_chip(int amount, float cx, float cy, float scale, float depth);

/* Pone value fichas del monedero en *bet. Devuelve false (y suena el
 * error) si el monedero está vacío. */
bool casino_place(int* bet, int value);
/* Devuelve al monedero todas las apuestas y las deja a cero */
void casino_refund(int* bets, int n);
/* Vuelve a poner las apuestas de last (si hay fichas para todas) */
bool casino_repeat(int* bets, const int* last, int n);
int casino_sum(const int* bets, int n);

#endif
