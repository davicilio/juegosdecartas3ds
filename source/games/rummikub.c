#include "games/rummikub.h"

#include <string.h>

static bool is_joker(RkTile t)
{
	return t.color == RK_JOKER;
}

/* Escalera: mismo color, números distintos; los comodines tapan huecos y,
 * si sobran, alargan por arriba (o por abajo si no caben). */
static bool make_run(const RkTile* tiles, int n, RkSet* out)
{
	int jokers = 0, color = -1;
	bool used[14] = { false };
	int lo = 14, hi = 0;
	for (int i = 0; i < n; i++) {
		if (is_joker(tiles[i])) {
			jokers++;
			continue;
		}
		if (color >= 0 && tiles[i].color != color)
			return false;
		color = tiles[i].color;
		if (used[tiles[i].number])
			return false;
		used[tiles[i].number] = true;
		if (tiles[i].number < lo)
			lo = tiles[i].number;
		if (tiles[i].number > hi)
			hi = tiles[i].number;
	}
	if (color < 0) {
		/* Solo comodines: no se puede saber qué son */
		return false;
	}
	int gaps = (hi - lo + 1) - (n - jokers);
	if (gaps > jokers || n > 13)
		return false;
	int extra = jokers - gaps;
	int up = hi + extra <= 13 ? extra : 13 - hi;
	int down = extra - up;
	if (lo - down < 1)
		return false;
	lo -= down;
	hi += up;
	out->n = 0;
	for (int v = lo; v <= hi; v++)
		out->tiles[out->n++] = used[v] ? (RkTile){ (uint8_t)color, (uint8_t)v } : (RkTile){ RK_JOKER, 0 };
	return out->n == n;
}

/* Grupo: el mismo número en colores distintos (3 o 4 fichas) */
static bool make_group(const RkTile* tiles, int n, RkSet* out)
{
	if (n > 4)
		return false;
	int number = 0;
	bool colors[RK_COLORS] = { false };
	for (int i = 0; i < n; i++) {
		if (is_joker(tiles[i]))
			continue;
		if (number && tiles[i].number != number)
			return false;
		number = tiles[i].number;
		if (colors[tiles[i].color])
			return false;
		colors[tiles[i].color] = true;
	}
	if (!number)
		return false;
	/* Primero las fichas por color y los comodines al final */
	out->n = 0;
	for (int c = 0; c < RK_COLORS; c++)
		if (colors[c])
			out->tiles[out->n++] = (RkTile){ (uint8_t)c, (uint8_t)number };
	while (out->n < n)
		out->tiles[out->n++] = (RkTile){ RK_JOKER, 0 };
	return true;
}

bool rk_make_set(const RkTile* tiles, int n, RkSet* out)
{
	if (n < 3 || n > RK_MAX_SET)
		return false;
	return make_group(tiles, n, out) || make_run(tiles, n, out);
}

/* Puntos de una jugada: en un grupo, el número por las fichas; en una
 * escalera, la suma de los números que ocupa (el comodín vale lo que tapa) */
int rk_set_points(const RkSet* s)
{
	int first = -1;
	bool group = s->n <= 4;
	for (int i = 0; i < s->n; i++) {
		if (is_joker(s->tiles[i]))
			continue;
		if (first < 0)
			first = i;
		else if (s->tiles[i].number != s->tiles[first].number)
			group = false;
	}
	if (first < 0)
		return 0;
	if (group)
		return s->tiles[first].number * s->n;
	int start = s->tiles[first].number - first;
	int total = 0;
	for (int i = 0; i < s->n; i++)
		total += start + i;
	return total;
}

int rk_tile_penalty(RkTile t)
{
	return is_joker(t) ? RK_JOKER_PENALTY : t.number;
}

void rk_init(Rummikub* g, uint32_t seed)
{
	rng_seed(&g->rng, seed);
	for (int p = 0; p < RK_PLAYERS; p++)
		g->score[p] = 0;
	g->rounds = 0;
	g->current = RK_PLAYERS - 1;
	rk_new_round(g);
}

static void save_turn(Rummikub* g)
{
	memcpy(g->saved_table, g->table, sizeof g->table);
	g->saved_table_count = g->table_count;
	int p = g->current;
	memcpy(g->saved_rack, g->racks[p], sizeof(RkTile) * g->rack_count[p]);
	g->saved_rack_count = g->rack_count[p];
	g->turn_points = 0;
	g->turn_tiles = 0;
	g->last_changed = -1;
}

void rk_new_round(Rummikub* g)
{
	g->pool_count = 0;
	for (int k = 0; k < 2; k++)
		for (int c = 0; c < RK_COLORS; c++)
			for (int v = 1; v <= 13; v++)
				g->pool[g->pool_count++] = (RkTile){ (uint8_t)c, (uint8_t)v };
	g->pool[g->pool_count++] = (RkTile){ RK_JOKER, 0 };
	g->pool[g->pool_count++] = (RkTile){ RK_JOKER, 0 };
	for (int i = g->pool_count - 1; i > 0; i--) {
		int j = rng_range(&g->rng, i + 1);
		RkTile t = g->pool[i];
		g->pool[i] = g->pool[j];
		g->pool[j] = t;
	}
	for (int p = 0; p < RK_PLAYERS; p++) {
		g->rack_count[p] = 0;
		g->melded[p] = false;
		for (int k = 0; k < RK_START; k++)
			g->racks[p][g->rack_count[p]++] = g->pool[--g->pool_count];
	}
	g->table_count = 0;
	g->idle_turns = 0;
	g->current = (g->current + 1) % RK_PLAYERS;
	g->round_winner = -1;
	g->round_points = 0;
	g->phase = RK_PHASE_PLAYING;
	rk_sort_rack(g, 0, false);
	save_turn(g);
}

static void remove_indices(Rummikub* g, int p, const int* idx, int n)
{
	bool gone[RK_MAX_RACK] = { false };
	for (int i = 0; i < n; i++)
		gone[idx[i]] = true;
	int k = 0;
	for (int i = 0; i < g->rack_count[p]; i++)
		if (!gone[i])
			g->racks[p][k++] = g->racks[p][i];
	g->rack_count[p] = k;
}

bool rk_play_set(Rummikub* g, const int* idx, int n)
{
	int p = g->current;
	if (g->phase != RK_PHASE_PLAYING || n < 3 || n > RK_MAX_SET || g->table_count >= RK_MAX_SETS)
		return false;
	RkTile tiles[RK_MAX_SET];
	for (int i = 0; i < n; i++) {
		if (idx[i] < 0 || idx[i] >= g->rack_count[p])
			return false;
		for (int j = 0; j < i; j++)
			if (idx[j] == idx[i])
				return false;
		tiles[i] = g->racks[p][idx[i]];
	}
	RkSet s;
	if (!rk_make_set(tiles, n, &s))
		return false;
	g->table[g->table_count++] = s;
	g->last_changed = g->table_count - 1;
	g->turn_points += rk_set_points(&s);
	g->turn_tiles += n;
	remove_indices(g, p, idx, n);
	return true;
}

/* Las jugadas que ya estaban al empezar el turno solo se tocan tras la primera jugada */
static bool can_touch(const Rummikub* g, int set)
{
	int p = g->current;
	return g->melded[p] || set >= g->saved_table_count;
}

bool rk_can_add(const Rummikub* g, int set, int idx)
{
	int p = g->current;
	if (g->phase != RK_PHASE_PLAYING || set < 0 || set >= g->table_count || idx < 0 || idx >= g->rack_count[p] ||
		!can_touch(g, set))
		return false;
	const RkSet* s = &g->table[set];
	if (s->n >= RK_MAX_SET)
		return false;
	RkTile tiles[RK_MAX_SET];
	memcpy(tiles, s->tiles, sizeof(RkTile) * s->n);
	tiles[s->n] = g->racks[p][idx];
	RkSet out;
	return rk_make_set(tiles, s->n + 1, &out);
}

bool rk_add(Rummikub* g, int set, int idx)
{
	if (!rk_can_add(g, set, idx))
		return false;
	int p = g->current;
	RkSet* s = &g->table[set];
	RkTile tiles[RK_MAX_SET];
	memcpy(tiles, s->tiles, sizeof(RkTile) * s->n);
	tiles[s->n] = g->racks[p][idx];
	int before = rk_set_points(s);
	rk_make_set(tiles, s->n + 1, s);
	if (set >= g->saved_table_count)
		g->turn_points += rk_set_points(s) - before; /* jugada nueva de este turno */
	g->turn_tiles++;
	g->last_changed = set;
	remove_indices(g, p, &idx, 1);
	return true;
}

void rk_undo(Rummikub* g)
{
	int p = g->current;
	memcpy(g->table, g->saved_table, sizeof g->table);
	g->table_count = g->saved_table_count;
	memcpy(g->racks[p], g->saved_rack, sizeof(RkTile) * g->saved_rack_count);
	g->rack_count[p] = g->saved_rack_count;
	g->turn_points = 0;
	g->turn_tiles = 0;
	g->last_changed = -1;
}

bool rk_turn_valid(const Rummikub* g)
{
	if (g->turn_tiles == 0)
		return false;
	return g->melded[g->current] || g->turn_points >= RK_MIN_MELD;
}

bool rk_end_turn(Rummikub* g)
{
	if (g->phase != RK_PHASE_PLAYING)
		return false;
	int p = g->current;
	bool drew = false;
	if (rk_turn_valid(g)) {
		g->melded[p] = true;
	} else {
		rk_undo(g);
		if (g->pool_count > 0) {
			g->racks[p][g->rack_count[p]++] = g->pool[--g->pool_count];
			drew = true;
		}
	}
	if (g->rack_count[p] == 0) {
		int pts = 0;
		for (int q = 0; q < RK_PLAYERS; q++) {
			int own = 0;
			for (int i = 0; i < g->rack_count[q]; i++)
				own += rk_tile_penalty(g->racks[q][i]);
			g->score[q] -= own;
			pts += own;
		}
		g->score[p] += pts;
		g->round_winner = p;
		g->round_points = pts;
		g->rounds++;
		g->phase = RK_PHASE_ROUND_OVER;
		return drew;
	}
	/* Sin fichas en el pozo y una vuelta entera sin bajar nada: se acaba la
	 * ronda y gana quien tenga menos puntos en el atril */
	if (g->turn_tiles == 0 && !drew)
		g->idle_turns++;
	else
		g->idle_turns = 0;
	if (g->idle_turns >= RK_PLAYERS) {
		int best = 0, best_pen = 1 << 30;
		int pen[RK_PLAYERS];
		for (int q = 0; q < RK_PLAYERS; q++) {
			pen[q] = 0;
			for (int i = 0; i < g->rack_count[q]; i++)
				pen[q] += rk_tile_penalty(g->racks[q][i]);
			if (pen[q] < best_pen) {
				best_pen = pen[q];
				best = q;
			}
		}
		int pts = 0;
		for (int q = 0; q < RK_PLAYERS; q++)
			if (q != best) {
				g->score[q] -= pen[q];
				pts += pen[q];
			}
		g->score[best] += pts;
		g->round_winner = best;
		g->round_points = pts;
		g->rounds++;
		g->phase = RK_PHASE_ROUND_OVER;
		return drew;
	}
	g->current = (p + 1) % RK_PLAYERS;
	save_turn(g);
	return drew;
}

void rk_sort_rack(Rummikub* g, int player, bool by_number)
{
	RkTile* r = g->racks[player];
	int n = g->rack_count[player];
	for (int i = 1; i < n; i++) {
		RkTile t = r[i];
		int key = by_number ? t.number * 8 + t.color : t.color * 16 + t.number;
		int j = i - 1;
		while (j >= 0) {
			int kj = by_number ? r[j].number * 8 + r[j].color : r[j].color * 16 + r[j].number;
			if (kj <= key)
				break;
			r[j + 1] = r[j];
			j--;
		}
		r[j + 1] = t;
	}
}

/* ---------------------------------------------------------------------------
 * IA: busca jugadas en el atril y elige las que más fichas colocan.
 * ------------------------------------------------------------------------- */

#define RK_MAX_CAND 160

typedef struct {
	int idx[RK_MAX_SET];
	int n;
	int points;
} Cand;

static void add_cand(Cand* c, int* nc, const RkTile* rack, const int* idx, int n)
{
	if (*nc >= RK_MAX_CAND)
		return;
	RkTile tiles[RK_MAX_SET];
	for (int i = 0; i < n; i++)
		tiles[i] = rack[idx[i]];
	RkSet s;
	if (!rk_make_set(tiles, n, &s))
		return;
	Cand* k = &c[(*nc)++];
	memcpy(k->idx, idx, sizeof(int) * n);
	k->n = n;
	k->points = rk_set_points(&s);
}

static int find_candidates(const RkTile* rack, int n, Cand* c)
{
	int nc = 0;
	int jokers[2], nj = 0;
	for (int i = 0; i < n && nj < 2; i++)
		if (is_joker(rack[i]))
			jokers[nj++] = i;
	/* Grupos: un índice por color (de 3 o 4), con o sin comodín */
	for (int v = 1; v <= 13; v++) {
		int by_color[RK_COLORS];
		for (int col = 0; col < RK_COLORS; col++) {
			by_color[col] = -1;
			for (int i = 0; i < n; i++)
				if (rack[i].color == col && rack[i].number == v) {
					by_color[col] = i;
					break;
				}
		}
		int idx[4], m = 0;
		for (int col = 0; col < RK_COLORS; col++)
			if (by_color[col] >= 0)
				idx[m++] = by_color[col];
		if (m >= 3)
			add_cand(c, &nc, rack, idx, m);
		if (m == 4)
			for (int skip = 0; skip < 4; skip++) {
				int sub[3], s = 0;
				for (int q = 0; q < 4; q++)
					if (q != skip)
						sub[s++] = idx[q];
				add_cand(c, &nc, rack, sub, 3);
			}
		if (m == 2 && nj > 0) {
			idx[2] = jokers[0];
			add_cand(c, &nc, rack, idx, 3);
		}
	}
	/* Escaleras: tramos de números seguidos de cada color, con un comodín como mucho */
	for (int col = 0; col < RK_COLORS; col++) {
		int at[14];
		for (int v = 1; v <= 13; v++) {
			at[v] = -1;
			for (int i = 0; i < n; i++)
				if (rack[i].color == col && rack[i].number == v) {
					at[v] = i;
					break;
				}
		}
		for (int start = 1; start <= 13; start++)
			for (int len = 3; start + len - 1 <= 13; len++) {
				int idx[RK_MAX_SET], m = 0, missing = 0;
				for (int v = start; v < start + len; v++) {
					if (at[v] >= 0)
						idx[m++] = at[v];
					else
						missing++;
				}
				if (missing > nj || missing > 1)
					break;
				if (at[start] < 0 || at[start + len - 1] < 0)
					continue; /* los comodines solo dentro */
				for (int j = 0; j < missing; j++)
					idx[m++] = jokers[j];
				add_cand(c, &nc, rack, idx, m);
			}
	}
	return nc;
}

/* Elige candidatas sin fichas repetidas que coloquen el máximo de fichas */
static void best_combo(const Cand* c, int nc, int from, bool* used, int* chosen, int depth, int tiles, int points,
	int* best, int* best_n, int* best_tiles, int* best_points, int* budget)
{
	if (tiles > *best_tiles || (tiles == *best_tiles && points > *best_points)) {
		*best_tiles = tiles;
		*best_points = points;
		*best_n = depth;
		memcpy(best, chosen, sizeof(int) * depth);
	}
	for (int k = from; k < nc && *budget > 0; k++) {
		(*budget)--;
		bool ok = true;
		for (int i = 0; i < c[k].n && ok; i++)
			ok = !used[c[k].idx[i]];
		if (!ok)
			continue;
		for (int i = 0; i < c[k].n; i++)
			used[c[k].idx[i]] = true;
		chosen[depth] = k;
		best_combo(c, nc, k + 1, used, chosen, depth + 1, tiles + c[k].n, points + c[k].points, best, best_n,
			best_tiles, best_points, budget);
		for (int i = 0; i < c[k].n; i++)
			used[c[k].idx[i]] = false;
	}
}

int rk_ai_turn(Rummikub* g)
{
	int p = g->current;
	static Cand cand[RK_MAX_CAND];
	int nc = find_candidates(g->racks[p], g->rack_count[p], cand);
	bool used[RK_MAX_RACK] = { false };
	int chosen[RK_MAX_SETS], best[RK_MAX_SETS], best_n = 0, best_tiles = 0, best_points = 0;
	int budget = 20000;
	best_combo(cand, nc, 0, used, chosen, 0, 0, 0, best, &best_n, &best_tiles, &best_points, &budget);

	int placed = 0;
	if (best_n > 0 && (g->melded[p] || best_points >= RK_MIN_MELD)) {
		/* Bajar de la última a la primera no cambia los índices de las anteriores... */
		int idx_all[RK_MAX_SETS][RK_MAX_SET], ns[RK_MAX_SETS];
		RkTile want[RK_MAX_SETS][RK_MAX_SET];
		for (int k = 0; k < best_n; k++) {
			ns[k] = cand[best[k]].n;
			for (int i = 0; i < ns[k]; i++) {
				idx_all[k][i] = cand[best[k]].idx[i];
				want[k][i] = g->racks[p][idx_all[k][i]];
			}
		}
		/* ...pero al quitar fichas se mueven: se buscan de nuevo por valor */
		for (int k = 0; k < best_n; k++) {
			int idx[RK_MAX_SET];
			bool taken[RK_MAX_RACK] = { false };
			for (int i = 0; i < ns[k]; i++) {
				idx[i] = -1;
				for (int j = 0; j < g->rack_count[p]; j++)
					if (!taken[j] && g->racks[p][j].color == want[k][i].color &&
						g->racks[p][j].number == want[k][i].number) {
						idx[i] = j;
						taken[j] = true;
						break;
					}
			}
			if (rk_play_set(g, idx, ns[k]))
				placed += ns[k];
		}
	}
	/* Con la primera jugada hecha (o hecha ahora), añadir fichas sueltas a la mesa */
	if (g->melded[p] || g->turn_points >= RK_MIN_MELD) {
		bool again = true;
		while (again) {
			again = false;
			for (int i = 0; i < g->rack_count[p] && !again; i++) {
				if (is_joker(g->racks[p][i]) && g->rack_count[p] > 1)
					continue; /* el comodín se guarda */
				for (int s = 0; s < g->table_count && !again; s++)
					if ((g->melded[p] || s >= g->saved_table_count) && rk_add(g, s, i)) {
						placed++;
						again = true;
					}
			}
		}
	}
	rk_end_turn(g);
	return placed;
}
