#include "games/mancala.h"

#include <string.h>

int mc_store(int p)
{
	return p == 0 ? 6 : 13;
}

void mc_init(Mancala* g, int level, uint32_t seed)
{
	memset(g, 0, sizeof *g);
	rng_seed(&g->rng, seed);
	for (int i = 0; i < 6; i++)
		g->pit[i] = g->pit[7 + i] = 4;
	g->level = level;
	g->last = -1;
}

static int pit_index(int p, int i)
{
	return p * 7 + i;
}

bool mc_can_play(const Mancala* g, int i)
{
	return !g->over && i >= 0 && i < 6 && g->pit[pit_index(g->current, i)] > 0;
}

static void finish_if_empty(Mancala* g)
{
	int a = 0, b = 0;
	for (int i = 0; i < 6; i++) {
		a += g->pit[i];
		b += g->pit[7 + i];
	}
	if (a && b)
		return;
	for (int i = 0; i < 6; i++) {
		g->pit[6] += g->pit[i];
		g->pit[13] += g->pit[7 + i];
		g->pit[i] = g->pit[7 + i] = 0;
	}
	g->over = true;
}

bool mc_play(Mancala* g, int i)
{
	if (!mc_can_play(g, i))
		return false;
	int p = g->current, k = pit_index(p, i);
	int seeds = g->pit[k];
	g->pit[k] = 0;
	while (seeds > 0) {
		k = (k + 1) % MC_PITS;
		if (k == mc_store(1 - p))
			continue;
		g->pit[k]++;
		seeds--;
	}
	g->last = k;
	g->captured = false;
	g->again = k == mc_store(p);
	bool mine = p == 0 ? k <= 5 : k >= 7 && k <= 12;
	if (mine && g->pit[k] == 1 && g->pit[12 - k] > 0) {
		g->pit[mc_store(p)] += g->pit[k] + g->pit[12 - k];
		g->pit[k] = g->pit[12 - k] = 0;
		g->captured = true;
	}
	if (!g->again)
		g->current = 1 - p;
	finish_if_empty(g);
	return true;
}

static int score(const Mancala* g, int p)
{
	return g->pit[mc_store(p)] - g->pit[mc_store(1 - p)];
}

static int search(const Mancala* g, int depth, int me, int alpha, int beta)
{
	if (g->over || depth == 0)
		return score(g, me) * 10 + (g->over ? (score(g, me) > 0 ? 1000 : score(g, me) < 0 ? -1000 : 0) : 0);
	bool max = g->current == me;
	int best = max ? -100000 : 100000;
	for (int i = 0; i < 6; i++) {
		if (!mc_can_play(g, i))
			continue;
		Mancala n = *g;
		mc_play(&n, i);
		int sc = search(&n, depth - 1, me, alpha, beta);
		if (max) {
			if (sc > best)
				best = sc;
			if (sc > alpha)
				alpha = sc;
		} else {
			if (sc < best)
				best = sc;
			if (sc < beta)
				beta = sc;
		}
		if (alpha >= beta)
			break;
	}
	return best;
}

int mc_ai(Mancala* g)
{
	static const int DEPTH[3] = { 1, 4, 8 };
	int me = g->current, best = -1, best_sc = -1000000;
	for (int i = 0; i < 6; i++) {
		if (!mc_can_play(g, i))
			continue;
		Mancala n = *g;
		mc_play(&n, i);
		int sc = search(&n, DEPTH[g->level] - 1, me, -100000, 100000);
		if (g->level == 0)
			sc += rng_range(&g->rng, 40);
		if (sc > best_sc) {
			best_sc = sc;
			best = i;
		}
	}
	return best;
}
