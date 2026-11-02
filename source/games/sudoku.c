#include "games/sudoku.h"

#include <string.h>

static bool fits(const uint8_t g[81], int i, int v)
{
	int r = i / 9, c = i % 9, br = r / 3 * 3, bc = c / 3 * 3;
	for (int k = 0; k < 9; k++) {
		if (g[r * 9 + k] == v || g[k * 9 + c] == v)
			return false;
		if (g[(br + k / 3) * 9 + bc + k % 3] == v)
			return false;
	}
	return true;
}

/* Busca la casilla vacía con menos candidatos (así la búsqueda va rápida) */
static int best_empty(const uint8_t g[81], uint16_t* cand)
{
	int best = -1, best_n = 10;
	for (int i = 0; i < 81; i++) {
		if (g[i])
			continue;
		uint16_t m = 0;
		int n = 0;
		for (int v = 1; v <= 9; v++)
			if (fits(g, i, v)) {
				m |= (uint16_t)(1 << v);
				n++;
			}
		if (n < best_n) {
			best_n = n;
			best = i;
			*cand = m;
			if (n <= 1)
				break;
		}
	}
	return best;
}

static int count(uint8_t g[81], int limit)
{
	uint16_t cand = 0;
	int i = best_empty(g, &cand);
	if (i < 0)
		return 1;
	int total = 0;
	for (int v = 1; v <= 9 && total < limit; v++)
		if (cand & (1 << v)) {
			g[i] = (uint8_t)v;
			total += count(g, limit - total);
			g[i] = 0;
		}
	return total;
}

int sd_count_solutions(const uint8_t grid[81], int limit)
{
	uint8_t g[81];
	memcpy(g, grid, sizeof g);
	return count(g, limit);
}

static bool fill(uint8_t g[81], Rng* rng)
{
	uint16_t cand = 0;
	int i = best_empty(g, &cand);
	if (i < 0)
		return true;
	int order[9];
	for (int k = 0; k < 9; k++)
		order[k] = k + 1;
	for (int k = 8; k > 0; k--) {
		int j = rng_range(rng, k + 1), t = order[k];
		order[k] = order[j];
		order[j] = t;
	}
	for (int k = 0; k < 9; k++) {
		int v = order[k];
		if (!(cand & (1 << v)))
			continue;
		g[i] = (uint8_t)v;
		if (fill(g, rng))
			return true;
		g[i] = 0;
	}
	return false;
}

void sd_generate(Sudoku* g, SdLevel level, uint32_t seed)
{
	memset(g, 0, sizeof *g);
	rng_seed(&g->rng, seed);
	g->level = level;
	fill(g->solution, &g->rng);
	memcpy(g->cell, g->solution, sizeof g->cell);
	/* Se quitan números (de dos en dos, simétricos) mientras la solución
	 * siga siendo única, hasta llegar a las pistas del nivel */
	static const int TARGET[SD_LEVELS] = { 40, 32, 26 };
	int order[81];
	for (int i = 0; i < 81; i++)
		order[i] = i;
	for (int i = 80; i > 0; i--) {
		int j = rng_range(&g->rng, i + 1), t = order[i];
		order[i] = order[j];
		order[j] = t;
	}
	int clues = 81;
	for (int k = 0; k < 81 && clues > TARGET[level]; k++) {
		int a = order[k], b = 80 - a;
		if (!g->cell[a])
			continue;
		uint8_t va = g->cell[a], vb = g->cell[b];
		g->cell[a] = g->cell[b] = 0;
		if (sd_count_solutions(g->cell, 2) == 1) {
			clues -= a == b ? 1 : 2;
		} else {
			g->cell[a] = va;
			g->cell[b] = vb;
		}
	}
	for (int i = 0; i < 81; i++)
		g->given[i] = g->cell[i] != 0;
}

bool sd_set(Sudoku* g, int i, int v)
{
	if (i < 0 || i >= 81 || g->given[i] || v < 0 || v > 9)
		return false;
	g->cell[i] = (uint8_t)v;
	g->notes[i] = 0;
	if (v && v != g->solution[i])
		g->mistakes++;
	/* Al poner un número se borra de las notas de sus vecinas */
	if (v) {
		int r = i / 9, c = i % 9, br = r / 3 * 3, bc = c / 3 * 3;
		for (int k = 0; k < 9; k++) {
			g->notes[r * 9 + k] &= (uint16_t)~(1 << v);
			g->notes[k * 9 + c] &= (uint16_t)~(1 << v);
			g->notes[(br + k / 3) * 9 + bc + k % 3] &= (uint16_t)~(1 << v);
		}
	}
	return true;
}

bool sd_toggle_note(Sudoku* g, int i, int v)
{
	if (i < 0 || i >= 81 || g->given[i] || g->cell[i] || v < 1 || v > 9)
		return false;
	g->notes[i] ^= (uint16_t)(1 << v);
	return true;
}

bool sd_conflict(const Sudoku* g, int i)
{
	int v = g->cell[i];
	if (!v)
		return false;
	int r = i / 9, c = i % 9, br = r / 3 * 3, bc = c / 3 * 3;
	for (int k = 0; k < 9; k++) {
		int a = r * 9 + k, b = k * 9 + c, d = (br + k / 3) * 9 + bc + k % 3;
		if ((a != i && g->cell[a] == v) || (b != i && g->cell[b] == v) || (d != i && g->cell[d] == v))
			return true;
	}
	return false;
}

bool sd_solved(const Sudoku* g)
{
	return memcmp(g->cell, g->solution, sizeof g->cell) == 0;
}

int sd_givens(const Sudoku* g)
{
	int n = 0;
	for (int i = 0; i < 81; i++)
		n += g->given[i];
	return n;
}

int sd_hint(Sudoku* g, int prefer)
{
	int i = prefer;
	if (i < 0 || i >= 81 || g->cell[i] == g->solution[i]) {
		i = -1;
		for (int k = 0; k < 81 && i < 0; k++)
			if (g->cell[k] != g->solution[k])
				i = k;
	}
	if (i < 0)
		return -1;
	int mistakes = g->mistakes;
	sd_set(g, i, g->solution[i]);
	g->mistakes = mistakes;
	return i;
}
