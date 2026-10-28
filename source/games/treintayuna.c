#include "games/treintayuna.h"

int tyu_card_value(Card c)
{
	if (c.rank == 1)
		return 11;
	if (c.rank >= RANK_SOTA)
		return 10;
	return c.rank;
}

int tyu_hand_value(const Card* cards)
{
	int best = 0;
	for (int s = 0; s < NUM_SUITS; s++) {
		int sum = 0;
		for (int i = 0; i < TYU_CARDS; i++)
			if (cards[i].suit == s)
				sum += tyu_card_value(cards[i]);
		if (sum > best)
			best = sum;
	}
	return best;
}

bool tyu_alive(const TreintaYUna* g, int player)
{
	return g->lives[player] > 0;
}

static int next_alive(const TreintaYUna* g, int from)
{
	for (int k = 1; k <= TYU_PLAYERS; k++) {
		int q = (from + k) % TYU_PLAYERS;
		if (tyu_alive(g, q))
			return q;
	}
	return from;
}

static int alive_count(const TreintaYUna* g)
{
	int n = 0;
	for (int p = 0; p < TYU_PLAYERS; p++)
		n += tyu_alive(g, p);
	return n;
}

void tyu_init(TreintaYUna* g, uint32_t seed)
{
	rng_seed(&g->rng, seed);
	g->dealer = TYU_PLAYERS - 1;
	tyu_new_game(g);
}

void tyu_new_game(TreintaYUna* g)
{
	for (int p = 0; p < TYU_PLAYERS; p++)
		g->lives[p] = TYU_LIVES;
	tyu_new_round(g);
}

static void reveal(TreintaYUna* g)
{
	for (int p = 0; p < TYU_PLAYERS; p++)
		g->lost[p] = false;
	if (g->thirty_one >= 0) {
		for (int p = 0; p < TYU_PLAYERS; p++)
			if (tyu_alive(g, p) && p != g->thirty_one)
				g->lost[p] = true;
	} else {
		int low = 99;
		for (int p = 0; p < TYU_PLAYERS; p++)
			if (tyu_alive(g, p) && tyu_hand_value(g->hands[p]) < low)
				low = tyu_hand_value(g->hands[p]);
		for (int p = 0; p < TYU_PLAYERS; p++)
			if (tyu_alive(g, p) && tyu_hand_value(g->hands[p]) == low)
				g->lost[p] = true;
	}
	/* Si perdieran todos los que quedan, nadie pierde */
	int losing = 0;
	for (int p = 0; p < TYU_PLAYERS; p++)
		losing += g->lost[p];
	if (losing == alive_count(g))
		for (int p = 0; p < TYU_PLAYERS; p++)
			g->lost[p] = false;
	for (int p = 0; p < TYU_PLAYERS; p++)
		if (g->lost[p])
			g->lives[p]--;
	g->phase = tyu_winner(g) >= 0 ? TYU_PHASE_GAME_OVER : TYU_PHASE_REVEAL;
}

void tyu_new_round(TreintaYUna* g)
{
	deck_init(&g->deck);
	deck_shuffle(&g->deck, &g->rng);
	g->dealer = next_alive(g, g->dealer);
	for (int p = 0; p < TYU_PLAYERS; p++) {
		g->lost[p] = false;
		if (tyu_alive(g, p))
			for (int i = 0; i < TYU_CARDS; i++)
				deck_draw(&g->deck, &g->hands[p][i]);
	}
	for (int i = 0; i < TYU_CARDS; i++)
		deck_draw(&g->deck, &g->table[i]);
	g->current = next_alive(g, g->dealer);
	g->knocker = -1;
	g->turns_left = -1;
	g->turns_played = 0;
	g->thirty_one = -1;
	g->phase = TYU_PHASE_PLAYING;
	/* Alguien puede empezar ya con 31 */
	for (int k = 0; k < TYU_PLAYERS; k++) {
		int p = (g->current + k) % TYU_PLAYERS;
		if (tyu_alive(g, p) && tyu_hand_value(g->hands[p]) == 31) {
			g->thirty_one = p;
			reveal(g);
			return;
		}
	}
}

static void end_turn(TreintaYUna* g)
{
	int p = g->current;
	g->turns_played++;
	if (tyu_hand_value(g->hands[p]) == 31) {
		g->thirty_one = p;
		reveal(g);
		return;
	}
	if (g->turns_left > 0)
		g->turns_left--;
	g->current = next_alive(g, p);
	/* Una ronda no puede durar para siempre: tras ocho vueltas se enseñan */
	if (g->turns_played >= alive_count(g) * 8) {
		reveal(g);
		return;
	}
	if (g->turns_left == 0 || g->current == g->knocker)
		reveal(g);
}

bool tyu_swap(TreintaYUna* g, int hand_index, int table_index)
{
	if (g->phase != TYU_PHASE_PLAYING || hand_index < 0 || hand_index >= TYU_CARDS || table_index < 0 ||
		table_index >= TYU_CARDS)
		return false;
	Card* h = &g->hands[g->current][hand_index];
	Card t = *h;
	*h = g->table[table_index];
	g->table[table_index] = t;
	end_turn(g);
	return true;
}

bool tyu_swap_all(TreintaYUna* g)
{
	if (g->phase != TYU_PHASE_PLAYING)
		return false;
	for (int i = 0; i < TYU_CARDS; i++) {
		Card t = g->hands[g->current][i];
		g->hands[g->current][i] = g->table[i];
		g->table[i] = t;
	}
	end_turn(g);
	return true;
}

bool tyu_can_knock(const TreintaYUna* g)
{
	return g->phase == TYU_PHASE_PLAYING && g->knocker < 0 && g->turns_played >= alive_count(g);
}

bool tyu_knock(TreintaYUna* g)
{
	if (!tyu_can_knock(g))
		return false;
	g->knocker = g->current;
	g->turns_left = alive_count(g) - 1;
	g->current = next_alive(g, g->current);
	if (g->turns_left == 0)
		reveal(g);
	return true;
}

void tyu_ai_turn(TreintaYUna* g)
{
	int p = g->current;
	int now = tyu_hand_value(g->hands[p]);
	/* El mejor cambio de una carta (aunque no mejore) */
	int best_h = 0, best_t = 0, best = -1;
	for (int h = 0; h < TYU_CARDS; h++)
		for (int t = 0; t < TYU_CARDS; t++) {
			Card tmp[TYU_CARDS] = { g->hands[p][0], g->hands[p][1], g->hands[p][2] };
			tmp[h] = g->table[t];
			int v = tyu_hand_value(tmp);
			if (v > best) {
				best = v;
				best_h = h;
				best_t = t;
			}
		}
	int all = tyu_hand_value(g->table);
	if (all > best + 3 && all > now) {
		tyu_swap_all(g);
		return;
	}
	/* Plantarse con una buena mano si cambiar apenas mejora */
	if (tyu_can_knock(g) && now >= 25 && best - now < 4) {
		tyu_knock(g);
		return;
	}
	/* Si no mejora (o la ronda se alarga), plantarse */
	int laps = g->turns_played / alive_count(g);
	if (tyu_can_knock(g) && ((best <= now && now >= 21 - laps * 2) || laps >= 4)) {
		tyu_knock(g);
		return;
	}
	tyu_swap(g, best_h, best_t);
}

int tyu_winner(const TreintaYUna* g)
{
	if (alive_count(g) != 1)
		return -1;
	for (int p = 0; p < TYU_PLAYERS; p++)
		if (tyu_alive(g, p))
			return p;
	return -1;
}
