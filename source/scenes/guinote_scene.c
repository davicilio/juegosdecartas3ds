/* Mesa del guiñote: tú y tu compañero (arriba) contra los rivales de los lados. */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "games/guinote.h"
#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/flyers.h"
#include "ui/gfx.h"
#include "ui/mii.h"
#include "ui/widgets.h"

/* Asientos: 0 tú (abajo), 1 derecha, 2 arriba (compañero), 3 izquierda */
static const CpuId SEAT_CPU[GN_PLAYERS] = { CPU_COUNT, CPU_LUCIA, CPU_PEPE, CPU_CARMEN };
static const float SEAT_X[GN_PLAYERS] = { 200, 360, 200, 40 };
static const float SEAT_Y[GN_PLAYERS] = { 290, 78, 30, 78 };
static const float TRICK_X[GN_PLAYERS] = { 200, 244, 200, 156 };
static const float TRICK_Y[GN_PLAYERS] = { 152, 116, 82, 116 };

#define TRICK_SCALE 0.6f
#define HAND_SCALE  0.78f
#define HAND_Y      108.0f
#define HAND_STEP   50.0f
#define DECK_X      300.0f
#define DECK_Y      202.0f

static Guinote s_game;
static bool s_started;
static Mood s_mood[GN_PLAYERS];
static VisCard s_hand[DECK_SIZE]; /* tus cartas, por card_index */
static VisCard s_trick[GN_PLAYERS];
static int s_selected;
static float s_cpu_timer, s_trick_timer, s_result_time;
static GnPhase s_prev_phase;
static char s_message[96];
static float s_message_flash;
static char s_bubble[GN_PLAYERS][24];
static float s_bubble_time[GN_PLAYERS];
static Button s_btn_play, s_btn_sing, s_btn_swap, s_btn_menu;

/* Puntos de la partida, contando los de la mano en curso */
static int total_points(int team)
{
	bool counted = s_game.phase == GN_PHASE_HAND_OVER || s_game.phase == GN_PHASE_GAME_OVER;
	return s_game.score[team] + (counted ? 0 : s_game.points[team]);
}

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

static void bubble(int p, const char* text)
{
	snprintf(s_bubble[p], sizeof s_bubble[p], "%s", text);
	s_bubble_time[p] = 2.0f;
}

static void reset_visuals(void)
{
	memset(s_hand, 0, sizeof s_hand);
	memset(s_trick, 0, sizeof s_trick);
	memset(s_bubble_time, 0, sizeof s_bubble_time);
	flyers_clear();
	s_selected = 0;
	s_cpu_timer = s_trick_timer = s_result_time = 0.0f;
	s_prev_phase = s_game.phase;
	char buf[96];
	if (s_game.current == 0)
		snprintf(buf, sizeof buf, "Triunfo: %s. Sales tú", suit_name((Suit)s_game.trump));
	else
		snprintf(buf, sizeof buf, "Triunfo: %s. Sale %s", suit_name((Suit)s_game.trump), seat_name(s_game.current));
	set_message(buf);
}

static void gn_scene_enter(void)
{
	if (!s_started) {
		gn_init(&s_game, (uint32_t)time(NULL) ^ 0x6017E);
		s_started = true;
		reset_visuals();
	}
	button_set(&s_btn_menu, 258, 6, 54, 20, "Menú", C2D_Color32(120, 30, 35, 255), KEY_START);
}

static bool busy(void)
{
	if (flyers_busy())
		return true;
	for (int p = 0; p < GN_PLAYERS; p++)
		if (vis_moving(&s_trick[p]))
			return true;
	for (int i = 0; i < DECK_SIZE; i++)
		if (s_hand[i].active && s_hand[i].delay > 0.0f)
			return true;
	return false;
}

static void sync_hand(void)
{
	const int n = s_game.hand_count[0];
	bool present[DECK_SIZE] = { false };
	bool my_turn = s_game.phase == GN_PHASE_PLAYING && s_game.current == 0;
	float delay = 0.0f;
	for (int i = 0; i < n; i++) {
		Card c = s_game.hands[0][i];
		int id = card_index(c);
		VisCard* v = &s_hand[id];
		present[id] = true;
		if (!v->active) {
			vis_spawn(v, c, BOTTOM_W / 2.0f, -60.0f, HAND_SCALE, false, delay);
			delay += 0.12f;
		}
		bool selected = my_turn && i == s_selected;
		vis_target(v, BOTTOM_W / 2.0f + (i - (n - 1) / 2.0f) * HAND_STEP, HAND_Y - (selected ? 12.0f : 0.0f),
			HAND_SCALE, true);
		/* En el arrastre se oscurecen las cartas que no se pueden jugar */
		v->dim = my_turn && !gn_can_play(&s_game, 0, i) ? 0.45f : 0.0f;
	}
	for (int id = 0; id < DECK_SIZE; id++)
		if (s_hand[id].active && !present[id])
			s_hand[id].active = false;
}

static void after_play(int p)
{
	VisCard* v = &s_trick[p];
	float fx = p == 0 ? 200.0f : SEAT_X[p], fy = p == 0 ? 260.0f : SEAT_Y[p];
	vis_spawn(v, s_game.trick[p], fx, fy, 0.5f, p == 0, 0.0f);
	vis_target(v, TRICK_X[p], TRICK_Y[p], TRICK_SCALE, true);
	v->tangle = (p - 1.5f) * 0.06f;
	v->depth = DEPTH_CARDS + s_game.played_in_trick * 0.1f;
	if (s_game.phase == GN_PHASE_TRICK_DONE) {
		char buf[96];
		int w = s_game.trick_winner;
		if (w == 0)
			snprintf(buf, sizeof buf, "Te llevas la baza (+%d)", s_game.trick_points);
		else
			snprintf(buf, sizeof buf, "%s se lleva la baza (+%d)", seat_name(w), s_game.trick_points);
		set_message(buf);
		s_trick_timer = 0.0f;
	}
}

static void announce_cante(void)
{
	int p = s_game.last_cante_player;
	char buf[96];
	bubble(p, s_game.last_cante_points == 40 ? "¡Las cuarenta!" : "¡Las veinte!");
	snprintf(buf, sizeof buf, "%s canta %s en %s", p == 0 ? "Tú" : seat_name(p),
		s_game.last_cante_points == 40 ? "las cuarenta" : "las veinte", suit_name((Suit)s_game.last_cante_suit));
	set_message(buf);
	audio_play(SFX_WIN);
	mood_set(&s_mood[p], EXPR_HAPPY, 1.8f);
	for (int q = 0; q < GN_PLAYERS; q++)
		if (gn_team(q) != gn_team(p))
			mood_set(&s_mood[q], EXPR_SURPRISED, 1.4f);
}

static void collect(void)
{
	int w = s_game.trick_winner;
	if (s_game.trick_points >= 15)
		for (int p = 0; p < GN_PLAYERS; p++)
			mood_set(&s_mood[p], gn_team(p) == gn_team(w) ? EXPR_HAPPY : EXPR_SAD, 1.4f);
	gn_collect(&s_game);
	for (int p = 0; p < GN_PLAYERS; p++) {
		flyers_add(&s_trick[p], SEAT_X[w], w == 0 ? 260.0f : SEAT_Y[w], p * 0.06f, false, FLY_TOP);
		s_trick[p].active = false;
	}
	audio_play(SFX_CARD);
	if (s_game.phase == GN_PHASE_PLAYING && gn_arrastre(&s_game) && s_game.hand_count[0] == GN_HAND_SIZE)
		set_message("¡Arrastre! Asiste y mata si puedes");
	s_selected = 0;
}

static void human_play(int idx)
{
	if (!gn_can_play(&s_game, 0, idx)) {
		audio_play(SFX_ERROR);
		set_message("En el arrastre hay que asistir, montar o fallar");
		return;
	}
	gn_play(&s_game, idx);
	after_play(0);
	if (s_selected >= s_game.hand_count[0])
		s_selected = s_game.hand_count[0] - 1;
	if (s_selected < 0)
		s_selected = 0;
}

static int touched_card(const Input* in)
{
	if (!in->touch_down)
		return -1;
	for (int i = s_game.hand_count[0] - 1; i >= 0; i--) {
		const VisCard* v = &s_hand[card_index(s_game.hands[0][i])];
		float w = CARD_W * HAND_SCALE / 2.0f, h = CARD_H * HAND_SCALE / 2.0f;
		if (in->tx >= v->x - w && in->tx <= v->x + w && in->ty >= v->y - h && in->ty <= v->y + h)
			return i;
	}
	return -1;
}

static void cpu_turn(int p)
{
	if (gn_swap_index(&s_game, p) >= 0) {
		gn_swap(&s_game, p);
		audio_play(SFX_FLIP);
		bubble(p, "Cambio el 7");
		return;
	}
	if (gn_cante(&s_game, p)) {
		announce_cante();
		return;
	}
	int i = gn_ai_choose(&s_game, p);
	if (i >= 0) {
		gn_play(&s_game, i);
		after_play(p);
	}
}

static SceneId gn_scene_update(const Input* in, float dt)
{
	if (button_update(&s_btn_menu, in))
		return SCENE_MENU;
	bool is_busy = busy();
	const float y = 205, h = 28;

	switch (s_game.phase) {
	case GN_PHASE_PLAYING:
		button_set(&s_btn_play, 8, y, 96, h, GLYPH_A " Jugar", C2D_Color32(40, 140, 60, 255), KEY_A);
		button_set(&s_btn_sing, 112, y, 96, h, GLYPH_X " Cantar", C2D_Color32(190, 60, 120, 255), KEY_X);
		button_set(&s_btn_swap, 216, y, 96, h, GLYPH_Y " Cambiar 7", C2D_Color32(200, 140, 30, 255), KEY_Y);
		if (s_game.current == 0) {
			s_btn_play.enabled = !is_busy;
			s_btn_sing.enabled = !is_busy && gn_cante_suit(&s_game, 0) >= 0;
			s_btn_swap.enabled = !is_busy && gn_swap_index(&s_game, 0) >= 0;
			if (is_busy)
				break;
			int n = s_game.hand_count[0];
			if (n == 0)
				break;
			/* El cursor empieza en una carta que se pueda jugar */
			if (!gn_can_play(&s_game, 0, s_selected))
				for (int i = 0; i < n; i++)
					if (gn_can_play(&s_game, 0, i)) {
						s_selected = i;
						break;
					}
			if (in->repeat & KEY_LEFT) {
				s_selected = (s_selected + n - 1) % n;
				audio_play(SFX_SELECT);
			}
			if (in->repeat & KEY_RIGHT) {
				s_selected = (s_selected + 1) % n;
				audio_play(SFX_SELECT);
			}
			int t = touched_card(in);
			if (t >= 0) {
				s_selected = t;
				human_play(t);
			} else if (button_update(&s_btn_play, in)) {
				human_play(s_selected);
			} else if (button_update(&s_btn_sing, in)) {
				if (gn_cante(&s_game, 0))
					announce_cante();
			} else if (button_update(&s_btn_swap, in)) {
				if (gn_swap(&s_game, 0)) {
					audio_play(SFX_FLIP);
					set_message("Has cambiado el 7 por la muestra");
				}
			}
		} else {
			s_btn_play.enabled = s_btn_sing.enabled = s_btn_swap.enabled = false;
			s_cpu_timer += dt;
			if (!is_busy && s_cpu_timer > 0.75f) {
				s_cpu_timer = 0.0f;
				cpu_turn(s_game.current);
			}
		}
		break;
	case GN_PHASE_TRICK_DONE:
		s_btn_play.enabled = s_btn_sing.enabled = s_btn_swap.enabled = false;
		if (!is_busy) {
			s_trick_timer += dt;
			if (s_trick_timer > 1.1f)
				collect();
		}
		break;
	case GN_PHASE_HAND_OVER:
	case GN_PHASE_GAME_OVER: {
		bool game_over = s_game.phase == GN_PHASE_GAME_OVER;
		button_set(&s_btn_play, 12, y, 144, h, game_over ? GLYPH_A " Otra partida" : GLYPH_A " Siguiente mano",
			C2D_Color32(40, 140, 60, 255), KEY_A);
		button_set(&s_btn_swap, 164, y, 144, h, GLYPH_B " Menú", C2D_Color32(190, 60, 50, 255), KEY_B);
		s_btn_play.enabled = s_btn_swap.enabled = !is_busy && s_result_time > 0.6f;
		s_btn_sing.enabled = false;
		if (button_update(&s_btn_play, in)) {
			if (game_over)
				gn_new_game(&s_game);
			else
				gn_new_hand(&s_game);
			reset_visuals();
		} else if (button_update(&s_btn_swap, in)) {
			return SCENE_MENU;
		}
		break;
	}
	}

	if (s_game.phase != s_prev_phase) {
		s_prev_phase = s_game.phase;
		s_cpu_timer = 0.0f;
		s_result_time = 0.0f;
	}
	if ((s_game.phase == GN_PHASE_HAND_OVER || s_game.phase == GN_PHASE_GAME_OVER) && !is_busy) {
		if (s_result_time == 0.0f) {
			int hand_winner = s_game.points[0] >= s_game.points[1] ? 0 : 1;
			if (s_game.phase == GN_PHASE_GAME_OVER) {
				int w = gn_game_winner(&s_game);
				audio_play(w == 0 ? SFX_WIN : SFX_LOSE);
				profile_record_game(GAME_GUINOTE, w == 0);
				hand_winner = w;
			}
			for (int p = 0; p < GN_PLAYERS; p++)
				mood_set(&s_mood[p], gn_team(p) == hand_winner ? EXPR_HAPPY : EXPR_SAD, 4.0f);
		}
		s_result_time += dt;
	}

	sync_hand();
	for (int i = 0; i < DECK_SIZE; i++)
		vis_update(&s_hand[i], dt);
	for (int p = 0; p < GN_PLAYERS; p++) {
		vis_update(&s_trick[p], dt);
		mood_update(&s_mood[p], dt);
		if (s_bubble_time[p] > 0.0f)
			s_bubble_time[p] -= dt;
	}
	flyers_update(dt);
	if (s_message_flash > 0.0f)
		s_message_flash -= dt * 2.0f;
	return SCENE_GUINOTE;
}

/* ---------------------------------------------------------------------------
 * Dibujo
 * ------------------------------------------------------------------------- */

static void draw_seat(int p)
{
	float x = SEAT_X[p], y = SEAT_Y[p];
	int n = s_game.hand_count[p];
	/* Cartas boca abajo */
	for (int i = 0; i < n; i++) {
		float off = (i - (n - 1) / 2.0f);
		if (p == 2)
			gfx_card_back(x + 40 + off * 12.0f, y, 0.42f, 0.42f, off * 0.06f, DEPTH_CARDS + i * 0.02f, NULL);
		else
			gfx_card_back(x + off * 4.0f, y + 46 + off * 3.0f, 0.42f, 0.42f, (p == 1 ? -1.0f : 1.0f) * (0.3f + off * 0.05f),
				DEPTH_CARDS + i * 0.02f, NULL);
	}
	float ax = p == 2 ? x - 50 : x;
	mii_draw(seat_avatar(p), ax, y, 17, DEPTH_UI + 0.5f, mood_expr(&s_mood[p]), false);
	bool turn = s_game.current == p && s_game.phase == GN_PHASE_PLAYING;
	u32 border = turn ? COLOR_GOLD : gn_team(p) == 0 ? C2D_Color32(60, 120, 200, 255) : C2D_Color32(190, 60, 50, 255);
	float nx = p == 2 ? x - 50 : x, ny = y + 20;
	gfx_panel(nx - 36, ny, 72, 18, DEPTH_UI, C2D_Color32(10, 50, 25, 220), border);
	gfx_text(seat_name(p), nx, ny + 2, DEPTH_UI, 0.42f, COLOR_CREAM, C2D_AlignCenter);
}

static void draw_bubble(int p)
{
	if (s_bubble_time[p] <= 0.0f)
		return;
	float bx = p == 0 ? 200 : p == 2 ? 300 : SEAT_X[p], by = p == 0 ? 196 : p == 2 ? 30 : SEAT_Y[p] - 30;
	if (p == 1)
		bx -= 40;
	if (p == 3)
		bx += 40;
	float pop = ease_out_back(fminf(1.0f, (2.0f - s_bubble_time[p]) / 0.2f));
	float w = gfx_text_width(s_bubble[p], 0.5f) + 16;
	gfx_rrect(bx - w / 2 * pop, by - 11 * pop, w * pop, 22 * pop, 8, DEPTH_BANNER, C2D_Color32(255, 250, 235, 240));
	if (pop > 0.8f)
		gfx_text(s_bubble[p], bx, by - 8, DEPTH_BANNER, 0.5f, C2D_Color32(120, 20, 20, 255), C2D_AlignCenter);
}

static void draw_result(void)
{
	if (!banner_draw(s_result_time, 200, 118, 320, 92))
		return;
	bool over = s_game.phase == GN_PHASE_GAME_OVER;
	if (over) {
		int w = gn_game_winner(&s_game);
		gfx_text(w == 0 ? "¡Ganáis la partida!" : "Ganan los rivales", 200, 78, DEPTH_BANNER, 0.75f,
			w == 0 ? COLOR_GOLD : COLOR_RED, C2D_AlignCenter);
	} else {
		gfx_text("Nadie llega a 101: ¡de vueltas!", 200, 78, DEPTH_BANNER, 0.6f, COLOR_GOLD, C2D_AlignCenter);
	}
	gfx_textf(200, 106, DEPTH_BANNER, 0.5f, COLOR_CREAM, C2D_AlignCenter, "Esta mano: nosotros %d - ellos %d",
		s_game.points[0], s_game.points[1]);
	gfx_textf(200, 128, DEPTH_BANNER, 0.55f, COLOR_GOLD, C2D_AlignCenter, "Total: nosotros %d - ellos %d",
		s_game.score[0], s_game.score[1]);
}

static void gn_scene_draw_top(void)
{
	gfx_background(BG_TABLE_TOP);

	/* Mazo con la muestra atravesada debajo */
	if (s_game.deck.count > 0) {
		gfx_card(s_game.trump_card, DECK_X + 24, DECK_Y, 0.55f, 1.0f, (float)M_PI / 2.0f, DEPTH_CARDS, 0.0f, true);
		int layers = (s_game.deck.count - 1 + 3) / 4;
		for (int i = 0; i < layers; i++)
			gfx_card_back(DECK_X - i * 0.5f, DECK_Y - i * 0.7f, 0.55f, 0.55f, 0.0f, DEPTH_CARDS + 0.3f + i * 0.08f, NULL);
		gfx_textf(DECK_X + 10, DECK_Y - 44, DEPTH_UI, 0.4f, COLOR_CREAM, C2D_AlignCenter, "Mazo: %d",
			s_game.deck.count);
	} else {
		gfx_text("Arrastre", DECK_X + 10, DECK_Y - 8, DEPTH_UI, 0.55f, COLOR_GOLD, C2D_AlignCenter);
	}

	/* Marcador */
	gfx_panel(8, 168, 112, 64, DEPTH_UI, C2D_Color32(10, 50, 25, 210), C2D_Color32(20, 60, 30, 255));
	gfx_sprite(SPR_SUIT_FIRST + s_game.trump, 24, 182, 0.55f, DEPTH_UI);
	gfx_text(suit_name((Suit)s_game.trump), 38, 174, DEPTH_UI, 0.42f, COLOR_GOLD, C2D_AlignLeft);
	gfx_textf(64, 192, DEPTH_UI, 0.42f, C2D_Color32(140, 200, 255, 255), C2D_AlignCenter, "Nosotros %d",
		total_points(0));
	gfx_textf(64, 208, DEPTH_UI, 0.42f, C2D_Color32(255, 150, 140, 255), C2D_AlignCenter, "Ellos %d",
		total_points(1));

	for (int p = 1; p < GN_PLAYERS; p++)
		draw_seat(p);
	mii_draw(seat_avatar(0), 200, 212, 15, DEPTH_UI + 0.5f, mood_expr(&s_mood[0]), false);

	/* Baza: la primera carta debajo */
	for (int k = 0; k < GN_PLAYERS; k++)
		vis_draw(&s_trick[(s_game.leader + k) % GN_PLAYERS]);
	flyers_draw(FLY_TOP);
	for (int p = 0; p < GN_PLAYERS; p++)
		draw_bubble(p);

	if ((s_game.phase == GN_PHASE_HAND_OVER || s_game.phase == GN_PHASE_GAME_OVER) && !busy())
		draw_result();
}

static void gn_scene_draw_bottom(void)
{
	gfx_background(BG_TABLE_BOTTOM);
	gfx_text(s_message, 10, 10, 0, 0.45f, s_message_flash > 0.0f ? COLOR_GOLD : COLOR_CREAM, C2D_AlignLeft);
	button_draw(&s_btn_menu, false);

	bool my_turn = s_game.phase == GN_PHASE_PLAYING && s_game.current == 0;
	if (my_turn)
		gfx_text("¡Tu turno!", BOTTOM_W / 2, 32, 0, 0.55f, COLOR_GOLD, C2D_AlignCenter);
	if (my_turn && s_game.hand_count[0] > 0 && !busy()) {
		const VisCard* v = &s_hand[card_index(s_game.hands[0][s_selected])];
		float pulse = 0.75f + 0.25f * sinf(gfx_time() * 6.0f);
		C2D_ImageTint tint;
		C2D_AlphaImageTint(&tint, pulse);
		gfx_sprite_ex(SPR_CARD_GLOW, v->x, v->y, HAND_SCALE, HAND_SCALE, 0.0f, 0, &tint);
	}
	for (int i = 0; i < s_game.hand_count[0]; i++)
		vis_draw(&s_hand[card_index(s_game.hands[0][i])]);
	flyers_draw(FLY_BOTTOM);

	gfx_textf(BOTTOM_W / 2, 160, 0, 0.45f, COLOR_CREAM, C2D_AlignCenter, "Nosotros %d - Ellos %d   (a %d)",
		total_points(0), total_points(1), GN_GAME_POINTS);
	gfx_text("As 11 · Tres 10 · Rey 4 · Sota 3 · Caballo 2", BOTTOM_W / 2, 180, 0, 0.4f,
		C2D_Color32(200, 230, 200, 255), C2D_AlignCenter);

	bool playing = s_game.phase == GN_PHASE_PLAYING;
	button_draw(&s_btn_play, s_btn_play.enabled);
	if (playing)
		button_draw(&s_btn_sing, s_btn_sing.enabled);
	button_draw(&s_btn_swap, s_btn_swap.enabled && playing);
}

const Scene scene_guinote = { gn_scene_enter, gn_scene_update, gn_scene_draw_top, gn_scene_draw_bottom };
