/* Tests de la lógica de los juegos. Se compilan con el gcc del sistema:
 *     make -C tests
 */
#include <stdio.h>
#include <string.h>

#include "core/cardback.h"
#include "core/cards.h"
#include "games/brisca.h"
#include "games/chinchon.h"
#include "games/cinquillo.h"
#include "games/escoba.h"
#include "games/guinote.h"
#include "games/mus.h"
#include "games/piramide.h"
#include "games/rummikub.h"
#include "core/profile.h"
#include "games/sieteymedia.h"
#include "games/uno.h"
#include "games/domino.h"
#include "games/damas.h"
#include "games/conecta4.h"
#include "games/pocha.h"
#include "games/corazones.h"
#include "games/blackjack.h"
#include "games/poker.h"
#include "games/treintayuna.h"
#include "games/mona.h"
#include "games/mentiroso.h"
#include "games/culo.h"
#include "games/cucu.h"
#include "games/solitario.h"

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


static void test_mus_senas(void)
{
	Card duples[4] = { CARD(OROS, RANK_REY), CARD(COPAS, 3), CARD(BASTOS, 1), CARD(ESPADAS, 2) };
	Card medias[4] = { CARD(OROS, 7), CARD(COPAS, 7), CARD(BASTOS, 7), CARD(ESPADAS, 1) };
	Card t31[4] = { CARD(OROS, RANK_REY), CARD(COPAS, RANK_SOTA), CARD(BASTOS, RANK_CABALLO), CARD(ESPADAS, 1) };
	Card reyes[4] = { CARD(OROS, RANK_REY), CARD(COPAS, 3), CARD(BASTOS, 5), CARD(ESPADAS, 4) };
	Card ases[4] = { CARD(OROS, 1), CARD(COPAS, 2), CARD(BASTOS, 5), CARD(ESPADAS, 4) };
	Card ciego[4] = { CARD(OROS, 4), CARD(COPAS, 5), CARD(BASTOS, 6), CARD(ESPADAS, 7) };
	CHECK(mus_best_sena(duples) == MUS_SENA_DUPLES);
	CHECK(mus_best_sena(medias) == MUS_SENA_MEDIAS);
	CHECK(mus_best_sena(t31) == MUS_SENA_31);
	CHECK(mus_best_sena(reyes) == MUS_SENA_REYES);
	CHECK(mus_best_sena(ases) == MUS_SENA_ASES);
	CHECK(mus_best_sena(ciego) == MUS_SENA_CIEGO);

	Mus g;
	mus_init(&g, 31);
	CHECK(mus_make_sena(&g, 0, MUS_SENA_31));
	CHECK(g.sena[0] == MUS_SENA_31);
	/* Las señas se olvidan al cambiar las cartas */
	for (int p = 0; p < 4; p++)
		mus_say_mus(&g, true);
	for (int p = 0; p < 4; p++)
		mus_discard(&g, 0x1);
	CHECK(g.sena[0] == MUS_SENA_NONE);
}

/* Dos parejas de IA, una con señas y otra sin ellas. */
static void test_mus_senas_simulation(void)
{
	int wins[2] = { 0 };
	for (uint32_t seed = 1; seed <= 300; seed++) {
		Mus g;
		mus_init(&g, seed + 1000);
		int guard = 0;
		int seen_phase = -1, seen_rounds = -1;
		while (g.phase != MUS_PHASE_GAME_OVER && guard++ < 20000) {
			if (g.phase == MUS_PHASE_MUS && (seen_phase != MUS_PHASE_MUS || seen_rounds != g.mus_rounds)) {
				/* Al empezar cada vuelta de mus, la pareja 0 se hace sus señas */
				mus_make_sena(&g, 0, mus_best_sena(g.hands[0]));
				mus_make_sena(&g, 2, mus_best_sena(g.hands[2]));
				seen_rounds = g.mus_rounds;
			}
			seen_phase = g.phase;
			int p = g.current, amount = 2;
			switch (g.phase) {
			case MUS_PHASE_MUS:
				mus_say_mus(&g, mus_ai_wants_mus(&g, p));
				break;
			case MUS_PHASE_DISCARD:
				mus_discard(&g, mus_ai_discard(&g, p));
				break;
			case MUS_PHASE_BET: {
				MusAction a = mus_ai_bet(&g, p, &amount);
				CHECK(mus_bet(&g, a, amount));
				break;
			}
			case MUS_PHASE_HAND_OVER:
				mus_next_hand(&g);
				seen_phase = -1;
				break;
			default:
				break;
			}
		}
		CHECK(g.phase == MUS_PHASE_GAME_OVER);
		if (g.winner_team >= 0)
			wins[g.winner_team]++;
	}
	printf("  mus: con señas %d - sin señas %d\n", wins[0], wins[1]);
	CHECK(wins[0] > wins[1]);
}

static void test_profile(void)
{
	PlayerProfile p, q;
	profile_init(&p);
	CHECK(!p.created);
	snprintf(p.avatar.name, sizeof p.avatar.name, "Davicilio");
	p.avatar.hair_style = HAIR_CURLY;
	p.created = 1;
	profile_record(&p, GAME_MUS, true);
	profile_record(&p, GAME_MUS, false);
	CHECK(p.played[GAME_MUS] == 2 && p.won[GAME_MUS] == 1);
	const char* path = "test_perfil.dat";
	CHECK(profile_save(&p, path));
	CHECK(profile_load(&q, path));
	CHECK(strcmp(q.avatar.name, "Davicilio") == 0);
	CHECK(q.avatar.hair_style == HAIR_CURLY && q.created == 1 && q.won[GAME_MUS] == 1);
	/* Un fichero dañado no se carga */
	FILE* f = fopen(path, "wb");
	fputs("basura", f);
	fclose(f);
	CHECK(!profile_load(&q, path));
	remove(path);

	/* Los Miis de la CPU tienen nombre y valores válidos */
	for (int i = 0; i < CPU_COUNT; i++) {
		Avatar a = *cpu_avatar((CpuId)i);
		Avatar b = a;
		avatar_sanitize(&b);
		CHECK(memcmp(&a, &b, sizeof a) == 0);
		CHECK(a.name[0] != '\0');
	}
	CHECK(strcmp(cpu_avatar(CPU_LOLA)->name, "Lola") == 0);
}


static void test_cardback(void)
{
	CardBack b;
	cardback_default(&b);
	CHECK(b.value[CB_BACKGROUND] == 0 && b.value[CB_PATTERN] == 1 && b.text[0] == '\0');
	CHECK(strcmp(cardback_value_name(CB_PATTERN, b.value[CB_PATTERN]), "Rombos") == 0);
	CHECK(strcmp(cardback_value_name(CB_PATTERN_COLOR, 0), "Automático") == 0);
	CHECK(strcmp(cardback_value_name(CB_EMBLEM, 99), "?") == 0);

	/* Cada opción da la vuelta en los dos sentidos */
	for (int o = 0; o < CB_OPTIONS; o++) {
		int n = cardback_option_count((CardBackOption)o);
		CHECK(n >= 2);
		uint8_t start = b.value[o];
		for (int i = 0; i < n; i++) {
			CHECK(strcmp(cardback_value_name((CardBackOption)o, b.value[o]), "?") != 0);
			cardback_step(&b, (CardBackOption)o, 1);
		}
		CHECK(b.value[o] == start);
		cardback_step(&b, (CardBackOption)o, -1);
		CHECK(b.value[o] == (start + n - 1) % n);
		cardback_step(&b, (CardBackOption)o, 1);
	}

	/* El color automático del dibujo se distingue del fondo */
	CHECK(cardback_pattern_rgb(&b) != cardback_background_rgb(&b));
	b.value[CB_BACKGROUND] = CB_COLORS - 1; /* blanco: el dibujo se oscurece */
	CHECK((cardback_pattern_rgb(&b) & 0xFF) < (cardback_background_rgb(&b) & 0xFF));
	b.value[CB_PATTERN_COLOR] = 1;
	CHECK(cardback_pattern_rgb(&b) == cardback_color_rgb(0));

	/* Texto: hasta 16 caracteres sin partir los de varios bytes */
	cardback_set_text(&b, "Caja Madrid");
	CHECK(strcmp(b.text, "Caja Madrid") == 0);
	cardback_set_text(&b, "ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ");
	CHECK(strlen(b.text) == 32);
	cardback_set_text(&b, "abc\xC3");
	CHECK(strcmp(b.text, "abc") == 0);
	cardback_set_text(&b, NULL);
	CHECK(b.text[0] == '\0');

	/* Valores fuera de rango */
	b.value[CB_EMBLEM] = 200;
	memset(b.text, 'x', sizeof b.text);
	cardback_sanitize(&b);
	CHECK(b.value[CB_EMBLEM] < CB_EMBLEMS);
	CHECK(strlen(b.text) == CARDBACK_TEXT_CHARS);

	/* Guardar y cargar */
	cardback_set_text(&b, "Peña El Mus");
	const char* path = "/tmp/test_dorso.dat";
	CHECK(cardback_save(&b, path));
	CardBack c;
	CHECK(cardback_load(&c, path));
	CHECK(memcmp(&b, &c, sizeof b) == 0);
	FILE* f = fopen(path, "wb");
	fputs("basura", f);
	fclose(f);
	CHECK(!cardback_load(&c, path));
	remove(path);
}

static void test_mii_parts(void)
{
	MiiParts m;
	mii_parts_default(&m, false);
	MiiParts other = m;
	other.hair_type = 77;
	CHECK(mii_parts_hash(&m) != mii_parts_hash(&other));
	other.hair_type = m.hair_type;
	CHECK(mii_parts_hash(&m) == mii_parts_hash(&other));

	/* Todos los Miis de la CPU son Miis de verdad y distintos entre sí */
	for (int i = 0; i < CPU_COUNT; i++) {
		const Avatar* a = cpu_avatar((CpuId)i);
		CHECK(a->official);
		for (int j = 0; j < i; j++)
			CHECK(mii_parts_hash(&a->mii) != mii_parts_hash(&cpu_avatar((CpuId)j)->mii));
	}
	CHECK(cpu_avatar(CPU_LOLA)->female && cpu_avatar(CPU_LOLA)->mii.gender == 1);
}

static void test_profile_v2(void)
{
	/* Un perfil de la versión 2 (6 juegos) conserva su Mii y sus estadísticas */
	struct {
		uint32_t magic;
		uint16_t version;
		uint8_t created, from_console, online_miis, reserved[3];
		Avatar avatar;
		uint32_t played[PROFILE_LEGACY_GAMES], won[PROFILE_LEGACY_GAMES];
	} old;
	memset(&old, 0, sizeof old);
	old.magic = PROFILE_MAGIC;
	old.version = 2;
	old.created = 1;
	old.from_console = 1;
	avatar_from_mii(&old.avatar, &cpu_avatar(CPU_LOLA)->mii, "Antigua");
	old.played[GAME_CHINCHON] = 7;
	old.won[GAME_CHINCHON] = 2;
	const char* path = "test_perfil_v2.dat";
	FILE* f = fopen(path, "wb");
	fwrite(&old, 1, sizeof old, f);
	fclose(f);
	PlayerProfile p;
	CHECK(profile_load(&p, path));
	CHECK(p.version == PROFILE_VERSION && p.created == 1 && p.from_console == 1);
	CHECK(strcmp(p.avatar.name, "Antigua") == 0 && p.avatar.official && p.avatar.female);
	CHECK(p.played[GAME_CHINCHON] == 7 && p.won[GAME_CHINCHON] == 2);
	CHECK(p.played[GAME_GUINOTE] == 0 && p.played[GAME_PIRAMIDE] == 0);
	remove(path);
}

static void test_profile_v1(void)
{
	/* Un perfil de la versión 1 se convierte sin perder las estadísticas */
	struct {
		uint32_t magic;
		uint16_t version;
		uint8_t created, from_console;
		char name[AVATAR_NAME_MAX];
		uint8_t look[8];
		uint32_t played[PROFILE_LEGACY_GAMES], won[PROFILE_LEGACY_GAMES];
	} old;
	memset(&old, 0, sizeof old);
	old.magic = PROFILE_MAGIC;
	old.version = 1;
	old.created = 1;
	snprintf(old.name, sizeof old.name, "Viejo");
	old.look[2] = HAIR_CURLY;
	old.played[GAME_BRISCA] = 5;
	old.won[GAME_BRISCA] = 3;
	const char* path = "test_perfil_v1.dat";
	FILE* f = fopen(path, "wb");
	fwrite(&old, 1, sizeof old, f);
	fclose(f);
	PlayerProfile p;
	CHECK(profile_load(&p, path));
	CHECK(p.version == PROFILE_VERSION && p.created == 1 && p.online_miis == 1);
	CHECK(strcmp(p.avatar.name, "Viejo") == 0 && p.avatar.hair_style == HAIR_CURLY);
	CHECK(p.played[GAME_BRISCA] == 5 && p.won[GAME_BRISCA] == 3);
	CHECK(!p.avatar.official);
	remove(path);
}


static void make_pool_mii(Avatar* a, const char* name, int hair)
{
	MiiParts m;
	mii_parts_default(&m, false);
	m.hair_type = (uint8_t)hair;
	avatar_from_mii(a, &m, name);
}

static void test_rivals(void)
{
	/* Sin Miis de la consola: todos los puestos usan los de reserva */
	CHECK(cpu_assign_pool(NULL, 0, NULL) == 0);
	for (int i = 0; i < CPU_COUNT; i++)
		CHECK(cpu_is_fallback((CpuId)i));
	CHECK(strcmp(cpu_name(CPU_LOLA), "Lola") == 0);

	/* Cuatro Miis en la consola, uno de ellos es el jugador */
	Avatar pool[4];
	make_pool_mii(&pool[0], "Ana", 10);
	make_pool_mii(&pool[1], "Yo", 20);
	make_pool_mii(&pool[2], "Luis", 30);
	make_pool_mii(&pool[3], "Marta", 40);
	CHECK(cpu_assign_pool(pool, 4, &pool[1]) == 3);
	/* Los tres puestos de la mesa de cinquillo y mus son Miis distintos */
	CHECK(strcmp(cpu_name(CPU_PACO), "Ana") == 0);
	CHECK(strcmp(cpu_name(CPU_LOLA), "Luis") == 0);
	CHECK(strcmp(cpu_name(CPU_MANOLO), "Marta") == 0);
	for (int i = 0; i < CPU_COUNT; i++) {
		CHECK(!cpu_is_fallback((CpuId)i));
		CHECK(strcmp(cpu_name((CpuId)i), "Yo") != 0);
	}

	/* Un Mii elegido a mano (por ejemplo un invitado) manda sobre el reparto */
	Avatar guest;
	make_pool_mii(&guest, "Invitado A", 50);
	cpu_set_manual(CPU_CARMEN, &guest);
	cpu_assign_pool(pool, 4, &pool[1]);
	CHECK(cpu_is_manual(CPU_CARMEN));
	CHECK(strcmp(cpu_name(CPU_CARMEN), "Invitado A") == 0);
	/* Si se elige a mano un Mii de la consola, no se repite en otro puesto */
	cpu_set_manual(CPU_RAMON, &pool[0]);
	CHECK(cpu_assign_pool(pool, 4, &pool[1]) == 2);
	CHECK(strcmp(cpu_name(CPU_PACO), "Luis") == 0);

	/* Se deja todo como estaba para el resto de tests */
	cpu_set_manual(CPU_CARMEN, NULL);
	cpu_set_manual(CPU_RAMON, NULL);
	cpu_assign_pool(NULL, 0, NULL);
	CHECK(cpu_is_fallback(CPU_CARMEN));
}

/* ---------------------------------------------------------------------------
 * Guiñote
 * ------------------------------------------------------------------------- */

static void test_gn_rules(void)
{
	Guinote g;
	gn_init(&g, GN_GUINOTE, 42);
	g.trump = SUIT_OROS;
	CHECK(gn_card_points(&g, card_make(SUIT_OROS, 1)) == 11);
	CHECK(gn_card_points(&g, card_make(SUIT_OROS, RANK_SOTA)) == 3);
	CHECK(gn_card_points(&g, card_make(SUIT_OROS, RANK_CABALLO)) == 2);
	/* En el guiñote la sota gana al caballo */
	CHECK(gn_beats(&g, card_make(SUIT_COPAS, RANK_SOTA), card_make(SUIT_COPAS, RANK_CABALLO)));
	CHECK(gn_beats(&g, card_make(SUIT_COPAS, RANK_REY), card_make(SUIT_COPAS, RANK_SOTA)));
	CHECK(gn_beats(&g, card_make(SUIT_COPAS, 3), card_make(SUIT_COPAS, RANK_REY)));
	CHECK(gn_beats(&g, card_make(SUIT_OROS, 2), card_make(SUIT_COPAS, 1)));
	CHECK(!gn_beats(&g, card_make(SUIT_ESPADAS, 1), card_make(SUIT_COPAS, 2)));
	int total = 0;
	for (int i = 0; i < DECK_SIZE; i++)
		total += gn_card_points(&g, card_from_index(i));
	CHECK(total == 120);

	gn_init(&g, GN_GUINOTE, 42);
	CHECK(g.deck.count == DECK_SIZE - GN_PLAYERS * gn_hand_size(&g));
	CHECK(card_equal(g.deck.cards[0], g.trump_card) && g.trump == g.trump_card.suit);
	for (int p = 0; p < GN_PLAYERS; p++)
		CHECK(g.hand_count[p] == gn_hand_size(&g));
	/* Con mazo se puede jugar cualquier carta */
	for (int i = 0; i < gn_hand_size(&g); i++)
		CHECK(gn_can_play(&g, g.current, i));
	CHECK(!gn_can_play(&g, (g.current + 1) % GN_PLAYERS, 0));

	/* Arrastre: asistir y montar */
	gn_init(&g, GN_GUINOTE, 7);
	g.deck.count = 0;
	g.trump = SUIT_OROS;
	g.leader = g.current = 0;
	g.played_in_trick = 1;
	g.trick[0] = card_make(SUIT_COPAS, RANK_REY);
	g.trick_played[0] = true;
	g.current = 1;
	g.hand_count[1] = 3;
	g.hands[1][0] = card_make(SUIT_COPAS, 2);
	g.hands[1][1] = card_make(SUIT_COPAS, 1);
	g.hands[1][2] = card_make(SUIT_OROS, 5);
	CHECK(!gn_can_play(&g, 1, 0)); /* puede montar con el as */
	CHECK(gn_can_play(&g, 1, 1));
	CHECK(!gn_can_play(&g, 1, 2)); /* tiene copas: no puede fallar */
	/* Sin copas: hay que fallar */
	g.hands[1][0] = card_make(SUIT_ESPADAS, 2);
	g.hands[1][1] = card_make(SUIT_BASTOS, 1);
	CHECK(!gn_can_play(&g, 1, 0) && !gn_can_play(&g, 1, 1) && gn_can_play(&g, 1, 2));
	/* Si gana el compañero no hay obligación de fallar */
	g.leader = 3;
	g.played_in_trick = 2;
	g.trick[3] = card_make(SUIT_COPAS, 1);
	g.trick[0] = card_make(SUIT_COPAS, 2);
	g.trick_played[3] = true;
	g.current = 1;
	CHECK(gn_trick_leader_now(&g) == 3);
	CHECK(gn_can_play(&g, 1, 0) && gn_can_play(&g, 1, 1));
	/* Montar el fallo: si ya han fallado, hay que fallar por encima */
	g.leader = 0;
	g.played_in_trick = 2;
	g.trick[0] = card_make(SUIT_COPAS, 4);
	g.trick[1] = card_make(SUIT_OROS, 6);
	g.trick_played[1] = true;
	g.current = 2;
	g.hand_count[2] = 3;
	g.hands[2][0] = card_make(SUIT_OROS, 4);
	g.hands[2][1] = card_make(SUIT_OROS, 7);
	g.hands[2][2] = card_make(SUIT_ESPADAS, 7);
	CHECK(!gn_can_play(&g, 2, 0) && gn_can_play(&g, 2, 1) && !gn_can_play(&g, 2, 2));

	/* Cantes y cambio del 7 */
	gn_init(&g, GN_GUINOTE, 9);
	g.trump = SUIT_BASTOS;
	g.trump_card = card_make(SUIT_BASTOS, 1);
	g.deck.cards[0] = g.trump_card;
	g.current = 2;
	g.last_trick_side = 0;
	g.hands[2][0] = card_make(SUIT_COPAS, RANK_REY);
	g.hands[2][1] = card_make(SUIT_COPAS, RANK_SOTA);
	g.hands[2][2] = card_make(SUIT_BASTOS, RANK_REY);
	g.hands[2][3] = card_make(SUIT_BASTOS, RANK_SOTA);
	g.hands[2][4] = card_make(SUIT_BASTOS, 7);
	g.hands[2][5] = card_make(SUIT_ESPADAS, 2);
	g.points[0] = 0;
	for (int s = 0; s < NUM_SUITS; s++)
		g.sung[s] = false;
	CHECK(gn_cante_suit(&g, 2) == SUIT_BASTOS); /* primero las cuarenta */
	CHECK(gn_cante(&g, 2) && g.points[0] == 40);
	CHECK(gn_cante(&g, 2) && g.points[0] == 60 && g.last_cante_suit == SUIT_COPAS);
	CHECK(!gn_cante(&g, 2));
	CHECK(gn_swap_index(&g, 2) == 4);
	CHECK(gn_swap(&g, 2));
	CHECK(g.hands[2][4].rank == 1 && g.trump_card.rank == 7 && g.deck.cards[0].rank == 7);
	g.last_trick_side = 1; /* sin ganar baza no se canta ni se cambia */
	CHECK(gn_cante_suit(&g, 2) < 0 && gn_swap_index(&g, 2) < 0);
}

/* Partidas completas: las reglas se cumplen y la IA gana a jugadas al azar. */
static void gn_simulate(GnVariant variant, const char* name)
{
	int wins[GN_PLAYERS] = { 0 };
	for (int game = 0; game < 300; game++) {
		Guinote g;
		gn_init(&g, variant, 5000 + game);
		Rng r;
		rng_seed(&r, 77 + game);
		int guard = 0;
		while (g.phase != GN_PHASE_GAME_OVER && guard++ < 5000) {
			if (g.phase == GN_PHASE_HAND_OVER) {
				CHECK((g.hand_count[0] == 0 && g.deck.count == 0) || g.tute);
				gn_new_hand(&g);
				continue;
			}
			if (g.phase == GN_PHASE_TRICK_DONE) {
				CHECK(gn_collect(&g));
				continue;
			}
			int p = g.current;
			if (gn_side(&g, p) == 0) {
				/* El bando 0 juega con la IA, canta y cambia el 7 */
				if (gn_sing_tute(&g, p))
					continue;
				gn_swap(&g, p);
				while (gn_cante(&g, p))
					;
				int i = gn_ai_choose(&g, p);
				CHECK(i >= 0 && gn_can_play(&g, p, i));
				CHECK(gn_play(&g, i));
			} else {
				int legal[GN_MAX_HAND], n = 0;
				for (int i = 0; i < g.hand_count[p]; i++)
					if (gn_can_play(&g, p, i))
						legal[n++] = i;
				CHECK(n > 0);
				if (n == 0)
					break;
				CHECK(gn_play(&g, legal[rng_range(&r, n)]));
			}
		}
		CHECK(g.phase == GN_PHASE_GAME_OVER);
		int w = gn_game_winner(&g);
		CHECK(w >= 0 && w < gn_sides(&g));
		if (variant == GN_GUINOTE)
			CHECK(g.score[w] >= GN_GAME_POINTS);
		else
			CHECK(g.score[w] == GN_TUTE_HANDS);
		if (w >= 0)
			wins[w]++;
	}
	int others = 0;
	for (int s = 1; s < GN_PLAYERS; s++)
		others += wins[s];
	printf("  %s: IA %d - azar %d\n", name, wins[0], others);
	/* En el tute cabrón la IA juega sola contra tres */
	if (variant == GN_TUTE_CABRON)
		CHECK(wins[0] * 3 > others);
	else
		CHECK(wins[0] > others * 2);
}

static void test_gn_simulation(void)
{
	gn_simulate(GN_GUINOTE, "guiñote");
	gn_simulate(GN_TUTE, "tute");
	gn_simulate(GN_TUTE_CABRON, "tute cabrón");
}

static void test_tute_rules(void)
{
	Guinote g;
	gn_init(&g, GN_TUTE, 21);
	CHECK(g.deck.count == 0 && gn_arrastre(&g));
	for (int p = 0; p < GN_PLAYERS; p++)
		CHECK(g.hand_count[p] == GN_MAX_HAND);
	/* El triunfo es la última carta, que tiene quien reparte (el de antes del que sale) */
	int dealer = (g.leader + GN_PLAYERS - 1) % GN_PLAYERS;
	CHECK(card_equal(g.hands[dealer][GN_MAX_HAND - 1], g.trump_card) && g.trump == g.trump_card.suit);
	/* En el tute el caballo gana a la sota y vale 3 */
	CHECK(gn_beats(&g, card_make(SUIT_COPAS, RANK_CABALLO), card_make(SUIT_COPAS, RANK_SOTA)));
	CHECK(gn_card_points(&g, card_make(SUIT_COPAS, RANK_CABALLO)) == 3);
	CHECK(gn_cante_rank(&g) == RANK_CABALLO);
	/* Asistir desde la primera baza */
	CHECK(gn_can_play(&g, g.current, 0));

	/* Cantes con rey y caballo, también sin mazo */
	g.current = 1;
	g.last_trick_side = 1;
	g.trump = SUIT_ESPADAS;
	g.hands[1][0] = card_make(SUIT_ESPADAS, RANK_REY);
	g.hands[1][1] = card_make(SUIT_ESPADAS, RANK_CABALLO);
	for (int s = 0; s < NUM_SUITS; s++)
		g.sung[s] = false;
	int before = g.points[1];
	CHECK(gn_cante_suit(&g, 1) == SUIT_ESPADAS);
	CHECK(gn_cante(&g, 1) && g.points[1] == before + 40);
	CHECK(gn_swap_index(&g, 1) < 0); /* en el tute no hay cambio del 7 */

	/* El tute: cuatro reyes ganan la mano */
	g.hands[1][0] = card_make(SUIT_OROS, RANK_REY);
	g.hands[1][1] = card_make(SUIT_COPAS, RANK_REY);
	g.hands[1][2] = card_make(SUIT_ESPADAS, RANK_REY);
	g.hands[1][3] = card_make(SUIT_BASTOS, RANK_REY);
	g.hand_count[1] = 4;
	CHECK(gn_can_sing_tute(&g, 1));
	CHECK(!gn_can_sing_tute(&g, 3)); /* no es su turno */
	CHECK(gn_sing_tute(&g, 1));
	CHECK(g.tute && g.hand_winner == 1 && g.score[1] == 1 && g.phase == GN_PHASE_HAND_OVER);

	/* Tute cabrón: cada uno para sí, sin excepción del compañero */
	gn_init(&g, GN_TUTE_CABRON, 3);
	CHECK(gn_sides(&g) == 4 && gn_side(&g, 2) == 2);
	g.trump = SUIT_OROS;
	g.leader = 0;
	g.current = 2;
	g.played_in_trick = 2;
	g.trick[0] = card_make(SUIT_COPAS, 1);
	g.trick[1] = card_make(SUIT_COPAS, 2);
	g.trick_played[0] = g.trick_played[1] = true;
	g.hand_count[2] = 2;
	g.hands[2][0] = card_make(SUIT_BASTOS, 4);
	g.hands[2][1] = card_make(SUIT_OROS, 4);
	CHECK(!gn_can_play(&g, 2, 0) && gn_can_play(&g, 2, 1)); /* hay que fallar aunque gane el 0 */
}

/* Una mano completa reparte 130 puntos más los cantes */
static void test_gn_points(void)
{
	for (int seed = 0; seed < 200; seed++) {
		Guinote g;
		gn_init(&g, GN_GUINOTE, seed);
		while (g.phase == GN_PHASE_PLAYING || g.phase == GN_PHASE_TRICK_DONE) {
			if (g.phase == GN_PHASE_TRICK_DONE)
				gn_collect(&g);
			else
				gn_play(&g, gn_ai_choose(&g, g.current));
		}
		CHECK(g.points[0] + g.points[1] == 120 + GN_LAST_TRICK);
	}
	for (int v = GN_TUTE; v < GN_VARIANTS; v++)
		for (int seed = 0; seed < 100; seed++) {
			Guinote g;
			gn_init(&g, (GnVariant)v, seed);
			while (g.phase == GN_PHASE_PLAYING || g.phase == GN_PHASE_TRICK_DONE) {
				if (g.phase == GN_PHASE_TRICK_DONE)
					gn_collect(&g);
				else
					gn_play(&g, gn_ai_choose(&g, g.current));
			}
			int total = 0;
			for (int s = 0; s < gn_sides(&g); s++)
				total += g.points[s];
			CHECK(total == 120 + GN_LAST_TRICK);
		}
}

/* ---------------------------------------------------------------------------
 * Solitario
 * ------------------------------------------------------------------------- */

static int sol_count_cards(const Solitario* g)
{
	int n = g->stock_count + g->waste_count;
	for (int s = 0; s < NUM_SUITS; s++)
		n += g->foundation[s];
	for (int k = 0; k < SOL_COLUMNS; k++)
		n += g->col[k].count;
	return n;
}

static void test_sol_rules(void)
{
	Solitario g;
	sol_init(&g, 3);
	CHECK(g.stock_count == SOL_STOCK && g.waste_count == 0);
	for (int k = 0; k < SOL_COLUMNS; k++)
		CHECK(g.col[k].count == k + 1 && g.col[k].hidden == k);
	CHECK(sol_count_cards(&g) == DECK_SIZE);

	/* Colocar en columnas: descendente y cambiando de palo */
	for (int k = 0; k < SOL_COLUMNS; k++)
		g.col[k].count = g.col[k].hidden = 0;
	g.col[0].count = 1;
	g.col[0].cards[0] = card_make(SUIT_OROS, RANK_CABALLO);
	g.col[1].count = 1;
	g.col[1].cards[0] = card_make(SUIT_COPAS, RANK_SOTA);
	g.col[2].count = 1;
	g.col[2].cards[0] = card_make(SUIT_OROS, RANK_SOTA);
	g.col[3].count = 1;
	g.col[3].cards[0] = card_make(SUIT_BASTOS, 7);
	CHECK(sol_can_move(&g, SOL_PILE_COLUMN + 1, 0, SOL_PILE_COLUMN + 0)); /* sota de copas sobre caballo de oros */
	CHECK(!sol_can_move(&g, SOL_PILE_COLUMN + 2, 0, SOL_PILE_COLUMN + 0)); /* mismo palo */
	CHECK(sol_can_move(&g, SOL_PILE_COLUMN + 3, 0, SOL_PILE_COLUMN + 1));  /* el 7 va sobre la sota */
	CHECK(!sol_can_move(&g, SOL_PILE_COLUMN + 3, 0, SOL_PILE_COLUMN + 4)); /* en vacía solo reyes */
	CHECK(sol_move(&g, SOL_PILE_COLUMN + 1, 0, SOL_PILE_COLUMN + 0));
	CHECK(sol_move(&g, SOL_PILE_COLUMN + 3, 0, SOL_PILE_COLUMN + 0));
	CHECK(g.col[0].count == 3 && g.col[1].count == 0);
	/* Un grupo ordenado se mueve entero: caballo, sota y 7 sobre un rey */
	g.col[4].count = 1;
	g.col[4].cards[0] = card_make(SUIT_ESPADAS, RANK_REY);
	CHECK(sol_first_movable(&g, 0) == 0);
	CHECK(sol_move(&g, SOL_PILE_COLUMN + 0, 0, SOL_PILE_COLUMN + 4));
	CHECK(g.col[4].count == 4 && g.col[0].count == 0);
	/* Los palos empiezan por el as */
	g.waste_count = 1;
	g.waste[0] = card_make(SUIT_COPAS, 2);
	CHECK(!sol_can_move(&g, SOL_PILE_WASTE, 0, SOL_PILE_FOUNDATION + SUIT_COPAS));
	g.waste[0] = card_make(SUIT_COPAS, 1);
	CHECK(sol_best_target(&g, SOL_PILE_WASTE, 0) == SOL_PILE_FOUNDATION + SUIT_COPAS);
	CHECK(sol_move(&g, SOL_PILE_WASTE, 0, SOL_PILE_FOUNDATION + SUIT_COPAS));
	CHECK(g.foundation[SUIT_COPAS] == 1);

	/* Al mover se descubre la carta de debajo */
	sol_init(&g, 11);
	g.col[1].hidden = 1;
	g.col[1].cards[1] = card_make(SUIT_OROS, RANK_REY);
	g.col[0].count = 0;
	g.col[0].hidden = 0;
	CHECK(sol_move(&g, SOL_PILE_COLUMN + 1, 1, SOL_PILE_COLUMN + 0));
	CHECK(g.col[1].count == 1 && g.col[1].hidden == 0);

	/* El mazo da la vuelta cuando se acaba */
	sol_init(&g, 5);
	for (int i = 0; i < SOL_STOCK; i++)
		CHECK(sol_draw(&g));
	CHECK(g.stock_count == 0 && g.waste_count == SOL_STOCK);
	Card first = g.waste[0];
	CHECK(sol_draw(&g) && g.passes == 1 && g.stock_count == SOL_STOCK);
	CHECK(sol_draw(&g) && card_equal(g.waste[0], first));
}

/* Jugador sencillo: sube a los palos, descubre cartas y saca del mazo. */
static bool sol_greedy_step(Solitario* g)
{
	if (sol_auto_step(g))
		return true;
	for (int k = 0; k < SOL_COLUMNS; k++) {
		int from = SOL_PILE_COLUMN + k;
		int i = sol_first_movable(g, k);
		if (i < 0 || (i == 0 && g->col[k].hidden == 0 && g->col[k].cards[0].rank == RANK_REY))
			continue;
		/* Solo si descubre algo o deja la columna libre */
		if (i > g->col[k].hidden && i != 0)
			continue;
		int to = sol_best_target(g, from, i);
		if (to >= SOL_PILE_COLUMN && sol_move(g, from, i, to))
			return true;
	}
	if (g->waste_count > 0) {
		int to = sol_best_target(g, SOL_PILE_WASTE, g->waste_count - 1);
		if (to >= 0 && sol_move(g, SOL_PILE_WASTE, g->waste_count - 1, to))
			return true;
	}
	return false;
}

static void test_sol_simulation(void)
{
	int won = 0;
	for (int game = 0; game < 300; game++) {
		Solitario g;
		sol_init(&g, 900 + game);
		int idle = 0;
		for (int step = 0; step < 3000 && !g.won; step++) {
			if (sol_greedy_step(&g)) {
				idle = 0;
			} else {
				if (++idle > SOL_STOCK * 2 + 4 || !sol_draw(&g))
					break;
			}
			CHECK(sol_count_cards(&g) == DECK_SIZE);
			if (sol_can_finish(&g))
				while (sol_auto_step(&g))
					;
		}
		won += g.won;
	}
	printf("  solitario: %d de 300 ganadas por un jugador sencillo\n", won);
	CHECK(won > 0);
}

/* ---------------------------------------------------------------------------
 * Solitario en pirámide
 * ------------------------------------------------------------------------- */

static void test_pyr_rules(void)
{
	CHECK(pyr_index(6, 0) == 21 && pyr_row(21) == 6 && pyr_row(20) == 5 && pyr_row(0) == 0);
	Piramide g;
	pyr_init(&g, 4);
	CHECK(g.stock_count == PYR_STOCK && g.left == PYR_CARDS);
	int exposed = 0;
	for (int i = 0; i < PYR_CARDS; i++)
		exposed += pyr_exposed(&g, i);
	CHECK(exposed == PYR_ROWS); /* solo la base */

	/* Una carta se descubre cuando no tiene ninguna de las dos de debajo */
	int top6 = pyr_index(5, 0);
	g.removed[pyr_index(6, 0)] = true;
	CHECK(!pyr_exposed(&g, top6));
	g.removed[pyr_index(6, 1)] = true;
	CHECK(pyr_exposed(&g, top6));

	/* Quitar una carta igual a la del mazo */
	pyr_init(&g, 4);
	Card c;
	CHECK(!pyr_current(&g, &c));
	CHECK(pyr_draw(&g) && pyr_current(&g, &c));
	int base = pyr_index(6, 3);
	g.card[base] = card_make(SUIT_ESPADAS, c.rank);
	g.card[pyr_index(5, 2)] = card_make(SUIT_ESPADAS, c.rank == 1 ? 2 : 1);
	CHECK(pyr_can_take(&g, base));
	CHECK(!pyr_can_take(&g, pyr_index(5, 2))); /* tapada */
	CHECK(pyr_take(&g, base) && g.left == PYR_CARDS - 1 && !pyr_take(&g, base));

	/* Quitar dos iguales de la base */
	g.card[pyr_index(6, 0)] = card_make(SUIT_OROS, RANK_CABALLO);
	g.card[pyr_index(6, 6)] = card_make(SUIT_COPAS, RANK_CABALLO);
	g.card[pyr_index(6, 1)] = card_make(SUIT_COPAS, RANK_REY);
	CHECK(pyr_can_pair(&g, pyr_index(6, 0), pyr_index(6, 6)));
	CHECK(!pyr_can_pair(&g, pyr_index(6, 0), pyr_index(6, 1)));
	CHECK(pyr_pair(&g, pyr_index(6, 0), pyr_index(6, 6)) && g.left == PYR_CARDS - 3);

	/* Tres pasadas del mazo y se acabó */
	pyr_init(&g, 8);
	int draws = 0;
	while (pyr_draw(&g))
		draws++;
	CHECK(draws == PYR_STOCK * PYR_PASSES && g.pass == PYR_PASSES);
}

static void test_pyr_simulation(void)
{
	int won = 0;
	for (int game = 0; game < 1000; game++) {
		Piramide g;
		pyr_init(&g, 3000 + game);
		for (int step = 0; step < 500 && !pyr_won(&g) && !pyr_stuck(&g); step++) {
			bool done = false;
			/* Primero con la carta del mazo; luego parejas de la pirámide */
			for (int i = PYR_CARDS - 1; i >= 0 && !done; i--)
				done = pyr_take(&g, i);
			for (int i = PYR_CARDS - 1; i >= 0 && !done; i--)
				for (int j = i - 1; j >= 0 && !done; j--)
					done = pyr_pair(&g, i, j);
			if (!done)
				CHECK(pyr_draw(&g));
		}
		CHECK(pyr_won(&g) || pyr_stuck(&g));
		won += pyr_won(&g);
	}
	printf("  pirámide: %d de 1000 ganadas por un jugador sencillo\n", won);
	CHECK(won > 0);
}

/* ---------------------------------------------------------------------------
 * Uno
 * ------------------------------------------------------------------------- */

static int uno_count_cards(const Uno* g)
{
	int n = g->deck_count + g->discard_count;
	for (int p = 0; p < UNO_PLAYERS; p++)
		n += g->hand_count[p];
	return n;
}

static void test_uno_rules(void)
{
	Uno g;
	uno_init(&g, 12);
	CHECK(uno_count_cards(&g) == UNO_DECK_SIZE);
	CHECK(!uno_is_wild(uno_top(&g)));
	int wild = 0, zeros = 0;
	for (int i = 0; i < g.deck_count; i++) {
		wild += uno_is_wild(g.deck[i]);
		zeros += g.deck[i].value == 0;
	}
	CHECK(uno_card_points((UnoCard){ UNO_RED, 7 }) == 7);
	CHECK(uno_card_points((UnoCard){ UNO_RED, UNO_SKIP }) == 20);
	CHECK(uno_card_points((UnoCard){ UNO_WILD, UNO_DRAW4 }) == 50);

	/* Reglas de juego en una situación preparada */
	g.phase = UNO_PHASE_PLAYING;
	g.current = 0;
	g.direction = 1;
	g.drew = false;
	g.drawn_index = -1;
	g.discard[0] = (UnoCard){ UNO_RED, 5 };
	g.discard_count = 1;
	g.color = UNO_RED;
	g.hand_count[0] = 5;
	g.hands[0][0] = (UnoCard){ UNO_BLUE, 5 };  /* mismo número */
	g.hands[0][1] = (UnoCard){ UNO_RED, 9 };   /* mismo color */
	g.hands[0][2] = (UnoCard){ UNO_GREEN, 2 }; /* no vale */
	g.hands[0][3] = (UnoCard){ UNO_WILD, UNO_WILDCARD };
	g.hands[0][4] = (UnoCard){ UNO_RED, UNO_DRAW2 };
	CHECK(uno_can_play(&g, 0, 0) && uno_can_play(&g, 0, 1) && !uno_can_play(&g, 0, 2) && uno_can_play(&g, 0, 3));
	CHECK(!uno_can_play(&g, 1, 0));
	/* Roba dos: el siguiente roba y pierde el turno */
	int before = g.hand_count[1];
	CHECK(uno_play(&g, 4));
	CHECK(g.hand_count[1] == before + 2 && g.current == 2);
	/* Cambio de sentido */
	g.current = 0;
	g.hands[0][g.hand_count[0]++] = (UnoCard){ UNO_RED, UNO_REVERSE };
	CHECK(uno_play(&g, g.hand_count[0] - 1));
	CHECK(g.direction == -1 && g.current == 3);
	/* Salta */
	g.current = 0;
	g.hands[0][g.hand_count[0]++] = (UnoCard){ UNO_RED, UNO_SKIP };
	CHECK(uno_play(&g, g.hand_count[0] - 1));
	CHECK(g.current == 2);
	/* Comodín: hay que elegir color */
	g.current = 0;
	int w = -1;
	for (int i = 0; i < g.hand_count[0]; i++)
		if (g.hands[0][i].value == UNO_WILDCARD)
			w = i;
	CHECK(w >= 0 && uno_play(&g, w));
	CHECK(g.phase == UNO_PHASE_CHOOSE_COLOR && g.current == 0);
	CHECK(uno_choose_color(&g, UNO_GREEN));
	CHECK(g.color == UNO_GREEN && g.phase == UNO_PHASE_PLAYING && g.current == 3);

	/* ¡Uno! y pillar al que no lo dice */
	g.current = 1;
	g.hand_count[1] = 2;
	g.hands[1][0] = (UnoCard){ UNO_GREEN, 3 };
	g.hands[1][1] = (UnoCard){ UNO_BLUE, 8 };
	g.said_uno[1] = false;
	CHECK(uno_play(&g, 0));
	CHECK(uno_catchable(&g, 1) && uno_catch(&g, 1) && g.hand_count[1] == 3);
	CHECK(!uno_catchable(&g, 1));
	g.hand_count[2] = 2;
	g.hands[2][0] = (UnoCard){ UNO_GREEN, 4 };
	g.hands[2][1] = (UnoCard){ UNO_BLUE, 8 };
	g.current = 2;
	CHECK(uno_say_uno(&g, 2) && uno_play(&g, 0));
	CHECK(!uno_catchable(&g, 2));

	/* Robar: solo una vez y solo se puede echar la robada */
	uno_init(&g, 31);
	int p = g.current;
	int n = g.hand_count[p];
	CHECK(uno_draw(&g));
	CHECK(g.hand_count[p] == n + 1);
	if (g.current == p) {
		CHECK(!uno_draw(&g));
		CHECK(uno_can_play(&g, p, g.drawn_index));
		CHECK(uno_pass(&g) && g.current != p);
	}
}

static void test_uno_simulation(void)
{
	int wins[UNO_PLAYERS] = { 0 };
	for (int game = 0; game < 200; game++) {
		Uno g;
		uno_init(&g, 7000 + game);
		Rng r;
		rng_seed(&r, game);
		int guard = 0;
		while (g.phase != UNO_PHASE_GAME_OVER && guard++ < 100000) {
			if (g.phase == UNO_PHASE_ROUND_OVER) {
				uno_new_round(&g);
				continue;
			}
			CHECK(uno_count_cards(&g) == UNO_DECK_SIZE);
			int p = g.current;
			if (g.phase == UNO_PHASE_CHOOSE_COLOR) {
				CHECK(uno_choose_color(&g, p == 0 ? uno_ai_color(&g, p) : (int)rng_range(&r, UNO_COLORS)));
				continue;
			}
			if (g.hand_count[p] == 2)
				uno_say_uno(&g, p);
			int i;
			if (p == 0) {
				i = uno_ai_choose(&g, p);
			} else {
				/* Los demás echan una carta válida al azar */
				int legal[UNO_MAX_HAND], n = 0;
				for (int k = 0; k < g.hand_count[p]; k++)
					if (uno_can_play(&g, p, k))
						legal[n++] = k;
				i = n ? legal[rng_range(&r, n)] : -1;
			}
			if (i >= 0)
				CHECK(uno_play(&g, i));
			else if (!g.drew)
				CHECK(uno_draw(&g));
			else
				CHECK(uno_pass(&g));
		}
		CHECK(g.phase == UNO_PHASE_GAME_OVER);
		int w = uno_game_winner(&g);
		CHECK(w >= 0);
		if (w >= 0)
			wins[w]++;
	}
	printf("  uno: IA %d - azar %d/%d/%d\n", wins[0], wins[1], wins[2], wins[3]);
	CHECK(wins[0] > wins[1] && wins[0] > wins[2] && wins[0] > wins[3]);
}

/* ---------------------------------------------------------------------------
 * Rummikub
 * ------------------------------------------------------------------------- */

#define T(c, v) ((RkTile){ c, v })
#define JK ((RkTile){ RK_JOKER, 0 })

static int rk_count_tiles(const Rummikub* g)
{
	int n = g->pool_count;
	for (int p = 0; p < RK_PLAYERS; p++)
		n += g->rack_count[p];
	for (int s = 0; s < g->table_count; s++)
		n += g->table[s].n;
	return n;
}

static void test_rk_rules(void)
{
	RkSet s;
	RkTile run[] = { T(0, 6), T(0, 4), T(0, 5) };
	CHECK(rk_make_set(run, 3, &s) && s.tiles[0].number == 4 && s.tiles[2].number == 6);
	CHECK(rk_set_points(&s) == 15);
	RkTile group[] = { T(0, 9), T(2, 9), T(3, 9) };
	CHECK(rk_make_set(group, 3, &s) && rk_set_points(&s) == 27);
	RkTile bad_group[] = { T(0, 9), T(0, 9), T(3, 9) };
	CHECK(!rk_make_set(bad_group, 3, &s));
	RkTile mixed[] = { T(0, 4), T(1, 5), T(0, 6) };
	CHECK(!rk_make_set(mixed, 3, &s));
	RkTile two[] = { T(0, 4), T(0, 5) };
	CHECK(!rk_make_set(two, 2, &s));
	/* Comodín en el hueco: 4 J 6 vale 4+5+6 */
	RkTile gap[] = { T(1, 4), JK, T(1, 6) };
	CHECK(rk_make_set(gap, 3, &s) && s.tiles[1].color == RK_JOKER && rk_set_points(&s) == 15);
	/* Comodín al final de la escalera; en el 13, por abajo */
	RkTile top[] = { T(2, 12), T(2, 13), JK };
	CHECK(rk_make_set(top, 3, &s) && s.tiles[0].color == RK_JOKER && rk_set_points(&s) == 36);
	/* Grupo con comodín */
	RkTile gj[] = { T(0, 11), JK, T(3, 11) };
	CHECK(rk_make_set(gj, 3, &s) && rk_set_points(&s) == 33);
	/* Ni el 14 ni números repetidos en la escalera */
	RkTile rep[] = { T(0, 3), T(0, 3), T(0, 4) };
	CHECK(!rk_make_set(rep, 3, &s));

	Rummikub g;
	rk_init(&g, 5);
	CHECK(rk_count_tiles(&g) == RK_TILES);
	for (int p = 0; p < RK_PLAYERS; p++)
		CHECK(g.rack_count[p] == RK_START);
	/* Primera jugada: menos de 30 no vale y se roba */
	int p = g.current;
	g.rack_count[p] = 5;
	g.racks[p][0] = T(0, 1);
	g.racks[p][1] = T(0, 2);
	g.racks[p][2] = T(0, 3);
	g.racks[p][3] = T(1, 10);
	g.racks[p][4] = T(2, 10);
	/* El turno empieza con este atril */
	memcpy(g.saved_rack, g.racks[p], sizeof(RkTile) * 5);
	g.saved_rack_count = 5;
	int idx[3] = { 0, 1, 2 };
	CHECK(rk_play_set(&g, idx, 3) && g.rack_count[p] == 2 && g.table_count == 1);
	CHECK(!rk_turn_valid(&g));
	CHECK(rk_end_turn(&g)); /* se deshace y roba */
	CHECK(g.table_count == 0 && g.rack_count[p] == 6 && !g.melded[p] && g.current != p);

	/* Con 30 o más vale; luego se pueden añadir fichas a la mesa */
	rk_init(&g, 6);
	p = g.current;
	g.rack_count[p] = 6;
	g.racks[p][0] = T(3, 10);
	g.racks[p][1] = T(3, 11);
	g.racks[p][2] = T(3, 12);
	g.racks[p][3] = T(3, 13);
	g.racks[p][4] = T(1, 2);
	g.racks[p][5] = T(1, 9);
	memcpy(g.saved_rack, g.racks[p], sizeof(RkTile) * 6);
	g.saved_rack_count = 6;
	int meld[3] = { 0, 1, 2 };
	CHECK(rk_play_set(&g, meld, 3) && rk_turn_valid(&g));
	CHECK(rk_can_add(&g, 0, 0)); /* el 13 alarga la escalera */
	CHECK(rk_add(&g, 0, 0) && g.table[0].n == 4);
	CHECK(!rk_can_add(&g, 0, 0)); /* el 2 azul no encaja */
	CHECK(!rk_end_turn(&g) && g.melded[p]);
	CHECK(rk_count_tiles(&g) == RK_TILES - (RK_START - 6));
}

static void test_rk_simulation(void)
{
	int wins[RK_PLAYERS] = { 0 };
	int rounds = 0, placed = 0;
	for (int game = 0; game < 120; game++) {
		Rummikub g;
		rk_init(&g, 9000 + game);
		int guard = 0;
		while (g.phase == RK_PHASE_PLAYING && guard++ < 2000) {
			placed += rk_ai_turn(&g);
			/* Todas las jugadas de la mesa son válidas */
			for (int s = 0; s < g.table_count; s++) {
				RkSet tmp;
				CHECK(rk_make_set(g.table[s].tiles, g.table[s].n, &tmp));
			}
			CHECK(rk_count_tiles(&g) + 0 == RK_TILES);
		}
		CHECK(g.phase == RK_PHASE_ROUND_OVER);
		int sum = 0;
		for (int p = 0; p < RK_PLAYERS; p++)
			sum += g.score[p];
		CHECK(sum == 0); /* lo que gana uno lo pierden los demás */
		if (g.round_winner >= 0)
			wins[g.round_winner]++;
		rounds++;
	}
	printf("  rummikub: %d rondas, %d fichas puestas, victorias %d/%d/%d/%d\n", rounds, placed, wins[0], wins[1],
		wins[2], wins[3]);
}

/* ---------------------------------------------------------------------------
 * La mona, cucú, treinta y una, mentiroso y culo
 * ------------------------------------------------------------------------- */

static void test_mona(void)
{
	int losers[MONA_PLAYERS] = { 0 };
	for (int game = 0; game < 300; game++) {
		Mona g;
		mona_init(&g, 400 + game);
		int total = 0;
		for (int p = 0; p < MONA_PLAYERS; p++) {
			total += g.hand_count[p] + g.pairs[p] * 2;
			/* Tras echar las parejas nadie tiene dos cartas iguales */
			for (int i = 0; i < g.hand_count[p]; i++)
				for (int j = i + 1; j < g.hand_count[p]; j++)
					CHECK(g.hands[p][i].rank != g.hands[p][j].rank);
		}
		CHECK(total == DECK_SIZE - 1);
		int guard = 0;
		while (!mona_over(&g) && guard++ < 1000) {
			CHECK(mona_active(&g, g.current));
			CHECK(mona_take(&g, mona_ai_pick(&g)));
		}
		CHECK(mona_over(&g));
		/* El que pierde se queda solo con la mona: una sota */
		CHECK(g.hand_count[g.loser] == 1 && g.hands[g.loser][0].rank == RANK_SOTA);
		losers[g.loser]++;
	}
	printf("  la mona: pierden %d/%d/%d/%d\n", losers[0], losers[1], losers[2], losers[3]);
}

static void test_cucu(void)
{
	Cucu g;
	cucu_init(&g, 3);
	CHECK(cucu_value(card_make(SUIT_OROS, 1)) < cucu_value(card_make(SUIT_OROS, 2)));
	CHECK(cucu_value(card_make(SUIT_OROS, RANK_REY)) == 9);
	/* Con un rey no se puede cambiar */
	g.current = (g.dealer + 1) % CUCU_PLAYERS;
	int q = cucu_partner(&g);
	g.card[g.current] = card_make(SUIT_OROS, 2);
	g.card[q] = card_make(SUIT_COPAS, RANK_REY);
	CHECK(cucu_decide(&g, true) && g.last_blocked && g.card[q].rank == RANK_REY);
	int wins[CUCU_PLAYERS] = { 0 };
	for (int game = 0; game < 300; game++) {
		cucu_init(&g, 600 + game);
		int guard = 0;
		while (g.phase != CUCU_PHASE_GAME_OVER && guard++ < 2000) {
			if (g.phase == CUCU_PHASE_REVEAL) {
				int lost = 0, alive = 0;
				for (int p = 0; p < CUCU_PLAYERS; p++) {
					lost += g.lost[p];
					alive += cucu_alive(&g, p) || g.lost[p];
				}
				CHECK(lost < alive); /* nunca pierden todos a la vez */
				cucu_new_round(&g);
				continue;
			}
			CHECK(cucu_alive(&g, g.current));
			cucu_decide(&g, cucu_ai_swap(&g));
		}
		int w = cucu_winner(&g);
		CHECK(w >= 0 && cucu_alive(&g, w));
		wins[w]++;
	}
	printf("  cucú: victorias %d/%d/%d/%d\n", wins[0], wins[1], wins[2], wins[3]);
}

static void test_treintayuna(void)
{
	Card a[3] = { card_make(SUIT_OROS, 1), card_make(SUIT_OROS, RANK_REY), card_make(SUIT_OROS, RANK_SOTA) };
	CHECK(tyu_hand_value(a) == 31);
	Card b[3] = { card_make(SUIT_OROS, 7), card_make(SUIT_COPAS, RANK_REY), card_make(SUIT_COPAS, 5) };
	CHECK(tyu_hand_value(b) == 15);
	int wins[TYU_PLAYERS] = { 0 };
	for (int game = 0; game < 300; game++) {
		TreintaYUna g;
		tyu_init(&g, 800 + game);
		int guard = 0;
		while (g.phase != TYU_PHASE_GAME_OVER && guard++ < 5000) {
			if (g.phase == TYU_PHASE_REVEAL) {
				tyu_new_round(&g);
				continue;
			}
			CHECK(tyu_alive(&g, g.current));
			if (g.current == 0 || game % 2) {
				tyu_ai_turn(&g);
			} else {
				/* Los demás cambian al azar (y se plantan a veces) */
				Rng* r = &g.rng;
				if (tyu_can_knock(&g) && rng_range(r, 6) == 0)
					tyu_knock(&g);
				else
					tyu_swap(&g, rng_range(r, 3), rng_range(r, 3));
			}
		}
		CHECK(g.phase == TYU_PHASE_GAME_OVER);
		int w = tyu_winner(&g);
		CHECK(w >= 0);
		if (w >= 0)
			wins[w]++;
	}
	printf("  treinta y una: victorias %d/%d/%d/%d\n", wins[0], wins[1], wins[2], wins[3]);
}

static void test_mentiroso(void)
{
	Mentiroso g;
	men_init(&g, 4);
	for (int p = 0; p < MEN_PLAYERS; p++)
		CHECK(g.hand_count[p] == 10);
	/* Acusación: si mintió, se lleva el montón */
	int p = g.current;
	int idx[1] = { 0 };
	Card c = g.hands[p][0];
	int liar_rank = c.rank == 1 ? 2 : 1;
	CHECK(men_play(&g, idx, 1, liar_rank));
	CHECK(men_can_call(&g) && men_call(&g));
	CHECK(g.was_lie && g.taker == p && g.hand_count[p] == 10);
	men_continue(&g);
	CHECK(g.rank == 0 && g.current == (p + 1) % MEN_PLAYERS);

	int wins[MEN_PLAYERS] = { 0 };
	for (int game = 0; game < 200; game++) {
		men_init(&g, 1100 + game);
		int guard = 0;
		while (g.phase != MEN_PHASE_GAME_OVER && guard++ < 5000) {
			if (g.phase == MEN_PHASE_REVEAL) {
				men_continue(&g);
				continue;
			}
			int total = g.pile_count;
			for (int q = 0; q < MEN_PLAYERS; q++)
				total += g.hand_count[q];
			CHECK(total == DECK_SIZE);
			int ids[MEN_MAX_PLAY], n, rank;
			if (men_ai_turn(&g, ids, &n, &rank))
				CHECK(men_call(&g));
			else
				CHECK(men_play(&g, ids, n, rank));
		}
		CHECK(g.phase == MEN_PHASE_GAME_OVER && g.winner >= 0);
		if (g.winner >= 0) {
			CHECK(g.hand_count[g.winner] == 0);
			wins[g.winner]++;
		}
	}
	printf("  mentiroso: victorias %d/%d/%d/%d\n", wins[0], wins[1], wins[2], wins[3]);
}

static void test_culo(void)
{
	CHECK(culo_value(card_make(SUIT_OROS, 3)) == 0);
	CHECK(culo_value(card_make(SUIT_OROS, 2)) == 9);
	CHECK(culo_value(card_make(SUIT_OROS, 1)) > culo_value(card_make(SUIT_OROS, RANK_REY)));
	Culo g;
	culo_init(&g, 2);
	/* Sale quien tiene el 3 de oros */
	bool has3 = false;
	for (int i = 0; i < g.hand_count[g.current]; i++)
		has3 |= g.hands[g.current][i].rank == 3 && g.hands[g.current][i].suit == SUIT_OROS;
	CHECK(has3);
	CHECK(!culo_can_pass(&g));
	int pres[CULO_PLAYERS] = { 0 };
	for (int game = 0; game < 100; game++) {
		culo_init(&g, 1500 + game);
		for (int round = 0; round < 4; round++) {
			int guard = 0;
			while (g.phase == CULO_PHASE_PLAYING && guard++ < 2000) {
				int total = 0;
				for (int q = 0; q < CULO_PLAYERS; q++)
					total += g.hand_count[q];
				CHECK(!culo_done(&g, g.current));
				int idx[CULO_MAX_PLAY], n;
				if (culo_ai_choose(&g, idx, &n)) {
					CHECK(culo_play(&g, idx, n));
				} else {
					CHECK(culo_pass(&g));
				}
				(void)total;
			}
			CHECK(g.phase == CULO_PHASE_ROUND_OVER);
			/* Todos tienen un puesto distinto */
			bool seen[CULO_PLAYERS] = { false };
			for (int q = 0; q < CULO_PLAYERS; q++) {
				CHECK(g.role[q] >= 0 && g.role[q] < CULO_PLAYERS && !seen[g.role[q]]);
				seen[g.role[q]] = true;
			}
			pres[g.finish[0]]++;
			culo_new_round(&g);
			int total = 0;
			for (int q = 0; q < CULO_PLAYERS; q++)
				total += g.hand_count[q];
			CHECK(total == DECK_SIZE);
		}
	}
	printf("  culo: presidencias %d/%d/%d/%d\n", pres[0], pres[1], pres[2], pres[3]);
}

/* ---------------------------------------------------------------------------
 * Póker
 * ------------------------------------------------------------------------- */

static uint32_t pk_hand(const char* text)
{
	/* "AsKs..." -> cartas: valor (2-9, T, J, Q, K, A) y palo (s, h, d, c) */
	PkCard c[7];
	int n = 0;
	for (const char* t = text; t[0] && t[1]; t += 2) {
		const char* ranks = "23456789TJQKA";
		const char* suits = "shdc";
		c[n].rank = (uint8_t)(strchr(ranks, t[0]) - ranks + 2);
		c[n].suit = (uint8_t)(strchr(suits, t[1]) - suits);
		n++;
	}
	return pk_evaluate(c, n);
}

static void test_poker_eval(void)
{
	CHECK(pk_category(pk_hand("AsKsQsJsTs2d3c")) == 8);
	CHECK(pk_category(pk_hand("9h9d9c9s2d3c4h")) == 7);
	CHECK(pk_category(pk_hand("9h9d9c2s2d3c4h")) == 6);
	CHECK(pk_category(pk_hand("2h5h9hJhKh3c4d")) == 5);
	CHECK(pk_category(pk_hand("As2d3c4h5s9dKc")) == 4); /* escalera con el as abajo */
	CHECK(pk_category(pk_hand("7h7d7c2s9dJcKh")) == 3);
	CHECK(pk_category(pk_hand("7h7d2c2s9dJcKh")) == 2);
	CHECK(pk_category(pk_hand("7h7d3c2s9dJcKh")) == 1);
	CHECK(pk_category(pk_hand("7h8d3c2s9dJcKh")) == 0);
	/* Full con color en la mesa: gana el full */
	CHECK(pk_category(pk_hand("2h2d2c5h5d9hJh")) == 6);
	/* Desempates */
	CHECK(pk_hand("AhAd9c8s2d3c4h") > pk_hand("KhKdQcJs2d3c4h"));
	CHECK(pk_hand("AhAdKc8s2d3c4h") > pk_hand("AcAsQcJs2d3c4h"));
	CHECK(pk_hand("6h7h8h9hTh2c2d") > pk_hand("5h6h7h8h9h2c2d"));
	CHECK(pk_hand("AsKdQcJhTs2c3d") > pk_hand("As2d3c4h5s9dKc"));
	CHECK(pk_hand("AhKd8c8s2d2c4h") == pk_hand("AsKc8h8d2h2s4d"));
}

static int pk_total(const Poker* g)
{
	int t = 0;
	for (int p = 0; p < PK_PLAYERS; p++)
		t += g->chips[p];
	return t;
}

static void test_poker_game(void)
{
	/* Botes secundarios: el que va all-in con poco solo gana lo suyo */
	Poker g;
	pk_init(&g, 1);
	for (int p = 0; p < PK_PLAYERS; p++) {
		g.chips[p] = 0;
		g.bet[p] = 0;
		g.folded[p] = false;
		g.out[p] = false;
	}
	g.invested[0] = 100; /* el mejor, pero con poco */
	g.invested[1] = 300;
	g.invested[2] = 300;
	g.invested[3] = 50; /* se retira */
	g.folded[3] = true;
	g.hole[0][0] = (PkCard){ 14, 0 };
	g.hole[0][1] = (PkCard){ 14, 1 };
	g.hole[1][0] = (PkCard){ 13, 0 };
	g.hole[1][1] = (PkCard){ 13, 1 };
	g.hole[2][0] = (PkCard){ 3, 2 };
	g.hole[2][1] = (PkCard){ 4, 3 };
	PkCard board[5] = { { 2, 0 }, { 7, 1 }, { 9, 2 }, { 11, 3 }, { 12, 0 } };
	for (int i = 0; i < 5; i++)
		g.board[i] = board[i];
	g.board_count = 5;
	g.stage = PK_RIVER;
	g.to_call = 0;
	for (int p = 0; p < PK_PLAYERS; p++)
		g.acted[p] = true;
	/* Se fuerza el reparto con la última acción de la ronda */
	g.chips[1] = 10;
	g.current = 1;
	g.acted[1] = false;
	CHECK(pk_act(&g, PK_CHECK_CALL, 0));
	CHECK(g.stage == PK_SHOWDOWN || g.stage == PK_GAME_OVER);
	CHECK(g.won[0] == 100 * 3 + 50);      /* capa de 100 de los tres y los 50 del retirado */
	CHECK(g.won[1] == 400);              /* lo que queda entre el 1 y el 2 */
	CHECK(g.won[2] == 0);

	/* Partidas completas: las fichas no se crean ni se pierden */
	int wins[PK_PLAYERS] = { 0 }, hands = 0;
	for (int game = 0; game < 40; game++) {
		pk_init(&g, 2000 + game);
		int guard = 0;
		while (!pk_game_over(&g) && guard++ < 20000) {
			if (pk_hand_finished(&g)) {
				CHECK(pk_total(&g) == PK_START * PK_PLAYERS);
				hands++;
				pk_new_hand(&g);
				continue;
			}
			int amount = 0;
			PkAction a = pk_ai_decide(&g, &amount);
			CHECK(pk_act(&g, a, amount));
			int sum = 0;
			for (int p = 0; p < PK_PLAYERS; p++) {
				CHECK(g.chips[p] >= 0);
				sum += g.chips[p] + g.invested[p];
			}
			if (!pk_hand_finished(&g))
				CHECK(sum == PK_START * PK_PLAYERS);
			if (hands > 400)
				break;
		}
		for (int p = 0; p < PK_PLAYERS; p++)
			if (g.chips[p] == PK_START * PK_PLAYERS)
				wins[p]++;
	}
	printf("  póker: %d manos, ganadores %d/%d/%d/%d\n", hands, wins[0], wins[1], wins[2], wins[3]);
}

/* ---------------------------------------------------------------------------
 * Blackjack, corazones, pocha, conecta 4, damas y dominó
 * ------------------------------------------------------------------------- */

static void test_blackjack(void)
{
	BjHand h = { { { 14, 0 }, { 13, 1 } }, 2, 10, false, false, 0 };
	CHECK(bj_total(&h, NULL) == 21 && bj_is_blackjack(&h));
	BjHand s = { { { 14, 0 }, { 6, 1 }, { 9, 2 } }, 3, 10, false, false, 0 };
	bool soft;
	CHECK(bj_total(&s, &soft) == 16 && !soft); /* el as pasa a valer 1 */
	BjHand t = { { { 14, 0 }, { 14, 1 }, { 9, 2 } }, 3, 10, false, false, 0 };
	CHECK(bj_total(&t, &soft) == 21 && soft);

	Blackjack g;
	int won = 0, played = 0;
	for (int game = 0; game < 50; game++) {
		bj_init(&g, 300 + game);
		for (int hand = 0; hand < 60 && g.phase != BJ_BROKE; hand++) {
			int before = g.chips;
			bj_set_bet(&g, 20);
			CHECK(bj_deal(&g));
			int guard = 0;
			while (g.phase == BJ_PLAYING && guard++ < 50) {
				char a = bj_advice(&g);
				if (a == 'P' && bj_split(&g))
					continue;
				if (a == 'D' && bj_double(&g))
					continue;
				if (a == 'H' || a == 'D' || a == 'P')
					bj_hit(&g);
				else
					bj_stand(&g);
			}
			while (g.phase == BJ_DEALER)
				bj_dealer_step(&g);
			CHECK(g.phase == BJ_RESULT || g.phase == BJ_BROKE);
			CHECK(g.chips - before == g.net);
			bool dsoft;
			CHECK(bj_total(&g.dealer, &dsoft) >= 17 || bj_is_blackjack(&g.hands[0]) ||
			      bj_total(&g.hands[0], NULL) > 21 || g.hand_count == 2);
			played++;
			won += g.net > 0;
		}
	}
	printf("  blackjack: %d manos, %d ganadas con la estrategia básica\n", played, won);
	CHECK(won * 100 / played > 35);
}

static void test_corazones(void)
{
	int wins[HE_PLAYERS] = { 0 };
	for (int game = 0; game < 60; game++) {
		Corazones g;
		he_init(&g, 1700 + game);
		int guard = 0;
		while (g.phase != HE_GAME_OVER && guard++ < 5000) {
			if (g.phase == HE_PASSING) {
				int idx[3];
				he_ai_pass(&g, 0, idx);
				CHECK(he_pass(&g, idx));
				for (int p = 0; p < HE_PLAYERS; p++)
					CHECK(g.hand_count[p] == HE_HAND);
				continue;
			}
			if (g.phase == HE_HAND_OVER) {
				int sum = 0;
				for (int p = 0; p < HE_PLAYERS; p++)
					sum += g.points[p];
				CHECK(sum == 26 || (g.moon >= 0 && sum == 78));
				he_new_hand(&g);
				continue;
			}
			if (g.phase == HE_TRICK_DONE) {
				CHECK(he_collect(&g));
				continue;
			}
			if (g.tricks == 0 && g.played_in_trick == 0) {
				/* Sale el 2 de tréboles */
				bool two = false;
				for (int i = 0; i < g.hand_count[g.current]; i++)
					two |= g.hands[g.current][i].suit == 3 && g.hands[g.current][i].rank == 2;
				CHECK(two);
			}
			int i = he_ai_choose(&g, g.current);
			CHECK(i >= 0 && he_play(&g, i));
		}
		CHECK(g.phase == HE_GAME_OVER);
		wins[he_winner(&g)]++;
	}
	printf("  corazones: victorias %d/%d/%d/%d\n", wins[0], wins[1], wins[2], wins[3]);
}

static void test_pocha(void)
{
	Pocha g;
	po_init(&g, 5);
	CHECK(g.round == 1 && g.phase == PO_BIDDING);
	/* El que reparte no puede cuadrar las bazas */
	for (int k = 0; k < 3; k++)
		CHECK(po_bid(&g, 0));
	CHECK(g.current == g.dealer && po_forbidden_bid(&g) == 1 && !po_can_bid(&g, 1) && po_can_bid(&g, 0));
	int wins[PO_PLAYERS] = { 0 };
	for (int game = 0; game < 100; game++) {
		po_init(&g, 1900 + game);
		int guard = 0;
		while (g.phase != PO_GAME_OVER && guard++ < 5000) {
			if (g.phase == PO_ROUND_OVER) {
				int tricks = 0, bids = 0;
				for (int p = 0; p < PO_PLAYERS; p++) {
					tricks += g.tricks_won[p];
					bids += g.bid[p];
				}
				CHECK(tricks == g.round && bids != g.round);
				po_new_round(&g);
				continue;
			}
			if (g.phase == PO_BIDDING) {
				CHECK(po_bid(&g, po_ai_bid(&g)));
				continue;
			}
			if (g.phase == PO_TRICK_DONE) {
				CHECK(po_collect(&g));
				continue;
			}
			int i = po_ai_choose(&g, g.current);
			CHECK(i >= 0 && po_play(&g, i));
		}
		CHECK(g.phase == PO_GAME_OVER && g.round == PO_ROUNDS);
		wins[po_winner(&g)]++;
	}
	printf("  pocha: victorias %d/%d/%d/%d\n", wins[0], wins[1], wins[2], wins[3]);
}

static void test_conecta4(void)
{
	Conecta4 g;
	c4_init(&g, 1);
	g.current = 1;
	/* Cuatro en vertical */
	for (int k = 0; k < 3; k++) {
		CHECK(c4_drop(&g, 0));
		CHECK(c4_drop(&g, 1));
	}
	CHECK(c4_drop(&g, 0) && g.winner == 1);
	/* La CPU tapa un tres en raya */
	c4_new_game(&g);
	g.current = 1;
	c4_drop(&g, 0);
	c4_drop(&g, 6);
	c4_drop(&g, 1);
	c4_drop(&g, 6);
	c4_drop(&g, 2);
	CHECK(g.current == 2 && c4_ai_choose(&g) == 3);
	/* Partidas contra jugadas al azar */
	int wins = 0;
	for (int game = 0; game < 20; game++) {
		c4_init(&g, 50 + game);
		g.level = 4;
		while (!g.winner) {
			int col = g.current == 2 ? c4_ai_choose(&g) : (int)rng_range(&g.rng, C4_COLS);
			c4_drop(&g, col);
		}
		wins += g.winner == 2;
	}
	printf("  conecta 4: la CPU gana %d de 20 al azar\n", wins);
	CHECK(wins >= 18);
}

static void test_damas(void)
{
	Damas g;
	da_init(&g, 3);
	CHECK(da_count(&g, 1) == 12 && da_count(&g, 2) == 12);
	DaMove moves[DA_MAX_MOVES];
	CHECK(da_moves(&g, moves) == 7); /* jugadas de salida de las blancas */
	/* Comer es obligatorio y se elige la que más come */
	memset(g.b, 0, sizeof g.b);
	g.current = 1;
	g.b[5][2] = DA_WHITE;
	g.b[4][3] = DA_BLACK;
	g.b[2][5] = DA_BLACK;
	g.b[5][6] = DA_WHITE;
	int n = da_moves(&g, moves);
	CHECK(n == 1 && moves[0].n_caps == 2 && moves[0].path[1][0] == 1 && moves[0].path[1][1] == 6);
	CHECK(da_apply(&g, &moves[0]) && da_count(&g, 2) == 0 && g.winner == 1);
	/* La dama vuela y come a distancia */
	memset(g.b, 0, sizeof g.b);
	g.winner = 0;
	g.current = 1;
	g.b[7][0] = DA_WHITE_KING;
	g.b[3][4] = DA_BLACK;
	g.b[0][1] = DA_BLACK;
	n = da_moves(&g, moves);
	CHECK(n == 3 && moves[0].n_caps == 1); /* puede caer en 2,5 / 1,6 / 0,7 */
	/* Corona al llegar al final */
	memset(g.b, 0, sizeof g.b);
	g.current = 1;
	g.b[1][2] = DA_WHITE;
	g.b[6][1] = DA_BLACK;
	n = da_moves(&g, moves);
	DaMove up = moves[0];
	CHECK(da_apply(&g, &up) && g.b[0][up.path[0][1]] == DA_WHITE_KING);
	/* Partidas de la CPU contra jugadas al azar */
	int wins = 0;
	for (int game = 0; game < 6; game++) {
		da_init(&g, 70 + game);
		g.level = 3;
		int guard = 0;
		while (!g.winner && guard++ < 400) {
			DaMove m;
			if (g.current == 2) {
				CHECK(da_ai_choose(&g, &m) > 0);
			} else {
				int k = da_moves(&g, moves);
				m = moves[rng_range(&g.rng, k)];
			}
			CHECK(da_apply(&g, &m));
			CHECK(da_count(&g, 1) <= 12 && da_count(&g, 2) <= 12);
		}
		wins += g.winner == 2;
	}
	printf("  damas: la CPU gana %d de 6 al azar\n", wins);
	CHECK(wins >= 5);
}

static void test_domino(void)
{
	Domino g;
	do_init(&g, 9);
	/* Empieza quien tiene el seis doble, y con él */
	bool has66 = false;
	for (int i = 0; i < g.hand_count[g.current]; i++)
		has66 |= g.hands[g.current][i].a == 6 && g.hands[g.current][i].b == 6;
	CHECK(has66);
	int wins[2] = { 0 };
	for (int game = 0; game < 100; game++) {
		do_init(&g, 2300 + game);
		int guard = 0;
		while (g.phase != DO_GAME_OVER && guard++ < 5000) {
			if (g.phase == DO_HAND_OVER) {
				do_new_hand(&g);
				continue;
			}
			int idx, side;
			if (do_ai_choose(&g, &idx, &side))
				CHECK(do_play(&g, idx, side));
			else
				CHECK(do_pass(&g));
			/* La cadena siempre casa */
			for (int i = 1; i < g.chain_count; i++)
				CHECK(g.chain[i - 1].b == g.chain[i].a);
			int total = g.chain_count;
			for (int p = 0; p < DO_PLAYERS; p++)
				total += g.hand_count[p];
			CHECK(total == DO_TILES);
		}
		CHECK(g.phase == DO_GAME_OVER);
		wins[g.score[0] >= DO_GAME ? 0 : 1]++;
	}
	printf("  dominó: parejas %d - %d\n", wins[0], wins[1]);
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
	test_mus_senas();
	test_mus_senas_simulation();
	test_gn_rules();
	test_gn_points();
	test_gn_simulation();
	test_tute_rules();
	test_uno_rules();
	test_uno_simulation();
	test_rk_rules();
	test_rk_simulation();
	test_mona();
	test_cucu();
	test_treintayuna();
	test_mentiroso();
	test_culo();
	test_poker_eval();
	test_poker_game();
	test_blackjack();
	test_corazones();
	test_pocha();
	test_conecta4();
	test_damas();
	test_domino();
	test_sol_rules();
	test_sol_simulation();
	test_pyr_rules();
	test_pyr_simulation();
	test_profile();
	test_mii_parts();
	test_cardback();
	test_profile_v1();
	test_profile_v2();
	test_rivals();
	printf("%d comprobaciones, %d fallos\n", checks, failures);
	return failures ? 1 : 0;
}
