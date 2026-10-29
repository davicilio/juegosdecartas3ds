#include "games/conecta4.h"

#include <string.h>

void c4_init(Conecta4* g, uint32_t seed)
{
	rng_seed(&g->rng, seed);
	g->level = 6;
	g->starter = 2;
	c4_new_game(g);
}

void c4_new_game(Conecta4* g)
{
	memset(g->cell, 0, sizeof g->cell);
	g->starter = 3 - g->starter; /* se alterna quién empieza */
	g->current = g->starter;
	g->winner = 0;
	g->moves = 0;
	g->last_row = g->last_col = -1;
}

int c4_drop_row(const Conecta4* g, int col)
{
	if (col < 0 || col >= C4_COLS)
		return -1;
	for (int r = 0; r < C4_ROWS; r++)
		if (!g->cell[r][col])
			return r;
	return -1;
}

bool c4_can_drop(const Conecta4* g, int col)
{
	return g->winner == 0 && c4_drop_row(g, col) >= 0;
}

static const int DIRS[4][2] = { { 0, 1 }, { 1, 0 }, { 1, 1 }, { 1, -1 } };

/* ¿Hay cuatro en línea pasando por (r, c)? Guarda la línea si la hay */
static bool four_at(const uint8_t cell[C4_ROWS][C4_COLS], int r, int c, uint8_t line[4][2])
{
	int who = cell[r][c];
	for (int d = 0; d < 4; d++) {
		int dr = DIRS[d][0], dc = DIRS[d][1];
		int start_r = r, start_c = c;
		while (start_r - dr >= 0 && start_r - dr < C4_ROWS && start_c - dc >= 0 && start_c - dc < C4_COLS &&
		       cell[start_r - dr][start_c - dc] == who) {
			start_r -= dr;
			start_c -= dc;
		}
		int n = 0;
		int rr = start_r, cc = start_c;
		while (rr >= 0 && rr < C4_ROWS && cc >= 0 && cc < C4_COLS && cell[rr][cc] == who) {
			if (line && n < 4) {
				line[n][0] = (uint8_t)rr;
				line[n][1] = (uint8_t)cc;
			}
			n++;
			rr += dr;
			cc += dc;
		}
		if (n >= 4)
			return true;
	}
	return false;
}

bool c4_drop(Conecta4* g, int col)
{
	if (!c4_can_drop(g, col))
		return false;
	int r = c4_drop_row(g, col);
	g->cell[r][col] = (uint8_t)g->current;
	g->last_row = r;
	g->last_col = col;
	g->moves++;
	if (four_at(g->cell, r, col, g->line))
		g->winner = g->current;
	else if (g->moves == C4_ROWS * C4_COLS)
		g->winner = 3;
	g->current = 3 - g->current;
	return true;
}

/* Valoración de una ventana de cuatro casillas */
static int window(int mine, int theirs)
{
	if (mine && theirs)
		return 0;
	static const int v[5] = { 0, 1, 8, 60, 0 };
	return mine ? v[mine] : -v[theirs] * 5 / 4;
}

static int evaluate(const uint8_t cell[C4_ROWS][C4_COLS], int me)
{
	int score = 0;
	for (int r = 0; r < C4_ROWS; r++)
		score += (cell[r][3] == me) * 6 - (cell[r][3] == 3 - me) * 6; /* el centro vale más */
	for (int d = 0; d < 4; d++)
		for (int r = 0; r < C4_ROWS; r++)
			for (int c = 0; c < C4_COLS; c++) {
				int er = r + DIRS[d][0] * 3, ec = c + DIRS[d][1] * 3;
				if (er < 0 || er >= C4_ROWS || ec < 0 || ec >= C4_COLS)
					continue;
				int mine = 0, theirs = 0;
				for (int k = 0; k < 4; k++) {
					int v = cell[r + DIRS[d][0] * k][c + DIRS[d][1] * k];
					mine += v == me;
					theirs += v == 3 - me;
				}
				score += window(mine, theirs);
			}
	return score;
}

static const int ORDER[C4_COLS] = { 3, 2, 4, 1, 5, 0, 6 };

static int negamax(uint8_t cell[C4_ROWS][C4_COLS], int who, int depth, int alpha, int beta, int moves)
{
	if (moves == C4_ROWS * C4_COLS)
		return 0;
	if (depth == 0)
		return evaluate(cell, who);
	for (int k = 0; k < C4_COLS; k++) {
		int c = ORDER[k];
		int r = 0;
		while (r < C4_ROWS && cell[r][c])
			r++;
		if (r == C4_ROWS)
			continue;
		cell[r][c] = (uint8_t)who;
		int v;
		if (four_at(cell, r, c, NULL))
			v = 100000 + depth;
		else
			v = -negamax(cell, 3 - who, depth - 1, -beta, -alpha, moves + 1);
		cell[r][c] = 0;
		if (v > alpha)
			alpha = v;
		if (alpha >= beta)
			break;
	}
	return alpha;
}

int c4_ai_choose(Conecta4* g)
{
	uint8_t cell[C4_ROWS][C4_COLS];
	memcpy(cell, g->cell, sizeof cell);
	int best = -1, best_v = -1000000;
	for (int k = 0; k < C4_COLS; k++) {
		int c = ORDER[k];
		int r = c4_drop_row(g, c);
		if (r < 0)
			continue;
		cell[r][c] = (uint8_t)g->current;
		int v = four_at(cell, r, c, NULL) ? 1000000
		                                  : -negamax(cell, 3 - g->current, g->level - 1, -1000000, 1000000, g->moves + 1);
		cell[r][c] = 0;
		v += rng_range(&g->rng, 3); /* un poco de variedad */
		if (v > best_v) {
			best_v = v;
			best = c;
		}
	}
	return best;
}
