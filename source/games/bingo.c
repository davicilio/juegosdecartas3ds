#include "games/bingo.h"

#include <string.h>

static int col_lo(int c)
{
	return c == 0 ? 1 : c * 10;
}

static int col_hi(int c)
{
	return c == BI_COLS - 1 ? 90 : c * 10 + 9;
}

void bi_make_card(Rng* rng, BiCard* card)
{
	for (;;) {
		memset(card, 0, sizeof *card);
		/* Cuántos números lleva cada columna: al menos uno, como mucho tres */
		int count[BI_COLS];
		for (int c = 0; c < BI_COLS; c++)
			count[c] = 1;
		for (int extra = BI_ROWS * BI_PER_ROW - BI_COLS; extra > 0;) {
			int c = rng_range(rng, BI_COLS);
			if (count[c] < BI_ROWS) {
				count[c]++;
				extra--;
			}
		}
		/* Filas de cada número: las columnas más llenas primero, eligiendo las
		 * filas a las que más les falta */
		int row_left[BI_ROWS] = { BI_PER_ROW, BI_PER_ROW, BI_PER_ROW };
		int cols[BI_COLS];
		for (int c = 0; c < BI_COLS; c++)
			cols[c] = c;
		for (int i = BI_COLS - 1; i > 0; i--) {
			int j = rng_range(rng, i + 1);
			int t = cols[i];
			cols[i] = cols[j];
			cols[j] = t;
		}
		for (int i = 1; i < BI_COLS; i++)
			for (int j = i; j > 0 && count[cols[j]] > count[cols[j - 1]]; j--) {
				int t = cols[j];
				cols[j] = cols[j - 1];
				cols[j - 1] = t;
			}
		bool ok = true;
		bool used[BI_ROWS][BI_COLS] = { { false } };
		for (int k = 0; k < BI_COLS && ok; k++) {
			int c = cols[k];
			for (int n = 0; n < count[c]; n++) {
				int best = -1;
				for (int r = 0; r < BI_ROWS; r++) {
					if (used[r][c] || row_left[r] == 0)
						continue;
					if (best < 0 || row_left[r] > row_left[best] ||
						(row_left[r] == row_left[best] && rng_range(rng, 2)))
						best = r;
				}
				if (best < 0) {
					ok = false;
					break;
				}
				used[best][c] = true;
				row_left[best]--;
			}
		}
		if (!ok)
			continue;
		/* Números distintos de la decena, ordenados de arriba abajo */
		for (int c = 0; c < BI_COLS; c++) {
			int pick[BI_ROWS], n = 0;
			while (n < count[c]) {
				int v = col_lo(c) + rng_range(rng, col_hi(c) - col_lo(c) + 1);
				bool dup = false;
				for (int i = 0; i < n; i++)
					dup |= pick[i] == v;
				if (!dup)
					pick[n++] = v;
			}
			for (int i = 1; i < n; i++)
				for (int j = i; j > 0 && pick[j] < pick[j - 1]; j--) {
					int t = pick[j];
					pick[j] = pick[j - 1];
					pick[j - 1] = t;
				}
			for (int r = 0, i = 0; r < BI_ROWS; r++)
				if (used[r][c])
					card->num[r][c] = (uint8_t)pick[i++];
		}
		return;
	}
}

bool bi_card_valid(const BiCard* c)
{
	int total = 0;
	for (int r = 0; r < BI_ROWS; r++) {
		int n = 0;
		for (int k = 0; k < BI_COLS; k++)
			if (c->num[r][k]) {
				n++;
				if (c->num[r][k] < col_lo(k) || c->num[r][k] > col_hi(k))
					return false;
			}
		if (n != BI_PER_ROW)
			return false;
		total += n;
	}
	for (int k = 0; k < BI_COLS; k++) {
		int prev = 0, n = 0;
		for (int r = 0; r < BI_ROWS; r++)
			if (c->num[r][k]) {
				if (c->num[r][k] <= prev)
					return false;
				prev = c->num[r][k];
				n++;
			}
		if (n == 0)
			return false;
	}
	return total == BI_ROWS * BI_PER_ROW;
}

void bi_init(Bingo* g, uint32_t seed)
{
	memset(g, 0, sizeof *g);
	rng_seed(&g->rng, seed);
	for (int p = 0; p < BI_PLAYERS; p++)
		g->chips[p] = BI_START;
	g->phase = BI_SETUP;
}

bool bi_start(Bingo* g, int n)
{
	if (n < 1 || n > BI_MAX_CARDS || g->chips[0] < n * BI_PRICE)
		return false;
	memset(g->drawn, 0, sizeof g->drawn);
	g->drawn_n = 0;
	g->pot = 0;
	for (int p = 0; p < BI_PLAYERS; p++) {
		int k = p == 0 ? n : 1 + rng_range(&g->rng, 2);
		/* Si un rival se queda sin fichas, se le presta para un cartón */
		if (g->chips[p] < k * BI_PRICE)
			g->chips[p] = BI_START / 2;
		g->card_n[p] = k;
		g->chips[p] -= k * BI_PRICE;
		g->pot += k * BI_PRICE;
		for (int i = 0; i < k; i++)
			bi_make_card(&g->rng, &g->cards[p][i]);
	}
	for (int i = 0; i < BI_BALLS; i++)
		g->order[i] = (uint8_t)(i + 1);
	for (int i = BI_BALLS - 1; i > 0; i--) {
		int j = rng_range(&g->rng, i + 1);
		uint8_t t = g->order[i];
		g->order[i] = g->order[j];
		g->order[j] = t;
	}
	g->line_prize = g->pot * BI_LINE_PCT / 100;
	g->bingo_prize = g->pot - g->line_prize;
	g->line_winner = g->bingo_winner = -1;
	g->phase = BI_PLAYING;
	return true;
}

int bi_last(const Bingo* g)
{
	return g->drawn_n ? g->order[g->drawn_n - 1] : 0;
}

int bi_draw(Bingo* g)
{
	if (g->phase != BI_PLAYING || g->drawn_n >= BI_BALLS)
		return 0;
	int b = g->order[g->drawn_n++];
	g->drawn[b] = true;
	return b;
}

int bi_card_line(const Bingo* g, const BiCard* c)
{
	for (int r = 0; r < BI_ROWS; r++) {
		bool full = true;
		for (int k = 0; k < BI_COLS; k++)
			if (c->num[r][k] && !g->drawn[c->num[r][k]])
				full = false;
		if (full)
			return r;
	}
	return -1;
}

int bi_card_missing(const Bingo* g, const BiCard* c)
{
	int n = 0;
	for (int r = 0; r < BI_ROWS; r++)
		for (int k = 0; k < BI_COLS; k++)
			if (c->num[r][k] && !g->drawn[c->num[r][k]])
				n++;
	return n;
}

bool bi_card_full(const Bingo* g, const BiCard* c)
{
	return bi_card_missing(g, c) == 0;
}

bool bi_has_line(const Bingo* g, int p)
{
	if (g->phase != BI_PLAYING || g->line_winner >= 0)
		return false;
	for (int i = 0; i < g->card_n[p]; i++)
		if (bi_card_line(g, &g->cards[p][i]) >= 0)
			return true;
	return false;
}

bool bi_has_bingo(const Bingo* g, int p)
{
	if (g->phase != BI_PLAYING)
		return false;
	for (int i = 0; i < g->card_n[p]; i++)
		if (bi_card_full(g, &g->cards[p][i]))
			return true;
	return false;
}

bool bi_claim_line(Bingo* g, int p)
{
	if (!bi_has_line(g, p))
		return false;
	for (int i = 0; i < g->card_n[p]; i++) {
		int r = bi_card_line(g, &g->cards[p][i]);
		if (r >= 0) {
			g->line_winner = p;
			g->line_card = i;
			g->line_row = r;
			break;
		}
	}
	g->chips[p] += g->line_prize;
	return true;
}

bool bi_claim_bingo(Bingo* g, int p)
{
	if (!bi_has_bingo(g, p))
		return false;
	for (int i = 0; i < g->card_n[p]; i++)
		if (bi_card_full(g, &g->cards[p][i])) {
			g->bingo_card = i;
			break;
		}
	g->bingo_winner = p;
	/* Si nadie cantó línea, el bingo se lo lleva todo */
	g->chips[p] += g->bingo_prize + (g->line_winner < 0 ? g->line_prize : 0);
	if (g->line_winner < 0)
		g->line_prize = 0;
	g->phase = BI_OVER;
	return true;
}

bool bi_mark(Bingo* g, int card, int row, int col)
{
	if (card < 0 || card >= g->card_n[0] || row < 0 || row >= BI_ROWS || col < 0 || col >= BI_COLS)
		return false;
	BiCard* c = &g->cards[0][card];
	int n = c->num[row][col];
	if (!n || !g->drawn[n])
		return false;
	c->marked[row][col] = !c->marked[row][col];
	return true;
}

void bi_mark_all(Bingo* g, int p)
{
	for (int i = 0; i < g->card_n[p]; i++)
		for (int r = 0; r < BI_ROWS; r++)
			for (int k = 0; k < BI_COLS; k++)
				if (g->cards[p][i].num[r][k] && g->drawn[g->cards[p][i].num[r][k]])
					g->cards[p][i].marked[r][k] = true;
}

int bi_best_missing(const Bingo* g, int p)
{
	int best = 99;
	for (int i = 0; i < g->card_n[p]; i++) {
		int m = bi_card_missing(g, &g->cards[p][i]);
		if (m < best)
			best = m;
	}
	return best;
}
