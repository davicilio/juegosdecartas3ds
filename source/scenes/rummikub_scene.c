/* Rummikub: la mesa con las jugadas arriba y tu atril en la pantalla táctil. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "games/rummikub.h"
#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/gfx.h"
#include "ui/mii.h"
#include "ui/widgets.h"

static const CpuId SEAT_CPU[RK_PLAYERS] = { CPU_COUNT, CPU_PACO, CPU_LUCIA, CPU_MANOLO };
static const u32 TILE_RGB[RK_COLORS] = { 0x23232A, 0xD2282D, 0x285AC8, 0xE6821A };

#define TILE_W 26.0f
#define TILE_H 34.0f
#define RACK_Y 46.0f

static Rummikub s_game;
static bool s_started;
static bool s_sel[RK_MAX_RACK];
static int s_cursor;
static bool s_by_number;
static int s_add_from; /* para probar las jugadas de la mesa por turnos */
static float s_cpu_timer, s_result_time, s_highlight;
static RkPhase s_prev_phase;
static Mood s_mood[RK_PLAYERS];
static char s_message[96];
static float s_message_flash;
static Button s_btn_play, s_btn_add, s_btn_undo, s_btn_end, s_btn_menu, s_btn_sort;

static const char* seat_name(int p)
{
	return p == 0 ? profile_get()->avatar.name : cpu_name(SEAT_CPU[p]);
}

static const Avatar* seat_avatar(int p)
{
	return p == 0 ? &profile_get()->avatar : cpu_avatar(SEAT_CPU[p]);
}

static void set_message(const char* msg)
{
	snprintf(s_message, sizeof s_message, "%s", msg);
	s_message_flash = 1.0f;
}

static void clear_selection(void)
{
	memset(s_sel, 0, sizeof s_sel);
}

static void reset_round(void)
{
	clear_selection();
	s_cursor = 0;
	s_cpu_timer = s_result_time = s_highlight = 0.0f;
	s_prev_phase = s_game.phase;
	set_message(s_game.current == 0 ? "Empiezas tú: baja jugadas que sumen al menos 30" : "Nueva ronda");
}

static void rk_scene_enter(void)
{
	if (!s_started) {
		rk_init(&s_game, (uint32_t)time(NULL) ^ 0x52AB);
		s_started = true;
		reset_round();
	}
	button_set(&s_btn_menu, 258, 4, 54, 20, "Menú", C2D_Color32(120, 30, 35, 255), KEY_START);
	button_set(&s_btn_sort, 172, 4, 80, 20, "Ordenar", C2D_Color32(60, 90, 160, 255), KEY_SELECT);
}

/* ---------------------------------------------------------------------------
 * Fichas
 * ------------------------------------------------------------------------- */

static void draw_tile(RkTile t, float cx, float cy, float scale, float depth, bool dim)
{
	C2D_ImageTint tint;
	C2D_PlainImageTint(&tint, C2D_Color32(20, 20, 30, 255), 0.4f);
	gfx_board(BOARD_TILE, cx, cy, scale, 0.0f, depth, dim ? &tint : NULL);
	if (t.color == RK_JOKER) {
		gfx_board(BOARD_JOKER, cx, cy, scale, 0.0f, depth, dim ? &tint : NULL);
		return;
	}
	char num[4];
	snprintf(num, sizeof num, "%d", t.number);
	/* El número, sin sombra para que no se emborrone en las fichas pequeñas */
	float ts = fmaxf(0.4f, 0.56f * scale);
	C2D_Text text;
	C2D_TextBuf buf = gfx_textbuf();
	C2D_TextParse(&text, buf, num);
	C2D_TextOptimize(&text);
	C2D_DrawText(&text, C2D_WithColor | C2D_AlignCenter, floorf(cx + gfx_dx(depth) + 0.5f),
		floorf(cy - TILE_H * scale * 0.42f + 0.5f), 0.5f, ts, ts, gfx_rgb(TILE_RGB[t.color], dim ? 140 : 255));
}

static int rack_per_row(void)
{
	return s_game.rack_count[0] > 33 ? 14 : 11;
}

static float rack_scale(void)
{
	return s_game.rack_count[0] > 33 ? 0.8f : 1.0f;
}

static void rack_pos(int i, float* x, float* y)
{
	int per = rack_per_row();
	float sc = rack_scale();
	float step = (TILE_W + 2.0f) * sc;
	int row = i / per, col = i % per;
	*x = BOTTOM_W / 2.0f + (col - (per - 1) / 2.0f) * step;
	*y = RACK_Y + TILE_H * sc / 2.0f + row * (TILE_H + 3.0f) * sc;
}

/* ---------------------------------------------------------------------------
 * Acciones del jugador
 * ------------------------------------------------------------------------- */

static int selected(int* idx)
{
	int n = 0;
	for (int i = 0; i < s_game.rack_count[0]; i++)
		if (s_sel[i])
			idx[n++] = i;
	return n;
}

static void play_selected(void)
{
	int idx[RK_MAX_RACK];
	int n = selected(idx);
	if (n < 3) {
		audio_play(SFX_ERROR);
		set_message("Elige al menos tres fichas para bajar una jugada");
		return;
	}
	if (n > RK_MAX_SET || !rk_play_set(&s_game, idx, n)) {
		audio_play(SFX_ERROR);
		set_message("Eso no es ni una escalera ni un grupo");
		return;
	}
	audio_play(SFX_CARD);
	clear_selection();
	s_highlight = 1.5f;
	char buf[96];
	if (!s_game.melded[0])
		snprintf(buf, sizeof buf, "Llevas %d de %d puntos para la primera jugada", s_game.turn_points, RK_MIN_MELD);
	else
		snprintf(buf, sizeof buf, "¡Jugada bajada!");
	set_message(buf);
}

/* Añade a la mesa las fichas elegidas, cada una a la primera jugada donde encaje */
static void add_selected(void)
{
	int idx[RK_MAX_RACK];
	int n = selected(idx);
	if (n == 0) {
		audio_play(SFX_ERROR);
		set_message("Elige las fichas que quieres añadir a la mesa");
		return;
	}
	if (!s_game.melded[0] && s_game.turn_points < RK_MIN_MELD) {
		/* Antes de la primera jugada solo se tocan las jugadas de este turno */
		bool any_new = s_game.table_count > s_game.saved_table_count;
		if (!any_new) {
			audio_play(SFX_ERROR);
			set_message("Primero tienes que bajar jugadas de 30 puntos");
			return;
		}
	}
	int added = 0;
	/* De la última a la primera, para que no cambien los índices pendientes */
	for (int k = n - 1; k >= 0; k--) {
		int count = s_game.table_count;
		for (int t = 0; t < count; t++) {
			int set = (s_add_from + t) % count;
			if (rk_add(&s_game, set, idx[k])) {
				added++;
				s_add_from = set + 1;
				break;
			}
		}
	}
	clear_selection();
	if (added == 0) {
		audio_play(SFX_ERROR);
		set_message("Esas fichas no encajan en ninguna jugada de la mesa");
		return;
	}
	audio_play(SFX_CARD);
	s_highlight = 1.5f;
	char buf[64];
	snprintf(buf, sizeof buf, added == n ? "Has añadido %d ficha%s" : "Solo encajan %d ficha%s", added,
		added == 1 ? "" : "s");
	set_message(buf);
}

static void end_turn(void)
{
	bool valid = rk_turn_valid(&s_game);
	bool tried = s_game.turn_tiles > 0;
	bool drew = rk_end_turn(&s_game);
	clear_selection();
	if (valid)
		set_message("Turno terminado");
	else if (tried)
		set_message(drew ? "No llegabas a 30 puntos: se deshace y robas" : "No llegabas a 30 puntos");
	else
		set_message(drew ? "Robas una ficha" : "No quedan fichas: pasas");
	rk_sort_rack(&s_game, 0, s_by_number);
	s_cpu_timer = 0.0f;
}

static int touched_tile(const Input* in)
{
	if (!in->touch_down)
		return -1;
	float sc = rack_scale();
	for (int i = 0; i < s_game.rack_count[0]; i++) {
		float x, y;
		rack_pos(i, &x, &y);
		if (fabsf(in->tx - x) <= TILE_W * sc / 2.0f && fabsf(in->ty - y) <= TILE_H * sc / 2.0f)
			return i;
	}
	return -1;
}

static SceneId rk_scene_update(const Input* in, float dt)
{
	if (button_update(&s_btn_menu, in))
		return SCENE_MENU;
	const float y = 205, h = 28;

	if (s_game.phase == RK_PHASE_PLAYING) {
		bool mine = s_game.current == 0;
		button_set(&s_btn_play, 4, y, 76, h, GLYPH_X " Bajar", C2D_Color32(40, 140, 60, 255), KEY_X);
		button_set(&s_btn_add, 84, y, 76, h, GLYPH_Y " Añadir", C2D_Color32(200, 140, 30, 255), KEY_Y);
		button_set(&s_btn_undo, 164, y, 72, h, GLYPH_L " Atrás", C2D_Color32(60, 90, 160, 255), KEY_L);
		button_set(&s_btn_end, 240, y, 76, h, s_game.turn_tiles > 0 ? GLYPH_R " Fin" : GLYPH_R " Robar",
			C2D_Color32(190, 60, 50, 255), KEY_R);
		s_btn_play.enabled = s_btn_add.enabled = s_btn_end.enabled = mine;
		s_btn_undo.enabled = mine && s_game.turn_tiles > 0;
		s_btn_sort.enabled = true;

		if (button_update(&s_btn_sort, in)) {
			s_by_number = !s_by_number;
			rk_sort_rack(&s_game, 0, s_by_number);
			clear_selection();
		}
		if (mine) {
			int n = s_game.rack_count[0];
			int per = rack_per_row();
			if (n > 0) {
				if (in->repeat & KEY_LEFT)
					s_cursor = (s_cursor + n - 1) % n;
				if (in->repeat & KEY_RIGHT)
					s_cursor = (s_cursor + 1) % n;
				if ((in->repeat & KEY_DOWN) && s_cursor + per < n)
					s_cursor += per;
				if ((in->repeat & KEY_UP) && s_cursor - per >= 0)
					s_cursor -= per;
				if (s_cursor >= n)
					s_cursor = n - 1;
				if (in->repeat & (KEY_LEFT | KEY_RIGHT | KEY_UP | KEY_DOWN))
					audio_play(SFX_SELECT);
			}
			int t = touched_tile(in);
			if (t >= 0) {
				s_cursor = t;
				s_sel[t] = !s_sel[t];
				audio_play(SFX_SELECT);
			} else if ((in->down & KEY_A) && n > 0) {
				s_sel[s_cursor] = !s_sel[s_cursor];
				audio_play(SFX_SELECT);
			} else if (in->down & KEY_B) {
				clear_selection();
			} else if (button_update(&s_btn_play, in)) {
				play_selected();
			} else if (button_update(&s_btn_add, in)) {
				add_selected();
			} else if (button_update(&s_btn_undo, in)) {
				rk_undo(&s_game);
				rk_sort_rack(&s_game, 0, s_by_number);
				clear_selection();
				set_message("Has deshecho lo de este turno");
			} else if (button_update(&s_btn_end, in)) {
				end_turn();
			}
		} else {
			s_cpu_timer += dt;
			if (s_cpu_timer > 0.9f) {
				s_cpu_timer = 0.0f;
				int p = s_game.current;
				int pool = s_game.pool_count;
				int placed = rk_ai_turn(&s_game);
				char buf[96];
				if (placed > 0) {
					snprintf(buf, sizeof buf, "%s pone %d ficha%s en la mesa", seat_name(p), placed, placed == 1 ? "" : "s");
					audio_play(SFX_CARD);
					s_highlight = 1.5f;
					mood_set(&s_mood[p], EXPR_HAPPY, 1.2f);
				} else {
					snprintf(buf, sizeof buf, s_game.pool_count < pool ? "%s roba" : "%s pasa", seat_name(p));
				}
				set_message(buf);
			}
		}
	} else {
		button_set(&s_btn_play, 12, y, 144, h, GLYPH_A " Otra ronda", C2D_Color32(40, 140, 60, 255), KEY_A);
		button_set(&s_btn_end, 164, y, 144, h, GLYPH_B " Menú", C2D_Color32(190, 60, 50, 255), KEY_B);
		s_btn_play.enabled = s_btn_end.enabled = s_result_time > 0.6f;
		if (button_update(&s_btn_play, in)) {
			rk_new_round(&s_game);
			rk_sort_rack(&s_game, 0, s_by_number);
			reset_round();
		} else if (button_update(&s_btn_end, in)) {
			return SCENE_MENU;
		}
	}

	if (s_game.phase != s_prev_phase) {
		s_prev_phase = s_game.phase;
		if (s_game.phase == RK_PHASE_ROUND_OVER) {
			s_result_time = 0.0f;
			int w = s_game.round_winner;
			for (int p = 0; p < RK_PLAYERS; p++)
				mood_set(&s_mood[p], p == w ? EXPR_HAPPY : EXPR_SAD, 4.0f);
			audio_play(w == 0 ? SFX_WIN : SFX_LOSE);
			profile_record_game(GAME_RUMMIKUB, w == 0);
		}
	}
	if (s_game.phase == RK_PHASE_ROUND_OVER)
		s_result_time += dt;
	if (s_highlight > 0.0f)
		s_highlight -= dt;
	for (int p = 0; p < RK_PLAYERS; p++)
		mood_update(&s_mood[p], dt);
	if (s_message_flash > 0.0f)
		s_message_flash -= dt * 2.0f;
	return SCENE_RUMMIKUB;
}

/* ---------------------------------------------------------------------------
 * Dibujo
 * ------------------------------------------------------------------------- */

/* Coloca las jugadas de la mesa en filas; devuelve false si no caben a esa escala */
static bool layout_table(float scale, bool draw)
{
	float tw = TILE_W * scale, th = TILE_H * scale;
	float x = 8.0f, y = 8.0f;
	const float right = 392.0f, bottom = 196.0f;
	for (int s = 0; s < s_game.table_count; s++) {
		const RkSet* set = &s_game.table[s];
		float w = set->n * tw + 2.0f * (set->n - 1);
		if (x + w > right && x > 8.0f) {
			x = 8.0f;
			y += th + 5.0f;
		}
		if (y + th > bottom)
			return false;
		if (draw) {
			bool hl = s == s_game.last_changed && s_highlight > 0.0f;
			bool mine_new = s_game.current == 0 && s >= s_game.saved_table_count;
			if (hl || mine_new)
				gfx_rrect(x - 3 + gfx_dx(DEPTH_CARDS), y - 3, w + 6, th + 6, 4, DEPTH_CARDS,
					hl ? C2D_Color32(255, 215, 90, 200) : C2D_Color32(120, 200, 255, 140));
			for (int i = 0; i < set->n; i++)
				draw_tile(set->tiles[i], x + tw / 2 + i * (tw + 2.0f), y + th / 2, scale, DEPTH_CARDS + 0.2f, false);
		}
		x += w + 10.0f;
	}
	return true;
}

static void rk_scene_draw_top(void)
{
	gfx_background(BG_TABLE_TOP);
	static const float scales[] = { 0.75f, 0.65f, 0.56f, 0.5f, 0.45f };
	float scale = 0.45f;
	for (unsigned i = 0; i < sizeof scales / sizeof scales[0]; i++)
		if (layout_table(scales[i], false)) {
			scale = scales[i];
			break;
		}
	layout_table(scale, true);
	if (s_game.table_count == 0)
		gfx_text("La mesa está vacía", 200, 90, DEPTH_UI, 0.6f, C2D_Color32(200, 230, 200, 255), C2D_AlignCenter);

	/* Jugadores y pozo */
	gfx_rect(0, 200, TOP_W, 40, DEPTH_UI, C2D_Color32(10, 40, 20, 200));
	for (int p = 0; p < RK_PLAYERS; p++) {
		float x = 22 + p * 92;
		bool turn = s_game.current == p && s_game.phase == RK_PHASE_PLAYING;
		if (turn)
			gfx_rrect(x - 18, 202, 88, 36, 6, DEPTH_UI, C2D_Color32(255, 215, 90, 120));
		mii_draw(seat_avatar(p), x, 220, 13, DEPTH_UI + 0.3f, mood_expr(&s_mood[p]), false);
		gfx_text(seat_name(p), x + 16, 204, DEPTH_UI, 0.42f, COLOR_CREAM, C2D_AlignLeft);
		gfx_textf(x + 16, 220, DEPTH_UI, 0.42f, s_game.melded[p] ? COLOR_GOLD : COLOR_CREAM, C2D_AlignLeft,
			"%d fichas", s_game.rack_count[p]);
	}
	gfx_textf(388, 186, DEPTH_UI, 0.42f, COLOR_CREAM, C2D_AlignRight, "Pozo: %d", s_game.pool_count);

	if (s_game.phase == RK_PHASE_ROUND_OVER && banner_draw(s_result_time, 200, 104, 340, 96)) {
		int w = s_game.round_winner;
		if (w == 0)
			gfx_text("¡Ganas la ronda!", 200, 64, DEPTH_BANNER, 0.7f, COLOR_GOLD, C2D_AlignCenter);
		else
			gfx_textf(200, 64, DEPTH_BANNER, 0.65f, COLOR_RED, C2D_AlignCenter, "Gana la ronda %s", seat_name(w));
		gfx_textf(200, 92, DEPTH_BANNER, 0.5f, COLOR_CREAM, C2D_AlignCenter, "Se lleva %d puntos", s_game.round_points);
		gfx_textf(200, 116, DEPTH_BANNER, 0.45f, COLOR_GOLD, C2D_AlignCenter, "Total: tú %d · %d · %d · %d",
			s_game.score[0], s_game.score[1], s_game.score[2], s_game.score[3]);
	}
}

static void rk_scene_draw_bottom(void)
{
	gfx_background(BG_TABLE_BOTTOM);
	gfx_text(s_message, 8, 26, 0, 0.42f, s_message_flash > 0.0f ? COLOR_GOLD : COLOR_CREAM, C2D_AlignLeft);
	button_draw(&s_btn_menu, false);
	button_draw(&s_btn_sort, false);
	bool mine = s_game.current == 0 && s_game.phase == RK_PHASE_PLAYING;
	if (mine)
		gfx_text("¡Tu turno!", 8, 6, 0, 0.5f, COLOR_GOLD, C2D_AlignLeft);

	float sc = rack_scale();
	float pulse = 0.6f + 0.4f * sinf(gfx_time() * 6.0f);
	for (int i = 0; i < s_game.rack_count[0]; i++) {
		float x, y;
		rack_pos(i, &x, &y);
		if (s_sel[i])
			y -= 5.0f;
		if (s_sel[i])
			gfx_rrect(x - TILE_W * sc / 2 - 2, y - TILE_H * sc / 2 - 2, TILE_W * sc + 4, TILE_H * sc + 4, 4, 0,
				C2D_Color32(255, 215, 90, 255));
		if (mine && i == s_cursor)
			gfx_rrect(x - TILE_W * sc / 2 - 3, y + TILE_H * sc / 2 + 1, TILE_W * sc + 6, 3, 1, 0,
				C2D_Color32(255, 255, 255, (u8)(255 * pulse)));
		draw_tile(s_game.racks[0][i], x, y, sc, 0, false);
	}

	if (s_game.phase == RK_PHASE_PLAYING && !s_game.melded[0])
		gfx_textf(BOTTOM_W / 2, 184, 0, 0.42f, C2D_Color32(200, 230, 200, 255), C2D_AlignCenter,
			"Primera jugada: %d de %d puntos", s_game.turn_points, RK_MIN_MELD);
	else if (s_game.phase == RK_PHASE_PLAYING)
		gfx_text(GLYPH_A " o toca: elegir fichas   SELECT: ordenar", BOTTOM_W / 2, 184, 0, 0.42f,
			C2D_Color32(200, 230, 200, 255), C2D_AlignCenter);

	if (s_game.phase == RK_PHASE_PLAYING) {
		button_draw(&s_btn_play, s_btn_play.enabled);
		button_draw(&s_btn_add, s_btn_add.enabled);
		button_draw(&s_btn_undo, s_btn_undo.enabled);
		button_draw(&s_btn_end, s_btn_end.enabled);
	} else {
		button_draw(&s_btn_play, s_btn_play.enabled);
		button_draw(&s_btn_end, s_btn_end.enabled);
	}
}

const Scene scene_rummikub = { rk_scene_enter, rk_scene_update, rk_scene_draw_top, rk_scene_draw_bottom };
