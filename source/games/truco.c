#include "games/truco.h"

#include <string.h>

int tr_power(Card c)
{
	switch (c.rank) {
	case 1:
		return c.suit == SUIT_ESPADAS ? 14 : c.suit == SUIT_BASTOS ? 13 : 8;
	case 7:
		return c.suit == SUIT_ESPADAS ? 12 : c.suit == SUIT_OROS ? 11 : 4;
	case 3:
		return 10;
	case 2:
		return 9;
	case RANK_REY:
		return 7;
	case RANK_CABALLO:
		return 6;
	case RANK_SOTA:
		return 5;
	default:
		return c.rank - 3; /* 6 -> 3, 5 -> 2, 4 -> 1 */
	}
}

static int envido_value(Card c)
{
	return c.rank <= 7 ? c.rank : 0;
}

int tr_envido(const Card* cards, int n)
{
	int best = 0;
	for (int i = 0; i < n; i++) {
		if (envido_value(cards[i]) > best)
			best = envido_value(cards[i]);
		for (int j = i + 1; j < n; j++)
			if (cards[i].suit == cards[j].suit) {
				int v = 20 + envido_value(cards[i]) + envido_value(cards[j]);
				if (v > best)
					best = v;
			}
	}
	return best;
}

void tr_init(Truco* g, uint32_t seed)
{
	tr_init_variant(g, false, seed);
}

void tr_init_variant(Truco* g, bool valenciano, uint32_t seed)
{
	memset(g, 0, sizeof *g);
	g->valenciano = valenciano;
	g->target = valenciano ? TR_GAME_TRUC : TR_GAME;
	rng_seed(&g->rng, seed);
	g->mano = rng_range(&g->rng, 2);
	tr_new_game(g);
}

void tr_new_game(Truco* g)
{
	g->score[0] = g->score[1] = 0;
	tr_new_hand(g);
}

void tr_new_hand(Truco* g)
{
	Deck d;
	deck_init(&d);
	deck_shuffle(&d, &g->rng);
	g->mano = 1 - g->mano;
	for (int k = 0; k < 3; k++)
		for (int i = 0; i < 2; i++) {
			int p = (g->mano + i) % 2;
			deck_draw(&d, &g->hand[p][k]);
			g->dealt[p][k] = g->hand[p][k];
			g->used[p][k] = false;
		}
	memset(g->on_table, 0, sizeof g->on_table);
	for (int b = 0; b < 3; b++)
		g->baza_winner[b] = -1;
	g->baza = 0;
	g->baza_leader = g->turn = g->mano;
	g->phase = TR_PLAY;
	g->pending = TR_ACT_NONE;
	g->truco_level = 1;
	g->truco_call = 0;
	g->truco_owner = -1;
	g->truco_suspended = false;
	g->envido_done = false;
	g->envido_count = 0;
	g->envido_pts[0] = g->envido_pts[1] = 0;
	g->envido_winner = -1;
	g->envido_value = 0;
	g->envido_taker = -1;
	g->hand_winner = -1;
	g->hand_points = 0;
	g->folded = false;
	g->last_actor = -1;
	g->last_action = TR_ACT_NONE;
}

int tr_actor(const Truco* g)
{
	return g->phase == TR_RESPOND ? g->responder : g->turn;
}

int tr_falta(const Truco* g)
{
	int top = g->score[0] > g->score[1] ? g->score[0] : g->score[1];
	int v = g->target - top;
	return v < 1 ? 1 : v;
}

int tr_max_level(const Truco* g)
{
	return g->valenciano ? 5 : 4;
}

int tr_level_value(const Truco* g, int level)
{
	return level >= 5 ? g->target : level; /* el joc fora gana la partida */
}

int tr_next_truco(const Truco* g)
{
	return g->phase == TR_RESPOND && g->pending == TR_ACT_TRUCO ? g->truco_call + 1 : g->truco_level + 1;
}

static bool chain_has(const Truco* g, int a)
{
	for (int i = 0; i < g->envido_count; i++)
		if (g->envido_chain[i] == a)
			return true;
	return false;
}

static bool envido_open(const Truco* g, int player)
{
	return !g->envido_done && g->baza == 0 && !g->on_table[0][player] && g->truco_level == 1;
}

bool tr_can(const Truco* g, int player, TrAction a)
{
	if (g->phase == TR_PLAY) {
		if (player != g->turn)
			return false;
		switch (a) {
		case TR_ACT_REAL:
			return !g->valenciano && envido_open(g, player);
		case TR_ACT_ENVIDO:
		case TR_ACT_FALTA:
			return envido_open(g, player);
		case TR_ACT_TRUCO:
			return g->truco_level < tr_max_level(g) && (g->truco_owner < 0 || g->truco_owner == player);
		case TR_ACT_MAZO:
			return true;
		default:
			return false;
		}
	}
	if (g->phase != TR_RESPOND || player != g->responder)
		return false;
	switch (a) {
	case TR_ACT_QUIERO:
	case TR_ACT_NOQUIERO:
		return true;
	case TR_ACT_TRUCO:
		return g->pending == TR_ACT_TRUCO && g->truco_call < tr_max_level(g);
	case TR_ACT_ENVIDO:
	case TR_ACT_REAL:
	case TR_ACT_FALTA:
		if (g->pending == TR_ACT_TRUCO) /* el envido está primero */
			return g->truco_call == 2 && envido_open(g, player) && !(a == TR_ACT_REAL && g->valenciano);
		if (g->pending != TR_ACT_ENVIDO || chain_has(g, TR_ACT_FALTA))
			return false;
		if (a == TR_ACT_ENVIDO)
			return g->envido_count == 1 && g->envido_chain[0] == TR_ACT_ENVIDO;
		if (a == TR_ACT_REAL)
			return !g->valenciano && !chain_has(g, TR_ACT_REAL);
		return true;
	default:
		return false;
	}
}

static void finish(Truco* g)
{
	if (g->score[0] >= g->target || g->score[1] >= g->target)
		g->phase = TR_GAME_OVER;
}

static void end_hand(Truco* g, int winner, int points)
{
	g->hand_winner = winner;
	g->hand_points = points;
	g->score[winner] += points;
	g->pending = TR_ACT_NONE;
	g->phase = TR_HAND_OVER;
	finish(g);
}

static int chain_value(const Truco* g, int count)
{
	int v = 0;
	for (int i = 0; i < count; i++) {
		if (g->envido_chain[i] == TR_ACT_FALTA)
			return tr_falta(g);
		v += g->envido_chain[i] == TR_ACT_REAL ? 3 : 2;
	}
	return v;
}

/* Tras el envido se sigue con lo que hubiera: el truco pendiente o el juego */
static void resume(Truco* g)
{
	g->envido_done = true;
	finish(g);
	if (g->phase == TR_GAME_OVER) {
		g->pending = TR_ACT_NONE;
		return;
	}
	if (g->truco_suspended) {
		g->truco_suspended = false;
		g->pending = TR_ACT_TRUCO;
		g->caller = g->truco_caller_saved;
		g->responder = 1 - g->caller;
		g->phase = TR_RESPOND;
	} else {
		g->pending = TR_ACT_NONE;
		g->phase = TR_PLAY;
	}
}

bool tr_act(Truco* g, int player, TrAction a)
{
	if (!tr_can(g, player, a))
		return false;
	g->last_actor = player;
	g->last_action = a;
	switch (a) {
	case TR_ACT_ENVIDO:
	case TR_ACT_REAL:
	case TR_ACT_FALTA:
		if (g->phase == TR_RESPOND && g->pending == TR_ACT_TRUCO) {
			g->truco_suspended = true;
			g->truco_caller_saved = g->caller;
			g->envido_count = 0;
		} else if (g->phase == TR_PLAY) {
			g->envido_count = 0;
		}
		g->envido_chain[g->envido_count++] = a;
		g->pending = TR_ACT_ENVIDO;
		g->caller = player;
		g->responder = 1 - player;
		g->phase = TR_RESPOND;
		return true;
	case TR_ACT_TRUCO:
		if (g->phase == TR_RESPOND) {
			g->truco_level = g->truco_call; /* subir es querer lo anterior */
			g->truco_call++;
		} else {
			g->truco_call = g->truco_level + 1;
		}
		g->last_level = g->truco_call;
		g->pending = TR_ACT_TRUCO;
		g->caller = player;
		g->responder = 1 - player;
		g->phase = TR_RESPOND;
		return true;
	case TR_ACT_QUIERO:
		if (g->pending == TR_ACT_ENVIDO) {
			for (int p = 0; p < 2; p++)
				g->envido_pts[p] = tr_envido(g->dealt[p], 3);
			int w = g->envido_pts[0] == g->envido_pts[1] ? g->mano : g->envido_pts[0] > g->envido_pts[1] ? 0 : 1;
			g->envido_winner = w;
			g->envido_value = chain_value(g, g->envido_count);
			g->envido_taker = w;
			g->score[w] += g->envido_value;
			resume(g);
		} else {
			g->truco_level = g->truco_call;
			g->truco_owner = player;
			g->pending = TR_ACT_NONE;
			g->phase = TR_PLAY;
		}
		return true;
	case TR_ACT_NOQUIERO:
		if (g->pending == TR_ACT_ENVIDO) {
			int v = g->envido_count == 1 ? 1 : chain_value(g, g->envido_count - 1);
			g->envido_value = v;
			g->envido_taker = g->caller;
			g->envido_winner = -1;
			g->score[g->caller] += v;
			resume(g);
		} else {
			g->folded = true;
			end_hand(g, g->caller, tr_level_value(g, g->truco_level));
		}
		return true;
	case TR_ACT_MAZO:
		g->folded = true;
		end_hand(g, 1 - player, tr_level_value(g, g->truco_level));
		return true;
	default:
		return false;
	}
}

bool tr_can_play(const Truco* g, int player, int idx)
{
	return g->phase == TR_PLAY && player == g->turn && idx >= 0 && idx < 3 && !g->used[player][idx];
}

/* ¿Se ha decidido la mano tras la baza b? Devuelve el ganador o -1 */
static int decided(const Truco* g, int b)
{
	const int* r = g->baza_winner;
	if (b == 0)
		return -1;
	if (b == 1) {
		if (r[0] >= 0 && (r[1] == r[0] || r[1] < 0))
			return r[0];
		if (r[0] < 0 && r[1] >= 0)
			return r[1];
		return -1;
	}
	if (r[2] >= 0)
		return r[2];
	if (r[0] >= 0)
		return r[0];
	if (r[1] >= 0)
		return r[1];
	return g->mano;
}

bool tr_play(Truco* g, int player, int idx)
{
	if (!tr_can_play(g, player, idx))
		return false;
	int b = g->baza;
	g->table[b][player] = g->hand[player][idx];
	g->on_table[b][player] = true;
	g->used[player][idx] = true;
	g->last_actor = player;
	g->last_action = TR_ACT_NONE;
	if (!g->on_table[b][1 - player]) {
		g->turn = 1 - player;
		return true;
	}
	int p0 = tr_power(g->table[b][0]), p1 = tr_power(g->table[b][1]);
	int w = p0 == p1 ? -1 : p0 > p1 ? 0 : 1;
	g->baza_winner[b] = w;
	int hw = decided(g, b);
	if (hw >= 0) {
		end_hand(g, hw, tr_level_value(g, g->truco_level));
		return true;
	}
	g->baza++;
	if (w >= 0)
		g->baza_leader = w;
	g->turn = g->baza_leader;
	return true;
}

int tr_winner(const Truco* g)
{
	if (g->score[0] >= g->target && g->score[0] >= g->score[1])
		return 0;
	if (g->score[1] >= g->target)
		return 1;
	return -1;
}

/* ---------------------------------------------------------------------------
 * IA
 * ------------------------------------------------------------------------- */

static int chance(const Truco* g, int salt)
{
	Rng r = g->rng;
	uint32_t s = (uint32_t)(salt * 2654435761u) ^ rng_next(&r);
	return (int)(s % 100u);
}

/* Lo buena que es la mano para el truco, de 0 a 100 aproximadamente */
static int truco_strength(const Truco* g, int p)
{
	int won = 0, lost = 0;
	for (int b = 0; b < g->baza; b++) {
		if (g->baza_winner[b] == p)
			won++;
		else if (g->baza_winner[b] == 1 - p)
			lost++;
	}
	int s = won * 35 - lost * 35;
	for (int i = 0; i < 3; i++)
		if (!g->used[p][i]) {
			int pw = tr_power(g->hand[p][i]);
			s += pw >= 12 ? 30 : pw >= 10 ? 18 : pw >= 8 ? 10 : pw >= 6 ? 4 : 0;
		}
	/* Si el rival ya ha echado en esta baza, se sabe si se gana */
	int b = g->baza;
	if (g->on_table[b][1 - p] && !g->on_table[b][p]) {
		int their = tr_power(g->table[b][1 - p]);
		bool beat = false;
		for (int i = 0; i < 3; i++)
			if (!g->used[p][i] && tr_power(g->hand[p][i]) > their)
				beat = true;
		s += beat ? 15 : -20;
	}
	return s;
}

static int pick_card(const Truco* g, int p)
{
	int b = g->baza;
	int low = -1, high = -1, mid = -1;
	int n = 0, idx[3];
	for (int i = 0; i < 3; i++)
		if (!g->used[p][i])
			idx[n++] = i;
	for (int i = 0; i < n; i++) {
		int c = idx[i];
		if (low < 0 || tr_power(g->hand[p][c]) < tr_power(g->hand[p][low]))
			low = c;
		if (high < 0 || tr_power(g->hand[p][c]) > tr_power(g->hand[p][high]))
			high = c;
	}
	if (g->on_table[b][1 - p]) {
		/* Gana con la más baja que gane; si no, se deshace de la más baja */
		int their = tr_power(g->table[b][1 - p]);
		int best = -1;
		for (int i = 0; i < n; i++) {
			int c = idx[i];
			int pw = tr_power(g->hand[p][c]);
			if (pw > their && (best < 0 || pw < tr_power(g->hand[p][best])))
				best = c;
		}
		if (best >= 0)
			return best;
		/* Si puede empatar y ya ganó la primera, la parda le vale */
		for (int i = 0; i < n; i++)
			if (tr_power(g->hand[p][idx[i]]) == their && b > 0 && g->baza_winner[0] == p)
				return idx[i];
		return low;
	}
	/* Sale: en la primera, con la del medio; después de ganar, con la alta */
	if (b == 0 && n == 3) {
		for (int i = 0; i < n; i++)
			if (idx[i] != low && idx[i] != high)
				mid = idx[i];
		return mid >= 0 ? mid : low;
	}
	if (b > 0 && g->baza_winner[b - 1] < 0)
		return high; /* tras una parda, a ganar */
	return g->baza_winner[0] == p ? high : low;
}

TrAction tr_ai(const Truco* g, int p, int* card)
{
	*card = -1;
	int env = tr_envido(g->dealt[p], 3);
	int roll = chance(g, p * 31 + g->baza * 7 + g->envido_count * 3 + g->truco_call);
	if (g->phase == TR_RESPOND) {
		if (g->pending == TR_ACT_ENVIDO) {
			int bonus = g->mano == p ? 1 : 0;
			if (env + bonus >= 32 && tr_can(g, p, TR_ACT_FALTA) && roll < 40)
				return TR_ACT_FALTA;
			if (env + bonus >= 30 && tr_can(g, p, TR_ACT_REAL) && roll < 50)
				return TR_ACT_REAL;
			int need = 26 + (g->envido_count - 1) * 2 + (chain_has(g, TR_ACT_FALTA) ? 3 : 0);
			return env + bonus >= need ? TR_ACT_QUIERO : TR_ACT_NOQUIERO;
		}
		/* Truco */
		if (tr_can(g, p, TR_ACT_ENVIDO) && env >= 28)
			return env >= 31 && tr_can(g, p, TR_ACT_REAL) ? TR_ACT_REAL : TR_ACT_ENVIDO;
		int s = truco_strength(g, p);
		if (s >= 60 + g->truco_call * 5 && tr_can(g, p, TR_ACT_TRUCO) && roll < 60 && (g->truco_call < 4 || s >= 90))
			return TR_ACT_TRUCO;
		if (s >= 25 + g->truco_call * 5 || roll < 12)
			return TR_ACT_QUIERO;
		return TR_ACT_NOQUIERO;
	}
	/* Le toca jugar */
	if (tr_can(g, p, TR_ACT_ENVIDO)) {
		if (env >= 31 && roll < 60)
			return roll < 25 || !tr_can(g, p, TR_ACT_REAL) ? TR_ACT_FALTA : TR_ACT_REAL;
		if (env >= 27 && roll < 80)
			return TR_ACT_ENVIDO;
		if (env <= 22 && roll < 8)
			return TR_ACT_ENVIDO; /* farol */
	}
	if (tr_can(g, p, TR_ACT_TRUCO)) {
		int s = truco_strength(g, p);
		bool joc_fora = g->truco_level + 1 >= 5;
		if (((s >= 50 + g->truco_level * 8 && roll < 70) || (roll < 5 && g->truco_level < 3)) && (!joc_fora || s >= 90))
			return TR_ACT_TRUCO;
	}
	*card = pick_card(g, p);
	return TR_ACT_NONE;
}
