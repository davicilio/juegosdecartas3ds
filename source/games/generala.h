#ifndef GAMES_GENERALA_H
#define GAMES_GENERALA_H

#include <stdbool.h>

#include "core/rng.h"

/* Generala: cinco dados y hasta tres tiradas por turno, guardando los que
 * quieras. Al final del turno se apunta la jugada en una casilla libre (o se
 * tacha con 0). Casillas: del 1 al 6 (suma de esos dados), escalera (1-5 o
 * 2-6) 20, full (trío y pareja) 30, póker (cuatro iguales) 40, generala
 * (cinco iguales) 50 y doble generala 100. Hecha en la primera tirada
 * ("servida") la escalera, el full y el póker valen 5 más, y la generala
 * servida gana la partida en el acto. Gana quien más puntos sume al llenar
 * todas las casillas. */

#define GE_DICE     5
#define GE_PLAYERS  4
#define GE_ROLLS    3

enum {
	GE_ONES, GE_TWOS, GE_THREES, GE_FOURS, GE_FIVES, GE_SIXES,
	GE_STRAIGHT, GE_FULL, GE_POKER, GE_GENERALA, GE_DOUBLE,
	GE_CATS
};

typedef enum { GE_ROLL, GE_OVER } GePhase;

typedef struct {
	int players;
	int dice[GE_DICE];
	bool hold[GE_DICE];
	int rolls;               /* tiradas hechas este turno (0..3) */
	int score[GE_PLAYERS][GE_CATS]; /* -1 = libre */
	int current;
	int turns;
	int winner;
	bool served_win;         /* ganó con generala servida */
	GePhase phase;
	Rng rng;
} Generala;

const char* ge_cat_name(int cat);
void ge_init(Generala* g, int players, uint32_t seed);
/* Tira los dados no guardados (values != NULL fija el resultado, para tests) */
bool ge_roll(Generala* g, const int* values);
void ge_toggle_hold(Generala* g, int i);
/* Puntos que daría la casilla con los dados actuales */
int ge_points(const Generala* g, int cat);
/* Lo que se apuntaría de verdad (la doble generala sin generala vale 0) */
int ge_value(const Generala* g, int cat);
bool ge_can_score(const Generala* g, int cat);
/* Apunta la casilla y pasa el turno */
bool ge_score(Generala* g, int cat);
int ge_total(const Generala* g, int p);

/* IA: decide qué dados guardar (true si quiere volver a tirar) y qué casilla apuntar */
bool ge_ai_hold(Generala* g);
int ge_ai_category(const Generala* g);

#endif
