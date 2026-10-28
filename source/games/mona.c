#include "games/mona.h"

static void shuffle_hand(Mona* g, int p)
{
	for (int i = g->hand_count[p] - 1; i > 0; i--) {
		int j = rng_range(&g->rng, i + 1);
		Card t = g->hands[p][i];
		g->hands[p][i] = g->hands[p][j];
		g->hands[p][j] = t;
	}
}

/* Echa la pareja de la carta index si la hay. Devuelve true si la ha echado. */
static bool discard_pair_of(Mona* g, int p, int index)
{
	Card c = g->hands[p][index];
	for (int j = 0; j < g->hand_count[p]; j++) {
		if (j == index || g->hands[p][j].rank != c.rank)
			continue;
		int a = index > j ? index : j, b = index > j ? j : index;
		for (int k = a; k < g->hand_count[p] - 1; k++)
			g->hands[p][k] = g->hands[p][k + 1];
		g->hand_count[p]--;
		for (int k = b; k < g->hand_count[p] - 1; k++)
			g->hands[p][k] = g->hands[p][k + 1];
		g->hand_count[p]--;
		g->pairs[p]++;
		return true;
	}
	return false;
}

static void mark_out(Mona* g, int p)
{
	for (int i = 0; i < g->out_count; i++)
		if (g->out_order[i] == p)
			return;
	g->out_order[g->out_count++] = p;
}

static void check_end(Mona* g)
{
	int active = 0, last = -1;
	for (int p = 0; p < MONA_PLAYERS; p++) {
		if (g->hand_count[p] > 0) {
			active++;
			last = p;
		} else {
			mark_out(g, p);
		}
	}
	if (active <= 1)
		g->loser = last;
}

void mona_init(Mona* g, uint32_t seed)
{
	rng_seed(&g->rng, seed);
	g->current = MONA_PLAYERS - 1;
	mona_new_game(g);
}

void mona_new_game(Mona* g)
{
	Deck d;
	deck_init(&d);
	deck_shuffle(&d, &g->rng);
	for (int p = 0; p < MONA_PLAYERS; p++) {
		g->hand_count[p] = 0;
		g->pairs[p] = 0;
	}
	int k = 0;
	for (int i = 0; i < d.count; i++) {
		Card c = d.cards[i];
		if (c.suit == SUIT_OROS && c.rank == RANK_SOTA)
			continue; /* fuera la sota de oros */
		g->hands[k % MONA_PLAYERS][g->hand_count[k % MONA_PLAYERS]++] = c;
		k++;
	}
	/* Cada uno echa sus parejas */
	for (int p = 0; p < MONA_PLAYERS; p++) {
		bool again = true;
		while (again) {
			again = false;
			for (int i = 0; i < g->hand_count[p] && !again; i++)
				again = discard_pair_of(g, p, i);
		}
	}
	g->out_count = 0;
	g->loser = -1;
	g->last_from = -1;
	g->current = (g->current + 1) % MONA_PLAYERS;
	check_end(g);
	while (g->loser < 0 && g->hand_count[g->current] == 0)
		g->current = (g->current + 1) % MONA_PLAYERS;
}

bool mona_active(const Mona* g, int player)
{
	return g->hand_count[player] > 0;
}

int mona_victim(const Mona* g)
{
	for (int k = 1; k < MONA_PLAYERS; k++) {
		int q = (g->current + k) % MONA_PLAYERS;
		if (g->hand_count[q] > 0)
			return q;
	}
	return -1;
}

bool mona_take(Mona* g, int index)
{
	if (g->loser >= 0)
		return false;
	int p = g->current, v = mona_victim(g);
	if (v < 0 || index < 0 || index >= g->hand_count[v])
		return false;
	Card c = g->hands[v][index];
	for (int k = index; k < g->hand_count[v] - 1; k++)
		g->hands[v][k] = g->hands[v][k + 1];
	g->hand_count[v]--;
	g->hands[p][g->hand_count[p]++] = c;
	g->last_from = v;
	g->last_card = c;
	g->last_paired = discard_pair_of(g, p, g->hand_count[p] - 1);
	/* Cada uno baraja su mano para que no se sepa dónde está cada carta */
	shuffle_hand(g, p);
	check_end(g);
	if (g->loser < 0) {
		do
			g->current = (g->current + 1) % MONA_PLAYERS;
		while (g->hand_count[g->current] == 0);
	}
	return true;
}

int mona_ai_pick(Mona* g)
{
	int v = mona_victim(g);
	return v < 0 ? -1 : (int)rng_range(&g->rng, g->hand_count[v]);
}

bool mona_over(const Mona* g)
{
	return g->loser >= 0;
}
