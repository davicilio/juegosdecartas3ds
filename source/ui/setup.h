#ifndef UI_SETUP_H
#define UI_SETUP_H

#include <stdbool.h>

#include "ui/input.h"
#include "ui/widgets.h"

/* Pantalla de antes de jugar: unas opciones (jugadores, dificultad...) que se
 * cambian tocando o con la cruceta, y el botón de jugar. Lo que se elige aquí
 * no se puede cambiar a mitad de partida (para eso, Reiniciar en la pausa). */

#define SETUP_MAX 3

typedef struct {
	const char* label;
	const char* const* names;
	int count;
	int* value;
} SetupOption;

typedef struct {
	const char* title;
	const char* text;
	SetupOption opt[SETUP_MAX];
	int n;
	int cursor;
	Button prev[SETUP_MAX], next[SETUP_MAX], play;
} Setup;

void setup_init(Setup* s, const char* title, const char* text);
void setup_add(Setup* s, const char* label, const char* const* names, int count, int* value);
/* Devuelve true al pulsar Jugar */
bool setup_update(Setup* s, const Input* in);
void setup_draw_top(const Setup* s);
void setup_draw_bottom(const Setup* s);

#endif
