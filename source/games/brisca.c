#include "games/brisca.h"

int br_card_points(Card c)
{
	switch (c.rank) {
	case 1:
		return 11;
	case 3:
		return 10;
	case RANK_REY:
		return 4;
	case RANK_CABALLO:
		return 3;
	case RANK_SOTA:
		return 2;
	default:
		return 0;
	}
}

int br_card_strength(Card c)
{
	/* índice = rango: 1..12 */
	static const int strength[13] = { 0, 10, 1, 9, 2, 3, 4, 5, 0, 0, 6, 7, 8 };
	return strength[c.rank];
}

bool br_beats(Card follow, Card lead, int trump)
{
	if (follow.suit == lead.suit)
		return br_card_strength(follow) > br_card_strength(lead);
	return follow.suit == trump;
}

void br_init(Brisca* g, uint32_t seed)
{
	rng_seed(&g->rng, seed);
	for (int p = 0; p < BR_PLAYERS; p++)
		g->games_won[p] = 0;
	g->first_leader = 1; /* br_new_game lo alterna: empieza saliendo el humano */
	br_new_game(g);
}

static bool draw_to(Brisca* g, int player)
{
	Card c;
	if (g->hand_count[player] >= BR_HAND_SIZE || !deck_draw(&g->deck, &c))
		return false;
	g->hands[player][g->hand_count[player]++] = c;
	return true;
}

void br_new_game(Brisca* g)
{
	deck_init(&g->deck);
	deck_shuffle(&g->deck, &g->rng);
	for (int p = 0; p < BR_PLAYERS; p++) {
		g->hand_count[p] = 0;
		g->points[p] = 0;
		g->won_cards[p] = 0;
		g->trick_played[p] = false;
	}
	for (int i = 0; i < BR_HAND_SIZE; i++)
		for (int p = 0; p < BR_PLAYERS; p++)
			draw_to(g, p);

	/* La muestra pasa al fondo del mazo para robarse la última. */
	Card muestra;
	deck_draw(&g->deck, &muestra);
	for (int i = g->deck.count; i > 0; i--)
		g->deck.cards[i] = g->deck.cards[i - 1];
	g->deck.cards[0] = muestra;
	g->deck.count++;
	g->trump_card = muestra;
	g->trump = muestra.suit;

	g->first_leader = 1 - g->first_leader;
	g->leader = g->current = g->first_leader;
	g->trick_winner = -1;
	g->trick_points = 0;
	g->tricks = 0;
	g->swapped = false;
	g->phase = BR_PHASE_PLAYING;
}

bool br_play(Brisca* g, int hand_index)
{
	int p = g->current;
	if (g->phase != BR_PHASE_PLAYING || hand_index < 0 || hand_index >= g->hand_count[p])
		return false;
	g->trick[p] = g->hands[p][hand_index];
	g->trick_played[p] = true;
	for (int i = hand_index; i < g->hand_count[p] - 1; i++)
		g->hands[p][i] = g->hands[p][i + 1];
	g->hand_count[p]--;
	g->swapped = false;

	int other = 1 - p;
	if (!g->trick_played[other]) {
		g->current = other;
		return true;
	}
	/* Baza completa: p es quien ha jugado en segundo lugar. */
	Card lead = g->trick[g->leader];
	Card follow = g->trick[1 - g->leader];
	g->trick_winner = br_beats(follow, lead, g->trump) ? 1 - g->leader : g->leader;
	g->trick_points = br_card_points(lead) + br_card_points(follow);
	g->phase = BR_PHASE_TRICK_DONE;
	return true;
}

bool br_collect(Brisca* g)
{
	if (g->phase != BR_PHASE_TRICK_DONE)
		return false;
	int w = g->trick_winner;
	g->points[w] += g->trick_points;
	g->won_cards[w] += BR_PLAYERS;
	g->trick_played[0] = g->trick_played[1] = false;
	g->tricks++;

	draw_to(g, w);
	draw_to(g, 1 - w);

	g->leader = g->current = w;
	if (g->hand_count[0] == 0 && g->hand_count[1] == 0) {
		int winner = br_game_winner(g);
		if (winner >= 0)
			g->games_won[winner]++;
		g->phase = BR_PHASE_GAME_OVER;
	} else {
		g->phase = BR_PHASE_PLAYING;
	}
	return true;
}

int br_swap_index(const Brisca* g, int player)
{
	/* Solo al salir, antes de jugar, y mientras quede mazo además de la muestra. */
	if (g->phase != BR_PHASE_PLAYING || g->current != player || g->leader != player ||
		g->trick_played[player] || g->deck.count < 2)
		return -1;
	int needed = (g->trump_card.rank == 1 || g->trump_card.rank == 3 || g->trump_card.rank >= RANK_SOTA) ? 7 : 2;
	if (g->trump_card.rank == needed)
		return -1;
	for (int i = 0; i < g->hand_count[player]; i++)
		if (g->hands[player][i].suit == g->trump && g->hands[player][i].rank == needed)
			return i;
	return -1;
}

bool br_swap(Brisca* g, int player)
{
	int idx = br_swap_index(g, player);
	if (idx < 0)
		return false;
	Card tmp = g->hands[player][idx];
	g->hands[player][idx] = g->trump_card;
	g->trump_card = tmp;
	g->deck.cards[0] = tmp;
	g->swapped = true;
	return true;
}

int br_ai_choose(const Brisca* g, int player)
{
	int best = 0;
	int best_score = -100000;
	int n = g->hand_count[player];
	bool leading = !g->trick_played[1 - player];

	for (int i = 0; i < n; i++) {
		Card c = g->hands[player][i];
		bool is_trump = c.suit == g->trump;
		int score;
		if (leading) {
			/* Salir con lo más barato: sin puntos y sin gastar triunfos. */
			score = -br_card_points(c) * 10 - (is_trump ? 60 + br_card_strength(c) * 5 : 0) - br_card_strength(c);
		} else {
			Card lead = g->trick[1 - player];
			int pts = br_card_points(lead) + br_card_points(c);
			bool wins = br_beats(c, lead, g->trump);
			if (wins) {
				/* Ganar la baza; usar un triunfo tiene un coste. */
				score = pts * 10 - (is_trump ? 30 + br_card_strength(c) * 3 : br_card_strength(c));
			} else {
				score = -pts * 10 - (is_trump ? 40 : 0) - br_card_strength(c);
			}
		}
		if (score > best_score) {
			best_score = score;
			best = i;
		}
	}
	return best;
}

int br_game_winner(const Brisca* g)
{
	if (g->points[0] > BR_TOTAL_POINTS / 2)
		return 0;
	if (g->points[1] > BR_TOTAL_POINTS / 2)
		return 1;
	return -1;
}
