#include "games/burro.h"

#include <string.h>

void bur_init(Burro* g, uint32_t seed)
{
	memset(g, 0, sizeof *g);
	rng_seed(&g->rng, seed);
	bur_new_game(g);
}

void bur_new_game(Burro* g)
{
	for (int p = 0; p < BUR_PLAYERS; p++)
		g->letters[p] = 0;
	bur_new_round(g);
}

void bur_new_round(Burro* g)
{
	/* Cuatro números al azar, con sus cuatro palos */
	int pos[RANKS_PER_SUIT];
	for (int i = 0; i < RANKS_PER_SUIT; i++)
		pos[i] = i;
	for (int i = RANKS_PER_SUIT - 1; i > 0; i--) {
		int j = rng_range(&g->rng, i + 1);
		int t = pos[i];
		pos[i] = pos[j];
		pos[j] = t;
	}
	Card cards[BUR_PLAYERS * BUR_HAND];
	int n = 0;
	for (int k = 0; k < BUR_PLAYERS; k++)
		for (int s = 0; s < NUM_SUITS; s++)
			cards[n++] = card_make((Suit)s, rank_from_pos(pos[k]));
	do {
		for (int i = n - 1; i > 0; i--) {
			int j = rng_range(&g->rng, i + 1);
			Card t = cards[i];
			cards[i] = cards[j];
			cards[j] = t;
		}
		for (int i = 0; i < n; i++)
			g->hands[i / BUR_HAND][i % BUR_HAND] = cards[i];
	} while (bur_holder(g) >= 0); /* nadie empieza ya con cuatro */
	memset(g->slapped, 0, sizeof g->slapped);
	g->slaps = 0;
	g->round_loser = -1;
	g->false_slap = false;
	g->passes = 0;
	g->phase = BUR_PASSING;
}

bool bur_has_four(const Burro* g, int player)
{
	for (int i = 1; i < BUR_HAND; i++)
		if (g->hands[player][i].rank != g->hands[player][0].rank)
			return false;
	return true;
}

int bur_holder(const Burro* g)
{
	for (int p = 0; p < BUR_PLAYERS; p++)
		if (bur_has_four(g, p))
			return p;
	return -1;
}

bool bur_pass(Burro* g, const int choice[BUR_PLAYERS])
{
	if (g->phase != BUR_PASSING)
		return false;
	Card out[BUR_PLAYERS];
	for (int p = 0; p < BUR_PLAYERS; p++) {
		if (choice[p] < 0 || choice[p] >= BUR_HAND)
			return false;
		out[p] = g->hands[p][choice[p]];
	}
	/* Cada uno recibe la del de su izquierda (el anterior) en el hueco que deja */
	for (int p = 0; p < BUR_PLAYERS; p++)
		g->hands[p][choice[p]] = out[(p + BUR_PLAYERS - 1) % BUR_PLAYERS];
	g->passes++;
	return true;
}

static void end_round(Burro* g, int loser)
{
	g->round_loser = loser;
	g->letters[loser]++;
	g->phase = g->letters[loser] >= BUR_WORD ? BUR_GAME_OVER : BUR_ROUND_OVER;
}

bool bur_slap(Burro* g, int player)
{
	if (g->phase == BUR_PASSING) {
		if (bur_holder(g) < 0) {
			/* Sin cuatro iguales en la mesa: la letra es para él */
			g->false_slap = true;
			end_round(g, player);
			return true;
		}
		g->phase = BUR_SLAP;
	}
	if (g->phase != BUR_SLAP || g->slapped[player])
		return false;
	g->slapped[player] = true;
	g->slap_order[g->slaps++] = player;
	if (g->slaps == BUR_PLAYERS - 1) {
		/* Queda uno: el último */
		for (int p = 0; p < BUR_PLAYERS; p++)
			if (!g->slapped[p]) {
				g->slapped[p] = true;
				g->slap_order[g->slaps++] = p;
				end_round(g, p);
			}
	}
	return true;
}

int bur_loser(const Burro* g)
{
	for (int p = 0; p < BUR_PLAYERS; p++)
		if (g->letters[p] >= BUR_WORD)
			return p;
	return -1;
}

int bur_ai_pass(Burro* g, int player)
{
	/* Se queda con el número del que más tiene (si empata, uno al azar) y pasa
	 * una carta de otro número */
	int count[13] = { 0 };
	for (int i = 0; i < BUR_HAND; i++)
		count[g->hands[player][i].rank]++;
	int keep = -1, ties = 0;
	for (int i = 0; i < BUR_HAND; i++) {
		int r = g->hands[player][i].rank;
		if (keep < 0 || count[r] > count[keep]) {
			keep = r;
			ties = 1;
		} else if (r != keep && count[r] == count[keep] && rng_range(&g->rng, ++ties) == 0) {
			keep = r;
		}
	}
	int options[BUR_HAND], n = 0;
	for (int i = 0; i < BUR_HAND; i++)
		if (g->hands[player][i].rank != keep)
			options[n++] = i;
	return n ? options[rng_range(&g->rng, n)] : 0;
}
