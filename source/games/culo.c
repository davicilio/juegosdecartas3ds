#include "games/culo.h"

int culo_value(Card c)
{
	/* índice = rango 1..12 */
	static const int value[13] = { 0, 8, 9, 0, 1, 2, 3, 4, 0, 0, 5, 6, 7 };
	return c.rank <= 12 ? value[c.rank] : 0;
}

const char* culo_role_name(int role)
{
	static const char* const names[CULO_PLAYERS] = { "Presidente", "Vicepresidente", "Vicecu", "Culo" };
	return role >= 0 && role < CULO_PLAYERS ? names[role] : "";
}

static void sort_hand(Culo* g, int p)
{
	for (int i = 1; i < g->hand_count[p]; i++) {
		Card c = g->hands[p][i];
		int j = i - 1;
		while (j >= 0 && culo_value(g->hands[p][j]) * 4 + g->hands[p][j].suit > culo_value(c) * 4 + c.suit) {
			g->hands[p][j + 1] = g->hands[p][j];
			j--;
		}
		g->hands[p][j + 1] = c;
	}
}

void culo_init(Culo* g, uint32_t seed)
{
	rng_seed(&g->rng, seed);
	for (int p = 0; p < CULO_PLAYERS; p++) {
		g->role[p] = -1;
		g->presidencies[p] = 0;
	}
	g->rounds = 0;
	culo_new_round(g);
}

/* Pasa n cartas de from a to: las mejores (best) o las peores */
static void give(Culo* g, int from, int to, int n, bool best)
{
	for (int k = 0; k < n; k++) {
		sort_hand(g, from);
		int i = best ? g->hand_count[from] - 1 : 0;
		g->hands[to][g->hand_count[to]++] = g->hands[from][i];
		for (int j = i; j < g->hand_count[from] - 1; j++)
			g->hands[from][j] = g->hands[from][j + 1];
		g->hand_count[from]--;
	}
}

void culo_new_round(Culo* g)
{
	Deck d;
	deck_init(&d);
	deck_shuffle(&d, &g->rng);
	for (int p = 0; p < CULO_PLAYERS; p++)
		g->hand_count[p] = 0;
	for (int i = 0; i < d.count; i++)
		g->hands[i % CULO_PLAYERS][g->hand_count[i % CULO_PLAYERS]++] = d.cards[i];
	int by_role[CULO_PLAYERS];
	bool roles = g->role[0] >= 0;
	if (roles) {
		for (int p = 0; p < CULO_PLAYERS; p++)
			by_role[g->role[p]] = p;
		/* El culo da sus dos mejores y recibe las dos peores del presidente */
		give(g, by_role[3], by_role[0], 2, true);
		give(g, by_role[0], by_role[3], 2, false);
		give(g, by_role[2], by_role[1], 1, true);
		give(g, by_role[1], by_role[2], 1, false);
	}
	for (int p = 0; p < CULO_PLAYERS; p++)
		sort_hand(g, p);
	/* Sale el culo; en la primera ronda, quien tiene el 3 de oros */
	g->current = 0;
	if (roles) {
		g->current = by_role[3];
	} else {
		for (int p = 0; p < CULO_PLAYERS; p++)
			for (int i = 0; i < g->hand_count[p]; i++)
				if (g->hands[p][i].rank == 3 && g->hands[p][i].suit == SUIT_OROS)
					g->current = p;
	}
	g->pile_n = 0;
	g->pile_owner = -1;
	g->passes = 0;
	g->finished = 0;
	g->phase = CULO_PHASE_PLAYING;
}

bool culo_done(const Culo* g, int player)
{
	return g->hand_count[player] == 0;
}

static int active_count(const Culo* g)
{
	int n = 0;
	for (int p = 0; p < CULO_PLAYERS; p++)
		n += !culo_done(g, p);
	return n;
}

bool culo_can_play(const Culo* g, const int* idx, int n)
{
	int p = g->current;
	if (g->phase != CULO_PHASE_PLAYING || n < 1 || n > CULO_MAX_PLAY)
		return false;
	for (int i = 0; i < n; i++) {
		if (idx[i] < 0 || idx[i] >= g->hand_count[p])
			return false;
		for (int j = 0; j < i; j++)
			if (idx[j] == idx[i])
				return false;
		if (g->hands[p][idx[i]].rank != g->hands[p][idx[0]].rank)
			return false;
	}
	if (g->pile_n == 0)
		return true;
	return n == g->pile_n && culo_value(g->hands[p][idx[0]]) > culo_value(g->pile[0]);
}

/* Siguiente jugador que aún tiene cartas */
static int next_active(const Culo* g, int from)
{
	for (int k = 1; k <= CULO_PLAYERS; k++) {
		int q = (from + k) % CULO_PLAYERS;
		if (!culo_done(g, q))
			return q;
	}
	return from;
}

static void finish_round(Culo* g)
{
	for (int p = 0; p < CULO_PLAYERS; p++)
		if (!culo_done(g, p))
			g->finish[g->finished++] = p;
	for (int k = 0; k < CULO_PLAYERS; k++)
		g->role[g->finish[k]] = k;
	g->presidencies[g->finish[0]]++;
	g->rounds++;
	g->phase = CULO_PHASE_ROUND_OVER;
}

bool culo_play(Culo* g, const int* idx, int n)
{
	if (!culo_can_play(g, idx, n))
		return false;
	int p = g->current;
	bool gone[DECK_SIZE] = { false };
	for (int i = 0; i < n; i++) {
		g->pile[i] = g->hands[p][idx[i]];
		gone[idx[i]] = true;
	}
	int k = 0;
	for (int i = 0; i < g->hand_count[p]; i++)
		if (!gone[i])
			g->hands[p][k++] = g->hands[p][i];
	g->hand_count[p] = k;
	g->pile_n = n;
	g->pile_owner = p;
	g->passes = 0;
	if (culo_done(g, p)) {
		g->finish[g->finished++] = p;
		if (active_count(g) <= 1) {
			finish_round(g);
			return true;
		}
	}
	g->current = next_active(g, p);
	return true;
}

bool culo_can_pass(const Culo* g)
{
	return g->phase == CULO_PHASE_PLAYING && g->pile_n > 0;
}

bool culo_pass(Culo* g)
{
	if (!culo_can_pass(g))
		return false;
	g->passes++;
	int next = next_active(g, g->current);
	/* Han pasado todos los demás: recoge quien echó lo último (o el siguiente si ya acabó) */
	int others = active_count(g) - (culo_done(g, g->pile_owner) ? 0 : 1);
	if (g->passes >= others) {
		g->pile_n = 0;
		g->passes = 0;
		g->current = culo_done(g, g->pile_owner) ? next_active(g, g->pile_owner) : g->pile_owner;
		return true;
	}
	g->current = next;
	return true;
}

bool culo_ai_choose(Culo* g, int* idx, int* n)
{
	int p = g->current;
	const Card* h = g->hands[p];
	int count = g->hand_count[p];
	/* Grupos de cartas iguales (la mano está ordenada) */
	int best_start = -1, best_len = 0;
	for (int i = 0; i < count;) {
		int j = i;
		while (j < count && h[j].rank == h[i].rank)
			j++;
		int len = j - i;
		if (g->pile_n == 0) {
			/* Salir: con lo más bajo, cuantas más cartas mejor (sin romper el 2) */
			if (best_start < 0 || (culo_value(h[i]) < 8 && len > best_len && culo_value(h[i]) <= culo_value(h[best_start]) + 2)) {
				best_start = i;
				best_len = len;
			}
		} else if (len >= g->pile_n && culo_value(h[i]) > culo_value(g->pile[0])) {
			/* Lo más bajo que gane, sin romper grupos grandes si se puede */
			if (best_start < 0 || (len == g->pile_n && best_len != g->pile_n)) {
				best_start = i;
				best_len = len;
			}
		}
		i = j;
	}
	if (best_start < 0)
		return false;
	/* Guardarse los doses y ases mientras haya otras cartas */
	if (g->pile_n > 0 && culo_value(h[best_start]) >= 8 && count > 4 && rng_range(&g->rng, 2))
		return false;
	*n = g->pile_n ? g->pile_n : best_len;
	if (*n > CULO_MAX_PLAY)
		*n = CULO_MAX_PLAY;
	for (int k = 0; k < *n; k++)
		idx[k] = best_start + k;
	return true;
}
