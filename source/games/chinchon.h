#ifndef GAMES_CHINCHON_H
#define GAMES_CHINCHON_H

#include <stdbool.h>

#include "core/cards.h"

/* Chinchón a dos jugadores (el 0 es el humano, el 1 la CPU).
 *
 * Cada jugador tiene siete cartas. En su turno roba una (del mazo o la de
 * arriba del montón de descartes) y descarta otra. Se buscan combinaciones:
 * tríos o cuartetos del mismo número, y escaleras de tres o más cartas
 * seguidas del mismo palo (1-7, sota, caballo, rey). Se puede cerrar al
 * descartar si las siete cartas que quedan están todas combinadas o queda una
 * sola suelta que valga CH_CLOSE_MAX o menos.
 *
 * Al cerrar, cada jugador suma el valor de sus cartas sueltas (las figuras
 * valen 10); el que no ha cerrado puede antes "ligar" sus cartas sueltas a
 * las combinaciones del que cierra. Cerrar con todo combinado resta 10 puntos
 * y cerrar con una escalera de siete del mismo palo ("chinchón") gana la
 * partida. Pierde quien pase de CH_LIMIT puntos. */

#define CH_PLAYERS   2
#define CH_HAND_SIZE 7
#define CH_MAX_HAND  (CH_HAND_SIZE + 1)
#define CH_LIMIT     100
#define CH_CLOSE_MAX 3
#define CH_MAX_MELDS 4

typedef enum {
	CH_PHASE_DRAW,    /* el jugador actual tiene que robar */
	CH_PHASE_DISCARD, /* tiene ocho cartas: descarta (o cierra) */
	CH_PHASE_ROUND_OVER,
	CH_PHASE_GAME_OVER
} ChPhase;

/* Mejor forma de agrupar una mano. Las máscaras son de bits sobre los
 * índices de la mano. */
typedef struct {
	uint16_t melds[CH_MAX_MELDS];
	int meld_count;
	uint16_t loose;
	int loose_count;
	int loose_value;
} ChAnalysis;

typedef struct {
	Deck deck;
	Card discard[DECK_SIZE];
	int discard_count;
	Card hands[CH_PLAYERS][CH_MAX_HAND];
	int hand_count[CH_PLAYERS];
	int score[CH_PLAYERS];
	int dealer;
	int current;
	int round;
	int turns;
	ChPhase phase;
	/* Resultado de la ronda */
	int closer;
	bool chinchon;
	bool menos_diez;
	int round_points[CH_PLAYERS];
	ChAnalysis final[CH_PLAYERS];
	uint16_t laid_off; /* cartas sueltas del que no cierra ligadas al que cierra */
	/* Última acción, para la interfaz */
	int last_player;
	bool last_from_discard;
	Card last_discarded;
	int reshuffles;
	Rng rng;
} Chinchon;

int ch_card_value(Card c);
void ch_analyze(const Card* cards, int n, ChAnalysis* out);
/* Ordena la mano: combinaciones primero (en orden) y después las sueltas. */
void ch_sort_hand(Card* cards, int n);
/* ¿Puede el jugador actual cerrar descartando la carta hand_index? */
bool ch_can_close(const Chinchon* g, int hand_index);

void ch_init(Chinchon* g, uint32_t seed);
void ch_start_round(Chinchon* g);
bool ch_draw_deck(Chinchon* g);
bool ch_draw_discard(Chinchon* g);
bool ch_discard(Chinchon* g, int hand_index);
bool ch_close(Chinchon* g, int hand_index);
bool ch_next_round(Chinchon* g);
/* Ganador de la partida, o -1 si aún no ha terminado. */
int ch_winner(const Chinchon* g);

/* IA: ¿roba del descarte? y ¿qué carta descarta? (cierra si puede). */
bool ch_ai_take_discard(const Chinchon* g, int player);
int ch_ai_discard(const Chinchon* g, int player, bool* close_out);

#endif
