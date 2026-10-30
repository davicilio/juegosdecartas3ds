#ifndef GAMES_TOMA6_H
#define GAMES_TOMA6_H

#include <stdbool.h>
#include <stdint.h>

#include "core/rng.h"

/* Toma 6, para cuatro jugadores (el 0 es el humano).
 *
 * Hay 104 cartas numeradas del 1 al 104, cada una con sus cabezas de buey:
 * una la mayoría, dos las acabadas en 5, tres las de 10, cinco las de números
 * repetidos (11, 22...) y siete el 55. Se reparten diez a cada uno y se ponen
 * cuatro en la mesa, empezando cuatro filas.
 *
 * Cada turno todos eligen una carta a la vez y se descubren. Se colocan de la
 * más baja a la más alta: cada una va a la fila cuya última carta es la más
 * alta de las que son menores que ella. Si es la sexta de la fila, quien la
 * echa se lleva las cinco y la suya empieza la fila. Si es más baja que el
 * final de todas las filas, se lleva la fila que quiera.
 *
 * Al acabar las diez cartas se cuentan las cabezas. Cuando alguien llega a
 * TM_END, gana quien menos tenga. */

#define TM_PLAYERS 4
#define TM_HAND    10
#define TM_ROWS    4
#define TM_ROW_MAX 5
#define TM_CARDS   104
#define TM_END     66

typedef enum {
	TM_CHOOSE,    /* cada uno elige carta */
	TM_PLACE,     /* se van colocando, de una en una */
	TM_TAKE_ROW,  /* chooser tiene que llevarse una fila */
	TM_ROUND_OVER,
	TM_GAME_OVER
} TmPhase;

typedef struct {
	uint8_t hand[TM_PLAYERS][TM_HAND];
	int hand_n[TM_PLAYERS];
	uint8_t row[TM_ROWS][TM_ROW_MAX + 1];
	int row_n[TM_ROWS];
	int score[TM_PLAYERS];      /* cabezas acumuladas */
	int round_heads[TM_PLAYERS];
	uint8_t played[TM_PLAYERS]; /* carta elegida este turno (0 = aún no) */
	int order[TM_PLAYERS];      /* jugadores por orden de su carta */
	int placed;                 /* cuántas se han colocado ya este turno */
	int chooser;
	/* Lo último que ha pasado, para la interfaz */
	int last_player, last_row, last_taken;
	bool last_took;
	TmPhase phase;
	Rng rng;
} Toma6;

int tm_heads(int card);
int tm_row_heads(const Toma6* g, int row);
void tm_init(Toma6* g, uint32_t seed);
void tm_new_game(Toma6* g);
void tm_new_round(Toma6* g);
/* El jugador p elige la carta idx de su mano */
bool tm_choose(Toma6* g, int p, int idx);
/* Fila a la que iría una carta (-1 si es más baja que todas) */
int tm_target_row(const Toma6* g, int card);
/* Coloca la siguiente carta del turno */
bool tm_step(Toma6* g);
bool tm_take_row(Toma6* g, int row);
int tm_winner(const Toma6* g);

int tm_ai_card(const Toma6* g, int p);
int tm_ai_row(const Toma6* g);

#endif
