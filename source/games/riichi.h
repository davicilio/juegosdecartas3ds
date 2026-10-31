#ifndef GAMES_RIICHI_H
#define GAMES_RIICHI_H

#include <stdbool.h>
#include <stdint.h>

#include "core/rng.h"

/* Mahjong riichi (japonés) para cuatro jugadores: 0 es el humano y los
 * demás van en el orden de juego (1 a su derecha, 2 enfrente, 3 a su
 * izquierda). Se juega la ronda del Este (tonpuusen) con 25000 puntos.
 *
 * Fichas: 34 tipos, cuatro de cada. 0..8 caracteres (man) del 1 al 9,
 * 9..17 círculos (pin), 18..26 bambúes (sou), 27..30 vientos este, sur,
 * oeste y norte, y 31..33 dragones blanco, verde y rojo.
 *
 * - Cada uno roba y descarta. Con 4 combinaciones (escaleras o tríos) y una
 *   pareja (o siete parejas, o los trece huérfanos) se gana: tsumo con la
 *   ficha robada o ron con el descarte de otro. Hace falta al menos un yaku.
 * - Llamadas: pon (trío), chi (escalera, solo del jugador de la izquierda)
 *   y kan (cuatro iguales, que da una ficha de reposición y otro dora).
 * - Riichi: con la mano cerrada y a una ficha de ganar se apuestan 1000
 *   puntos; desde entonces se descarta lo que se roba.
 * - Furiten: no se puede ganar por ron con una ficha que tú mismo has
 *   descartado (ni, hasta tu siguiente descarte, con una que dejaste pasar).
 * - Puntos: han (yaku y dora) y fu, con la tabla de siempre: mangan,
 *   haneman, baiman, sanbaiman y yakuman.
 * - Si se acaba el muro, los que no están en tenpai pagan 3000 entre los
 *   que sí. El que reparte sigue si gana o está en tenpai. */

#define RC_TYPES    34
#define RC_TILES    136
#define RC_PLAYERS  4
#define RC_DEAD     14 /* muro muerto */
#define RC_MAX_HAND 14
#define RC_RIVER    32
#define RC_START    25000
#define RC_MAX_YAKU 16

/* Tipos de ficha */
#define RC_MAN   0
#define RC_PIN   9
#define RC_SOU   18
#define RC_EAST  27
#define RC_SOUTH 28
#define RC_WEST  29
#define RC_NORTH 30
#define RC_HAKU  31
#define RC_HATSU 32
#define RC_CHUN  33

typedef enum { RC_CHI, RC_PON, RC_KAN_OPEN, RC_KAN_CLOSED, RC_KAN_ADDED } RcMeldType;

typedef struct {
	uint8_t type;   /* RcMeldType */
	uint8_t tile;   /* la más baja (en un chi) o la repetida */
	uint8_t from;   /* jugador del que se tomó (el propio en un kan cerrado) */
	uint8_t called; /* ficha tomada */
} RcMeld;

typedef enum {
	RC_TURN,      /* current tiene la ficha robada y descarta (o tsumo, riichi, kan) */
	RC_CLAIM,     /* se acaba de descartar: los demás pueden llamar */
	RC_HAND_OVER, /* mano terminada: resultado en result */
	RC_GAME_OVER
} RcPhase;

/* Lo que puede hacer un jugador con el último descarte */
enum { RC_CAN_RON = 1, RC_CAN_PON = 2, RC_CAN_KAN = 4, RC_CAN_CHI = 8 };

typedef enum { RC_PASS, RC_DO_RON, RC_DO_PON, RC_DO_KAN, RC_DO_CHI } RcClaim;

typedef struct {
	const char* name;
	int han;
} RcYaku;

typedef struct {
	bool draw;                  /* empate por fin del muro */
	bool tenpai[RC_PLAYERS];    /* en un empate */
	int winner;                 /* -1 en un empate */
	int loser;                  /* quien dio el ron (-1 si tsumo) */
	bool tsumo;
	int han, fu, yakuman;       /* yakuman: cuántos (0 si no) */
	int dora, ura;
	RcYaku yaku[RC_MAX_YAKU];
	int yaku_n;
	int points;                 /* lo que cobra el ganador (sin palitos) */
	int delta[RC_PLAYERS];      /* cambio en los marcadores */
	const char* limit;          /* "Mangan", "Haneman"... o NULL */
	uint8_t hand[RC_MAX_HAND];  /* mano del ganador para enseñarla */
	int hand_n;
	int win_tile;
} RcResult;

typedef struct {
	uint8_t wall[RC_TILES];
	int wall_pos;               /* siguiente ficha del muro vivo */
	int live_end;               /* fin del muro vivo (se acorta con cada kan) */
	int rinshan_pos;            /* siguiente ficha de reposición */
	int dora_n;                 /* indicadores de dora a la vista */
	uint8_t hand[RC_PLAYERS][RC_MAX_HAND];
	int hand_n[RC_PLAYERS];
	RcMeld melds[RC_PLAYERS][4];
	int meld_n[RC_PLAYERS];
	uint8_t river[RC_PLAYERS][RC_RIVER];
	bool river_taken[RC_PLAYERS][RC_RIVER]; /* se la llevó otro con una llamada */
	int river_n[RC_PLAYERS];
	int riichi_at[RC_PLAYERS];  /* posición del descarte de riichi (-1 sin riichi) */
	bool ippatsu[RC_PLAYERS];
	bool skip_ron[RC_PLAYERS];  /* furiten temporal (o permanente con riichi) */
	int scores[RC_PLAYERS];
	int dealer;                 /* el que reparte en esta mano (viento este) */
	int hand_num;               /* 0..3: Este 1..4 */
	int honba, sticks;
	int current;
	int drawn;                  /* ficha robada en este turno (-1 tras una llamada) */
	bool rinshan;               /* el robo ha sido de reposición */
	int discard_tile, discarder;
	bool any_call;              /* ha habido llamadas (anula el ippatsu) */
	int kans;
	RcPhase phase;
	RcResult result;
	Rng rng;
} Riichi;

/* --- Utilidades de manos (también para la IA y los tests) --- */
void rc_tile_name(int t, char* buf, int size);
/* Siguiente ficha a la del indicador (la que es dora) */
int rc_dora_of(int indicator);
/* Shanten (0 = tenpai, -1 = mano completa) de 14-3m o 13-3m fichas con m combinaciones ya hechas */
int rc_shanten(const int counts[RC_TYPES], int melds);
/* Esperas de una mano en tenpai: rellena waits[t] y devuelve cuántas */
int rc_waits(const int counts[RC_TYPES], int melds, bool waits[RC_TYPES]);
bool rc_is_complete(const int counts[RC_TYPES], int melds);

/* Evalúa una mano ganadora: concealed tiene las fichas ocultas (con la
 * ganadora), melds las combinaciones a la vista. Devuelve false si no hay
 * yaku. Rellena han, fu, yaku y points (sin honba). */
typedef struct {
	bool tsumo, riichi, ippatsu, haitei, rinshan;
	int seat_wind, round_wind; /* tipos de ficha (RC_EAST...) */
	bool dealer;
	int dora_indicators[5], ura_indicators[5], dora_n;
} RcWinInfo;
bool rc_score_hand(const int concealed[RC_TYPES], const RcMeld* melds, int meld_n, int win_tile, const RcWinInfo* info,
	RcResult* out);
/* Puntos por han y fu: base y lo que paga cada uno */
int rc_base_points(int han, int fu, int yakuman, const char** limit);

/* --- Partida --- */
void rc_init(Riichi* g, uint32_t seed);
void rc_new_hand(Riichi* g);
int rc_seat_wind(const Riichi* g, int p); /* RC_EAST.. */
int rc_live_left(const Riichi* g);
void rc_counts(const Riichi* g, int p, int counts[RC_TYPES]);
bool rc_open(const Riichi* g, int p);

/* En RC_TURN */
bool rc_can_tsumo(const Riichi* g);
bool rc_tsumo(Riichi* g);
bool rc_can_riichi(const Riichi* g);
/* ¿Se puede descartar la ficha i declarando riichi (quedando en tenpai)? */
bool rc_riichi_discard_ok(const Riichi* g, int i);
/* Kan cerrado o añadido con la ficha de tipo t (-1 = el primero posible) */
int rc_kan_option(const Riichi* g);
bool rc_self_kan(Riichi* g, int t);
/* Descarta la ficha i de la mano (con riichi si se pide) */
bool rc_discard(Riichi* g, int i, bool riichi);

/* En RC_CLAIM */
int rc_claim_options(const Riichi* g, int p);
/* Opciones de chi: la ficha más baja de cada escalera posible */
int rc_chi_options(const Riichi* g, int p, int out[3]);
/* Resuelve las llamadas (claims[p] para cada jugador; chi_low para el chi) */
void rc_resolve(Riichi* g, const RcClaim claims[RC_PLAYERS], int chi_low);
/* Siguiente mano tras RC_HAND_OVER */
void rc_next(Riichi* g);

/* --- IA --- */
int rc_ai_discard(const Riichi* g, bool* riichi);
RcClaim rc_ai_claim(const Riichi* g, int p, int* chi_low);
bool rc_ai_tsumo(const Riichi* g);
int rc_ai_kan(const Riichi* g);

#endif
