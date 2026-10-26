#ifndef UI_WIDGETS_H
#define UI_WIDGETS_H

#include <stdbool.h>

#include "core/cards.h"
#include "ui/gfx.h"
#include "ui/input.h"

/* Botón táctil de la pantalla inferior, que también responde a una tecla. */
typedef struct {
	float x, y, w, h;
	const char* label;
	u32 color;
	u32 key;       /* KEY_A, KEY_B... o 0 */
	bool enabled;
	float pressed; /* temporizador del efecto de pulsación */
} Button;

void button_set(Button* b, float x, float y, float w, float h, const char* label, u32 color, u32 key);
/* Devuelve true si se ha activado este fotograma (por toque o por tecla). */
bool button_update(Button* b, const Input* in);
void button_draw(const Button* b, bool highlight);

/* Carta animada: se desplaza suavemente hacia su destino y puede girarse. */
typedef struct {
	Card card;
	bool active;
	float x, y, tx, ty;
	float flip, tflip;   /* 0 boca abajo, 1 boca arriba */
	float scale, tscale;
	float angle, tangle;
	float depth;         /* profundidad en reposo */
	float delay;         /* segundos antes de empezar a moverse */
	float dim;
	bool sound;          /* sonar al empezar a moverse */
} VisCard;

void vis_spawn(VisCard* v, Card c, float x, float y, float scale, bool face_up, float delay);
void vis_target(VisCard* v, float tx, float ty, float tscale, bool face_up);
void vis_update(VisCard* v, float dt);
bool vis_moving(const VisCard* v);
void vis_draw(const VisCard* v);

/* Interpolación exponencial independiente del framerate. */
float approach(float value, float target, float speed, float dt);
float ease_out_back(float t);

#endif
