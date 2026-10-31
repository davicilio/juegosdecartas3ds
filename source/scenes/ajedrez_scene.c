/* Ajedrez contra la consola: antes de jugar se elige la dificultad y el color.
 * El tablero táctil está abajo y arriba, el tablero grande con las jugadas. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "games/ajedrez.h"
#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/gfx.h"
#include "ui/mii.h"
#include "ui/widgets.h"

#define RIVAL CPU_MANOLO
#define B_CELL 26.0f /* tablero de abajo */
#define B_X    4.0f
#define B_Y    26.0f
#define T_CELL 27.0f /* tablero de arriba */
#define T_X    14.0f
#define T_Y    12.0f

static const char* const LEVEL_NAME[AJ_LEVELS] = { "Principiante", "Fácil", "Medio", "Difícil" };
static const char* const LEVEL_INFO[AJ_LEVELS] = {
	"Mueve casi sin pensar y se deja piezas. Para aprender.",
	"Ve las jugadas de un par de movimientos, pero se equivoca a menudo.",
	"Piensa tres jugadas y no regala piezas así como así.",
	"Piensa más a fondo y castiga los errores. ¡Suerte!",
};

typedef enum { MODE_SETUP, MODE_PLAY } Mode;

static Ajedrez s_game;
static bool s_started, s_recorded;
static Mode s_mode;
static int s_level = 1, s_color_choice; /* 0 blancas, 1 negras, 2 al azar */
static int s_me = 1;                    /* 1 blancas, -1 negras */
static int s_sel = -1, s_cursor = 12;
static int s_promo_from = -1, s_promo_to = -1;
static int s_think;
static AjStatus s_status;
static char s_names[AJ_MAX_HISTORY][10];
static char s_message[96];
static float s_end_time;
static Mood s_mood;
static Button s_btn_level[AJ_LEVELS], s_btn_color[3], s_btn_play, s_btn_undo, s_btn_resign, s_btn_promo[4], s_btn_menu;
static bool s_resigned;

/* ---------------------------------------------------------------------------
 * Piezas
 * ------------------------------------------------------------------------- */

/* Dibuja la silueta de una pieza con los pies en by; e engorda la figura
 * (para el contorno) */
static void piece_shape(int kind, float cx, float by, float u, float e, float depth, u32 col)
{
	gfx_rect(cx - 8 * u - e, by - 3 * u - e, 16 * u + 2 * e, 4 * u + 2 * e, depth, col);
	switch (kind) {
	case AJ_PAWN:
		gfx_ellipse(cx, by - 6 * u, 5 * u + e, 4.5f * u + e, depth, col);
		gfx_ellipse(cx, by - 12 * u, 4 * u + e, 4 * u + e, depth, col);
		break;
	case AJ_ROOK:
		gfx_rect(cx - 5 * u - e, by - 15 * u - e, 10 * u + 2 * e, 13 * u + 2 * e, depth, col);
		gfx_rect(cx - 7 * u - e, by - 18 * u - e, 14 * u + 2 * e, 4 * u + 2 * e, depth, col);
		for (int k = 0; k < 3; k++)
			gfx_rect(cx - 7 * u + k * 5.25f * u - e, by - 21 * u - e, 3.5f * u + 2 * e, 4 * u + 2 * e, depth, col);
		break;
	case AJ_KNIGHT:
		gfx_triangle(cx - 7 * u - e, by - 2 * u + e, cx + 6 * u + e, by - 2 * u + e, cx + 3 * u + e, by - 16 * u - e, depth,
			col);
		gfx_ellipse(cx - 1 * u, by - 15 * u, 5.5f * u + e, 4.5f * u + e, depth, col);
		gfx_triangle(cx - 2 * u, by - 19 * u - e, cx - 10 * u - e, by - 10 * u + e, cx - 1 * u, by - 11 * u + e, depth, col);
		gfx_triangle(cx + 1 * u - e, by - 18 * u, cx + 3 * u, by - 23 * u - e, cx + 4 * u + e, by - 16 * u, depth, col);
		break;
	case AJ_BISHOP:
		gfx_ellipse(cx, by - 10 * u, 5 * u + e, 8 * u + e, depth, col);
		gfx_ellipse(cx, by - 19.5f * u, 2.2f * u + e, 2.2f * u + e, depth, col);
		gfx_rect(cx - 6 * u - e, by - 4 * u - e, 12 * u + 2 * e, 2.5f * u + 2 * e, depth, col);
		break;
	case AJ_QUEEN:
		gfx_triangle(cx - 7 * u - e, by - 2 * u + e, cx + 7 * u + e, by - 2 * u + e, cx, by - 15 * u - e, depth, col);
		gfx_rect(cx - 4 * u - e, by - 16 * u - e, 8 * u + 2 * e, 12 * u + 2 * e, depth, col);
		gfx_rect(cx - 7 * u - e, by - 18 * u - e, 14 * u + 2 * e, 3 * u + 2 * e, depth, col);
		/* Corona de picos con bolitas, para no confundirla con la torre */
		for (int k = 0; k < 5; k++) {
			float px = cx - 7 * u + k * 3.5f * u;
			gfx_triangle(px - 2 * u - e, by - 17 * u, px + 2 * u + e, by - 17 * u, px, by - 23 * u - e, depth, col);
			gfx_ellipse(px, by - 23.5f * u, 1.2f * u + e, 1.2f * u + e, depth, col);
		}
		break;
	case AJ_KING:
		gfx_triangle(cx - 7 * u - e, by - 2 * u + e, cx + 7 * u + e, by - 2 * u + e, cx, by - 14 * u - e, depth, col);
		gfx_rect(cx - 5 * u - e, by - 16 * u - e, 10 * u + 2 * e, 14 * u + 2 * e, depth, col);
		gfx_rect(cx - 6.5f * u - e, by - 17 * u - e, 13 * u + 2 * e, 3 * u + 2 * e, depth, col);
		gfx_rect(cx - 1.2f * u - e, by - 25 * u - e, 2.4f * u + 2 * e, 8 * u + 2 * e, depth, col);
		gfx_rect(cx - 4 * u - e, by - 22.5f * u - e, 8 * u + 2 * e, 2.4f * u + 2 * e, depth, col);
		break;
	default:
		break;
	}
}

static void draw_piece(int p, float cx, float cy, float cell, float depth)
{
	if (!p)
		return;
	float u = cell / 29.0f;
	float by = cy + cell * 0.40f;
	bool white = p > 0;
	u32 fill = white ? C2D_Color32(250, 246, 235, 255) : C2D_Color32(45, 40, 45, 255);
	u32 line = white ? C2D_Color32(50, 40, 40, 255) : C2D_Color32(220, 210, 190, 255);
	piece_shape(p < 0 ? -p : p, cx, by, u, 1.0f, depth, line);
	piece_shape(p < 0 ? -p : p, cx, by, u, 0.0f, depth, fill);
}

/* ---------------------------------------------------------------------------
 * Tablero
 * ------------------------------------------------------------------------- */

/* Columna y fila en pantalla de la casilla s (el tablero se gira si llevas negras) */
static void screen_cell(int s, int* col, int* row)
{
	int f = s & 7, r = s >> 3;
	if (s_me > 0) {
		*col = f;
		*row = 7 - r;
	} else {
		*col = 7 - f;
		*row = r;
	}
}

static int cell_square(int col, int row)
{
	return s_me > 0 ? (7 - row) * 8 + col : row * 8 + (7 - col);
}

static void draw_board(float x0, float y0, float cell, float depth, bool interactive)
{
	const Ajedrez* g = &s_game;
	AjMove last = { 0, 0, 0, 0 };
	bool has_last = g->ply > 0;
	if (has_last)
		last = g->undo[g->ply - 1].move;
	int check_king = -1;
	if (aj_in_check(g, g->side))
		for (int s = 0; s < 64; s++)
			if (g->sq[s] == g->side * AJ_KING)
				check_king = s;
	gfx_rect(x0 - 3, y0 - 3, cell * 8 + 6, cell * 8 + 6, depth, C2D_Color32(90, 55, 25, 255));
	for (int s = 0; s < 64; s++) {
		int col, row;
		screen_cell(s, &col, &row);
		float x = x0 + col * cell, y = y0 + row * cell;
		bool light = ((s & 7) + (s >> 3)) % 2 == 1;
		u32 c = light ? C2D_Color32(240, 217, 181, 255) : C2D_Color32(181, 136, 99, 255);
		if (has_last && (s == last.from || s == last.to))
			c = light ? C2D_Color32(246, 236, 140, 255) : C2D_Color32(214, 196, 90, 255);
		if (s == check_king)
			c = C2D_Color32(230, 80, 70, 255);
		if (interactive && s == s_sel)
			c = C2D_Color32(120, 200, 120, 255);
		gfx_rect(x, y, cell, cell, depth, c);
	}
	/* Coordenadas */
	if (!interactive)
		for (int i = 0; i < 8; i++) {
			char file[2] = { (char)('a' + (s_me > 0 ? i : 7 - i)), 0 }, rank[2] = { (char)('8' - (s_me > 0 ? i : 7 - i)), 0 };
			gfx_text(file, x0 + i * cell + cell - 6, y0 + 8 * cell - 13, depth, 0.4f, C2D_Color32(90, 60, 40, 255),
				C2D_AlignCenter);
			gfx_text(rank, x0 + 4, y0 + i * cell + 1, depth, 0.4f, C2D_Color32(90, 60, 40, 255), C2D_AlignCenter);
		}
	/* Destinos de la pieza elegida */
	if (interactive && s_sel >= 0) {
		AjMove moves[AJ_MAX_MOVES];
		int n = aj_legal_moves(&s_game, moves);
		for (int i = 0; i < n; i++)
			if (moves[i].from == s_sel) {
				int col, row;
				screen_cell(moves[i].to, &col, &row);
				float cx = x0 + col * cell + cell / 2, cy = y0 + row * cell + cell / 2;
				if (g->sq[moves[i].to])
					gfx_ellipse(cx, cy, cell * 0.45f, cell * 0.45f, depth, C2D_Color32(40, 120, 40, 110));
				else
					gfx_ellipse(cx, cy, cell * 0.16f, cell * 0.16f, depth, C2D_Color32(40, 120, 40, 160));
			}
	}
	for (int s = 0; s < 64; s++) {
		if (!g->sq[s])
			continue;
		int col, row;
		screen_cell(s, &col, &row);
		draw_piece(g->sq[s], x0 + col * cell + cell / 2, y0 + row * cell + cell / 2, cell, depth);
	}
	if (interactive && s_mode == MODE_PLAY && s_status == AJ_PLAYING) {
		int col, row;
		screen_cell(s_cursor, &col, &row);
		float pulse = 0.5f + 0.5f * sinf(gfx_time() * 6.0f);
		u32 k = C2D_Color32(255, 200, 40, (u8)(150 + 100 * pulse));
		float x = x0 + col * cell, y = y0 + row * cell;
		gfx_rect(x, y, cell, 2, depth, k);
		gfx_rect(x, y + cell - 2, cell, 2, depth, k);
		gfx_rect(x, y, 2, cell, depth, k);
		gfx_rect(x + cell - 2, y, 2, cell, depth, k);
	}
}

/* ---------------------------------------------------------------------------
 * Partida
 * ------------------------------------------------------------------------- */

static void set_message(const char* m)
{
	snprintf(s_message, sizeof s_message, "%s", m);
}

static void start_game(void)
{
	aj_init(&s_game, (uint32_t)time(NULL) ^ 0xC4E55);
	s_me = s_color_choice == 0 ? 1 : s_color_choice == 1 ? -1 : (rng_range(&s_game.rng, 2) ? 1 : -1);
	s_sel = -1;
	s_cursor = s_me > 0 ? 12 : 52;
	s_promo_from = s_promo_to = -1;
	s_status = AJ_PLAYING;
	s_recorded = s_resigned = false;
	s_think = 0;
	s_mode = MODE_PLAY;
	set_message(s_me > 0 ? "Llevas las blancas: empiezas tú" : "Llevas las negras: empieza tu rival");
}

static void aj_enter(void)
{
	if (!s_started) {
		s_started = true;
		s_mode = MODE_SETUP;
	}
	button_set(&s_btn_menu, 262, 2, 54, 20, "Menú", C2D_Color32(120, 30, 35, 255), KEY_START);
}

static void play(AjMove m)
{
	bool capture = m.flags & AJ_F_CAPTURE;
	aj_move_name(&s_game, m, s_names[s_game.ply], sizeof s_names[0]);
	aj_make(&s_game, m);
	s_status = aj_status(&s_game);
	audio_play(capture ? SFX_CHIP : SFX_CARD);
	s_sel = -1;
	if (s_status != AJ_PLAYING) {
		s_end_time = 0.0f;
	} else if (aj_in_check(&s_game, s_game.side)) {
		set_message(s_game.side == s_me ? "¡Jaque! Protege a tu rey" : "¡Jaque!");
	} else {
		char buf[64];
		snprintf(buf, sizeof buf, "%s: %s", s_game.side == s_me ? "Tu rival juega" : "Juegas", s_names[s_game.ply - 1]);
		set_message(buf);
	}
}

/* Toca o pulsa Ⓐ en la casilla s */
static void touch_square(int s)
{
	Ajedrez* g = &s_game;
	if (s_sel >= 0 && s != s_sel) {
		AjMove m;
		if (aj_find_move(g, s_sel, s, 0, &m)) {
			if (m.promo) {
				s_promo_from = s_sel;
				s_promo_to = s;
				set_message("¿En qué pieza se convierte tu peón?");
				return;
			}
			play(m);
			return;
		}
	}
	if (g->sq[s] && (g->sq[s] > 0) == (s_me > 0)) {
		s_sel = s == s_sel ? -1 : s;
		audio_play(SFX_SELECT);
	} else if (s_sel >= 0) {
		s_sel = -1;
		audio_play(SFX_ERROR);
	}
}

static SceneId setup_update(const Input* in)
{
	for (int i = 0; i < AJ_LEVELS; i++)
		button_set(&s_btn_level[i], 8 + (i % 2) * 154, 44 + (i / 2) * 34, 150, 30, LEVEL_NAME[i],
			i == s_level ? C2D_Color32(220, 150, 30, 255) : C2D_Color32(60, 90, 160, 255), 0);
	static const char* const COLOR[3] = { "Blancas", "Negras", "Al azar" };
	for (int i = 0; i < 3; i++)
		button_set(&s_btn_color[i], 8 + i * 103, 152, 98, 28, COLOR[i],
			i == s_color_choice ? C2D_Color32(220, 150, 30, 255) : C2D_Color32(40, 110, 120, 255), 0);
	button_set(&s_btn_play, 60, 200, 200, 30, GLYPH_A " Jugar", C2D_Color32(40, 140, 60, 255), KEY_A);
	if (in->down & KEY_UP)
		s_level = (s_level + AJ_LEVELS - 2) % AJ_LEVELS;
	if (in->down & KEY_DOWN)
		s_level = (s_level + 2) % AJ_LEVELS;
	if (in->down & KEY_LEFT)
		s_level = (s_level + AJ_LEVELS - 1) % AJ_LEVELS;
	if (in->down & KEY_RIGHT)
		s_level = (s_level + 1) % AJ_LEVELS;
	if (in->down & (KEY_L | KEY_R))
		s_color_choice = (s_color_choice + 1) % 3;
	for (int i = 0; i < AJ_LEVELS; i++)
		if (button_update(&s_btn_level[i], in))
			s_level = i;
	for (int i = 0; i < 3; i++)
		if (button_update(&s_btn_color[i], in))
			s_color_choice = i;
	if (button_update(&s_btn_play, in))
		start_game();
	return SCENE_AJEDREZ;
}

static SceneId play_update(const Input* in, float dt)
{
	Ajedrez* g = &s_game;
	if (s_status != AJ_PLAYING) {
		s_end_time += dt;
		if (!s_recorded) {
			s_recorded = true;
			bool won = s_status == AJ_CHECKMATE && g->side != s_me;
			bool lost = s_resigned || (s_status == AJ_CHECKMATE && g->side == s_me);
			profile_record_game(GAME_AJEDREZ, won);
			audio_play(won ? SFX_WIN : lost ? SFX_LOSE : SFX_SELECT);
			mood_set(&s_mood, won ? EXPR_SAD : lost ? EXPR_HAPPY : EXPR_NORMAL, 30.0f);
		}
		button_set(&s_btn_play, 60, 205, 200, 28, GLYPH_A " Otra partida", C2D_Color32(40, 140, 60, 255), KEY_A);
		s_btn_play.enabled = s_end_time > 0.8f;
		if (button_update(&s_btn_play, in))
			s_mode = MODE_SETUP;
		return SCENE_AJEDREZ;
	}
	/* Turno de la consola: se pinta "Pensando..." y luego se busca */
	if (g->side != s_me) {
		if (s_think == 0)
			set_message("Tu rival está pensando...");
		if (++s_think >= 3) {
			s_think = 0;
			AjMove m = aj_search(g, s_level);
			play(m);
			mood_set(&s_mood, (m.flags & AJ_F_CAPTURE) ? EXPR_HAPPY : EXPR_NORMAL, 1.5f);
		}
		return SCENE_AJEDREZ;
	}
	/* Coronación */
	if (s_promo_from >= 0) {
		static const char* const PIECE[4] = { "Dama", "Torre", "Alfil", "Caballo" };
		static const int KIND[4] = { AJ_QUEEN, AJ_ROOK, AJ_BISHOP, AJ_KNIGHT };
		for (int i = 0; i < 4; i++) {
			button_set(&s_btn_promo[i], 222, 40 + i * 34, 94, 30, PIECE[i], C2D_Color32(60, 90, 160, 255), 0);
			AjMove m;
			if (button_update(&s_btn_promo[i], in) && aj_find_move(g, s_promo_from, s_promo_to, KIND[i], &m)) {
				s_promo_from = s_promo_to = -1;
				play(m);
				return SCENE_AJEDREZ;
			}
		}
		if (in->down & KEY_B)
			s_promo_from = s_promo_to = -1;
		return SCENE_AJEDREZ;
	}
	button_set(&s_btn_undo, 222, 150, 94, 26, GLYPH_Y " Deshacer", C2D_Color32(200, 140, 30, 255), KEY_Y);
	button_set(&s_btn_resign, 222, 182, 94, 26, "Rendirse", C2D_Color32(150, 40, 40, 255), 0);
	s_btn_undo.enabled = g->ply >= 2;
	if (button_update(&s_btn_undo, in)) {
		aj_unmake(g);
		aj_unmake(g);
		s_sel = -1;
		set_message("Jugada deshecha");
		audio_play(SFX_FLIP);
		return SCENE_AJEDREZ;
	}
	if (button_update(&s_btn_resign, in)) {
		s_resigned = true;
		s_status = AJ_CHECKMATE; /* se cuenta como perdida */
		s_end_time = 0.0f;
		set_message("Te rindes");
		return SCENE_AJEDREZ;
	}
	int col, row;
	screen_cell(s_cursor, &col, &row);
	if (in->repeat & KEY_LEFT)
		col = (col + 7) % 8;
	if (in->repeat & KEY_RIGHT)
		col = (col + 1) % 8;
	if (in->repeat & KEY_UP)
		row = (row + 7) % 8;
	if (in->repeat & KEY_DOWN)
		row = (row + 1) % 8;
	s_cursor = cell_square(col, row);
	if (in->down & KEY_A)
		touch_square(s_cursor);
	if (in->down & KEY_B)
		s_sel = -1;
	if (in->touch_down) {
		int c = (int)floorf((in->tx - B_X) / B_CELL), r = (int)floorf((in->ty - B_Y) / B_CELL);
		if (c >= 0 && c < 8 && r >= 0 && r < 8) {
			s_cursor = cell_square(c, r);
			touch_square(s_cursor);
		}
	}
	return SCENE_AJEDREZ;
}

static SceneId aj_update(const Input* in, float dt)
{
	if (button_update(&s_btn_menu, in))
		return SCENE_MENU;
	mood_update(&s_mood, dt);
	return s_mode == MODE_SETUP ? setup_update(in) : play_update(in, dt);
}

static const char* status_text(void)
{
	switch (s_status) {
	case AJ_CHECKMATE:
		if (s_resigned)
			return "Te has rendido";
		return s_game.side == s_me ? "Jaque mate: gana tu rival" : "¡Jaque mate! ¡Has ganado!";
	case AJ_STALEMATE:
		return "Rey ahogado: tablas";
	case AJ_DRAW_MATERIAL:
		return "Tablas: no queda material para dar mate";
	case AJ_DRAW_50:
		return "Tablas por la regla de los 50 movimientos";
	case AJ_DRAW_REPETITION:
		return "Tablas por triple repetición";
	default:
		return "";
	}
}

static void aj_draw_top(void)
{
	gfx_background(BG_TABLE_TOP);
	if (s_mode == MODE_SETUP) {
		gfx_text("Ajedrez", 200, 20, DEPTH_UI, 1.0f, COLOR_GOLD, C2D_AlignCenter);
		static const int ROW[8] = { AJ_ROOK, AJ_KNIGHT, AJ_BISHOP, AJ_QUEEN, AJ_KING, AJ_BISHOP, AJ_KNIGHT, AJ_ROOK };
		for (int i = 0; i < 8; i++) {
			gfx_rect(80 + i * 30, 70, 30, 30, DEPTH_TABLE,
				i % 2 ? C2D_Color32(181, 136, 99, 255) : C2D_Color32(240, 217, 181, 255));
			gfx_rect(80 + i * 30, 100, 30, 30, DEPTH_TABLE,
				i % 2 ? C2D_Color32(240, 217, 181, 255) : C2D_Color32(181, 136, 99, 255));
			draw_piece(-ROW[i], 95 + i * 30, 85, 30, DEPTH_CARDS);
			draw_piece(AJ_PAWN, 95 + i * 30, 115, 30, DEPTH_CARDS);
		}
		mii_draw(cpu_avatar(RIVAL), 200, 168, 18, DEPTH_UI, EXPR_NORMAL, false);
		gfx_textf(200, 194, DEPTH_UI, 0.5f, COLOR_CREAM, C2D_AlignCenter, "%s · %s", cpu_name(RIVAL), LEVEL_NAME[s_level]);
		gfx_text(LEVEL_INFO[s_level], 200, 214, DEPTH_UI, 0.42f, C2D_Color32(200, 230, 200, 255), C2D_AlignCenter);
		return;
	}
	draw_board(T_X, T_Y, T_CELL, DEPTH_TABLE, false);
	/* Rival y jugadas */
	float px = 246;
	gfx_panel(px, 10, 146, 50, DEPTH_UI, C2D_Color32(10, 50, 25, 220),
		s_game.side != s_me && s_status == AJ_PLAYING ? COLOR_GOLD : C2D_Color32(20, 70, 35, 255));
	mii_draw(cpu_avatar(RIVAL), px + 22, 35, 14, DEPTH_UI, mood_expr(&s_mood), false);
	gfx_text(cpu_name(RIVAL), px + 42, 16, DEPTH_UI, 0.45f, COLOR_CREAM, C2D_AlignLeft);
	gfx_text(LEVEL_NAME[s_level], px + 42, 34, DEPTH_UI, 0.42f, COLOR_GOLD, C2D_AlignLeft);
	gfx_panel(px, 64, 146, 166, DEPTH_UI, C2D_Color32(20, 30, 25, 220), C2D_Color32(150, 110, 40, 255));
	int ply = s_game.ply, first_move = ((ply > 20 ? ply - 20 : 0) / 2) * 2;
	for (int i = first_move, line = 0; i < ply; i += 2, line++) {
		float y = 70 + line * 15;
		gfx_textf(px + 6, y, DEPTH_UI, 0.42f, C2D_Color32(200, 230, 200, 255), C2D_AlignLeft, "%d.", i / 2 + 1);
		gfx_text(s_names[i], px + 34, y, DEPTH_UI, 0.42f, COLOR_CREAM, C2D_AlignLeft);
		if (i + 1 < ply)
			gfx_text(s_names[i + 1], px + 92, y, DEPTH_UI, 0.42f, COLOR_CREAM, C2D_AlignLeft);
	}
	if (s_status != AJ_PLAYING && banner_draw(s_end_time, 128, 120, 236, 60))
		gfx_text_wrap(status_text(), 20, 104, DEPTH_BANNER, 0.5f, COLOR_GOLD, 216);
}

static void aj_draw_bottom(void)
{
	gfx_background(BG_TABLE_BOTTOM);
	button_draw(&s_btn_menu, false);
	if (s_mode == MODE_SETUP) {
		gfx_text("Elige la dificultad", BOTTOM_W / 2, 22, 0, 0.5f, COLOR_GOLD, C2D_AlignCenter);
		for (int i = 0; i < AJ_LEVELS; i++)
			button_draw(&s_btn_level[i], i == s_level);
		gfx_text("Y tu color (L/R)", BOTTOM_W / 2, 130, 0, 0.45f, COLOR_GOLD, C2D_AlignCenter);
		for (int i = 0; i < 3; i++)
			button_draw(&s_btn_color[i], i == s_color_choice);
		button_draw(&s_btn_play, true);
		return;
	}
	gfx_text_wrap(s_message, 8, 3, 0, 0.42f, COLOR_GOLD, 246);
	draw_board(B_X, B_Y, B_CELL, 0, true);
	if (s_status != AJ_PLAYING) {
		button_draw(&s_btn_play, s_btn_play.enabled);
		return;
	}
	if (s_promo_from >= 0) {
		for (int i = 0; i < 4; i++)
			button_draw(&s_btn_promo[i], true);
		return;
	}
	gfx_text(s_game.side == s_me ? "Te toca" : "Piensa...", 269, 40, 0, 0.5f, COLOR_GOLD, C2D_AlignCenter);
	gfx_text(s_me > 0 ? "Blancas" : "Negras", 269, 62, 0, 0.45f, COLOR_CREAM, C2D_AlignCenter);
	/* Piezas capturadas por ti */
	static const int START[7] = { 0, 8, 2, 2, 2, 1, 1 };
	int count[7] = { 0 };
	for (int s = 0; s < 64; s++)
		if (s_game.sq[s] && (s_game.sq[s] > 0) != (s_me > 0))
			count[s_game.sq[s] < 0 ? -s_game.sq[s] : s_game.sq[s]]++;
	int k = 0;
	for (int kind = AJ_QUEEN; kind >= AJ_PAWN; kind--)
		for (int i = count[kind]; i < START[kind]; i++, k++)
			draw_piece(-s_me * kind, 228 + (k % 6) * 15, 92 + (k / 6) * 18, 15, 0);
	button_draw(&s_btn_undo, s_btn_undo.enabled);
	button_draw(&s_btn_resign, false);
}

/* Salir a medias desde la pausa: la próxima vez se empieza de cero */
static void scene_leave(void)
{
	s_started = false;
}

const Scene scene_ajedrez = { aj_enter, aj_update, aj_draw_top, aj_draw_bottom, scene_leave };
