#include "games/sieteymedia.h"

#include <stdio.h>

int sym_card_halves(Card c)
{
	return c.rank <= 7 ? c.rank * 2 : 1;
}

int sym_hand_halves(const SymHand* hand)
{
	int total = 0;
	for (int i = 0; i < hand->count; i++)
		total += sym_card_halves(hand->cards[i]);
	return total;
}

void sym_format_value(int halves, char* buf, int size)
{
	if (halves % 2)
		snprintf(buf, size, "%d\xC2\xBD", halves / 2); /* "½" */
	else
		snprintf(buf, size, "%d", halves / 2);
}

static void hand_add(SieteYMedia* g, SymHand* hand)
{
	Card c;
	if (hand->count < SYM_MAX_HAND && deck_draw(&g->deck, &c))
		hand->cards[hand->count++] = c;
}

static void clamp_bet(SieteYMedia* g)
{
	if (g->bet > SYM_MAX_BET)
		g->bet = SYM_MAX_BET;
	if (g->bet > g->chips)
		g->bet = g->chips;
	if (g->bet < SYM_MIN_BET)
		g->bet = g->chips < SYM_MIN_BET ? g->chips : SYM_MIN_BET;
}

void sym_init(SieteYMedia* g, uint32_t seed)
{
	rng_seed(&g->rng, seed);
	g->player.count = 0;
	g->bank.count = 0;
	g->chips = SYM_START_CHIPS;
	g->bet = 10;
	g->last_delta = 0;
	g->rounds_played = 0;
	g->rounds_won = 0;
	g->phase = SYM_PHASE_BETTING;
	g->outcome = SYM_OUTCOME_NONE;
	deck_init(&g->deck);
}

void sym_change_bet(SieteYMedia* g, int delta)
{
	if (g->phase != SYM_PHASE_BETTING)
		return;
	g->bet += delta;
	clamp_bet(g);
}

bool sym_deal(SieteYMedia* g)
{
	if (g->phase != SYM_PHASE_BETTING || g->bet <= 0 || g->bet > g->chips)
		return false;
	/* Cada mano se juega con la baraja completa recién barajada. */
	deck_init(&g->deck);
	deck_shuffle(&g->deck, &g->rng);
	g->player.count = 0;
	g->bank.count = 0;
	g->outcome = SYM_OUTCOME_NONE;
	g->last_delta = 0;
	hand_add(g, &g->player);
	hand_add(g, &g->bank);
	g->phase = SYM_PHASE_PLAYER;
	return true;
}

static void resolve(SieteYMedia* g)
{
	int p = sym_hand_halves(&g->player);
	int b = sym_hand_halves(&g->bank);

	if (p > SYM_LIMIT)
		g->outcome = SYM_OUTCOME_PLAYER_BUST;
	else if (b > SYM_LIMIT)
		g->outcome = SYM_OUTCOME_BANK_BUST;
	else if (p > b)
		g->outcome = SYM_OUTCOME_PLAYER_WINS;
	else
		g->outcome = SYM_OUTCOME_BANK_WINS;

	bool player_won = g->outcome == SYM_OUTCOME_BANK_BUST || g->outcome == SYM_OUTCOME_PLAYER_WINS;
	if (player_won && p == SYM_LIMIT && g->player.count == 2) {
		g->outcome = SYM_OUTCOME_REAL;
		g->last_delta = g->bet * 2;
	} else {
		g->last_delta = player_won ? g->bet : -g->bet;
	}

	g->chips += g->last_delta;
	g->rounds_played++;
	if (player_won)
		g->rounds_won++;
	g->phase = g->chips <= 0 ? SYM_PHASE_GAME_OVER : SYM_PHASE_ROUND_OVER;
}

bool sym_hit(SieteYMedia* g)
{
	if (g->phase != SYM_PHASE_PLAYER)
		return false;
	hand_add(g, &g->player);
	int v = sym_hand_halves(&g->player);
	if (v > SYM_LIMIT)
		resolve(g); /* se ha pasado: la banca no necesita jugar */
	else if (v == SYM_LIMIT)
		g->phase = SYM_PHASE_BANK; /* con 7½ no tiene sentido seguir */
	return true;
}

bool sym_stand(SieteYMedia* g)
{
	if (g->phase != SYM_PHASE_PLAYER)
		return false;
	g->phase = SYM_PHASE_BANK;
	return true;
}

bool sym_bank_wants_card(const SieteYMedia* g)
{
	int bank = sym_hand_halves(&g->bank);
	if (bank >= SYM_LIMIT)
		return false;

	/* La primera carta del jugador está boca abajo: la banca solo conoce el resto. */
	int visible = 0;
	for (int i = 1; i < g->player.count; i++)
		visible += sym_card_halves(g->player.cards[i]);
	int min_player = visible + 1;
	int max_player = visible + 14;
	if (max_player > SYM_LIMIT)
		max_player = SYM_LIMIT;

	if (bank >= max_player)
		return false; /* gana seguro (los empates son de la banca) */
	if (bank < min_player)
		return true; /* pierde seguro si no roba */

	int target = visible + 8;
	if (target > 11)
		target = 11; /* a partir de 5½ el riesgo de pasarse es demasiado alto */
	return bank < target;
}

bool sym_bank_step(SieteYMedia* g)
{
	if (g->phase != SYM_PHASE_BANK)
		return false;
	if (sym_bank_wants_card(g) && g->deck.count > 0) {
		hand_add(g, &g->bank);
		if (sym_hand_halves(&g->bank) > SYM_LIMIT)
			resolve(g);
		return true;
	}
	resolve(g);
	return false;
}

bool sym_next_round(SieteYMedia* g)
{
	if (g->phase != SYM_PHASE_ROUND_OVER)
		return false;
	g->player.count = 0;
	g->bank.count = 0;
	g->outcome = SYM_OUTCOME_NONE;
	g->phase = SYM_PHASE_BETTING;
	clamp_bet(g);
	return true;
}
