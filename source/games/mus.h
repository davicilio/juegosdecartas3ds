#ifndef GAMES_MUS_H
#define GAMES_MUS_H

#include <stdbool.h>

#include "core/cards.h"

/* Mus a cuatro jugadores por parejas: el 0 (humano) va con el 2, contra el
 * 1 y el 3. Se juega "a ocho reyes": los treses cuentan como reyes y los
 * doses como ases. El turno va en sentido antihorario (0, 1, 2, 3) y es mano
 * el jugador siguiente al que reparte.
 *
 * Cada jugador recibe cuatro cartas. Primero se pide mus: si los cuatro
 * quieren, cada uno descarta las cartas que quiera (al menos una) y recibe
 * otras tantas; en cuanto alguien corta ("no hay mus") empiezan los lances:
 *
 *   Grande  gana la jugada más alta (rey, caballo, sota, 7 ... as).
 *   Chica   gana la más baja.
 *   Pares   pareja (1 piedra), medias (trío, 2) y duples (dos parejas, 3).
 *   Juego   31 o más sumando figuras a 10 (31 vale 3, el resto 2; orden
 *           31, 32, 40, 37, 36, 35, 34, 33). Si nadie tiene juego se juega
 *           al Punto: gana quien más sume.
 *
 * En cada lance se puede pasar, envidar, reenvidar, querer, no querer u
 * órdago. Los empates los gana quien esté más cerca de la mano. Lo que no
 * se quiere da el "deje" al que envidó (1 piedra o lo último que se quiso);
 * lo querido y los lances en paso se cuentan al final, junto con los tantos
 * de pares y juego de la pareja ganadora. Un órdago querido decide la
 * partida en ese lance. Gana la pareja que llegue a MUS_GAME_POINTS. */

#define MUS_PLAYERS     4
#define MUS_HAND_SIZE   4
#define MUS_GAME_POINTS 40
#define MUS_ORDAGO      (-1)

typedef enum {
	MUS_LANCE_GRANDE,
	MUS_LANCE_CHICA,
	MUS_LANCE_PARES,
	MUS_LANCE_JUEGO, /* se convierte en Punto si nadie tiene juego */
	MUS_NUM_LANCES
} MusLance;

typedef enum {
	MUS_PHASE_MUS,       /* ¿mus o no hay mus? */
	MUS_PHASE_DISCARD,   /* todos han pedido mus: cada uno descarta */
	MUS_PHASE_BET,       /* envites del lance actual */
	MUS_PHASE_HAND_OVER, /* se descubren las cartas y se cuenta */
	MUS_PHASE_GAME_OVER
} MusPhase;

typedef enum {
	MUS_ACT_NONE,
	MUS_ACT_MUS,
	MUS_ACT_NO_MUS,
	MUS_ACT_DISCARD,
	MUS_ACT_PASO,
	MUS_ACT_ENVIDO, /* envido o reenvido */
	MUS_ACT_ORDAGO,
	MUS_ACT_QUIERO,
	MUS_ACT_NO_QUIERO
} MusAction;

/* Señas entre compañeros: cada una es un gesto de la cara. */
typedef enum {
	MUS_SENA_NONE,
	MUS_SENA_REYES,  /* dos reyes o más: morderse el labio */
	MUS_SENA_ASES,   /* dos ases o más: sacar la punta de la lengua */
	MUS_SENA_MEDIAS, /* torcer la boca */
	MUS_SENA_DUPLES, /* levantar las cejas */
	MUS_SENA_31,     /* guiñar un ojo */
	MUS_SENA_CIEGO,  /* nada de nada: cerrar los ojos */
	MUS_NUM_SENAS
} MusSena;

typedef enum {
	MUS_RES_PENDING,
	MUS_RES_NONE,       /* nadie tiene (pares o juego): no se juega */
	MUS_RES_SINGLE,     /* solo una pareja tiene: cobra sin envites */
	MUS_RES_PASO,       /* todos pasan: se cuenta al final */
	MUS_RES_QUERIDO,
	MUS_RES_NO_QUERIDO, /* el que envidó cobra el deje */
	MUS_RES_ORDAGO      /* órdago querido: decide la partida */
} MusResultKind;

typedef struct {
	MusResultKind kind;
	int amount;      /* piedras queridas (o deje en no querido) */
	int deje_team;   /* pareja que cobró el deje (no querido) */
	int winner;      /* jugador ganador del lance a la vista de las cartas */
	int points[2];   /* piedras cobradas por cada pareja en este lance */
} MusLanceResult;

typedef struct {
	Deck deck;
	Card discards[DECK_SIZE];
	int discard_count;
	Card hands[MUS_PLAYERS][MUS_HAND_SIZE];
	int score[2];
	int dealer;
	int mano;
	int hands_played;
	MusPhase phase;
	int current;
	/* Mus */
	int mus_count;
	int mus_rounds;
	int discarded_now; /* jugadores que ya han descartado en esta vuelta */
	/* Envites del lance actual */
	int lance;
	bool punto;
	bool eligible[MUS_PLAYERS];
	int bet_accepted; /* piedras ya queridas en el lance */
	int bet_pending;  /* envite total propuesto (MUS_ORDAGO si es órdago) */
	int bet_team;     /* pareja que ha hecho el envite pendiente */
	int passes;
	MusLanceResult results[MUS_NUM_LANCES];
	int winner_team; /* pareja que gana la partida (-1 mientras se juega) */
	bool ordago_played;
	/* Última acción, para la interfaz */
	int last_player;
	MusAction last_action;
	int last_amount;
	int actions; /* contador de acciones, para detectar cambios */
	/* Última seña de cada jugador en esta mano (la ve su compañero) */
	MusSena sena[MUS_PLAYERS];
	Rng rng;
} Mus;

static inline int mus_team(int player)
{
	return player % 2;
}

/* Valor de la carta para grande y chica: rey (y tres) 10, caballo 9, sota 8,
 * del 7 al 4 su número, as (y dos) 1. */
int mus_rank(Card c);
/* Valor para el juego: figuras (y treses) 10, el resto su número (doses 1). */
int mus_card_points(Card c);
int mus_juego_sum(const Card* hand);
bool mus_has_juego(const Card* hand);
/* 0 sin pares, 1 pareja, 2 medias, 3 duples */
int mus_pares_kind(const Card* hand);
/* Piedras de tantos de pares o juego de una mano. */
int mus_pares_value(const Card* hand);
int mus_juego_value(const Card* hand);
/* > 0 si a gana a b en el lance (0 = empate). */
int mus_compare(int lance, bool punto, const Card* a, const Card* b);
/* Jugador que gana el lance entre los que pueden jugarlo (empates: la mano). */
int mus_lance_winner(const Mus* g, int lance);
/* Nombres para mostrar: "Grande", "Chica", "Pares", "Juego"/"Punto" */
const char* mus_lance_name(const Mus* g, int lance);
/* Descripción de la jugada de una mano para un lance: "Medias de reyes", "31"... */
void mus_describe(const Card* hand, int lance, bool punto, char* buf, int size);

void mus_init(Mus* g, uint32_t seed);
void mus_start_hand(Mus* g);
bool mus_say_mus(Mus* g, bool mus);
/* Descarta las cartas cuyo bit está en mask (al menos una). */
bool mus_discard(Mus* g, unsigned mask);
/* En el turno de envites: paso, envido (amount piedras más), órdago, quiero
 * o no quiero. */
bool mus_bet(Mus* g, MusAction action, int amount);
/* ¿Hay un envite pendiente de respuesta? */
bool mus_responding(const Mus* g);
bool mus_next_hand(Mus* g);

/* Señas */
MusSena mus_best_sena(const Card* hand);
const char* mus_sena_meaning(MusSena s);
const char* mus_sena_gesture(MusSena s);
/* El jugador hace una seña a su compañero (durante el mus o los lances). */
bool mus_make_sena(Mus* g, int player, MusSena s);
static inline int mus_partner(int player)
{
	return (player + 2) % MUS_PLAYERS;
}

/* IA */
bool mus_ai_wants_mus(const Mus* g, int player);
unsigned mus_ai_discard(const Mus* g, int player);
MusAction mus_ai_bet(const Mus* g, int player, int* amount);

#endif
