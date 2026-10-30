#ifndef GAMES_VIDEOPOKER_H
#define GAMES_VIDEOPOKER_H

#include <stdbool.h>

#include "games/poker.h"

/* Videopóker "Jacks or Better", como las máquinas de los bares y casinos.
 *
 * Apuestas de 1 a 5 créditos y recibes cinco cartas. Marcas las que quieres
 * guardar y las demás se cambian una sola vez. Paga según la jugada final,
 * multiplicado por la apuesta; la pareja tiene que ser de jotas o más alta. */

#define VP_START 100
#define VP_MAX_BET 5

enum {
	VP_ROYAL, VP_STRAIGHT_FLUSH, VP_QUADS, VP_FULL, VP_FLUSH, VP_STRAIGHT, VP_TRIPS, VP_TWO_PAIR, VP_JACKS,
	VP_NOTHING, VP_HANDS
};

typedef enum { VP_BET, VP_HOLD, VP_RESULT } VpPhase;

typedef struct {
	PkCard deck[52];
	int deck_n;
	PkCard hand[5];
	bool held[5];
	int credits;
	int bet;
	int result; /* VP_ROYAL ... VP_NOTHING */
	int win;
	int hands, best_win;
	VpPhase phase;
	Rng rng;
} VideoPoker;

/* Lo que paga cada jugada por crédito apostado (la escalera real con 5
 * créditos paga 800 por crédito) */
int vp_pay(int hand, int bet);
const char* vp_hand_name(int hand);
int vp_rank(const PkCard* hand);

void vp_init(VideoPoker* g, uint32_t seed);
bool vp_deal(VideoPoker* g);
bool vp_toggle(VideoPoker* g, int i);
bool vp_draw(VideoPoker* g);
/* Consejo: qué cartas guardar para ganar más de media (por simulación) */
void vp_advice(VideoPoker* g, bool* hold);

#endif
