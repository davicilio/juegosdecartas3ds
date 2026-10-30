#include "games/toma6.h"

#include <string.h>

int tm_heads(int c)
{
	if (c == 55)
		return 7;
	if (c % 11 == 0)
		return 5;
	if (c % 10 == 0)
		return 3;
	if (c % 5 == 0)
		return 2;
	return 1;
}

int tm_row_heads(const Toma6* g, int r)
{
	int h = 0;
	for (int i = 0; i < g->row_n[r]; i++)
		h += tm_heads(g->row[r][i]);
	return h;
}

void tm_init(Toma6* g, uint32_t seed)
{
	memset(g, 0, sizeof *g);
	rng_seed(&g->rng, seed);
	tm_new_game(g);
}

void tm_new_game(Toma6* g)
{
	memset(g->score, 0, sizeof g->score);
	tm_new_round(g);
}

static void sort_hand(uint8_t* h, int n)
{
	for (int i = 1; i < n; i++)
		for (int j = i; j > 0 && h[j] < h[j - 1]; j--) {
			uint8_t t = h[j];
			h[j] = h[j - 1];
			h[j - 1] = t;
		}
}

void tm_new_round(Toma6* g)
{
	uint8_t deck[TM_CARDS];
	for (int i = 0; i < TM_CARDS; i++)
		deck[i] = (uint8_t)(i + 1);
	for (int i = TM_CARDS - 1; i > 0; i--) {
		int j = rng_range(&g->rng, i + 1);
		uint8_t t = deck[i];
		deck[i] = deck[j];
		deck[j] = t;
	}
	int k = 0;
	for (int p = 0; p < TM_PLAYERS; p++) {
		for (int i = 0; i < TM_HAND; i++)
			g->hand[p][i] = deck[k++];
		g->hand_n[p] = TM_HAND;
		sort_hand(g->hand[p], TM_HAND);
		g->round_heads[p] = 0;
		g->played[p] = 0;
	}
	for (int r = 0; r < TM_ROWS; r++) {
		g->row[r][0] = deck[k++];
		g->row_n[r] = 1;
	}
	g->placed = 0;
	g->last_player = -1;
	g->phase = TM_CHOOSE;
}

bool tm_choose(Toma6* g, int p, int idx)
{
	if (g->phase != TM_CHOOSE || g->played[p] || idx < 0 || idx >= g->hand_n[p])
		return false;
	g->played[p] = g->hand[p][idx];
	for (int i = idx; i < g->hand_n[p] - 1; i++)
		g->hand[p][i] = g->hand[p][i + 1];
	g->hand_n[p]--;
	for (int q = 0; q < TM_PLAYERS; q++)
		if (!g->played[q])
			return true;
	/* Todos han elegido: se colocan de menor a mayor */
	for (int q = 0; q < TM_PLAYERS; q++)
		g->order[q] = q;
	for (int i = 1; i < TM_PLAYERS; i++)
		for (int j = i; j > 0 && g->played[g->order[j]] < g->played[g->order[j - 1]]; j--) {
			int t = g->order[j];
			g->order[j] = g->order[j - 1];
			g->order[j - 1] = t;
		}
	g->placed = 0;
	g->phase = TM_PLACE;
	return true;
}

int tm_target_row(const Toma6* g, int card)
{
	int best = -1;
	for (int r = 0; r < TM_ROWS; r++) {
		int last = g->row[r][g->row_n[r] - 1];
		if (last < card && (best < 0 || last > g->row[best][g->row_n[best] - 1]))
			best = r;
	}
	return best;
}

static void take(Toma6* g, int p, int r, int card)
{
	int h = tm_row_heads(g, r);
	g->score[p] += h;
	g->round_heads[p] += h;
	g->row[r][0] = (uint8_t)card;
	g->row_n[r] = 1;
	g->last_took = true;
	g->last_taken = h;
}

static void finish_card(Toma6* g)
{
	g->placed++;
	if (g->placed < TM_PLAYERS) {
		g->phase = TM_PLACE;
		return;
	}
	memset(g->played, 0, sizeof g->played);
	if (g->hand_n[0] > 0) {
		g->phase = TM_CHOOSE;
		return;
	}
	g->phase = tm_winner(g) >= 0 ? TM_GAME_OVER : TM_ROUND_OVER;
}

bool tm_step(Toma6* g)
{
	if (g->phase != TM_PLACE)
		return false;
	int p = g->order[g->placed];
	int card = g->played[p];
	int r = tm_target_row(g, card);
	g->last_player = p;
	g->last_took = false;
	g->last_taken = 0;
	if (r < 0) {
		g->chooser = p;
		g->phase = TM_TAKE_ROW;
		return true;
	}
	g->last_row = r;
	if (g->row_n[r] == TM_ROW_MAX)
		take(g, p, r, card); /* la sexta se lleva la fila */
	else
		g->row[r][g->row_n[r]++] = (uint8_t)card;
	finish_card(g);
	return true;
}

bool tm_take_row(Toma6* g, int r)
{
	if (g->phase != TM_TAKE_ROW || r < 0 || r >= TM_ROWS)
		return false;
	int p = g->chooser;
	g->last_player = p;
	g->last_row = r;
	take(g, p, r, g->played[p]);
	finish_card(g);
	return true;
}

int tm_winner(const Toma6* g)
{
	bool end = false;
	for (int p = 0; p < TM_PLAYERS; p++)
		end |= g->score[p] >= TM_END;
	if (!end)
		return -1;
	int best = 0;
	for (int p = 1; p < TM_PLAYERS; p++)
		if (g->score[p] < g->score[best])
			best = p;
	return best;
}

int tm_ai_row(const Toma6* g)
{
	int best = 0;
	for (int r = 1; r < TM_ROWS; r++)
		if (tm_row_heads(g, r) < tm_row_heads(g, best) ||
			(tm_row_heads(g, r) == tm_row_heads(g, best) && g->row_n[r] < g->row_n[best]))
			best = r;
	return best;
}

int tm_ai_card(const Toma6* g, int p)
{
	int best = 0, best_v = 1 << 30;
	int min_row = tm_row_heads(g, tm_ai_row(g));
	for (int i = 0; i < g->hand_n[p]; i++) {
		int c = g->hand[p][i];
		int r = tm_target_row(g, c);
		int v;
		if (r < 0) {
			v = min_row * 10 + 5; /* se llevará la fila más barata */
		} else {
			int last = g->row[r][g->row_n[r] - 1];
			int gap = c - last;
			int n = g->row_n[r];
			if (n == TM_ROW_MAX)
				v = tm_row_heads(g, r) * 10; /* seguro que se la lleva */
			else
				/* Cuantas menos cartas puedan colarse delante y más hueco en la fila, mejor */
				v = gap * (n + 1) / 2 + (n == TM_ROW_MAX - 1 ? tm_row_heads(g, r) * 3 : 0);
		}
		v += tm_heads(c); /* mejor soltar las cartas con muchas cabezas sin riesgo */
		if (v < best_v) {
			best_v = v;
			best = i;
		}
	}
	return best;
}
