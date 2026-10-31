#include "games/riichi.h"

#include <stdio.h>
#include <string.h>

/* ---------------------------------------------------------------------------
 * Fichas
 * ------------------------------------------------------------------------- */

static bool is_honor(int t)
{
	return t >= RC_EAST;
}

static bool is_terminal(int t)
{
	return t < RC_EAST && (t % 9 == 0 || t % 9 == 8);
}

static bool is_yaochu(int t)
{
	return is_honor(t) || is_terminal(t);
}

static int suit_of(int t)
{
	return t < RC_EAST ? t / 9 : 3;
}

void rc_tile_name(int t, char* buf, int size)
{
	static const char* const SUIT[3] = { "caracteres", "círculos", "bambúes" };
	static const char* const HONOR[7] = { "Este", "Sur", "Oeste", "Norte", "Dragón blanco", "Dragón verde",
		"Dragón rojo" };
	if (t < RC_EAST)
		snprintf(buf, (size_t)size, "%d de %s", t % 9 + 1, SUIT[t / 9]);
	else
		snprintf(buf, (size_t)size, "%s", HONOR[t - RC_EAST]);
}

int rc_dora_of(int ind)
{
	if (ind < RC_EAST)
		return ind / 9 * 9 + (ind % 9 + 1) % 9;
	if (ind <= RC_NORTH)
		return RC_EAST + (ind - RC_EAST + 1) % 4;
	return RC_HAKU + (ind - RC_HAKU + 1) % 3;
}

/* ---------------------------------------------------------------------------
 * Shanten
 * ------------------------------------------------------------------------- */

static int s_best;

static void shanten_dfs(int* c, int i, int m, int t, int pair)
{
	while (i < RC_TYPES && c[i] == 0)
		i++;
	if (i >= RC_TYPES) {
		int tt = m + t > 4 ? 4 - m : t;
		int s = 8 - 2 * m - tt - pair;
		if (s < s_best)
			s_best = s;
		return;
	}
	if (c[i] >= 3) {
		c[i] -= 3;
		shanten_dfs(c, i, m + 1, t, pair);
		c[i] += 3;
	}
	if (i < RC_EAST && i % 9 <= 6 && c[i + 1] && c[i + 2]) {
		c[i]--, c[i + 1]--, c[i + 2]--;
		shanten_dfs(c, i, m + 1, t, pair);
		c[i]++, c[i + 1]++, c[i + 2]++;
	}
	if (!pair && c[i] >= 2) {
		c[i] -= 2;
		shanten_dfs(c, i, m, t, 1);
		c[i] += 2;
	}
	if (m + t < 4) {
		if (c[i] >= 2) {
			c[i] -= 2;
			shanten_dfs(c, i, m, t + 1, pair);
			c[i] += 2;
		}
		if (i < RC_EAST && i % 9 <= 7 && c[i + 1]) {
			c[i]--, c[i + 1]--;
			shanten_dfs(c, i, m, t + 1, pair);
			c[i]++, c[i + 1]++;
		}
		if (i < RC_EAST && i % 9 <= 6 && c[i + 2]) {
			c[i]--, c[i + 2]--;
			shanten_dfs(c, i, m, t + 1, pair);
			c[i]++, c[i + 2]++;
		}
	}
	c[i]--;
	shanten_dfs(c, i, m, t, pair);
	c[i]++;
}

static int shanten_normal(const int counts[RC_TYPES], int melds)
{
	int c[RC_TYPES];
	memcpy(c, counts, sizeof c);
	s_best = 8;
	shanten_dfs(c, 0, melds, 0, 0);
	return s_best;
}

static int shanten_chiitoi(const int c[RC_TYPES])
{
	int pairs = 0, kinds = 0;
	for (int t = 0; t < RC_TYPES; t++) {
		pairs += c[t] >= 2;
		kinds += c[t] >= 1;
	}
	return 6 - pairs + (kinds < 7 ? 7 - kinds : 0);
}

static int shanten_kokushi(const int c[RC_TYPES])
{
	int kinds = 0;
	bool pair = false;
	for (int t = 0; t < RC_TYPES; t++)
		if (is_yaochu(t) && c[t]) {
			kinds++;
			if (c[t] >= 2)
				pair = true;
		}
	return 13 - kinds - (pair ? 1 : 0);
}

int rc_shanten(const int counts[RC_TYPES], int melds)
{
	int s = shanten_normal(counts, melds);
	if (melds == 0) {
		int c7 = shanten_chiitoi(counts), k = shanten_kokushi(counts);
		if (c7 < s)
			s = c7;
		if (k < s)
			s = k;
	}
	return s;
}

bool rc_is_complete(const int counts[RC_TYPES], int melds)
{
	return rc_shanten(counts, melds) == -1;
}

int rc_waits(const int counts[RC_TYPES], int melds, bool waits[RC_TYPES])
{
	int c[RC_TYPES], n = 0;
	memcpy(c, counts, sizeof c);
	for (int t = 0; t < RC_TYPES; t++) {
		waits[t] = false;
		if (c[t] >= 4)
			continue;
		c[t]++;
		if (rc_is_complete(c, melds)) {
			waits[t] = true;
			n++;
		}
		c[t]--;
	}
	return n;
}

/* ---------------------------------------------------------------------------
 * Puntuación
 * ------------------------------------------------------------------------- */

enum { G_SEQ, G_TRIP, G_KAN };

typedef struct {
	uint8_t kind, tile;
	bool open;
} Group;

typedef struct {
	Group g[4];
	int n;
	int pair;
} Shape;

#define MAX_SHAPES 32
static Shape s_shapes[MAX_SHAPES];
static int s_nshapes;

static void decompose(int* c, int i, Shape* cur)
{
	while (i < RC_TYPES && c[i] == 0)
		i++;
	if (i >= RC_TYPES) {
		if (s_nshapes < MAX_SHAPES)
			s_shapes[s_nshapes++] = *cur;
		return;
	}
	if (c[i] >= 3) {
		c[i] -= 3;
		cur->g[cur->n++] = (Group){ G_TRIP, (uint8_t)i, false };
		decompose(c, i, cur);
		cur->n--;
		c[i] += 3;
	}
	if (i < RC_EAST && i % 9 <= 6 && c[i + 1] && c[i + 2]) {
		c[i]--, c[i + 1]--, c[i + 2]--;
		cur->g[cur->n++] = (Group){ G_SEQ, (uint8_t)i, false };
		decompose(c, i, cur);
		cur->n--;
		c[i]++, c[i + 1]++, c[i + 2]++;
	}
}

int rc_base_points(int han, int fu, int yakuman, const char** limit)
{
	const char* l = NULL;
	int base;
	if (yakuman > 0) {
		base = 8000 * yakuman;
		l = yakuman > 1 ? "Doble yakuman" : "Yakuman";
	} else if (han >= 13) {
		base = 8000;
		l = "Yakuman (por han)";
	} else if (han >= 11) {
		base = 6000;
		l = "Sanbaiman";
	} else if (han >= 8) {
		base = 4000;
		l = "Baiman";
	} else if (han >= 6) {
		base = 3000;
		l = "Haneman";
	} else {
		base = fu << (han + 2);
		if (han >= 5 || base > 2000) {
			base = 2000;
			l = "Mangan";
		}
	}
	if (limit)
		*limit = l;
	return base;
}

static int up100(int x)
{
	return (x + 99) / 100 * 100;
}

typedef struct {
	RcYaku yaku[RC_MAX_YAKU];
	int n, han, yakuman;
} YakuList;

static void add(YakuList* y, const char* name, int han)
{
	if (y->n < RC_MAX_YAKU)
		y->yaku[y->n++] = (RcYaku){ name, han };
	y->han += han;
}

static void add_yakuman(YakuList* y, const char* name)
{
	if (y->n < RC_MAX_YAKU)
		y->yaku[y->n++] = (RcYaku){ name, 13 };
	y->yakuman++;
}

static bool is_green(int t)
{
	return t == RC_SOU + 1 || t == RC_SOU + 2 || t == RC_SOU + 3 || t == RC_SOU + 5 || t == RC_SOU + 7 || t == RC_HATSU;
}

/* Yaku que dependen solo de las fichas (sirven para todas las formas) */
static void tile_yaku(const int all[RC_TYPES], bool closed, YakuList* y, bool* yakuman_colors)
{
	bool simples = true, honors = false, terms_honors = true, green = true;
	int suits_used[3] = { 0 };
	for (int t = 0; t < RC_TYPES; t++) {
		if (!all[t])
			continue;
		if (is_yaochu(t))
			simples = false;
		else
			terms_honors = false;
		if (is_honor(t))
			honors = true;
		else
			suits_used[suit_of(t)] = 1;
		if (!is_green(t))
			green = false;
	}
	int nsuits = suits_used[0] + suits_used[1] + suits_used[2];
	*yakuman_colors = false;
	if (honors && nsuits == 0) {
		add_yakuman(y, "Tsuuiisou");
		*yakuman_colors = true;
	}
	if (green) {
		add_yakuman(y, "Ryuuiisou");
		*yakuman_colors = true;
	}
	if (simples)
		add(y, "Tanyao", 1);
	if (nsuits == 1 && !honors)
		add(y, "Chinitsu", closed ? 6 : 5);
	else if (nsuits == 1 && honors)
		add(y, "Honitsu", closed ? 3 : 2);
	(void)terms_honors;
}

typedef struct {
	int han, fu, yakuman, base;
	YakuList y;
} Eval;

static void common_yaku(const RcWinInfo* in, bool closed, YakuList* y)
{
	if (in->riichi)
		add(y, "Riichi", 1);
	if (in->ippatsu)
		add(y, "Ippatsu", 1);
	if (closed && in->tsumo)
		add(y, "Menzen tsumo", 1);
	if (in->haitei)
		add(y, in->tsumo ? "Haitei" : "Houtei", 1);
	if (in->rinshan && in->tsumo)
		add(y, "Rinshan kaihou", 1);
}

/* Evalúa una forma concreta (4 grupos y pareja) con la ficha ganadora
 * completando el grupo win_group (-1: la pareja) */
static void eval_shape(const Shape* s, int win_tile, int win_group, const int all[RC_TYPES], bool closed,
	const RcWinInfo* in, Eval* e)
{
	YakuList y = { 0 };
	int seqs = 0, trips = 0, kans = 0, concealed_trips = 0;
	for (int i = 0; i < 4; i++) {
		const Group* g = &s->g[i];
		if (g->kind == G_SEQ) {
			seqs++;
		} else {
			trips++;
			if (g->kind == G_KAN)
				kans++;
			bool concealed = !g->open && !(i == win_group && !in->tsumo);
			concealed_trips += concealed;
		}
	}
	/* Espera */
	bool ryanmen = false, closed_wait = false;
	if (win_group < 0) {
		closed_wait = true; /* tanki */
	} else if (s->g[win_group].kind == G_SEQ) {
		int b = s->g[win_group].tile, r = b % 9;
		if (win_tile == b + 1)
			closed_wait = true; /* kanchan */
		else if ((win_tile == b && r == 6) || (win_tile == b + 2 && r == 0))
			closed_wait = true; /* penchan */
		else
			ryanmen = true;
	}
	int seat = in->seat_wind, round = in->round_wind;
	bool pair_yakuhai = s->pair >= RC_HAKU || s->pair == seat || s->pair == round;

	/* Yakuman */
	bool colors;
	YakuList ty = { 0 };
	tile_yaku(all, closed, &ty, &colors);
	int dragons_trip = 0, winds_trip = 0;
	for (int i = 0; i < 4; i++)
		if (s->g[i].kind != G_SEQ) {
			dragons_trip += s->g[i].tile >= RC_HAKU;
			winds_trip += s->g[i].tile >= RC_EAST && s->g[i].tile <= RC_NORTH;
		}
	if (concealed_trips == 4)
		add_yakuman(&y, "Suuankou");
	if (dragons_trip == 3)
		add_yakuman(&y, "Daisangen");
	if (winds_trip == 4)
		add_yakuman(&y, "Daisuushi");
	else if (winds_trip == 3 && s->pair >= RC_EAST && s->pair <= RC_NORTH)
		add_yakuman(&y, "Shousuushi");
	if (kans == 4)
		add_yakuman(&y, "Suukantsu");
	bool all_terminals = true;
	for (int t = 0; t < RC_TYPES; t++)
		if (all[t] && !is_terminal(t))
			all_terminals = false;
	if (all_terminals)
		add_yakuman(&y, "Chinroutou");
	if (colors)
		for (int k = 0; k < ty.n; k++)
			if (ty.yaku[k].han == 13)
				add_yakuman(&y, ty.yaku[k].name);
	/* Chuuren poutou: 1112345678999 + una del mismo palo, cerrada */
	if (closed && kans == 0) {
		int suit = -1;
		bool one_suit = true;
		for (int t = 0; t < RC_TYPES; t++)
			if (all[t]) {
				if (is_honor(t) || (suit >= 0 && suit_of(t) != suit))
					one_suit = false;
				suit = suit_of(t);
			}
		if (one_suit && suit >= 0 && suit < 3) {
			static const int need[9] = { 3, 1, 1, 1, 1, 1, 1, 1, 3 };
			bool ok = true;
			for (int k = 0; k < 9; k++)
				if (all[suit * 9 + k] < need[k])
					ok = false;
			if (ok)
				add_yakuman(&y, "Chuuren poutou");
		}
	}
	if (y.yakuman > 0) {
		e->y = y;
		e->yakuman = y.yakuman;
		e->han = 13 * y.yakuman;
		e->fu = 0;
		e->base = rc_base_points(0, 0, y.yakuman, NULL);
		return;
	}

	/* Yaku normales */
	common_yaku(in, closed, &y);
	for (int k = 0; k < ty.n; k++)
		add(&y, ty.yaku[k].name, ty.yaku[k].han);
	bool pinfu = closed && seqs == 4 && !pair_yakuhai && ryanmen;
	if (pinfu)
		add(&y, "Pinfu", 1);
	/* Escaleras iguales */
	int same = 0;
	for (int i = 0; i < 4; i++)
		for (int j = i + 1; j < 4; j++)
			if (s->g[i].kind == G_SEQ && s->g[j].kind == G_SEQ && s->g[i].tile == s->g[j].tile)
				same++;
	if (closed && same >= 2 && seqs == 4) {
		/* Dos parejas de escaleras iguales (o cuatro iguales) */
		add(&y, "Ryanpeikou", 3);
	} else if (closed && same >= 1) {
		add(&y, "Iipeikou", 1);
	}
	/* Yakuhai */
	for (int i = 0; i < 4; i++) {
		if (s->g[i].kind == G_SEQ)
			continue;
		int t = s->g[i].tile;
		if (t == RC_HAKU)
			add(&y, "Yakuhai: blanco", 1);
		else if (t == RC_HATSU)
			add(&y, "Yakuhai: verde", 1);
		else if (t == RC_CHUN)
			add(&y, "Yakuhai: rojo", 1);
		if (t == seat)
			add(&y, "Yakuhai: tu viento", 1);
		if (t == round)
			add(&y, "Yakuhai: viento de la ronda", 1);
	}
	/* Sanshoku, ittsu, sanshoku doukou */
	bool sanshoku = false, ittsu = false, doukou = false;
	for (int i = 0; i < 4; i++) {
		const Group* a = &s->g[i];
		if (a->tile >= RC_EAST)
			continue;
		int r = a->tile % 9;
		bool found[3] = { false, false, false };
		for (int j = 0; j < 4; j++) {
			const Group* b = &s->g[j];
			if (b->tile < RC_EAST && b->tile % 9 == r && (b->kind == G_SEQ) == (a->kind == G_SEQ))
				found[b->tile / 9] = true;
		}
		if (found[0] && found[1] && found[2]) {
			if (a->kind == G_SEQ)
				sanshoku = true;
			else
				doukou = true;
		}
		if (a->kind == G_SEQ && r == 0) {
			bool mid = false, hi = false;
			for (int j = 0; j < 4; j++)
				if (s->g[j].kind == G_SEQ) {
					mid |= s->g[j].tile == a->tile + 3;
					hi |= s->g[j].tile == a->tile + 6;
				}
			if (mid && hi)
				ittsu = true;
		}
	}
	if (sanshoku)
		add(&y, "Sanshoku", closed ? 2 : 1);
	if (ittsu)
		add(&y, "Ittsu", closed ? 2 : 1);
	if (doukou)
		add(&y, "Sanshoku doukou", 2);
	/* Chanta y junchan: todos los grupos y la pareja con terminales o honores */
	bool all_yaochu = is_yaochu(s->pair), any_honor = is_honor(s->pair);
	for (int i = 0; i < 4; i++) {
		const Group* g = &s->g[i];
		bool has = g->kind == G_SEQ ? (g->tile % 9 == 0 || g->tile % 9 == 6) : is_yaochu(g->tile);
		if (!has)
			all_yaochu = false;
		if (g->kind != G_SEQ && is_honor(g->tile))
			any_honor = true;
	}
	if (all_yaochu && seqs > 0)
		add(&y, any_honor ? "Chanta" : "Junchan", any_honor ? (closed ? 2 : 1) : (closed ? 3 : 2));
	if (trips == 4)
		add(&y, "Toitoi", 2);
	if (concealed_trips == 3)
		add(&y, "Sanankou", 2);
	if (kans == 3)
		add(&y, "Sankantsu", 2);
	if (dragons_trip == 2 && s->pair >= RC_HAKU)
		add(&y, "Shousangen", 2);
	if (seqs == 0) {
		bool hr = true;
		for (int t = 0; t < RC_TYPES; t++)
			if (all[t] && !is_yaochu(t))
				hr = false;
		if (hr)
			add(&y, "Honroutou", 2);
	}

	/* Fu */
	int fu;
	if (pinfu) {
		fu = in->tsumo ? 20 : 30;
	} else {
		fu = 20;
		if (closed && !in->tsumo)
			fu += 10;
		if (in->tsumo)
			fu += 2;
		for (int i = 0; i < 4; i++) {
			const Group* g = &s->g[i];
			if (g->kind == G_SEQ)
				continue;
			bool concealed = !g->open && !(i == win_group && !in->tsumo);
			int f = 2;
			if (concealed)
				f *= 2;
			if (is_yaochu(g->tile))
				f *= 2;
			if (g->kind == G_KAN)
				f *= 4;
			fu += f;
		}
		if (s->pair >= RC_HAKU)
			fu += 2;
		if (s->pair == seat)
			fu += 2;
		if (s->pair == round)
			fu += 2;
		if (closed_wait)
			fu += 2;
		fu = (fu + 9) / 10 * 10;
		if (!closed && fu == 20)
			fu = 30;
	}
	e->y = y;
	e->han = y.han;
	e->fu = fu;
	e->yakuman = 0;
	e->base = y.han > 0 ? rc_base_points(y.han, fu, 0, NULL) : 0;
}

static int count_dora(const int all[RC_TYPES], const int* ind, int n)
{
	int d = 0;
	for (int i = 0; i < n; i++)
		d += all[rc_dora_of(ind[i])];
	return d;
}

bool rc_score_hand(const int concealed[RC_TYPES], const RcMeld* melds, int meld_n, int win_tile, const RcWinInfo* info,
	RcResult* out)
{
	int all[RC_TYPES];
	memcpy(all, concealed, sizeof all);
	bool closed = true;
	for (int i = 0; i < meld_n; i++) {
		const RcMeld* m = &melds[i];
		if (m->type == RC_CHI) {
			all[m->tile]++, all[m->tile + 1]++, all[m->tile + 2]++;
		} else {
			all[m->tile] += m->type == RC_PON ? 3 : 4;
		}
		if (m->type != RC_KAN_CLOSED)
			closed = false;
	}
	Eval best = { 0 };
	best.base = -1;
	bool found = false;

	/* Formas de 4 grupos y pareja */
	int c[RC_TYPES];
	memcpy(c, concealed, sizeof c);
	for (int p = 0; p < RC_TYPES; p++) {
		if (c[p] < 2)
			continue;
		c[p] -= 2;
		Shape cur = { .n = 0, .pair = p };
		for (int i = 0; i < meld_n; i++) {
			const RcMeld* m = &melds[i];
			cur.g[cur.n++] = (Group){ m->type == RC_CHI ? G_SEQ : m->type == RC_PON ? G_TRIP : G_KAN, m->tile,
				m->type != RC_KAN_CLOSED };
		}
		s_nshapes = 0;
		decompose(c, 0, &cur);
		c[p] += 2;
		for (int k = 0; k < s_nshapes; k++) {
			const Shape* s = &s_shapes[k];
			if (s->n != 4)
				continue;
			/* Cada grupo cerrado que contenga la ficha ganadora es una espera posible */
			for (int w = -1; w < 4; w++) {
				if (w < 0 && s->pair != win_tile)
					continue;
				if (w >= 0) {
					const Group* g = &s->g[w];
					if (g->open || g->kind == G_KAN)
						continue;
					bool has = g->kind == G_SEQ ? (win_tile >= g->tile && win_tile <= g->tile + 2 &&
					                                  suit_of(win_tile) == suit_of(g->tile))
					                            : g->tile == win_tile;
					if (!has)
						continue;
				}
				Eval e;
				eval_shape(s, win_tile, w, all, closed, info, &e);
				if (e.base > best.base || (e.base == best.base && e.han > best.han)) {
					best = e;
					found = true;
				}
			}
		}
	}

	/* Siete parejas */
	if (meld_n == 0) {
		int pairs = 0;
		for (int t = 0; t < RC_TYPES; t++)
			pairs += concealed[t] == 2;
		if (pairs == 7) {
			Eval e = { 0 };
			bool colors;
			YakuList ty = { 0 };
			tile_yaku(all, true, &ty, &colors);
			if (colors) {
				for (int k = 0; k < ty.n; k++)
					if (ty.yaku[k].han == 13)
						add_yakuman(&e.y, ty.yaku[k].name);
			} else {
				common_yaku(info, true, &e.y);
				add(&e.y, "Chiitoitsu", 2);
				for (int k = 0; k < ty.n; k++)
					add(&e.y, ty.yaku[k].name, ty.yaku[k].han);
				bool hr = true;
				for (int t = 0; t < RC_TYPES; t++)
					if (all[t] && !is_yaochu(t))
						hr = false;
				if (hr)
					add(&e.y, "Honroutou", 2);
			}
			e.yakuman = e.y.yakuman;
			e.han = e.yakuman ? 13 * e.yakuman : e.y.han;
			e.fu = 25;
			e.base = rc_base_points(e.han, 25, e.yakuman, NULL);
			if (e.base > best.base) {
				best = e;
				found = true;
			}
		}
		/* Trece huérfanos */
		if (shanten_kokushi(concealed) == -1) {
			Eval e = { 0 };
			add_yakuman(&e.y, "Kokushi musou");
			e.yakuman = 1;
			e.han = 13;
			e.base = rc_base_points(0, 0, 1, NULL);
			if (e.base > best.base) {
				best = e;
				found = true;
			}
		}
	}
	if (!found || (best.han == 0 && best.yakuman == 0))
		return false;

	memset(out, 0, sizeof *out);
	out->yaku_n = best.y.n;
	memcpy(out->yaku, best.y.yaku, sizeof out->yaku);
	out->fu = best.fu;
	out->yakuman = best.yakuman;
	int han = best.han;
	if (!best.yakuman) {
		out->dora = count_dora(all, info->dora_indicators, info->dora_n);
		out->ura = info->riichi ? count_dora(all, info->ura_indicators, info->dora_n) : 0;
		han += out->dora + out->ura;
	}
	out->han = han;
	int base = rc_base_points(han, best.fu, best.yakuman, &out->limit);
	out->tsumo = info->tsumo;
	out->win_tile = win_tile;
	/* Lo que cobra en total (sin honba) */
	if (info->tsumo)
		out->points = info->dealer ? 3 * up100(2 * base) : up100(2 * base) + 2 * up100(base);
	else
		out->points = up100((info->dealer ? 6 : 4) * base);
	return true;
}

/* ---------------------------------------------------------------------------
 * Partida
 * ------------------------------------------------------------------------- */

void rc_init(Riichi* g, uint32_t seed)
{
	memset(g, 0, sizeof *g);
	rng_seed(&g->rng, seed);
	for (int p = 0; p < RC_PLAYERS; p++)
		g->scores[p] = RC_START;
	g->dealer = rng_range(&g->rng, RC_PLAYERS);
	g->hand_num = 0;
	rc_new_hand(g);
}

int rc_seat_wind(const Riichi* g, int p)
{
	return RC_EAST + (p - g->dealer + RC_PLAYERS) % RC_PLAYERS;
}

int rc_live_left(const Riichi* g)
{
	return g->live_end - g->wall_pos;
}

bool rc_open(const Riichi* g, int p)
{
	for (int i = 0; i < g->meld_n[p]; i++)
		if (g->melds[p][i].type != RC_KAN_CLOSED)
			return true;
	return false;
}

void rc_counts(const Riichi* g, int p, int counts[RC_TYPES])
{
	memset(counts, 0, sizeof(int) * RC_TYPES);
	for (int i = 0; i < g->hand_n[p]; i++)
		counts[g->hand[p][i]]++;
}

static void sort_hand(Riichi* g, int p, int n)
{
	uint8_t* h = g->hand[p];
	for (int i = 1; i < n; i++) {
		uint8_t v = h[i];
		int j = i - 1;
		while (j >= 0 && h[j] > v) {
			h[j + 1] = h[j];
			j--;
		}
		h[j + 1] = v;
	}
}

static void draw_tile(Riichi* g, int p, bool rinshan)
{
	int t;
	if (rinshan) {
		t = g->wall[g->rinshan_pos--];
		g->live_end--;
	} else {
		t = g->wall[g->wall_pos++];
	}
	g->hand[p][g->hand_n[p]++] = (uint8_t)t;
	g->drawn = t;
	g->rinshan = rinshan;
	g->current = p;
	g->phase = RC_TURN;
}

static int dora_pos(int i)
{
	return RC_TILES - 6 - 2 * i; /* 130, 128, ... */
}

static int ura_pos(int i)
{
	return RC_TILES - 5 - 2 * i; /* 131, 129, ... */
}

void rc_new_hand(Riichi* g)
{
	int n = 0;
	for (int t = 0; t < RC_TYPES; t++)
		for (int k = 0; k < 4; k++)
			g->wall[n++] = (uint8_t)t;
	for (int i = RC_TILES - 1; i > 0; i--) {
		int j = rng_range(&g->rng, i + 1);
		uint8_t tmp = g->wall[i];
		g->wall[i] = g->wall[j];
		g->wall[j] = tmp;
	}
	g->wall_pos = 0;
	g->live_end = RC_TILES - RC_DEAD;
	g->rinshan_pos = RC_TILES - 1;
	g->dora_n = 1;
	g->kans = 0;
	g->any_call = false;
	for (int p = 0; p < RC_PLAYERS; p++) {
		g->hand_n[p] = 0;
		g->meld_n[p] = 0;
		g->river_n[p] = 0;
		g->riichi_at[p] = -1;
		g->ippatsu[p] = false;
		g->skip_ron[p] = false;
		for (int k = 0; k < 13; k++)
			g->hand[p][g->hand_n[p]++] = g->wall[g->wall_pos++];
		sort_hand(g, p, 13);
	}
	memset(&g->result, 0, sizeof g->result);
	g->result.winner = -1;
	g->discard_tile = -1;
	g->discarder = -1;
	draw_tile(g, g->dealer, false);
}

static void win_info(const Riichi* g, int p, bool tsumo, RcWinInfo* in)
{
	memset(in, 0, sizeof *in);
	in->tsumo = tsumo;
	in->riichi = g->riichi_at[p] >= 0;
	in->ippatsu = g->ippatsu[p];
	in->haitei = rc_live_left(g) == 0 && !(tsumo && g->rinshan);
	in->rinshan = tsumo && g->rinshan;
	in->seat_wind = rc_seat_wind(g, p);
	in->round_wind = RC_EAST;
	in->dealer = p == g->dealer;
	in->dora_n = g->dora_n;
	for (int i = 0; i < g->dora_n; i++) {
		in->dora_indicators[i] = g->wall[dora_pos(i)];
		in->ura_indicators[i] = g->wall[ura_pos(i)];
	}
}

/* ¿Puede ganar p con la ficha t (que ya está en counts si tsumo)? */
static bool can_win(const Riichi* g, int p, const int counts[RC_TYPES], int win_tile, bool tsumo, RcResult* out)
{
	if (!rc_is_complete(counts, g->meld_n[p]))
		return false;
	RcWinInfo in;
	win_info(g, p, tsumo, &in);
	RcResult tmp;
	return rc_score_hand(counts, g->melds[p], g->meld_n[p], win_tile, &in, out ? out : &tmp);
}

static void end_game_check(Riichi* g)
{
	bool broke = false;
	for (int p = 0; p < RC_PLAYERS; p++)
		if (g->scores[p] < 0)
			broke = true;
	if (broke || g->hand_num >= 4)
		g->phase = RC_GAME_OVER;
}

static void settle_win(Riichi* g, int p, int loser, int win_tile)
{
	int counts[RC_TYPES];
	rc_counts(g, p, counts);
	if (loser >= 0)
		counts[win_tile]++;
	RcResult r;
	RcWinInfo in;
	win_info(g, p, loser < 0, &in);
	rc_score_hand(counts, g->melds[p], g->meld_n[p], win_tile, &in, &r);
	int base = rc_base_points(r.han, r.fu, r.yakuman, NULL);
	r.winner = p;
	r.loser = loser;
	memset(r.delta, 0, sizeof r.delta);
	if (loser >= 0) {
		int pay = r.points + 300 * g->honba;
		r.delta[loser] -= pay;
		r.delta[p] += pay;
	} else {
		for (int q = 0; q < RC_PLAYERS; q++) {
			if (q == p)
				continue;
			int pay = (p == g->dealer || q == g->dealer ? up100(2 * base) : up100(base)) + 100 * g->honba;
			r.delta[q] -= pay;
			r.delta[p] += pay;
		}
	}
	r.delta[p] += 1000 * g->sticks;
	g->sticks = 0;
	for (int q = 0; q < RC_PLAYERS; q++)
		g->scores[q] += r.delta[q];
	/* Mano para enseñar */
	r.hand_n = 0;
	for (int t = 0; t < RC_TYPES; t++)
		for (int k = 0; k < counts[t]; k++)
			r.hand[r.hand_n++] = (uint8_t)t;
	g->result = r;
	g->phase = RC_HAND_OVER;
	/* Repite el que reparte si gana */
	if (p == g->dealer) {
		g->honba++;
	} else {
		g->honba = 0;
		g->dealer = (g->dealer + 1) % RC_PLAYERS;
		g->hand_num++;
	}
}

static void exhaustive_draw(Riichi* g)
{
	RcResult* r = &g->result;
	memset(r, 0, sizeof *r);
	r->draw = true;
	r->winner = r->loser = -1;
	int k = 0;
	for (int p = 0; p < RC_PLAYERS; p++) {
		int c[RC_TYPES];
		rc_counts(g, p, c);
		r->tenpai[p] = rc_shanten(c, g->meld_n[p]) == 0;
		k += r->tenpai[p];
	}
	if (k > 0 && k < RC_PLAYERS)
		for (int p = 0; p < RC_PLAYERS; p++)
			r->delta[p] = r->tenpai[p] ? 3000 / k : -3000 / (RC_PLAYERS - k);
	for (int p = 0; p < RC_PLAYERS; p++)
		g->scores[p] += r->delta[p];
	g->phase = RC_HAND_OVER;
	g->honba++;
	if (!r->tenpai[g->dealer]) {
		g->dealer = (g->dealer + 1) % RC_PLAYERS;
		g->hand_num++;
	}
}

bool rc_can_tsumo(const Riichi* g)
{
	if (g->phase != RC_TURN || g->drawn < 0)
		return false;
	int c[RC_TYPES];
	rc_counts(g, g->current, c);
	return can_win(g, g->current, c, g->drawn, true, NULL);
}

bool rc_tsumo(Riichi* g)
{
	if (!rc_can_tsumo(g))
		return false;
	settle_win(g, g->current, -1, g->drawn);
	return true;
}

bool rc_riichi_discard_ok(const Riichi* g, int i)
{
	int p = g->current;
	int c[RC_TYPES];
	rc_counts(g, p, c);
	c[g->hand[p][i]]--;
	return rc_shanten(c, g->meld_n[p]) == 0;
}

bool rc_can_riichi(const Riichi* g)
{
	int p = g->current;
	if (g->phase != RC_TURN || g->riichi_at[p] >= 0 || rc_open(g, p) || g->scores[p] < 1000 || rc_live_left(g) < 4)
		return false;
	for (int i = 0; i < g->hand_n[p]; i++)
		if (rc_riichi_discard_ok(g, i))
			return true;
	return false;
}

int rc_kan_option(const Riichi* g)
{
	int p = g->current;
	if (g->phase != RC_TURN || g->riichi_at[p] >= 0 || g->kans >= 4 || rc_live_left(g) < 1)
		return -1;
	int c[RC_TYPES];
	rc_counts(g, p, c);
	for (int t = 0; t < RC_TYPES; t++)
		if (c[t] == 4)
			return t;
	for (int i = 0; i < g->meld_n[p]; i++)
		if (g->melds[p][i].type == RC_PON && c[g->melds[p][i].tile])
			return g->melds[p][i].tile;
	return -1;
}

static void remove_tiles(Riichi* g, int p, int t, int n)
{
	for (int k = 0; k < n; k++)
		for (int i = 0; i < g->hand_n[p]; i++)
			if (g->hand[p][i] == t) {
				memmove(&g->hand[p][i], &g->hand[p][i + 1], (size_t)(g->hand_n[p] - i - 1));
				g->hand_n[p]--;
				break;
			}
}

static void after_kan(Riichi* g, int p)
{
	g->kans++;
	if (g->dora_n < 5)
		g->dora_n++;
	g->any_call = true;
	for (int q = 0; q < RC_PLAYERS; q++)
		g->ippatsu[q] = false;
	sort_hand(g, p, g->hand_n[p]);
	draw_tile(g, p, true);
}

bool rc_self_kan(Riichi* g, int t)
{
	int p = g->current;
	int opt = rc_kan_option(g);
	if (opt < 0)
		return false;
	if (t < 0)
		t = opt;
	int c[RC_TYPES];
	rc_counts(g, p, c);
	if (c[t] == 4) {
		remove_tiles(g, p, t, 4);
		g->melds[p][g->meld_n[p]++] = (RcMeld){ RC_KAN_CLOSED, (uint8_t)t, (uint8_t)p, (uint8_t)t };
	} else {
		int m = -1;
		for (int i = 0; i < g->meld_n[p]; i++)
			if (g->melds[p][i].type == RC_PON && g->melds[p][i].tile == t)
				m = i;
		if (m < 0 || c[t] < 1)
			return false;
		remove_tiles(g, p, t, 1);
		g->melds[p][m].type = RC_KAN_ADDED;
	}
	after_kan(g, p);
	return true;
}

/* Hay alguna llamada posible sobre el último descarte */
static bool any_claim(const Riichi* g)
{
	for (int p = 0; p < RC_PLAYERS; p++)
		if (p != g->discarder && rc_claim_options(g, p))
			return true;
	return false;
}

static void next_draw(Riichi* g)
{
	if (rc_live_left(g) <= 0) {
		exhaustive_draw(g);
		return;
	}
	draw_tile(g, (g->discarder + 1) % RC_PLAYERS, false);
}

bool rc_discard(Riichi* g, int i, bool riichi)
{
	int p = g->current;
	if (g->phase != RC_TURN || i < 0 || i >= g->hand_n[p])
		return false;
	if (g->riichi_at[p] >= 0 && g->drawn >= 0 && g->hand[p][i] != g->drawn)
		return false; /* con riichi se tira lo que se roba */
	if (riichi && (!rc_can_riichi(g) || !rc_riichi_discard_ok(g, i)))
		return false;
	int t = g->hand[p][i];
	memmove(&g->hand[p][i], &g->hand[p][i + 1], (size_t)(g->hand_n[p] - i - 1));
	g->hand_n[p]--;
	sort_hand(g, p, g->hand_n[p]);
	/* El ippatsu dura hasta el siguiente descarte propio */
	g->ippatsu[p] = false;
	if (riichi) {
		g->riichi_at[p] = g->river_n[p];
		g->scores[p] -= 1000;
		g->sticks++;
		g->ippatsu[p] = true;
	}
	if (g->riichi_at[p] < 0)
		g->skip_ron[p] = false;
	if (g->river_n[p] < RC_RIVER) {
		g->river[p][g->river_n[p]] = (uint8_t)t;
		g->river_taken[p][g->river_n[p]] = false;
		g->river_n[p]++;
	}
	g->discard_tile = t;
	g->discarder = p;
	g->drawn = -1;
	g->phase = RC_CLAIM;
	if (!any_claim(g))
		next_draw(g);
	return true;
}

/* El jugador p ha descartado alguna vez la ficha t (furiten) */
static bool in_own_river(const Riichi* g, int p, int t)
{
	for (int i = 0; i < g->river_n[p]; i++)
		if (g->river[p][i] == t)
			return true;
	return false;
}

int rc_chi_options(const Riichi* g, int p, int out[3])
{
	int t = g->discard_tile, n = 0;
	if (t < 0 || t >= RC_EAST || p != (g->discarder + 1) % RC_PLAYERS)
		return 0;
	int c[RC_TYPES];
	rc_counts(g, p, c);
	int r = t % 9;
	for (int low = t - 2; low <= t; low++) {
		int lr = r - (t - low);
		if (lr < 0 || lr > 6)
			continue;
		bool ok = true;
		for (int k = 0; k < 3; k++)
			if (low + k != t && c[low + k] == 0)
				ok = false;
		if (ok)
			out[n++] = low;
	}
	return n;
}

int rc_claim_options(const Riichi* g, int p)
{
	if (g->phase != RC_CLAIM || p == g->discarder)
		return 0;
	int t = g->discard_tile, opts = 0;
	int c[RC_TYPES];
	rc_counts(g, p, c);
	/* Ron */
	c[t]++;
	if (!g->skip_ron[p] && can_win(g, p, c, t, false, NULL)) {
		bool waits[RC_TYPES];
		c[t]--;
		rc_waits(c, g->meld_n[p], waits);
		bool furiten = false;
		for (int w = 0; w < RC_TYPES; w++)
			if (waits[w] && in_own_river(g, p, w))
				furiten = true;
		if (!furiten)
			opts |= RC_CAN_RON;
		c[t]++;
	}
	c[t]--;
	if (g->riichi_at[p] >= 0 || rc_live_left(g) <= 0)
		return opts;
	if (c[t] >= 2)
		opts |= RC_CAN_PON;
	if (c[t] == 3 && g->kans < 4)
		opts |= RC_CAN_KAN;
	int chi[3];
	if (rc_chi_options(g, p, chi) > 0)
		opts |= RC_CAN_CHI;
	return opts;
}

static void take_discard(Riichi* g, int p)
{
	int d = g->discarder;
	if (g->river_n[d] > 0)
		g->river_taken[d][g->river_n[d] - 1] = true;
	g->any_call = true;
	for (int q = 0; q < RC_PLAYERS; q++)
		g->ippatsu[q] = false;
	g->current = p;
	g->drawn = -1;
	g->rinshan = false;
}

void rc_resolve(Riichi* g, const RcClaim claims[RC_PLAYERS], int chi_low)
{
	if (g->phase != RC_CLAIM)
		return;
	int t = g->discard_tile, d = g->discarder;
	/* Ron: gana el primero en el orden de juego */
	for (int k = 1; k < RC_PLAYERS; k++) {
		int p = (d + k) % RC_PLAYERS;
		int opts = rc_claim_options(g, p);
		if ((opts & RC_CAN_RON) && claims[p] == RC_DO_RON) {
			settle_win(g, p, d, t);
			return;
		}
	}
	/* Quien deja pasar un ron queda en furiten hasta su descarte */
	for (int p = 0; p < RC_PLAYERS; p++)
		if (p != d && (rc_claim_options(g, p) & RC_CAN_RON))
			g->skip_ron[p] = true;
	/* Pon o kan */
	for (int k = 1; k < RC_PLAYERS; k++) {
		int p = (d + k) % RC_PLAYERS;
		int opts = rc_claim_options(g, p);
		if (claims[p] == RC_DO_KAN && (opts & RC_CAN_KAN)) {
			remove_tiles(g, p, t, 3);
			g->melds[p][g->meld_n[p]++] = (RcMeld){ RC_KAN_OPEN, (uint8_t)t, (uint8_t)d, (uint8_t)t };
			take_discard(g, p);
			after_kan(g, p);
			return;
		}
		if (claims[p] == RC_DO_PON && (opts & RC_CAN_PON)) {
			remove_tiles(g, p, t, 2);
			g->melds[p][g->meld_n[p]++] = (RcMeld){ RC_PON, (uint8_t)t, (uint8_t)d, (uint8_t)t };
			take_discard(g, p);
			g->phase = RC_TURN;
			return;
		}
	}
	/* Chi */
	int p = (d + 1) % RC_PLAYERS;
	if (claims[p] == RC_DO_CHI) {
		int opts[3], n = rc_chi_options(g, p, opts);
		for (int k = 0; k < n; k++)
			if (opts[k] == chi_low) {
				for (int j = 0; j < 3; j++)
					if (chi_low + j != t)
						remove_tiles(g, p, chi_low + j, 1);
				g->melds[p][g->meld_n[p]++] = (RcMeld){ RC_CHI, (uint8_t)chi_low, (uint8_t)d, (uint8_t)t };
				take_discard(g, p);
				g->phase = RC_TURN;
				return;
			}
	}
	next_draw(g);
}

void rc_next(Riichi* g)
{
	if (g->phase != RC_HAND_OVER)
		return;
	end_game_check(g);
	if (g->phase == RC_GAME_OVER)
		return;
	rc_new_hand(g);
}

/* ---------------------------------------------------------------------------
 * IA
 * ------------------------------------------------------------------------- */

/* Fichas que p no ve (para contar las que le sirven) */
static void unseen(const Riichi* g, int p, int out[RC_TYPES])
{
	for (int t = 0; t < RC_TYPES; t++)
		out[t] = 4;
	for (int i = 0; i < g->hand_n[p]; i++)
		out[g->hand[p][i]]--;
	for (int q = 0; q < RC_PLAYERS; q++) {
		for (int i = 0; i < g->river_n[q]; i++)
			if (!g->river_taken[q][i])
				out[g->river[q][i]]--;
		for (int i = 0; i < g->meld_n[q]; i++) {
			const RcMeld* m = &g->melds[q][i];
			if (m->type == RC_CHI)
				out[m->tile]--, out[m->tile + 1]--, out[m->tile + 2]--;
			else
				out[m->tile] -= m->type == RC_PON ? 3 : 4;
		}
	}
	for (int i = 0; i < g->dora_n; i++)
		out[g->wall[dora_pos(i)]]--;
	for (int t = 0; t < RC_TYPES; t++)
		if (out[t] < 0)
			out[t] = 0;
}

static int ukeire(int c[RC_TYPES], int melds, int sh, const int left[RC_TYPES])
{
	int n = 0;
	for (int t = 0; t < RC_TYPES; t++) {
		if (!left[t] || c[t] >= 4)
			continue;
		c[t]++;
		if (rc_shanten(c, melds) < sh)
			n += left[t];
		c[t]--;
	}
	return n;
}

/* Peligro de descartar t frente a los que están en riichi (0 = seguro) */
static int danger(const Riichi* g, int p, int t, const int left[RC_TYPES])
{
	int d = 0;
	for (int q = 0; q < RC_PLAYERS; q++) {
		if (q == p || g->riichi_at[q] < 0)
			continue;
		if (in_own_river(g, q, t))
			continue; /* genbutsu */
		if (is_honor(t))
			d += left[t] <= 1 ? 1 : 3;
		else
			d += is_terminal(t) ? 5 : 8;
	}
	return d;
}

int rc_ai_discard(const Riichi* g, bool* riichi)
{
	int p = g->current;
	*riichi = false;
	if (g->riichi_at[p] >= 0 && g->drawn >= 0) {
		for (int i = g->hand_n[p] - 1; i >= 0; i--)
			if (g->hand[p][i] == g->drawn)
				return i;
	}
	int c[RC_TYPES], left[RC_TYPES];
	rc_counts(g, p, c);
	unseen(g, p, left);
	bool threat = false;
	for (int q = 0; q < RC_PLAYERS; q++)
		if (q != p && g->riichi_at[q] >= 0)
			threat = true;
	int best = -1, best_sh = 99, best_uk = -1, best_dg = 99, best_val = 99;
	for (int i = 0; i < g->hand_n[p]; i++) {
		int t = g->hand[p][i];
		if (i > 0 && g->hand[p][i - 1] == t && g->drawn != t)
			continue;
		c[t]--;
		int sh = rc_shanten(c, g->meld_n[p]);
		int uk = sh <= 2 ? ukeire(c, g->meld_n[p], sh, left) : 0;
		c[t]++;
		int dg = threat ? danger(g, p, t, left) : 0;
		/* Valor de la ficha suelta: honores y terminales primero */
		int val = is_honor(t) ? (t >= RC_HAKU || t == rc_seat_wind(g, p) || t == RC_EAST ? 2 : 0) : is_terminal(t) ? 1 : 3;
		bool better;
		if (threat && sh >= 2) {
			better = dg < best_dg || (dg == best_dg && (sh < best_sh || (sh == best_sh && uk > best_uk)));
		} else {
			better = sh < best_sh || (sh == best_sh && (uk > best_uk || (uk == best_uk && (dg < best_dg ||
			                                                                   (dg == best_dg && val < best_val)))));
		}
		if (best < 0 || better) {
			best = i;
			best_sh = sh;
			best_uk = uk;
			best_dg = dg;
			best_val = val;
		}
	}
	if (best_sh == 0 && rc_can_riichi(g) && rc_riichi_discard_ok(g, best))
		*riichi = true;
	return best;
}

static bool yakuhai_for(const Riichi* g, int p, int t)
{
	return t >= RC_HAKU || t == rc_seat_wind(g, p) || t == RC_EAST;
}

/* La mano tiene un yaku asegurado aunque esté abierta */
static bool open_yaku(const Riichi* g, int p, const int c[RC_TYPES])
{
	for (int i = 0; i < g->meld_n[p]; i++) {
		const RcMeld* m = &g->melds[p][i];
		if (m->type != RC_CHI && yakuhai_for(g, p, m->tile))
			return true;
	}
	for (int t = 0; t < RC_TYPES; t++)
		if (yakuhai_for(g, p, t) && c[t] >= 3)
			return true;
	/* Tanyao: todo simples */
	for (int t = 0; t < RC_TYPES; t++)
		if (c[t] && is_yaochu(t))
			return false;
	for (int i = 0; i < g->meld_n[p]; i++) {
		const RcMeld* m = &g->melds[p][i];
		if (is_yaochu(m->tile) || (m->type == RC_CHI && is_yaochu(m->tile + 2)))
			return false;
	}
	return true;
}

RcClaim rc_ai_claim(const Riichi* g, int p, int* chi_low)
{
	int opts = rc_claim_options(g, p);
	*chi_low = -1;
	if (opts & RC_CAN_RON)
		return RC_DO_RON;
	int t = g->discard_tile;
	int c[RC_TYPES];
	rc_counts(g, p, c);
	int sh = rc_shanten(c, g->meld_n[p]);
	if (opts & RC_CAN_PON) {
		c[t] -= 2;
		int sh2 = rc_shanten(c, g->meld_n[p] + 1);
		c[t] += 2;
		/* Un pon de yakuhai ya da yaku; si no, solo con la mano abierta y un yaku seguro */
		if (yakuhai_for(g, p, t) && sh2 < sh + 1)
			return RC_DO_PON;
		c[t]++;
		bool yaku = open_yaku(g, p, c);
		c[t]--;
		if (rc_open(g, p) && yaku && sh2 < sh)
			return RC_DO_PON;
	}
	if ((opts & RC_CAN_CHI) && rc_open(g, p)) {
		int o[3], n = rc_chi_options(g, p, o);
		for (int k = 0; k < n; k++) {
			for (int j = 0; j < 3; j++)
				if (o[k] + j != t)
					c[o[k] + j]--;
			int sh2 = rc_shanten(c, g->meld_n[p] + 1);
			c[t]++;
			bool yaku = open_yaku(g, p, c);
			c[t]--;
			for (int j = 0; j < 3; j++)
				if (o[k] + j != t)
					c[o[k] + j]++;
			if (yaku && sh2 < sh) {
				*chi_low = o[k];
				return RC_DO_CHI;
			}
		}
	}
	return RC_PASS;
}

bool rc_ai_tsumo(const Riichi* g)
{
	return rc_can_tsumo(g);
}

int rc_ai_kan(const Riichi* g)
{
	int t = rc_kan_option(g);
	if (t < 0)
		return -1;
	/* Solo kanes que no estropean la mano */
	int p = g->current, c[RC_TYPES];
	rc_counts(g, p, c);
	int sh = rc_shanten(c, g->meld_n[p]);
	if (c[t] == 4) {
		/* Kan cerrado solo de honores y si no empeora la mano */
		c[t] -= 4;
		int sh2 = rc_shanten(c, g->meld_n[p] + 1);
		return is_honor(t) && sh2 <= sh ? t : -1;
	}
	return t; /* añadir a un pon nunca empeora */
}
