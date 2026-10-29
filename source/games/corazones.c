#include "games/corazones.h"

#include <string.h>

#define CLUBS  3
#define HEARTS 1
#define SPADES 0

int he_card_points(PkCard c)
{
	if (c.suit == HEARTS)
		return 1;
	if (c.suit == SPADES && c.rank == 12)
		return 13;
	return 0;
}

int he_pass_direction(const Corazones* g)
{
	static const int dir[4] = { 1, -1, 2, 0 };
	return dir[g->hand_number % 4];
}

static void sort_hand(Corazones* g, int p)
{
	/* Por palos (tréboles, diamantes, picas, corazones) y valor */
	static const int order[4] = { 2, 3, 1, 0 };
	PkCard* h = g->hands[p];
	for (int i = 1; i < g->hand_count[p]; i++) {
		PkCard c = h[i];
		int key = order[c.suit] * 16 + c.rank;
		int j = i - 1;
		while (j >= 0 && order[h[j].suit] * 16 + h[j].rank > key) {
			h[j + 1] = h[j];
			j--;
		}
		h[j + 1] = c;
	}
}

void he_init(Corazones* g, uint32_t seed)
{
	rng_seed(&g->rng, seed);
	for (int p = 0; p < HE_PLAYERS; p++)
		g->score[p] = 0;
	g->hand_number = -1;
	he_new_hand(g);
}

static void find_two_of_clubs(Corazones* g)
{
	for (int p = 0; p < HE_PLAYERS; p++)
		for (int i = 0; i < g->hand_count[p]; i++)
			if (g->hands[p][i].suit == CLUBS && g->hands[p][i].rank == 2)
				g->leader = g->current = p;
}

void he_new_hand(Corazones* g)
{
	PkCard deck[52];
	int n = 0;
	for (int s = 0; s < 4; s++)
		for (int r = 2; r <= 14; r++)
			deck[n++] = (PkCard){ (uint8_t)r, (uint8_t)s };
	for (int i = n - 1; i > 0; i--) {
		int j = rng_range(&g->rng, i + 1);
		PkCard t = deck[i];
		deck[i] = deck[j];
		deck[j] = t;
	}
	for (int p = 0; p < HE_PLAYERS; p++) {
		g->hand_count[p] = HE_HAND;
		for (int i = 0; i < HE_HAND; i++)
			g->hands[p][i] = deck[p * HE_HAND + i];
		sort_hand(g, p);
		g->points[p] = 0;
		g->trick_played[p] = false;
	}
	g->hand_number++;
	g->played_in_trick = 0;
	g->tricks = 0;
	g->hearts_broken = false;
	g->moon = -1;
	g->trick_winner = -1;
	g->phase = he_pass_direction(g) ? HE_PASSING : HE_PLAYING;
	find_two_of_clubs(g);
}

void he_ai_pass(const Corazones* g, int player, int* idx)
{
	/* Se pasan las más peligrosas: la reina, el as y el rey de picas, y las
	 * cartas más altas, sobre todo corazones */
	int score[HE_HAND];
	for (int i = 0; i < g->hand_count[player]; i++) {
		PkCard c = g->hands[player][i];
		score[i] = c.rank * 2;
		if (c.suit == SPADES && c.rank >= 12)
			score[i] += 60;
		if (c.suit == HEARTS)
			score[i] += 8;
	}
	bool used[HE_HAND] = { false };
	for (int k = 0; k < 3; k++) {
		int best = -1;
		for (int i = 0; i < g->hand_count[player]; i++)
			if (!used[i] && (best < 0 || score[i] > score[best]))
				best = i;
		used[best] = true;
		idx[k] = best;
	}
}

bool he_pass(Corazones* g, const int* idx)
{
	if (g->phase != HE_PASSING)
		return false;
	int dir = he_pass_direction(g);
	PkCard out[HE_PLAYERS][3];
	for (int p = 0; p < HE_PLAYERS; p++) {
		int sel[3];
		if (p == 0) {
			for (int k = 0; k < 3; k++) {
				if (idx[k] < 0 || idx[k] >= HE_HAND)
					return false;
				for (int j = 0; j < k; j++)
					if (idx[j] == idx[k])
						return false;
				sel[k] = idx[k];
			}
		} else {
			he_ai_pass(g, p, sel);
		}
		bool gone[HE_HAND] = { false };
		for (int k = 0; k < 3; k++) {
			out[p][k] = g->hands[p][sel[k]];
			gone[sel[k]] = true;
		}
		int m = 0;
		for (int i = 0; i < HE_HAND; i++)
			if (!gone[i])
				g->hands[p][m++] = g->hands[p][i];
		g->hand_count[p] = m;
	}
	for (int p = 0; p < HE_PLAYERS; p++) {
		int to = (p + (dir == 2 ? 2 : dir) + HE_PLAYERS) % HE_PLAYERS;
		for (int k = 0; k < 3; k++)
			g->hands[to][g->hand_count[to]++] = out[p][k];
	}
	for (int p = 0; p < HE_PLAYERS; p++)
		sort_hand(g, p);
	find_two_of_clubs(g);
	g->phase = HE_PLAYING;
	return true;
}

static bool has_suit(const Corazones* g, int p, int suit)
{
	for (int i = 0; i < g->hand_count[p]; i++)
		if (g->hands[p][i].suit == suit)
			return true;
	return false;
}

static bool only_points(const Corazones* g, int p)
{
	for (int i = 0; i < g->hand_count[p]; i++)
		if (he_card_points(g->hands[p][i]) == 0)
			return false;
	return true;
}

bool he_can_play(const Corazones* g, int player, int index)
{
	if (g->phase != HE_PLAYING || player != g->current || index < 0 || index >= g->hand_count[player])
		return false;
	PkCard c = g->hands[player][index];
	bool first_trick = g->tricks == 0;
	if (g->played_in_trick == 0) {
		if (first_trick)
			return c.suit == CLUBS && c.rank == 2;
		if (c.suit == HEARTS && !g->hearts_broken) {
			/* Solo si no le quedan más que corazones */
			for (int i = 0; i < g->hand_count[player]; i++)
				if (g->hands[player][i].suit != HEARTS)
					return false;
		}
		return true;
	}
	int lead = g->trick[g->leader].suit;
	if (has_suit(g, player, lead))
		return c.suit == lead;
	if (first_trick && he_card_points(c) > 0 && !only_points(g, player))
		return false;
	return true;
}

static int trick_leader_now(const Corazones* g)
{
	int best = g->leader;
	for (int k = 1; k < g->played_in_trick; k++) {
		int p = (g->leader + k) % HE_PLAYERS;
		if (g->trick[p].suit == g->trick[g->leader].suit && g->trick[p].rank > g->trick[best].rank)
			best = p;
	}
	return best;
}

bool he_play(Corazones* g, int index)
{
	int p = g->current;
	if (!he_can_play(g, p, index))
		return false;
	PkCard c = g->hands[p][index];
	for (int i = index; i < g->hand_count[p] - 1; i++)
		g->hands[p][i] = g->hands[p][i + 1];
	g->hand_count[p]--;
	g->trick[p] = c;
	g->trick_played[p] = true;
	g->played_in_trick++;
	if (c.suit == HEARTS)
		g->hearts_broken = true;
	if (g->played_in_trick < HE_PLAYERS) {
		g->current = (p + 1) % HE_PLAYERS;
		return true;
	}
	g->trick_winner = trick_leader_now(g);
	g->phase = HE_TRICK_DONE;
	return true;
}

int he_winner(const Corazones* g)
{
	int best = 0;
	for (int p = 1; p < HE_PLAYERS; p++)
		if (g->score[p] < g->score[best])
			best = p;
	return best;
}

bool he_collect(Corazones* g)
{
	if (g->phase != HE_TRICK_DONE)
		return false;
	int w = g->trick_winner;
	for (int p = 0; p < HE_PLAYERS; p++) {
		g->points[w] += he_card_points(g->trick[p]);
		g->trick_played[p] = false;
	}
	g->played_in_trick = 0;
	g->tricks++;
	g->leader = g->current = w;
	if (g->hand_count[w] > 0) {
		g->phase = HE_PLAYING;
		return true;
	}
	/* Fin de la mano */
	for (int p = 0; p < HE_PLAYERS; p++)
		if (g->points[p] == 26)
			g->moon = p;
	for (int p = 0; p < HE_PLAYERS; p++) {
		if (g->moon >= 0)
			g->points[p] = p == g->moon ? 0 : 26;
		g->score[p] += g->points[p];
	}
	bool end = false;
	for (int p = 0; p < HE_PLAYERS; p++)
		end |= g->score[p] >= HE_END;
	g->phase = end ? HE_GAME_OVER : HE_HAND_OVER;
	return true;
}

int he_ai_choose(const Corazones* g, int player)
{
	int n = g->hand_count[player];
	int best = -1, best_score = -100000;
	int winner = g->played_in_trick ? trick_leader_now(g) : -1;
	int trick_points = 0;
	for (int k = 0; k < g->played_in_trick; k++)
		trick_points += he_card_points(g->trick[(g->leader + k) % HE_PLAYERS]);
	bool last = g->played_in_trick == HE_PLAYERS - 1;
	for (int i = 0; i < n; i++) {
		if (!he_can_play(g, player, i))
			continue;
		PkCard c = g->hands[player][i];
		int score;
		if (winner < 0) {
			/* Salir: con cartas bajas, nunca con la reina de picas */
			score = -c.rank * 3 - (c.suit == HEARTS ? 10 : 0) - (c.suit == SPADES && c.rank >= 12 ? 80 : 0);
		} else if (c.suit == g->trick[g->leader].suit) {
			bool wins = c.rank > g->trick[winner].rank;
			if (!wins)
				score = 50 + c.rank; /* la más alta que no gana */
			else if (last && trick_points == 0 && !(c.suit == SPADES && c.rank == 12))
				score = 40 + c.rank; /* gana una baza limpia: mejor con una alta */
			else
				score = -c.rank * 4 - he_card_points(c) * 10 - trick_points * 6;
		} else {
			/* Sin el palo: deshacerse de lo peligroso */
			score = he_card_points(c) * 10 + c.rank + (c.suit == SPADES && c.rank > 12 ? 30 : 0);
		}
		if (score > best_score) {
			best_score = score;
			best = i;
		}
	}
	return best;
}
