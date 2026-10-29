#ifndef GAMES_TRUCO_H
#define GAMES_TRUCO_H

#include <stdbool.h>

#include "core/cards.h"

/* Truco argentino mano a mano: tú (0) contra la CPU (1), sin flor.
 *
 * Tres cartas a cada uno; se juegan tres bazas y gana la mano quien gana
 * dos. Orden, de mayor a menor: as de espadas, as de bastos, 7 de espadas,
 * 7 de oros, treses, doses, ases de copas y oros, reyes, caballos, sotas,
 * 7 de copas y bastos, seises, cincos y cuatros. Si una baza es parda
 * (empate), gana la mano quien gane la otra; si la primera la ganó alguien y
 * luego hay parda, gana él; si son todas pardas, gana el mano.
 *
 * ENVIDO (solo en la primera baza, antes de echar tu carta, y mientras no se
 * haya querido el truco): con dos cartas del mismo palo valen 20 más sus
 * números (las figuras, 0); si no, tu carta más alta. "Envido" vale 2, "real
 * envido" 3 y "falta envido" lo que le falta al que va ganando para acabar.
 * Se puede subir (envido, envido, real envido, falta envido). Si no se
 * quiere, el que cantó se lleva lo de antes (al menos 1). En empate gana el
 * mano. Se puede contestar al truco con el envido: "el envido está primero".
 *
 * TRUCO: la mano vale 1; con "truco" 2, "retruco" 3 y "vale cuatro" 4. Quien
 * quiere es el único que puede subir después. Si no se quiere, el que cantó
 * se lleva lo que valía antes. Irse al mazo da la mano al rival.
 *
 * La partida es a TR_GAME puntos.
 *
 * TRUC VALENCIANO (tr_init_variant con valenciano = true): las mismas cartas,
 * bazas y envit, pero a TR_GAME_TRUC piedras, sin real envido (el segundo
 * envite es "torne") y con un cuarto nivel del truc: tras el "quatre val"
 * se puede cantar "joc fora", que se juega la partida entera. */

#define TR_GAME      15
#define TR_GAME_TRUC 12

typedef enum {
	TR_PLAY,     /* turn echa carta o canta */
	TR_RESPOND,  /* responder contesta a un canto */
	TR_HAND_OVER,
	TR_GAME_OVER
} TrPhase;

typedef enum {
	TR_ACT_NONE,
	TR_ACT_ENVIDO,
	TR_ACT_REAL,
	TR_ACT_FALTA,
	TR_ACT_TRUCO,    /* truco, retruco o vale cuatro según el nivel */
	TR_ACT_QUIERO,
	TR_ACT_NOQUIERO,
	TR_ACT_MAZO,
	TR_ACT_COUNT
} TrAction;

typedef struct {
	Card hand[2][3];
	bool used[2][3];
	Card dealt[2][3]; /* las tres del reparto, para el envido */
	Card table[3][2]; /* carta de cada uno en cada baza */
	bool on_table[3][2];
	int baza;         /* baza en juego (0..2) */
	int baza_winner[3]; /* 0, 1 o -1 parda */
	int baza_leader;
	int mano;
	int turn;
	TrPhase phase;
	/* Canto pendiente */
	int pending;      /* TR_ACT_ENVIDO (cadena de envido) o TR_ACT_TRUCO, o TR_ACT_NONE */
	int caller, responder;
	/* Truco */
	int truco_level;  /* valor de la mano: 1..4 */
	int truco_call;   /* nivel cantado y aún sin contestar */
	int truco_owner;  /* quién puede subir el truco (-1 cualquiera) */
	bool truco_suspended; /* el envido ha interrumpido un truco */
	int truco_caller_saved;
	/* Envido */
	bool envido_done;
	int envido_chain[4];
	int envido_count;
	int envido_pts[2];
	int envido_winner;  /* -1 si no se jugó o no se quiso */
	int envido_value;   /* puntos que se llevó alguien por el envido */
	int envido_taker;
	/* Resultado */
	int score[2];
	int hand_winner;
	int hand_points;
	bool folded;
	int last_actor;
	int last_action;
	int last_level; /* nivel del truco cantado (2..4) para la interfaz */
	bool valenciano;
	int target; /* puntos para ganar */
	Rng rng;
} Truco;

int tr_power(Card c);
int tr_envido(const Card* cards, int n);
void tr_init(Truco* g, uint32_t seed);
void tr_init_variant(Truco* g, bool valenciano, uint32_t seed);
/* Nivel más alto del truco (4, o 5 con el joc fora) y lo que vale cada nivel */
int tr_max_level(const Truco* g);
int tr_level_value(const Truco* g, int level);
void tr_new_game(Truco* g);
void tr_new_hand(Truco* g);
/* Quién tiene que hacer algo ahora */
int tr_actor(const Truco* g);
bool tr_can(const Truco* g, int player, TrAction a);
bool tr_act(Truco* g, int player, TrAction a);
bool tr_can_play(const Truco* g, int player, int idx);
bool tr_play(Truco* g, int player, int idx);
/* Nivel al que se subiría el truco ahora (2..4) */
int tr_next_truco(const Truco* g);
/* Valor de la falta envido ahora */
int tr_falta(const Truco* g);
int tr_winner(const Truco* g);

/* IA: devuelve la acción; si es TR_ACT_NONE, echa la carta *card */
TrAction tr_ai(const Truco* g, int player, int* card);

#endif
