#include "games/spider.h"

#include <string.h>

void sp_init(Spider* g, int suits, uint32_t seed)
{
	memset(g, 0, sizeof *g);
	rng_seed(&g->rng, seed);
	g->suits = suits == 1 || suits == 2 ? suits : 4;
	/* 104 cartas: 8 palos completos de los palos elegidos */
	static const int SUITS2[2] = { 0, 1 };
	SpCard deck[104];
	int n = 0;
	for (int k = 0; k < 8; k++) {
		int suit = g->suits == 1 ? 0 : g->suits == 2 ? SUITS2[k % 2] : k % 4;
		for (int r = 1; r <= 13; r++)
			deck[n++] = (SpCard){ (uint8_t)r, (uint8_t)suit, false };
	}
	for (int i = 103; i > 0; i--) {
		int j = rng_range(&g->rng, i + 1);
		SpCard t = deck[i];
		deck[i] = deck[j];
		deck[j] = t;
	}
	n = 0;
	SpBoard* b = &g->b;
	for (int c = 0; c < SP_COLS; c++) {
		int cards = c < 4 ? 6 : 5;
		for (int k = 0; k < cards; k++)
			b->col[c][b->col_n[c]++] = deck[n++];
		b->col[c][b->col_n[c] - 1].up = true;
	}
	while (n < 104)
		b->stock[b->stock_n++] = deck[n++];
}

int sp_run(const SpBoard* b, int col)
{
	int n = b->col_n[col];
	if (n == 0)
		return 0;
	int k = 1;
	while (k < n) {
		const SpCard* lo = &b->col[col][n - k];
		const SpCard* hi = &b->col[col][n - k - 1];
		if (!hi->up || hi->suit != lo->suit || hi->rank != lo->rank + 1)
			break;
		k++;
	}
	return k;
}

bool sp_can_move(const SpBoard* b, int from, int n, int to)
{
	if (from < 0 || from >= SP_COLS || to < 0 || to >= SP_COLS || from == to || n < 1 || n > sp_run(b, from))
		return false;
	const SpCard* first = &b->col[from][b->col_n[from] - n];
	if (b->col_n[to] == 0)
		return true;
	const SpCard* top = &b->col[to][b->col_n[to] - 1];
	return top->rank == first->rank + 1;
}

static void save(Spider* g)
{
	if (g->history_n == SP_HISTORY) {
		memmove(g->history, g->history + 1, sizeof(SpBoard) * (SP_HISTORY - 1));
		g->history_n--;
	}
	g->history[g->history_n++] = g->b;
}

/* Saca las escaleras completas y destapa las cartas de arriba */
static void settle(Spider* g)
{
	SpBoard* b = &g->b;
	for (int c = 0; c < SP_COLS; c++) {
		int n = b->col_n[c];
		if (n >= 13 && sp_run(b, c) >= 13 && b->col[c][n - 1].rank == 1) {
			b->done_suit[b->done++] = b->col[c][n - 1].suit;
			b->col_n[c] -= 13;
		}
		if (b->col_n[c] > 0)
			b->col[c][b->col_n[c] - 1].up = true;
	}
	g->won = b->done == 8;
}

bool sp_move(Spider* g, int from, int n, int to)
{
	if (!sp_can_move(&g->b, from, n, to))
		return false;
	save(g);
	SpBoard* b = &g->b;
	memcpy(&b->col[to][b->col_n[to]], &b->col[from][b->col_n[from] - n], sizeof(SpCard) * (size_t)n);
	b->col_n[to] += n;
	b->col_n[from] -= n;
	b->moves++;
	settle(g);
	return true;
}

bool sp_can_deal(const SpBoard* b)
{
	if (b->stock_n < SP_COLS)
		return false;
	for (int c = 0; c < SP_COLS; c++)
		if (b->col_n[c] == 0)
			return false;
	return true;
}

bool sp_deal(Spider* g)
{
	if (!sp_can_deal(&g->b))
		return false;
	save(g);
	SpBoard* b = &g->b;
	for (int c = 0; c < SP_COLS; c++) {
		SpCard card = b->stock[--b->stock_n];
		card.up = true;
		b->col[c][b->col_n[c]++] = card;
	}
	b->moves++;
	settle(g);
	return true;
}

bool sp_undo(Spider* g)
{
	if (g->history_n == 0)
		return false;
	g->b = g->history[--g->history_n];
	g->won = false;
	return true;
}

bool sp_hint(const SpBoard* b, int* from, int* n, int* to)
{
	/* Primero, juntar del mismo palo; luego, cualquier movimiento que destape
	 * una carta o libere una columna */
	for (int pass = 0; pass < 2; pass++)
		for (int f = 0; f < SP_COLS; f++) {
			int run = sp_run(b, f);
			if (!run)
				continue;
			const SpCard* first = &b->col[f][b->col_n[f] - run];
			bool uncovers = b->col_n[f] > run;
			for (int t = 0; t < SP_COLS; t++) {
				if (!sp_can_move(b, f, run, t) || b->col_n[t] == 0)
					continue;
				const SpCard* top = &b->col[t][b->col_n[t] - 1];
				bool same = top->suit == first->suit;
				/* No sirve mover de una carta igual a otra igual */
				bool pointless = uncovers && b->col[f][b->col_n[f] - run - 1].up &&
					b->col[f][b->col_n[f] - run - 1].rank == top->rank && !same;
				if ((pass == 0 && same) || (pass == 1 && (uncovers || b->col_n[f] == run) && !pointless)) {
					*from = f;
					*n = run;
					*to = t;
					return true;
				}
			}
		}
	return false;
}
