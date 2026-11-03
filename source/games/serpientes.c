#include "games/serpientes.h"

#include <string.h>

const SeLink SE_BOARD[SE_LINKS] = {
	/* Escaleras */
	{ 3, 22 }, { 8, 30 }, { 21, 42 }, { 28, 76 }, { 36, 57 }, { 50, 67 }, { 71, 92 }, { 80, 99 },
	/* Serpientes */
	{ 32, 10 }, { 47, 26 }, { 62, 18 }, { 74, 53 }, { 88, 24 }, { 93, 70 }, { 95, 75 }, { 98, 79 },
};

int se_follow(int square)
{
	for (int i = 0; i < SE_LINKS; i++)
		if (SE_BOARD[i].from == square)
			return SE_BOARD[i].to;
	return square;
}

void se_init(Serpientes* g, int players, uint32_t seed)
{
	memset(g, 0, sizeof *g);
	rng_seed(&g->rng, seed);
	g->players = players < 2 ? 2 : players > SE_PLAYERS ? SE_PLAYERS : players;
	g->winner = -1;
}

int se_roll(Serpientes* g, int die)
{
	if (g->winner >= 0)
		return 0;
	int d = die >= 1 && die <= 6 ? die : 1 + rng_range(&g->rng, 6);
	int p = g->current, t = g->pos[p] + d;
	if (t > SE_GOAL)
		t = SE_GOAL - (t - SE_GOAL);
	g->last_roll = d;
	g->last_land = t;
	g->last_to = se_follow(t);
	g->pos[p] = g->last_to;
	if (g->pos[p] == SE_GOAL)
		g->winner = p;
	else if (d != 6)
		g->current = (p + 1) % g->players;
	return d;
}
