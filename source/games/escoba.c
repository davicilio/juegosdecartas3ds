#include "games/escoba.h"

#include <string.h>

int es_card_value(Card c)
{
	return c.rank <= 7 ? c.rank : c.rank - 2;
}

static int table_sum(const Escoba* g, uint32_t mask)
{
	int sum = 0;
	for (int i = 0; i < g->table_count; i++)
		if (mask & (1u << i))
			sum += es_card_value(g->table[i]);
	return sum;
}

static void search(const Escoba* g, int idx, int remaining, uint32_t mask, uint32_t* masks, int max, int* n)
{
	if (remaining == 0) {
		if (mask && *n < max)
			masks[(*n)++] = mask;
		return;
	}
	if (remaining < 0 || idx >= g->table_count || *n >= max)
		return;
	search(g, idx + 1, remaining - es_card_value(g->table[idx]), mask | (1u << idx), masks, max, n);
	search(g, idx + 1, remaining, mask, masks, max, n);
}

int es_find_captures(const Escoba* g, Card c, uint32_t* masks, int max)
{
	int n = 0;
	/* La mesa nunca supera 32 cartas en la práctica: con más, ya sumaría 15. */
	if (g->table_count > 32)
		return 0;
	search(g, 0, ES_SUM - es_card_value(c), 0, masks, max, &n);
	return n;
}

static void take(Escoba* g, int player, Card c)
{
	g->won[player][g->won_count[player]++] = c;
}

static void remove_table(Escoba* g, uint32_t mask)
{
	int k = 0;
	for (int i = 0; i < g->table_count; i++)
		if (!(mask & (1u << i)))
			g->table[k++] = g->table[i];
	g->table_count = k;
}

static void deal_hands(Escoba* g)
{
	/* Reparte de tres en tres empezando por el que no da. */
	for (int i = 0; i < ES_HAND_SIZE; i++)
		for (int k = 1; k <= ES_PLAYERS; k++) {
			int p = (g->dealer + k) % ES_PLAYERS;
			Card c;
			if (deck_draw(&g->deck, &c))
				g->hands[p][g->hand_count[p]++] = c;
		}
	g->deals++;
}

void es_init(Escoba* g, uint32_t seed)
{
	rng_seed(&g->rng, seed);
	g->score[0] = g->score[1] = 0;
	g->round = 0;
	g->dealer = 1; /* en la primera ronda es mano el humano */
	es_start_round(g);
}

void es_start_round(Escoba* g)
{
	deck_init(&g->deck);
	deck_shuffle(&g->deck, &g->rng);
	for (int p = 0; p < ES_PLAYERS; p++) {
		g->hand_count[p] = 0;
		g->won_count[p] = 0;
		g->escobas[p] = 0;
		memset(&g->round_score[p], 0, sizeof g->round_score[p]);
	}
	g->table_count = 0;
	for (int i = 0; i < 4; i++)
		deck_draw(&g->deck, &g->table[g->table_count++]);
	g->deals = 0;
	deal_hands(g);

	/* Si la mesa inicial suma 15 (o 30) el que reparte hace escoba (o dos). */
	int sum = table_sum(g, 0xF);
	g->deal_escobas = 0;
	g->last_capturer = g->dealer;
	if (sum == ES_SUM || sum == 2 * ES_SUM) {
		g->deal_escobas = sum / ES_SUM;
		g->escobas[g->dealer] += g->deal_escobas;
		for (int i = 0; i < g->table_count; i++)
			take(g, g->dealer, g->table[i]);
		g->table_count = 0;
	}

	g->round++;
	g->turns = 0;
	g->current = (g->dealer + 1) % ES_PLAYERS;
	g->last_player = -1;
	g->last_captured_count = 0;
	g->last_escoba = false;
	g->phase = ES_PHASE_PLAYING;
}

static void score_round(Escoba* g)
{
	EsRoundScore* s = g->round_score;
	for (int p = 0; p < ES_PLAYERS; p++) {
		s[p].escobas = g->escobas[p];
		s[p].cards = g->won_count[p];
		for (int i = 0; i < g->won_count[p]; i++) {
			Card c = g->won[p][i];
			if (c.suit == SUIT_OROS)
				s[p].oros++;
			if (c.rank == 7)
				s[p].sietes++;
			if (c.suit == SUIT_OROS && c.rank == 7)
				s[p].siete_oros = true;
		}
	}
	for (int p = 0; p < ES_PLAYERS; p++) {
		int o = 1 - p;
		s[p].pt_cards = s[p].cards > s[o].cards;
		s[p].pt_oros = s[p].oros > s[o].oros;
		s[p].pt_sietes = s[p].sietes > s[o].sietes;
		s[p].pt_siete_oros = s[p].siete_oros;
		s[p].total = s[p].escobas + s[p].pt_cards + s[p].pt_oros + s[p].pt_sietes + s[p].pt_siete_oros;
		g->score[p] += s[p].total;
	}
	g->phase = es_winner(g) >= 0 ? ES_PHASE_GAME_OVER : ES_PHASE_ROUND_OVER;
}

bool es_play(Escoba* g, int hand_index, uint32_t mask)
{
	int p = g->current;
	if (g->phase != ES_PHASE_PLAYING || hand_index < 0 || hand_index >= g->hand_count[p])
		return false;
	Card c = g->hands[p][hand_index];
	uint32_t options[ES_MAX_OPTIONS];
	int n = es_find_captures(g, c, options, ES_MAX_OPTIONS);
	if (n == 0 && mask != 0)
		return false;
	if (n > 0) {
		if (((uint64_t)mask >> g->table_count) != 0 || table_sum(g, mask) + es_card_value(c) != ES_SUM)
			return false;
	}

	for (int i = hand_index; i < g->hand_count[p] - 1; i++)
		g->hands[p][i] = g->hands[p][i + 1];
	g->hand_count[p]--;

	g->last_player = p;
	g->last_card = c;
	g->last_captured_count = 0;
	g->last_escoba = false;
	if (mask) {
		for (int i = 0; i < g->table_count; i++)
			if (mask & (1u << i)) {
				g->last_captured[g->last_captured_count++] = g->table[i];
				take(g, p, g->table[i]);
			}
		take(g, p, c);
		remove_table(g, mask);
		g->last_capturer = p;
		if (g->table_count == 0) {
			g->last_escoba = true;
			g->escobas[p]++;
		}
	} else {
		g->table[g->table_count++] = c;
	}

	g->turns++;
	g->current = (p + 1) % ES_PLAYERS;
	if (g->hand_count[0] == 0 && g->hand_count[1] == 0) {
		if (g->deck.count > 0) {
			deal_hands(g);
		} else {
			/* Fin de la ronda: lo que queda en la mesa es del último que recogió. */
			for (int i = 0; i < g->table_count; i++)
				take(g, g->last_capturer, g->table[i]);
			g->table_count = 0;
			score_round(g);
		}
	}
	return true;
}

static int card_worth(Card c)
{
	int w = 0;
	if (c.suit == SUIT_OROS)
		w += 4;
	if (c.rank == 7)
		w += 10;
	if (c.suit == SUIT_OROS && c.rank == 7)
		w += 40;
	return w;
}

int es_ai_choose(const Escoba* g, int player, uint32_t* mask_out)
{
	int best = 0;
	int best_score = -100000;
	uint32_t best_mask = 0;
	int total = 0;
	for (int t = 0; t < g->table_count; t++)
		total += es_card_value(g->table[t]);

	for (int i = 0; i < g->hand_count[player]; i++) {
		Card c = g->hands[player][i];
		uint32_t masks[ES_MAX_OPTIONS];
		int n = es_find_captures(g, c, masks, ES_MAX_OPTIONS);
		if (n == 0) {
			/* Dejar la carta: evitar que el rival pueda hacer escoba. */
			int after = total + es_card_value(c);
			int score = -card_worth(c) * 2 - es_card_value(c);
			if (after >= ES_SUM - 10 && after < ES_SUM)
				score -= 60;
			if (score > best_score) {
				best_score = score;
				best = i;
				best_mask = 0;
			}
			continue;
		}
		for (int k = 0; k < n; k++) {
			int score = 20 + card_worth(c);
			int rest = total;
			int taken = 1;
			for (int t = 0; t < g->table_count; t++)
				if (masks[k] & (1u << t)) {
					score += card_worth(g->table[t]) + 3;
					rest -= es_card_value(g->table[t]);
					taken++;
				}
			if (taken == g->table_count + 1)
				score += 100; /* escoba */
			else if (rest >= ES_SUM - 10 && rest < ES_SUM)
				score -= 30; /* el rival podría barrer lo que queda */
			if (score > best_score) {
				best_score = score;
				best = i;
				best_mask = masks[k];
			}
		}
	}
	*mask_out = best_mask;
	return best;
}

bool es_next_round(Escoba* g)
{
	if (g->phase != ES_PHASE_ROUND_OVER)
		return false;
	g->dealer = (g->dealer + 1) % ES_PLAYERS;
	es_start_round(g);
	return true;
}

int es_winner(const Escoba* g)
{
	if (g->score[0] < ES_GAME_POINTS && g->score[1] < ES_GAME_POINTS)
		return -1;
	if (g->score[0] == g->score[1])
		return -1; /* empate por encima del límite: otra ronda */
	return g->score[0] > g->score[1] ? 0 : 1;
}
