#include "games/solitario.h"

void sol_init(Solitario* g, uint32_t seed)
{
	rng_seed(&g->rng, seed);
	sol_new_game(g);
}

void sol_new_game(Solitario* g)
{
	Deck d;
	deck_init(&d);
	deck_shuffle(&d, &g->rng);
	int k = 0;
	for (int c = 0; c < SOL_COLUMNS; c++) {
		g->col[c].count = c + 1;
		g->col[c].hidden = c;
		for (int i = 0; i <= c; i++)
			g->col[c].cards[i] = d.cards[k++];
	}
	g->stock_count = 0;
	while (k < DECK_SIZE)
		g->stock[g->stock_count++] = d.cards[k++];
	g->waste_count = 0;
	for (int s = 0; s < NUM_SUITS; s++)
		g->foundation[s] = 0;
	g->moves = 0;
	g->passes = 0;
	g->won = false;
}

bool sol_draw(Solitario* g)
{
	if (g->won)
		return false;
	if (g->stock_count > 0) {
		g->waste[g->waste_count++] = g->stock[--g->stock_count];
		g->moves++;
		return true;
	}
	if (g->waste_count == 0)
		return false;
	/* Se le da la vuelta a los descartes: la primera que salió vuelve a ser la de arriba */
	while (g->waste_count > 0)
		g->stock[g->stock_count++] = g->waste[--g->waste_count];
	g->passes++;
	g->moves++;
	return true;
}

static bool is_column(int pile)
{
	return pile >= SOL_PILE_COLUMN && pile < SOL_PILES;
}

static bool is_foundation(int pile)
{
	return pile >= SOL_PILE_FOUNDATION && pile < SOL_PILE_COLUMN;
}

int sol_top_index(const Solitario* g, int pile)
{
	if (pile == SOL_PILE_WASTE)
		return g->waste_count - 1;
	if (is_foundation(pile))
		return g->foundation[pile - SOL_PILE_FOUNDATION] - 1;
	if (is_column(pile))
		return g->col[pile - SOL_PILE_COLUMN].count - 1;
	return g->stock_count - 1;
}

/* Carta from_index del montón from (debe existir) */
static bool card_at(const Solitario* g, int from, int from_index, Card* out)
{
	if (from == SOL_PILE_WASTE) {
		if (g->waste_count == 0 || from_index != g->waste_count - 1)
			return false;
		*out = g->waste[from_index];
		return true;
	}
	if (is_foundation(from)) {
		int s = from - SOL_PILE_FOUNDATION;
		if (g->foundation[s] == 0 || from_index != g->foundation[s] - 1)
			return false;
		*out = card_make((Suit)s, rank_from_pos(from_index));
		return true;
	}
	if (is_column(from)) {
		const SolColumn* c = &g->col[from - SOL_PILE_COLUMN];
		if (from_index < c->hidden || from_index >= c->count)
			return false;
		*out = c->cards[from_index];
		return true;
	}
	return false;
}

/* Las cartas desde from_index hasta arriba forman una escalera válida */
static bool run_ok(const SolColumn* c, int from_index)
{
	for (int i = from_index; i < c->count - 1; i++) {
		Card a = c->cards[i], b = c->cards[i + 1];
		if (card_pos(b) != card_pos(a) - 1 || a.suit == b.suit)
			return false;
	}
	return true;
}

bool sol_can_move(const Solitario* g, int from, int from_index, int to)
{
	Card c;
	if (g->won || from == to || !card_at(g, from, from_index, &c))
		return false;
	if (is_column(from) && !run_ok(&g->col[from - SOL_PILE_COLUMN], from_index))
		return false;
	int moving = is_column(from) ? g->col[from - SOL_PILE_COLUMN].count - from_index : 1;

	if (is_foundation(to)) {
		int s = to - SOL_PILE_FOUNDATION;
		return moving == 1 && c.suit == s && card_pos(c) == g->foundation[s];
	}
	if (is_column(to)) {
		const SolColumn* dst = &g->col[to - SOL_PILE_COLUMN];
		if (dst->count == 0)
			return c.rank == RANK_REY;
		Card top = dst->cards[dst->count - 1];
		return card_pos(c) == card_pos(top) - 1 && c.suit != top.suit;
	}
	return false;
}

bool sol_move(Solitario* g, int from, int from_index, int to)
{
	if (!sol_can_move(g, from, from_index, to))
		return false;
	Card moving[DECK_SIZE];
	int n = 0;
	if (from == SOL_PILE_WASTE) {
		moving[n++] = g->waste[--g->waste_count];
	} else if (is_foundation(from)) {
		int s = from - SOL_PILE_FOUNDATION;
		moving[n++] = card_make((Suit)s, rank_from_pos(--g->foundation[s]));
	} else {
		SolColumn* c = &g->col[from - SOL_PILE_COLUMN];
		for (int i = from_index; i < c->count; i++)
			moving[n++] = c->cards[i];
		c->count = from_index;
		/* Se descubre la carta que queda arriba */
		if (c->hidden >= c->count && c->count > 0)
			c->hidden = c->count - 1;
		if (c->count == 0)
			c->hidden = 0;
	}
	if (is_foundation(to)) {
		g->foundation[to - SOL_PILE_FOUNDATION]++;
	} else {
		SolColumn* dst = &g->col[to - SOL_PILE_COLUMN];
		for (int i = 0; i < n; i++)
			dst->cards[dst->count++] = moving[i];
	}
	g->moves++;
	int total = 0;
	for (int s = 0; s < NUM_SUITS; s++)
		total += g->foundation[s];
	g->won = total == DECK_SIZE;
	return true;
}

int sol_best_target(const Solitario* g, int from, int from_index)
{
	Card c;
	if (!card_at(g, from, from_index, &c))
		return -1;
	int f = SOL_PILE_FOUNDATION + c.suit;
	if (sol_can_move(g, from, from_index, f))
		return f;
	/* Primero las columnas con cartas; un rey, a una vacía solo si sirve de algo */
	for (int k = 0; k < SOL_COLUMNS; k++) {
		int to = SOL_PILE_COLUMN + k;
		if (g->col[k].count > 0 && sol_can_move(g, from, from_index, to))
			return to;
	}
	bool pointless = is_column(from) && from_index == 0; /* el rey ya está al fondo de una columna */
	for (int k = 0; k < SOL_COLUMNS && !pointless; k++) {
		int to = SOL_PILE_COLUMN + k;
		if (sol_can_move(g, from, from_index, to))
			return to;
	}
	return -1;
}

int sol_first_movable(const Solitario* g, int column)
{
	const SolColumn* c = &g->col[column];
	if (c->count == 0)
		return -1;
	for (int i = c->hidden; i < c->count; i++)
		if (run_ok(c, i))
			return i;
	return c->count - 1;
}

bool sol_auto_step(Solitario* g)
{
	if (g->waste_count > 0) {
		Card c = g->waste[g->waste_count - 1];
		if (sol_move(g, SOL_PILE_WASTE, g->waste_count - 1, SOL_PILE_FOUNDATION + c.suit))
			return true;
	}
	/* La carta más baja que se pueda subir, para no dejar ninguna atrás */
	int best = -1, best_pos = 99;
	for (int k = 0; k < SOL_COLUMNS; k++) {
		const SolColumn* c = &g->col[k];
		if (c->count == 0)
			continue;
		Card top = c->cards[c->count - 1];
		if (sol_can_move(g, SOL_PILE_COLUMN + k, c->count - 1, SOL_PILE_FOUNDATION + top.suit) &&
			card_pos(top) < best_pos) {
			best = k;
			best_pos = card_pos(top);
		}
	}
	if (best < 0)
		return false;
	const SolColumn* c = &g->col[best];
	return sol_move(g, SOL_PILE_COLUMN + best, c->count - 1, SOL_PILE_FOUNDATION + c->cards[c->count - 1].suit);
}

bool sol_can_finish(const Solitario* g)
{
	if (g->won || g->stock_count > 0 || g->waste_count > 0)
		return false;
	for (int k = 0; k < SOL_COLUMNS; k++)
		if (g->col[k].hidden > 0)
			return false;
	return true;
}

bool sol_has_moves(const Solitario* g)
{
	if (g->won)
		return false;
	if (g->stock_count > 0 || g->waste_count > 0)
		return true; /* siempre se puede sacar del mazo o darle la vuelta */
	for (int k = 0; k < SOL_COLUMNS; k++) {
		int from = SOL_PILE_COLUMN + k;
		for (int i = g->col[k].hidden; i < g->col[k].count; i++)
			if (sol_best_target(g, from, i) >= 0)
				return true;
	}
	return false;
}
