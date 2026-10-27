#ifndef CORE_CARDS_H
#define CORE_CARDS_H

#include <stdbool.h>
#include <stdint.h>

#include "core/rng.h"

/* Baraja española de 40 cartas: 1-7, sota (10), caballo (11) y rey (12). */

typedef enum {
	SUIT_OROS,
	SUIT_COPAS,
	SUIT_ESPADAS,
	SUIT_BASTOS,
	NUM_SUITS
} Suit;

#define RANKS_PER_SUIT 10
#define DECK_SIZE      (NUM_SUITS * RANKS_PER_SUIT)

#define RANK_SOTA    10
#define RANK_CABALLO 11
#define RANK_REY     12

typedef struct {
	uint8_t suit; /* Suit */
	uint8_t rank; /* 1..7, 10, 11, 12 */
} Card;

typedef struct {
	Card cards[DECK_SIZE];
	int count;
} Deck;

/* Posición de la carta dentro de su palo: 1..7 -> 0..6, sota 7, caballo 8, rey 9. */
int card_pos(Card c);
int rank_from_pos(int pos);
/* Índice único 0..39 (palo * 10 + posición); coincide con el orden del atlas. */
int card_index(Card c);
Card card_from_index(int index);
Card card_make(Suit suit, int rank);
bool card_equal(Card a, Card b);
bool card_valid(Card c);

const char* suit_name(Suit suit);      /* "oros" */
const char* rank_name(int rank);       /* "as", "dos", ..., "rey" */
/* Escribe "as de oros", "sota de copas"... en buf. */
void card_describe(Card c, char* buf, int size);
/* Igual, con artículo: "el as de oros", "la sota de copas". */
void card_describe_article(Card c, char* buf, int size);

void deck_init(Deck* deck);
void deck_shuffle(Deck* deck, Rng* rng);
/* Saca la carta de arriba. Devuelve false si el mazo está vacío. */
bool deck_draw(Deck* deck, Card* out);

#endif
