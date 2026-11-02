#include "games/backgammon.h"

#include <string.h>

static int dir(int p)
{
	return p == 0 ? -1 : 1;
}

static int owner_count(const BgBoard* b, int i, int p)
{
	int v = b->pt[i];
	return p == 0 ? (v > 0 ? v : 0) : (v < 0 ? -v : 0);
}

void bg_start_board(BgBoard* b)
{
	memset(b, 0, sizeof *b);
	b->pt[23] = 2;
	b->pt[12] = 5;
	b->pt[7] = 3;
	b->pt[5] = 5;
	b->pt[0] = -2;
	b->pt[11] = -5;
	b->pt[16] = -3;
	b->pt[18] = -5;
}

void bg_init(Backgammon* g, uint32_t seed)
{
	memset(g, 0, sizeof *g);
	rng_seed(&g->rng, seed);
	bg_start_board(&g->b);
	g->winner = -1;
	g->phase = BG_ROLL;
	/* Empieza quien saque más con un dado cada uno */
	int a, c;
	do {
		a = 1 + rng_range(&g->rng, 6);
		c = 1 + rng_range(&g->rng, 6);
	} while (a == c);
	g->current = a > c ? 0 : 1;
}

int bg_pips(const BgBoard* b, int p)
{
	int n = b->bar[p] * 25;
	for (int i = 0; i < BG_POINTS; i++)
		n += owner_count(b, i, p) * (p == 0 ? i + 1 : BG_POINTS - i);
	return n;
}

static bool all_home(const BgBoard* b, int p)
{
	if (b->bar[p])
		return false;
	for (int i = 0; i < BG_POINTS; i++) {
		bool home = p == 0 ? i <= 5 : i >= 18;
		if (!home && owner_count(b, i, p))
			return false;
	}
	return true;
}

/* Destino de mover desde from con el dado, o -1 si no se puede */
static int target(const BgBoard* b, int p, int from, int die)
{
	if (b->bar[p] && from != BG_BAR)
		return -1;
	if (from == BG_BAR) {
		if (!b->bar[p])
			return -1;
		int t = p == 0 ? BG_POINTS - die : die - 1;
		return owner_count(b, t, 1 - p) >= 2 ? -1 : t;
	}
	if (from < 0 || from >= BG_POINTS || !owner_count(b, from, p))
		return -1;
	int t = from + dir(p) * die;
	if (t >= 0 && t < BG_POINTS)
		return owner_count(b, t, 1 - p) >= 2 ? -1 : t;
	/* Sacar ficha */
	if (!all_home(b, p))
		return -1;
	int exact = p == 0 ? from + 1 : BG_POINTS - from;
	if (die == exact)
		return BG_OFF;
	/* Con un dado mayor, solo desde la punta más alejada */
	for (int i = 0; i < BG_POINTS; i++) {
		int dist = p == 0 ? i + 1 : BG_POINTS - i;
		if (dist > exact && owner_count(b, i, p))
			return -1;
	}
	return BG_OFF;
}

static void apply(BgBoard* b, int p, int from, int to)
{
	int s = p == 0 ? 1 : -1;
	if (from == BG_BAR)
		b->bar[p]--;
	else
		b->pt[from] -= (int8_t)s;
	if (to == BG_OFF) {
		b->off[p]++;
		return;
	}
	/* Golpe a una ficha suelta contraria */
	if (owner_count(b, to, 1 - p) == 1) {
		b->pt[to] = 0;
		b->bar[1 - p]++;
	}
	b->pt[to] += (int8_t)s;
}

/* ---------------------------------------------------------------------------
 * Secuencias de movimientos
 * ------------------------------------------------------------------------- */

#define MAX_SEQ 4

typedef struct {
	BgStep step[MAX_SEQ];
	int n;
	BgBoard result;
} Sequence;

typedef struct {
	Sequence* out;
	int max_out, count, best_len;
} SeqSearch;

static void search(SeqSearch* s, const BgBoard* b, int p, const int* dice, int dn, Sequence* cur)
{
	bool moved = false;
	for (int k = 0; k < dn; k++) {
		/* Con dados repetidos basta probar uno */
		bool dup = false;
		for (int j = 0; j < k; j++)
			dup |= dice[j] == dice[k];
		if (dup)
			continue;
		int rest[4], rn = 0;
		for (int j = 0; j < dn; j++)
			if (j != k)
				rest[rn++] = dice[j];
		for (int from = 0; from <= BG_BAR; from++) {
			int t = target(b, p, from, dice[k]);
			if (t < 0)
				continue;
			moved = true;
			BgBoard nb = *b;
			apply(&nb, p, from, t);
			cur->step[cur->n++] = (BgStep){ (uint8_t)from, (uint8_t)t, (uint8_t)dice[k] };
			search(s, &nb, p, rest, rn, cur);
			cur->n--;
		}
	}
	if (!moved && cur->n > 0) {
		if (cur->n > s->best_len) {
			s->best_len = cur->n;
			s->count = 0;
		}
		if (cur->n == s->best_len && s->count < s->max_out) {
			s->out[s->count] = *cur;
			s->out[s->count].result = *b;
			s->count++;
		}
	}
}

/* Todas las jugadas completas válidas (con el máximo de dados y, si solo
 * cabe uno, el mayor) */
static int sequences(const Backgammon* g, Sequence* out, int max)
{
	SeqSearch s = { out, max, 0, 0 };
	Sequence cur = { .n = 0 };
	search(&s, &g->b, g->current, g->left, g->left_n, &cur);
	if (s.best_len == 1 && g->left_n == 2 && g->left[0] != g->left[1]) {
		int big = g->left[0] > g->left[1] ? g->left[0] : g->left[1];
		bool big_ok = false;
		for (int i = 0; i < s.count; i++)
			big_ok |= out[i].step[0].die == big;
		if (big_ok) {
			int k = 0;
			for (int i = 0; i < s.count; i++)
				if (out[i].step[0].die == big)
					out[k++] = out[i];
			s.count = k;
		}
	}
	return s.count;
}

#define MAX_SEQS 1500
static Sequence s_seqs[MAX_SEQS];

int bg_legal_steps(const Backgammon* g, BgStep* out, int max)
{
	if (g->phase != BG_MOVE)
		return 0;
	int n = sequences(g, s_seqs, MAX_SEQS), k = 0;
	for (int i = 0; i < n && k < max; i++) {
		BgStep st = s_seqs[i].step[0];
		bool dup = false;
		for (int j = 0; j < k; j++)
			dup |= out[j].from == st.from && out[j].to == st.to;
		if (!dup)
			out[k++] = st;
	}
	return k;
}

static void end_turn(Backgammon* g)
{
	int p = g->current;
	if (g->b.off[p] == BG_CHECKERS) {
		int o = 1 - p;
		g->winner = p;
		g->points = 1;
		if (g->b.off[o] == 0) {
			g->points = 2;
			bool deep = g->b.bar[o] > 0;
			for (int i = 0; i < BG_POINTS; i++) {
				bool winner_home = p == 0 ? i <= 5 : i >= 18;
				if (winner_home && owner_count(&g->b, i, o))
					deep = true;
			}
			if (deep)
				g->points = 3;
		}
		g->phase = BG_OVER;
		return;
	}
	g->current = 1 - p;
	g->left_n = 0;
	g->phase = BG_ROLL;
}

void bg_roll(Backgammon* g, int d1, int d2)
{
	if (g->phase != BG_ROLL)
		return;
	g->dice[0] = d1 >= 1 && d1 <= 6 ? d1 : 1 + rng_range(&g->rng, 6);
	g->dice[1] = d2 >= 1 && d2 <= 6 ? d2 : 1 + rng_range(&g->rng, 6);
	g->left_n = g->dice[0] == g->dice[1] ? 4 : 2;
	for (int i = 0; i < g->left_n; i++)
		g->left[i] = g->dice[i % 2];
	g->phase = BG_MOVE;
	BgStep st[1];
	g->no_move = bg_legal_steps(g, st, 1) == 0;
	if (g->no_move)
		end_turn(g);
}

bool bg_step(Backgammon* g, int from, int to)
{
	BgStep steps[64];
	int n = bg_legal_steps(g, steps, 64);
	for (int i = 0; i < n; i++) {
		if (steps[i].from != from || steps[i].to != to)
			continue;
		apply(&g->b, g->current, from, to);
		for (int k = 0; k < g->left_n; k++)
			if (g->left[k] == steps[i].die) {
				g->left[k] = g->left[--g->left_n];
				break;
			}
		BgStep more[1];
		if (g->left_n == 0 || g->b.off[g->current] == BG_CHECKERS || bg_legal_steps(g, more, 1) == 0)
			end_turn(g);
		return true;
	}
	return false;
}

/* ---------------------------------------------------------------------------
 * IA
 * ------------------------------------------------------------------------- */

/* Cuántas tiradas (de 36) le dan a o un tiro directo a la ficha de i */
static int exposure(const BgBoard* b, int p, int i)
{
	int o = 1 - p, shots = 0;
	for (int d = 1; d <= 24; d++) {
		int src = i - dir(o) * d; /* de dónde vendría el contrario */
		bool has = src >= 0 && src < BG_POINTS ? owner_count(b, src, o) > 0 : (src == (o == 0 ? 24 : -1) && b->bar[o] > 0);
		if (has)
			shots += d <= 6 ? 11 : d <= 12 ? 3 : 1;
	}
	return shots;
}

static double evaluate(const BgBoard* b, int p)
{
	int o = 1 - p;
	double sc = (bg_pips(b, o) - bg_pips(b, p)) * 1.0;
	sc += b->off[p] * 6 - b->off[o] * 6;
	sc += b->bar[o] * 18 - b->bar[p] * 18;
	bool race = true; /* ya no hay contacto */
	int my_back = p == 0 ? -1 : 24, their_back = p == 0 ? 24 : -1;
	for (int i = 0; i < BG_POINTS; i++) {
		if (owner_count(b, i, p))
			my_back = p == 0 ? (i > my_back ? i : my_back) : (i < my_back ? i : my_back);
		if (owner_count(b, i, o))
			their_back = p == 0 ? (i < their_back ? i : their_back) : (i > their_back ? i : their_back);
	}
	if (b->bar[0] || b->bar[1] || (p == 0 ? my_back > their_back : my_back < their_back))
		race = false;
	if (race)
		return sc;
	for (int i = 0; i < BG_POINTS; i++) {
		int n = owner_count(b, i, p);
		bool home = p == 0 ? i <= 5 : i >= 18;
		if (n >= 2)
			sc += home ? 6 : 3;
		else if (n == 1)
			sc -= exposure(b, p, i) * 0.6 * (home ? 1.3 : 1.0);
	}
	/* Bloqueos: puntas seguidas hechas */
	int run = 0, best = 0;
	for (int i = 0; i < BG_POINTS; i++) {
		run = owner_count(b, i, p) >= 2 ? run + 1 : 0;
		if (run > best)
			best = run;
	}
	sc += best * best * 1.5;
	return sc;
}

void bg_ai_play(Backgammon* g)
{
	if (g->phase != BG_MOVE)
		return;
	int n = sequences(g, s_seqs, MAX_SEQS);
	if (n == 0) {
		end_turn(g);
		return;
	}
	int best = 0;
	double best_sc = -1e18;
	for (int i = 0; i < n; i++) {
		double sc = evaluate(&s_seqs[i].result, g->current) + rng_range(&g->rng, 100) * 0.001;
		if (sc > best_sc) {
			best_sc = sc;
			best = i;
		}
	}
	Sequence chosen = s_seqs[best];
	for (int k = 0; k < chosen.n && g->phase == BG_MOVE; k++)
		bg_step(g, chosen.step[k].from, chosen.step[k].to);
}
