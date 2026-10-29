/* Bingo de 90 bolas: el bombo y el panel arriba, tus cartones abajo. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "games/bingo.h"
#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/gfx.h"
#include "ui/mii.h"
#include "ui/widgets.h"

#define CELL_W  34.0f
#define CELL_H  25.0f
#define CARD_X  ((BOTTOM_W - BI_COLS * CELL_W) / 2.0f)
#define CARD_HEAD 11.0f
#define CARD_TOTAL_H (CARD_HEAD + BI_ROWS * CELL_H + 4)

static const CpuId RIVAL[BI_PLAYERS] = { CPU_COUNT, CPU_CARMEN, CPU_RAMON, CPU_LOLA };
static const float SPEEDS[3] = { 4.0f, 2.6f, 1.6f };
static const char* const SPEED_NAME[3] = { "lento", "normal", "rápido" };

static Bingo s_game;
static bool s_started;
static int s_ncards = 2;
static int s_page;
static int s_speed = 1;
static bool s_auto = true;
static float s_ball_timer, s_pause, s_ball_anim, s_end_time;
static float s_claim_at[BI_PLAYERS][2]; /* cuándo canta línea/bingo cada rival (-1 nada) */
static float s_clock;
static Mood s_mood[BI_PLAYERS];
static char s_message[96];
static float s_banner_time;
static char s_banner[48];
static Button s_btn_n[BI_MAX_CARDS], s_btn_go, s_btn_line, s_btn_bingo, s_btn_menu;

static const char* name(int p)
{
	return p == 0 ? profile_get()->avatar.name : cpu_name(RIVAL[p]);
}

static const Avatar* avatar(int p)
{
	return p == 0 ? &profile_get()->avatar : cpu_avatar(RIVAL[p]);
}

static const char* nickname(int n)
{
	switch (n) {
	case 1:
		return "el primero";
	case 13:
		return "la mala suerte";
	case 15:
		return "la niña bonita";
	case 22:
		return "los dos patitos";
	case 33:
		return "la edad de Cristo";
	case 69:
		return "del derecho y del revés";
	case 88:
		return "las dos gordas";
	case 90:
		return "el abuelo";
	default:
		return NULL;
	}
}

static void message(const char* m)
{
	snprintf(s_message, sizeof s_message, "%s", m);
}

static void banner(const char* t)
{
	snprintf(s_banner, sizeof s_banner, "%s", t);
	s_banner_time = 0.0f;
	s_pause = 2.5f;
}

static void bi_enter(void)
{
	if (!s_started) {
		bi_init(&s_game, (uint32_t)time(NULL) ^ 0xB160);
		s_started = true;
		message("¿Cuántos cartones quieres?");
	}
	button_set(&s_btn_menu, 258, 2, 54, 18, "Menú", C2D_Color32(120, 30, 35, 255), KEY_START);
	static char labels[BI_MAX_CARDS][8];
	for (int i = 0; i < BI_MAX_CARDS; i++) {
		snprintf(labels[i], sizeof labels[i], "%d", i + 1);
		button_set(&s_btn_n[i], 40 + i * 62, 120, 54, 40, labels[i], C2D_Color32(60, 90, 160, 255), 0);
	}
}

static void start(void)
{
	if (!bi_start(&s_game, s_ncards)) {
		audio_play(SFX_ERROR);
		message("No tienes fichas para tantos cartones");
		return;
	}
	s_page = 0;
	s_ball_timer = 0.0f;
	s_pause = 1.0f;
	s_end_time = 0.0f;
	s_banner_time = 99.0f;
	for (int p = 0; p < BI_PLAYERS; p++)
		s_claim_at[p][0] = s_claim_at[p][1] = -1.0f;
	audio_play(SFX_CONFIRM);
	message("¡Empieza el bingo! Atento a las bolas");
}

/* Si marcas a mano, la línea o el bingo tienen que estar marcados */
static bool marked_ok(bool bingo)
{
	if (s_auto)
		return true;
	for (int i = 0; i < s_game.card_n[0]; i++) {
		const BiCard* c = &s_game.cards[0][i];
		int full_rows = 0;
		for (int r = 0; r < BI_ROWS; r++) {
			bool ok = true;
			for (int k = 0; k < BI_COLS; k++)
				if (c->num[r][k] && !(c->marked[r][k] && s_game.drawn[c->num[r][k]]))
					ok = false;
			full_rows += ok;
		}
		if (bingo ? full_rows == BI_ROWS : full_rows > 0)
			return true;
	}
	return false;
}

static void claimed_line(int p)
{
	char buf[48];
	snprintf(buf, sizeof buf, "¡Línea! %s", p == 0 ? "¡Para ti!" : name(p));
	banner(buf);
	if (p == 0)
		snprintf(s_message, sizeof s_message, "Ganas %d fichas por la línea", s_game.line_prize);
	else
		snprintf(s_message, sizeof s_message, "%s canta línea: %d fichas", name(p), s_game.line_prize);
	mood_set(&s_mood[p], EXPR_HAPPY, 3.0f);
	audio_play(p == 0 ? SFX_WIN : SFX_CHIP);
}

static void claimed_bingo(int p)
{
	char buf[48];
	snprintf(buf, sizeof buf, "¡BINGO! %s", p == 0 ? "¡Has ganado!" : name(p));
	banner(buf);
	for (int q = 0; q < BI_PLAYERS; q++)
		mood_set(&s_mood[q], q == p ? EXPR_HAPPY : EXPR_SAD, 5.0f);
	audio_play(p == 0 ? SFX_WIN : SFX_LOSE);
	profile_record_game(GAME_BINGO, p == 0);
	s_end_time = 0.0f;
}

static float rnd(float a, float b)
{
	return a + (b - a) * (float)rng_range(&s_game.rng, 1000) / 1000.0f;
}

static void draw_ball(void)
{
	int b = bi_draw(&s_game);
	if (!b)
		return;
	s_ball_anim = 0.0f;
	audio_play(SFX_CHIP);
	const char* nick = nickname(b);
	if (nick)
		snprintf(s_message, sizeof s_message, "¡El %d! %s", b, nick);
	else
		snprintf(s_message, sizeof s_message, "El %d", b);
	if (s_auto)
		bi_mark_all(&s_game, 0);
	/* Los rivales cantan en cuanto se dan cuenta */
	for (int p = 1; p < BI_PLAYERS; p++) {
		if (bi_has_line(&s_game, p) && s_claim_at[p][0] < 0)
			s_claim_at[p][0] = s_clock + rnd(0.7f, 2.0f);
		if (bi_has_bingo(&s_game, p) && s_claim_at[p][1] < 0)
			s_claim_at[p][1] = s_clock + rnd(0.7f, 2.0f);
	}
	if (bi_has_bingo(&s_game, 0))
		message("¡Tienes bingo! ¡Cántalo con " GLYPH_Y "!");
	else if (bi_has_line(&s_game, 0))
		message("¡Tienes línea! ¡Cántala con " GLYPH_X "!");
}

/* Cartón i en la pantalla de abajo: su esquina superior izquierda */
static bool bingo_card_pos(int i, float* x, float* y)
{
	int first = s_page * 2;
	if (i < first || i >= first + 2)
		return false;
	*x = CARD_X;
	*y = 22 + (i - first) * (CARD_TOTAL_H + 3);
	return true;
}

static SceneId bi_update(const Input* in, float dt)
{
	if (button_update(&s_btn_menu, in))
		return SCENE_MENU;
	s_clock += dt;
	s_ball_anim += dt;
	s_banner_time += dt;
	if (in->down & KEY_SELECT) {
		s_auto = !s_auto;
		if (s_auto)
			bi_mark_all(&s_game, 0);
		message(s_auto ? "Las bolas se marcan solas" : "Marca tú los números tocándolos");
	}
	if (in->down & KEY_UP)
		s_speed = s_speed < 2 ? s_speed + 1 : 2;
	if (in->down & KEY_DOWN)
		s_speed = s_speed > 0 ? s_speed - 1 : 0;

	if (s_game.phase == BI_SETUP || s_game.phase == BI_OVER) {
		for (int i = 0; i < BI_MAX_CARDS; i++)
			if (button_update(&s_btn_n[i], in))
				s_ncards = i + 1;
		if (in->repeat & KEY_LEFT && s_ncards > 1)
			s_ncards--;
		if (in->repeat & KEY_RIGHT && s_ncards < BI_MAX_CARDS)
			s_ncards++;
		static char go[32];
		snprintf(go, sizeof go, GLYPH_A " Jugar (%d fichas)", s_ncards * BI_PRICE);
		button_set(&s_btn_go, 60, 200, 200, 30, go, C2D_Color32(40, 140, 60, 255), KEY_A);
		s_end_time += dt;
		s_btn_go.enabled = s_game.phase == BI_SETUP || s_end_time > 1.5f;
		if (button_update(&s_btn_go, in))
			start();
	} else {
		button_set(&s_btn_line, 8, 206, 120, 28, GLYPH_X " ¡Línea!", C2D_Color32(60, 90, 160, 255), KEY_X);
		button_set(&s_btn_bingo, 136, 206, 120, 28, GLYPH_Y " ¡Bingo!", C2D_Color32(200, 40, 45, 255), KEY_Y);
		s_btn_line.enabled = s_game.line_winner < 0;
		if (button_update(&s_btn_line, in)) {
			if (bi_has_line(&s_game, 0) && marked_ok(false) && bi_claim_line(&s_game, 0))
				claimed_line(0);
			else {
				audio_play(SFX_ERROR);
				message(s_auto || !bi_has_line(&s_game, 0) ? "¡No tienes línea!" : "Marca antes los números de la línea");
			}
		}
		if (button_update(&s_btn_bingo, in)) {
			if (bi_has_bingo(&s_game, 0) && marked_ok(true) && bi_claim_bingo(&s_game, 0))
				claimed_bingo(0);
			else {
				audio_play(SFX_ERROR);
				message(s_auto || !bi_has_bingo(&s_game, 0) ? "¡Aún no tienes bingo!" : "Marca antes todos los números");
			}
		}
		/* Cambiar de cartones */
		int pages = (s_game.card_n[0] + 1) / 2;
		if (pages > 1 && (in->down & (KEY_L | KEY_R)))
			s_page = (s_page + 1) % pages;
		if (pages > 1 && in->touch_down && in->tx > 262 && in->ty > 204)
			s_page = (s_page + 1) % pages;
		/* Marcar a mano */
		if (!s_auto && in->touch_down)
			for (int i = 0; i < s_game.card_n[0]; i++) {
				float x, y;
				if (!bingo_card_pos(i, &x, &y))
					continue;
				float cy = y + CARD_HEAD;
				if (in->tx >= x && in->tx < x + BI_COLS * CELL_W && in->ty >= cy && in->ty < cy + BI_ROWS * CELL_H) {
					int col = (int)((in->tx - x) / CELL_W), row = (int)((in->ty - cy) / CELL_H);
					if (s_game.cards[0][i].num[row][col]) {
						if (bi_mark(&s_game, i, row, col))
							audio_play(SFX_SELECT);
						else
							audio_play(SFX_ERROR);
					}
				}
			}
		/* Rivales que cantan */
		for (int p = 1; p < BI_PLAYERS && s_game.phase == BI_PLAYING; p++) {
			if (s_claim_at[p][0] >= 0 && s_clock >= s_claim_at[p][0]) {
				s_claim_at[p][0] = -1.0f;
				if (bi_claim_line(&s_game, p))
					claimed_line(p);
			}
			if (s_claim_at[p][1] >= 0 && s_clock >= s_claim_at[p][1]) {
				s_claim_at[p][1] = -1.0f;
				if (bi_claim_bingo(&s_game, p))
					claimed_bingo(p);
			}
		}
		/* Siguiente bola */
		if (s_game.phase == BI_PLAYING) {
			if (s_pause > 0.0f)
				s_pause -= dt;
			else {
				s_ball_timer += dt;
				if (s_ball_timer >= SPEEDS[s_speed]) {
					s_ball_timer = 0.0f;
					draw_ball();
				}
			}
		}
	}
	for (int p = 0; p < BI_PLAYERS; p++)
		mood_update(&s_mood[p], dt);
	return SCENE_BINGO;
}

/* ---------------------------------------------------------------------------
 * Dibujo
 * ------------------------------------------------------------------------- */

static u32 ball_color(int n)
{
	static const u32 cols[9] = { 0xD83A3A, 0xE07A1E, 0xE0B81E, 0x4CA84C, 0x2E9C9C,
		0x2E6CC8, 0x6A4CC0, 0xB04CB0, 0x707070 };
	int c = n >= 90 ? 8 : n / 10;
	return cols[c];
}

static void draw_ball_at(float x, float y, float r, int n, float depth)
{
	gfx_ellipse(x + 1.5f + gfx_dx(depth), y + 2, r, r, depth, C2D_Color32(0, 0, 0, 80));
	gfx_ellipse(x + gfx_dx(depth), y, r, r, depth, gfx_rgb(ball_color(n), 255));
	gfx_ellipse(x + gfx_dx(depth), y, r * 0.62f, r * 0.62f, depth, C2D_Color32(255, 255, 255, 255));
	gfx_ellipse(x - r * 0.45f + gfx_dx(depth), y - r * 0.5f, r * 0.22f, r * 0.15f, depth, C2D_Color32(255, 255, 255, 160));
	gfx_textf(x, y - r * 0.42f, depth, r / 22.0f > 0.45f ? r / 22.0f : 0.45f, C2D_Color32(30, 30, 40, 255),
		C2D_AlignCenter, "%d", n);
}

static void bi_draw_top(void)
{
	gfx_background(BG_TABLE_TOP);
	float t = gfx_time();
	/* El bombo */
	float bx = 82, by = 92, br = 60;
	gfx_ellipse(bx + gfx_dx(DEPTH_CARDS), by, br + 4, br + 4, DEPTH_CARDS, C2D_Color32(150, 110, 40, 255));
	gfx_ellipse(bx + gfx_dx(DEPTH_CARDS), by, br, br, DEPTH_CARDS, C2D_Color32(220, 235, 245, 120));
	int left = BI_BALLS - s_game.drawn_n;
	int shown = left > 18 ? 18 : left;
	for (int i = 0; i < shown; i++) {
		float a = t * (s_game.phase == BI_PLAYING ? 2.2f : 0.3f) + i * 2.399f;
		float rr = 18 + (i * 37 % 30);
		float x = bx + cosf(a) * rr, y = by + sinf(a * 1.3f) * rr * 0.8f + 10;
		gfx_ellipse(x + gfx_dx(DEPTH_CARDS + 0.2f), y, 7, 7, DEPTH_CARDS + 0.2f, gfx_rgb(ball_color(i * 5 + 1), 255));
	}
	gfx_rect(bx - 6 + gfx_dx(DEPTH_CARDS), by + br, 12, 20, DEPTH_CARDS, C2D_Color32(150, 110, 40, 255));
	/* La última bola sale del bombo */
	int last = bi_last(&s_game);
	if (last) {
		float k = fminf(1.0f, s_ball_anim / 0.4f);
		float e = 1.0f - (1.0f - k) * (1.0f - k);
		draw_ball_at(bx + 60 * e, by + 70 - 30 * e, 12 + 14 * e, last, DEPTH_UI + 1.0f);
	}
	/* Las cinco anteriores */
	for (int i = 1; i <= 5 && s_game.drawn_n - 1 - i >= 0; i++)
		draw_ball_at(18 + (i - 1) * 26, 214, 11, s_game.order[s_game.drawn_n - 1 - i], DEPTH_UI);
	/* Panel de las 90 bolas */
	float px = 184, py = 8;
	gfx_panel(px - 4, py - 4, 10 * 21 + 8, 9 * 15 + 8, DEPTH_UI, C2D_Color32(10, 40, 20, 220), C2D_Color32(150, 110, 40, 255));
	for (int n = 1; n <= BI_BALLS; n++) {
		int r = (n - 1) / 10, c = (n - 1) % 10;
		float x = px + c * 21, y = py + r * 15;
		bool on = s_game.drawn[n];
		if (on)
			gfx_rect(x + gfx_dx(DEPTH_UI), y, 20, 14, DEPTH_UI,
				n == last ? C2D_Color32(250, 200, 50, 255) : gfx_rgb(ball_color(n), 255));
		gfx_textf(x + 10, y, DEPTH_UI, 0.45f, on ? COLOR_WHITE : C2D_Color32(90, 130, 100, 255), C2D_AlignCenter, "%d", n);
	}
	gfx_textf(px + 105, py + 140, DEPTH_UI, 0.42f, COLOR_CREAM, C2D_AlignCenter, "Bolas: %d · velocidad %s (cruceta)",
		s_game.drawn_n, SPEED_NAME[s_speed]);
	/* Premios y rivales */
	if (s_game.phase != BI_SETUP) {
		gfx_textf(px + 105, py + 158, DEPTH_UI, 0.45f, COLOR_GOLD, C2D_AlignCenter, "Línea %d%s · Bingo %d",
			s_game.line_prize, s_game.line_winner >= 0 ? " (cantada)" : "", s_game.bingo_prize);
		for (int p = 1; p < BI_PLAYERS; p++) {
			float x = 172 + (p - 1) * 76, y = 206;
			mii_draw(avatar(p), x, y, 11, DEPTH_UI, mood_expr(&s_mood[p]), false);
			gfx_text(name(p), x + 14, y - 14, DEPTH_UI, 0.42f, COLOR_CREAM, C2D_AlignLeft);
			int m = bi_best_missing(&s_game, p);
			gfx_textf(x + 14, y, DEPTH_UI, 0.42f, m <= 2 ? COLOR_GOLD : C2D_Color32(200, 230, 200, 255), C2D_AlignLeft,
				m ? "faltan %d" : "¡bingo!", m);
		}
	}
	gfx_textf(150, 176, DEPTH_UI, 0.42f, COLOR_CREAM, C2D_AlignLeft, "Fichas: %d", s_game.chips[0]);
	if (s_banner_time < 3.0f && banner_draw(s_banner_time, 200, 110, 300, 56))
		gfx_text(s_banner, 200, 98, DEPTH_BANNER, 0.75f, COLOR_GOLD, C2D_AlignCenter);
}

static void draw_card(int i, float x, float y)
{
	const BiCard* c = &s_game.cards[0][i];
	u32 blue = C2D_Color32(40, 85, 170, 255);
	gfx_rect(x - 3, y - 2, BI_COLS * CELL_W + 6, CARD_TOTAL_H, 0, blue);
	gfx_textf(x + 2, y - 2, 0, 0.42f, COLOR_WHITE, C2D_AlignLeft, "Cartón nº %d", i + 1);
	bool winner_line = s_game.line_winner == 0 && s_game.line_card == i;
	float cy = y + CARD_HEAD;
	for (int r = 0; r < BI_ROWS; r++)
		for (int k = 0; k < BI_COLS; k++) {
			float cx = x + k * CELL_W, yy = cy + r * CELL_H;
			int n = c->num[r][k];
			u32 fill = n ? C2D_Color32(255, 255, 255, 255) : C2D_Color32(175, 200, 232, 255);
			if (n && winner_line && s_game.line_row == r)
				fill = C2D_Color32(255, 240, 170, 255);
			gfx_rect(cx + 1, yy + 1, CELL_W - 2, CELL_H - 2, 0, fill);
			if (!n)
				continue;
			gfx_textf_plain(cx + CELL_W / 2, yy + 2, 0, 0.75f, blue, C2D_AlignCenter, "%d", n);
			if (c->marked[r][k])
				gfx_ellipse(cx + CELL_W / 2, yy + CELL_H / 2, 10, 10, 0, C2D_Color32(220, 40, 60, 140));
			else if (!s_auto && s_game.drawn[n])
				gfx_ellipse(cx + CELL_W - 5, yy + 5, 2.5f, 2.5f, 0, C2D_Color32(220, 40, 60, 200));
		}
	if (s_game.phase == BI_PLAYING) {
		int m = bi_card_missing(&s_game, c);
		gfx_textf(x + BI_COLS * CELL_W, y - 2, 0, 0.42f, m <= 3 ? COLOR_GOLD : COLOR_WHITE, C2D_AlignRight,
			m ? "Faltan %d" : "¡Completo!", m);
	}
}

static void bi_draw_bottom(void)
{
	gfx_background(BG_TABLE_BOTTOM);
	gfx_text(s_message, 8, 3, 0, 0.45f, COLOR_GOLD, C2D_AlignLeft);
	button_draw(&s_btn_menu, false);
	if (s_game.phase == BI_SETUP || (s_game.phase == BI_OVER && s_end_time > 1.5f)) {
		gfx_text(s_game.phase == BI_OVER ? "¿Otra partida? Elige cartones" : "Elige cuántos cartones juegas", BOTTOM_W / 2,
			40, 0, 0.55f, COLOR_GOLD, C2D_AlignCenter);
		gfx_textf(BOTTOM_W / 2, 66, 0, 0.47f, COLOR_CREAM, C2D_AlignCenter,
			"Cada cartón cuesta %d fichas. Tienes %d.", BI_PRICE, s_game.chips[0]);
		gfx_text("Línea: 30 % del bote · Bingo: el resto", BOTTOM_W / 2, 88, 0, 0.45f, C2D_Color32(200, 230, 200, 255),
			C2D_AlignCenter);
		for (int i = 0; i < BI_MAX_CARDS; i++) {
			s_btn_n[i].color = i + 1 == s_ncards ? C2D_Color32(220, 150, 30, 255) : C2D_Color32(60, 90, 160, 255);
			button_draw(&s_btn_n[i], (i + 1) * BI_PRICE <= s_game.chips[0]);
		}
		gfx_text(s_auto ? "SELECT: se marcan solas" : "SELECT: marcas tú", BOTTOM_W / 2, 172, 0, 0.42f, COLOR_CREAM,
			C2D_AlignCenter);
		button_draw(&s_btn_go, s_btn_go.enabled);
		return;
	}
	for (int i = 0; i < s_game.card_n[0]; i++) {
		float x, y;
		if (bingo_card_pos(i, &x, &y))
			draw_card(i, x, y);
	}
	if (s_game.phase == BI_PLAYING) {
		button_draw(&s_btn_line, s_btn_line.enabled);
		button_draw(&s_btn_bingo, true);
	}
	int pages = (s_game.card_n[0] + 1) / 2;
	if (pages > 1) {
		gfx_rrect(264, 206, 50, 28, 6, 0, C2D_Color32(60, 90, 160, 255));
		gfx_textf(289, 212, 0, 0.45f, COLOR_WHITE, C2D_AlignCenter, "L/R %d/%d", s_page + 1, pages);
	}
}

const Scene scene_bingo = { bi_enter, bi_update, bi_draw_top, bi_draw_bottom };
