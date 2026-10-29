#ifndef GAMES_AS23_H
#define GAMES_AS23_H

#include <stdbool.h>

#include "core/cards.h"

/* As, 2, 3, para 2, 3 o 4 jugadores (el 0 es el humano).
 *
 * Se reparten todas las cartas boca abajo; nadie sabe qué tiene en su mazo.
 * Por turnos, cada uno echa al montón del centro la carta de arriba de su
 * mazo mientras se va contando en voz alta: as, dos, tres... siete, sota,
 * caballo, rey, y otra vez as. Si la carta coincide con lo que se dice, quien
 * la ha echado se lleva todo el montón debajo de su mazo y se vuelve a contar
 * desde el as. Gana el primero que se queda sin cartas. */

#define AS23_MAX_PLAYERS 4
/* Si una partida se alarga tanto, gana quien menos cartas tenga */
#define AS23_MAX_PLAYS   3000

typedef struct {
	int players;
	Card stack[AS23_MAX_PLAYERS][DECK_SIZE]; /* la de arriba es la última */
	int stack_count[AS23_MAX_PLAYERS];
	Card pile[DECK_SIZE];
	int pile_count;
	int count;   /* posición que toca decir: 0 = as ... 9 = rey */
	int current;
	int plays;
	int winner;  /* -1 mientras se juega */
	/* Última jugada, para la interfaz */
	int last_player;
	int last_said; /* rango dicho */
	Card last_card;
	bool last_match;
	int last_taken; /* cartas que se llevó */
	Rng rng;
} As23;

void as23_init(As23* g, int players, uint32_t seed);
void as23_new_game(As23* g);
/* Rango que hay que decir ahora (1..7, 10, 11, 12) */
int as23_called(const As23* g);
/* El jugador actual echa su carta de arriba */
bool as23_play(As23* g);
bool as23_over(const As23* g);

#endif
