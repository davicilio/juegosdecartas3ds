#ifndef GAMES_JULEPE_H
#define GAMES_JULEPE_H

#include <stdbool.h>

#include "core/cards.h"

/* Julepe para cuatro jugadores (el 0 es el humano), en sentido 0 -> 1 -> 2 -> 3.
 *
 * Quien reparte pone JU_ANTE fichas en el plato, da cinco cartas a cada uno
 * y vuelve la siguiente: es el triunfo. Empezando por su derecha, cada uno
 * dice si juega o pasa (quien reparte habla el último y, si solo ha entrado
 * uno, está obligado a jugar). Si no juegan al menos dos, el plato se queda
 * para la mano siguiente. Los que juegan pueden descartarse de las cartas
 * que quieran y robar otras tantas del mazo.
 *
 * Se juegan cinco bazas entre los que han entrado. Orden: as, tres, rey,
 * caballo, sota, 7, 6, 5, 4, 2. Hay que asistir al palo y montar si se
 * puede; quien no tiene del palo debe fallar con triunfo, montando si ya se
 * ha fallado. Cada baza cobra la quinta parte del plato. Quien juega y hace
 * menos de dos bazas lleva julepe: paga lo que había en el plato, que va al
 * plato de la mano siguiente.
 *
 * Se empieza con JU_START fichas. La partida acaba cuando alguien se queda
 * sin fichas o tras JU_MAX_HANDS manos; gana quien más fichas tiene. */

#define JU_PLAYERS   4
#define JU_HAND      5
#define JU_ANTE      5
#define JU_START     60
#define JU_MAX_HANDS 16

typedef enum {
	JU_DECIDING,   /* current dice si juega o pasa */
	JU_DISCARDING, /* current se descarta */
	JU_PLAYING,
	JU_TRICK_DONE,
	JU_HAND_OVER,
	JU_GAME_OVER
} JuPhase;

typedef struct {
	Deck deck;
	Card trump_card;
	int trump;
	int dealer;
	Card hands[JU_PLAYERS][JU_HAND];
	int hand_count[JU_PLAYERS];
	bool decided[JU_PLAYERS];
	bool playing[JU_PLAYERS];
	int discarded[JU_PLAYERS]; /* cuántas cambió (para la interfaz) */
	int current;
	Card trick[JU_PLAYERS];
	bool trick_played[JU_PLAYERS];
	int played_in_trick;
	int leader;
	int trick_winner;
	int tricks[JU_PLAYERS];
	int tricks_done;
	int pot;
	int chips[JU_PLAYERS];
	int change[JU_PLAYERS]; /* ganado (o perdido) en la última mano */
	bool julepe[JU_PLAYERS];
	bool void_hand;         /* no jugaron dos: el plato se queda */
	int hands_played;
	JuPhase phase;
	Rng rng;
} Julepe;

int ju_strength(Card c);
bool ju_beats(const Julepe* g, Card challenger, Card best);
void ju_init(Julepe* g, uint32_t seed);
void ju_new_game(Julepe* g);
void ju_new_hand(Julepe* g);
int ju_entered(const Julepe* g);
/* Quien reparte no puede pasar si solo ha entrado uno */
bool ju_must_play(const Julepe* g, int player);
bool ju_decide(Julepe* g, bool play);
/* El jugador actual cambia las cartas marcadas */
bool ju_discard(Julepe* g, const bool sel[JU_HAND]);
int ju_trick_leader_now(const Julepe* g);
bool ju_can_play(const Julepe* g, int player, int hand_index);
bool ju_play(Julepe* g, int hand_index);
bool ju_collect(Julepe* g);
int ju_winner(const Julepe* g);

bool ju_ai_enter(const Julepe* g, int player);
void ju_ai_discard(const Julepe* g, int player, bool sel[JU_HAND]);
int ju_ai_choose(const Julepe* g, int player);

#endif
