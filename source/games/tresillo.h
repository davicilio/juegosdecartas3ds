#ifndef GAMES_TRESILLO_H
#define GAMES_TRESILLO_H

#include <stdbool.h>

#include "core/cards.h"

/* Tresillo (tres jugadores) y cuatrillo (cuatro). El 0 es el humano y se
 * juega en sentido 0 -> 1 -> 2 (-> 3).
 *
 * TRESILLO. Nueve cartas a cada uno; las trece que sobran son el monte. Se
 * puja por ser el "hombre", empezando por el mano: "entrada" (elige triunfo y
 * cambia cartas del monte), "vuelta" (el triunfo lo marca la primera carta del
 * monte) o "solo" (elige triunfo y no cambia). Hay que pujar más alto que lo
 * anterior; quien pasa ya no puja. Después cambian cartas los otros dos (los
 * contrarios) con lo que queda del monte.
 *
 * CUATRILLO. Diez cartas a cada uno y sin monte. El hombre puja "entrada"
 * (elige triunfo y llama un rey que no tenga: quien lo tenga es su compañero)
 * o "solo" (juega sin compañero).
 *
 * Los triunfos más altos son los matadores: la espadilla (as de espadas), la
 * manilla (el 2 del triunfo si es negro, el 7 si es rojo), el basto (as de
 * bastos) y, si el triunfo es rojo, el punto (su as). Siguen rey, caballo y
 * sota, y luego, en negro, 7, 6, 5, 4, 3 y, en rojo, 2, 3, 4, 5, 6. En los palos
 * que no son triunfo: rey, caballo, sota y, en negro, 7 ... 2; en rojo, as,
 * 2 ... 7. Hay que asistir al palo que sale (los matadores no están obligados
 * a salir ante un triunfo más bajo); quien no tiene puede echar cualquiera.
 *
 * Tresillo: el hombre gana ("sacada") si hace más bazas que cada contrario;
 * si un contrario hace más que él es "codillo", y si empata es "puesta".
 * Cuatrillo: el hombre (con su compañero) gana con 6 bazas, empata con 5 y
 * con menos es codillo. Se juegan TS_HANDS manos y gana quien más puntos
 * tiene. */

#define TS_MAX_PLAYERS 4
#define TS_MAX_HAND    10
#define TS_HANDS       12

enum { TS_PASS, TS_ENTRADA, TS_VUELTA, TS_SOLO };
enum { TS_SACADA, TS_PUESTA, TS_CODILLO, TS_NADIE };

typedef enum {
	TS_BIDDING,
	TS_TRUMP,     /* el hombre elige triunfo (y en el cuatrillo, el rey) */
	TS_DISCARD,   /* current cambia cartas */
	TS_PLAYING,
	TS_TRICK_DONE,
	TS_HAND_OVER,
	TS_GAME_OVER
} TsPhase;

typedef struct {
	int players;
	Card hands[TS_MAX_PLAYERS][TS_MAX_HAND];
	int hand_n[TS_MAX_PLAYERS];
	Card talon[DECK_SIZE];
	int talon_n;      /* se roba de talon[talon_n - 1] */
	int dealer, mano;
	int bid, hombre;
	bool passed[TS_MAX_PLAYERS];
	int trump;
	int partner;      /* cuatrillo: quién tiene el rey llamado (-1 ninguno) */
	int called_king;  /* palo del rey llamado, o -1 */
	bool discarded[TS_MAX_PLAYERS];
	int changed[TS_MAX_PLAYERS]; /* cuántas cambió cada uno */
	int current;
	Card trick[TS_MAX_PLAYERS];
	bool trick_played[TS_MAX_PLAYERS];
	int played_in_trick;
	int leader;
	int trick_winner;
	int tricks[TS_MAX_PLAYERS];
	int tricks_done;
	int result;       /* TS_SACADA... */
	int codillo_player;
	int gain[TS_MAX_PLAYERS];
	int score[TS_MAX_PLAYERS];
	int hands_played;
	int last_bidder;
	int last_bid;
	TsPhase phase;
	Rng rng;
} Tresillo;

bool ts_is_trump(const Tresillo* g, Card c);
bool ts_is_matador(const Tresillo* g, Card c);
int ts_power(const Tresillo* g, Card c);
bool ts_beats(const Tresillo* g, Card challenger, Card best, int lead_suit);
int ts_hand_size(const Tresillo* g);
int ts_side(const Tresillo* g, int player); /* 0 el hombre (y su compañero), 1 los contrarios */

void ts_init(Tresillo* g, int players, uint32_t seed);
void ts_new_game(Tresillo* g);
void ts_new_hand(Tresillo* g);
int ts_min_bid(const Tresillo* g);
bool ts_can_bid(const Tresillo* g, int bid);
bool ts_bid(Tresillo* g, int bid);
/* Elige triunfo (y, en el cuatrillo, el palo del rey que llama, o -1) */
bool ts_can_call(const Tresillo* g, int king_suit);
bool ts_choose(Tresillo* g, int trump, int king_suit);
bool ts_discard(Tresillo* g, const bool* sel);
int ts_trick_leader_now(const Tresillo* g);
bool ts_can_play(const Tresillo* g, int player, int idx);
bool ts_play(Tresillo* g, int idx);
bool ts_collect(Tresillo* g);
int ts_winner(const Tresillo* g);

int ts_ai_bid(const Tresillo* g, int player);
void ts_ai_choose(const Tresillo* g, int player, int* trump, int* king);
void ts_ai_discard(const Tresillo* g, int player, bool* sel);
int ts_ai_play(const Tresillo* g, int player);

#endif
