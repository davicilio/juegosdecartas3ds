#include "games/mahjong.h"

#include <string.h>

/* La tortuga: 87 fichas en el piso de abajo, luego 36, 16, 4 y 1 */
const MjPos MJ_LAYOUT[MJ_TILES] = {
	{ 2, 0, 0 }, { 4, 0, 0 }, { 6, 0, 0 }, { 8, 0, 0 }, { 10, 0, 0 }, { 12, 0, 0 },
	{ 14, 0, 0 }, { 16, 0, 0 }, { 18, 0, 0 }, { 20, 0, 0 }, { 22, 0, 0 }, { 24, 0, 0 },
	{ 6, 2, 0 }, { 8, 2, 0 }, { 10, 2, 0 }, { 12, 2, 0 }, { 14, 2, 0 }, { 16, 2, 0 },
	{ 18, 2, 0 }, { 20, 2, 0 }, { 4, 4, 0 }, { 6, 4, 0 }, { 8, 4, 0 }, { 10, 4, 0 },
	{ 12, 4, 0 }, { 14, 4, 0 }, { 16, 4, 0 }, { 18, 4, 0 }, { 20, 4, 0 }, { 22, 4, 0 },
	{ 2, 6, 0 }, { 4, 6, 0 }, { 6, 6, 0 }, { 8, 6, 0 }, { 10, 6, 0 }, { 12, 6, 0 },
	{ 14, 6, 0 }, { 16, 6, 0 }, { 18, 6, 0 }, { 20, 6, 0 }, { 22, 6, 0 }, { 24, 6, 0 },
	{ 2, 8, 0 }, { 4, 8, 0 }, { 6, 8, 0 }, { 8, 8, 0 }, { 10, 8, 0 }, { 12, 8, 0 },
	{ 14, 8, 0 }, { 16, 8, 0 }, { 18, 8, 0 }, { 20, 8, 0 }, { 22, 8, 0 }, { 24, 8, 0 },
	{ 4, 10, 0 }, { 6, 10, 0 }, { 8, 10, 0 }, { 10, 10, 0 }, { 12, 10, 0 }, { 14, 10, 0 },
	{ 16, 10, 0 }, { 18, 10, 0 }, { 20, 10, 0 }, { 22, 10, 0 }, { 6, 12, 0 }, { 8, 12, 0 },
	{ 10, 12, 0 }, { 12, 12, 0 }, { 14, 12, 0 }, { 16, 12, 0 }, { 18, 12, 0 }, { 20, 12, 0 },
	{ 2, 14, 0 }, { 4, 14, 0 }, { 6, 14, 0 }, { 8, 14, 0 }, { 10, 14, 0 }, { 12, 14, 0 },
	{ 14, 14, 0 }, { 16, 14, 0 }, { 18, 14, 0 }, { 20, 14, 0 }, { 22, 14, 0 }, { 24, 14, 0 },
	{ 0, 7, 0 }, { 26, 7, 0 }, { 28, 7, 0 }, { 8, 2, 1 }, { 10, 2, 1 }, { 12, 2, 1 },
	{ 14, 2, 1 }, { 16, 2, 1 }, { 18, 2, 1 }, { 8, 4, 1 }, { 10, 4, 1 }, { 12, 4, 1 },
	{ 14, 4, 1 }, { 16, 4, 1 }, { 18, 4, 1 }, { 8, 6, 1 }, { 10, 6, 1 }, { 12, 6, 1 },
	{ 14, 6, 1 }, { 16, 6, 1 }, { 18, 6, 1 }, { 8, 8, 1 }, { 10, 8, 1 }, { 12, 8, 1 },
	{ 14, 8, 1 }, { 16, 8, 1 }, { 18, 8, 1 }, { 8, 10, 1 }, { 10, 10, 1 }, { 12, 10, 1 },
	{ 14, 10, 1 }, { 16, 10, 1 }, { 18, 10, 1 }, { 8, 12, 1 }, { 10, 12, 1 }, { 12, 12, 1 },
	{ 14, 12, 1 }, { 16, 12, 1 }, { 18, 12, 1 }, { 10, 4, 2 }, { 12, 4, 2 }, { 14, 4, 2 },
	{ 16, 4, 2 }, { 10, 6, 2 }, { 12, 6, 2 }, { 14, 6, 2 }, { 16, 6, 2 }, { 10, 8, 2 },
	{ 12, 8, 2 }, { 14, 8, 2 }, { 16, 8, 2 }, { 10, 10, 2 }, { 12, 10, 2 }, { 14, 10, 2 },
	{ 16, 10, 2 }, { 12, 6, 3 }, { 14, 6, 3 }, { 12, 8, 3 }, { 14, 8, 3 }, { 13, 7, 4 },
};

bool mj_match(int a, int b)
{
	if (a >= MJ_SEASONS)
		return b >= MJ_SEASONS;
	if (a >= MJ_FLOWERS)
		return b >= MJ_FLOWERS && b < MJ_SEASONS;
	return a == b;
}

/* ¿Tapa t a u (está encima y se solapan)? */
static bool overlaps(int t, int u)
{
	int dx = MJ_LAYOUT[t].x - MJ_LAYOUT[u].x, dy = MJ_LAYOUT[t].y - MJ_LAYOUT[u].y;
	return dx > -2 && dx < 2 && dy > -2 && dy < 2;
}

static bool free_in(const bool* present, int t)
{
	bool left = false, right = false;
	for (int u = 0; u < MJ_TILES; u++) {
		if (!present[u] || u == t)
			continue;
		int dz = MJ_LAYOUT[u].z - MJ_LAYOUT[t].z;
		if (dz > 0 && overlaps(t, u))
			return false;
		if (dz == 0) {
			int dx = MJ_LAYOUT[u].x - MJ_LAYOUT[t].x, dy = MJ_LAYOUT[u].y - MJ_LAYOUT[t].y;
			if (dy > -2 && dy < 2) {
				if (dx == -2)
					left = true;
				if (dx == 2)
					right = true;
			}
		}
	}
	return !left || !right;
}

bool mj_free(const Mahjong* g, int t)
{
	return t >= 0 && t < MJ_TILES && g->present[t] && free_in(g->present, t);
}

/* Reparte caras en las posiciones presentes de forma que se pueda resolver:
 * se van quitando parejas de fichas libres al azar y a cada pareja se le da
 * una pareja de caras. Si se atasca, se vuelve a probar. */
static bool deal(Mahjong* g, const uint8_t* faces, int n)
{
	for (int attempt = 0; attempt < 200; attempt++) {
		/* Parejas de caras barajadas */
		int pairs = n / 2;
		uint8_t pf[MJ_TILES][2];
		for (int i = 0; i < pairs; i++) {
			pf[i][0] = faces[i * 2];
			pf[i][1] = faces[i * 2 + 1];
		}
		for (int i = pairs - 1; i > 0; i--) {
			int j = rng_range(&g->rng, i + 1);
			uint8_t a = pf[i][0], b = pf[i][1];
			pf[i][0] = pf[j][0];
			pf[i][1] = pf[j][1];
			pf[j][0] = a;
			pf[j][1] = b;
		}
		bool present[MJ_TILES];
		memcpy(present, g->present, sizeof present);
		bool ok = true;
		for (int k = 0; k < pairs && ok; k++) {
			int list[MJ_TILES], m = 0;
			for (int t = 0; t < MJ_TILES; t++)
				if (present[t] && free_in(present, t))
					list[m++] = t;
			if (m < 2) {
				ok = false;
				break;
			}
			int i = rng_range(&g->rng, m);
			int j = rng_range(&g->rng, m - 1);
			if (j >= i)
				j++;
			int a = list[i], b = list[j];
			g->face[a] = pf[k][0];
			g->face[b] = pf[k][1];
			present[a] = present[b] = false;
		}
		if (ok)
			return true;
	}
	return false;
}

void mj_init(Mahjong* g, uint32_t seed)
{
	memset(g, 0, sizeof *g);
	rng_seed(&g->rng, seed);
	mj_new_game(g);
}

void mj_new_game(Mahjong* g)
{
	/* Las caras, ya emparejadas de dos en dos */
	uint8_t faces[MJ_TILES];
	int n = 0;
	for (int k = 0; k < MJ_FLOWERS; k++)
		for (int c = 0; c < 4; c++)
			faces[n++] = (uint8_t)k;
	for (int k = MJ_FLOWERS; k < MJ_KINDS; k++)
		faces[n++] = (uint8_t)k;
	for (int t = 0; t < MJ_TILES; t++)
		g->present[t] = true;
	g->left = MJ_TILES;
	g->undo_n = 0;
	g->shuffles = 0;
	deal(g, faces, n);
}

bool mj_remove(Mahjong* g, int a, int b)
{
	if (a == b || !mj_free(g, a) || !mj_free(g, b) || !mj_match(g->face[a], g->face[b]))
		return false;
	g->present[a] = g->present[b] = false;
	g->left -= 2;
	g->undo[g->undo_n++] = a;
	g->undo[g->undo_n++] = b;
	return true;
}

bool mj_undo(Mahjong* g)
{
	if (g->undo_n < 2)
		return false;
	g->present[g->undo[--g->undo_n]] = true;
	g->present[g->undo[--g->undo_n]] = true;
	g->left += 2;
	return true;
}

bool mj_hint(const Mahjong* g, int* a, int* b)
{
	int list[MJ_TILES], m = 0;
	for (int t = 0; t < MJ_TILES; t++)
		if (mj_free(g, t))
			list[m++] = t;
	for (int i = 0; i < m; i++)
		for (int j = i + 1; j < m; j++)
			if (mj_match(g->face[list[i]], g->face[list[j]])) {
				*a = list[i];
				*b = list[j];
				return true;
			}
	return false;
}

bool mj_shuffle(Mahjong* g)
{
	/* Junta las caras que quedan por parejas que emparejan */
	uint8_t faces[MJ_TILES];
	bool used[MJ_TILES] = { false };
	int n = 0;
	for (int t = 0; t < MJ_TILES; t++) {
		if (!g->present[t] || used[t])
			continue;
		for (int u = t + 1; u < MJ_TILES; u++)
			if (g->present[u] && !used[u] && mj_match(g->face[t], g->face[u])) {
				used[t] = used[u] = true;
				faces[n++] = g->face[t];
				faces[n++] = g->face[u];
				break;
			}
	}
	if (n != g->left || !deal(g, faces, n))
		return false;
	g->shuffles++;
	g->undo_n = 0; /* después de barajar ya no se deshace */
	return true;
}
