#include "games/klondike.h"

#include <string.h>

bool kl_red(KlCard c)
{
	return c.suit == 1 || c.suit == 2;
}

void kl_init(Klondike* g, int draw, uint32_t seed)
{
	memset(g, 0, sizeof *g);
	rng_seed(&g->rng, seed);
	g->draw = draw == 3 ? 3 : 1;
	KlCard deck[52];
	for (int i = 0; i < 52; i++)
		deck[i] = (KlCard){ (uint8_t)(i % 13 + 1), (uint8_t)(i / 13), false };
	for (int i = 51; i > 0; i--) {
		int j = rng_range(&g->rng, i + 1);
		KlCard t = deck[i];
		deck[i] = deck[j];
		deck[j] = t;
	}
	int n = 0;
	KlBoard* b = &g->b;
	for (int c = 0; c < KL_COLS; c++) {
		for (int k = 0; k <= c; k++)
			b->col[c][b->col_n[c]++] = deck[n++];
		b->col[c][c].up = true;
	}
	while (n < 52)
		b->stock[b->stock_n++] = deck[n++];
}

static void save(Klondike* g)
{
	if (g->history_n == KL_HISTORY) {
		memmove(g->history, g->history + 1, sizeof(KlBoard) * (KL_HISTORY - 1));
		g->history_n--;
	}
	g->history[g->history_n++] = g->b;
}

static void settle(Klondike* g)
{
	KlBoard* b = &g->b;
	for (int c = 0; c < KL_COLS; c++)
		if (b->col_n[c])
			b->col[c][b->col_n[c] - 1].up = true;
	g->won = b->found[0] == 13 && b->found[1] == 13 && b->found[2] == 13 && b->found[3] == 13;
}

void kl_draw(Klondike* g)
{
	KlBoard* b = &g->b;
	if (b->stock_n == 0 && b->waste_n == 0)
		return;
	save(g);
	if (b->stock_n == 0) {
		while (b->waste_n > 0) {
			KlCard c = b->waste[--b->waste_n];
			c.up = false;
			b->stock[b->stock_n++] = c;
		}
		b->passes++;
	} else {
		for (int k = 0; k < g->draw && b->stock_n > 0; k++) {
			KlCard c = b->stock[--b->stock_n];
			c.up = true;
			b->waste[b->waste_n++] = c;
		}
	}
	b->moves++;
}

/* Carta de arriba de un lugar de origen (n cartas desde el final) */
static const KlCard* source_card(const KlBoard* b, int from, int n)
{
	if (from == KL_WASTE)
		return n == 1 && b->waste_n ? &b->waste[b->waste_n - 1] : NULL;
	if (from >= KL_FOUND0 && from < KL_WASTE) {
		static KlCard top;
		int s = from - KL_FOUND0;
		if (n != 1 || !b->found[s])
			return NULL;
		top = (KlCard){ (uint8_t)b->found[s], (uint8_t)s, true };
		return &top;
	}
	if (from < 0 || from >= KL_COLS || n < 1 || n > b->col_n[from])
		return NULL;
	const KlCard* c = &b->col[from][b->col_n[from] - n];
	if (!c->up)
		return NULL;
	/* Lo que va encima tiene que ser una escalera bien hecha */
	for (int k = b->col_n[from] - n; k < b->col_n[from] - 1; k++) {
		const KlCard *lo = &b->col[from][k + 1], *hi = &b->col[from][k];
		if (hi->rank != lo->rank + 1 || kl_red(*hi) == kl_red(*lo))
			return NULL;
	}
	return c;
}

bool kl_can_move(const KlBoard* b, int from, int n, int to)
{
	if (from == to)
		return false;
	const KlCard* c = source_card(b, from, n);
	if (!c)
		return false;
	if (to >= KL_FOUND0 && to < KL_WASTE) {
		int s = to - KL_FOUND0;
		return n == 1 && c->suit == s && c->rank == b->found[s] + 1;
	}
	if (to < 0 || to >= KL_COLS)
		return false;
	if (b->col_n[to] == 0)
		return c->rank == 13;
	const KlCard* top = &b->col[to][b->col_n[to] - 1];
	return top->up && top->rank == c->rank + 1 && kl_red(*top) != kl_red(*c);
}

bool kl_move(Klondike* g, int from, int n, int to)
{
	KlBoard* b = &g->b;
	if (!kl_can_move(b, from, n, to))
		return false;
	save(g);
	KlCard moved[20];
	if (from == KL_WASTE) {
		moved[0] = b->waste[--b->waste_n];
	} else if (from >= KL_FOUND0) {
		int s = from - KL_FOUND0;
		moved[0] = (KlCard){ (uint8_t)b->found[s], (uint8_t)s, true };
		b->found[s]--;
	} else {
		memcpy(moved, &b->col[from][b->col_n[from] - n], sizeof(KlCard) * (size_t)n);
		b->col_n[from] -= n;
	}
	if (to >= KL_FOUND0)
		b->found[to - KL_FOUND0] = moved[0].rank;
	else
		for (int k = 0; k < n; k++)
			b->col[to][b->col_n[to]++] = moved[k];
	b->moves++;
	settle(g);
	return true;
}

bool kl_auto_found(Klondike* g, int from)
{
	for (int s = 0; s < 4; s++)
		if (kl_move(g, from, 1, KL_FOUND0 + s))
			return true;
	return false;
}

bool kl_undo(Klondike* g)
{
	if (!g->history_n)
		return false;
	g->b = g->history[--g->history_n];
	g->won = false;
	return true;
}

bool kl_can_finish(const KlBoard* b)
{
	if (b->stock_n || b->waste_n)
		return false;
	for (int c = 0; c < KL_COLS; c++)
		for (int k = 0; k < b->col_n[c]; k++)
			if (!b->col[c][k].up)
				return false;
	return true;
}

int kl_finish_step(Klondike* g)
{
	for (int c = 0; c < KL_COLS; c++)
		if (g->b.col_n[c] && kl_auto_found(g, c))
			return c;
	return -1;
}
