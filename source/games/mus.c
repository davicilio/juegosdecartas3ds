#include "games/mus.h"

#include <stdio.h>
#include <string.h>

/* ---------------------------------------------------------------------------
 * Valoración de las manos
 * ------------------------------------------------------------------------- */

int mus_rank(Card c)
{
	if (c.rank == 3 || c.rank == RANK_REY)
		return 10;
	if (c.rank == 2 || c.rank == 1)
		return 1;
	if (c.rank == RANK_CABALLO)
		return 9;
	if (c.rank == RANK_SOTA)
		return 8;
	return c.rank;
}

int mus_card_points(Card c)
{
	int r = mus_rank(c);
	return r >= 8 ? 10 : r;
}

int mus_juego_sum(const Card* hand)
{
	int sum = 0;
	for (int i = 0; i < MUS_HAND_SIZE; i++)
		sum += mus_card_points(hand[i]);
	return sum;
}

bool mus_has_juego(const Card* hand)
{
	return mus_juego_sum(hand) >= 31;
}

static void sorted_ranks(const Card* hand, int* r, bool descending)
{
	for (int i = 0; i < MUS_HAND_SIZE; i++)
		r[i] = mus_rank(hand[i]);
	for (int i = 1; i < MUS_HAND_SIZE; i++)
		for (int j = i; j > 0 && (descending ? r[j - 1] < r[j] : r[j - 1] > r[j]); j--) {
			int t = r[j];
			r[j] = r[j - 1];
			r[j - 1] = t;
		}
}

/* Pares: tipo y rangos (alto y bajo) para compararlos. */
static int pares_info(const Card* hand, int* high, int* low)
{
	int count[11] = { 0 };
	for (int i = 0; i < MUS_HAND_SIZE; i++)
		count[mus_rank(hand[i])]++;
	int pairs[2], np = 0, trio = 0;
	for (int r = 10; r >= 1; r--) {
		if (count[r] == 4) {
			*high = *low = r;
			return 3; /* duples de cuatro iguales */
		}
		if (count[r] == 3)
			trio = r;
		else if (count[r] == 2)
			pairs[np++] = r;
	}
	if (np == 2) {
		*high = pairs[0];
		*low = pairs[1];
		return 3;
	}
	if (trio) {
		*high = *low = trio;
		return 2;
	}
	if (np == 1) {
		*high = *low = pairs[0];
		return 1;
	}
	*high = *low = 0;
	return 0;
}

int mus_pares_kind(const Card* hand)
{
	int h, l;
	return pares_info(hand, &h, &l);
}

int mus_pares_value(const Card* hand)
{
	return mus_pares_kind(hand);
}

int mus_juego_value(const Card* hand)
{
	int s = mus_juego_sum(hand);
	if (s < 31)
		return 0;
	return s == 31 ? 3 : 2;
}

static int juego_order(int sum)
{
	/* 31 > 32 > 40 > 37 > 36 > 35 > 34 > 33 */
	switch (sum) {
	case 31:
		return 8;
	case 32:
		return 7;
	case 40:
		return 6;
	default:
		return sum >= 33 && sum <= 37 ? sum - 32 : 0;
	}
}

int mus_compare(int lance, bool punto, const Card* a, const Card* b)
{
	int ra[MUS_HAND_SIZE], rb[MUS_HAND_SIZE];
	switch (lance) {
	case MUS_LANCE_GRANDE:
	case MUS_LANCE_CHICA: {
		bool grande = lance == MUS_LANCE_GRANDE;
		sorted_ranks(a, ra, grande);
		sorted_ranks(b, rb, grande);
		for (int i = 0; i < MUS_HAND_SIZE; i++)
			if (ra[i] != rb[i])
				return grande ? ra[i] - rb[i] : rb[i] - ra[i];
		return 0;
	}
	case MUS_LANCE_PARES: {
		int ha, la, hb, lb;
		int ka = pares_info(a, &ha, &la), kb = pares_info(b, &hb, &lb);
		if (ka != kb)
			return ka - kb;
		if (ha != hb)
			return ha - hb;
		return la - lb;
	}
	default:
		if (punto)
			return mus_juego_sum(a) - mus_juego_sum(b);
		return juego_order(mus_juego_sum(a)) - juego_order(mus_juego_sum(b));
	}
}

static bool can_play(const Mus* g, int lance, int p)
{
	if (lance == MUS_LANCE_PARES)
		return mus_pares_kind(g->hands[p]) > 0;
	if (lance == MUS_LANCE_JUEGO && !g->punto)
		return mus_has_juego(g->hands[p]);
	return true;
}

int mus_lance_winner(const Mus* g, int lance)
{
	int best = -1;
	for (int k = 0; k < MUS_PLAYERS; k++) {
		int p = (g->mano + k) % MUS_PLAYERS;
		if (!can_play(g, lance, p))
			continue;
		/* Solo gana si es estrictamente mejor: en empate gana el más cercano a la mano */
		if (best < 0 || mus_compare(lance, g->punto, g->hands[p], g->hands[best]) > 0)
			best = p;
	}
	return best;
}

const char* mus_lance_name(const Mus* g, int lance)
{
	static const char* names[MUS_NUM_LANCES] = { "Grande", "Chica", "Pares", "Juego" };
	if (lance == MUS_LANCE_JUEGO && g->punto)
		return "Punto";
	return names[lance];
}

static const char* rank_plural(int r)
{
	switch (r) {
	case 10:
		return "reyes";
	case 9:
		return "caballos";
	case 8:
		return "sotas";
	case 1:
		return "ases";
	case 4:
		return "cuatros";
	case 5:
		return "cincos";
	case 6:
		return "seises";
	default:
		return "sietes";
	}
}

void mus_describe(const Card* hand, int lance, bool punto, char* buf, int size)
{
	int r[MUS_HAND_SIZE];
	static const char* letters = "?A234567SCR";
	switch (lance) {
	case MUS_LANCE_GRANDE:
	case MUS_LANCE_CHICA:
		sorted_ranks(hand, r, lance == MUS_LANCE_GRANDE);
		snprintf(buf, size, "%c %c %c %c", letters[r[0]], letters[r[1]], letters[r[2]], letters[r[3]]);
		break;
	case MUS_LANCE_PARES: {
		int h, l;
		int k = pares_info(hand, &h, &l);
		if (k == 0)
			snprintf(buf, size, "Sin pares");
		else if (k == 1)
			snprintf(buf, size, "Pareja de %s", rank_plural(h));
		else if (k == 2)
			snprintf(buf, size, "Medias de %s", rank_plural(h));
		else if (h == l)
			snprintf(buf, size, "Duples de %s", rank_plural(h));
		else
			snprintf(buf, size, "Duples %s y %s", rank_plural(h), rank_plural(l));
		break;
	}
	default: {
		int s = mus_juego_sum(hand);
		if (s >= 31)
			snprintf(buf, size, "Juego: %d", s);
		else
			snprintf(buf, size, punto ? "Punto: %d" : "Sin juego (%d)", s);
		break;
	}
	}
}

/* ---------------------------------------------------------------------------
 * Desarrollo
 * ------------------------------------------------------------------------- */

static bool draw_card(Mus* g, Card* out)
{
	if (g->deck.count == 0) {
		/* Se barajan los descartes para seguir dando */
		for (int i = 0; i < g->discard_count; i++)
			g->deck.cards[i] = g->discards[i];
		g->deck.count = g->discard_count;
		g->discard_count = 0;
		deck_shuffle(&g->deck, &g->rng);
	}
	return deck_draw(&g->deck, out);
}

void mus_init(Mus* g, uint32_t seed)
{
	rng_seed(&g->rng, seed);
	g->score[0] = g->score[1] = 0;
	g->dealer = 3; /* en la primera mano es mano el humano */
	g->hands_played = 0;
	g->winner_team = -1;
	mus_start_hand(g);
}

void mus_start_hand(Mus* g)
{
	deck_init(&g->deck);
	deck_shuffle(&g->deck, &g->rng);
	g->discard_count = 0;
	g->mano = (g->dealer + 1) % MUS_PLAYERS;
	for (int i = 0; i < MUS_HAND_SIZE; i++)
		for (int k = 0; k < MUS_PLAYERS; k++)
			draw_card(g, &g->hands[(g->mano + k) % MUS_PLAYERS][i]);
	for (int l = 0; l < MUS_NUM_LANCES; l++)
		memset(&g->results[l], 0, sizeof g->results[l]);
	g->phase = MUS_PHASE_MUS;
	g->current = g->mano;
	g->mus_count = 0;
	g->mus_rounds = 0;
	g->punto = false;
	g->ordago_played = false;
	for (int p = 0; p < MUS_PLAYERS; p++)
		g->sena[p] = MUS_SENA_NONE;
	g->last_player = -1;
	g->last_action = MUS_ACT_NONE;
	g->last_amount = 0;
	g->actions++;
}

static void note(Mus* g, int p, MusAction a, int amount)
{
	g->last_player = p;
	g->last_action = a;
	g->last_amount = amount;
	g->actions++;
}

static void start_lance(Mus* g, int lance);

static void award(Mus* g, int team, int points, MusLanceResult* r)
{
	if (g->winner_team >= 0 || points <= 0)
		return;
	g->score[team] += points;
	r->points[team] += points;
	if (g->score[team] >= MUS_GAME_POINTS) {
		g->winner_team = team;
		g->phase = MUS_PHASE_GAME_OVER;
	}
}

static int team_tantos(const Mus* g, int lance, int team)
{
	int sum = 0;
	for (int p = team; p < MUS_PLAYERS; p += 2) {
		if (lance == MUS_LANCE_PARES)
			sum += mus_pares_value(g->hands[p]);
		else if (lance == MUS_LANCE_JUEGO && !g->punto)
			sum += mus_juego_value(g->hands[p]);
	}
	return sum;
}

/* Recuento final de la mano, lance a lance. */
static void count_hand(Mus* g)
{
	g->phase = MUS_PHASE_HAND_OVER;
	for (int l = 0; l < MUS_NUM_LANCES && g->winner_team < 0; l++) {
		MusLanceResult* r = &g->results[l];
		r->winner = mus_lance_winner(g, l);
		if (r->kind == MUS_RES_NONE)
			continue;
		int team = r->kind == MUS_RES_NO_QUERIDO ? r->deje_team : mus_team(r->winner);
		int pts = 0;
		switch (r->kind) {
		case MUS_RES_PASO:
			pts = (l == MUS_LANCE_GRANDE || l == MUS_LANCE_CHICA || (l == MUS_LANCE_JUEGO && g->punto)) ? 1 : 0;
			break;
		case MUS_RES_QUERIDO:
			pts = r->amount + (l == MUS_LANCE_JUEGO && g->punto ? 1 : 0);
			break;
		default:
			break; /* el deje ya se cobró; SINGLE solo cobra tantos */
		}
		pts += team_tantos(g, l, team);
		award(g, team, pts, r);
	}
	g->hands_played++;
}

static void finish_lance(Mus* g)
{
	if (g->phase == MUS_PHASE_GAME_OVER)
		return;
	if (g->lance + 1 < MUS_NUM_LANCES)
		start_lance(g, g->lance + 1);
	else
		count_hand(g);
}

static int next_eligible(const Mus* g, int from, int team)
{
	/* Siguiente jugador (después de from) que puede jugar; team < 0 = cualquiera */
	for (int k = 1; k <= MUS_PLAYERS; k++) {
		int p = (from + k) % MUS_PLAYERS;
		if (g->eligible[p] && (team < 0 || mus_team(p) == team))
			return p;
	}
	return -1;
}

static void start_lance(Mus* g, int lance)
{
	g->lance = lance;
	g->phase = MUS_PHASE_BET;
	if (lance == MUS_LANCE_JUEGO) {
		g->punto = true;
		for (int p = 0; p < MUS_PLAYERS; p++)
			if (mus_has_juego(g->hands[p]))
				g->punto = false;
	}
	bool teams[2] = { false, false };
	for (int p = 0; p < MUS_PLAYERS; p++) {
		g->eligible[p] = can_play(g, lance, p);
		if (g->eligible[p])
			teams[mus_team(p)] = true;
	}
	g->bet_accepted = 0;
	g->bet_pending = 0;
	g->bet_team = -1;
	g->passes = 0;
	MusLanceResult* r = &g->results[lance];
	if (!teams[0] && !teams[1]) {
		r->kind = MUS_RES_NONE;
		finish_lance(g);
		return;
	}
	if (!teams[0] || !teams[1]) {
		r->kind = MUS_RES_SINGLE;
		finish_lance(g);
		return;
	}
	g->current = next_eligible(g, g->mano + MUS_PLAYERS - 1, -1);
}

bool mus_say_mus(Mus* g, bool mus)
{
	if (g->phase != MUS_PHASE_MUS)
		return false;
	int p = g->current;
	note(g, p, mus ? MUS_ACT_MUS : MUS_ACT_NO_MUS, 0);
	if (!mus) {
		start_lance(g, MUS_LANCE_GRANDE);
		return true;
	}
	if (++g->mus_count == MUS_PLAYERS) {
		g->phase = MUS_PHASE_DISCARD;
		g->discarded_now = 0;
		g->current = g->mano;
	} else {
		g->current = (p + 1) % MUS_PLAYERS;
	}
	return true;
}

bool mus_discard(Mus* g, unsigned mask)
{
	mask &= (1u << MUS_HAND_SIZE) - 1;
	if (g->phase != MUS_PHASE_DISCARD || mask == 0)
		return false;
	int p = g->current;
	int n = 0;
	for (int i = 0; i < MUS_HAND_SIZE; i++)
		if (mask & (1u << i)) {
			g->discards[g->discard_count++] = g->hands[p][i];
			draw_card(g, &g->hands[p][i]);
			n++;
		}
	note(g, p, MUS_ACT_DISCARD, n);
	if (++g->discarded_now == MUS_PLAYERS) {
		/* Con cartas nuevas las señas anteriores ya no valen */
		for (int q = 0; q < MUS_PLAYERS; q++)
			g->sena[q] = MUS_SENA_NONE;
		g->phase = MUS_PHASE_MUS;
		g->mus_count = 0;
		g->mus_rounds++;
		g->current = g->mano;
	} else {
		g->current = (p + 1) % MUS_PLAYERS;
	}
	return true;
}

bool mus_responding(const Mus* g)
{
	return g->phase == MUS_PHASE_BET && g->bet_pending != 0;
}

bool mus_bet(Mus* g, MusAction action, int amount)
{
	if (g->phase != MUS_PHASE_BET)
		return false;
	int p = g->current;
	MusLanceResult* r = &g->results[g->lance];
	bool responding = mus_responding(g);

	switch (action) {
	case MUS_ACT_PASO: {
		if (responding)
			return false;
		note(g, p, action, 0);
		int next = next_eligible(g, p, -1);
		/* Si la vuelta llega otra vez al primero que habló, todos han pasado */
		int first = next_eligible(g, g->mano + MUS_PLAYERS - 1, -1);
		if (next == first) {
			r->kind = MUS_RES_PASO;
			finish_lance(g);
		} else {
			g->current = next;
		}
		return true;
	}
	case MUS_ACT_ENVIDO:
	case MUS_ACT_ORDAGO:
		if (action == MUS_ACT_ENVIDO && amount < 1)
			return false;
		if (responding) {
			if (g->bet_pending == MUS_ORDAGO)
				return false; /* a un órdago solo se puede querer o no */
			g->bet_accepted = g->bet_pending; /* reenvidar implica querer lo anterior */
		}
		if (action == MUS_ACT_ORDAGO)
			g->bet_pending = MUS_ORDAGO;
		else
			g->bet_pending = (responding ? g->bet_accepted : 0) + amount;
		g->bet_team = mus_team(p);
		note(g, p, action, action == MUS_ACT_ORDAGO ? 0 : g->bet_pending);
		g->current = next_eligible(g, p, 1 - g->bet_team);
		return true;
	case MUS_ACT_QUIERO:
		if (!responding)
			return false;
		note(g, p, action, 0);
		if (g->bet_pending == MUS_ORDAGO) {
			/* Se descubren las cartas: el ganador del lance gana la partida */
			r->kind = MUS_RES_ORDAGO;
			r->winner = mus_lance_winner(g, g->lance);
			g->ordago_played = true;
			g->winner_team = mus_team(r->winner);
			g->score[g->winner_team] = g->score[g->winner_team] < MUS_GAME_POINTS ? MUS_GAME_POINTS
			                                                                       : g->score[g->winner_team];
			g->phase = MUS_PHASE_GAME_OVER;
			return true;
		}
		r->kind = MUS_RES_QUERIDO;
		r->amount = g->bet_pending;
		finish_lance(g);
		return true;
	case MUS_ACT_NO_QUIERO: {
		if (!responding)
			return false;
		note(g, p, action, 0);
		int deje = g->bet_accepted > 0 ? g->bet_accepted : 1;
		r->kind = MUS_RES_NO_QUERIDO;
		r->amount = deje;
		r->deje_team = g->bet_team;
		award(g, g->bet_team, deje, r);
		finish_lance(g);
		return true;
	}
	default:
		return false;
	}
}

bool mus_next_hand(Mus* g)
{
	if (g->phase != MUS_PHASE_HAND_OVER)
		return false;
	g->dealer = (g->dealer + 1) % MUS_PLAYERS;
	mus_start_hand(g);
	return true;
}

/* ---------------------------------------------------------------------------
 * Señas
 * ------------------------------------------------------------------------- */

MusSena mus_best_sena(const Card* hand)
{
	int kind = mus_pares_kind(hand);
	int reyes = 0, ases = 0;
	for (int i = 0; i < MUS_HAND_SIZE; i++) {
		int r = mus_rank(hand[i]);
		reyes += r == 10;
		ases += r == 1;
	}
	if (kind == 3)
		return MUS_SENA_DUPLES;
	if (kind == 2)
		return MUS_SENA_MEDIAS;
	if (mus_juego_sum(hand) == 31)
		return MUS_SENA_31;
	if (reyes >= 2)
		return MUS_SENA_REYES;
	if (ases >= 2)
		return MUS_SENA_ASES;
	if (kind == 0 && !mus_has_juego(hand))
		return MUS_SENA_CIEGO;
	return MUS_SENA_NONE;
}

const char* mus_sena_meaning(MusSena s)
{
	static const char* names[MUS_NUM_SENAS] = {
		"Nada que decir", "Dos reyes o más", "Dos ases o más", "Medias", "Duples", "31 de juego", "Ciego: nada"
	};
	return s < MUS_NUM_SENAS ? names[s] : "?";
}

const char* mus_sena_gesture(MusSena s)
{
	static const char* names[MUS_NUM_SENAS] = {
		"-", "Morderse el labio", "Sacar la lengua", "Torcer la boca", "Levantar las cejas",
		"Guiñar un ojo", "Cerrar los ojos"
	};
	return s < MUS_NUM_SENAS ? names[s] : "?";
}

bool mus_make_sena(Mus* g, int player, MusSena s)
{
	if (g->phase > MUS_PHASE_BET || player < 0 || player >= MUS_PLAYERS || s >= MUS_NUM_SENAS)
		return false;
	g->sena[player] = s;
	return true;
}

/* ---------------------------------------------------------------------------
 * IA
 * ------------------------------------------------------------------------- */

/* Probabilidad estimada (0..100) de que la mano gane el lance a una mano al
 * azar con las cartas que no se ven. */
static int win_chance(const Mus* g, int player, int lance, bool punto)
{
	const Card* hand = g->hands[player];
	Card pool[DECK_SIZE];
	int n = 0;
	for (int i = 0; i < DECK_SIZE; i++) {
		Card c = card_from_index(i);
		bool mine = false;
		for (int k = 0; k < MUS_HAND_SIZE; k++)
			if (card_equal(c, hand[k]))
				mine = true;
		if (!mine)
			pool[n++] = c;
	}
	Rng rng;
	rng_seed(&rng, g->rng.state ^ (uint32_t)(player * 7919 + lance * 104729));
	const int samples = 160;
	int score = 0;
	for (int s = 0; s < samples; s++) {
		Card other[MUS_HAND_SIZE];
		for (int k = 0; k < MUS_HAND_SIZE; k++) {
			int j = k + rng_range(&rng, n - k);
			Card t = pool[k];
			pool[k] = pool[j];
			pool[j] = t;
			other[k] = pool[k];
		}
		if (lance == MUS_LANCE_PARES && mus_pares_kind(other) == 0) {
			score += 2;
			continue;
		}
		if (lance == MUS_LANCE_JUEGO && !punto && !mus_has_juego(other)) {
			score += 2;
			continue;
		}
		int c = mus_compare(lance, punto, hand, other);
		score += c > 0 ? 2 : c == 0 ? 1 : 0;
	}
	return score * 100 / (2 * samples);
}

/* Fuerza contra la pareja rival (hay que ganar a los dos), teniendo en
 * cuenta lo que el compañero ha dicho con sus señas. */
static int team_chance(const Mus* g, int player, int lance)
{
	int p = win_chance(g, player, lance, g->punto);
	int chance = p * p / 100;
	int floor = 0;
	switch (g->sena[mus_partner(player)]) {
	case MUS_SENA_REYES:
		floor = lance == MUS_LANCE_GRANDE ? 75 : 0;
		break;
	case MUS_SENA_ASES:
		floor = lance == MUS_LANCE_CHICA ? 75 : 0;
		break;
	case MUS_SENA_MEDIAS:
		floor = lance == MUS_LANCE_PARES ? 70 : 0;
		break;
	case MUS_SENA_DUPLES:
		floor = lance == MUS_LANCE_PARES ? 85 : 0;
		break;
	case MUS_SENA_31:
		floor = lance == MUS_LANCE_JUEGO && !g->punto ? 85 : 0;
		break;
	case MUS_SENA_CIEGO:
		chance -= 10;
		break;
	default:
		break;
	}
	if (chance < floor)
		chance = floor;
	return chance < 0 ? 0 : chance;
}

bool mus_ai_wants_mus(const Mus* g, int player)
{
	const Card* h = g->hands[player];
	int kind = mus_pares_kind(h);
	int sum = mus_juego_sum(h);
	int reyes = 0;
	for (int i = 0; i < MUS_HAND_SIZE; i++)
		if (mus_rank(h[i]) == 10)
			reyes++;
	bool good = sum == 31 || kind >= 2 || (kind == 1 && sum >= 31) || reyes >= 3;
	/* Si el compañero ha hecho seña de buena jugada, se corta con algo decente */
	MusSena s = g->sena[mus_partner(player)];
	if ((s == MUS_SENA_DUPLES || s == MUS_SENA_MEDIAS || s == MUS_SENA_31) && (kind >= 1 || reyes >= 2))
		good = true;
	return !good;
}

unsigned mus_ai_discard(const Mus* g, int player)
{
	const Card* h = g->hands[player];
	int count[11] = { 0 };
	for (int i = 0; i < MUS_HAND_SIZE; i++)
		count[mus_rank(h[i])]++;
	unsigned mask = 0;
	for (int i = 0; i < MUS_HAND_SIZE; i++) {
		int r = mus_rank(h[i]);
		/* Se guardan los reyes y lo que forma pareja; el resto se tira */
		if (r != 10 && count[r] < 2)
			mask |= 1u << i;
	}
	if (mask == 0) {
		/* Todo sirve: se tira la carta más baja que no sea rey */
		int worst = 0;
		for (int i = 1; i < MUS_HAND_SIZE; i++)
			if (mus_rank(h[i]) < mus_rank(h[worst]))
				worst = i;
		mask = 1u << worst;
	}
	return mask;
}

MusAction mus_ai_bet(const Mus* g, int player, int* amount)
{
	int chance = team_chance(g, player, g->lance);
	/* El compañero de la mano también cuenta: si es la mano gana los empates */
	*amount = 2;
	int to_win = MUS_GAME_POINTS - g->score[mus_team(player)];
	int to_lose = MUS_GAME_POINTS - g->score[1 - mus_team(player)];

	if (!mus_responding(g)) {
		/* De vez en cuando, un órdago de farol... o no tan de farol */
		Rng rng;
		rng_seed(&rng, g->rng.state ^ (uint32_t)(player * 31 + g->lance * 17 + g->hands_played * 131));
		if (chance >= 90 && (to_lose <= 5 || rng_range(&rng, 100) < 5))
			return MUS_ACT_ORDAGO;
		if (chance >= 55)
			return MUS_ACT_ENVIDO;
		return MUS_ACT_PASO;
	}
	if (g->bet_pending == MUS_ORDAGO)
		return (chance >= 70 || (to_lose <= 3 && chance >= 35)) ? MUS_ACT_QUIERO : MUS_ACT_NO_QUIERO;
	int pending = g->bet_pending;
	if (chance >= 80 && pending < 10)
		return MUS_ACT_ENVIDO;
	if (chance >= 40 + pending * 3 || (to_win <= pending && chance >= 40))
		return MUS_ACT_QUIERO;
	return MUS_ACT_NO_QUIERO;
}
