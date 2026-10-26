#include "core/cards.h"

#include <stdio.h>

int card_pos(Card c)
{
	return c.rank <= 7 ? c.rank - 1 : c.rank - 3;
}

int rank_from_pos(int pos)
{
	return pos <= 6 ? pos + 1 : pos + 3;
}

int card_index(Card c)
{
	return c.suit * RANKS_PER_SUIT + card_pos(c);
}

Card card_from_index(int index)
{
	Card c;
	c.suit = (uint8_t)(index / RANKS_PER_SUIT);
	c.rank = (uint8_t)rank_from_pos(index % RANKS_PER_SUIT);
	return c;
}

Card card_make(Suit suit, int rank)
{
	Card c;
	c.suit = (uint8_t)suit;
	c.rank = (uint8_t)rank;
	return c;
}

bool card_equal(Card a, Card b)
{
	return a.suit == b.suit && a.rank == b.rank;
}

bool card_valid(Card c)
{
	return c.suit < NUM_SUITS && ((c.rank >= 1 && c.rank <= 7) || (c.rank >= 10 && c.rank <= 12));
}

const char* suit_name(Suit suit)
{
	static const char* names[NUM_SUITS] = { "oros", "copas", "espadas", "bastos" };
	return suit < NUM_SUITS ? names[suit] : "?";
}

const char* rank_name(int rank)
{
	static const char* names[] = {
		"?", "as", "dos", "tres", "cuatro", "cinco", "seis", "siete",
		"?", "?", "sota", "caballo", "rey"
	};
	return (rank >= 1 && rank <= 12) ? names[rank] : "?";
}

void card_describe(Card c, char* buf, int size)
{
	snprintf(buf, size, "%s de %s", rank_name(c.rank), suit_name(c.suit));
}

void deck_init(Deck* deck)
{
	for (int i = 0; i < DECK_SIZE; i++)
		deck->cards[i] = card_from_index(i);
	deck->count = DECK_SIZE;
}

void deck_shuffle(Deck* deck, Rng* rng)
{
	/* Fisher-Yates */
	for (int i = deck->count - 1; i > 0; i--) {
		int j = rng_range(rng, i + 1);
		Card tmp = deck->cards[i];
		deck->cards[i] = deck->cards[j];
		deck->cards[j] = tmp;
	}
}

bool deck_draw(Deck* deck, Card* out)
{
	if (deck->count <= 0)
		return false;
	*out = deck->cards[--deck->count];
	return true;
}
