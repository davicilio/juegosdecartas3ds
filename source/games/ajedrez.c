#include "games/ajedrez.h"

#include <stdio.h>
#include <string.h>

#define FILE_OF(s) ((s) & 7)
#define RANK_OF(s) ((s) >> 3)
#define SQ(f, r)   ((r) * 8 + (f))

static int sign(int x)
{
	return (x > 0) - (x < 0);
}

static int absv(int x)
{
	return x < 0 ? -x : x;
}

/* ---------------------------------------------------------------------------
 * Claves de Zobrist (para detectar repeticiones)
 * ------------------------------------------------------------------------- */

static uint64_t s_zpiece[13][64], s_zside, s_zcastle[16], s_zep[8];
static bool s_zinit;
/* Durante la búsqueda no se calcula la clave (no se miran repeticiones) */
static bool s_searching;

static uint64_t splitmix(uint64_t* x)
{
	uint64_t z = (*x += 0x9E3779B97F4A7C15ull);
	z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
	z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
	return z ^ (z >> 31);
}

static void zobrist_init(void)
{
	if (s_zinit)
		return;
	uint64_t x = 0xA1EDE2ull;
	for (int p = 0; p < 13; p++)
		for (int s = 0; s < 64; s++)
			s_zpiece[p][s] = splitmix(&x);
	s_zside = splitmix(&x);
	for (int i = 0; i < 16; i++)
		s_zcastle[i] = splitmix(&x);
	for (int i = 0; i < 8; i++)
		s_zep[i] = splitmix(&x);
	s_zinit = true;
}

static uint64_t compute_hash(const Ajedrez* g)
{
	uint64_t h = 0;
	for (int s = 0; s < 64; s++)
		if (g->sq[s])
			h ^= s_zpiece[g->sq[s] + 6][s];
	if (g->side < 0)
		h ^= s_zside;
	h ^= s_zcastle[g->castling & 15];
	if (g->ep >= 0)
		h ^= s_zep[FILE_OF(g->ep)];
	return h;
}

/* ---------------------------------------------------------------------------
 * Posición
 * ------------------------------------------------------------------------- */

bool aj_from_fen(Ajedrez* g, const char* fen)
{
	zobrist_init();
	Rng keep = g->rng;
	memset(g, 0, sizeof *g);
	g->rng = keep;
	int f = 0, r = 7;
	const char* p = fen;
	for (; *p && *p != ' '; p++) {
		if (*p == '/') {
			r--;
			f = 0;
		} else if (*p >= '1' && *p <= '8') {
			f += *p - '0';
		} else {
			static const char* const PIECES = "pnbrqk";
			const char* q = strchr(PIECES, *p >= 'a' ? *p : *p + 32);
			if (!q || f > 7 || r < 0)
				return false;
			int piece = (int)(q - PIECES) + 1;
			g->sq[SQ(f, r)] = (int8_t)(*p >= 'a' ? -piece : piece);
			f++;
		}
	}
	if (*p == ' ')
		p++;
	g->side = *p == 'b' ? -1 : 1;
	while (*p && *p != ' ')
		p++;
	if (*p == ' ')
		p++;
	g->castling = 0;
	for (; *p && *p != ' '; p++) {
		if (*p == 'K')
			g->castling |= AJ_WK;
		else if (*p == 'Q')
			g->castling |= AJ_WQ;
		else if (*p == 'k')
			g->castling |= AJ_BK;
		else if (*p == 'q')
			g->castling |= AJ_BQ;
	}
	if (*p == ' ')
		p++;
	g->ep = -1;
	if (*p >= 'a' && *p <= 'h' && p[1] >= '1' && p[1] <= '8') {
		g->ep = SQ(*p - 'a', p[1] - '1');
		p += 2;
	} else if (*p) {
		p++;
	}
	int half = 0, full = 1;
	if (sscanf(p, " %d %d", &half, &full) < 2) {
		half = 0;
		full = 1;
	}
	g->halfmove = half;
	g->fullmove = full;
	g->hash = compute_hash(g);
	return true;
}

void aj_init(Ajedrez* g, uint32_t seed)
{
	rng_seed(&g->rng, seed);
	aj_from_fen(g, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

/* ---------------------------------------------------------------------------
 * Ataques y jugadas
 * ------------------------------------------------------------------------- */

static const int KNIGHT_D[8][2] = { { 1, 2 }, { 2, 1 }, { 2, -1 }, { 1, -2 }, { -1, -2 }, { -2, -1 }, { -2, 1 }, { -1, 2 } };
static const int KING_D[8][2] = { { 1, 0 }, { 1, 1 }, { 0, 1 }, { -1, 1 }, { -1, 0 }, { -1, -1 }, { 0, -1 }, { 1, -1 } };
static const int DIAG_D[4][2] = { { 1, 1 }, { -1, 1 }, { -1, -1 }, { 1, -1 } };
static const int ORTH_D[4][2] = { { 1, 0 }, { 0, 1 }, { -1, 0 }, { 0, -1 } };

static bool on_board(int f, int r)
{
	return f >= 0 && f < 8 && r >= 0 && r < 8;
}

/* ¿Ataca el bando by a la casilla s? */
static bool attacked(const Ajedrez* g, int s, int by)
{
	int f = FILE_OF(s), r = RANK_OF(s);
	/* Peones */
	int pr = r - by;
	for (int df = -1; df <= 1; df += 2)
		if (on_board(f + df, pr) && g->sq[SQ(f + df, pr)] == by * AJ_PAWN)
			return true;
	for (int k = 0; k < 8; k++) {
		int nf = f + KNIGHT_D[k][0], nr = r + KNIGHT_D[k][1];
		if (on_board(nf, nr) && g->sq[SQ(nf, nr)] == by * AJ_KNIGHT)
			return true;
		nf = f + KING_D[k][0];
		nr = r + KING_D[k][1];
		if (on_board(nf, nr) && g->sq[SQ(nf, nr)] == by * AJ_KING)
			return true;
	}
	for (int k = 0; k < 4; k++) {
		for (int nf = f + DIAG_D[k][0], nr = r + DIAG_D[k][1]; on_board(nf, nr); nf += DIAG_D[k][0], nr += DIAG_D[k][1]) {
			int p = g->sq[SQ(nf, nr)];
			if (p) {
				if (p == by * AJ_BISHOP || p == by * AJ_QUEEN)
					return true;
				break;
			}
		}
		for (int nf = f + ORTH_D[k][0], nr = r + ORTH_D[k][1]; on_board(nf, nr); nf += ORTH_D[k][0], nr += ORTH_D[k][1]) {
			int p = g->sq[SQ(nf, nr)];
			if (p) {
				if (p == by * AJ_ROOK || p == by * AJ_QUEEN)
					return true;
				break;
			}
		}
	}
	return false;
}

static int king_square(const Ajedrez* g, int side)
{
	for (int s = 0; s < 64; s++)
		if (g->sq[s] == side * AJ_KING)
			return s;
	return -1;
}

bool aj_in_check(const Ajedrez* g, int side)
{
	int k = king_square(g, side);
	return k >= 0 && attacked(g, k, -side);
}

static void add(AjMove* out, int* n, int from, int to, int promo, int flags)
{
	out[*n].from = (uint8_t)from;
	out[*n].to = (uint8_t)to;
	out[*n].promo = (int8_t)promo;
	out[*n].flags = (uint8_t)flags;
	(*n)++;
}

static void add_pawn(AjMove* out, int* n, int from, int to, int flags, int side)
{
	int r = RANK_OF(to);
	if ((side > 0 && r == 7) || (side < 0 && r == 0)) {
		for (int p = AJ_QUEEN; p >= AJ_KNIGHT; p--)
			add(out, n, from, to, p, flags);
	} else {
		add(out, n, from, to, 0, flags);
	}
}

/* Jugadas pseudolegales (pueden dejar el rey en jaque). Con captures_only,
 * solo capturas y coronaciones. */
static int pseudo_moves(const Ajedrez* g, AjMove* out, bool captures_only)
{
	int n = 0, side = g->side;
	for (int s = 0; s < 64; s++) {
		int p = g->sq[s];
		if (sign(p) != side)
			continue;
		int f = FILE_OF(s), r = RANK_OF(s), kind = absv(p);
		if (kind == AJ_PAWN) {
			int nr = r + side;
			if (on_board(f, nr) && !g->sq[SQ(f, nr)]) {
				bool promo = nr == 0 || nr == 7;
				if (!captures_only || promo)
					add_pawn(out, &n, s, SQ(f, nr), 0, side);
				int start = side > 0 ? 1 : 6;
				if (!captures_only && r == start && !g->sq[SQ(f, nr + side)])
					add(out, &n, s, SQ(f, nr + side), 0, AJ_F_DOUBLE);
			}
			for (int df = -1; df <= 1; df += 2) {
				int nf = f + df;
				if (!on_board(nf, nr))
					continue;
				int t = SQ(nf, nr);
				if (sign(g->sq[t]) == -side)
					add_pawn(out, &n, s, t, AJ_F_CAPTURE, side);
				else if (t == g->ep)
					add(out, &n, s, t, 0, AJ_F_CAPTURE | AJ_F_EP);
			}
		} else if (kind == AJ_KNIGHT || kind == AJ_KING) {
			const int(*d)[2] = kind == AJ_KNIGHT ? KNIGHT_D : KING_D;
			for (int k = 0; k < 8; k++) {
				int nf = f + d[k][0], nr = r + d[k][1];
				if (!on_board(nf, nr))
					continue;
				int t = SQ(nf, nr), q = g->sq[t];
				if (sign(q) == side)
					continue;
				if (q)
					add(out, &n, s, t, 0, AJ_F_CAPTURE);
				else if (!captures_only)
					add(out, &n, s, t, 0, 0);
			}
			if (kind == AJ_KING && !captures_only) {
				int home = side > 0 ? 4 : 60;
				int ks = side > 0 ? AJ_WK : AJ_BK, qs = side > 0 ? AJ_WQ : AJ_BQ;
				if (s == home && !attacked(g, home, -side)) {
					if ((g->castling & ks) && !g->sq[home + 1] && !g->sq[home + 2] && g->sq[home + 3] == side * AJ_ROOK &&
						!attacked(g, home + 1, -side) && !attacked(g, home + 2, -side))
						add(out, &n, s, home + 2, 0, AJ_F_CASTLE);
					if ((g->castling & qs) && !g->sq[home - 1] && !g->sq[home - 2] && !g->sq[home - 3] &&
						g->sq[home - 4] == side * AJ_ROOK && !attacked(g, home - 1, -side) && !attacked(g, home - 2, -side))
						add(out, &n, s, home - 2, 0, AJ_F_CASTLE);
				}
			}
		} else {
			bool diag = kind == AJ_BISHOP || kind == AJ_QUEEN, orth = kind == AJ_ROOK || kind == AJ_QUEEN;
			for (int k = 0; k < 8; k++) {
				const int* d = k < 4 ? DIAG_D[k] : ORTH_D[k - 4];
				if ((k < 4 && !diag) || (k >= 4 && !orth))
					continue;
				for (int nf = f + d[0], nr = r + d[1]; on_board(nf, nr); nf += d[0], nr += d[1]) {
					int t = SQ(nf, nr), q = g->sq[t];
					if (sign(q) == side)
						break;
					if (q) {
						add(out, &n, s, t, 0, AJ_F_CAPTURE);
						break;
					}
					if (!captures_only)
						add(out, &n, s, t, 0, 0);
				}
			}
		}
	}
	return n;
}

static void update_castling(Ajedrez* g, int s)
{
	if (s == 4)
		g->castling &= (uint8_t)~(AJ_WK | AJ_WQ);
	else if (s == 0)
		g->castling &= (uint8_t)~AJ_WQ;
	else if (s == 7)
		g->castling &= (uint8_t)~AJ_WK;
	else if (s == 60)
		g->castling &= (uint8_t)~(AJ_BK | AJ_BQ);
	else if (s == 56)
		g->castling &= (uint8_t)~AJ_BQ;
	else if (s == 63)
		g->castling &= (uint8_t)~AJ_BK;
}

void aj_make(Ajedrez* g, AjMove m)
{
	if (g->ply >= AJ_MAX_HISTORY)
		return;
	AjUndo* u = &g->undo[g->ply++];
	u->move = m;
	u->castling = g->castling;
	u->ep = (int8_t)g->ep;
	u->halfmove = g->halfmove;
	u->hash = g->hash;
	int side = g->side, piece = g->sq[m.from];
	u->captured = g->sq[m.to];
	if (m.flags & AJ_F_EP) {
		int cap = m.to - 8 * side;
		u->captured = g->sq[cap];
		g->sq[cap] = 0;
	}
	g->sq[m.to] = (int8_t)(m.promo ? side * m.promo : piece);
	g->sq[m.from] = 0;
	if (m.flags & AJ_F_CASTLE) {
		bool king_side = m.to > m.from;
		int rook_from = king_side ? m.from + 3 : m.from - 4, rook_to = king_side ? m.from + 1 : m.from - 1;
		g->sq[rook_to] = g->sq[rook_from];
		g->sq[rook_from] = 0;
	}
	update_castling(g, m.from);
	update_castling(g, m.to);
	g->ep = (m.flags & AJ_F_DOUBLE) ? m.from + 8 * side : -1;
	g->halfmove = (absv(piece) == AJ_PAWN || u->captured) ? 0 : g->halfmove + 1;
	if (side < 0)
		g->fullmove++;
	g->side = -side;
	if (!s_searching)
		g->hash = compute_hash(g);
}

void aj_unmake(Ajedrez* g)
{
	if (g->ply <= 0)
		return;
	AjUndo* u = &g->undo[--g->ply];
	AjMove m = u->move;
	g->side = -g->side;
	int side = g->side;
	if (side < 0)
		g->fullmove--;
	int piece = m.promo ? side * AJ_PAWN : g->sq[m.to];
	g->sq[m.from] = (int8_t)piece;
	if (m.flags & AJ_F_EP) {
		g->sq[m.to] = 0;
		g->sq[m.to - 8 * side] = u->captured;
	} else {
		g->sq[m.to] = u->captured;
	}
	if (m.flags & AJ_F_CASTLE) {
		bool king_side = m.to > m.from;
		int rook_from = king_side ? m.from + 3 : m.from - 4, rook_to = king_side ? m.from + 1 : m.from - 1;
		g->sq[rook_from] = g->sq[rook_to];
		g->sq[rook_to] = 0;
	}
	g->castling = u->castling;
	g->ep = u->ep;
	g->halfmove = u->halfmove;
	g->hash = u->hash;
}

int aj_legal_moves(Ajedrez* g, AjMove* out)
{
	AjMove pseudo[AJ_MAX_MOVES];
	int n = pseudo_moves(g, pseudo, false), k = 0;
	int side = g->side;
	for (int i = 0; i < n; i++) {
		aj_make(g, pseudo[i]);
		if (!aj_in_check(g, side))
			out[k++] = pseudo[i];
		aj_unmake(g);
	}
	return k;
}

uint64_t aj_perft(Ajedrez* g, int depth)
{
	if (depth == 0)
		return 1;
	AjMove moves[AJ_MAX_MOVES];
	int n = aj_legal_moves(g, moves);
	if (depth == 1)
		return (uint64_t)n;
	uint64_t total = 0;
	for (int i = 0; i < n; i++) {
		aj_make(g, moves[i]);
		total += aj_perft(g, depth - 1);
		aj_unmake(g);
	}
	return total;
}

static bool insufficient(const Ajedrez* g)
{
	int minors = 0;
	for (int s = 0; s < 64; s++) {
		int k = absv(g->sq[s]);
		if (k == AJ_PAWN || k == AJ_ROOK || k == AJ_QUEEN)
			return false;
		if (k == AJ_KNIGHT || k == AJ_BISHOP)
			minors++;
	}
	return minors <= 1;
}

static int repetitions(const Ajedrez* g)
{
	int n = 0;
	for (int i = g->ply - 2; i >= 0 && i >= g->ply - g->halfmove; i -= 2)
		if (g->undo[i].hash == g->hash)
			n++;
	return n;
}

AjStatus aj_status(Ajedrez* g)
{
	AjMove moves[AJ_MAX_MOVES];
	if (aj_legal_moves(g, moves) == 0)
		return aj_in_check(g, g->side) ? AJ_CHECKMATE : AJ_STALEMATE;
	if (insufficient(g))
		return AJ_DRAW_MATERIAL;
	if (g->halfmove >= 100)
		return AJ_DRAW_50;
	if (repetitions(g) >= 2)
		return AJ_DRAW_REPETITION;
	return AJ_PLAYING;
}

bool aj_find_move(Ajedrez* g, int from, int to, int promo, AjMove* out)
{
	AjMove moves[AJ_MAX_MOVES];
	int n = aj_legal_moves(g, moves);
	for (int i = 0; i < n; i++)
		if (moves[i].from == from && moves[i].to == to && (moves[i].promo == 0 || moves[i].promo == (promo ? promo : AJ_QUEEN))) {
			*out = moves[i];
			return true;
		}
	return false;
}

void aj_move_name(Ajedrez* g, AjMove m, char* buf, int size)
{
	static const char LETTER[7] = { 0, 0, 'C', 'A', 'T', 'D', 'R' };
	char s[16];
	int k = 0, kind = absv(g->sq[m.from]);
	if (m.flags & AJ_F_CASTLE) {
		k = snprintf(s, sizeof s, m.to > m.from ? "O-O" : "O-O-O");
	} else {
		if (kind == AJ_PAWN) {
			if (m.flags & AJ_F_CAPTURE)
				s[k++] = (char)('a' + FILE_OF(m.from));
		} else {
			s[k++] = LETTER[kind];
			/* Si otra pieza igual puede ir a la misma casilla, se dice de dónde sale */
			AjMove moves[AJ_MAX_MOVES];
			int n = aj_legal_moves(g, moves);
			bool same_file = false, same_rank = false, other = false;
			for (int i = 0; i < n; i++)
				if (moves[i].to == m.to && moves[i].from != m.from && absv(g->sq[moves[i].from]) == kind) {
					other = true;
					same_file |= FILE_OF(moves[i].from) == FILE_OF(m.from);
					same_rank |= RANK_OF(moves[i].from) == RANK_OF(m.from);
				}
			if (other) {
				if (!same_file)
					s[k++] = (char)('a' + FILE_OF(m.from));
				else if (!same_rank)
					s[k++] = (char)('1' + RANK_OF(m.from));
				else {
					s[k++] = (char)('a' + FILE_OF(m.from));
					s[k++] = (char)('1' + RANK_OF(m.from));
				}
			}
		}
		if (m.flags & AJ_F_CAPTURE)
			s[k++] = 'x';
		s[k++] = (char)('a' + FILE_OF(m.to));
		s[k++] = (char)('1' + RANK_OF(m.to));
		if (m.promo) {
			s[k++] = '=';
			s[k++] = LETTER[(int)m.promo];
		}
		s[k] = '\0';
	}
	aj_make(g, m);
	AjStatus st = aj_status(g);
	bool check = aj_in_check(g, g->side);
	aj_unmake(g);
	if (st == AJ_CHECKMATE)
		k = (int)strlen(s), s[k++] = '#', s[k] = '\0';
	else if (check)
		k = (int)strlen(s), s[k++] = '+', s[k] = '\0';
	snprintf(buf, (size_t)size, "%s", s);
}

/* ---------------------------------------------------------------------------
 * Valoración
 * ------------------------------------------------------------------------- */

static const int VALUE[7] = { 0, 100, 320, 330, 500, 900, 0 };

/* Tablas de posición desde el punto de vista de las blancas, con la fila 8
 * arriba (como se ve el tablero) */
static const int8_t PST[7][64] = {
	{ 0 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 50, 50, 50, 50, 50, 50, 50, 50, 10, 10, 20, 30, 30, 20, 10, 10, 5, 5, 10, 25, 25, 10, 5, 5,
		0, 0, 0, 20, 20, 0, 0, 0, 5, -5, -10, 0, 0, -10, -5, 5, 5, 10, 10, -20, -20, 10, 10, 5, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ -50, -40, -30, -30, -30, -30, -40, -50, -40, -20, 0, 0, 0, 0, -20, -40, -30, 0, 10, 15, 15, 10, 0, -30, -30, 5, 15,
		20, 20, 15, 5, -30, -30, 0, 15, 20, 20, 15, 0, -30, -30, 5, 10, 15, 15, 10, 5, -30, -40, -20, 0, 5, 5, 0, -20,
		-40, -50, -40, -30, -30, -30, -30, -40, -50 },
	{ -20, -10, -10, -10, -10, -10, -10, -20, -10, 0, 0, 0, 0, 0, 0, -10, -10, 0, 5, 10, 10, 5, 0, -10, -10, 5, 5, 10, 10,
		5, 5, -10, -10, 0, 10, 10, 10, 10, 0, -10, -10, 10, 10, 10, 10, 10, 10, -10, -10, 5, 0, 0, 0, 0, 5, -10, -20, -10,
		-10, -10, -10, -10, -10, -20 },
	{ 0, 0, 0, 0, 0, 0, 0, 0, 5, 10, 10, 10, 10, 10, 10, 5, -5, 0, 0, 0, 0, 0, 0, -5, -5, 0, 0, 0, 0, 0, 0, -5, -5, 0, 0,
		0, 0, 0, 0, -5, -5, 0, 0, 0, 0, 0, 0, -5, -5, 0, 0, 0, 0, 0, 0, -5, 0, 0, 0, 5, 5, 0, 0, 0 },
	{ -20, -10, -10, -5, -5, -10, -10, -20, -10, 0, 0, 0, 0, 0, 0, -10, -10, 0, 5, 5, 5, 5, 0, -10, -5, 0, 5, 5, 5, 5, 0,
		-5, 0, 0, 5, 5, 5, 5, 0, -5, -10, 5, 5, 5, 5, 5, 0, -10, -10, 0, 5, 0, 0, 0, 0, -10, -20, -10, -10, -5, -5, -10,
		-10, -20 },
	{ -30, -40, -40, -50, -50, -40, -40, -30, -30, -40, -40, -50, -50, -40, -40, -30, -30, -40, -40, -50, -50, -40, -40,
		-30, -30, -40, -40, -50, -50, -40, -40, -30, -20, -30, -30, -40, -40, -30, -30, -20, -10, -20, -20, -20, -20, -20,
		-20, -10, 20, 20, 0, 0, 0, 0, 20, 20, 20, 30, 10, 0, 0, 10, 30, 20 },
};

/* Rey en el final: al centro */
static const int8_t KING_END[64] = { -50, -40, -30, -20, -20, -30, -40, -50, -30, -20, -10, 0, 0, -10, -20, -30, -30,
	-10, 20, 30, 30, 20, -10, -30, -30, -10, 30, 40, 40, 30, -10, -30, -30, -10, 30, 40, 40, 30, -10, -30, -30, -10, 20,
	30, 30, 20, -10, -30, -30, -30, 0, 0, 0, 0, -30, -30, -50, -30, -30, -30, -30, -30, -30, -50 };

int aj_evaluate(const Ajedrez* g)
{
	int score = 0, material = 0;
	for (int s = 0; s < 64; s++) {
		int k = absv(g->sq[s]);
		if (k != AJ_PAWN && k != AJ_KING)
			material += VALUE[k];
	}
	bool endgame = material <= 2600;
	for (int s = 0; s < 64; s++) {
		int p = g->sq[s];
		if (!p)
			continue;
		int k = absv(p);
		/* Índice en la tabla: las blancas la leen con la fila 8 arriba */
		int idx = p > 0 ? (7 - RANK_OF(s)) * 8 + FILE_OF(s) : RANK_OF(s) * 8 + FILE_OF(s);
		int v = VALUE[k] + (k == AJ_KING && endgame ? KING_END[idx] : PST[k][idx]);
		score += p > 0 ? v : -v;
	}
	return score;
}

/* ---------------------------------------------------------------------------
 * Búsqueda
 * ------------------------------------------------------------------------- */

#define INF  1000000
#define MATE 100000

static long s_nodes, s_node_limit;
static bool s_abort;

static int order_score(const Ajedrez* g, AjMove m)
{
	int sc = 0;
	if (m.flags & AJ_F_CAPTURE) {
		int victim = (m.flags & AJ_F_EP) ? AJ_PAWN : absv(g->sq[m.to]);
		sc += 10000 + VALUE[victim] * 10 - VALUE[absv(g->sq[m.from])] / 10;
	}
	if (m.promo)
		sc += 9000 + VALUE[(int)m.promo];
	return sc;
}

static void sort_moves(const Ajedrez* g, AjMove* m, int n, int first_from, int first_to)
{
	int key[AJ_MAX_MOVES];
	for (int i = 0; i < n; i++) {
		key[i] = order_score(g, m[i]);
		if (m[i].from == first_from && m[i].to == first_to)
			key[i] = 1 << 20;
	}
	for (int i = 1; i < n; i++) {
		AjMove mv = m[i];
		int k = key[i], j = i - 1;
		while (j >= 0 && key[j] < k) {
			m[j + 1] = m[j];
			key[j + 1] = key[j];
			j--;
		}
		m[j + 1] = mv;
		key[j + 1] = k;
	}
}

static int quiesce(Ajedrez* g, int alpha, int beta, int depth)
{
	if (++s_nodes > s_node_limit) {
		s_abort = true;
		return 0;
	}
	int stand = aj_evaluate(g) * g->side;
	if (stand >= beta)
		return stand;
	if (stand > alpha)
		alpha = stand;
	if (depth <= 0)
		return alpha;
	AjMove moves[AJ_MAX_MOVES];
	int n = pseudo_moves(g, moves, true);
	sort_moves(g, moves, n, -1, -1);
	int side = g->side;
	for (int i = 0; i < n; i++) {
		aj_make(g, moves[i]);
		if (aj_in_check(g, side)) {
			aj_unmake(g);
			continue;
		}
		int sc = -quiesce(g, -beta, -alpha, depth - 1);
		aj_unmake(g);
		if (s_abort)
			return 0;
		if (sc >= beta)
			return sc;
		if (sc > alpha)
			alpha = sc;
	}
	return alpha;
}

static int negamax(Ajedrez* g, int depth, int alpha, int beta, int ply)
{
	if (g->halfmove >= 100)
		return 0;
	if (depth <= 0)
		return quiesce(g, alpha, beta, 6);
	if (++s_nodes > s_node_limit) {
		s_abort = true;
		return 0;
	}
	AjMove moves[AJ_MAX_MOVES];
	int n = pseudo_moves(g, moves, false);
	sort_moves(g, moves, n, -1, -1);
	int side = g->side, legal = 0, best = -INF;
	for (int i = 0; i < n; i++) {
		aj_make(g, moves[i]);
		if (aj_in_check(g, side)) {
			aj_unmake(g);
			continue;
		}
		legal++;
		int sc = -negamax(g, depth - 1, -beta, -alpha, ply + 1);
		aj_unmake(g);
		if (s_abort)
			return 0;
		if (sc > best)
			best = sc;
		if (sc > alpha)
			alpha = sc;
		if (alpha >= beta)
			break;
	}
	if (!legal)
		return aj_in_check(g, side) ? -MATE + ply : 0;
	return best;
}

AjMove aj_search(Ajedrez* g, int level)
{
	static const int DEPTH[AJ_LEVELS] = { 1, 2, 3, 5 };
	static const int NOISE[AJ_LEVELS] = { 220, 70, 15, 0 };
	/* Presupuesto de nodos: en la consola, el difícil piensa un par de segundos */
	static const long NODES[AJ_LEVELS] = { 20000, 50000, 100000, 250000 };
	if (level < 0)
		level = 0;
	if (level >= AJ_LEVELS)
		level = AJ_LEVELS - 1;
	AjMove moves[AJ_MAX_MOVES];
	int n = aj_legal_moves(g, moves);
	AjMove best = n > 0 ? moves[0] : (AjMove){ 0, 0, 0, 0 };
	if (n <= 1)
		return best;
	/* El principiante a veces mueve sin pensar */
	if (level == 0 && rng_range(&g->rng, 100) < 25)
		return moves[rng_range(&g->rng, n)];
	s_nodes = 0;
	s_node_limit = NODES[level];
	s_searching = true;
	int best_from = -1, best_to = -1;
	/* Profundidad creciente: si se acaba el presupuesto, vale la última completa */
	for (int depth = 1; depth <= DEPTH[level]; depth++) {
		s_abort = false;
		sort_moves(g, moves, n, best_from, best_to);
		int alpha = -INF, best_sc = -INF;
		AjMove iter_best = moves[0];
		for (int i = 0; i < n; i++) {
			aj_make(g, moves[i]);
			/* Con ruido se busca cada jugada con la ventana entera, para poder compararlas */
			int sc = NOISE[level] ? -negamax(g, depth - 1, -INF, INF, 1) : -negamax(g, depth - 1, -INF, -alpha, 1);
			aj_unmake(g);
			if (s_abort)
				break;
			if (NOISE[level] && sc > -MATE / 2 && sc < MATE / 2)
				sc += rng_range(&g->rng, 2 * NOISE[level] + 1) - NOISE[level];
			if (sc > best_sc) {
				best_sc = sc;
				iter_best = moves[i];
			}
			if (sc > alpha)
				alpha = sc;
		}
		if (s_abort && depth > 1)
			break;
		best = iter_best;
		best_from = best.from;
		best_to = best.to;
		if (best_sc >= MATE - 100)
			break; /* mate encontrado */
	}
	s_searching = false;
	return best;
}
