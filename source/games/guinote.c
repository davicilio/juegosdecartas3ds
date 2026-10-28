#include "games/guinote.h"

int gn_sides(const Guinote* g)
{
	return g->variant == GN_TUTE_CABRON ? GN_PLAYERS : 2;
}

int gn_side(const Guinote* g, int player)
{
	return g->variant == GN_TUTE_CABRON ? player : player % 2;
}

int gn_hand_size(const Guinote* g)
{
	return g->variant == GN_GUINOTE ? 6 : GN_MAX_HAND;
}

int gn_cante_rank(const Guinote* g)
{
	return g->variant == GN_GUINOTE ? RANK_SOTA : RANK_CABALLO;
}

int gn_card_points(const Guinote* g, Card c)
{
	switch (c.rank) {
	case 1:
		return 11;
	case 3:
		return 10;
	case RANK_REY:
		return 4;
	case RANK_SOTA:
		return g->variant == GN_GUINOTE ? 3 : 2;
	case RANK_CABALLO:
		return g->variant == GN_GUINOTE ? 2 : 3;
	default:
		return 0;
	}
}

int gn_card_strength(const Guinote* g, Card c)
{
	/* índice = rango: 1..12. En el guiñote la sota gana al caballo. */
	static const int guinote[13] = { 0, 10, 1, 9, 2, 3, 4, 5, 0, 0, 7, 6, 8 };
	static const int tute[13] = { 0, 10, 1, 9, 2, 3, 4, 5, 0, 0, 6, 7, 8 };
	if (c.rank > 12)
		return 0;
	return g->variant == GN_GUINOTE ? guinote[c.rank] : tute[c.rank];
}

bool gn_beats(const Guinote* g, Card challenger, Card best)
{
	if (challenger.suit == best.suit)
		return gn_card_strength(g, challenger) > gn_card_strength(g, best);
	return challenger.suit == g->trump;
}

bool gn_arrastre(const Guinote* g)
{
	return g->deck.count == 0;
}

/* Se puede cantar: en el guiñote solo mientras queda mazo */
static bool cantes_open(const Guinote* g)
{
	return g->variant != GN_GUINOTE || !gn_arrastre(g);
}

void gn_init(Guinote* g, GnVariant variant, uint32_t seed)
{
	rng_seed(&g->rng, seed);
	g->variant = variant;
	for (int s = 0; s < GN_PLAYERS; s++)
		g->games_won[s] = 0;
	g->first_leader = 0;
	gn_new_game(g);
}

void gn_new_game(Guinote* g)
{
	for (int s = 0; s < GN_PLAYERS; s++)
		g->score[s] = 0;
	g->hands_played = 0;
	gn_new_hand(g);
}

static bool draw_to(Guinote* g, int player)
{
	Card c;
	if (g->hand_count[player] >= gn_hand_size(g) || !deck_draw(&g->deck, &c))
		return false;
	g->hands[player][g->hand_count[player]++] = c;
	return true;
}

void gn_new_hand(Guinote* g)
{
	deck_init(&g->deck);
	deck_shuffle(&g->deck, &g->rng);
	for (int p = 0; p < GN_PLAYERS; p++) {
		g->hand_count[p] = 0;
		g->trick_played[p] = false;
	}
	/* Se reparte empezando por quien sale; la última carta es de quien reparte */
	Card last = { 0, 1 };
	for (int i = 0; i < gn_hand_size(g); i++)
		for (int k = 0; k < GN_PLAYERS; k++) {
			int p = (g->first_leader + k) % GN_PLAYERS;
			draw_to(g, p);
			last = g->hands[p][g->hand_count[p] - 1];
		}

	if (g->variant == GN_GUINOTE) {
		/* La muestra pasa al fondo del mazo para robarse la última. */
		Card muestra;
		deck_draw(&g->deck, &muestra);
		for (int i = g->deck.count; i > 0; i--)
			g->deck.cards[i] = g->deck.cards[i - 1];
		g->deck.cards[0] = muestra;
		g->deck.count++;
		g->trump_card = muestra;
	} else {
		g->trump_card = last; /* la última carta repartida, que se queda quien reparte */
	}
	g->trump = g->trump_card.suit;

	for (int s = 0; s < NUM_SUITS; s++)
		g->sung[s] = false;
	for (int s = 0; s < GN_PLAYERS; s++)
		g->points[s] = 0;
	g->leader = g->current = g->first_leader;
	g->first_leader = (g->first_leader + 1) % GN_PLAYERS;
	g->played_in_trick = 0;
	g->trick_winner = -1;
	g->trick_points = 0;
	g->last_trick_side = -1;
	g->hand_winner = -1;
	g->tute = false;
	g->last_cante_player = -1;
	g->last_cante_suit = -1;
	g->last_cante_points = 0;
	g->swapped = false;
	g->phase = GN_PHASE_PLAYING;
}

int gn_trick_leader_now(const Guinote* g)
{
	if (g->played_in_trick == 0)
		return -1;
	int best = g->leader;
	for (int k = 1; k < g->played_in_trick; k++) {
		int p = (g->leader + k) % GN_PLAYERS;
		if (gn_beats(g, g->trick[p], g->trick[best]))
			best = p;
	}
	return best;
}

bool gn_can_play(const Guinote* g, int player, int hand_index)
{
	if (g->phase != GN_PHASE_PLAYING || player != g->current || hand_index < 0 ||
		hand_index >= g->hand_count[player])
		return false;
	if (!gn_arrastre(g) || g->played_in_trick == 0)
		return true;

	const Card* hand = g->hands[player];
	int n = g->hand_count[player];
	Card c = hand[hand_index];
	Card lead = g->trick[g->leader];
	int best_player = gn_trick_leader_now(g);
	Card best = g->trick[best_player];
	bool partner_winning = best_player != player && gn_side(g, best_player) == gn_side(g, player);

	bool has_suit = false, can_beat_in_suit = false, has_trump = false, can_beat_trump = false;
	for (int i = 0; i < n; i++) {
		if (hand[i].suit == lead.suit) {
			has_suit = true;
			if (gn_beats(g, hand[i], best))
				can_beat_in_suit = true;
		}
		if (hand[i].suit == g->trump) {
			has_trump = true;
			if (gn_beats(g, hand[i], best))
				can_beat_trump = true;
		}
	}
	if (has_suit) {
		if (c.suit != lead.suit)
			return false;
		if (!partner_winning && can_beat_in_suit)
			return gn_beats(g, c, best);
		return true;
	}
	if (partner_winning || !has_trump)
		return true;
	/* Hay que fallar, y por encima si se puede */
	if (c.suit != g->trump)
		return false;
	if (can_beat_trump)
		return gn_beats(g, c, best);
	return true;
}

bool gn_play(Guinote* g, int hand_index)
{
	int p = g->current;
	if (!gn_can_play(g, p, hand_index))
		return false;
	g->trick[p] = g->hands[p][hand_index];
	g->trick_played[p] = true;
	for (int i = hand_index; i < g->hand_count[p] - 1; i++)
		g->hands[p][i] = g->hands[p][i + 1];
	g->hand_count[p]--;
	g->played_in_trick++;
	g->swapped = false;

	if (g->played_in_trick < GN_PLAYERS) {
		g->current = (p + 1) % GN_PLAYERS;
		return true;
	}
	g->trick_winner = gn_trick_leader_now(g);
	g->trick_points = 0;
	for (int q = 0; q < GN_PLAYERS; q++)
		g->trick_points += gn_card_points(g, g->trick[q]);
	g->phase = GN_PHASE_TRICK_DONE;
	return true;
}

/* Fin de la mano: suma la mano a la partida y mira si alguien ha ganado */
static void end_hand(Guinote* g)
{
	g->hands_played++;
	if (g->variant == GN_GUINOTE) {
		for (int s = 0; s < 2; s++)
			g->score[s] += g->points[s];
		g->hand_winner = g->points[0] >= g->points[1] ? 0 : 1;
	} else {
		if (!g->tute) {
			int best = 0;
			for (int s = 1; s < gn_sides(g); s++)
				if (g->points[s] > g->points[best] ||
					(g->points[s] == g->points[best] && s == g->last_trick_side))
					best = s;
			g->hand_winner = best;
		}
		g->score[g->hand_winner]++;
	}
	int winner = gn_game_winner(g);
	if (winner >= 0) {
		g->games_won[winner]++;
		g->phase = GN_PHASE_GAME_OVER;
	} else {
		g->phase = GN_PHASE_HAND_OVER;
	}
}

bool gn_collect(Guinote* g)
{
	if (g->phase != GN_PHASE_TRICK_DONE)
		return false;
	int w = g->trick_winner;
	int side = gn_side(g, w);
	g->points[side] += g->trick_points;
	g->last_trick_side = side;
	for (int p = 0; p < GN_PLAYERS; p++)
		g->trick_played[p] = false;
	g->played_in_trick = 0;
	g->last_cante_player = -1;

	for (int k = 0; k < GN_PLAYERS; k++)
		draw_to(g, (w + k) % GN_PLAYERS);

	g->leader = g->current = w;
	if (g->hand_count[w] > 0) {
		g->phase = GN_PHASE_PLAYING;
		return true;
	}
	g->points[side] += GN_LAST_TRICK; /* diez de últimas */
	end_hand(g);
	return true;
}

int gn_game_winner(const Guinote* g)
{
	if (g->variant != GN_GUINOTE) {
		for (int s = 0; s < gn_sides(g); s++)
			if (g->score[s] >= GN_TUTE_HANDS)
				return s;
		return -1;
	}
	bool a = g->score[0] >= GN_GAME_POINTS, b = g->score[1] >= GN_GAME_POINTS;
	if (!a && !b)
		return -1;
	if (a && b && g->score[0] == g->score[1])
		return g->last_trick_side >= 0 ? g->last_trick_side : 0;
	if (a && b)
		return g->score[0] > g->score[1] ? 0 : 1;
	return a ? 0 : 1;
}

static bool may_act(const Guinote* g, int player)
{
	/* Tras ganar su bando la última baza, en su turno */
	return g->phase == GN_PHASE_PLAYING && g->current == player && g->last_trick_side == gn_side(g, player) &&
	       !g->trick_played[player];
}

int gn_cante_suit(const Guinote* g, int player)
{
	if (!may_act(g, player) || !cantes_open(g))
		return -1;
	int other = gn_cante_rank(g);
	int found = -1;
	for (int s = 0; s < NUM_SUITS; s++) {
		if (g->sung[s])
			continue;
		bool rey = false, pareja = false;
		for (int i = 0; i < g->hand_count[player]; i++) {
			if (g->hands[player][i].suit != s)
				continue;
			rey |= g->hands[player][i].rank == RANK_REY;
			pareja |= g->hands[player][i].rank == other;
		}
		if (rey && pareja) {
			if (s == g->trump)
				return s;
			if (found < 0)
				found = s;
		}
	}
	return found;
}

bool gn_cante(Guinote* g, int player)
{
	int s = gn_cante_suit(g, player);
	if (s < 0)
		return false;
	int pts = s == g->trump ? 40 : 20;
	g->sung[s] = true;
	g->points[gn_side(g, player)] += pts;
	g->last_cante_player = player;
	g->last_cante_suit = s;
	g->last_cante_points = pts;
	return true;
}

bool gn_can_sing_tute(const Guinote* g, int player)
{
	if (g->variant == GN_GUINOTE || !may_act(g, player))
		return false;
	int reyes = 0, caballos = 0;
	for (int i = 0; i < g->hand_count[player]; i++) {
		reyes += g->hands[player][i].rank == RANK_REY;
		caballos += g->hands[player][i].rank == RANK_CABALLO;
	}
	return reyes == 4 || caballos == 4;
}

bool gn_sing_tute(Guinote* g, int player)
{
	if (!gn_can_sing_tute(g, player))
		return false;
	g->tute = true;
	g->hand_winner = gn_side(g, player);
	g->last_cante_player = player;
	g->last_cante_suit = -1;
	g->last_cante_points = 0;
	end_hand(g);
	return true;
}

int gn_swap_index(const Guinote* g, int player)
{
	/* Guiñote: con mazo además de la muestra, y si la muestra vale más que el 7 */
	if (g->variant != GN_GUINOTE || !may_act(g, player) || gn_arrastre(g) || g->deck.count < 2)
		return -1;
	int r = g->trump_card.rank;
	if (!(r == 1 || r == 3 || r >= RANK_SOTA))
		return -1;
	for (int i = 0; i < g->hand_count[player]; i++)
		if (g->hands[player][i].suit == g->trump && g->hands[player][i].rank == 7)
			return i;
	return -1;
}

bool gn_swap(Guinote* g, int player)
{
	int idx = gn_swap_index(g, player);
	if (idx < 0)
		return false;
	Card tmp = g->hands[player][idx];
	g->hands[player][idx] = g->trump_card;
	g->trump_card = tmp;
	g->deck.cards[0] = tmp;
	g->swapped = true;
	return true;
}

/* ---------------------------------------------------------------------------
 * IA
 * ------------------------------------------------------------------------- */

/* Coste de deshacerse de una carta: puntos, triunfos y piezas para cantar. */
static int keep_value(const Guinote* g, int player, Card c)
{
	int v = gn_card_points(g, c) * 10 + gn_card_strength(g, c);
	if (c.suit == g->trump)
		v += 40 + gn_card_strength(g, c) * 4;
	int other = gn_cante_rank(g);
	if ((c.rank == RANK_REY || c.rank == other) && !g->sung[c.suit] && cantes_open(g)) {
		int need = c.rank == RANK_REY ? other : RANK_REY;
		for (int i = 0; i < g->hand_count[player]; i++)
			if (g->hands[player][i].suit == c.suit && g->hands[player][i].rank == need)
				v += c.suit == g->trump ? 120 : 60;
	}
	return v;
}

int gn_ai_choose(const Guinote* g, int player)
{
	int n = g->hand_count[player];
	int best = -1, best_score = -1000000;
	int leader_now = gn_trick_leader_now(g);
	int trick_pts = 0;
	for (int k = 0; k < g->played_in_trick; k++)
		trick_pts += gn_card_points(g, g->trick[(g->leader + k) % GN_PLAYERS]);
	bool last = g->played_in_trick == GN_PLAYERS - 1;

	for (int i = 0; i < n; i++) {
		if (!gn_can_play(g, player, i))
			continue;
		Card c = g->hands[player][i];
		int keep = keep_value(g, player, c);
		int score;
		if (leader_now < 0) {
			/* Salir: con lo que menos duela, sin regalar puntos */
			score = -keep;
		} else if (leader_now != player && gn_side(g, leader_now) == gn_side(g, player)) {
			/* Gana el compañero: si es seguro, se le cargan puntos */
			Card best_card = g->trick[leader_now];
			bool safe = last || (best_card.suit == g->trump && gn_card_strength(g, best_card) >= 9) ||
			            (best_card.suit != g->trump && gn_card_strength(g, best_card) == 10 && gn_arrastre(g));
			score = safe ? gn_card_points(g, c) * 12 - (c.suit == g->trump ? 80 : 0) - gn_card_strength(g, c) : -keep;
		} else {
			bool wins = gn_beats(g, c, g->trick[leader_now]);
			int pts = trick_pts + gn_card_points(g, c);
			if (wins) {
				/* Merece la pena ganar si hay puntos o se juega el último */
				int worth = pts * 12 + (last ? 15 : 0) + (gn_arrastre(g) ? 20 : 0);
				score = worth - keep / 2;
				if (!last && c.suit != g->trump && gn_card_strength(g, c) < 9)
					score -= 20; /* los que faltan aún pueden ganarla */
			} else {
				score = -keep - pts * 3;
			}
		}
		if (score > best_score) {
			best_score = score;
			best = i;
		}
	}
	return best;
}
