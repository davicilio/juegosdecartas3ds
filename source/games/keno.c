#include "games/keno.h"

/* KN_PAY[elegidos][aciertos], en décimas: 36 = 3,6 veces la apuesta */
const int KN_PAY[KN_MAX_PICK + 1][KN_MAX_PICK + 1] = {
	[1] = { [1] = 36 },
	[2] = { [2] = 150 },
	[3] = { [2] = 20, [3] = 460 },
	[4] = { [2] = 10, [3] = 60, [4] = 1500 },
	[5] = { [3] = 30, [4] = 200, [5] = 6000 },
	[6] = { [3] = 20, [4] = 70, [5] = 800, [6] = 15000 },
	[7] = { [3] = 10, [4] = 50, [5] = 200, [6] = 3000, [7] = 50000 },
	[8] = { [4] = 20, [5] = 120, [6] = 1000, [7] = 15000, [8] = 100000 },
	[9] = { [4] = 20, [5] = 50, [6] = 400, [7] = 3000, [8] = 40000, [9] = 100000 },
	[10] = { [0] = 30, [5] = 30, [6] = 200, [7] = 1400, [8] = 10000, [9] = 40000, [10] = 100000 },
};

void kn_init(Keno* g, uint32_t seed)
{
	rng_seed(&g->rng, seed);
	kn_clear(g);
}

void kn_clear(Keno* g)
{
	for (int i = 0; i <= KN_NUMBERS; i++)
		g->picked[i] = g->is_drawn[i] = false;
	g->picks = 0;
	g->hits = 0;
}

bool kn_toggle(Keno* g, int n)
{
	if (n < 1 || n > KN_NUMBERS)
		return false;
	if (g->picked[n]) {
		g->picked[n] = false;
		g->picks--;
		return true;
	}
	if (g->picks >= KN_MAX_PICK)
		return false;
	g->picked[n] = true;
	g->picks++;
	return true;
}

void kn_quick_pick(Keno* g, int count)
{
	for (int i = 0; i <= KN_NUMBERS; i++)
		g->picked[i] = false;
	g->picks = 0;
	while (g->picks < count && g->picks < KN_MAX_PICK)
		kn_toggle(g, 1 + rng_range(&g->rng, KN_NUMBERS));
}

int kn_draw(Keno* g)
{
	int pool[KN_NUMBERS];
	for (int i = 0; i < KN_NUMBERS; i++)
		pool[i] = i + 1;
	for (int i = 0; i <= KN_NUMBERS; i++)
		g->is_drawn[i] = false;
	g->hits = 0;
	for (int k = 0; k < KN_DRAWN; k++) {
		int j = k + rng_range(&g->rng, KN_NUMBERS - k);
		int t = pool[k];
		pool[k] = pool[j];
		pool[j] = t;
		g->drawn[k] = pool[k];
		g->is_drawn[pool[k]] = true;
		if (g->picked[pool[k]])
			g->hits++;
	}
	return kn_pay(g->picks, g->hits);
}

int kn_payout(int bet, int picks, int hits)
{
	return bet * kn_pay(picks, hits) / 10;
}

int kn_pay(int picks, int hits)
{
	if (picks < 1 || picks > KN_MAX_PICK || hits < 0 || hits > picks)
		return 0;
	return KN_PAY[picks][hits];
}
