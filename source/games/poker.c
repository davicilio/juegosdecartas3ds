#include "games/poker.h"

#include <string.h>

/* ---------------------------------------------------------------------------
 * Valoración de jugadas
 * ------------------------------------------------------------------------- */

/* score = categoría << 20 | cinco valores de desempate de 4 bits */
static uint32_t pack(int cat, const int* k, int n)
{
	uint32_t s = (uint32_t)cat << 20;
	for (int i = 0; i < 5; i++)
		s |= (uint32_t)(i < n ? k[i] : 0) << (16 - 4 * i);
	return s;
}

/* Carta más alta de una escalera en el conjunto de valores (0 si no hay) */
static int straight_high(uint16_t mask)
{
	if (mask & (1 << 14))
		mask |= 1 << 1; /* el as también vale 1 */
	for (int hi = 14; hi >= 5; hi--) {
		uint16_t need = (uint16_t)(0x1F << (hi - 4));
		if ((mask & need) == need)
			return hi;
	}
	return 0;
}

uint32_t pk_evaluate(const PkCard* cards, int n)
{
	int count[15] = { 0 };
	uint16_t suit_mask[4] = { 0 };
	int suit_count[4] = { 0 };
	uint16_t mask = 0;
	for (int i = 0; i < n; i++) {
		count[cards[i].rank]++;
		suit_mask[cards[i].suit] |= (uint16_t)(1 << cards[i].rank);
		suit_count[cards[i].suit]++;
		mask |= (uint16_t)(1 << cards[i].rank);
	}
	/* Escalera de color y color */
	for (int s = 0; s < 4; s++) {
		if (suit_count[s] < 5)
			continue;
		int sf = straight_high(suit_mask[s]);
		if (sf)
			return pack(8, &sf, 1);
		int k[5], m = 0;
		for (int r = 14; r >= 2 && m < 5; r--)
			if (suit_mask[s] & (1 << r))
				k[m++] = r;
		/* Puede haber póker o full a la vez que color: se miran antes */
		int quads = 0, trips = 0, pairs = 0;
		for (int r = 2; r <= 14; r++) {
			quads += count[r] == 4;
			trips += count[r] == 3;
			pairs += count[r] == 2;
		}
		if (!quads && !(trips && (pairs || trips > 1)))
			return pack(5, k, 5);
	}
	int quad = 0, trip[2] = { 0 }, nt = 0, pair[3] = { 0 }, np = 0;
	for (int r = 14; r >= 2; r--) {
		if (count[r] == 4)
			quad = r;
		else if (count[r] == 3 && nt < 2)
			trip[nt++] = r;
		else if (count[r] == 2 && np < 3)
			pair[np++] = r;
	}
	if (quad) {
		int k[2] = { quad, 0 };
		for (int r = 14; r >= 2; r--)
			if (r != quad && count[r]) {
				k[1] = r;
				break;
			}
		return pack(7, k, 2);
	}
	if (nt && (np || nt > 1)) {
		int k[2] = { trip[0], nt > 1 ? (trip[1] > pair[0] ? trip[1] : pair[0]) : pair[0] };
		return pack(6, k, 2);
	}
	/* Color sin póker ni full se ha devuelto arriba */
	for (int s = 0; s < 4; s++)
		if (suit_count[s] >= 5) {
			int k[5], m = 0;
			for (int r = 14; r >= 2 && m < 5; r--)
				if (suit_mask[s] & (1 << r))
					k[m++] = r;
			return pack(5, k, 5);
		}
	int st = straight_high(mask);
	if (st)
		return pack(4, &st, 1);
	int k[5], m = 0;
	if (nt) {
		k[m++] = trip[0];
		for (int r = 14; r >= 2 && m < 3; r--)
			if (r != trip[0] && count[r])
				k[m++] = r;
		return pack(3, k, m);
	}
	if (np >= 2) {
		k[m++] = pair[0];
		k[m++] = pair[1];
		for (int r = 14; r >= 2 && m < 3; r--)
			if (r != pair[0] && r != pair[1] && count[r])
				k[m++] = r;
		return pack(2, k, m);
	}
	if (np == 1) {
		k[m++] = pair[0];
		for (int r = 14; r >= 2 && m < 4; r--)
			if (r != pair[0] && count[r])
				k[m++] = r;
		return pack(1, k, m);
	}
	for (int r = 14; r >= 2 && m < 5; r--)
		if (count[r])
			k[m++] = r;
	return pack(0, k, m);
}

int pk_category(uint32_t score)
{
	return (int)(score >> 20);
}

const char* pk_category_name(int category)
{
	static const char* const names[9] = { "Carta alta", "Pareja", "Doble pareja", "Trío", "Escalera", "Color", "Full",
		"Póker", "Escalera de color" };
	return category >= 0 && category < 9 ? names[category] : "";
}

/* ---------------------------------------------------------------------------
 * Partida
 * ------------------------------------------------------------------------- */

void pk_init(Poker* g, uint32_t seed)
{
	pk_init_variant(g, PK_HOLDEM, seed);
}

void pk_init_variant(Poker* g, PkVariant variant, uint32_t seed)
{
	pk_init_stack(g, variant, seed, PK_START);
}

void pk_init_stack(Poker* g, PkVariant variant, uint32_t seed, int my_chips)
{
	g->variant = variant;
	g->hole_n = variant == PK_OMAHA ? 4 : variant == PK_DRAW ? 5 : 2;
	rng_seed(&g->rng, seed);
	for (int p = 0; p < PK_PLAYERS; p++) {
		g->chips[p] = PK_START;
		g->out[p] = false;
	}
	g->chips[0] = my_chips;
	g->dealer = PK_PLAYERS - 1;
	g->hands_played = 0;
	pk_new_hand(g);
}

static int next_seat(const Poker* g, int from)
{
	for (int k = 1; k <= PK_PLAYERS; k++) {
		int q = (from + k) % PK_PLAYERS;
		if (!g->out[q])
			return q;
	}
	return from;
}

bool pk_active(const Poker* g, int player)
{
	return !g->out[player] && !g->folded[player];
}

int pk_players_left(const Poker* g)
{
	int n = 0;
	for (int p = 0; p < PK_PLAYERS; p++)
		n += !g->out[p];
	return n;
}

static void put(Poker* g, int p, int amount)
{
	if (amount > g->chips[p])
		amount = g->chips[p];
	g->chips[p] -= amount;
	g->bet[p] += amount;
	g->invested[p] += amount;
	if (g->bet[p] > g->to_call)
		g->to_call = g->bet[p];
}

/* Jugadores que aún pueden apostar (en la mano y con fichas) */
static int can_bet_count(const Poker* g)
{
	int n = 0;
	for (int p = 0; p < PK_PLAYERS; p++)
		n += pk_active(g, p) && g->chips[p] > 0;
	return n;
}

static int active_count(const Poker* g)
{
	int n = 0;
	for (int p = 0; p < PK_PLAYERS; p++)
		n += pk_active(g, p);
	return n;
}

static int next_to_act(const Poker* g, int from)
{
	for (int k = 1; k <= PK_PLAYERS; k++) {
		int q = (from + k) % PK_PLAYERS;
		if (pk_active(g, q) && g->chips[q] > 0)
			return q;
	}
	return -1;
}

static PkCard draw(Poker* g)
{
	return g->deck[--g->deck_count];
}

void pk_new_hand(Poker* g)
{
	g->deck_count = 0;
	for (int s = 0; s < 4; s++)
		for (int r = 2; r <= 14; r++)
			g->deck[g->deck_count++] = (PkCard){ (uint8_t)r, (uint8_t)s };
	for (int i = g->deck_count - 1; i > 0; i--) {
		int j = rng_range(&g->rng, i + 1);
		PkCard t = g->deck[i];
		g->deck[i] = g->deck[j];
		g->deck[j] = t;
	}
	for (int p = 0; p < PK_PLAYERS; p++) {
		g->out[p] = g->chips[p] <= 0;
		g->folded[p] = g->out[p];
		g->bet[p] = g->invested[p] = 0;
		g->acted[p] = false;
		g->won[p] = 0;
		g->score[p] = 0;
		g->last_action[p] = -1;
		g->last_amount[p] = 0;
		g->drew[p] = false;
		g->discards[p] = 0;
	}
	g->board_count = 0;
	g->to_call = 0;
	g->min_raise = PK_BIG;
	g->dealer = next_seat(g, g->dealer);
	for (int k = 0; k < g->hole_n; k++)
		for (int p = 0; p < PK_PLAYERS; p++)
			if (!g->out[p])
				g->hole[p][k] = draw(g);
	/* Ciegas (con dos jugadores, el que reparte pone la pequeña) */
	int sb = pk_players_left(g) == 2 ? g->dealer : next_seat(g, g->dealer);
	int bb = next_seat(g, sb);
	put(g, sb, PK_SMALL);
	put(g, bb, PK_BIG);
	g->stage = PK_PREFLOP;
	g->current = next_to_act(g, bb);
	if (g->current < 0)
		g->current = bb;
	g->hands_played++;
}

int pk_pot(const Poker* g)
{
	int pot = 0;
	for (int p = 0; p < PK_PLAYERS; p++)
		pot += g->invested[p];
	return pot;
}

int pk_to_call(const Poker* g, int player)
{
	int need = g->to_call - g->bet[player];
	return need < g->chips[player] ? need : g->chips[player];
}

bool pk_can_check(const Poker* g)
{
	return g->bet[g->current] >= g->to_call;
}

int pk_min_raise_to(const Poker* g)
{
	int p = g->current;
	int to = g->to_call + g->min_raise;
	int max = g->bet[p] + g->chips[p];
	return to < max ? to : max;
}

int pk_max_raise_to(const Poker* g)
{
	int p = g->current;
	return g->bet[p] + g->chips[p];
}

/* Sin fichas (o solo queda uno): se acaba la partida */
static void check_game_over(Poker* g)
{
	int with_chips = 0;
	for (int p = 0; p < PK_PLAYERS; p++)
		with_chips += g->chips[p] > 0;
	if (g->chips[0] <= 0 || with_chips <= 1)
		g->stage = PK_GAME_OVER;
}

/* Reparte el bote entre los que han llegado al final, por capas: cada uno
 * solo puede ganar de cada rival lo mismo que ha puesto él (botes secundarios) */
static void showdown(Poker* g)
{
	for (int p = 0; p < PK_PLAYERS; p++)
		if (pk_active(g, p))
			g->score[p] = pk_player_score(g, p);
	int prev = 0;
	while (true) {
		int level = 0;
		for (int p = 0; p < PK_PLAYERS; p++)
			if (pk_active(g, p) && g->invested[p] > prev && (level == 0 || g->invested[p] < level))
				level = g->invested[p];
		if (level == 0)
			break;
		int pot = 0;
		for (int p = 0; p < PK_PLAYERS; p++) {
			int a = g->invested[p] < level ? g->invested[p] : level;
			int b = g->invested[p] < prev ? g->invested[p] : prev;
			pot += a - b;
		}
		uint32_t best = 0;
		for (int p = 0; p < PK_PLAYERS; p++)
			if (pk_active(g, p) && g->invested[p] >= level && g->score[p] > best)
				best = g->score[p];
		int winners[PK_PLAYERS], nw = 0;
		for (int p = 0; p < PK_PLAYERS; p++)
			if (pk_active(g, p) && g->invested[p] >= level && g->score[p] == best)
				winners[nw++] = p;
		for (int i = 0; i < nw; i++)
			g->won[winners[i]] += pot / nw;
		g->won[winners[0]] += pot % nw;
		prev = level;
	}
	/* Lo que pusieron los retirados por encima de todo lo demás, al último ganador */
	int rest = 0;
	for (int p = 0; p < PK_PLAYERS; p++)
		if (g->invested[p] > prev)
			rest += g->invested[p] - prev;
	if (rest > 0)
		for (int p = 0; p < PK_PLAYERS; p++)
			if (pk_active(g, p) && g->invested[p] == prev && g->won[p] > 0) {
				g->won[p] += rest;
				break;
			}
	for (int p = 0; p < PK_PLAYERS; p++)
		g->chips[p] += g->won[p];
	g->stage = PK_SHOWDOWN;
	check_game_over(g);
}

static void finish_by_fold(Poker* g)
{
	for (int p = 0; p < PK_PLAYERS; p++)
		if (pk_active(g, p)) {
			g->won[p] = pk_pot(g);
			g->chips[p] += g->won[p];
		}
	g->stage = PK_HAND_OVER;
	check_game_over(g);
}

/* Siguiente fase: se reparten cartas y empieza otra ronda de apuestas */
static void next_stage(Poker* g)
{
	for (int p = 0; p < PK_PLAYERS; p++) {
		g->bet[p] = 0;
		g->acted[p] = false;
	}
	g->to_call = 0;
	g->min_raise = PK_BIG;
	if (g->variant == PK_DRAW && g->stage == PK_PREFLOP) {
		/* Descarte: empiezan a cambiar desde la izquierda de quien reparte */
		g->stage = PK_DISCARD;
		for (int k = 1; k <= PK_PLAYERS; k++) {
			int q = (g->dealer + k) % PK_PLAYERS;
			if (pk_active(g, q)) {
				g->current = q;
				break;
			}
		}
		return;
	}
	if (g->stage == PK_PREFLOP) {
		for (int i = 0; i < 3; i++)
			g->board[g->board_count++] = draw(g);
		g->stage = PK_FLOP;
	} else if (g->stage == PK_FLOP) {
		g->board[g->board_count++] = draw(g);
		g->stage = PK_TURN;
	} else if (g->stage == PK_TURN) {
		g->board[g->board_count++] = draw(g);
		g->stage = PK_RIVER;
	} else {
		showdown(g);
		return;
	}
	/* Si ya no puede apostar más de uno, se reparte todo sin parar */
	if (can_bet_count(g) <= 1) {
		next_stage(g);
		return;
	}
	g->current = next_to_act(g, g->dealer);
}

static bool round_done(const Poker* g)
{
	for (int p = 0; p < PK_PLAYERS; p++)
		if (pk_active(g, p) && g->chips[p] > 0 && (!g->acted[p] || g->bet[p] < g->to_call))
			return false;
	return true;
}

bool pk_act(Poker* g, PkAction action, int amount)
{
	if (g->stage > PK_RIVER)
		return false;
	int p = g->current;
	g->last_action[p] = action;
	switch (action) {
	case PK_FOLD:
		g->folded[p] = true;
		break;
	case PK_CHECK_CALL:
		g->last_amount[p] = pk_to_call(g, p);
		put(g, p, pk_to_call(g, p));
		break;
	case PK_RAISE:
	case PK_ALL_IN: {
		int max = pk_max_raise_to(g);
		int to = action == PK_ALL_IN ? max : amount;
		if (to > max)
			to = max;
		if (to < pk_min_raise_to(g) && to < max)
			to = pk_min_raise_to(g);
		if (to <= g->to_call) {
			/* No llega a subir: es igualar */
			g->last_action[p] = PK_CHECK_CALL;
			g->last_amount[p] = pk_to_call(g, p);
			put(g, p, pk_to_call(g, p));
			break;
		}
		int raise = to - g->to_call;
		if (raise >= g->min_raise)
			g->min_raise = raise;
		g->last_amount[p] = to - g->bet[p];
		put(g, p, to - g->bet[p]);
		/* Los demás tienen que volver a hablar */
		for (int q = 0; q < PK_PLAYERS; q++)
			if (q != p)
				g->acted[q] = false;
		if (g->chips[p] == 0)
			g->last_action[p] = PK_ALL_IN;
		break;
	}
	}
	g->acted[p] = true;

	if (active_count(g) == 1) {
		finish_by_fold(g);
		return true;
	}
	if (round_done(g)) {
		next_stage(g);
		return true;
	}
	int n = next_to_act(g, p);
	if (n < 0) {
		next_stage(g);
		return true;
	}
	g->current = n;
	return true;
}

/* ---------------------------------------------------------------------------
 * Omaha y descarte
 * ------------------------------------------------------------------------- */

static uint32_t score_with(int variant, const PkCard* hole, const PkCard* board, int nb)
{
	if (variant == PK_DRAW)
		return pk_evaluate(hole, 5);
	if (variant == PK_HOLDEM) {
		PkCard seven[7] = { hole[0], hole[1] };
		for (int i = 0; i < nb; i++)
			seven[2 + i] = board[i];
		return pk_evaluate(seven, 2 + nb);
	}
	/* Omaha: dos propias y tres de la mesa, exactamente */
	if (nb < 3)
		return pk_evaluate(hole, 4);
	uint32_t best = 0;
	for (int a = 0; a < 4; a++)
		for (int b = a + 1; b < 4; b++)
			for (int i = 0; i < nb; i++)
				for (int j = i + 1; j < nb; j++)
					for (int k = j + 1; k < nb; k++) {
						PkCard five[5] = { hole[a], hole[b], board[i], board[j], board[k] };
						uint32_t s = pk_evaluate(five, 5);
						if (s > best)
							best = s;
					}
	return best;
}

uint32_t pk_player_score(const Poker* g, int p)
{
	return score_with(g->variant, g->hole[p], g->board, g->board_count);
}

bool pk_can_discard(const Poker* g, const bool* sel)
{
	if (g->stage != PK_DISCARD)
		return false;
	int n = 0;
	bool ace_kept = false;
	for (int i = 0; i < 5; i++) {
		n += sel[i];
		if (!sel[i] && g->hole[g->current][i].rank == 14)
			ace_kept = true;
	}
	return n <= 3 || (n == 4 && ace_kept);
}

bool pk_discard(Poker* g, const bool* sel)
{
	if (!pk_can_discard(g, sel))
		return false;
	int p = g->current;
	int n = 0;
	for (int i = 0; i < 5; i++)
		if (sel[i]) {
			g->hole[p][i] = draw(g);
			n++;
		}
	g->discards[p] = n;
	g->drew[p] = true;
	for (int k = 1; k <= PK_PLAYERS; k++) {
		int q = (p + k) % PK_PLAYERS;
		if (pk_active(g, q) && !g->drew[q]) {
			g->current = q;
			return true;
		}
	}
	/* Han cambiado todos: segunda ronda de apuestas */
	if (can_bet_count(g) <= 1) {
		showdown(g);
		return true;
	}
	g->stage = PK_RIVER;
	g->current = next_to_act(g, g->dealer);
	return true;
}

void pk_ai_discard(const Poker* g, int p, bool* sel)
{
	const PkCard* h = g->hole[p];
	int count[15] = { 0 }, suits[4] = { 0 };
	for (int i = 0; i < 5; i++) {
		count[h[i].rank]++;
		suits[h[i].suit]++;
		sel[i] = false;
	}
	int cat = pk_category(pk_evaluate(h, 5));
	if (cat >= 4 && cat != 7)
		return; /* escalera o mejor: se queda con todo */
	if (cat >= 1) {
		/* Se queda con las cartas repetidas */
		for (int i = 0; i < 5; i++)
			sel[i] = count[h[i].rank] < 2;
		return;
	}
	for (int s = 0; s < 4; s++)
		if (suits[s] == 4) {
			for (int i = 0; i < 5; i++)
				sel[i] = h[i].suit != s; /* a por el color */
			return;
		}
	/* Nada: se queda con las dos más altas (o solo con el as) */
	int hi = 0, hi2 = -1;
	for (int i = 1; i < 5; i++)
		if (h[i].rank > h[hi].rank)
			hi = i;
	for (int i = 0; i < 5; i++)
		if (i != hi && (hi2 < 0 || h[i].rank > h[hi2].rank))
			hi2 = i;
	bool ace_alone = h[hi].rank == 14 && h[hi2].rank < 11;
	for (int i = 0; i < 5; i++)
		sel[i] = i != hi && (ace_alone || i != hi2);
}

bool pk_hand_finished(const Poker* g)
{
	return g->stage >= PK_SHOWDOWN;
}

/* ---------------------------------------------------------------------------
 * IA
 * ------------------------------------------------------------------------- */

int pk_equity(Poker* g, int player, int samples)
{
	/* Cartas que no se ven: todas menos las propias y las de la mesa */
	PkCard unseen[52];
	int nu = 0;
	int hn = g->hole_n;
	for (int s = 0; s < 4; s++)
		for (int r = 2; r <= 14; r++) {
			bool seen = false;
			for (int i = 0; i < hn; i++)
				seen |= g->hole[player][i].rank == r && g->hole[player][i].suit == s;
			for (int i = 0; i < g->board_count; i++)
				seen |= g->board[i].rank == r && g->board[i].suit == s;
			if (!seen)
				unseen[nu++] = (PkCard){ (uint8_t)r, (uint8_t)s };
		}
	int opponents = active_count(g) - 1;
	if (opponents < 1)
		return 100;
	/* En el Omaha cada jugada cuesta 60 valoraciones: menos muestras */
	if (g->variant == PK_OMAHA && samples > 70)
		samples = 70;
	/* Descarte antes de cambiar: se simula el cambio propio */
	bool sel[5] = { false };
	bool will_draw = g->variant == PK_DRAW && g->stage < PK_DISCARD;
	if (will_draw)
		pk_ai_discard(g, player, sel);
	int wins2 = 0;
	for (int k = 0; k < samples; k++) {
		int need = (g->variant == PK_DRAW ? 0 : PK_BOARD - g->board_count) + opponents * hn + (will_draw ? 4 : 0);
		if (need > nu)
			need = nu;
		for (int i = 0; i < need; i++) {
			int j = i + rng_range(&g->rng, nu - i);
			PkCard t = unseen[i];
			unseen[i] = unseen[j];
			unseen[j] = t;
		}
		int u = 0;
		PkCard board[PK_BOARD];
		int nb = 0;
		if (g->variant != PK_DRAW) {
			for (int i = 0; i < g->board_count; i++)
				board[nb++] = g->board[i];
			while (nb < PK_BOARD)
				board[nb++] = unseen[u++];
		}
		PkCard mine_cards[PK_MAX_HOLE];
		for (int i = 0; i < hn; i++)
			mine_cards[i] = will_draw && sel[i] ? unseen[u++] : g->hole[player][i];
		uint32_t mine = score_with(g->variant, mine_cards, board, nb);
		int result = 2; /* 2 gana, 1 empata, 0 pierde */
		for (int o = 0; o < opponents && result > 0; o++) {
			uint32_t s = score_with(g->variant, &unseen[u], board, nb);
			u += hn;
			if (s > mine)
				result = 0;
			else if (s == mine)
				result = 1;
		}
		wins2 += result;
	}
	return wins2 * 50 / samples;
}

PkAction pk_ai_decide(Poker* g, int* amount)
{
	int p = g->current;
	int equity = pk_equity(g, p, 220);
	int call = pk_to_call(g, p);
	int pot = pk_pot(g);
	/* Probabilidad que hace falta para que igualar compense */
	int needed = call > 0 ? call * 100 / (pot + call) : 0;
	int r = rng_range(&g->rng, 100);
	int fair = 100 / (active_count(g));

	/* Muy buena mano: subir (a veces fuerte) */
	if (equity > fair + 30 || (equity > fair + 18 && r < 50)) {
		int to = g->to_call + (pot / 2 > PK_BIG ? pot / 2 : PK_BIG);
		if (equity > 85 && r < 30)
			return PK_ALL_IN;
		if (to >= pk_max_raise_to(g))
			return PK_ALL_IN;
		*amount = to;
		return PK_RAISE;
	}
	/* Farol de vez en cuando */
	if (call == 0 && r < 8 && g->stage >= PK_FLOP) {
		*amount = g->to_call + PK_BIG * 2;
		return PK_RAISE;
	}
	if (call == 0)
		return PK_CHECK_CALL;
	if (equity + 5 >= needed || (call <= PK_BIG && equity >= fair - 10))
		return PK_CHECK_CALL;
	return PK_FOLD;
}
