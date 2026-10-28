#ifndef GAMES_TREINTAYUNA_H
#define GAMES_TREINTAYUNA_H

#include <stdbool.h>

#include "core/cards.h"

/* Treinta y una, para cuatro jugadores (el 0 es el humano), con tres vidas.
 *
 * Cada uno recibe tres cartas y otras tres se ponen boca arriba en la mesa.
 * La mano vale la suma de las cartas de un mismo palo (as 11, figuras 10, el
 * resto su número); se cuenta el palo que más sume, hasta 31. En tu turno
 * cambias una carta tuya por una de la mesa, cambias las tres a la vez o te
 * plantas: entonces los demás juegan una vez más y se enseñan las manos. La
 * mano más baja pierde una vida. Quien llega a 31 lo enseña al momento y
 * todos los demás pierden una vida. Gana el último que siga vivo. */

#define TYU_PLAYERS 4
#define TYU_LIVES   3
#define TYU_CARDS   3

typedef enum {
	TYU_PHASE_PLAYING,
	TYU_PHASE_REVEAL,
	TYU_PHASE_GAME_OVER
} TyuPhase;

typedef struct {
	Deck deck;
	Card hands[TYU_PLAYERS][TYU_CARDS];
	Card table[TYU_CARDS];
	int lives[TYU_PLAYERS];
	int dealer;
	int current;
	int knocker;      /* quien se ha plantado (-1 nadie) */
	int turns_left;   /* turnos que quedan tras plantarse */
	int turns_played; /* turnos jugados en la ronda */
	bool lost[TYU_PLAYERS];
	int thirty_one;   /* quien ha hecho 31 (-1 nadie) */
	TyuPhase phase;
	Rng rng;
} TreintaYUna;

int tyu_card_value(Card c);
int tyu_hand_value(const Card* cards);

void tyu_init(TreintaYUna* g, uint32_t seed);
void tyu_new_game(TreintaYUna* g);
void tyu_new_round(TreintaYUna* g);
bool tyu_alive(const TreintaYUna* g, int player);
bool tyu_swap(TreintaYUna* g, int hand_index, int table_index);
bool tyu_swap_all(TreintaYUna* g);
/* Plantarse (no en el primer turno de la ronda ni si ya se ha plantado alguien) */
bool tyu_can_knock(const TreintaYUna* g);
bool tyu_knock(TreintaYUna* g);
/* IA: hace su jugada */
void tyu_ai_turn(TreintaYUna* g);
int tyu_winner(const TreintaYUna* g);

#endif
