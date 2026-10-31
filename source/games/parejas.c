#include "games/parejas.h"

#include <string.h>

bool pj_same(Card a, Card b)
{
	return a.suit == b.suit && a.rank == b.rank;
}

void pj_init(Parejas* g, int pairs, int players, int memory, uint32_t seed)
{
	memset(g, 0, sizeof *g);
	rng_seed(&g->rng, seed);
	if (pairs * 2 > PJ_MAX)
		pairs = PJ_MAX / 2;
	g->n = pairs * 2;
	g->players = players < 1 ? 1 : players > 2 ? 2 : players;
	g->memory = memory;
	/* Cartas distintas al azar, dos de cada */
	Deck deck;
	deck_init(&deck);
	deck_shuffle(&deck, &g->rng);
	for (int i = 0; i < pairs; i++) {
		Card c;
		deck_draw(&deck, &c);
		g->card[2 * i] = g->card[2 * i + 1] = c;
	}
	for (int i = g->n - 1; i > 0; i--) {
		int j = rng_range(&g->rng, i + 1);
		Card t = g->card[i];
		g->card[i] = g->card[j];
		g->card[j] = t;
	}
	for (int i = 0; i < g->n; i++)
		g->owner[i] = -1;
	g->first = g->second = -1;
	g->phase = PJ_PICK1;
}

/* El rival ve la carta y quizá la recuerda */
static void observe(Parejas* g, int i)
{
	if (g->players > 1 && rng_range(&g->rng, 100) < g->memory)
		g->seen[i] = true;
}

bool pj_flip(Parejas* g, int i)
{
	if (i < 0 || i >= g->n || g->taken[i] || (g->phase != PJ_PICK1 && g->phase != PJ_PICK2) || i == g->first)
		return false;
	observe(g, i);
	if (g->phase == PJ_PICK1) {
		g->first = i;
		g->phase = PJ_PICK2;
	} else {
		g->second = i;
		g->moves++;
		g->last_match = pj_same(g->card[g->first], g->card[i]);
		g->phase = PJ_SHOW;
	}
	return true;
}

void pj_resolve(Parejas* g)
{
	if (g->phase != PJ_SHOW)
		return;
	if (g->last_match) {
		g->taken[g->first] = g->taken[g->second] = true;
		g->owner[g->first] = g->owner[g->second] = g->current;
		g->score[g->current]++;
	} else {
		g->current = (g->current + 1) % g->players;
	}
	g->first = g->second = -1;
	g->phase = PJ_PICK1;
	int left = 0;
	for (int i = 0; i < g->n; i++)
		left += !g->taken[i];
	if (left == 0)
		g->phase = PJ_OVER;
}

static int random_unknown(Parejas* g, int avoid)
{
	int cand[PJ_MAX], n = 0, any[PJ_MAX], na = 0;
	for (int i = 0; i < g->n; i++) {
		if (g->taken[i] || i == avoid)
			continue;
		any[na++] = i;
		if (!g->seen[i])
			cand[n++] = i;
	}
	if (n > 0)
		return cand[rng_range(&g->rng, n)];
	return na > 0 ? any[rng_range(&g->rng, na)] : -1;
}

int pj_ai_pick(Parejas* g)
{
	if (g->phase == PJ_PICK1) {
		/* Una pareja que ya conoce */
		for (int i = 0; i < g->n; i++)
			for (int j = i + 1; j < g->n; j++)
				if (!g->taken[i] && !g->taken[j] && g->seen[i] && g->seen[j] && pj_same(g->card[i], g->card[j]))
					return i;
		return random_unknown(g, -1);
	}
	if (g->phase == PJ_PICK2) {
		for (int j = 0; j < g->n; j++)
			if (j != g->first && !g->taken[j] && g->seen[j] && pj_same(g->card[g->first], g->card[j]))
				return j;
		return random_unknown(g, g->first);
	}
	return -1;
}

int pj_winner(const Parejas* g)
{
	if (g->players < 2 || g->score[0] == g->score[1])
		return g->players < 2 ? 0 : -1;
	return g->score[0] > g->score[1] ? 0 : 1;
}
