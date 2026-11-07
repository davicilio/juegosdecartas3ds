/* Tragaperras: la máquina arriba y los mandos y la tabla de premios abajo. */
#include <math.h>
#include <stdio.h>
#include <time.h>

#include "games/tragaperras.h"
#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/gfx.h"
#include "ui/mii.h"
#include "ui/wallet.h"
#include "ui/widgets.h"

#define REEL_W  76.0f
#define SYM_H   54.0f
#define REELS_X (200.0f - 1.5f * REEL_W)
#define REELS_Y 52.0f

static const int BETS[] = { 1, 2, 5, 10, 25, 50 };
#define NBETS ((int)(sizeof BETS / sizeof BETS[0]))
static const int LINES[] = { 1, 3, 5 };

static Rng s_rng;
static bool s_started, s_spinning;
static int s_bet_idx, s_lines_idx = 2;
static int s_stop[TP_REELS];
static float s_pos[TP_REELS];     /* posición de cada rodillo, en símbolos */
static float s_speed[TP_REELS];
static float s_stop_at[TP_REELS]; /* cuándo empieza a frenar cada rodillo */
static bool s_reel_done[TP_REELS];
static float s_t, s_win_time;
static int s_win, s_win_mask, s_paid;
static char s_message[80];
static Button s_btn_spin, s_btn_bet, s_btn_lines, s_btn_menu;

static const u32 LINE_COL[TP_LINES] = { 0xF0C828, 0xE04040, 0x40A0E0, 0x50C060, 0xD060D0 };

static void tp_enter(void)
{
	if (!s_started) {
		s_started = true;
		rng_seed(&s_rng, (uint32_t)time(NULL) ^ 0x7777);
		for (int r = 0; r < TP_REELS; r++) {
			s_stop[r] = rng_range(&s_rng, TP_STRIP);
			s_pos[r] = (float)s_stop[r];
		}
		s_spinning = false;
		s_win = 0;
		s_win_mask = 0;
		snprintf(s_message, sizeof s_message, "Elige líneas y apuesta, y tira de la palanca");
	}
	button_set(&s_btn_menu, 262, 2, 54, 18, "Menú", C2D_Color32(120, 30, 35, 255), KEY_START);
}

static int cost(void)
{
	return BETS[s_bet_idx] * LINES[s_lines_idx];
}

static void spin(void)
{
	int c = cost();
	if (wallet_chips() < c) {
		audio_play(SFX_ERROR);
		snprintf(s_message, sizeof s_message, "No tienes fichas para %d", c);
		return;
	}
	s_paid = wallet_take(c);
	tp_spin(&s_rng, s_stop);
	for (int r = 0; r < TP_REELS; r++) {
		s_speed[r] = 18.0f + r * 2.0f;
		s_stop_at[r] = 0.9f + r * 0.45f;
		s_reel_done[r] = false;
	}
	s_t = 0.0f;
	s_spinning = true;
	s_win = 0;
	s_win_mask = 0;
	snprintf(s_message, sizeof s_message, "¡Suerte!");
	audio_play(SFX_FLIP);
}

static void finish(void)
{
	s_spinning = false;
	int mult = tp_eval(s_stop, LINES[s_lines_idx], &s_win_mask);
	s_win = mult * BETS[s_bet_idx];
	s_paid = 0;
	s_win_time = 0.0f;
	if (s_win) {
		wallet_give(s_win);
		snprintf(s_message, sizeof s_message, "¡Premio! Ganas %d fichas", s_win);
		audio_play(SFX_WIN);
	} else {
		snprintf(s_message, sizeof s_message, "Sin premio. ¡Otra!");
	}
	profile_record_game(GAME_TRAGAPERRAS, s_win > cost());
}

static SceneId tp_update(const Input* in, float dt)
{
	if (button_update(&s_btn_menu, in))
		return SCENE_MENU;
	static char bet_label[32], lines_label[32];
	snprintf(bet_label, sizeof bet_label, GLYPH_X " Apuesta %d", BETS[s_bet_idx]);
	snprintf(lines_label, sizeof lines_label, GLYPH_Y " %d línea%s", LINES[s_lines_idx], s_lines_idx ? "s" : "");
	button_set(&s_btn_bet, 4, 205, 100, 28, bet_label, C2D_Color32(60, 90, 160, 255), KEY_X);
	button_set(&s_btn_lines, 108, 205, 96, 28, lines_label, C2D_Color32(200, 140, 30, 255), KEY_Y);
	button_set(&s_btn_spin, 208, 205, 108, 28, s_spinning ? GLYPH_A " Parar" : GLYPH_A " Tirar", C2D_Color32(190, 40, 40, 255), KEY_A);
	s_btn_bet.enabled = s_btn_lines.enabled = !s_spinning;
	s_btn_spin.enabled = true;
	s_win_time += dt;
	if (!s_spinning) {
		if (button_update(&s_btn_bet, in)) {
			s_bet_idx = (s_bet_idx + 1) % NBETS;
			audio_play(SFX_CHIP);
		}
		if (button_update(&s_btn_lines, in)) {
			s_lines_idx = (s_lines_idx + 1) % 3;
			audio_play(SFX_SELECT);
		}
		if (button_update(&s_btn_spin, in)) {
			if (wallet_chips() == 0)
				return SCENE_FICHAS;
			spin();
		}
		return SCENE_TRAGAPERRAS;
	}
	/* Parar antes: los rodillos frenan ya */
	if (button_update(&s_btn_spin, in))
		for (int r = 0; r < TP_REELS; r++)
			if (s_stop_at[r] > s_t + 0.1f * r)
				s_stop_at[r] = s_t + 0.1f * r;
	s_t += dt;
	bool all = true;
	for (int r = 0; r < TP_REELS; r++) {
		if (s_reel_done[r])
			continue;
		all = false;
		if (s_t < s_stop_at[r]) {
			s_pos[r] = fmodf(s_pos[r] + s_speed[r] * dt, TP_STRIP);
		} else {
			/* Se acerca a su parada desde arriba, con un pequeño rebote */
			float target = (float)s_stop[r];
			float dist = fmodf(target - s_pos[r] + 2 * TP_STRIP, TP_STRIP);
			float v = fmaxf(3.0f, fminf(s_speed[r], dist * 6.0f));
			if (dist <= v * dt || dist > TP_STRIP - 0.05f) {
				s_pos[r] = target;
				s_reel_done[r] = true;
				audio_play(SFX_CARD);
			} else {
				s_pos[r] = fmodf(s_pos[r] + v * dt, TP_STRIP);
			}
		}
	}
	if (all)
		finish();
	return SCENE_TRAGAPERRAS;
}

/* Símbolos dibujados a mano (s = escala, 1 = unos 40 px) */
static void draw_symbol(int sym, float cx, float cy, float s, float d)
{
	switch (sym) {
	case TP_CHERRY:
		gfx_line(cx - 6 * s, cy + 2 * s, cx + 3 * s, cy - 14 * s, 2 * s, d, C2D_Color32(60, 130, 40, 255));
		gfx_line(cx + 8 * s, cy + 5 * s, cx + 3 * s, cy - 14 * s, 2 * s, d, C2D_Color32(60, 130, 40, 255));
		gfx_ellipse(cx + 8 * s, cy - 13 * s, 6 * s, 3 * s, d, C2D_Color32(80, 170, 50, 255));
		gfx_ellipse(cx - 7 * s, cy + 8 * s, 8 * s, 8 * s, d, C2D_Color32(200, 20, 40, 255));
		gfx_ellipse(cx + 8 * s, cy + 10 * s, 8 * s, 8 * s, d, C2D_Color32(220, 30, 50, 255));
		gfx_ellipse(cx - 10 * s, cy + 5 * s, 2.5f * s, 2 * s, d, C2D_Color32(255, 180, 180, 255));
		gfx_ellipse(cx + 5 * s, cy + 7 * s, 2.5f * s, 2 * s, d, C2D_Color32(255, 180, 180, 255));
		break;
	case TP_LEMON:
		gfx_ellipse(cx - 15 * s, cy, 4 * s, 3 * s, d, C2D_Color32(230, 200, 30, 255));
		gfx_ellipse(cx + 15 * s, cy, 4 * s, 3 * s, d, C2D_Color32(230, 200, 30, 255));
		gfx_ellipse(cx, cy, 16 * s, 12 * s, d, C2D_Color32(250, 225, 50, 255));
		gfx_ellipse(cx - 5 * s, cy - 4 * s, 5 * s, 3 * s, d, C2D_Color32(255, 250, 170, 255));
		break;
	case TP_ORANGE:
		gfx_ellipse(cx, cy + 2 * s, 15 * s, 15 * s, d, C2D_Color32(240, 140, 20, 255));
		gfx_ellipse(cx - 5 * s, cy - 3 * s, 5 * s, 4 * s, d, C2D_Color32(255, 200, 120, 255));
		gfx_ellipse(cx + 5 * s, cy - 13 * s, 6 * s, 3 * s, d, C2D_Color32(60, 150, 50, 255));
		break;
	case TP_PLUM:
		gfx_ellipse(cx, cy + 2 * s, 13 * s, 15 * s, d, C2D_Color32(120, 40, 140, 255));
		gfx_line(cx, cy - 12 * s, cx + 1 * s, cy + 14 * s, 1.2f * s, d, C2D_Color32(90, 25, 105, 255));
		gfx_ellipse(cx - 5 * s, cy - 4 * s, 3 * s, 5 * s, d, C2D_Color32(190, 120, 210, 255));
		gfx_line(cx, cy - 12 * s, cx + 4 * s, cy - 18 * s, 2 * s, d, C2D_Color32(110, 80, 40, 255));
		break;
	case TP_BELL:
		gfx_ellipse(cx, cy - 6 * s, 9 * s, 9 * s, d, C2D_Color32(240, 190, 30, 255));
		gfx_triangle(cx - 9 * s, cy - 6 * s, cx + 9 * s, cy - 6 * s, cx - 16 * s, cy + 10 * s, d, C2D_Color32(240, 190, 30, 255));
		gfx_triangle(cx + 9 * s, cy - 6 * s, cx + 16 * s, cy + 10 * s, cx - 16 * s, cy + 10 * s, d, C2D_Color32(240, 190, 30, 255));
		gfx_rrect(cx - 17 * s, cy + 8 * s, 34 * s, 5 * s, 2 * s, d, C2D_Color32(200, 150, 20, 255));
		gfx_ellipse(cx, cy + 15 * s, 4 * s, 4 * s, d, C2D_Color32(170, 120, 20, 255));
		gfx_ellipse(cx - 4 * s, cy - 8 * s, 2.5f * s, 4 * s, d, C2D_Color32(255, 240, 160, 255));
		gfx_ellipse(cx, cy - 16 * s, 2.5f * s, 2.5f * s, d, C2D_Color32(200, 150, 20, 255));
		break;
	case TP_BAR:
		gfx_rrect(cx - 21 * s, cy - 11 * s, 42 * s, 22 * s, 4 * s, d, COLOR_GOLD);
		gfx_rrect(cx - 19 * s, cy - 9 * s, 38 * s, 18 * s, 3 * s, d, C2D_Color32(25, 25, 30, 255));
		gfx_text("BAR", cx, cy - 8 * s, d, 0.55f * s, COLOR_WHITE, C2D_AlignCenter);
		break;
	case TP_SEVEN:
		gfx_text("7", cx + 2 * s, cy - 20 * s, d, 1.45f * s, C2D_Color32(90, 0, 0, 255), C2D_AlignCenter);
		gfx_text("7", cx, cy - 22 * s, d, 1.45f * s, C2D_Color32(230, 30, 40, 255), C2D_AlignCenter);
		break;
	}
}

static void tp_draw_top(void)
{
	gfx_background(BG_TABLE_TOP);
	float d = DEPTH_CARDS;
	/* Mueble */
	gfx_rrect(REELS_X - 30, 4, 3 * REEL_W + 60, 234, 16, DEPTH_TABLE, C2D_Color32(140, 20, 30, 255));
	gfx_rrect(REELS_X - 24, 8, 3 * REEL_W + 48, 226, 12, DEPTH_TABLE, C2D_Color32(190, 40, 45, 255));
	/* Bombillas */
	for (int k = 0; k < 14; k++) {
		float x = REELS_X - 14 + k * (3 * REEL_W + 28) / 13.0f;
		bool on = ((int)(gfx_time() * (s_spinning || s_win ? 8 : 2)) + k) % 2;
		gfx_ellipse(x, 16, 3.5f, 3.5f, DEPTH_UI, on ? C2D_Color32(255, 230, 120, 255) : C2D_Color32(140, 100, 50, 255));
	}
	gfx_text("TRAGAPERRAS", 200, 22, DEPTH_UI, 0.72f, COLOR_GOLD, C2D_AlignCenter);
	/* Ventana de los rodillos */
	gfx_rrect(REELS_X - 6, REELS_Y - 8, 3 * REEL_W + 12, 3 * SYM_H + 16, 6, DEPTH_TABLE, C2D_Color32(40, 20, 15, 255));
	for (int r = 0; r < TP_REELS; r++) {
		float x0 = REELS_X + r * REEL_W;
		gfx_rect(x0 + 2, REELS_Y, REEL_W - 4, 3 * SYM_H, d, C2D_Color32(250, 246, 235, 255));
		float pos = s_pos[r];
		int base = (int)floorf(pos);
		float frac = pos - base;
		for (int row = -1; row <= 3; row++) {
			float cy = REELS_Y + (row + 0.5f) * SYM_H - frac * SYM_H;
			if (cy < REELS_Y + 10 || cy > REELS_Y + 3 * SYM_H - 10)
				continue;
			int sym = TP_REEL[r][((base + row - 1) % TP_STRIP + TP_STRIP) % TP_STRIP];
			draw_symbol(sym, x0 + REEL_W / 2, cy, 1.0f, d);
		}
		/* Sombra arriba y abajo: efecto de cilindro */
		gfx_rect(x0 + 2, REELS_Y, REEL_W - 4, 10, d, C2D_Color32(0, 0, 0, 70));
		gfx_rect(x0 + 2, REELS_Y + 3 * SYM_H - 10, REEL_W - 4, 10, d, C2D_Color32(0, 0, 0, 70));
	}
	/* Líneas jugadas y premiadas */
	for (int l = 0; l < LINES[s_lines_idx]; l++) {
		bool win = !s_spinning && (s_win_mask >> l & 1);
		if (s_spinning || (s_win_mask && !win))
			continue;
		u32 c = gfx_rgb(LINE_COL[l], win ? 255 : 90);
		float y0 = REELS_Y + (TP_LINE[l][0] + 0.5f) * SYM_H, y2 = REELS_Y + (TP_LINE[l][2] + 0.5f) * SYM_H;
		float ym = REELS_Y + (TP_LINE[l][1] + 0.5f) * SYM_H;
		float w = win ? 3.0f : 1.5f;
		gfx_line(REELS_X, y0, REELS_X + 1.5f * REEL_W, ym, w, DEPTH_UI, c);
		gfx_line(REELS_X + 1.5f * REEL_W, ym, REELS_X + 3 * REEL_W, y2, w, DEPTH_UI, c);
		gfx_ellipse(REELS_X - 10, y0, 5, 5, DEPTH_UI, gfx_rgb(LINE_COL[l], 255));
	}
	/* Palanca */
	float pull = s_spinning && s_t < 0.4f ? sinf(s_t / 0.4f * 3.14159f) : 0.0f;
	float kx = REELS_X + 3 * REEL_W + 44, ky = 70 + pull * 80;
	gfx_rect(kx - 3, 120, 6, 40, DEPTH_TABLE, C2D_Color32(120, 120, 130, 255));
	gfx_line(kx, 140, kx, ky, 4, DEPTH_TABLE, C2D_Color32(190, 190, 200, 255));
	gfx_ellipse(kx, ky, 9, 9, DEPTH_TABLE, C2D_Color32(220, 30, 40, 255));
	gfx_ellipse(kx - 3, ky - 3, 3, 3, DEPTH_TABLE, C2D_Color32(255, 160, 160, 255));
	/* Marcador */
	gfx_rrect(REELS_X - 6, 218, 3 * REEL_W + 12, 18, 4, DEPTH_UI, C2D_Color32(20, 10, 10, 255));
	gfx_textf(REELS_X, 219, DEPTH_UI, 0.45f, C2D_Color32(255, 120, 60, 255), C2D_AlignLeft, "Fichas %d", wallet_chips());
	gfx_textf(REELS_X + 3 * REEL_W, 219, DEPTH_UI, 0.45f, C2D_Color32(255, 120, 60, 255), C2D_AlignRight, "Premio %d", s_win);
	if (s_win >= 50 * BETS[s_bet_idx] && !s_spinning && banner_draw(s_win_time, 200, 120, 260, 56))
		gfx_textf(200, 106, DEPTH_BANNER, 0.7f, COLOR_GOLD, C2D_AlignCenter, "¡GRAN PREMIO! %d", s_win);
}

static void tp_draw_bottom(void)
{
	gfx_background(BG_TABLE_BOTTOM);
	gfx_text(s_message, 6, 4, 0, 0.45f, COLOR_GOLD, C2D_AlignLeft);
	button_draw(&s_btn_menu, false);
	/* Tabla de premios (por línea, veces la apuesta) */
	gfx_panel(6, 26, BOTTOM_W - 12, 150, 0, C2D_Color32(40, 10, 15, 220), COLOR_GOLD);
	static const int ORDER[TP_SYMBOLS] = { TP_SEVEN, TP_BAR, TP_BELL, TP_CHERRY, TP_PLUM, TP_ORANGE, TP_LEMON };
	for (int i = 0; i < TP_SYMBOLS; i++) {
		int sym = ORDER[i];
		float x = i < 4 ? 14 : 166, y = 46 + (i % 4) * 30;
		for (int k = 0; k < 3; k++)
			draw_symbol(sym, x + 12 + k * 26, y, 0.5f, 0);
		int three[3] = { sym, sym, sym };
		gfx_textf(x + 96, y - 8, 0, 0.5f, COLOR_GOLD, C2D_AlignLeft, "x%d", tp_line_pay(three) * BETS[s_bet_idx]);
	}
	draw_symbol(TP_CHERRY, 204, 136, 0.5f, 0);
	draw_symbol(TP_CHERRY, 230, 136, 0.5f, 0);
	gfx_textf(262, 128, 0, 0.5f, COLOR_GOLD, C2D_AlignLeft, "x%d", 5 * BETS[s_bet_idx]);
	draw_symbol(TP_CHERRY, 230, 162, 0.5f, 0);
	gfx_textf(262, 154, 0, 0.5f, COLOR_GOLD, C2D_AlignLeft, "x%d", 2 * BETS[s_bet_idx]);
	gfx_text("Cerezas: desde la izquierda", 20, 156, 0, 0.36f, COLOR_CREAM, C2D_AlignLeft);
	gfx_textf(BOTTOM_W / 2, 182, 0, 0.45f, COLOR_CREAM, C2D_AlignCenter, "Cada tirada: %d fichas  ·  Tienes %d", cost(),
		wallet_chips());
	button_draw(&s_btn_bet, false);
	button_draw(&s_btn_lines, false);
	button_draw(&s_btn_spin, true);
}

/* Salir con los rodillos girando: la tirada se anula y se devuelve */
static void scene_leave(void)
{
	if (s_spinning && s_paid)
		wallet_give(s_paid);
	s_spinning = false;
	s_paid = 0;
	s_started = false;
}

const Scene scene_tragaperras = { tp_enter, tp_update, tp_draw_top, tp_draw_bottom, scene_leave };
