#include "games/oca.h"

#include <string.h>

static const int GEESE[] = { 5, 9, 14, 18, 23, 27, 32, 36, 41, 45, 50, 54, 59 };

OcaSquare oca_square(int n)
{
	for (unsigned i = 0; i < sizeof GEESE / sizeof GEESE[0]; i++)
		if (GEESE[i] == n)
			return OCA_GOOSE;
	switch (n) {
	case 6:
	case 12:
		return OCA_BRIDGE;
	case 26:
	case 53:
		return OCA_DICE;
	case 19:
		return OCA_INN;
	case 31:
		return OCA_WELL;
	case 42:
		return OCA_MAZE;
	case 56:
		return OCA_JAIL;
	case 58:
		return OCA_DEATH;
	case OCA_GOAL:
		return OCA_GARDEN;
	default:
		return OCA_NORMAL;
	}
}

void oca_init(Oca* g, int players, uint32_t seed)
{
	memset(g, 0, sizeof *g);
	g->players = players < 2 ? 2 : players > OCA_MAX_PLAYERS ? OCA_MAX_PLAYERS : players;
	rng_seed(&g->rng, seed);
	oca_new_game(g);
}

void oca_new_game(Oca* g)
{
	for (int p = 0; p < OCA_MAX_PLAYERS; p++) {
		g->pos[p] = 1;
		g->skip[p] = 0;
		g->in_well[p] = false;
	}
	g->current = 0;
	g->winner = -1;
	g->last_player = -1;
	g->last_freed = -1;
}

static int next_goose(int from)
{
	for (unsigned i = 0; i < sizeof GEESE / sizeof GEESE[0]; i++)
		if (GEESE[i] > from)
			return GEESE[i];
	return OCA_GOAL;
}

static void advance_turn(Oca* g)
{
	int p = g->current;
	for (int guard = 0; guard < 64; guard++) {
		p = (p + 1) % g->players;
		if (g->in_well[p])
			continue;
		if (g->skip[p] > 0) {
			g->skip[p]--;
			continue;
		}
		break;
	}
	g->current = p;
}

int oca_roll(Oca* g, int roll)
{
	if (g->winner >= 0)
		return 0;
	int p = g->current;
	if (roll < 1 || roll > 6)
		roll = 1 + rng_range(&g->rng, 6);
	int to = g->pos[p] + roll;
	if (to > OCA_GOAL)
		to = OCA_GOAL - (to - OCA_GOAL);
	g->last_player = p;
	g->last_roll = roll;
	g->last_land = to;
	g->last_square = oca_square(to);
	g->last_again = false;
	g->last_freed = -1;
	switch (g->last_square) {
	case OCA_GOOSE:
		to = next_goose(to);
		g->last_again = to != OCA_GOAL;
		break;
	case OCA_BRIDGE:
		to = to == 6 ? 12 : 6;
		g->last_again = true;
		break;
	case OCA_DICE:
		to = to == 26 ? 53 : 26;
		g->last_again = true;
		break;
	case OCA_INN:
		g->skip[p] = 1;
		break;
	case OCA_WELL:
		for (int q = 0; q < g->players; q++)
			if (q != p && g->in_well[q]) {
				g->in_well[q] = false;
				g->last_freed = q;
			}
		g->in_well[p] = true;
		break;
	case OCA_MAZE:
		to = 30;
		break;
	case OCA_JAIL:
		g->skip[p] = 2;
		break;
	case OCA_DEATH:
		to = 1;
		break;
	default:
		break;
	}
	g->pos[p] = to;
	g->last_to = to;
	if (to == OCA_GOAL) {
		g->winner = p;
		return roll;
	}
	/* Si todos los demás están en el pozo o castigados, sigue jugando el mismo */
	if (!g->last_again)
		advance_turn(g);
	return roll;
}
