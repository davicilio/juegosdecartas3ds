#include "games/gomoku.h"

#include <string.h>

static const int DIRS[4][2] = { { 0, 1 }, { 1, 0 }, { 1, 1 }, { 1, -1 } };

void go_init(Gomoku* g, int level, uint32_t seed)
{
	memset(g, 0, sizeof *g);
	rng_seed(&g->rng, seed);
	g->current = 1;
	g->level = level;
	g->last_r = g->last_c = -1;
}

static bool inside(int r, int c)
{
	return r >= 0 && c >= 0 && r < GO_N && c < GO_N;
}

static bool five_from(Gomoku* g, int r, int c)
{
	int p = g->cell[r][c];
	for (int d = 0; d < 4; d++) {
		int dr = DIRS[d][0], dc = DIRS[d][1];
		int a = 0, b = 0;
		while (inside(r - (a + 1) * dr, c - (a + 1) * dc) && g->cell[r - (a + 1) * dr][c - (a + 1) * dc] == p)
			a++;
		while (inside(r + (b + 1) * dr, c + (b + 1) * dc) && g->cell[r + (b + 1) * dr][c + (b + 1) * dc] == p)
			b++;
		if (a + b + 1 >= 5) {
			for (int k = 0; k < 5; k++) {
				g->win_r[k] = r + (k - a) * dr;
				g->win_c[k] = c + (k - a) * dc;
			}
			return true;
		}
	}
	return false;
}

bool go_play(Gomoku* g, int r, int c)
{
	if (g->winner || !inside(r, c) || g->cell[r][c])
		return false;
	g->cell[r][c] = (int8_t)g->current;
	g->moves++;
	g->last_r = r;
	g->last_c = c;
	if (five_from(g, r, c))
		g->winner = g->current;
	else if (g->moves == GO_N * GO_N)
		g->winner = 3;
	g->current = 3 - g->current;
	return true;
}

/* Valor de una línea de n piedras seguidas con open extremos libres */
static int pattern(int n, int open)
{
	if (n >= 5)
		return 1000000;
	if (open == 0)
		return 0;
	static const int OPEN2[5] = { 0, 10, 100, 5000, 100000 };  /* los dos lados libres */
	static const int OPEN1[5] = { 0, 1, 10, 500, 8000 };       /* un lado libre */
	return open == 2 ? OPEN2[n] : OPEN1[n];
}

/* Lo que vale para p poner en (r, c) */
static int value(const Gomoku* g, int r, int c, int p)
{
	int total = 0;
	for (int d = 0; d < 4; d++) {
		int dr = DIRS[d][0], dc = DIRS[d][1];
		int n = 1, open = 0;
		int k = 1;
		while (inside(r + k * dr, c + k * dc) && g->cell[r + k * dr][c + k * dc] == p)
			n++, k++;
		if (inside(r + k * dr, c + k * dc) && g->cell[r + k * dr][c + k * dc] == 0)
			open++;
		k = 1;
		while (inside(r - k * dr, c - k * dc) && g->cell[r - k * dr][c - k * dc] == p)
			n++, k++;
		if (inside(r - k * dr, c - k * dc) && g->cell[r - k * dr][c - k * dc] == 0)
			open++;
		total += pattern(n, open);
	}
	return total;
}

int go_ai(Gomoku* g)
{
	if (g->moves == 0)
		return GO_N / 2 * GO_N + GO_N / 2;
	int me = g->current, you = 3 - me;
	/* Cuánto pesa tapar al rival y cuánto se equivoca según el nivel */
	static const int DEFENSE[3] = { 6, 9, 11 };
	static const int NOISE[3] = { 400, 40, 0 };
	int best = -1, best_sc = -1;
	for (int r = 0; r < GO_N; r++)
		for (int c = 0; c < GO_N; c++) {
			if (g->cell[r][c])
				continue;
			/* Solo casillas cerca de alguna piedra */
			bool near = false;
			for (int dr = -2; dr <= 2 && !near; dr++)
				for (int dc = -2; dc <= 2 && !near; dc++)
					if (inside(r + dr, c + dc) && g->cell[r + dr][c + dc])
						near = true;
			if (!near)
				continue;
			int sc = value(g, r, c, me) * 10 + value(g, r, c, you) * DEFENSE[g->level];
			if (NOISE[g->level])
				sc += rng_range(&g->rng, NOISE[g->level]);
			/* En fácil, a veces no ve las amenazas del rival */
			if (g->level == 0 && rng_range(&g->rng, 4) == 0)
				sc = value(g, r, c, me) * 10 + rng_range(&g->rng, 200);
			if (sc > best_sc) {
				best_sc = sc;
				best = r * GO_N + c;
			}
		}
	return best;
}
