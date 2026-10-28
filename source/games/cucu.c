#include "games/cucu.h"

int cucu_value(Card c)
{
	return card_pos(c); /* as 0 ... rey 9 */
}

void cucu_init(Cucu* g, uint32_t seed)
{
	rng_seed(&g->rng, seed);
	g->dealer = CUCU_PLAYERS - 1;
	cucu_new_game(g);
}

void cucu_new_game(Cucu* g)
{
	for (int p = 0; p < CUCU_PLAYERS; p++)
		g->lives[p] = CUCU_LIVES;
	cucu_new_round(g);
}

bool cucu_alive(const Cucu* g, int player)
{
	return g->lives[player] > 0;
}

static int next_alive(const Cucu* g, int from)
{
	for (int k = 1; k <= CUCU_PLAYERS; k++) {
		int q = (from + k) % CUCU_PLAYERS;
		if (cucu_alive(g, q))
			return q;
	}
	return from;
}

void cucu_new_round(Cucu* g)
{
	deck_init(&g->deck);
	deck_shuffle(&g->deck, &g->rng);
	g->dealer = next_alive(g, g->dealer);
	for (int p = 0; p < CUCU_PLAYERS; p++) {
		g->lost[p] = false;
		if (cucu_alive(g, p))
			deck_draw(&g->deck, &g->card[p]);
	}
	g->current = next_alive(g, g->dealer);
	g->last_player = g->last_with = -1;
	g->last_blocked = false;
	g->phase = CUCU_PHASE_DECIDE;
}

int cucu_partner(const Cucu* g)
{
	return g->current == g->dealer ? -1 : next_alive(g, g->current);
}

static void reveal(Cucu* g)
{
	int low = 99;
	for (int p = 0; p < CUCU_PLAYERS; p++)
		if (cucu_alive(g, p) && cucu_value(g->card[p]) < low)
			low = cucu_value(g->card[p]);
	int losing = 0, alive = 0;
	for (int p = 0; p < CUCU_PLAYERS; p++)
		if (cucu_alive(g, p)) {
			alive++;
			g->lost[p] = cucu_value(g->card[p]) == low;
			losing += g->lost[p];
		}
	/* Si empatan todos, nadie pierde vida */
	for (int p = 0; p < CUCU_PLAYERS; p++)
		if (g->lost[p] && losing < alive)
			g->lives[p]--;
		else
			g->lost[p] = false;
	g->phase = cucu_winner(g) >= 0 ? CUCU_PHASE_GAME_OVER : CUCU_PHASE_REVEAL;
}

int cucu_winner(const Cucu* g)
{
	int alive = 0, last = -1;
	for (int p = 0; p < CUCU_PLAYERS; p++)
		if (cucu_alive(g, p)) {
			alive++;
			last = p;
		}
	if (alive == 1)
		return last;
	if (alive == 0) {
		/* Todos han perdido la última vida a la vez: gana el que reparte */
		return g->dealer;
	}
	return -1;
}

bool cucu_decide(Cucu* g, bool swap)
{
	if (g->phase != CUCU_PHASE_DECIDE)
		return false;
	int p = g->current;
	int q = cucu_partner(g);
	g->last_player = p;
	g->last_with = swap ? q : -2;
	g->last_blocked = false;
	/* Quien tiene un rey no cambia nunca */
	if (swap && g->card[p].rank != RANK_REY) {
		if (q < 0) {
			Card c;
			if (deck_draw(&g->deck, &c))
				g->card[p] = c;
		} else if (g->card[q].rank == RANK_REY) {
			g->last_blocked = true;
		} else {
			Card t = g->card[p];
			g->card[p] = g->card[q];
			g->card[q] = t;
		}
	}
	if (p == g->dealer)
		reveal(g);
	else
		g->current = next_alive(g, p);
	return true;
}

bool cucu_ai_swap(Cucu* g)
{
	int v = cucu_value(g->card[g->current]);
	/* Cuanto más baja, más ganas de cambiar */
	if (v <= 2)
		return true;
	if (v >= 6)
		return false;
	return (int)rng_range(&g->rng, 10) < (6 - v) * 2;
}
