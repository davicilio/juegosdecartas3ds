#include "games/conquista.h"

#include <string.h>

/* Provincias (capital aproximada; Canarias va en un recuadro al suroeste),
 * comunidad autónoma y vecinas. Baleares se une por mar con Barcelona y
 * Valencia, y Canarias con Cádiz y Huelva. */
const CnProvince CN_MAP[CN_PROVINCES] = {
	{ "Almería", 0, { 3, 44, -1, -1, -1, -1, -1, -1 }, -2.46f, 36.84f },
	{ "Cádiz", 0, { 7, 6, 13, -1, -1, -1, -1, -1 }, -6.00f, 36.53f },
	{ "Córdoba", 0, { 37, 17, 5, 3, 6, 7, -1, -1 }, -4.78f, 37.88f },
	{ "Granada", 0, { 5, 2, 6, 0, 44, 16, -1, -1 }, -3.60f, 37.18f },
	{ "Huelva", 0, { 37, 7, 14, -1, -1, -1, -1, -1 }, -6.95f, 37.40f },
	{ "Jaén", 0, { 2, 17, 16, 3, -1, -1, -1, -1 }, -3.60f, 38.05f },
	{ "Málaga", 0, { 1, 7, 2, 3, -1, -1, -1, -1 }, -4.62f, 36.72f },
	{ "Sevilla", 0, { 4, 37, 2, 6, 1, -1, -1, -1 }, -5.85f, 37.39f },
	{ "Huesca", 1, { 45, 10, 32, -1, -1, -1, -1, -1 }, -0.30f, 42.14f },
	{ "Teruel", 1, { 35, 36, 18, 19, 10, 33, -1, -1 }, -1.11f, 40.34f },
	{ "Zaragoza", 1, { 9, 19, 27, 49, 45, 8, 32, 33 }, -1.00f, 41.55f },
	{ "Asturias", 2, { 40, 23, 15, -1, -1, -1, -1, -1 }, -5.85f, 43.36f },
	{ "Baleares", 3, { 30, 36, -1, -1, -1, -1, -1, -1 }, 2.65f, 39.57f },
	{ "Las Palmas", 4, { 14, 1, -1, -1, -1, -1, -1, -1 }, -8.00f, 36.00f },
	{ "Santa Cruz de Tenerife", 4, { 13, 4, -1, -1, -1, -1, -1, -1 }, -9.00f, 36.10f },
	{ "Cantabria", 5, { 11, 23, 24, 22, 48, -1, -1, -1 }, -4.15f, 43.30f },
	{ "Albacete", 6, { 18, 17, 5, 3, 44, 34, 36, -1 }, -1.86f, 38.99f },
	{ "Ciudad Real", 6, { 20, 18, 16, 5, 2, 37, -1, -1 }, -3.93f, 38.98f },
	{ "Cuenca", 6, { 19, 43, 20, 17, 16, 36, 9, -1 }, -2.13f, 40.07f },
	{ "Guadalajara", 6, { 27, 26, 43, 18, 9, 10, -1, -1 }, -2.80f, 40.80f },
	{ "Toledo", 6, { 43, 21, 38, 37, 17, 18, -1, -1 }, -4.33f, 39.86f },
	{ "Ávila", 7, { 26, 43, 20, 38, 25, 28, -1, -1 }, -5.00f, 40.56f },
	{ "Burgos", 7, { 15, 48, 46, 49, 27, 26, 28, 24 }, -3.70f, 42.34f },
	{ "León", 7, { 11, 40, 41, 29, 28, 24, 15, -1 }, -5.57f, 42.60f },
	{ "Palencia", 7, { 23, 15, 22, 28, -1, -1, -1, -1 }, -4.45f, 42.40f },
	{ "Salamanca", 7, { 29, 28, 21, 38, -1, -1, -1, -1 }, -6.10f, 40.97f },
	{ "Segovia", 7, { 22, 27, 19, 43, 21, 28, -1, -1 }, -4.00f, 41.15f },
	{ "Soria", 7, { 22, 49, 10, 19, 26, -1, -1, -1 }, -2.55f, 41.45f },
	{ "Valladolid", 7, { 23, 24, 22, 26, 21, 25, 29, -1 }, -4.85f, 41.65f },
	{ "Zamora", 7, { 41, 23, 28, 25, -1, -1, -1, -1 }, -5.90f, 41.70f },
	{ "Barcelona", 8, { 32, 33, 31, 12, -1, -1, -1, -1 }, 2.10f, 41.50f },
	{ "Girona", 8, { 30, 32, -1, -1, -1, -1, -1, -1 }, 2.75f, 42.10f },
	{ "Lleida", 8, { 8, 10, 33, 30, 31, -1, -1, -1 }, 0.85f, 41.80f },
	{ "Tarragona", 8, { 32, 10, 9, 35, 30, -1, -1, -1 }, 0.90f, 41.00f },
	{ "Alicante", 9, { 44, 16, 36, -1, -1, -1, -1, -1 }, -0.48f, 38.40f },
	{ "Castellón", 9, { 36, 9, 33, -1, -1, -1, -1, -1 }, -0.05f, 40.10f },
	{ "Valencia", 9, { 34, 16, 18, 9, 35, 12, -1, -1 }, -0.50f, 39.40f },
	{ "Badajoz", 10, { 38, 20, 17, 2, 7, 4, -1, -1 }, -6.40f, 38.80f },
	{ "Cáceres", 10, { 25, 21, 20, 37, -1, -1, -1, -1 }, -6.15f, 39.65f },
	{ "A Coruña", 11, { 40, 42, -1, -1, -1, -1, -1, -1 }, -8.41f, 43.30f },
	{ "Lugo", 11, { 39, 42, 41, 11, 23, -1, -1, -1 }, -7.40f, 43.01f },
	{ "Ourense", 11, { 40, 42, 23, 29, -1, -1, -1, -1 }, -7.70f, 42.20f },
	{ "Pontevedra", 11, { 39, 40, 41, -1, -1, -1, -1, -1 }, -8.64f, 42.43f },
	{ "Madrid", 12, { 26, 21, 20, 18, 19, -1, -1, -1 }, -3.55f, 40.42f },
	{ "Murcia", 13, { 0, 3, 16, 34, -1, -1, -1, -1 }, -1.20f, 37.98f },
	{ "Navarra", 14, { 47, 46, 49, 10, 8, -1, -1, -1 }, -1.35f, 42.70f },
	{ "Álava", 15, { 48, 47, 45, 49, 22, -1, -1, -1 }, -2.60f, 42.70f },
	{ "Gipuzkoa", 15, { 48, 46, 45, -1, -1, -1, -1, -1 }, -1.75f, 43.30f },
	{ "Bizkaia", 15, { 15, 22, 46, 47, -1, -1, -1, -1 }, -3.05f, 43.40f },
	{ "La Rioja", 16, { 46, 45, 10, 27, 22, -1, -1, -1 }, -2.05f, 42.10f },
};

const CnRegion CN_REGION[CN_REGIONS] = {
	{ "Andalucía", 5 },
	{ "Aragón", 2 },
	{ "Asturias", 1 },
	{ "Baleares", 1 },
	{ "Canarias", 1 },
	{ "Cantabria", 1 },
	{ "Castilla-La Mancha", 3 },
	{ "Castilla y León", 5 },
	{ "Cataluña", 3 },
	{ "Comunidad Valenciana", 2 },
	{ "Extremadura", 2 },
	{ "Galicia", 2 },
	{ "Madrid", 1 },
	{ "Murcia", 1 },
	{ "Navarra", 1 },
	{ "País Vasco", 2 },
	{ "La Rioja", 1 },
};

bool cn_adjacent(int a, int b)
{
	for (int k = 0; k < CN_MAX_ADJ && CN_MAP[a].adj[k] >= 0; k++)
		if (CN_MAP[a].adj[k] == b)
			return true;
	return false;
}

int cn_provinces_of(const Conquista* g, int p)
{
	int n = 0;
	for (int i = 0; i < CN_PROVINCES; i++)
		n += g->owner[i] == p;
	return n;
}

bool cn_region_owned(const Conquista* g, int region, int p)
{
	for (int i = 0; i < CN_PROVINCES; i++)
		if (CN_MAP[i].region == region && g->owner[i] != p)
			return false;
	return true;
}

int cn_reinforcements(const Conquista* g, int p)
{
	int n = cn_provinces_of(g, p) / 3;
	if (n < 3)
		n = 3;
	for (int r = 0; r < CN_REGIONS; r++)
		if (cn_region_owned(g, r, p))
			n += CN_REGION[r].bonus;
	return n;
}

static void start_turn(Conquista* g)
{
	g->phase = CN_REINFORCE;
	g->to_place = cn_reinforcements(g, g->current);
}

void cn_init(Conquista* g, int players, uint32_t seed)
{
	memset(g, 0, sizeof *g);
	rng_seed(&g->rng, seed);
	g->players = players < 2 ? 2 : players > CN_PLAYERS ? CN_PLAYERS : players;
	g->winner = -1;
	/* Reparto de provincias al azar */
	int order[CN_PROVINCES];
	for (int i = 0; i < CN_PROVINCES; i++)
		order[i] = i;
	for (int i = CN_PROVINCES - 1; i > 0; i--) {
		int j = rng_range(&g->rng, i + 1), t = order[i];
		order[i] = order[j];
		order[j] = t;
	}
	for (int i = 0; i < CN_PROVINCES; i++) {
		g->owner[order[i]] = i % g->players;
		g->troops[order[i]] = 1;
	}
	for (int p = g->players; p < CN_PLAYERS; p++)
		g->out[p] = true;
	/* Tropas iniciales que se colocan solas */
	static const int START[CN_PLAYERS + 1] = { 0, 0, 45, 35, 30 };
	for (int p = 0; p < g->players; p++) {
		int extra = START[g->players] - cn_provinces_of(g, p);
		while (extra-- > 0) {
			int i;
			do
				i = rng_range(&g->rng, CN_PROVINCES);
			while (g->owner[i] != p);
			g->troops[i]++;
		}
	}
	g->current = 0;
	start_turn(g);
}

bool cn_place(Conquista* g, int prov, int n)
{
	if (g->phase != CN_REINFORCE || prov < 0 || prov >= CN_PROVINCES || g->owner[prov] != g->current || n < 1)
		return false;
	if (n > g->to_place)
		n = g->to_place;
	g->troops[prov] += n;
	g->to_place -= n;
	if (g->to_place == 0)
		g->phase = CN_ATTACK;
	return true;
}

bool cn_can_attack(const Conquista* g, int from, int to)
{
	return g->phase == CN_ATTACK && from >= 0 && to >= 0 && from < CN_PROVINCES && to < CN_PROVINCES &&
		g->owner[from] == g->current && g->owner[to] != g->current && g->troops[from] >= 2 && cn_adjacent(from, to);
}

static void finish(Conquista* g)
{
	int best = -1;
	for (int p = 0; p < g->players; p++) {
		if (g->out[p])
			continue;
		if (best < 0 || cn_provinces_of(g, p) > cn_provinces_of(g, best))
			best = p;
	}
	g->winner = best;
	g->phase = CN_OVER;
}

bool cn_attack_round(Conquista* g, int from, int to)
{
	if (!cn_can_attack(g, from, to))
		return false;
	g->conquered = false;
	g->die_a = 1 + rng_range(&g->rng, 6);
	g->die_d = 1 + rng_range(&g->rng, 6);
	int bonus = g->troops[from] - 1 > g->troops[to] ? 1 : 0;
	g->attacker_won_round = g->die_a + bonus > g->die_d;
	if (g->attacker_won_round)
		g->troops[to]--;
	else
		g->troops[from]--;
	if (g->troops[to] == 0) {
		int loser = g->owner[to];
		int move = g->troops[from] / 2;
		if (move < 1)
			move = 1;
		g->owner[to] = g->current;
		g->troops[to] = move;
		g->troops[from] -= move;
		g->conquered = true;
		if (cn_provinces_of(g, loser) == 0)
			g->out[loser] = true;
		if (cn_provinces_of(g, g->current) == CN_PROVINCES || g->out[0])
			finish(g);
	}
	return true;
}

bool cn_attack_all(Conquista* g, int from, int to)
{
	if (!cn_can_attack(g, from, to))
		return false;
	while (cn_can_attack(g, from, to))
		cn_attack_round(g, from, to);
	return true;
}

void cn_end_attacks(Conquista* g)
{
	if (g->phase == CN_ATTACK)
		g->phase = CN_FORTIFY;
}

bool cn_can_move(const Conquista* g, int from, int to)
{
	return g->phase == CN_FORTIFY && from >= 0 && to >= 0 && from < CN_PROVINCES && to < CN_PROVINCES && from != to &&
		g->owner[from] == g->current && g->owner[to] == g->current && g->troops[from] >= 2 && cn_adjacent(from, to);
}

bool cn_move(Conquista* g, int from, int to, int n)
{
	if (!cn_can_move(g, from, to) || n < 1)
		return false;
	if (n > g->troops[from] - 1)
		n = g->troops[from] - 1;
	g->troops[from] -= n;
	g->troops[to] += n;
	cn_end_turn(g);
	return true;
}

void cn_end_turn(Conquista* g)
{
	if (g->phase == CN_OVER)
		return;
	int p = g->current;
	do {
		p = (p + 1) % g->players;
		if (p == 0)
			g->round++;
	} while (g->out[p]);
	g->current = p;
	if (g->round >= CN_MAX_ROUNDS) {
		finish(g);
		return;
	}
	start_turn(g);
}

/* ---------------------------------------------------------------------------
 * IA
 * ------------------------------------------------------------------------- */

static int enemy_neighbors(const Conquista* g, int prov, int* weakest)
{
	int n = 0, w = 999;
	for (int k = 0; k < CN_MAX_ADJ && CN_MAP[prov].adj[k] >= 0; k++) {
		int b = CN_MAP[prov].adj[k];
		if (g->owner[b] != g->owner[prov]) {
			n++;
			if (g->troops[b] < w)
				w = g->troops[b];
		}
	}
	if (weakest)
		*weakest = w;
	return n;
}

/* Cuántas provincias de su comunidad le faltan a p */
static int region_missing(const Conquista* g, int region, int p)
{
	int n = 0;
	for (int i = 0; i < CN_PROVINCES; i++)
		if (CN_MAP[i].region == region && g->owner[i] != p)
			n++;
	return n;
}

bool cn_ai_step(Conquista* g)
{
	int p = g->current;
	switch (g->phase) {
	case CN_REINFORCE: {
		/* A la frontera con más posibilidades: enemigos débiles y comunidades a medias */
		int best = -1, best_sc = -9999;
		for (int i = 0; i < CN_PROVINCES; i++) {
			if (g->owner[i] != p)
				continue;
			int weakest, enemies = enemy_neighbors(g, i, &weakest);
			if (!enemies)
				continue;
			int sc = 20 - weakest * 2 + g->troops[i] - region_missing(g, CN_MAP[i].region, p) * 2 +
				rng_range(&g->rng, 4);
			if (sc > best_sc) {
				best_sc = sc;
				best = i;
			}
		}
		if (best < 0)
			for (int i = 0; i < CN_PROVINCES && best < 0; i++)
				if (g->owner[i] == p)
					best = i;
		cn_place(g, best, g->to_place);
		return true;
	}
	case CN_ATTACK: {
		int bf = -1, bt = -1, best_sc = -9999;
		for (int f = 0; f < CN_PROVINCES; f++) {
			if (g->owner[f] != p || g->troops[f] < 3)
				continue;
			for (int k = 0; k < CN_MAX_ADJ && CN_MAP[f].adj[k] >= 0; k++) {
				int t = CN_MAP[f].adj[k];
				if (g->owner[t] == p)
					continue;
				int margin = g->troops[f] - 1 - g->troops[t];
				if (margin < 2)
					continue;
				int sc = margin * 3 - region_missing(g, CN_MAP[t].region, p) * 2 + (CN_MAP[t].region == CN_MAP[f].region ? 3 : 0);
				if (sc > best_sc) {
					best_sc = sc;
					bf = f;
					bt = t;
				}
			}
		}
		if (bf < 0) {
			cn_end_attacks(g);
			return true;
		}
		cn_attack_all(g, bf, bt);
		return g->phase != CN_OVER;
	}
	case CN_FORTIFY: {
		/* Tropas del interior hacia la frontera */
		int bf = -1, bt = -1, most = 1;
		for (int f = 0; f < CN_PROVINCES; f++) {
			if (g->owner[f] != p || g->troops[f] <= most || enemy_neighbors(g, f, NULL))
				continue;
			for (int k = 0; k < CN_MAX_ADJ && CN_MAP[f].adj[k] >= 0; k++) {
				int t = CN_MAP[f].adj[k];
				if (g->owner[t] == p && enemy_neighbors(g, t, NULL)) {
					most = g->troops[f];
					bf = f;
					bt = t;
					break;
				}
			}
		}
		if (bf >= 0)
			cn_move(g, bf, bt, g->troops[bf] - 1);
		else
			cn_end_turn(g);
		return false;
	}
	default:
		return false;
	}
}
