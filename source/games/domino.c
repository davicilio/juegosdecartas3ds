#include "games/domino.h"

int do_left(const Domino* g)
{
	return g->chain_count ? g->chain[0].a : -1;
}

int do_right(const Domino* g)
{
	return g->chain_count ? g->chain[g->chain_count - 1].b : -1;
}

int do_pips(const Domino* g, int player)
{
	int s = 0;
	for (int i = 0; i < g->hand_count[player]; i++)
		s += g->hands[player][i].a + g->hands[player][i].b;
	return s;
}

void do_init(Domino* g, uint32_t seed)
{
	rng_seed(&g->rng, seed);
	g->score[0] = g->score[1] = 0;
	g->first_hand = true;
	g->starter = 0;
	do_new_hand(g);
}

void do_new_hand(Domino* g)
{
	DoTile all[DO_TILES];
	int n = 0;
	for (int a = 0; a <= 6; a++)
		for (int b = a; b <= 6; b++)
			all[n++] = (DoTile){ (uint8_t)a, (uint8_t)b };
	for (int i = n - 1; i > 0; i--) {
		int j = rng_range(&g->rng, i + 1);
		DoTile t = all[i];
		all[i] = all[j];
		all[j] = t;
	}
	for (int p = 0; p < DO_PLAYERS; p++) {
		g->hand_count[p] = DO_HAND;
		for (int i = 0; i < DO_HAND; i++)
			g->hands[p][i] = all[p * DO_HAND + i];
	}
	g->chain_count = 0;
	g->passes = 0;
	g->hand_winner = -1;
	g->hand_points = 0;
	g->blocked = false;
	g->domino_player = -1;
	g->last_player = -1;
	g->last_passed = false;
	g->phase = DO_PLAYING;
	g->current = g->starter;
	if (g->first_hand)
		for (int p = 0; p < DO_PLAYERS; p++)
			for (int i = 0; i < DO_HAND; i++)
				if (g->hands[p][i].a == 6 && g->hands[p][i].b == 6)
					g->current = p;
}

bool do_can_play(const Domino* g, int idx, int side)
{
	int p = g->current;
	if (g->phase != DO_PLAYING || idx < 0 || idx >= g->hand_count[p])
		return false;
	DoTile t = g->hands[p][idx];
	if (g->chain_count == 0) {
		/* La primera de la partida tiene que ser el seis doble */
		return !g->first_hand || (t.a == 6 && t.b == 6);
	}
	int end = side == 0 ? do_left(g) : do_right(g);
	return t.a == end || t.b == end;
}

bool do_has_move(const Domino* g, int player)
{
	Domino tmp = *g;
	tmp.current = player;
	for (int i = 0; i < g->hand_count[player]; i++)
		if (do_can_play(&tmp, i, 0) || do_can_play(&tmp, i, 1))
			return true;
	return false;
}

static void end_hand(Domino* g, int team)
{
	int pts = 0;
	for (int p = 0; p < DO_PLAYERS; p++)
		if (p % 2 != team)
			pts += do_pips(g, p);
	g->hand_winner = team;
	g->hand_points = pts;
	g->score[team] += pts;
	g->first_hand = false;
	g->phase = g->score[team] >= DO_GAME ? DO_GAME_OVER : DO_HAND_OVER;
}

bool do_play(Domino* g, int idx, int side)
{
	if (!do_can_play(g, idx, side))
		return false;
	int p = g->current;
	DoTile t = g->hands[p][idx];
	for (int i = idx; i < g->hand_count[p] - 1; i++)
		g->hands[p][i] = g->hands[p][i + 1];
	g->hand_count[p]--;
	if (g->chain_count == 0) {
		g->chain[g->chain_count++] = t;
	} else if (side == 0) {
		/* Por la izquierda: su lado derecho tiene que casar con el extremo */
		DoTile o = t.b == do_left(g) ? t : (DoTile){ t.b, t.a };
		for (int i = g->chain_count; i > 0; i--)
			g->chain[i] = g->chain[i - 1];
		g->chain[0] = o;
		g->chain_count++;
	} else {
		DoTile o = t.a == do_right(g) ? t : (DoTile){ t.b, t.a };
		g->chain[g->chain_count++] = o;
	}
	g->passes = 0;
	g->last_player = p;
	g->last_passed = false;
	if (g->hand_count[p] == 0) {
		g->domino_player = p;
		g->starter = p;
		end_hand(g, p % 2);
		return true;
	}
	g->current = (p + 1) % DO_PLAYERS;
	return true;
}

bool do_pass(Domino* g)
{
	if (g->phase != DO_PLAYING || do_has_move(g, g->current))
		return false;
	g->last_player = g->current;
	g->last_passed = true;
	g->passes++;
	if (g->passes >= DO_PLAYERS) {
		/* Cierre: gana la pareja con menos puntos */
		g->blocked = true;
		int t0 = do_pips(g, 0) + do_pips(g, 2), t1 = do_pips(g, 1) + do_pips(g, 3);
		int team = t0 <= t1 ? 0 : 1;
		/* Empieza la siguiente quien menos puntos tenga de esa pareja */
		g->starter = do_pips(g, team) <= do_pips(g, team + 2) ? team : team + 2;
		end_hand(g, team);
		return true;
	}
	g->current = (g->current + 1) % DO_PLAYERS;
	return true;
}

bool do_ai_choose(const Domino* g, int* idx, int* side)
{
	int p = g->current;
	int best = -1, best_side = 0, best_score = -1000;
	/* Cuántas fichas tiene de cada número: conviene dejar extremos que tenga */
	int count[7] = { 0 };
	for (int i = 0; i < g->hand_count[p]; i++) {
		count[g->hands[p][i].a]++;
		count[g->hands[p][i].b]++;
	}
	for (int i = 0; i < g->hand_count[p]; i++)
		for (int s = 0; s < 2; s++) {
			if (!do_can_play(g, i, s))
				continue;
			DoTile t = g->hands[p][i];
			int score = (t.a + t.b) * 2 + (t.a == t.b ? 8 : 0);
			if (g->chain_count > 0) {
				int end = s == 0 ? do_left(g) : do_right(g);
				int left_open = t.a == end ? t.b : t.a; /* el número que queda en ese extremo */
				score += count[left_open] * 3;
			}
			if (score > best_score) {
				best_score = score;
				best = i;
				best_side = s;
			}
		}
	if (best < 0)
		return false;
	*idx = best;
	*side = best_side;
	return true;
}
