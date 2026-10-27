#include "games/chinchon.h"

#include <string.h>

int ch_card_value(Card c)
{
	return c.rank <= 7 ? c.rank : 10;
}

/* ---------------------------------------------------------------------------
 * Análisis de combinaciones
 * ------------------------------------------------------------------------- */

#define MAX_CANDIDATES 64

static int popcount16(uint16_t m)
{
	int n = 0;
	for (; m; m &= m - 1)
		n++;
	return n;
}

static int find_melds(const Card* cards, int n, uint16_t* out)
{
	int count = 0;

	/* Tríos y cuartetos: todos los subconjuntos de 3 o más cartas del mismo número */
	for (int rank = 1; rank <= RANK_REY; rank++) {
		uint16_t same = 0;
		for (int i = 0; i < n; i++)
			if (cards[i].rank == rank)
				same |= 1u << i;
		if (popcount16(same) < 3)
			continue;
		for (uint16_t sub = same; sub; sub = (sub - 1) & same)
			if (popcount16(sub) >= 3 && count < MAX_CANDIDATES)
				out[count++] = sub;
	}

	/* Escaleras: tramos de 3 o más posiciones seguidas del mismo palo */
	for (int suit = 0; suit < NUM_SUITS; suit++) {
		int at[RANKS_PER_SUIT];
		for (int p = 0; p < RANKS_PER_SUIT; p++)
			at[p] = -1;
		for (int i = 0; i < n; i++)
			if (cards[i].suit == suit)
				at[card_pos(cards[i])] = i;
		for (int start = 0; start < RANKS_PER_SUIT; start++) {
			uint16_t run = 0;
			for (int p = start; p < RANKS_PER_SUIT && at[p] >= 0; p++) {
				run |= 1u << at[p];
				if (p - start >= 2 && count < MAX_CANDIDATES)
					out[count++] = run;
			}
		}
	}
	return count;
}

typedef struct {
	const Card* cards;
	int n;
	const uint16_t* cand;
	int cand_count;
	uint16_t chosen[CH_MAX_MELDS];
	int chosen_count;
	ChAnalysis best;
	int best_value;
	int best_loose;
} Search;

static void dfs(Search* s, int idx, uint16_t used, int loose_value, uint16_t loose)
{
	if (loose_value > s->best_value)
		return;
	while (idx < s->n && (used & (1u << idx)))
		idx++;
	if (idx >= s->n) {
		int lc = popcount16(loose);
		if (loose_value < s->best_value || (loose_value == s->best_value && lc < s->best_loose)) {
			s->best_value = loose_value;
			s->best_loose = lc;
			s->best.meld_count = s->chosen_count;
			memcpy(s->best.melds, s->chosen, sizeof s->chosen);
			s->best.loose = loose;
			s->best.loose_count = lc;
			s->best.loose_value = loose_value;
		}
		return;
	}
	uint16_t bit = 1u << idx;
	/* Usar la carta idx en alguna combinación compatible */
	if (s->chosen_count < CH_MAX_MELDS) {
		for (int k = 0; k < s->cand_count; k++) {
			uint16_t m = s->cand[k];
			if ((m & bit) && !(m & used)) {
				s->chosen[s->chosen_count++] = m;
				dfs(s, idx + 1, used | m, loose_value, loose);
				s->chosen_count--;
			}
		}
	}
	/* O dejarla suelta */
	dfs(s, idx + 1, used | bit, loose_value + ch_card_value(s->cards[idx]), loose | bit);
}

void ch_analyze(const Card* cards, int n, ChAnalysis* out)
{
	uint16_t cand[MAX_CANDIDATES];
	Search s;
	memset(&s, 0, sizeof s);
	s.cards = cards;
	s.n = n;
	s.cand = cand;
	s.cand_count = find_melds(cards, n, cand);
	s.best_value = 1 << 30;
	s.best_loose = 99;
	dfs(&s, 0, 0, 0, 0);
	*out = s.best;
}

static int card_cmp(Card a, Card b)
{
	return card_index(a) - card_index(b);
}

static int sort_key_cmp(Card a, Card b)
{
	/* Sueltas ordenadas por número y después por palo */
	if (a.rank != b.rank)
		return card_pos(a) - card_pos(b);
	return a.suit - b.suit;
}

void ch_sort_hand(Card* cards, int n)
{
	ChAnalysis a;
	ch_analyze(cards, n, &a);
	Card out[CH_MAX_HAND];
	int k = 0;
	for (int m = 0; m < a.meld_count; m++) {
		int start = k;
		for (int i = 0; i < n; i++)
			if (a.melds[m] & (1u << i))
				out[k++] = cards[i];
		/* dentro de la combinación, en orden de baraja */
		for (int i = start + 1; i < k; i++)
			for (int j = i; j > start && card_cmp(out[j - 1], out[j]) > 0; j--) {
				Card t = out[j];
				out[j] = out[j - 1];
				out[j - 1] = t;
			}
	}
	int start = k;
	for (int i = 0; i < n; i++)
		if (a.loose & (1u << i))
			out[k++] = cards[i];
	for (int i = start + 1; i < k; i++)
		for (int j = i; j > start && sort_key_cmp(out[j - 1], out[j]) > 0; j--) {
			Card t = out[j];
			out[j] = out[j - 1];
			out[j - 1] = t;
		}
	memcpy(cards, out, sizeof(Card) * n);
}

static void without(const Card* cards, int n, int skip, Card* out)
{
	int k = 0;
	for (int i = 0; i < n; i++)
		if (i != skip)
			out[k++] = cards[i];
}

static bool closable(const ChAnalysis* a)
{
	return a->loose_count == 0 || (a->loose_count == 1 && a->loose_value <= CH_CLOSE_MAX);
}

static bool is_chinchon(const Card* cards, int n)
{
	if (n != CH_HAND_SIZE)
		return false;
	int low = RANKS_PER_SUIT, high = -1;
	for (int i = 0; i < n; i++) {
		if (cards[i].suit != cards[0].suit)
			return false;
		int p = card_pos(cards[i]);
		low = p < low ? p : low;
		high = p > high ? p : high;
	}
	/* siete cartas distintas del mismo palo en un tramo de siete posiciones */
	return high - low == CH_HAND_SIZE - 1;
}

bool ch_can_close(const Chinchon* g, int hand_index)
{
	int p = g->current;
	if (g->phase != CH_PHASE_DISCARD || hand_index < 0 || hand_index >= g->hand_count[p])
		return false;
	Card rest[CH_MAX_HAND];
	without(g->hands[p], g->hand_count[p], hand_index, rest);
	ChAnalysis a;
	ch_analyze(rest, g->hand_count[p] - 1, &a);
	return closable(&a);
}

/* ---------------------------------------------------------------------------
 * Desarrollo de la partida
 * ------------------------------------------------------------------------- */

void ch_init(Chinchon* g, uint32_t seed)
{
	rng_seed(&g->rng, seed);
	g->score[0] = g->score[1] = 0;
	g->round = 0;
	g->dealer = 1; /* en la primera ronda empieza el humano */
	ch_start_round(g);
}

void ch_start_round(Chinchon* g)
{
	deck_init(&g->deck);
	deck_shuffle(&g->deck, &g->rng);
	for (int p = 0; p < CH_PLAYERS; p++) {
		g->hand_count[p] = 0;
		g->round_points[p] = 0;
		memset(&g->final[p], 0, sizeof g->final[p]);
	}
	for (int i = 0; i < CH_HAND_SIZE; i++)
		for (int k = 1; k <= CH_PLAYERS; k++) {
			int p = (g->dealer + k) % CH_PLAYERS;
			deck_draw(&g->deck, &g->hands[p][g->hand_count[p]++]);
		}
	for (int p = 0; p < CH_PLAYERS; p++)
		ch_sort_hand(g->hands[p], g->hand_count[p]);
	g->discard_count = 0;
	deck_draw(&g->deck, &g->discard[g->discard_count++]);

	g->round++;
	g->turns = 0;
	g->current = (g->dealer + 1) % CH_PLAYERS;
	g->phase = CH_PHASE_DRAW;
	g->closer = -1;
	g->chinchon = false;
	g->menos_diez = false;
	g->laid_off = 0;
	g->last_player = -1;
	g->reshuffles = 0;
}

static void refill_deck(Chinchon* g)
{
	/* Se baraja el montón de descartes salvo la carta de arriba. */
	if (g->deck.count > 0 || g->discard_count <= 1)
		return;
	Card top = g->discard[g->discard_count - 1];
	g->deck.count = 0;
	for (int i = 0; i < g->discard_count - 1; i++)
		g->deck.cards[g->deck.count++] = g->discard[i];
	deck_shuffle(&g->deck, &g->rng);
	g->discard[0] = top;
	g->discard_count = 1;
	g->reshuffles++;
}

static bool add_to_hand(Chinchon* g, Card c, bool from_discard)
{
	int p = g->current;
	g->hands[p][g->hand_count[p]++] = c;
	ch_sort_hand(g->hands[p], g->hand_count[p]);
	g->last_player = p;
	g->last_from_discard = from_discard;
	g->phase = CH_PHASE_DISCARD;
	return true;
}

bool ch_draw_deck(Chinchon* g)
{
	if (g->phase != CH_PHASE_DRAW)
		return false;
	refill_deck(g);
	Card c;
	if (!deck_draw(&g->deck, &c))
		return false;
	return add_to_hand(g, c, false);
}

bool ch_draw_discard(Chinchon* g)
{
	if (g->phase != CH_PHASE_DRAW || g->discard_count == 0)
		return false;
	return add_to_hand(g, g->discard[--g->discard_count], true);
}

static void remove_card(Chinchon* g, int p, int idx)
{
	for (int i = idx; i < g->hand_count[p] - 1; i++)
		g->hands[p][i] = g->hands[p][i + 1];
	g->hand_count[p]--;
}

bool ch_discard(Chinchon* g, int hand_index)
{
	int p = g->current;
	if (g->phase != CH_PHASE_DISCARD || hand_index < 0 || hand_index >= g->hand_count[p])
		return false;
	g->last_discarded = g->hands[p][hand_index];
	g->discard[g->discard_count++] = g->last_discarded;
	remove_card(g, p, hand_index);
	ch_sort_hand(g->hands[p], g->hand_count[p]);
	g->turns++;
	g->current = 1 - p;
	g->phase = CH_PHASE_DRAW;
	return true;
}

/* ¿Puede la carta c añadirse a la combinación m (de la mano cards)? */
static bool extends(const Card* cards, int n, uint16_t m, Card c, const Card* extra, int extra_n)
{
	Card set[CH_MAX_HAND * 2];
	int k = 0;
	for (int i = 0; i < n; i++)
		if (m & (1u << i))
			set[k++] = cards[i];
	for (int i = 0; i < extra_n; i++)
		set[k++] = extra[i];
	bool group = true;
	for (int i = 1; i < k; i++)
		if (set[i].rank != set[0].rank)
			group = false;
	if (group)
		return c.rank == set[0].rank && k < NUM_SUITS;
	int low = RANKS_PER_SUIT, high = -1;
	for (int i = 0; i < k; i++) {
		int p = card_pos(set[i]);
		low = p < low ? p : low;
		high = p > high ? p : high;
	}
	return c.suit == set[0].suit && (card_pos(c) == low - 1 || card_pos(c) == high + 1);
}

static void lay_off(Chinchon* g, int closer)
{
	int o = 1 - closer;
	const ChAnalysis* ca = &g->final[closer];
	ChAnalysis* oa = &g->final[o];
	Card added[CH_MAX_MELDS][CH_MAX_HAND];
	int added_n[CH_MAX_MELDS] = { 0 };
	bool changed = true;
	while (changed) {
		changed = false;
		for (int i = 0; i < g->hand_count[o]; i++) {
			uint16_t bit = 1u << i;
			if (!(oa->loose & bit) || (g->laid_off & bit))
				continue;
			Card c = g->hands[o][i];
			for (int m = 0; m < ca->meld_count; m++) {
				if (extends(g->hands[closer], g->hand_count[closer], ca->melds[m], c, added[m], added_n[m])) {
					added[m][added_n[m]++] = c;
					g->laid_off |= bit;
					changed = true;
					break;
				}
			}
		}
	}
	for (int i = 0; i < g->hand_count[o]; i++)
		if (g->laid_off & (1u << i)) {
			oa->loose &= ~(1u << i);
			oa->loose_count--;
			oa->loose_value -= ch_card_value(g->hands[o][i]);
		}
}

bool ch_close(Chinchon* g, int hand_index)
{
	if (!ch_can_close(g, hand_index))
		return false;
	int p = g->current;
	g->last_discarded = g->hands[p][hand_index];
	g->discard[g->discard_count++] = g->last_discarded;
	remove_card(g, p, hand_index);
	g->turns++;
	g->closer = p;
	g->last_player = p;

	for (int q = 0; q < CH_PLAYERS; q++) {
		ch_sort_hand(g->hands[q], g->hand_count[q]);
		ch_analyze(g->hands[q], g->hand_count[q], &g->final[q]);
	}
	g->chinchon = is_chinchon(g->hands[p], g->hand_count[p]);
	g->menos_diez = g->final[p].loose_count == 0;
	lay_off(g, p);

	g->round_points[p] = g->menos_diez ? -10 : g->final[p].loose_value;
	g->round_points[1 - p] = g->final[1 - p].loose_value;
	if (!g->chinchon)
		for (int q = 0; q < CH_PLAYERS; q++)
			g->score[q] += g->round_points[q];
	g->phase = ch_winner(g) >= 0 ? CH_PHASE_GAME_OVER : CH_PHASE_ROUND_OVER;
	return true;
}

bool ch_next_round(Chinchon* g)
{
	if (g->phase != CH_PHASE_ROUND_OVER)
		return false;
	g->dealer = 1 - g->dealer;
	ch_start_round(g);
	return true;
}

int ch_winner(const Chinchon* g)
{
	if (g->closer >= 0 && g->chinchon)
		return g->closer;
	bool out0 = g->score[0] > CH_LIMIT, out1 = g->score[1] > CH_LIMIT;
	if (!out0 && !out1)
		return -1;
	if (out0 && out1)
		return g->score[0] < g->score[1] ? 0 : 1;
	return out0 ? 1 : 0;
}

/* ---------------------------------------------------------------------------
 * IA
 * ------------------------------------------------------------------------- */

/* Valor de quedarse con estas siete cartas: puntos sueltos menos un premio
 * por las "casi combinaciones" (parejas y cartas seguidas del mismo palo). */
static int hand_cost(const Card* cards, int n)
{
	ChAnalysis a;
	ch_analyze(cards, n, &a);
	int cost = a.loose_value * 4;
	for (int i = 0; i < n; i++) {
		if (!(a.loose & (1u << i)))
			continue;
		for (int j = 0; j < n; j++) {
			if (i == j)
				continue;
			bool pair = cards[i].rank == cards[j].rank;
			int d = card_pos(cards[i]) - card_pos(cards[j]);
			bool near = cards[i].suit == cards[j].suit && (d == 1 || d == -1 || d == 2 || d == -2);
			if (pair || near)
				cost -= 5;
		}
	}
	return cost;
}

static int best_discard(const Card* cards, int n, int* cost_out)
{
	int best = 0, best_cost = 1 << 30;
	Card rest[CH_MAX_HAND];
	for (int i = 0; i < n; i++) {
		without(cards, n, i, rest);
		int cost = hand_cost(rest, n - 1) * 10 - ch_card_value(cards[i]);
		if (cost < best_cost) {
			best_cost = cost;
			best = i;
		}
	}
	if (cost_out)
		*cost_out = best_cost;
	return best;
}

bool ch_ai_take_discard(const Chinchon* g, int player)
{
	if (g->discard_count == 0)
		return false;
	int n = g->hand_count[player];
	Card with[CH_MAX_HAND];
	memcpy(with, g->hands[player], sizeof(Card) * n);
	with[n] = g->discard[g->discard_count - 1];
	int cost_with;
	int idx = best_discard(with, n + 1, &cost_with);
	if (idx == n)
		return false; /* la descartaría enseguida: no interesa */
	/* Solo si mejora claramente lo que ya tiene. */
	int now = hand_cost(g->hands[player], n) * 10;
	return cost_with < now - 30;
}

int ch_ai_discard(const Chinchon* g, int player, bool* close_out)
{
	int n = g->hand_count[player];
	*close_out = false;
	/* Cerrar si se puede, prefiriendo quedarse con todo combinado. */
	int close_idx = -1, close_value = 1 << 30;
	for (int i = 0; i < n; i++) {
		Card rest[CH_MAX_HAND];
		without(g->hands[player], n, i, rest);
		ChAnalysis a;
		ch_analyze(rest, n - 1, &a);
		if (closable(&a) && a.loose_value < close_value) {
			close_value = a.loose_value;
			close_idx = i;
		}
	}
	if (close_idx >= 0) {
		*close_out = true;
		return close_idx;
	}
	return best_discard(g->hands[player], n, NULL);
}
