#include "games/tresillo.h"

#include <string.h>

static bool black(int suit)
{
	return suit == SUIT_ESPADAS || suit == SUIT_BASTOS;
}

static bool is_ace(Card c, int suit)
{
	return c.rank == 1 && c.suit == suit;
}

bool ts_is_trump(const Tresillo* g, Card c)
{
	if (g->trump < 0)
		return false;
	return c.suit == g->trump || is_ace(c, SUIT_ESPADAS) || is_ace(c, SUIT_BASTOS);
}

int ts_power(const Tresillo* g, Card c)
{
	if (ts_is_trump(g, c)) {
		int t = g->trump;
		if (is_ace(c, SUIT_ESPADAS))
			return 100; /* espadilla */
		if (c.suit == t && c.rank == (black(t) ? 2 : 7))
			return 99; /* manilla */
		if (is_ace(c, SUIT_BASTOS))
			return 98; /* basto */
		if (c.suit == t && c.rank == 1)
			return 97; /* punto (solo en rojo: en negro los ases ya son matadores) */
		switch (c.rank) {
		case RANK_REY:
			return 90;
		case RANK_CABALLO:
			return 89;
		case RANK_SOTA:
			return 88;
		default:
			return black(t) ? 80 + c.rank : 89 - c.rank; /* negro 7 -> 87 ... 3; rojo 2 -> 87 ... 6 */
		}
	}
	switch (c.rank) {
	case RANK_REY:
		return 10;
	case RANK_CABALLO:
		return 9;
	case RANK_SOTA:
		return 8;
	default:
		if (black(c.suit))
			return c.rank; /* 7 ... 2 */
		return c.rank == 1 ? 7 : 8 - c.rank; /* as 7, 2 -> 6 ... 7 -> 1 */
	}
}

bool ts_is_matador(const Tresillo* g, Card c)
{
	return ts_is_trump(g, c) && ts_power(g, c) >= 97;
}

/* Palo "efectivo": los triunfos cuentan como el palo del triunfo */
static int eff_suit(const Tresillo* g, Card c)
{
	return ts_is_trump(g, c) ? g->trump : c.suit;
}

bool ts_beats(const Tresillo* g, Card a, Card best, int lead)
{
	bool ta = ts_is_trump(g, a), tb = ts_is_trump(g, best);
	if (ta != tb)
		return ta;
	if (ta)
		return ts_power(g, a) > ts_power(g, best);
	if (a.suit != best.suit)
		return a.suit == lead && best.suit != lead;
	return ts_power(g, a) > ts_power(g, best);
}

int ts_hand_size(const Tresillo* g)
{
	return g->players == 3 ? 9 : 10;
}

int ts_side(const Tresillo* g, int p)
{
	return p == g->hombre || p == g->partner ? 0 : 1;
}

void ts_init(Tresillo* g, int players, uint32_t seed)
{
	memset(g, 0, sizeof *g);
	g->players = players == 4 ? 4 : 3;
	rng_seed(&g->rng, seed);
	g->dealer = rng_range(&g->rng, g->players);
	ts_new_game(g);
}

void ts_new_game(Tresillo* g)
{
	for (int p = 0; p < TS_MAX_PLAYERS; p++)
		g->score[p] = 0;
	g->hands_played = 0;
	ts_new_hand(g);
}

void ts_new_hand(Tresillo* g)
{
	Deck d;
	deck_init(&d);
	deck_shuffle(&d, &g->rng);
	g->dealer = (g->dealer + 1) % g->players;
	g->mano = (g->dealer + 1) % g->players;
	int n = ts_hand_size(g);
	for (int p = 0; p < g->players; p++) {
		g->hand_n[p] = 0;
		g->passed[p] = g->discarded[p] = g->trick_played[p] = false;
		g->changed[p] = 0;
		g->tricks[p] = 0;
		g->gain[p] = 0;
	}
	for (int k = 0; k < n; k++)
		for (int i = 0; i < g->players; i++) {
			int p = (g->mano + i) % g->players;
			deck_draw(&d, &g->hands[p][g->hand_n[p]++]);
		}
	g->talon_n = 0;
	Card c;
	while (deck_draw(&d, &c))
		g->talon[g->talon_n++] = c;
	g->bid = TS_PASS;
	g->hombre = -1;
	g->partner = -1;
	g->called_king = -1;
	g->trump = -1;
	g->current = g->mano;
	g->played_in_trick = 0;
	g->tricks_done = 0;
	g->result = TS_NADIE;
	g->codillo_player = -1;
	g->last_bidder = -1;
	g->phase = TS_BIDDING;
}

int ts_min_bid(const Tresillo* g)
{
	int b = g->bid + 1;
	if (g->players == 4 && b == TS_VUELTA)
		b = TS_SOLO; /* en el cuatrillo no hay vuelta */
	return b;
}

bool ts_can_bid(const Tresillo* g, int bid)
{
	if (g->phase != TS_BIDDING)
		return false;
	if (bid == TS_PASS)
		return true;
	if (g->players == 4 && bid == TS_VUELTA)
		return false;
	return bid >= ts_min_bid(g) && bid <= TS_SOLO;
}

static void end_hand(Tresillo* g)
{
	g->hands_played++;
	g->phase = g->hands_played >= TS_HANDS ? TS_GAME_OVER : TS_HAND_OVER;
}

static void start_discards(Tresillo* g);

bool ts_bid(Tresillo* g, int bid)
{
	if (!ts_can_bid(g, bid))
		return false;
	int p = g->current;
	g->last_bidder = p;
	g->last_bid = bid;
	if (bid == TS_PASS) {
		g->passed[p] = true;
	} else {
		g->bid = bid;
		g->hombre = p;
	}
	int next = -1;
	for (int i = 1; i <= g->players; i++) {
		int q = (p + i) % g->players;
		if (!g->passed[q]) {
			next = q;
			break;
		}
	}
	if (next < 0) {
		/* Nadie juega: se reparte otra vez */
		g->result = TS_NADIE;
		end_hand(g);
		return true;
	}
	if (next == g->hombre) {
		g->current = g->hombre;
		if (g->bid == TS_VUELTA) {
			g->trump = g->talon[g->talon_n - 1].suit; /* la primera del monte */
			start_discards(g);
		} else {
			g->phase = TS_TRUMP;
		}
		return true;
	}
	g->current = next;
	return true;
}

bool ts_can_call(const Tresillo* g, int king)
{
	if (g->players != 4 || g->bid == TS_SOLO)
		return king < 0;
	if (king < 0)
		return true; /* sin compañero si tiene los cuatro reyes */
	for (int i = 0; i < g->hand_n[g->hombre]; i++)
		if (g->hands[g->hombre][i].suit == king && g->hands[g->hombre][i].rank == RANK_REY)
			return false;
	return true;
}

static void start_play(Tresillo* g)
{
	g->phase = TS_PLAYING;
	g->leader = g->current = g->mano;
	g->played_in_trick = 0;
}

static int next_discarder(const Tresillo* g)
{
	/* Primero el hombre (salvo en solo) y luego los contrarios por orden */
	if (!g->discarded[g->hombre] && g->bid != TS_SOLO)
		return g->hombre;
	for (int i = 1; i < g->players; i++) {
		int p = (g->hombre + i) % g->players;
		if (!g->discarded[p])
			return p;
	}
	return -1;
}

static void start_discards(Tresillo* g)
{
	if (g->players == 4) {
		start_play(g);
		return;
	}
	if (g->bid == TS_SOLO)
		g->discarded[g->hombre] = true;
	g->phase = TS_DISCARD;
	g->current = next_discarder(g);
}

bool ts_choose(Tresillo* g, int trump, int king)
{
	if (g->phase != TS_TRUMP || trump < 0 || trump >= NUM_SUITS || !ts_can_call(g, king))
		return false;
	g->trump = trump;
	g->called_king = king;
	g->partner = -1;
	if (king >= 0)
		for (int p = 0; p < g->players; p++)
			for (int i = 0; i < g->hand_n[p]; i++)
				if (g->hands[p][i].suit == king && g->hands[p][i].rank == RANK_REY)
					g->partner = p;
	start_discards(g);
	return true;
}

bool ts_discard(Tresillo* g, const bool* sel)
{
	if (g->phase != TS_DISCARD)
		return false;
	int p = g->current;
	int n = 0;
	for (int i = 0; i < g->hand_n[p] && g->talon_n > 0; i++)
		if (sel && sel[i]) {
			g->hands[p][i] = g->talon[--g->talon_n];
			n++;
		}
	g->changed[p] = n;
	g->discarded[p] = true;
	int next = next_discarder(g);
	if (next < 0)
		start_play(g);
	else
		g->current = next;
	return true;
}

int ts_trick_leader_now(const Tresillo* g)
{
	if (g->played_in_trick == 0)
		return -1;
	int lead = eff_suit(g, g->trick[g->leader]);
	int best = g->leader;
	for (int k = 1; k < g->played_in_trick; k++) {
		int p = (g->leader + k) % g->players;
		if (ts_beats(g, g->trick[p], g->trick[best], lead))
			best = p;
	}
	return best;
}

bool ts_can_play(const Tresillo* g, int p, int idx)
{
	if (g->phase != TS_PLAYING || p != g->current || idx < 0 || idx >= g->hand_n[p])
		return false;
	if (g->played_in_trick == 0)
		return true;
	Card led = g->trick[g->leader];
	int lead = eff_suit(g, led);
	Card c = g->hands[p][idx];
	bool forced = false; /* tiene alguna carta que obliga a asistir */
	for (int i = 0; i < g->hand_n[p]; i++) {
		Card h = g->hands[p][i];
		if (eff_suit(g, h) != lead)
			continue;
		/* Un matador no sale ante un triunfo más bajo */
		if (ts_is_trump(g, led) && ts_is_matador(g, h) && ts_power(g, h) > ts_power(g, led))
			continue;
		forced = true;
	}
	if (!forced)
		return true;
	return eff_suit(g, c) == lead;
}

bool ts_play(Tresillo* g, int idx)
{
	int p = g->current;
	if (!ts_can_play(g, p, idx))
		return false;
	g->trick[p] = g->hands[p][idx];
	g->trick_played[p] = true;
	for (int i = idx; i < g->hand_n[p] - 1; i++)
		g->hands[p][i] = g->hands[p][i + 1];
	g->hand_n[p]--;
	g->played_in_trick++;
	if (g->played_in_trick == g->players) {
		g->trick_winner = ts_trick_leader_now(g);
		g->phase = TS_TRICK_DONE;
	} else {
		g->current = (p + 1) % g->players;
	}
	return true;
}

static int bid_value(const Tresillo* g)
{
	return g->bid == TS_SOLO ? 3 : g->bid == TS_VUELTA ? 2 : 1;
}

bool ts_collect(Tresillo* g)
{
	if (g->phase != TS_TRICK_DONE)
		return false;
	int w = g->trick_winner;
	g->tricks[w]++;
	g->tricks_done++;
	for (int p = 0; p < g->players; p++)
		g->trick_played[p] = false;
	g->played_in_trick = 0;
	if (g->tricks_done < ts_hand_size(g)) {
		g->leader = g->current = w;
		g->phase = TS_PLAYING;
		return true;
	}
	int v = bid_value(g), h = g->hombre;
	if (g->players == 3) {
		int best_def = -1;
		for (int p = 0; p < 3; p++)
			if (p != h && (best_def < 0 || g->tricks[p] > g->tricks[best_def]))
				best_def = p;
		if (g->tricks[h] > g->tricks[best_def])
			g->result = TS_SACADA;
		else if (g->tricks[best_def] > g->tricks[h]) {
			g->result = TS_CODILLO;
			g->codillo_player = best_def;
		} else
			g->result = TS_PUESTA;
	} else {
		int team = g->tricks[h] + (g->partner >= 0 ? g->tricks[g->partner] : 0);
		g->result = team >= 6 ? TS_SACADA : team == 5 ? TS_PUESTA : TS_CODILLO;
	}
	switch (g->result) {
	case TS_SACADA:
		g->gain[h] = 2 * v;
		if (g->partner >= 0)
			g->gain[g->partner] = v;
		break;
	case TS_PUESTA:
		g->gain[h] = -v;
		break;
	default:
		g->gain[h] = -2 * v;
		for (int p = 0; p < g->players; p++)
			if (ts_side(g, p) == 1 && (g->players == 4 || p == g->codillo_player))
				g->gain[p] = g->players == 4 ? v : 2 * v;
		break;
	}
	for (int p = 0; p < g->players; p++)
		g->score[p] += g->gain[p];
	end_hand(g);
	return true;
}

int ts_winner(const Tresillo* g)
{
	int best = 0;
	for (int p = 1; p < g->players; p++)
		if (g->score[p] > g->score[best])
			best = p;
	return best;
}

/* ---------------------------------------------------------------------------
 * IA
 * ------------------------------------------------------------------------- */

/* Fuerza de la mano si el triunfo fuera suit */
static int strength(const Tresillo* g, int player, int suit)
{
	Tresillo t = *g;
	t.trump = suit;
	int v = 0;
	for (int i = 0; i < g->hand_n[player]; i++) {
		Card c = g->hands[player][i];
		if (ts_is_trump(&t, c))
			v += ts_is_matador(&t, c) ? 5 : ts_power(&t, c) >= 88 ? 3 : 2;
		else if (c.rank == RANK_REY)
			v += 2;
	}
	return v;
}

static int best_suit(const Tresillo* g, int player, int* value)
{
	int best = 0, bv = -1;
	for (int s = 0; s < NUM_SUITS; s++) {
		int v = strength(g, player, s);
		if (v > bv) {
			bv = v;
			best = s;
		}
	}
	if (value)
		*value = bv;
	return best;
}

int ts_ai_bid(const Tresillo* g, int player)
{
	int v;
	best_suit(g, player, &v);
	int want = TS_PASS;
	int need = g->players == 3 ? 16 : 17;
	if (v >= need + 9)
		want = TS_SOLO;
	else if (v >= need)
		want = TS_ENTRADA;
	if (g->players == 3 && want == TS_ENTRADA && g->bid >= TS_ENTRADA && v >= need + 4)
		want = TS_VUELTA;
	if (want == TS_PASS || !ts_can_bid(g, want))
		return ts_can_bid(g, TS_PASS) ? TS_PASS : TS_PASS;
	return want;
}

void ts_ai_choose(const Tresillo* g, int player, int* trump, int* king)
{
	*trump = best_suit(g, player, NULL);
	*king = -1;
	if (g->players != 4 || g->bid == TS_SOLO)
		return;
	/* Llama el rey del palo en el que tiene más cartas sin el rey */
	int best = -1, bn = -1;
	for (int s = 0; s < NUM_SUITS; s++) {
		if (!ts_can_call(g, s) || s == *trump)
			continue;
		int n = 0;
		for (int i = 0; i < g->hand_n[player]; i++)
			n += g->hands[player][i].suit == s;
		if (n > bn) {
			bn = n;
			best = s;
		}
	}
	if (best < 0 && ts_can_call(g, *trump))
		best = *trump;
	*king = best;
}

void ts_ai_discard(const Tresillo* g, int player, bool* sel)
{
	/* El primer contrario deja la mitad del monte para el otro */
	int limit = g->talon_n;
	if (player != g->hombre && !g->discarded[(player + 1) % g->players] && (player + 1) % g->players != g->hombre)
		limit = (g->talon_n + 1) / 2;
	for (int i = 0; i < g->hand_n[player]; i++) {
		Card c = g->hands[player][i];
		sel[i] = false;
		if (limit > 0 && !ts_is_trump(g, c) && c.rank != RANK_REY && c.rank != RANK_CABALLO) {
			sel[i] = true;
			limit--;
		}
	}
}

static int value_of(const Tresillo* g, Card c)
{
	return ts_power(g, c) + (ts_is_trump(g, c) ? 100 : 0);
}

int ts_ai_play(const Tresillo* g, int p)
{
	int n = g->hand_n[p];
	int lw = ts_trick_leader_now(g);
	int low = -1, high = -1, win_low = -1;
	for (int i = 0; i < n; i++) {
		if (!ts_can_play(g, p, i))
			continue;
		Card c = g->hands[p][i];
		if (low < 0 || value_of(g, c) < value_of(g, g->hands[p][low]))
			low = i;
		if (high < 0 || value_of(g, c) > value_of(g, g->hands[p][high]))
			high = i;
		if (lw >= 0 && ts_beats(g, c, g->trick[lw], eff_suit(g, g->trick[g->leader])) &&
			(win_low < 0 || value_of(g, c) < value_of(g, g->hands[p][win_low])))
			win_low = i;
	}
	if (lw < 0) {
		/* Sale: el hombre arrastra con triunfos altos; los demás, con reyes o lo más bajo */
		Card h = g->hands[p][high];
		if (p == g->hombre && ts_is_trump(g, h))
			return high;
		for (int i = 0; i < n; i++)
			if (!ts_is_trump(g, g->hands[p][i]) && g->hands[p][i].rank == RANK_REY)
				return i;
		return low;
	}
	/* Si gana uno de su bando, se echa la más baja */
	if (lw != p && ts_side(g, lw) == ts_side(g, p))
		return low;
	return win_low >= 0 ? win_low : low;
}
