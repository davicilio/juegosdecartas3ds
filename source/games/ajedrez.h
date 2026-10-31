#ifndef GAMES_AJEDREZ_H
#define GAMES_AJEDREZ_H

#include <stdbool.h>
#include <stdint.h>

#include "core/rng.h"

/* Ajedrez con todas las reglas: enroque, captura al paso, coronación, jaque
 * mate, rey ahogado y tablas por material insuficiente, por la regla de los
 * 50 movimientos y por triple repetición. La IA busca con alfa-beta y
 * búsqueda de capturas, con cuatro niveles.
 *
 * Casillas 0..63: 0 = a1, 7 = h1, 56 = a8. Piezas: 1 peón, 2 caballo,
 * 3 alfil, 4 torre, 5 dama, 6 rey; positivas las blancas y negativas las
 * negras. */

enum { AJ_EMPTY, AJ_PAWN, AJ_KNIGHT, AJ_BISHOP, AJ_ROOK, AJ_QUEEN, AJ_KING };
enum { AJ_WK = 1, AJ_WQ = 2, AJ_BK = 4, AJ_BQ = 8 };

#define AJ_MAX_MOVES   256
#define AJ_MAX_HISTORY 1024

typedef enum {
	AJ_PLAYING,
	AJ_CHECKMATE,
	AJ_STALEMATE,
	AJ_DRAW_MATERIAL,
	AJ_DRAW_50,
	AJ_DRAW_REPETITION
} AjStatus;

typedef struct {
	uint8_t from, to;
	int8_t promo;  /* pieza de coronación (sin signo) o 0 */
	uint8_t flags; /* AJ_F_* */
} AjMove;

enum { AJ_F_CAPTURE = 1, AJ_F_EP = 2, AJ_F_CASTLE = 4, AJ_F_DOUBLE = 8 };

typedef struct {
	AjMove move;
	int8_t captured;
	uint8_t castling;
	int8_t ep;
	int halfmove;
	uint64_t hash;
} AjUndo;

typedef struct {
	int8_t sq[64];
	int side;       /* 1 blancas, -1 negras */
	uint8_t castling;
	int ep;         /* casilla de captura al paso o -1 */
	int halfmove;   /* para la regla de los 50 movimientos */
	int fullmove;
	uint64_t hash;
	AjUndo undo[AJ_MAX_HISTORY];
	int ply;        /* jugadas hechas */
	Rng rng;
} Ajedrez;

void aj_init(Ajedrez* g, uint32_t seed);
bool aj_from_fen(Ajedrez* g, const char* fen);
/* Jugadas legales del bando que mueve */
int aj_legal_moves(Ajedrez* g, AjMove* out);
bool aj_in_check(const Ajedrez* g, int side);
void aj_make(Ajedrez* g, AjMove m);
void aj_unmake(Ajedrez* g);
AjStatus aj_status(Ajedrez* g);
/* Busca la jugada legal de from a to (con coronación promo, o dama si 0) */
bool aj_find_move(Ajedrez* g, int from, int to, int promo, AjMove* out);
/* Notación algebraica en español (R, D, T, A, C) antes de hacer la jugada */
void aj_move_name(Ajedrez* g, AjMove m, char* buf, int size);
uint64_t aj_perft(Ajedrez* g, int depth);

/* IA. level: 0 principiante, 1 fácil, 2 medio, 3 difícil */
#define AJ_LEVELS 4
AjMove aj_search(Ajedrez* g, int level);
/* Valoración desde el punto de vista de las blancas (en centipeones) */
int aj_evaluate(const Ajedrez* g);

#endif
