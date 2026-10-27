#ifndef UI_FLYERS_H
#define UI_FLYERS_H

#include <stdbool.h>

#include "ui/widgets.h"

/* Cartas "de paso": vuelan hasta un destino y desaparecen al llegar (a un
 * montón de bazas, fuera de la pantalla...). Cada una se dibuja en una de
 * las dos pantallas. */

typedef enum {
	FLY_TOP,
	FLY_BOTTOM
} FlyScreen;

void flyers_clear(void);
/* Copia v y la manda volando a (tx, ty). */
void flyers_add(const VisCard* v, float tx, float ty, float delay, bool face_up, FlyScreen screen);
void flyers_update(float dt);
void flyers_draw(FlyScreen screen);
bool flyers_busy(void);

#endif
