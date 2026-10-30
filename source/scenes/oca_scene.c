/* El juego de la oca: el tablero en espiral arriba y el dado abajo. */
#include <math.h>
#include <stdio.h>
#include <time.h>

#include "games/oca.h"
#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/gfx.h"
#include "ui/mii.h"
#include "ui/widgets.h"

#define COLS   9
#define ROWS   7
#define CELL_W 42.0f
#define CELL_H 32.0f
#define X0     ((TOP_W - COLS * CELL_W) / 2.0f)
#define Y0     ((240.0f - ROWS * CELL_H) / 2.0f)

static const CpuId CPUS[OCA_MAX_PLAYERS] = { CPU_COUNT, CPU_PACO, CPU_LOLA, CPU_PEPE };
static const u32 TOKEN[OCA_MAX_PLAYERS] = { 0xDC282D, 0x2864C8, 0x3CA846, 0xF0B41E };

typedef enum { ANIM_NONE, ANIM_ROLL, ANIM_WALK, ANIM_JUMP } Anim;

static Oca s_game;
static bool s_started;
static int s_players = 4;
static int s_cell_col[OCA_GOAL + 1], s_cell_row[OCA_GOAL + 1];
static float s_disp_x[OCA_MAX_PLAYERS], s_disp_y[OCA_MAX_PLAYERS];
static int s_shown[OCA_MAX_PLAYERS]; /* casilla en la que se ve cada ficha */
static Anim s_anim;
static float s_anim_t, s_cpu_timer, s_end_time;
static int s_from, s_step, s_face = 1, s_mover;
static Mood s_mood[OCA_MAX_PLAYERS];
static char s_message[96];
static bool s_recorded;
static Button s_btn_roll, s_btn_players, s_btn_menu;

/* La espiral: de la casilla 1 (abajo a la izquierda) hacia el centro */
static void build_spiral(void)
{
	int top = 0, bottom = ROWS - 1, left = 0, right = COLS - 1, n = 1;
	while (n <= OCA_GOAL) {
		for (int c = left; c <= right && n <= OCA_GOAL; c++, n++) {
			s_cell_col[n] = c;
			s_cell_row[n] = bottom;
		}
		bottom--;
		for (int r = bottom; r >= top && n <= OCA_GOAL; r--, n++) {
			s_cell_col[n] = right;
			s_cell_row[n] = r;
		}
		right--;
		for (int c = right; c >= left && n <= OCA_GOAL; c--, n++) {
			s_cell_col[n] = c;
			s_cell_row[n] = top;
		}
		top++;
		for (int r = top; r <= bottom && n <= OCA_GOAL; r++, n++) {
			s_cell_col[n] = left;
			s_cell_row[n] = r;
		}
		left++;
	}
}

static float cell_x(int n)
{
	return X0 + s_cell_col[n] * CELL_W + CELL_W / 2;
}

static float cell_y(int n)
{
	return Y0 + s_cell_row[n] * CELL_H + CELL_H / 2;
}

/* Posición de la ficha de p dentro de la casilla, para que no se tapen */
static void token_target(int p, int n, float* x, float* y)
{
	static const float off[4][2] = { { -10, 6 }, { 10, 6 }, { -10, -6 }, { 10, -6 } };
	*x = cell_x(n) + off[p][0];
	*y = cell_y(n) + off[p][1] + 2;
}

static const char* name(int p)
{
	return p == 0 ? profile_get()->avatar.name : cpu_name(CPUS[p]);
}

static const Avatar* avatar(int p)
{
	return p == 0 ? &profile_get()->avatar : cpu_avatar(CPUS[p]);
}

static void set_message(const char* m)
{
	snprintf(s_message, sizeof s_message, "%s", m);
}

static void new_game(void)
{
	oca_init(&s_game, s_players, (uint32_t)time(NULL) ^ 0x0CA);
	for (int p = 0; p < OCA_MAX_PLAYERS; p++) {
		s_shown[p] = 1;
		token_target(p, 1, &s_disp_x[p], &s_disp_y[p]);
	}
	s_anim = ANIM_NONE;
	s_recorded = false;
	s_end_time = 0.0f;
	set_message("¡A jugar! Empiezas tú");
}

static void oca_enter(void)
{
	if (!s_started) {
		build_spiral();
		s_started = true;
		new_game();
	}
	button_set(&s_btn_menu, 258, 6, 54, 20, "Menú", C2D_Color32(120, 30, 35, 255), KEY_START);
}

static const char* square_label(OcaSquare s)
{
	switch (s) {
	case OCA_GOOSE:
		return "Oca";
	case OCA_BRIDGE:
		return "Puente";
	case OCA_DICE:
		return "Dados";
	case OCA_INN:
		return "Posada";
	case OCA_WELL:
		return "Pozo";
	case OCA_MAZE:
		return "Lab.";
	case OCA_JAIL:
		return "Cárcel";
	case OCA_DEATH:
		return "Muerte";
	case OCA_GARDEN:
		return "Jardín";
	default:
		return NULL;
	}
}

static void announce(void)
{
	int p = s_game.last_player;
	char buf[96];
	const char* n = p == 0 ? "Tú" : name(p);
	switch (s_game.last_square) {
	case OCA_GOOSE:
		snprintf(buf, sizeof buf, "¡De oca a oca y tiro porque me toca!");
		break;
	case OCA_BRIDGE:
		snprintf(buf, sizeof buf, "¡De puente a puente y tiro porque me lleva la corriente!");
		break;
	case OCA_DICE:
		snprintf(buf, sizeof buf, "¡De dado a dado y tiro porque me ha tocado!");
		break;
	case OCA_INN:
		snprintf(buf, sizeof buf, "%s: posada, un turno sin tirar", n);
		break;
	case OCA_WELL:
		if (s_game.last_freed >= 0)
			snprintf(buf, sizeof buf, "%s cae al pozo y saca a %s", n, s_game.last_freed == 0 ? "ti" : name(s_game.last_freed));
		else
			snprintf(buf, sizeof buf, "%s cae al pozo: hasta que otro le saque", n);
		break;
	case OCA_MAZE:
		snprintf(buf, sizeof buf, "%s se pierde en el laberinto: a la 30", n);
		break;
	case OCA_JAIL:
		snprintf(buf, sizeof buf, "%s va a la cárcel: dos turnos sin tirar", n);
		break;
	case OCA_DEATH:
		snprintf(buf, sizeof buf, "¡La muerte! %s vuelve a empezar", n);
		break;
	case OCA_GARDEN:
		snprintf(buf, sizeof buf, "¡%s llega al jardín de la oca!", n);
		break;
	default:
		snprintf(buf, sizeof buf, "%s saca un %d", n, s_game.last_roll);
		break;
	}
	set_message(buf);
	if (s_game.last_square == OCA_GOOSE || s_game.last_square == OCA_BRIDGE || s_game.last_square == OCA_DICE) {
		mood_set(&s_mood[p], EXPR_HAPPY, 1.5f);
		audio_play(SFX_CHIP);
	} else if (s_game.last_square >= OCA_INN && s_game.last_square <= OCA_DEATH) {
		mood_set(&s_mood[p], s_game.last_square == OCA_DEATH ? EXPR_SURPRISED : EXPR_SAD, 2.0f);
		audio_play(SFX_LOSE);
	}
}

static void start_roll(void)
{
	s_anim = ANIM_ROLL;
	s_anim_t = 0.0f;
	s_mover = s_game.current;
	s_from = s_game.pos[s_mover];
	audio_play(SFX_FLIP);
}

static SceneId oca_update(const Input* in, float dt)
{
	if (button_update(&s_btn_menu, in))
		return SCENE_MENU;
	static char players_label[24];
	snprintf(players_label, sizeof players_label, GLYPH_X " %d jugadores", s_players);
	button_set(&s_btn_players, 12, 205, 144, 28, players_label, C2D_Color32(60, 90, 160, 255), KEY_X);
	if (s_anim == ANIM_NONE && button_update(&s_btn_players, in)) {
		if (!s_recorded && s_game.winner < 0 && s_game.last_player >= 0)
			profile_record_game(GAME_OCA, false);
		s_players = s_players == 4 ? 2 : s_players + 1;
		new_game();
		return SCENE_OCA;
	}
	bool over = s_game.winner >= 0;
	button_set(&s_btn_roll, 164, 205, 144, 28, over ? GLYPH_A " Otra partida" : GLYPH_A " Tirar el dado",
		C2D_Color32(40, 140, 60, 255), KEY_A);
	s_btn_roll.enabled = s_anim == ANIM_NONE && (over ? s_end_time > 0.8f : s_game.current == 0);

	switch (s_anim) {
	case ANIM_NONE:
		if (over) {
			s_end_time += dt;
			if (button_update(&s_btn_roll, in))
				new_game();
		} else if (s_game.current == 0) {
			bool die_touched = in->touch_down && fabsf(in->tx - 250) < 34 && fabsf(in->ty - 100) < 34;
			if (die_touched || button_update(&s_btn_roll, in))
				start_roll();
		} else {
			s_cpu_timer += dt;
			if (s_cpu_timer > 0.7f) {
				s_cpu_timer = 0.0f;
				start_roll();
			}
		}
		break;
	case ANIM_ROLL:
		s_anim_t += dt;
		s_face = 1 + (int)(gfx_time() * 20.0f) % 6;
		if (s_anim_t > 0.5f) {
			oca_roll(&s_game, 0);
			s_face = s_game.last_roll;
			s_anim = ANIM_WALK;
			s_step = 0;
			s_anim_t = 0.0f;
		}
		break;
	case ANIM_WALK:
		s_anim_t += dt;
		if (s_anim_t > 0.16f) {
			s_anim_t = 0.0f;
			s_step++;
			int n = s_from + s_step;
			if (n > OCA_GOAL)
				n = OCA_GOAL - (n - OCA_GOAL);
			s_shown[s_mover] = n;
			audio_play(SFX_SELECT);
			if (s_step >= s_game.last_roll) {
				announce();
				s_anim = s_game.last_to != s_game.last_land ? ANIM_JUMP : ANIM_NONE;
				s_anim_t = -0.4f;
			}
		}
		break;
	case ANIM_JUMP:
		s_anim_t += dt;
		if (s_anim_t > 0.0f) {
			s_shown[s_mover] = s_game.last_to;
			s_anim = ANIM_NONE;
		}
		break;
	}
	/* Las fichas se deslizan hacia su casilla */
	for (int p = 0; p < s_players; p++) {
		float tx, ty;
		token_target(p, s_shown[p], &tx, &ty);
		float k = fminf(1.0f, dt * (s_anim == ANIM_JUMP || s_anim == ANIM_NONE ? 6.0f : 14.0f));
		s_disp_x[p] += (tx - s_disp_x[p]) * k;
		s_disp_y[p] += (ty - s_disp_y[p]) * k;
		mood_update(&s_mood[p], dt);
	}
	if (s_game.winner >= 0 && s_anim == ANIM_NONE && !s_recorded) {
		s_recorded = true;
		s_end_time = 0.0f;
		bool won = s_game.winner == 0;
		profile_record_game(GAME_OCA, won);
		audio_play(won ? SFX_WIN : SFX_LOSE);
		for (int p = 0; p < s_players; p++)
			mood_set(&s_mood[p], p == s_game.winner ? EXPR_HAPPY : EXPR_SAD, 5.0f);
	}
	return SCENE_OCA;
}

static u32 square_color(OcaSquare s)
{
	switch (s) {
	case OCA_GOOSE:
		return C2D_Color32(150, 210, 240, 255);
	case OCA_BRIDGE:
		return C2D_Color32(200, 160, 110, 255);
	case OCA_DICE:
		return C2D_Color32(240, 200, 230, 255);
	case OCA_INN:
		return C2D_Color32(250, 190, 120, 255);
	case OCA_WELL:
		return C2D_Color32(120, 140, 170, 255);
	case OCA_MAZE:
		return C2D_Color32(170, 220, 140, 255);
	case OCA_JAIL:
		return C2D_Color32(170, 170, 170, 255);
	case OCA_DEATH:
		return C2D_Color32(60, 50, 60, 255);
	case OCA_GARDEN:
		return C2D_Color32(250, 210, 70, 255);
	default:
		return C2D_Color32(250, 243, 225, 255);
	}
}

static void draw_goose(float cx, float cy, float depth)
{
	u32 white = C2D_Color32(255, 255, 255, 255), orange = C2D_Color32(240, 140, 30, 255);
	float dx = gfx_dx(depth);
	gfx_ellipse(cx + dx, cy + 3, 7, 4.5f, depth, white);
	gfx_ellipse(cx + 5 + dx, cy - 3, 2, 4, depth, white);
	gfx_ellipse(cx + 6 + dx, cy - 6, 2.6f, 2.4f, depth, white);
	gfx_ellipse(cx + 9 + dx, cy - 5.5f, 2, 1, depth, orange);
}

static void oca_draw_top(void)
{
	gfx_background(BG_TABLE_TOP);
	gfx_rrect(X0 - 4 + gfx_dx(DEPTH_TABLE), Y0 - 4, COLS * CELL_W + 8, ROWS * CELL_H + 8, 6, DEPTH_TABLE,
		C2D_Color32(110, 70, 30, 255));
	for (int n = 1; n <= OCA_GOAL; n++) {
		OcaSquare sq = oca_square(n);
		float x = X0 + s_cell_col[n] * CELL_W, y = Y0 + s_cell_row[n] * CELL_H;
		gfx_rect(x + 1 + gfx_dx(DEPTH_TABLE), y + 1, CELL_W - 2, CELL_H - 2, DEPTH_TABLE, square_color(sq));
		u32 ink = sq == OCA_DEATH ? COLOR_CREAM : C2D_Color32(90, 50, 30, 255);
		gfx_textf(x + 3, y + 1, DEPTH_TABLE, 0.42f, ink, C2D_AlignLeft, "%d", n);
		if (sq == OCA_GOOSE)
			draw_goose(x + CELL_W - 13, y + CELL_H - 11, DEPTH_TABLE);
		else if (square_label(sq) && n != 1)
			gfx_text(square_label(sq), x + CELL_W / 2, y + CELL_H - 15, DEPTH_TABLE, 0.42f, ink, C2D_AlignCenter);
	}
	for (int p = 0; p < s_players; p++) {
		float x = s_disp_x[p], y = s_disp_y[p];
		float lift = (s_anim == ANIM_WALK || s_anim == ANIM_JUMP) && p == s_mover ? 3.0f : 0.0f;
		gfx_ellipse(x + gfx_dx(DEPTH_CARDS) + 1, y + 2, 7, 7, DEPTH_CARDS, C2D_Color32(0, 0, 0, 90));
		gfx_ellipse(x + gfx_dx(DEPTH_CARDS + lift), y - lift, 7, 7, DEPTH_CARDS + lift, gfx_rgb(TOKEN[p], 255));
		gfx_ellipse(x - 2 + gfx_dx(DEPTH_CARDS + lift), y - 2 - lift, 2.5f, 2, DEPTH_CARDS + lift,
			C2D_Color32(255, 255, 255, 120));
	}
	if (s_game.winner >= 0 && s_anim == ANIM_NONE && banner_draw(s_end_time, 200, 120, 320, 60)) {
		if (s_game.winner == 0)
			gfx_text("¡Has llegado al jardín de la oca!", 200, 106, DEPTH_BANNER, 0.58f, COLOR_GOLD, C2D_AlignCenter);
		else
			gfx_textf(200, 106, DEPTH_BANNER, 0.58f, COLOR_CREAM, C2D_AlignCenter, "Gana %s", name(s_game.winner));
	}
}

static void draw_die(float cx, float cy, int face)
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
	float s = 28;
	gfx_rrect(cx - s + 3, cy - s + 4, 2 * s, 2 * s, 10, 0, C2D_Color32(0, 0, 0, 90));
	gfx_rrect(cx - s, cy - s, 2 * s, 2 * s, 10, 0, C2D_Color32(250, 250, 245, 255));
	for (int i = 0; i < face; i++)
		gfx_ellipse(cx + pip[face][i][0] * 15, cy + pip[face][i][1] * 15, 5, 5, 0, C2D_Color32(180, 30, 40, 255));
}

static void oca_draw_bottom(void)
{
	gfx_background(BG_TABLE_BOTTOM);
	gfx_text_wrap(s_message, 10, 4, 0, 0.42f, COLOR_GOLD, 240);
	button_draw(&s_btn_menu, false);
	/* Jugadores */
	for (int p = 0; p < s_players; p++) {
		float y = 46 + p * 38;
		bool turn = s_game.current == p && s_game.winner < 0;
		gfx_panel(8, y - 16, 168, 34, 0, C2D_Color32(10, 50, 25, 200), turn ? COLOR_GOLD : C2D_Color32(20, 60, 30, 255));
		mii_draw(avatar(p), 26, y + 1, 12, 0, mood_expr(&s_mood[p]), false);
		gfx_ellipse(48, y + 1, 6, 6, 0, gfx_rgb(TOKEN[p], 255));
		gfx_text(name(p), 60, y - 12, 0, 0.45f, COLOR_CREAM, C2D_AlignLeft);
		const char* st = s_game.in_well[p] ? "en el pozo" : s_game.skip[p] ? "sin tirar" : "";
		gfx_textf(60, y + 2, 0, 0.42f, C2D_Color32(200, 230, 200, 255), C2D_AlignLeft, "Casilla %d %s", s_shown[p], st);
	}
	draw_die(250, 100, s_face);
	if (s_game.winner < 0 && s_anim == ANIM_NONE)
		gfx_text(s_game.current == 0 ? "¡Te toca!" : "", 250, 140, 0, 0.5f, COLOR_GOLD, C2D_AlignCenter);
	button_draw(&s_btn_players, s_anim == ANIM_NONE);
	button_draw(&s_btn_roll, s_btn_roll.enabled);
}

/* Salir a medias desde la pausa: la próxima vez se empieza de cero */
static void scene_leave(void)
{
	s_started = false;
}

const Scene scene_oca = { oca_enter, oca_update, oca_draw_top, oca_draw_bottom, scene_leave };
