/* Negocio redondo: compraventa de calles en un tablero que cada uno puede
 * personalizar. Esta pantalla tiene la elección de tablero, la partida y el
 * editor de tableros (que es una escena aparte, SCENE_TABLEROS). */
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "games/negocio.h"
#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/gfx.h"
#include "ui/mii.h"
#include "ui/widgets.h"

#define BOARDS_DIR  "sdmc:/3ds/JuegosDeCartas"
#define BOARDS_PATH BOARDS_DIR "/tableros.dat"

/* Tablero en la pantalla de arriba: 10 x 6 casillas de 40 px alrededor */
#define CELL 40.0f

static const CpuId CPUS[NG_PLAYERS] = { CPU_COUNT, CPU_CARMEN, CPU_MANOLO, CPU_LUCIA };
static const u32 TOKEN[NG_PLAYERS] = { 0xDC282D, 0x2864C8, 0x3CA846, 0xF0B41E };
static const u32 GROUP_COLOR[NG_GROUPS] = { 0x8B5A2B, 0x7EC8E3, 0xE05AA0, 0xF08C28,
	0xD7262D, 0xF2D028, 0x2E9E4A, 0x6A3FA0 };
static const char* const GROUP_NAME[NG_GROUPS] = { "marrón", "celeste", "rosa", "naranja",
	"rojo", "amarillo", "verde", "morado" };
static const char* const TYPE_NAME[NG_TYPES] = { "Salida", "Calle", "Suerte", "Hacienda", "Obras", "Plaza" };

/* Tableros: 0 es el de serie y 1..s_nboards los del jugador */
static NgBoard s_default, s_boards[NG_MAX_BOARDS];
static int s_nboards, s_board;
static bool s_boards_loaded;

typedef enum { MODE_SETUP, MODE_PLAY } Mode;
typedef enum { ANIM_NONE, ANIM_DICE, ANIM_WALK } Anim;

static Negocio s_game;
static bool s_started, s_recorded, s_streets_view;
static Mode s_mode;
static int s_players = 4;
static Anim s_anim;
static float s_anim_t, s_cpu_timer, s_end_time, s_delete_armed;
static int s_shown[NG_PLAYERS]; /* casilla en la que se ve cada ficha */
static int s_walk_player, s_walk_left, s_walk_target;
static int s_face[2] = { 1, 1 };
static int s_list_cursor;
static Mood s_mood[NG_PLAYERS];
static char s_message[128];
static Button s_btn_a, s_btn_b, s_btn_x, s_btn_y, s_btn_new, s_btn_del, s_btn_prev, s_btn_next, s_btn_menu;

static const NgBoard* board_at(int i)
{
	return i <= 0 ? &s_default : &s_boards[i - 1];
}

static void load_boards(void)
{
	if (s_boards_loaded)
		return;
	ng_board_default(&s_default);
	s_nboards = ng_boards_load(s_boards, NG_MAX_BOARDS, BOARDS_PATH);
	s_boards_loaded = true;
}

static void save_boards(void)
{
	mkdir("sdmc:/3ds", 0777);
	mkdir(BOARDS_DIR, 0777);
	ng_boards_save(s_boards, s_nboards, BOARDS_PATH);
}

static const char* name(int p)
{
	return p == 0 ? profile_get()->avatar.name : cpu_name(CPUS[p]);
}

static const Avatar* avatar(int p)
{
	return p == 0 ? &profile_get()->avatar : cpu_avatar(CPUS[p]);
}

static void set_message(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
static void set_message(const char* fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(s_message, sizeof s_message, fmt, ap);
	va_end(ap);
}

/* ---------------------------------------------------------------------------
 * Dibujo del tablero (compartido con el editor)
 * ------------------------------------------------------------------------- */

static void cell_origin(int i, float* x, float* y)
{
	int col, row;
	if (i < 10) {
		col = i;
		row = 0;
	} else if (i < 14) {
		col = 9;
		row = i - 9;
	} else if (i < 24) {
		col = 9 - (i - 14);
		row = 5;
	} else {
		col = 0;
		row = 4 - (i - 24);
	}
	*x = col * CELL;
	*y = row * CELL;
}

static u32 cell_fill(const NgSquare* q)
{
	switch (q->type) {
	case NG_START:
		return C2D_Color32(200, 240, 200, 255);
	case NG_LUCK:
		return C2D_Color32(225, 205, 245, 255);
	case NG_TAX:
		return C2D_Color32(215, 215, 220, 255);
	case NG_WORKS:
		return C2D_Color32(250, 225, 140, 255);
	case NG_REST:
		return C2D_Color32(195, 230, 180, 255);
	default:
		return C2D_Color32(250, 246, 232, 255);
	}
}

/* Dibuja la casilla i; con g, también el dueño y las plantas */
static void draw_cell(const NgBoard* b, const Negocio* g, int i, float depth)
{
	const NgSquare* q = &b->sq[i];
	float x, y;
	cell_origin(i, &x, &y);
	u32 ink = C2D_Color32(70, 40, 30, 255);
	gfx_rect(x + 1, y + 1, CELL - 2, CELL - 2, depth, cell_fill(q));
	float cx = x + CELL / 2, cy = y + CELL / 2;
	switch (q->type) {
	case NG_STREET:
		gfx_rect(x + 1, y + 1, CELL - 2, 10, depth, gfx_rgb(GROUP_COLOR[q->group], 255));
		gfx_textf_plain(cx, y + 17, depth, 0.45f, ink, C2D_AlignCenter, "%d", q->price);
		if (g && g->owner[i] >= 0) {
			gfx_rect(x + 3, y + CELL - 9, 8, 6, depth, gfx_rgb(TOKEN[g->owner[i]], 255));
			for (int k = 0; k < g->level[i]; k++)
				gfx_rect(x + 14 + k * 7, y + CELL - 9, 5, 6, depth, C2D_Color32(250, 250, 250, 255));
			if (g->level[i] > 0)
				for (int k = 0; k < g->level[i]; k++)
					gfx_rect(x + 14 + k * 7, y + CELL - 9, 5, 1, depth, C2D_Color32(120, 60, 40, 255));
		}
		break;
	case NG_START:
		gfx_text("Salida", cx, y + 6, depth, 0.45f, C2D_Color32(20, 100, 40, 255), C2D_AlignCenter);
		gfx_triangle(cx - 10, y + 26, cx + 6, y + 26, cx + 6, y + 21, depth, C2D_Color32(20, 120, 50, 255));
		gfx_triangle(cx + 6, y + 18, cx + 6, y + 34, cx + 14, y + 26, depth, C2D_Color32(20, 120, 50, 255));
		gfx_rect(cx - 12, y + 24, 18, 5, depth, C2D_Color32(20, 120, 50, 255));
		break;
	case NG_LUCK:
		gfx_textf_plain(cx, cy - 14, depth, 1.0f, C2D_Color32(110, 50, 160, 255), C2D_AlignCenter, "?");
		break;
	case NG_TAX:
		gfx_textf_plain(cx, cy - 12, depth, 0.9f, C2D_Color32(80, 80, 90, 255), C2D_AlignCenter, "%%");
		break;
	case NG_WORKS:
		for (int k = 0; k < 4; k++)
			gfx_rect(x + 4 + k * 8, cy - 2, 4, 8, depth, C2D_Color32(40, 40, 40, 255));
		gfx_rect(x + 4, cy - 4, CELL - 8, 2, depth, C2D_Color32(40, 40, 40, 255));
		gfx_text("Obras", cx, y + 3, depth, 0.45f, ink, C2D_AlignCenter);
		break;
	case NG_REST:
		gfx_ellipse(cx, cy - 2, 10, 9, depth, C2D_Color32(60, 150, 70, 255));
		gfx_rect(cx - 2, cy + 6, 4, 9, depth, C2D_Color32(120, 80, 40, 255));
		break;
	default:
		break;
	}
}

static void draw_board(const NgBoard* b, const Negocio* g)
{
	gfx_rect(0, 0, TOP_W, SCREEN_H, DEPTH_TABLE, C2D_Color32(90, 60, 30, 255));
	gfx_rect(CELL, CELL, TOP_W - 2 * CELL, SCREEN_H - 2 * CELL, DEPTH_TABLE, C2D_Color32(30, 105, 60, 255));
	for (int i = 0; i < NG_SQUARES; i++)
		draw_cell(b, g, i, DEPTH_TABLE);
}

/* Ficha de cada jugador dentro de la casilla, para que no se tapen */
static void token_pos(int p, int sq, float* x, float* y)
{
	static const float off[NG_PLAYERS][2] = { { -10, 0 }, { 10, 0 }, { -10, 11 }, { 10, 11 } };
	cell_origin(sq, x, y);
	*x += CELL / 2 + off[p][0];
	*y += CELL / 2 + off[p][1];
}

static void draw_die(float cx, float cy, float s, int face)
{
	static const float pip[7][6][2] = {
		{ { 0 } },
		{ { 0, 0 } },
		{ { -1, -1 }, { 1, 1 } },
		{ { -1, -1 }, { 0, 0 }, { 1, 1 } },
		{ { -1, -1 }, { 1, -1 }, { -1, 1 }, { 1, 1 } },
		{ { -1, -1 }, { 1, -1 }, { 0, 0 }, { -1, 1 }, { 1, 1 } },
		{ { -1, -1 }, { 1, -1 }, { -1, 0 }, { 1, 0 }, { -1, 1 }, { 1, 1 } },
	};
	gfx_rrect(cx - s + 2, cy - s + 3, 2 * s, 2 * s, s * 0.35f, DEPTH_UI, C2D_Color32(0, 0, 0, 90));
	gfx_rrect(cx - s, cy - s, 2 * s, 2 * s, s * 0.35f, DEPTH_UI, C2D_Color32(250, 250, 245, 255));
	for (int i = 0; i < face; i++)
		gfx_ellipse(cx + pip[face][i][0] * s * 0.52f, cy + pip[face][i][1] * s * 0.52f, s * 0.17f, s * 0.17f, DEPTH_UI,
			C2D_Color32(180, 30, 40, 255));
}

/* Ficha de información de una casilla, en el centro del tablero */
static void draw_square_info(const NgBoard* b, const Negocio* g, int i, float x, float y)
{
	const NgSquare* q = &b->sq[i];
	gfx_panel(x, y, 200, 74, DEPTH_UI, C2D_Color32(250, 246, 232, 245), C2D_Color32(110, 70, 30, 255));
	if (q->type == NG_STREET)
		gfx_rect(x + 3, y + 3, 194, 16, DEPTH_UI, gfx_rgb(GROUP_COLOR[q->group], 255));
	u32 ink = C2D_Color32(60, 30, 20, 255);
	gfx_text(q->name, x + 100, y + 2, DEPTH_UI, 0.55f, q->type == NG_STREET ? COLOR_WHITE : ink, C2D_AlignCenter);
	switch (q->type) {
	case NG_STREET: {
		int owner = g ? g->owner[i] : -1;
		gfx_textf(x + 10, y + 24, DEPTH_UI, 0.45f, ink, C2D_AlignLeft, "Precio %d · planta %d", q->price, q->price / 2);
		if (owner >= 0) {
			gfx_textf(x + 10, y + 40, DEPTH_UI, 0.45f, ink, C2D_AlignLeft, "De %s · %d planta%s", name(owner),
				g->level[i], g->level[i] == 1 ? "" : "s");
			gfx_textf(x + 10, y + 55, DEPTH_UI, 0.45f, C2D_Color32(170, 40, 40, 255), C2D_AlignLeft, "Alquiler: %d",
				ng_rent(g, i));
		} else {
			gfx_textf(x + 10, y + 40, DEPTH_UI, 0.45f, ink, C2D_AlignLeft, "Alquiler %d (%d con el color)",
				ng_base_rent(q->price, 0, false), ng_base_rent(q->price, 0, true));
			gfx_textf(x + 10, y + 55, DEPTH_UI, 0.45f, ink, C2D_AlignLeft, "Con plantas: %d / %d / %d",
				ng_base_rent(q->price, 1, true), ng_base_rent(q->price, 2, true), ng_base_rent(q->price, 3, true));
		}
		break;
	}
	case NG_START:
		gfx_textf(x + 100, y + 32, DEPTH_UI, 0.45f, ink, C2D_AlignCenter, "Al pasar cobras %d", NG_SALARY);
		break;
	case NG_LUCK:
		gfx_text("Coge una tarjeta de la suerte", x + 100, y + 32, DEPTH_UI, 0.45f, ink, C2D_AlignCenter);
		break;
	case NG_TAX:
		gfx_text("Pagas el 10 % de tu dinero", x + 100, y + 32, DEPTH_UI, 0.45f, ink, C2D_AlignCenter);
		break;
	case NG_WORKS:
		gfx_text("Calle cortada: un turno sin tirar", x + 100, y + 32, DEPTH_UI, 0.45f, ink, C2D_AlignCenter);
		break;
	default:
		gfx_text("Descanso: aquí no pasa nada", x + 100, y + 32, DEPTH_UI, 0.45f, ink, C2D_AlignCenter);
		break;
	}
}

/* ---------------------------------------------------------------------------
 * Partida
 * ------------------------------------------------------------------------- */

static void start_game(void)
{
	ng_init(&s_game, board_at(s_board), s_players, (uint32_t)time(NULL) ^ 0x4E47);
	for (int p = 0; p < NG_PLAYERS; p++)
		s_shown[p] = 0;
	s_mode = MODE_PLAY;
	s_anim = ANIM_NONE;
	s_recorded = false;
	s_streets_view = false;
	s_cpu_timer = 0.0f;
	set_message("Empiezas con %d. ¡Tira los dados!", NG_START_MONEY);
}

static void neg_enter(void)
{
	load_boards();
	if (s_board > s_nboards)
		s_board = 0;
	if (!s_started) {
		s_started = true;
		s_mode = MODE_SETUP;
		set_message("Elige el tablero y cuántos jugáis");
	}
	button_set(&s_btn_menu, 258, 2, 54, 18, "Menú", C2D_Color32(120, 30, 35, 255), KEY_START);
}

/* Cuenta lo que acaba de pasar */
static void announce(void)
{
	const Negocio* g = &s_game;
	int p = g->ev_player;
	const char* n = p == 0 ? "Tú" : name(p);
	const NgSquare* q = &g->board.sq[g->pos[p]];
	char extra[48] = "";
	if (g->ev_salary)
		snprintf(extra, sizeof extra, " (cobra %d por la salida)", NG_SALARY);
	switch (g->ev) {
	case NG_EV_RENT:
		set_message("%s paga %d de alquiler a %s en %s%s", n, g->ev_amount, g->ev_other == 0 ? "ti" : name(g->ev_other),
			q->name, extra);
		mood_set(&s_mood[p], EXPR_SAD, 2.0f);
		mood_set(&s_mood[g->ev_other], EXPR_HAPPY, 2.0f);
		audio_play(SFX_CHIP);
		break;
	case NG_EV_TAX:
		set_message("%s paga %d a Hacienda%s", n, g->ev_amount, extra);
		audio_play(SFX_LOSE);
		break;
	case NG_EV_WORKS:
		set_message("%s se encuentra la calle en obras: un turno sin tirar", n);
		mood_set(&s_mood[p], EXPR_SAD, 2.0f);
		break;
	case NG_EV_LUCK:
		set_message("Suerte: %s", ng_card_text(g->ev_card));
		mood_set(&s_mood[p], EXPR_SURPRISED, 1.5f);
		audio_play(SFX_FLIP);
		break;
	case NG_EV_SKIP:
		set_message("%s pierde el turno", n);
		break;
	case NG_EV_BANKRUPT:
		set_message("¡%s %s en bancarrota!", n, p == 0 ? "estás" : "está");
		mood_set(&s_mood[p], EXPR_SAD, 4.0f);
		audio_play(SFX_LOSE);
		break;
	case NG_EV_OWN:
		set_message("%s %s en %s, que es suya%s", n, p == 0 ? "caes" : "cae", q->name, extra);
		break;
	default:
		if (g->phase == NG_BUY && g->current == p)
			set_message("%s: %s está libre. ¿%s por %d?", p == 0 ? "Tú" : n, q->name, p == 0 ? "La compras" : "La compra",
				q->price);
		else
			set_message("%s %s en %s%s", n, p == 0 ? "caes" : "cae", q->name, extra);
		break;
	}
}

static void roll(void)
{
	int p = s_game.current;
	int from = s_game.pos[p];
	ng_roll(&s_game, 0, 0);
	s_face[0] = s_game.die[0];
	s_face[1] = s_game.die[1];
	if (s_game.ev == NG_EV_SKIP) {
		announce();
		return;
	}
	/* La ficha anda los pasos de los dados y luego, si una tarjeta la ha
	 * movido, salta a su sitio */
	s_anim = ANIM_DICE;
	s_anim_t = 0.0f;
	s_walk_player = p;
	s_walk_left = s_game.die[0] + s_game.die[1];
	s_walk_target = s_game.pos[p];
	s_shown[p] = from;
	audio_play(SFX_FLIP);
}

static void buy(bool yes)
{
	int p = s_game.current, sq = s_game.pos[p];
	ng_buy(&s_game, yes);
	if (yes && s_game.owner[sq] == p) {
		set_message("%s %s %s", p == 0 ? "Compras" : name(p), p == 0 ? "" : "compra", s_game.board.sq[sq].name);
		mood_set(&s_mood[p], EXPR_HAPPY, 1.5f);
		audio_play(SFX_CHIP);
	} else {
		set_message("%s no %s %s", p == 0 ? "Tú" : name(p), p == 0 ? "compras" : "compra", s_game.board.sq[sq].name);
	}
}

/* Calles del jugador (para la lista de "Mis calles") */
static int my_streets(int* out)
{
	int n = 0;
	for (int i = 0; i < NG_SQUARES; i++)
		if (s_game.owner[i] == 0)
			out[n++] = i;
	return n;
}

/* La lista de calles se desplaza para que se vea el cursor */
#define LIST_ROWS 11
static int list_first(int n)
{
	int first = s_list_cursor - LIST_ROWS + 1;
	if (first > n - LIST_ROWS)
		first = n - LIST_ROWS;
	return first < 0 ? 0 : first;
}

static SceneId play_update(const Input* in, float dt)
{
	Negocio* g = &s_game;
	const float y = 205, h = 28;
	bool idle = s_anim == ANIM_NONE;
	bool mine = idle && g->phase != NG_OVER && g->current == 0;

	switch (s_anim) {
	case ANIM_DICE:
		s_anim_t += dt;
		s_face[0] = 1 + (int)(gfx_time() * 23.0f) % 6;
		s_face[1] = 1 + (int)(gfx_time() * 17.0f + 3) % 6;
		if (s_anim_t > 0.45f) {
			s_face[0] = g->die[0];
			s_face[1] = g->die[1];
			s_anim = ANIM_WALK;
			s_anim_t = 0.0f;
		}
		break;
	case ANIM_WALK:
		s_anim_t += dt;
		if (s_anim_t > 0.13f) {
			s_anim_t = 0.0f;
			if (s_walk_left > 0) {
				s_shown[s_walk_player] = (s_shown[s_walk_player] + 1) % NG_SQUARES;
				s_walk_left--;
				audio_play(SFX_SELECT);
			} else {
				s_shown[s_walk_player] = s_walk_target;
				s_anim = ANIM_NONE;
				announce();
			}
		}
		break;
	default:
		break;
	}

	if (g->phase == NG_OVER) {
		if (idle && !s_recorded) {
			s_recorded = true;
			s_end_time = 0.0f;
			bool won = g->winner == 0;
			profile_record_game(GAME_NEGOCIO, won);
			audio_play(won ? SFX_WIN : SFX_LOSE);
			for (int p = 0; p < g->players; p++)
				mood_set(&s_mood[p], p == g->winner ? EXPR_HAPPY : EXPR_SAD, 5.0f);
		}
		s_end_time += dt;
		button_set(&s_btn_a, 60, y, 200, h, GLYPH_A " Otra partida", C2D_Color32(40, 140, 60, 255), KEY_A);
		s_btn_a.enabled = idle && s_end_time > 1.0f;
		if (button_update(&s_btn_a, in)) {
			s_mode = MODE_SETUP;
			set_message("Elige el tablero y cuántos jugáis");
		}
		return SCENE_NEGOCIO;
	}

	if (s_streets_view) {
		/* Lista de tus calles: Ⓐ levanta una planta */
		int list[NG_SQUARES];
		int n = my_streets(list);
		if (n == 0)
			s_streets_view = false;
		if (s_list_cursor >= n)
			s_list_cursor = n - 1;
		if (s_list_cursor < 0)
			s_list_cursor = 0;
		if (in->repeat & KEY_UP)
			s_list_cursor = (s_list_cursor + n - 1) % (n ? n : 1);
		if (in->repeat & KEY_DOWN)
			s_list_cursor = (s_list_cursor + 1) % (n ? n : 1);
		int first = list_first(n);
		for (int k = first; k < n && k < first + LIST_ROWS; k++)
			if (input_touch_in(in, 8, 30 + (k - first) * 14, 304, 14))
				s_list_cursor = k;
		int sq = n ? list[s_list_cursor] : -1;
		static char build_label[32];
		snprintf(build_label, sizeof build_label, GLYPH_A " Planta (%d)", sq >= 0 ? ng_floor_cost(g, sq) : 0);
		button_set(&s_btn_a, 12, y, 144, h, build_label, C2D_Color32(40, 140, 60, 255), KEY_A);
		button_set(&s_btn_b, 164, y, 144, h, GLYPH_B " Volver", C2D_Color32(190, 60, 50, 255), KEY_B);
		s_btn_a.enabled = mine && sq >= 0 && ng_can_build(g, 0, sq);
		s_btn_b.enabled = true;
		if (button_update(&s_btn_a, in) && ng_build(g, 0, sq)) {
			audio_play(SFX_CHIP);
			set_message("Levantas una planta en %s: ahora cobras %d", g->board.sq[sq].name, ng_rent(g, sq));
		} else if (button_update(&s_btn_b, in)) {
			s_streets_view = false;
		}
		return SCENE_NEGOCIO;
	}

	if (g->phase == NG_BUY) {
		int sq = g->pos[g->current];
		static char buy_label[32];
		snprintf(buy_label, sizeof buy_label, GLYPH_A " Comprar (%d)", g->board.sq[sq].price);
		button_set(&s_btn_a, 12, y, 144, h, buy_label, C2D_Color32(40, 140, 60, 255), KEY_A);
		button_set(&s_btn_b, 164, y, 144, h, GLYPH_B " No", C2D_Color32(190, 60, 50, 255), KEY_B);
		s_btn_a.enabled = s_btn_b.enabled = mine;
		if (mine) {
			if (button_update(&s_btn_a, in))
				buy(true);
			else if (button_update(&s_btn_b, in))
				buy(false);
		}
	} else {
		button_set(&s_btn_a, 12, y, 144, h, GLYPH_A " Tirar los dados", C2D_Color32(40, 140, 60, 255), KEY_A);
		button_set(&s_btn_y, 164, y, 144, h, GLYPH_Y " Mis calles", C2D_Color32(60, 90, 160, 255), KEY_Y);
		s_btn_a.enabled = mine;
		int list[NG_SQUARES];
		s_btn_y.enabled = mine && my_streets(list) > 0;
		if (mine) {
			bool dice_touched = in->touch_down && in->tx > 236 && in->ty > 120 && in->ty < 196;
			if (button_update(&s_btn_y, in)) {
				s_streets_view = true;
				s_list_cursor = 0;
			} else if (dice_touched || button_update(&s_btn_a, in)) {
				roll();
			}
		}
	}
	/* Turno de la CPU */
	if (idle && g->current != 0) {
		s_cpu_timer += dt;
		if (s_cpu_timer > 0.8f) {
			s_cpu_timer = 0.0f;
			int p = g->current;
			if (g->phase == NG_BUY) {
				buy(ng_ai_buy(g));
			} else {
				int sq, built = 0;
				while ((sq = ng_ai_build(g, p)) >= 0 && ng_build(g, p, sq))
					built++;
				if (built) {
					set_message("%s levanta %d planta%s", name(p), built, built == 1 ? "" : "s");
					audio_play(SFX_CHIP);
					s_cpu_timer = -0.6f; /* que dé tiempo a leerlo */
				} else {
					roll();
				}
			}
		}
	}
	return SCENE_NEGOCIO;
}

/* ---------------------------------------------------------------------------
 * Elección de tablero
 * ------------------------------------------------------------------------- */

static int new_board_from(int from)
{
	if (s_nboards >= NG_MAX_BOARDS)
		return -1;
	NgBoard* b = &s_boards[s_nboards];
	*b = *board_at(from);
	snprintf(b->title, sizeof b->title, "Mi tablero %d", s_nboards + 1);
	s_nboards++;
	save_boards();
	return s_nboards; /* índice en la lista (0 es el de serie) */
}

static SceneId setup_update(const Input* in, float dt)
{
	if (s_delete_armed > 0.0f)
		s_delete_armed -= dt;
	int count = s_nboards + 1;
	button_set(&s_btn_prev, 8, 44, 34, 34, "<", C2D_Color32(60, 90, 160, 255), KEY_L);
	button_set(&s_btn_next, 278, 44, 34, 34, ">", C2D_Color32(60, 90, 160, 255), KEY_R);
	static char players_label[24];
	snprintf(players_label, sizeof players_label, GLYPH_X " %d jugadores", s_players);
	button_set(&s_btn_x, 12, 130, 144, 28, players_label, C2D_Color32(60, 90, 160, 255), KEY_X);
	button_set(&s_btn_y, 164, 130, 144, 28, GLYPH_Y " Editar", C2D_Color32(200, 140, 30, 255), KEY_Y);
	button_set(&s_btn_new, 12, 166, 144, 28, "Nuevo tablero", C2D_Color32(40, 110, 120, 255), KEY_SELECT);
	button_set(&s_btn_del, 164, 166, 144, 28, s_delete_armed > 0.0f ? "¿Seguro? Toca" : "Borrar",
		C2D_Color32(150, 40, 40, 255), 0);
	button_set(&s_btn_a, 60, 205, 200, 28, GLYPH_A " Jugar", C2D_Color32(40, 140, 60, 255), KEY_A);
	s_btn_del.enabled = s_board > 0;
	s_btn_new.enabled = s_nboards < NG_MAX_BOARDS;
	if (button_update(&s_btn_prev, in) || (in->repeat & KEY_LEFT)) {
		s_board = (s_board + count - 1) % count;
		s_delete_armed = 0.0f;
	} else if (button_update(&s_btn_next, in) || (in->repeat & KEY_RIGHT)) {
		s_board = (s_board + 1) % count;
		s_delete_armed = 0.0f;
	} else if (button_update(&s_btn_x, in)) {
		s_players = s_players == 4 ? 2 : s_players + 1;
	} else if (button_update(&s_btn_y, in)) {
		/* El de serie no se toca: se edita una copia */
		if (s_board == 0) {
			int k = new_board_from(0);
			if (k < 0) {
				audio_play(SFX_ERROR);
				set_message("Ya tienes %d tableros: borra alguno", NG_MAX_BOARDS);
				return SCENE_NEGOCIO;
			}
			s_board = k;
		}
		return SCENE_TABLEROS;
	} else if (button_update(&s_btn_new, in)) {
		int k = new_board_from(s_board);
		if (k > 0) {
			s_board = k;
			return SCENE_TABLEROS;
		}
	} else if (button_update(&s_btn_del, in)) {
		if (s_delete_armed > 0.0f) {
			for (int i = s_board - 1; i < s_nboards - 1; i++)
				s_boards[i] = s_boards[i + 1];
			s_nboards--;
			save_boards();
			s_board--;
			s_delete_armed = 0.0f;
			audio_play(SFX_LOSE);
			set_message("Tablero borrado");
		} else {
			s_delete_armed = 2.5f;
		}
	} else if (button_update(&s_btn_a, in)) {
		start_game();
	}
	return SCENE_NEGOCIO;
}

static SceneId neg_update(const Input* in, float dt)
{
	if (button_update(&s_btn_menu, in))
		return SCENE_MENU;
	for (int p = 0; p < NG_PLAYERS; p++)
		mood_update(&s_mood[p], dt);
	return s_mode == MODE_SETUP ? setup_update(in, dt) : play_update(in, dt);
}

static void neg_draw_top(void)
{
	if (s_mode == MODE_SETUP) {
		const NgBoard* b = board_at(s_board);
		draw_board(b, NULL);
		gfx_text(b->title, 200, 52, DEPTH_UI, 0.8f, COLOR_GOLD, C2D_AlignCenter);
		gfx_text("Negocio redondo", 200, 84, DEPTH_UI, 0.5f, COLOR_CREAM, C2D_AlignCenter);
		int streets = 0;
		for (int i = 0; i < NG_SQUARES; i++)
			streets += b->sq[i].type == NG_STREET;
		gfx_textf(200, 104, DEPTH_UI, 0.45f, C2D_Color32(200, 230, 200, 255), C2D_AlignCenter, "%d calles · %s",
			streets, s_board == 0 ? "tablero de serie" : "tablero tuyo");
		draw_square_info(b, NULL, 1, 100, 122);
		return;
	}
	const Negocio* g = &s_game;
	draw_board(&g->board, g);
	/* Fichas */
	for (int p = 0; p < g->players; p++) {
		if (g->out[p])
			continue;
		float x, y;
		token_pos(p, s_shown[p], &x, &y);
		bool moving = s_anim == ANIM_WALK && p == s_walk_player;
		float lift = moving ? 3.0f : 0.0f;
		gfx_ellipse(x + 1, y + 2, 5, 5, DEPTH_CARDS, C2D_Color32(0, 0, 0, 100));
		gfx_ellipse(x, y - lift, 5, 5, DEPTH_CARDS + lift, gfx_rgb(TOKEN[p], 255));
		gfx_ellipse(x - 2, y - 2 - lift, 2, 1.6f, DEPTH_CARDS + lift, C2D_Color32(255, 255, 255, 140));
	}
	/* Centro: dados y la casilla de quien acaba de mover */
	draw_die(80, 70, 14, s_face[0]);
	draw_die(114, 70, 14, s_face[1]);
	gfx_textf(97, 92, DEPTH_UI, 0.45f, COLOR_CREAM, C2D_AlignCenter, "Vuelta %d de %d", g->round + 1, NG_MAX_ROUNDS);
	int focus = s_shown[s_anim == ANIM_NONE ? g->ev_player : s_walk_player];
	draw_square_info(&g->board, g, focus, 150, 46);
	/* Dinero de cada uno */
	for (int p = 0; p < g->players; p++) {
		float x = 52 + p * 76, y = 140;
		bool turn = g->current == p && g->phase != NG_OVER;
		gfx_panel(x - 4, y - 4, 72, 52, DEPTH_UI, C2D_Color32(10, 50, 25, 220), turn ? COLOR_GOLD : gfx_rgb(TOKEN[p], 255));
		mii_draw(avatar(p), x + 12, y + 14, 10, DEPTH_UI, mood_expr(&s_mood[p]), false);
		gfx_ellipse(x + 26, y + 26, 4, 4, DEPTH_UI, gfx_rgb(TOKEN[p], 255));
		if (g->out[p])
			gfx_text("Quiebra", x + 32, y + 30, DEPTH_UI, 0.45f, COLOR_RED, C2D_AlignCenter);
		else
			gfx_textf(x + 50, y + 2, DEPTH_UI, 0.45f, COLOR_GOLD, C2D_AlignCenter, "%d", g->money[p]);
		gfx_textf(x + 50, y + 18, DEPTH_UI, 0.42f, COLOR_CREAM, C2D_AlignCenter, "%d c.", ng_streets_of(g, p));
	}
	if (g->phase == NG_OVER && s_anim == ANIM_NONE && banner_draw(s_end_time, 200, 120, 320, 64)) {
		if (g->winner == 0)
			gfx_text("¡Eres el rey del negocio!", 200, 100, DEPTH_BANNER, 0.62f, COLOR_GOLD, C2D_AlignCenter);
		else
			gfx_textf(200, 100, DEPTH_BANNER, 0.62f, COLOR_CREAM, C2D_AlignCenter, "Gana %s", name(g->winner));
		gfx_textf(200, 126, DEPTH_BANNER, 0.45f, COLOR_CREAM, C2D_AlignCenter, "con un patrimonio de %d",
			ng_worth(g, g->winner));
	}
}

static void neg_draw_bottom(void)
{
	gfx_background(BG_TABLE_BOTTOM);
	button_draw(&s_btn_menu, false);
	if (s_mode == MODE_SETUP) {
		const NgBoard* b = board_at(s_board);
		gfx_text("Negocio redondo", 10, 4, 0, 0.55f, COLOR_GOLD, C2D_AlignLeft);
		button_draw(&s_btn_prev, true);
		button_draw(&s_btn_next, true);
		gfx_text(b->title, BOTTOM_W / 2, 46, 0, 0.6f, COLOR_CREAM, C2D_AlignCenter);
		gfx_textf(BOTTOM_W / 2, 66, 0, 0.42f, C2D_Color32(200, 230, 200, 255), C2D_AlignCenter, "Tablero %d de %d",
			s_board + 1, s_nboards + 1);
		gfx_text_wrap(s_message, 14, 92, 0, 0.42f, COLOR_GOLD, 292);
		button_draw(&s_btn_x, false);
		button_draw(&s_btn_y, false);
		button_draw(&s_btn_new, false);
		button_draw(&s_btn_del, false);
		button_draw(&s_btn_a, true);
		return;
	}
	const Negocio* g = &s_game;
	gfx_text_wrap(s_message, 8, 2, 0, 0.42f, COLOR_GOLD, 246);
	if (s_streets_view) {
		int list[NG_SQUARES];
		int n = my_streets(list);
		gfx_text("Tus calles (con todo el color puedes levantar plantas)", 8, 30 - 14, 0, 0.4f, COLOR_CREAM,
			C2D_AlignLeft);
		int first = list_first(n);
		for (int k = first; k < n && k < first + LIST_ROWS; k++) {
			int sq = list[k];
			const NgSquare* q = &g->board.sq[sq];
			float y = 30 + (k - first) * 14;
			if (k == s_list_cursor)
				gfx_rect(6, y, 308, 14, 0, C2D_Color32(255, 215, 90, 90));
			gfx_rect(10, y + 3, 10, 8, 0, gfx_rgb(GROUP_COLOR[q->group], 255));
			gfx_textf_plain(26, y, 0, 0.42f, COLOR_CREAM, C2D_AlignLeft, "%s", q->name);
			gfx_textf_plain(196, y, 0, 0.42f, C2D_Color32(200, 230, 200, 255), C2D_AlignLeft, "%d pl. · %d",
				g->level[sq], ng_rent(g, sq));
			if (ng_full_group(g, sq))
				gfx_text("color", 312, y, 0, 0.4f, COLOR_GOLD, C2D_AlignRight);
		}
		gfx_textf(BOTTOM_W / 2, 190, 0, 0.42f, COLOR_CREAM, C2D_AlignCenter, "Tienes %d", g->money[0]);
		button_draw(&s_btn_a, s_btn_a.enabled);
		button_draw(&s_btn_b, true);
		return;
	}
	/* Jugadores */
	for (int p = 0; p < g->players; p++) {
		float y = 46 + p * 36;
		bool turn = g->current == p && g->phase != NG_OVER;
		gfx_panel(8, y - 14, 214, 32, 0, C2D_Color32(10, 50, 25, 200), turn ? COLOR_GOLD : C2D_Color32(20, 60, 30, 255));
		mii_draw(avatar(p), 24, y + 2, 11, 0, mood_expr(&s_mood[p]), false);
		gfx_ellipse(44, y + 2, 5, 5, 0, gfx_rgb(TOKEN[p], 255));
		gfx_text(name(p), 54, y - 12, 0, 0.45f, COLOR_CREAM, C2D_AlignLeft);
		if (g->out[p])
			gfx_text("en bancarrota", 54, y + 2, 0, 0.42f, COLOR_RED, C2D_AlignLeft);
		else
			gfx_textf(54, y + 2, 0, 0.42f, C2D_Color32(200, 230, 200, 255), C2D_AlignLeft, "%d · %d calles%s", g->money[p],
				ng_streets_of(g, p), g->skip[p] ? " · sin tirar" : "");
	}
	draw_die(256, 140, 16, s_face[0]);
	draw_die(292, 156, 16, s_face[1]);
	if (g->phase == NG_ROLL && g->current == 0 && s_anim == ANIM_NONE)
		gfx_text("¡Te toca!", 274, 104, 0, 0.5f, COLOR_GOLD, C2D_AlignCenter);
	if (g->phase == NG_OVER) {
		button_draw(&s_btn_a, s_btn_a.enabled);
		return;
	}
	button_draw(&s_btn_a, s_btn_a.enabled);
	if (g->phase == NG_BUY)
		button_draw(&s_btn_b, s_btn_b.enabled);
	else
		button_draw(&s_btn_y, s_btn_y.enabled);
}

/* Salir a medias desde la pausa: la próxima vez se empieza de cero */
static void scene_leave(void)
{
	s_started = false;
}

const Scene scene_negocio = { neg_enter, neg_update, neg_draw_top, neg_draw_bottom, scene_leave };

/* ---------------------------------------------------------------------------
 * Editor de tableros
 * ------------------------------------------------------------------------- */

static int s_edit_sq;
static Button s_ed_name, s_ed_type, s_ed_color, s_ed_minus, s_ed_plus, s_ed_title, s_ed_done, s_ed_prev, s_ed_next;

static NgBoard* edited(void)
{
	return &s_boards[s_board - 1];
}

static void ed_enter(void)
{
	load_boards();
	s_edit_sq = 1;
}

/* Teclado del sistema para un nombre */
static void edit_text(char* text, size_t size, const char* hint)
{
	SwkbdState kbd;
	char buf[NG_NAME_MAX];
	swkbdInit(&kbd, SWKBD_TYPE_NORMAL, 2, 12);
	swkbdSetHintText(&kbd, hint);
	swkbdSetInitialText(&kbd, text);
	swkbdSetValidation(&kbd, SWKBD_NOTEMPTY_NOTBLANK, 0, 0);
	if (swkbdInputText(&kbd, buf, sizeof buf) == SWKBD_BUTTON_CONFIRM) {
		snprintf(text, size, "%s", buf);
		ng_board_sanitize(edited());
	}
}

static SceneId ed_update(const Input* in, float dt)
{
	(void)dt;
	if (s_board < 1 || s_board > s_nboards)
		return SCENE_NEGOCIO;
	NgBoard* b = edited();
	NgSquare* q = &b->sq[s_edit_sq];
	button_set(&s_ed_prev, 8, 26, 34, 30, "<", C2D_Color32(60, 90, 160, 255), 0);
	button_set(&s_ed_next, 278, 26, 34, 30, ">", C2D_Color32(60, 90, 160, 255), 0);
	button_set(&s_ed_name, 12, 64, 296, 26, GLYPH_A " Cambiar el nombre", C2D_Color32(40, 140, 60, 255), KEY_A);
	button_set(&s_ed_type, 12, 96, 144, 26, GLYPH_X " Tipo", C2D_Color32(60, 90, 160, 255), KEY_X);
	button_set(&s_ed_color, 164, 96, 144, 26, GLYPH_Y " Color", C2D_Color32(200, 140, 30, 255), KEY_Y);
	button_set(&s_ed_minus, 12, 128, 60, 26, "- " GLYPH_L, C2D_Color32(120, 30, 35, 255), 0);
	button_set(&s_ed_plus, 248, 128, 60, 26, "+ " GLYPH_R, C2D_Color32(40, 140, 60, 255), 0);
	button_set(&s_ed_title, 12, 205, 144, 28, "Nombre del tablero", C2D_Color32(40, 110, 120, 255), KEY_SELECT);
	button_set(&s_ed_done, 164, 205, 144, 28, GLYPH_B " Guardar", C2D_Color32(40, 140, 60, 255), KEY_B);
	bool street = q->type == NG_STREET;
	s_ed_type.enabled = s_edit_sq != 0;
	s_ed_color.enabled = s_ed_minus.enabled = s_ed_plus.enabled = street;

	if (button_update(&s_ed_prev, in) || (in->repeat & KEY_LEFT)) {
		s_edit_sq = (s_edit_sq + NG_SQUARES - 1) % NG_SQUARES;
		audio_play(SFX_SELECT);
	} else if (button_update(&s_ed_next, in) || (in->repeat & KEY_RIGHT)) {
		s_edit_sq = (s_edit_sq + 1) % NG_SQUARES;
		audio_play(SFX_SELECT);
	} else if (button_update(&s_ed_name, in)) {
		edit_text(q->name, sizeof q->name, "Nombre de la casilla (por ejemplo, Calle del Pez)");
	} else if (button_update(&s_ed_type, in)) {
		/* De Calle a Suerte, Hacienda, Obras, Plaza y vuelta a Calle */
		q->type = q->type + 1 >= NG_TYPES ? NG_STREET : q->type + 1;
		if (q->type == NG_STREET && q->price == 0)
			q->price = 100;
		static const char* const DEFAULT_NAME[NG_TYPES] = { "Salida", "Calle nueva", "Suerte", "Hacienda", "Obras",
			"Plaza" };
		snprintf(q->name, sizeof q->name, "%s", DEFAULT_NAME[q->type]);
		ng_board_sanitize(b);
	} else if (button_update(&s_ed_color, in)) {
		q->group = (q->group + 1) % NG_GROUPS;
	} else if (street && (button_update(&s_ed_minus, in) || (in->repeat & (KEY_L | KEY_DOWN)))) {
		if (q->price > NG_PRICE_MIN)
			q->price -= 10;
	} else if (street && (button_update(&s_ed_plus, in) || (in->repeat & (KEY_R | KEY_UP)))) {
		if (q->price < NG_PRICE_MAX)
			q->price += 10;
	} else if (button_update(&s_ed_title, in)) {
		edit_text(b->title, sizeof b->title, "Nombre del tablero (por ejemplo, Mi pueblo)");
	} else if (button_update(&s_ed_done, in) || (in->down & KEY_START)) {
		ng_board_sanitize(b);
		save_boards();
		set_message("Tablero «%s» guardado", b->title);
		return SCENE_NEGOCIO;
	}
	return SCENE_TABLEROS;
}

static void ed_draw_top(void)
{
	if (s_board < 1 || s_board > s_nboards)
		return;
	const NgBoard* b = edited();
	draw_board(b, NULL);
	/* Casilla elegida */
	float x, y;
	cell_origin(s_edit_sq, &x, &y);
	float pulse = 0.5f + 0.5f * sinf(gfx_time() * 6.0f);
	u32 c = C2D_Color32(255, 200, 40, (u8)(160 + 95 * pulse));
	gfx_rect(x, y, CELL, 3, DEPTH_UI, c);
	gfx_rect(x, y + CELL - 3, CELL, 3, DEPTH_UI, c);
	gfx_rect(x, y, 3, CELL, DEPTH_UI, c);
	gfx_rect(x + CELL - 3, y, 3, CELL, DEPTH_UI, c);
	gfx_text(b->title, 200, 48, DEPTH_UI, 0.7f, COLOR_GOLD, C2D_AlignCenter);
	gfx_text("Editor de tableros", 200, 76, DEPTH_UI, 0.45f, COLOR_CREAM, C2D_AlignCenter);
	draw_square_info(b, NULL, s_edit_sq, 100, 112);
}

static void ed_draw_bottom(void)
{
	gfx_background(BG_TABLE_BOTTOM);
	if (s_board < 1 || s_board > s_nboards)
		return;
	const NgBoard* b = edited();
	const NgSquare* q = &b->sq[s_edit_sq];
	gfx_textf(BOTTOM_W / 2, 4, 0, 0.45f, COLOR_GOLD, C2D_AlignCenter, "Casilla %d de %d (cruceta)", s_edit_sq + 1,
		NG_SQUARES);
	button_draw(&s_ed_prev, true);
	button_draw(&s_ed_next, true);
	gfx_text(q->name, BOTTOM_W / 2, 28, 0, 0.6f, COLOR_CREAM, C2D_AlignCenter);
	button_draw(&s_ed_name, true);
	static char type_label[32], color_label[32];
	snprintf(type_label, sizeof type_label, GLYPH_X " %s", TYPE_NAME[q->type]);
	snprintf(color_label, sizeof color_label, GLYPH_Y " %s", q->type == NG_STREET ? GROUP_NAME[q->group] : "Color");
	s_ed_type.label = type_label;
	s_ed_color.label = color_label;
	if (q->type == NG_STREET)
		s_ed_color.color = gfx_rgb(GROUP_COLOR[q->group], 255);
	button_draw(&s_ed_type, s_ed_type.enabled);
	button_draw(&s_ed_color, s_ed_color.enabled);
	if (q->type == NG_STREET) {
		button_draw(&s_ed_minus, true);
		button_draw(&s_ed_plus, true);
		gfx_textf(BOTTOM_W / 2, 130, 0, 0.55f, COLOR_CREAM, C2D_AlignCenter, "Precio: %d", q->price);
		gfx_textf(BOTTOM_W / 2, 164, 0, 0.42f, C2D_Color32(200, 230, 200, 255), C2D_AlignCenter,
			"Alquiler %d · planta %d · con 3 plantas %d", ng_base_rent(q->price, 0, false), q->price / 2,
			ng_base_rent(q->price, 3, true));
	} else if (s_edit_sq == 0) {
		gfx_text("La salida siempre es la primera casilla", BOTTOM_W / 2, 134, 0, 0.42f,
			C2D_Color32(200, 230, 200, 255), C2D_AlignCenter);
	}
	gfx_text("Los cambios se guardan en la tarjeta SD", BOTTOM_W / 2, 184, 0, 0.4f, C2D_Color32(200, 180, 170, 255),
		C2D_AlignCenter);
	button_draw(&s_ed_title, false);
	button_draw(&s_ed_done, true);
}

const Scene scene_tableros = { ed_enter, ed_update, ed_draw_top, ed_draw_bottom };
