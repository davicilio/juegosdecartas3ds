#ifndef GAMES_PUMBA_H
#define GAMES_PUMBA_H

#include <stdbool.h>

#include "core/cards.h"

/* Pumba con la baraja española, para cuatro jugadores (el 0 es el humano).
 *
 * Cada uno empieza con seis cartas. En tu turno echas una carta del mismo
 * palo o del mismo número que la de arriba del montón. Si no puedes, robas
 * una; si la robada vale, puedes echarla.
 *   - As: el siguiente pierde el turno.
 *   - Dos: el siguiente roba dos, salvo que eche otro dos: entonces se
 *     acumulan y le toca al siguiente (cuatro, seis...).
 *   - Sota: cambia el sentido.
 *   - Caballo: comodín; se echa sobre cualquier carta y eliges el palo.
 * Con una carta hay que decir "¡Pumba!"; si te pillan sin decirlo, robas dos.
 * Gana la ronda quien se queda sin cartas y se lleva los puntos de las de los
 * demás (su número; sota y rey 10, caballo 20). La partida, a PU_GAME_POINTS. */

#define PU_PLAYERS     4
#define PU_START_HAND  6
#define PU_MAX_HAND    DECK_SIZE
#define PU_GAME_POINTS 100

typedef enum {
	PU_PHASE_PLAYING,
	PU_PHASE_CHOOSE_SUIT, /* el jugador actual ha echado un caballo */
	PU_PHASE_ROUND_OVER,
	PU_PHASE_GAME_OVER
} PuPhase;

typedef struct {
	Card deck[DECK_SIZE];
	int deck_count;
	Card discard[DECK_SIZE];
	int discard_count;
	Card hands[PU_PLAYERS][PU_MAX_HAND];
	int hand_count[PU_PLAYERS];
	int current;
	int direction;
	int suit;        /* palo que vale (el elegido tras un caballo) */
	int pending;     /* cartas acumuladas por doses que tiene que robar el actual */
	bool drew;
	int drawn_index;
	bool said[PU_PLAYERS];
	int score[PU_PLAYERS];
	int round_winner;
	int round_points;
	int dealer;
	/* Para la interfaz */
	int last_player;
	Card last_card;
	int last_penalty_player;
	int last_penalty;
	bool last_skipped;
	PuPhase phase;
	Rng rng;
} Pumba;

int pu_card_points(Card c);
void pu_init(Pumba* g, uint32_t seed);
void pu_new_game(Pumba* g);
void pu_new_round(Pumba* g);
Card pu_top(const Pumba* g);
int pu_next_player(const Pumba* g, int from);
bool pu_can_play(const Pumba* g, int player, int hand_index);
bool pu_play(Pumba* g, int hand_index);
bool pu_choose_suit(Pumba* g, int suit);
/* Roba: una carta o, si hay doses acumulados, todas las que tocan (y pasa). */
bool pu_draw(Pumba* g);
bool pu_pass(Pumba* g);
bool pu_say(Pumba* g, int player);
bool pu_catchable(const Pumba* g, int player);
bool pu_catch(Pumba* g, int player);
int pu_ai_choose(const Pumba* g, int player);
int pu_ai_suit(const Pumba* g, int player);
int pu_game_winner(const Pumba* g);

#endif
