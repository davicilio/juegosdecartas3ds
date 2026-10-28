#ifndef UI_TABLE_H
#define UI_TABLE_H

#include <stdbool.h>

#include "core/avatar.h"
#include "core/cards.h"
#include "ui/input.h"
#include "ui/mii.h"

/* Piezas comunes de las mesas de cuatro jugadores: los asientos (tú abajo y
 * los tres rivales a la izquierda, arriba y a la derecha), los bocadillos,
 * el mensaje de la pantalla inferior y tu mano en la pantalla táctil. */

#define TABLE_PLAYERS 4

typedef struct {
	CpuId cpu[TABLE_PLAYERS]; /* cpu[0] no se usa: eres tú */
	Mood mood[TABLE_PLAYERS];
	char bubble[TABLE_PLAYERS][28];
	float bubble_time[TABLE_PLAYERS];
	char message[96];
	float message_flash;
} Table;

void table_init(Table* t, CpuId left, CpuId top, CpuId right);
const char* table_name(const Table* t, int p);
const Avatar* table_avatar(const Table* t, int p);
void table_message(Table* t, const char* fmt, ...) __attribute__((format(printf, 2, 3)));
void table_bubble(Table* t, int p, const char* text);
void table_mood(Table* t, int p, Expression e, float seconds);
void table_update(Table* t, float dt);

/* Centro de cada asiento en la pantalla superior */
float table_seat_x(int p);
float table_seat_y(int p);
/* Asiento con su personaje, su nombre (y un texto, si lo hay) y sus cartas
 * boca abajo. turn resalta al que le toca. */
void table_draw_seat(const Table* t, int p, int cards, bool turn, const char* info);
void table_draw_bubbles(const Table* t);
/* Tu personaje abajo a la izquierda en la pantalla superior */
void table_draw_me(const Table* t, const char* info);
/* Mensaje arriba de la pantalla inferior */
void table_draw_message(const Table* t);

/* Mano en la pantalla táctil: posición de la carta i de n */
#define HAND_Y 104.0f
float hand_x(int i, int n, float scale);
/* Carta tocada (o -1) */
int hand_touched(const Input* in, int n, float scale);
/* Dibuja la mano: sel marca cartas elegidas (puede ser NULL), cursor la
 * señalada con la cruceta (-1 ninguna), playable oscurece las que no valen
 * (puede ser NULL). */
void hand_draw(const Card* cards, int n, float scale, const bool* sel, int cursor, const bool* playable);

#endif
