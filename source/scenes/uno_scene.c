/* Uno: tú contra tres personajes. El montón y los rivales arriba; tu mano en
 * la pantalla táctil. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "games/uno.h"
#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/gfx.h"
#include "ui/mii.h"
#include "ui/widgets.h"

/* Asientos en el sentido de las agujas del reloj: tú abajo, izquierda, arriba, derecha */
static const CpuId SEAT_CPU[UNO_PLAYERS] = { CPU_COUNT, CPU_PEPE, CPU_LOLA, CPU_CARMEN };
static const float SEAT_X[UNO_PLAYERS] = { 200, 44, 200, 356 };
static const float SEAT_Y[UNO_PLAYERS] = { 290, 92, 30, 92 };
static const char* const COLOR_NAME[UNO_COLORS] = { "rojo", "amarillo", "verde", "azul" };
static const u32 COLOR_RGB[UNO_COLORS] = { 0xDC282D, 0xF5C41E, 0x3CA846, 0x2864C8 };

#define PILE_X     214.0f
#define PILE_Y     122.0f
#define DECK_X     150.0f
#define HAND_Y     110.0f
#define HAND_SCALE 0.72f

typedef struct {
	bool active;
	int sprite;
	float x, y, tx, ty, t;
	bool top_screen;
} Flyer;

static Uno s_game;
static bool s_started;
static Mood s_mood[UNO_PLAYERS];
static int s_selected;
static float s_cpu_timer, s_catch_timer, s_result_time;
static UnoPhase s_prev_phase;
static Flyer s_fly;
static char s_message[96];
static float s_message_flash;
static char s_bubble[UNO_PLAYERS][24];
static float s_bubble_time[UNO_PLAYERS];
static Button s_btn_play, s_btn_draw, s_btn_uno, s_btn_menu, s_btn_color[UNO_COLORS];

static const char* seat_name(int p)
{
	return p == 0 ? profile_get()->avatar.name : cpu_name(SEAT_CPU[p]);
}

static const Avatar* seat_avatar(int p)
{
	return p == 0 ? &profile_get()->avatar : cpu_avatar(SEAT_CPU[p]);
}

static int sprite_of(UnoCard c)
{
	if (c.value == UNO_WILDCARD)
		return BOARD_UNO_WILD;
	if (c.value == UNO_DRAW4)
		return BOARD_UNO_DRAW4;
	return BOARD_UNO_FIRST + c.color * 13 + c.value;
}

static void set_message(const char* msg)
{
	snprintf(s_message, sizeof s_message, "%s", msg);
	s_message_flash = 1.0f;
}

static void bubble(int p, const char* text)
{
	snprintf(s_bubble[p], sizeof s_bubble[p], "%s", text);
	s_bubble_time[p] = 1.8f;
}

static void reset_round(void)
{
	s_selected = 0;
	s_cpu_timer = s_catch_timer = s_result_time = 0.0f;
	s_prev_phase = s_game.phase;
	memset(&s_fly, 0, sizeof s_fly);
	memset(s_bubble_time, 0, sizeof s_bubble_time);
	char buf[96];
	snprintf(buf, sizeof buf, "Empieza %s", s_game.current == 0 ? "tú" : seat_name(s_game.current));
	set_message(buf);
}

static void uno_scene_enter(void)
{
	if (!s_started) {
		uno_init(&s_game, (uint32_t)time(NULL) ^ 0x0D0);
		s_started = true;
		reset_round();
	}
	button_set(&s_btn_menu, 258, 6, 54, 20, "Menú", C2D_Color32(120, 30, 35, 255), KEY_START);
	static const char* const labels[UNO_COLORS] = { "Rojo", "Amarillo", "Verde", "Azul" };
	for (int c = 0; c < UNO_COLORS; c++)
		button_set(&s_btn_color[c], 20 + (c % 2) * 144, 70 + (c / 2) * 56, 136, 48, labels[c],
			gfx_rgb(COLOR_RGB[c], 255), 0);
}

static void fly(int sprite, float x, float y, bool top)
{
	s_fly = (Flyer){ true, sprite, x, y, PILE_X, PILE_Y, 0.0f, top };
}

/* Mensaje y caras después de cada jugada */
static void after_play(int p)
{
	char buf[96];
	if (s_game.phase == UNO_PHASE_ROUND_OVER || s_game.phase == UNO_PHASE_GAME_OVER)
		return;
	UnoCard c = s_game.last_card;
	if (s_game.last_penalty_player >= 0 && s_game.last_penalty > 0) {
		int v = s_game.last_penalty_player;
		snprintf(buf, sizeof buf, "%s roba %d", v == 0 ? "Tú" : seat_name(v), s_game.last_penalty);
		set_message(buf);
		mood_set(&s_mood[v], EXPR_SAD, 1.5f);
		mood_set(&s_mood[p], EXPR_HAPPY, 1.5f);
	} else if (c.value == UNO_SKIP) {
		set_message("¡Salta!");
	} else if (c.value == UNO_REVERSE) {
		set_message("¡Cambio de sentido!");
	} else if (uno_is_wild(c)) {
		snprintf(buf, sizeof buf, "Color: %s", COLOR_NAME[s_game.color]);
		set_message(buf);
	}
	if (s_game.hand_count[p] == 1 && s_game.said_uno[p])
		bubble(p, "¡Uno!");
	s_catch_timer = 0.0f;
}

static void human_play(int idx)
{
	if (!uno_can_play(&s_game, 0, idx)) {
		audio_play(SFX_ERROR);
		set_message(s_game.drew ? "Solo puedes echar la carta que has robado" : "Esa carta no vale");
		return;
	}
	UnoCard c = s_game.hands[0][idx];
	uno_play(&s_game, idx);
	fly(sprite_of(c), 200, 250, true);
	audio_play(SFX_CARD);
	after_play(0);
	if (s_selected >= s_game.hand_count[0])
		s_selected = s_game.hand_count[0] - 1;
	if (s_selected < 0)
		s_selected = 0;
}

static void cpu_turn(int p)
{
	if (s_game.phase == UNO_PHASE_CHOOSE_COLOR) {
		uno_choose_color(&s_game, uno_ai_color(&s_game, p));
		after_play(p);
		return;
	}
	/* Casi siempre se acuerdan de decir "¡Uno!" */
	if (s_game.hand_count[p] == 2 && rng_range(&s_game.rng, 10) < 9)
		uno_say_uno(&s_game, p);
	int i = uno_ai_choose(&s_game, p);
	if (i >= 0) {
		UnoCard c = s_game.hands[p][i];
		uno_play(&s_game, i);
		fly(sprite_of(c), SEAT_X[p], SEAT_Y[p], true);
		audio_play(SFX_CARD);
		after_play(p);
	} else if (!s_game.drew) {
		uno_draw(&s_game);
		audio_play(SFX_FLIP);
	} else {
		uno_pass(&s_game);
	}
}

static int touched_card(const Input* in)
{
	if (!in->touch_down)
		return -1;
	int n = s_game.hand_count[0];
	float step = n > 1 ? fminf(46.0f, (300.0f - 64 * HAND_SCALE) / (n - 1)) : 0.0f;
	float w = 64 * HAND_SCALE / 2.0f, h = 96 * HAND_SCALE / 2.0f;
	for (int i = n - 1; i >= 0; i--) {
		float x = BOTTOM_W / 2.0f + (i - (n - 1) / 2.0f) * step;
		if (in->tx >= x - w && in->tx <= x + w && in->ty >= HAND_Y - h - 12 && in->ty <= HAND_Y + h)
			return i;
	}
	return -1;
}

/* Algún rival se ha quedado con una carta sin decir "¡Uno!" */
static int catchable_cpu(void)
{
	for (int p = 1; p < UNO_PLAYERS; p++)
		if (uno_catchable(&s_game, p))
			return p;
	return -1;
}

static SceneId uno_scene_update(const Input* in, float dt)
{
	if (button_update(&s_btn_menu, in))
		return SCENE_MENU;
	const float y = 205, h = 28;
	bool flying = s_fly.active;

	if (s_game.phase == UNO_PHASE_PLAYING || s_game.phase == UNO_PHASE_CHOOSE_COLOR) {
		bool mine = s_game.current == 0;
		button_set(&s_btn_play, 8, y, 96, h, GLYPH_A " Echar", C2D_Color32(40, 140, 60, 255), KEY_A);
		button_set(&s_btn_draw, 112, y, 96, h, s_game.drew && mine ? GLYPH_X " Pasar" : GLYPH_X " Robar",
			C2D_Color32(60, 90, 160, 255), KEY_X);
		int victim = catchable_cpu();
		button_set(&s_btn_uno, 216, y, 96, h, victim >= 0 ? GLYPH_Y " ¡Pillado!" : GLYPH_Y " ¡Uno!",
			C2D_Color32(220, 60, 50, 255), KEY_Y);
		bool my_uno = s_game.hand_count[0] <= 2 && !s_game.said_uno[0] &&
		              (s_game.hand_count[0] == 1 || mine);
		s_btn_uno.enabled = !flying && (victim >= 0 || my_uno);
		s_btn_play.enabled = s_btn_draw.enabled = mine && !flying && s_game.phase == UNO_PHASE_PLAYING;

		if (button_update(&s_btn_uno, in)) {
			if (victim >= 0 && uno_catch(&s_game, victim)) {
				char buf[64];
				snprintf(buf, sizeof buf, "¡Pillas a %s! Roba dos", seat_name(victim));
				set_message(buf);
				mood_set(&s_mood[victim], EXPR_SURPRISED, 1.5f);
			} else if (uno_say_uno(&s_game, 0)) {
				bubble(0, "¡Uno!");
			}
		}

		if (mine && s_game.phase == UNO_PHASE_CHOOSE_COLOR) {
			for (int c = 0; c < UNO_COLORS; c++)
				if (button_update(&s_btn_color[c], in)) {
					uno_choose_color(&s_game, c);
					after_play(0);
				}
		} else if (mine && !flying) {
			int n = s_game.hand_count[0];
			if (n > 0) {
				if (in->repeat & KEY_LEFT) {
					s_selected = (s_selected + n - 1) % n;
					audio_play(SFX_SELECT);
				}
				if (in->repeat & KEY_RIGHT) {
					s_selected = (s_selected + 1) % n;
					audio_play(SFX_SELECT);
				}
				if (s_game.drew && s_game.drawn_index >= 0)
					s_selected = s_game.drawn_index;
			}
			int t = touched_card(in);
			if (t >= 0) {
				s_selected = t;
				human_play(t);
			} else if (button_update(&s_btn_play, in)) {
				human_play(s_selected);
			} else if (button_update(&s_btn_draw, in)) {
				if (s_game.drew) {
					uno_pass(&s_game);
				} else {
					uno_draw(&s_game);
					audio_play(SFX_FLIP);
					if (s_game.current == 0)
						set_message("Puedes echar la carta robada o pasar");
					else
						set_message("La carta robada no vale: pasas");
				}
			}
		} else if (!mine) {
			s_cpu_timer += dt;
			if (!flying && s_cpu_timer > 0.8f) {
				s_cpu_timer = 0.0f;
				cpu_turn(s_game.current);
			}
		}

		/* Si te quedas con una carta sin decir "¡Uno!", te pillan */
		if (uno_catchable(&s_game, 0)) {
			s_catch_timer += dt;
			if (s_catch_timer > 2.2f) {
				uno_catch(&s_game, 0);
				set_message("¡Te han pillado sin decir uno! Robas dos");
				mood_set(&s_mood[0], EXPR_SAD, 1.5f);
			}
		}
	} else {
		bool over = s_game.phase == UNO_PHASE_GAME_OVER;
		button_set(&s_btn_play, 12, y, 144, h, over ? GLYPH_A " Otra partida" : GLYPH_A " Otra ronda",
			C2D_Color32(40, 140, 60, 255), KEY_A);
		button_set(&s_btn_draw, 164, y, 144, h, GLYPH_B " Menú", C2D_Color32(190, 60, 50, 255), KEY_B);
		s_btn_play.enabled = s_btn_draw.enabled = s_result_time > 0.6f;
		if (button_update(&s_btn_play, in)) {
			if (over)
				uno_new_game(&s_game);
			else
				uno_new_round(&s_game);
			reset_round();
		} else if (button_update(&s_btn_draw, in)) {
			return SCENE_MENU;
		}
	}

	if (s_game.phase != s_prev_phase) {
		s_prev_phase = s_game.phase;
		if (s_game.phase == UNO_PHASE_ROUND_OVER || s_game.phase == UNO_PHASE_GAME_OVER) {
			s_result_time = 0.0f;
			int w = s_game.round_winner;
			for (int p = 0; p < UNO_PLAYERS; p++)
				mood_set(&s_mood[p], p == w ? EXPR_HAPPY : EXPR_SAD, 4.0f);
			audio_play(w == 0 ? SFX_WIN : SFX_LOSE);
			if (s_game.phase == UNO_PHASE_GAME_OVER)
				profile_record_game(GAME_UNO, uno_game_winner(&s_game) == 0);
		}
	}
	if (s_game.phase >= UNO_PHASE_ROUND_OVER)
		s_result_time += dt;

	if (s_fly.active) {
		s_fly.t += dt * 3.5f;
		if (s_fly.t >= 1.0f)
			s_fly.active = false;
	}
	for (int p = 0; p < UNO_PLAYERS; p++) {
		mood_update(&s_mood[p], dt);
		if (s_bubble_time[p] > 0.0f)
			s_bubble_time[p] -= dt;
	}
	if (s_message_flash > 0.0f)
		s_message_flash -= dt * 2.0f;
	return SCENE_UNO;
}

/* ---------------------------------------------------------------------------
 * Dibujo
 * ------------------------------------------------------------------------- */

static void draw_seat(int p)
{
	float x = SEAT_X[p], y = SEAT_Y[p];
	int n = s_game.hand_count[p];
	int shown = n > 10 ? 10 : n;
	for (int i = 0; i < shown; i++) {
		float off = i - (shown - 1) / 2.0f;
		if (p == 2)
			gfx_board(BOARD_UNO_BACK, x + 50 + off * 9.0f, y, 0.4f, off * 0.05f, DEPTH_CARDS + i * 0.02f, NULL);
		else
			gfx_board(BOARD_UNO_BACK, x + off * 3.0f, y + 48 + off * 3.0f, 0.4f,
				(p == 1 ? 1.0f : -1.0f) * (0.3f + off * 0.04f), DEPTH_CARDS + i * 0.02f, NULL);
	}
	float ax = p == 2 ? x - 40 : x;
	mii_draw(seat_avatar(p), ax, y, 17, DEPTH_UI + 0.5f, mood_expr(&s_mood[p]), false);
	bool turn = s_game.current == p && s_game.phase <= UNO_PHASE_CHOOSE_COLOR;
	gfx_panel(ax - 36, y + 20, 72, 18, DEPTH_UI, C2D_Color32(10, 50, 25, 220),
		turn ? COLOR_GOLD : C2D_Color32(190, 60, 50, 255));
	gfx_textf(ax, y + 22, DEPTH_UI, 0.42f, COLOR_CREAM, C2D_AlignCenter, "%s: %d", seat_name(p), n);
}

static void draw_bubble(int p)
{
	if (s_bubble_time[p] <= 0.0f)
		return;
	float bx = p == 0 ? 200 : p == 2 ? 290 : SEAT_X[p] + (p == 1 ? 40 : -40), by = p == 0 ? 196 : SEAT_Y[p] - 4;
	float pop = ease_out_back(fminf(1.0f, (1.8f - s_bubble_time[p]) / 0.2f));
	float w = gfx_text_width(s_bubble[p], 0.5f) + 16;
	gfx_rrect(bx - w / 2 * pop, by - 11 * pop, w * pop, 22 * pop, 8, DEPTH_BANNER, C2D_Color32(255, 250, 235, 240));
	if (pop > 0.8f)
		gfx_text(s_bubble[p], bx, by - 8, DEPTH_BANNER, 0.5f, C2D_Color32(120, 20, 20, 255), C2D_AlignCenter);
}

static void uno_scene_draw_top(void)
{
	gfx_background(BG_TABLE_TOP);

	/* Mazo y montón, con un aro del color que vale */
	for (int i = 0; i < 4; i++)
		gfx_board(BOARD_UNO_BACK, DECK_X - i * 0.6f, PILE_Y - i * 0.8f, 0.85f, 0.0f, DEPTH_CARDS + i * 0.05f, NULL);
	gfx_textf(DECK_X, PILE_Y + 46, DEPTH_UI, 0.4f, COLOR_CREAM, C2D_AlignCenter, "%d", s_game.deck_count);
	float pulse = 0.6f + 0.4f * sinf(gfx_time() * 4.0f);
	gfx_rrect(PILE_X - 34 + gfx_dx(DEPTH_CARDS), PILE_Y - 48, 68, 96, 9, DEPTH_CARDS,
		gfx_rgb(COLOR_RGB[s_game.color], (u8)(200 * pulse)));
	if (s_game.discard_count > 1)
		gfx_board(sprite_of(s_game.discard[s_game.discard_count - 2]), PILE_X - 3, PILE_Y + 2, 0.85f, -0.12f,
			DEPTH_CARDS, NULL);
	if (!s_fly.active || !s_fly.top_screen)
		gfx_board(sprite_of(uno_top(&s_game)), PILE_X, PILE_Y, 0.85f, 0.05f, DEPTH_CARDS + 0.3f, NULL);
	else {
		if (s_game.discard_count > 1)
			gfx_board(sprite_of(s_game.discard[s_game.discard_count - 2]), PILE_X, PILE_Y, 0.85f, 0.05f,
				DEPTH_CARDS + 0.3f, NULL);
		float t = s_fly.t;
		float e = 1.0f - (1.0f - t) * (1.0f - t);
		gfx_board(s_fly.sprite, s_fly.x + (s_fly.tx - s_fly.x) * e, s_fly.y + (s_fly.ty - s_fly.y) * e,
			0.6f + 0.25f * e, 0.05f, DEPTH_LIFTED, NULL);
	}

	/* Sentido de juego y color */
	gfx_panel(262, 186, 132, 46, DEPTH_UI, C2D_Color32(10, 50, 25, 210), C2D_Color32(20, 60, 30, 255));
	gfx_text(s_game.direction > 0 ? "Sentido: horario" : "Sentido: al revés", 328, 190, DEPTH_UI, 0.45f, COLOR_CREAM,
		C2D_AlignCenter);
	gfx_textf(328, 210, DEPTH_UI, 0.5f, gfx_rgb(COLOR_RGB[s_game.color], 255), C2D_AlignCenter, "Color: %s",
		COLOR_NAME[s_game.color]);

	for (int p = 1; p < UNO_PLAYERS; p++)
		draw_seat(p);
	mii_draw(seat_avatar(0), 30, 212, 15, DEPTH_UI + 0.5f, mood_expr(&s_mood[0]), false);
	gfx_textf(52, 204, DEPTH_UI, 0.45f, COLOR_CREAM, C2D_AlignLeft, "Tus puntos: %d", s_game.score[0]);
	for (int p = 0; p < UNO_PLAYERS; p++)
		draw_bubble(p);

	if (s_game.phase >= UNO_PHASE_ROUND_OVER && banner_draw(s_result_time, 200, 112, 340, 96)) {
		bool over = s_game.phase == UNO_PHASE_GAME_OVER;
		int w = over ? uno_game_winner(&s_game) : s_game.round_winner;
		char title[64];
		if (w == 0)
			snprintf(title, sizeof title, over ? "¡Ganas la partida!" : "¡Ganas la ronda!");
		else
			snprintf(title, sizeof title, over ? "Gana la partida %s" : "Gana la ronda %s", seat_name(w));
		gfx_text(title, 200, 72, DEPTH_BANNER, 0.65f, w == 0 ? COLOR_GOLD : COLOR_RED, C2D_AlignCenter);
		gfx_textf(200, 98, DEPTH_BANNER, 0.5f, COLOR_CREAM, C2D_AlignCenter, "%s se lleva %d puntos",
			s_game.round_winner == 0 ? "Te llevas" : seat_name(s_game.round_winner), s_game.round_points);
		gfx_textf(200, 122, DEPTH_BANNER, 0.45f, COLOR_GOLD, C2D_AlignCenter, "Tú %d · %s %d · %s %d · %s %d (a %d)",
			s_game.score[0], seat_name(1), s_game.score[1], seat_name(2), s_game.score[2], seat_name(3),
			s_game.score[3], UNO_GAME_POINTS);
	}
}

static void uno_scene_draw_bottom(void)
{
	gfx_background(BG_TABLE_BOTTOM);
	gfx_text(s_message, 10, 10, 0, 0.45f, s_message_flash > 0.0f ? COLOR_GOLD : COLOR_CREAM, C2D_AlignLeft);
	button_draw(&s_btn_menu, false);

	bool mine = s_game.current == 0 && s_game.phase <= UNO_PHASE_CHOOSE_COLOR;
	if (mine && s_game.phase == UNO_PHASE_CHOOSE_COLOR) {
		gfx_text("Elige el color", BOTTOM_W / 2, 40, 0, 0.6f, COLOR_GOLD, C2D_AlignCenter);
		for (int c = 0; c < UNO_COLORS; c++)
			button_draw(&s_btn_color[c], true);
		return;
	}
	if (mine)
		gfx_text("¡Tu turno!", BOTTOM_W / 2, 30, 0, 0.55f, COLOR_GOLD, C2D_AlignCenter);

	int n = s_game.hand_count[0];
	float step = n > 1 ? fminf(46.0f, (300.0f - 64 * HAND_SCALE) / (n - 1)) : 0.0f;
	for (int i = 0; i < n; i++) {
		float x = BOTTOM_W / 2.0f + (i - (n - 1) / 2.0f) * step;
		bool sel = mine && i == s_selected;
		bool ok = !mine || uno_can_play(&s_game, 0, i);
		C2D_ImageTint tint;
		C2D_PlainImageTint(&tint, C2D_Color32(20, 20, 30, 255), 0.45f);
		gfx_board(sprite_of(s_game.hands[0][i]), x, HAND_Y - (sel ? 12.0f : 0.0f), HAND_SCALE, 0.0f, 0,
			ok ? NULL : &tint);
	}
	if (s_game.said_uno[0] && n == 1)
		gfx_text("Has dicho ¡Uno!", BOTTOM_W / 2, 168, 0, 0.5f, COLOR_GOLD, C2D_AlignCenter);

	button_draw(&s_btn_play, s_btn_play.enabled);
	button_draw(&s_btn_draw, s_btn_draw.enabled);
	if (s_game.phase <= UNO_PHASE_CHOOSE_COLOR)
		button_draw(&s_btn_uno, s_btn_uno.enabled);
}

/* Salir a medias desde la pausa: la próxima vez se empieza de cero */
static void scene_leave(void)
{
	s_started = false;
}

const Scene scene_uno = { uno_scene_enter, uno_scene_update, uno_scene_draw_top, uno_scene_draw_bottom, scene_leave };
