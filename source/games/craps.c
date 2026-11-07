#include "games/craps.h"

void cr_init(Craps* g, uint32_t seed)
{
	rng_seed(&g->rng, seed);
	g->point = 0;
	g->die[0] = 5;
	g->die[1] = 2;
	for (int i = 0; i < CR_BETS; i++)
		g->bets[i] = 0;
	g->last_return = g->last_lost = 0;
}

bool cr_can_bet(const Craps* g, int bet)
{
	return (bet != CR_PASS && bet != CR_DONTPASS) || g->point == 0;
}

/* Resuelve una apuesta: mult = lo que se devuelve por ficha (0 pierde) */
static void settle(Craps* g, int bet, int mult, int* ret)
{
	int a = g->bets[bet];
	if (!a)
		return;
	if (mult)
		*ret += a * mult;
	else
		g->last_lost += a;
	g->bets[bet] = 0;
}

int cr_roll(Craps* g, int d1, int d2)
{
	g->die[0] = d1 ? d1 : 1 + rng_range(&g->rng, 6);
	g->die[1] = d2 ? d2 : 1 + rng_range(&g->rng, 6);
	int s = g->die[0] + g->die[1], ret = 0;
	g->last_lost = 0;
	/* Las de una tirada */
	settle(g, CR_FIELD, s == 2 ? 3 : s == 12 ? 4 : (s <= 4 || s >= 9) ? 2 : 0, &ret);
	settle(g, CR_ANY7, s == 7 ? 5 : 0, &ret);
	settle(g, CR_CRAPS, (s == 2 || s == 3 || s == 12) ? 8 : 0, &ret);
	if (g->point == 0) {
		if (s == 7 || s == 11) {
			settle(g, CR_PASS, 2, &ret);
			settle(g, CR_DONTPASS, 0, &ret);
		} else if (s == 2 || s == 3 || s == 12) {
			settle(g, CR_PASS, 0, &ret);
			if (s == 12)
				settle(g, CR_DONTPASS, 1, &ret); /* empate: se devuelve */
			else
				settle(g, CR_DONTPASS, 2, &ret);
		} else {
			g->point = s;
		}
	} else if (s == g->point) {
		settle(g, CR_PASS, 2, &ret);
		settle(g, CR_DONTPASS, 0, &ret);
		g->point = 0;
	} else if (s == 7) {
		settle(g, CR_PASS, 0, &ret);
		settle(g, CR_DONTPASS, 2, &ret);
		g->point = 0;
	}
	g->last_return = ret;
	return ret;
}
