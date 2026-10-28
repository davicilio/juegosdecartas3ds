#ifndef GAMES_CUCU_H
#define GAMES_CUCU_H

#include <stdbool.h>

#include "core/cards.h"

/* Cucú, para cuatro jugadores (el 0 es el humano), con tres vidas cada uno.
 *
 * En cada ronda se reparte una carta boca abajo a cada jugador. Empezando por
 * el de después del que reparte, cada uno decide si se queda su carta o la
 * cambia con la del siguiente. Quien tiene un rey dice "¡cucú!" y no se le
 * puede cambiar. El que reparte, el último, puede cambiar su carta por la de
 * arriba del mazo. Al final se enseñan las cartas y la más baja (as, 2 ...
 * 7, sota, caballo, rey) pierde una vida. Gana el último que siga vivo. */

#define CUCU_PLAYERS 4
#define CUCU_LIVES   3

typedef enum {
	CUCU_PHASE_DECIDE, /* el jugador actual decide */
	CUCU_PHASE_REVEAL, /* cartas a la vista: se ha perdido una vida */
	CUCU_PHASE_GAME_OVER
} CucuPhase;

typedef struct {
	Deck deck;
	Card card[CUCU_PLAYERS];
	int lives[CUCU_PLAYERS];
	int dealer;
	int current;
	bool lost[CUCU_PLAYERS]; /* ha perdido vida en esta ronda */
	/* Último cambio, para la interfaz: quién, con quién y si se bloqueó */
	int last_player, last_with;
	bool last_blocked;
	CucuPhase phase;
	Rng rng;
} Cucu;

int cucu_value(Card c);
void cucu_init(Cucu* g, uint32_t seed);
void cucu_new_game(Cucu* g);
void cucu_new_round(Cucu* g);
bool cucu_alive(const Cucu* g, int player);
/* Con quién cambiaría el jugador actual (-1: el mazo, si reparte). */
int cucu_partner(const Cucu* g);
/* El jugador actual se queda (swap = false) o cambia. Si el otro tiene un
 * rey, el cambio no se hace (last_blocked). */
bool cucu_decide(Cucu* g, bool swap);
bool cucu_ai_swap(Cucu* g);
int cucu_winner(const Cucu* g);

#endif
