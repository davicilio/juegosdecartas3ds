#include "games/julepe.h"

#include <string.h>

int ju_strength(Card c)
{
	switch (c.rank) {
	case 1:
		return 10;
	case 3:
		return 9;
	case RANK_REY:
		return 8;
	case RANK_CABALLO:
		return 7;
	case RANK_SOTA:
		return 6;
	case 2:
		return 1;
	default:
		return c.rank - 2; /* 7 -> 5 ... 4 -> 2 */
	}
}

bool ju_beats(const Julepe* g, Card challenger, Card best)
{
	if (challenger.suit == best.suit)
		return ju_strength(challenger) > ju_strength(best);
	return challenger.suit == g->trump;
}

void ju_init(Julepe* g, uint32_t seed)
{
	memset(g, 0, sizeof *g);
	rng_seed(&g->rng, seed);
	g->dealer = (int)rng_range(&g->rng, JU_PLAYERS);
	ju_new_game(g);
}

void ju_new_game(Julepe* g)
{
	for (int p = 0; p < JU_PLAYERS; p++)
		g->chips[p] = JU_START;
	g->pot = 0;
	g->hands_played = 0;
	ju_new_hand(g);
}

void ju_new_hand(Julepe* g)
{
	g->dealer = (g->dealer + 1) % JU_PLAYERS;
	g->chips[g->dealer] -= JU_ANTE;
	g->pot += JU_ANTE;
	deck_init(&g->deck);
	deck_shuffle(&g->deck, &g->rng);
	for (int p = 0; p < JU_PLAYERS; p++) {
		g->hand_count[p] = 0;
		g->decided[p] = g->playing[p] = g->julepe[p] = false;
		g->discarded[p] = 0;
		g->tricks[p] = 0;
		g->change[p] = 0;
		g->trick_played[p] = false;
	}
	for (int k = 0; k < JU_HAND; k++)
		for (int i = 1; i <= JU_PLAYERS; i++) {
			int p = (g->dealer + i) % JU_PLAYERS;
			deck_draw(&g->deck, &g->hands[p][g->hand_count[p]++]);
		}
	deck_draw(&g->deck, &g->trump_card);
	g->trump = g->trump_card.suit;
	g->played_in_trick = 0;
	g->tricks_done = 0;
	g->void_hand = false;
	g->current = (g->dealer + 1) % JU_PLAYERS;
	g->phase = JU_DECIDING;
}

int ju_entered(const Julepe* g)
{
	int n = 0;
	for (int p = 0; p < JU_PLAYERS; p++)
		n += g->playing[p];
	return n;
}

bool ju_must_play(const Julepe* g, int player)
{
	return g->phase == JU_DECIDING && player == g->dealer && ju_entered(g) == 1;
}

static int next_playing(const Julepe* g, int from)
{
	for (int i = 1; i <= JU_PLAYERS; i++) {
		int p = (from + i) % JU_PLAYERS;
		if (g->playing[p])
			return p;
	}
	return from;
}

static void end_hand(Julepe* g)
{
	g->hands_played++;
	bool broke = false;
	for (int p = 0; p < JU_PLAYERS; p++)
		if (g->chips[p] <= 0)
			broke = true;
	g->phase = broke || g->hands_played >= JU_MAX_HANDS ? JU_GAME_OVER : JU_HAND_OVER;
}

bool ju_decide(Julepe* g, bool play)
{
	if (g->phase != JU_DECIDING)
		return false;
	int p = g->current;
	if (!play && ju_must_play(g, p))
		return false;
	g->decided[p] = true;
	g->playing[p] = play;
	if (p != g->dealer) {
		g->current = (p + 1) % JU_PLAYERS;
		return true;
	}
	if (ju_entered(g) < 2) {
		/* Nadie se anima: el plato se queda para la siguiente */
		g->void_hand = true;
		end_hand(g);
		return true;
	}
	g->phase = JU_DISCARDING;
	g->current = next_playing(g, g->dealer);
	return true;
}

bool ju_discard(Julepe* g, const bool sel[JU_HAND])
{
	if (g->phase != JU_DISCARDING)
		return false;
	int p = g->current;
	int n = 0;
	for (int i = 0; i < g->hand_count[p]; i++)
		if (sel && sel[i]) {
			Card c;
			if (!deck_draw(&g->deck, &c))
				break; /* no quedan cartas para cambiar */
			g->hands[p][i] = c;
			n++;
		}
	g->discarded[p] = n;
	/* Se descartan por orden desde la derecha de quien reparte */
	int first = next_playing(g, g->dealer);
	int nxt = next_playing(g, p);
	if (nxt == first) {
		g->phase = JU_PLAYING;
		g->leader = g->current = first;
		g->played_in_trick = 0;
	} else {
		g->current = nxt;
	}
	return true;
}

int ju_trick_leader_now(const Julepe* g)
{
	if (g->played_in_trick == 0)
		return -1;
	int best = g->leader;
	for (int k = 1; k < g->played_in_trick; k++) {
		int p = g->leader;
		for (int j = 0; j < k; j++)
			p = next_playing(g, p);
		if (ju_beats(g, g->trick[p], g->trick[best]))
			best = p;
	}
	return best;
}

bool ju_can_play(const Julepe* g, int player, int hand_index)
{
	if (g->phase != JU_PLAYING || player != g->current || hand_index < 0 || hand_index >= g->hand_count[player])
		return false;
	if (g->played_in_trick == 0)
		return true;
	const Card* h = g->hands[player];
	int n = g->hand_count[player];
	Card c = h[hand_index];
	int lead = g->trick[g->leader].suit;
	Card best = g->trick[ju_trick_leader_now(g)];
	bool has_suit = false, can_beat_suit = false, can_beat_trump = false, has_trump = false;
	for (int i = 0; i < n; i++) {
		if (h[i].suit == lead) {
			has_suit = true;
			if (ju_beats(g, h[i], best))
				can_beat_suit = true;
		} else if (h[i].suit == g->trump) {
			has_trump = true;
			if (ju_beats(g, h[i], best))
				can_beat_trump = true;
		}
	}
	if (has_suit) {
		if (c.suit != lead)
			return false;
		return !can_beat_suit || ju_beats(g, c, best);
	}
	if (can_beat_trump)
		return c.suit == g->trump && ju_beats(g, c, best);
	(void)has_trump;
	return true;
}

bool ju_play(Julepe* g, int hand_index)
{
	int p = g->current;
	if (!ju_can_play(g, p, hand_index))
		return false;
	g->trick[p] = g->hands[p][hand_index];
	g->trick_played[p] = true;
	for (int i = hand_index; i < g->hand_count[p] - 1; i++)
		g->hands[p][i] = g->hands[p][i + 1];
	g->hand_count[p]--;
	g->played_in_trick++;
	if (g->played_in_trick == ju_entered(g)) {
		g->trick_winner = ju_trick_leader_now(g);
		g->phase = JU_TRICK_DONE;
	} else {
		g->current = next_playing(g, p);
	}
	return true;
}

bool ju_collect(Julepe* g)
{
	if (g->phase != JU_TRICK_DONE)
		return false;
	int w = g->trick_winner;
	g->tricks[w]++;
	g->tricks_done++;
	for (int p = 0; p < JU_PLAYERS; p++)
		g->trick_played[p] = false;
	g->played_in_trick = 0;
	if (g->tricks_done < JU_HAND) {
		g->leader = g->current = w;
		g->phase = JU_PLAYING;
		return true;
	}
	/* Reparto del plato */
	int share = g->pot / JU_HAND;
	int julepe_total = 0;
	for (int p = 0; p < JU_PLAYERS; p++) {
		if (!g->playing[p])
			continue;
		g->change[p] = g->tricks[p] * share;
		if (g->tricks[p] < 2) {
			g->julepe[p] = true;
			g->change[p] -= g->pot;
			julepe_total += g->pot;
		}
		g->chips[p] += g->change[p];
	}
	g->pot = julepe_total;
	end_hand(g);
	return true;
}

int ju_winner(const Julepe* g)
{
	int best = 0;
	for (int p = 1; p < JU_PLAYERS; p++)
		if (g->chips[p] > g->chips[best])
			best = p;
	return best;
}

/* ---------------------------------------------------------------------------
 * IA
 * ------------------------------------------------------------------------- */

/* Bazas que espera hacer, más o menos */
static float hand_value(const Julepe* g, int player)
{
	float v = 0.0f;
	for (int i = 0; i < g->hand_count[player]; i++) {
		Card c = g->hands[player][i];
		int s = ju_strength(c);
		if (c.suit == g->trump)
			v += s >= 8 ? 1.0f : s >= 5 ? 0.6f : 0.4f;
		else
			v += c.rank == 1 ? 0.7f : c.rank == 3 ? 0.4f : 0.0f;
	}
	return v;
}

bool ju_ai_enter(const Julepe* g, int player)
{
	/* Con el cambio de cartas se mejora algo */
	float v = hand_value(g, player) + 0.4f;
	int entered = ju_entered(g);
	return v >= 2.0f - (entered == 0 ? 0.2f : 0.0f) + (g->pot > 20 ? 0.2f : 0.0f);
}

void ju_ai_discard(const Julepe* g, int player, bool sel[JU_HAND])
{
	for (int i = 0; i < JU_HAND; i++) {
		sel[i] = false;
		if (i >= g->hand_count[player])
			continue;
		Card c = g->hands[player][i];
		if (c.suit != g->trump && c.rank != 1 && c.rank != 3)
			sel[i] = true;
	}
}

int ju_ai_choose(const Julepe* g, int player)
{
	int n = g->hand_count[player];
	int best = -1;
	if (g->played_in_trick == 0) {
		/* Sale con su carta más fuerte si es un as o triunfo alto; si no, la más baja */
		int top = -1, low = -1;
		for (int i = 0; i < n; i++) {
			Card c = g->hands[player][i];
			int v = ju_strength(c) + (c.suit == g->trump ? 20 : 0);
			if (top < 0 || v > ju_strength(g->hands[player][top]) + (g->hands[player][top].suit == g->trump ? 20 : 0))
				top = i;
			if (low < 0 || v < ju_strength(g->hands[player][low]) + (g->hands[player][low].suit == g->trump ? 20 : 0))
				low = i;
		}
		Card t = g->hands[player][top];
		bool strong = t.rank == 1 || (t.suit == g->trump && ju_strength(t) >= 8);
		return strong && g->tricks[player] < 2 ? top : low;
	}
	Card cur = g->trick[ju_trick_leader_now(g)];
	/* Gana con la carta más baja que gane; si no puede, echa la más baja */
	int win = -1, low = -1;
	for (int i = 0; i < n; i++) {
		if (!ju_can_play(g, player, i))
			continue;
		Card c = g->hands[player][i];
		int v = ju_strength(c) + (c.suit == g->trump ? 20 : 0);
		if (ju_beats(g, c, cur)) {
			if (win < 0 ||
				v < ju_strength(g->hands[player][win]) + (g->hands[player][win].suit == g->trump ? 20 : 0))
				win = i;
		}
		if (low < 0 || v < ju_strength(g->hands[player][low]) + (g->hands[player][low].suit == g->trump ? 20 : 0))
			low = i;
	}
	best = win >= 0 ? win : low;
	return best;
}
