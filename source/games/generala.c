#include "games/generala.h"

#include <string.h>

const char* ge_cat_name(int cat)
{
	static const char* const NAMES[GE_CATS] = { "Al 1", "Al 2", "Al 3", "Al 4", "Al 5", "Al 6", "Escalera", "Full",
		"Póker", "Generala", "Doble generala" };
	return cat >= 0 && cat < GE_CATS ? NAMES[cat] : "";
}

void ge_init(Generala* g, int players, uint32_t seed)
{
	memset(g, 0, sizeof *g);
	rng_seed(&g->rng, seed);
	g->players = players < 1 ? 1 : players > GE_PLAYERS ? GE_PLAYERS : players;
	for (int p = 0; p < GE_PLAYERS; p++)
		for (int c = 0; c < GE_CATS; c++)
			g->score[p][c] = -1;
	g->winner = -1;
	g->phase = GE_ROLL;
}

bool ge_roll(Generala* g, const int* values)
{
	if (g->phase != GE_ROLL || g->rolls >= GE_ROLLS)
		return false;
	for (int i = 0; i < GE_DICE; i++) {
		if (g->rolls > 0 && g->hold[i])
			continue;
		g->dice[i] = values ? values[i] : 1 + rng_range(&g->rng, 6);
	}
	if (g->rolls == 0)
		for (int i = 0; i < GE_DICE; i++)
			g->hold[i] = false;
	g->rolls++;
	return true;
}

void ge_toggle_hold(Generala* g, int i)
{
	if (g->rolls > 0 && g->rolls < GE_ROLLS && i >= 0 && i < GE_DICE)
		g->hold[i] = !g->hold[i];
}

static void counts(const Generala* g, int c[7])
{
	memset(c, 0, sizeof(int) * 7);
	for (int i = 0; i < GE_DICE; i++)
		c[g->dice[i]]++;
}

int ge_points(const Generala* g, int cat)
{
	if (g->rolls == 0)
		return 0;
	int c[7];
	counts(g, c);
	bool served = g->rolls == 1;
	int max = 0;
	bool two = false, three = false;
	for (int v = 1; v <= 6; v++) {
		if (c[v] > max)
			max = c[v];
		two |= c[v] == 2;
		three |= c[v] == 3;
	}
	switch (cat) {
	case GE_STRAIGHT: {
		bool low = c[1] && c[2] && c[3] && c[4] && c[5], high = c[2] && c[3] && c[4] && c[5] && c[6];
		return low || high ? (served ? 25 : 20) : 0;
	}
	case GE_FULL:
		return two && three ? (served ? 35 : 30) : 0;
	case GE_POKER:
		return max >= 4 ? (served ? 45 : 40) : 0;
	case GE_GENERALA:
		return max == 5 ? 50 : 0;
	case GE_DOUBLE:
		return max == 5 ? 100 : 0;
	default:
		return c[cat + 1] * (cat + 1);
	}
}

int ge_value(const Generala* g, int cat)
{
	if (cat == GE_DOUBLE && g->score[g->current][GE_GENERALA] <= 0)
		return 0;
	return ge_points(g, cat);
}

bool ge_can_score(const Generala* g, int cat)
{
	if (g->phase != GE_ROLL || g->rolls == 0 || cat < 0 || cat >= GE_CATS || g->score[g->current][cat] >= 0)
		return false;
	return true;
}

int ge_total(const Generala* g, int p)
{
	int t = 0;
	for (int c = 0; c < GE_CATS; c++)
		if (g->score[p][c] > 0)
			t += g->score[p][c];
	return t;
}

bool ge_score(Generala* g, int cat)
{
	if (!ge_can_score(g, cat))
		return false;
	int p = g->current;
	int pts = ge_points(g, cat);
	/* La doble generala solo cuenta con la generala ya apuntada (y no tachada) */
	if (cat == GE_DOUBLE && g->score[p][GE_GENERALA] <= 0)
		pts = 0;
	/* Generala servida: gana en el acto */
	if (cat == GE_GENERALA && pts && g->rolls == 1) {
		g->score[p][cat] = pts;
		g->winner = p;
		g->served_win = true;
		g->phase = GE_OVER;
		return true;
	}
	g->score[p][cat] = pts;
	g->rolls = 0;
	for (int i = 0; i < GE_DICE; i++)
		g->hold[i] = false;
	g->current = (g->current + 1) % g->players;
	if (g->current == 0)
		g->turns++;
	if (g->turns >= GE_CATS) {
		int best = 0;
		for (int q = 1; q < g->players; q++)
			if (ge_total(g, q) > ge_total(g, best))
				best = q;
		g->winner = best;
		g->phase = GE_OVER;
	}
	return true;
}

/* ---------------------------------------------------------------------------
 * IA
 * ------------------------------------------------------------------------- */

static bool free_cat(const Generala* g, int cat)
{
	return g->score[g->current][cat] < 0;
}

bool ge_ai_hold(Generala* g)
{
	if (g->rolls >= GE_ROLLS)
		return false;
	int c[7];
	counts(g, c);
	/* ¿Ya hay algo bueno hecho? */
	if ((ge_points(g, GE_GENERALA) && (free_cat(g, GE_GENERALA) || free_cat(g, GE_DOUBLE))) ||
		(ge_points(g, GE_STRAIGHT) && free_cat(g, GE_STRAIGHT)) || (ge_points(g, GE_FULL) && free_cat(g, GE_FULL) && g->rolls == 1))
		return false;
	int best_v = 1;
	for (int v = 1; v <= 6; v++)
		if (c[v] > c[best_v] || (c[v] == c[best_v] && v > best_v))
			best_v = v;
	/* Cuatro distintos seguidos y la escalera libre: a por ella */
	if (free_cat(g, GE_STRAIGHT) && c[best_v] <= 2) {
		int run = (c[2] && c[3] && c[4] && c[5]) ? 2 : (c[1] && c[2] && c[3] && c[4]) ? 1 : (c[3] && c[4] && c[5] && c[6]) ? 3 : 0;
		if (run) {
			bool used[7] = { false };
			for (int i = 0; i < GE_DICE; i++) {
				int v = g->dice[i];
				g->hold[i] = v >= run && v <= run + 3 && !used[v];
				if (g->hold[i])
					used[v] = true;
			}
			return true;
		}
	}
	/* Full casi hecho: dos parejas */
	int pairs = 0;
	for (int v = 1; v <= 6; v++)
		pairs += c[v] == 2;
	if (pairs == 2 && free_cat(g, GE_FULL)) {
		for (int i = 0; i < GE_DICE; i++)
			g->hold[i] = c[g->dice[i]] == 2;
		return true;
	}
	for (int i = 0; i < GE_DICE; i++)
		g->hold[i] = g->dice[i] == best_v;
	return true;
}

int ge_ai_category(const Generala* g)
{
	int best = -1;
	double best_sc = -1e9;
	/* Valor esperado aproximado de cada casilla, para no gastar las buenas en balde */
	static const double EXPECT[GE_CATS] = { 2, 5, 8, 11, 14, 17, 12, 15, 10, 8, 3 };
	for (int cat = 0; cat < GE_CATS; cat++) {
		if (!ge_can_score(g, cat))
			continue;
		int pts = ge_value(g, cat);
		double sc = pts - EXPECT[cat];
		if (cat <= GE_SIXES)
			sc += pts >= 3 * (cat + 1) ? 4 : 0;
		if (sc > best_sc) {
			best_sc = sc;
			best = cat;
		}
	}
	return best;
}
