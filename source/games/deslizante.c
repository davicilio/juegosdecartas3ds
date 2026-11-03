#include "games/deslizante.h"

#include <string.h>

int dz_blank(const Deslizante* g)
{
	for (int i = 0; i < g->n * g->n; i++)
		if (!g->cell[i])
			return i;
	return -1;
}

bool dz_solved(const Deslizante* g)
{
	int t = g->n * g->n;
	for (int i = 0; i < t - 1; i++)
		if (g->cell[i] != i + 1)
			return false;
	return true;
}

bool dz_slide(Deslizante* g, int i)
{
	int n = g->n, b = dz_blank(g);
	if (i < 0 || i >= n * n || i == b)
		return false;
	int ir = i / n, ic = i % n, br = b / n, bc = b % n;
	if (ir != br && ic != bc)
		return false;
	int step = ir == br ? (ic < bc ? -1 : 1) : (ir < br ? -n : n);
	/* Desde el hueco hacia la ficha tocada, cada una ocupa el sitio de la anterior */
	for (int k = b; k != i; k += step)
		g->cell[k] = g->cell[k + step];
	g->cell[i] = 0;
	g->moves++;
	return true;
}

void dz_init(Deslizante* g, int n, uint32_t seed)
{
	memset(g, 0, sizeof *g);
	rng_seed(&g->rng, seed);
	g->n = n < 3 ? 3 : n > DZ_MAX ? DZ_MAX : n;
	int t = g->n * g->n;
	for (int i = 0; i < t - 1; i++)
		g->cell[i] = i + 1;
	g->cell[t - 1] = 0;
	/* Desordenar con movimientos de verdad (sin deshacer el anterior) */
	int prev = -1;
	for (int k = 0; k < 300 * g->n || dz_solved(g); k++) {
		int b = dz_blank(g), cand[4], m = 0;
		int r = b / g->n, c = b % g->n;
		if (r > 0)
			cand[m++] = b - g->n;
		if (r < g->n - 1)
			cand[m++] = b + g->n;
		if (c > 0)
			cand[m++] = b - 1;
		if (c < g->n - 1)
			cand[m++] = b + 1;
		int pick;
		do
			pick = cand[rng_range(&g->rng, m)];
		while (pick == prev && m > 1);
		prev = b;
		dz_slide(g, pick);
	}
	g->moves = 0;
}
