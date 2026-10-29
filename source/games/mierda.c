#include "games/mierda.h"

#include <string.h>

int mi_value(Card c)
{
	switch (c.rank) {
	case 2:
		return 20; /* especial */
	case RANK_SOTA:
		return 21; /* especial */
	case RANK_CABALLO:
		return 6;
	case RANK_REY:
		return 7;
	case 1:
		return 8;
	default:
		return c.rank - 2; /* 3 -> 1 ... 7 -> 5 */
	}
}

static bool special(Card c)
{
	return c.rank == 2 || c.rank == RANK_SOTA;
}

void mi_init(Mierda* g, uint32_t seed)
{
	memset(g, 0, sizeof *g);
	rng_seed(&g->rng, seed);
	mi_new_game(g);
}

void mi_new_game(Mierda* g)
{
	Deck d;
	deck_init(&d);
	deck_shuffle(&d, &g->rng);
	for (int p = 0; p < MI_PLAYERS; p++) {
		g->hand_n[p] = 0;
		g->out[p] = false;
		for (int i = 0; i < 3; i++) {
			deck_draw(&d, &g->down[p][i]);
			g->down_has[p][i] = true;
		}
		for (int i = 0; i < 3; i++) {
			deck_draw(&d, &g->up[p][i]);
			g->up_has[p][i] = true;
		}
		for (int i = 0; i < 3; i++)
			deck_draw(&d, &g->hand[p][g->hand_n[p]++]);
	}
	g->deck_n = 0;
	Card c;
	while (deck_draw(&d, &c))
		g->deck[g->deck_n++] = c;
	g->pile_n = 0;
	g->burned = 0;
	g->out_n = 0;
	g->loser = -1;
	g->current = rng_range(&g->rng, MI_PLAYERS);
	g->last_player = -1;
	g->phase = MI_PLAYING;
}

MiSource mi_source(const Mierda* g, int p)
{
	if (g->hand_n[p] > 0)
		return MI_FROM_HAND;
	for (int i = 0; i < 3; i++)
		if (g->up_has[p][i])
			return MI_FROM_UP;
	for (int i = 0; i < 3; i++)
		if (g->down_has[p][i])
			return MI_FROM_DOWN;
	return MI_FROM_NONE;
}

bool mi_card_ok(const Mierda* g, Card c)
{
	if (special(c) || g->pile_n == 0)
		return true;
	Card top = g->pile[g->pile_n - 1];
	if (top.rank == 2)
		return true;
	return mi_value(c) >= mi_value(top);
}

bool mi_can_play(const Mierda* g, int p, int idx)
{
	if (g->phase != MI_PLAYING || p != g->current)
		return false;
	switch (mi_source(g, p)) {
	case MI_FROM_HAND:
		return idx >= 0 && idx < g->hand_n[p] && mi_card_ok(g, g->hand[p][idx]);
	case MI_FROM_UP:
		return idx >= 0 && idx < 3 && g->up_has[p][idx] && mi_card_ok(g, g->up[p][idx]);
	case MI_FROM_DOWN:
		return idx >= 0 && idx < 3 && g->down_has[p][idx]; /* a ciegas */
	default:
		return false;
	}
}

bool mi_must_take(const Mierda* g, int p)
{
	MiSource s = mi_source(g, p);
	if (s == MI_FROM_DOWN || s == MI_FROM_NONE || g->pile_n == 0)
		return false;
	int n = s == MI_FROM_HAND ? g->hand_n[p] : 3;
	for (int i = 0; i < n; i++)
		if (mi_can_play(g, p, i))
			return false;
	return true;
}

static void take_pile(Mierda* g, int p)
{
	for (int i = 0; i < g->pile_n; i++)
		g->hand[p][g->hand_n[p]++] = g->pile[i];
	g->pile_n = 0;
}

static int next_player(const Mierda* g, int from)
{
	for (int i = 1; i <= MI_PLAYERS; i++) {
		int p = (from + i) % MI_PLAYERS;
		if (!g->out[p])
			return p;
	}
	return from;
}

static void check_out(Mierda* g, int p)
{
	if (g->out[p] || mi_source(g, p) != MI_FROM_NONE)
		return;
	g->out[p] = true;
	g->out_order[g->out_n++] = p;
	if (g->out_n == MI_PLAYERS - 1) {
		for (int q = 0; q < MI_PLAYERS; q++)
			if (!g->out[q])
				g->loser = q;
		g->phase = MI_GAME_OVER;
	}
}

bool mi_take(Mierda* g)
{
	int p = g->current;
	if (!mi_must_take(g, p))
		return false;
	take_pile(g, p);
	g->last_player = p;
	g->last_took = true;
	g->last_burn = g->last_blind_fail = false;
	g->last_count = 0;
	g->current = next_player(g, p);
	return true;
}

static bool burns(const Mierda* g)
{
	if (g->pile_n == 0)
		return false;
	if (g->pile[g->pile_n - 1].rank == RANK_SOTA)
		return true;
	if (g->pile_n < 4)
		return false;
	int r = g->pile[g->pile_n - 1].rank;
	for (int i = 2; i <= 4; i++)
		if (g->pile[g->pile_n - i].rank != r)
			return false;
	return true;
}

bool mi_play(Mierda* g, int idx)
{
	int p = g->current;
	if (!mi_can_play(g, p, idx))
		return false;
	MiSource s = mi_source(g, p);
	g->last_player = p;
	g->last_took = g->last_burn = g->last_blind_fail = false;
	g->last_count = 0;
	if (s == MI_FROM_DOWN) {
		Card c = g->down[p][idx];
		g->down_has[p][idx] = false;
		g->last_card = c;
		g->last_count = 1;
		if (!mi_card_ok(g, c)) {
			/* No vale: se lleva el montón con ella */
			g->pile[g->pile_n++] = c;
			take_pile(g, p);
			g->last_blind_fail = true;
			g->last_took = true;
			g->current = next_player(g, p);
			return true;
		}
		g->pile[g->pile_n++] = c;
	} else if (s == MI_FROM_HAND) {
		int r = g->hand[p][idx].rank;
		g->last_card = g->hand[p][idx];
		int n = 0;
		for (int i = 0; i < g->hand_n[p]; i++) {
			if (g->hand[p][i].rank == r) {
				g->pile[g->pile_n++] = g->hand[p][i];
				g->last_count++;
			} else {
				g->hand[p][n++] = g->hand[p][i];
			}
		}
		g->hand_n[p] = n;
		while (g->hand_n[p] < 3 && g->deck_n > 0)
			g->hand[p][g->hand_n[p]++] = g->deck[--g->deck_n];
	} else {
		int r = g->up[p][idx].rank;
		g->last_card = g->up[p][idx];
		for (int i = 0; i < 3; i++)
			if (g->up_has[p][i] && g->up[p][i].rank == r) {
				g->pile[g->pile_n++] = g->up[p][i];
				g->up_has[p][i] = false;
				g->last_count++;
			}
	}
	bool burn = burns(g);
	if (burn) {
		g->burned += g->pile_n;
		g->pile_n = 0;
		g->last_burn = true;
	}
	check_out(g, p);
	if (g->phase == MI_GAME_OVER)
		return true;
	if (!(burn && !g->out[p]))
		g->current = next_player(g, p);
	return true;
}

int mi_ai_choose(Mierda* g, int p)
{
	MiSource s = mi_source(g, p);
	if (s == MI_FROM_DOWN) {
		int opts[3], n = 0;
		for (int i = 0; i < 3; i++)
			if (g->down_has[p][i])
				opts[n++] = i;
		return opts[rng_range(&g->rng, n)];
	}
	if (mi_must_take(g, p))
		return -1;
	int n = s == MI_FROM_HAND ? g->hand_n[p] : 3;
	const Card* cards = s == MI_FROM_HAND ? g->hand[p] : g->up[p];
	/* La carta normal más baja que valga; las especiales, para cuando hagan falta */
	int best = -1;
	for (int i = 0; i < n; i++) {
		if (!mi_can_play(g, p, i))
			continue;
		if (best < 0 || mi_value(cards[i]) < mi_value(cards[best]))
			best = i;
	}
	/* Con un montón grande y una sota a mano, se quema */
	if (best >= 0 && g->pile_n >= 6)
		for (int i = 0; i < n; i++)
			if (mi_can_play(g, p, i) && cards[i].rank == RANK_SOTA)
				return i;
	return best;
}
