#include "games/videopoker.h"

#include <string.h>

int vp_pay(int hand, int bet)
{
	static const int PAY[VP_HANDS] = { 250, 50, 25, 9, 6, 4, 3, 2, 1, 0 };
	if (hand == VP_ROYAL && bet == VP_MAX_BET)
		return 800 * bet;
	return hand >= 0 && hand < VP_HANDS ? PAY[hand] * bet : 0;
}

const char* vp_hand_name(int hand)
{
	static const char* const names[VP_HANDS] = { "Escalera real", "Escalera de color", "Póker", "Full", "Color",
		"Escalera", "Trío", "Doble pareja", "Jotas o más", "Nada" };
	return hand >= 0 && hand < VP_HANDS ? names[hand] : "?";
}

int vp_rank(const PkCard* h)
{
	uint32_t s = pk_evaluate(h, 5);
	switch (pk_category(s)) {
	case 8: {
		/* Escalera real: de diez a as del mismo palo */
		int lo = 14;
		for (int i = 0; i < 5; i++)
			if (h[i].rank < lo)
				lo = h[i].rank;
		return lo == 10 ? VP_ROYAL : VP_STRAIGHT_FLUSH;
	}
	case 7:
		return VP_QUADS;
	case 6:
		return VP_FULL;
	case 5:
		return VP_FLUSH;
	case 4:
		return VP_STRAIGHT;
	case 3:
		return VP_TRIPS;
	case 2:
		return VP_TWO_PAIR;
	case 1: {
		int count[15] = { 0 };
		for (int i = 0; i < 5; i++)
			count[h[i].rank]++;
		for (int r = 11; r <= 14; r++)
			if (count[r] == 2)
				return VP_JACKS;
		return VP_NOTHING;
	}
	default:
		return VP_NOTHING;
	}
}

void vp_init(VideoPoker* g, uint32_t seed)
{
	memset(g, 0, sizeof *g);
	rng_seed(&g->rng, seed);
	g->credits = VP_START;
	g->bet = 1;
	g->phase = VP_BET;
	g->result = VP_NOTHING;
}

static void shuffle(VideoPoker* g)
{
	g->deck_n = 0;
	for (int s = 0; s < 4; s++)
		for (int r = 2; r <= 14; r++)
			g->deck[g->deck_n++] = (PkCard){ (uint8_t)r, (uint8_t)s };
	for (int i = g->deck_n - 1; i > 0; i--) {
		int j = rng_range(&g->rng, i + 1);
		PkCard t = g->deck[i];
		g->deck[i] = g->deck[j];
		g->deck[j] = t;
	}
}

bool vp_deal(VideoPoker* g)
{
	if (g->phase == VP_HOLD || g->bet < 1 || g->bet > VP_MAX_BET || g->credits < g->bet)
		return false;
	g->credits -= g->bet;
	shuffle(g);
	for (int i = 0; i < 5; i++) {
		g->hand[i] = g->deck[--g->deck_n];
		g->held[i] = false;
	}
	g->win = 0;
	g->result = vp_rank(g->hand);
	g->phase = VP_HOLD;
	return true;
}

bool vp_toggle(VideoPoker* g, int i)
{
	if (g->phase != VP_HOLD || i < 0 || i >= 5)
		return false;
	g->held[i] = !g->held[i];
	return true;
}

bool vp_draw(VideoPoker* g)
{
	if (g->phase != VP_HOLD)
		return false;
	for (int i = 0; i < 5; i++)
		if (!g->held[i])
			g->hand[i] = g->deck[--g->deck_n];
	g->result = vp_rank(g->hand);
	g->win = vp_pay(g->result, g->bet);
	g->credits += g->win;
	g->hands++;
	if (g->win > g->best_win)
		g->best_win = g->win;
	g->phase = VP_RESULT;
	return true;
}

void vp_advice(VideoPoker* g, bool* hold)
{
	/* Las 32 formas de guardar, cada una con unas cuantas manos al azar */
	float best = -1.0f;
	int best_mask = 0;
	PkCard rest[52];
	for (int mask = 0; mask < 32; mask++) {
		int keep = __builtin_popcount(mask);
		int samples = keep == 5 ? 1 : keep == 4 ? 94 : keep == 3 ? 200 : 400;
		int total = 0;
		for (int k = 0; k < samples; k++) {
			int n = g->deck_n;
			memcpy(rest, g->deck, sizeof(PkCard) * n);
			PkCard h[5];
			for (int i = 0; i < 5; i++) {
				if (mask & (1 << i)) {
					h[i] = g->hand[i];
				} else {
					int j = rng_range(&g->rng, n);
					h[i] = rest[j];
					rest[j] = rest[--n];
				}
			}
			total += vp_pay(vp_rank(h), 1);
		}
		float ev = (float)total / samples;
		if (ev > best) {
			best = ev;
			best_mask = mask;
		}
	}
	for (int i = 0; i < 5; i++)
		hold[i] = (best_mask >> i) & 1;
}
