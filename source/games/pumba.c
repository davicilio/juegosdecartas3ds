#include "games/pumba.h"

int pu_card_points(Card c)
{
	if (c.rank == RANK_CABALLO)
		return 20;
	if (c.rank >= RANK_SOTA)
		return 10;
	return c.rank;
}

void pu_init(Pumba* g, uint32_t seed)
{
	rng_seed(&g->rng, seed);
	g->dealer = PU_PLAYERS - 1;
	pu_new_game(g);
}

void pu_new_game(Pumba* g)
{
	for (int p = 0; p < PU_PLAYERS; p++)
		g->score[p] = 0;
	pu_new_round(g);
}

static void shuffle(Pumba* g, Card* cards, int n)
{
	for (int i = n - 1; i > 0; i--) {
		int j = rng_range(&g->rng, i + 1);
		Card t = cards[i];
		cards[i] = cards[j];
		cards[j] = t;
	}
}

/* Saca una carta del mazo; si se acaba, se baraja el montón menos la de arriba */
static bool take(Pumba* g, Card* out)
{
	if (g->deck_count == 0) {
		if (g->discard_count <= 1)
			return false;
		Card top = g->discard[g->discard_count - 1];
		for (int i = 0; i < g->discard_count - 1; i++)
			g->deck[g->deck_count++] = g->discard[i];
		g->discard[0] = top;
		g->discard_count = 1;
		shuffle(g, g->deck, g->deck_count);
	}
	*out = g->deck[--g->deck_count];
	return true;
}

static int give(Pumba* g, int player, int n)
{
	int got = 0;
	Card c;
	while (got < n && take(g, &c)) {
		g->hands[player][g->hand_count[player]++] = c;
		got++;
	}
	if (g->hand_count[player] > 1)
		g->said[player] = false;
	return got;
}

void pu_new_round(Pumba* g)
{
	Deck d;
	deck_init(&d);
	g->deck_count = 0;
	for (int i = 0; i < d.count; i++)
		g->deck[g->deck_count++] = d.cards[i];
	shuffle(g, g->deck, g->deck_count);
	g->discard_count = 0;
	for (int p = 0; p < PU_PLAYERS; p++) {
		g->hand_count[p] = 0;
		g->said[p] = false;
	}
	g->dealer = (g->dealer + 1) % PU_PLAYERS;
	for (int k = 0; k < PU_START_HAND; k++)
		for (int p = 0; p < PU_PLAYERS; p++)
			give(g, p, 1);
	/* La de salida es una carta sin efecto */
	for (;;) {
		Card first;
		take(g, &first);
		if (first.rank >= 3 && first.rank <= 7) {
			g->discard[g->discard_count++] = first;
			break;
		}
		g->deck[g->deck_count++] = first;
		for (int i = g->deck_count - 1; i > 0; i--) { /* la devuelve abajo */
			Card t = g->deck[i];
			g->deck[i] = g->deck[i - 1];
			g->deck[i - 1] = t;
		}
	}
	g->suit = pu_top(g).suit;
	g->direction = 1;
	g->current = (g->dealer + 1) % PU_PLAYERS;
	g->pending = 0;
	g->drew = false;
	g->drawn_index = -1;
	g->round_winner = -1;
	g->round_points = 0;
	g->last_player = -1;
	g->last_penalty_player = -1;
	g->last_penalty = 0;
	g->last_skipped = false;
	g->phase = PU_PHASE_PLAYING;
}

Card pu_top(const Pumba* g)
{
	return g->discard[g->discard_count - 1];
}

int pu_next_player(const Pumba* g, int from)
{
	return (from + g->direction + PU_PLAYERS) % PU_PLAYERS;
}

bool pu_can_play(const Pumba* g, int player, int hand_index)
{
	if (g->phase != PU_PHASE_PLAYING || player != g->current || hand_index < 0 ||
		hand_index >= g->hand_count[player])
		return false;
	if (g->drew && hand_index != g->drawn_index)
		return false;
	Card c = g->hands[player][hand_index];
	if (g->pending > 0)
		return c.rank == 2; /* solo se libra echando otro dos */
	if (c.rank == RANK_CABALLO)
		return true;
	return c.suit == g->suit || c.rank == pu_top(g).rank;
}

static void end_round(Pumba* g, int winner)
{
	int pts = 0;
	for (int p = 0; p < PU_PLAYERS; p++)
		for (int i = 0; i < g->hand_count[p]; i++)
			pts += pu_card_points(g->hands[p][i]);
	g->round_winner = winner;
	g->round_points = pts;
	g->score[winner] += pts;
	g->phase = pu_game_winner(g) >= 0 ? PU_PHASE_GAME_OVER : PU_PHASE_ROUND_OVER;
}

static void advance(Pumba* g, Card c)
{
	int next = pu_next_player(g, g->current);
	g->last_skipped = false;
	if (c.rank == 1) {
		g->last_skipped = true;
		next = pu_next_player(g, next);
	}
	g->current = next;
	g->drew = false;
	g->drawn_index = -1;
}

bool pu_play(Pumba* g, int hand_index)
{
	int p = g->current;
	if (!pu_can_play(g, p, hand_index))
		return false;
	Card c = g->hands[p][hand_index];
	for (int i = hand_index; i < g->hand_count[p] - 1; i++)
		g->hands[p][i] = g->hands[p][i + 1];
	g->hand_count[p]--;
	g->discard[g->discard_count++] = c;
	g->last_player = p;
	g->last_card = c;
	g->last_penalty_player = -1;
	g->last_penalty = 0;
	if (g->hand_count[p] != 1)
		g->said[p] = false;
	if (c.rank == 2)
		g->pending += 2;
	if (g->hand_count[p] == 0) {
		/* Los doses de la última carta también se roban */
		if (g->pending > 0)
			give(g, pu_next_player(g, p), g->pending);
		g->pending = 0;
		end_round(g, p);
		return true;
	}
	if (c.rank == RANK_CABALLO) {
		g->phase = PU_PHASE_CHOOSE_SUIT;
		return true;
	}
	g->suit = c.suit;
	if (c.rank == RANK_SOTA)
		g->direction = -g->direction;
	advance(g, c);
	return true;
}

bool pu_choose_suit(Pumba* g, int suit)
{
	if (g->phase != PU_PHASE_CHOOSE_SUIT || suit < 0 || suit >= NUM_SUITS)
		return false;
	g->suit = suit;
	g->phase = PU_PHASE_PLAYING;
	advance(g, pu_top(g));
	return true;
}

bool pu_draw(Pumba* g)
{
	if (g->phase != PU_PHASE_PLAYING || g->drew)
		return false;
	int p = g->current;
	g->last_penalty_player = -1;
	if (g->pending > 0) {
		int n = give(g, p, g->pending);
		g->last_penalty_player = p;
		g->last_penalty = n;
		g->pending = 0;
		g->drew = true;
		pu_pass(g);
		return true;
	}
	g->drew = true;
	if (give(g, p, 1) == 0) {
		pu_pass(g);
		return true;
	}
	g->drawn_index = g->hand_count[p] - 1;
	if (!pu_can_play(g, p, g->drawn_index))
		pu_pass(g);
	return true;
}

bool pu_pass(Pumba* g)
{
	if (g->phase != PU_PHASE_PLAYING || !g->drew)
		return false;
	g->current = pu_next_player(g, g->current);
	g->drew = false;
	g->drawn_index = -1;
	g->last_skipped = false;
	return true;
}

bool pu_say(Pumba* g, int player)
{
	int n = g->hand_count[player];
	if (g->phase >= PU_PHASE_ROUND_OVER || n > 2 || (n == 2 && g->current != player))
		return false;
	g->said[player] = true;
	return true;
}

bool pu_catchable(const Pumba* g, int player)
{
	return g->phase < PU_PHASE_ROUND_OVER && g->hand_count[player] == 1 && !g->said[player];
}

bool pu_catch(Pumba* g, int player)
{
	if (!pu_catchable(g, player))
		return false;
	give(g, player, 2);
	g->last_penalty_player = player;
	g->last_penalty = 2;
	return true;
}

int pu_game_winner(const Pumba* g)
{
	for (int p = 0; p < PU_PLAYERS; p++)
		if (g->score[p] >= PU_GAME_POINTS)
			return p;
	return -1;
}

int pu_ai_suit(const Pumba* g, int player)
{
	int count[NUM_SUITS] = { 0 };
	for (int i = 0; i < g->hand_count[player]; i++)
		if (g->hands[player][i].rank != RANK_CABALLO)
			count[g->hands[player][i].suit]++;
	int best = 0;
	for (int s = 1; s < NUM_SUITS; s++)
		if (count[s] > count[best])
			best = s;
	return best;
}

int pu_ai_choose(const Pumba* g, int player)
{
	int next = pu_next_player(g, player);
	bool danger = g->hand_count[next] <= 2;
	int count[NUM_SUITS] = { 0 };
	for (int i = 0; i < g->hand_count[player]; i++)
		count[g->hands[player][i].suit]++;
	int best = -1, best_score = -100000;
	for (int i = 0; i < g->hand_count[player]; i++) {
		if (!pu_can_play(g, player, i))
			continue;
		Card c = g->hands[player][i];
		int score;
		if (c.rank == RANK_CABALLO) {
			score = -40; /* el comodín se guarda */
		} else {
			score = count[c.suit] * 4 + pu_card_points(c);
			bool action = c.rank == 1 || c.rank == 2 || c.rank == RANK_SOTA;
			if (action && danger)
				score += 50;
		}
		if (score > best_score) {
			best_score = score;
			best = i;
		}
	}
	return best;
}
