#include "games/butifarra.h"

#include <string.h>

int bu_team(int player)
{
	return player % 2;
}

int bu_card_points(Card c)
{
	switch (c.rank) {
	case 9:
		return 5;
	case 1:
		return 4;
	case RANK_REY:
		return 3;
	case RANK_CABALLO:
		return 2;
	case RANK_SOTA:
		return 1;
	default:
		return 0;
	}
}

int bu_strength(Card c)
{
	switch (c.rank) {
	case 9:
		return 12;
	case 1:
		return 11;
	case RANK_REY:
		return 10;
	case RANK_CABALLO:
		return 9;
	case RANK_SOTA:
		return 8;
	case 8:
		return 7;
	default:
		return c.rank - 1; /* 7 -> 6 ... 2 -> 1 */
	}
}

bool bu_beats(const Butifarra* g, Card challenger, Card best)
{
	if (challenger.suit == best.suit)
		return bu_strength(challenger) > bu_strength(best);
	return g->trump >= 0 && challenger.suit == g->trump;
}

int bu_multiplier(const Butifarra* g)
{
	return (g->butifarra ? 2 : 1) << g->doubles;
}

void bu_init(Butifarra* g, uint32_t seed)
{
	memset(g, 0, sizeof *g);
	rng_seed(&g->rng, seed);
	g->dealer = rng_range(&g->rng, BU_PLAYERS);
	bu_new_game(g);
}

void bu_new_game(Butifarra* g)
{
	g->score[0] = g->score[1] = 0;
	bu_new_hand(g);
}

void bu_new_hand(Butifarra* g)
{
	Card deck[BU_DECK];
	int n = 0;
	for (int s = 0; s < NUM_SUITS; s++)
		for (int r = 1; r <= 12; r++)
			deck[n++] = card_make((Suit)s, r);
	for (int i = n - 1; i > 0; i--) {
		int j = rng_range(&g->rng, i + 1);
		Card t = deck[i];
		deck[i] = deck[j];
		deck[j] = t;
	}
	g->dealer = (g->dealer + 1) % BU_PLAYERS;
	for (int p = 0; p < BU_PLAYERS; p++) {
		g->hand_count[p] = 0;
		g->trick_played[p] = false;
	}
	for (int i = 0; i < BU_DECK; i++) {
		int p = (g->dealer + 1 + i) % BU_PLAYERS;
		g->hands[p][g->hand_count[p]++] = deck[i];
	}
	g->chooser = g->current = g->dealer;
	g->delegated = false;
	g->trump = -1;
	g->butifarra = false;
	g->doubles = 0;
	g->played_in_trick = 0;
	g->tricks_done = 0;
	g->points[0] = g->points[1] = 0;
	g->hand_gain[0] = g->hand_gain[1] = 0;
	g->last_double_player = -1;
	g->phase = BU_CHOOSE;
}

bool bu_can_delegate(const Butifarra* g)
{
	return g->phase == BU_CHOOSE && !g->delegated;
}

/* Quién decide las contradas de cada pareja: tú si es la tuya */
static int decider(const Butifarra* g, int team)
{
	if (team == bu_team(0))
		return 0;
	return team == bu_team(g->chooser) ? g->chooser : (g->chooser + 1) % BU_PLAYERS;
}

static void start_play(Butifarra* g)
{
	g->phase = BU_PLAYING;
	g->leader = g->current = (g->dealer + 1) % BU_PLAYERS;
}

bool bu_choose(Butifarra* g, int choice)
{
	if (g->phase != BU_CHOOSE)
		return false;
	if (choice == BU_DELEGATE) {
		if (g->delegated)
			return false;
		g->delegated = true;
		g->chooser = g->current = (g->dealer + 2) % BU_PLAYERS;
		return true;
	}
	if (choice < 0 || choice > BU_BUTIFARRA)
		return false;
	g->butifarra = choice == BU_BUTIFARRA;
	g->trump = g->butifarra ? -1 : choice;
	g->phase = BU_DOUBLE;
	g->current = decider(g, 1 - bu_team(g->chooser));
	return true;
}

bool bu_double(Butifarra* g, bool yes)
{
	if (g->phase != BU_DOUBLE)
		return false;
	g->last_double_player = g->current;
	g->last_double = yes;
	if (!yes) {
		start_play(g);
		return true;
	}
	g->doubles++;
	if (g->doubles >= 3) {
		start_play(g);
		return true;
	}
	/* Contesta la otra pareja */
	int team = bu_team(g->current);
	g->current = decider(g, 1 - team);
	return true;
}

int bu_trick_leader_now(const Butifarra* g)
{
	if (g->played_in_trick == 0)
		return -1;
	int best = g->leader;
	for (int k = 1; k < g->played_in_trick; k++) {
		int p = (g->leader + k) % BU_PLAYERS;
		if (bu_beats(g, g->trick[p], g->trick[best]))
			best = p;
	}
	return best;
}

bool bu_can_play(const Butifarra* g, int player, int idx)
{
	if (g->phase != BU_PLAYING || player != g->current || idx < 0 || idx >= g->hand_count[player])
		return false;
	if (g->played_in_trick == 0)
		return true;
	const Card* h = g->hands[player];
	int n = g->hand_count[player];
	Card c = h[idx];
	int lead = g->trick[g->leader].suit;
	int bp = bu_trick_leader_now(g);
	Card best = g->trick[bp];
	bool partner = bu_team(bp) == bu_team(player);
	bool has_suit = false, beat_suit = false, has_trump = false, beat_trump = false;
	for (int i = 0; i < n; i++) {
		if (h[i].suit == lead) {
			has_suit = true;
			beat_suit |= bu_beats(g, h[i], best);
		} else if (g->trump >= 0 && h[i].suit == g->trump) {
			has_trump = true;
			beat_trump |= bu_beats(g, h[i], best);
		}
	}
	if (has_suit) {
		if (c.suit != lead)
			return false;
		return partner || !beat_suit || bu_beats(g, c, best);
	}
	if (partner || !has_trump)
		return true;
	if (beat_trump)
		return bu_beats(g, c, best);
	return true;
}

bool bu_play(Butifarra* g, int idx)
{
	int p = g->current;
	if (!bu_can_play(g, p, idx))
		return false;
	g->trick[p] = g->hands[p][idx];
	g->trick_played[p] = true;
	for (int i = idx; i < g->hand_count[p] - 1; i++)
		g->hands[p][i] = g->hands[p][i + 1];
	g->hand_count[p]--;
	g->played_in_trick++;
	if (g->played_in_trick == BU_PLAYERS) {
		g->trick_winner = bu_trick_leader_now(g);
		g->phase = BU_TRICK_DONE;
	} else {
		g->current = (p + 1) % BU_PLAYERS;
	}
	return true;
}

bool bu_collect(Butifarra* g)
{
	if (g->phase != BU_TRICK_DONE)
		return false;
	int w = g->trick_winner;
	int pts = 1;
	for (int p = 0; p < BU_PLAYERS; p++) {
		pts += bu_card_points(g->trick[p]);
		g->trick_played[p] = false;
	}
	g->points[bu_team(w)] += pts;
	g->played_in_trick = 0;
	g->tricks_done++;
	if (g->tricks_done < BU_HAND) {
		g->leader = g->current = w;
		g->phase = BU_PLAYING;
		return true;
	}
	for (int t = 0; t < 2; t++)
		if (g->points[t] > 36) {
			g->hand_gain[t] = (g->points[t] - 36) * bu_multiplier(g);
			g->score[t] += g->hand_gain[t];
		}
	g->phase = bu_winner(g) >= 0 ? BU_GAME_OVER : BU_HAND_OVER;
	return true;
}

int bu_winner(const Butifarra* g)
{
	bool a = g->score[0] >= BU_GAME, b = g->score[1] >= BU_GAME;
	if (!a && !b)
		return -1;
	if (a && b)
		return g->score[0] >= g->score[1] ? 0 : 1;
	return a ? 0 : 1;
}

/* ---------------------------------------------------------------------------
 * IA
 * ------------------------------------------------------------------------- */

static int suit_value(const Butifarra* g, int player, int suit)
{
	int v = 0;
	for (int i = 0; i < g->hand_count[player]; i++) {
		Card c = g->hands[player][i];
		if (c.suit != suit)
			continue;
		v += 3 + (c.rank == 9 ? 6 : c.rank == 1 ? 4 : c.rank == RANK_REY ? 2 : 0);
	}
	return v;
}

static int high_cards(const Butifarra* g, int player)
{
	int n = 0;
	for (int i = 0; i < g->hand_count[player]; i++)
		n += g->hands[player][i].rank == 9 ? 2 : g->hands[player][i].rank == 1 ? 1 : 0;
	return n;
}

int bu_ai_choose(const Butifarra* g, int player)
{
	int best = 0, best_v = -1;
	for (int s = 0; s < NUM_SUITS; s++) {
		int v = suit_value(g, player, s);
		if (v > best_v) {
			best_v = v;
			best = s;
		}
	}
	int highs = high_cards(g, player);
	if (highs >= 9)
		return BU_BUTIFARRA;
	if (best_v < 20 && !g->delegated)
		return BU_DELEGATE;
	return best;
}

bool bu_ai_double(const Butifarra* g, int player)
{
	bool declarer = bu_team(player) == bu_team(g->chooser);
	int strength = g->butifarra ? high_cards(g, player) * 4 : suit_value(g, player, g->trump) + high_cards(g, player) * 2;
	if (declarer)
		return strength >= 34 + g->doubles * 4;
	return strength >= 30 + g->doubles * 6;
}

int bu_ai_play(const Butifarra* g, int player)
{
	int n = g->hand_count[player];
	int lead_now = bu_trick_leader_now(g);
	int trick_pts = 0;
	for (int k = 0; k < g->played_in_trick; k++)
		trick_pts += bu_card_points(g->trick[(g->leader + k) % BU_PLAYERS]);
	bool last = g->played_in_trick == BU_PLAYERS - 1;
	int best = -1, best_score = -1000000;
	for (int i = 0; i < n; i++) {
		if (!bu_can_play(g, player, i))
			continue;
		Card c = g->hands[player][i];
		int keep = bu_card_points(c) * 10 + bu_strength(c) + (g->trump >= 0 && c.suit == g->trump ? 40 : 0);
		int score;
		if (lead_now < 0) {
			/* Sale con las manillas y ases, o con lo que menos duela */
			score = c.rank == 9 ? 60 : c.rank == 1 && g->trump != c.suit ? 20 : -keep;
		} else if (bu_team(lead_now) == bu_team(player)) {
			Card b = g->trick[lead_now];
			bool safe = last || bu_strength(b) >= 11;
			score = safe ? bu_card_points(c) * 12 - (g->trump >= 0 && c.suit == g->trump ? 60 : 0) : -keep;
		} else if (bu_beats(g, c, g->trick[lead_now])) {
			score = (trick_pts + bu_card_points(c)) * 12 + 20 - keep / 2;
			if (!last && bu_strength(c) < 11 && c.suit != g->trump)
				score -= 15;
		} else {
			score = -keep - trick_pts * 2;
		}
		if (score > best_score) {
			best_score = score;
			best = i;
		}
	}
	return best;
}
