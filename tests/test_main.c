/* Tests de la lógica de los juegos. Se compilan con el gcc del sistema:
 *     make -C tests
 */
#include <stdio.h>
#include <string.h>

#include "core/cards.h"
#include "games/brisca.h"
#include "games/cinquillo.h"
#include "games/escoba.h"
#include "games/sieteymedia.h"

static int failures = 0;
static int checks = 0;

#define CHECK(cond)                                                          \
	do {                                                                     \
		checks++;                                                            \
		if (!(cond)) {                                                       \
			failures++;                                                      \
			printf("FALLO %s:%d: %s\n", __FILE__, __LINE__, #cond);          \
		}                                                                    \
	} while (0)

static void test_deck(void)
{
	Deck d;
	Rng rng;
	rng_seed(&rng, 1234);
	deck_init(&d);
	CHECK(d.count == 40);
	deck_shuffle(&d, &rng);
	int seen[DECK_SIZE] = { 0 };
	for (int i = 0; i < DECK_SIZE; i++) {
		CHECK(card_valid(d.cards[i]));
		seen[card_index(d.cards[i])]++;
	}
	for (int i = 0; i < DECK_SIZE; i++)
		CHECK(seen[i] == 1);

	for (int i = 0; i < DECK_SIZE; i++)
		CHECK(card_index(card_from_index(i)) == i);
	CHECK(card_pos(card_make(SUIT_OROS, 7)) == 6);
	CHECK(card_pos(card_make(SUIT_OROS, RANK_SOTA)) == 7);
	CHECK(card_pos(card_make(SUIT_OROS, RANK_REY)) == 9);

	char buf[32];
	card_describe(card_make(SUIT_COPAS, RANK_CABALLO), buf, sizeof buf);
	CHECK(strcmp(buf, "caballo de copas") == 0);

	Card c;
	int drawn = 0;
	while (deck_draw(&d, &c))
		drawn++;
	CHECK(drawn == 40);
}

static void test_sym_values(void)
{
	SymHand h = { .count = 0 };
	h.cards[h.count++] = card_make(SUIT_OROS, 7);
	h.cards[h.count++] = card_make(SUIT_BASTOS, RANK_REY);
	CHECK(sym_hand_halves(&h) == 15);
	char buf[16];
	sym_format_value(15, buf, sizeof buf);
	CHECK(strcmp(buf, "7\xC2\xBD") == 0);
	sym_format_value(6, buf, sizeof buf);
	CHECK(strcmp(buf, "3") == 0);
}

static void test_sym_bets(void)
{
	SieteYMedia g;
	sym_init(&g, 1);
	CHECK(g.chips == SYM_START_CHIPS);
	sym_change_bet(&g, 1000);
	CHECK(g.bet == SYM_MAX_BET);
	sym_change_bet(&g, -1000);
	CHECK(g.bet == SYM_MIN_BET);
	CHECK(sym_deal(&g));
	CHECK(g.phase == SYM_PHASE_PLAYER);
	CHECK(g.player.count == 1 && g.bank.count == 1);
	/* No se puede cambiar la apuesta en mitad de la mano. */
	sym_change_bet(&g, SYM_BET_STEP);
	CHECK(g.bet == SYM_MIN_BET);
}

static void test_sym_resolve(void)
{
	/* Jugador con siete y media real contra una banca que se planta en 5. */
	SieteYMedia g;
	sym_init(&g, 7);
	g.bet = 10;
	g.phase = SYM_PHASE_BANK;
	g.player.count = 2;
	g.player.cards[0] = card_make(SUIT_OROS, 7);
	g.player.cards[1] = card_make(SUIT_COPAS, RANK_SOTA);
	g.bank.count = 2;
	g.bank.cards[0] = card_make(SUIT_ESPADAS, 3);
	g.bank.cards[1] = card_make(SUIT_ESPADAS, 4);
	/* Con 7 frente a una sota visible la banca se planta. */
	CHECK(!sym_bank_wants_card(&g));
	/* Con 5 frente a ese 7½ seguro (el 7 está boca abajo) tampoco arriesga más. */
	g.bank.cards[1] = card_make(SUIT_ESPADAS, 2);
	CHECK(!sym_bank_wants_card(&g));
	/* Pero si ve un 7 descubierto sabe que pierde y tiene que robar. */
	g.player.cards[0] = card_make(SUIT_COPAS, RANK_SOTA);
	g.player.cards[1] = card_make(SUIT_OROS, 7);
	CHECK(sym_bank_wants_card(&g));
	/* Forzamos la resolución vaciando el mazo para que no pueda robar. */
	g.deck.count = 0;
	while (sym_bank_step(&g))
		;
	CHECK(g.phase == SYM_PHASE_ROUND_OVER);
	CHECK(g.outcome == SYM_OUTCOME_REAL);
	CHECK(g.last_delta == 20);
	CHECK(g.chips == SYM_START_CHIPS + 20);

	/* Empate: gana la banca. */
	sym_init(&g, 7);
	g.bet = 10;
	g.phase = SYM_PHASE_BANK;
	g.player.count = 1;
	g.player.cards[0] = card_make(SUIT_OROS, 5);
	g.bank.count = 1;
	g.bank.cards[0] = card_make(SUIT_COPAS, 5);
	g.deck.count = 0;
	while (sym_bank_step(&g))
		;
	CHECK(g.outcome == SYM_OUTCOME_BANK_WINS);
	CHECK(g.chips == SYM_START_CHIPS - 10);
}

static void test_sym_bust(void)
{
	SieteYMedia g;
	sym_init(&g, 99);
	g.bet = 10;
	CHECK(sym_deal(&g));
	/* Pide cartas hasta pasarse o llegar a 7½. */
	while (g.phase == SYM_PHASE_PLAYER)
		sym_hit(&g);
	int v = sym_hand_halves(&g.player);
	if (v > SYM_LIMIT) {
		CHECK(g.outcome == SYM_OUTCOME_PLAYER_BUST);
		CHECK(g.phase == SYM_PHASE_ROUND_OVER);
		CHECK(g.bank.count == 1);
	} else {
		CHECK(v == SYM_LIMIT);
		CHECK(g.phase == SYM_PHASE_BANK);
	}
}

/* Simula muchas partidas completas para comprobar invariantes. */
static void test_sym_simulation(void)
{
	int player_wins = 0, rounds = 0;
	for (uint32_t seed = 1; seed <= 200; seed++) {
		SieteYMedia g;
		sym_init(&g, seed);
		for (int r = 0; r < 50 && g.phase != SYM_PHASE_GAME_OVER; r++) {
			int before = g.chips;
			CHECK(sym_deal(&g));
			Rng choice;
			rng_seed(&choice, seed * 31 + r);
			int stop_at = 8 + rng_range(&choice, 6);
			while (g.phase == SYM_PHASE_PLAYER) {
				if (sym_hand_halves(&g.player) < stop_at)
					sym_hit(&g);
				else
					sym_stand(&g);
			}
			int guard = 0;
			while (g.phase == SYM_PHASE_BANK && guard++ < 50)
				sym_bank_step(&g);
			CHECK(g.phase == SYM_PHASE_ROUND_OVER || g.phase == SYM_PHASE_GAME_OVER);
			CHECK(g.chips == before + g.last_delta);
			CHECK(g.chips >= 0);
			rounds++;
			if (g.last_delta > 0)
				player_wins++;
			if (g.phase == SYM_PHASE_ROUND_OVER)
				CHECK(sym_next_round(&g));
		}
	}
	printf("  siete y media: %d manos simuladas, el jugador gana el %.1f%%\n", rounds,
		100.0 * player_wins / rounds);
	CHECK(player_wins > rounds / 5); /* la banca no debe ser imbatible */
}

static void test_cq_rules(void)
{
	Cinquillo g;
	cq_init(&g, 42);
	int total = 0;
	for (int p = 0; p < CQ_PLAYERS; p++) {
		CHECK(g.hand_count[p] == CQ_HAND_SIZE);
		total += g.chips[p];
	}
	CHECK(total + g.pot == CQ_PLAYERS * CQ_START_CHIPS);
	CHECK(g.pot == CQ_PLAYERS * CQ_ANTE);

	/* El primero en jugar tiene el cinco de oros y es su única jugada. */
	int moves[CQ_HAND_SIZE];
	int n = cq_legal_moves(&g, g.current, moves);
	CHECK(n == 1);
	CHECK(card_equal(g.hands[g.current][moves[0]], card_make(SUIT_OROS, 5)));
	/* Nadie más puede jugar todavía. */
	for (int p = 0; p < CQ_PLAYERS; p++)
		if (p != g.current)
			CHECK(!cq_can_play(&g, p));
	/* No se puede pasar teniendo jugada. */
	CHECK(!cq_pass(&g));
	int first = g.current;
	CHECK(cq_play(&g, moves[0]));
	CHECK(g.low[SUIT_OROS] == 4 && g.high[SUIT_OROS] == 4);
	CHECK(g.current == (first + 1) % CQ_PLAYERS);

	/* Ahora valen el 4 y el 6 de oros y cualquier cinco. */
	CHECK(cq_is_legal(&g, card_make(SUIT_OROS, 4)));
	CHECK(cq_is_legal(&g, card_make(SUIT_OROS, 6)));
	CHECK(!cq_is_legal(&g, card_make(SUIT_OROS, 7)));
	CHECK(cq_is_legal(&g, card_make(SUIT_COPAS, 5)));
	CHECK(!cq_is_legal(&g, card_make(SUIT_COPAS, 6)));

	/* Tras el 7 de oros va la sota. */
	g.high[SUIT_OROS] = card_pos(card_make(SUIT_OROS, 7));
	CHECK(cq_is_legal(&g, card_make(SUIT_OROS, RANK_SOTA)));
}

static void test_cq_simulation(void)
{
	int games = 0, rounds = 0;
	int wins[CQ_PLAYERS] = { 0 };
	for (uint32_t seed = 1; seed <= 300; seed++) {
		Cinquillo g;
		cq_init(&g, seed);
		int guard = 0;
		while (g.phase != CQ_PHASE_GAME_OVER && guard++ < 10000) {
			if (g.phase == CQ_PHASE_ROUND_OVER) {
				cq_next_round(&g);
				continue;
			}
			int idx = cq_ai_choose(&g, g.current);
			if (idx >= 0)
				CHECK(cq_play(&g, idx));
			else
				CHECK(cq_pass(&g));
			if (g.phase != CQ_PHASE_PLAYING) {
				rounds++;
				CHECK(g.round_winner >= 0);
				CHECK(g.hand_count[g.round_winner] == 0);
				CHECK(g.pot == 0);
				wins[g.round_winner]++;
				/* Cartas en mesa + cartas en mano = 40 */
				int on_table = 0, in_hands = 0;
				for (int s = 0; s < NUM_SUITS; s++)
					if (g.low[s] >= 0)
						on_table += g.high[s] - g.low[s] + 1;
				for (int p = 0; p < CQ_PLAYERS; p++)
					in_hands += g.hand_count[p];
				CHECK(on_table + in_hands == DECK_SIZE);
			}
			int total = g.pot;
			for (int p = 0; p < CQ_PLAYERS; p++) {
				total += g.chips[p];
				CHECK(g.chips[p] >= 0);
			}
			CHECK(total == CQ_PLAYERS * CQ_START_CHIPS);
		}
		CHECK(g.phase == CQ_PHASE_GAME_OVER);
		games++;
	}
	printf("  cinquillo: %d partidas, %d rondas, victorias por jugador %d/%d/%d/%d\n", games, rounds,
		wins[0], wins[1], wins[2], wins[3]);
}


static void test_br_rules(void)
{
	int trump = SUIT_OROS;
	/* Mismo palo: manda el orden de la brisca (el tres gana al rey). */
	CHECK(br_beats(card_make(SUIT_COPAS, 3), card_make(SUIT_COPAS, RANK_REY), trump));
	CHECK(!br_beats(card_make(SUIT_COPAS, 7), card_make(SUIT_COPAS, RANK_SOTA), trump));
	CHECK(br_beats(card_make(SUIT_COPAS, 1), card_make(SUIT_COPAS, 3), trump));
	/* Un triunfo cualquiera gana a otro palo; otro palo no triunfo pierde. */
	CHECK(br_beats(card_make(SUIT_OROS, 2), card_make(SUIT_COPAS, 1), trump));
	CHECK(!br_beats(card_make(SUIT_BASTOS, 1), card_make(SUIT_COPAS, 2), trump));

	int total = 0;
	for (int i = 0; i < DECK_SIZE; i++)
		total += br_card_points(card_from_index(i));
	CHECK(total == BR_TOTAL_POINTS);

	Brisca g;
	br_init(&g, 5);
	CHECK(g.hand_count[0] == 3 && g.hand_count[1] == 3);
	CHECK(g.deck.count == DECK_SIZE - 6);
	CHECK(card_equal(g.deck.cards[0], g.trump_card));
	CHECK(g.current == 0);

	/* Cambio de la muestra: con muestra rey de copas se cambia el 7 de copas. */
	g.trump_card = card_make(SUIT_COPAS, RANK_REY);
	g.trump = SUIT_COPAS;
	g.deck.cards[0] = g.trump_card;
	g.hands[0][1] = card_make(SUIT_COPAS, 7);
	CHECK(br_swap_index(&g, 0) == 1);
	CHECK(br_swap_index(&g, 1) == -1);
	CHECK(br_swap(&g, 0));
	CHECK(card_equal(g.hands[0][1], card_make(SUIT_COPAS, RANK_REY)));
	CHECK(card_equal(g.trump_card, card_make(SUIT_COPAS, 7)));
	CHECK(card_equal(g.deck.cards[0], g.trump_card));
	/* Ahora la muestra es un 7: se cambiaría con el 2. */
	g.hands[0][0] = card_make(SUIT_COPAS, 2);
	CHECK(br_swap_index(&g, 0) == 0);
}

static void test_br_simulation(void)
{
	int wins[2] = { 0 }, ties = 0;
	for (uint32_t seed = 1; seed <= 500; seed++) {
		Brisca g;
		br_init(&g, seed);
		Rng r;
		rng_seed(&r, seed + 17);
		int guard = 0;
		while (g.phase != BR_PHASE_GAME_OVER && guard++ < 100) {
			if (g.phase == BR_PHASE_TRICK_DONE) {
				CHECK(br_collect(&g));
				continue;
			}
			/* El jugador 0 juega al azar y la CPU con su IA. */
			if (g.current == 0) {
				if (br_swap_index(&g, 0) >= 0)
					CHECK(br_swap(&g, 0));
				CHECK(br_play(&g, rng_range(&r, g.hand_count[0])));
			} else {
				CHECK(br_play(&g, br_ai_choose(&g, 1)));
			}
		}
		CHECK(g.phase == BR_PHASE_GAME_OVER);
		CHECK(g.tricks == DECK_SIZE / 2);
		CHECK(g.points[0] + g.points[1] == BR_TOTAL_POINTS);
		CHECK(g.won_cards[0] + g.won_cards[1] == DECK_SIZE);
		CHECK(g.deck.count == 0);
		int w = br_game_winner(&g);
		if (w < 0)
			ties++;
		else
			wins[w]++;
	}
	printf("  brisca: azar %d - IA %d (empates %d)\n", wins[0], wins[1], ties);
	CHECK(wins[1] > wins[0]); /* la IA debe jugar mejor que el azar */
}

static void test_es_rules(void)
{
	Escoba g;
	es_init(&g, 3);
	CHECK(es_card_value(card_make(SUIT_OROS, RANK_REY)) == 10);
	CHECK(es_card_value(card_make(SUIT_OROS, RANK_SOTA)) == 8);

	/* Mesa preparada a mano: 5, 3, 7 y sota (8). */
	g.phase = ES_PHASE_PLAYING;
	g.current = 0;
	g.table_count = 4;
	g.table[0] = card_make(SUIT_COPAS, 5);
	g.table[1] = card_make(SUIT_BASTOS, 3);
	g.table[2] = card_make(SUIT_OROS, 7);
	g.table[3] = card_make(SUIT_ESPADAS, RANK_SOTA);
	g.hand_count[0] = 2;
	g.hands[0][0] = card_make(SUIT_OROS, 7); /* 7 + 5 + 3, o 7 + sota */
	g.hands[0][1] = card_make(SUIT_COPAS, 1); /* no suma 15 con nada */
	g.hand_count[1] = 1;
	g.hands[1][0] = card_make(SUIT_BASTOS, 2);
	g.won_count[0] = g.won_count[1] = 0;
	g.deck.count = 0;

	uint32_t masks[ES_MAX_OPTIONS];
	int n = es_find_captures(&g, g.hands[0][0], masks, ES_MAX_OPTIONS);
	CHECK(n == 2);
	CHECK(es_find_captures(&g, g.hands[0][1], masks, ES_MAX_OPTIONS) == 0);
	/* Capturar es obligatorio y la combinación ha de sumar 15. */
	CHECK(!es_play(&g, 0, 0));
	CHECK(!es_play(&g, 0, 0x1));
	CHECK(!es_play(&g, 1, 0x1));
	CHECK(es_play(&g, 0, 0x3)); /* 7 + 5 + 3 */
	CHECK(g.table_count == 2);
	CHECK(g.won_count[0] == 3);
	CHECK(!g.last_escoba);
	CHECK(g.last_captured_count == 2);
}

static void test_es_escoba(void)
{
	Escoba g;
	es_init(&g, 3);
	g.phase = ES_PHASE_PLAYING;
	g.current = 0;
	g.table_count = 2;
	g.table[0] = card_make(SUIT_COPAS, 4);
	g.table[1] = card_make(SUIT_BASTOS, 4);
	g.hand_count[0] = 1;
	g.hands[0][0] = card_make(SUIT_OROS, 7);
	g.hand_count[1] = 1;
	g.hands[1][0] = card_make(SUIT_ESPADAS, 2);
	g.deck.count = 0;
	int escobas = g.escobas[0];
	CHECK(es_play(&g, 0, 0x3));
	CHECK(g.last_escoba);
	CHECK(g.escobas[0] == escobas + 1);
	/* El rival deja su carta y termina la ronda: la mesa es del último que recogió. */
	CHECK(es_play(&g, 0, 0));
	CHECK(g.phase != ES_PHASE_PLAYING);
	CHECK(g.round_score[0].siete_oros);
	CHECK(g.round_score[0].pt_siete_oros == 1);
}

static void test_es_simulation(void)
{
	int wins[2] = { 0 }, escobas = 0, rounds = 0;
	for (uint32_t seed = 1; seed <= 300; seed++) {
		Escoba g;
		es_init(&g, seed);
		Rng r;
		rng_seed(&r, seed * 7);
		int guard = 0;
		while (g.phase != ES_PHASE_GAME_OVER && guard++ < 5000) {
			if (g.phase == ES_PHASE_ROUND_OVER) {
				rounds++;
				CHECK(es_next_round(&g));
				continue;
			}
			int p = g.current;
			uint32_t mask = 0;
			int idx;
			if (p == 0) {
				/* Jugador "al azar" que respeta la obligación de capturar. */
				idx = rng_range(&r, g.hand_count[0]);
				uint32_t masks[ES_MAX_OPTIONS];
				int n = es_find_captures(&g, g.hands[0][idx], masks, ES_MAX_OPTIONS);
				if (n)
					mask = masks[rng_range(&r, n)];
			} else {
				idx = es_ai_choose(&g, p, &mask);
			}
			CHECK(es_play(&g, idx, mask));
			int cards = g.deck.count + g.table_count + g.won_count[0] + g.won_count[1] + g.hand_count[0] +
				g.hand_count[1];
			CHECK(cards == DECK_SIZE);
			if (g.phase != ES_PHASE_PLAYING) {
				CHECK(g.won_count[0] + g.won_count[1] == DECK_SIZE);
				CHECK(g.round_score[0].oros + g.round_score[1].oros == RANKS_PER_SUIT);
				CHECK(g.round_score[0].sietes + g.round_score[1].sietes == NUM_SUITS);
				CHECK(g.round_score[0].pt_siete_oros + g.round_score[1].pt_siete_oros == 1);
				escobas += g.escobas[0] + g.escobas[1];
			}
		}
		CHECK(g.phase == ES_PHASE_GAME_OVER);
		CHECK(es_winner(&g) >= 0);
		wins[es_winner(&g)]++;
	}
	printf("  escoba: %d rondas, %d escobas, azar %d - IA %d\n", rounds, escobas, wins[0], wins[1]);
	CHECK(wins[1] > wins[0]);
}

int main(void)
{
	printf("Ejecutando tests...\n");
	test_deck();
	test_sym_values();
	test_sym_bets();
	test_sym_resolve();
	test_sym_bust();
	test_sym_simulation();
	test_cq_rules();
	test_cq_simulation();
	test_br_rules();
	test_br_simulation();
	test_es_rules();
	test_es_escoba();
	test_es_simulation();
	printf("%d comprobaciones, %d fallos\n", checks, failures);
	return failures ? 1 : 0;
}
