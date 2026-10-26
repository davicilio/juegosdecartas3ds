#ifndef GAMES_SIETEYMEDIA_H
#define GAMES_SIETEYMEDIA_H

#include <stdbool.h>

#include "core/cards.h"

/* Siete y media: el jugador se enfrenta a la banca.
 * Las cartas del 1 al 7 valen su número y las figuras medio punto.
 * Gana quien más se acerque a 7,5 sin pasarse; los empates son para la banca.
 * Siete y media "real" (un 7 y una figura con solo dos cartas) cobra el doble.
 *
 * Todos los valores se guardan en medios puntos (7,5 = 15) para evitar floats. */

#define SYM_MAX_HAND    16
#define SYM_LIMIT       15  /* 7,5 en medios puntos */
#define SYM_START_CHIPS 100
#define SYM_MIN_BET     5
#define SYM_MAX_BET     50
#define SYM_BET_STEP    5

typedef enum {
	SYM_PHASE_BETTING,    /* eligiendo apuesta */
	SYM_PHASE_PLAYER,     /* turno del jugador: pedir o plantarse */
	SYM_PHASE_BANK,       /* la banca roba cartas */
	SYM_PHASE_ROUND_OVER, /* mano terminada, se muestra el resultado */
	SYM_PHASE_GAME_OVER   /* el jugador se ha quedado sin fichas */
} SymPhase;

typedef enum {
	SYM_OUTCOME_NONE,
	SYM_OUTCOME_PLAYER_BUST, /* el jugador se pasa */
	SYM_OUTCOME_BANK_BUST,   /* la banca se pasa */
	SYM_OUTCOME_PLAYER_WINS,
	SYM_OUTCOME_BANK_WINS,   /* incluye empates */
	SYM_OUTCOME_REAL         /* siete y media real del jugador: paga doble */
} SymOutcome;

typedef struct {
	Card cards[SYM_MAX_HAND];
	int count;
} SymHand;

typedef struct {
	Deck deck;
	SymHand player;
	SymHand bank;
	int chips;
	int bet;
	int last_delta; /* fichas ganadas (+) o perdidas (-) en la última mano */
	int rounds_played;
	int rounds_won;
	SymPhase phase;
	SymOutcome outcome;
	Rng rng;
} SieteYMedia;

int sym_card_halves(Card c);
int sym_hand_halves(const SymHand* hand);
/* Formatea un valor en medios puntos: 15 -> "7½" (UTF-8). */
void sym_format_value(int halves, char* buf, int size);

void sym_init(SieteYMedia* g, uint32_t seed);
void sym_change_bet(SieteYMedia* g, int delta);
bool sym_deal(SieteYMedia* g);
bool sym_hit(SieteYMedia* g);
bool sym_stand(SieteYMedia* g);
/* Decisión de la IA de la banca (solo ve las cartas descubiertas del jugador). */
bool sym_bank_wants_card(const SieteYMedia* g);
/* Avanza un paso del turno de la banca. Devuelve true si ha robado carta. */
bool sym_bank_step(SieteYMedia* g);
bool sym_next_round(SieteYMedia* g);

#endif
