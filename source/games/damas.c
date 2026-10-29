#include "games/damas.h"

#include <string.h>

void da_init(Damas* g, uint32_t seed)
{
	rng_seed(&g->rng, seed);
	g->level = 5;
	da_new_game(g);
}

void da_new_game(Damas* g)
{
	memset(g->b, 0, sizeof g->b);
	for (int r = 0; r < DA_SIZE; r++)
		for (int c = 0; c < DA_SIZE; c++)
			if ((r + c) % 2 == 1) {
				if (r < 3)
					g->b[r][c] = DA_BLACK;
				else if (r > 4)
					g->b[r][c] = DA_WHITE;
			}
	g->current = 1;
	g->winner = 0;
	g->quiet = 0;
	g->has_last = false;
}

static bool inside(int r, int c)
{
	return r >= 0 && r < DA_SIZE && c >= 0 && c < DA_SIZE;
}

static bool is_king(int piece)
{
	return piece == DA_WHITE_KING || piece == DA_BLACK_KING;
}

typedef struct {
	DaMove* out;
	int n;
	int best; /* mayor número de capturas encontrado */
} Gen;

static void add(Gen* gen, const DaMove* m)
{
	if (m->n_caps < gen->best)
		return;
	if (m->n_caps > gen->best) {
		gen->best = m->n_caps;
		gen->n = 0; /* las de menos capturas ya no valen */
	}
	if (gen->n < DA_MAX_MOVES) {
		/* Copia limpia: sin restos de otras ramas de la búsqueda */
		DaMove* o = &gen->out[gen->n++];
		memset(o, 0, sizeof *o);
		o->fr = m->fr;
		o->fc = m->fc;
		o->n_path = m->n_path;
		o->n_caps = m->n_caps;
		memcpy(o->path, m->path, sizeof(o->path[0]) * m->n_path);
		memcpy(o->caps, m->caps, sizeof(o->caps[0]) * m->n_caps);
	}
}

static bool captured(const DaMove* m, int r, int c)
{
	for (int i = 0; i < m->n_caps; i++)
		if (m->caps[i][0] == r && m->caps[i][1] == c)
			return true;
	return false;
}

/* Búsqueda de capturas desde (r, c) con la pieza piece; m lleva lo hecho */
static void captures_from(const uint8_t b[DA_SIZE][DA_SIZE], int piece, int r, int c, DaMove* m, Gen* gen)
{
	int side = da_owner(piece);
	int fwd = side == 1 ? -1 : 1;
	bool found = false;
	for (int dr = -1; dr <= 1; dr += 2)
		for (int dc = -1; dc <= 1; dc += 2) {
			if (!is_king(piece) && dr != fwd)
				continue; /* las fichas solo comen hacia delante */
			int rr = r + dr, cc = c + dc;
			if (is_king(piece))
				while (inside(rr, cc) && b[rr][cc] == DA_EMPTY) {
					rr += dr;
					cc += dc;
				}
			if (!inside(rr, cc) || da_owner(b[rr][cc]) == side || b[rr][cc] == DA_EMPTY || captured(m, rr, cc))
				continue;
			int er = rr + dr, ec = cc + dc;
			while (inside(er, ec) && b[er][ec] == DA_EMPTY) {
				if (m->n_path >= DA_MAX_PATH)
					break;
				found = true;
				m->caps[m->n_caps][0] = (int8_t)rr;
				m->caps[m->n_caps][1] = (int8_t)cc;
				m->n_caps++;
				m->path[m->n_path][0] = (int8_t)er;
				m->path[m->n_path][1] = (int8_t)ec;
				m->n_path++;
				captures_from(b, piece, er, ec, m, gen);
				m->n_path--;
				m->n_caps--;
				if (!is_king(piece))
					break; /* la ficha cae justo detrás */
				er += dr;
				ec += dc;
			}
		}
	if (!found && m->n_caps > 0)
		add(gen, m);
}

int da_moves(const Damas* g, DaMove* out)
{
	Gen gen = { out, 0, 1 };
	uint8_t b[DA_SIZE][DA_SIZE];
	memcpy(b, g->b, sizeof b);
	/* Capturas */
	for (int r = 0; r < DA_SIZE; r++)
		for (int c = 0; c < DA_SIZE; c++) {
			int piece = b[r][c];
			if (da_owner(piece) != g->current)
				continue;
			DaMove m;
			memset(&m, 0, sizeof m);
			m.fr = (int8_t)r;
			m.fc = (int8_t)c;
			b[r][c] = DA_EMPTY; /* la pieza deja su casilla libre mientras salta */
			captures_from(b, piece, r, c, &m, &gen);
			b[r][c] = (uint8_t)piece;
		}
	if (gen.n > 0)
		return gen.n;
	/* Movimientos sin comer */
	int n = 0;
	int fwd = g->current == 1 ? -1 : 1;
	for (int r = 0; r < DA_SIZE; r++)
		for (int c = 0; c < DA_SIZE; c++) {
			int piece = b[r][c];
			if (da_owner(piece) != g->current)
				continue;
			for (int dr = -1; dr <= 1; dr += 2)
				for (int dc = -1; dc <= 1; dc += 2) {
					if (!is_king(piece) && dr != fwd)
						continue;
					int rr = r + dr, cc = c + dc;
					while (inside(rr, cc) && b[rr][cc] == DA_EMPTY && n < DA_MAX_MOVES) {
						DaMove* m = &out[n++];
						memset(m, 0, sizeof *m);
						m->fr = (int8_t)r;
						m->fc = (int8_t)c;
						m->path[0][0] = (int8_t)rr;
						m->path[0][1] = (int8_t)cc;
						m->n_path = 1;
						if (!is_king(piece))
							break;
						rr += dr;
						cc += dc;
					}
				}
		}
	return n;
}

int da_count(const Damas* g, int side)
{
	int n = 0;
	for (int r = 0; r < DA_SIZE; r++)
		for (int c = 0; c < DA_SIZE; c++)
			n += da_owner(g->b[r][c]) == side;
	return n;
}

/* Aplica la jugada sin comprobarla */
static void apply_raw(Damas* g, const DaMove* m)
{
	int piece = g->b[m->fr][m->fc];
	g->b[m->fr][m->fc] = DA_EMPTY;
	for (int i = 0; i < m->n_caps; i++)
		g->b[m->caps[i][0]][m->caps[i][1]] = DA_EMPTY;
	int tr = m->path[m->n_path - 1][0], tc = m->path[m->n_path - 1][1];
	if (piece == DA_WHITE && tr == 0)
		piece = DA_WHITE_KING;
	if (piece == DA_BLACK && tr == DA_SIZE - 1)
		piece = DA_BLACK_KING;
	g->b[tr][tc] = (uint8_t)piece;
	g->quiet = (m->n_caps > 0 || !is_king(piece)) ? 0 : g->quiet + 1;
	g->current = 3 - g->current;
}

static void check_end(Damas* g)
{
	DaMove moves[DA_MAX_MOVES];
	if (da_moves(g, moves) == 0)
		g->winner = 3 - g->current;
	else if (g->quiet >= 80)
		g->winner = 3;
}

bool da_apply(Damas* g, const DaMove* m)
{
	if (g->winner)
		return false;
	DaMove moves[DA_MAX_MOVES];
	int n = da_moves(g, moves);
	for (int i = 0; i < n; i++)
		if (memcmp(&moves[i], m, sizeof *m) == 0) {
			apply_raw(g, m);
			g->last = *m;
			g->has_last = true;
			check_end(g);
			return true;
		}
	return false;
}

/* ---------------------------------------------------------------------------
 * IA: negamax con poda alfa-beta
 * ------------------------------------------------------------------------- */

static int evaluate(const Damas* g, int side)
{
	int score = 0;
	for (int r = 0; r < DA_SIZE; r++)
		for (int c = 0; c < DA_SIZE; c++) {
			int p = g->b[r][c];
			if (!p)
				continue;
			int v = is_king(p) ? 320 : 100;
			if (!is_king(p))
				v += da_owner(p) == 1 ? (7 - r) * 4 : r * 4; /* avanzar */
			if (c == 0 || c == 7)
				v += 4; /* en el borde no se la pueden comer */
			score += da_owner(p) == side ? v : -v;
		}
	return score;
}

static int negamax(const Damas* g, int depth, int alpha, int beta)
{
	DaMove moves[DA_MAX_MOVES];
	int n = da_moves(g, moves);
	if (n == 0)
		return -100000 - depth;
	if (depth == 0)
		return evaluate(g, g->current);
	for (int i = 0; i < n; i++) {
		Damas next = *g;
		apply_raw(&next, &moves[i]);
		int v = -negamax(&next, depth - 1, -beta, -alpha);
		if (v > alpha)
			alpha = v;
		if (alpha >= beta)
			break;
	}
	return alpha;
}

int da_ai_choose(Damas* g, DaMove* out)
{
	DaMove moves[DA_MAX_MOVES];
	int n = da_moves(g, moves);
	if (n == 0)
		return 0;
	int best = 0, best_v = -10000000;
	for (int i = 0; i < n; i++) {
		Damas next = *g;
		apply_raw(&next, &moves[i]);
		int v = -negamax(&next, g->level - 1, -10000000, 10000000) + rng_range(&g->rng, 6);
		if (v > best_v) {
			best_v = v;
			best = i;
		}
	}
	*out = moves[best];
	return n;
}
