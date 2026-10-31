#include "games/buscaminas.h"

#include <string.h>

void bm_level_size(BmLevel level, int* w, int* h, int* mines)
{
	static const int SIZE[BM_LEVELS][3] = { { 9, 9, 10 }, { 13, 9, 22 }, { 16, 10, 35 } };
	*w = SIZE[level][0];
	*h = SIZE[level][1];
	*mines = SIZE[level][2];
}

void bm_init(Buscaminas* g, BmLevel level, uint32_t seed)
{
	memset(g, 0, sizeof *g);
	g->level = level;
	bm_level_size(level, &g->w, &g->h, &g->mines);
	rng_seed(&g->rng, seed);
	g->state = BM_PLAYING;
	g->boom_x = g->boom_y = -1;
}

static bool inside(const Buscaminas* g, int x, int y)
{
	return x >= 0 && y >= 0 && x < g->w && y < g->h;
}

/* Pone las minas lejos de la primera casilla (sus vecinas también quedan libres) */
static void place(Buscaminas* g, int sx, int sy)
{
	int free_cells = 0;
	for (int y = 0; y < g->h; y++)
		for (int x = 0; x < g->w; x++)
			free_cells += !(x >= sx - 1 && x <= sx + 1 && y >= sy - 1 && y <= sy + 1);
	int left = g->mines < free_cells ? g->mines : free_cells;
	while (left > 0) {
		int x = rng_range(&g->rng, g->w), y = rng_range(&g->rng, g->h);
		if (g->mine[y][x] || (x >= sx - 1 && x <= sx + 1 && y >= sy - 1 && y <= sy + 1))
			continue;
		g->mine[y][x] = true;
		left--;
	}
	for (int y = 0; y < g->h; y++)
		for (int x = 0; x < g->w; x++) {
			int n = 0;
			for (int dy = -1; dy <= 1; dy++)
				for (int dx = -1; dx <= 1; dx++)
					if (inside(g, x + dx, y + dy) && g->mine[y + dy][x + dx])
						n++;
			g->near[y][x] = (uint8_t)n;
		}
	g->placed = true;
}

static void check_won(Buscaminas* g)
{
	if (g->opened == g->w * g->h - g->mines) {
		g->state = BM_WON;
		/* Al ganar se ponen las banderas que falten */
		for (int y = 0; y < g->h; y++)
			for (int x = 0; x < g->w; x++)
				if (g->mine[y][x])
					g->flag[y][x] = true;
	}
}

bool bm_reveal(Buscaminas* g, int x, int y)
{
	if (g->state != BM_PLAYING || !inside(g, x, y) || g->open[y][x] || g->flag[y][x])
		return false;
	if (!g->placed)
		place(g, x, y);
	if (g->mine[y][x]) {
		g->open[y][x] = true;
		g->state = BM_LOST;
		g->boom_x = x;
		g->boom_y = y;
		return true;
	}
	/* Relleno de las casillas sin minas alrededor */
	static int stack[BM_MAX_W * BM_MAX_H][2];
	int n = 0;
	stack[n][0] = x;
	stack[n][1] = y;
	n++;
	g->open[y][x] = true;
	g->opened++;
	while (n > 0) {
		n--;
		int cx = stack[n][0], cy = stack[n][1];
		if (g->near[cy][cx] != 0)
			continue;
		for (int dy = -1; dy <= 1; dy++)
			for (int dx = -1; dx <= 1; dx++) {
				int nx = cx + dx, ny = cy + dy;
				if (!inside(g, nx, ny) || g->open[ny][nx] || g->flag[ny][nx] || g->mine[ny][nx])
					continue;
				g->open[ny][nx] = true;
				g->opened++;
				stack[n][0] = nx;
				stack[n][1] = ny;
				n++;
			}
	}
	check_won(g);
	return true;
}

bool bm_toggle_flag(Buscaminas* g, int x, int y)
{
	if (g->state != BM_PLAYING || !inside(g, x, y) || g->open[y][x])
		return false;
	g->flag[y][x] = !g->flag[y][x];
	return true;
}

bool bm_chord(Buscaminas* g, int x, int y)
{
	if (g->state != BM_PLAYING || !inside(g, x, y) || !g->open[y][x] || g->near[y][x] == 0)
		return false;
	int flags = 0;
	for (int dy = -1; dy <= 1; dy++)
		for (int dx = -1; dx <= 1; dx++)
			if (inside(g, x + dx, y + dy) && g->flag[y + dy][x + dx])
				flags++;
	if (flags != g->near[y][x])
		return false;
	bool any = false;
	for (int dy = -1; dy <= 1; dy++)
		for (int dx = -1; dx <= 1; dx++)
			if (g->state == BM_PLAYING)
				any |= bm_reveal(g, x + dx, y + dy);
	return any;
}

int bm_flags(const Buscaminas* g)
{
	int n = 0;
	for (int y = 0; y < g->h; y++)
		for (int x = 0; x < g->w; x++)
			n += g->flag[y][x];
	return n;
}
