#ifndef UI_MJTILE_H
#define UI_MJTILE_H

#include <stdbool.h>

#include "ui/gfx.h"

/* Fichas de mahjong. mjtile_face dibuja la cara de una ficha del solitario
 * (índices de games/mahjong.h) centrada en (cx, cy); mjtile_riichi dibuja
 * una ficha entera del riichi (índices de games/riichi.h) con su canto. */
void mjtile_face(int f, float cx, float cy, float w, float h, float s, float depth);
/* Ficha del riichi con la esquina superior izquierda en (x, y). Boca abajo
 * si t < 0. dim oscurece la cara (por ejemplo, fichas que no se pueden tirar). */
void mjtile_riichi(int t, float x, float y, float w, float h, float depth, bool dim);
/* Ficha tumbada (girada 90 grados), como la de riichi o la llamada */
void mjtile_riichi_sideways(int t, float x, float y, float w, float h, float depth);

#endif
