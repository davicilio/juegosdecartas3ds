#include "games/uno.h"

int uno_card_points(UnoCard c)
{
	if (c.value <= 9)
		return c.value;
	if (c.value >= UNO_WILDCARD)
		return 50;
	return 20;
}

bool uno_is_wild(UnoCard c)
{
	return c.value >= UNO_WILDCARD;
}

void uno_init(Uno* g, uint32_t seed)
{
	rng_seed(&g->rng, seed);
	g->dealer = UNO_PLAYERS - 1;
	uno_new_game(g);
}

void uno_new_game(Uno* g)
{
	for (int p = 0; p < UNO_PLAYERS; p++)
		g->score[p] = 0;
	uno_new_round(g);
}

static void shuffle(Uno* g, UnoCard* cards, int n)
{
	for (int i = n - 1; i > 0; i--) {
		int j = rng_range(&g->rng, i + 1);
		UnoCard t = cards[i];
		cards[i] = cards[j];
		cards[j] = t;
	}
}

/* Saca una carta del mazo; si se acaba, se baraja el montón menos la de arriba */
static bool take(Uno* g, UnoCard* out)
{
	if (g->deck_count == 0) {
		if (g->discard_count <= 1)
			return false;
		UnoCard top = g->discard[g->discard_count - 1];
		for (int i = 0; i < g->discard_count - 1; i++)
			g->deck[g->deck_count++] = g->discard[i];
		g->discard[0] = top;
		g->discard_count = 1;
		shuffle(g, g->deck, g->deck_count);
	}
	*out = g->deck[--g->deck_count];
	return true;
}

static int give(Uno* g, int player, int n)
{
	int got = 0;
	UnoCard c;
	while (got < n && take(g, &c)) {
		g->hands[player][g->hand_count[player]++] = c;
		got++;
	}
	if (g->hand_count[player] > 1)
		g->said_uno[player] = false;
	return got;
}

void uno_new_round(Uno* g)
{
	g->deck_count = 0;
	for (int col = 0; col < UNO_COLORS; col++) {
		g->deck[g->deck_count++] = (UnoCard){ (uint8_t)col, 0 };
		for (int v = 1; v <= UNO_DRAW2; v++)
			for (int k = 0; k < 2; k++)
				g->deck[g->deck_count++] = (UnoCard){ (uint8_t)col, (uint8_t)v };
	}
	for (int k = 0; k < 4; k++) {
		g->deck[g->deck_count++] = (UnoCard){ UNO_WILD, UNO_WILDCARD };
		g->deck[g->deck_count++] = (UnoCard){ UNO_WILD, UNO_DRAW4 };
	}
	shuffle(g, g->deck, g->deck_count);
	g->discard_count = 0;
	for (int p = 0; p < UNO_PLAYERS; p++) {
		g->hand_count[p] = 0;
		g->said_uno[p] = false;
	}
	g->dealer = (g->dealer + 1) % UNO_PLAYERS;
	for (int k = 0; k < UNO_START_HAND; k++)
		for (int p = 0; p < UNO_PLAYERS; p++)
			give(g, p, 1);
	/* La primera carta del montón no puede ser un comodín */
	UnoCard first;
	do {
		take(g, &first);
		g->discard[g->discard_count++] = first;
	} while (uno_is_wild(first));
	g->color = first.color;
	g->direction = 1;
	g->current = (g->dealer + 1) % UNO_PLAYERS;
	g->drew = false;
	g->drawn_index = -1;
	g->round_winner = -1;
	g->round_points = 0;
	g->last_player = -1;
	g->last_penalty_player = -1;
	g->last_penalty = 0;
	g->last_skipped = false;
	g->phase = UNO_PHASE_PLAYING;
	/* La carta de salida hace su efecto sobre el primer jugador */
	if (first.value == UNO_REVERSE) {
		g->direction = -1;
		g->current = g->dealer;
	} else if (first.value == UNO_SKIP) {
		g->current = uno_next_player(g, g->current);
	} else if (first.value == UNO_DRAW2) {
		give(g, g->current, 2);
		g->current = uno_next_player(g, g->current);
	}
}

UnoCard uno_top(const Uno* g)
{
	return g->discard[g->discard_count - 1];
}

int uno_next_player(const Uno* g, int from)
{
	return (from + g->direction + UNO_PLAYERS) % UNO_PLAYERS;
}

bool uno_can_play(const Uno* g, int player, int hand_index)
{
	if (g->phase != UNO_PHASE_PLAYING || player != g->current || hand_index < 0 ||
		hand_index >= g->hand_count[player])
		return false;
	if (g->drew && hand_index != g->drawn_index)
		return false; /* tras robar solo se puede echar la robada */
	UnoCard c = g->hands[player][hand_index];
	if (uno_is_wild(c))
		return true;
	UnoCard top = uno_top(g);
	return c.color == g->color || c.value == top.value;
}

static void end_round(Uno* g, int winner)
{
	int pts = 0;
	for (int p = 0; p < UNO_PLAYERS; p++)
		for (int i = 0; i < g->hand_count[p]; i++)
			pts += uno_card_points(g->hands[p][i]);
	g->round_winner = winner;
	g->round_points = pts;
	g->score[winner] += pts;
	g->phase = uno_game_winner(g) >= 0 ? UNO_PHASE_GAME_OVER : UNO_PHASE_ROUND_OVER;
}

/* Pasa el turno aplicando el efecto de la carta echada */
static void advance(Uno* g, UnoCard c)
{
	int p = g->current;
	int next = uno_next_player(g, p);
	g->last_skipped = false;
	if (c.value == UNO_SKIP) {
		g->last_skipped = true;
		next = uno_next_player(g, next);
	} else if (c.value == UNO_DRAW2 || c.value == UNO_DRAW4) {
		int n = c.value == UNO_DRAW2 ? 2 : 4;
		give(g, next, n);
		g->last_penalty_player = next;
		g->last_penalty = n;
		g->last_skipped = true;
		next = uno_next_player(g, next);
	}
	g->current = next;
	g->drew = false;
	g->drawn_index = -1;
}

bool uno_play(Uno* g, int hand_index)
{
	int p = g->current;
	if (!uno_can_play(g, p, hand_index))
		return false;
	UnoCard c = g->hands[p][hand_index];
	for (int i = hand_index; i < g->hand_count[p] - 1; i++)
		g->hands[p][i] = g->hands[p][i + 1];
	g->hand_count[p]--;
	g->discard[g->discard_count++] = c;
	g->last_player = p;
	g->last_card = c;
	g->last_penalty_player = -1;
	g->last_penalty = 0;
	if (g->hand_count[p] != 1)
		g->said_uno[p] = false;

	if (g->hand_count[p] == 0) {
		/* El roba dos o cuatro de la última carta también se roba */
		if (c.value == UNO_DRAW2 || c.value == UNO_DRAW4) {
			give(g, uno_next_player(g, p), c.value == UNO_DRAW2 ? 2 : 4);
		}
		end_round(g, p);
		return true;
	}
	if (uno_is_wild(c)) {
		g->phase = UNO_PHASE_CHOOSE_COLOR;
		return true;
	}
	g->color = c.color;
	if (c.value == UNO_REVERSE)
		g->direction = -g->direction;
	advance(g, c);
	return true;
}

bool uno_choose_color(Uno* g, int color)
{
	if (g->phase != UNO_PHASE_CHOOSE_COLOR || color < 0 || color >= UNO_COLORS)
		return false;
	g->color = color;
	g->phase = UNO_PHASE_PLAYING;
	advance(g, uno_top(g));
	return true;
}

bool uno_draw(Uno* g)
{
	if (g->phase != UNO_PHASE_PLAYING || g->drew)
		return false;
	int p = g->current;
	g->drew = true;
	g->last_penalty_player = -1;
	if (give(g, p, 1) == 0) {
		/* No quedan cartas en ningún sitio: se pasa */
		uno_pass(g);
		return true;
	}
	g->drawn_index = g->hand_count[p] - 1;
	if (!uno_can_play(g, p, g->drawn_index))
		uno_pass(g); /* la robada no vale: se pasa el turno */
	return true;
}

bool uno_pass(Uno* g)
{
	if (g->phase != UNO_PHASE_PLAYING || !g->drew)
		return false;
	g->current = uno_next_player(g, g->current);
	g->drew = false;
	g->drawn_index = -1;
	g->last_skipped = false;
	return true;
}

bool uno_say_uno(Uno* g, int player)
{
	int n = g->hand_count[player];
	if (g->phase >= UNO_PHASE_ROUND_OVER || n > 2 || (n == 2 && g->current != player))
		return false;
	g->said_uno[player] = true;
	return true;
}

bool uno_catchable(const Uno* g, int player)
{
	return g->phase < UNO_PHASE_ROUND_OVER && g->hand_count[player] == 1 && !g->said_uno[player];
}

bool uno_catch(Uno* g, int player)
{
	if (!uno_catchable(g, player))
		return false;
	give(g, player, 2);
	g->last_penalty_player = player;
	g->last_penalty = 2;
	return true;
}

int uno_game_winner(const Uno* g)
{
	for (int p = 0; p < UNO_PLAYERS; p++)
		if (g->score[p] >= UNO_GAME_POINTS)
			return p;
	return -1;
}

/* ---------------------------------------------------------------------------
 * IA
 * ------------------------------------------------------------------------- */

int uno_ai_color(const Uno* g, int player)
{
	/* El color del que más cartas tiene */
	int count[UNO_COLORS] = { 0 };
	for (int i = 0; i < g->hand_count[player]; i++)
		if (g->hands[player][i].color < UNO_COLORS)
			count[g->hands[player][i].color] += 1 + (g->hands[player][i].value > 9);
	int best = UNO_RED;
	for (int c = 0; c < UNO_COLORS; c++)
		if (count[c] > count[best])
			best = c;
	return best;
}

int uno_ai_choose(const Uno* g, int player)
{
	int next = uno_next_player(g, player);
	bool danger = g->hand_count[next] <= 2; /* al siguiente le quedan pocas */
	int count[UNO_COLORS] = { 0 };
	for (int i = 0; i < g->hand_count[player]; i++)
		if (g->hands[player][i].color < UNO_COLORS)
			count[g->hands[player][i].color]++;

	int best = -1, best_score = -100000;
	for (int i = 0; i < g->hand_count[player]; i++) {
		if (!uno_can_play(g, player, i))
			continue;
		UnoCard c = g->hands[player][i];
		int score;
		if (uno_is_wild(c)) {
			/* Los comodines se guardan para cuando hacen falta */
			score = -60 + (c.value == UNO_DRAW4 && danger ? 120 : 0);
		} else {
			score = count[c.color] * 4 + uno_card_points(c);
			if (c.value > 9 && danger)
				score += 60; /* frenar al que está a punto de ganar */
			if (c.value == UNO_REVERSE && danger)
				score += 10;
		}
		if (score > best_score) {
			best_score = score;
			best = i;
		}
	}
	return best;
}
