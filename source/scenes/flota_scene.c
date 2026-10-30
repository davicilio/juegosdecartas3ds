/* Hundir la flota: arriba tu flota y los disparos de la CPU; abajo, el mar
 * enemigo, donde disparas tocando una casilla. */
#include <math.h>
#include <stdio.h>
#include <time.h>

#include "games/flota.h"
#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/gfx.h"
#include "ui/mii.h"
#include "ui/widgets.h"

#define BOT_CELL 17.0f
#define BOT_X0   ((BOTTOM_W - HF_SIZE * BOT_CELL) / 2.0f)
#define BOT_Y0   28.0f
#define TOP_CELL 19.0f

static Flota s_game;
static bool s_started, s_playing;
static int s_cur_r = 4, s_cur_c = 4;
static float s_cpu_timer, s_end_time, s_splash;
static Mood s_cpu_mood, s_my_mood;
static char s_message[96];
static bool s_recorded;
static Button s_btn_start, s_btn_shuffle, s_btn_menu;

static const char* const COL_NAME = "ABCDEFGHIJ";

static void message_shot(void)
{
	const char* who = s_game.last_shooter == 0 ? "Tú" : cpu_name(CPU_PEPE);
	char where[16];
	snprintf(where, sizeof where, "%c%d", COL_NAME[s_game.last_col], s_game.last_row + 1);
	switch (s_game.last_result) {
	case HF_WATER:
		snprintf(s_message, sizeof s_message, "%s: %s... ¡agua!", who, where);
		break;
	case HF_HIT:
		snprintf(s_message, sizeof s_message, "%s: %s... ¡tocado! Vuelve a disparar", who, where);
		break;
	case HF_SUNK:
		snprintf(s_message, sizeof s_message, "%s: %s... ¡tocado y hundido el %s!", who, where,
			hf_ship_name(s_game.last_ship));
		break;
	default:
		break;
	}
}

static void new_game(void)
{
	hf_new_game(&s_game);
	s_playing = false;
	s_recorded = false;
	s_end_time = 0.0f;
	snprintf(s_message, sizeof s_message, "Coloca tu flota (" GLYPH_X " recoloca) y empieza");
}

static void hf_enter(void)
{
	if (!s_started) {
		hf_init(&s_game, (uint32_t)time(NULL) ^ 0xF107A);
		s_started = true;
		new_game();
	}
	button_set(&s_btn_menu, 258, 2, 54, 20, "Menú", C2D_Color32(120, 30, 35, 255), KEY_START);
}

static void fire(int r, int c)
{
	int res = hf_fire(&s_game, r, c);
	if (res == HF_NONE) {
		audio_play(SFX_ERROR);
		return;
	}
	s_splash = 0.0f;
	message_shot();
	audio_play(res == HF_WATER ? SFX_FLIP : res == HF_SUNK ? SFX_WIN : SFX_CHIP);
	Mood* shooter = s_game.last_shooter == 0 ? &s_my_mood : &s_cpu_mood;
	Mood* target = s_game.last_shooter == 0 ? &s_cpu_mood : &s_my_mood;
	if (res == HF_SUNK) {
		mood_set(shooter, EXPR_HAPPY, 2.0f);
		mood_set(target, EXPR_SAD, 2.0f);
	} else if (res == HF_HIT) {
		mood_set(target, EXPR_SURPRISED, 1.2f);
	}
}

static SceneId hf_update(const Input* in, float dt)
{
	if (button_update(&s_btn_menu, in))
		return SCENE_MENU;
	s_splash += dt;
	if (!s_playing) {
		button_set(&s_btn_shuffle, 12, 205, 144, 28, GLYPH_X " Recolocar", C2D_Color32(60, 90, 160, 255), KEY_X);
		button_set(&s_btn_start, 164, 205, 144, 28, s_game.winner >= 0 ? GLYPH_A " Otra partida" : GLYPH_A " ¡A jugar!",
			C2D_Color32(40, 140, 60, 255), KEY_A);
		s_end_time += dt;
		bool ready = s_game.winner < 0 || s_end_time > 1.0f;
		if (ready && button_update(&s_btn_shuffle, in)) {
			if (s_game.winner >= 0)
				new_game();
			hf_place_random(&s_game, 0);
			audio_play(SFX_SELECT);
		} else if (ready && button_update(&s_btn_start, in)) {
			if (s_game.winner >= 0) {
				new_game();
			} else {
				s_playing = true;
				snprintf(s_message, sizeof s_message, "¡Dispara! Toca una casilla del mar enemigo");
			}
		}
	} else if (s_game.winner < 0) {
		if (s_game.current == 0) {
			if (in->repeat & KEY_LEFT)
				s_cur_c = (s_cur_c + HF_SIZE - 1) % HF_SIZE;
			if (in->repeat & KEY_RIGHT)
				s_cur_c = (s_cur_c + 1) % HF_SIZE;
			if (in->repeat & KEY_UP)
				s_cur_r = (s_cur_r + HF_SIZE - 1) % HF_SIZE;
			if (in->repeat & KEY_DOWN)
				s_cur_r = (s_cur_r + 1) % HF_SIZE;
			if (in->touch_down && in->tx >= BOT_X0 && in->tx < BOT_X0 + HF_SIZE * BOT_CELL && in->ty >= BOT_Y0 &&
				in->ty < BOT_Y0 + HF_SIZE * BOT_CELL) {
				s_cur_c = (int)((in->tx - BOT_X0) / BOT_CELL);
				s_cur_r = (int)((in->ty - BOT_Y0) / BOT_CELL);
				fire(s_cur_r, s_cur_c);
			} else if (in->down & KEY_A) {
				fire(s_cur_r, s_cur_c);
			}
		} else {
			s_cpu_timer += dt;
			if (s_cpu_timer > 0.9f) {
				s_cpu_timer = 0.0f;
				int t = hf_ai_target(&s_game);
				if (t >= 0)
					fire(t / HF_SIZE, t % HF_SIZE);
			}
		}
	}
	if (s_game.winner >= 0 && !s_recorded) {
		s_recorded = true;
		s_playing = false;
		s_end_time = 0.0f;
		bool won = s_game.winner == 0;
		profile_record_game(GAME_FLOTA, won);
		audio_play(won ? SFX_WIN : SFX_LOSE);
		mood_set(&s_my_mood, won ? EXPR_HAPPY : EXPR_SAD, 5.0f);
		mood_set(&s_cpu_mood, won ? EXPR_SAD : EXPR_HAPPY, 5.0f);
		snprintf(s_message, sizeof s_message, won ? "¡Has hundido toda su flota!" : "Te han hundido la flota");
	}
	mood_update(&s_cpu_mood, dt);
	mood_update(&s_my_mood, dt);
	return SCENE_FLOTA;
}

static void draw_sea(float x0, float y0, float cell, float depth, int owner, bool show_ships)
{
	float w = HF_SIZE * cell;
	float dx = gfx_dx(depth);
	gfx_rect(x0 + dx, y0, w, w, depth, C2D_Color32(30, 90, 160, 255));
	for (int k = 0; k <= HF_SIZE; k++) {
		gfx_rect(x0 + k * cell + dx, y0, 1, w, depth, C2D_Color32(90, 150, 210, 160));
		gfx_rect(x0 + dx, y0 + k * cell, w, 1, depth, C2D_Color32(90, 150, 210, 160));
	}
	for (int k = 0; k < HF_SIZE; k++) {
		char l[2] = { COL_NAME[k], 0 };
		gfx_text(l, x0 + k * cell + cell / 2, y0 - 13, depth, 0.42f, COLOR_CREAM, C2D_AlignCenter);
		gfx_textf(x0 - 4, y0 + k * cell + cell / 2 - 7, depth, 0.42f, COLOR_CREAM, C2D_AlignRight, "%d", k + 1);
	}
	/* Barcos (los propios, o los del rival ya hundidos) */
	for (int s = 0; s < HF_SHIPS; s++) {
		const HfShip* sh = &s_game.ships[owner][s];
		if (!show_ships && sh->hits < sh->len)
			continue;
		float bx = x0 + sh->col * cell + 2, by = y0 + sh->row * cell + 2;
		float bw = (sh->horizontal ? sh->len : 1) * cell - 4, bh = (sh->horizontal ? 1 : sh->len) * cell - 4;
		gfx_rrect(bx + dx, by, bw, bh, cell * 0.4f, depth, C2D_Color32(150, 155, 165, 255));
	}
	/* Disparos */
	for (int r = 0; r < HF_SIZE; r++)
		for (int c = 0; c < HF_SIZE; c++) {
			uint8_t v = s_game.shot[owner][r][c];
			float cx = x0 + c * cell + cell / 2 + dx, cy = y0 + r * cell + cell / 2;
			if (v == HF_WATER)
				gfx_ellipse(cx, cy, cell * 0.16f, cell * 0.16f, depth, C2D_Color32(230, 240, 250, 220));
			else if (v == HF_HIT || v == HF_SUNK) {
				gfx_ellipse(cx, cy, cell * 0.32f, cell * 0.32f, depth, C2D_Color32(240, 120, 30, 255));
				gfx_ellipse(cx, cy, cell * 0.18f, cell * 0.18f, depth,
					v == HF_SUNK ? C2D_Color32(60, 40, 40, 255) : C2D_Color32(255, 220, 60, 255));
			}
		}
	/* El último disparo hace una onda */
	if (s_game.last_shooter == 1 - owner && s_splash < 0.8f) {
		float cx = x0 + s_game.last_col * cell + cell / 2 + dx, cy = y0 + s_game.last_row * cell + cell / 2;
		float r = cell * (0.3f + s_splash);
		u8 a = (u8)(200 * (1.0f - s_splash / 0.8f));
		gfx_ellipse(cx, cy, r, r, depth, C2D_Color32(255, 255, 255, a / 3));
	}
}

static void hf_draw_top(void)
{
	gfx_background(BG_TABLE_TOP);
	float x0 = (TOP_W - HF_SIZE * TOP_CELL) / 2.0f + 30, y0 = 26;
	draw_sea(x0, y0, TOP_CELL, DEPTH_CARDS, 0, true);
	gfx_text("Tu flota", x0 + HF_SIZE * TOP_CELL / 2, 2, DEPTH_UI, 0.5f, COLOR_GOLD, C2D_AlignCenter);
	mii_draw(&profile_get()->avatar, 40, 70, 20, DEPTH_UI, mood_expr(&s_my_mood), false);
	gfx_textf(40, 100, DEPTH_UI, 0.42f, COLOR_CREAM, C2D_AlignCenter, "Hundidos: %d", s_game.sunk[1]);
	mii_draw(cpu_avatar(CPU_PEPE), 40, 150, 20, DEPTH_UI, mood_expr(&s_cpu_mood), false);
	gfx_textf(40, 180, DEPTH_UI, 0.42f, COLOR_CREAM, C2D_AlignCenter, "%s: %d", cpu_name(CPU_PEPE), s_game.sunk[0]);
	if (s_game.winner >= 0 && banner_draw(s_end_time, 200, 120, 300, 56))
		gfx_text(s_game.winner == 0 ? "¡Victoria en el mar!" : "Gana la CPU", 200, 108, DEPTH_BANNER, 0.65f,
			s_game.winner == 0 ? COLOR_GOLD : COLOR_RED, C2D_AlignCenter);
}

static void hf_draw_bottom(void)
{
	gfx_background(BG_TABLE_BOTTOM);
	gfx_text(s_message, 6, 2, 0, 0.42f, COLOR_GOLD, C2D_AlignLeft);
	button_draw(&s_btn_menu, false);
	if (!s_playing) {
		/* Antes de empezar: tu flota en grande para colocarla */
		draw_sea(BOT_X0, BOT_Y0, BOT_CELL, 0, 0, true);
		button_draw(&s_btn_shuffle, true);
		button_draw(&s_btn_start, true);
		return;
	}
	draw_sea(BOT_X0, BOT_Y0, BOT_CELL, 0, 1, s_game.winner >= 0);
	if (s_game.current == 0 && s_game.winner < 0) {
		float pulse = 0.6f + 0.4f * sinf(gfx_time() * 6.0f);
		u32 col = C2D_Color32(255, 220, 60, (u8)(255 * pulse));
		float x = BOT_X0 + s_cur_c * BOT_CELL, y = BOT_Y0 + s_cur_r * BOT_CELL;
		gfx_rect(x, y, BOT_CELL, 2, 0, col);
		gfx_rect(x, y + BOT_CELL - 2, BOT_CELL, 2, 0, col);
		gfx_rect(x, y, 2, BOT_CELL, 0, col);
		gfx_rect(x + BOT_CELL - 2, y, 2, BOT_CELL, 0, col);
	}
	gfx_textf(BOTTOM_W / 2, 204, 0, 0.45f, COLOR_CREAM, C2D_AlignCenter, "Disparos: %d · hundidos %d de %d",
		s_game.shots[0], s_game.sunk[1], HF_SHIPS);
	if (s_game.current == 1 && s_game.winner < 0)
		gfx_text("Dispara la CPU...", BOTTOM_W / 2, 220, 0, 0.45f, C2D_Color32(200, 230, 200, 255), C2D_AlignCenter);
}

const Scene scene_flota = { hf_enter, hf_update, hf_draw_top, hf_draw_bottom };
