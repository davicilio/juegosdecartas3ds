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
#include "games/as23.h"
#include "games/videopoker.h"
#include "games/mahjong.h"
#include "games/toma6.h"
#include "games/freecell.h"
#include "games/negocio.h"
#include "games/parchis.h"
#include "games/flota.h"
#include "games/tresenraya.h"
#include "games/bingo.h"
#include "games/tresillo.h"
#include "games/mierda.h"
#include "games/burro.h"
#include "games/oca.h"
#include "games/reversi.h"
#include "games/butifarra.h"
#include "games/julepe.h"
#include "games/pumba.h"
#include "games/truco.h"
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

static void test_profile_v3(void)
{
	/* Un perfil de la versión 3 (32 juegos) conserva todo */
	struct {
		uint32_t magic;
		uint16_t version;
		uint8_t created, from_console, online_miis, reserved[3];
		Avatar avatar;
		uint32_t played[PROFILE_V3_SLOTS], won[PROFILE_V3_SLOTS];
	} old;
	memset(&old, 0, sizeof old);
	old.magic = PROFILE_MAGIC;
	old.version = 3;
	old.created = 1;
	avatar_from_mii(&old.avatar, &cpu_avatar(CPU_PEPE)->mii, "Tres");
	old.played[GAME_TRUCO] = 9;
	old.won[GAME_TRUCO] = 4;
	const char* path = "test_perfil_v3.dat";
	FILE* f = fopen(path, "wb");
	fwrite(&old, 1, sizeof old, f);
	fclose(f);
	PlayerProfile p;
	CHECK(profile_load(&p, path));
	CHECK(p.version == PROFILE_VERSION && p.created == 1 && strcmp(p.avatar.name, "Tres") == 0);
	CHECK(p.played[GAME_TRUCO] == 9 && p.won[GAME_TRUCO] == 4 && p.played[GAME_OCA] == 0);
	remove(path);
}

static void test_profile_v4(void)
{
	/* Un perfil de la versión 4 conserva todo y recibe el monedero inicial */
	struct {
		uint32_t magic;
		uint16_t version;
		uint8_t created, from_console, online_miis, reserved[3];
		Avatar avatar;
		uint32_t played[PROFILE_GAME_SLOTS], won[PROFILE_GAME_SLOTS];
	} old;
	memset(&old, 0, sizeof old);
	old.magic = PROFILE_MAGIC;
	old.version = 4;
	old.created = 1;
	avatar_from_mii(&old.avatar, &cpu_avatar(CPU_PEPE)->mii, "Cuatro");
	old.played[GAME_MAHJONG] = 7;
	old.won[GAME_VIDEOPOKER] = 2;
	const char* path = "test_perfil_v4.dat";
	FILE* f = fopen(path, "wb");
	fwrite(&old, 1, sizeof old, f);
	fclose(f);
	PlayerProfile p;
	CHECK(profile_load(&p, path));
	CHECK(p.version == PROFILE_VERSION && p.created == 1 && strcmp(p.avatar.name, "Cuatro") == 0);
	CHECK(p.played[GAME_MAHJONG] == 7 && p.won[GAME_VIDEOPOKER] == 2);
	CHECK(p.chips == PROFILE_START_CHIPS);
	/* El monedero se guarda */
	CHECK(profile_take_chips(&p, 500) == 500 && p.chips == PROFILE_START_CHIPS - 500);
	CHECK(profile_take_chips(&p, 99999) == PROFILE_START_CHIPS - 500 && p.chips == 0);
	CHECK(profile_take_chips(&p, 10) == 0);
	profile_give_chips(&p, 321);
	profile_give_chips(&p, -5);
	CHECK(profile_save(&p, path));
	PlayerProfile q;
	CHECK(profile_load(&q, path) && q.chips == 321);
	remove(path);
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
	for (int v = GN_TUTE; v <= GN_TUTE_CABRON; v++)
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

static void test_as23(void)
{
	As23 g;
	as23_init(&g, 3, 1);
	/* Se cuenta as, dos... y si coincide se lleva el montón */
	CHECK(as23_called(&g) == 1);
	g.current = 0;
	g.stack_count[0] = 2;
	g.stack[0][0] = card_make(SUIT_OROS, 5);
	g.stack[0][1] = card_make(SUIT_OROS, 4);
	CHECK(as23_play(&g) && !g.last_match && as23_called(&g) == 2);
	g.current = 1;
	g.stack[1][g.stack_count[1] - 1] = card_make(SUIT_COPAS, 2);
	int before = g.stack_count[1];
	CHECK(as23_play(&g) && g.last_match && g.last_taken == 2);
	CHECK(g.stack_count[1] == before - 1 + 2 && g.pile_count == 0 && as23_called(&g) == 1);
	/* La carta de abajo del mazo es la primera que se echó */
	CHECK(card_equal(g.stack[1][1], card_make(SUIT_OROS, 4)));
	/* Echar la última carta sin coincidir gana */
	g.current = 0;
	g.stack[0][0] = card_make(SUIT_OROS, 7);
	CHECK(as23_play(&g) && as23_over(&g) && g.winner == 0);
	CHECK(!as23_play(&g));

	for (int players = 2; players <= 4; players++) {
		int wins[AS23_MAX_PLAYERS] = { 0 }, capped = 0;
		long plays = 0;
		for (int game = 0; game < 500; game++) {
			as23_init(&g, players, 7000 + game);
			int total = 0;
			for (int p = 0; p < players; p++)
				total += g.stack_count[p];
			CHECK(total == DECK_SIZE);
			while (!as23_over(&g)) {
				CHECK(as23_play(&g));
				total = g.pile_count;
				for (int p = 0; p < players; p++)
					total += g.stack_count[p];
				CHECK(total == DECK_SIZE);
			}
			CHECK(g.winner >= 0 && g.winner < players);
			if (g.plays >= AS23_MAX_PLAYS)
				capped++;
			else
				CHECK(g.stack_count[g.winner] == 0);
			wins[g.winner]++;
			plays += g.plays;
		}
		printf("  as, 2, 3 con %d: %ld cartas de media, %d alargadas, ganan %d/%d/%d/%d\n", players, plays / 500, capped,
			wins[0], wins[1], wins[2], wins[3]);
	}
}

static void test_pumba(void)
{
	Pumba g;
	pu_init(&g, 5);
	CHECK(g.hand_count[0] == PU_START_HAND && g.deck_count == DECK_SIZE - 4 * PU_START_HAND - 1);
	/* Los doses se acumulan y el que no tiene roba todos */
	g.current = 0;
	g.direction = 1;
	g.suit = SUIT_OROS;
	g.discard[g.discard_count - 1] = card_make(SUIT_OROS, 5);
	g.hand_count[0] = 2;
	g.hands[0][0] = card_make(SUIT_OROS, 2);
	g.hands[0][1] = card_make(SUIT_COPAS, 6);
	g.hand_count[1] = 2;
	g.hands[1][0] = card_make(SUIT_ESPADAS, 2);
	g.hands[1][1] = card_make(SUIT_ESPADAS, 6);
	g.hand_count[2] = 1;
	g.hands[2][0] = card_make(SUIT_BASTOS, 7);
	CHECK(pu_play(&g, 0) && g.pending == 2 && g.current == 1);
	CHECK(!pu_can_play(&g, 1, 1) && pu_can_play(&g, 1, 0));
	CHECK(pu_play(&g, 0) && g.pending == 4 && g.current == 2);
	CHECK(!pu_can_play(&g, 2, 0));
	CHECK(pu_draw(&g) && g.hand_count[2] == 5 && g.pending == 0 && g.current == 3);
	/* El caballo vale sobre cualquier carta y cambia el palo */
	g.hand_count[3] = 2;
	g.hands[3][0] = card_make(SUIT_COPAS, RANK_CABALLO);
	g.hands[3][1] = card_make(SUIT_COPAS, 4);
	g.suit = SUIT_ESPADAS;
	CHECK(pu_can_play(&g, 3, 0) && !pu_can_play(&g, 3, 1));
	CHECK(pu_play(&g, 0) && g.phase == PU_PHASE_CHOOSE_SUIT);
	CHECK(pu_choose_suit(&g, SUIT_BASTOS) && g.suit == SUIT_BASTOS && g.current == 0);
	/* La sota cambia el sentido y el as salta */
	g.hand_count[0] = 3;
	g.hands[0][0] = card_make(SUIT_BASTOS, RANK_SOTA);
	g.hands[0][1] = card_make(SUIT_BASTOS, 1);
	g.hands[0][2] = card_make(SUIT_BASTOS, 3);
	CHECK(pu_play(&g, 0) && g.direction == -1 && g.current == 3);
	g.current = 0;
	CHECK(pu_play(&g, 0) && g.current == 2);

	int wins[PU_PLAYERS] = { 0 };
	for (int game = 0; game < 200; game++) {
		pu_init(&g, 900 + game);
		int guard = 0;
		while (g.phase != PU_PHASE_GAME_OVER && guard++ < 100000) {
			if (g.phase == PU_PHASE_ROUND_OVER) {
				pu_new_round(&g);
				continue;
			}
			int total = g.deck_count + g.discard_count;
			for (int p = 0; p < PU_PLAYERS; p++)
				total += g.hand_count[p];
			CHECK(total == DECK_SIZE);
			int p = g.current;
			if (g.phase == PU_PHASE_CHOOSE_SUIT) {
				CHECK(pu_choose_suit(&g, pu_ai_suit(&g, p)));
				continue;
			}
			if (g.hand_count[p] == 2)
				pu_say(&g, p);
			int i = pu_ai_choose(&g, p);
			if (i >= 0)
				CHECK(pu_play(&g, i));
			else if (!g.drew)
				CHECK(pu_draw(&g));
			else
				CHECK(pu_pass(&g));
		}
		CHECK(g.phase == PU_PHASE_GAME_OVER);
		wins[pu_game_winner(&g)]++;
	}
	printf("  pumba: ganan %d/%d/%d/%d\n", wins[0], wins[1], wins[2], wins[3]);
}

static void test_julepe(void)
{
	Julepe g;
	ju_init(&g, 3);
	g.trump = SUIT_OROS;
	CHECK(ju_strength(card_make(SUIT_COPAS, 1)) > ju_strength(card_make(SUIT_COPAS, 3)));
	CHECK(ju_strength(card_make(SUIT_COPAS, 3)) > ju_strength(card_make(SUIT_COPAS, RANK_REY)));
	CHECK(ju_strength(card_make(SUIT_COPAS, RANK_SOTA)) > ju_strength(card_make(SUIT_COPAS, 7)));
	CHECK(ju_strength(card_make(SUIT_COPAS, 4)) > ju_strength(card_make(SUIT_COPAS, 2)));
	CHECK(ju_beats(&g, card_make(SUIT_OROS, 2), card_make(SUIT_COPAS, 1)));
	CHECK(!ju_beats(&g, card_make(SUIT_ESPADAS, 1), card_make(SUIT_COPAS, 2)));
	/* Si solo entra uno, quien reparte no puede pasar */
	ju_init(&g, 4);
	while (g.current != g.dealer)
		CHECK(ju_decide(&g, g.current == (g.dealer + 1) % JU_PLAYERS));
	CHECK(ju_must_play(&g, g.dealer) && !ju_decide(&g, false) && ju_decide(&g, true));
	CHECK(g.phase == JU_DISCARDING && ju_entered(&g) == 2);
	/* Si no entra nadie, el plato se queda */
	ju_init(&g, 5);
	int pot = g.pot;
	while (g.phase == JU_DECIDING)
		CHECK(ju_decide(&g, false));
	CHECK(g.void_hand && g.pot == pot && g.phase == JU_HAND_OVER);
	/* Asistir y montar; fallar si no se tiene el palo */
	ju_init(&g, 6);
	g.trump = SUIT_OROS;
	for (int p = 0; p < JU_PLAYERS; p++)
		g.playing[p] = true;
	g.phase = JU_PLAYING;
	g.leader = g.current = 0;
	g.played_in_trick = 0;
	g.hand_count[0] = 1;
	g.hands[0][0] = card_make(SUIT_COPAS, 5);
	CHECK(ju_play(&g, 0));
	g.hand_count[1] = 3;
	g.hands[1][0] = card_make(SUIT_COPAS, 4);
	g.hands[1][1] = card_make(SUIT_COPAS, 7);
	g.hands[1][2] = card_make(SUIT_OROS, 2);
	CHECK(!ju_can_play(&g, 1, 0) && ju_can_play(&g, 1, 1) && !ju_can_play(&g, 1, 2));
	CHECK(ju_play(&g, 1));
	g.hand_count[2] = 2;
	g.hands[2][0] = card_make(SUIT_ESPADAS, 1);
	g.hands[2][1] = card_make(SUIT_OROS, 4);
	CHECK(!ju_can_play(&g, 2, 0) && ju_can_play(&g, 2, 1));

	int wins[JU_PLAYERS] = { 0 }, julepes = 0, voids = 0;
	for (int game = 0; game < 300; game++) {
		ju_init(&g, 1200 + game);
		int guard = 0;
		while (g.phase != JU_GAME_OVER && guard++ < 10000) {
			int chips = g.pot;
			for (int p = 0; p < JU_PLAYERS; p++)
				chips += g.chips[p];
			CHECK(chips == JU_PLAYERS * JU_START);
			int p = g.current;
			switch (g.phase) {
			case JU_DECIDING:
				CHECK(ju_decide(&g, ju_must_play(&g, p) || ju_ai_enter(&g, p)));
				break;
			case JU_DISCARDING: {
				bool sel[JU_HAND];
				ju_ai_discard(&g, p, sel);
				CHECK(ju_discard(&g, sel));
				break;
			}
			case JU_PLAYING: {
				int i = ju_ai_choose(&g, p);
				CHECK(ju_can_play(&g, p, i));
				CHECK(ju_play(&g, i));
				break;
			}
			case JU_TRICK_DONE:
				CHECK(ju_collect(&g));
				if (g.phase >= JU_HAND_OVER) {
					int t = 0;
					for (int q = 0; q < JU_PLAYERS; q++) {
						t += g.tricks[q];
						julepes += g.julepe[q];
					}
					CHECK(t == JU_HAND);
				}
				break;
			case JU_HAND_OVER:
				voids += g.void_hand;
				ju_new_hand(&g);
				break;
			default:
				break;
			}
		}
		CHECK(g.phase == JU_GAME_OVER);
		wins[ju_winner(&g)]++;
	}
	printf("  julepe: ganan %d/%d/%d/%d, %d julepes, %d manos sin jugar\n", wins[0], wins[1], wins[2], wins[3],
		julepes, voids);
}

static void test_tute_subastado(void)
{
	Guinote g;
	gn_init(&g, GN_TUTE_SUBASTADO, 77);
	CHECK(g.phase == GN_PHASE_BIDDING && g.hand_count[0] == 10 && g.trump < 0);
	int first = g.current;
	CHECK(!gn_bid(&g, 60) && gn_bid(&g, 70) && g.bidder == first);
	CHECK(!gn_bid(&g, 70) && gn_bid(&g, 80));
	CHECK(gn_bid(&g, 0) && gn_bid(&g, 0)); /* pasan los otros dos */
	CHECK(g.current == first && gn_bid(&g, 0));
	CHECK(g.phase == GN_PHASE_CHOOSE_TRUMP && g.bidder == (first + 1) % GN_PLAYERS && g.bid == 80);
	CHECK(gn_choose_trump(&g, SUIT_COPAS) && g.trump == SUIT_COPAS && g.current == g.bidder);
	/* Si pasan tres, el último tiene que pujar */
	gn_init(&g, GN_TUTE_SUBASTADO, 78);
	CHECK(gn_bid(&g, 0) && gn_bid(&g, 0) && gn_bid(&g, 0));
	CHECK(gn_must_bid(&g) && !gn_bid(&g, 0) && gn_bid(&g, 70) && g.phase == GN_PHASE_CHOOSE_TRUMP);

	int wins[2] = { 0 }, made = 0, hands = 0;
	for (int game = 0; game < 100; game++) {
		gn_init(&g, GN_TUTE_SUBASTADO, 3100 + game);
		int guard = 0;
		while (g.phase != GN_PHASE_GAME_OVER && guard++ < 20000) {
			int p = g.current;
			switch (g.phase) {
			case GN_PHASE_BIDDING:
				CHECK(gn_bid(&g, gn_ai_bid(&g, p)));
				break;
			case GN_PHASE_CHOOSE_TRUMP:
				CHECK(gn_choose_trump(&g, gn_ai_trump(&g, p)));
				break;
			case GN_PHASE_PLAYING:
				if (gn_sing_tute(&g, p) || gn_cante(&g, p))
					break;
				CHECK(gn_play(&g, gn_ai_choose(&g, p)));
				break;
			case GN_PHASE_TRICK_DONE:
				CHECK(gn_collect(&g));
				if (g.phase == GN_PHASE_HAND_OVER || g.phase == GN_PHASE_GAME_OVER) {
					hands++;
					made += g.bid_made;
					if (!g.tute)
						CHECK(g.points[0] + g.points[1] >= 130);
				}
				break;
			case GN_PHASE_HAND_OVER:
				gn_new_hand(&g);
				break;
			default:
				break;
			}
		}
		CHECK(g.phase == GN_PHASE_GAME_OVER);
		wins[gn_game_winner(&g)]++;
	}
	printf("  tute subastado: ganan %d/%d, %d de %d subastas cumplidas\n", wins[0], wins[1], made, hands);
}

static void tr_set(Truco* g, int p, Card a, Card b, Card c)
{
	g->hand[p][0] = g->dealt[p][0] = a;
	g->hand[p][1] = g->dealt[p][1] = b;
	g->hand[p][2] = g->dealt[p][2] = c;
}

static void test_truco(void)
{
	/* Orden de las cartas */
	CHECK(tr_power(card_make(SUIT_ESPADAS, 1)) > tr_power(card_make(SUIT_BASTOS, 1)));
	CHECK(tr_power(card_make(SUIT_BASTOS, 1)) > tr_power(card_make(SUIT_ESPADAS, 7)));
	CHECK(tr_power(card_make(SUIT_ESPADAS, 7)) > tr_power(card_make(SUIT_OROS, 7)));
	CHECK(tr_power(card_make(SUIT_OROS, 7)) > tr_power(card_make(SUIT_COPAS, 3)));
	CHECK(tr_power(card_make(SUIT_COPAS, 3)) > tr_power(card_make(SUIT_COPAS, 2)));
	CHECK(tr_power(card_make(SUIT_COPAS, 2)) > tr_power(card_make(SUIT_OROS, 1)));
	CHECK(tr_power(card_make(SUIT_OROS, 1)) > tr_power(card_make(SUIT_OROS, RANK_REY)));
	CHECK(tr_power(card_make(SUIT_OROS, RANK_SOTA)) > tr_power(card_make(SUIT_COPAS, 7)));
	CHECK(tr_power(card_make(SUIT_COPAS, 7)) > tr_power(card_make(SUIT_COPAS, 6)));
	CHECK(tr_power(card_make(SUIT_COPAS, 5)) > tr_power(card_make(SUIT_COPAS, 4)));
	/* Envido */
	Card e1[3] = { card_make(SUIT_OROS, 7), card_make(SUIT_OROS, 6), card_make(SUIT_COPAS, 5) };
	CHECK(tr_envido(e1, 3) == 33);
	Card e2[3] = { card_make(SUIT_OROS, RANK_REY), card_make(SUIT_OROS, RANK_SOTA), card_make(SUIT_COPAS, 5) };
	CHECK(tr_envido(e2, 3) == 20);
	Card e3[3] = { card_make(SUIT_OROS, RANK_REY), card_make(SUIT_ESPADAS, 4), card_make(SUIT_COPAS, 6) };
	CHECK(tr_envido(e3, 3) == 6);

	Truco g;
	tr_init(&g, 1);
	g.mano = g.turn = g.baza_leader = 0;
	tr_set(&g, 0, card_make(SUIT_ESPADAS, 1), card_make(SUIT_OROS, 4), card_make(SUIT_OROS, 5));
	tr_set(&g, 1, card_make(SUIT_COPAS, 3), card_make(SUIT_COPAS, 7), card_make(SUIT_BASTOS, 6));
	/* Envido, real envido, quiero: 5 puntos para el que más tiene (29 contra 27) */
	CHECK(tr_act(&g, 0, TR_ACT_ENVIDO) && g.phase == TR_RESPOND && g.responder == 1);
	CHECK(tr_act(&g, 1, TR_ACT_REAL) && g.responder == 0);
	CHECK(!tr_can(&g, 0, TR_ACT_ENVIDO) && !tr_can(&g, 0, TR_ACT_REAL) && tr_can(&g, 0, TR_ACT_FALTA));
	CHECK(tr_act(&g, 0, TR_ACT_QUIERO));
	CHECK(g.envido_pts[0] == 29 && g.envido_pts[1] == 30 && g.envido_winner == 1 && g.score[1] == 5);
	CHECK(g.phase == TR_PLAY && g.turn == 0 && !tr_can(&g, 0, TR_ACT_ENVIDO));
	/* Truco, quiero; retruco solo puede cantarlo quien quiso */
	CHECK(tr_act(&g, 0, TR_ACT_TRUCO) && tr_act(&g, 1, TR_ACT_QUIERO) && g.truco_level == 2);
	CHECK(!tr_can(&g, 0, TR_ACT_TRUCO));
	CHECK(tr_play(&g, 0, 0)); /* as de espadas */
	CHECK(tr_can(&g, 1, TR_ACT_TRUCO) && tr_act(&g, 1, TR_ACT_TRUCO) && g.responder == 0);
	CHECK(tr_act(&g, 0, TR_ACT_TRUCO) && g.truco_call == 4 && g.truco_level == 3); /* vale cuatro */
	CHECK(tr_act(&g, 1, TR_ACT_NOQUIERO) && g.phase == TR_HAND_OVER && g.hand_winner == 0 && g.hand_points == 3);
	CHECK(g.score[0] == 3);

	/* Pardas: la primera parda la decide la segunda */
	tr_init(&g, 2);
	g.mano = g.turn = g.baza_leader = 1;
	tr_set(&g, 0, card_make(SUIT_OROS, 3), card_make(SUIT_OROS, 4), card_make(SUIT_ESPADAS, 7));
	tr_set(&g, 1, card_make(SUIT_COPAS, 3), card_make(SUIT_COPAS, 5), card_make(SUIT_BASTOS, 4));
	CHECK(tr_play(&g, 1, 0) && tr_play(&g, 0, 0) && g.baza_winner[0] == -1 && g.turn == 1);
	CHECK(tr_play(&g, 1, 1) && tr_play(&g, 0, 2) && g.phase == TR_HAND_OVER && g.hand_winner == 0);
	/* Gana la primera y empata la segunda: gana el de la primera */
	tr_init(&g, 3);
	g.mano = g.turn = g.baza_leader = 0;
	tr_set(&g, 0, card_make(SUIT_OROS, 3), card_make(SUIT_OROS, 4), card_make(SUIT_ESPADAS, 6));
	tr_set(&g, 1, card_make(SUIT_COPAS, 2), card_make(SUIT_COPAS, 4), card_make(SUIT_BASTOS, 5));
	CHECK(tr_play(&g, 0, 0) && tr_play(&g, 1, 0) && g.baza_winner[0] == 0);
	CHECK(tr_play(&g, 0, 1) && tr_play(&g, 1, 1) && g.phase == TR_HAND_OVER && g.hand_winner == 0);
	/* El envido está primero: se contesta al truco con el envido y luego se sigue con el truco */
	tr_init(&g, 4);
	g.mano = g.turn = g.baza_leader = 0;
	tr_set(&g, 0, card_make(SUIT_OROS, 3), card_make(SUIT_OROS, 4), card_make(SUIT_ESPADAS, 6));
	tr_set(&g, 1, card_make(SUIT_COPAS, 7), card_make(SUIT_COPAS, 6), card_make(SUIT_BASTOS, 5));
	CHECK(tr_act(&g, 0, TR_ACT_TRUCO) && tr_can(&g, 1, TR_ACT_ENVIDO) && tr_act(&g, 1, TR_ACT_ENVIDO));
	CHECK(tr_act(&g, 0, TR_ACT_NOQUIERO) && g.score[1] == 1);
	CHECK(g.phase == TR_RESPOND && g.pending == TR_ACT_TRUCO && g.responder == 1);
	CHECK(tr_act(&g, 1, TR_ACT_QUIERO) && g.truco_level == 2 && g.phase == TR_PLAY && g.turn == 0);
	/* Irse al mazo */
	CHECK(tr_act(&g, 0, TR_ACT_MAZO) && g.hand_winner == 1 && g.score[1] == 3);

	/* Truc valenciano: a 12, sin real envido y con joc fora */
	tr_init_variant(&g, true, 6);
	g.mano = g.turn = g.baza_leader = 0;
	CHECK(g.target == TR_GAME_TRUC && !tr_can(&g, 0, TR_ACT_REAL) && tr_can(&g, 0, TR_ACT_ENVIDO));
	CHECK(tr_act(&g, 0, TR_ACT_TRUCO) && tr_act(&g, 1, TR_ACT_TRUCO) && tr_act(&g, 0, TR_ACT_TRUCO));
	CHECK(g.truco_call == 4 && tr_can(&g, 1, TR_ACT_TRUCO) && tr_act(&g, 1, TR_ACT_TRUCO) && g.truco_call == 5);
	CHECK(!tr_can(&g, 0, TR_ACT_TRUCO) && tr_act(&g, 0, TR_ACT_QUIERO) && g.truco_level == 5);
	CHECK(tr_act(&g, 0, TR_ACT_MAZO) && g.phase == TR_GAME_OVER && tr_winner(&g) == 1 && g.score[1] == TR_GAME_TRUC);

	int wins[2] = { 0 }, hands = 0, envidos = 0, trucos = 0;
	for (int game = 0; game < 400; game++) {
		tr_init_variant(&g, game % 2 == 1, 5000 + game);
		int guard = 0;
		while (g.phase != TR_GAME_OVER && guard++ < 5000) {
			if (g.phase == TR_HAND_OVER) {
				hands++;
				envidos += g.envido_done;
				trucos += g.truco_level > 1;
				tr_new_hand(&g);
				continue;
			}
			int p = tr_actor(&g), c;
			TrAction a = tr_ai(&g, p, &c);
			if (a == TR_ACT_NONE)
				CHECK(tr_play(&g, p, c));
			else
				CHECK(tr_act(&g, p, a));
		}
		CHECK(g.phase == TR_GAME_OVER && tr_winner(&g) >= 0);
		wins[tr_winner(&g)]++;
	}
	printf("  truco: ganan %d/%d, %d manos, %d con envido, %d con truco querido\n", wins[0], wins[1], hands, envidos,
		trucos);
}

static void test_butifarra(void)
{
	Butifarra g;
	bu_init(&g, 9);
	int pts = 0;
	for (int p = 0; p < BU_PLAYERS; p++) {
		CHECK(g.hand_count[p] == BU_HAND);
		for (int i = 0; i < BU_HAND; i++)
			pts += bu_card_points(g.hands[p][i]);
	}
	CHECK(pts == 60);
	CHECK(bu_strength(card_make(SUIT_OROS, 9)) > bu_strength(card_make(SUIT_OROS, 1)));
	CHECK(bu_strength(card_make(SUIT_OROS, RANK_SOTA)) > bu_strength(card_make(SUIT_OROS, 8)));
	CHECK(bu_strength(card_make(SUIT_OROS, 8)) > bu_strength(card_make(SUIT_OROS, 7)));
	/* Delegar, elegir y contradas */
	int d = g.dealer;
	CHECK(g.phase == BU_CHOOSE && g.current == d && bu_choose(&g, BU_DELEGATE));
	CHECK(g.current == (d + 2) % 4 && !bu_choose(&g, BU_DELEGATE) && bu_choose(&g, BU_BUTIFARRA));
	CHECK(g.butifarra && g.trump < 0 && g.phase == BU_DOUBLE && bu_team(g.current) != bu_team(d));
	CHECK(bu_double(&g, true) && g.doubles == 1 && bu_team(g.current) == bu_team(d));
	CHECK(bu_double(&g, false) && g.phase == BU_PLAYING && bu_multiplier(&g) == 4);
	CHECK(g.current == (d + 1) % 4);
	/* Asistir y matar a los rivales; con el compañero ganando, libre */
	bu_init(&g, 10);
	bu_choose(&g, SUIT_OROS);
	while (g.phase == BU_DOUBLE)
		bu_double(&g, false);
	g.leader = g.current = 1;
	g.hand_count[1] = 1;
	g.hands[1][0] = card_make(SUIT_COPAS, 5);
	CHECK(bu_play(&g, 0));
	g.hand_count[2] = 3;
	g.hands[2][0] = card_make(SUIT_COPAS, 4);
	g.hands[2][1] = card_make(SUIT_COPAS, RANK_REY);
	g.hands[2][2] = card_make(SUIT_OROS, 2);
	CHECK(!bu_can_play(&g, 2, 0) && bu_can_play(&g, 2, 1) && !bu_can_play(&g, 2, 2));
	CHECK(bu_play(&g, 1));
	g.hand_count[3] = 2;
	g.hands[3][0] = card_make(SUIT_ESPADAS, 4);
	g.hands[3][1] = card_make(SUIT_OROS, 3);
	CHECK(!bu_can_play(&g, 3, 0) && bu_can_play(&g, 3, 1)); /* fallar a los rivales */
	CHECK(bu_play(&g, 1));
	g.hand_count[0] = 2;
	g.hands[0][0] = card_make(SUIT_ESPADAS, 6);
	g.hands[0][1] = card_make(SUIT_OROS, 9);
	CHECK(bu_can_play(&g, 0, 0) == false && bu_can_play(&g, 0, 1)); /* montar al fallo */

	int wins[2] = { 0 }, hands = 0, buti = 0, contros = 0;
	for (int game = 0; game < 200; game++) {
		bu_init(&g, 4400 + game);
		int guard = 0;
		while (g.phase != BU_GAME_OVER && guard++ < 20000) {
			int p = g.current;
			switch (g.phase) {
			case BU_CHOOSE:
				CHECK(bu_choose(&g, bu_ai_choose(&g, p)));
				break;
			case BU_DOUBLE:
				CHECK(bu_double(&g, bu_ai_double(&g, p)));
				break;
			case BU_PLAYING:
				CHECK(bu_play(&g, bu_ai_play(&g, p)));
				break;
			case BU_TRICK_DONE:
				CHECK(bu_collect(&g));
				if (g.phase >= BU_HAND_OVER) {
					CHECK(g.points[0] + g.points[1] == 72);
					hands++;
					buti += g.butifarra;
					contros += g.doubles > 0;
				}
				break;
			case BU_HAND_OVER:
				bu_new_hand(&g);
				break;
			default:
				break;
			}
		}
		CHECK(g.phase == BU_GAME_OVER);
		wins[bu_winner(&g)]++;
	}
	printf("  butifarra: ganan %d/%d, %d manos, %d butifarras, %d contradas\n", wins[0], wins[1], hands, buti, contros);
}

static void test_reversi(void)
{
	Reversi g;
	rv_init(&g, 1);
	CHECK(g.count[1] == 2 && g.count[2] == 2 && g.current == 1);
	CHECK(rv_flips(&g, 1, 2, 3) == 1 && rv_flips(&g, 1, 2, 2) == 0 && rv_flips(&g, 1, 3, 3) == 0);
	CHECK(rv_play(&g, 2, 3) && g.cell[3][3] == 1 && g.count[1] == 4 && g.count[2] == 1 && g.current == 2);
	CHECK(!rv_play(&g, 0, 0));
	int wins[4] = { 0 };
	for (int game = 0; game < 60; game++) {
		rv_init(&g, 700 + game);
		int guard = 0;
		while (!g.winner && guard++ < 200) {
			int before = g.count[1] + g.count[2];
			int m;
			if (g.current == 1) {
				g.level = 1; /* el negro juega flojo */
				m = rv_ai_choose(&g);
				g.level = 3;
			} else {
				m = rv_ai_choose(&g);
			}
			CHECK(m >= 0 && rv_play(&g, m / RV_SIZE, m % RV_SIZE));
			CHECK(g.count[1] + g.count[2] == before + 1);
		}
		CHECK(g.winner >= 1 && g.winner <= 3);
		wins[g.winner]++;
	}
	CHECK(wins[2] > wins[1]); /* la búsqueda más profunda gana más */
	printf("  reversi: negras %d, blancas %d, empates %d\n", wins[1], wins[2], wins[3]);
}

static void test_oca(void)
{
	CHECK(oca_square(5) == OCA_GOOSE && oca_square(59) == OCA_GOOSE && oca_square(6) == OCA_BRIDGE);
	CHECK(oca_square(31) == OCA_WELL && oca_square(58) == OCA_DEATH && oca_square(63) == OCA_GARDEN);
	Oca g;
	oca_init(&g, 3, 1);
	/* De oca a oca y tiro porque me toca */
	CHECK(oca_roll(&g, 4) == 4 && g.pos[0] == 9 && g.last_again && g.current == 0);
	/* Puente */
	g.pos[0] = 3;
	oca_roll(&g, 3);
	CHECK(g.pos[0] == 12 && g.current == 0);
	/* Posada: pierde un turno */
	g.pos[0] = 15;
	oca_roll(&g, 4);
	CHECK(g.pos[0] == 19 && g.skip[0] == 1 && g.current == 1);
	oca_roll(&g, 1);
	CHECK(g.current == 2);
	oca_roll(&g, 1);
	CHECK(g.current == 1 && g.skip[0] == 0);
	/* Pozo: hasta que otro caiga */
	g.pos[1] = 28;
	oca_roll(&g, 3);
	CHECK(g.in_well[1] && g.current == 2);
	g.pos[2] = 30;
	oca_roll(&g, 1);
	CHECK(g.in_well[2] && !g.in_well[1] && g.last_freed == 1);
	/* Muerte y rebote al final */
	g.current = 0;
	g.pos[0] = 55;
	oca_roll(&g, 3);
	CHECK(g.pos[0] == 1);
	g.current = 0;
	g.pos[0] = 60;
	oca_roll(&g, 5); /* 65: rebota a 61 */
	CHECK(g.pos[0] == 61 && g.winner < 0);
	g.current = 0;
	g.pos[0] = 61;
	oca_roll(&g, 2);
	CHECK(g.winner == 0);

	int wins[OCA_MAX_PLAYERS] = { 0 };
	long rolls = 0;
	for (int game = 0; game < 1000; game++) {
		oca_init(&g, 2 + game % 3, 800 + game);
		int guard = 0;
		while (g.winner < 0 && guard++ < 5000) {
			CHECK(!g.in_well[g.current]);
			oca_roll(&g, 0);
			rolls++;
		}
		CHECK(g.winner >= 0);
		wins[g.winner]++;
	}
	printf("  oca: %ld tiradas de media, ganan %d/%d/%d/%d\n", rolls / 1000, wins[0], wins[1], wins[2], wins[3]);
}

static void test_burro(void)
{
	Burro g;
	bur_init(&g, 3);
	CHECK(bur_holder(&g) < 0 && g.phase == BUR_PASSING);
	/* Pasar: cada uno recibe la del anterior */
	Card c3 = g.hands[3][2];
	int choice[BUR_PLAYERS] = { 2, 0, 0, 2 };
	CHECK(bur_pass(&g, choice) && card_equal(g.hands[0][2], c3));
	/* Poner la mano sin cuatro iguales: letra para él */
	CHECK(bur_slap(&g, 1) && g.round_loser == 1 && g.letters[1] == 1 && g.false_slap);
	/* Con cuatro iguales, el último en poner la mano se lleva la letra */
	bur_new_round(&g);
	for (int i = 0; i < BUR_HAND; i++)
		g.hands[2][i].rank = 5;
	CHECK(bur_slap(&g, 2) && g.phase == BUR_SLAP && bur_slap(&g, 0) && !bur_slap(&g, 0) && bur_slap(&g, 3));
	CHECK(g.phase == BUR_ROUND_OVER && g.round_loser == 1 && g.letters[1] == 2);

	int losers[BUR_PLAYERS] = { 0 }, max_passes = 0;
	long passes = 0, rounds = 0;
	for (int game = 0; game < 200; game++) {
		bur_init(&g, 900 + game);
		while (g.phase != BUR_GAME_OVER) {
			int guard = 0;
			while (bur_holder(&g) < 0 && guard++ < 500) {
				int ch[BUR_PLAYERS];
				for (int p = 0; p < BUR_PLAYERS; p++)
					ch[p] = bur_ai_pass(&g, p);
				CHECK(bur_pass(&g, ch));
			}
			CHECK(bur_holder(&g) >= 0);
			if (g.passes > max_passes)
				max_passes = g.passes;
			passes += g.passes;
			rounds++;
			/* Reaccionan en un orden cualquiera */
			int first = bur_holder(&g);
			bur_slap(&g, first);
			int step = game % 2 ? 1 : 3; /* el orden de los demás cambia */
			for (int k = 1; g.phase == BUR_SLAP && k < BUR_PLAYERS; k++)
				bur_slap(&g, (first + k * step) % BUR_PLAYERS);
			if (g.phase == BUR_ROUND_OVER)
				bur_new_round(&g);
		}
		losers[bur_loser(&g)]++;
	}
	printf("  burro: %ld pases por ronda (máximo %d), pierden %d/%d/%d/%d\n", passes / rounds, max_passes, losers[0],
		losers[1], losers[2], losers[3]);
}

static int mi_total(const Mierda* g)
{
	int n = g->deck_n + g->pile_n + g->burned;
	for (int p = 0; p < MI_PLAYERS; p++) {
		n += g->hand_n[p];
		for (int i = 0; i < 3; i++)
			n += g->up_has[p][i] + g->down_has[p][i];
	}
	return n;
}

static void test_mierda(void)
{
	Mierda g;
	mi_init(&g, 2);
	CHECK(mi_total(&g) == DECK_SIZE && g.deck_n == DECK_SIZE - 36);
	/* Orden y especiales */
	g.pile_n = 1;
	g.pile[0] = card_make(SUIT_OROS, RANK_REY);
	CHECK(!mi_card_ok(&g, card_make(SUIT_OROS, 7)) && mi_card_ok(&g, card_make(SUIT_COPAS, 1)));
	CHECK(mi_card_ok(&g, card_make(SUIT_COPAS, 2)) && mi_card_ok(&g, card_make(SUIT_COPAS, RANK_SOTA)));
	g.pile[0] = card_make(SUIT_OROS, 2);
	CHECK(mi_card_ok(&g, card_make(SUIT_OROS, 3)));
	/* Echar varias iguales y robar hasta tres */
	mi_init(&g, 3);
	g.pile_n = 0;
	g.current = 0;
	g.hand_n[0] = 3;
	g.hand[0][0] = card_make(SUIT_OROS, 5);
	g.hand[0][1] = card_make(SUIT_COPAS, 5);
	g.hand[0][2] = card_make(SUIT_OROS, 7);
	int deck = g.deck_n;
	CHECK(mi_play(&g, 1) && g.pile_n == 2 && g.last_count == 2 && g.hand_n[0] == 3 && g.deck_n == deck - 2);
	CHECK(g.current == 1);
	/* La sota quema y repite turno */
	g.hand_n[1] = 2;
	g.hand[1][0] = card_make(SUIT_OROS, RANK_SOTA);
	g.hand[1][1] = card_make(SUIT_OROS, 4);
	g.deck_n = 0;
	CHECK(mi_play(&g, 0) && g.last_burn && g.pile_n == 0 && g.current == 1);
	/* Sin carta que valga, se lleva el montón */
	g.pile[g.pile_n++] = card_make(SUIT_ESPADAS, 1);
	CHECK(mi_must_take(&g, 1) && !mi_play(&g, 0) && mi_take(&g) && g.hand_n[1] == 2 && g.pile_n == 0);

	int losers[MI_PLAYERS] = { 0 }, burns = 0;
	for (int game = 0; game < 300; game++) {
		mi_init(&g, 1500 + game);
		int guard = 0;
		while (g.phase != MI_GAME_OVER && guard++ < 3000) {
			int p = g.current;
			int i = mi_ai_choose(&g, p);
			if (i < 0)
				CHECK(mi_take(&g));
			else
				CHECK(mi_play(&g, i));
			burns += g.last_burn;
			CHECK(mi_total(&g) == DECK_SIZE);
		}
		CHECK(g.phase == MI_GAME_OVER && g.loser >= 0);
		losers[g.loser]++;
	}
	printf("  mierda: %d quemas, pierden %d/%d/%d/%d\n", burns, losers[0], losers[1], losers[2], losers[3]);
}

static void test_tresillo(void)
{
	Tresillo g;
	ts_init(&g, 3, 1);
	CHECK(g.hand_n[0] == 9 && g.talon_n == 13);
	/* Matadores con triunfo rojo (copas) y negro (bastos) */
	g.trump = SUIT_COPAS;
	CHECK(ts_power(&g, card_make(SUIT_ESPADAS, 1)) > ts_power(&g, card_make(SUIT_COPAS, 7)));
	CHECK(ts_power(&g, card_make(SUIT_COPAS, 7)) > ts_power(&g, card_make(SUIT_BASTOS, 1)));
	CHECK(ts_power(&g, card_make(SUIT_BASTOS, 1)) > ts_power(&g, card_make(SUIT_COPAS, 1)));
	CHECK(ts_is_matador(&g, card_make(SUIT_COPAS, 1)) && !ts_is_matador(&g, card_make(SUIT_COPAS, RANK_REY)));
	CHECK(ts_power(&g, card_make(SUIT_COPAS, 2)) > ts_power(&g, card_make(SUIT_COPAS, 3)));
	CHECK(ts_power(&g, card_make(SUIT_OROS, 1)) > ts_power(&g, card_make(SUIT_OROS, 2)));
	CHECK(ts_power(&g, card_make(SUIT_OROS, 2)) > ts_power(&g, card_make(SUIT_OROS, 7)));
	g.trump = SUIT_BASTOS;
	CHECK(ts_power(&g, card_make(SUIT_BASTOS, 2)) == 99 && ts_power(&g, card_make(SUIT_BASTOS, 1)) == 98);
	CHECK(ts_power(&g, card_make(SUIT_BASTOS, 7)) > ts_power(&g, card_make(SUIT_BASTOS, 3)));
	CHECK(ts_power(&g, card_make(SUIT_ESPADAS, 7)) > ts_power(&g, card_make(SUIT_ESPADAS, 2)));
	CHECK(ts_is_trump(&g, card_make(SUIT_ESPADAS, 1)));
	/* Subasta: hay que subir; la vuelta pinta con la primera del monte */
	ts_init(&g, 3, 2);
	int m = g.mano;
	CHECK(ts_bid(&g, TS_ENTRADA) && !ts_can_bid(&g, TS_ENTRADA) && ts_bid(&g, TS_VUELTA));
	CHECK(ts_bid(&g, TS_PASS) && g.current == m && ts_bid(&g, TS_PASS));
	CHECK(g.hombre == (m + 1) % 3 && g.bid == TS_VUELTA && g.phase == TS_DISCARD && g.current == g.hombre);
	CHECK(g.trump == g.talon[g.talon_n - 1].suit);
	/* Los matadores no salen ante triunfo más bajo */
	ts_init(&g, 3, 3);
	g.trump = SUIT_OROS;
	g.hombre = 0;
	g.phase = TS_PLAYING;
	g.leader = g.current = 0;
	g.hand_n[0] = 1;
	g.hands[0][0] = card_make(SUIT_OROS, 4);
	CHECK(ts_play(&g, 0));
	g.hand_n[1] = 2;
	g.hands[1][0] = card_make(SUIT_ESPADAS, 1);
	g.hands[1][1] = card_make(SUIT_COPAS, 5);
	CHECK(ts_can_play(&g, 1, 1)); /* la espadilla no está obligada */
	g.hands[1][1] = card_make(SUIT_OROS, RANK_REY);
	g.hands[1][0] = card_make(SUIT_COPAS, 3);
	CHECK(!ts_can_play(&g, 1, 0) && ts_can_play(&g, 1, 1));

	for (int players = 3; players <= 4; players++) {
		int wins[TS_MAX_PLAYERS] = { 0 }, results[4] = { 0 };
		for (int game = 0; game < 200; game++) {
			ts_init(&g, players, 2600 + game);
			int guard = 0;
			while (g.phase != TS_GAME_OVER && guard++ < 20000) {
				int p = g.current;
				switch (g.phase) {
				case TS_BIDDING:
					CHECK(ts_bid(&g, ts_ai_bid(&g, p)));
					break;
				case TS_TRUMP: {
					int t, k;
					ts_ai_choose(&g, p, &t, &k);
					CHECK(ts_choose(&g, t, k));
					break;
				}
				case TS_DISCARD: {
					bool sel[TS_MAX_HAND];
					ts_ai_discard(&g, p, sel);
					CHECK(ts_discard(&g, sel));
					break;
				}
				case TS_PLAYING:
					CHECK(ts_play(&g, ts_ai_play(&g, p)));
					break;
				case TS_TRICK_DONE:
					CHECK(ts_collect(&g));
					if (g.phase >= TS_HAND_OVER) {
						int t = 0, sum = 0;
						for (int q = 0; q < players; q++) {
							t += g.tricks[q];
							sum += g.hand_n[q];
						}
						CHECK(t == ts_hand_size(&g) && sum == 0);
						results[g.result]++;
					}
					break;
				case TS_HAND_OVER:
					if (g.result == TS_NADIE)
						results[TS_NADIE]++;
					ts_new_hand(&g);
					break;
				default:
					break;
				}
			}
			CHECK(g.phase == TS_GAME_OVER);
			wins[ts_winner(&g)]++;
		}
		printf("  %s: ganan %d/%d/%d/%d; sacadas %d, puestas %d, codillos %d, sin jugar %d\n",
			players == 3 ? "tresillo" : "cuatrillo", wins[0], wins[1], wins[2], wins[3], results[0], results[1],
			results[2], results[3]);
	}
}

static void test_bingo(void)
{
	Rng rng;
	rng_seed(&rng, 5);
	int col_count[BI_COLS][4] = { { 0 } };
	for (int i = 0; i < 20000; i++) {
		BiCard c;
		bi_make_card(&rng, &c);
		CHECK(bi_card_valid(&c));
		for (int k = 0; k < BI_COLS; k++) {
			int n = 0;
			for (int r = 0; r < BI_ROWS; r++)
				n += c.num[r][k] != 0;
			col_count[k][n]++;
		}
	}
	/* Hay columnas de uno, dos y tres números, y el 90 va en la última */
	CHECK(col_count[0][1] > 0 && col_count[0][2] > 0 && col_count[8][3] > 0);
	BiCard bad;
	memset(&bad, 0, sizeof bad);
	CHECK(!bi_card_valid(&bad));

	Bingo g;
	bi_init(&g, 9);
	CHECK(!bi_start(&g, 0) && !bi_start(&g, 5) && bi_start(&g, 2));
	CHECK(g.chips[0] == BI_START - 2 * BI_PRICE && g.card_n[0] == 2);
	int total = g.pot;
	for (int p = 0; p < BI_PLAYERS; p++)
		total += g.chips[p];
	CHECK(total == BI_PLAYERS * BI_START);
	/* No se puede cantar sin tener */
	CHECK(!bi_claim_line(&g, 0) && !bi_claim_bingo(&g, 0));
	/* Marcar solo lo que ha salido */
	int b = bi_draw(&g);
	CHECK(b >= 1 && b <= 90 && g.drawn[b] && bi_last(&g) == b);

	int lines = 0, line_at = 0, bingo_at = 0, games = 0;
	for (int game = 0; game < 500; game++) {
		bi_init(&g, 3000 + game);
		CHECK(bi_start(&g, 1 + game % 4));
		int pot = g.pot;
		while (g.phase == BI_PLAYING) {
			CHECK(bi_draw(&g) > 0);
			for (int p = 0; p < BI_PLAYERS; p++) {
				if (bi_has_line(&g, p)) {
					CHECK(bi_claim_line(&g, p));
					line_at += g.drawn_n;
					lines++;
				}
				if (g.phase == BI_PLAYING && bi_has_bingo(&g, p)) {
					CHECK(bi_claim_bingo(&g, p));
					bingo_at += g.drawn_n;
				}
			}
		}
		CHECK(g.line_winner >= 0 && g.bingo_winner >= 0);
		CHECK(g.line_prize + g.bingo_prize == pot);
		int chips = 0;
		for (int p = 0; p < BI_PLAYERS; p++)
			chips += g.chips[p];
		CHECK(chips == BI_PLAYERS * BI_START);
		games++;
	}
	printf("  bingo: línea a la bola %d, bingo a la bola %d de media\n", line_at / lines, bingo_at / games);
}

static void test_tresenraya(void)
{
	TresEnRaya g;
	tx_init(&g, 1);
	CHECK(g.current == 1 && tx_play(&g, 0) && !tx_play(&g, 0) && g.current == 2);
	tx_play(&g, 3);
	tx_play(&g, 1);
	tx_play(&g, 4);
	CHECK(tx_play(&g, 2) && g.winner == 1 && g.line[0] == 0 && g.line[2] == 2);
	CHECK(!tx_play(&g, 8));
	/* En difícil la CPU no pierde nunca, ni contra otra CPU al azar */
	int results[4] = { 0 };
	for (int game = 0; game < 400; game++) {
		tx_init(&g, 50 + game);
		g.level = 2;
		while (!g.winner) {
			int m;
			if (g.current == 1) {
				int free_cells[9], n = 0;
				for (int i = 0; i < 9; i++)
					if (!g.cell[i])
						free_cells[n++] = i;
				m = free_cells[rng_range(&g.rng, n)];
			} else {
				m = tx_ai_choose(&g);
			}
			CHECK(tx_play(&g, m));
		}
		results[g.winner]++;
	}
	CHECK(results[1] == 0);
	printf("  tres en raya: al azar %d, CPU %d, empates %d\n", results[1], results[2], results[3]);
}

static void test_flota(void)
{
	Flota g;
	for (int game = 0; game < 200; game++) {
		hf_init(&g, 70 + game);
		/* Barcos completos y sin tocarse */
		for (int p = 0; p < 2; p++) {
			int cells = 0;
			for (int r = 0; r < HF_SIZE; r++)
				for (int c = 0; c < HF_SIZE; c++) {
					int s = g.ship_at[p][r][c];
					if (s < 0)
						continue;
					cells++;
					for (int dr = -1; dr <= 1; dr++)
						for (int dc = -1; dc <= 1; dc++) {
							int a = r + dr, b = c + dc;
							if (a >= 0 && a < HF_SIZE && b >= 0 && b < HF_SIZE && g.ship_at[p][a][b] >= 0)
								CHECK(g.ship_at[p][a][b] == s);
						}
				}
			CHECK(cells == 5 + 4 + 3 + 3 + 2);
		}
	}
	/* Agua pasa el turno; tocado repite */
	hf_init(&g, 3);
	int wr = -1, wc = -1, sr = -1, sc = -1;
	for (int r = 0; r < HF_SIZE; r++)
		for (int c = 0; c < HF_SIZE; c++) {
			if (g.ship_at[1][r][c] < 0 && wr < 0) {
				wr = r;
				wc = c;
			}
			if (g.ship_at[1][r][c] >= 0 && sr < 0) {
				sr = r;
				sc = c;
			}
		}
	CHECK(hf_fire(&g, sr, sc) == HF_HIT && g.current == 0);
	CHECK(hf_fire(&g, sr, sc) == HF_NONE);
	CHECK(hf_fire(&g, wr, wc) == HF_WATER && g.current == 1);
	/* La IA siempre gana en menos de 100 disparos, y más rápido que al azar */
	long total = 0;
	int games = 300;
	for (int game = 0; game < games; game++) {
		hf_init(&g, 900 + game);
		g.current = 1;
		int guard = 0;
		while (g.winner < 0 && guard++ < 200) {
			if (g.current == 0) {
				/* El humano no dispara: se da agua al azar para pasar el turno */
				g.current = 1;
				continue;
			}
			int t = hf_ai_target(&g);
			CHECK(t >= 0 && hf_fire(&g, t / HF_SIZE, t % HF_SIZE) != HF_NONE);
		}
		CHECK(g.winner == 1);
		total += g.shots[1];
	}
	CHECK(total / games < 65);
	printf("  hundir la flota: la CPU necesita %ld disparos de media\n", total / games);
}

static void test_parchis(void)
{
	CHECK(pa_square(0, 0) == 5 && pa_square(0, 63) == 68 && pa_square(0, 64) == 0);
	CHECK(pa_square(1, 0) == 22 && pa_square(1, 63) == 17 && pa_square(3, 63) == 51);
	CHECK(pa_safe(12) && pa_safe(68) && !pa_safe(13));
	Parchis g;
	pa_init(&g, 1);
	/* Sin un 5 no se sale: pasa el turno */
	CHECK(pa_roll(&g, 3) == 3 && g.current == 1 && g.phase == PA_ROLL);
	g.current = 0;
	CHECK(pa_roll(&g, 5) && g.phase == PA_MOVE && pa_target(&g, 0) == 0);
	CHECK(pa_move(&g, 0) && g.pos[0][0] == 0 && g.current == 1);
	/* Comer: 20 de premio */
	g.current = 0;
	g.pos[1][0] = 8; /* azul en la 30: el amarillo a 2 pasos de la 7 */
	g.pos[0][0] = 0;
	int sq = pa_square(1, 8);
	g.pos[0][1] = sq - pa_exit(0) - 2; /* dos casillas por detrás */
	CHECK(pa_roll(&g, 2) && pa_target(&g, 1) == g.pos[0][1] + 2);
	CHECK(pa_move(&g, 1) && g.pos[1][0] == PA_HOME && g.last_captured_player == 1);
	CHECK(g.phase == PA_MOVE && g.move == 20 && g.current == 0);
	CHECK(pa_move(&g, 0) && g.pos[0][0] == 20);
	/* Barrera: no se puede pasar */
	pa_init(&g, 2);
	g.pos[0][0] = 10;
	g.pos[1][0] = g.pos[1][1] = (pa_square(0, 12) - pa_exit(1) + PA_RING) % PA_RING;
	CHECK(pa_square(1, g.pos[1][0]) == pa_square(0, 12));
	g.pos[0][1] = g.pos[0][2] = g.pos[0][3] = PA_GOAL;
	CHECK(pa_roll(&g, 4) && g.current != 0); /* la única ficha no puede pasar: pasa el turno */
	/* Meta: hay que sacar lo justo y se cuentan 10 */
	pa_init(&g, 3);
	g.pos[0][0] = PA_GOAL - 3;
	g.pos[0][1] = 0;
	CHECK(pa_roll(&g, 4) && pa_target(&g, 0) == -2 && pa_move(&g, 1));
	g.current = 0;
	CHECK(pa_roll(&g, 3) && pa_move(&g, 0) && g.pos[0][0] == PA_GOAL && g.move == 10 && g.phase == PA_MOVE);

	int wins[PA_PLAYERS] = { 0 };
	long turns = 0;
	for (int game = 0; game < 300; game++) {
		pa_init(&g, 4000 + game);
		int guard = 0;
		while (g.phase != PA_OVER && guard++ < 20000) {
			if (g.phase == PA_ROLL) {
				pa_roll(&g, 0);
				turns++;
			} else {
				int i = pa_ai_choose(&g);
				CHECK(i >= 0 && pa_move(&g, i));
			}
			/* Nunca más de dos fichas en una casilla */
			for (int s = 1; s <= PA_RING; s++) {
				int n = 0;
				for (int p = 0; p < PA_PLAYERS; p++)
					for (int i = 0; i < PA_PIECES; i++)
						n += pa_square(p, g.pos[p][i]) == s;
				CHECK(n <= 2);
			}
		}
		CHECK(g.phase == PA_OVER && g.winner >= 0 && pa_in_goal(&g, g.winner) == PA_PIECES);
		wins[g.winner]++;
	}
	printf("  parchís: %ld tiradas por partida, ganan %d/%d/%d/%d\n", turns / 300, wins[0], wins[1], wins[2], wins[3]);
}

static void test_freecell(void)
{
	FreeCell g;
	fc_new_game(&g, 1);
	/* La partida nº 1 de Windows empieza: JD 2D 9H JC 5D 7H 7C 5H */
	static const char* const want = "JD2D9HJC5D7H7C5H";
	const char* ranks = "A23456789TJQK";
	const char* suits = "SHDC";
	char got[17];
	for (int c = 0; c < FC_COLS; c++) {
		got[c * 2] = ranks[g.b.col[c][0].rank - 1];
		got[c * 2 + 1] = suits[g.b.col[c][0].suit];
	}
	got[16] = 0;
	CHECK(strcmp(got, want) == 0);
	CHECK(g.b.col_n[0] == 7 && g.b.col_n[7] == 6);
	/* Capacidad: (libres + 1) x 2^vacías */
	FcBoard b;
	memset(&b, 0, sizeof b);
	b.col_n[0] = 1;
	CHECK(fc_capacity(&b, 0) == 5 << 7 && fc_capacity(&b, 1) == 5 << 6);
	/* Mover a casilla libre, a montón y deshacer */
	fc_new_game(&g, 1);
	CHECK(fc_move(&g, 0, FC_CELL0) && g.b.cell[0].rank && g.b.col_n[0] == 6);
	CHECK(!fc_move(&g, 1, FC_CELL0));
	CHECK(fc_undo(&g) && g.b.col_n[0] == 7 && g.b.cell[0].rank == 0);
	/* Partidas al azar con movimientos legales: nunca se pierden cartas */
	Rng rng;
	rng_seed(&rng, 9);
	int won = 0;
	for (int game = 1; game <= 200; game++) {
		fc_new_game(&g, game);
		for (int step = 0; step < 400 && !g.won; step++) {
			fc_auto(&g);
			int from = rng_range(&rng, FC_FOUND0), to = rng_range(&rng, FC_PLACES);
			if (fc_can_move(&g.b, from, to))
				CHECK(fc_move(&g, from, to));
			int n = fc_cards_home(&g.b);
			for (int c = 0; c < FC_COLS; c++) {
				n += g.b.col_n[c];
				CHECK(g.b.col_n[c] <= FC_MAXCOL);
			}
			for (int i = 0; i < FC_CELLS; i++)
				n += g.b.cell[i].rank != 0;
			CHECK(n == 52);
		}
		won += g.won;
	}
	printf("  carta blanca: el reparto nº 1 coincide; %d/200 ganadas moviendo al azar\n", won);
}

static void test_toma6(void)
{
	CHECK(tm_heads(55) == 7 && tm_heads(33) == 5 && tm_heads(20) == 3 && tm_heads(15) == 2 && tm_heads(7) == 1);
	int total = 0;
	for (int c = 1; c <= TM_CARDS; c++)
		total += tm_heads(c);
	CHECK(total == 171);
	Toma6 g;
	tm_init(&g, 1);
	/* Filas preparadas: 10, 20, 30, 40 */
	for (int r = 0; r < TM_ROWS; r++) {
		g.row[r][0] = (uint8_t)(10 * (r + 1));
		g.row_n[r] = 1;
	}
	CHECK(tm_target_row(&g, 25) == 1 && tm_target_row(&g, 99) == 3 && tm_target_row(&g, 5) == -1);
	/* La sexta se lleva la fila */
	g.row_n[3] = 5;
	g.row[3][1] = 41;
	g.row[3][2] = 42;
	g.row[3][3] = 43;
	g.row[3][4] = 44;
	g.hand[0][0] = 50;
	g.hand[1][0] = 3;
	g.hand[2][0] = 11;
	g.hand[3][0] = 12;
	CHECK(tm_choose(&g, 0, 0) && tm_choose(&g, 1, 0) && tm_choose(&g, 2, 0) && g.phase == TM_CHOOSE);
	CHECK(tm_choose(&g, 3, 0) && g.phase == TM_PLACE);
	/* El 3 va primero y es más bajo que todo: elige fila */
	CHECK(tm_step(&g) && g.phase == TM_TAKE_ROW && g.chooser == 1);
	CHECK(tm_take_row(&g, 0) && g.score[1] == 3 && g.row[0][0] == 3 && g.row_n[0] == 1);
	CHECK(tm_step(&g) && g.row_n[0] == 2 && g.row[0][1] == 11); /* 11 detrás del 3 */
	CHECK(tm_step(&g) && g.row[0][2] == 12);
	int before = g.score[0];
	CHECK(tm_step(&g) && g.score[0] == before + 3 + 5 + 1 + 1 + 1 && g.row[3][0] == 50 && g.row_n[3] == 1);
	CHECK(g.phase == TM_CHOOSE);

	int wins[TM_PLAYERS] = { 0 }, rounds = 0;
	for (int game = 0; game < 300; game++) {
		tm_init(&g, 600 + game);
		int guard = 0;
		while (g.phase != TM_GAME_OVER && guard++ < 10000) {
			switch (g.phase) {
			case TM_CHOOSE:
				for (int p = 0; p < TM_PLAYERS; p++)
					CHECK(tm_choose(&g, p, p == 0 ? rng_range(&g.rng, g.hand_n[0]) : tm_ai_card(&g, p)));
				break;
			case TM_PLACE:
				CHECK(tm_step(&g));
				break;
			case TM_TAKE_ROW:
				CHECK(tm_take_row(&g, tm_ai_row(&g)));
				break;
			case TM_ROUND_OVER: {
				int cards = 0;
				for (int r = 0; r < TM_ROWS; r++) {
					cards += g.row_n[r];
					CHECK(g.row_n[r] >= 1 && g.row_n[r] <= TM_ROW_MAX);
				}
				rounds++;
				tm_new_round(&g);
				break;
			}
			default:
				break;
			}
		}
		CHECK(g.phase == TM_GAME_OVER);
		wins[tm_winner(&g)]++;
	}
	/* El que juega al azar (el 0) gana menos que la IA */
	CHECK(wins[0] < wins[1] && wins[0] < wins[2]);
	printf("  toma 6: %d rondas por partida; ganan azar %d, IA %d/%d/%d\n", rounds / 300, wins[0], wins[1], wins[2],
		wins[3]);
}

static void test_mahjong(void)
{
	CHECK(mj_match(MJ_FLOWERS, MJ_FLOWERS + 3) && !mj_match(MJ_FLOWERS, MJ_SEASONS) && mj_match(5, 5) && !mj_match(5, 6));
	/* Posiciones distintas y piso de abajo con 87 */
	int floor0 = 0;
	for (int t = 0; t < MJ_TILES; t++) {
		floor0 += MJ_LAYOUT[t].z == 0;
		for (int u = t + 1; u < MJ_TILES; u++)
			CHECK(!(MJ_LAYOUT[t].x == MJ_LAYOUT[u].x && MJ_LAYOUT[t].y == MJ_LAYOUT[u].y && MJ_LAYOUT[t].z == MJ_LAYOUT[u].z));
	}
	CHECK(floor0 == 87);
	Mahjong g;
	int solved = 0;
	for (int game = 0; game < 100; game++) {
		mj_init(&g, 50 + game);
		/* Cuatro de cada cara normal */
		int count[MJ_KINDS] = { 0 };
		for (int t = 0; t < MJ_TILES; t++)
			count[g.face[t]]++;
		for (int k = 0; k < MJ_FLOWERS; k++)
			CHECK(count[k] == 4);
		for (int k = MJ_FLOWERS; k < MJ_KINDS; k++)
			CHECK(count[k] == 1);
		/* Al principio solo están libres las de los extremos y la de arriba */
		int free_n = 0;
		for (int t = 0; t < MJ_TILES; t++)
			free_n += mj_free(&g, t);
		CHECK(free_n == 35); /* 14 abajo, 12 + 8 en los pisos 1 y 2 y la de arriba */
		/* Jugando a lo loco (la primera pareja que salga) y barajando cuando no
		 * hay movimiento, casi siempre se acaba */
		int guard = 0;
		while (g.left > 0 && guard++ < 200) {
			int a, b;
			if (mj_hint(&g, &a, &b))
				CHECK(mj_remove(&g, a, b));
			else if (!mj_shuffle(&g))
				break; /* fichas apiladas sin salida: no hay barajado que valga */
		}
		solved += g.left == 0;
	}
	CHECK(solved >= 80);
	/* Deshacer */
	mj_init(&g, 7);
	int a, b;
	CHECK(mj_hint(&g, &a, &b) && mj_remove(&g, a, b) && g.left == 142 && !mj_remove(&g, a, b));
	CHECK(mj_undo(&g) && g.left == 144 && g.present[a] && g.present[b]);
	printf("  mahjong: %d de 100 resueltas cogiendo la primera pareja\n", solved);
}

static void test_negocio(void)
{
	NgBoard b, c;
	ng_board_default(&b);
	c = b;
	ng_board_sanitize(&c);
	CHECK(memcmp(&b, &c, sizeof b) == 0); /* el de serie ya es válido */
	CHECK(b.sq[0].type == NG_START);
	/* Saneado de un tablero estropeado */
	memset(&c, 0xFF, sizeof c);
	ng_board_sanitize(&c);
	CHECK(c.sq[0].type == NG_START && c.title[0]);
	for (int i = 0; i < NG_SQUARES; i++) {
		CHECK(c.sq[i].type < NG_TYPES && c.sq[i].group < NG_GROUPS);
		if (c.sq[i].type == NG_STREET)
			CHECK(c.sq[i].price >= NG_PRICE_MIN && c.sq[i].price <= NG_PRICE_MAX && c.sq[i].price % 10 == 0);
		else
			CHECK(c.sq[i].price == 0);
		CHECK(strlen(c.sq[i].name) < NG_NAME_MAX);
	}
	/* Un nombre UTF-8 cortado a medias no deja un carácter roto */
	memset(&c, 0, sizeof c);
	ng_board_default(&c);
	memset(c.sq[1].name, 'a', NG_NAME_MAX);
	c.sq[1].name[NG_NAME_MAX - 2] = (char)0xC3; /* "ñ" sin su segundo byte al final */
	ng_board_sanitize(&c);
	CHECK(strlen(c.sq[1].name) == NG_NAME_MAX - 2);
	/* Alquileres */
	CHECK(ng_base_rent(200, 0, false) == 20 && ng_base_rent(200, 0, true) == 40);
	CHECK(ng_base_rent(200, 1, true) == 60 && ng_base_rent(200, 3, true) == 200);
	/* Guardar y leer tableros */
	NgBoard list[3], back[NG_MAX_BOARDS];
	for (int i = 0; i < 3; i++) {
		ng_board_default(&list[i]);
		snprintf(list[i].title, sizeof list[i].title, "Barrio %d", i);
		snprintf(list[i].sq[5].name, sizeof list[i].sq[5].name, "Calle de la Peña");
		list[i].sq[5].price = (uint16_t)(100 + i * 10);
	}
	const char* path = "test_tableros.dat";
	CHECK(ng_boards_save(list, 3, path));
	CHECK(ng_boards_load(back, NG_MAX_BOARDS, path) == 3);
	CHECK(strcmp(back[2].title, "Barrio 2") == 0 && back[1].sq[5].price == 110);
	CHECK(strcmp(back[0].sq[5].name, "Calle de la Peña") == 0);
	CHECK(ng_boards_load(back, 2, path) == 2);
	remove(path);
	CHECK(ng_boards_load(back, NG_MAX_BOARDS, path) == 0);

	/* Comprar, cobrar alquiler y pasar por la salida */
	Negocio g;
	ng_init(&g, &b, 2, 1);
	ng_roll(&g, 1, 3); /* 0 -> 4: Calle Larios (100), libre */
	CHECK(g.phase == NG_BUY && g.pos[0] == 4);
	ng_buy(&g, true);
	CHECK(g.owner[4] == 0 && g.money[0] == NG_START_MONEY - 100 && g.current == 1);
	ng_roll(&g, 1, 3); /* el rival cae en ella y paga 10 */
	CHECK(g.ev == NG_EV_RENT && g.ev_amount == 10 && g.money[1] == NG_START_MONEY - 10);
	CHECK(g.money[0] == NG_START_MONEY - 100 + 10);
	CHECK(g.phase == NG_ROLL && g.current == 0);
	/* Con dobles se repite */
	ng_roll(&g, 1, 1); /* 4 -> 6: Hacienda (10 %) */
	CHECK(g.ev == NG_EV_TAX && g.ev_amount == 140 && g.current == 0);
	ng_roll(&g, 6, 6); /* 6 -> 18: Calle Corrida, segundos dobles */
	CHECK(g.phase == NG_BUY);
	ng_buy(&g, false);
	CHECK(g.current == 0 && g.owner[18] == -1);
	ng_roll(&g, 6, 6); /* terceros dobles: 18 -> 30 = 2 (Suerte) y pasa por la salida */
	CHECK(g.ev_salary); /* ha pasado por la salida */
	CHECK(g.doubles == 0 || g.phase == NG_BUY); /* tras los terceros dobles ya no repite */

	/* Plantas: hacen falta todas las calles del color */
	ng_init(&g, &b, 2, 2);
	g.owner[1] = 0;
	CHECK(!ng_full_group(&g, 1) && !ng_can_build(&g, 0, 1));
	g.owner[3] = 0;
	CHECK(ng_full_group(&g, 1) && ng_rent(&g, 1) == 12);
	CHECK(ng_build(&g, 0, 1) && g.level[1] == 1 && g.money[0] == NG_START_MONEY - 30);
	CHECK(ng_rent(&g, 1) == 18);
	CHECK(!ng_build(&g, 1, 3)); /* no es suya */
	g.level[1] = NG_MAX_LEVEL;
	CHECK(!ng_can_build(&g, 0, 1));
	CHECK(ng_worth(&g, 0) == g.money[0] + 60 + 60 + 3 * 30);

	/* Bancarrota: se venden plantas y calles y, si no llega, se quiebra */
	ng_init(&g, &b, 3, 3);
	g.owner[27] = 1;
	g.owner[24] = 1;
	g.owner[25] = 1;
	g.level[27] = 3;
	g.money[0] = 50;
	g.owner[1] = 0;
	g.pos[0] = 25;
	ng_roll(&g, 1, 1); /* cae en La Concha con 3 plantas: 320 de alquiler */
	CHECK(g.ev == NG_EV_BANKRUPT && g.out[0] && g.owner[1] == -1);
	CHECK(g.money[1] == NG_START_MONEY + 50 + 30); /* el dinero y la calle vendida por la mitad */
	CHECK(g.phase == NG_OVER); /* si quiebras tú, se acaba */

	/* Partidas enteras entre la IA: terminan y nadie queda con dinero negativo */
	int over = 0;
	for (int seed = 0; seed < 300; seed++) {
		ng_init(&g, &b, 2 + seed % 3, (uint32_t)seed * 7919u);
		int steps = 0;
		while (g.phase != NG_OVER && steps < 20000) {
			if (g.phase == NG_BUY)
				ng_buy(&g, ng_ai_buy(&g));
			else {
				int sq;
				while ((sq = ng_ai_build(&g, g.current)) >= 0)
					CHECK(ng_build(&g, g.current, sq));
				ng_roll(&g, 0, 0);
			}
			for (int p = 0; p < g.players; p++)
				CHECK(g.money[p] >= 0);
			steps++;
		}
		over += g.phase == NG_OVER;
		CHECK(g.winner >= 0 && !g.out[g.winner]);
	}
	CHECK(over == 300);
	for (int k = 0; k < ng_cards(); k++)
		CHECK(ng_card_text(k)[0] != '\0');
}

static void test_chip_stacks(void)
{
	/* Póker: te sientas con las fichas que traes del monedero */
	Poker g;
	pk_init_stack(&g, PK_OMAHA, 77, 7);
	CHECK(g.chips[0] + g.invested[0] == 7);
	CHECK(g.chips[1] + g.invested[1] == PK_START);
	int total = 0;
	for (int p = 0; p < PK_PLAYERS; p++)
		total += g.chips[p] + g.invested[p];
	CHECK(total == 7 + 3 * PK_START);
	/* Cinquillo: lo puesto en el plato en cada ronda */
	Cinquillo c;
	cq_init(&c, 5);
	for (int p = 0; p < CQ_PLAYERS; p++)
		CHECK(c.paid[p] == CQ_ANTE && c.chips[p] == CQ_START_CHIPS - CQ_ANTE);
	int pot = 0;
	for (int p = 0; p < CQ_PLAYERS; p++)
		pot += c.paid[p];
	CHECK(pot == c.pot);
	for (int guard = 0; guard < 500 && c.phase == CQ_PHASE_PLAYING; guard++) {
		int i = cq_ai_choose(&c, c.current);
		if (i >= 0)
			cq_play(&c, i);
		else
			cq_pass(&c);
		if (c.phase == CQ_PHASE_PLAYING) {
			pot = 0;
			for (int p = 0; p < CQ_PLAYERS; p++)
				pot += c.paid[p];
			CHECK(pot == c.pot);
		}
	}
	CHECK(c.phase != CQ_PHASE_PLAYING);
}

static void test_poker_variants(void)
{
	Poker g;
	/* Omaha: con cuatro corazones en la mano y uno en la mesa NO hay color
	 * (hacen falta tres de la mesa) */
	pk_init_variant(&g, PK_OMAHA, 1);
	CHECK(g.hole_n == 4);
	g.hole[0][0] = (PkCard){ 14, 1 };
	g.hole[0][1] = (PkCard){ 13, 1 };
	g.hole[0][2] = (PkCard){ 12, 1 };
	g.hole[0][3] = (PkCard){ 11, 1 };
	g.board[0] = (PkCard){ 2, 1 };
	g.board[1] = (PkCard){ 7, 0 };
	g.board[2] = (PkCard){ 8, 2 };
	g.board[3] = (PkCard){ 9, 3 };
	g.board[4] = (PkCard){ 3, 0 };
	g.board_count = 5;
	CHECK(pk_category(pk_player_score(&g, 0)) < 5);
	/* Con tres corazones en la mesa, sí */
	g.board[1] = (PkCard){ 7, 1 };
	g.board[2] = (PkCard){ 8, 1 };
	CHECK(pk_category(pk_player_score(&g, 0)) == 5);
	/* Y un trío en la mesa con una pareja en la mano no es póker: es full */
	g.hole[0][0] = (PkCard){ 9, 0 };
	g.hole[0][1] = (PkCard){ 9, 1 };
	g.board[0] = (PkCard){ 9, 2 };
	g.board[1] = (PkCard){ 4, 3 };
	g.board[2] = (PkCard){ 4, 0 };
	g.board[3] = (PkCard){ 4, 1 };
	CHECK(pk_category(pk_player_score(&g, 0)) == 6);

	/* Descarte: cinco cartas, se cambian como mucho tres (cuatro con un as) */
	pk_init_variant(&g, PK_DRAW, 2);
	CHECK(g.hole_n == 5 && g.stage == PK_PREFLOP && g.board_count == 0);
	while (g.stage == PK_PREFLOP)
		CHECK(pk_act(&g, PK_CHECK_CALL, 0));
	CHECK(g.stage == PK_DISCARD && g.board_count == 0);
	bool four[5] = { true, true, true, true, false };
	g.hole[g.current][4] = (PkCard){ 5, 0 };
	CHECK(!pk_can_discard(&g, four));
	g.hole[g.current][4] = (PkCard){ 14, 0 };
	CHECK(pk_can_discard(&g, four));
	int first = g.current;
	CHECK(pk_discard(&g, four) && g.discards[first] == 4 && g.current != first);
	while (g.stage == PK_DISCARD) {
		bool sel[5];
		pk_ai_discard(&g, g.current, sel);
		CHECK(pk_can_discard(&g, sel) && pk_discard(&g, sel));
	}
	CHECK(g.stage == PK_RIVER);

	/* Partidas enteras de las dos variantes */
	for (int v = PK_OMAHA; v <= PK_DRAW; v++) {
		int finished = 0;
		for (int game = 0; game < 20; game++) {
			pk_init_variant(&g, (PkVariant)v, 300 + game);
			int guard = 0;
			while (!pk_game_over(&g) && guard++ < 4000) {
				if (pk_hand_finished(&g)) {
					int total = 0;
					for (int p = 0; p < PK_PLAYERS; p++)
						total += g.chips[p];
					CHECK(total == PK_PLAYERS * PK_START);
					pk_new_hand(&g);
					continue;
				}
				if (g.stage == PK_DISCARD) {
					bool sel[5];
					pk_ai_discard(&g, g.current, sel);
					CHECK(pk_discard(&g, sel));
					continue;
				}
				int amount = 0;
				PkAction a = pk_ai_decide(&g, &amount);
				CHECK(pk_act(&g, a, amount));
			}
			finished += pk_game_over(&g);
		}
		printf("  póker %s: %d/20 partidas acabadas\n", v == PK_OMAHA ? "Omaha" : "de descarte", finished);
	}
}

static void test_videopoker(void)
{
	PkCard royal[5] = { { 10, 1 }, { 11, 1 }, { 12, 1 }, { 13, 1 }, { 14, 1 } };
	PkCard sf[5] = { { 9, 1 }, { 10, 1 }, { 11, 1 }, { 12, 1 }, { 13, 1 } };
	PkCard jacks[5] = { { 11, 1 }, { 11, 2 }, { 3, 1 }, { 5, 0 }, { 9, 3 } };
	PkCard tens[5] = { { 10, 1 }, { 10, 2 }, { 3, 1 }, { 5, 0 }, { 9, 3 } };
	CHECK(vp_rank(royal) == VP_ROYAL && vp_rank(sf) == VP_STRAIGHT_FLUSH);
	CHECK(vp_rank(jacks) == VP_JACKS && vp_rank(tens) == VP_NOTHING);
	CHECK(vp_pay(VP_ROYAL, 5) == 4000 && vp_pay(VP_ROYAL, 4) == 1000 && vp_pay(VP_FULL, 2) == 18);
	VideoPoker g;
	vp_init(&g, 3);
	g.bet = 5;
	CHECK(vp_deal(&g) && g.credits == VP_START - 5 && !vp_deal(&g));
	CHECK(vp_toggle(&g, 0) && g.held[0]);
	PkCard kept = g.hand[0];
	CHECK(vp_draw(&g) && g.hand[0].rank == kept.rank && g.hand[0].suit == kept.suit && g.phase == VP_RESULT);
	/* Jugando con el consejo se recupera casi todo lo apostado (la máquina gana un poco) */
	long bet = 0, won = 0;
	vp_init(&g, 11);
	for (int k = 0; k < 600; k++) {
		g.credits = 1000;
		g.bet = 1;
		CHECK(vp_deal(&g));
		bool hold[5];
		vp_advice(&g, hold);
		for (int i = 0; i < 5; i++)
			g.held[i] = hold[i];
		CHECK(vp_draw(&g));
		bet++;
		won += g.win;
	}
	CHECK(won * 100 / bet > 70); /* 600 manos: mucha suerte de por medio */
	printf("  videopóker: con el consejo devuelve el %ld %% de lo apostado\n", won * 100 / bet);
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
	test_as23();
	test_videopoker();
	test_poker_variants();
	test_chip_stacks();
	test_negocio();
	test_mahjong();
	test_toma6();
	test_freecell();
	test_parchis();
	test_tresenraya();
	test_flota();
	test_bingo();
	test_tresillo();
	test_mierda();
	test_burro();
	test_reversi();
	test_oca();
	test_butifarra();
	test_truco();
	test_tute_subastado();
	test_pumba();
	test_julepe();
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
	test_profile_v3();
	test_profile_v4();
	test_mii_parts();
	test_cardback();
	test_profile_v1();
	test_profile_v2();
	test_rivals();
	printf("%d comprobaciones, %d fallos\n", checks, failures);
	return failures ? 1 : 0;
}
