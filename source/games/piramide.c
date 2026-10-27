#include "games/piramide.h"

int pyr_row(int index)
{
	int row = 0;
	while (pyr_index(row + 1, 0) <= index)
		row++;
	return row;
}

void pyr_init(Piramide* g, uint32_t seed)
{
	rng_seed(&g->rng, seed);
	pyr_new_game(g);
}

void pyr_new_game(Piramide* g)
{
	Deck d;
	deck_init(&d);
	deck_shuffle(&d, &g->rng);
	for (int i = 0; i < PYR_CARDS; i++) {
		g->card[i] = d.cards[i];
		g->removed[i] = false;
	}
	g->stock_count = 0;
	for (int i = PYR_CARDS; i < DECK_SIZE; i++)
		g->stock[g->stock_count++] = d.cards[i];
	g->waste_count = 0;
	g->pass = 1;
	g->left = PYR_CARDS;
	g->moves = 0;
}

bool pyr_exposed(const Piramide* g, int index)
{
	if (index < 0 || index >= PYR_CARDS || g->removed[index])
		return false;
	int row = pyr_row(index);
	if (row == PYR_ROWS - 1)
		return true;
	int pos = index - pyr_index(row, 0);
	return g->removed[pyr_index(row + 1, pos)] && g->removed[pyr_index(row + 1, pos + 1)];
}

bool pyr_current(const Piramide* g, Card* out)
{
	if (g->waste_count == 0)
		return false;
	*out = g->waste[g->waste_count - 1];
	return true;
}

bool pyr_can_draw(const Piramide* g)
{
	if (pyr_won(g))
		return false;
	return g->stock_count > 0 || (g->waste_count > 0 && g->pass < PYR_PASSES);
}

bool pyr_draw(Piramide* g)
{
	if (!pyr_can_draw(g))
		return false;
	if (g->stock_count == 0) {
		/* Nueva pasada: los descartes vuelven al mazo en el mismo orden */
		while (g->waste_count > 0)
			g->stock[g->stock_count++] = g->waste[--g->waste_count];
		g->pass++;
	}
	g->waste[g->waste_count++] = g->stock[--g->stock_count];
	g->moves++;
	return true;
}

bool pyr_can_take(const Piramide* g, int index)
{
	Card cur;
	return pyr_exposed(g, index) && pyr_current(g, &cur) && cur.rank == g->card[index].rank;
}

bool pyr_take(Piramide* g, int index)
{
	if (!pyr_can_take(g, index))
		return false;
	g->removed[index] = true;
	g->waste[g->waste_count++] = g->card[index];
	g->left--;
	g->moves++;
	return true;
}

bool pyr_can_pair(const Piramide* g, int a, int b)
{
	return a != b && pyr_exposed(g, a) && pyr_exposed(g, b) && g->card[a].rank == g->card[b].rank;
}

bool pyr_pair(Piramide* g, int a, int b)
{
	if (!pyr_can_pair(g, a, b))
		return false;
	g->removed[a] = g->removed[b] = true;
	g->waste[g->waste_count++] = g->card[a];
	g->waste[g->waste_count++] = g->card[b];
	g->left -= 2;
	g->moves++;
	return true;
}

bool pyr_won(const Piramide* g)
{
	return g->left == 0;
}

bool pyr_stuck(const Piramide* g)
{
	if (pyr_won(g) || pyr_can_draw(g))
		return false;
	for (int i = 0; i < PYR_CARDS; i++) {
		if (pyr_can_take(g, i))
			return false;
		for (int j = i + 1; j < PYR_CARDS; j++)
			if (pyr_can_pair(g, i, j))
				return false;
	}
	return true;
}
