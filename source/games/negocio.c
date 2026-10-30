#include "games/negocio.h"

#include <stdio.h>
#include <string.h>

/* ---------------------------------------------------------------------------
 * Tableros
 * ------------------------------------------------------------------------- */

typedef struct {
	uint8_t type, group;
	uint16_t price;
	const char* name;
} SquareDef;

/* Tablero de serie: calles conocidas de ciudades de toda España. Las esquinas
 * (0, 9, 14 y 23) son la Salida, una Plaza, las Obras y otra Plaza. */
static const SquareDef DEFAULT_BOARD[NG_SQUARES] = {
	{ NG_START, 0, 0, "Salida" },
	{ NG_STREET, 0, 60, "Calle Mayor" },
	{ NG_LUCK, 0, 0, "Suerte" },
	{ NG_STREET, 0, 60, "Calle Real" },
	{ NG_STREET, 1, 100, "Calle Larios" },
	{ NG_STREET, 1, 100, "Calle Sierpes" },
	{ NG_TAX, 0, 0, "Hacienda" },
	{ NG_STREET, 1, 120, "Calle Feria" },
	{ NG_LUCK, 0, 0, "Suerte" },
	{ NG_REST, 0, 0, "Plaza Mayor" },
	{ NG_STREET, 2, 140, "Calle Laurel" },
	{ NG_LUCK, 0, 0, "Suerte" },
	{ NG_STREET, 2, 160, "Calle Estafeta" },
	{ NG_TAX, 0, 0, "Hacienda" },
	{ NG_WORKS, 0, 0, "Obras" },
	{ NG_STREET, 3, 180, "Rúa do Franco" },
	{ NG_STREET, 3, 180, "Calle Uría" },
	{ NG_LUCK, 0, 0, "Suerte" },
	{ NG_STREET, 3, 200, "Calle Corrida" },
	{ NG_STREET, 4, 220, "Calle Colón" },
	{ NG_STREET, 4, 220, "Calle Alfonso I" },
	{ NG_TAX, 0, 0, "Hacienda" },
	{ NG_STREET, 4, 240, "Calle Triana" },
	{ NG_REST, 0, 0, "Plaza del Pilar" },
	{ NG_STREET, 5, 280, "La Rambla" },
	{ NG_STREET, 5, 300, "Paseo de Gracia" },
	{ NG_LUCK, 0, 0, "Suerte" },
	{ NG_STREET, 5, 320, "La Concha" },
};

void ng_board_default(NgBoard* b)
{
	memset(b, 0, sizeof *b);
	snprintf(b->title, sizeof b->title, "España");
	for (int i = 0; i < NG_SQUARES; i++) {
		b->sq[i].type = DEFAULT_BOARD[i].type;
		b->sq[i].group = DEFAULT_BOARD[i].group;
		b->sq[i].price = DEFAULT_BOARD[i].price;
		snprintf(b->sq[i].name, sizeof b->sq[i].name, "%s", DEFAULT_BOARD[i].name);
	}
}

/* Corta una cadena UTF-8 sin partir un carácter */
static void fix_name(char* s, size_t n, const char* fallback)
{
	s[n - 1] = '\0';
	size_t len = strlen(s);
	/* Si el último carácter quedó a medias, se quita */
	size_t i = len;
	while (i > 0 && ((unsigned char)s[i - 1] & 0xC0) == 0x80)
		i--;
	if (i > 0 && ((unsigned char)s[i - 1] & 0x80)) {
		unsigned char lead = (unsigned char)s[i - 1];
		size_t need = lead >= 0xF0 ? 4 : lead >= 0xE0 ? 3 : lead >= 0xC0 ? 2 : 1;
		if (len - (i - 1) < need)
			s[i - 1] = '\0';
	}
	if (!s[0])
		snprintf(s, n, "%s", fallback);
}

void ng_board_sanitize(NgBoard* b)
{
	fix_name(b->title, sizeof b->title, "Mi tablero");
	for (int i = 0; i < NG_SQUARES; i++) {
		NgSquare* q = &b->sq[i];
		if (i == 0)
			q->type = NG_START;
		else if (q->type == NG_START || q->type >= NG_TYPES)
			q->type = NG_STREET;
		q->group %= NG_GROUPS;
		if (q->type != NG_STREET) {
			q->group = 0;
			q->price = 0;
		} else if (q->price < NG_PRICE_MIN)
			q->price = NG_PRICE_MIN;
		else if (q->price > NG_PRICE_MAX)
			q->price = NG_PRICE_MAX;
		q->price -= q->price % 10;
		static const char* const FALLBACK[NG_TYPES] = { "Salida", "Calle", "Suerte", "Hacienda", "Obras", "Plaza" };
		fix_name(q->name, sizeof q->name, FALLBACK[q->type]);
	}
}

#define BOARDS_MAGIC   0x3142474Eu /* "NGB1" */
#define BOARDS_VERSION 1

typedef struct {
	uint32_t magic;
	uint16_t version;
	uint16_t count;
} BoardsHeader;

int ng_boards_load(NgBoard* boards, int max, const char* path)
{
	FILE* f = fopen(path, "rb");
	if (!f)
		return 0;
	BoardsHeader h;
	int n = 0;
	if (fread(&h, sizeof h, 1, f) == 1 && h.magic == BOARDS_MAGIC && h.version == BOARDS_VERSION) {
		int want = h.count < max ? h.count : max;
		while (n < want && fread(&boards[n], sizeof boards[n], 1, f) == 1) {
			ng_board_sanitize(&boards[n]);
			n++;
		}
	}
	fclose(f);
	return n;
}

bool ng_boards_save(const NgBoard* boards, int n, const char* path)
{
	FILE* f = fopen(path, "wb");
	if (!f)
		return false;
	BoardsHeader h = { BOARDS_MAGIC, BOARDS_VERSION, (uint16_t)n };
	bool ok = fwrite(&h, sizeof h, 1, f) == 1;
	if (n > 0)
		ok = ok && fwrite(boards, sizeof boards[0], (size_t)n, f) == (size_t)n;
	return fclose(f) == 0 && ok;
}

int ng_base_rent(int price, int level, bool full_group)
{
	static const int MULT[NG_MAX_LEVEL + 1] = { 1, 3, 6, 10 };
	int base = price / 10;
	if (level == 0)
		return full_group ? base * 2 : base;
	return base * MULT[level];
}

/* ---------------------------------------------------------------------------
 * Tarjetas de la suerte
 * ------------------------------------------------------------------------- */

enum {
	CARD_DEPOSIT,
	CARD_TORTILLA,
	CARD_PARKING,
	CARD_TO_START,
	CARD_BACK3,
	CARD_REPAIRS,
	CARD_BIRTHDAY,
	CARD_ROUND,
	CARD_LOTTERY,
	CARD_BOILER,
	CARD_JAM,
	CARD_CAR,
	CARD_COUNT
};

static const char* const CARD_TEXT[CARD_COUNT] = {
	"Te devuelven la fianza del piso: cobras 100",
	"Ganas el concurso de tortillas del barrio: cobras 50",
	"Multa de aparcamiento: pagas 40",
	"Te vas derecho a la Salida",
	"Te has pasado de calle: retrocede tres casillas",
	"Revisión de los ascensores: pagas 25 por planta",
	"¡Es tu cumpleaños! Cada uno te da 20",
	"Invitas a una ronda: pagas 20 a cada uno",
	"Te toca la pedrea de la lotería: cobras 150",
	"Se rompe la caldera: pagas 100",
	"Atasco monumental: un turno sin tirar",
	"Vendes tu coche viejo: cobras 80",
};

const char* ng_card_text(int card)
{
	return card >= 0 && card < CARD_COUNT ? CARD_TEXT[card] : "";
}

int ng_cards(void)
{
	return CARD_COUNT;
}

/* ---------------------------------------------------------------------------
 * Partida
 * ------------------------------------------------------------------------- */

void ng_init(Negocio* g, const NgBoard* board, int players, uint32_t seed)
{
	memset(g, 0, sizeof *g);
	g->board = *board;
	ng_board_sanitize(&g->board);
	g->players = players < 2 ? 2 : players > NG_PLAYERS ? NG_PLAYERS : players;
	rng_seed(&g->rng, seed);
	for (int p = 0; p < NG_PLAYERS; p++) {
		g->money[p] = NG_START_MONEY;
		g->out[p] = p >= g->players;
	}
	for (int i = 0; i < NG_SQUARES; i++)
		g->owner[i] = -1;
	g->winner = -1;
	g->phase = NG_ROLL;
	g->ev = NG_EV_NONE;
}

static bool is_street(const Negocio* g, int sq)
{
	return g->board.sq[sq].type == NG_STREET;
}

bool ng_full_group(const Negocio* g, int square)
{
	int owner = g->owner[square];
	if (owner < 0 || !is_street(g, square))
		return false;
	int group = g->board.sq[square].group;
	for (int i = 0; i < NG_SQUARES; i++)
		if (is_street(g, i) && g->board.sq[i].group == group && g->owner[i] != owner)
			return false;
	return true;
}

int ng_rent(const Negocio* g, int square)
{
	if (!is_street(g, square) || g->owner[square] < 0)
		return 0;
	return ng_base_rent(g->board.sq[square].price, g->level[square], ng_full_group(g, square));
}

int ng_floor_cost(const Negocio* g, int square)
{
	return g->board.sq[square].price / 2;
}

bool ng_can_build(const Negocio* g, int player, int square)
{
	return g->phase != NG_OVER && is_street(g, square) && g->owner[square] == player &&
		g->level[square] < NG_MAX_LEVEL && ng_full_group(g, square) && g->money[player] >= ng_floor_cost(g, square);
}

bool ng_build(Negocio* g, int player, int square)
{
	if (!ng_can_build(g, player, square))
		return false;
	g->money[player] -= ng_floor_cost(g, square);
	g->level[square]++;
	return true;
}

int ng_worth(const Negocio* g, int player)
{
	int w = g->money[player];
	for (int i = 0; i < NG_SQUARES; i++)
		if (g->owner[i] == player)
			w += g->board.sq[i].price + g->level[i] * ng_floor_cost(g, i);
	return w;
}

int ng_streets_of(const Negocio* g, int player)
{
	int n = 0;
	for (int i = 0; i < NG_SQUARES; i++)
		n += g->owner[i] == player;
	return n;
}

static int active_players(const Negocio* g)
{
	int n = 0;
	for (int p = 0; p < g->players; p++)
		n += !g->out[p];
	return n;
}

/* Vende al banco plantas y luego calles (por la mitad) hasta tener need */
static void raise_money(Negocio* g, int p, int need)
{
	while (g->money[p] < need) {
		int best = -1;
		/* Primero las plantas más caras, para vender menos veces */
		for (int i = 0; i < NG_SQUARES; i++)
			if (g->owner[i] == p && g->level[i] > 0 && (best < 0 || ng_floor_cost(g, i) > ng_floor_cost(g, best)))
				best = i;
		if (best >= 0) {
			g->level[best]--;
			g->money[p] += ng_floor_cost(g, best) / 2;
			continue;
		}
		/* Luego las calles más baratas */
		for (int i = 0; i < NG_SQUARES; i++)
			if (g->owner[i] == p && (best < 0 || g->board.sq[i].price < g->board.sq[best].price))
				best = i;
		if (best < 0)
			return;
		g->owner[best] = -1;
		g->money[p] += g->board.sq[best].price / 2;
	}
}

static void go_bankrupt(Negocio* g, int p)
{
	g->out[p] = true;
	g->money[p] = 0;
	for (int i = 0; i < NG_SQUARES; i++)
		if (g->owner[i] == p) {
			g->owner[i] = -1;
			g->level[i] = 0;
		}
	g->ev_bankrupt = true;
}

/* p paga amount a to (-1: al banco). Devuelve lo pagado. */
static int pay(Negocio* g, int p, int amount, int to)
{
	if (amount <= 0)
		return 0;
	raise_money(g, p, amount);
	int paid = amount;
	if (g->money[p] < amount) {
		paid = g->money[p];
		go_bankrupt(g, p);
	} else {
		g->money[p] -= amount;
	}
	if (to >= 0)
		g->money[to] += paid;
	return paid;
}

static void check_over(Negocio* g)
{
	/* También se acaba si quiebra el humano (el jugador 0) */
	if (active_players(g) <= 1 || g->round >= NG_MAX_ROUNDS || g->out[0]) {
		int best = -1;
		for (int p = 0; p < g->players; p++)
			if (!g->out[p] && (best < 0 || ng_worth(g, p) > ng_worth(g, best)))
				best = p;
		g->winner = best;
		g->phase = NG_OVER;
	}
}

static void next_turn(Negocio* g)
{
	check_over(g);
	if (g->phase == NG_OVER)
		return;
	g->phase = NG_ROLL;
	/* Con dobles se vuelve a tirar (dos veces como mucho) */
	if (!g->out[g->current] && g->doubles > 0 && g->doubles < 3)
		return;
	g->doubles = 0;
	int p = g->current;
	do {
		p = (p + 1) % g->players;
		if (p == 0)
			g->round++;
	} while (g->out[p]);
	g->current = p;
	check_over(g);
}

static void move_to(Negocio* g, int p, int square, bool salary)
{
	if (salary) {
		g->money[p] += NG_SALARY;
		g->ev_salary = true;
	}
	g->pos[p] = square;
}

static void land(Negocio* g, int p, int depth);

static void luck(Negocio* g, int p, int depth)
{
	int card = rng_range(&g->rng, CARD_COUNT);
	g->ev = NG_EV_LUCK;
	g->ev_card = card;
	switch (card) {
	case CARD_DEPOSIT:
		g->money[p] += 100;
		break;
	case CARD_TORTILLA:
		g->money[p] += 50;
		break;
	case CARD_PARKING:
		pay(g, p, 40, -1);
		break;
	case CARD_TO_START:
		move_to(g, p, 0, true);
		break;
	case CARD_BACK3:
		g->pos[p] = (g->pos[p] + NG_SQUARES - 3) % NG_SQUARES;
		if (depth < 2) {
			land(g, p, depth + 1);
			/* Se sigue viendo la tarjeta aunque luego pase otra cosa */
			if (g->ev == NG_EV_MOVE || g->ev == NG_EV_OWN) {
				g->ev = NG_EV_LUCK;
				g->ev_card = card;
			}
		}
		break;
	case CARD_REPAIRS: {
		int floors = 0;
		for (int i = 0; i < NG_SQUARES; i++)
			if (g->owner[i] == p)
				floors += g->level[i];
		pay(g, p, 25 * floors, -1);
		break;
	}
	case CARD_BIRTHDAY:
		for (int q = 0; q < g->players; q++)
			if (q != p && !g->out[q])
				pay(g, q, 20, p);
		break;
	case CARD_ROUND:
		for (int q = 0; q < g->players && !g->out[p]; q++)
			if (q != p && !g->out[q])
				pay(g, p, 20, q);
		break;
	case CARD_LOTTERY:
		g->money[p] += 150;
		break;
	case CARD_BOILER:
		pay(g, p, 100, -1);
		break;
	case CARD_JAM:
		g->skip[p] = 1;
		break;
	case CARD_CAR:
		g->money[p] += 80;
		break;
	}
}

/* Lo que pasa al caer en la casilla. Puede dejar la partida en NG_BUY. */
static void land(Negocio* g, int p, int depth)
{
	int sq = g->pos[p];
	const NgSquare* s = &g->board.sq[sq];
	g->ev_square = sq;
	g->ev = NG_EV_MOVE;
	switch (s->type) {
	case NG_STREET:
		if (g->owner[sq] < 0) {
			if (g->money[p] >= s->price)
				g->phase = NG_BUY;
		} else if (g->owner[sq] == p) {
			g->ev = NG_EV_OWN;
		} else {
			g->ev = NG_EV_RENT;
			g->ev_other = g->owner[sq];
			g->ev_amount = pay(g, p, ng_rent(g, sq), g->owner[sq]);
		}
		break;
	case NG_TAX: {
		int tax = g->money[p] / 10;
		tax -= tax % 10;
		if (tax < NG_TAX_MIN)
			tax = NG_TAX_MIN;
		g->ev = NG_EV_TAX;
		g->ev_amount = pay(g, p, tax, -1);
		break;
	}
	case NG_WORKS:
		g->ev = NG_EV_WORKS;
		g->skip[p] = 1;
		break;
	case NG_LUCK:
		luck(g, p, depth);
		break;
	default:
		break;
	}
}

void ng_roll(Negocio* g, int d1, int d2)
{
	if (g->phase != NG_ROLL)
		return;
	int p = g->current;
	g->ev_player = p;
	g->ev_salary = g->ev_bankrupt = false;
	g->ev_amount = 0;
	g->ev_other = -1;
	if (g->skip[p] > 0) {
		g->skip[p]--;
		g->ev = NG_EV_SKIP;
		g->doubles = 0;
		next_turn(g);
		return;
	}
	g->die[0] = d1 >= 1 && d1 <= 6 ? d1 : 1 + rng_range(&g->rng, 6);
	g->die[1] = d2 >= 1 && d2 <= 6 ? d2 : 1 + rng_range(&g->rng, 6);
	g->doubles = g->die[0] == g->die[1] ? g->doubles + 1 : 0;
	int to = g->pos[p] + g->die[0] + g->die[1];
	move_to(g, p, to % NG_SQUARES, to >= NG_SQUARES);
	land(g, p, 0);
	if (g->out[p]) {
		g->ev = NG_EV_BANKRUPT;
		g->phase = NG_ROLL;
	}
	if (g->phase != NG_BUY)
		next_turn(g);
}

void ng_buy(Negocio* g, bool buy)
{
	if (g->phase != NG_BUY)
		return;
	int p = g->current, sq = g->pos[p];
	if (buy && g->owner[sq] < 0 && g->money[p] >= g->board.sq[sq].price) {
		g->money[p] -= g->board.sq[sq].price;
		g->owner[sq] = p;
	}
	g->phase = NG_ROLL;
	next_turn(g);
}

/* ---------------------------------------------------------------------------
 * IA
 * ------------------------------------------------------------------------- */

/* Cuántas calles de ese color le faltarían a p si comprase sq */
static int missing_after(const Negocio* g, int p, int sq)
{
	int group = g->board.sq[sq].group, missing = 0;
	for (int i = 0; i < NG_SQUARES; i++)
		if (i != sq && is_street(g, i) && g->board.sq[i].group == group && g->owner[i] != p)
			missing++;
	return missing;
}

bool ng_ai_buy(const Negocio* g)
{
	int p = g->current, sq = g->pos[p];
	int left = g->money[p] - g->board.sq[sq].price;
	if (left < 0)
		return false;
	if (missing_after(g, p, sq) == 0)
		return left >= 50;
	return left >= 200;
}

int ng_ai_build(const Negocio* g, int player)
{
	int best = -1;
	for (int i = 0; i < NG_SQUARES; i++)
		if (ng_can_build(g, player, i) && g->money[player] - ng_floor_cost(g, i) >= 300 &&
			(best < 0 || g->level[i] < g->level[best] ||
				(g->level[i] == g->level[best] && ng_floor_cost(g, i) < ng_floor_cost(g, best))))
			best = i;
	return best;
}
