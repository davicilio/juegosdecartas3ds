#ifndef GAMES_UNO_H
#define GAMES_UNO_H

#include <stdbool.h>
#include <stdint.h>

#include "core/rng.h"

/* Uno para cuatro jugadores (el 0 es el humano).
 *
 * Baraja de 108 cartas: en cada color (rojo, amarillo, verde y azul) un 0,
 * dos de cada número del 1 al 9 y dos de cada carta de acción (salta,
 * cambio de sentido y roba dos); además, cuatro comodines de color y cuatro
 * de roba cuatro.
 *
 * Cada uno empieza con siete cartas. En tu turno echas una carta del mismo
 * color o del mismo número o símbolo que la de arriba del montón, o un
 * comodín (eliges el color). Si no puedes o no quieres, robas una; si la que
 * robas se puede echar, puedes echarla. Salta: el siguiente pierde el turno.
 * Cambio de sentido: se juega al revés. Roba dos y roba cuatro: el siguiente
 * roba esas cartas y pierde el turno (no se acumulan).
 *
 * Al quedarte con una carta hay que decir "¡Uno!"; si te pillan sin decirlo,
 * robas dos. Gana la ronda quien se queda sin cartas y se lleva los puntos de
 * las cartas de los demás (números, su valor; acciones, 20; comodines, 50).
 * Gana la partida el primero en llegar a UNO_GAME_POINTS. */

#define UNO_PLAYERS     4
#define UNO_START_HAND  7
#define UNO_DECK_SIZE   108
#define UNO_MAX_HAND    UNO_DECK_SIZE
#define UNO_GAME_POINTS 200

typedef enum { UNO_RED, UNO_YELLOW, UNO_GREEN, UNO_BLUE, UNO_COLORS, UNO_WILD = UNO_COLORS } UnoColor;

typedef enum {
	/* 0..9 son los números */
	UNO_SKIP = 10,
	UNO_REVERSE,
	UNO_DRAW2,
	UNO_WILDCARD, /* comodín de color */
	UNO_DRAW4,    /* comodín roba cuatro */
	UNO_VALUES
} UnoValue;

typedef struct {
	uint8_t color; /* UnoColor (UNO_WILD en los comodines) */
	uint8_t value; /* UnoValue */
} UnoCard;

typedef enum {
	UNO_PHASE_PLAYING,
	UNO_PHASE_CHOOSE_COLOR, /* el jugador actual ha echado un comodín */
	UNO_PHASE_ROUND_OVER,
	UNO_PHASE_GAME_OVER
} UnoPhase;

typedef struct {
	UnoCard deck[UNO_DECK_SIZE];
	int deck_count;
	UnoCard discard[UNO_DECK_SIZE];
	int discard_count;
	UnoCard hands[UNO_PLAYERS][UNO_MAX_HAND];
	int hand_count[UNO_PLAYERS];
	int current;
	int direction;    /* 1 o -1 */
	int color;        /* color que vale ahora (el elegido tras un comodín) */
	bool drew;        /* el jugador actual ya ha robado este turno */
	int drawn_index;  /* la carta robada (la única que puede echar), o -1 */
	bool said_uno[UNO_PLAYERS];
	int score[UNO_PLAYERS];
	int round_winner;
	int round_points;
	int dealer;
	/* Para la interfaz: lo último que ha pasado */
	int last_player;
	UnoCard last_card;
	int last_penalty_player; /* quién ha tenido que robar (roba dos/cuatro o por no decir uno) */
	int last_penalty;
	bool last_skipped;
	UnoPhase phase;
	Rng rng;
} Uno;

int uno_card_points(UnoCard c);
bool uno_is_wild(UnoCard c);

void uno_init(Uno* g, uint32_t seed);
void uno_new_game(Uno* g);
void uno_new_round(Uno* g);

UnoCard uno_top(const Uno* g);
int uno_next_player(const Uno* g, int from);
bool uno_can_play(const Uno* g, int player, int hand_index);
/* Echa la carta. Si es un comodín, la partida pasa a elegir color. */
bool uno_play(Uno* g, int hand_index);
bool uno_choose_color(Uno* g, int color);
/* Roba una carta (una vez por turno). */
bool uno_draw(Uno* g);
/* Tras robar, pasar sin echar nada. */
bool uno_pass(Uno* g);
/* Decir "¡Uno!" con dos cartas antes de echar la penúltima, o con una. */
bool uno_say_uno(Uno* g, int player);
/* Otro jugador pilla a player con una carta sin haber dicho uno: roba dos. */
bool uno_catch(Uno* g, int player);
bool uno_catchable(const Uno* g, int player);

/* IA: índice de la carta a echar, o -1 para robar (o pasar si ya robó). */
int uno_ai_choose(const Uno* g, int player);
int uno_ai_color(const Uno* g, int player);
int uno_game_winner(const Uno* g);

#endif
