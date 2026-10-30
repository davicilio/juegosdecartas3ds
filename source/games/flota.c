#include "games/flota.h"

#include <string.h>

const int HF_LENGTHS[HF_SHIPS] = { 5, 4, 3, 3, 2 };

const char* hf_ship_name(int ship)
{
	static const char* const names[HF_SHIPS] = { "portaaviones", "acorazado", "crucero", "crucero", "destructor" };
	return ship >= 0 && ship < HF_SHIPS ? names[ship] : "?";
}

void hf_init(Flota* g, uint32_t seed)
{
	memset(g, 0, sizeof *g);
	rng_seed(&g->rng, seed);
	hf_new_game(g);
}

void hf_new_game(Flota* g)
{
	memset(g->shot, 0, sizeof g->shot);
	g->sunk[0] = g->sunk[1] = 0;
	g->shots[0] = g->shots[1] = 0;
	hf_place_random(g, 0);
	hf_place_random(g, 1);
	g->current = 0;
	g->winner = -1;
	g->last_result = HF_NONE;
	g->last_shooter = -1;
}

static bool fits(const Flota* g, int p, int r, int c, int len, bool horiz)
{
	for (int k = 0; k < len; k++) {
		int rr = r + (horiz ? 0 : k), cc = c + (horiz ? k : 0);
		if (rr >= HF_SIZE || cc >= HF_SIZE)
			return false;
		/* Ni encima ni pegado a otro barco */
		for (int dr = -1; dr <= 1; dr++)
			for (int dc = -1; dc <= 1; dc++) {
				int a = rr + dr, b = cc + dc;
				if (a >= 0 && a < HF_SIZE && b >= 0 && b < HF_SIZE && g->ship_at[p][a][b] >= 0)
					return false;
			}
	}
	return true;
}

void hf_place_random(Flota* g, int p)
{
	for (;;) {
		memset(g->ship_at[p], -1, sizeof g->ship_at[p]);
		bool ok = true;
		for (int s = 0; s < HF_SHIPS && ok; s++) {
			int len = HF_LENGTHS[s], tries = 0;
			for (;;) {
				bool h = rng_range(&g->rng, 2);
				int r = rng_range(&g->rng, HF_SIZE), c = rng_range(&g->rng, HF_SIZE);
				if (fits(g, p, r, c, len, h)) {
					g->ships[p][s] = (HfShip){ r, c, len, h, 0 };
					for (int k = 0; k < len; k++)
						g->ship_at[p][r + (h ? 0 : k)][c + (h ? k : 0)] = (int8_t)s;
					break;
				}
				if (++tries > 500) {
					ok = false;
					break;
				}
			}
		}
		if (ok)
			return;
	}
}

int hf_fire(Flota* g, int r, int c)
{
	if (g->winner >= 0 || r < 0 || r >= HF_SIZE || c < 0 || c >= HF_SIZE)
		return HF_NONE;
	int p = g->current, t = 1 - p;
	if (g->shot[t][r][c])
		return HF_NONE;
	g->shots[p]++;
	g->last_shooter = p;
	g->last_row = r;
	g->last_col = c;
	g->last_ship = g->ship_at[t][r][c];
	if (g->last_ship < 0) {
		g->shot[t][r][c] = HF_WATER;
		g->last_result = HF_WATER;
		g->current = t;
		return HF_WATER;
	}
	HfShip* s = &g->ships[t][g->last_ship];
	s->hits++;
	g->shot[t][r][c] = HF_HIT;
	g->last_result = HF_HIT;
	if (s->hits == s->len) {
		g->last_result = HF_SUNK;
		for (int k = 0; k < s->len; k++)
			g->shot[t][s->row + (s->horizontal ? 0 : k)][s->col + (s->horizontal ? k : 0)] = HF_SUNK;
		if (++g->sunk[t] == HF_SHIPS)
			g->winner = p;
	}
	return g->last_result;
}

/* IA: cuenta en cuántas posiciones posibles de los barcos que quedan entra
 * cada casilla. Si hay barcos tocados sin hundir, solo valen las posiciones
 * que pasan por ellos (y cuentan mucho más). */
int hf_ai_target(Flota* g)
{
	int p = g->current, t = 1 - p;
	int score[HF_SIZE][HF_SIZE];
	memset(score, 0, sizeof score);
	bool hunting = false;
	for (int r = 0; r < HF_SIZE; r++)
		for (int c = 0; c < HF_SIZE; c++)
			if (g->shot[t][r][c] == HF_HIT)
				hunting = true;
	for (int s = 0; s < HF_SHIPS; s++) {
		const HfShip* ship = &g->ships[t][s];
		if (ship->hits == ship->len)
			continue;
		int len = ship->len;
		for (int h = 0; h < 2; h++)
			for (int r = 0; r < HF_SIZE; r++)
				for (int c = 0; c < HF_SIZE; c++) {
					int hits = 0;
					bool ok = true;
					for (int k = 0; k < len && ok; k++) {
						int rr = r + (h ? 0 : k), cc = c + (h ? k : 0);
						if (rr >= HF_SIZE || cc >= HF_SIZE) {
							ok = false;
							break;
						}
						uint8_t v = g->shot[t][rr][cc];
						if (v == HF_WATER || v == HF_SUNK)
							ok = false;
						else if (v == HF_HIT)
							hits++;
					}
					if (!ok || (hunting && hits == 0))
						continue;
					int w = hunting ? 1 + hits * 20 : 1;
					for (int k = 0; k < len; k++) {
						int rr = r + (h ? 0 : k), cc = c + (h ? k : 0);
						if (!g->shot[t][rr][cc])
							score[rr][cc] += w;
					}
				}
	}
	int best = -1, bv = -1, ties = 0;
	for (int r = 0; r < HF_SIZE; r++)
		for (int c = 0; c < HF_SIZE; c++) {
			if (g->shot[t][r][c])
				continue;
			int v = score[r][c] * 4 + (!hunting && (r + c) % 2 == 0 ? 1 : 0);
			if (v > bv) {
				bv = v;
				best = r * HF_SIZE + c;
				ties = 1;
			} else if (v == bv && rng_range(&g->rng, ++ties) == 0) {
				best = r * HF_SIZE + c;
			}
		}
	return best;
}
