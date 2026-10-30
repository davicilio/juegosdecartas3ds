#include "games/freecell.h"

#include <string.h>

bool fc_red(FcCard c)
{
	return c.suit == 1 || c.suit == 2;
}

static bool fits_on(FcCard c, FcCard below)
{
	return below.rank == c.rank + 1 && fc_red(below) != fc_red(c);
}

void fc_new_game(FreeCell* g, int deal)
{
	memset(g, 0, sizeof *g);
	g->deal = deal;
	/* El reparto del FreeCell de Windows */
	int cards[52];
	for (int i = 0; i < 52; i++)
		cards[i] = 51 - i;
	uint32_t state = (uint32_t)deal;
	for (int i = 0; i < 52; i++) {
		state = (state * 214013u + 2531011u) & 0x7fffffffu;
		int r = (int)(state >> 16);
		int j = 51 - r % (52 - i);
		int t = cards[i];
		cards[i] = cards[j];
		cards[j] = t;
	}
	/* Allí: número = valor * 4 + palo, con los palos tréboles, diamantes, corazones y picas */
	static const uint8_t SUIT[4] = { 3, 2, 1, 0 };
	for (int i = 0; i < 52; i++) {
		int c = i % FC_COLS;
		g->b.col[c][g->b.col_n[c]++] = (FcCard){ (uint8_t)(cards[i] / 4 + 1), SUIT[cards[i] % 4] };
	}
}

static int free_cells(const FcBoard* b)
{
	int n = 0;
	for (int i = 0; i < FC_CELLS; i++)
		n += b->cell[i].rank == 0;
	return n;
}

int fc_capacity(const FcBoard* b, int dest)
{
	int empty = 0;
	for (int c = 0; c < FC_COLS; c++)
		if (b->col_n[c] == 0 && c != dest)
			empty++;
	return (free_cells(b) + 1) << empty;
}

int fc_run(const FcBoard* b, int col)
{
	int n = b->col_n[col];
	if (n == 0)
		return 0;
	int k = 1;
	while (k < n && fits_on(b->col[col][n - k], b->col[col][n - k - 1]))
		k++;
	return k;
}

static FcCard top_of(const FcBoard* b, int place)
{
	FcCard none = { 0, 0 };
	if (place < FC_CELL0)
		return b->col_n[place] ? b->col[place][b->col_n[place] - 1] : none;
	if (place < FC_FOUND0)
		return b->cell[place - FC_CELL0];
	return none;
}

/* Cartas que se moverían de from a una columna to (0 si no se puede) */
static int col_move_count(const FcBoard* b, int from, int to)
{
	int run = fc_run(b, from);
	int cap = fc_capacity(b, to);
	if (b->col_n[to] == 0)
		return run < cap ? run : cap; /* a una columna vacía, todo lo que quepa */
	FcCard dest = b->col[to][b->col_n[to] - 1];
	for (int k = 1; k <= run && k <= cap; k++)
		if (fits_on(b->col[from][b->col_n[from] - k], dest))
			return k;
	return 0;
}

bool fc_can_move(const FcBoard* b, int from, int to)
{
	if (from < 0 || from >= FC_FOUND0 || to < 0 || to >= FC_PLACES || from == to)
		return false;
	FcCard c = top_of(b, from);
	if (!c.rank)
		return false;
	if (to >= FC_FOUND0)
		return c.suit == to - FC_FOUND0 && c.rank == b->found[c.suit] + 1;
	if (to >= FC_CELL0)
		return b->cell[to - FC_CELL0].rank == 0;
	if (from < FC_CELL0)
		return col_move_count(b, from, to) > 0;
	return b->col_n[to] == 0 || fits_on(c, b->col[to][b->col_n[to] - 1]);
}

static void push_history(FreeCell* g)
{
	if (g->history_n == FC_HISTORY) {
		memmove(g->history, g->history + 1, sizeof(FcBoard) * (FC_HISTORY - 1));
		g->history_n--;
	}
	g->history[g->history_n++] = g->b;
}

static void do_move(FcBoard* b, int from, int to)
{
	FcCard moving[FC_MAXCOL];
	int n = 1;
	if (from < FC_CELL0) {
		if (to < FC_CELL0)
			n = col_move_count(b, from, to);
		for (int k = 0; k < n; k++)
			moving[k] = b->col[from][b->col_n[from] - n + k];
		b->col_n[from] -= n;
	} else {
		moving[0] = b->cell[from - FC_CELL0];
		b->cell[from - FC_CELL0] = (FcCard){ 0, 0 };
	}
	if (to >= FC_FOUND0)
		b->found[moving[0].suit] = moving[0].rank;
	else if (to >= FC_CELL0)
		b->cell[to - FC_CELL0] = moving[0];
	else
		for (int k = 0; k < n; k++)
			b->col[to][b->col_n[to]++] = moving[k];
	b->moves++;
}

int fc_cards_home(const FcBoard* b)
{
	return b->found[0] + b->found[1] + b->found[2] + b->found[3];
}

bool fc_move(FreeCell* g, int from, int to)
{
	if (g->won || !fc_can_move(&g->b, from, to))
		return false;
	push_history(g);
	do_move(&g->b, from, to);
	g->won = fc_cards_home(&g->b) == 52;
	return true;
}

int fc_auto(FreeCell* g)
{
	int moved = 0;
	for (bool again = true; again;) {
		again = false;
		for (int from = 0; from < FC_FOUND0; from++) {
			FcCard c = top_of(&g->b, from);
			if (!c.rank)
				continue;
			/* Solo si ya no hace falta para poner encima las del otro color */
			int other = fc_red(c) ? (g->b.found[0] < g->b.found[3] ? g->b.found[0] : g->b.found[3])
			                      : (g->b.found[1] < g->b.found[2] ? g->b.found[1] : g->b.found[2]);
			if (c.rank > 2 && c.rank > other + 1)
				continue;
			if (fc_can_move(&g->b, from, FC_FOUND0 + c.suit)) {
				do_move(&g->b, from, FC_FOUND0 + c.suit);
				moved++;
				again = true;
			}
		}
	}
	g->won = fc_cards_home(&g->b) == 52;
	return moved;
}

bool fc_undo(FreeCell* g)
{
	if (g->history_n == 0)
		return false;
	g->b = g->history[--g->history_n];
	g->won = false;
	return true;
}
