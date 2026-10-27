/* Tests de la lógica de los juegos. Se compilan con el gcc del sistema:
 *     make -C tests
 */
#include <stdio.h>
#include <string.h>

#include "core/cards.h"
#include "games/brisca.h"
#include "games/chinchon.h"
#include "games/cinquillo.h"
#include "games/escoba.h"
#include "games/mus.h"
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

static int popcount_ok(unsigned m)
{
	int n = 0;
	for (; m; m &= m - 1)
		n++;
	return n;
}

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
	card_describe_article(card_make(SUIT_OROS, RANK_SOTA), buf, sizeof buf);
	CHECK(strcmp(buf, "la sota de oros") == 0);
	card_describe_article(card_make(SUIT_OROS, 1), buf, sizeof buf);
	CHECK(strcmp(buf, "el as de oros") == 0);

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


#define CARD(s, r) card_make(SUIT_##s, r)

static void test_ch_analysis(void)
{
	/* Trío de cincos + escalera 6-7-sota de copas + rey suelto */
	Card h[7] = { CARD(OROS, 5), CARD(COPAS, 5), CARD(BASTOS, 5), CARD(COPAS, 6), CARD(COPAS, 7),
		CARD(COPAS, RANK_SOTA), CARD(ESPADAS, RANK_REY) };
	ChAnalysis a;
	ch_analyze(h, 7, &a);
	CHECK(a.meld_count == 2);
	CHECK(a.loose_count == 1);
	CHECK(a.loose_value == 10);

	/* Con cuatro cincos y el 6-7 de copas, lo mejor es partirlos: trío de
	 * cincos y escalera 5-6-7 de copas. */
	Card h2[7] = { CARD(OROS, 5), CARD(COPAS, 5), CARD(BASTOS, 5), CARD(ESPADAS, 5), CARD(COPAS, 6),
		CARD(COPAS, 7), CARD(OROS, 1) };
	ch_analyze(h2, 7, &a);
	CHECK(a.loose_count == 1); /* trío de cincos + 5-6-7 de copas, suelto el as */
	CHECK(a.loose_value == 1);

	/* Nada combinado */
	Card h3[7] = { CARD(OROS, 1), CARD(COPAS, 3), CARD(BASTOS, 5), CARD(ESPADAS, 7), CARD(OROS, RANK_SOTA),
		CARD(COPAS, RANK_REY), CARD(BASTOS, 2) };
	ch_analyze(h3, 7, &a);
	CHECK(a.meld_count == 0);
	CHECK(a.loose_value == 1 + 3 + 5 + 7 + 10 + 10 + 2);

	/* Ordenar pone las combinaciones delante */
	ch_sort_hand(h, 7);
	CHECK(h[6].rank == RANK_REY);
}

static void setup_hand(Chinchon* g, int p, const Card* cards, int n)
{
	memcpy(g->hands[p], cards, sizeof(Card) * n);
	g->hand_count[p] = n;
	ch_sort_hand(g->hands[p], n);
}

static void test_ch_close(void)
{
	Chinchon g;
	ch_init(&g, 11);
	CHECK(g.hand_count[0] == 7 && g.hand_count[1] == 7);
	CHECK(g.discard_count == 1);
	CHECK(g.deck.count == DECK_SIZE - 15);
	CHECK(g.current == 0 && g.phase == CH_PHASE_DRAW);
	CHECK(!ch_discard(&g, 0)); /* primero hay que robar */
	CHECK(ch_draw_discard(&g));
	CHECK(g.hand_count[0] == 8 && g.phase == CH_PHASE_DISCARD);

	/* Mano para cerrar: trío de reyes, escalera 1-2-3 de oros, as de copas suelto y un 7 de bastos para descartar */
	Card h[8] = { CARD(OROS, RANK_REY), CARD(COPAS, RANK_REY), CARD(BASTOS, RANK_REY), CARD(OROS, 1), CARD(OROS, 2),
		CARD(OROS, 3), CARD(COPAS, 1), CARD(BASTOS, 7) };
	setup_hand(&g, 0, h, 8);
	/* Rival: sin combinaciones salvo la escalera 2-3-4 de espadas. */
	Card r[7] = { CARD(OROS, 4), CARD(OROS, 5), CARD(ESPADAS, RANK_REY), CARD(ESPADAS, 2), CARD(ESPADAS, 3),
		CARD(ESPADAS, 4), CARD(COPAS, 7) };
	setup_hand(&g, 1, r, 7);
	int seven = -1;
	for (int i = 0; i < 8; i++)
		if (card_equal(g.hands[0][i], CARD(BASTOS, 7)))
			seven = i;
	CHECK(seven >= 0);
	CHECK(ch_can_close(&g, seven));
	for (int i = 0; i < 8; i++)
		if (i != seven && g.hands[0][i].rank == RANK_REY)
			CHECK(!ch_can_close(&g, i));
	CHECK(ch_close(&g, seven));
	CHECK(g.closer == 0);
	CHECK(!g.chinchon && !g.menos_diez);
	CHECK(g.round_points[0] == 1);
	/* El rival liga el 4 y el 5 de oros a la escalera y el rey de espadas al
	 * trío: solo le queda suelto el 7 de copas. */
	CHECK(popcount_ok(g.laid_off) == 3);
	CHECK(g.round_points[1] == 7);
	CHECK(g.score[0] == 1 && g.score[1] == 7);
	CHECK(g.phase == CH_PHASE_ROUND_OVER);
	CHECK(ch_next_round(&g));
	CHECK(g.round == 2 && g.current == 1);
}

static void test_ch_chinchon(void)
{
	Chinchon g;
	ch_init(&g, 12);
	ch_draw_deck(&g);
	Card h[8] = { CARD(COPAS, 4), CARD(COPAS, 5), CARD(COPAS, 6), CARD(COPAS, 7), CARD(COPAS, RANK_SOTA),
		CARD(COPAS, RANK_CABALLO), CARD(COPAS, RANK_REY), CARD(OROS, 2) };
	setup_hand(&g, 0, h, 8);
	int two = -1;
	for (int i = 0; i < 8; i++)
		if (g.hands[0][i].suit == SUIT_OROS)
			two = i;
	CHECK(ch_close(&g, two));
	CHECK(g.chinchon);
	CHECK(g.phase == CH_PHASE_GAME_OVER);
	CHECK(ch_winner(&g) == 0);
}

static void test_ch_simulation(void)
{
	int wins[2] = { 0 }, rounds = 0, closes = 0, chinchones = 0, menos_diez = 0;
	for (uint32_t seed = 1; seed <= 200; seed++) {
		Chinchon g;
		ch_init(&g, seed);
		Rng r;
		rng_seed(&r, seed * 13);
		int guard = 0;
		while (g.phase != CH_PHASE_GAME_OVER && guard++ < 20000) {
			if (g.phase == CH_PHASE_ROUND_OVER) {
				CHECK(ch_next_round(&g));
				continue;
			}
			int p = g.current;
			if (g.phase == CH_PHASE_DRAW) {
				bool take = p == 1 ? ch_ai_take_discard(&g, 1) : rng_range(&r, 2) == 0;
				CHECK(take ? ch_draw_discard(&g) : ch_draw_deck(&g));
			} else {
				bool close;
				int idx;
				if (p == 1) {
					idx = ch_ai_discard(&g, 1, &close);
				} else {
					/* "Jugador" sencillo: cierra si puede, si no descarta la suelta más alta */
					idx = ch_ai_discard(&g, 0, &close);
					if (!close)
						idx = rng_range(&r, g.hand_count[0]);
				}
				if (close) {
					CHECK(ch_close(&g, idx));
					closes++;
					rounds++;
					chinchones += g.chinchon;
					menos_diez += g.menos_diez;
				} else {
					CHECK(ch_discard(&g, idx));
				}
			}
			int cards = g.deck.count + g.discard_count + g.hand_count[0] + g.hand_count[1];
			CHECK(cards == DECK_SIZE);
		}
		CHECK(g.phase == CH_PHASE_GAME_OVER);
		int w = ch_winner(&g);
		CHECK(w >= 0);
		if (w >= 0)
			wins[w]++;
	}
	printf("  chinchón: %d rondas, %d menos diez, %d chinchones, azar %d - IA %d\n", rounds, menos_diez,
		chinchones, wins[0], wins[1]);
	CHECK(wins[1] > wins[0]);
}


static void set_hand(Mus* g, int p, Card a, Card b, Card c, Card d)
{
	g->hands[p][0] = a;
	g->hands[p][1] = b;
	g->hands[p][2] = c;
	g->hands[p][3] = d;
}

static void test_mus_values(void)
{
	Card h[4] = { CARD(OROS, 3), CARD(COPAS, RANK_REY), CARD(BASTOS, 2), CARD(ESPADAS, 1) };
	/* A ocho reyes: tres = rey y dos = as -> duples de reyes y ases */
	CHECK(mus_pares_kind(h) == 3);
	CHECK(mus_juego_sum(h) == 22);
	char buf[48];
	mus_describe(h, MUS_LANCE_PARES, false, buf, sizeof buf);
	CHECK(strcmp(buf, "Duples reyes y ases") == 0);

	Card j31[4] = { CARD(OROS, RANK_REY), CARD(COPAS, RANK_SOTA), CARD(BASTOS, RANK_CABALLO), CARD(ESPADAS, 1) };
	Card j32[4] = { CARD(OROS, RANK_REY), CARD(COPAS, RANK_SOTA), CARD(BASTOS, 5), CARD(ESPADAS, 7) };
	Card j40[4] = { CARD(OROS, RANK_REY), CARD(COPAS, RANK_SOTA), CARD(BASTOS, RANK_CABALLO), CARD(ESPADAS, 3) };
	Card j33[4] = { CARD(OROS, RANK_REY), CARD(COPAS, RANK_SOTA), CARD(BASTOS, 6), CARD(ESPADAS, 7) };
	CHECK(mus_juego_sum(j31) == 31 && mus_juego_value(j31) == 3);
	CHECK(mus_juego_sum(j40) == 40 && mus_juego_value(j40) == 2);
	CHECK(mus_compare(MUS_LANCE_JUEGO, false, j31, j32) > 0);
	CHECK(mus_compare(MUS_LANCE_JUEGO, false, j32, j40) > 0);
	CHECK(mus_compare(MUS_LANCE_JUEGO, false, j40, j33) > 0);

	/* Grande: R R 7 4 gana a R C C C; chica: A A 4 5 gana a A 4 4 7 */
	Card g1[4] = { CARD(OROS, 3), CARD(COPAS, RANK_REY), CARD(BASTOS, 7), CARD(ESPADAS, 4) };
	Card g2[4] = { CARD(OROS, RANK_REY), CARD(COPAS, RANK_CABALLO), CARD(BASTOS, RANK_CABALLO),
		CARD(ESPADAS, RANK_CABALLO) };
	CHECK(mus_compare(MUS_LANCE_GRANDE, false, g1, g2) > 0);
	Card c1[4] = { CARD(OROS, 1), CARD(COPAS, 2), CARD(BASTOS, 4), CARD(ESPADAS, 5) };
	Card c2[4] = { CARD(OROS, 1), CARD(COPAS, 4), CARD(BASTOS, 4), CARD(ESPADAS, 7) };
	CHECK(mus_compare(MUS_LANCE_CHICA, false, c1, c2) > 0);
	/* Pares: medias ganan a pareja, duples a medias */
	CHECK(mus_compare(MUS_LANCE_PARES, false, g2, g1) > 0);
	CHECK(mus_compare(MUS_LANCE_PARES, false, h, g2) > 0);
}

static void test_mus_lances(void)
{
	Mus g;
	mus_init(&g, 21);
	CHECK(g.mano == 0 && g.current == 0 && g.phase == MUS_PHASE_MUS);
	/* Manos preparadas: nosotros (0 y 2) contra ellos (1 y 3) */
	set_hand(&g, 0, CARD(OROS, RANK_REY), CARD(COPAS, RANK_REY), CARD(BASTOS, RANK_SOTA), CARD(ESPADAS, 1)); /* pareja reyes, 31 */
	set_hand(&g, 1, CARD(OROS, 1), CARD(COPAS, 2), CARD(BASTOS, 4), CARD(ESPADAS, 5));                       /* chica, pareja ases */
	set_hand(&g, 2, CARD(OROS, 6), CARD(COPAS, 7), CARD(BASTOS, 5), CARD(ESPADAS, 4));                       /* nada */
	set_hand(&g, 3, CARD(OROS, 7), CARD(COPAS, 7), CARD(BASTOS, 7), CARD(ESPADAS, 6));                       /* medias de sietes, 27 */

	/* El 0 corta: empieza la grande */
	CHECK(mus_say_mus(&g, false));
	CHECK(g.phase == MUS_PHASE_BET && g.lance == MUS_LANCE_GRANDE && g.current == 0);
	/* Grande: 0 envida 2, 1 no quiere -> deje de 1 para nosotros */
	CHECK(mus_bet(&g, MUS_ACT_ENVIDO, 2));
	CHECK(g.current == 1 && mus_responding(&g));
	CHECK(!mus_bet(&g, MUS_ACT_PASO, 0));
	CHECK(mus_bet(&g, MUS_ACT_NO_QUIERO, 0));
	CHECK(g.score[0] == 1);
	/* Chica: todos pasan */
	CHECK(g.lance == MUS_LANCE_CHICA && g.current == 0);
	for (int i = 0; i < 4; i++)
		CHECK(mus_bet(&g, MUS_ACT_PASO, 0));
	/* Pares: juegan 0 (pareja de reyes), 1 (pareja de ases) y 3 (medias) */
	CHECK(g.lance == MUS_LANCE_PARES);
	CHECK(g.eligible[0] && g.eligible[1] && !g.eligible[2] && g.eligible[3]);
	CHECK(g.current == 0);
	CHECK(mus_bet(&g, MUS_ACT_PASO, 0));
	CHECK(g.current == 1);
	CHECK(mus_bet(&g, MUS_ACT_ENVIDO, 2));     /* ellos envidan 2 */
	CHECK(g.current == 0);                     /* responde el único de nosotros con pares */
	CHECK(mus_bet(&g, MUS_ACT_ENVIDO, 3));     /* reenvido: 5 */
	CHECK(g.bet_pending == 5 && g.bet_accepted == 2);
	CHECK(g.current == 1);
	CHECK(mus_bet(&g, MUS_ACT_QUIERO, 0));
	/* Juego: solo el 0 tiene juego -> cobra sus tantos sin envites */
	CHECK(g.results[MUS_LANCE_JUEGO].kind == MUS_RES_SINGLE);
	CHECK(g.phase == MUS_PHASE_HAND_OVER);

	/* Recuento: grande deje 1 (ya cobrado); chica en paso para ellos (1);
	 * pares querido 5 para ellos (medias) + tantos 1 (pareja ases) + 2 (medias) = 8;
	 * juego: 31 de nuestro jugador 0 = 3 */
	CHECK(g.results[MUS_LANCE_CHICA].points[1] == 1);
	CHECK(g.results[MUS_LANCE_PARES].points[1] == 8);
	CHECK(g.results[MUS_LANCE_JUEGO].points[0] == 3);
	CHECK(g.score[0] == 4 && g.score[1] == 9);
	CHECK(mus_next_hand(&g));
	CHECK(g.mano == 1);
}

static void test_mus_ordago(void)
{
	Mus g;
	mus_init(&g, 22);
	set_hand(&g, 0, CARD(OROS, RANK_REY), CARD(COPAS, RANK_REY), CARD(BASTOS, 3), CARD(ESPADAS, 3));
	set_hand(&g, 1, CARD(OROS, 1), CARD(COPAS, 2), CARD(BASTOS, 4), CARD(ESPADAS, 5));
	set_hand(&g, 2, CARD(OROS, 6), CARD(COPAS, 7), CARD(BASTOS, 5), CARD(ESPADAS, 4));
	set_hand(&g, 3, CARD(OROS, 7), CARD(COPAS, 7), CARD(BASTOS, 7), CARD(ESPADAS, RANK_CABALLO));
	mus_say_mus(&g, false);
	CHECK(mus_bet(&g, MUS_ACT_ORDAGO, 0));
	CHECK(!mus_bet(&g, MUS_ACT_ENVIDO, 2)); /* a un órdago no se le reenvida */
	CHECK(mus_bet(&g, MUS_ACT_QUIERO, 0));
	CHECK(g.phase == MUS_PHASE_GAME_OVER);
	CHECK(g.winner_team == 0);
}

static void test_mus_discard(void)
{
	Mus g;
	mus_init(&g, 23);
	for (int p = 0; p < 4; p++)
		CHECK(mus_say_mus(&g, true));
	CHECK(g.phase == MUS_PHASE_DISCARD);
	CHECK(!mus_discard(&g, 0));
	Card before = g.hands[0][1];
	CHECK(mus_discard(&g, 0x2));
	CHECK(!card_equal(before, g.hands[0][1]));
	for (int p = 1; p < 4; p++)
		CHECK(mus_discard(&g, 0xF));
	CHECK(g.phase == MUS_PHASE_MUS && g.mus_rounds == 1);
	CHECK(g.deck.count + g.discard_count + 16 == DECK_SIZE);
}

static void test_mus_simulation(void)
{
	int wins[2] = { 0 }, hands = 0, ordagos = 0;
	for (uint32_t seed = 1; seed <= 300; seed++) {
		Mus g;
		mus_init(&g, seed);
		Rng r;
		rng_seed(&r, seed * 5);
		int guard = 0;
		while (g.phase != MUS_PHASE_GAME_OVER && guard++ < 20000) {
			int p = g.current;
			bool random_player = mus_team(p) == 0; /* nosotros al azar, ellos con IA */
			switch (g.phase) {
			case MUS_PHASE_MUS:
				CHECK(mus_say_mus(&g, random_player ? rng_range(&r, 3) > 0 : mus_ai_wants_mus(&g, p)));
				break;
			case MUS_PHASE_DISCARD:
				CHECK(mus_discard(&g, random_player ? 1u + rng_range(&r, 15) : mus_ai_discard(&g, p)));
				break;
			case MUS_PHASE_BET: {
				int amount = 2;
				MusAction a;
				if (random_player) {
					if (mus_responding(&g))
						a = rng_range(&r, 2) ? MUS_ACT_QUIERO : MUS_ACT_NO_QUIERO;
					else
						a = rng_range(&r, 3) ? MUS_ACT_PASO : MUS_ACT_ENVIDO;
				} else {
					a = mus_ai_bet(&g, p, &amount);
				}
				CHECK(mus_bet(&g, a, amount));
				break;
			}
			case MUS_PHASE_HAND_OVER:
				hands++;
				for (int l = 0; l < MUS_NUM_LANCES; l++)
					CHECK(g.results[l].kind != MUS_RES_PENDING);
				CHECK(mus_next_hand(&g));
				break;
			default:
				break;
			}
			CHECK(g.deck.count + g.discard_count + 16 == DECK_SIZE);
		}
		CHECK(g.phase == MUS_PHASE_GAME_OVER);
		CHECK(g.winner_team == 0 || g.winner_team == 1);
		if (g.winner_team >= 0)
			wins[g.winner_team]++;
		ordagos += g.ordago_played;
	}
	printf("  mus: %d manos, %d órdagos, azar %d - IA %d\n", hands, ordagos, wins[0], wins[1]);
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
	test_ch_analysis();
	test_ch_close();
	test_ch_chinchon();
	test_ch_simulation();
	test_mus_values();
	test_mus_lances();
	test_mus_ordago();
	test_mus_discard();
	test_mus_simulation();
	printf("%d comprobaciones, %d fallos\n", checks, failures);
	return failures ? 1 : 0;
}
