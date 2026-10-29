#include "games/blackjack.h"

#include <stddef.h>

int bj_card_value(PkCard c)
{
	if (c.rank == 14)
		return 11;
	return c.rank > 10 ? 10 : c.rank;
}

int bj_total(const BjHand* h, bool* soft)
{
	int total = 0, aces = 0;
	for (int i = 0; i < h->n; i++) {
		total += bj_card_value(h->cards[i]);
		aces += h->cards[i].rank == 14;
	}
	while (total > 21 && aces > 0) {
		total -= 10;
		aces--;
	}
	if (soft)
		*soft = aces > 0;
	return total;
}

bool bj_is_blackjack(const BjHand* h)
{
	return h->n == 2 && bj_total(h, NULL) == 21;
}

static void shuffle_shoe(Blackjack* g)
{
	g->shoe_count = 0;
	for (int d = 0; d < BJ_DECKS; d++)
		for (int s = 0; s < 4; s++)
			for (int r = 2; r <= 14; r++)
				g->shoe[g->shoe_count++] = (PkCard){ (uint8_t)r, (uint8_t)s };
	for (int i = g->shoe_count - 1; i > 0; i--) {
		int j = rng_range(&g->rng, i + 1);
		PkCard t = g->shoe[i];
		g->shoe[i] = g->shoe[j];
		g->shoe[j] = t;
	}
}

static PkCard draw(Blackjack* g)
{
	if (g->shoe_count == 0)
		shuffle_shoe(g);
	return g->shoe[--g->shoe_count];
}

void bj_reset(Blackjack* g)
{
	g->chips = BJ_START;
	g->bet = BJ_MIN_BET * 5;
	g->net = 0;
	g->hand_count = 0;
	g->dealer.n = 0;
	g->phase = BJ_BETTING;
}

void bj_init(Blackjack* g, uint32_t seed)
{
	rng_seed(&g->rng, seed);
	shuffle_shoe(g);
	g->hands_won = g->hands_played = 0;
	bj_reset(g);
}

void bj_set_bet(Blackjack* g, int bet)
{
	if (bet < BJ_MIN_BET)
		bet = BJ_MIN_BET;
	if (bet > g->chips)
		bet = g->chips;
	g->bet = bet;
}

static void settle(Blackjack* g)
{
	int dealer = bj_total(&g->dealer, NULL);
	bool dealer_bj = bj_is_blackjack(&g->dealer);
	g->net = 0;
	for (int k = 0; k < g->hand_count; k++) {
		BjHand* h = &g->hands[k];
		int me = bj_total(h, NULL);
		bool bj = bj_is_blackjack(h) && g->hand_count == 1;
		int r;
		if (me > 21)
			r = -h->bet;
		else if (bj && !dealer_bj)
			r = h->bet * 3 / 2;
		else if (dealer_bj && !bj)
			r = -h->bet;
		else if (dealer > 21 || me > dealer)
			r = h->bet;
		else if (me == dealer)
			r = 0;
		else
			r = -h->bet;
		h->result = r;
		g->chips += h->bet + r; /* la apuesta ya se había apartado */
		g->net += r;
	}
	g->hands_played++;
	if (g->net > 0)
		g->hands_won++;
	g->phase = g->chips < BJ_MIN_BET ? BJ_BROKE : BJ_RESULT;
	if (g->bet > g->chips)
		g->bet = g->chips;
}

/* Pasa a la siguiente mano o a la banca */
static void advance(Blackjack* g)
{
	while (g->active < g->hand_count && g->hands[g->active].done)
		g->active++;
	if (g->active < g->hand_count)
		return;
	/* Si todas se han pasado, la banca no juega */
	bool any = false;
	for (int k = 0; k < g->hand_count; k++)
		any |= bj_total(&g->hands[k], NULL) <= 21;
	if (!any) {
		settle(g);
		return;
	}
	g->phase = BJ_DEALER;
}

bool bj_deal(Blackjack* g)
{
	if ((g->phase != BJ_BETTING && g->phase != BJ_RESULT) || g->chips < BJ_MIN_BET)
		return false;
	bj_set_bet(g, g->bet);
	/* Se baraja el zapato cuando queda menos de una cuarta parte */
	if (g->shoe_count < BJ_SHOE / 4)
		shuffle_shoe(g);
	g->hand_count = 1;
	g->active = 0;
	BjHand* h = &g->hands[0];
	h->n = 0;
	h->bet = g->bet;
	h->doubled = h->done = false;
	h->result = 0;
	g->chips -= g->bet;
	g->dealer.n = 0;
	h->cards[h->n++] = draw(g);
	g->dealer.cards[g->dealer.n++] = draw(g);
	h->cards[h->n++] = draw(g);
	g->dealer.cards[g->dealer.n++] = draw(g);
	g->phase = BJ_PLAYING;
	/* Blackjack de entrada (tuyo o de la banca): se resuelve al momento */
	if (bj_is_blackjack(h) || bj_is_blackjack(&g->dealer)) {
		h->done = true;
		settle(g);
	}
	return true;
}

bool bj_hit(Blackjack* g)
{
	if (g->phase != BJ_PLAYING)
		return false;
	BjHand* h = &g->hands[g->active];
	if (h->n >= BJ_MAX_CARDS)
		return false;
	h->cards[h->n++] = draw(g);
	if (bj_total(h, NULL) >= 21) {
		h->done = true;
		advance(g);
	}
	return true;
}

bool bj_stand(Blackjack* g)
{
	if (g->phase != BJ_PLAYING)
		return false;
	g->hands[g->active].done = true;
	advance(g);
	return true;
}

bool bj_can_double(const Blackjack* g)
{
	const BjHand* h = &g->hands[g->active];
	return g->phase == BJ_PLAYING && h->n == 2 && g->chips >= h->bet;
}

bool bj_double(Blackjack* g)
{
	if (!bj_can_double(g))
		return false;
	BjHand* h = &g->hands[g->active];
	g->chips -= h->bet;
	h->bet *= 2;
	h->doubled = true;
	h->cards[h->n++] = draw(g);
	h->done = true;
	advance(g);
	return true;
}

bool bj_can_split(const Blackjack* g)
{
	const BjHand* h = &g->hands[0];
	return g->phase == BJ_PLAYING && g->hand_count == 1 && h->n == 2 &&
	       bj_card_value(h->cards[0]) == bj_card_value(h->cards[1]) && g->chips >= h->bet;
}

bool bj_split(Blackjack* g)
{
	if (!bj_can_split(g))
		return false;
	BjHand* a = &g->hands[0];
	BjHand* b = &g->hands[1];
	*b = *a;
	g->chips -= a->bet;
	a->n = b->n = 1;
	b->cards[0] = a->cards[1];
	a->cards[a->n++] = draw(g);
	b->cards[b->n++] = draw(g);
	g->hand_count = 2;
	/* Con ases separados solo se recibe una carta en cada mano */
	if (a->cards[0].rank == 14)
		a->done = b->done = true;
	advance(g);
	return true;
}

bool bj_dealer_step(Blackjack* g)
{
	if (g->phase != BJ_DEALER)
		return false;
	if (bj_total(&g->dealer, NULL) < 17 && g->dealer.n < BJ_MAX_CARDS) {
		g->dealer.cards[g->dealer.n++] = draw(g);
		return true;
	}
	settle(g);
	return false;
}

char bj_advice(const Blackjack* g)
{
	const BjHand* h = &g->hands[g->active];
	bool soft;
	int me = bj_total(h, &soft);
	int up = bj_card_value(g->dealer.cards[0]);
	if (bj_can_split(g)) {
		int v = bj_card_value(h->cards[0]);
		if (v == 11 || v == 8)
			return 'P';
		if ((v == 9 && up != 7 && up < 10) || ((v == 2 || v == 3 || v == 7) && up <= 7) || (v == 6 && up <= 6))
			return 'P';
	}
	bool can_double = bj_can_double(g);
	if (soft) {
		if (me >= 19)
			return 'S';
		if (me == 18)
			return up >= 9 ? 'H' : (can_double && up >= 3 && up <= 6 ? 'D' : 'S');
		return can_double && up >= 4 && up <= 6 ? 'D' : 'H';
	}
	if (me >= 17)
		return 'S';
	if (me >= 13)
		return up <= 6 ? 'S' : 'H';
	if (me == 12)
		return up >= 4 && up <= 6 ? 'S' : 'H';
	if (me == 11)
		return can_double ? 'D' : 'H';
	if (me == 10)
		return can_double && up <= 9 ? 'D' : 'H';
	if (me == 9)
		return can_double && up >= 3 && up <= 6 ? 'D' : 'H';
	return 'H';
}
