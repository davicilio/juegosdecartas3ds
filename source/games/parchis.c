#include "games/parchis.h"

#include <string.h>

#define LANE_START (PA_RING - 4) /* 64: primer paso dentro del pasillo */

int pa_exit(int p)
{
	return 5 + 17 * p;
}

int pa_square(int p, int steps)
{
	if (steps < 0 || steps >= LANE_START)
		return 0;
	return (pa_exit(p) - 1 + steps) % PA_RING + 1;
}

bool pa_safe(int sq)
{
	switch (sq) {
	case 5: case 12: case 17: case 22: case 29: case 34:
	case 39: case 46: case 51: case 56: case 63: case 68:
		return true;
	default:
		return false;
	}
}

void pa_init(Parchis* g, uint32_t seed)
{
	memset(g, 0, sizeof *g);
	rng_seed(&g->rng, seed);
	pa_new_game(g);
}

void pa_new_game(Parchis* g)
{
	for (int p = 0; p < PA_PLAYERS; p++)
		for (int i = 0; i < PA_PIECES; i++)
			g->pos[p][i] = PA_HOME;
	g->current = 0;
	g->winner = -1;
	g->sixes = 0;
	g->bonus_n = 0;
	g->last_piece = -1;
	g->last_player = -1;
	g->phase = PA_ROLL;
}

/* Fichas en una casilla del circuito */
static int count_at(const Parchis* g, int sq, int* owner)
{
	int n = 0;
	for (int p = 0; p < PA_PLAYERS; p++)
		for (int i = 0; i < PA_PIECES; i++)
			if (pa_square(p, g->pos[p][i]) == sq) {
				n++;
				if (owner)
					*owner = p;
			}
	return n;
}

static bool barrier_at(const Parchis* g, int sq)
{
	for (int p = 0; p < PA_PLAYERS; p++) {
		int n = 0;
		for (int i = 0; i < PA_PIECES; i++)
			n += pa_square(p, g->pos[p][i]) == sq;
		if (n >= 2)
			return true;
	}
	return false;
}

static int lane_count(const Parchis* g, int p, int steps)
{
	int n = 0;
	for (int i = 0; i < PA_PIECES; i++)
		n += g->pos[p][i] == steps;
	return n;
}

static bool all_out(const Parchis* g, int p)
{
	for (int i = 0; i < PA_PIECES; i++)
		if (g->pos[p][i] == PA_HOME)
			return false;
	return true;
}

int pa_target(const Parchis* g, int piece)
{
	int p = g->current;
	if (g->phase != PA_MOVE || piece < 0 || piece >= PA_PIECES)
		return -2;
	int from = g->pos[p][piece];
	if (from == PA_GOAL)
		return -2;
	bool is_die = g->move == g->die && g->bonus_n == 0;
	if (from == PA_HOME) {
		/* Solo con el 5 del dado y si en la salida no hay dos de su color */
		if (!is_die || g->die != 5)
			return -2;
		int own = 0, owner = -1;
		int n = count_at(g, pa_exit(p), &owner);
		for (int i = 0; i < PA_PIECES; i++)
			own += pa_square(p, g->pos[p][i]) == pa_exit(p);
		if (own >= 2 || (n >= 2 && own >= 1))
			return -2;
		return 0;
	}
	int to = from + g->move;
	if (to > PA_GOAL)
		return -2;
	/* No se puede pasar ni caer en una barrera */
	for (int s = from + 1; s <= to; s++) {
		int sq = pa_square(p, s);
		if (sq && barrier_at(g, sq))
			return -2;
		if (!sq && s < PA_GOAL && lane_count(g, p, s) >= 2)
			return -2;
	}
	int sq = pa_square(p, to);
	if (sq && count_at(g, sq, NULL) >= 2)
		return -2;
	return to;
}

bool pa_can_move(const Parchis* g, int piece)
{
	return pa_target(g, piece) >= -1;
}

/* Si hay que sacar ficha (un 5 y fichas en casa), solo vale eso */
static bool must_exit(const Parchis* g)
{
	if (g->move != 5 || g->bonus_n || g->die != 5)
		return false;
	int p = g->current;
	for (int i = 0; i < PA_PIECES; i++)
		if (g->pos[p][i] == PA_HOME && pa_target(g, i) == 0)
			return true;
	return false;
}

static bool any_move(const Parchis* g)
{
	for (int i = 0; i < PA_PIECES; i++)
		if (pa_can_move(g, i) && (!must_exit(g) || g->pos[g->current][i] == PA_HOME))
			return true;
	return false;
}

static void next_turn(Parchis* g)
{
	g->sixes = 0;
	g->last_piece = -1;
	g->current = (g->current + 1) % PA_PLAYERS;
	g->phase = PA_ROLL;
}

/* Tras mover: premios pendientes, volver a tirar o pasar el turno */
static void after_move(Parchis* g)
{
	while (g->bonus_n > 0) {
		g->move = g->bonus[0];
		memmove(g->bonus, g->bonus + 1, sizeof(int) * (g->bonus_n - 1));
		g->bonus_n--;
		g->phase = PA_MOVE;
		if (any_move(g))
			return;
	}
	if (g->die == 6 && g->sixes > 0)
		g->phase = PA_ROLL; /* con un 6 se repite */
	else
		next_turn(g);
}

int pa_roll(Parchis* g, int die)
{
	if (g->phase != PA_ROLL || g->winner >= 0)
		return 0;
	int p = g->current;
	g->die = die >= 1 && die <= 6 ? die : 1 + rng_range(&g->rng, 6);
	g->last_three_sixes = g->last_no_move = false;
	g->bonus_n = 0;
	if (g->die == 6) {
		if (++g->sixes == 3) {
			/* Tres seises: la última ficha movida vuelve a casa */
			int i = g->last_piece;
			if (i >= 0 && g->pos[p][i] != PA_GOAL && g->pos[p][i] < LANE_START)
				g->pos[p][i] = PA_HOME;
			g->last_three_sixes = true;
			next_turn(g);
			return g->die;
		}
	} else {
		g->sixes = 0;
	}
	g->move = g->die == 6 && all_out(g, p) ? 7 : g->die;
	g->phase = PA_MOVE;
	if (!any_move(g)) {
		g->last_no_move = true;
		after_move(g);
	}
	return g->die;
}

bool pa_move(Parchis* g, int piece)
{
	int p = g->current;
	int to = pa_target(g, piece);
	if (to < -1 || (must_exit(g) && g->pos[p][piece] != PA_HOME))
		return false;
	int from = g->pos[p][piece];
	g->last_player = p;
	g->last_moved = piece;
	g->last_from = from;
	g->last_to = to;
	g->last_goal = false;
	g->last_captured_player = -1;
	g->pos[p][piece] = to;
	g->last_piece = piece;
	/* ¿Se come a alguien? Fuera de los seguros, o al salir de casa */
	int sq = pa_square(p, to);
	if (sq && (!pa_safe(sq) || from == PA_HOME)) {
		for (int q = 0; q < PA_PLAYERS; q++) {
			if (q == p)
				continue;
			for (int i = 0; i < PA_PIECES; i++)
				if (pa_square(q, g->pos[q][i]) == sq) {
					g->pos[q][i] = PA_HOME;
					g->last_captured_player = q;
					g->last_captured_piece = i;
					g->bonus[g->bonus_n++] = 20;
					q = PA_PLAYERS; /* solo una */
					break;
				}
		}
	}
	if (to == PA_GOAL) {
		g->last_goal = true;
		if (pa_in_goal(g, p) == PA_PIECES) {
			g->winner = p;
			g->phase = PA_OVER;
			return true;
		}
		g->bonus[g->bonus_n++] = 10;
	}
	after_move(g);
	return true;
}

int pa_in_goal(const Parchis* g, int p)
{
	int n = 0;
	for (int i = 0; i < PA_PIECES; i++)
		n += g->pos[p][i] == PA_GOAL;
	return n;
}

/* ¿Puede alguien comerse una ficha en esta casilla con un dado? */
static int danger(const Parchis* g, int p, int sq)
{
	if (!sq || pa_safe(sq))
		return 0;
	int d = 0;
	for (int q = 0; q < PA_PLAYERS; q++) {
		if (q == p)
			continue;
		for (int i = 0; i < PA_PIECES; i++) {
			int qs = pa_square(q, g->pos[q][i]);
			if (!qs)
				continue;
			int dist = (sq - qs + PA_RING) % PA_RING;
			if (dist >= 1 && dist <= 7)
				d++;
		}
	}
	return d;
}

int pa_ai_choose(const Parchis* g)
{
	int p = g->current, best = -1, best_v = -100000;
	for (int i = 0; i < PA_PIECES; i++) {
		if (!pa_can_move(g, i) || (must_exit(g) && g->pos[p][i] != PA_HOME))
			continue;
		int from = g->pos[p][i], to = pa_target(g, i);
		int v = to - (from < 0 ? -10 : from);
		int sq = pa_square(p, to);
		if (from == PA_HOME)
			v += 40;
		if (to == PA_GOAL)
			v += 80;
		else if (!sq)
			v += 30; /* a salvo en el pasillo */
		if (sq && (!pa_safe(sq) || from == PA_HOME)) {
			int owner = -1;
			if (count_at(g, sq, &owner) == 1 && owner != p)
				v += 100; /* se come una */
		}
		if (sq && pa_safe(sq))
			v += 15;
		v -= danger(g, p, sq) * 25;
		v += danger(g, p, pa_square(p, from)) * 20; /* sale de peligro */
		if (v > best_v) {
			best_v = v;
			best = i;
		}
	}
	return best;
}
