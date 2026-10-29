#include "games/reversi.h"

#include <string.h>

static const int DIRS[8][2] = { { -1, -1 }, { -1, 0 }, { -1, 1 }, { 0, -1 }, { 0, 1 }, { 1, -1 }, { 1, 0 }, { 1, 1 } };

void rv_init(Reversi* g, uint32_t seed)
{
	memset(g, 0, sizeof *g);
	rng_seed(&g->rng, seed);
	g->level = 3;
	rv_new_game(g);
}

static void recount(Reversi* g)
{
	g->count[0] = g->count[1] = g->count[2] = 0;
	for (int r = 0; r < RV_SIZE; r++)
		for (int c = 0; c < RV_SIZE; c++)
			g->count[g->cell[r][c]]++;
}

void rv_new_game(Reversi* g)
{
	memset(g->cell, 0, sizeof g->cell);
	memset(g->flipped, 0, sizeof g->flipped);
	g->cell[3][3] = g->cell[4][4] = 2;
	g->cell[3][4] = g->cell[4][3] = 1;
	g->current = 1;
	g->winner = 0;
	g->last_row = g->last_col = -1;
	g->last_passed = false;
	recount(g);
}

static int flips_dir(const uint8_t cell[RV_SIZE][RV_SIZE], int player, int row, int col, int dr, int dc)
{
	int other = 3 - player, n = 0;
	int r = row + dr, c = col + dc;
	while (r >= 0 && r < RV_SIZE && c >= 0 && c < RV_SIZE && cell[r][c] == other) {
		r += dr;
		c += dc;
		n++;
	}
	if (n > 0 && r >= 0 && r < RV_SIZE && c >= 0 && c < RV_SIZE && cell[r][c] == player)
		return n;
	return 0;
}

static int flips_at(const uint8_t cell[RV_SIZE][RV_SIZE], int player, int row, int col)
{
	if (row < 0 || row >= RV_SIZE || col < 0 || col >= RV_SIZE || cell[row][col])
		return 0;
	int n = 0;
	for (int d = 0; d < 8; d++)
		n += flips_dir(cell, player, row, col, DIRS[d][0], DIRS[d][1]);
	return n;
}

int rv_flips(const Reversi* g, int player, int row, int col)
{
	return flips_at(g->cell, player, row, col);
}

static bool any_move(const uint8_t cell[RV_SIZE][RV_SIZE], int player)
{
	for (int r = 0; r < RV_SIZE; r++)
		for (int c = 0; c < RV_SIZE; c++)
			if (flips_at(cell, player, r, c))
				return true;
	return false;
}

bool rv_can_move(const Reversi* g, int player)
{
	return any_move(g->cell, player);
}

static void apply(uint8_t cell[RV_SIZE][RV_SIZE], uint8_t* flipped, int player, int row, int col)
{
	for (int d = 0; d < 8; d++) {
		int n = flips_dir(cell, player, row, col, DIRS[d][0], DIRS[d][1]);
		for (int k = 1; k <= n; k++) {
			int r = row + DIRS[d][0] * k, c = col + DIRS[d][1] * k;
			cell[r][c] = (uint8_t)player;
			if (flipped)
				flipped[r * RV_SIZE + c] = 1;
		}
	}
	cell[row][col] = (uint8_t)player;
}

bool rv_play(Reversi* g, int row, int col)
{
	if (g->winner || !flips_at(g->cell, g->current, row, col))
		return false;
	memset(g->flipped, 0, sizeof g->flipped);
	apply(g->cell, &g->flipped[0][0], g->current, row, col);
	g->last_row = row;
	g->last_col = col;
	recount(g);
	int next = 3 - g->current;
	g->last_passed = false;
	if (any_move(g->cell, next)) {
		g->current = next;
	} else if (any_move(g->cell, g->current)) {
		g->last_passed = true; /* el rival pasa */
	} else {
		g->winner = g->count[1] > g->count[2] ? 1 : g->count[2] > g->count[1] ? 2 : 3;
	}
	return true;
}

/* ---------------------------------------------------------------------------
 * IA: minimax con poda alfa-beta y una tabla de valores de las casillas
 * ------------------------------------------------------------------------- */

static const int WEIGHT[RV_SIZE][RV_SIZE] = {
	{ 100, -20, 10, 5, 5, 10, -20, 100 },
	{ -20, -50, -2, -2, -2, -2, -50, -20 },
	{ 10, -2, 1, 1, 1, 1, -2, 10 },
	{ 5, -2, 1, 0, 0, 1, -2, 5 },
	{ 5, -2, 1, 0, 0, 1, -2, 5 },
	{ 10, -2, 1, 1, 1, 1, -2, 10 },
	{ -20, -50, -2, -2, -2, -2, -50, -20 },
	{ 100, -20, 10, 5, 5, 10, -20, 100 },
};

static int evaluate(const uint8_t cell[RV_SIZE][RV_SIZE], int me)
{
	int v = 0, mine = 0, theirs = 0;
	for (int r = 0; r < RV_SIZE; r++)
		for (int c = 0; c < RV_SIZE; c++) {
			if (cell[r][c] == me) {
				v += WEIGHT[r][c];
				mine++;
			} else if (cell[r][c]) {
				v -= WEIGHT[r][c];
				theirs++;
			}
		}
	int mob = 0;
	for (int r = 0; r < RV_SIZE; r++)
		for (int c = 0; c < RV_SIZE; c++) {
			if (flips_at(cell, me, r, c))
				mob++;
			if (flips_at(cell, 3 - me, r, c))
				mob--;
		}
	if (mine + theirs > 54)
		v += (mine - theirs) * 4;
	return v + mob * 3;
}

static int search(const uint8_t cell[RV_SIZE][RV_SIZE], int player, int me, int depth, int alpha, int beta, bool passed)
{
	bool any = false;
	int best = player == me ? -1000000 : 1000000;
	if (depth == 0)
		return evaluate(cell, me);
	for (int r = 0; r < RV_SIZE; r++)
		for (int c = 0; c < RV_SIZE; c++) {
			if (!flips_at(cell, player, r, c))
				continue;
			any = true;
			uint8_t next[RV_SIZE][RV_SIZE];
			memcpy(next, cell, sizeof next);
			apply(next, NULL, player, r, c);
			int v = search(next, 3 - player, me, depth - 1, alpha, beta, false);
			if (player == me) {
				if (v > best)
					best = v;
				if (best > alpha)
					alpha = best;
			} else {
				if (v < best)
					best = v;
				if (best < beta)
					beta = best;
			}
			if (alpha >= beta)
				return best;
		}
	if (!any) {
		if (passed) {
			/* Fin: se cuentan las fichas */
			int d = 0;
			for (int r = 0; r < RV_SIZE; r++)
				for (int c = 0; c < RV_SIZE; c++)
					d += cell[r][c] == me ? 1 : cell[r][c] ? -1 : 0;
			return d * 1000;
		}
		return search(cell, 3 - player, me, depth - 1, alpha, beta, true);
	}
	return best;
}

int rv_ai_choose(const Reversi* g)
{
	int me = g->current, best = -1, best_v = -10000000;
	Rng rng = g->rng;
	for (int r = 0; r < RV_SIZE; r++)
		for (int c = 0; c < RV_SIZE; c++) {
			if (!flips_at(g->cell, me, r, c))
				continue;
			uint8_t next[RV_SIZE][RV_SIZE];
			memcpy(next, g->cell, sizeof next);
			apply(next, NULL, me, r, c);
			int v = search(next, 3 - me, me, g->level - 1, -10000000, 10000000, false) * 4 + rng_range(&rng, 4);
			if (v > best_v) {
				best_v = v;
				best = r * RV_SIZE + c;
			}
		}
	return best;
}
