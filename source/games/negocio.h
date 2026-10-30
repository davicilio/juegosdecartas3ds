#ifndef GAMES_NEGOCIO_H
#define GAMES_NEGOCIO_H

#include <stdbool.h>
#include <stdint.h>

#include "core/rng.h"

/* Negocio redondo: juego de compraventa de calles para 2 a 4 jugadores
 * (0 es el humano) en un tablero de NG_SQUARES casillas que el jugador puede
 * personalizar: nombre, tipo, color y precio de cada una.
 *
 * - Cada uno empieza con NG_START_MONEY. Se tiran dos dados; con dobles se
 *   vuelve a tirar (como mucho dos veces seguidas).
 * - Al pasar por la Salida (o caer en ella) se cobran NG_SALARY.
 * - Calle libre: se puede comprar por su precio. Calle de otro: se paga el
 *   alquiler, que es un décimo del precio; el doble si el dueño tiene todas
 *   las de ese color, y con plantas, x3, x6 o x10.
 * - Con todas las calles de un color se pueden levantar plantas (hasta
 *   NG_MAX_LEVEL por calle), cada una por la mitad del precio de la calle.
 * - Suerte: una tarjeta de la suerte. Hacienda: se paga el 10 % del dinero
 *   (como poco NG_TAX_MIN). Obras: un turno sin tirar. Plaza: descanso.
 * - Quien no puede pagar vende plantas y calles al banco por la mitad de lo
 *   que costaron; si ni así llega, queda en bancarrota y sus calles vuelven
 *   al banco.
 * - Gana el último que quede o, tras NG_MAX_ROUNDS vueltas, el más rico
 *   (dinero, calles y plantas). */

#define NG_SQUARES     28
#define NG_PLAYERS     4
#define NG_NAME_MAX    25 /* bytes en UTF-8, con el cero final */
#define NG_GROUPS      8
#define NG_MAX_LEVEL   3
#define NG_START_MONEY 1500
#define NG_SALARY      200
#define NG_TAX_MIN     20
#define NG_PRICE_MIN   20
#define NG_PRICE_MAX   900
#define NG_MAX_ROUNDS  30
#define NG_MAX_BOARDS  6

typedef enum {
	NG_START,  /* la salida (siempre la casilla 0) */
	NG_STREET,
	NG_LUCK,
	NG_TAX,
	NG_WORKS,
	NG_REST,
	NG_TYPES
} NgType;

typedef struct {
	uint8_t type;  /* NgType */
	uint8_t group; /* color, para las calles */
	uint16_t price;
	char name[NG_NAME_MAX];
} NgSquare;

typedef struct {
	char title[NG_NAME_MAX];
	NgSquare sq[NG_SQUARES];
} NgBoard;

/* Tableros */
void ng_board_default(NgBoard* b);
/* Deja un tablero en un estado válido (por ejemplo, uno leído de la SD) */
void ng_board_sanitize(NgBoard* b);
/* Alquiler de una calle a la vista de cómo está la partida */
int ng_base_rent(int price, int level, bool full_group);
/* Lee y guarda los tableros propios. Devuelve cuántos hay (0 si no hay fichero). */
int ng_boards_load(NgBoard* boards, int max, const char* path);
bool ng_boards_save(const NgBoard* boards, int n, const char* path);

typedef enum {
	NG_ROLL,  /* le toca tirar a current */
	NG_BUY,   /* current ha caído en una calle libre y decide si la compra */
	NG_OVER
} NgPhase;

/* Lo último que ha pasado, para la interfaz */
typedef enum {
	NG_EV_NONE,
	NG_EV_MOVE,       /* ha caído en una casilla sin más */
	NG_EV_RENT,       /* ha pagado alquiler a ev_other */
	NG_EV_OWN,        /* ha caído en una calle suya */
	NG_EV_TAX,
	NG_EV_WORKS,
	NG_EV_LUCK,       /* ev_card: la tarjeta */
	NG_EV_SKIP,       /* ha perdido el turno por las obras */
	NG_EV_BANKRUPT
} NgEvent;

typedef struct {
	NgBoard board;
	int players;
	int pos[NG_PLAYERS];
	int money[NG_PLAYERS];
	bool out[NG_PLAYERS];
	int skip[NG_PLAYERS];   /* turnos sin tirar */
	int owner[NG_SQUARES];  /* -1 del banco */
	int level[NG_SQUARES];  /* plantas */
	int current;
	int doubles;            /* dobles seguidos en este turno */
	int die[2];
	int round;
	int winner;
	NgPhase phase;
	/* Último suceso */
	NgEvent ev;
	int ev_player, ev_other, ev_amount, ev_card, ev_square;
	bool ev_salary;         /* ha pasado por la salida */
	bool ev_bankrupt;       /* además ha quebrado */
	Rng rng;
} Negocio;

void ng_init(Negocio* g, const NgBoard* board, int players, uint32_t seed);
/* Tira los dados (d1/d2 a 0 = al azar) y mueve a current */
void ng_roll(Negocio* g, int d1, int d2);
/* Decisión de compra en NG_BUY */
void ng_buy(Negocio* g, bool buy);
/* El jugador tiene todas las calles del color de la casilla */
bool ng_full_group(const Negocio* g, int square);
int ng_rent(const Negocio* g, int square);
int ng_floor_cost(const Negocio* g, int square);
bool ng_can_build(const Negocio* g, int player, int square);
bool ng_build(Negocio* g, int player, int square);
/* Patrimonio: dinero, calles y plantas a precio de compra */
int ng_worth(const Negocio* g, int player);
int ng_streets_of(const Negocio* g, int player);
/* Texto de una tarjeta de la suerte */
const char* ng_card_text(int card);
int ng_cards(void);

/* IA */
bool ng_ai_buy(const Negocio* g);
/* Una planta que le conviene levantar a player, o -1 */
int ng_ai_build(const Negocio* g, int player);

#endif
