#include "games/tresenraya.h"

#include <string.h>

static const int LINES[8][3] = { { 0, 1, 2 }, { 3, 4, 5 }, { 6, 7, 8 }, { 0, 3, 6 },
	{ 1, 4, 7 }, { 2, 5, 8 }, { 0, 4, 8 }, { 2, 4, 6 } };

void tx_init(TresEnRaya* g, uint32_t seed)
{
	memset(g, 0, sizeof *g);
	rng_seed(&g->rng, seed);
	g->level = 1;
	g->starter = 2;
	tx_new_game(g);
}

void tx_new_game(TresEnRaya* g)
{
	memset(g->cell, 0, sizeof g->cell);
	g->starter = 3 - g->starter; /* se alterna quién empieza */
	g->current = g->starter;
	g->winner = 0;
}

static int winner_of(const uint8_t* c, int* line)
{
	for (int i = 0; i < 8; i++) {
		int a = c[LINES[i][0]];
		if (a && a == c[LINES[i][1]] && a == c[LINES[i][2]]) {
			if (line)
				memcpy(line, LINES[i], sizeof LINES[i]);
			return a;
		}
	}
	for (int i = 0; i < 9; i++)
		if (!c[i])
			return 0;
	return 3;
}

bool tx_play(TresEnRaya* g, int cell)
{
	if (g->winner || cell < 0 || cell > 8 || g->cell[cell])
		return false;
	g->cell[cell] = (uint8_t)g->current;
	g->winner = winner_of(g->cell, g->line);
	if (!g->winner)
		g->current = 3 - g->current;
	return true;
}

/* Minimax completo: el tablero es tan pequeño que se puede mirar todo */
static int minimax(uint8_t* c, int player, int me, int depth)
{
	int w = winner_of(c, NULL);
	if (w == me)
		return 10 - depth;
	if (w == 3)
		return 0;
	if (w)
		return depth - 10;
	int best = player == me ? -100 : 100;
	for (int i = 0; i < 9; i++) {
		if (c[i])
			continue;
		c[i] = (uint8_t)player;
		int v = minimax(c, 3 - player, me, depth + 1);
		c[i] = 0;
		if (player == me ? v > best : v < best)
			best = v;
	}
	return best;
}

int tx_ai_choose(TresEnRaya* g)
{
	int free_cells[9], n = 0;
	for (int i = 0; i < 9; i++)
		if (!g->cell[i])
			free_cells[n++] = i;
	if (!n)
		return -1;
	/* Despistes según el nivel */
	int slip = g->level == 0 ? 45 : g->level == 1 ? 15 : 0;
	if (rng_range(&g->rng, 100) < slip)
		return free_cells[rng_range(&g->rng, n)];
	int me = g->current, best_v = -1000, best[9], nb = 0;
	for (int k = 0; k < n; k++) {
		int i = free_cells[k];
		g->cell[i] = (uint8_t)me;
		int v = minimax(g->cell, 3 - me, me, 1);
		g->cell[i] = 0;
		if (v > best_v) {
			best_v = v;
			nb = 0;
		}
		if (v == best_v)
			best[nb++] = i;
	}
	return best[rng_range(&g->rng, nb)];
}
