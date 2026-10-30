/* Solitario tradicional: toda la mesa en la pantalla táctil; arriba, los
 * cuatro palos en grande y el marcador. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "games/solitario.h"
#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/gfx.h"
#include "ui/mii.h"
#include "ui/widgets.h"

#define SC        0.62f /* escala de las cartas en la pantalla táctil */
#define CW        (CARD_W * SC)
#define CH        (CARD_H * SC)
#define COL_X(k)  (22.0f + (k) * 46.0f)
#define ROW_Y     34.0f
#define TAB_Y     104.0f
#define TAB_BOTTOM 238.0f
#define DOWN_STEP 6.0f
#define UP_STEP   15.0f
#define UNDO_MAX  64

static Solitario s_game;
static Solitario s_undo[UNDO_MAX];
static int s_undo_count;
static bool s_started;
static int s_cursor; /* montón señalado (SolPile) */
static int s_depth;  /* carta señalada dentro de una columna */
static int s_held_pile = -1, s_held_index = -1;
static float s_auto_timer;
static float s_time;
static float s_end_time;
static bool s_recorded;
static Mood s_mood;
static char s_message[96];
static float s_message_flash;
static Button s_btn_menu, s_btn_undo;

static void set_message(const char* msg)
{
	snprintf(s_message, sizeof s_message, "%s", msg);
	s_message_flash = 1.0f;
}

static bool is_col(int pile)
{
	return pile >= SOL_PILE_COLUMN;
}

static float pile_x(int pile)
{
	if (pile == SOL_PILE_STOCK)
		return COL_X(0);
	if (pile == SOL_PILE_WASTE)
		return COL_X(1);
	if (pile < SOL_PILE_COLUMN)
		return COL_X(3 + pile - SOL_PILE_FOUNDATION);
	return COL_X(pile - SOL_PILE_COLUMN);
}

/* Separación entre cartas de una columna para que quepa en la pantalla */
static void col_steps(int k, float* down, float* up)
{
	const SolColumn* c = &s_game.col[k];
	*down = DOWN_STEP;
	*up = UP_STEP;
	int nd = c->hidden, nu = c->count - c->hidden - 1;
	if (nu < 0)
		nu = 0;
	float avail = TAB_BOTTOM - CH / 2.0f - TAB_Y;
	float need = nd * *down + nu * *up;
	if (need > avail && nu > 0)
		*up = fmaxf(5.0f, (avail - nd * *down) / nu);
}

static float card_y(int k, int i)
{
	float down, up;
	col_steps(k, &down, &up);
	const SolColumn* c = &s_game.col[k];
	int nd = i < c->hidden ? i : c->hidden;
	int nu = i > c->hidden ? i - c->hidden : 0;
	return TAB_Y + nd * down + nu * up;
}

static void push_undo(void)
{
	if (s_undo_count == UNDO_MAX) {
		memmove(s_undo, s_undo + 1, sizeof s_undo[0] * (UNDO_MAX - 1));
		s_undo_count--;
	}
	s_undo[s_undo_count++] = s_game;
}

static void clamp_depth(void)
{
	if (!is_col(s_cursor))
		return;
	const SolColumn* c = &s_game.col[s_cursor - SOL_PILE_COLUMN];
	if (c->count == 0) {
		s_depth = 0;
		return;
	}
	if (s_depth < c->hidden)
		s_depth = c->hidden;
	if (s_depth > c->count - 1)
		s_depth = c->count - 1;
}

static void new_game(void)
{
	sol_new_game(&s_game);
	s_undo_count = 0;
	s_held_pile = s_held_index = -1;
	s_cursor = SOL_PILE_STOCK;
	s_depth = 0;
	s_time = 0.0f;
	s_end_time = 0.0f;
	s_recorded = false;
	set_message("Lleva todas las cartas a sus palos, del as al rey");
}

static void sol_scene_enter(void)
{
	if (!s_started) {
		sol_init(&s_game, (uint32_t)time(NULL) ^ 0x501174);
		s_started = true;
		new_game();
	}
	button_set(&s_btn_menu, COL_X(2) - 20, 6, 40, 20, "Menú", C2D_Color32(120, 30, 35, 255), KEY_START);
	button_set(&s_btn_undo, COL_X(2) - 20, 38, 40, 20, GLYPH_L, C2D_Color32(60, 90, 160, 255), KEY_L);
}

static bool try_move(int from, int index, int to)
{
	Solitario before = s_game;
	if (!sol_move(&s_game, from, index, to))
		return false;
	s_game = before;
	push_undo();
	sol_move(&s_game, from, index, to);
	audio_play(to < SOL_PILE_COLUMN ? SFX_CHIP : SFX_CARD);
	return true;
}

static void release(void)
{
	s_held_pile = s_held_index = -1;
}

/* Coge la carta (y las de encima) o, si ya hay una cogida, la deja aquí */
static void act_on(int pile, int index)
{
	if (pile == SOL_PILE_STOCK) {
		release();
		push_undo();
		if (sol_draw(&s_game))
			audio_play(SFX_FLIP);
		else {
			s_undo_count--;
			audio_play(SFX_ERROR);
		}
		return;
	}
	if (s_held_pile >= 0) {
		int from = s_held_pile, idx = s_held_index;
		release();
		if (pile == from) {
			/* Volver a tocar la misma carta: va sola a su sitio */
			int to = sol_best_target(&s_game, from, idx);
			if (to >= 0 && try_move(from, idx, to))
				return;
			audio_play(SFX_ERROR);
			set_message("Esa carta no tiene dónde ir");
			return;
		}
		if (try_move(from, idx, pile))
			return;
		audio_play(SFX_ERROR);
		set_message("Ahí no se puede poner");
		return;
	}
	if (index < 0) {
		audio_play(SFX_ERROR);
		return;
	}
	if (is_col(pile) && index < s_game.col[pile - SOL_PILE_COLUMN].hidden) {
		audio_play(SFX_ERROR);
		return;
	}
	if (is_col(pile) && index < sol_first_movable(&s_game, pile - SOL_PILE_COLUMN))
		index = sol_first_movable(&s_game, pile - SOL_PILE_COLUMN);
	s_held_pile = pile;
	s_held_index = index;
	audio_play(SFX_SELECT);
}

/* Montón y carta bajo el lápiz */
static bool touched(const Input* in, int* pile, int* index)
{
	if (!in->touch_down)
		return false;
	float tx = in->tx, ty = in->ty;
	if (ty < ROW_Y + CH / 2.0f + 4.0f) {
		static const int top[] = { SOL_PILE_STOCK, SOL_PILE_WASTE, SOL_PILE_FOUNDATION, SOL_PILE_FOUNDATION + 1,
			SOL_PILE_FOUNDATION + 2, SOL_PILE_FOUNDATION + 3 };
		for (unsigned i = 0; i < sizeof top / sizeof top[0]; i++) {
			if (fabsf(tx - pile_x(top[i])) <= CW / 2.0f + 2.0f) {
				*pile = top[i];
				*index = sol_top_index(&s_game, top[i]);
				return true;
			}
		}
		return false;
	}
	for (int k = 0; k < SOL_COLUMNS; k++) {
		if (fabsf(tx - COL_X(k)) > CW / 2.0f + 2.0f)
			continue;
		const SolColumn* c = &s_game.col[k];
		*pile = SOL_PILE_COLUMN + k;
		*index = -1;
		for (int i = c->count - 1; i >= 0; i--) {
			float y = card_y(k, i);
			if (ty >= y - CH / 2.0f && ty <= y + CH / 2.0f) {
				*index = i;
				break;
			}
		}
		return true;
	}
	return false;
}

static void move_cursor(int dx, int dy)
{
	static const int top[] = { SOL_PILE_STOCK, SOL_PILE_WASTE, -1, SOL_PILE_FOUNDATION, SOL_PILE_FOUNDATION + 1,
		SOL_PILE_FOUNDATION + 2, SOL_PILE_FOUNDATION + 3 };
	int col = 0;
	for (int k = 0; k < SOL_COLUMNS; k++)
		if (fabsf(pile_x(s_cursor) - COL_X(k)) < 1.0f)
			col = k;
	if (dx) {
		if (is_col(s_cursor)) {
			s_cursor = SOL_PILE_COLUMN + (col + dx + SOL_COLUMNS) % SOL_COLUMNS;
			s_depth = sol_first_movable(&s_game, s_cursor - SOL_PILE_COLUMN);
		} else {
			do
				col = (col + dx + SOL_COLUMNS) % SOL_COLUMNS;
			while (top[col] < 0);
			s_cursor = top[col];
		}
	} else if (dy) {
		if (!is_col(s_cursor)) {
			if (dy > 0) {
				s_cursor = SOL_PILE_COLUMN + col;
				s_depth = sol_first_movable(&s_game, col);
			}
		} else {
			const SolColumn* c = &s_game.col[col];
			int first = c->count ? c->hidden : 0;
			if (dy < 0 && s_depth <= first) {
				s_cursor = top[col] >= 0 ? top[col] : SOL_PILE_WASTE;
			} else {
				s_depth += dy;
			}
		}
	}
	clamp_depth();
	audio_play(SFX_SELECT);
}

static int cursor_index(void)
{
	if (is_col(s_cursor))
		return s_game.col[s_cursor - SOL_PILE_COLUMN].count ? s_depth : -1;
	return sol_top_index(&s_game, s_cursor);
}

static SceneId sol_scene_update(const Input* in, float dt)
{
	if (button_update(&s_btn_menu, in))
		return SCENE_MENU;
	bool over = s_game.won;
	s_btn_undo.enabled = s_undo_count > 0 && !over;

	if (in->down & KEY_Y) {
		if (!s_recorded && s_game.moves > 0)
			profile_record_game(GAME_SOLITARIO, false); /* abandonada: cuenta como perdida */
		new_game();
		return SCENE_SOLITARIO;
	}

	if (!over) {
		s_time += dt;
		if (button_update(&s_btn_undo, in)) {
			release();
			s_game = s_undo[--s_undo_count];
			clamp_depth();
		}
		if (sol_can_finish(&s_game)) {
			/* Ya está todo descubierto: las cartas suben solas */
			release();
			s_auto_timer += dt;
			if (s_auto_timer > 0.09f) {
				s_auto_timer = 0.0f;
				if (sol_auto_step(&s_game))
					audio_play(SFX_CHIP);
			}
		} else {
			int dx = (in->repeat & KEY_RIGHT) ? 1 : (in->repeat & KEY_LEFT) ? -1 : 0;
			int dy = (in->repeat & KEY_DOWN) ? 1 : (in->repeat & KEY_UP) ? -1 : 0;
			if (dx || dy)
				move_cursor(dx, dy);
			int pile, index;
			if (touched(in, &pile, &index)) {
				s_cursor = pile;
				s_depth = index < 0 ? 0 : index;
				clamp_depth();
				act_on(pile, index);
			} else if (in->down & KEY_A) {
				act_on(s_cursor, cursor_index());
			} else if (in->down & KEY_B) {
				release();
			} else if (in->down & KEY_X) {
				/* Mandar la carta señalada a su sitio */
				int from = s_held_pile >= 0 ? s_held_pile : s_cursor;
				int idx = s_held_pile >= 0 ? s_held_index : cursor_index();
				release();
				int to = idx >= 0 ? sol_best_target(&s_game, from, idx) : -1;
				if (to < 0 || !try_move(from, idx, to))
					audio_play(SFX_ERROR);
			} else if (in->down & KEY_R) {
				act_on(SOL_PILE_STOCK, -1);
			}
		}
		clamp_depth();
	}

	if (s_game.won && !s_recorded) {
		s_recorded = true;
		profile_record_game(GAME_SOLITARIO, true);
		audio_play(SFX_WIN);
		mood_set(&s_mood, EXPR_HAPPY, 6.0f);
		set_message("¡Solitario completado!");
	}
	if (s_game.won) {
		s_end_time += dt;
		if (in->down & KEY_A)
			new_game();
	} else if (!sol_has_moves(&s_game)) {
		set_message("No quedan jugadas: " GLYPH_L " deshacer o Y nueva partida");
	}
	mood_update(&s_mood, dt);
	if (s_message_flash > 0.0f)
		s_message_flash -= dt * 2.0f;
	return SCENE_SOLITARIO;
}

/* ---------------------------------------------------------------------------
 * Dibujo
 * ------------------------------------------------------------------------- */

static void empty_slot(float x, float y, float scale, float depth, int suit)
{
	gfx_rrect(x - CARD_W * scale / 2.0f, y - CARD_H * scale / 2.0f, CARD_W * scale, CARD_H * scale,
		5, depth, C2D_Color32(0, 0, 0, 60));
	if (suit >= 0) {
		C2D_ImageTint tint;
		C2D_AlphaImageTint(&tint, 0.35f);
		gfx_sprite_ex(SPR_SUIT_FIRST + suit, x, y, scale * 1.2f, scale * 1.2f, 0.0f, depth, &tint);
	}
}

static void sol_scene_draw_top(void)
{
	gfx_background(BG_TABLE_TOP);
	gfx_text("Solitario", 200, 8, DEPTH_UI, 0.6f, COLOR_GOLD, C2D_AlignCenter);

	/* Los cuatro palos en grande */
	for (int s = 0; s < NUM_SUITS; s++) {
		float x = 80 + s * 80.0f, y = 100;
		int n = s_game.foundation[s];
		if (n == 0)
			empty_slot(x, y, 1.0f, DEPTH_TABLE, s);
		else
			gfx_card(card_make((Suit)s, rank_from_pos(n - 1)), x, y, 1.0f, 1.0f, 0.0f, DEPTH_CARDS + n * 0.05f, 0.0f,
				true);
		gfx_textf(x, y + 54, DEPTH_UI, 0.42f, COLOR_CREAM, C2D_AlignCenter, "%d de 10", n);
	}

	gfx_panel(20, 176, 360, 54, DEPTH_UI, C2D_Color32(10, 50, 25, 200), C2D_Color32(20, 60, 30, 255));
	mii_draw(&profile_get()->avatar, 46, 203, 16, DEPTH_UI + 0.4f, mood_expr(&s_mood), false);
	int secs = (int)s_time;
	gfx_textf(72, 182, DEPTH_UI, 0.45f, COLOR_CREAM, C2D_AlignLeft, "Movimientos: %d   Tiempo: %d:%02d",
		s_game.moves, secs / 60, secs % 60);
	gfx_text(s_message, 72, 202, DEPTH_UI, 0.45f, s_message_flash > 0.0f ? COLOR_GOLD : C2D_Color32(200, 230, 200, 255),
		C2D_AlignLeft);

	if (s_game.won && banner_draw(s_end_time, 200, 110, 300, 64)) {
		gfx_text("¡Solitario completado!", 200, 82, DEPTH_BANNER, 0.75f, COLOR_GOLD, C2D_AlignCenter);
		gfx_textf(200, 112, DEPTH_BANNER, 0.5f, COLOR_CREAM, C2D_AlignCenter, "%d movimientos.  " GLYPH_A " Otra partida",
			s_game.moves);
	}
}

static void draw_highlight(float x, float y, bool held)
{
	float pulse = 0.6f + 0.4f * sinf(gfx_time() * 6.0f);
	C2D_ImageTint tint;
	C2D_AlphaImageTint(&tint, held ? 1.0f : pulse);
	gfx_sprite_ex(SPR_CARD_GLOW, x, y, SC, SC, 0.0f, 0, &tint);
}

static void sol_scene_draw_bottom(void)
{
	gfx_background(BG_TABLE_BOTTOM);
	button_draw(&s_btn_menu, false);
	button_draw(&s_btn_undo, s_btn_undo.enabled);

	/* Cursor de la cruceta en la fila de arriba (detrás de la carta) */
	if (!is_col(s_cursor) && s_held_pile != s_cursor)
		draw_highlight(pile_x(s_cursor), ROW_Y, false);

	/* Mazo y descartes */
	if (s_game.stock_count > 0)
		gfx_card_back(COL_X(0), ROW_Y, SC, SC, 0.0f, 0, NULL);
	else
		empty_slot(COL_X(0), ROW_Y, SC, 0, -1);
	gfx_textf(COL_X(0), ROW_Y + CH / 2.0f + 1, 0, 0.4f, COLOR_CREAM, C2D_AlignCenter, "%d", s_game.stock_count);
	if (s_game.waste_count > 1)
		gfx_card(s_game.waste[s_game.waste_count - 2], COL_X(1) - 3, ROW_Y, SC, 1.0f, 0.0f, 0, 0.25f, false);
	if (s_game.waste_count > 0) {
		if (s_held_pile == SOL_PILE_WASTE)
			draw_highlight(COL_X(1), ROW_Y, true);
		gfx_card(s_game.waste[s_game.waste_count - 1], COL_X(1), ROW_Y, SC, 1.0f, 0.0f, 0, 0.0f, true);
	} else {
		empty_slot(COL_X(1), ROW_Y, SC, 0, -1);
	}
	/* Palos */
	for (int s = 0; s < NUM_SUITS; s++) {
		int pile = SOL_PILE_FOUNDATION + s;
		int n = s_game.foundation[s];
		if (n == 0) {
			empty_slot(pile_x(pile), ROW_Y, SC, 0, s);
			continue;
		}
		if (s_held_pile == pile)
			draw_highlight(pile_x(pile), ROW_Y, true);
		gfx_card(card_make((Suit)s, rank_from_pos(n - 1)), pile_x(pile), ROW_Y, SC, 1.0f, 0.0f, 0, 0.0f, true);
	}

	/* Columnas */
	for (int k = 0; k < SOL_COLUMNS; k++) {
		const SolColumn* c = &s_game.col[k];
		int pile = SOL_PILE_COLUMN + k;
		if (c->count == 0) {
			empty_slot(COL_X(k), TAB_Y, SC, 0, -1);
			if (s_cursor == pile)
				draw_highlight(COL_X(k), TAB_Y, false);
		}
		for (int i = 0; i < c->count; i++) {
			float y = card_y(k, i);
			bool held = s_held_pile == pile && i >= s_held_index;
			bool pointed = s_cursor == pile && s_held_pile < 0 && i == s_depth;
			if (held || pointed)
				draw_highlight(COL_X(k), y, held);
			gfx_card(c->cards[i], COL_X(k), y + (held ? -2.0f : 0.0f), SC, i >= c->hidden ? 1.0f : 0.0f, 0.0f, 0, 0.0f,
				i == 0);
		}
	}
}

/* Salir a medias desde la pausa: la próxima vez se empieza de cero */
static void scene_leave(void)
{
	s_started = false;
}

const Scene scene_solitario = { sol_scene_enter, sol_scene_update, sol_scene_draw_top, sol_scene_draw_bottom, scene_leave };
