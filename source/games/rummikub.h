#ifndef GAMES_RUMMIKUB_H
#define GAMES_RUMMIKUB_H

#include <stdbool.h>
#include <stdint.h>

#include "core/rng.h"

/* Rummikub para cuatro jugadores (el 0 es el humano).
 *
 * 106 fichas: los números del 1 al 13 en cuatro colores, dos de cada, y dos
 * comodines. Cada jugador empieza con 14 fichas. En tu turno bajas a la mesa
 * jugadas de al menos tres fichas:
 *   - escalera: números seguidos del mismo color (4-5-6 rojos);
 *   - grupo: el mismo número en colores distintos (7 rojo, 7 azul, 7 negro).
 * El comodín sustituye a cualquier ficha. La primera vez hay que bajar
 * jugadas que sumen al menos 30 puntos solo con tus fichas. Después también
 * puedes añadir fichas a las jugadas de la mesa (si siguen siendo válidas).
 * Si no bajas nada, robas una ficha.
 *
 * Gana la ronda quien se queda sin fichas: suma los puntos de las fichas que
 * les quedan a los demás (el comodín vale 30) y ellos los restan.
 *
 * Esta versión no permite deshacer o reorganizar las jugadas de la mesa;
 * solo añadirles fichas. */

#define RK_PLAYERS    4
#define RK_TILES      106
#define RK_START      14
#define RK_MAX_RACK   RK_TILES
#define RK_MAX_SETS   40
#define RK_MAX_SET    13
#define RK_MIN_MELD   30
#define RK_JOKER      4 /* color del comodín */
#define RK_COLORS     4
#define RK_JOKER_PENALTY 30

typedef struct {
	uint8_t color; /* 0..3, o RK_JOKER */
	uint8_t number; /* 1..13 (0 en el comodín) */
} RkTile;

typedef struct {
	RkTile tiles[RK_MAX_SET]; /* en orden: las escaleras de menor a mayor */
	int n;
} RkSet;

typedef enum {
	RK_PHASE_PLAYING,
	RK_PHASE_ROUND_OVER
} RkPhase;

typedef struct {
	RkTile pool[RK_TILES];
	int pool_count;
	RkTile racks[RK_PLAYERS][RK_MAX_RACK];
	int rack_count[RK_PLAYERS];
	bool melded[RK_PLAYERS]; /* ya ha hecho la primera jugada de 30 */
	RkSet table[RK_MAX_SETS];
	int table_count;
	int current;
	int score[RK_PLAYERS];
	int round_winner;
	int round_points;
	int rounds;
	/* Turno en curso: copia del principio para deshacer y lo bajado */
	RkSet saved_table[RK_MAX_SETS];
	int saved_table_count;
	RkTile saved_rack[RK_MAX_RACK];
	int saved_rack_count;
	int turn_points;  /* puntos bajados en jugadas nuevas este turno */
	int turn_tiles;   /* fichas que ha puesto este turno */
	int last_changed; /* última jugada de la mesa tocada (para resaltarla) */
	int idle_turns;   /* turnos seguidos sin poner fichas ni robar */
	RkPhase phase;
	Rng rng;
} Rummikub;

/* Ordena y comprueba una jugada; si es válida, la deja ordenada en out. */
bool rk_make_set(const RkTile* tiles, int n, RkSet* out);
/* Puntos de una jugada válida (el comodín vale lo que sustituye). */
int rk_set_points(const RkSet* s);
int rk_tile_penalty(RkTile t);

void rk_init(Rummikub* g, uint32_t seed);
void rk_new_round(Rummikub* g);

/* Baja como jugada nueva las fichas del atril con esos índices. */
bool rk_play_set(Rummikub* g, const int* idx, int n);
/* Añade la ficha idx del atril a la jugada set de la mesa. */
bool rk_can_add(const Rummikub* g, int set, int idx);
bool rk_add(Rummikub* g, int set, int idx);
/* Vuelve al principio del turno. */
void rk_undo(Rummikub* g);
/* Termina el turno. Si no ha bajado nada (o no llega a 30 en la primera
 * jugada) se deshace lo hecho y roba. Devuelve true si ha robado. */
bool rk_end_turn(Rummikub* g);
/* Lo bajado en este turno ya vale para terminar sin robar */
bool rk_turn_valid(const Rummikub* g);

/* IA: hace su turno completo (bajar, añadir o robar). Devuelve las fichas
 * que ha puesto en la mesa. */
int rk_ai_turn(Rummikub* g);
/* Ordena el atril por color y número, o por número y color */
void rk_sort_rack(Rummikub* g, int player, bool by_number);

#endif
