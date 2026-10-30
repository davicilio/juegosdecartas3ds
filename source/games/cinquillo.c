#include "games/cinquillo.h"

#include <stddef.h>

#define POS_FIVE 4 /* card_pos de un cinco */

static int pay(Cinquillo* g, int player, int amount)
{
	/* Quien no tiene fichas suficientes paga lo que le queda. */
	if (amount > g->chips[player])
		amount = g->chips[player];
	g->chips[player] -= amount;
	g->paid[player] += amount;
	g->pot += amount;
	return amount;
}

void cq_sort_hand(Cinquillo* g, int player)
{
	Card* h = g->hands[player];
	int n = g->hand_count[player];
	for (int i = 1; i < n; i++) {
		Card c = h[i];
		int j = i - 1;
		while (j >= 0 && card_index(h[j]) > card_index(c)) {
			h[j + 1] = h[j];
			j--;
		}
		h[j + 1] = c;
	}
}

void cq_init(Cinquillo* g, uint32_t seed)
{
	rng_seed(&g->rng, seed);
	for (int p = 0; p < CQ_PLAYERS; p++)
		g->chips[p] = CQ_START_CHIPS;
	g->pot = 0;
	g->round = 0;
	cq_start_round(g);
}

void cq_start_round(Cinquillo* g)
{
	Deck deck;
	deck_init(&deck);
	deck_shuffle(&deck, &g->rng);

	for (int p = 0; p < CQ_PLAYERS; p++) {
		g->hand_count[p] = 0;
		g->paid[p] = 0;
		for (int i = 0; i < CQ_HAND_SIZE; i++)
			deck_draw(&deck, &g->hands[p][g->hand_count[p]++]);
		cq_sort_hand(g, p);
		pay(g, p, CQ_ANTE);
	}
	for (int s = 0; s < NUM_SUITS; s++)
		g->low[s] = g->high[s] = -1;

	/* Sale quien tenga el cinco de oros. */
	Card five_oros = card_make(SUIT_OROS, 5);
	g->current = 0;
	for (int p = 0; p < CQ_PLAYERS; p++)
		for (int i = 0; i < g->hand_count[p]; i++)
			if (card_equal(g->hands[p][i], five_oros))
				g->current = p;

	g->round++;
	g->turns = 0;
	g->round_winner = -1;
	g->last_action = CQ_ACTION_NONE;
	g->last_player = -1;
	g->phase = CQ_PHASE_PLAYING;
}

bool cq_is_legal(const Cinquillo* g, Card c)
{
	int pos = card_pos(c);
	/* La primera carta de la ronda ha de ser el cinco de oros. */
	if (g->low[SUIT_OROS] < 0)
		return c.suit == SUIT_OROS && pos == POS_FIVE;
	if (g->low[c.suit] < 0)
		return pos == POS_FIVE;
	return pos == g->low[c.suit] - 1 || pos == g->high[c.suit] + 1;
}

int cq_legal_moves(const Cinquillo* g, int player, int* out)
{
	int n = 0;
	for (int i = 0; i < g->hand_count[player]; i++)
		if (cq_is_legal(g, g->hands[player][i])) {
			if (out)
				out[n] = i;
			n++;
		}
	return n;
}

bool cq_can_play(const Cinquillo* g, int player)
{
	return cq_legal_moves(g, player, NULL) > 0;
}

static void advance_turn(Cinquillo* g)
{
	g->turns++;
	g->current = (g->current + 1) % CQ_PLAYERS;
}

static void end_round(Cinquillo* g, int winner)
{
	g->round_winner = winner;
	g->chips[winner] += g->pot;
	g->pot = 0;

	bool someone_broke = false;
	for (int p = 0; p < CQ_PLAYERS; p++)
		if (g->chips[p] <= 0)
			someone_broke = true;
	g->phase = (someone_broke || g->round >= CQ_MAX_ROUNDS) ? CQ_PHASE_GAME_OVER : CQ_PHASE_ROUND_OVER;
}

bool cq_play(Cinquillo* g, int hand_index)
{
	int p = g->current;
	if (g->phase != CQ_PHASE_PLAYING || hand_index < 0 || hand_index >= g->hand_count[p])
		return false;
	Card c = g->hands[p][hand_index];
	if (!cq_is_legal(g, c))
		return false;

	int pos = card_pos(c);
	if (g->low[c.suit] < 0) {
		g->low[c.suit] = g->high[c.suit] = pos;
	} else if (pos < g->low[c.suit]) {
		g->low[c.suit] = pos;
	} else {
		g->high[c.suit] = pos;
	}

	for (int i = hand_index; i < g->hand_count[p] - 1; i++)
		g->hands[p][i] = g->hands[p][i + 1];
	g->hand_count[p]--;

	g->last_action = CQ_ACTION_PLAY;
	g->last_player = p;
	g->last_card = c;

	if (g->hand_count[p] == 0) {
		g->turns++;
		end_round(g, p);
	} else {
		advance_turn(g);
	}
	return true;
}

bool cq_pass(Cinquillo* g)
{
	if (g->phase != CQ_PHASE_PLAYING || cq_can_play(g, g->current))
		return false;
	pay(g, g->current, CQ_PASS_COST);
	g->last_action = CQ_ACTION_PASS;
	g->last_player = g->current;
	advance_turn(g);
	return true;
}

static bool has_card(const Cinquillo* g, int player, int suit, int pos)
{
	for (int i = 0; i < g->hand_count[player]; i++) {
		Card c = g->hands[player][i];
		if (c.suit == suit && card_pos(c) == pos)
			return true;
	}
	return false;
}

/* Puntuación heurística de jugar la carta c: se prefiere jugar cartas que
 * desbloquean otras propias y evitar abrir camino a los rivales. */
static int score_move(const Cinquillo* g, int player, Card c)
{
	int suit = c.suit;
	int pos = card_pos(c);
	int score = 0;

	if (g->low[suit] < 0 && pos == POS_FIVE) {
		/* Abrir un palo: interesa si tenemos cartas de ese palo. */
		int mine = 0;
		for (int i = 0; i < g->hand_count[player]; i++)
			if (g->hands[player][i].suit == suit)
				mine++;
		score += mine * 3 - 4;
		if (has_card(g, player, suit, POS_FIVE - 1))
			score += 4;
		if (has_card(g, player, suit, POS_FIVE + 1))
			score += 4;
		return score;
	}

	int dir = pos < POS_FIVE ? -1 : 1;
	int next = pos + dir;
	bool end_of_suit = next < 0 || next >= RANKS_PER_SUIT;

	if (end_of_suit) {
		score += 2; /* no abre camino a nadie */
	} else if (has_card(g, player, suit, next)) {
		score += 8; /* la siguiente carta es nuestra: la jugaremos después */
	} else {
		score -= 3; /* deja la siguiente carta libre para los rivales */
	}
	/* Cuantas más cartas tengamos en esa dirección, más prisa por avanzar. */
	for (int q = next; q >= 0 && q < RANKS_PER_SUIT; q += dir)
		if (has_card(g, player, suit, q))
			score += 2;
	return score;
}

int cq_ai_choose(const Cinquillo* g, int player)
{
	int moves[CQ_HAND_SIZE];
	int n = cq_legal_moves(g, player, moves);
	if (n == 0)
		return -1;
	int best = moves[0];
	int best_score = -1000;
	for (int i = 0; i < n; i++) {
		int s = score_move(g, player, g->hands[player][moves[i]]);
		if (s > best_score) {
			best_score = s;
			best = moves[i];
		}
	}
	return best;
}

bool cq_next_round(Cinquillo* g)
{
	if (g->phase != CQ_PHASE_ROUND_OVER)
		return false;
	cq_start_round(g);
	return true;
}

int cq_leader(const Cinquillo* g)
{
	int best = 0;
	for (int p = 1; p < CQ_PLAYERS; p++)
		if (g->chips[p] > g->chips[best])
			best = p;
	return best;
}
