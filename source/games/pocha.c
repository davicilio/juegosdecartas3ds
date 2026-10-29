#include "games/pocha.h"

int po_strength(Card c)
{
	/* índice = rango 1..12: as 10, tres 9, rey 8, caballo 7, sota 6, 7 5 ... 2 1 */
	static const int s[13] = { 0, 10, 1, 9, 2, 3, 4, 5, 0, 0, 6, 7, 8 };
	return c.rank <= 12 ? s[c.rank] : 0;
}

bool po_beats(const Pocha* g, Card challenger, Card best)
{
	if (challenger.suit == best.suit)
		return po_strength(challenger) > po_strength(best);
	return challenger.suit == g->trump;
}

void po_init(Pocha* g, uint32_t seed)
{
	rng_seed(&g->rng, seed);
	for (int p = 0; p < PO_PLAYERS; p++)
		g->score[p] = 0;
	g->round = 0;
	g->dealer = PO_PLAYERS - 1;
	po_new_round(g);
}

static void sort_hand(Pocha* g, int p)
{
	Card* h = g->hands[p];
	for (int i = 1; i < g->hand_count[p]; i++) {
		Card c = h[i];
		int key = c.suit * 16 + po_strength(c);
		int j = i - 1;
		while (j >= 0 && h[j].suit * 16 + po_strength(h[j]) > key) {
			h[j + 1] = h[j];
			j--;
		}
		h[j + 1] = c;
	}
}

void po_new_round(Pocha* g)
{
	g->round++;
	g->dealer = (g->dealer + 1) % PO_PLAYERS;
	Deck d;
	deck_init(&d);
	deck_shuffle(&d, &g->rng);
	Card last = { 0, 1 };
	for (int p = 0; p < PO_PLAYERS; p++) {
		g->hand_count[p] = 0;
		g->bid[p] = -1;
		g->tricks_won[p] = 0;
		g->round_points[p] = 0;
	}
	for (int i = 0; i < g->round; i++)
		for (int k = 1; k <= PO_PLAYERS; k++) {
			int p = (g->dealer + k) % PO_PLAYERS;
			deck_draw(&d, &g->hands[p][g->hand_count[p]++]);
			last = g->hands[p][g->hand_count[p] - 1];
		}
	if (d.count > 0)
		deck_draw(&d, &g->trump_card);
	else
		g->trump_card = last;
	g->trump = g->trump_card.suit;
	for (int p = 0; p < PO_PLAYERS; p++)
		sort_hand(g, p);
	g->bids_made = 0;
	g->current = g->leader = (g->dealer + 1) % PO_PLAYERS;
	g->played_in_trick = 0;
	g->trick_winner = -1;
	g->phase = PO_BIDDING;
}

int po_forbidden_bid(const Pocha* g)
{
	if (g->current != g->dealer || g->bids_made != PO_PLAYERS - 1)
		return -1;
	int sum = 0;
	for (int p = 0; p < PO_PLAYERS; p++)
		if (g->bid[p] >= 0)
			sum += g->bid[p];
	int f = g->round - sum;
	return f >= 0 && f <= g->round ? f : -1;
}

bool po_can_bid(const Pocha* g, int bid)
{
	return g->phase == PO_BIDDING && bid >= 0 && bid <= g->round && bid != po_forbidden_bid(g);
}

bool po_bid(Pocha* g, int bid)
{
	if (!po_can_bid(g, bid))
		return false;
	g->bid[g->current] = bid;
	g->bids_made++;
	if (g->bids_made == PO_PLAYERS) {
		g->phase = PO_PLAYING;
		g->current = g->leader = (g->dealer + 1) % PO_PLAYERS;
	} else {
		g->current = (g->current + 1) % PO_PLAYERS;
	}
	return true;
}

static int winning_now(const Pocha* g)
{
	int best = g->leader;
	for (int k = 1; k < g->played_in_trick; k++) {
		int p = (g->leader + k) % PO_PLAYERS;
		if (po_beats(g, g->trick[p], g->trick[best]))
			best = p;
	}
	return best;
}

bool po_can_play(const Pocha* g, int player, int index)
{
	if (g->phase != PO_PLAYING || player != g->current || index < 0 || index >= g->hand_count[player])
		return false;
	if (g->played_in_trick == 0)
		return true;
	const Card* h = g->hands[player];
	int n = g->hand_count[player];
	Card c = h[index];
	Card lead = g->trick[g->leader];
	Card best = g->trick[winning_now(g)];
	bool has_suit = false, beat_suit = false, has_trump = false, beat_trump = false;
	for (int i = 0; i < n; i++) {
		if (h[i].suit == lead.suit) {
			has_suit = true;
			beat_suit |= po_beats(g, h[i], best);
		}
		if (h[i].suit == g->trump) {
			has_trump = true;
			beat_trump |= po_beats(g, h[i], best);
		}
	}
	if (has_suit) {
		if (c.suit != lead.suit)
			return false;
		return !beat_suit || po_beats(g, c, best);
	}
	if (!has_trump)
		return true;
	if (c.suit != g->trump)
		return false;
	return !beat_trump || po_beats(g, c, best);
}

bool po_play(Pocha* g, int index)
{
	int p = g->current;
	if (!po_can_play(g, p, index))
		return false;
	g->trick[p] = g->hands[p][index];
	for (int i = index; i < g->hand_count[p] - 1; i++)
		g->hands[p][i] = g->hands[p][i + 1];
	g->hand_count[p]--;
	g->played_in_trick++;
	if (g->played_in_trick < PO_PLAYERS) {
		g->current = (p + 1) % PO_PLAYERS;
		return true;
	}
	g->trick_winner = winning_now(g);
	g->phase = PO_TRICK_DONE;
	return true;
}

bool po_collect(Pocha* g)
{
	if (g->phase != PO_TRICK_DONE)
		return false;
	int w = g->trick_winner;
	g->tricks_won[w]++;
	g->played_in_trick = 0;
	g->leader = g->current = w;
	if (g->hand_count[w] > 0) {
		g->phase = PO_PLAYING;
		return true;
	}
	for (int p = 0; p < PO_PLAYERS; p++) {
		int diff = g->tricks_won[p] - g->bid[p];
		g->round_points[p] = diff == 0 ? 10 + 5 * g->bid[p] : -5 * (diff < 0 ? -diff : diff);
		g->score[p] += g->round_points[p];
	}
	g->phase = g->round >= PO_ROUNDS ? PO_GAME_OVER : PO_ROUND_OVER;
	return true;
}

int po_winner(const Pocha* g)
{
	int best = 0;
	for (int p = 1; p < PO_PLAYERS; p++)
		if (g->score[p] > g->score[best])
			best = p;
	return best;
}

/* Bazas que espera hacer con su mano (en décimas) */
static int expected_tricks10(const Pocha* g, int p)
{
	int e = 0;
	for (int i = 0; i < g->hand_count[p]; i++) {
		Card c = g->hands[p][i];
		int s = po_strength(c);
		if (c.suit == g->trump)
			e += s >= 9 ? 9 : s >= 6 ? 6 : 3;
		else
			e += s == 10 ? 7 : s == 9 ? 4 : s == 8 && g->round >= 6 ? 2 : 0;
	}
	return e;
}

int po_ai_bid(const Pocha* g)
{
	int p = g->current;
	int b = (expected_tricks10(g, p) + 5) / 10;
	if (b > g->round)
		b = g->round;
	if (!po_can_bid(g, b))
		b = b > 0 && po_can_bid(g, b - 1) ? b - 1 : b + 1;
	return b;
}

int po_ai_choose(const Pocha* g, int player)
{
	bool want = g->tricks_won[player] < g->bid[player];
	int best = -1, best_score = -100000;
	int winner = g->played_in_trick ? winning_now(g) : -1;
	for (int i = 0; i < g->hand_count[player]; i++) {
		if (!po_can_play(g, player, i))
			continue;
		Card c = g->hands[player][i];
		int strength = po_strength(c) + (c.suit == g->trump ? 12 : 0);
		int score;
		if (winner < 0) {
			/* Salir: fuerte si quiere bazas, flojo si no */
			score = want ? strength : -strength;
		} else {
			bool wins = po_beats(g, c, g->trick[winner]);
			if (want)
				score = wins ? 100 - strength : -strength; /* ganar con lo justo */
			else
				score = wins ? -100 - strength : strength; /* perder con lo más alto posible */
		}
		if (score > best_score) {
			best_score = score;
			best = i;
		}
	}
	return best;
}
