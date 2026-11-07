#include "games/baccarat.h"

static void shuffle(Baccarat* g)
{
	int k = 0;
	for (int d = 0; d < BC_DECKS; d++)
		for (int s = 0; s < 4; s++)
			for (int r = 2; r <= 14; r++)
				g->shoe[k++] = (PkCard){ (uint8_t)r, (uint8_t)s };
	for (int i = BC_SHOE - 1; i > 0; i--) {
		int j = rng_range(&g->rng, i + 1);
		PkCard t = g->shoe[i];
		g->shoe[i] = g->shoe[j];
		g->shoe[j] = t;
	}
	g->shoe_n = BC_SHOE;
}

void bc_init(Baccarat* g, uint32_t seed)
{
	rng_seed(&g->rng, seed);
	shuffle(g);
	g->n[0] = g->n[1] = 0;
	g->winner = -1;
}

int bc_card_value(PkCard c)
{
	return c.rank == 14 ? 1 : c.rank >= 10 ? 0 : c.rank;
}

int bc_total(const PkCard* cards, int n)
{
	int t = 0;
	for (int i = 0; i < n; i++)
		t += bc_card_value(cards[i]);
	return t % 10;
}

typedef struct {
	Baccarat* g;
	const PkCard* order;
	int k;
} Source;

static PkCard next(Source* s)
{
	if (s->order)
		return s->order[s->k++];
	if (s->g->shoe_n < 6)
		shuffle(s->g);
	return s->g->shoe[--s->g->shoe_n];
}

static void deal(Baccarat* g, const PkCard* order)
{
	Source s = { g, order, 0 };
	if (!order && g->shoe_n < 60)
		shuffle(g);
	g->n[0] = g->n[1] = 2;
	g->hand[0][0] = next(&s);
	g->hand[1][0] = next(&s);
	g->hand[0][1] = next(&s);
	g->hand[1][1] = next(&s);
	int p = bc_total(g->hand[0], 2), b = bc_total(g->hand[1], 2);
	if (p < 8 && b < 8) {
		int third = -1; /* valor de la tercera del punto */
		if (p <= 5) {
			g->hand[0][2] = next(&s);
			g->n[0] = 3;
			third = bc_card_value(g->hand[0][2]);
		}
		bool draw;
		if (third < 0)
			draw = b <= 5;
		else if (b <= 2)
			draw = true;
		else if (b == 3)
			draw = third != 8;
		else if (b == 4)
			draw = third >= 2 && third <= 7;
		else if (b == 5)
			draw = third >= 4 && third <= 7;
		else if (b == 6)
			draw = third == 6 || third == 7;
		else
			draw = false;
		if (draw) {
			g->hand[1][2] = next(&s);
			g->n[1] = 3;
		}
	}
	p = bc_total(g->hand[0], g->n[0]);
	b = bc_total(g->hand[1], g->n[1]);
	g->winner = p > b ? BC_PLAYER : b > p ? BC_BANKER : BC_TIE;
}

void bc_deal(Baccarat* g)
{
	deal(g, 0);
}

void bc_deal_cards(Baccarat* g, const PkCard* order)
{
	deal(g, order);
}

int bc_payout(const Baccarat* g, const int bets[BC_BETS])
{
	switch (g->winner) {
	case BC_PLAYER:
		return bets[BC_PLAYER] * 2;
	case BC_BANKER:
		return bets[BC_BANKER] * 2 - bets[BC_BANKER] / 20; /* comisión del 5 % (redondeada a favor del jugador) */
	case BC_TIE:
		return bets[BC_TIE] * 9 + bets[BC_PLAYER] + bets[BC_BANKER];
	}
	return 0;
}
