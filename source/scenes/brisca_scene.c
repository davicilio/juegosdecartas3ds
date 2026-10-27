/* Mesa de la brisca: el jugador contra Carmen (CPU). */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "games/brisca.h"
#include "scenes/scene.h"
#include "ui/audio.h"
#include "ui/flyers.h"
#include "ui/gfx.h"
#include "ui/mii.h"
#include "ui/widgets.h"

#define RIVAL_NAME   "Carmen"
#define RIVAL_Y      36.0f
#define RIVAL_SCALE  0.6f
#define TRICK_SCALE  0.8f
#define DECK_X       60.0f
#define DECK_Y       118.0f
#define PILE_X       354.0f
#define PILE_RIVAL_Y 62.0f
#define PILE_MINE_Y  180.0f
#define HAND_SCALE   1.0f
#define HAND_Y       100.0f

static Brisca s_game;
static Mood s_rival_mood, s_my_mood;
static bool s_started;

static VisCard s_hand[DECK_SIZE]; /* indexado por card_index */
static VisCard s_rival[BR_HAND_SIZE];
static int s_rival_shown;
static VisCard s_trick[BR_PLAYERS];
static int s_selected;
static float s_cpu_timer;
static float s_trick_timer;
static float s_result_time;
static BrPhase s_prev_phase;
static char s_message[96];
static float s_message_flash;

static Button s_btn_play, s_btn_swap, s_btn_menu;

static void set_message(const char* msg)
{
	snprintf(s_message, sizeof s_message, "%s", msg);
	s_message_flash = 1.0f;
}

static float rival_slot_x(int i, int n)
{
	return 200.0f + (i - (n - 1) / 2.0f) * 30.0f;
}

static float pile_y(int player)
{
	return player == 0 ? PILE_MINE_Y : PILE_RIVAL_Y;
}

static void reset_visuals(void)
{
	memset(s_hand, 0, sizeof s_hand);
	memset(s_rival, 0, sizeof s_rival);
	memset(s_trick, 0, sizeof s_trick);
	flyers_clear();
	s_rival_shown = 0;
	s_selected = 0;
	s_cpu_timer = 0.0f;
	s_trick_timer = 0.0f;
	s_result_time = 0.0f;
	s_prev_phase = s_game.phase;
	set_message(s_game.current == 0 ? "Sales tú" : "Sale " RIVAL_NAME);
}

static void br_scene_enter(void)
{
	if (!s_started) {
		br_init(&s_game, (uint32_t)time(NULL) ^ 0xB415CA);
		s_started = true;
		reset_visuals();
	}
	button_set(&s_btn_menu, 258, 10, 54, 20, "Menú", C2D_Color32(120, 30, 35, 255), KEY_START);
}

static bool busy(void)
{
	if (flyers_busy())
		return true;
	for (int p = 0; p < BR_PLAYERS; p++)
		if (vis_moving(&s_trick[p]))
			return true;
	for (int i = 0; i < s_rival_shown; i++)
		if (vis_moving(&s_rival[i]))
			return true;
	for (int i = 0; i < DECK_SIZE; i++)
		if (s_hand[i].active && s_hand[i].delay > 0.0f)
			return true;
	return false;
}

/* La posición en la mesa depende de quién ha salido: la primera carta queda
 * debajo y la segunda encima, algo desplazada. */
static void place_trick_card(int p, float from_x, float from_y, float from_scale, bool from_face_up)
{
	bool first = p == s_game.leader;
	VisCard* v = &s_trick[p];
	vis_spawn(v, s_game.trick[p], from_x, from_y, from_scale, from_face_up, 0.0f);
	vis_target(v, first ? 182.0f : 220.0f, first ? 120.0f : 130.0f, TRICK_SCALE, true);
	v->tangle = first ? -0.08f : 0.1f;
}

static void sync_rival(void)
{
	int n = s_game.hand_count[1];
	float delay = 0.0f;
	if (n < s_rival_shown) {
		/* Ha jugado: su carta sale del último hueco. */
		const VisCard* from = &s_rival[s_rival_shown - 1];
		place_trick_card(1, from->x, from->y, RIVAL_SCALE, false);
	}
	for (int i = s_rival_shown; i < n; i++) {
		vis_spawn(&s_rival[i], card_make(SUIT_OROS, 1), DECK_X, DECK_Y, RIVAL_SCALE, false, 0.35f + delay);
		delay += 0.15f;
	}
	s_rival_shown = n;
	for (int i = 0; i < n; i++) {
		vis_target(&s_rival[i], rival_slot_x(i, n), RIVAL_Y, RIVAL_SCALE, false);
		s_rival[i].tangle = (i - (n - 1) / 2.0f) * 0.08f;
	}
}

static void sync_hand(void)
{
	const int n = s_game.hand_count[0];
	bool present[DECK_SIZE] = { false };
	bool my_turn = s_game.phase == BR_PHASE_PLAYING && s_game.current == 0;
	float delay = 0.0f;

	for (int i = 0; i < n; i++) {
		Card c = s_game.hands[0][i];
		int id = card_index(c);
		VisCard* v = &s_hand[id];
		present[id] = true;
		if (!v->active) {
			/* Sale del mazo (arriba) y aparece por el borde de la pantalla inferior. */
			VisCard tmp;
			vis_spawn(&tmp, c, DECK_X, DECK_Y, 0.75f, false, delay);
			flyers_add(&tmp, 200.0f, 290.0f, delay, false, FLY_TOP);
			vis_spawn(v, c, BOTTOM_W / 2.0f, -60.0f, HAND_SCALE, false, delay + 0.3f);
			delay += 0.15f;
		}
		bool selected = my_turn && i == s_selected;
		vis_target(v, BOTTOM_W / 2.0f + (i - (n - 1) / 2.0f) * 78.0f, HAND_Y - (selected ? 12.0f : 0.0f),
			HAND_SCALE, true);
	}
	for (int id = 0; id < DECK_SIZE; id++)
		if (s_hand[id].active && !present[id]) {
			flyers_add(&s_hand[id], s_hand[id].x, -80.0f, 0.0f, true, FLY_BOTTOM);
			s_hand[id].active = false;
		}
}

static void after_play(int p)
{
	if (p == 0)
		place_trick_card(0, 200.0f, 290.0f, 0.9f, true);
	if (s_game.phase == BR_PHASE_TRICK_DONE) {
		char buf[96];
		int w = s_game.trick_winner;
		if (w == 0)
			snprintf(buf, sizeof buf, "Te llevas la baza (+%d)", s_game.trick_points);
		else
			snprintf(buf, sizeof buf, RIVAL_NAME " se lleva la baza (+%d)", s_game.trick_points);
		set_message(buf);
		s_trick_timer = 0.0f;
	}
}

static void collect(void)
{
	int w = s_game.trick_winner;
	if (s_game.trick_points >= 10) {
		/* Las bazas gordas se celebran (o se lamentan) */
		mood_set(&s_rival_mood, w == 1 ? EXPR_HAPPY : EXPR_SAD, 1.5f);
		mood_set(&s_my_mood, w == 0 ? EXPR_HAPPY : EXPR_SAD, 1.5f);
	}
	br_collect(&s_game);
	for (int p = 0; p < BR_PLAYERS; p++) {
		flyers_add(&s_trick[p], PILE_X, pile_y(w), p * 0.08f, false, FLY_TOP);
		s_trick[p].active = false;
	}
	audio_play(SFX_CARD);
	if (s_game.phase == BR_PHASE_PLAYING && s_game.current == 0)
		s_selected = 0;
}

static void human_play(int idx)
{
	br_play(&s_game, idx);
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
	for (int i = 0; i < s_game.hand_count[0]; i++) {
		const VisCard* v = &s_hand[card_index(s_game.hands[0][i])];
		float w = CARD_W * HAND_SCALE / 2.0f, h = CARD_H * HAND_SCALE / 2.0f;
		if (in->tx >= v->x - w && in->tx <= v->x + w && in->ty >= v->y - h && in->ty <= v->y + h)
			return i;
	}
	return -1;
}

static const char* swap_label(void)
{
	int idx = br_swap_index(&s_game, 0);
	if (idx >= 0 && s_game.hands[0][idx].rank == 2)
		return GLYPH_Y " Cambiar el 2";
	return GLYPH_Y " Cambiar el 7";
}

static SceneId br_scene_update(const Input* in, float dt)
{
	if (button_update(&s_btn_menu, in))
		return SCENE_MENU;

	bool is_busy = busy();
	const float y = 205, h = 28;

	switch (s_game.phase) {
	case BR_PHASE_PLAYING:
		button_set(&s_btn_play, 12, y, 144, h, GLYPH_A " Jugar", C2D_Color32(40, 140, 60, 255), KEY_A);
		button_set(&s_btn_swap, 164, y, 144, h, swap_label(), C2D_Color32(200, 140, 30, 255), KEY_Y);
		if (s_game.current == 0) {
			s_btn_play.enabled = !is_busy;
			s_btn_swap.enabled = !is_busy && br_swap_index(&s_game, 0) >= 0;
			if (is_busy)
				break;
			int n = s_game.hand_count[0];
			if (n == 0)
				break;
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
			} else if (button_update(&s_btn_swap, in)) {
				br_swap(&s_game, 0);
				audio_play(SFX_FLIP);
				set_message("Has cambiado la muestra");
			}
		} else {
			s_btn_play.enabled = s_btn_swap.enabled = false;
			s_cpu_timer += dt;
			if (!is_busy && s_cpu_timer > 0.7f) {
				s_cpu_timer = 0.0f;
				if (br_swap_index(&s_game, 1) >= 0) {
					br_swap(&s_game, 1);
					audio_play(SFX_FLIP);
					set_message(RIVAL_NAME " cambia la muestra");
				} else {
					br_play(&s_game, br_ai_choose(&s_game, 1));
					after_play(1);
				}
			}
		}
		break;
	case BR_PHASE_TRICK_DONE:
		s_btn_play.enabled = s_btn_swap.enabled = false;
		if (!is_busy) {
			s_trick_timer += dt;
			if (s_trick_timer > 1.0f)
				collect();
		}
		break;
	case BR_PHASE_GAME_OVER:
		button_set(&s_btn_play, 12, y, 144, h, GLYPH_A " Otra partida", C2D_Color32(40, 140, 60, 255), KEY_A);
		button_set(&s_btn_swap, 164, y, 144, h, GLYPH_B " Menú", C2D_Color32(190, 60, 50, 255), KEY_B);
		s_btn_play.enabled = s_btn_swap.enabled = !is_busy && s_result_time > 0.6f;
		if (button_update(&s_btn_play, in)) {
			br_new_game(&s_game);
			reset_visuals();
		} else if (button_update(&s_btn_swap, in)) {
			return SCENE_MENU;
		}
		break;
	}

	if (s_game.phase != s_prev_phase) {
		s_prev_phase = s_game.phase;
		s_cpu_timer = 0.0f;
		s_result_time = 0.0f;
	}
	if (s_game.phase == BR_PHASE_GAME_OVER && !is_busy) {
		if (s_result_time == 0.0f) {
			int w = br_game_winner(&s_game);
			audio_play(w == 0 ? SFX_WIN : SFX_LOSE);
			mood_set(&s_rival_mood, w == 1 ? EXPR_HAPPY : w == 0 ? EXPR_SAD : EXPR_SURPRISED, 4.0f);
			mood_set(&s_my_mood, w == 0 ? EXPR_HAPPY : w == 1 ? EXPR_SAD : EXPR_SURPRISED, 4.0f);
			profile_record_game(GAME_BRISCA, w == 0);
		}
		s_result_time += dt;
	}

	sync_rival();
	sync_hand();
	for (int i = 0; i < DECK_SIZE; i++)
		vis_update(&s_hand[i], dt);
	for (int i = 0; i < s_rival_shown; i++)
		vis_update(&s_rival[i], dt);
	for (int p = 0; p < BR_PLAYERS; p++)
		vis_update(&s_trick[p], dt);
	flyers_update(dt);
	mood_update(&s_rival_mood, dt);
	mood_update(&s_my_mood, dt);
	if (s_message_flash > 0.0f)
		s_message_flash -= dt * 2.0f;
	return SCENE_BRISCA;
}

static void draw_pile(int player)
{
	int cards = s_game.won_cards[player];
	if (cards == 0)
		return;
	int layers = cards / 4 + 1;
	if (layers > 6)
		layers = 6;
	float y = pile_y(player);
	for (int i = 0; i < layers; i++)
		gfx_card(card_make(SUIT_OROS, 1), PILE_X - i * 0.5f, y - i * 0.8f, 0.6f, 0.0f, 0.04f * (i % 2 ? 1 : -1),
			DEPTH_CARDS + i * 0.1f, 0.0f, i == 0);
	gfx_textf(PILE_X, y + 30, DEPTH_UI, 0.42f, COLOR_CREAM, C2D_AlignCenter, "%d cartas", cards);
}

static void draw_banner(void)
{
	if (!banner_draw(s_result_time, 200, 120, 300, 64))
		return;
	int w = br_game_winner(&s_game);
	if (w == 0)
		gfx_text("¡Ganas la partida!", 200, 92, DEPTH_BANNER, 0.8f, COLOR_GOLD, C2D_AlignCenter);
	else if (w == 1)
		gfx_text("Gana " RIVAL_NAME, 200, 92, DEPTH_BANNER, 0.8f, COLOR_RED, C2D_AlignCenter);
	else
		gfx_text("Empate a 60", 200, 92, DEPTH_BANNER, 0.8f, COLOR_CREAM, C2D_AlignCenter);
	gfx_textf(200, 122, DEPTH_BANNER, 0.55f, COLOR_CREAM, C2D_AlignCenter, "Tú %d  -  %d " RIVAL_NAME,
		s_game.points[0], s_game.points[1]);
}

static void br_scene_draw_top(void)
{
	gfx_background(BG_TABLE_TOP);

	/* Marcador del rival */
	gfx_panel(14, 14, 104, 50, DEPTH_UI, C2D_Color32(10, 50, 25, 210),
		s_game.current == 1 && s_game.phase == BR_PHASE_PLAYING ? COLOR_GOLD : C2D_Color32(20, 60, 30, 255));
	mii_draw(cpu_avatar(CPU_CARMEN), 36, 40, 15, DEPTH_UI + 0.4f, mood_expr(&s_rival_mood), false);
	gfx_text(RIVAL_NAME, 84, 20, DEPTH_UI, 0.5f, COLOR_CREAM, C2D_AlignCenter);
	gfx_textf(84, 40, DEPTH_UI, 0.38f, COLOR_CREAM, C2D_AlignCenter, "Partidas: %d", s_game.games_won[1]);

	/* Mazo con la muestra atravesada debajo */
	if (s_game.deck.count > 0) {
		gfx_card(s_game.trump_card, DECK_X + 34, DECK_Y, 0.75f, 1.0f, (float)M_PI / 2.0f, DEPTH_CARDS, 0.0f, true);
		int layers = (s_game.deck.count - 1 + 3) / 4;
		for (int i = 0; i < layers; i++)
			gfx_card(card_make(SUIT_OROS, 1), DECK_X - i * 0.5f, DECK_Y - i * 0.7f, 0.75f, 0.0f, 0.0f,
				DEPTH_CARDS + 0.3f + i * 0.08f, 0.0f, i == 0);
		gfx_textf(DECK_X + 8, DECK_Y + 42, DEPTH_UI, 0.42f, COLOR_CREAM, C2D_AlignCenter, "Mazo: %d",
			s_game.deck.count);
	}
	/* Recordatorio del triunfo */
	gfx_panel(14, 178, 84, 40, DEPTH_UI, C2D_Color32(60, 30, 10, 210), C2D_Color32(200, 150, 60, 255));
	gfx_text("Triunfo", 56, 181, DEPTH_UI, 0.4f, COLOR_CREAM, C2D_AlignCenter);
	gfx_sprite(SPR_SUIT_FIRST + s_game.trump, 36, 205, 0.7f, DEPTH_UI);
	gfx_text(suit_name((Suit)s_game.trump), 66, 197, DEPTH_UI, 0.45f, COLOR_GOLD, C2D_AlignCenter);

	draw_pile(1);
	draw_pile(0);

	for (int i = 0; i < s_rival_shown; i++)
		vis_draw(&s_rival[i]);
	/* La carta que salió primero se dibuja debajo */
	vis_draw(&s_trick[s_game.leader]);
	vis_draw(&s_trick[1 - s_game.leader]);
	flyers_draw(FLY_TOP);

	if (s_game.phase == BR_PHASE_GAME_OVER && !busy())
		draw_banner();
}

static void br_scene_draw_bottom(void)
{
	gfx_background(BG_TABLE_BOTTOM);

	gfx_text(s_message, 14, 12, 0, 0.5f, s_message_flash > 0.0f ? COLOR_GOLD : COLOR_CREAM, C2D_AlignLeft);
	button_draw(&s_btn_menu, false);

	bool my_turn = s_game.phase == BR_PHASE_PLAYING && s_game.current == 0;
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

	mii_draw(&profile_get()->avatar, 24, 168, 9, 0, mood_expr(&s_my_mood), false);
	gfx_textf(38, 160, 0, 0.5f, COLOR_CREAM, C2D_AlignLeft, "Tus puntos: %d", s_game.points[0]);
	gfx_textf(306, 160, 0, 0.5f, COLOR_CREAM, C2D_AlignRight, "Partidas: %d", s_game.games_won[0]);
	gfx_text("As 11 · Tres 10 · Rey 4 · Caballo 3 · Sota 2", BOTTOM_W / 2, 180, 0, 0.4f,
		C2D_Color32(200, 230, 200, 255), C2D_AlignCenter);

	button_draw(&s_btn_play, s_btn_play.enabled);
	button_draw(&s_btn_swap, s_btn_swap.enabled && s_game.phase == BR_PHASE_PLAYING);
}

const Scene scene_brisca = { br_scene_enter, br_scene_update, br_scene_draw_top, br_scene_draw_bottom };
