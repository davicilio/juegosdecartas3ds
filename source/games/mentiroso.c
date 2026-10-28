#include "games/mentiroso.h"

#include <string.h>

void men_init(Mentiroso* g, uint32_t seed)
{
	rng_seed(&g->rng, seed);
	g->current = 0;
	men_new_game(g);
}

void men_new_game(Mentiroso* g)
{
	Deck d;
	deck_init(&d);
	deck_shuffle(&d, &g->rng);
	for (int p = 0; p < MEN_PLAYERS; p++)
		g->hand_count[p] = 0;
	for (int i = 0; i < d.count; i++)
		g->hands[i % MEN_PLAYERS][g->hand_count[i % MEN_PLAYERS]++] = d.cards[i];
	for (int p = 0; p < MEN_PLAYERS; p++) {
		/* Mano ordenada para que sea más fácil verla */
		for (int i = 1; i < g->hand_count[p]; i++) {
			Card c = g->hands[p][i];
			int j = i - 1;
			while (j >= 0 && card_pos(g->hands[p][j]) * 4 + g->hands[p][j].suit > card_pos(c) * 4 + c.suit) {
				g->hands[p][j + 1] = g->hands[p][j];
				j--;
			}
			g->hands[p][j + 1] = c;
		}
	}
	g->pile_count = 0;
	g->rank = 0;
	g->declared = 0;
	g->last_n = 0;
	g->last_player = -1;
	g->current = (g->current + 1) % MEN_PLAYERS;
	g->winner = -1;
	g->accuser = g->accused = g->taker = -1;
	g->phase = MEN_PHASE_PLAYING;
}

static void remove_cards(Mentiroso* g, int p, const int* idx, int n)
{
	bool gone[DECK_SIZE] = { false };
	for (int i = 0; i < n; i++)
		gone[idx[i]] = true;
	int k = 0;
	for (int i = 0; i < g->hand_count[p]; i++)
		if (!gone[i])
			g->hands[p][k++] = g->hands[p][i];
	g->hand_count[p] = k;
}

/* Quien ha echado sus últimas cartas sin que le acusen, gana */
static bool check_winner(Mentiroso* g)
{
	if (g->last_player >= 0 && g->hand_count[g->last_player] == 0) {
		g->winner = g->last_player;
		g->phase = MEN_PHASE_GAME_OVER;
		return true;
	}
	return false;
}

bool men_play(Mentiroso* g, const int* idx, int n, int rank)
{
	int p = g->current;
	if (g->phase != MEN_PHASE_PLAYING || n < 1 || n > MEN_MAX_PLAY || n > g->hand_count[p])
		return false;
	for (int i = 0; i < n; i++) {
		if (idx[i] < 0 || idx[i] >= g->hand_count[p])
			return false;
		for (int j = 0; j < i; j++)
			if (idx[i] == idx[j])
				return false;
	}
	if (g->rank == 0) {
		if (!card_valid(card_make(SUIT_OROS, rank)))
			return false;
		g->rank = rank;
		g->declared = 0;
	}
	/* Si el anterior se quedó sin cartas y nadie le acusa, ha ganado */
	if (check_winner(g))
		return true;
	for (int i = 0; i < n; i++) {
		g->last[i] = g->hands[p][idx[i]];
		g->pile[g->pile_count++] = g->last[i];
	}
	g->last_n = n;
	g->last_player = p;
	g->declared += n;
	remove_cards(g, p, idx, n);
	g->current = (p + 1) % MEN_PLAYERS;
	return true;
}

bool men_can_call(const Mentiroso* g)
{
	return g->phase == MEN_PHASE_PLAYING && g->last_player >= 0 && g->last_player != g->current;
}

bool men_call(Mentiroso* g)
{
	if (!men_can_call(g))
		return false;
	bool lie = false;
	for (int i = 0; i < g->last_n; i++)
		if (g->last[i].rank != g->rank)
			lie = true;
	g->accuser = g->current;
	g->accused = g->last_player;
	g->was_lie = lie;
	g->taker = lie ? g->accused : g->accuser;
	for (int i = 0; i < g->pile_count; i++)
		g->hands[g->taker][g->hand_count[g->taker]++] = g->pile[i];
	g->pile_count = 0;
	g->phase = MEN_PHASE_REVEAL;
	return true;
}

void men_continue(Mentiroso* g)
{
	if (g->phase != MEN_PHASE_REVEAL)
		return;
	g->phase = MEN_PHASE_PLAYING;
	/* Si dijo la verdad con sus últimas cartas, gana */
	if (!g->was_lie && check_winner(g))
		return;
	/* Empieza tanda nueva quien ha ganado la acusación */
	g->current = g->taker == g->accused ? g->accuser : g->accused;
	if (g->hand_count[g->current] == 0)
		g->current = g->accuser;
	g->rank = 0;
	g->declared = 0;
	g->last_player = -1;
	g->last_n = 0;
}

static int count_rank(const Mentiroso* g, int p, int rank)
{
	int n = 0;
	for (int i = 0; i < g->hand_count[p]; i++)
		n += g->hands[p][i].rank == rank;
	return n;
}

bool men_ai_turn(Mentiroso* g, int* idx, int* n, int* rank)
{
	int p = g->current;
	if (men_can_call(g)) {
		int mine = count_rank(g, p, g->rank);
		/* Imposible: hay más declaradas de las que pueden quedar */
		if (mine + g->declared > 4)
			return true;
		/* El que se queda sin cartas: siempre se le acusa */
		if (g->hand_count[g->last_player] == 0)
			return true;
		int suspicion = (g->last_n - 1) * 20 + mine * 15 + (g->declared - g->last_n) * 8;
		if ((int)rng_range(&g->rng, 100) < suspicion)
			return true;
	}
	int r = g->rank;
	if (r == 0) {
		/* Empieza con el número del que más cartas tiene */
		int best = 0;
		for (int i = 0; i < g->hand_count[p]; i++) {
			int c = count_rank(g, p, g->hands[p][i].rank);
			if (c > best) {
				best = c;
				r = g->hands[p][i].rank;
			}
		}
	}
	*rank = r;
	*n = 0;
	for (int i = 0; i < g->hand_count[p] && *n < MEN_MAX_PLAY; i++)
		if (g->hands[p][i].rank == r)
			idx[(*n)++] = i;
	if (*n == 0) {
		/* Toca mentir: una carta (a veces dos), las que menos sirven */
		int want = g->hand_count[p] > 4 && rng_range(&g->rng, 3) == 0 ? 2 : 1;
		for (int i = 0; i < g->hand_count[p] && *n < want; i++)
			if (count_rank(g, p, g->hands[p][i].rank) == 1)
				idx[(*n)++] = i;
		for (int i = 0; i < g->hand_count[p] && *n < want; i++) {
			bool used = false;
			for (int k = 0; k < *n; k++)
				used |= idx[k] == i;
			if (!used)
				idx[(*n)++] = i;
		}
	}
	return false;
}
