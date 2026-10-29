#include "games/as23.h"

#include <string.h>

void as23_init(As23* g, int players, uint32_t seed)
{
	memset(g, 0, sizeof *g);
	if (players < 2)
		players = 2;
	if (players > AS23_MAX_PLAYERS)
		players = AS23_MAX_PLAYERS;
	g->players = players;
	rng_seed(&g->rng, seed);
	as23_new_game(g);
}

void as23_new_game(As23* g)
{
	Deck deck;
	deck_init(&deck);
	deck_shuffle(&deck, &g->rng);
	memset(g->stack_count, 0, sizeof g->stack_count);
	/* Se reparte de una en una empezando por un jugador al azar; con tres
	 * jugadores alguno tiene una carta más */
	int first = (int)(rng_next(&g->rng) % (uint32_t)g->players);
	for (int i = 0; i < DECK_SIZE; i++) {
		int p = (first + i) % g->players;
		deck_draw(&deck, &g->stack[p][g->stack_count[p]++]);
	}
	g->pile_count = 0;
	g->count = 0;
	g->current = first;
	g->plays = 0;
	g->winner = -1;
	g->last_player = -1;
	g->last_match = false;
	g->last_taken = 0;
}

int as23_called(const As23* g)
{
	return rank_from_pos(g->count);
}

static void finish_by_count(As23* g)
{
	int best = 0;
	for (int p = 1; p < g->players; p++)
		if (g->stack_count[p] < g->stack_count[best])
			best = p;
	g->winner = best;
}

bool as23_play(As23* g)
{
	if (g->winner >= 0)
		return false;
	int p = g->current;
	if (g->stack_count[p] == 0)
		return false;
	Card c = g->stack[p][--g->stack_count[p]];
	int said = as23_called(g);
	g->pile[g->pile_count++] = c;
	g->last_player = p;
	g->last_said = said;
	g->last_card = c;
	g->last_match = c.rank == said;
	g->last_taken = 0;
	g->plays++;
	if (g->last_match) {
		/* El montón va debajo del mazo, en el orden en que se echó */
		int n = g->pile_count;
		memmove(&g->stack[p][n], &g->stack[p][0], (size_t)g->stack_count[p] * sizeof(Card));
		for (int i = 0; i < n; i++)
			g->stack[p][i] = g->pile[n - 1 - i];
		g->stack_count[p] += n;
		g->pile_count = 0;
		g->last_taken = n;
		g->count = 0;
	} else {
		g->count = (g->count + 1) % RANKS_PER_SUIT;
		if (g->stack_count[p] == 0) {
			g->winner = p;
			return true;
		}
	}
	g->current = (p + 1) % g->players;
	if (g->plays >= AS23_MAX_PLAYS)
		finish_by_count(g);
	return true;
}

bool as23_over(const As23* g)
{
	return g->winner >= 0;
}
