/* Generala: los dados y las casillas para apuntar abajo; la hoja de puntos
 * de todos arriba. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "games/generala.h"
#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/gfx.h"
#include "ui/mii.h"
#include "ui/widgets.h"

static const CpuId CPUS[GE_PLAYERS] = { CPU_COUNT, CPU_PEPE, CPU_LOLA, CPU_PACO };

typedef enum { MODE_SETUP, MODE_PLAY } Mode;

static Generala s_game;
static bool s_started, s_recorded;
static Mode s_mode;
static int s_players = 2;
static float s_cpu_timer, s_end_time, s_shake[GE_DICE];
static int s_last_cat = -1, s_last_player = -1;
static char s_message[96];
static Mood s_mood[GE_PLAYERS];
static Button s_btn_roll, s_btn_cat[GE_CATS], s_btn_players, s_btn_play, s_btn_menu;

static const char* name(int p)
{
	return p == 0 ? profile_get()->avatar.name : cpu_name(CPUS[p]);
}

static const Avatar* avatar(int p)
{
	return p == 0 ? &profile_get()->avatar : cpu_avatar(CPUS[p]);
}

static void ge_enter(void)
{
	if (!s_started) {
		s_started = true;
		s_mode = MODE_SETUP;
	}
	button_set(&s_btn_menu, 262, 2, 54, 18, "Menú", C2D_Color32(120, 30, 35, 255), KEY_START);
}

static void start(void)
{
	ge_init(&s_game, s_players, (uint32_t)time(NULL) ^ 0x6E4);
	s_mode = MODE_PLAY;
	s_recorded = false;
	s_cpu_timer = s_end_time = 0.0f;
	s_last_cat = s_last_player = -1;
	snprintf(s_message, sizeof s_message, "Te toca: tira los dados");
}

static void do_roll(void)
{
	if (!ge_roll(&s_game, NULL))
		return;
	for (int i = 0; i < GE_DICE; i++)
		if (s_game.rolls == 1 || !s_game.hold[i])
			s_shake[i] = 0.35f;
	audio_play(SFX_FLIP);
}

static void after_score(int p, int cat)
{
	s_last_cat = cat;
	s_last_player = p;
	int pts = s_game.score[p][cat];
	if (s_game.phase == GE_OVER && s_game.served_win)
		snprintf(s_message, sizeof s_message, "¡Generala servida! %s la partida", p == 0 ? "Ganas" : "Gana");
	else if (pts)
		snprintf(s_message, sizeof s_message, "%s %d %s", p == 0 ? "Apuntas" : name(p), pts, ge_cat_name(cat));
	else
		snprintf(s_message, sizeof s_message, "%s %s", p == 0 ? "Tachas" : "Tacha", ge_cat_name(cat));
	mood_set(&s_mood[p], pts >= 30 ? EXPR_HAPPY : pts == 0 ? EXPR_SAD : EXPR_NORMAL, 1.5f);
	audio_play(pts >= 30 ? SFX_WIN : pts ? SFX_CHIP : SFX_SELECT);
	if (s_game.phase != GE_OVER && s_game.current == 0)
		snprintf(s_message + strlen(s_message), sizeof s_message - strlen(s_message), ". Te toca");
}

static SceneId setup_update(const Input* in)
{
	static char label[32];
	snprintf(label, sizeof label, GLYPH_X " %d jugadores", s_players);
	button_set(&s_btn_players, 60, 150, 200, 30, label, C2D_Color32(60, 90, 160, 255), KEY_X);
	button_set(&s_btn_play, 60, 200, 200, 30, GLYPH_A " Jugar", C2D_Color32(40, 140, 60, 255), KEY_A);
	if (button_update(&s_btn_players, in))
		s_players = s_players == 4 ? 2 : s_players + 1;
	else if (button_update(&s_btn_play, in))
		start();
	return SCENE_GENERALA;
}

static float die_x(int i)
{
	return 26 + i * 40;
}

static SceneId ge_update(const Input* in, float dt)
{
	if (button_update(&s_btn_menu, in))
		return SCENE_MENU;
	for (int p = 0; p < GE_PLAYERS; p++)
		mood_update(&s_mood[p], dt);
	for (int i = 0; i < GE_DICE; i++)
		if (s_shake[i] > 0)
			s_shake[i] -= dt;
	if (s_mode == MODE_SETUP)
		return setup_update(in);
	Generala* g = &s_game;
	if (g->phase == GE_OVER) {
		s_end_time += dt;
		if (!s_recorded) {
			s_recorded = true;
			bool won = g->winner == 0;
			profile_record_game(GAME_GENERALA, won);
			audio_play(won ? SFX_WIN : SFX_LOSE);
		}
		if (s_end_time > 1.2f && ((in->down & KEY_A) || in->touch_down))
			s_mode = MODE_SETUP;
		return SCENE_GENERALA;
	}
	/* Casillas para apuntar: dos columnas */
	static char labels[GE_CATS][32];
	for (int c = 0; c < GE_CATS; c++) {
		bool open = g->score[g->current][c] < 0;
		if (open && g->rolls > 0)
			snprintf(labels[c], sizeof labels[c], "%s: %d", ge_cat_name(c), ge_value(g, c));
		else if (open)
			snprintf(labels[c], sizeof labels[c], "%s", ge_cat_name(c));
		else
			snprintf(labels[c], sizeof labels[c], "%s (%d)", ge_cat_name(c), g->score[g->current][c]);
		int col = c < 6 ? 0 : 1, row = c < 6 ? c : c - 6;
		u32 color = !open ? C2D_Color32(90, 90, 90, 255) : ge_value(g, c) > 0 && g->rolls ? C2D_Color32(40, 130, 60, 255)
		                                                                                   : C2D_Color32(60, 90, 160, 255);
		button_set(&s_btn_cat[c], 4 + col * 158, 82 + row * 26, 154, 23, labels[c], color, 0);
		s_btn_cat[c].enabled = g->current == 0 && open && g->rolls > 0;
	}
	static char roll_label[24];
	snprintf(roll_label, sizeof roll_label, GLYPH_A " Tirar %d/3", g->rolls + 1 > 3 ? 3 : g->rolls + 1);
	button_set(&s_btn_roll, 216, 28, 100, 34, roll_label, C2D_Color32(200, 140, 30, 255), KEY_A);
	s_btn_roll.enabled = g->current == 0 && g->rolls < GE_ROLLS;

	if (g->current == 0) {
		if (button_update(&s_btn_roll, in))
			do_roll();
		/* Tocar un dado lo guarda o lo suelta */
		if (in->touch_down && in->ty >= 24 && in->ty < 70)
			for (int i = 0; i < GE_DICE; i++)
				if (fabsf(in->tx - die_x(i)) < 18 && g->rolls > 0 && g->rolls < GE_ROLLS) {
					ge_toggle_hold(g, i);
					audio_play(SFX_SELECT);
				}
		for (int c = 0; c < GE_CATS; c++)
			if (button_update(&s_btn_cat[c], in) && ge_score(g, c)) {
				after_score(0, c);
				s_cpu_timer = -0.5f;
				break;
			}
	} else {
		s_cpu_timer += dt;
		if (s_cpu_timer > 0.8f) {
			s_cpu_timer = 0.0f;
			int p = g->current;
			if (g->rolls == 0) {
				do_roll();
				snprintf(s_message, sizeof s_message, "Tira %s", name(p));
			} else if (g->rolls < GE_ROLLS && ge_ai_hold(g)) {
				do_roll();
			} else {
				int cat = ge_ai_category(g);
				if (ge_score(g, cat))
					after_score(p, cat);
				s_cpu_timer = -0.8f;
			}
		}
	}
	return SCENE_GENERALA;
}

static void ge_draw_top(void)
{
	const Generala* g = &s_game;
	gfx_background(BG_TABLE_TOP);
	if (s_mode == MODE_SETUP) {
		gfx_text("Generala", 200, 30, DEPTH_UI, 1.0f, COLOR_GOLD, C2D_AlignCenter);
		for (int i = 0; i < 5; i++)
			gfx_piece(PIECE_DIE_FIRST + 4, 120 + i * 40, 110 + (i % 2) * 6, 1.1f, 1.1f, DEPTH_CARDS);
		gfx_text_wrap("Tres tiradas por turno para hacer escalera, full, póker o generala. Una generala a la primera "
		              "(servida) gana la partida.",
			60, 150, DEPTH_UI, 0.5f, COLOR_CREAM, 280);
		return;
	}
	/* Hoja de puntos */
	float x0 = 8, y0 = 6, row_h = 16.5f, name_w = 112, col_w = (384 - name_w) / g->players;
	gfx_panel(x0, y0, 384, row_h * (GE_CATS + 2) + 8, DEPTH_UI, C2D_Color32(250, 246, 232, 240), C2D_Color32(110, 70, 30, 255));
	u32 ink = C2D_Color32(60, 35, 25, 255);
	for (int p = 0; p < g->players; p++) {
		if (g->current == p && g->phase != GE_OVER)
			gfx_rect(x0 + name_w + col_w * p + 2, y0 + 4, col_w - 4, row_h * (GE_CATS + 2), DEPTH_UI,
				C2D_Color32(255, 220, 120, 120));
		float left = x0 + name_w + col_w * p;
		mii_draw(avatar(p), left + 12, y0 + 12, 7, DEPTH_UI, mood_expr(&s_mood[p]), false);
		gfx_text(name(p), left + 23, y0 + 4, DEPTH_UI, 0.42f, ink, C2D_AlignLeft);
	}
	for (int c = 0; c <= GE_CATS; c++) {
		float y = y0 + 4 + row_h * (c + 1);
		if (c % 2 == 0)
			gfx_rect(x0 + 4, y, 376, row_h, DEPTH_UI, C2D_Color32(110, 70, 30, 25));
		gfx_text(c < GE_CATS ? ge_cat_name(c) : "Total", x0 + 8, y + 1, DEPTH_UI, 0.42f, c < GE_CATS ? ink : COLOR_RED,
			C2D_AlignLeft);
		for (int p = 0; p < g->players; p++) {
			float cx = x0 + name_w + col_w * p + col_w / 2;
			if (c == GE_CATS) {
				gfx_textf(cx, y + 1, DEPTH_UI, 0.45f, COLOR_RED, C2D_AlignCenter, "%d", ge_total(g, p));
				continue;
			}
			int v = g->score[p][c];
			bool flash = c == s_last_cat && p == s_last_player;
			if (v > 0)
				gfx_textf(cx, y + 1, DEPTH_UI, 0.45f, flash ? C2D_Color32(40, 120, 40, 255) : ink, C2D_AlignCenter, "%d", v);
			else if (v == 0)
				gfx_text("X", cx, y + 1, DEPTH_UI, 0.45f, C2D_Color32(170, 60, 50, 255), C2D_AlignCenter);
			else if (p == g->current && g->rolls > 0 && g->phase != GE_OVER)
				gfx_textf(cx, y + 1, DEPTH_UI, 0.42f, C2D_Color32(170, 150, 120, 255), C2D_AlignCenter, "%d",
					ge_value(g, c));
		}
	}
	if (g->phase == GE_OVER && banner_draw(s_end_time, 200, 120, 300, 64)) {
		if (g->winner == 0)
			gfx_text("¡Has ganado!", 200, 100, DEPTH_BANNER, 0.75f, COLOR_GOLD, C2D_AlignCenter);
		else
			gfx_textf(200, 100, DEPTH_BANNER, 0.62f, COLOR_CREAM, C2D_AlignCenter, "Gana %s", name(g->winner));
		gfx_textf(200, 128, DEPTH_BANNER, 0.45f, COLOR_CREAM, C2D_AlignCenter, "%s",
			g->served_win ? "con generala servida" : "Toca para seguir");
	}
}

static void ge_draw_bottom(void)
{
	const Generala* g = &s_game;
	gfx_background(BG_TABLE_BOTTOM);
	button_draw(&s_btn_menu, false);
	if (s_mode == MODE_SETUP) {
		gfx_text("Tú contra la consola", BOTTOM_W / 2, 40, 0, 0.55f, COLOR_GOLD, C2D_AlignCenter);
		button_draw(&s_btn_players, false);
		button_draw(&s_btn_play, true);
		return;
	}
	gfx_text(s_message, 6, 3, 0, 0.42f, COLOR_GOLD, C2D_AlignLeft);
	/* Dados */
	for (int i = 0; i < GE_DICE; i++) {
		float x = die_x(i), y = 46;
		if (g->rolls == 0) {
			gfx_rrect(x - 16, y - 16, 32, 32, 6, 0, C2D_Color32(0, 0, 0, 60));
			continue;
		}
		float jig = s_shake[i] > 0 ? sinf(s_shake[i] * 60) * 3 : 0;
		if (g->hold[i]) {
			y -= 4;
			gfx_rrect(x - 19, y - 19, 38, 38, 8, 0, COLOR_GOLD);
		}
		int face = s_shake[i] > 0 ? 1 + (int)(gfx_time() * 30 + i * 7) % 6 : g->dice[i];
		gfx_piece(PIECE_DIE_FIRST + face - 1, x + jig, y, 1.05f, 1.05f, 0);
	}
	if (g->phase == GE_OVER)
		return;
	if (g->current == 0) {
		button_draw(&s_btn_roll, s_btn_roll.enabled);
		if (g->rolls > 0 && g->rolls < GE_ROLLS)
			gfx_text("Toca un dado para guardarlo", 110, 66, 0, 0.4f, COLOR_CREAM, C2D_AlignCenter);
	}
	for (int c = 0; c < GE_CATS; c++)
		button_draw(&s_btn_cat[c], s_btn_cat[c].enabled);
}

/* Salir a medias desde la pausa: la próxima vez se empieza de cero */
static void scene_leave(void)
{
	s_started = false;
}

const Scene scene_generala = { ge_enter, ge_update, ge_draw_top, ge_draw_bottom, scene_leave };
